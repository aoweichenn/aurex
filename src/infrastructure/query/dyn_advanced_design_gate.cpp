#include <aurex/infrastructure/query/dyn_advanced_design_gate.hpp>

#include <algorithm>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_DYN_ADVANCED_GATE_FINGERPRINT_MARKER =
    "query.dyn_advanced_design_gate.v1";
constexpr std::string_view QUERY_DYN_ADVANCED_M9C_GATE_NAME = "M9c Advanced Dyn Design Gate";
constexpr std::string_view QUERY_DYN_ADVANCED_BORROWED_ABI_POLICY = "borrowed_view_v1";
constexpr std::string_view QUERY_DYN_ADVANCED_BORROWED_METADATA_POLICY = "borrowed_methods_only_v1";
constexpr std::string_view QUERY_DYN_ADVANCED_SUPERTRAIT_METADATA_POLICY = "supertrait_vptr_metadata_v1";
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
            return true;
    }
    return false;
}

bool is_valid(const DynAdvancedDesignCandidate& candidate) noexcept
{
    return is_valid(candidate.capability) && is_valid(candidate.stage) && is_valid(candidate.decision)
        && candidate.stage != DynAdvancedGateStage::ready_for_future_stage
        && has_policy_when_required(candidate.impact.abi_policy_required, candidate.required_abi_policy)
        && has_policy_when_required(candidate.impact.metadata_policy_required,
            candidate.required_metadata_policy)
        && std::string_view(candidate.required_abi_policy) != QUERY_DYN_ADVANCED_BORROWED_ABI_POLICY
        && std::string_view(candidate.required_metadata_policy) != QUERY_DYN_ADVANCED_BORROWED_METADATA_POLICY
        && has_required_detail_vectors(candidate) && decision_matches_impact(candidate)
        && baseline_capability_gate_is_valid(candidate)
        && contains_text(candidate.non_goals, QUERY_DYN_ADVANCED_M9C_NO_STD_NON_GOAL);
}

bool is_valid(const DynAdvancedDesignGate& gate) noexcept
{
    return !gate.name.empty() && !gate.candidates.empty()
        && std::all_of(gate.candidates.begin(), gate.candidates.end(),
            [](const DynAdvancedDesignCandidate& candidate) {
                return is_valid(candidate);
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
    }

    std::ostringstream label;
    label << "dyn_advanced_design_gate name="
          << (gate.name.empty() ? "<anonymous>" : gate.name)
          << " candidates=" << gate.candidates.size()
          << " abi_policy_candidates=" << abi_policy_count
          << " metadata_policy_candidates=" << metadata_policy_count
          << " standard_library_blocked=" << standard_library_blocked_count
          << " runtime_blocked=" << runtime_blocked_count
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

} // namespace aurex::query
