#include <aurex/midend/ir/ir_owned_dyn_drop_allocator_identity_gate.hpp>
#include <aurex/midend/ir/ir_owned_dyn_ir_shape_prototype_gate.hpp>

#include <algorithm>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace aurex::ir {
namespace {

constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_M20C_SUBJECT =
    "M20c Drop / Allocator Identity Prerequisite Gate";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_DROP_FACT = "owned_dyn_erased_drop_identity_prerequisite_fact";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_ALLOCATOR_FACT =
    "owned_dyn_allocator_identity_prerequisite_fact";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_CLEANUP_FACT = "owned_dyn_cleanup_dropck_bridge_identity_fact";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_BINDING_FACT = "owned_dyn_handle_identity_binding_fact";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_RUNTIME_FACT =
    "owned_dyn_drop_allocator_runtime_lowering_blocker_fact";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_M20B_GATE = "requires_m20b_owned_dyn_ir_shape_prototype_gate";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_DROP_GUARD =
    "verifier_requires_compiler_owned_erased_drop_identity";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_ALLOCATOR_GUARD =
    "verifier_requires_compiler_owned_allocator_identity";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_CLEANUP_GUARD =
    "verifier_keeps_cleanup_dropck_static_until_runtime";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_BINDING_GUARD =
    "verifier_binds_identity_keys_to_owned_handle_prototype";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_RUNTIME_GUARD =
    "verifier_keeps_drop_allocator_runtime_lowering_blocked";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_NO_STDLIB = "standard_library_api_not_in_m20c";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_NO_BOX = "box_dyn_trait_not_in_m20c";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_NO_ALLOCATOR = "allocator_api_not_in_m20c";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_NO_RUNTIME = "runtime_abi_lowering_not_in_m20c";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_NO_DYNAMIC_DROP = "dynamic_drop_runtime_not_in_m20c";
constexpr std::string_view IR_OWNED_DYN_DROP_ALLOCATOR_IDENTITY_SET_KEY = "ir.m20c.owned_dyn.prototype_identity_set.v1";

struct PrototypeIdentityEntry {
    base::u64 object_type_id = 0;
    query::StableFingerprint128 object_type_key;
    query::StableFingerprint128 drop_identity_key;
    query::StableFingerprint128 allocator_identity_key;
};

[[nodiscard]] std::string_view safe_symbol(
    const Module& module, const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    return module.has_text(prototype.symbol) ? module.text(prototype.symbol) : std::string_view{};
}

[[nodiscard]] bool identity_key_is_valid(
    const query::StableFingerprint128 drop_key, const query::StableFingerprint128 allocator_key) noexcept
{
    return drop_key != query::StableFingerprint128{} && allocator_key != query::StableFingerprint128{}
    && drop_key != allocator_key;
}

[[nodiscard]] bool prototype_keeps_blockers(const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    return prototype.compiler_owned && prototype.borrowed_abi_unchanged && prototype.standard_library_blocked
        && prototype.box_surface_blocked && prototype.owning_dyn_user_value_blocked && prototype.allocator_api_blocked
        && prototype.runtime_lowering_blocked && prototype.dynamic_drop_runtime_blocked
        && prototype.backend_helper_blocked
        && prototype.erased_drop_runtime_slot == IR_OWNED_DYN_OBJECT_RUNTIME_SLOT_BLOCKED
        && prototype.allocator_runtime_slot == IR_OWNED_DYN_OBJECT_RUNTIME_SLOT_BLOCKED;
}

[[nodiscard]] bool prototype_identity_is_valid(
    const Module& module, const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    return query::is_valid(prototype.object_type_key) && is_valid(prototype.policy)
        && sema::is_valid(prototype.object_type) && prototype.object_type.value < module.types.size()
        && module.types.is_trait_object(prototype.object_type)
        && module.types.get(prototype.object_type).trait_object_principal_types.empty()
        && module.types.get(prototype.object_type).trait_object_key == prototype.object_type_key
        && module.has_text(prototype.symbol) && !module.text(prototype.symbol).empty()
        && identity_key_is_valid(prototype.erased_drop_identity_key, prototype.allocator_identity_key);
}

[[nodiscard]] bool all_prototypes_have_valid_identities(const Module& module)
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
    return std::ranges::all_of(
        module.owned_dyn_object_layout_prototypes, [&](const OwnedDynObjectLayoutPrototype& prototype) {
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

[[nodiscard]] bool all_prototypes_have_identity_keys(const Module& module) noexcept
{
    return !module.owned_dyn_object_layout_prototypes.empty()
        && std::ranges::all_of(
            module.owned_dyn_object_layout_prototypes, [](const OwnedDynObjectLayoutPrototype& prototype) {
                return identity_key_is_valid(prototype.erased_drop_identity_key, prototype.allocator_identity_key);
            });
}

using FingerprintOrder = std::tuple<base::u64, base::u64, base::u32>;
using PrototypeIdentityOrder = std::tuple<base::u64, FingerprintOrder, FingerprintOrder, FingerprintOrder>;

[[nodiscard]] FingerprintOrder fingerprint_order(const query::StableFingerprint128 fingerprint) noexcept
{
    return {
        fingerprint.primary,
        fingerprint.secondary,
        fingerprint.byte_count,
    };
}

[[nodiscard]] PrototypeIdentityOrder identity_entry_order(const PrototypeIdentityEntry& entry) noexcept
{
    return {
        entry.object_type_id,
        fingerprint_order(entry.object_type_key),
        fingerprint_order(entry.drop_identity_key),
        fingerprint_order(entry.allocator_identity_key),
    };
}

[[nodiscard]] bool identity_entry_less(const PrototypeIdentityEntry& lhs, const PrototypeIdentityEntry& rhs) noexcept
{
    return identity_entry_order(lhs) < identity_entry_order(rhs);
}

[[nodiscard]] PrototypeIdentityEntry prototype_identity_entry(const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    return PrototypeIdentityEntry{
        .object_type_id = prototype.object_type_key.global_id,
        .object_type_key = query::stable_key_fingerprint(prototype.object_type_key),
        .drop_identity_key = prototype.erased_drop_identity_key,
        .allocator_identity_key = prototype.allocator_identity_key,
    };
}

[[nodiscard]] bool prototype_identity_less(
    const OwnedDynObjectLayoutPrototype& lhs, const OwnedDynObjectLayoutPrototype& rhs) noexcept
{
    return identity_entry_less(prototype_identity_entry(lhs), prototype_identity_entry(rhs));
}

[[nodiscard]] const OwnedDynObjectLayoutPrototype& canonical_identity_prototype(const Module& module) noexcept
{
    return *std::ranges::min_element(module.owned_dyn_object_layout_prototypes, prototype_identity_less);
}

[[nodiscard]] query::StableFingerprint128 prototype_identity_set_key(const Module& module) noexcept
{
    std::vector<PrototypeIdentityEntry> entries;
    entries.reserve(module.owned_dyn_object_layout_prototypes.size());
    for (const OwnedDynObjectLayoutPrototype& prototype : module.owned_dyn_object_layout_prototypes) {
        entries.push_back(prototype_identity_entry(prototype));
    }
    std::ranges::sort(entries, identity_entry_less);

    query::StableHashBuilder builder;
    builder.mix_string(IR_OWNED_DYN_DROP_ALLOCATOR_IDENTITY_SET_KEY);
    builder.mix_u64(static_cast<base::u64>(entries.size()));
    for (const PrototypeIdentityEntry& entry : entries) {
        builder.mix_u64(entry.object_type_id);
        builder.mix_fingerprint(entry.object_type_key);
        builder.mix_fingerprint(entry.drop_identity_key);
        builder.mix_fingerprint(entry.allocator_identity_key);
    }
    return builder.finish();
}

[[nodiscard]] query::OwnedDynDropAllocatorIdentityFact make_fact(const Module& module,
    const query::OwnedDynDropAllocatorIdentityFactKind kind, const query::OwnedDynDropAllocatorIdentityStage stage,
    const query::OwnedDynDropAllocatorIdentityPolicy policy, const std::string_view fact_name,
    const std::string_view verifier_guard, const std::string_view blocked_surface)
{
    const OwnedDynObjectLayoutPrototype& representative = canonical_identity_prototype(module);
    const bool identities_valid = all_prototypes_have_valid_identities(module);
    const bool blockers_intact = identities_valid && all_prototypes_keep_blockers(module);
    const bool identity_keys_visible = identities_valid && all_prototypes_have_identity_keys(module);

    query::OwnedDynDropAllocatorIdentityFact fact;
    fact.fact_name = std::string(fact_name);
    fact.kind = kind;
    fact.stage = stage;
    fact.policy = policy;
    fact.references_m20b_ir_shape_gate = true;
    fact.compiler_owned_identity = blockers_intact;
    fact.drop_identity_visible = identity_keys_visible
        && (kind == query::OwnedDynDropAllocatorIdentityFactKind::erased_drop_identity
            || kind == query::OwnedDynDropAllocatorIdentityFactKind::cleanup_dropck_bridge
            || kind == query::OwnedDynDropAllocatorIdentityFactKind::owned_handle_identity_binding);
    fact.allocator_identity_visible = identity_keys_visible
        && (kind == query::OwnedDynDropAllocatorIdentityFactKind::allocator_identity
            || kind == query::OwnedDynDropAllocatorIdentityFactKind::owned_handle_identity_binding);
    fact.cleanup_dropck_bridge_visible =
        blockers_intact && kind == query::OwnedDynDropAllocatorIdentityFactKind::cleanup_dropck_bridge;
    fact.owned_handle_binding_visible =
        blockers_intact && kind == query::OwnedDynDropAllocatorIdentityFactKind::owned_handle_identity_binding;
    fact.borrowed_dyn_abi_unchanged = blockers_intact;
    fact.standard_library_api_blocked = blockers_intact;
    fact.box_dyn_surface_blocked = blockers_intact;
    fact.owning_dyn_user_value_blocked = blockers_intact;
    fact.allocator_api_blocked = blockers_intact;
    fact.runtime_lowering_blocked = blockers_intact;
    fact.dynamic_drop_runtime_blocked = blockers_intact;
    fact.backend_helper_blocked = blockers_intact;
    fact.executable_runtime_implemented = false;
    fact.object_type = representative.object_type_key;
    fact.drop_identity_key = representative.erased_drop_identity_key;
    fact.allocator_identity_key = representative.allocator_identity_key;
    fact.prototype_identity_set_key =
        identities_valid ? prototype_identity_set_key(module) : query::StableFingerprint128{};
    fact.layout_prototype_count = static_cast<base::u64>(module.owned_dyn_object_layout_prototypes.size());
    fact.subject_symbol = identities_valid ? std::string(safe_symbol(module, representative)) : std::string{};
    fact.m20b_gate_fact = std::string(IR_OWNED_DYN_DROP_ALLOCATOR_M20B_GATE);
    fact.verifier_guard_fact = std::string(verifier_guard);
    fact.blocked_surface_fact = std::string(blocked_surface);
    return fact;
}

void record_module_identity_facts(query::OwnedDynDropAllocatorIdentityGate& gate, const Module& module)
{
    if (module.owned_dyn_object_layout_prototypes.empty()) {
        return;
    }
    query::record_owned_dyn_drop_allocator_identity_fact(gate,
        make_fact(module, query::OwnedDynDropAllocatorIdentityFactKind::erased_drop_identity,
            query::OwnedDynDropAllocatorIdentityStage::identity_prerequisite,
            query::OwnedDynDropAllocatorIdentityPolicy::compiler_owned_erased_drop_identity_v1,
            IR_OWNED_DYN_DROP_ALLOCATOR_DROP_FACT, IR_OWNED_DYN_DROP_ALLOCATOR_DROP_GUARD,
            IR_OWNED_DYN_DROP_ALLOCATOR_NO_DYNAMIC_DROP));
    query::record_owned_dyn_drop_allocator_identity_fact(gate,
        make_fact(module, query::OwnedDynDropAllocatorIdentityFactKind::allocator_identity,
            query::OwnedDynDropAllocatorIdentityStage::identity_prerequisite,
            query::OwnedDynDropAllocatorIdentityPolicy::compiler_owned_allocator_identity_v1,
            IR_OWNED_DYN_DROP_ALLOCATOR_ALLOCATOR_FACT, IR_OWNED_DYN_DROP_ALLOCATOR_ALLOCATOR_GUARD,
            IR_OWNED_DYN_DROP_ALLOCATOR_NO_ALLOCATOR));
    query::record_owned_dyn_drop_allocator_identity_fact(gate,
        make_fact(module, query::OwnedDynDropAllocatorIdentityFactKind::cleanup_dropck_bridge,
            query::OwnedDynDropAllocatorIdentityStage::verifier_identity_guard,
            query::OwnedDynDropAllocatorIdentityPolicy::cleanup_dropck_static_bridge_v1,
            IR_OWNED_DYN_DROP_ALLOCATOR_CLEANUP_FACT, IR_OWNED_DYN_DROP_ALLOCATOR_CLEANUP_GUARD,
            IR_OWNED_DYN_DROP_ALLOCATOR_NO_STDLIB));
    query::record_owned_dyn_drop_allocator_identity_fact(gate,
        make_fact(module, query::OwnedDynDropAllocatorIdentityFactKind::owned_handle_identity_binding,
            query::OwnedDynDropAllocatorIdentityStage::verifier_identity_guard,
            query::OwnedDynDropAllocatorIdentityPolicy::owned_handle_identity_binding_v1,
            IR_OWNED_DYN_DROP_ALLOCATOR_BINDING_FACT, IR_OWNED_DYN_DROP_ALLOCATOR_BINDING_GUARD,
            IR_OWNED_DYN_DROP_ALLOCATOR_NO_BOX));
    query::record_owned_dyn_drop_allocator_identity_fact(gate,
        make_fact(module, query::OwnedDynDropAllocatorIdentityFactKind::runtime_lowering_blocker,
            query::OwnedDynDropAllocatorIdentityStage::blocked_future_runtime,
            query::OwnedDynDropAllocatorIdentityPolicy::runtime_lowering_not_implemented_v1,
            IR_OWNED_DYN_DROP_ALLOCATOR_RUNTIME_FACT, IR_OWNED_DYN_DROP_ALLOCATOR_RUNTIME_GUARD,
            IR_OWNED_DYN_DROP_ALLOCATOR_NO_RUNTIME));
}

} // namespace

query::OwnedDynDropAllocatorIdentityGate owned_dyn_drop_allocator_identity_gate(const Module& module)
{
    query::OwnedDynDropAllocatorIdentityGate gate;
    gate.subject = std::string(IR_OWNED_DYN_DROP_ALLOCATOR_M20C_SUBJECT);
    gate.ir_shape_gate = owned_dyn_ir_shape_prototype_gate(module);
    gate.ir_shape_gate_fingerprint = gate.ir_shape_gate.fingerprint;
    record_module_identity_facts(gate, module);
    gate.summary = query::summarize_owned_dyn_drop_allocator_identity_gate_counts(gate);
    gate.fingerprint = query::owned_dyn_drop_allocator_identity_gate_fingerprint(gate);
    return gate;
}

} // namespace aurex::ir
