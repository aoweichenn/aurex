#include <aurex/infrastructure/query/stable_hash.hpp>
#include <aurex/midend/ir/ir_fingerprint.hpp>
#include <aurex/midend/ir/ir_value_closure.hpp>

#include <string>
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
    builder.mix_fingerprint(info.trait_object_principal_set_identity);
    builder.mix_u64(static_cast<base::u64>(info.trait_object_principal_types.size()));
    for (const sema::TypeHandle principal : info.trait_object_principal_types) {
        builder.mix_u32(principal.value);
        if (sema::is_valid(principal) && principal.value < module.types.size()) {
            builder.mix_string(module.types.display_name(principal));
        }
    }

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

void mix_trait_object_vtable_layout(
    query::StableHashBuilder& builder, const Module& module, const TraitObjectVTableLayout& layout)
{
    builder.mix_u64(layout.layout_key.global_id);
    mix_type(builder, module, layout.concrete_type);
    mix_type(builder, module, layout.object_type);
    mix_text(builder, module, layout.symbol);
    builder.mix_bool(layout.destructor_slot_blocked);
    builder.mix_u64(static_cast<base::u64>(layout.method_slots.size()));
    for (const TraitObjectVTableMethodSlot& slot : layout.method_slots) {
        builder.mix_u32(slot.slot);
        mix_function_id(builder, module, slot.function);
        mix_type(builder, module, slot.function_type);
        mix_type(builder, module, slot.receiver_type);
        mix_type(builder, module, slot.return_type);
        mix_text(builder, module, slot.method_name);
    }
    builder.mix_u64(static_cast<base::u64>(layout.supertrait_edges.size()));
    for (const TraitObjectVTableSupertraitEdge& edge : layout.supertrait_edges) {
        builder.mix_u32(edge.edge_index);
        builder.mix_u64(edge.upcast_key.global_id);
        builder.mix_fingerprint(edge.edge_fingerprint);
        builder.mix_u64(edge.source_layout.global_id);
        builder.mix_u64(edge.target_layout.global_id);
        mix_type(builder, module, edge.source_reference_type);
        mix_type(builder, module, edge.target_reference_type);
        mix_type(builder, module, edge.source_object_type);
        mix_type(builder, module, edge.target_object_type);
        builder.mix_u8(static_cast<base::u8>(edge.borrow_kind));
    }
}

void mix_principal_set_metadata_layout(
    query::StableHashBuilder& builder, const Module& module, const PrincipalSetMetadataLayout& layout)
{
    builder.mix_fingerprint(layout.principal_set_identity);
    builder.mix_u8(static_cast<base::u8>(layout.metadata_policy));
    mix_type(builder, module, layout.concrete_type);
    mix_type(builder, module, layout.object_type);
    mix_text(builder, module, layout.symbol);
    builder.mix_u64(static_cast<base::u64>(layout.witnesses.size()));
    for (const PrincipalSetMetadataWitness& witness : layout.witnesses) {
        builder.mix_u32(witness.principal_index);
        builder.mix_u64(witness.principal_object.global_id);
        builder.mix_fingerprint(query::stable_key_fingerprint(witness.principal_object));
        builder.mix_u64(witness.vtable_layout.global_id);
        builder.mix_fingerprint(query::stable_key_fingerprint(witness.vtable_layout));
        mix_type(builder, module, witness.object_type);
    }
}

void mix_owned_dyn_object_layout_prototype(
    query::StableHashBuilder& builder,
    const Module& module,
    const OwnedDynObjectLayoutPrototype& prototype)
{
    builder.mix_u64(prototype.object_type_key.global_id);
    builder.mix_fingerprint(query::stable_key_fingerprint(prototype.object_type_key));
    mix_type(builder, module, prototype.object_type);
    mix_type(builder, module, prototype.data_pointer_type);
    mix_type(builder, module, prototype.vtable_pointer_type);
    mix_text(builder, module, prototype.symbol);
    builder.mix_u8(static_cast<base::u8>(prototype.policy));
    builder.mix_u32(prototype.handle_field_count);
    builder.mix_u32(prototype.data_pointer_field_index);
    builder.mix_u32(prototype.vtable_pointer_field_index);
    builder.mix_u32(prototype.erased_drop_runtime_slot);
    builder.mix_u32(prototype.allocator_runtime_slot);
    builder.mix_bool(prototype.compiler_owned);
    builder.mix_bool(prototype.borrowed_abi_unchanged);
    builder.mix_bool(prototype.standard_library_blocked);
    builder.mix_bool(prototype.box_surface_blocked);
    builder.mix_bool(prototype.owning_dyn_user_value_blocked);
    builder.mix_bool(prototype.allocator_api_blocked);
    builder.mix_bool(prototype.runtime_lowering_blocked);
    builder.mix_bool(prototype.dynamic_drop_runtime_blocked);
    builder.mix_bool(prototype.backend_helper_blocked);
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
    builder.mix_u64(value.vtable_layout.global_id);
    builder.mix_u64(value.target_vtable_layout.global_id);
    builder.mix_u64(value.upcast_key.global_id);
    builder.mix_fingerprint(value.principal_set_identity);
    builder.mix_u64(value.principal_object.global_id);
    builder.mix_fingerprint(query::stable_key_fingerprint(value.principal_object));
    builder.mix_u32(value.principal_index);
    builder.mix_u32(value.vtable_slot);
    builder.mix_u32(value.vtable_supertrait_edge);

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
    builder.mix_u64(module.trait_object_vtables.size());
    for (const TraitObjectVTableLayout& layout : module.trait_object_vtables) {
        mix_trait_object_vtable_layout(builder, module, layout);
    }
    builder.mix_u64(module.principal_set_metadata_layouts.size());
    for (const PrincipalSetMetadataLayout& layout : module.principal_set_metadata_layouts) {
        mix_principal_set_metadata_layout(builder, module, layout);
    }
    builder.mix_u64(module.owned_dyn_object_layout_prototypes.size());
    for (const OwnedDynObjectLayoutPrototype& prototype : module.owned_dyn_object_layout_prototypes) {
        mix_owned_dyn_object_layout_prototype(builder, module, prototype);
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
