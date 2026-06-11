#include <aurex/infrastructure/query/owned_dyn_drop_allocator_identity_gate.hpp>

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_FINGERPRINT_MARKER =
    "query.owned_dyn_drop_allocator_identity_gate.v1";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_M20C_SUBJECT =
    "M20c Drop / Allocator Identity Prerequisite Gate";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_DROP_FACT =
    "owned_dyn_erased_drop_identity_prerequisite_fact";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_ALLOCATOR_FACT =
    "owned_dyn_allocator_identity_prerequisite_fact";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_CLEANUP_FACT =
    "owned_dyn_cleanup_dropck_bridge_identity_fact";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_BINDING_FACT = "owned_dyn_handle_identity_binding_fact";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_RUNTIME_FACT =
    "owned_dyn_drop_allocator_runtime_lowering_blocker_fact";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_M20B_GATE = "requires_m20b_owned_dyn_ir_shape_prototype_gate";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_DROP_KEY = "m20c.compiler_owned_erased_drop_identity";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_ALLOCATOR_KEY = "m20c.compiler_owned_allocator_identity";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_BASELINE_IDENTITY_SET =
    "m20c.baseline.prototype_identity_set";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_BASELINE_MODULE = "m20c_owned_dyn_identity";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_BASELINE_TRAIT = "OwnedDynIdentityBaseline";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_DROP_GUARD =
    "verifier_requires_compiler_owned_erased_drop_identity";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_ALLOCATOR_GUARD =
    "verifier_requires_compiler_owned_allocator_identity";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_CLEANUP_GUARD =
    "verifier_keeps_cleanup_dropck_static_until_runtime";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_BINDING_GUARD =
    "verifier_binds_identity_keys_to_owned_handle_prototype";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_RUNTIME_GUARD =
    "verifier_keeps_drop_allocator_runtime_lowering_blocked";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_NO_STDLIB = "standard_library_api_not_in_m20c";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_NO_BOX = "box_dyn_trait_not_in_m20c";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_NO_ALLOCATOR = "allocator_api_not_in_m20c";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_NO_RUNTIME = "runtime_abi_lowering_not_in_m20c";
constexpr std::string_view QUERY_OWNED_DYN_DROP_ALLOCATOR_NO_DYNAMIC_DROP = "dynamic_drop_runtime_not_in_m20c";
constexpr base::u8 QUERY_OWNED_DYN_DROP_ALLOCATOR_INVALID_ENUM_VALUE = 255U;
constexpr base::u64 QUERY_OWNED_DYN_DROP_ALLOCATOR_FACT_COUNT = 5U;
constexpr base::u64 QUERY_OWNED_DYN_DROP_ALLOCATOR_SINGLE_FACT = 1U;

[[nodiscard]] base::u8 stable_kind_value(const OwnedDynDropAllocatorIdentityFactKind kind) noexcept
{
    return is_valid(kind) ? static_cast<base::u8>(kind) : QUERY_OWNED_DYN_DROP_ALLOCATOR_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_stage_value(const OwnedDynDropAllocatorIdentityStage stage) noexcept
{
    return is_valid(stage) ? static_cast<base::u8>(stage) : QUERY_OWNED_DYN_DROP_ALLOCATOR_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_policy_value(const OwnedDynDropAllocatorIdentityPolicy policy) noexcept
{
    return is_valid(policy) ? static_cast<base::u8>(policy) : QUERY_OWNED_DYN_DROP_ALLOCATOR_INVALID_ENUM_VALUE;
}

[[nodiscard]] bool nonempty(const std::string_view value) noexcept
{
    return !value.empty();
}

[[nodiscard]] bool is_nonzero(const StableFingerprint128 fingerprint) noexcept
{
    return fingerprint != StableFingerprint128{};
}

[[nodiscard]] bool fact_payload_is_named(const OwnedDynDropAllocatorIdentityFact& fact) noexcept
{
    return nonempty(fact.fact_name) && nonempty(fact.subject_symbol) && nonempty(fact.m20b_gate_fact)
        && nonempty(fact.verifier_guard_fact) && nonempty(fact.blocked_surface_fact);
}

[[nodiscard]] bool blockers_are_intact(const OwnedDynDropAllocatorIdentityFact& fact) noexcept
{
    return fact.references_m20b_ir_shape_gate && fact.compiler_owned_identity && fact.borrowed_dyn_abi_unchanged
        && fact.standard_library_api_blocked && fact.box_dyn_surface_blocked && fact.owning_dyn_user_value_blocked
        && fact.allocator_api_blocked && fact.runtime_lowering_blocked && fact.dynamic_drop_runtime_blocked
        && fact.backend_helper_blocked && !fact.executable_runtime_implemented && fact.layout_prototype_count > 0U
        && is_nonzero(fact.drop_identity_key) && is_nonzero(fact.allocator_identity_key)
        && is_nonzero(fact.prototype_identity_set_key) && fact.drop_identity_key != fact.allocator_identity_key;
}

[[nodiscard]] bool kind_policy_stage_match(const OwnedDynDropAllocatorIdentityFact& fact) noexcept
{
    switch (fact.kind) {
        case OwnedDynDropAllocatorIdentityFactKind::erased_drop_identity:
            return fact.stage == OwnedDynDropAllocatorIdentityStage::identity_prerequisite
                && fact.policy == OwnedDynDropAllocatorIdentityPolicy::compiler_owned_erased_drop_identity_v1
                && fact.drop_identity_visible && !fact.allocator_identity_visible && !fact.cleanup_dropck_bridge_visible
                && !fact.owned_handle_binding_visible;
        case OwnedDynDropAllocatorIdentityFactKind::allocator_identity:
            return fact.stage == OwnedDynDropAllocatorIdentityStage::identity_prerequisite
                && fact.policy == OwnedDynDropAllocatorIdentityPolicy::compiler_owned_allocator_identity_v1
                && fact.allocator_identity_visible && !fact.drop_identity_visible && !fact.cleanup_dropck_bridge_visible
                && !fact.owned_handle_binding_visible;
        case OwnedDynDropAllocatorIdentityFactKind::cleanup_dropck_bridge:
            return fact.stage == OwnedDynDropAllocatorIdentityStage::verifier_identity_guard
                && fact.policy == OwnedDynDropAllocatorIdentityPolicy::cleanup_dropck_static_bridge_v1
                && fact.cleanup_dropck_bridge_visible && fact.drop_identity_visible && !fact.allocator_identity_visible
                && !fact.owned_handle_binding_visible;
        case OwnedDynDropAllocatorIdentityFactKind::owned_handle_identity_binding:
            return fact.stage == OwnedDynDropAllocatorIdentityStage::verifier_identity_guard
                && fact.policy == OwnedDynDropAllocatorIdentityPolicy::owned_handle_identity_binding_v1
                && fact.owned_handle_binding_visible && fact.drop_identity_visible && fact.allocator_identity_visible
                && !fact.cleanup_dropck_bridge_visible;
        case OwnedDynDropAllocatorIdentityFactKind::runtime_lowering_blocker:
            return fact.stage == OwnedDynDropAllocatorIdentityStage::blocked_future_runtime
                && fact.policy == OwnedDynDropAllocatorIdentityPolicy::runtime_lowering_not_implemented_v1
                && !fact.drop_identity_visible && !fact.allocator_identity_visible
                && !fact.cleanup_dropck_bridge_visible && !fact.owned_handle_binding_visible;
    }
    return false;
}

[[nodiscard]] bool summary_equals(
    const OwnedDynDropAllocatorIdentitySummary& lhs, const OwnedDynDropAllocatorIdentitySummary& rhs) noexcept
{
    return lhs.fact_count == rhs.fact_count && lhs.erased_drop_identity_count == rhs.erased_drop_identity_count
        && lhs.allocator_identity_count == rhs.allocator_identity_count
        && lhs.cleanup_dropck_bridge_count == rhs.cleanup_dropck_bridge_count
        && lhs.owned_handle_identity_binding_count == rhs.owned_handle_identity_binding_count
        && lhs.runtime_lowering_blocker_count == rhs.runtime_lowering_blocker_count
        && lhs.m20b_reference_count == rhs.m20b_reference_count
        && lhs.compiler_owned_identity_count == rhs.compiler_owned_identity_count
        && lhs.drop_identity_visible_count == rhs.drop_identity_visible_count
        && lhs.allocator_identity_visible_count == rhs.allocator_identity_visible_count
        && lhs.cleanup_dropck_bridge_visible_count == rhs.cleanup_dropck_bridge_visible_count
        && lhs.owned_handle_binding_visible_count == rhs.owned_handle_binding_visible_count
        && lhs.borrowed_dyn_abi_unchanged_count == rhs.borrowed_dyn_abi_unchanged_count
        && lhs.standard_library_api_blocked_count == rhs.standard_library_api_blocked_count
        && lhs.box_dyn_surface_blocked_count == rhs.box_dyn_surface_blocked_count
        && lhs.owning_dyn_user_value_blocked_count == rhs.owning_dyn_user_value_blocked_count
        && lhs.allocator_api_blocked_count == rhs.allocator_api_blocked_count
        && lhs.runtime_lowering_blocked_count == rhs.runtime_lowering_blocked_count
        && lhs.dynamic_drop_runtime_blocked_count == rhs.dynamic_drop_runtime_blocked_count
        && lhs.backend_helper_blocked_count == rhs.backend_helper_blocked_count
        && lhs.executable_runtime_implemented_count == rhs.executable_runtime_implemented_count
        && lhs.observed_layout_prototype_total == rhs.observed_layout_prototype_total;
}

void mix_summary(StableHashBuilder& builder, const OwnedDynDropAllocatorIdentitySummary& summary) noexcept
{
    builder.mix_u64(summary.fact_count);
    builder.mix_u64(summary.erased_drop_identity_count);
    builder.mix_u64(summary.allocator_identity_count);
    builder.mix_u64(summary.cleanup_dropck_bridge_count);
    builder.mix_u64(summary.owned_handle_identity_binding_count);
    builder.mix_u64(summary.runtime_lowering_blocker_count);
    builder.mix_u64(summary.m20b_reference_count);
    builder.mix_u64(summary.compiler_owned_identity_count);
    builder.mix_u64(summary.drop_identity_visible_count);
    builder.mix_u64(summary.allocator_identity_visible_count);
    builder.mix_u64(summary.cleanup_dropck_bridge_visible_count);
    builder.mix_u64(summary.owned_handle_binding_visible_count);
    builder.mix_u64(summary.borrowed_dyn_abi_unchanged_count);
    builder.mix_u64(summary.standard_library_api_blocked_count);
    builder.mix_u64(summary.box_dyn_surface_blocked_count);
    builder.mix_u64(summary.owning_dyn_user_value_blocked_count);
    builder.mix_u64(summary.allocator_api_blocked_count);
    builder.mix_u64(summary.runtime_lowering_blocked_count);
    builder.mix_u64(summary.dynamic_drop_runtime_blocked_count);
    builder.mix_u64(summary.backend_helper_blocked_count);
    builder.mix_u64(summary.executable_runtime_implemented_count);
    builder.mix_u64(summary.observed_layout_prototype_total);
}

void mix_fact(StableHashBuilder& builder, const OwnedDynDropAllocatorIdentityFact& fact) noexcept
{
    builder.mix_string(fact.fact_name);
    builder.mix_u8(stable_kind_value(fact.kind));
    builder.mix_u8(stable_stage_value(fact.stage));
    builder.mix_u8(stable_policy_value(fact.policy));
    builder.mix_bool(fact.references_m20b_ir_shape_gate);
    builder.mix_bool(fact.compiler_owned_identity);
    builder.mix_bool(fact.drop_identity_visible);
    builder.mix_bool(fact.allocator_identity_visible);
    builder.mix_bool(fact.cleanup_dropck_bridge_visible);
    builder.mix_bool(fact.owned_handle_binding_visible);
    builder.mix_bool(fact.borrowed_dyn_abi_unchanged);
    builder.mix_bool(fact.standard_library_api_blocked);
    builder.mix_bool(fact.box_dyn_surface_blocked);
    builder.mix_bool(fact.owning_dyn_user_value_blocked);
    builder.mix_bool(fact.allocator_api_blocked);
    builder.mix_bool(fact.runtime_lowering_blocked);
    builder.mix_bool(fact.dynamic_drop_runtime_blocked);
    builder.mix_bool(fact.backend_helper_blocked);
    builder.mix_bool(fact.executable_runtime_implemented);
    builder.mix_u64(fact.object_type.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(fact.object_type));
    builder.mix_fingerprint(fact.drop_identity_key);
    builder.mix_fingerprint(fact.allocator_identity_key);
    builder.mix_fingerprint(fact.prototype_identity_set_key);
    builder.mix_u64(fact.layout_prototype_count);
    builder.mix_string(fact.subject_symbol);
    builder.mix_string(fact.m20b_gate_fact);
    builder.mix_string(fact.verifier_guard_fact);
    builder.mix_string(fact.blocked_surface_fact);
}

[[nodiscard]] StableFingerprint128 identity_key(
    const std::string_view identity_kind, const TraitObjectTypeKey& object_type)
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_OWNED_DYN_DROP_ALLOCATOR_M20C_SUBJECT);
    builder.mix_string(identity_kind);
    builder.mix_u64(object_type.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(object_type));
    return builder.finish();
}

[[nodiscard]] StableFingerprint128 baseline_identity_set_key(const TraitObjectTypeKey& object_type,
    const StableFingerprint128 drop_identity_key, const StableFingerprint128 allocator_identity_key)
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_OWNED_DYN_DROP_ALLOCATOR_BASELINE_IDENTITY_SET);
    builder.mix_u64(object_type.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(object_type));
    builder.mix_fingerprint(drop_identity_key);
    builder.mix_fingerprint(allocator_identity_key);
    return builder.finish();
}

[[nodiscard]] TraitObjectTypeKey baseline_object_type_key()
{
    const StableModuleId module = stable_module_id(std::span<const std::string_view>{
        &QUERY_OWNED_DYN_DROP_ALLOCATOR_BASELINE_MODULE,
        1U,
    });
    const StableDefId trait =
        stable_definition_id(module, StableSymbolKind::type, QUERY_OWNED_DYN_DROP_ALLOCATOR_BASELINE_TRAIT);
    return trait_object_type_key(def_key_from_stable_id(trait, DefNamespace::trait_, DefKind::trait_),
        std::span<const CanonicalTypeKey>{}, std::span<const TraitObjectAssociatedTypeEqualityKey>{},
        stable_fingerprint("m20c.baseline.object_origin"),
        stable_fingerprint("m20c.baseline.object_callability_schema"));
}

[[nodiscard]] OwnedDynDropAllocatorIdentityFact identity_fact(const OwnedDynDropAllocatorIdentityFactKind kind,
    const OwnedDynDropAllocatorIdentityStage stage, const OwnedDynDropAllocatorIdentityPolicy policy,
    const std::string_view fact_name, const std::string_view verifier_guard, const std::string_view blocked_surface)
{
    const OwnedDynIrShapePrototypeGate shape_gate = m20b_owned_dyn_ir_shape_prototype_gate_baseline();
    const TraitObjectTypeKey object_type = baseline_object_type_key();

    OwnedDynDropAllocatorIdentityFact fact;
    fact.fact_name = std::string(fact_name);
    fact.kind = kind;
    fact.stage = stage;
    fact.policy = policy;
    fact.drop_identity_visible = kind == OwnedDynDropAllocatorIdentityFactKind::erased_drop_identity
        || kind == OwnedDynDropAllocatorIdentityFactKind::cleanup_dropck_bridge
        || kind == OwnedDynDropAllocatorIdentityFactKind::owned_handle_identity_binding;
    fact.allocator_identity_visible = kind == OwnedDynDropAllocatorIdentityFactKind::allocator_identity
        || kind == OwnedDynDropAllocatorIdentityFactKind::owned_handle_identity_binding;
    fact.cleanup_dropck_bridge_visible = kind == OwnedDynDropAllocatorIdentityFactKind::cleanup_dropck_bridge;
    fact.owned_handle_binding_visible = kind == OwnedDynDropAllocatorIdentityFactKind::owned_handle_identity_binding;
    fact.object_type = object_type;
    fact.drop_identity_key = identity_key(QUERY_OWNED_DYN_DROP_ALLOCATOR_DROP_KEY, object_type);
    fact.allocator_identity_key = identity_key(QUERY_OWNED_DYN_DROP_ALLOCATOR_ALLOCATOR_KEY, object_type);
    fact.prototype_identity_set_key =
        baseline_identity_set_key(object_type, fact.drop_identity_key, fact.allocator_identity_key);
    fact.layout_prototype_count = shape_gate.summary.observed_layout_prototype_total;
    fact.subject_symbol = std::string(QUERY_OWNED_DYN_DROP_ALLOCATOR_M20C_SUBJECT);
    fact.m20b_gate_fact = std::string(QUERY_OWNED_DYN_DROP_ALLOCATOR_M20B_GATE);
    fact.verifier_guard_fact = std::string(verifier_guard);
    fact.blocked_surface_fact = std::string(blocked_surface);
    return fact;
}

[[nodiscard]] bool m20c_fact_kinds_are_complete(const OwnedDynDropAllocatorIdentityGate& gate) noexcept
{
    std::array<bool, QUERY_OWNED_DYN_DROP_ALLOCATOR_FACT_COUNT> seen{};
    for (const OwnedDynDropAllocatorIdentityFact& fact : gate.facts) {
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

[[nodiscard]] bool prototype_identity_set_keys_are_consistent(const OwnedDynDropAllocatorIdentityGate& gate) noexcept
{
    if (gate.facts.empty()) {
        return false;
    }
    const StableFingerprint128 expected = gate.facts.front().prototype_identity_set_key;
    return expected != StableFingerprint128{}
    && std::all_of(gate.facts.begin(), gate.facts.end(), [expected](const OwnedDynDropAllocatorIdentityFact& fact) {
           return fact.prototype_identity_set_key == expected;
       });
}

[[nodiscard]] bool layout_prototype_counts_are_consistent(const OwnedDynDropAllocatorIdentityGate& gate) noexcept
{
    if (gate.facts.empty()) {
        return false;
    }
    const base::u64 expected = gate.facts.front().layout_prototype_count;
    return expected > 0U
        && std::all_of(gate.facts.begin(), gate.facts.end(), [expected](const OwnedDynDropAllocatorIdentityFact& fact) {
               return fact.layout_prototype_count == expected;
           });
}

[[nodiscard]] const char* fallback_name(const std::string& value, const char* fallback) noexcept
{
    return value.empty() ? fallback : value.c_str();
}

} // namespace

std::string_view owned_dyn_drop_allocator_identity_fact_kind_name(
    const OwnedDynDropAllocatorIdentityFactKind kind) noexcept
{
    switch (kind) {
        case OwnedDynDropAllocatorIdentityFactKind::erased_drop_identity:
            return "erased_drop_identity";
        case OwnedDynDropAllocatorIdentityFactKind::allocator_identity:
            return "allocator_identity";
        case OwnedDynDropAllocatorIdentityFactKind::cleanup_dropck_bridge:
            return "cleanup_dropck_bridge";
        case OwnedDynDropAllocatorIdentityFactKind::owned_handle_identity_binding:
            return "owned_handle_identity_binding";
        case OwnedDynDropAllocatorIdentityFactKind::runtime_lowering_blocker:
            return "runtime_lowering_blocker";
    }
    return "invalid";
}

std::string_view owned_dyn_drop_allocator_identity_stage_name(const OwnedDynDropAllocatorIdentityStage stage) noexcept
{
    switch (stage) {
        case OwnedDynDropAllocatorIdentityStage::identity_prerequisite:
            return "identity_prerequisite";
        case OwnedDynDropAllocatorIdentityStage::verifier_identity_guard:
            return "verifier_identity_guard";
        case OwnedDynDropAllocatorIdentityStage::blocked_future_runtime:
            return "blocked_future_runtime";
    }
    return "invalid";
}

std::string_view owned_dyn_drop_allocator_identity_policy_name(
    const OwnedDynDropAllocatorIdentityPolicy policy) noexcept
{
    switch (policy) {
        case OwnedDynDropAllocatorIdentityPolicy::compiler_owned_erased_drop_identity_v1:
            return "compiler_owned_erased_drop_identity_v1";
        case OwnedDynDropAllocatorIdentityPolicy::compiler_owned_allocator_identity_v1:
            return "compiler_owned_allocator_identity_v1";
        case OwnedDynDropAllocatorIdentityPolicy::cleanup_dropck_static_bridge_v1:
            return "cleanup_dropck_static_bridge_v1";
        case OwnedDynDropAllocatorIdentityPolicy::owned_handle_identity_binding_v1:
            return "owned_handle_identity_binding_v1";
        case OwnedDynDropAllocatorIdentityPolicy::runtime_lowering_not_implemented_v1:
            return "runtime_lowering_not_implemented_v1";
    }
    return "invalid";
}

bool is_valid(const OwnedDynDropAllocatorIdentityFactKind kind) noexcept
{
    return kind == OwnedDynDropAllocatorIdentityFactKind::erased_drop_identity
        || kind == OwnedDynDropAllocatorIdentityFactKind::allocator_identity
        || kind == OwnedDynDropAllocatorIdentityFactKind::cleanup_dropck_bridge
        || kind == OwnedDynDropAllocatorIdentityFactKind::owned_handle_identity_binding
        || kind == OwnedDynDropAllocatorIdentityFactKind::runtime_lowering_blocker;
}

bool is_valid(const OwnedDynDropAllocatorIdentityStage stage) noexcept
{
    return stage == OwnedDynDropAllocatorIdentityStage::identity_prerequisite
        || stage == OwnedDynDropAllocatorIdentityStage::verifier_identity_guard
        || stage == OwnedDynDropAllocatorIdentityStage::blocked_future_runtime;
}

bool is_valid(const OwnedDynDropAllocatorIdentityPolicy policy) noexcept
{
    return policy == OwnedDynDropAllocatorIdentityPolicy::compiler_owned_erased_drop_identity_v1
        || policy == OwnedDynDropAllocatorIdentityPolicy::compiler_owned_allocator_identity_v1
        || policy == OwnedDynDropAllocatorIdentityPolicy::cleanup_dropck_static_bridge_v1
        || policy == OwnedDynDropAllocatorIdentityPolicy::owned_handle_identity_binding_v1
        || policy == OwnedDynDropAllocatorIdentityPolicy::runtime_lowering_not_implemented_v1;
}

bool is_valid(const OwnedDynDropAllocatorIdentityFact& fact) noexcept
{
    return is_valid(fact.kind) && is_valid(fact.stage) && is_valid(fact.policy) && query::is_valid(fact.object_type)
        && fact_payload_is_named(fact) && blockers_are_intact(fact) && kind_policy_stage_match(fact);
}

bool is_valid(
    const OwnedDynDropAllocatorIdentitySummary& summary, const OwnedDynDropAllocatorIdentityGate& gate) noexcept
{
    return summary_equals(summary, summarize_owned_dyn_drop_allocator_identity_gate_counts(gate));
}

bool is_valid(const OwnedDynDropAllocatorIdentityGate& gate) noexcept
{
    return !gate.subject.empty() && is_valid_m20b_owned_dyn_ir_shape_prototype_gate(gate.ir_shape_gate)
        && gate.ir_shape_gate_fingerprint == gate.ir_shape_gate.fingerprint
        && gate.ir_shape_gate_fingerprint == owned_dyn_ir_shape_prototype_gate_fingerprint(gate.ir_shape_gate)
        && !gate.facts.empty()
        && std::all_of(gate.facts.begin(), gate.facts.end(),
            [](const OwnedDynDropAllocatorIdentityFact& fact) {
                return is_valid(fact);
            })
        && prototype_identity_set_keys_are_consistent(gate) && layout_prototype_counts_are_consistent(gate)
        && is_valid(gate.summary, gate)
        && (gate.fingerprint == StableFingerprint128{}
            || gate.fingerprint == owned_dyn_drop_allocator_identity_gate_fingerprint(gate));
}

bool is_valid_m20c_owned_dyn_drop_allocator_identity_gate(const OwnedDynDropAllocatorIdentityGate& gate) noexcept
{
    return is_valid(gate) && std::string_view(gate.subject) == QUERY_OWNED_DYN_DROP_ALLOCATOR_M20C_SUBJECT
        && gate.summary.fact_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_FACT_COUNT
        && gate.summary.erased_drop_identity_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_SINGLE_FACT
        && gate.summary.allocator_identity_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_SINGLE_FACT
        && gate.summary.cleanup_dropck_bridge_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_SINGLE_FACT
        && gate.summary.owned_handle_identity_binding_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_SINGLE_FACT
        && gate.summary.runtime_lowering_blocker_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_SINGLE_FACT
        && gate.summary.m20b_reference_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_FACT_COUNT
        && gate.summary.compiler_owned_identity_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_FACT_COUNT
        && gate.summary.drop_identity_visible_count == 3U && gate.summary.allocator_identity_visible_count == 2U
        && gate.summary.cleanup_dropck_bridge_visible_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_SINGLE_FACT
        && gate.summary.owned_handle_binding_visible_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_SINGLE_FACT
        && gate.summary.borrowed_dyn_abi_unchanged_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_FACT_COUNT
        && gate.summary.standard_library_api_blocked_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_FACT_COUNT
        && gate.summary.box_dyn_surface_blocked_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_FACT_COUNT
        && gate.summary.owning_dyn_user_value_blocked_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_FACT_COUNT
        && gate.summary.allocator_api_blocked_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_FACT_COUNT
        && gate.summary.runtime_lowering_blocked_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_FACT_COUNT
        && gate.summary.dynamic_drop_runtime_blocked_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_FACT_COUNT
        && gate.summary.backend_helper_blocked_count == QUERY_OWNED_DYN_DROP_ALLOCATOR_FACT_COUNT
        && gate.summary.executable_runtime_implemented_count == 0U && gate.summary.observed_layout_prototype_total > 0U
        && m20c_fact_kinds_are_complete(gate);
}

void record_owned_dyn_drop_allocator_identity_fact(
    OwnedDynDropAllocatorIdentityGate& gate, OwnedDynDropAllocatorIdentityFact fact)
{
    gate.facts.push_back(std::move(fact));
    gate.summary = summarize_owned_dyn_drop_allocator_identity_gate_counts(gate);
}

OwnedDynDropAllocatorIdentitySummary summarize_owned_dyn_drop_allocator_identity_gate_counts(
    const OwnedDynDropAllocatorIdentityGate& gate) noexcept
{
    OwnedDynDropAllocatorIdentitySummary summary;
    summary.fact_count = static_cast<base::u64>(gate.facts.size());
    for (const OwnedDynDropAllocatorIdentityFact& fact : gate.facts) {
        switch (fact.kind) {
            case OwnedDynDropAllocatorIdentityFactKind::erased_drop_identity:
                ++summary.erased_drop_identity_count;
                break;
            case OwnedDynDropAllocatorIdentityFactKind::allocator_identity:
                ++summary.allocator_identity_count;
                break;
            case OwnedDynDropAllocatorIdentityFactKind::cleanup_dropck_bridge:
                ++summary.cleanup_dropck_bridge_count;
                break;
            case OwnedDynDropAllocatorIdentityFactKind::owned_handle_identity_binding:
                ++summary.owned_handle_identity_binding_count;
                break;
            case OwnedDynDropAllocatorIdentityFactKind::runtime_lowering_blocker:
                ++summary.runtime_lowering_blocker_count;
                break;
        }
        if (fact.references_m20b_ir_shape_gate) {
            ++summary.m20b_reference_count;
        }
        if (fact.compiler_owned_identity) {
            ++summary.compiler_owned_identity_count;
        }
        if (fact.drop_identity_visible) {
            ++summary.drop_identity_visible_count;
        }
        if (fact.allocator_identity_visible) {
            ++summary.allocator_identity_visible_count;
        }
        if (fact.cleanup_dropck_bridge_visible) {
            ++summary.cleanup_dropck_bridge_visible_count;
        }
        if (fact.owned_handle_binding_visible) {
            ++summary.owned_handle_binding_visible_count;
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
        if (fact.executable_runtime_implemented) {
            ++summary.executable_runtime_implemented_count;
        }
        summary.observed_layout_prototype_total =
            std::max(summary.observed_layout_prototype_total, fact.layout_prototype_count);
    }
    return summary;
}

StableFingerprint128 owned_dyn_drop_allocator_identity_gate_fingerprint(
    const OwnedDynDropAllocatorIdentityGate& gate) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_OWNED_DYN_DROP_ALLOCATOR_FINGERPRINT_MARKER);
    builder.mix_string(gate.subject);
    builder.mix_fingerprint(gate.ir_shape_gate_fingerprint);
    mix_summary(builder, gate.summary);
    builder.mix_u64(static_cast<base::u64>(gate.facts.size()));
    for (const OwnedDynDropAllocatorIdentityFact& fact : gate.facts) {
        mix_fact(builder, fact);
    }
    return builder.finish();
}

std::string summarize_owned_dyn_drop_allocator_identity_gate(const OwnedDynDropAllocatorIdentityGate& gate)
{
    std::ostringstream label;
    label << "owned_dyn_drop_allocator_identity_gate subject=" << (gate.subject.empty() ? "<anonymous>" : gate.subject)
          << " facts=" << gate.summary.fact_count << " drop_identity=" << gate.summary.erased_drop_identity_count
          << " allocator_identity=" << gate.summary.allocator_identity_count
          << " cleanup_dropck_bridge=" << gate.summary.cleanup_dropck_bridge_count
          << " handle_binding=" << gate.summary.owned_handle_identity_binding_count
          << " runtime_lowering_blocker=" << gate.summary.runtime_lowering_blocker_count
          << " m20b_refs=" << gate.summary.m20b_reference_count
          << " compiler_owned=" << gate.summary.compiler_owned_identity_count
          << " drop_visible=" << gate.summary.drop_identity_visible_count
          << " allocator_visible=" << gate.summary.allocator_identity_visible_count
          << " stdlib_blocked=" << gate.summary.standard_library_api_blocked_count
          << " allocator_api_blocked=" << gate.summary.allocator_api_blocked_count
          << " runtime_lowering_blocked=" << gate.summary.runtime_lowering_blocked_count
          << " dynamic_drop_runtime_blocked=" << gate.summary.dynamic_drop_runtime_blocked_count
          << " backend_helper_blocked=" << gate.summary.backend_helper_blocked_count
          << " executable_runtime_implemented=" << gate.summary.executable_runtime_implemented_count;
    if (!gate.facts.empty()) {
        label << " first_fact=" << gate.facts.front().fact_name
              << " policy=" << owned_dyn_drop_allocator_identity_policy_name(gate.facts.front().policy);
    }
    label << " fingerprint=" << debug_string(owned_dyn_drop_allocator_identity_gate_fingerprint(gate));
    return label.str();
}

std::string dump_owned_dyn_drop_allocator_identity_gate(const OwnedDynDropAllocatorIdentityGate& gate)
{
    std::ostringstream stream;
    stream << "owned_dyn_drop_allocator_identity_gate subject=" << (gate.subject.empty() ? "<anonymous>" : gate.subject)
           << " facts=" << gate.summary.fact_count << " ir_shape_gate=" << debug_string(gate.ir_shape_gate_fingerprint)
           << " fingerprint=" << debug_string(owned_dyn_drop_allocator_identity_gate_fingerprint(gate)) << '\n';
    for (base::usize index = 0; index < gate.facts.size(); ++index) {
        const OwnedDynDropAllocatorIdentityFact& fact = gate.facts[index];
        stream << "  identity #" << index << " name=" << fact.fact_name
               << " kind=" << owned_dyn_drop_allocator_identity_fact_kind_name(fact.kind)
               << " stage=" << owned_dyn_drop_allocator_identity_stage_name(fact.stage)
               << " policy=" << owned_dyn_drop_allocator_identity_policy_name(fact.policy)
               << " m20b=" << (fact.references_m20b_ir_shape_gate ? "yes" : "no")
               << " compiler_owned=" << (fact.compiler_owned_identity ? "yes" : "no")
               << " drop_identity=" << (fact.drop_identity_visible ? "yes" : "no")
               << " allocator_identity=" << (fact.allocator_identity_visible ? "yes" : "no")
               << " cleanup_dropck_bridge=" << (fact.cleanup_dropck_bridge_visible ? "yes" : "no")
               << " handle_binding=" << (fact.owned_handle_binding_visible ? "yes" : "no")
               << " borrowed_abi_unchanged=" << (fact.borrowed_dyn_abi_unchanged ? "yes" : "no")
               << " stdlib_blocked=" << (fact.standard_library_api_blocked ? "yes" : "no")
               << " box_blocked=" << (fact.box_dyn_surface_blocked ? "yes" : "no")
               << " owning_value_blocked=" << (fact.owning_dyn_user_value_blocked ? "yes" : "no")
               << " allocator_api_blocked=" << (fact.allocator_api_blocked ? "yes" : "no")
               << " runtime_blocked=" << (fact.runtime_lowering_blocked ? "yes" : "no")
               << " dynamic_drop_blocked=" << (fact.dynamic_drop_runtime_blocked ? "yes" : "no")
               << " backend_helper_blocked=" << (fact.backend_helper_blocked ? "yes" : "no")
               << " executable_runtime_implemented=" << (fact.executable_runtime_implemented ? "yes" : "no")
               << " object_key=" << fact.object_type.global_id << " drop_key=" << debug_string(fact.drop_identity_key)
               << " allocator_key=" << debug_string(fact.allocator_identity_key)
               << " identity_set_key=" << debug_string(fact.prototype_identity_set_key)
               << " prototype_count=" << fact.layout_prototype_count
               << " m20b_gate=" << fallback_name(fact.m20b_gate_fact, "<unknown>")
               << " verifier_guard=" << fallback_name(fact.verifier_guard_fact, "<unknown>")
               << " blocked_surface=" << fallback_name(fact.blocked_surface_fact, "<unknown>") << '\n';
    }
    return stream.str();
}

OwnedDynDropAllocatorIdentityGate m20c_owned_dyn_drop_allocator_identity_gate_baseline()
{
    OwnedDynDropAllocatorIdentityGate gate;
    gate.subject = std::string(QUERY_OWNED_DYN_DROP_ALLOCATOR_M20C_SUBJECT);
    gate.ir_shape_gate = m20b_owned_dyn_ir_shape_prototype_gate_baseline();
    gate.ir_shape_gate_fingerprint = gate.ir_shape_gate.fingerprint;

    record_owned_dyn_drop_allocator_identity_fact(gate,
        identity_fact(OwnedDynDropAllocatorIdentityFactKind::erased_drop_identity,
            OwnedDynDropAllocatorIdentityStage::identity_prerequisite,
            OwnedDynDropAllocatorIdentityPolicy::compiler_owned_erased_drop_identity_v1,
            QUERY_OWNED_DYN_DROP_ALLOCATOR_DROP_FACT, QUERY_OWNED_DYN_DROP_ALLOCATOR_DROP_GUARD,
            QUERY_OWNED_DYN_DROP_ALLOCATOR_NO_DYNAMIC_DROP));
    record_owned_dyn_drop_allocator_identity_fact(gate,
        identity_fact(OwnedDynDropAllocatorIdentityFactKind::allocator_identity,
            OwnedDynDropAllocatorIdentityStage::identity_prerequisite,
            OwnedDynDropAllocatorIdentityPolicy::compiler_owned_allocator_identity_v1,
            QUERY_OWNED_DYN_DROP_ALLOCATOR_ALLOCATOR_FACT, QUERY_OWNED_DYN_DROP_ALLOCATOR_ALLOCATOR_GUARD,
            QUERY_OWNED_DYN_DROP_ALLOCATOR_NO_ALLOCATOR));
    record_owned_dyn_drop_allocator_identity_fact(gate,
        identity_fact(OwnedDynDropAllocatorIdentityFactKind::cleanup_dropck_bridge,
            OwnedDynDropAllocatorIdentityStage::verifier_identity_guard,
            OwnedDynDropAllocatorIdentityPolicy::cleanup_dropck_static_bridge_v1,
            QUERY_OWNED_DYN_DROP_ALLOCATOR_CLEANUP_FACT, QUERY_OWNED_DYN_DROP_ALLOCATOR_CLEANUP_GUARD,
            QUERY_OWNED_DYN_DROP_ALLOCATOR_NO_STDLIB));
    record_owned_dyn_drop_allocator_identity_fact(gate,
        identity_fact(OwnedDynDropAllocatorIdentityFactKind::owned_handle_identity_binding,
            OwnedDynDropAllocatorIdentityStage::verifier_identity_guard,
            OwnedDynDropAllocatorIdentityPolicy::owned_handle_identity_binding_v1,
            QUERY_OWNED_DYN_DROP_ALLOCATOR_BINDING_FACT, QUERY_OWNED_DYN_DROP_ALLOCATOR_BINDING_GUARD,
            QUERY_OWNED_DYN_DROP_ALLOCATOR_NO_BOX));
    record_owned_dyn_drop_allocator_identity_fact(gate,
        identity_fact(OwnedDynDropAllocatorIdentityFactKind::runtime_lowering_blocker,
            OwnedDynDropAllocatorIdentityStage::blocked_future_runtime,
            OwnedDynDropAllocatorIdentityPolicy::runtime_lowering_not_implemented_v1,
            QUERY_OWNED_DYN_DROP_ALLOCATOR_RUNTIME_FACT, QUERY_OWNED_DYN_DROP_ALLOCATOR_RUNTIME_GUARD,
            QUERY_OWNED_DYN_DROP_ALLOCATOR_NO_RUNTIME));

    gate.fingerprint = owned_dyn_drop_allocator_identity_gate_fingerprint(gate);
    return gate;
}

} // namespace aurex::query
