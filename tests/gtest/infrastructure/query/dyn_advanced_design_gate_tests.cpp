#include <aurex/infrastructure/query/dyn_advanced_design_gate.hpp>

#include <algorithm>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u8 QUERY_TEST_INVALID_DYN_ADVANCED_CAPABILITY = 240U;
constexpr base::u8 QUERY_TEST_INVALID_DYN_ADVANCED_STAGE = 241U;
constexpr base::u8 QUERY_TEST_INVALID_DYN_ADVANCED_DECISION = 242U;
constexpr base::usize QUERY_TEST_M9C_ADVANCED_CANDIDATE_COUNT = 5;
constexpr base::usize QUERY_TEST_M11A_ADVANCED_CANDIDATE_COUNT = 5;

[[nodiscard]] const query::DynAdvancedDesignCandidate* find_candidate(
    const query::DynAdvancedDesignGate& gate, const query::DynAdvancedCapability capability) noexcept
{
    const auto found = std::find_if(gate.candidates.begin(), gate.candidates.end(),
        [capability](const query::DynAdvancedDesignCandidate& candidate) {
            return candidate.capability == capability;
        });
    return found == gate.candidates.end() ? nullptr : &*found;
}

[[nodiscard]] query::DynAdvancedDesignCandidate* find_candidate(
    query::DynAdvancedDesignGate& gate, const query::DynAdvancedCapability capability) noexcept
{
    const auto found = std::find_if(gate.candidates.begin(), gate.candidates.end(),
        [capability](const query::DynAdvancedDesignCandidate& candidate) {
            return candidate.capability == capability;
        });
    return found == gate.candidates.end() ? nullptr : &*found;
}

[[nodiscard]] bool has_non_goal(
    const query::DynAdvancedDesignCandidate& candidate, const std::string_view non_goal) noexcept
{
    return std::any_of(candidate.non_goals.begin(), candidate.non_goals.end(),
        [non_goal](const std::string& value) {
            return std::string_view(value) == non_goal;
        });
}

} // namespace

TEST(QueryUnit, DynAdvancedDesignGateExposesEnumNamesAndInvalidFallbacks)
{
    EXPECT_EQ(query::dyn_advanced_capability_name(query::DynAdvancedCapability::supertrait_upcasting),
        "supertrait_upcasting");
    EXPECT_EQ(query::dyn_advanced_capability_name(query::DynAdvancedCapability::owning_dyn), "owning_dyn");
    EXPECT_EQ(query::dyn_advanced_capability_name(query::DynAdvancedCapability::dynamic_drop_dispatch),
        "dynamic_drop_dispatch");
    EXPECT_EQ(query::dyn_advanced_capability_name(query::DynAdvancedCapability::allocator_policy),
        "allocator_policy");
    EXPECT_EQ(query::dyn_advanced_capability_name(query::DynAdvancedCapability::multi_trait_composition),
        "multi_trait_composition");
    EXPECT_EQ(query::dyn_advanced_capability_name(
                  static_cast<query::DynAdvancedCapability>(QUERY_TEST_INVALID_DYN_ADVANCED_CAPABILITY)),
        "invalid");

    EXPECT_EQ(query::dyn_advanced_gate_stage_name(query::DynAdvancedGateStage::research_only),
        "research_only");
    EXPECT_EQ(query::dyn_advanced_gate_stage_name(query::DynAdvancedGateStage::design_gate), "design_gate");
    EXPECT_EQ(query::dyn_advanced_gate_stage_name(query::DynAdvancedGateStage::prototype_blocked),
        "prototype_blocked");
    EXPECT_EQ(query::dyn_advanced_gate_stage_name(query::DynAdvancedGateStage::ready_for_future_stage),
        "ready_for_future_stage");
    EXPECT_EQ(query::dyn_advanced_gate_stage_name(query::DynAdvancedGateStage::completed_release_baseline),
        "completed_release_baseline");
    EXPECT_EQ(query::dyn_advanced_gate_stage_name(
                  static_cast<query::DynAdvancedGateStage>(QUERY_TEST_INVALID_DYN_ADVANCED_STAGE)),
        "invalid");

    EXPECT_EQ(query::dyn_advanced_policy_decision_name(query::DynAdvancedPolicyDecision::rejected_for_m9c),
        "rejected_for_m9c");
    EXPECT_EQ(query::dyn_advanced_policy_decision_name(
                  query::DynAdvancedPolicyDecision::requires_new_abi_policy),
        "requires_new_abi_policy");
    EXPECT_EQ(query::dyn_advanced_policy_decision_name(
                  query::DynAdvancedPolicyDecision::requires_new_metadata_policy),
        "requires_new_metadata_policy");
    EXPECT_EQ(query::dyn_advanced_policy_decision_name(
                  query::DynAdvancedPolicyDecision::requires_standard_library_stage),
        "requires_standard_library_stage");
    EXPECT_EQ(query::dyn_advanced_policy_decision_name(
                  query::DynAdvancedPolicyDecision::requires_runtime_stage),
        "requires_runtime_stage");
    EXPECT_EQ(query::dyn_advanced_policy_decision_name(
                  static_cast<query::DynAdvancedPolicyDecision>(QUERY_TEST_INVALID_DYN_ADVANCED_DECISION)),
        "invalid");

    EXPECT_FALSE(query::is_valid(
        static_cast<query::DynAdvancedCapability>(QUERY_TEST_INVALID_DYN_ADVANCED_CAPABILITY)));
    EXPECT_FALSE(query::is_valid(static_cast<query::DynAdvancedGateStage>(
        QUERY_TEST_INVALID_DYN_ADVANCED_STAGE)));
    EXPECT_FALSE(query::is_valid(static_cast<query::DynAdvancedPolicyDecision>(
        QUERY_TEST_INVALID_DYN_ADVANCED_DECISION)));
}

TEST(QueryUnit, DynAdvancedDesignGateBaselineIsValidAndSeparatesAdvancedPolicies)
{
    const query::DynAdvancedDesignGate gate = query::m9c_dyn_advanced_design_gate_baseline();

    ASSERT_EQ(gate.name, "M9c Advanced Dyn Design Gate");
    ASSERT_EQ(gate.candidates.size(), QUERY_TEST_M9C_ADVANCED_CANDIDATE_COUNT);
    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_EQ(gate.fingerprint, query::dyn_advanced_design_gate_fingerprint(gate));

    const query::DynAdvancedDesignCandidate* supertrait =
        find_candidate(gate, query::DynAdvancedCapability::supertrait_upcasting);
    ASSERT_NE(supertrait, nullptr);
    EXPECT_EQ(supertrait->stage, query::DynAdvancedGateStage::design_gate);
    EXPECT_EQ(supertrait->decision, query::DynAdvancedPolicyDecision::requires_new_metadata_policy);
    EXPECT_TRUE(supertrait->impact.metadata_policy_required);
    EXPECT_FALSE(supertrait->impact.standard_library_required);
    EXPECT_FALSE(supertrait->impact.runtime_required);
    EXPECT_EQ(supertrait->required_metadata_policy, "supertrait_vptr_metadata_v1");

    const query::DynAdvancedDesignCandidate* owning =
        find_candidate(gate, query::DynAdvancedCapability::owning_dyn);
    ASSERT_NE(owning, nullptr);
    EXPECT_EQ(owning->stage, query::DynAdvancedGateStage::prototype_blocked);
    EXPECT_EQ(owning->decision, query::DynAdvancedPolicyDecision::requires_standard_library_stage);
    EXPECT_TRUE(owning->impact.abi_policy_required);
    EXPECT_TRUE(owning->impact.metadata_policy_required);
    EXPECT_TRUE(owning->impact.standard_library_required);
    EXPECT_TRUE(owning->impact.runtime_required);
    EXPECT_EQ(owning->required_abi_policy, "owning_dyn_container_v1");
    EXPECT_EQ(owning->required_metadata_policy, "owning_dyn_metadata_v1");
    EXPECT_TRUE(has_non_goal(*owning, "standard_library_runtime_not_in_m9c"));
    EXPECT_TRUE(has_non_goal(*owning, "owning_dyn_runtime_not_in_m9c"));

    const query::DynAdvancedDesignCandidate* dynamic_drop =
        find_candidate(gate, query::DynAdvancedCapability::dynamic_drop_dispatch);
    ASSERT_NE(dynamic_drop, nullptr);
    EXPECT_EQ(dynamic_drop->decision, query::DynAdvancedPolicyDecision::requires_runtime_stage);
    EXPECT_TRUE(dynamic_drop->impact.drop_model_impact);
    EXPECT_TRUE(dynamic_drop->impact.resource_model_impact);
    EXPECT_TRUE(dynamic_drop->impact.runtime_required);
    EXPECT_EQ(dynamic_drop->required_metadata_policy, "dynamic_drop_metadata_v1");

    const query::DynAdvancedDesignCandidate* allocator =
        find_candidate(gate, query::DynAdvancedCapability::allocator_policy);
    ASSERT_NE(allocator, nullptr);
    EXPECT_EQ(allocator->decision, query::DynAdvancedPolicyDecision::requires_standard_library_stage);
    EXPECT_TRUE(allocator->impact.abi_policy_required);
    EXPECT_TRUE(allocator->impact.standard_library_required);
    EXPECT_FALSE(allocator->impact.runtime_required);

    const query::DynAdvancedDesignCandidate* composition =
        find_candidate(gate, query::DynAdvancedCapability::multi_trait_composition);
    ASSERT_NE(composition, nullptr);
    EXPECT_EQ(composition->decision, query::DynAdvancedPolicyDecision::requires_new_metadata_policy);
    EXPECT_TRUE(composition->impact.borrow_model_impact);
    EXPECT_TRUE(composition->impact.tooling_cache_impact);
    EXPECT_FALSE(composition->impact.standard_library_required);
}

TEST(QueryUnit, DynAdvancedDesignGateValidationRejectsPolicyAndStageDrift)
{
    const query::DynAdvancedDesignGate gate = query::m9c_dyn_advanced_design_gate_baseline();
    ASSERT_TRUE(query::is_valid(gate));

    query::DynAdvancedDesignCandidate missing_metadata =
        *find_candidate(gate, query::DynAdvancedCapability::supertrait_upcasting);
    missing_metadata.required_metadata_policy.clear();
    EXPECT_FALSE(query::is_valid(missing_metadata));

    query::DynAdvancedDesignCandidate borrowed_metadata =
        *find_candidate(gate, query::DynAdvancedCapability::dynamic_drop_dispatch);
    borrowed_metadata.required_metadata_policy = "borrowed_methods_only_v1";
    EXPECT_FALSE(query::is_valid(borrowed_metadata));

    query::DynAdvancedDesignCandidate missing_abi =
        *find_candidate(gate, query::DynAdvancedCapability::owning_dyn);
    missing_abi.required_abi_policy.clear();
    EXPECT_FALSE(query::is_valid(missing_abi));

    query::DynAdvancedDesignCandidate borrowed_abi =
        *find_candidate(gate, query::DynAdvancedCapability::allocator_policy);
    borrowed_abi.required_abi_policy = "borrowed_view_v1";
    EXPECT_FALSE(query::is_valid(borrowed_abi));

    query::DynAdvancedDesignCandidate wrong_decision =
        *find_candidate(gate, query::DynAdvancedCapability::dynamic_drop_dispatch);
    wrong_decision.decision = query::DynAdvancedPolicyDecision::requires_new_metadata_policy;
    EXPECT_FALSE(query::is_valid(wrong_decision));

    query::DynAdvancedDesignCandidate research_stage =
        *find_candidate(gate, query::DynAdvancedCapability::owning_dyn);
    research_stage.stage = query::DynAdvancedGateStage::research_only;
    EXPECT_FALSE(query::is_valid(research_stage));

    query::DynAdvancedDesignCandidate premature_ready =
        *find_candidate(gate, query::DynAdvancedCapability::multi_trait_composition);
    premature_ready.stage = query::DynAdvancedGateStage::ready_for_future_stage;
    EXPECT_FALSE(query::is_valid(premature_ready));

    query::DynAdvancedDesignCandidate missing_detail =
        *find_candidate(gate, query::DynAdvancedCapability::supertrait_upcasting);
    missing_detail.blockers.clear();
    EXPECT_FALSE(query::is_valid(missing_detail));

    query::DynAdvancedDesignGate empty_gate;
    empty_gate.name = "empty";
    EXPECT_FALSE(query::is_valid(empty_gate));

    query::DynAdvancedDesignGate duplicate_capability = gate;
    ASSERT_GE(duplicate_capability.candidates.size(), QUERY_TEST_M9C_ADVANCED_CANDIDATE_COUNT);
    duplicate_capability.candidates.back().capability = query::DynAdvancedCapability::supertrait_upcasting;
    EXPECT_FALSE(query::is_valid(duplicate_capability));
}

TEST(QueryUnit, DynAdvancedDesignGateFingerprintSummaryAndDumpAreStable)
{
    const query::DynAdvancedDesignGate gate = query::m9c_dyn_advanced_design_gate_baseline();
    query::DynAdvancedDesignGate changed = gate;
    ASSERT_FALSE(changed.candidates.empty());
    changed.candidates.front().blockers.push_back("new blocker changes the gate contract");
    changed.fingerprint = query::dyn_advanced_design_gate_fingerprint(changed);

    EXPECT_NE(query::dyn_advanced_design_gate_fingerprint(gate),
        query::dyn_advanced_design_gate_fingerprint(changed));

    const std::string summary = query::summarize_dyn_advanced_design_gate(gate);
    EXPECT_NE(summary.find("dyn_advanced_design_gate name=M9c Advanced Dyn Design Gate"),
        std::string::npos);
    EXPECT_NE(summary.find("candidates=5"), std::string::npos);
    EXPECT_NE(summary.find("baseline_abi=borrowed_view_v1"), std::string::npos);
    EXPECT_NE(summary.find("baseline_metadata=borrowed_methods_only_v1"), std::string::npos);

    const std::string dump = query::dump_dyn_advanced_design_gate(gate);
    EXPECT_NE(dump.find("capability=owning_dyn"), std::string::npos);
    EXPECT_NE(dump.find("decision=requires_standard_library_stage"), std::string::npos);
    EXPECT_NE(dump.find("required_abi_policy=owning_dyn_container_v1"), std::string::npos);
    EXPECT_NE(dump.find("required_metadata_policy=dynamic_drop_metadata_v1"), std::string::npos);
    EXPECT_NE(dump.find("non_goal=standard_library_runtime_not_in_m9c"), std::string::npos);
    EXPECT_NE(dump.find("do_not_reuse_borrowed_view_v1_for_owning_dyn"), std::string::npos);
    EXPECT_NE(dump.find("do_not_add_destructor_slot_to_borrowed_methods_only_v1"), std::string::npos);
}

TEST(QueryUnit, DynAdvancedDesignGateM11aSelectsPrincipalSetComposition)
{
    const query::DynAdvancedDesignGate gate = query::m11a_dyn_advanced_design_gate_baseline();

    ASSERT_EQ(gate.name, "M11a Advanced Dyn Design Baseline");
    ASSERT_EQ(gate.candidates.size(), QUERY_TEST_M11A_ADVANCED_CANDIDATE_COUNT);
    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_TRUE(query::is_valid_m11a_dyn_advanced_design_gate(gate));
    EXPECT_EQ(gate.fingerprint, query::dyn_advanced_design_gate_fingerprint(gate));

    const query::DynAdvancedDesignCandidate* supertrait =
        find_candidate(gate, query::DynAdvancedCapability::supertrait_upcasting);
    ASSERT_NE(supertrait, nullptr);
    EXPECT_EQ(supertrait->stage, query::DynAdvancedGateStage::completed_release_baseline);
    EXPECT_EQ(supertrait->required_metadata_policy, "supertrait_vptr_metadata_v1");
    EXPECT_TRUE(has_non_goal(*supertrait, "do_not_reopen_m10_supertrait_runtime_in_m11a"));

    const query::DynAdvancedDesignCandidate* composition =
        find_candidate(gate, query::DynAdvancedCapability::multi_trait_composition);
    ASSERT_NE(composition, nullptr);
    EXPECT_EQ(composition->stage, query::DynAdvancedGateStage::ready_for_future_stage);
    EXPECT_EQ(composition->decision, query::DynAdvancedPolicyDecision::requires_new_metadata_policy);
    EXPECT_EQ(composition->required_metadata_policy, "principal_set_metadata_v1");
    EXPECT_TRUE(composition->impact.borrow_model_impact);
    EXPECT_TRUE(composition->impact.tooling_cache_impact);
    EXPECT_FALSE(composition->impact.standard_library_required);
    EXPECT_FALSE(composition->impact.runtime_required);
    EXPECT_TRUE(has_non_goal(*composition, "standard_library_runtime_not_in_m11a"));
    EXPECT_TRUE(has_non_goal(*composition, "runtime_dispatch_not_in_m11a"));
    EXPECT_TRUE(has_non_goal(*composition, "do_not_encode_principal_set_as_single_trait_object"));

    const query::DynAdvancedDesignCandidate* owning =
        find_candidate(gate, query::DynAdvancedCapability::owning_dyn);
    ASSERT_NE(owning, nullptr);
    EXPECT_EQ(owning->stage, query::DynAdvancedGateStage::prototype_blocked);
    EXPECT_EQ(owning->decision, query::DynAdvancedPolicyDecision::requires_standard_library_stage);
    EXPECT_TRUE(owning->impact.standard_library_required);
    EXPECT_TRUE(owning->impact.runtime_required);

    const query::DynAdvancedDesignCandidate* dynamic_drop =
        find_candidate(gate, query::DynAdvancedCapability::dynamic_drop_dispatch);
    ASSERT_NE(dynamic_drop, nullptr);
    EXPECT_EQ(dynamic_drop->decision, query::DynAdvancedPolicyDecision::requires_runtime_stage);
    EXPECT_TRUE(dynamic_drop->impact.runtime_required);
}

TEST(QueryUnit, DynAdvancedDesignGateM11aValidationRejectsBoundaryDrift)
{
    const query::DynAdvancedDesignGate gate = query::m11a_dyn_advanced_design_gate_baseline();
    ASSERT_TRUE(query::is_valid_m11a_dyn_advanced_design_gate(gate));

    query::DynAdvancedDesignGate reopened_supertrait = gate;
    query::DynAdvancedDesignCandidate* const supertrait =
        find_candidate(reopened_supertrait, query::DynAdvancedCapability::supertrait_upcasting);
    ASSERT_NE(supertrait, nullptr);
    supertrait->stage = query::DynAdvancedGateStage::design_gate;
    EXPECT_FALSE(query::is_valid_m11a_dyn_advanced_design_gate(reopened_supertrait));

    query::DynAdvancedDesignGate flattened_composition = gate;
    query::DynAdvancedDesignCandidate* const composition =
        find_candidate(flattened_composition, query::DynAdvancedCapability::multi_trait_composition);
    ASSERT_NE(composition, nullptr);
    composition->required_metadata_policy = "borrowed_methods_only_v1";
    EXPECT_FALSE(query::is_valid_m11a_dyn_advanced_design_gate(flattened_composition));

    query::DynAdvancedDesignGate runtime_composition = gate;
    query::DynAdvancedDesignCandidate* const runtime_candidate =
        find_candidate(runtime_composition, query::DynAdvancedCapability::multi_trait_composition);
    ASSERT_NE(runtime_candidate, nullptr);
    runtime_candidate->impact.runtime_required = true;
    runtime_candidate->decision = query::DynAdvancedPolicyDecision::requires_runtime_stage;
    EXPECT_FALSE(query::is_valid_m11a_dyn_advanced_design_gate(runtime_composition));

    query::DynAdvancedDesignGate duplicate_capability = gate;
    query::DynAdvancedDesignCandidate* const allocator =
        find_candidate(duplicate_capability, query::DynAdvancedCapability::allocator_policy);
    ASSERT_NE(allocator, nullptr);
    allocator->capability = query::DynAdvancedCapability::multi_trait_composition;
    EXPECT_FALSE(query::is_valid_m11a_dyn_advanced_design_gate(duplicate_capability));

    query::DynAdvancedDesignGate wrong_name = gate;
    wrong_name.name = "M11 wrong gate";
    EXPECT_FALSE(query::is_valid_m11a_dyn_advanced_design_gate(wrong_name));
    EXPECT_FALSE(query::is_valid(wrong_name));
}

TEST(QueryUnit, DynAdvancedDesignGateM11aSummaryAndDumpExposeSelection)
{
    const query::DynAdvancedDesignGate gate = query::m11a_dyn_advanced_design_gate_baseline();
    query::DynAdvancedDesignGate changed = gate;
    query::DynAdvancedDesignCandidate* const composition =
        find_candidate(changed, query::DynAdvancedCapability::multi_trait_composition);
    ASSERT_NE(composition, nullptr);
    composition->required_facts.push_back("new fact changes m11a gate contract");

    EXPECT_NE(query::dyn_advanced_design_gate_fingerprint(gate),
        query::dyn_advanced_design_gate_fingerprint(changed));

    const std::string summary = query::summarize_dyn_advanced_design_gate(gate);
    EXPECT_NE(summary.find("dyn_advanced_design_gate name=M11a Advanced Dyn Design Baseline"),
        std::string::npos) << summary;
    EXPECT_NE(summary.find("candidates=5"), std::string::npos) << summary;
    EXPECT_NE(summary.find("ready_for_future_stage=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("completed_release=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("standard_library_blocked=2"), std::string::npos) << summary;
    EXPECT_NE(summary.find("runtime_blocked=2"), std::string::npos) << summary;

    const std::string dump = query::dump_dyn_advanced_design_gate(gate);
    EXPECT_NE(dump.find("capability=multi_trait_composition"), std::string::npos) << dump;
    EXPECT_NE(dump.find("stage=ready_for_future_stage"), std::string::npos) << dump;
    EXPECT_NE(dump.find("required_metadata_policy=principal_set_metadata_v1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("required_fact=principal_set_identity_fact"), std::string::npos) << dump;
    EXPECT_NE(dump.find("non_goal=standard_library_runtime_not_in_m11a"), std::string::npos) << dump;
    EXPECT_NE(dump.find("non_goal=do_not_encode_principal_set_as_single_trait_object"), std::string::npos)
        << dump;
}

} // namespace aurex::test
