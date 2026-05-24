#include <aurex/ir/pass_pipeline.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aurex::ir {

namespace {

using ValueReplacementMap = std::unordered_map<base::u32, ValueId>;
using BlockMap = std::vector<BlockId>;

[[nodiscard]] PreservedAnalyses preserve_module_metadata_analyses() noexcept
{
    PreservedAnalyses preserved = PreservedAnalyses::none();
    preserved.preserve(AnalysisId::type_table);
    preserved.preserve(AnalysisId::symbol_table);
    preserved.preserve(AnalysisId::record_layouts);
    return preserved;
}

[[nodiscard]] PreservedAnalyses preserve_cfg_and_module_metadata_analyses() noexcept
{
    PreservedAnalyses preserved = preserve_module_metadata_analyses();
    preserved.preserve(AnalysisId::control_flow_graph);
    return preserved;
}

[[nodiscard]] ValueId resolve_replacement(ValueId id, const ValueReplacementMap& replacements)
{
    std::unordered_set<base::u32> seen;
    while (is_valid(id)) {
        const auto found = replacements.find(id.value);
        if (found == replacements.end()) {
            return id;
        }
        if (!seen.insert(id.value).second) {
            return id;
        }
        id = found->second;
    }
    return id;
}

void rewrite_value_id(ValueId& id, const ValueReplacementMap& replacements)
{
    if (is_valid(id)) {
        id = resolve_replacement(id, replacements);
    }
}

void rewrite_value(Value& value, const ValueReplacementMap& replacements)
{
    rewrite_value_id(value.lhs, replacements);
    rewrite_value_id(value.rhs, replacements);
    rewrite_value_id(value.object, replacements);
    rewrite_value_id(value.index, replacements);
    for (ValueId& arg : value.args) {
        rewrite_value_id(arg, replacements);
    }
    for (ValueId& element : value.elements) {
        rewrite_value_id(element, replacements);
    }
    for (FieldValue& field : value.fields) {
        rewrite_value_id(field.value, replacements);
    }
    for (PhiInput& incoming : value.incoming) {
        rewrite_value_id(incoming.value, replacements);
    }
}

void rewrite_terminator(Terminator& terminator, const ValueReplacementMap& replacements)
{
    rewrite_value_id(terminator.condition, replacements);
    rewrite_value_id(terminator.value, replacements);
}

[[nodiscard]] bool is_scalar_promotable_type(const Module& module, const sema::TypeHandle type)
{
    if (!sema::is_valid(type) || !module.types.is_pointer(type)) {
        return false;
    }
    const sema::TypeHandle pointee = module.types.get(type).pointee;
    if (!sema::is_valid(pointee)) {
        return false;
    }
    return module.types.is_bool(pointee) || module.types.is_integer(pointee) || module.types.is_float(pointee)
        || module.types.is_pointer(pointee) || module.types.is_reference(pointee) || module.types.is_function(pointee)
        || module.types.get(pointee).kind == sema::TypeKind::enum_;
}

struct FunctionUseInfo {
    std::unordered_map<base::u32, base::u32> promotable_slots;
};

[[nodiscard]] FunctionUseInfo collect_promotable_slots(const Module& module, const Function& function)
{
    FunctionUseInfo info;

    std::unordered_map<base::u32, base::u32> alloca_blocks;
    for (base::u32 block_index = 0; block_index < function.blocks.size(); ++block_index) {
        for (const ValueId value_id : function.blocks[block_index].values) {
            if (!is_valid(value_id) || value_id.value >= module.values.size()) {
                continue;
            }
            const Value& value = module.values[value_id.value];
            if (value.kind == ValueKind::alloca && is_scalar_promotable_type(module, value.type)) {
                alloca_blocks[value_id.value] = block_index;
            }
        }
    }

    std::unordered_map<base::u32, bool> escaped;
    std::unordered_map<base::u32, std::optional<base::u32>> use_blocks;
    for (const auto& entry : alloca_blocks) {
        escaped[entry.first] = false;
        use_blocks[entry.first] = std::nullopt;
    }

    const auto record_slot_use = [&](const ValueId value_id, const base::u32 block_index,
                                     const bool allowed_memory_use) {
        if (!is_valid(value_id)) {
            return;
        }
        const auto found = alloca_blocks.find(value_id.value);
        if (found == alloca_blocks.end()) {
            return;
        }
        if (!allowed_memory_use) {
            escaped[value_id.value] = true;
            return;
        }
        std::optional<base::u32>& use_block = use_blocks[value_id.value];
        if (!use_block.has_value()) {
            use_block = block_index;
        } else if (*use_block != block_index) {
            escaped[value_id.value] = true;
        }
    };

    for (base::u32 block_index = 0; block_index < function.blocks.size(); ++block_index) {
        const BasicBlock& block = function.blocks[block_index];
        for (const ValueId value_id : block.values) {
            if (!is_valid(value_id) || value_id.value >= module.values.size()) {
                continue;
            }
            const Value& value = module.values[value_id.value];
            switch (value.kind) {
                case ValueKind::load:
                    record_slot_use(value.object, block_index, true);
                    break;
                case ValueKind::store:
                    record_slot_use(value.object, block_index, true);
                    record_slot_use(value.lhs, block_index, false);
                    break;
                case ValueKind::unary:
                    record_slot_use(value.lhs, block_index, false);
                    break;
                case ValueKind::binary:
                    record_slot_use(value.lhs, block_index, false);
                    record_slot_use(value.rhs, block_index, false);
                    break;
                case ValueKind::phi:
                    for (const PhiInput& incoming : value.incoming) {
                        record_slot_use(incoming.value, block_index, false);
                    }
                    break;
                case ValueKind::call:
                    record_slot_use(value.object, block_index, false);
                    for (const ValueId arg : value.args) {
                        record_slot_use(arg, block_index, false);
                    }
                    break;
                case ValueKind::field_addr:
                    record_slot_use(value.object, block_index, false);
                    break;
                case ValueKind::index_addr:
                    record_slot_use(value.object, block_index, false);
                    record_slot_use(value.index, block_index, false);
                    break;
                case ValueKind::aggregate:
                    for (const ValueId element : value.elements) {
                        record_slot_use(element, block_index, false);
                    }
                    for (const FieldValue& field : value.fields) {
                        record_slot_use(field.value, block_index, false);
                    }
                    break;
                case ValueKind::slice:
                    record_slot_use(value.lhs, block_index, false);
                    record_slot_use(value.rhs, block_index, false);
                    break;
                case ValueKind::slice_data:
                case ValueKind::slice_len:
                    record_slot_use(value.object, block_index, false);
                    break;
                case ValueKind::cast:
                    record_slot_use(value.lhs, block_index, false);
                    break;
                case ValueKind::str_data:
                case ValueKind::str_byte_len:
                case ValueKind::str_is_valid_utf8:
                case ValueKind::str_from_utf8_checked:
                    record_slot_use(value.object, block_index, false);
                    break;
                case ValueKind::str_slice_checked:
                    record_slot_use(value.object, block_index, false);
                    record_slot_use(value.lhs, block_index, false);
                    record_slot_use(value.rhs, block_index, false);
                    break;
                case ValueKind::str_from_bytes_unchecked:
                    for (const ValueId arg : value.args) {
                        record_slot_use(arg, block_index, false);
                    }
                    break;
                case ValueKind::param:
                case ValueKind::integer_literal:
                case ValueKind::float_literal:
                case ValueKind::bool_literal:
                case ValueKind::char_literal:
                case ValueKind::null_literal:
                case ValueKind::string_literal:
                case ValueKind::raw_string_literal:
                case ValueKind::c_string_literal:
                case ValueKind::byte_literal:
                case ValueKind::undef:
                case ValueKind::constant_ref:
                case ValueKind::function_ref:
                case ValueKind::alloca:
                case ValueKind::size_of:
                case ValueKind::align_of:
                    break;
            }
        }
        record_slot_use(block.terminator.condition, block_index, false);
        record_slot_use(block.terminator.value, block_index, false);
    }

    for (const auto& entry : alloca_blocks) {
        if (escaped[entry.first]) {
            continue;
        }
        const std::optional<base::u32>& use_block = use_blocks[entry.first];
        if (!use_block.has_value() || *use_block == entry.second) {
            info.promotable_slots[entry.first] = entry.second;
        }
    }
    return info;
}

[[nodiscard]] bool run_local_mem2reg(Module& module)
{
    ValueReplacementMap replacements;
    bool changed = false;
    for (Function& function : module.functions) {
        if (function.linkage == Linkage::extern_c || function.blocks.empty()) {
            continue;
        }

        const FunctionUseInfo info = collect_promotable_slots(module, function);
        if (info.promotable_slots.empty()) {
            continue;
        }

        for (base::u32 block_index = 0; block_index < function.blocks.size(); ++block_index) {
            BasicBlock& block = function.blocks[block_index];
            std::unordered_map<base::u32, ValueId> current_slot_value;
            IrVector<ValueId> kept = module.make_vector<ValueId>();
            kept.reserve(block.values.size());

            for (const ValueId value_id : block.values) {
                if (!is_valid(value_id) || value_id.value >= module.values.size()) {
                    kept.push_back(value_id);
                    continue;
                }

                Value& value = module.values[value_id.value];
                rewrite_value(value, replacements);
                const auto slot = info.promotable_slots.find(value_id.value);
                if (slot != info.promotable_slots.end() && slot->second == block_index) {
                    changed = true;
                    continue;
                }

                if (value.kind == ValueKind::store && is_valid(value.object)) {
                    const auto promoted = info.promotable_slots.find(value.object.value);
                    if (promoted != info.promotable_slots.end() && promoted->second == block_index) {
                        current_slot_value[value.object.value] = resolve_replacement(value.lhs, replacements);
                        changed = true;
                        continue;
                    }
                }

                if (value.kind == ValueKind::load && is_valid(value.object)) {
                    const auto promoted = info.promotable_slots.find(value.object.value);
                    if (promoted != info.promotable_slots.end() && promoted->second == block_index) {
                        const auto current = current_slot_value.find(value.object.value);
                        if (current != current_slot_value.end()) {
                            replacements[value_id.value] = resolve_replacement(current->second, replacements);
                            changed = true;
                            continue;
                        }
                    }
                }

                kept.push_back(value_id);
            }

            block.values = std::move(kept);
            rewrite_terminator(block.terminator, replacements);
        }
    }

    if (replacements.empty()) {
        return changed;
    }
    for (Value& value : module.values) {
        rewrite_value(value, replacements);
    }
    for (GlobalConstant& constant : module.constants) {
        rewrite_value_id(constant.initializer, replacements);
    }
    for (Function& function : module.functions) {
        for (BasicBlock& block : function.blocks) {
            rewrite_terminator(block.terminator, replacements);
        }
    }
    return true;
}

void mark_reachable(const Function& function, const BlockId block, std::vector<bool>& reachable)
{
    std::vector<BlockId> pending;
    pending.push_back(block);
    while (!pending.empty()) {
        const BlockId current = pending.back();
        pending.pop_back();
        if (!is_valid(current) || current.value >= function.blocks.size() || reachable[current.value]) {
            continue;
        }
        reachable[current.value] = true;
        const Terminator& term = function.blocks[current.value].terminator;
        switch (term.kind) {
            case TerminatorKind::branch:
                pending.push_back(term.target);
                break;
            case TerminatorKind::cond_branch:
                pending.push_back(term.then_target);
                pending.push_back(term.else_target);
                break;
            case TerminatorKind::none:
            case TerminatorKind::return_:
                break;
        }
    }
}

void rewrite_block_id(BlockId& id, const BlockMap& block_map)
{
    if (!is_valid(id) || id.value >= block_map.size()) {
        id = INVALID_BLOCK_ID;
        return;
    }
    id = block_map[id.value];
}

void rewrite_phi_block_refs(Module& module, Function& function, const BlockMap& block_map)
{
    for (BasicBlock& block : function.blocks) {
        for (const ValueId value_id : block.values) {
            if (!is_valid(value_id) || value_id.value >= module.values.size()) {
                continue;
            }
            Value& value = module.values[value_id.value];
            if (value.kind != ValueKind::phi) {
                continue;
            }
            IrVector<PhiInput> kept = module.make_vector<PhiInput>();
            kept.reserve(value.incoming.size());
            for (PhiInput incoming : value.incoming) {
                rewrite_block_id(incoming.predecessor, block_map);
                if (is_valid(incoming.predecessor)) {
                    kept.push_back(incoming);
                }
            }
            value.incoming = std::move(kept);
        }
    }
}

[[nodiscard]] bool remove_unreachable_blocks(Module& module, Function& function)
{
    if (function.blocks.empty()) {
        return false;
    }

    std::vector<bool> reachable(function.blocks.size(), false);
    mark_reachable(function, BlockId{0}, reachable);
    if (std::all_of(reachable.begin(), reachable.end(), [](const bool value) {
            return value;
        })) {
        return false;
    }

    BlockMap block_map(function.blocks.size(), INVALID_BLOCK_ID);
    IrVector<BasicBlock> kept = module.make_vector<BasicBlock>();
    kept.reserve(function.blocks.size());
    for (base::u32 i = 0; i < function.blocks.size(); ++i) {
        if (!reachable[i]) {
            continue;
        }
        block_map[i] = BlockId{static_cast<base::u32>(kept.size())};
        kept.push_back(std::move(function.blocks[i]));
    }

    function.blocks = std::move(kept);
    for (BasicBlock& block : function.blocks) {
        rewrite_block_id(block.terminator.target, block_map);
        rewrite_block_id(block.terminator.then_target, block_map);
        rewrite_block_id(block.terminator.else_target, block_map);
    }
    rewrite_phi_block_refs(module, function, block_map);
    return true;
}

[[nodiscard]] bool block_has_phi(const Module& module, const BasicBlock& block)
{
    for (const ValueId value_id : block.values) {
        if (is_valid(value_id) && value_id.value < module.values.size()
            && module.values[value_id.value].kind == ValueKind::phi) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::optional<BlockId> empty_branch_target(const Function& function, const base::u32 block_index)
{
    if (block_index == 0 || block_index >= function.blocks.size()) {
        return std::nullopt;
    }
    const BasicBlock& block = function.blocks[block_index];
    if (!block.values.empty() || block.terminator.kind != TerminatorKind::branch) {
        return std::nullopt;
    }
    if (!is_valid(block.terminator.target) || block.terminator.target.value == block_index) {
        return std::nullopt;
    }
    return block.terminator.target;
}

[[nodiscard]] BlockId redirect_target(BlockId target, const std::vector<std::optional<BlockId>>& redirects)
{
    std::unordered_set<base::u32> seen;
    while (is_valid(target) && target.value < redirects.size() && redirects[target.value].has_value()) {
        if (!seen.insert(target.value).second) {
            return target;
        }
        target = *redirects[target.value];
    }
    return target;
}

[[nodiscard]] bool merge_empty_branch_blocks(Module& module, Function& function)
{
    std::vector<std::optional<BlockId>> redirects(function.blocks.size());
    bool changed = false;
    for (base::u32 block_index = 1; block_index < function.blocks.size(); ++block_index) {
        const std::optional<BlockId> target = empty_branch_target(function, block_index);
        if (!target.has_value() || target->value >= function.blocks.size()) {
            continue;
        }
        if (block_has_phi(module, function.blocks[target->value])) {
            continue;
        }
        redirects[block_index] = *target;
        changed = true;
    }
    if (!changed) {
        return false;
    }

    for (std::optional<BlockId>& redirect : redirects) {
        if (redirect.has_value()) {
            redirect = redirect_target(*redirect, redirects);
        }
    }
    for (BasicBlock& block : function.blocks) {
        if (block.terminator.kind == TerminatorKind::branch) {
            block.terminator.target = redirect_target(block.terminator.target, redirects);
        } else if (block.terminator.kind == TerminatorKind::cond_branch) {
            block.terminator.then_target = redirect_target(block.terminator.then_target, redirects);
            block.terminator.else_target = redirect_target(block.terminator.else_target, redirects);
            if (block.terminator.then_target.value == block.terminator.else_target.value) {
                block.terminator.kind = TerminatorKind::branch;
                block.terminator.target = block.terminator.then_target;
                block.terminator.condition = INVALID_VALUE_ID;
                block.terminator.then_target = INVALID_BLOCK_ID;
                block.terminator.else_target = INVALID_BLOCK_ID;
            }
        }
    }

    return remove_unreachable_blocks(module, function) || changed;
}

[[nodiscard]] bool run_cfg_cleanup(Module& module)
{
    bool any_changed = false;
    bool changed = true;
    while (changed) {
        changed = false;
        for (Function& function : module.functions) {
            if (function.linkage == Linkage::extern_c || function.blocks.empty()) {
                continue;
            }
            for (BasicBlock& block : function.blocks) {
                if (block.terminator.kind == TerminatorKind::cond_branch
                    && block.terminator.then_target.value == block.terminator.else_target.value) {
                    block.terminator.kind = TerminatorKind::branch;
                    block.terminator.target = block.terminator.then_target;
                    block.terminator.condition = INVALID_VALUE_ID;
                    block.terminator.then_target = INVALID_BLOCK_ID;
                    block.terminator.else_target = INVALID_BLOCK_ID;
                    changed = true;
                    any_changed = true;
                }
            }
            const bool removed_unreachable = remove_unreachable_blocks(module, function);
            changed = removed_unreachable || changed;
            any_changed = removed_unreachable || any_changed;
            const bool merged_empty_blocks = merge_empty_branch_blocks(module, function);
            changed = merged_empty_blocks || changed;
            any_changed = merged_empty_blocks || any_changed;
        }
    }
    return any_changed;
}

[[nodiscard]] base::Result<PassResult> run_local_mem2reg_pass(Module& module)
{
    const bool changed = run_local_mem2reg(module);
    if (!changed) {
        return base::Result<PassResult>::ok(PassResult::unchanged());
    }
    return base::Result<PassResult>::ok(PassResult::changed_result(preserve_cfg_and_module_metadata_analyses()));
}

[[nodiscard]] base::Result<PassResult> run_cfg_cleanup_pass(Module& module)
{
    const bool changed = run_cfg_cleanup(module);
    if (!changed) {
        return base::Result<PassResult>::ok(PassResult::unchanged());
    }
    return base::Result<PassResult>::ok(PassResult::changed_result(preserve_module_metadata_analyses()));
}

[[nodiscard]] std::vector<ModulePass> make_optimization_passes(const PassPipelineOptions& options)
{
    std::vector<ModulePass> passes;
    if (options.optimization_level == OptimizationLevel::none) {
        return passes;
    }
    if (options.enable_mem2reg) {
        passes.push_back(ModulePass{
            PassId::local_mem2reg,
            pass_id_name(PassId::local_mem2reg),
            run_local_mem2reg_pass,
        });
    }
    if (options.enable_cfg_cleanup) {
        passes.push_back(ModulePass{
            PassId::cfg_cleanup,
            pass_id_name(PassId::cfg_cleanup),
            run_cfg_cleanup_pass,
        });
    }
    return passes;
}

} // namespace

std::string_view optimization_level_name(const OptimizationLevel level) noexcept
{
    switch (level) {
        case OptimizationLevel::none:
            return "O0";
        case OptimizationLevel::basic:
            return "O1";
        case OptimizationLevel::standard:
            return "O2";
        case OptimizationLevel::aggressive:
            return "O3";
    }
    return "O0";
}

base::Result<PassPipelineRunSummary> run_pass_pipeline_with_summary(Module& module, const PassPipelineOptions& options)
{
    ModulePassManager manager;
    for (const ModulePass& pass : make_optimization_passes(options)) {
        manager.add(pass);
    }

    const VerifierGate verifier(VerifierGateOptions{
        options.verify_input,
        options.verify_output,
        options.verify_after_each_pass,
    });
    return manager.run(module, verifier);
}

base::Result<void> run_pass_pipeline(Module& module, const PassPipelineOptions& options)
{
    auto result = run_pass_pipeline_with_summary(module, options);
    if (!result) {
        return base::Result<void>::fail(result.error());
    }
    return base::Result<void>::ok();
}

} // namespace aurex::ir
