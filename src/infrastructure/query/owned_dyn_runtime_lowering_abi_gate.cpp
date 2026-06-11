#include <aurex/infrastructure/query/owned_dyn_runtime_lowering_abi_gate.hpp>

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FINGERPRINT_MARKER =
    "query.owned_dyn_runtime_lowering_abi_gate.v1";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_M20D_SUBJECT =
    "M20d Runtime Lowering ABI Design Closure";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_DESCRIPTOR_FACT =
    "owned_dyn_runtime_abi_descriptor_fact";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_TRANSITION_FACT =
    "owned_dyn_blocked_to_admitted_transition_guard_fact";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_BACKEND_FACT =
    "owned_dyn_backend_helper_prerequisite_fact";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_BRIDGE_FACT =
    "owned_dyn_drop_allocator_runtime_bridge_fact";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_DYNAMIC_DROP_FACT =
    "owned_dyn_dynamic_drop_runtime_blocker_fact";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_M20C_GATE =
    "requires_m20c_owned_dyn_drop_allocator_identity_gate";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_DESCRIPTOR_KEY =
    "m20d.compiler_owned_runtime_abi_descriptor";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_BACKEND_HELPER_KEY =
    "m20d.backend_helper_identity_prerequisite";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_DESCRIPTOR_GUARD =
    "verifier_records_owned_dyn_runtime_abi_descriptor";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_TRANSITION_GUARD =
    "verifier_keeps_blocked_to_admitted_transition_closed";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_BACKEND_GUARD =
    "verifier_records_backend_helper_identity_without_callability";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_BRIDGE_GUARD =
    "verifier_binds_drop_allocator_identity_to_runtime_abi_descriptor";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_DYNAMIC_DROP_GUARD =
    "verifier_keeps_dynamic_drop_runtime_blocked";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_STDLIB = "standard_library_api_not_in_m20d";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_ALLOCATOR = "allocator_api_not_in_m20d";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_RUNTIME =
    "runtime_abi_lowering_not_executable_in_m20d";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_DYNAMIC_DROP =
    "dynamic_drop_runtime_not_in_m20d";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_BACKEND_HELPER =
    "backend_runtime_helper_not_callable_in_m20d";
constexpr base::u8 QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_INVALID_ENUM_VALUE = 255U;
constexpr base::u64 QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FACT_COUNT = 5U;
constexpr base::u64 QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_SINGLE_FACT = 1U;

[[nodiscard]] base::u8 stable_kind_value(const OwnedDynRuntimeLoweringAbiFactKind kind) noexcept
{
    return is_valid(kind) ? static_cast<base::u8>(kind) : QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_stage_value(const OwnedDynRuntimeLoweringAbiStage stage) noexcept
{
    return is_valid(stage) ? static_cast<base::u8>(stage) : QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_policy_value(const OwnedDynRuntimeLoweringAbiPolicy policy) noexcept
{
    return is_valid(policy) ? static_cast<base::u8>(policy) : QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_INVALID_ENUM_VALUE;
}

[[nodiscard]] bool nonempty(const std::string_view value) noexcept
{
    return !value.empty();
}

[[nodiscard]] bool is_nonzero(const StableFingerprint128 fingerprint) noexcept
{
    return fingerprint != StableFingerprint128{};
}

[[nodiscard]] bool fact_payload_is_named(const OwnedDynRuntimeLoweringAbiFact& fact) noexcept
{
    return nonempty(fact.fact_name) && nonempty(fact.subject_symbol) && nonempty(fact.m20c_gate_fact)
        && nonempty(fact.verifier_guard_fact) && nonempty(fact.blocked_surface_fact);
}

[[nodiscard]] bool blockers_are_intact(const OwnedDynRuntimeLoweringAbiFact& fact) noexcept
{
    return fact.references_m20c_drop_allocator_identity_gate && fact.compiler_owned_runtime_abi_descriptor
        && fact.borrowed_dyn_abi_unchanged && fact.standard_library_api_blocked && fact.box_dyn_surface_blocked
        && fact.owning_dyn_user_value_blocked && fact.allocator_api_blocked && fact.runtime_lowering_blocked
        && fact.dynamic_drop_runtime_blocked && fact.backend_helper_blocked && !fact.backend_helper_callable
        && !fact.executable_runtime_implemented && fact.layout_prototype_count > 0U
        && is_nonzero(fact.drop_identity_key) && is_nonzero(fact.allocator_identity_key)
        && is_nonzero(fact.prototype_identity_set_key) && is_nonzero(fact.runtime_abi_descriptor_key)
        && is_nonzero(fact.backend_helper_identity_key) && fact.drop_identity_key != fact.allocator_identity_key
        && fact.runtime_abi_descriptor_key != fact.backend_helper_identity_key;
}

[[nodiscard]] bool kind_policy_stage_match(const OwnedDynRuntimeLoweringAbiFact& fact) noexcept
{
    switch (fact.kind) {
        case OwnedDynRuntimeLoweringAbiFactKind::runtime_abi_descriptor:
            return fact.stage == OwnedDynRuntimeLoweringAbiStage::abi_design_closure
                && fact.policy == OwnedDynRuntimeLoweringAbiPolicy::compiler_owned_runtime_abi_descriptor_v1
                && fact.runtime_abi_descriptor_visible && !fact.lowering_transition_guard_visible
                && !fact.backend_helper_prerequisite_visible && !fact.drop_allocator_runtime_bridge_visible
                && !fact.dynamic_drop_runtime_blocker_visible;
        case OwnedDynRuntimeLoweringAbiFactKind::lowering_transition_guard:
            return fact.stage == OwnedDynRuntimeLoweringAbiStage::verifier_lowering_guard
                && fact.policy == OwnedDynRuntimeLoweringAbiPolicy::blocked_to_admitted_transition_check_v1
                && fact.runtime_abi_descriptor_visible && fact.lowering_transition_guard_visible
                && !fact.backend_helper_prerequisite_visible && !fact.drop_allocator_runtime_bridge_visible
                && !fact.dynamic_drop_runtime_blocker_visible;
        case OwnedDynRuntimeLoweringAbiFactKind::backend_helper_prerequisite:
            return fact.stage == OwnedDynRuntimeLoweringAbiStage::abi_design_closure
                && fact.policy == OwnedDynRuntimeLoweringAbiPolicy::backend_helper_identity_prerequisite_v1
                && fact.runtime_abi_descriptor_visible && !fact.lowering_transition_guard_visible
                && fact.backend_helper_prerequisite_visible && !fact.drop_allocator_runtime_bridge_visible
                && !fact.dynamic_drop_runtime_blocker_visible;
        case OwnedDynRuntimeLoweringAbiFactKind::drop_allocator_runtime_bridge:
            return fact.stage == OwnedDynRuntimeLoweringAbiStage::verifier_lowering_guard
                && fact.policy == OwnedDynRuntimeLoweringAbiPolicy::drop_allocator_runtime_bridge_v1
                && fact.runtime_abi_descriptor_visible && !fact.lowering_transition_guard_visible
                && !fact.backend_helper_prerequisite_visible && fact.drop_allocator_runtime_bridge_visible
                && !fact.dynamic_drop_runtime_blocker_visible;
        case OwnedDynRuntimeLoweringAbiFactKind::dynamic_drop_runtime_blocker:
            return fact.stage == OwnedDynRuntimeLoweringAbiStage::blocked_future_runtime
                && fact.policy == OwnedDynRuntimeLoweringAbiPolicy::dynamic_drop_runtime_not_implemented_v1
                && !fact.runtime_abi_descriptor_visible && !fact.lowering_transition_guard_visible
                && !fact.backend_helper_prerequisite_visible && !fact.drop_allocator_runtime_bridge_visible
                && fact.dynamic_drop_runtime_blocker_visible;
    }
    return false;
}

[[nodiscard]] bool summary_equals(
    const OwnedDynRuntimeLoweringAbiSummary& lhs,
    const OwnedDynRuntimeLoweringAbiSummary& rhs) noexcept
{
    return lhs.fact_count == rhs.fact_count
        && lhs.runtime_abi_descriptor_count == rhs.runtime_abi_descriptor_count
        && lhs.lowering_transition_guard_count == rhs.lowering_transition_guard_count
        && lhs.backend_helper_prerequisite_count == rhs.backend_helper_prerequisite_count
        && lhs.drop_allocator_runtime_bridge_count == rhs.drop_allocator_runtime_bridge_count
        && lhs.dynamic_drop_runtime_blocker_count == rhs.dynamic_drop_runtime_blocker_count
        && lhs.m20c_reference_count == rhs.m20c_reference_count
        && lhs.compiler_owned_runtime_abi_descriptor_count == rhs.compiler_owned_runtime_abi_descriptor_count
        && lhs.runtime_abi_descriptor_visible_count == rhs.runtime_abi_descriptor_visible_count
        && lhs.lowering_transition_guard_visible_count == rhs.lowering_transition_guard_visible_count
        && lhs.backend_helper_prerequisite_visible_count == rhs.backend_helper_prerequisite_visible_count
        && lhs.drop_allocator_runtime_bridge_visible_count == rhs.drop_allocator_runtime_bridge_visible_count
        && lhs.dynamic_drop_runtime_blocker_visible_count == rhs.dynamic_drop_runtime_blocker_visible_count
        && lhs.borrowed_dyn_abi_unchanged_count == rhs.borrowed_dyn_abi_unchanged_count
        && lhs.standard_library_api_blocked_count == rhs.standard_library_api_blocked_count
        && lhs.box_dyn_surface_blocked_count == rhs.box_dyn_surface_blocked_count
        && lhs.owning_dyn_user_value_blocked_count == rhs.owning_dyn_user_value_blocked_count
        && lhs.allocator_api_blocked_count == rhs.allocator_api_blocked_count
        && lhs.runtime_lowering_blocked_count == rhs.runtime_lowering_blocked_count
        && lhs.dynamic_drop_runtime_blocked_count == rhs.dynamic_drop_runtime_blocked_count
        && lhs.backend_helper_blocked_count == rhs.backend_helper_blocked_count
        && lhs.backend_helper_callable_count == rhs.backend_helper_callable_count
        && lhs.executable_runtime_implemented_count == rhs.executable_runtime_implemented_count
        && lhs.observed_layout_prototype_total == rhs.observed_layout_prototype_total;
}

void mix_summary(StableHashBuilder& builder, const OwnedDynRuntimeLoweringAbiSummary& summary) noexcept
{
    builder.mix_u64(summary.fact_count);
    builder.mix_u64(summary.runtime_abi_descriptor_count);
    builder.mix_u64(summary.lowering_transition_guard_count);
    builder.mix_u64(summary.backend_helper_prerequisite_count);
    builder.mix_u64(summary.drop_allocator_runtime_bridge_count);
    builder.mix_u64(summary.dynamic_drop_runtime_blocker_count);
    builder.mix_u64(summary.m20c_reference_count);
    builder.mix_u64(summary.compiler_owned_runtime_abi_descriptor_count);
    builder.mix_u64(summary.runtime_abi_descriptor_visible_count);
    builder.mix_u64(summary.lowering_transition_guard_visible_count);
    builder.mix_u64(summary.backend_helper_prerequisite_visible_count);
    builder.mix_u64(summary.drop_allocator_runtime_bridge_visible_count);
    builder.mix_u64(summary.dynamic_drop_runtime_blocker_visible_count);
    builder.mix_u64(summary.borrowed_dyn_abi_unchanged_count);
    builder.mix_u64(summary.standard_library_api_blocked_count);
    builder.mix_u64(summary.box_dyn_surface_blocked_count);
    builder.mix_u64(summary.owning_dyn_user_value_blocked_count);
    builder.mix_u64(summary.allocator_api_blocked_count);
    builder.mix_u64(summary.runtime_lowering_blocked_count);
    builder.mix_u64(summary.dynamic_drop_runtime_blocked_count);
    builder.mix_u64(summary.backend_helper_blocked_count);
    builder.mix_u64(summary.backend_helper_callable_count);
    builder.mix_u64(summary.executable_runtime_implemented_count);
    builder.mix_u64(summary.observed_layout_prototype_total);
}

void mix_fact(StableHashBuilder& builder, const OwnedDynRuntimeLoweringAbiFact& fact) noexcept
{
    builder.mix_string(fact.fact_name);
    builder.mix_u8(stable_kind_value(fact.kind));
    builder.mix_u8(stable_stage_value(fact.stage));
    builder.mix_u8(stable_policy_value(fact.policy));
    builder.mix_bool(fact.references_m20c_drop_allocator_identity_gate);
    builder.mix_bool(fact.compiler_owned_runtime_abi_descriptor);
    builder.mix_bool(fact.runtime_abi_descriptor_visible);
    builder.mix_bool(fact.lowering_transition_guard_visible);
    builder.mix_bool(fact.backend_helper_prerequisite_visible);
    builder.mix_bool(fact.drop_allocator_runtime_bridge_visible);
    builder.mix_bool(fact.dynamic_drop_runtime_blocker_visible);
    builder.mix_bool(fact.borrowed_dyn_abi_unchanged);
    builder.mix_bool(fact.standard_library_api_blocked);
    builder.mix_bool(fact.box_dyn_surface_blocked);
    builder.mix_bool(fact.owning_dyn_user_value_blocked);
    builder.mix_bool(fact.allocator_api_blocked);
    builder.mix_bool(fact.runtime_lowering_blocked);
    builder.mix_bool(fact.dynamic_drop_runtime_blocked);
    builder.mix_bool(fact.backend_helper_blocked);
    builder.mix_bool(fact.backend_helper_callable);
    builder.mix_bool(fact.executable_runtime_implemented);
    builder.mix_u64(fact.object_type.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(fact.object_type));
    builder.mix_fingerprint(fact.drop_identity_key);
    builder.mix_fingerprint(fact.allocator_identity_key);
    builder.mix_fingerprint(fact.prototype_identity_set_key);
    builder.mix_fingerprint(fact.runtime_abi_descriptor_key);
    builder.mix_fingerprint(fact.backend_helper_identity_key);
    builder.mix_u64(fact.layout_prototype_count);
    builder.mix_string(fact.subject_symbol);
    builder.mix_string(fact.m20c_gate_fact);
    builder.mix_string(fact.verifier_guard_fact);
    builder.mix_string(fact.blocked_surface_fact);
}

[[nodiscard]] StableFingerprint128 runtime_abi_descriptor_key(const OwnedDynDropAllocatorIdentityFact& identity)
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_DESCRIPTOR_KEY);
    builder.mix_u64(identity.object_type.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(identity.object_type));
    builder.mix_fingerprint(identity.drop_identity_key);
    builder.mix_fingerprint(identity.allocator_identity_key);
    builder.mix_fingerprint(identity.prototype_identity_set_key);
    builder.mix_u64(identity.layout_prototype_count);
    return builder.finish();
}

[[nodiscard]] StableFingerprint128 backend_helper_identity_key(
    const OwnedDynDropAllocatorIdentityFact& identity,
    const StableFingerprint128 descriptor_key)
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_BACKEND_HELPER_KEY);
    builder.mix_u64(identity.object_type.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(identity.object_type));
    builder.mix_fingerprint(descriptor_key);
    builder.mix_fingerprint(identity.prototype_identity_set_key);
    return builder.finish();
}

[[nodiscard]] OwnedDynRuntimeLoweringAbiFact abi_fact(
    const OwnedDynDropAllocatorIdentityGate& identity_gate,
    const OwnedDynRuntimeLoweringAbiFactKind kind,
    const OwnedDynRuntimeLoweringAbiStage stage,
    const OwnedDynRuntimeLoweringAbiPolicy policy,
    const std::string_view fact_name,
    const std::string_view verifier_guard,
    const std::string_view blocked_surface)
{
    const OwnedDynDropAllocatorIdentityFact& identity = identity_gate.facts.front();
    const StableFingerprint128 descriptor_key = runtime_abi_descriptor_key(identity);

    OwnedDynRuntimeLoweringAbiFact fact;
    fact.fact_name = std::string(fact_name);
    fact.kind = kind;
    fact.stage = stage;
    fact.policy = policy;
    fact.runtime_abi_descriptor_visible =
        kind == OwnedDynRuntimeLoweringAbiFactKind::runtime_abi_descriptor
        || kind == OwnedDynRuntimeLoweringAbiFactKind::lowering_transition_guard
        || kind == OwnedDynRuntimeLoweringAbiFactKind::backend_helper_prerequisite
        || kind == OwnedDynRuntimeLoweringAbiFactKind::drop_allocator_runtime_bridge;
    fact.lowering_transition_guard_visible =
        kind == OwnedDynRuntimeLoweringAbiFactKind::lowering_transition_guard;
    fact.backend_helper_prerequisite_visible =
        kind == OwnedDynRuntimeLoweringAbiFactKind::backend_helper_prerequisite;
    fact.drop_allocator_runtime_bridge_visible =
        kind == OwnedDynRuntimeLoweringAbiFactKind::drop_allocator_runtime_bridge;
    fact.dynamic_drop_runtime_blocker_visible =
        kind == OwnedDynRuntimeLoweringAbiFactKind::dynamic_drop_runtime_blocker;
    fact.object_type = identity.object_type;
    fact.drop_identity_key = identity.drop_identity_key;
    fact.allocator_identity_key = identity.allocator_identity_key;
    fact.prototype_identity_set_key = identity.prototype_identity_set_key;
    fact.runtime_abi_descriptor_key = descriptor_key;
    fact.backend_helper_identity_key = backend_helper_identity_key(identity, descriptor_key);
    fact.layout_prototype_count = identity_gate.summary.observed_layout_prototype_total;
    fact.subject_symbol = std::string(QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_M20D_SUBJECT);
    fact.m20c_gate_fact = std::string(QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_M20C_GATE);
    fact.verifier_guard_fact = std::string(verifier_guard);
    fact.blocked_surface_fact = std::string(blocked_surface);
    return fact;
}

[[nodiscard]] bool m20d_fact_kinds_are_complete(const OwnedDynRuntimeLoweringAbiGate& gate) noexcept
{
    std::array<bool, QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FACT_COUNT> seen{};
    for (const OwnedDynRuntimeLoweringAbiFact& fact : gate.facts) {
        if (!is_valid(fact.kind)) {
            return false;
        }
        const auto index = static_cast<base::usize>(fact.kind) - 1U;
        if (index >= seen.size() || seen[index]) {
            return false;
        }
        seen[index] = true;
    }
    return std::all_of(seen.begin(), seen.end(), [](const bool present) {
        return present;
    });
}

[[nodiscard]] bool fact_keys_are_consistent(const OwnedDynRuntimeLoweringAbiGate& gate) noexcept
{
    if (gate.facts.empty()) {
        return false;
    }
    const OwnedDynRuntimeLoweringAbiFact& expected = gate.facts.front();
    return is_nonzero(expected.drop_identity_key) && is_nonzero(expected.allocator_identity_key)
        && is_nonzero(expected.prototype_identity_set_key) && is_nonzero(expected.runtime_abi_descriptor_key)
        && is_nonzero(expected.backend_helper_identity_key)
        && std::all_of(gate.facts.begin(), gate.facts.end(),
            [&expected](const OwnedDynRuntimeLoweringAbiFact& fact) {
                return fact.drop_identity_key == expected.drop_identity_key
                    && fact.allocator_identity_key == expected.allocator_identity_key
                    && fact.prototype_identity_set_key == expected.prototype_identity_set_key
                    && fact.runtime_abi_descriptor_key == expected.runtime_abi_descriptor_key
                    && fact.backend_helper_identity_key == expected.backend_helper_identity_key;
            });
}

[[nodiscard]] bool m20c_identity_gate_keys_are_consistent(
    const OwnedDynRuntimeLoweringAbiGate& gate) noexcept
{
    if (gate.drop_allocator_identity_gate.facts.empty()) {
        return false;
    }
    const OwnedDynDropAllocatorIdentityFact& expected = gate.drop_allocator_identity_gate.facts.front();
    return is_nonzero(expected.drop_identity_key) && is_nonzero(expected.allocator_identity_key)
        && is_nonzero(expected.prototype_identity_set_key) && expected.layout_prototype_count > 0U
        && std::all_of(gate.drop_allocator_identity_gate.facts.begin(),
            gate.drop_allocator_identity_gate.facts.end(),
            [&expected](const OwnedDynDropAllocatorIdentityFact& fact) {
                return fact.drop_identity_key == expected.drop_identity_key
                    && fact.allocator_identity_key == expected.allocator_identity_key
                    && fact.prototype_identity_set_key == expected.prototype_identity_set_key
                    && fact.layout_prototype_count == expected.layout_prototype_count;
            });
}

[[nodiscard]] bool fact_keys_match_m20c_identity_gate(
    const OwnedDynRuntimeLoweringAbiGate& gate) noexcept
{
    if (gate.facts.empty() || gate.drop_allocator_identity_gate.facts.empty()) {
        return false;
    }
    const OwnedDynDropAllocatorIdentityFact& expected = gate.drop_allocator_identity_gate.facts.front();
    return std::all_of(gate.facts.begin(), gate.facts.end(),
        [&expected](const OwnedDynRuntimeLoweringAbiFact& fact) {
            return fact.drop_identity_key == expected.drop_identity_key
                && fact.allocator_identity_key == expected.allocator_identity_key
                && fact.prototype_identity_set_key == expected.prototype_identity_set_key
                && fact.layout_prototype_count == expected.layout_prototype_count;
        });
}

[[nodiscard]] bool layout_prototype_counts_are_consistent(
    const OwnedDynRuntimeLoweringAbiGate& gate) noexcept
{
    if (gate.facts.empty()) {
        return false;
    }
    const base::u64 expected = gate.facts.front().layout_prototype_count;
    return expected > 0U
        && std::all_of(gate.facts.begin(), gate.facts.end(),
            [expected](const OwnedDynRuntimeLoweringAbiFact& fact) {
                return fact.layout_prototype_count == expected;
            });
}

[[nodiscard]] const char* fallback_name(const std::string& value, const char* fallback) noexcept
{
    return value.empty() ? fallback : value.c_str();
}

} // namespace

std::string_view owned_dyn_runtime_lowering_abi_fact_kind_name(
    const OwnedDynRuntimeLoweringAbiFactKind kind) noexcept
{
    switch (kind) {
        case OwnedDynRuntimeLoweringAbiFactKind::runtime_abi_descriptor:
            return "runtime_abi_descriptor";
        case OwnedDynRuntimeLoweringAbiFactKind::lowering_transition_guard:
            return "lowering_transition_guard";
        case OwnedDynRuntimeLoweringAbiFactKind::backend_helper_prerequisite:
            return "backend_helper_prerequisite";
        case OwnedDynRuntimeLoweringAbiFactKind::drop_allocator_runtime_bridge:
            return "drop_allocator_runtime_bridge";
        case OwnedDynRuntimeLoweringAbiFactKind::dynamic_drop_runtime_blocker:
            return "dynamic_drop_runtime_blocker";
    }
    return "invalid";
}

std::string_view owned_dyn_runtime_lowering_abi_stage_name(
    const OwnedDynRuntimeLoweringAbiStage stage) noexcept
{
    switch (stage) {
        case OwnedDynRuntimeLoweringAbiStage::abi_design_closure:
            return "abi_design_closure";
        case OwnedDynRuntimeLoweringAbiStage::verifier_lowering_guard:
            return "verifier_lowering_guard";
        case OwnedDynRuntimeLoweringAbiStage::blocked_future_runtime:
            return "blocked_future_runtime";
    }
    return "invalid";
}

std::string_view owned_dyn_runtime_lowering_abi_policy_name(
    const OwnedDynRuntimeLoweringAbiPolicy policy) noexcept
{
    switch (policy) {
        case OwnedDynRuntimeLoweringAbiPolicy::compiler_owned_runtime_abi_descriptor_v1:
            return "compiler_owned_runtime_abi_descriptor_v1";
        case OwnedDynRuntimeLoweringAbiPolicy::blocked_to_admitted_transition_check_v1:
            return "blocked_to_admitted_transition_check_v1";
        case OwnedDynRuntimeLoweringAbiPolicy::backend_helper_identity_prerequisite_v1:
            return "backend_helper_identity_prerequisite_v1";
        case OwnedDynRuntimeLoweringAbiPolicy::drop_allocator_runtime_bridge_v1:
            return "drop_allocator_runtime_bridge_v1";
        case OwnedDynRuntimeLoweringAbiPolicy::dynamic_drop_runtime_not_implemented_v1:
            return "dynamic_drop_runtime_not_implemented_v1";
    }
    return "invalid";
}

bool is_valid(const OwnedDynRuntimeLoweringAbiFactKind kind) noexcept
{
    return kind == OwnedDynRuntimeLoweringAbiFactKind::runtime_abi_descriptor
        || kind == OwnedDynRuntimeLoweringAbiFactKind::lowering_transition_guard
        || kind == OwnedDynRuntimeLoweringAbiFactKind::backend_helper_prerequisite
        || kind == OwnedDynRuntimeLoweringAbiFactKind::drop_allocator_runtime_bridge
        || kind == OwnedDynRuntimeLoweringAbiFactKind::dynamic_drop_runtime_blocker;
}

bool is_valid(const OwnedDynRuntimeLoweringAbiStage stage) noexcept
{
    return stage == OwnedDynRuntimeLoweringAbiStage::abi_design_closure
        || stage == OwnedDynRuntimeLoweringAbiStage::verifier_lowering_guard
        || stage == OwnedDynRuntimeLoweringAbiStage::blocked_future_runtime;
}

bool is_valid(const OwnedDynRuntimeLoweringAbiPolicy policy) noexcept
{
    return policy == OwnedDynRuntimeLoweringAbiPolicy::compiler_owned_runtime_abi_descriptor_v1
        || policy == OwnedDynRuntimeLoweringAbiPolicy::blocked_to_admitted_transition_check_v1
        || policy == OwnedDynRuntimeLoweringAbiPolicy::backend_helper_identity_prerequisite_v1
        || policy == OwnedDynRuntimeLoweringAbiPolicy::drop_allocator_runtime_bridge_v1
        || policy == OwnedDynRuntimeLoweringAbiPolicy::dynamic_drop_runtime_not_implemented_v1;
}

bool is_valid(const OwnedDynRuntimeLoweringAbiFact& fact) noexcept
{
    return is_valid(fact.kind) && is_valid(fact.stage) && is_valid(fact.policy)
        && query::is_valid(fact.object_type) && fact_payload_is_named(fact)
        && blockers_are_intact(fact) && kind_policy_stage_match(fact);
}

bool is_valid(
    const OwnedDynRuntimeLoweringAbiSummary& summary,
    const OwnedDynRuntimeLoweringAbiGate& gate) noexcept
{
    return summary_equals(summary, summarize_owned_dyn_runtime_lowering_abi_gate_counts(gate));
}

bool is_valid(const OwnedDynRuntimeLoweringAbiGate& gate) noexcept
{
    return !gate.subject.empty()
        && is_valid_m20c_owned_dyn_drop_allocator_identity_gate(gate.drop_allocator_identity_gate)
        && gate.drop_allocator_identity_gate_fingerprint == gate.drop_allocator_identity_gate.fingerprint
        && gate.drop_allocator_identity_gate_fingerprint
            == owned_dyn_drop_allocator_identity_gate_fingerprint(gate.drop_allocator_identity_gate)
        && !gate.facts.empty()
        && std::all_of(gate.facts.begin(), gate.facts.end(),
            [](const OwnedDynRuntimeLoweringAbiFact& fact) {
                return is_valid(fact);
            })
        && fact_keys_are_consistent(gate) && m20c_identity_gate_keys_are_consistent(gate)
        && fact_keys_match_m20c_identity_gate(gate) && layout_prototype_counts_are_consistent(gate)
        && is_valid(gate.summary, gate)
        && (gate.fingerprint == StableFingerprint128{}
            || gate.fingerprint == owned_dyn_runtime_lowering_abi_gate_fingerprint(gate));
}

bool is_valid_m20d_owned_dyn_runtime_lowering_abi_gate(
    const OwnedDynRuntimeLoweringAbiGate& gate) noexcept
{
    return is_valid(gate)
        && std::string_view(gate.subject) == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_M20D_SUBJECT
        && gate.summary.fact_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FACT_COUNT
        && gate.summary.runtime_abi_descriptor_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_SINGLE_FACT
        && gate.summary.lowering_transition_guard_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_SINGLE_FACT
        && gate.summary.backend_helper_prerequisite_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_SINGLE_FACT
        && gate.summary.drop_allocator_runtime_bridge_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_SINGLE_FACT
        && gate.summary.dynamic_drop_runtime_blocker_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_SINGLE_FACT
        && gate.summary.m20c_reference_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FACT_COUNT
        && gate.summary.compiler_owned_runtime_abi_descriptor_count
            == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FACT_COUNT
        && gate.summary.runtime_abi_descriptor_visible_count == 4U
        && gate.summary.lowering_transition_guard_visible_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_SINGLE_FACT
        && gate.summary.backend_helper_prerequisite_visible_count
            == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_SINGLE_FACT
        && gate.summary.drop_allocator_runtime_bridge_visible_count
            == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_SINGLE_FACT
        && gate.summary.dynamic_drop_runtime_blocker_visible_count
            == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_SINGLE_FACT
        && gate.summary.borrowed_dyn_abi_unchanged_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FACT_COUNT
        && gate.summary.standard_library_api_blocked_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FACT_COUNT
        && gate.summary.box_dyn_surface_blocked_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FACT_COUNT
        && gate.summary.owning_dyn_user_value_blocked_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FACT_COUNT
        && gate.summary.allocator_api_blocked_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FACT_COUNT
        && gate.summary.runtime_lowering_blocked_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FACT_COUNT
        && gate.summary.dynamic_drop_runtime_blocked_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FACT_COUNT
        && gate.summary.backend_helper_blocked_count == QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FACT_COUNT
        && gate.summary.backend_helper_callable_count == 0U
        && gate.summary.executable_runtime_implemented_count == 0U
        && gate.summary.observed_layout_prototype_total > 0U && m20d_fact_kinds_are_complete(gate);
}

void record_owned_dyn_runtime_lowering_abi_fact(
    OwnedDynRuntimeLoweringAbiGate& gate,
    OwnedDynRuntimeLoweringAbiFact fact)
{
    gate.facts.push_back(std::move(fact));
    gate.summary = summarize_owned_dyn_runtime_lowering_abi_gate_counts(gate);
}

OwnedDynRuntimeLoweringAbiSummary summarize_owned_dyn_runtime_lowering_abi_gate_counts(
    const OwnedDynRuntimeLoweringAbiGate& gate) noexcept
{
    OwnedDynRuntimeLoweringAbiSummary summary;
    summary.fact_count = static_cast<base::u64>(gate.facts.size());
    for (const OwnedDynRuntimeLoweringAbiFact& fact : gate.facts) {
        switch (fact.kind) {
            case OwnedDynRuntimeLoweringAbiFactKind::runtime_abi_descriptor:
                ++summary.runtime_abi_descriptor_count;
                break;
            case OwnedDynRuntimeLoweringAbiFactKind::lowering_transition_guard:
                ++summary.lowering_transition_guard_count;
                break;
            case OwnedDynRuntimeLoweringAbiFactKind::backend_helper_prerequisite:
                ++summary.backend_helper_prerequisite_count;
                break;
            case OwnedDynRuntimeLoweringAbiFactKind::drop_allocator_runtime_bridge:
                ++summary.drop_allocator_runtime_bridge_count;
                break;
            case OwnedDynRuntimeLoweringAbiFactKind::dynamic_drop_runtime_blocker:
                ++summary.dynamic_drop_runtime_blocker_count;
                break;
        }
        if (fact.references_m20c_drop_allocator_identity_gate) {
            ++summary.m20c_reference_count;
        }
        if (fact.compiler_owned_runtime_abi_descriptor) {
            ++summary.compiler_owned_runtime_abi_descriptor_count;
        }
        if (fact.runtime_abi_descriptor_visible) {
            ++summary.runtime_abi_descriptor_visible_count;
        }
        if (fact.lowering_transition_guard_visible) {
            ++summary.lowering_transition_guard_visible_count;
        }
        if (fact.backend_helper_prerequisite_visible) {
            ++summary.backend_helper_prerequisite_visible_count;
        }
        if (fact.drop_allocator_runtime_bridge_visible) {
            ++summary.drop_allocator_runtime_bridge_visible_count;
        }
        if (fact.dynamic_drop_runtime_blocker_visible) {
            ++summary.dynamic_drop_runtime_blocker_visible_count;
        }
        if (fact.borrowed_dyn_abi_unchanged) {
            ++summary.borrowed_dyn_abi_unchanged_count;
        }
        if (fact.standard_library_api_blocked) {
            ++summary.standard_library_api_blocked_count;
        }
        if (fact.box_dyn_surface_blocked) {
            ++summary.box_dyn_surface_blocked_count;
        }
        if (fact.owning_dyn_user_value_blocked) {
            ++summary.owning_dyn_user_value_blocked_count;
        }
        if (fact.allocator_api_blocked) {
            ++summary.allocator_api_blocked_count;
        }
        if (fact.runtime_lowering_blocked) {
            ++summary.runtime_lowering_blocked_count;
        }
        if (fact.dynamic_drop_runtime_blocked) {
            ++summary.dynamic_drop_runtime_blocked_count;
        }
        if (fact.backend_helper_blocked) {
            ++summary.backend_helper_blocked_count;
        }
        if (fact.backend_helper_callable) {
            ++summary.backend_helper_callable_count;
        }
        if (fact.executable_runtime_implemented) {
            ++summary.executable_runtime_implemented_count;
        }
        summary.observed_layout_prototype_total =
            std::max(summary.observed_layout_prototype_total, fact.layout_prototype_count);
    }
    return summary;
}

StableFingerprint128 owned_dyn_runtime_lowering_abi_gate_fingerprint(
    const OwnedDynRuntimeLoweringAbiGate& gate) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_FINGERPRINT_MARKER);
    builder.mix_string(gate.subject);
    builder.mix_fingerprint(gate.drop_allocator_identity_gate_fingerprint);
    mix_summary(builder, gate.summary);
    builder.mix_u64(static_cast<base::u64>(gate.facts.size()));
    for (const OwnedDynRuntimeLoweringAbiFact& fact : gate.facts) {
        mix_fact(builder, fact);
    }
    return builder.finish();
}

std::string summarize_owned_dyn_runtime_lowering_abi_gate(
    const OwnedDynRuntimeLoweringAbiGate& gate)
{
    std::ostringstream label;
    label << "owned_dyn_runtime_lowering_abi_gate subject="
          << (gate.subject.empty() ? "<anonymous>" : gate.subject)
          << " facts=" << gate.summary.fact_count
          << " runtime_abi_descriptor=" << gate.summary.runtime_abi_descriptor_count
          << " transition_guard=" << gate.summary.lowering_transition_guard_count
          << " backend_helper_prerequisite=" << gate.summary.backend_helper_prerequisite_count
          << " drop_allocator_bridge=" << gate.summary.drop_allocator_runtime_bridge_count
          << " dynamic_drop_blocker=" << gate.summary.dynamic_drop_runtime_blocker_count
          << " m20c_refs=" << gate.summary.m20c_reference_count
          << " descriptor_visible=" << gate.summary.runtime_abi_descriptor_visible_count
          << " backend_helper_blocked=" << gate.summary.backend_helper_blocked_count
          << " backend_helper_callable=" << gate.summary.backend_helper_callable_count
          << " runtime_lowering_blocked=" << gate.summary.runtime_lowering_blocked_count
          << " dynamic_drop_runtime_blocked=" << gate.summary.dynamic_drop_runtime_blocked_count
          << " executable_runtime_implemented=" << gate.summary.executable_runtime_implemented_count;
    if (!gate.facts.empty()) {
        label << " first_fact=" << gate.facts.front().fact_name
              << " policy=" << owned_dyn_runtime_lowering_abi_policy_name(gate.facts.front().policy);
    }
    label << " fingerprint=" << debug_string(owned_dyn_runtime_lowering_abi_gate_fingerprint(gate));
    return label.str();
}

std::string dump_owned_dyn_runtime_lowering_abi_gate(
    const OwnedDynRuntimeLoweringAbiGate& gate)
{
    std::ostringstream stream;
    stream << "owned_dyn_runtime_lowering_abi_gate subject="
           << (gate.subject.empty() ? "<anonymous>" : gate.subject)
           << " facts=" << gate.summary.fact_count
           << " drop_allocator_identity_gate=" << debug_string(gate.drop_allocator_identity_gate_fingerprint)
           << " fingerprint=" << debug_string(owned_dyn_runtime_lowering_abi_gate_fingerprint(gate)) << '\n';
    for (base::usize index = 0; index < gate.facts.size(); ++index) {
        const OwnedDynRuntimeLoweringAbiFact& fact = gate.facts[index];
        stream << "  runtime_abi #" << index << " name=" << fact.fact_name
               << " kind=" << owned_dyn_runtime_lowering_abi_fact_kind_name(fact.kind)
               << " stage=" << owned_dyn_runtime_lowering_abi_stage_name(fact.stage)
               << " policy=" << owned_dyn_runtime_lowering_abi_policy_name(fact.policy)
               << " m20c=" << (fact.references_m20c_drop_allocator_identity_gate ? "yes" : "no")
               << " compiler_owned_descriptor="
               << (fact.compiler_owned_runtime_abi_descriptor ? "yes" : "no")
               << " descriptor_visible=" << (fact.runtime_abi_descriptor_visible ? "yes" : "no")
               << " transition_guard=" << (fact.lowering_transition_guard_visible ? "yes" : "no")
               << " backend_helper_prerequisite="
               << (fact.backend_helper_prerequisite_visible ? "yes" : "no")
               << " drop_allocator_bridge="
               << (fact.drop_allocator_runtime_bridge_visible ? "yes" : "no")
               << " dynamic_drop_blocker="
               << (fact.dynamic_drop_runtime_blocker_visible ? "yes" : "no")
               << " borrowed_abi_unchanged=" << (fact.borrowed_dyn_abi_unchanged ? "yes" : "no")
               << " stdlib_blocked=" << (fact.standard_library_api_blocked ? "yes" : "no")
               << " box_blocked=" << (fact.box_dyn_surface_blocked ? "yes" : "no")
               << " owning_value_blocked=" << (fact.owning_dyn_user_value_blocked ? "yes" : "no")
               << " allocator_api_blocked=" << (fact.allocator_api_blocked ? "yes" : "no")
               << " runtime_lowering_blocked=" << (fact.runtime_lowering_blocked ? "yes" : "no")
               << " dynamic_drop_blocked=" << (fact.dynamic_drop_runtime_blocked ? "yes" : "no")
               << " backend_helper_blocked=" << (fact.backend_helper_blocked ? "yes" : "no")
               << " backend_helper_callable=" << (fact.backend_helper_callable ? "yes" : "no")
               << " executable_runtime_implemented="
               << (fact.executable_runtime_implemented ? "yes" : "no")
               << " object_key=" << fact.object_type.global_id
               << " drop_key=" << debug_string(fact.drop_identity_key)
               << " allocator_key=" << debug_string(fact.allocator_identity_key)
               << " identity_set_key=" << debug_string(fact.prototype_identity_set_key)
               << " runtime_abi_descriptor_key=" << debug_string(fact.runtime_abi_descriptor_key)
               << " backend_helper_identity_key=" << debug_string(fact.backend_helper_identity_key)
               << " prototype_count=" << fact.layout_prototype_count
               << " m20c_gate=" << fallback_name(fact.m20c_gate_fact, "<unknown>")
               << " verifier_guard=" << fallback_name(fact.verifier_guard_fact, "<unknown>")
               << " blocked_surface=" << fallback_name(fact.blocked_surface_fact, "<unknown>") << '\n';
    }
    return stream.str();
}

OwnedDynRuntimeLoweringAbiGate m20d_owned_dyn_runtime_lowering_abi_gate_baseline()
{
    OwnedDynRuntimeLoweringAbiGate gate;
    gate.subject = std::string(QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_M20D_SUBJECT);
    gate.drop_allocator_identity_gate = m20c_owned_dyn_drop_allocator_identity_gate_baseline();
    gate.drop_allocator_identity_gate_fingerprint = gate.drop_allocator_identity_gate.fingerprint;

    record_owned_dyn_runtime_lowering_abi_fact(gate,
        abi_fact(gate.drop_allocator_identity_gate,
            OwnedDynRuntimeLoweringAbiFactKind::runtime_abi_descriptor,
            OwnedDynRuntimeLoweringAbiStage::abi_design_closure,
            OwnedDynRuntimeLoweringAbiPolicy::compiler_owned_runtime_abi_descriptor_v1,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_DESCRIPTOR_FACT,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_DESCRIPTOR_GUARD,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_RUNTIME));
    record_owned_dyn_runtime_lowering_abi_fact(gate,
        abi_fact(gate.drop_allocator_identity_gate,
            OwnedDynRuntimeLoweringAbiFactKind::lowering_transition_guard,
            OwnedDynRuntimeLoweringAbiStage::verifier_lowering_guard,
            OwnedDynRuntimeLoweringAbiPolicy::blocked_to_admitted_transition_check_v1,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_TRANSITION_FACT,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_TRANSITION_GUARD,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_STDLIB));
    record_owned_dyn_runtime_lowering_abi_fact(gate,
        abi_fact(gate.drop_allocator_identity_gate,
            OwnedDynRuntimeLoweringAbiFactKind::backend_helper_prerequisite,
            OwnedDynRuntimeLoweringAbiStage::abi_design_closure,
            OwnedDynRuntimeLoweringAbiPolicy::backend_helper_identity_prerequisite_v1,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_BACKEND_FACT,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_BACKEND_GUARD,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_BACKEND_HELPER));
    record_owned_dyn_runtime_lowering_abi_fact(gate,
        abi_fact(gate.drop_allocator_identity_gate,
            OwnedDynRuntimeLoweringAbiFactKind::drop_allocator_runtime_bridge,
            OwnedDynRuntimeLoweringAbiStage::verifier_lowering_guard,
            OwnedDynRuntimeLoweringAbiPolicy::drop_allocator_runtime_bridge_v1,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_BRIDGE_FACT,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_BRIDGE_GUARD,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_ALLOCATOR));
    record_owned_dyn_runtime_lowering_abi_fact(gate,
        abi_fact(gate.drop_allocator_identity_gate,
            OwnedDynRuntimeLoweringAbiFactKind::dynamic_drop_runtime_blocker,
            OwnedDynRuntimeLoweringAbiStage::blocked_future_runtime,
            OwnedDynRuntimeLoweringAbiPolicy::dynamic_drop_runtime_not_implemented_v1,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_DYNAMIC_DROP_FACT,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_DYNAMIC_DROP_GUARD,
            QUERY_OWNED_DYN_RUNTIME_LOWERING_ABI_NO_DYNAMIC_DROP));

    gate.fingerprint = owned_dyn_runtime_lowering_abi_gate_fingerprint(gate);
    return gate;
}

} // namespace aurex::query
