#include <aurex/infrastructure/query/dyn_advanced_design_gate.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_DYN_ADVANCED_GATE_FINGERPRINT_MARKER =
    "query.dyn_advanced_design_gate.v1";
constexpr std::string_view QUERY_DYN_ADVANCED_M9C_GATE_NAME = "M9c Advanced Dyn Design Gate";
constexpr std::string_view QUERY_DYN_ADVANCED_M11A_GATE_NAME = "M11a Advanced Dyn Design Baseline";
constexpr std::string_view QUERY_DYN_ADVANCED_M13A_GATE_NAME =
    "M13a Advanced Dyn Remaining Policy Design Baseline";
constexpr std::string_view QUERY_DYN_ADVANCED_M15_GATE_NAME =
    "M15 Advanced Dyn Ownership / Runtime Boundary Design Baseline";
constexpr std::string_view QUERY_DYN_ADVANCED_BORROWED_ABI_POLICY = "borrowed_view_v1";
constexpr std::string_view QUERY_DYN_ADVANCED_BORROWED_METADATA_POLICY = "borrowed_methods_only_v1";
constexpr std::string_view QUERY_DYN_ADVANCED_SUPERTRAIT_METADATA_POLICY = "supertrait_vptr_metadata_v1";
constexpr std::string_view QUERY_DYN_ADVANCED_PRINCIPAL_SET_METADATA_POLICY =
    "principal_set_metadata_v1";
constexpr std::string_view QUERY_DYN_ADVANCED_OWNING_ABI_POLICY = "owning_dyn_container_v1";
constexpr std::string_view QUERY_DYN_ADVANCED_OWNING_METADATA_POLICY = "owning_dyn_metadata_v1";
constexpr std::string_view QUERY_DYN_ADVANCED_DROP_METADATA_POLICY = "dynamic_drop_metadata_v1";
constexpr std::string_view QUERY_DYN_ADVANCED_ALLOCATOR_ABI_POLICY = "allocator_placement_policy_v1";
constexpr std::string_view QUERY_DYN_ADVANCED_ALLOCATOR_METADATA_POLICY = "allocator_metadata_v1";
constexpr std::string_view QUERY_DYN_ADVANCED_COMPOSITION_METADATA_POLICY = "multi_trait_metadata_v1";
constexpr std::string_view QUERY_DYN_ADVANCED_M9C_NO_STD_NON_GOAL =
    "standard_library_runtime_not_in_m9c";
constexpr std::string_view QUERY_DYN_ADVANCED_M9C_NO_RUNTIME_NON_GOAL =
    "runtime_dispatch_not_in_m9c";
constexpr std::string_view QUERY_DYN_ADVANCED_M9C_NO_OWNING_NON_GOAL =
    "owning_dyn_runtime_not_in_m9c";
constexpr std::string_view QUERY_DYN_ADVANCED_M11A_NO_STD_NON_GOAL =
    "standard_library_runtime_not_in_m11a";
constexpr std::string_view QUERY_DYN_ADVANCED_M11A_NO_RUNTIME_NON_GOAL =
    "runtime_dispatch_not_in_m11a";
constexpr std::string_view QUERY_DYN_ADVANCED_M11A_NO_OWNING_NON_GOAL =
    "owning_dyn_runtime_not_in_m11a";
constexpr std::string_view QUERY_DYN_ADVANCED_M13A_NO_STD_NON_GOAL =
    "standard_library_runtime_not_in_m13a";
constexpr std::string_view QUERY_DYN_ADVANCED_M13A_NO_RUNTIME_NON_GOAL =
    "new_runtime_metadata_not_in_m13a";
constexpr std::string_view QUERY_DYN_ADVANCED_M13A_NO_OWNING_NON_GOAL =
    "owning_dyn_runtime_not_in_m13a";
constexpr std::string_view QUERY_DYN_ADVANCED_M15_NO_STD_NON_GOAL =
    "standard_library_runtime_not_in_m15";
constexpr std::string_view QUERY_DYN_ADVANCED_M15_NO_SURFACE_NON_GOAL =
    "no_new_dyn_surface_in_m15";
constexpr std::string_view QUERY_DYN_ADVANCED_M15_NO_OWNING_RUNTIME_NON_GOAL =
    "owning_dyn_runtime_not_in_m15";
constexpr base::usize QUERY_DYN_ADVANCED_LEGACY_CAPABILITY_COUNT = 5;
constexpr base::usize QUERY_DYN_ADVANCED_M13A_CAPABILITY_COUNT = 6;
constexpr base::u8 QUERY_DYN_ADVANCED_INVALID_CAPABILITY_VALUE = 255U;
constexpr base::u8 QUERY_DYN_ADVANCED_INVALID_STAGE_VALUE = 255U;
constexpr base::u8 QUERY_DYN_ADVANCED_INVALID_DECISION_VALUE = 255U;

[[nodiscard]] base::u8 stable_capability_value(const DynAdvancedCapability capability) noexcept
{
    return is_valid(capability) ? static_cast<base::u8>(capability)
                                : QUERY_DYN_ADVANCED_INVALID_CAPABILITY_VALUE;
}

[[nodiscard]] base::u8 stable_stage_value(const DynAdvancedGateStage stage) noexcept
{
    return is_valid(stage) ? static_cast<base::u8>(stage) : QUERY_DYN_ADVANCED_INVALID_STAGE_VALUE;
}

[[nodiscard]] base::u8 stable_decision_value(const DynAdvancedPolicyDecision decision) noexcept
{
    return is_valid(decision) ? static_cast<base::u8>(decision)
                              : QUERY_DYN_ADVANCED_INVALID_DECISION_VALUE;
}

[[nodiscard]] bool contains_text(
    const std::vector<std::string>& values, const std::string_view expected) noexcept
{
    return std::any_of(values.begin(), values.end(), [expected](const std::string& value) {
        return std::string_view(value) == expected;
    });
}

[[nodiscard]] bool has_policy_when_required(
    const bool required, const std::string& policy) noexcept
{
    return required ? !policy.empty() : policy.empty();
}

[[nodiscard]] bool has_required_detail_vectors(const DynAdvancedDesignCandidate& candidate) noexcept
{
    return !candidate.blockers.empty() && !candidate.required_facts.empty() && !candidate.non_goals.empty();
}

[[nodiscard]] bool decision_matches_impact(const DynAdvancedDesignCandidate& candidate) noexcept
{
    if (candidate.impact.standard_library_required
        && candidate.decision != DynAdvancedPolicyDecision::requires_standard_library_stage) {
        return false;
    }
    if (candidate.impact.runtime_required
        && candidate.decision != DynAdvancedPolicyDecision::requires_runtime_stage
        && candidate.decision != DynAdvancedPolicyDecision::requires_standard_library_stage) {
        return false;
    }
    if (candidate.impact.metadata_policy_required
        && candidate.decision == DynAdvancedPolicyDecision::rejected_for_m9c) {
        return false;
    }
    if (candidate.impact.abi_policy_required
        && candidate.decision == DynAdvancedPolicyDecision::rejected_for_m9c) {
        return false;
    }
    return true;
}

[[nodiscard]] bool baseline_capability_gate_is_valid(
    const DynAdvancedDesignCandidate& candidate) noexcept
{
    switch (candidate.capability) {
        case DynAdvancedCapability::supertrait_upcasting:
            return candidate.stage == DynAdvancedGateStage::design_gate
                && candidate.impact.metadata_policy_required && candidate.impact.borrow_model_impact
                && candidate.impact.tooling_cache_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_new_metadata_policy;
        case DynAdvancedCapability::owning_dyn:
            return candidate.stage == DynAdvancedGateStage::prototype_blocked
                && candidate.impact.abi_policy_required && candidate.impact.metadata_policy_required
                && candidate.impact.borrow_model_impact && candidate.impact.drop_model_impact
                && candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && candidate.impact.standard_library_required && candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_standard_library_stage;
        case DynAdvancedCapability::dynamic_drop_dispatch:
            return candidate.stage == DynAdvancedGateStage::prototype_blocked
                && candidate.impact.metadata_policy_required && candidate.impact.drop_model_impact
                && candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_runtime_stage;
        case DynAdvancedCapability::allocator_policy:
            return candidate.stage == DynAdvancedGateStage::prototype_blocked
                && candidate.impact.abi_policy_required && candidate.impact.metadata_policy_required
                && candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && candidate.impact.standard_library_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_standard_library_stage;
        case DynAdvancedCapability::multi_trait_composition:
            return candidate.stage == DynAdvancedGateStage::design_gate
                && candidate.impact.metadata_policy_required && candidate.impact.borrow_model_impact
                && candidate.impact.tooling_cache_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_new_metadata_policy;
        case DynAdvancedCapability::borrowed_composition_supertrait_projection:
            return false;
    }
    return false;
}

[[nodiscard]] bool candidate_shape_is_valid(const DynAdvancedDesignCandidate& candidate) noexcept
{
    return is_valid(candidate.capability) && is_valid(candidate.stage) && is_valid(candidate.decision)
        && has_policy_when_required(candidate.impact.abi_policy_required, candidate.required_abi_policy)
        && has_policy_when_required(candidate.impact.metadata_policy_required,
            candidate.required_metadata_policy)
        && std::string_view(candidate.required_abi_policy) != QUERY_DYN_ADVANCED_BORROWED_ABI_POLICY
        && std::string_view(candidate.required_metadata_policy) != QUERY_DYN_ADVANCED_BORROWED_METADATA_POLICY
        && has_required_detail_vectors(candidate) && decision_matches_impact(candidate);
}

[[nodiscard]] bool gate_has_each_advanced_capability_once(
    const DynAdvancedDesignGate& gate, const base::usize required_capability_count) noexcept
{
    if (gate.candidates.size() != required_capability_count) {
        return false;
    }

    std::array<bool, QUERY_DYN_ADVANCED_M13A_CAPABILITY_COUNT> seen = {};
    for (const DynAdvancedDesignCandidate& candidate : gate.candidates) {
        if (!is_valid(candidate.capability)) {
            return false;
        }
        const base::usize index = static_cast<base::usize>(candidate.capability) - 1U;
        if (index >= required_capability_count || seen[index]) {
            return false;
        }
        seen[index] = true;
    }
    return std::all_of(seen.begin(),
        seen.begin() + static_cast<std::ptrdiff_t>(required_capability_count),
        [](const bool present) {
            return present;
        });
}

[[nodiscard]] bool m11a_capability_gate_is_valid(
    const DynAdvancedDesignCandidate& candidate) noexcept
{
    switch (candidate.capability) {
        case DynAdvancedCapability::supertrait_upcasting:
            return candidate.stage == DynAdvancedGateStage::completed_release_baseline
                && candidate.impact.metadata_policy_required && candidate.impact.borrow_model_impact
                && candidate.impact.tooling_cache_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_new_metadata_policy
                && std::string_view(candidate.required_metadata_policy)
                    == QUERY_DYN_ADVANCED_SUPERTRAIT_METADATA_POLICY;
        case DynAdvancedCapability::owning_dyn:
            return candidate.stage == DynAdvancedGateStage::prototype_blocked
                && candidate.impact.abi_policy_required && candidate.impact.metadata_policy_required
                && candidate.impact.borrow_model_impact && candidate.impact.drop_model_impact
                && candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && candidate.impact.standard_library_required && candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_standard_library_stage;
        case DynAdvancedCapability::dynamic_drop_dispatch:
            return candidate.stage == DynAdvancedGateStage::prototype_blocked
                && candidate.impact.metadata_policy_required && candidate.impact.drop_model_impact
                && candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_runtime_stage;
        case DynAdvancedCapability::allocator_policy:
            return candidate.stage == DynAdvancedGateStage::prototype_blocked
                && candidate.impact.abi_policy_required && candidate.impact.metadata_policy_required
                && candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && candidate.impact.standard_library_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_standard_library_stage;
        case DynAdvancedCapability::multi_trait_composition:
            return candidate.stage == DynAdvancedGateStage::ready_for_future_stage
                && candidate.impact.metadata_policy_required && candidate.impact.borrow_model_impact
                && candidate.impact.tooling_cache_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_new_metadata_policy
                && std::string_view(candidate.required_metadata_policy)
                    == QUERY_DYN_ADVANCED_PRINCIPAL_SET_METADATA_POLICY;
        case DynAdvancedCapability::borrowed_composition_supertrait_projection:
            return false;
    }
    return false;
}

[[nodiscard]] bool m13a_capability_gate_is_valid(
    const DynAdvancedDesignCandidate& candidate) noexcept
{
    switch (candidate.capability) {
        case DynAdvancedCapability::supertrait_upcasting:
            return candidate.stage == DynAdvancedGateStage::completed_release_baseline
                && candidate.impact.metadata_policy_required && candidate.impact.borrow_model_impact
                && candidate.impact.tooling_cache_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_new_metadata_policy
                && std::string_view(candidate.required_metadata_policy)
                    == QUERY_DYN_ADVANCED_SUPERTRAIT_METADATA_POLICY;
        case DynAdvancedCapability::multi_trait_composition:
            return candidate.stage == DynAdvancedGateStage::completed_release_baseline
                && candidate.impact.metadata_policy_required && candidate.impact.borrow_model_impact
                && candidate.impact.tooling_cache_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_new_metadata_policy
                && std::string_view(candidate.required_metadata_policy)
                    == QUERY_DYN_ADVANCED_PRINCIPAL_SET_METADATA_POLICY;
        case DynAdvancedCapability::borrowed_composition_supertrait_projection:
            return candidate.stage == DynAdvancedGateStage::ready_for_future_stage
                && !candidate.impact.abi_policy_required && !candidate.impact.metadata_policy_required
                && candidate.impact.borrow_model_impact && !candidate.impact.drop_model_impact
                && !candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && !candidate.impact.standard_library_required && !candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::composes_existing_metadata_policies
                && candidate.required_abi_policy.empty() && candidate.required_metadata_policy.empty();
        case DynAdvancedCapability::owning_dyn:
            return candidate.stage == DynAdvancedGateStage::prototype_blocked
                && candidate.impact.abi_policy_required && candidate.impact.metadata_policy_required
                && candidate.impact.borrow_model_impact && candidate.impact.drop_model_impact
                && candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && candidate.impact.standard_library_required && candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_standard_library_stage;
        case DynAdvancedCapability::dynamic_drop_dispatch:
            return candidate.stage == DynAdvancedGateStage::prototype_blocked
                && candidate.impact.metadata_policy_required && candidate.impact.drop_model_impact
                && candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_runtime_stage;
        case DynAdvancedCapability::allocator_policy:
            return candidate.stage == DynAdvancedGateStage::prototype_blocked
                && candidate.impact.abi_policy_required && candidate.impact.metadata_policy_required
                && candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && candidate.impact.standard_library_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_standard_library_stage;
    }
    return false;
}

[[nodiscard]] bool m15_capability_gate_is_valid(
    const DynAdvancedDesignCandidate& candidate) noexcept
{
    switch (candidate.capability) {
        case DynAdvancedCapability::supertrait_upcasting:
            return candidate.stage == DynAdvancedGateStage::completed_release_baseline
                && candidate.impact.metadata_policy_required && candidate.impact.borrow_model_impact
                && candidate.impact.tooling_cache_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_new_metadata_policy
                && std::string_view(candidate.required_metadata_policy)
                    == QUERY_DYN_ADVANCED_SUPERTRAIT_METADATA_POLICY;
        case DynAdvancedCapability::multi_trait_composition:
            return candidate.stage == DynAdvancedGateStage::completed_release_baseline
                && candidate.impact.metadata_policy_required && candidate.impact.borrow_model_impact
                && candidate.impact.tooling_cache_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_new_metadata_policy
                && std::string_view(candidate.required_metadata_policy)
                    == QUERY_DYN_ADVANCED_PRINCIPAL_SET_METADATA_POLICY;
        case DynAdvancedCapability::borrowed_composition_supertrait_projection:
            return candidate.stage == DynAdvancedGateStage::completed_release_baseline
                && !candidate.impact.abi_policy_required && !candidate.impact.metadata_policy_required
                && candidate.impact.borrow_model_impact && !candidate.impact.drop_model_impact
                && !candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && !candidate.impact.standard_library_required && !candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::composes_existing_metadata_policies
                && candidate.required_abi_policy.empty() && candidate.required_metadata_policy.empty();
        case DynAdvancedCapability::owning_dyn:
            return candidate.stage == DynAdvancedGateStage::design_gate
                && candidate.impact.abi_policy_required && candidate.impact.metadata_policy_required
                && candidate.impact.borrow_model_impact && candidate.impact.drop_model_impact
                && candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && candidate.impact.standard_library_required && candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_standard_library_stage
                && std::string_view(candidate.required_abi_policy) == QUERY_DYN_ADVANCED_OWNING_ABI_POLICY
                && std::string_view(candidate.required_metadata_policy) == QUERY_DYN_ADVANCED_OWNING_METADATA_POLICY;
        case DynAdvancedCapability::dynamic_drop_dispatch:
            return candidate.stage == DynAdvancedGateStage::design_gate
                && !candidate.impact.abi_policy_required && candidate.impact.metadata_policy_required
                && !candidate.impact.borrow_model_impact && candidate.impact.drop_model_impact
                && candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && !candidate.impact.standard_library_required && candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_runtime_stage
                && candidate.required_abi_policy.empty()
                && std::string_view(candidate.required_metadata_policy)
                    == QUERY_DYN_ADVANCED_DROP_METADATA_POLICY;
        case DynAdvancedCapability::allocator_policy:
            return candidate.stage == DynAdvancedGateStage::design_gate
                && candidate.impact.abi_policy_required && candidate.impact.metadata_policy_required
                && !candidate.impact.borrow_model_impact && !candidate.impact.drop_model_impact
                && candidate.impact.resource_model_impact && candidate.impact.tooling_cache_impact
                && candidate.impact.standard_library_required && !candidate.impact.runtime_required
                && candidate.decision == DynAdvancedPolicyDecision::requires_standard_library_stage
                && std::string_view(candidate.required_abi_policy)
                    == QUERY_DYN_ADVANCED_ALLOCATOR_ABI_POLICY
                && std::string_view(candidate.required_metadata_policy)
                    == QUERY_DYN_ADVANCED_ALLOCATOR_METADATA_POLICY;
    }
    return false;
}

void mix_string_vector(StableHashBuilder& builder, const std::vector<std::string>& values) noexcept
{
    builder.mix_u64(static_cast<base::u64>(values.size()));
    for (const std::string& value : values) {
        builder.mix_string(value);
    }
}

void mix_impact(StableHashBuilder& builder, const DynAdvancedImpactSummary& impact) noexcept
{
    builder.mix_bool(impact.abi_policy_required);
    builder.mix_bool(impact.metadata_policy_required);
    builder.mix_bool(impact.borrow_model_impact);
    builder.mix_bool(impact.drop_model_impact);
    builder.mix_bool(impact.resource_model_impact);
    builder.mix_bool(impact.tooling_cache_impact);
    builder.mix_bool(impact.standard_library_required);
    builder.mix_bool(impact.runtime_required);
}

void mix_candidate(StableHashBuilder& builder, const DynAdvancedDesignCandidate& candidate) noexcept
{
    builder.mix_u8(stable_capability_value(candidate.capability));
    builder.mix_u8(stable_stage_value(candidate.stage));
    builder.mix_u8(stable_decision_value(candidate.decision));
    builder.mix_string(candidate.required_abi_policy);
    builder.mix_string(candidate.required_metadata_policy);
    mix_impact(builder, candidate.impact);
    mix_string_vector(builder, candidate.blockers);
    mix_string_vector(builder, candidate.required_facts);
    mix_string_vector(builder, candidate.non_goals);
}

void append_impact_summary(std::ostringstream& stream, const DynAdvancedImpactSummary& impact)
{
    stream << " impact="
           << "abi_policy_required:" << (impact.abi_policy_required ? "yes" : "no")
           << ",metadata_policy_required:" << (impact.metadata_policy_required ? "yes" : "no")
           << ",borrow_model:" << (impact.borrow_model_impact ? "yes" : "no")
           << ",drop_model:" << (impact.drop_model_impact ? "yes" : "no")
           << ",resource_model:" << (impact.resource_model_impact ? "yes" : "no")
           << ",tooling_cache:" << (impact.tooling_cache_impact ? "yes" : "no")
           << ",standard_library:" << (impact.standard_library_required ? "yes" : "no")
           << ",runtime:" << (impact.runtime_required ? "yes" : "no");
}

DynAdvancedDesignCandidate make_supertrait_upcasting_candidate()
{
    return DynAdvancedDesignCandidate{
        DynAdvancedCapability::supertrait_upcasting,
        DynAdvancedGateStage::design_gate,
        DynAdvancedPolicyDecision::requires_new_metadata_policy,
        "",
        std::string(QUERY_DYN_ADVANCED_SUPERTRAIT_METADATA_POLICY),
        DynAdvancedImpactSummary{
            false,
            true,
            true,
            false,
            false,
            true,
            false,
            false,
        },
        {
            "borrowed_methods_only_v1 has no supertrait edge metadata",
            "upcast coercion must preserve origin-bound erased view semantics",
        },
        {
            "supertrait_edge_fact",
            "upcast_layout_projection_fact",
            "origin_preserving_coercion_fact",
        },
        {
            std::string(QUERY_DYN_ADVANCED_M9C_NO_STD_NON_GOAL),
            std::string(QUERY_DYN_ADVANCED_M9C_NO_RUNTIME_NON_GOAL),
            "do_not_reuse_borrowed_methods_only_v1_for_supertrait_upcasting",
        },
    };
}

DynAdvancedDesignCandidate make_owning_dyn_candidate()
{
    return DynAdvancedDesignCandidate{
        DynAdvancedCapability::owning_dyn,
        DynAdvancedGateStage::prototype_blocked,
        DynAdvancedPolicyDecision::requires_standard_library_stage,
        std::string(QUERY_DYN_ADVANCED_OWNING_ABI_POLICY),
        std::string(QUERY_DYN_ADVANCED_OWNING_METADATA_POLICY),
        DynAdvancedImpactSummary{
            true,
            true,
            true,
            true,
            true,
            true,
            true,
            true,
        },
        {
            "requires standard library owner container such as future Box",
            "requires allocation, move, destroy, and ownership transfer policy",
        },
        {
            "owned_container_layout_fact",
            "copy_destroy_metadata_fact",
            "allocator_selection_fact",
        },
        {
            std::string(QUERY_DYN_ADVANCED_M9C_NO_STD_NON_GOAL),
            std::string(QUERY_DYN_ADVANCED_M9C_NO_OWNING_NON_GOAL),
            "do_not_reuse_borrowed_view_v1_for_owning_dyn",
        },
    };
}

DynAdvancedDesignCandidate make_dynamic_drop_candidate()
{
    return DynAdvancedDesignCandidate{
        DynAdvancedCapability::dynamic_drop_dispatch,
        DynAdvancedGateStage::prototype_blocked,
        DynAdvancedPolicyDecision::requires_runtime_stage,
        "",
        std::string(QUERY_DYN_ADVANCED_DROP_METADATA_POLICY),
        DynAdvancedImpactSummary{
            false,
            true,
            false,
            true,
            true,
            true,
            false,
            true,
        },
        {
            "borrowed_methods_only_v1 has no destructor slot or drop glue metadata",
            "generic and opaque cleanup are still marker-only",
        },
        {
            "dynamic_drop_slot_fact",
            "drop_glue_metadata_fact",
            "cleanup_runtime_policy_fact",
        },
        {
            std::string(QUERY_DYN_ADVANCED_M9C_NO_STD_NON_GOAL),
            std::string(QUERY_DYN_ADVANCED_M9C_NO_RUNTIME_NON_GOAL),
            "do_not_add_destructor_slot_to_borrowed_methods_only_v1",
        },
    };
}

DynAdvancedDesignCandidate make_allocator_candidate()
{
    return DynAdvancedDesignCandidate{
        DynAdvancedCapability::allocator_policy,
        DynAdvancedGateStage::prototype_blocked,
        DynAdvancedPolicyDecision::requires_standard_library_stage,
        std::string(QUERY_DYN_ADVANCED_ALLOCATOR_ABI_POLICY),
        std::string(QUERY_DYN_ADVANCED_ALLOCATOR_METADATA_POLICY),
        DynAdvancedImpactSummary{
            true,
            true,
            false,
            false,
            true,
            true,
            true,
            false,
        },
        {
            "allocator selection belongs to a later standard library resource stage",
            "placement and ownership transfer policy must be explicit before ABI exposure",
        },
        {
            "allocator_identity_fact",
            "placement_policy_fact",
            "owned_resource_transfer_fact",
        },
        {
            std::string(QUERY_DYN_ADVANCED_M9C_NO_STD_NON_GOAL),
            "do_not_encode_allocator_in_borrowed_view_v1",
            "do_not_allocate_for_borrowed_dyn_view",
        },
    };
}

DynAdvancedDesignCandidate make_multi_trait_composition_candidate()
{
    return DynAdvancedDesignCandidate{
        DynAdvancedCapability::multi_trait_composition,
        DynAdvancedGateStage::design_gate,
        DynAdvancedPolicyDecision::requires_new_metadata_policy,
        "",
        std::string(QUERY_DYN_ADVANCED_COMPOSITION_METADATA_POLICY),
        DynAdvancedImpactSummary{
            false,
            true,
            true,
            false,
            false,
            true,
            false,
            false,
        },
        {
            "principal trait identity is currently singular",
            "method slot namespace and associated equality merging need deterministic schema",
        },
        {
            "trait_object_principal_set_fact",
            "composition_method_namespace_fact",
            "associated_equality_merge_fact",
        },
        {
            std::string(QUERY_DYN_ADVANCED_M9C_NO_STD_NON_GOAL),
            std::string(QUERY_DYN_ADVANCED_M9C_NO_RUNTIME_NON_GOAL),
            "do_not_merge_multiple_principals_into_borrowed_methods_only_v1",
        },
    };
}

DynAdvancedDesignCandidate make_completed_supertrait_upcasting_candidate()
{
    DynAdvancedDesignCandidate candidate = make_supertrait_upcasting_candidate();
    candidate.stage = DynAdvancedGateStage::completed_release_baseline;
    candidate.blockers = {
        "completed_in_m10_release_baseline",
        "future composition must project through M10 supertrait metadata instead of replacing it",
    };
    candidate.required_facts = {
        "trait_supertrait_edge_fact",
        "trait_object_upcast_coercion_fact",
        "dyn_upcast_abi_descriptor",
    };
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M11A_NO_STD_NON_GOAL),
        std::string(QUERY_DYN_ADVANCED_M11A_NO_RUNTIME_NON_GOAL),
        "do_not_reopen_m10_supertrait_runtime_in_m11a",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_principal_set_composition_candidate()
{
    return DynAdvancedDesignCandidate{
        DynAdvancedCapability::multi_trait_composition,
        DynAdvancedGateStage::ready_for_future_stage,
        DynAdvancedPolicyDecision::requires_new_metadata_policy,
        "",
        std::string(QUERY_DYN_ADVANCED_PRINCIPAL_SET_METADATA_POLICY),
        DynAdvancedImpactSummary{
            false,
            true,
            true,
            false,
            false,
            true,
            false,
            false,
        },
        {
            "principal trait identity is singular before M11",
            "method namespace must be principal-qualified to avoid flattened-slot ambiguity",
            "associated equality requirements from multiple principals need deterministic merge facts",
        },
        {
            "principal_set_identity_fact",
            "composition_witness_set_fact",
            "principal_method_namespace_fact",
            "associated_equality_merge_fact",
            "composition_projection_fact",
        },
        {
            std::string(QUERY_DYN_ADVANCED_M11A_NO_STD_NON_GOAL),
            std::string(QUERY_DYN_ADVANCED_M11A_NO_RUNTIME_NON_GOAL),
            "do_not_encode_principal_set_as_single_trait_object",
            "do_not_flatten_method_slots_without_principal_namespace",
        },
    };
}

DynAdvancedDesignCandidate make_m11_owning_dyn_candidate()
{
    DynAdvancedDesignCandidate candidate = make_owning_dyn_candidate();
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M11A_NO_STD_NON_GOAL),
        std::string(QUERY_DYN_ADVANCED_M11A_NO_OWNING_NON_GOAL),
        "do_not_reuse_borrowed_view_v1_for_owning_dyn",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_m11_dynamic_drop_candidate()
{
    DynAdvancedDesignCandidate candidate = make_dynamic_drop_candidate();
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M11A_NO_STD_NON_GOAL),
        std::string(QUERY_DYN_ADVANCED_M11A_NO_RUNTIME_NON_GOAL),
        "do_not_add_destructor_slot_to_principal_set_metadata_v1",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_m11_allocator_candidate()
{
    DynAdvancedDesignCandidate candidate = make_allocator_candidate();
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M11A_NO_STD_NON_GOAL),
        "do_not_encode_allocator_in_borrowed_view_v1",
        "do_not_allocate_for_principal_set_dyn_view",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_m13_completed_supertrait_upcasting_candidate()
{
    DynAdvancedDesignCandidate candidate = make_completed_supertrait_upcasting_candidate();
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M13A_NO_STD_NON_GOAL),
        std::string(QUERY_DYN_ADVANCED_M13A_NO_RUNTIME_NON_GOAL),
        "do_not_reopen_m10_supertrait_runtime_in_m13a",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_m13_completed_principal_set_composition_candidate()
{
    DynAdvancedDesignCandidate candidate = make_principal_set_composition_candidate();
    candidate.stage = DynAdvancedGateStage::completed_release_baseline;
    candidate.blockers = {
        "completed_in_m11_m12_release_baseline",
        "future supertrait projection must compose principal_set_metadata_v1 with supertrait_vptr_metadata_v1",
    };
    candidate.required_facts = {
        "principal_set_identity_fact",
        "composition_witness_set_fact",
        "composition_projection_fact",
        "dyn_composition_projection_abi_descriptor",
    };
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M13A_NO_STD_NON_GOAL),
        std::string(QUERY_DYN_ADVANCED_M13A_NO_RUNTIME_NON_GOAL),
        "do_not_flatten_method_slots_without_principal_namespace",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_borrowed_composition_supertrait_projection_candidate()
{
    return DynAdvancedDesignCandidate{
        DynAdvancedCapability::borrowed_composition_supertrait_projection,
        DynAdvancedGateStage::ready_for_future_stage,
        DynAdvancedPolicyDecision::composes_existing_metadata_policies,
        "",
        "",
        DynAdvancedImpactSummary{
            false,
            false,
            true,
            false,
            false,
            true,
            false,
            false,
        },
        {
            "M12 rejects implicit composition-to-supertrait direct dispatch",
            "projection must choose an explicit source principal before following M10 supertrait metadata",
            "same parent reached through multiple principals needs ambiguity diagnostics",
        },
        {
            "composition_to_supertrait_projection_fact",
            "principal_supertrait_path_fact",
            "composition_supertrait_ambiguity_fact",
            "composition_supertrait_projection_abi_descriptor",
        },
        {
            std::string(QUERY_DYN_ADVANCED_M13A_NO_STD_NON_GOAL),
            std::string(QUERY_DYN_ADVANCED_M13A_NO_RUNTIME_NON_GOAL),
            "do_not_make_composition_to_supertrait_direct_call_implicit",
            "do_not_add_new_principal_set_metadata_policy",
        },
    };
}

DynAdvancedDesignCandidate make_m13_owning_dyn_candidate()
{
    DynAdvancedDesignCandidate candidate = make_owning_dyn_candidate();
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M13A_NO_STD_NON_GOAL),
        std::string(QUERY_DYN_ADVANCED_M13A_NO_OWNING_NON_GOAL),
        "do_not_reuse_borrowed_view_v1_for_owning_dyn",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_m13_dynamic_drop_candidate()
{
    DynAdvancedDesignCandidate candidate = make_dynamic_drop_candidate();
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M13A_NO_STD_NON_GOAL),
        std::string(QUERY_DYN_ADVANCED_M13A_NO_RUNTIME_NON_GOAL),
        "do_not_add_destructor_slot_to_principal_set_metadata_v1",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_m13_allocator_candidate()
{
    DynAdvancedDesignCandidate candidate = make_allocator_candidate();
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M13A_NO_STD_NON_GOAL),
        "do_not_encode_allocator_in_borrowed_view_v1",
        "do_not_allocate_for_composition_supertrait_projection",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_m15_completed_supertrait_upcasting_candidate()
{
    DynAdvancedDesignCandidate candidate = make_m13_completed_supertrait_upcasting_candidate();
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M15_NO_STD_NON_GOAL),
        std::string(QUERY_DYN_ADVANCED_M15_NO_SURFACE_NON_GOAL),
        "do_not_reopen_m10_supertrait_runtime_in_m15",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_m15_completed_principal_set_composition_candidate()
{
    DynAdvancedDesignCandidate candidate = make_m13_completed_principal_set_composition_candidate();
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M15_NO_STD_NON_GOAL),
        std::string(QUERY_DYN_ADVANCED_M15_NO_SURFACE_NON_GOAL),
        "do_not_reopen_principal_set_metadata_v1_in_m15",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_m15_completed_borrowed_view_path_candidate()
{
    DynAdvancedDesignCandidate candidate = make_borrowed_composition_supertrait_projection_candidate();
    candidate.stage = DynAdvancedGateStage::completed_release_baseline;
    candidate.blockers = {
        "completed_in_m13_m14_release_baseline",
        "future owning dyn must not change borrowed view path inference",
    };
    candidate.required_facts = {
        "composition_to_supertrait_projection_fact",
        "composition_supertrait_chain_fact",
        "borrowed_dyn_view_path_fact",
    };
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M15_NO_STD_NON_GOAL),
        std::string(QUERY_DYN_ADVANCED_M15_NO_SURFACE_NON_GOAL),
        "do_not_add_new_runtime_metadata_policy_for_borrowed_view_paths",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_m15_owning_dyn_boundary_candidate()
{
    DynAdvancedDesignCandidate candidate = make_owning_dyn_candidate();
    candidate.stage = DynAdvancedGateStage::design_gate;
    candidate.blockers = {
        "requires future owner container surface such as Box or unique owner",
        "requires explicit move_destroy_boundary and erased drop glue ownership transfer",
        "must choose thin owner handle versus fat owner value before runtime lowering",
    };
    candidate.required_facts = {
        "owned_dyn_container_layout_fact",
        "owned_dyn_move_boundary_fact",
        "owned_dyn_drop_obligation_fact",
        "owned_dyn_allocator_requirement_fact",
        "owned_dyn_tooling_boundary_fact",
    };
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M15_NO_STD_NON_GOAL),
        std::string(QUERY_DYN_ADVANCED_M15_NO_OWNING_RUNTIME_NON_GOAL),
        "do_not_implement_box_dyn_trait_in_m15",
        "do_not_add_allocator_api_in_m15",
        "do_not_lower_owning_dyn_to_runtime_in_m15",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_m15_dynamic_drop_boundary_candidate()
{
    DynAdvancedDesignCandidate candidate = make_dynamic_drop_candidate();
    candidate.stage = DynAdvancedGateStage::design_gate;
    candidate.blockers = {
        "requires erased drop glue identity before owning dyn runtime",
        "must keep borrowed vtables destructor-free",
        "cleanup lowering must know whether destruction is static or erased",
    };
    candidate.required_facts = {
        "erased_drop_glue_identity_fact",
        "dynamic_drop_slot_layout_fact",
        "dropck_erased_receiver_fact",
        "cleanup_runtime_boundary_fact",
    };
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M15_NO_STD_NON_GOAL),
        std::string(QUERY_DYN_ADVANCED_M15_NO_SURFACE_NON_GOAL),
        "do_not_add_destructor_slot_to_borrowed_vtables_in_m15",
        "do_not_emit_dynamic_drop_dispatch_in_m15",
    };
    return candidate;
}

DynAdvancedDesignCandidate make_m15_allocator_boundary_candidate()
{
    DynAdvancedDesignCandidate candidate = make_allocator_candidate();
    candidate.stage = DynAdvancedGateStage::design_gate;
    candidate.blockers = {
        "allocator identity belongs to future standard library resource surface",
        "owned dyn layout must not bake in a single global allocator",
        "placement and deallocation policy must round-trip through query keys",
    };
    candidate.required_facts = {
        "allocator_identity_fact",
        "allocator_placement_policy_fact",
        "owned_dyn_deallocation_policy_fact",
        "allocator_tooling_boundary_fact",
    };
    candidate.non_goals = {
        std::string(QUERY_DYN_ADVANCED_M15_NO_STD_NON_GOAL),
        "do_not_define_allocator_trait_or_std_module_in_m15",
        "do_not_allocate_for_borrowed_or_owned_dyn_in_m15",
    };
    return candidate;
}

} // namespace

std::string_view dyn_advanced_capability_name(const DynAdvancedCapability capability) noexcept
{
    switch (capability) {
        case DynAdvancedCapability::supertrait_upcasting:
            return "supertrait_upcasting";
        case DynAdvancedCapability::owning_dyn:
            return "owning_dyn";
        case DynAdvancedCapability::dynamic_drop_dispatch:
            return "dynamic_drop_dispatch";
        case DynAdvancedCapability::allocator_policy:
            return "allocator_policy";
        case DynAdvancedCapability::multi_trait_composition:
            return "multi_trait_composition";
        case DynAdvancedCapability::borrowed_composition_supertrait_projection:
            return "borrowed_composition_supertrait_projection";
    }
    return "invalid";
}

std::string_view dyn_advanced_gate_stage_name(const DynAdvancedGateStage stage) noexcept
{
    switch (stage) {
        case DynAdvancedGateStage::research_only:
            return "research_only";
        case DynAdvancedGateStage::design_gate:
            return "design_gate";
        case DynAdvancedGateStage::prototype_blocked:
            return "prototype_blocked";
        case DynAdvancedGateStage::ready_for_future_stage:
            return "ready_for_future_stage";
        case DynAdvancedGateStage::completed_release_baseline:
            return "completed_release_baseline";
    }
    return "invalid";
}

std::string_view dyn_advanced_policy_decision_name(const DynAdvancedPolicyDecision decision) noexcept
{
    switch (decision) {
        case DynAdvancedPolicyDecision::rejected_for_m9c:
            return "rejected_for_m9c";
        case DynAdvancedPolicyDecision::requires_new_abi_policy:
            return "requires_new_abi_policy";
        case DynAdvancedPolicyDecision::requires_new_metadata_policy:
            return "requires_new_metadata_policy";
        case DynAdvancedPolicyDecision::requires_standard_library_stage:
            return "requires_standard_library_stage";
        case DynAdvancedPolicyDecision::requires_runtime_stage:
            return "requires_runtime_stage";
        case DynAdvancedPolicyDecision::composes_existing_metadata_policies:
            return "composes_existing_metadata_policies";
    }
    return "invalid";
}

bool is_valid(const DynAdvancedCapability capability) noexcept
{
    switch (capability) {
        case DynAdvancedCapability::supertrait_upcasting:
        case DynAdvancedCapability::owning_dyn:
        case DynAdvancedCapability::dynamic_drop_dispatch:
        case DynAdvancedCapability::allocator_policy:
        case DynAdvancedCapability::multi_trait_composition:
        case DynAdvancedCapability::borrowed_composition_supertrait_projection:
            return true;
    }
    return false;
}

bool is_valid(const DynAdvancedGateStage stage) noexcept
{
    switch (stage) {
        case DynAdvancedGateStage::research_only:
        case DynAdvancedGateStage::design_gate:
        case DynAdvancedGateStage::prototype_blocked:
        case DynAdvancedGateStage::ready_for_future_stage:
        case DynAdvancedGateStage::completed_release_baseline:
            return true;
    }
    return false;
}

bool is_valid(const DynAdvancedPolicyDecision decision) noexcept
{
    switch (decision) {
        case DynAdvancedPolicyDecision::rejected_for_m9c:
        case DynAdvancedPolicyDecision::requires_new_abi_policy:
        case DynAdvancedPolicyDecision::requires_new_metadata_policy:
        case DynAdvancedPolicyDecision::requires_standard_library_stage:
        case DynAdvancedPolicyDecision::requires_runtime_stage:
        case DynAdvancedPolicyDecision::composes_existing_metadata_policies:
            return true;
    }
    return false;
}

bool is_valid(const DynAdvancedDesignCandidate& candidate) noexcept
{
    return candidate_shape_is_valid(candidate)
        && candidate.stage != DynAdvancedGateStage::ready_for_future_stage
        && candidate.stage != DynAdvancedGateStage::completed_release_baseline
        && baseline_capability_gate_is_valid(candidate)
        && contains_text(candidate.non_goals, QUERY_DYN_ADVANCED_M9C_NO_STD_NON_GOAL);
}

bool is_valid(const DynAdvancedDesignGate& gate) noexcept
{
    if (std::string_view(gate.name) == QUERY_DYN_ADVANCED_M15_GATE_NAME) {
        return is_valid_m15_dyn_advanced_design_gate(gate);
    }
    if (std::string_view(gate.name) == QUERY_DYN_ADVANCED_M13A_GATE_NAME) {
        return is_valid_m13a_dyn_advanced_design_gate(gate);
    }
    if (std::string_view(gate.name) == QUERY_DYN_ADVANCED_M11A_GATE_NAME) {
        return is_valid_m11a_dyn_advanced_design_gate(gate);
    }
    return std::string_view(gate.name) == QUERY_DYN_ADVANCED_M9C_GATE_NAME
        && gate_has_each_advanced_capability_once(gate, QUERY_DYN_ADVANCED_LEGACY_CAPABILITY_COUNT)
        && std::all_of(gate.candidates.begin(), gate.candidates.end(),
            [](const DynAdvancedDesignCandidate& candidate) {
                return is_valid(candidate);
            });
}

bool is_valid_m11a_dyn_advanced_design_gate(const DynAdvancedDesignGate& gate) noexcept
{
    return std::string_view(gate.name) == QUERY_DYN_ADVANCED_M11A_GATE_NAME
        && gate_has_each_advanced_capability_once(gate, QUERY_DYN_ADVANCED_LEGACY_CAPABILITY_COUNT)
        && std::all_of(gate.candidates.begin(), gate.candidates.end(),
            [](const DynAdvancedDesignCandidate& candidate) {
                return candidate_shape_is_valid(candidate) && m11a_capability_gate_is_valid(candidate)
                    && contains_text(candidate.non_goals, QUERY_DYN_ADVANCED_M11A_NO_STD_NON_GOAL);
            });
}

bool is_valid_m13a_dyn_advanced_design_gate(const DynAdvancedDesignGate& gate) noexcept
{
    return std::string_view(gate.name) == QUERY_DYN_ADVANCED_M13A_GATE_NAME
        && gate_has_each_advanced_capability_once(gate, QUERY_DYN_ADVANCED_M13A_CAPABILITY_COUNT)
        && std::all_of(gate.candidates.begin(), gate.candidates.end(),
            [](const DynAdvancedDesignCandidate& candidate) {
                return candidate_shape_is_valid(candidate) && m13a_capability_gate_is_valid(candidate)
                    && contains_text(candidate.non_goals, QUERY_DYN_ADVANCED_M13A_NO_STD_NON_GOAL);
            });
}

bool is_valid_m15_dyn_advanced_design_gate(const DynAdvancedDesignGate& gate) noexcept
{
    return std::string_view(gate.name) == QUERY_DYN_ADVANCED_M15_GATE_NAME
        && gate_has_each_advanced_capability_once(gate, QUERY_DYN_ADVANCED_M13A_CAPABILITY_COUNT)
        && std::all_of(gate.candidates.begin(), gate.candidates.end(),
            [](const DynAdvancedDesignCandidate& candidate) {
                return candidate_shape_is_valid(candidate) && m15_capability_gate_is_valid(candidate)
                    && contains_text(candidate.non_goals, QUERY_DYN_ADVANCED_M15_NO_STD_NON_GOAL);
            });
}

void record_dyn_advanced_design_candidate(
    DynAdvancedDesignGate& gate, DynAdvancedDesignCandidate candidate)
{
    gate.candidates.push_back(std::move(candidate));
}

StableFingerprint128 dyn_advanced_design_gate_fingerprint(
    const DynAdvancedDesignGate& gate) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_DYN_ADVANCED_GATE_FINGERPRINT_MARKER);
    builder.mix_string(gate.name);
    builder.mix_u64(gate.candidates.size());
    for (const DynAdvancedDesignCandidate& candidate : gate.candidates) {
        mix_candidate(builder, candidate);
    }
    return builder.finish();
}

std::string summarize_dyn_advanced_design_gate(const DynAdvancedDesignGate& gate)
{
    base::u64 standard_library_blocked_count = 0;
    base::u64 runtime_blocked_count = 0;
    base::u64 metadata_policy_count = 0;
    base::u64 abi_policy_count = 0;
    base::u64 ready_for_future_stage_count = 0;
    base::u64 completed_release_count = 0;
    for (const DynAdvancedDesignCandidate& candidate : gate.candidates) {
        if (candidate.impact.standard_library_required) {
            ++standard_library_blocked_count;
        }
        if (candidate.impact.runtime_required) {
            ++runtime_blocked_count;
        }
        if (candidate.impact.metadata_policy_required) {
            ++metadata_policy_count;
        }
        if (candidate.impact.abi_policy_required) {
            ++abi_policy_count;
        }
        if (candidate.stage == DynAdvancedGateStage::ready_for_future_stage) {
            ++ready_for_future_stage_count;
        }
        if (candidate.stage == DynAdvancedGateStage::completed_release_baseline) {
            ++completed_release_count;
        }
    }

    std::ostringstream label;
    label << "dyn_advanced_design_gate name="
          << (gate.name.empty() ? "<anonymous>" : gate.name)
          << " candidates=" << gate.candidates.size()
          << " abi_policy_candidates=" << abi_policy_count
          << " metadata_policy_candidates=" << metadata_policy_count
          << " standard_library_blocked=" << standard_library_blocked_count
          << " runtime_blocked=" << runtime_blocked_count
          << " ready_for_future_stage=" << ready_for_future_stage_count
          << " completed_release=" << completed_release_count
          << " baseline_abi=" << QUERY_DYN_ADVANCED_BORROWED_ABI_POLICY
          << " baseline_metadata=" << QUERY_DYN_ADVANCED_BORROWED_METADATA_POLICY
          << " fingerprint=" << debug_string(dyn_advanced_design_gate_fingerprint(gate));
    return label.str();
}

std::string dump_dyn_advanced_design_gate(const DynAdvancedDesignGate& gate)
{
    std::ostringstream stream;
    stream << "dyn_advanced_design_gate name="
           << (gate.name.empty() ? "<anonymous>" : gate.name)
           << " candidates=" << gate.candidates.size()
           << " baseline_abi=" << QUERY_DYN_ADVANCED_BORROWED_ABI_POLICY
           << " baseline_metadata=" << QUERY_DYN_ADVANCED_BORROWED_METADATA_POLICY
           << " fingerprint=" << debug_string(dyn_advanced_design_gate_fingerprint(gate)) << '\n';
    for (base::usize index = 0; index < gate.candidates.size(); ++index) {
        const DynAdvancedDesignCandidate& candidate = gate.candidates[index];
        stream << "  candidate #" << index
               << " capability=" << dyn_advanced_capability_name(candidate.capability)
               << " stage=" << dyn_advanced_gate_stage_name(candidate.stage)
               << " decision=" << dyn_advanced_policy_decision_name(candidate.decision);
        if (!candidate.required_abi_policy.empty()) {
            stream << " required_abi_policy=" << candidate.required_abi_policy;
        }
        if (!candidate.required_metadata_policy.empty()) {
            stream << " required_metadata_policy=" << candidate.required_metadata_policy;
        }
        append_impact_summary(stream, candidate.impact);
        stream << '\n';
        for (const std::string& blocker : candidate.blockers) {
            stream << "    blocker=" << blocker << '\n';
        }
        for (const std::string& fact : candidate.required_facts) {
            stream << "    required_fact=" << fact << '\n';
        }
        for (const std::string& non_goal : candidate.non_goals) {
            stream << "    non_goal=" << non_goal << '\n';
        }
    }
    return stream.str();
}

DynAdvancedDesignGate m9c_dyn_advanced_design_gate_baseline()
{
    DynAdvancedDesignGate gate;
    gate.name = std::string(QUERY_DYN_ADVANCED_M9C_GATE_NAME);
    record_dyn_advanced_design_candidate(gate, make_supertrait_upcasting_candidate());
    record_dyn_advanced_design_candidate(gate, make_owning_dyn_candidate());
    record_dyn_advanced_design_candidate(gate, make_dynamic_drop_candidate());
    record_dyn_advanced_design_candidate(gate, make_allocator_candidate());
    record_dyn_advanced_design_candidate(gate, make_multi_trait_composition_candidate());
    gate.fingerprint = dyn_advanced_design_gate_fingerprint(gate);
    return gate;
}

DynAdvancedDesignGate m11a_dyn_advanced_design_gate_baseline()
{
    DynAdvancedDesignGate gate;
    gate.name = std::string(QUERY_DYN_ADVANCED_M11A_GATE_NAME);
    record_dyn_advanced_design_candidate(gate, make_completed_supertrait_upcasting_candidate());
    record_dyn_advanced_design_candidate(gate, make_m11_owning_dyn_candidate());
    record_dyn_advanced_design_candidate(gate, make_m11_dynamic_drop_candidate());
    record_dyn_advanced_design_candidate(gate, make_m11_allocator_candidate());
    record_dyn_advanced_design_candidate(gate, make_principal_set_composition_candidate());
    gate.fingerprint = dyn_advanced_design_gate_fingerprint(gate);
    return gate;
}

DynAdvancedDesignGate m13a_dyn_advanced_design_gate_baseline()
{
    DynAdvancedDesignGate gate;
    gate.name = std::string(QUERY_DYN_ADVANCED_M13A_GATE_NAME);
    record_dyn_advanced_design_candidate(gate, make_m13_completed_supertrait_upcasting_candidate());
    record_dyn_advanced_design_candidate(gate, make_m13_owning_dyn_candidate());
    record_dyn_advanced_design_candidate(gate, make_m13_dynamic_drop_candidate());
    record_dyn_advanced_design_candidate(gate, make_m13_allocator_candidate());
    record_dyn_advanced_design_candidate(gate, make_m13_completed_principal_set_composition_candidate());
    record_dyn_advanced_design_candidate(gate, make_borrowed_composition_supertrait_projection_candidate());
    gate.fingerprint = dyn_advanced_design_gate_fingerprint(gate);
    return gate;
}

DynAdvancedDesignGate m15_dyn_advanced_design_gate_baseline()
{
    DynAdvancedDesignGate gate;
    gate.name = std::string(QUERY_DYN_ADVANCED_M15_GATE_NAME);
    record_dyn_advanced_design_candidate(gate, make_m15_completed_supertrait_upcasting_candidate());
    record_dyn_advanced_design_candidate(gate, make_m15_owning_dyn_boundary_candidate());
    record_dyn_advanced_design_candidate(gate, make_m15_dynamic_drop_boundary_candidate());
    record_dyn_advanced_design_candidate(gate, make_m15_allocator_boundary_candidate());
    record_dyn_advanced_design_candidate(gate, make_m15_completed_principal_set_composition_candidate());
    record_dyn_advanced_design_candidate(gate, make_m15_completed_borrowed_view_path_candidate());
    gate.fingerprint = dyn_advanced_design_gate_fingerprint(gate);
    return gate;
}

} // namespace aurex::query
