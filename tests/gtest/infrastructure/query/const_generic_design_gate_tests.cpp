#include <aurex/infrastructure/query/const_generic_design_gate.hpp>

#include <algorithm>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u8 QUERY_TEST_INVALID_CONST_GENERIC_CAPABILITY = 230U;
constexpr base::u8 QUERY_TEST_INVALID_CONST_GENERIC_STAGE = 231U;
constexpr base::u8 QUERY_TEST_INVALID_CONST_GENERIC_DECISION = 232U;
constexpr base::usize QUERY_TEST_M15_CONST_GENERIC_CANDIDATE_COUNT = 6;

[[nodiscard]] const query::ConstGenericDesignCandidate* find_candidate(
    const query::ConstGenericDesignGate& gate, const query::ConstGenericCapability capability) noexcept
{
    const auto found = std::find_if(gate.candidates.begin(), gate.candidates.end(),
        [capability](const query::ConstGenericDesignCandidate& candidate) {
            return candidate.capability == capability;
        });
    return found == gate.candidates.end() ? nullptr : &*found;
}

[[nodiscard]] query::ConstGenericDesignCandidate* find_candidate(
    query::ConstGenericDesignGate& gate, const query::ConstGenericCapability capability) noexcept
{
    const auto found = std::find_if(gate.candidates.begin(), gate.candidates.end(),
        [capability](const query::ConstGenericDesignCandidate& candidate) {
            return candidate.capability == capability;
        });
    return found == gate.candidates.end() ? nullptr : &*found;
}

[[nodiscard]] bool has_non_goal(
    const query::ConstGenericDesignCandidate& candidate, const std::string_view non_goal) noexcept
{
    return std::any_of(candidate.non_goals.begin(), candidate.non_goals.end(),
        [non_goal](const std::string& value) {
            return std::string_view(value) == non_goal;
        });
}

} // namespace

TEST(QueryUnit, ConstGenericDesignGateExposesEnumNamesAndInvalidFallbacks)
{
    EXPECT_EQ(query::const_generic_capability_name(
                  query::ConstGenericCapability::typed_const_parameter_surface),
        "typed_const_parameter_surface");
    EXPECT_EQ(query::const_generic_capability_name(
                  query::ConstGenericCapability::canonical_const_argument_identity),
        "canonical_const_argument_identity");
    EXPECT_EQ(query::const_generic_capability_name(
                  query::ConstGenericCapability::generic_instance_key_integration),
        "generic_instance_key_integration");
    EXPECT_EQ(query::const_generic_capability_name(
                  query::ConstGenericCapability::array_length_type_integration),
        "array_length_type_integration");
    EXPECT_EQ(query::const_generic_capability_name(
                  query::ConstGenericCapability::const_expression_evaluation_subset),
        "const_expression_evaluation_subset");
    EXPECT_EQ(query::const_generic_capability_name(
                  query::ConstGenericCapability::trait_predicate_and_dyn_boundary),
        "trait_predicate_and_dyn_boundary");
    EXPECT_EQ(query::const_generic_capability_name(
                  static_cast<query::ConstGenericCapability>(QUERY_TEST_INVALID_CONST_GENERIC_CAPABILITY)),
        "invalid");

    EXPECT_EQ(query::const_generic_gate_stage_name(query::ConstGenericGateStage::research_only),
        "research_only");
    EXPECT_EQ(query::const_generic_gate_stage_name(query::ConstGenericGateStage::design_gate),
        "design_gate");
    EXPECT_EQ(query::const_generic_gate_stage_name(query::ConstGenericGateStage::ready_for_implementation),
        "ready_for_implementation");
    EXPECT_EQ(query::const_generic_gate_stage_name(query::ConstGenericGateStage::blocked_by_dependency),
        "blocked_by_dependency");
    EXPECT_EQ(query::const_generic_gate_stage_name(query::ConstGenericGateStage::future_stage),
        "future_stage");
    EXPECT_EQ(query::const_generic_gate_stage_name(
                  static_cast<query::ConstGenericGateStage>(QUERY_TEST_INVALID_CONST_GENERIC_STAGE)),
        "invalid");

    EXPECT_EQ(query::const_generic_policy_decision_name(query::ConstGenericPolicyDecision::rejected),
        "rejected");
    EXPECT_EQ(query::const_generic_policy_decision_name(
                  query::ConstGenericPolicyDecision::selected_m15_frontend_query_path),
        "selected_m15_frontend_query_path");
    EXPECT_EQ(query::const_generic_policy_decision_name(
                  query::ConstGenericPolicyDecision::requires_comptime_engine),
        "requires_comptime_engine");
    EXPECT_EQ(query::const_generic_policy_decision_name(
                  query::ConstGenericPolicyDecision::requires_trait_solver_extension),
        "requires_trait_solver_extension");
    EXPECT_EQ(query::const_generic_policy_decision_name(
                  query::ConstGenericPolicyDecision::requires_runtime_or_std_boundary),
        "requires_runtime_or_std_boundary");
    EXPECT_EQ(query::const_generic_policy_decision_name(
                  static_cast<query::ConstGenericPolicyDecision>(QUERY_TEST_INVALID_CONST_GENERIC_DECISION)),
        "invalid");

    EXPECT_FALSE(query::is_valid(
        static_cast<query::ConstGenericCapability>(QUERY_TEST_INVALID_CONST_GENERIC_CAPABILITY)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::ConstGenericGateStage>(QUERY_TEST_INVALID_CONST_GENERIC_STAGE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::ConstGenericPolicyDecision>(QUERY_TEST_INVALID_CONST_GENERIC_DECISION)));
}

TEST(QueryUnit, ConstGenericDesignGateM15SelectsTypedScalarFrontendQueryPath)
{
    const query::ConstGenericDesignGate gate = query::m15_const_generic_design_gate_baseline();

    ASSERT_EQ(gate.name, "M15 Const Generic Design Baseline");
    ASSERT_EQ(gate.candidates.size(), QUERY_TEST_M15_CONST_GENERIC_CANDIDATE_COUNT);
    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_TRUE(query::is_valid_m15_const_generic_design_gate(gate));
    EXPECT_EQ(gate.fingerprint, query::const_generic_design_gate_fingerprint(gate));

    const query::ConstGenericDesignCandidate* surface = find_candidate(
        gate, query::ConstGenericCapability::typed_const_parameter_surface);
    ASSERT_NE(surface, nullptr);
    EXPECT_EQ(surface->stage, query::ConstGenericGateStage::ready_for_implementation);
    EXPECT_EQ(surface->decision, query::ConstGenericPolicyDecision::selected_m15_frontend_query_path);
    EXPECT_EQ(surface->selected_policy, "typed_const_param_v1");
    EXPECT_TRUE(surface->impact.parser_ast_impact);
    EXPECT_TRUE(surface->impact.query_key_impact);
    EXPECT_FALSE(surface->impact.runtime_required);
    EXPECT_TRUE(has_non_goal(*surface, "do_not_support_untyped_const_params"));

    const query::ConstGenericDesignCandidate* identity = find_candidate(
        gate, query::ConstGenericCapability::canonical_const_argument_identity);
    ASSERT_NE(identity, nullptr);
    EXPECT_EQ(identity->selected_policy, "canonical_const_value_v1");
    EXPECT_TRUE(identity->impact.incremental_cache_impact);
    EXPECT_TRUE(has_non_goal(*identity, "do_not_key_const_arguments_by_display_text"));

    const query::ConstGenericDesignCandidate* instance_key = find_candidate(
        gate, query::ConstGenericCapability::generic_instance_key_integration);
    ASSERT_NE(instance_key, nullptr);
    EXPECT_EQ(instance_key->selected_policy, "generic_instance_const_arg_key_v1");
    EXPECT_TRUE(has_non_goal(*instance_key, "do_not_reuse_type_placeholder_for_const_params"));

    const query::ConstGenericDesignCandidate* array_length = find_candidate(
        gate, query::ConstGenericCapability::array_length_type_integration);
    ASSERT_NE(array_length, nullptr);
    EXPECT_EQ(array_length->stage, query::ConstGenericGateStage::ready_for_implementation);
    EXPECT_TRUE(array_length->impact.ir_layout_impact);
    EXPECT_TRUE(has_non_goal(*array_length, "do_not_support_generic_arithmetic_array_lengths_in_m15"));
}

TEST(QueryUnit, ConstGenericDesignGateM15BlocksComptimeAndTraitSolverWork)
{
    const query::ConstGenericDesignGate gate = query::m15_const_generic_design_gate_baseline();

    const query::ConstGenericDesignCandidate* const_eval = find_candidate(
        gate, query::ConstGenericCapability::const_expression_evaluation_subset);
    ASSERT_NE(const_eval, nullptr);
    EXPECT_EQ(const_eval->stage, query::ConstGenericGateStage::blocked_by_dependency);
    EXPECT_EQ(const_eval->decision, query::ConstGenericPolicyDecision::requires_comptime_engine);
    EXPECT_TRUE(const_eval->impact.comptime_engine_required);
    EXPECT_TRUE(has_non_goal(*const_eval, "do_not_support_generic_const_arithmetic_in_m15"));

    const query::ConstGenericDesignCandidate* trait_boundary = find_candidate(
        gate, query::ConstGenericCapability::trait_predicate_and_dyn_boundary);
    ASSERT_NE(trait_boundary, nullptr);
    EXPECT_EQ(trait_boundary->stage, query::ConstGenericGateStage::future_stage);
    EXPECT_EQ(trait_boundary->decision,
        query::ConstGenericPolicyDecision::requires_trait_solver_extension);
    EXPECT_TRUE(trait_boundary->impact.trait_solver_impact);
    EXPECT_TRUE(has_non_goal(*trait_boundary, "dyn_const_generic_not_in_m15"));
    EXPECT_TRUE(has_non_goal(*trait_boundary, "do_not_support_const_where_predicates_in_m15"));
}

TEST(QueryUnit, ConstGenericDesignGateM15ValidationRejectsBoundaryDrift)
{
    const query::ConstGenericDesignGate gate = query::m15_const_generic_design_gate_baseline();
    ASSERT_TRUE(query::is_valid_m15_const_generic_design_gate(gate));

    query::ConstGenericDesignGate wrong_name = gate;
    wrong_name.name = "M15 wrong const generic gate";
    EXPECT_FALSE(query::is_valid_m15_const_generic_design_gate(wrong_name));
    EXPECT_FALSE(query::is_valid(wrong_name));

    query::ConstGenericDesignGate untyped_surface = gate;
    query::ConstGenericDesignCandidate* const surface = find_candidate(
        untyped_surface, query::ConstGenericCapability::typed_const_parameter_surface);
    ASSERT_NE(surface, nullptr);
    surface->selected_policy = "untyped_const_param";
    EXPECT_FALSE(query::is_valid_m15_const_generic_design_gate(untyped_surface));

    query::ConstGenericDesignGate hidden_runtime = gate;
    query::ConstGenericDesignCandidate* const array_length = find_candidate(
        hidden_runtime, query::ConstGenericCapability::array_length_type_integration);
    ASSERT_NE(array_length, nullptr);
    array_length->impact.runtime_required = true;
    array_length->decision = query::ConstGenericPolicyDecision::requires_runtime_or_std_boundary;
    EXPECT_FALSE(query::is_valid_m15_const_generic_design_gate(hidden_runtime));

    query::ConstGenericDesignGate premature_const_eval = gate;
    query::ConstGenericDesignCandidate* const const_eval = find_candidate(
        premature_const_eval, query::ConstGenericCapability::const_expression_evaluation_subset);
    ASSERT_NE(const_eval, nullptr);
    const_eval->stage = query::ConstGenericGateStage::ready_for_implementation;
    const_eval->impact.comptime_engine_required = false;
    const_eval->decision = query::ConstGenericPolicyDecision::selected_m15_frontend_query_path;
    EXPECT_FALSE(query::is_valid_m15_const_generic_design_gate(premature_const_eval));

    query::ConstGenericDesignGate duplicate_capability = gate;
    query::ConstGenericDesignCandidate* const trait_boundary = find_candidate(
        duplicate_capability, query::ConstGenericCapability::trait_predicate_and_dyn_boundary);
    ASSERT_NE(trait_boundary, nullptr);
    trait_boundary->capability = query::ConstGenericCapability::array_length_type_integration;
    EXPECT_FALSE(query::is_valid_m15_const_generic_design_gate(duplicate_capability));
}

TEST(QueryUnit, ConstGenericDesignGateM15SummaryAndDumpAreStable)
{
    const query::ConstGenericDesignGate gate = query::m15_const_generic_design_gate_baseline();
    query::ConstGenericDesignGate changed = gate;
    ASSERT_FALSE(changed.candidates.empty());
    changed.candidates.front().required_facts.push_back("new fact changes m15 const generic gate");

    EXPECT_NE(query::const_generic_design_gate_fingerprint(gate),
        query::const_generic_design_gate_fingerprint(changed));

    const std::string summary = query::summarize_const_generic_design_gate(gate);
    EXPECT_NE(summary.find("const_generic_design_gate name=M15 Const Generic Design Baseline"),
        std::string::npos) << summary;
    EXPECT_NE(summary.find("candidates=6"), std::string::npos) << summary;
    EXPECT_NE(summary.find("ready_for_implementation=4"), std::string::npos) << summary;
    EXPECT_NE(summary.find("blocked_by_dependency=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("future_stage=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("query_key_impact=6"), std::string::npos) << summary;
    EXPECT_NE(summary.find("ir_layout_impact=2"), std::string::npos) << summary;

    const std::string dump = query::dump_const_generic_design_gate(gate);
    EXPECT_NE(dump.find("capability=typed_const_parameter_surface"), std::string::npos) << dump;
    EXPECT_NE(dump.find("selected_policy=typed_const_param_v1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("required_fact=canonical_const_value_key"), std::string::npos) << dump;
    EXPECT_NE(dump.find("required_fact=array_length_const_param_fact"), std::string::npos) << dump;
    EXPECT_NE(dump.find("non_goal=standard_library_runtime_not_in_m15"), std::string::npos) << dump;
    EXPECT_NE(dump.find("non_goal=do_not_add_const_associated_values_to_dyn_trait_in_m15"),
        std::string::npos) << dump;
}

} // namespace aurex::test
