#include <aurex/midend/ir/ir_owned_dyn_ir_shape_prototype_gate.hpp>

#include <algorithm>
#include <unordered_set>

namespace aurex::ir {
namespace {

constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_M20B_SUBJECT =
    "M20b Owned Dyn IR Shape Prototype Gate";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_HANDLE_FACT =
    "owned_dyn_handle_metadata_ir_shape_fact";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_DATA_FACT =
    "owned_dyn_erased_payload_pointer_ir_shape_fact";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_VTABLE_FACT =
    "owned_dyn_vtable_pointer_ir_shape_fact";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_DROP_FACT =
    "owned_dyn_drop_identity_placeholder_ir_shape_fact";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_ALLOCATOR_FACT =
    "owned_dyn_allocator_identity_placeholder_ir_shape_fact";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_RUNTIME_FACT =
    "owned_dyn_runtime_lowering_blocker_ir_shape_fact";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_M20A_GATE =
    "requires_m20a_owned_dyn_runtime_admission_gate";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_VERIFIER_SHAPE =
    "verifier_requires_compiler_owned_two_field_shape";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_BORROWED_ABI =
    "borrowed_dyn_abi_remains_destructor_free";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_NO_BOX =
    "box_dyn_trait_not_in_m20b";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_NO_OWNING_VALUE =
    "owning_dyn_user_value_not_in_m20b";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_NO_ALLOCATOR =
    "allocator_api_not_in_m20b";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_NO_RUNTIME =
    "runtime_abi_lowering_not_in_m20b";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_NO_DYNAMIC_DROP =
    "dynamic_drop_runtime_not_in_m20b";
constexpr std::string_view IR_OWNED_DYN_IR_SHAPE_NO_BACKEND =
    "backend_runtime_helper_not_in_m20b";

[[nodiscard]] std::string_view safe_symbol(
    const Module& module,
    const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    return module.has_text(prototype.symbol) ? module.text(prototype.symbol) : std::string_view{};
}

[[nodiscard]] bool is_pointer_to_builtin(
    const Module& module,
    const sema::TypeHandle type,
    const sema::PointerMutability mutability,
    const sema::BuiltinType pointee) noexcept
{
    if (!module.types.is_pointer(type)) {
        return false;
    }
    const sema::TypeInfo& pointer = module.types.get(type);
    return pointer.pointer_mutability == mutability
        && module.types.same(pointer.pointee, module.types.builtin(pointee));
}

[[nodiscard]] bool all_prototypes_satisfy(
    const Module& module,
    bool (*predicate)(const Module&, const OwnedDynObjectLayoutPrototype&)) noexcept
{
    return !module.owned_dyn_object_layout_prototypes.empty()
        && std::ranges::all_of(module.owned_dyn_object_layout_prototypes,
            [&](const OwnedDynObjectLayoutPrototype& prototype) {
                return predicate(module, prototype);
            });
}

[[nodiscard]] bool prototype_has_two_field_handle(
    const Module& module,
    const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    static_cast<void>(module);
    return prototype.handle_field_count == IR_OWNED_DYN_OBJECT_HANDLE_FIELD_COUNT
        && prototype.data_pointer_field_index == IR_OWNED_DYN_OBJECT_DATA_POINTER_FIELD
        && prototype.vtable_pointer_field_index == IR_OWNED_DYN_OBJECT_VTABLE_POINTER_FIELD;
}

[[nodiscard]] bool prototype_has_payload_pointer(
    const Module& module,
    const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    return is_pointer_to_builtin(
        module,
        prototype.data_pointer_type,
        sema::PointerMutability::mut,
        sema::BuiltinType::u8);
}

[[nodiscard]] bool prototype_has_vtable_pointer(
    const Module& module,
    const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    return is_pointer_to_builtin(
        module,
        prototype.vtable_pointer_type,
        sema::PointerMutability::const_,
        sema::BuiltinType::u8);
}

[[nodiscard]] bool prototype_has_drop_placeholder(
    const Module& module,
    const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    static_cast<void>(module);
    return prototype.erased_drop_runtime_slot == IR_OWNED_DYN_OBJECT_RUNTIME_SLOT_BLOCKED;
}

[[nodiscard]] bool prototype_has_allocator_placeholder(
    const Module& module,
    const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    static_cast<void>(module);
    return prototype.allocator_runtime_slot == IR_OWNED_DYN_OBJECT_RUNTIME_SLOT_BLOCKED;
}

[[nodiscard]] bool identity_key_is_valid(
    const query::StableFingerprint128 drop_key,
    const query::StableFingerprint128 allocator_key) noexcept
{
    return drop_key != query::StableFingerprint128{}
        && allocator_key != query::StableFingerprint128{}
        && drop_key != allocator_key;
}

[[nodiscard]] bool prototype_keeps_blockers(
    const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    return prototype.compiler_owned
        && prototype.borrowed_abi_unchanged
        && prototype.standard_library_blocked
        && prototype.box_surface_blocked
        && prototype.owning_dyn_user_value_blocked
        && prototype.allocator_api_blocked
        && prototype.runtime_lowering_blocked
        && prototype.dynamic_drop_runtime_blocked
        && prototype.backend_helper_blocked;
}

[[nodiscard]] bool prototype_identity_is_valid(
    const Module& module,
    const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    return query::is_valid(prototype.object_type_key)
        && is_valid(prototype.policy)
        && sema::is_valid(prototype.object_type)
        && prototype.object_type.value < module.types.size()
        && module.types.is_trait_object(prototype.object_type)
        && module.types.get(prototype.object_type).trait_object_principal_types.empty()
        && module.types.get(prototype.object_type).trait_object_key == prototype.object_type_key
        && module.has_text(prototype.symbol)
        && !module.text(prototype.symbol).empty()
        && identity_key_is_valid(
            prototype.erased_drop_identity_key,
            prototype.allocator_identity_key);
}

[[nodiscard]] bool all_prototype_identities_are_valid(const Module& module)
{
    if (module.owned_dyn_object_layout_prototypes.empty()) {
        return false;
    }
    std::unordered_set<base::u64> object_keys;
    object_keys.reserve(module.owned_dyn_object_layout_prototypes.size());
    std::unordered_set<IrTextId, sema::IdentIdHash> symbols;
    symbols.reserve(module.owned_dyn_object_layout_prototypes.size());
    std::unordered_set<query::StableFingerprint128, query::StableFingerprintHash> drop_keys;
    drop_keys.reserve(module.owned_dyn_object_layout_prototypes.size());
    std::unordered_set<query::StableFingerprint128, query::StableFingerprintHash> allocator_keys;
    allocator_keys.reserve(module.owned_dyn_object_layout_prototypes.size());
    return std::ranges::all_of(module.owned_dyn_object_layout_prototypes,
        [&](const OwnedDynObjectLayoutPrototype& prototype) {
            return prototype_identity_is_valid(module, prototype)
                && object_keys.insert(prototype.object_type_key.global_id).second
                && symbols.insert(prototype.symbol).second
                && drop_keys.insert(prototype.erased_drop_identity_key).second
                && allocator_keys.insert(prototype.allocator_identity_key).second;
        });
}

[[nodiscard]] bool all_prototypes_keep_blockers(const Module& module) noexcept
{
    return !module.owned_dyn_object_layout_prototypes.empty()
        && std::ranges::all_of(module.owned_dyn_object_layout_prototypes, prototype_keeps_blockers);
}

[[nodiscard]] query::OwnedDynIrShapePrototypeFact make_fact(
    const Module& module,
    const query::OwnedDynIrShapePrototypeFactKind kind,
    const query::OwnedDynIrShapePrototypeStage stage,
    const query::OwnedDynIrShapePrototypePolicy policy,
    const std::string_view fact_name,
    const std::string_view verifier_guard,
    const std::string_view blocked_surface)
{
    const OwnedDynObjectLayoutPrototype& first = module.owned_dyn_object_layout_prototypes.front();
    const bool identities_valid = all_prototype_identities_are_valid(module);
    const bool blockers_intact = identities_valid && all_prototypes_keep_blockers(module);
    query::OwnedDynIrShapePrototypeFact fact;
    fact.fact_name = std::string(fact_name);
    fact.kind = kind;
    fact.stage = stage;
    fact.policy = policy;
    fact.references_m20a_admission_gate = true;
    fact.compiler_owned_ir_shape = blockers_intact;
    fact.owned_layout_prototype_visible = !module.owned_dyn_object_layout_prototypes.empty();
    fact.handle_metadata_visible =
        kind == query::OwnedDynIrShapePrototypeFactKind::owned_handle_metadata
        && all_prototypes_satisfy(module, prototype_has_two_field_handle);
    fact.erased_payload_pointer_visible =
        kind == query::OwnedDynIrShapePrototypeFactKind::erased_payload_pointer
        && all_prototypes_satisfy(module, prototype_has_payload_pointer);
    fact.vtable_pointer_visible =
        kind == query::OwnedDynIrShapePrototypeFactKind::vtable_pointer_metadata
        && all_prototypes_satisfy(module, prototype_has_vtable_pointer);
    fact.drop_identity_placeholder_visible =
        kind == query::OwnedDynIrShapePrototypeFactKind::drop_identity_placeholder
        && all_prototypes_satisfy(module, prototype_has_drop_placeholder);
    fact.allocator_identity_placeholder_visible =
        kind == query::OwnedDynIrShapePrototypeFactKind::allocator_identity_placeholder
        && all_prototypes_satisfy(module, prototype_has_allocator_placeholder);
    fact.borrowed_dyn_abi_unchanged = blockers_intact;
    fact.standard_library_api_blocked = blockers_intact;
    fact.box_dyn_surface_blocked = blockers_intact;
    fact.owning_dyn_user_value_blocked = blockers_intact;
    fact.allocator_api_blocked = blockers_intact;
    fact.runtime_lowering_blocked = blockers_intact;
    fact.dynamic_drop_runtime_blocked = blockers_intact;
    fact.backend_helper_blocked = blockers_intact;
    fact.executable_runtime_implemented = false;
    fact.object_type = first.object_type_key;
    fact.layout_prototype_count =
        static_cast<base::u64>(module.owned_dyn_object_layout_prototypes.size());
    fact.handle_field_count = all_prototypes_satisfy(module, prototype_has_two_field_handle)
        ? query::QUERY_OWNED_DYN_IR_SHAPE_HANDLE_FIELD_COUNT
        : first.handle_field_count;
    fact.subject_symbol = identities_valid ? std::string(safe_symbol(module, first)) : std::string{};
    fact.m20a_gate_fact = std::string(IR_OWNED_DYN_IR_SHAPE_M20A_GATE);
    fact.verifier_guard_fact = std::string(verifier_guard);
    fact.blocked_surface_fact = std::string(blocked_surface);
    return fact;
}

void record_module_shape_facts(query::OwnedDynIrShapePrototypeGate& gate, const Module& module)
{
    if (module.owned_dyn_object_layout_prototypes.empty()) {
        return;
    }
    query::record_owned_dyn_ir_shape_prototype_fact(gate,
        make_fact(module,
            query::OwnedDynIrShapePrototypeFactKind::owned_handle_metadata,
            query::OwnedDynIrShapePrototypeStage::ir_shape_prototype,
            query::OwnedDynIrShapePrototypePolicy::owned_handle_two_field_v1,
            IR_OWNED_DYN_IR_SHAPE_HANDLE_FACT,
            IR_OWNED_DYN_IR_SHAPE_VERIFIER_SHAPE,
            IR_OWNED_DYN_IR_SHAPE_NO_OWNING_VALUE));
    query::record_owned_dyn_ir_shape_prototype_fact(gate,
        make_fact(module,
            query::OwnedDynIrShapePrototypeFactKind::erased_payload_pointer,
            query::OwnedDynIrShapePrototypeStage::ir_shape_prototype,
            query::OwnedDynIrShapePrototypePolicy::erased_payload_pointer_v1,
            IR_OWNED_DYN_IR_SHAPE_DATA_FACT,
            "verifier_requires_mut_u8_erased_payload_pointer",
            IR_OWNED_DYN_IR_SHAPE_NO_BOX));
    query::record_owned_dyn_ir_shape_prototype_fact(gate,
        make_fact(module,
            query::OwnedDynIrShapePrototypeFactKind::vtable_pointer_metadata,
            query::OwnedDynIrShapePrototypeStage::verifier_shape_guard,
            query::OwnedDynIrShapePrototypePolicy::borrowed_vtable_pointer_unchanged_v1,
            IR_OWNED_DYN_IR_SHAPE_VTABLE_FACT,
            IR_OWNED_DYN_IR_SHAPE_BORROWED_ABI,
            IR_OWNED_DYN_IR_SHAPE_NO_BACKEND));
    query::record_owned_dyn_ir_shape_prototype_fact(gate,
        make_fact(module,
            query::OwnedDynIrShapePrototypeFactKind::drop_identity_placeholder,
            query::OwnedDynIrShapePrototypeStage::blocked_future_runtime,
            query::OwnedDynIrShapePrototypePolicy::drop_identity_not_lowered_v1,
            IR_OWNED_DYN_IR_SHAPE_DROP_FACT,
            "verifier_keeps_erased_drop_identity_as_placeholder",
            IR_OWNED_DYN_IR_SHAPE_NO_DYNAMIC_DROP));
    query::record_owned_dyn_ir_shape_prototype_fact(gate,
        make_fact(module,
            query::OwnedDynIrShapePrototypeFactKind::allocator_identity_placeholder,
            query::OwnedDynIrShapePrototypeStage::blocked_future_runtime,
            query::OwnedDynIrShapePrototypePolicy::allocator_identity_not_lowered_v1,
            IR_OWNED_DYN_IR_SHAPE_ALLOCATOR_FACT,
            "verifier_keeps_allocator_identity_as_placeholder",
            IR_OWNED_DYN_IR_SHAPE_NO_ALLOCATOR));
    query::record_owned_dyn_ir_shape_prototype_fact(gate,
        make_fact(module,
            query::OwnedDynIrShapePrototypeFactKind::runtime_lowering_blocker,
            query::OwnedDynIrShapePrototypeStage::blocked_future_runtime,
            query::OwnedDynIrShapePrototypePolicy::runtime_lowering_not_implemented_v1,
            IR_OWNED_DYN_IR_SHAPE_RUNTIME_FACT,
            "verifier_keeps_owned_dyn_runtime_lowering_blocked",
            IR_OWNED_DYN_IR_SHAPE_NO_RUNTIME));
}

} // namespace

query::OwnedDynIrShapePrototypeGate owned_dyn_ir_shape_prototype_gate(const Module& module)
{
    query::OwnedDynIrShapePrototypeGate gate;
    gate.subject = std::string(IR_OWNED_DYN_IR_SHAPE_M20B_SUBJECT);
    gate.admission_gate = query::m20_owned_dyn_runtime_admission_gate_baseline();
    gate.admission_gate_fingerprint = gate.admission_gate.fingerprint;
    record_module_shape_facts(gate, module);
    gate.summary = query::summarize_owned_dyn_ir_shape_prototype_gate_counts(gate);
    gate.fingerprint = query::owned_dyn_ir_shape_prototype_gate_fingerprint(gate);
    return gate;
}

} // namespace aurex::ir
