#include <aurex/infrastructure/query/stable_hash.hpp>
#include <aurex/midend/ir/ir_fingerprint.hpp>

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

namespace aurex::ir {
namespace {

constexpr std::string_view IR_LAYOUT_ABI_FINGERPRINT_MARKER = "ir-layout-abi:v1";
constexpr std::string_view IR_FUNCTION_UNIT_FINGERPRINT_MARKER = "ir-function-unit:v1";
constexpr std::string_view IR_LLVM_EMISSION_UNIT_FINGERPRINT_MARKER = "ir-llvm-emission-unit:v1";
constexpr std::string_view IR_FINGERPRINT_INVALID_TEXT = "<invalid>";
constexpr std::string_view IR_FINGERPRINT_INVALID_TYPE = "<invalid-type>";
constexpr std::string_view IR_FINGERPRINT_MISSING_FUNCTION = "<missing-function>";
constexpr std::string_view IR_FINGERPRINT_MISSING_CONSTANT = "<missing-constant>";

[[nodiscard]] std::string_view safe_text(const Module& module, const IrTextId id) noexcept
{
    return module.has_text(id) ? module.text(id) : IR_FINGERPRINT_INVALID_TEXT;
}

void mix_text(query::StableHashBuilder& builder, const Module& module, const IrTextId id) noexcept
{
    builder.mix_string(safe_text(module, id));
}

void mix_value_id(query::StableHashBuilder& builder, const ValueId id) noexcept
{
    builder.mix_u32(id.value);
}

void mix_block_id(query::StableHashBuilder& builder, const BlockId id) noexcept
{
    builder.mix_u32(id.value);
}

void mix_function_id(query::StableHashBuilder& builder, const Module& module, const FunctionId id)
{
    builder.mix_u32(id.value);
    if (!is_valid(id) || id.value >= module.functions.size()) {
        builder.mix_string(IR_FINGERPRINT_MISSING_FUNCTION);
        return;
    }
    mix_text(builder, module, module.functions[id.value].symbol);
}

void mix_type(query::StableHashBuilder& builder, const Module& module, const sema::TypeHandle type)
{
    builder.mix_u32(type.value);
    if (!sema::is_valid(type) || type.value >= module.types.size()) {
        builder.mix_string(IR_FINGERPRINT_INVALID_TYPE);
        return;
    }

    const sema::TypeInfo& info = module.types.get(type);
    builder.mix_u8(static_cast<base::u8>(info.kind));
    builder.mix_string(module.types.display_name(type));
    builder.mix_string(module.types.c_name(type));
    builder.mix_u8(static_cast<base::u8>(info.builtin));
    builder.mix_u8(static_cast<base::u8>(info.pointer_mutability));
    builder.mix_u32(info.pointee.value);
    builder.mix_u64(info.array_count);
    builder.mix_u32(info.array_element.value);
    builder.mix_u8(static_cast<base::u8>(info.slice_mutability));
    builder.mix_u32(info.slice_element.value);
    builder.mix_u8(static_cast<base::u8>(info.function_call_conv));
    builder.mix_bool(info.function_is_unsafe);
    builder.mix_bool(info.function_is_variadic);
    builder.mix_u32(info.function_return.value);
    builder.mix_u32(info.enum_underlying.value);
    builder.mix_u32(info.enum_payload_storage.value);
    builder.mix_u64(info.enum_payload_size);
    builder.mix_u64(info.enum_payload_align);
    builder.mix_bool(info.contains_array);
    builder.mix_string(info.name.view());
    builder.mix_string(info.c_name.view());
    builder.mix_string(info.generic_origin_key.view());

    builder.mix_u64(info.tuple_elements.size());
    for (const sema::TypeHandle element : info.tuple_elements) {
        builder.mix_string(module.types.display_name(element));
        builder.mix_string(module.types.c_name(element));
    }

    builder.mix_u64(info.function_params.size());
    for (const sema::TypeHandle param : info.function_params) {
        builder.mix_string(module.types.display_name(param));
        builder.mix_string(module.types.c_name(param));
    }

    builder.mix_u64(info.generic_args.size());
    for (const sema::TypeHandle arg : info.generic_args) {
        builder.mix_string(module.types.display_name(arg));
        builder.mix_string(module.types.c_name(arg));
    }
}

void mix_record_field(query::StableHashBuilder& builder, const Module& module, const RecordField& field)
{
    mix_text(builder, module, field.name);
    mix_type(builder, module, field.type);
}

void mix_record_layout(query::StableHashBuilder& builder, const Module& module, const RecordLayout& record)
{
    mix_type(builder, module, record.type);
    mix_text(builder, module, record.name);
    mix_text(builder, module, record.symbol);
    builder.mix_bool(record.is_opaque);
    builder.mix_u64(record.fields.size());
    for (const RecordField& field : record.fields) {
        mix_record_field(builder, module, field);
    }
}

void mix_global_constant(query::StableHashBuilder& builder, const Module& module, const GlobalConstant& constant)
{
    mix_text(builder, module, constant.name);
    mix_text(builder, module, constant.symbol);
    mix_type(builder, module, constant.type);
    mix_value_id(builder, constant.initializer);
}

void mix_function_signature(query::StableHashBuilder& builder, const Module& module, const Function& function)
{
    mix_text(builder, module, function.name);
    mix_text(builder, module, function.symbol);
    builder.mix_u8(static_cast<base::u8>(function.linkage));
    builder.mix_u8(static_cast<base::u8>(function.call_conv));
    builder.mix_bool(function.is_entry);
    builder.mix_bool(function.is_unsafe);
    builder.mix_bool(function.is_variadic);
    mix_type(builder, module, function.return_type);
    builder.mix_u64(function.signature_params.size());
    for (const FunctionParam& param : function.signature_params) {
        mix_text(builder, module, param.name);
        mix_type(builder, module, param.type);
    }
}

void push_value_id(std::vector<ValueId>& pending, const ValueId id)
{
    if (is_valid(id)) {
        pending.push_back(id);
    }
}

void push_value_references(std::vector<ValueId>& pending, const Value& value)
{
    push_value_id(pending, value.lhs);
    push_value_id(pending, value.rhs);
    push_value_id(pending, value.object);
    push_value_id(pending, value.index);
    for (const ValueId arg : value.args) {
        push_value_id(pending, arg);
    }
    for (const FieldValue& field : value.fields) {
        push_value_id(pending, field.value);
    }
    for (const PhiInput& incoming : value.incoming) {
        push_value_id(pending, incoming.value);
    }
    for (const ValueId element : value.elements) {
        push_value_id(pending, element);
    }
}

void push_terminator_values(std::vector<ValueId>& pending, const Terminator& terminator)
{
    push_value_id(pending, terminator.condition);
    push_value_id(pending, terminator.value);
}

[[nodiscard]] std::vector<ValueId> collect_function_value_closure(const Module& module, const Function& function)
{
    std::vector<ValueId> pending;
    pending.reserve(function.param_values.size() + function.blocks.size());
    for (const ValueId param : function.param_values) {
        push_value_id(pending, param);
    }
    for (const BasicBlock& block : function.blocks) {
        for (const ValueId value_id : block.values) {
            push_value_id(pending, value_id);
        }
        push_terminator_values(pending, block.terminator);
    }

    std::unordered_set<base::u32> seen;
    seen.reserve(pending.size());
    std::vector<ValueId> values;
    while (!pending.empty()) {
        const ValueId id = pending.back();
        pending.pop_back();
        if (!is_valid(id) || id.value >= module.values.size() || !seen.insert(id.value).second) {
            continue;
        }
        values.push_back(id);
        const Value& value = module.values[id.value];
        push_value_references(pending, value);
        if (is_valid(value.constant) && value.constant.value < module.constants.size()) {
            push_value_id(pending, module.constants[value.constant.value].initializer);
        }
    }

    std::sort(values.begin(), values.end(), [](const ValueId lhs, const ValueId rhs) noexcept {
        return lhs.value < rhs.value;
    });
    return values;
}

void mix_field_value(query::StableHashBuilder& builder, const Module& module, const FieldValue& field)
{
    mix_text(builder, module, field.name);
    mix_value_id(builder, field.value);
}

void mix_phi_input(query::StableHashBuilder& builder, const PhiInput& incoming) noexcept
{
    mix_block_id(builder, incoming.predecessor);
    mix_value_id(builder, incoming.value);
}

void mix_value(query::StableHashBuilder& builder, const Module& module, const ValueId id, const Value& value)
{
    mix_value_id(builder, id);
    builder.mix_u8(static_cast<base::u8>(value.kind));
    mix_type(builder, module, value.type);
    mix_text(builder, module, value.name);
    mix_text(builder, module, value.text);
    mix_function_id(builder, module, value.call_target);
    mix_value_id(builder, value.lhs);
    mix_value_id(builder, value.rhs);
    mix_value_id(builder, value.object);
    mix_value_id(builder, value.index);
    if (is_valid(value.constant) && value.constant.value < module.constants.size()) {
        mix_global_constant(builder, module, module.constants[value.constant.value]);
    } else {
        builder.mix_string(IR_FINGERPRINT_MISSING_CONSTANT);
        builder.mix_u32(value.constant.value);
    }
    builder.mix_u8(static_cast<base::u8>(value.unary_op));
    builder.mix_u8(static_cast<base::u8>(value.binary_op));
    builder.mix_u8(static_cast<base::u8>(value.cast_kind));
    mix_type(builder, module, value.target_type);
    builder.mix_u8(static_cast<base::u8>(value.cleanup_policy));

    builder.mix_u64(value.args.size());
    for (const ValueId arg : value.args) {
        mix_value_id(builder, arg);
    }
    builder.mix_u64(value.fields.size());
    for (const FieldValue& field : value.fields) {
        mix_field_value(builder, module, field);
    }
    builder.mix_u64(value.incoming.size());
    for (const PhiInput& incoming : value.incoming) {
        mix_phi_input(builder, incoming);
    }
    builder.mix_u64(value.elements.size());
    for (const ValueId element : value.elements) {
        mix_value_id(builder, element);
    }
}

void mix_terminator(query::StableHashBuilder& builder, const Terminator& terminator) noexcept
{
    builder.mix_u8(static_cast<base::u8>(terminator.kind));
    mix_value_id(builder, terminator.condition);
    mix_value_id(builder, terminator.value);
    mix_block_id(builder, terminator.target);
    mix_block_id(builder, terminator.then_target);
    mix_block_id(builder, terminator.else_target);
}

void mix_function_body(query::StableHashBuilder& builder, const Module& module, const Function& function)
{
    builder.mix_u64(function.param_values.size());
    for (const ValueId param : function.param_values) {
        mix_value_id(builder, param);
    }

    builder.mix_u64(function.blocks.size());
    for (base::u32 block_index = 0; block_index < function.blocks.size(); ++block_index) {
        const BasicBlock& block = function.blocks[block_index];
        builder.mix_u32(block_index);
        mix_text(builder, module, block.name);
        builder.mix_u64(block.values.size());
        for (const ValueId value_id : block.values) {
            mix_value_id(builder, value_id);
        }
        mix_terminator(builder, block.terminator);
    }

    const std::vector<ValueId> values = collect_function_value_closure(module, function);
    builder.mix_u64(values.size());
    for (const ValueId value_id : values) {
        mix_value(builder, module, value_id, module.values[value_id.value]);
    }
}

[[nodiscard]] query::QueryResultFingerprint function_ir_unit_fingerprint_with_layout(
    const Module& module, const Function& function, const query::QueryResultFingerprint layout)
{
    query::StableHashBuilder builder;
    builder.mix_string(IR_FUNCTION_UNIT_FINGERPRINT_MARKER);
    mix_function_signature(builder, module, function);
    mix_function_body(builder, module, function);
    builder.mix_fingerprint(layout.fingerprint);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint llvm_emission_unit_fingerprint_with_layout(const Module& module,
    const Function& function, const query::QueryResultFingerprint layout,
    const query::QueryResultFingerprint target_independent_ir)
{
    query::StableHashBuilder builder;
    builder.mix_string(IR_LLVM_EMISSION_UNIT_FINGERPRINT_MARKER);
    mix_text(builder, module, function.symbol);
    builder.mix_fingerprint(target_independent_ir.fingerprint);
    builder.mix_fingerprint(layout.fingerprint);
    return query::query_result_fingerprint(builder.finish());
}

} // namespace

query::QueryResultFingerprint layout_abi_fingerprint(const Module& module)
{
    query::StableHashBuilder builder;
    builder.mix_string(IR_LAYOUT_ABI_FINGERPRINT_MARKER);
    builder.mix_u64(module.records.size());
    for (const RecordLayout& record : module.records) {
        mix_record_layout(builder, module, record);
    }
    builder.mix_u64(module.constants.size());
    for (const GlobalConstant& constant : module.constants) {
        mix_global_constant(builder, module, constant);
    }
    builder.mix_u64(module.functions.size());
    for (const Function& function : module.functions) {
        mix_function_signature(builder, module, function);
    }
    return query::query_result_fingerprint(builder.finish());
}

query::QueryResultFingerprint function_ir_unit_fingerprint(const Module& module, const Function& function)
{
    return function_ir_unit_fingerprint_with_layout(module, function, layout_abi_fingerprint(module));
}

query::QueryResultFingerprint llvm_emission_unit_fingerprint(const Module& module, const Function& function)
{
    const query::QueryResultFingerprint layout = layout_abi_fingerprint(module);
    const query::QueryResultFingerprint target_independent_ir =
        function_ir_unit_fingerprint_with_layout(module, function, layout);
    return llvm_emission_unit_fingerprint_with_layout(module, function, layout, target_independent_ir);
}

std::vector<FunctionIRUnitFingerprint> function_ir_unit_fingerprints(const Module& module)
{
    std::vector<FunctionIRUnitFingerprint> fingerprints;
    fingerprints.reserve(module.functions.size());
    const query::QueryResultFingerprint layout = layout_abi_fingerprint(module);
    for (const Function& function : module.functions) {
        const query::QueryResultFingerprint target_independent_ir =
            function_ir_unit_fingerprint_with_layout(module, function, layout);
        fingerprints.push_back(FunctionIRUnitFingerprint{
            std::string(safe_text(module, function.symbol)),
            target_independent_ir,
            llvm_emission_unit_fingerprint_with_layout(module, function, layout, target_independent_ir),
        });
    }
    return fingerprints;
}

std::optional<FunctionIRUnitFingerprint> function_ir_unit_fingerprint_by_symbol(
    const Module& module, const std::string_view symbol)
{
    for (const Function& function : module.functions) {
        if (safe_text(module, function.symbol) != symbol) {
            continue;
        }
        const query::QueryResultFingerprint layout = layout_abi_fingerprint(module);
        const query::QueryResultFingerprint target_independent_ir =
            function_ir_unit_fingerprint_with_layout(module, function, layout);
        return FunctionIRUnitFingerprint{
            std::string(symbol),
            target_independent_ir,
            llvm_emission_unit_fingerprint_with_layout(module, function, layout, target_independent_ir),
        };
    }
    return std::nullopt;
}

} // namespace aurex::ir
