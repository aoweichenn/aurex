#include <aurex/midend/ir/ir_owned_dyn_drop_allocator_identity_gate.hpp>
#include <aurex/midend/ir/ir_owned_dyn_runtime_lowering_abi_gate.hpp>

#include <algorithm>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace aurex::ir {
namespace {

constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_M20D_SUBJECT =
    "M20d Runtime Lowering ABI Design Closure";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_DESCRIPTOR_FACT =
    "owned_dyn_runtime_abi_descriptor_fact";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_TRANSITION_FACT =
    "owned_dyn_blocked_to_admitted_transition_guard_fact";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_BACKEND_FACT =
    "owned_dyn_backend_helper_prerequisite_fact";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_BRIDGE_FACT =
    "owned_dyn_drop_allocator_runtime_bridge_fact";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_DYNAMIC_DROP_FACT =
    "owned_dyn_dynamic_drop_runtime_blocker_fact";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_M20C_GATE =
    "requires_m20c_owned_dyn_drop_allocator_identity_gate";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_DESCRIPTOR_GUARD =
    "verifier_records_owned_dyn_runtime_abi_descriptor";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_TRANSITION_GUARD =
    "verifier_keeps_blocked_to_admitted_transition_closed";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_BACKEND_GUARD =
    "verifier_records_backend_helper_identity_without_callability";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_BRIDGE_GUARD =
    "verifier_binds_drop_allocator_identity_to_runtime_abi_descriptor";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_DYNAMIC_DROP_GUARD =
    "verifier_keeps_dynamic_drop_runtime_blocked";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_STDLIB = "standard_library_api_not_in_m20d";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_ALLOCATOR = "allocator_api_not_in_m20d";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_RUNTIME =
    "runtime_abi_lowering_not_executable_in_m20d";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_DYNAMIC_DROP =
    "dynamic_drop_runtime_not_in_m20d";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_BACKEND_HELPER =
    "backend_runtime_helper_not_callable_in_m20d";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_DESCRIPTOR_KEY =
    "ir.m20d.owned_dyn.runtime_abi_descriptor.v1";
constexpr std::string_view IR_OWNED_DYN_RUNTIME_LOWERING_ABI_BACKEND_HELPER_KEY =
    "ir.m20d.owned_dyn.backend_helper_identity.v1";

struct RuntimeAbiIdentityEntry {
    base::u64 object_type_id = 0;
    query::StableFingerprint128 object_type_key;
    query::StableFingerprint128 drop_identity_key;
    query::StableFingerprint128 allocator_identity_key;
};

using FingerprintOrder = std::tuple<base::u64, base::u64, base::u32>;
using RuntimeAbiIdentityOrder = std::tuple<base::u64, FingerprintOrder, FingerprintOrder, FingerprintOrder>;

[[nodiscard]] std::string_view safe_symbol(
    const Module& module,
    const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    return module.has_text(prototype.symbol) ? module.text(prototype.symbol) : std::string_view{};
}

[[nodiscard]] bool identity_key_is_valid(
    const query::StableFingerprint128 drop_key,
    const query::StableFingerprint128 allocator_key) noexcept
{
    return drop_key != query::StableFingerprint128{}
        && allocator_key != query::StableFingerprint128{}
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
    const Module& module,
    const OwnedDynObjectLayoutPrototype& prototype) noexcept
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
        module.owned_dyn_object_layout_prototypes,
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

[[nodiscard]] FingerprintOrder fingerprint_order(
    const query::StableFingerprint128 fingerprint) noexcept
{
    return {
        fingerprint.primary,
        fingerprint.secondary,
        fingerprint.byte_count,
    };
}

[[nodiscard]] RuntimeAbiIdentityOrder identity_entry_order(const RuntimeAbiIdentityEntry& entry) noexcept
{
    return {
        entry.object_type_id,
        fingerprint_order(entry.object_type_key),
        fingerprint_order(entry.drop_identity_key),
        fingerprint_order(entry.allocator_identity_key),
    };
}

[[nodiscard]] bool identity_entry_less(
    const RuntimeAbiIdentityEntry& lhs,
    const RuntimeAbiIdentityEntry& rhs) noexcept
{
    return identity_entry_order(lhs) < identity_entry_order(rhs);
}

[[nodiscard]] RuntimeAbiIdentityEntry runtime_abi_identity_entry(
    const OwnedDynObjectLayoutPrototype& prototype) noexcept
{
    return RuntimeAbiIdentityEntry{
        .object_type_id = prototype.object_type_key.global_id,
        .object_type_key = query::stable_key_fingerprint(prototype.object_type_key),
        .drop_identity_key = prototype.erased_drop_identity_key,
        .allocator_identity_key = prototype.allocator_identity_key,
    };
}

[[nodiscard]] bool prototype_identity_less(
    const OwnedDynObjectLayoutPrototype& lhs,
    const OwnedDynObjectLayoutPrototype& rhs) noexcept
{
    return identity_entry_less(runtime_abi_identity_entry(lhs), runtime_abi_identity_entry(rhs));
}

[[nodiscard]] const OwnedDynObjectLayoutPrototype& canonical_identity_prototype(const Module& module) noexcept
{
    return *std::ranges::min_element(module.owned_dyn_object_layout_prototypes, prototype_identity_less);
}

[[nodiscard]] std::vector<RuntimeAbiIdentityEntry> sorted_runtime_abi_identity_entries(const Module& module)
{
    std::vector<RuntimeAbiIdentityEntry> entries;
    entries.reserve(module.owned_dyn_object_layout_prototypes.size());
    for (const OwnedDynObjectLayoutPrototype& prototype : module.owned_dyn_object_layout_prototypes) {
        entries.push_back(runtime_abi_identity_entry(prototype));
    }
    std::ranges::sort(entries, identity_entry_less);
    return entries;
}

[[nodiscard]] query::StableFingerprint128 runtime_abi_descriptor_key(
    const Module& module,
    const query::StableFingerprint128 identity_set_key) noexcept
{
    const std::vector<RuntimeAbiIdentityEntry> entries = sorted_runtime_abi_identity_entries(module);
    query::StableHashBuilder builder;
    builder.mix_string(IR_OWNED_DYN_RUNTIME_LOWERING_ABI_DESCRIPTOR_KEY);
    builder.mix_fingerprint(identity_set_key);
    builder.mix_u64(static_cast<base::u64>(entries.size()));
    for (const RuntimeAbiIdentityEntry& entry : entries) {
        builder.mix_u64(entry.object_type_id);
        builder.mix_fingerprint(entry.object_type_key);
        builder.mix_fingerprint(entry.drop_identity_key);
        builder.mix_fingerprint(entry.allocator_identity_key);
    }
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 backend_helper_identity_key(
    const Module& module,
    const query::StableFingerprint128 identity_set_key,
    const query::StableFingerprint128 descriptor_key) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(IR_OWNED_DYN_RUNTIME_LOWERING_ABI_BACKEND_HELPER_KEY);
    builder.mix_fingerprint(identity_set_key);
    builder.mix_fingerprint(descriptor_key);
    builder.mix_u64(static_cast<base::u64>(module.owned_dyn_object_layout_prototypes.size()));
    return builder.finish();
}

[[nodiscard]] query::OwnedDynRuntimeLoweringAbiFact make_fact(
    const Module& module,
    const query::OwnedDynDropAllocatorIdentityGate& identity_gate,
    const query::OwnedDynRuntimeLoweringAbiFactKind kind,
    const query::OwnedDynRuntimeLoweringAbiStage stage,
    const query::OwnedDynRuntimeLoweringAbiPolicy policy,
    const std::string_view fact_name,
    const std::string_view verifier_guard,
    const std::string_view blocked_surface)
{
    const OwnedDynObjectLayoutPrototype& representative = canonical_identity_prototype(module);
    const bool identities_valid = all_prototypes_have_valid_identities(module);
    const bool blockers_intact = identities_valid && all_prototypes_keep_blockers(module);
    const bool identity_gate_valid = query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(identity_gate)
        && !identity_gate.facts.empty();
    const query::OwnedDynDropAllocatorIdentityFact& identity = identity_gate.facts.front();
    const query::StableFingerprint128 identity_set_key =
        identity_gate_valid ? identity.prototype_identity_set_key : query::StableFingerprint128{};
    const query::StableFingerprint128 descriptor_key =
        identities_valid && identity_gate_valid ? runtime_abi_descriptor_key(module, identity_set_key)
                                                : query::StableFingerprint128{};

    query::OwnedDynRuntimeLoweringAbiFact fact;
    fact.fact_name = std::string(fact_name);
    fact.kind = kind;
    fact.stage = stage;
    fact.policy = policy;
    fact.references_m20c_drop_allocator_identity_gate = true;
    fact.compiler_owned_runtime_abi_descriptor = blockers_intact;
    fact.runtime_abi_descriptor_visible = blockers_intact
        && (kind == query::OwnedDynRuntimeLoweringAbiFactKind::runtime_abi_descriptor
            || kind == query::OwnedDynRuntimeLoweringAbiFactKind::lowering_transition_guard
            || kind == query::OwnedDynRuntimeLoweringAbiFactKind::backend_helper_prerequisite
            || kind == query::OwnedDynRuntimeLoweringAbiFactKind::drop_allocator_runtime_bridge);
    fact.lowering_transition_guard_visible =
        blockers_intact && kind == query::OwnedDynRuntimeLoweringAbiFactKind::lowering_transition_guard;
    fact.backend_helper_prerequisite_visible =
        blockers_intact && kind == query::OwnedDynRuntimeLoweringAbiFactKind::backend_helper_prerequisite;
    fact.drop_allocator_runtime_bridge_visible =
        blockers_intact && kind == query::OwnedDynRuntimeLoweringAbiFactKind::drop_allocator_runtime_bridge;
    fact.dynamic_drop_runtime_blocker_visible =
        blockers_intact && kind == query::OwnedDynRuntimeLoweringAbiFactKind::dynamic_drop_runtime_blocker;
    fact.borrowed_dyn_abi_unchanged = blockers_intact;
    fact.standard_library_api_blocked = blockers_intact;
    fact.box_dyn_surface_blocked = blockers_intact;
    fact.owning_dyn_user_value_blocked = blockers_intact;
    fact.allocator_api_blocked = blockers_intact;
    fact.runtime_lowering_blocked = blockers_intact;
    fact.dynamic_drop_runtime_blocked = blockers_intact;
    fact.backend_helper_blocked = blockers_intact;
    fact.backend_helper_callable = false;
    fact.executable_runtime_implemented = false;
    fact.object_type = identity_gate_valid ? identity.object_type : representative.object_type_key;
    fact.drop_identity_key = identity_gate_valid ? identity.drop_identity_key : query::StableFingerprint128{};
    fact.allocator_identity_key = identity_gate_valid ? identity.allocator_identity_key : query::StableFingerprint128{};
    fact.prototype_identity_set_key = identity_set_key;
    fact.runtime_abi_descriptor_key = descriptor_key;
    fact.backend_helper_identity_key = identities_valid && identity_gate_valid
        ? backend_helper_identity_key(module, identity_set_key, descriptor_key)
        : query::StableFingerprint128{};
    fact.layout_prototype_count =
        identity_gate_valid ? identity.layout_prototype_count
                            : static_cast<base::u64>(module.owned_dyn_object_layout_prototypes.size());
    fact.subject_symbol = identities_valid ? std::string(safe_symbol(module, representative)) : std::string{};
    fact.m20c_gate_fact = std::string(IR_OWNED_DYN_RUNTIME_LOWERING_ABI_M20C_GATE);
    fact.verifier_guard_fact = std::string(verifier_guard);
    fact.blocked_surface_fact = std::string(blocked_surface);
    return fact;
}

void record_module_runtime_abi_facts(
    query::OwnedDynRuntimeLoweringAbiGate& gate,
    const Module& module)
{
    if (module.owned_dyn_object_layout_prototypes.empty()) {
        return;
    }
    query::record_owned_dyn_runtime_lowering_abi_fact(gate,
        make_fact(module,
            gate.drop_allocator_identity_gate,
            query::OwnedDynRuntimeLoweringAbiFactKind::runtime_abi_descriptor,
            query::OwnedDynRuntimeLoweringAbiStage::abi_design_closure,
            query::OwnedDynRuntimeLoweringAbiPolicy::compiler_owned_runtime_abi_descriptor_v1,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_DESCRIPTOR_FACT,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_DESCRIPTOR_GUARD,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_RUNTIME));
    query::record_owned_dyn_runtime_lowering_abi_fact(gate,
        make_fact(module,
            gate.drop_allocator_identity_gate,
            query::OwnedDynRuntimeLoweringAbiFactKind::lowering_transition_guard,
            query::OwnedDynRuntimeLoweringAbiStage::verifier_lowering_guard,
            query::OwnedDynRuntimeLoweringAbiPolicy::blocked_to_admitted_transition_check_v1,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_TRANSITION_FACT,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_TRANSITION_GUARD,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_STDLIB));
    query::record_owned_dyn_runtime_lowering_abi_fact(gate,
        make_fact(module,
            gate.drop_allocator_identity_gate,
            query::OwnedDynRuntimeLoweringAbiFactKind::backend_helper_prerequisite,
            query::OwnedDynRuntimeLoweringAbiStage::abi_design_closure,
            query::OwnedDynRuntimeLoweringAbiPolicy::backend_helper_identity_prerequisite_v1,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_BACKEND_FACT,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_BACKEND_GUARD,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_BACKEND_HELPER));
    query::record_owned_dyn_runtime_lowering_abi_fact(gate,
        make_fact(module,
            gate.drop_allocator_identity_gate,
            query::OwnedDynRuntimeLoweringAbiFactKind::drop_allocator_runtime_bridge,
            query::OwnedDynRuntimeLoweringAbiStage::verifier_lowering_guard,
            query::OwnedDynRuntimeLoweringAbiPolicy::drop_allocator_runtime_bridge_v1,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_BRIDGE_FACT,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_BRIDGE_GUARD,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_ALLOCATOR));
    query::record_owned_dyn_runtime_lowering_abi_fact(gate,
        make_fact(module,
            gate.drop_allocator_identity_gate,
            query::OwnedDynRuntimeLoweringAbiFactKind::dynamic_drop_runtime_blocker,
            query::OwnedDynRuntimeLoweringAbiStage::blocked_future_runtime,
            query::OwnedDynRuntimeLoweringAbiPolicy::dynamic_drop_runtime_not_implemented_v1,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_DYNAMIC_DROP_FACT,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_DYNAMIC_DROP_GUARD,
            IR_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_DYNAMIC_DROP));
}

} // namespace

query::OwnedDynRuntimeLoweringAbiGate owned_dyn_runtime_lowering_abi_gate(const Module& module)
{
    query::OwnedDynRuntimeLoweringAbiGate gate;
    gate.subject = std::string(IR_OWNED_DYN_RUNTIME_LOWERING_ABI_M20D_SUBJECT);
    gate.drop_allocator_identity_gate = owned_dyn_drop_allocator_identity_gate(module);
    gate.drop_allocator_identity_gate_fingerprint = gate.drop_allocator_identity_gate.fingerprint;
    record_module_runtime_abi_facts(gate, module);
    gate.summary = query::summarize_owned_dyn_runtime_lowering_abi_gate_counts(gate);
    gate.fingerprint = query::owned_dyn_runtime_lowering_abi_gate_fingerprint(gate);
    return gate;
}

} // namespace aurex::ir
