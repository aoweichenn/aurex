#include <aurex/ir/analysis_manager.hpp>

#include <algorithm>
#include <utility>

namespace aurex::ir {
namespace {

using DominatorBits = std::vector<std::vector<bool>>;

[[nodiscard]] bool block_id_less(const BlockId lhs, const BlockId rhs) noexcept
{
    return lhs.value < rhs.value;
}

[[nodiscard]] bool contains_block_id(const std::vector<BlockId>& blocks, const BlockId block) noexcept
{
    return std::find_if(blocks.begin(), blocks.end(), [block](const BlockId current) {
        return current.value == block.value;
    }) != blocks.end();
}

void add_successor(std::vector<BlockId>& successors, const BlockId target, const base::usize block_count)
{
    if (!is_valid(target) || target.value >= block_count || contains_block_id(successors, target)) {
        return;
    }
    successors.push_back(target);
}

void collect_terminator_successors(
    std::vector<BlockId>& successors, const Terminator& terminator, const base::usize block_count)
{
    switch (terminator.kind) {
        case TerminatorKind::branch:
            add_successor(successors, terminator.target, block_count);
            break;
        case TerminatorKind::cond_branch:
            add_successor(successors, terminator.then_target, block_count);
            add_successor(successors, terminator.else_target, block_count);
            break;
        case TerminatorKind::none:
        case TerminatorKind::return_:
            break;
    }
}

[[nodiscard]] ControlFlowGraphAnalysis build_control_flow_graph(const Module& module)
{
    ControlFlowGraphAnalysis analysis;
    analysis.functions.reserve(module.functions.size());
    for (base::u32 function_index = 0; function_index < module.functions.size(); ++function_index) {
        const Function& function = module.functions[function_index];
        FunctionControlFlowGraph graph;
        graph.function = FunctionId{function_index};
        graph.blocks.resize(function.blocks.size());

        for (base::u32 block_index = 0; block_index < function.blocks.size(); ++block_index) {
            BlockControlFlow& block_flow = graph.blocks[block_index];
            block_flow.block = BlockId{block_index};
            collect_terminator_successors(
                block_flow.successors, function.blocks[block_index].terminator, function.blocks.size());
        }

        for (const BlockControlFlow& block_flow : graph.blocks) {
            for (const BlockId successor : block_flow.successors) {
                graph.blocks[successor.value].predecessors.push_back(block_flow.block);
            }
        }
        analysis.functions.push_back(std::move(graph));
    }
    return analysis;
}

void record_use(ValueUseAnalysis& analysis, const ValueId value, const ValueUseSite& site)
{
    if (!is_valid(value) || value.value >= analysis.values.size()) {
        return;
    }
    analysis.values[value.value].uses.push_back(site);
}

void record_value_operands(ValueUseAnalysis& analysis, const Value& value, const ValueUseSite& value_site)
{
    record_use(analysis, value.lhs, value_site);
    record_use(analysis, value.rhs, value_site);
    record_use(analysis, value.object, value_site);
    record_use(analysis, value.index, value_site);
    for (const ValueId argument : value.args) {
        record_use(analysis, argument, value_site);
    }
    for (const ValueId element : value.elements) {
        record_use(analysis, element, value_site);
    }
    for (const FieldValue& field : value.fields) {
        record_use(analysis, field.value, value_site);
    }

    ValueUseSite phi_site = value_site;
    phi_site.kind = ValueUseSiteKind::phi_input;
    for (const PhiInput& incoming : value.incoming) {
        record_use(analysis, incoming.value, phi_site);
    }
}

[[nodiscard]] ValueUseAnalysis build_value_uses(const Module& module)
{
    ValueUseAnalysis analysis;
    analysis.values.resize(module.values.size());
    for (base::u32 value_index = 0; value_index < module.values.size(); ++value_index) {
        analysis.values[value_index].value = ValueId{value_index};
    }

    for (base::u32 constant_index = 0; constant_index < module.constants.size(); ++constant_index) {
        const ValueUseSite site{
            ValueUseSiteKind::global_initializer,
            INVALID_FUNCTION_ID,
            INVALID_BLOCK_ID,
            INVALID_VALUE_ID,
            GlobalConstantId{constant_index},
        };
        record_use(analysis, module.constants[constant_index].initializer, site);
    }

    for (base::u32 function_index = 0; function_index < module.functions.size(); ++function_index) {
        const Function& function = module.functions[function_index];
        const FunctionId function_id{function_index};
        for (base::u32 block_index = 0; block_index < function.blocks.size(); ++block_index) {
            const BasicBlock& block = function.blocks[block_index];
            const BlockId block_id{block_index};
            for (const ValueId value_id : block.values) {
                if (!is_valid(value_id) || value_id.value >= module.values.size()) {
                    continue;
                }
                const ValueUseSite site{
                    ValueUseSiteKind::value_operand,
                    function_id,
                    block_id,
                    value_id,
                    INVALID_GLOBAL_CONSTANT_ID,
                };
                record_value_operands(analysis, module.values[value_id.value], site);
            }

            const ValueUseSite terminator_site{
                ValueUseSiteKind::terminator_operand,
                function_id,
                block_id,
                INVALID_VALUE_ID,
                INVALID_GLOBAL_CONSTANT_ID,
            };
            record_use(analysis, block.terminator.condition, terminator_site);
            record_use(analysis, block.terminator.value, terminator_site);
        }
    }
    return analysis;
}

[[nodiscard]] std::vector<bool> reachable_blocks(const FunctionControlFlowGraph& graph)
{
    std::vector<bool> reachable(graph.blocks.size(), false);
    std::vector<BlockId> pending;
    pending.push_back(BlockId{0});
    while (!pending.empty()) {
        const BlockId current = pending.back();
        pending.pop_back();
        if (!is_valid(current) || current.value >= graph.blocks.size() || reachable[current.value]) {
            continue;
        }
        reachable[current.value] = true;
        for (const BlockId successor : graph.blocks[current.value].successors) {
            pending.push_back(successor);
        }
    }
    return reachable;
}

[[nodiscard]] std::vector<bool> initial_reachable_dominators(
    const std::vector<bool>& reachable, const base::usize block_count, const base::u32 block_index)
{
    std::vector<bool> row(block_count, false);
    if (!reachable[block_index]) {
        row[block_index] = true;
        return row;
    }
    if (block_index == 0) {
        row[block_index] = true;
        return row;
    }
    for (base::u32 candidate = 0; candidate < block_count; ++candidate) {
        row[candidate] = reachable[candidate];
    }
    return row;
}

[[nodiscard]] std::vector<bool> intersect_predecessor_dominators(const FunctionControlFlowGraph& graph,
    const std::vector<bool>& reachable, const DominatorBits& dominators, const base::u32 block_index)
{
    std::vector<bool> row(graph.blocks.size(), false);
    bool has_reachable_predecessor = false;
    for (const BlockId predecessor : graph.blocks[block_index].predecessors) {
        if (!is_valid(predecessor) || predecessor.value >= graph.blocks.size() || !reachable[predecessor.value]) {
            continue;
        }
        if (!has_reachable_predecessor) {
            row = dominators[predecessor.value];
            has_reachable_predecessor = true;
            continue;
        }
        for (base::u32 candidate = 0; candidate < row.size(); ++candidate) {
            row[candidate] = row[candidate] && dominators[predecessor.value][candidate];
        }
    }
    row[block_index] = true;
    return row;
}

[[nodiscard]] FunctionDominanceAnalysis build_function_dominance(const FunctionControlFlowGraph& graph)
{
    FunctionDominanceAnalysis analysis;
    analysis.function = graph.function;
    analysis.dominators.resize(graph.blocks.size());
    if (graph.blocks.empty()) {
        return analysis;
    }

    const std::vector<bool> reachable = reachable_blocks(graph);
    DominatorBits dominator_bits;
    dominator_bits.reserve(graph.blocks.size());
    for (base::u32 block_index = 0; block_index < graph.blocks.size(); ++block_index) {
        dominator_bits.push_back(initial_reachable_dominators(reachable, graph.blocks.size(), block_index));
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (base::u32 block_index = 1; block_index < graph.blocks.size(); ++block_index) {
            if (!reachable[block_index]) {
                continue;
            }
            std::vector<bool> next = intersect_predecessor_dominators(graph, reachable, dominator_bits, block_index);
            if (next != dominator_bits[block_index]) {
                dominator_bits[block_index] = std::move(next);
                changed = true;
            }
        }
    }

    for (base::u32 block_index = 0; block_index < dominator_bits.size(); ++block_index) {
        std::vector<BlockId>& dominators = analysis.dominators[block_index];
        for (base::u32 candidate = 0; candidate < dominator_bits[block_index].size(); ++candidate) {
            if (dominator_bits[block_index][candidate]) {
                dominators.push_back(BlockId{candidate});
            }
        }
        std::sort(dominators.begin(), dominators.end(), block_id_less);
    }
    return analysis;
}

[[nodiscard]] DominanceAnalysis build_dominance(const ControlFlowGraphAnalysis& control_flow_graph)
{
    DominanceAnalysis analysis;
    analysis.functions.reserve(control_flow_graph.functions.size());
    for (const FunctionControlFlowGraph& graph : control_flow_graph.functions) {
        analysis.functions.push_back(build_function_dominance(graph));
    }
    return analysis;
}

} // namespace

const FunctionControlFlowGraph* ControlFlowGraphAnalysis::find_function(const FunctionId function) const noexcept
{
    if (!is_valid(function) || function.value >= this->functions.size()) {
        return nullptr;
    }
    return &this->functions[function.value];
}

std::span<const ValueUseSite> ValueUseAnalysis::uses(const ValueId value) const noexcept
{
    if (!is_valid(value) || value.value >= this->values.size()) {
        return {};
    }
    return std::span<const ValueUseSite>(this->values[value.value].uses.data(), this->values[value.value].uses.size());
}

const FunctionDominanceAnalysis* DominanceAnalysis::find_function(const FunctionId function) const noexcept
{
    if (!is_valid(function) || function.value >= this->functions.size()) {
        return nullptr;
    }
    return &this->functions[function.value];
}

bool DominanceAnalysis::dominates(
    const FunctionId function, const BlockId dominator, const BlockId block) const noexcept
{
    const FunctionDominanceAnalysis* function_analysis = this->find_function(function);
    if (function_analysis == nullptr || !is_valid(dominator) || !is_valid(block)
        || block.value >= function_analysis->dominators.size()) {
        return false;
    }
    const std::vector<BlockId>& dominators = function_analysis->dominators[block.value];
    return contains_block_id(dominators, dominator);
}

void ModuleAnalysisManager::clear() noexcept
{
    this->module_ = nullptr;
    this->control_flow_graph_.reset();
    this->value_uses_.reset();
    this->dominance_.reset();
}

void ModuleAnalysisManager::invalidate(const PreservedAnalyses& preserved_analyses) noexcept
{
    if (preserved_analyses.preserves_all()) {
        return;
    }
    if (!preserved_analyses.preserves(AnalysisId::control_flow_graph)) {
        this->control_flow_graph_.reset();
        this->dominance_.reset();
    } else if (!preserved_analyses.preserves(AnalysisId::dominance)) {
        this->dominance_.reset();
    }
    if (!preserved_analyses.preserves(AnalysisId::value_uses)) {
        this->value_uses_.reset();
    }
}

bool ModuleAnalysisManager::is_cached(const AnalysisId analysis) const noexcept
{
    switch (analysis) {
        case AnalysisId::control_flow_graph:
            return this->control_flow_graph_.has_value();
        case AnalysisId::dominance:
            return this->dominance_.has_value();
        case AnalysisId::value_uses:
            return this->value_uses_.has_value();
        case AnalysisId::type_table:
        case AnalysisId::symbol_table:
        case AnalysisId::record_layouts:
            return false;
    }
    return false;
}

PreservedAnalyses ModuleAnalysisManager::cached_analyses() const noexcept
{
    PreservedAnalyses cached = PreservedAnalyses::none();
    if (this->control_flow_graph_.has_value()) {
        cached.preserve(AnalysisId::control_flow_graph);
    }
    if (this->dominance_.has_value()) {
        cached.preserve(AnalysisId::dominance);
    }
    if (this->value_uses_.has_value()) {
        cached.preserve(AnalysisId::value_uses);
    }
    return cached;
}

const ControlFlowGraphAnalysis& ModuleAnalysisManager::control_flow_graph(const Module& module)
{
    this->ensure_module(module);
    if (!this->control_flow_graph_.has_value()) {
        this->control_flow_graph_ = build_control_flow_graph(module);
    }
    return *this->control_flow_graph_;
}

const ValueUseAnalysis& ModuleAnalysisManager::value_uses(const Module& module)
{
    this->ensure_module(module);
    if (!this->value_uses_.has_value()) {
        this->value_uses_ = build_value_uses(module);
    }
    return *this->value_uses_;
}

const DominanceAnalysis& ModuleAnalysisManager::dominance(const Module& module)
{
    this->ensure_module(module);
    if (!this->dominance_.has_value()) {
        this->dominance_ = build_dominance(this->control_flow_graph(module));
    }
    return *this->dominance_;
}

void ModuleAnalysisManager::ensure_module(const Module& module)
{
    if (this->module_ == &module) {
        return;
    }
    this->clear();
    this->module_ = &module;
}

} // namespace aurex::ir
