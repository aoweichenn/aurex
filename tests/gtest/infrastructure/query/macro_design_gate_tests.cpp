#include <aurex/infrastructure/query/macro_design_gate.hpp>

#include <algorithm>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u8 QUERY_TEST_INVALID_MACRO_CAPABILITY = 240U;
constexpr base::u8 QUERY_TEST_INVALID_MACRO_STAGE = 241U;
constexpr base::u8 QUERY_TEST_INVALID_MACRO_DECISION = 242U;
constexpr base::usize QUERY_TEST_M21A_MACRO_CANDIDATE_COUNT = 7U;

[[nodiscard]] const query::MacroDesignCandidate* find_candidate(
    const query::MacroDesignGate& gate, const query::MacroDesignCapability capability) noexcept
{
    const auto found = std::find_if(gate.candidates.begin(), gate.candidates.end(),
        [capability](const query::MacroDesignCandidate& candidate) {
            return candidate.capability == capability;
        });
    return found == gate.candidates.end() ? nullptr : &*found;
}

[[nodiscard]] query::MacroDesignCandidate* find_candidate(
    query::MacroDesignGate& gate, const query::MacroDesignCapability capability) noexcept
{
    const auto found = std::find_if(gate.candidates.begin(), gate.candidates.end(),
        [capability](const query::MacroDesignCandidate& candidate) {
            return candidate.capability == capability;
        });
    return found == gate.candidates.end() ? nullptr : &*found;
}

[[nodiscard]] bool has_non_goal(
    const query::MacroDesignCandidate& candidate, const std::string_view non_goal) noexcept
{
    return std::any_of(candidate.non_goals.begin(), candidate.non_goals.end(),
        [non_goal](const std::string& value) {
            return std::string_view(value) == non_goal;
        });
}

} // namespace

TEST(QueryUnit, MacroDesignGateExposesEnumNamesAndInvalidFallbacks)
{
    EXPECT_EQ(query::macro_design_capability_name(
                  query::MacroDesignCapability::token_tree_and_attribute_surface),
        "token_tree_and_attribute_surface");
    EXPECT_EQ(query::macro_design_capability_name(
                  query::MacroDesignCapability::hygienic_name_resolution),
        "hygienic_name_resolution");
    EXPECT_EQ(query::macro_design_capability_name(
                  query::MacroDesignCapability::expansion_source_map_and_debug_trace),
        "expansion_source_map_and_debug_trace");
    EXPECT_EQ(query::macro_design_capability_name(
                  query::MacroDesignCapability::query_backed_incremental_expansion),
        "query_backed_incremental_expansion");
    EXPECT_EQ(query::macro_design_capability_name(
                  query::MacroDesignCapability::attached_item_codegen_surface),
        "attached_item_codegen_surface");
    EXPECT_EQ(query::macro_design_capability_name(
                  query::MacroDesignCapability::typed_expression_macro_boundary),
        "typed_expression_macro_boundary");
    EXPECT_EQ(query::macro_design_capability_name(
                  query::MacroDesignCapability::external_procedural_macro_sandbox),
        "external_procedural_macro_sandbox");
    EXPECT_EQ(query::macro_design_capability_name(
                  static_cast<query::MacroDesignCapability>(QUERY_TEST_INVALID_MACRO_CAPABILITY)),
        "invalid");

    EXPECT_EQ(query::macro_design_gate_stage_name(query::MacroDesignGateStage::research_only),
        "research_only");
    EXPECT_EQ(query::macro_design_gate_stage_name(query::MacroDesignGateStage::design_gate),
        "design_gate");
    EXPECT_EQ(query::macro_design_gate_stage_name(query::MacroDesignGateStage::ready_for_implementation),
        "ready_for_implementation");
    EXPECT_EQ(query::macro_design_gate_stage_name(query::MacroDesignGateStage::blocked_by_dependency),
        "blocked_by_dependency");
    EXPECT_EQ(query::macro_design_gate_stage_name(query::MacroDesignGateStage::future_stage),
        "future_stage");
    EXPECT_EQ(query::macro_design_gate_stage_name(
                  static_cast<query::MacroDesignGateStage>(QUERY_TEST_INVALID_MACRO_STAGE)),
        "invalid");

    EXPECT_EQ(query::macro_design_policy_decision_name(query::MacroDesignPolicyDecision::rejected),
        "rejected");
    EXPECT_EQ(query::macro_design_policy_decision_name(
                  query::MacroDesignPolicyDecision::selected_m21_frontend_query_path),
        "selected_m21_frontend_query_path");
    EXPECT_EQ(query::macro_design_policy_decision_name(
                  query::MacroDesignPolicyDecision::requires_typed_macro_engine),
        "requires_typed_macro_engine");
    EXPECT_EQ(query::macro_design_policy_decision_name(
                  query::MacroDesignPolicyDecision::requires_process_sandbox),
        "requires_process_sandbox");
    EXPECT_EQ(query::macro_design_policy_decision_name(
                  query::MacroDesignPolicyDecision::requires_later_language_surface),
        "requires_later_language_surface");
    EXPECT_EQ(query::macro_design_policy_decision_name(
                  static_cast<query::MacroDesignPolicyDecision>(QUERY_TEST_INVALID_MACRO_DECISION)),
        "invalid");

    EXPECT_FALSE(query::is_valid(static_cast<query::MacroDesignCapability>(QUERY_TEST_INVALID_MACRO_CAPABILITY)));
    EXPECT_FALSE(query::is_valid(static_cast<query::MacroDesignGateStage>(QUERY_TEST_INVALID_MACRO_STAGE)));
    EXPECT_FALSE(query::is_valid(static_cast<query::MacroDesignPolicyDecision>(QUERY_TEST_INVALID_MACRO_DECISION)));
}

TEST(QueryUnit, MacroDesignGateM21aSelectsFrontendQueryMacroFoundation)
{
    const query::MacroDesignGate gate = query::m21a_macro_design_gate_baseline();

    ASSERT_EQ(gate.name, "M21a Macro System Design Gate");
    ASSERT_EQ(gate.candidates.size(), QUERY_TEST_M21A_MACRO_CANDIDATE_COUNT);
    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_TRUE(query::is_valid_m21a_macro_design_gate(gate));
    EXPECT_EQ(gate.fingerprint, query::macro_design_gate_fingerprint(gate));

    const query::MacroDesignCandidate* token_tree = find_candidate(
        gate, query::MacroDesignCapability::token_tree_and_attribute_surface);
    ASSERT_NE(token_tree, nullptr);
    EXPECT_EQ(token_tree->stage, query::MacroDesignGateStage::ready_for_implementation);
    EXPECT_EQ(token_tree->decision, query::MacroDesignPolicyDecision::selected_m21_frontend_query_path);
    EXPECT_EQ(token_tree->selected_policy, "token_tree_attribute_surface_v1");
    EXPECT_TRUE(token_tree->impact.lexer_parser_impact);
    EXPECT_TRUE(token_tree->impact.ast_model_impact);
    EXPECT_TRUE(token_tree->impact.query_key_impact);
    EXPECT_TRUE(has_non_goal(*token_tree, "do_not_support_textual_macros"));

    const query::MacroDesignCandidate* hygiene = find_candidate(
        gate, query::MacroDesignCapability::hygienic_name_resolution);
    ASSERT_NE(hygiene, nullptr);
    EXPECT_EQ(hygiene->selected_policy, "origin_mark_hygiene_v1");
    EXPECT_TRUE(hygiene->impact.hygiene_required);
    EXPECT_TRUE(hygiene->impact.sema_name_resolution_impact);
    EXPECT_TRUE(has_non_goal(*hygiene, "do_not_allow_macros_to_bypass_sema_or_borrow_checking"));

    const query::MacroDesignCandidate* trace = find_candidate(
        gate, query::MacroDesignCapability::expansion_source_map_and_debug_trace);
    ASSERT_NE(trace, nullptr);
    EXPECT_EQ(trace->selected_policy, "expansion_source_map_debug_trace_v1");
    EXPECT_TRUE(trace->impact.tooling_debug_impact);
    EXPECT_TRUE(trace->impact.source_map_impact);

    const query::MacroDesignCandidate* attached = find_candidate(
        gate, query::MacroDesignCapability::attached_item_codegen_surface);
    ASSERT_NE(attached, nullptr);
    EXPECT_EQ(attached->selected_policy, "attached_item_codegen_declared_names_v1");
    EXPECT_TRUE(attached->impact.hygiene_required);
    EXPECT_TRUE(has_non_goal(*attached, "standard_library_not_in_m21a"));
}

TEST(QueryUnit, MacroDesignGateM21aBlocksTypedAndExternalMacroWork)
{
    const query::MacroDesignGate gate = query::m21a_macro_design_gate_baseline();

    const query::MacroDesignCandidate* typed_expr = find_candidate(
        gate, query::MacroDesignCapability::typed_expression_macro_boundary);
    ASSERT_NE(typed_expr, nullptr);
    EXPECT_EQ(typed_expr->stage, query::MacroDesignGateStage::future_stage);
    EXPECT_EQ(typed_expr->decision, query::MacroDesignPolicyDecision::requires_typed_macro_engine);
    EXPECT_TRUE(typed_expr->impact.sema_name_resolution_impact);
    EXPECT_TRUE(typed_expr->impact.hygiene_required);
    EXPECT_FALSE(typed_expr->impact.external_process_required);

    const query::MacroDesignCandidate* external = find_candidate(
        gate, query::MacroDesignCapability::external_procedural_macro_sandbox);
    ASSERT_NE(external, nullptr);
    EXPECT_EQ(external->stage, query::MacroDesignGateStage::blocked_by_dependency);
    EXPECT_EQ(external->decision, query::MacroDesignPolicyDecision::requires_process_sandbox);
    EXPECT_EQ(external->selected_policy, "external_proc_macro_sandbox_boundary_v1");
    EXPECT_TRUE(external->impact.external_process_required);
    EXPECT_FALSE(external->impact.standard_library_required);
    EXPECT_TRUE(has_non_goal(*external, "do_not_expand_external_proc_macros_in_m21a"));
}

TEST(QueryUnit, MacroDesignGateM21aValidationRejectsBoundaryDrift)
{
    const query::MacroDesignGate gate = query::m21a_macro_design_gate_baseline();
    ASSERT_TRUE(query::is_valid_m21a_macro_design_gate(gate));

    query::MacroDesignGate wrong_name = gate;
    wrong_name.name = "M21a wrong macro gate";
    EXPECT_FALSE(query::is_valid(wrong_name));
    EXPECT_FALSE(query::is_valid_m21a_macro_design_gate(wrong_name));

    query::MacroDesignGate textual_macros = gate;
    query::MacroDesignCandidate* const token_tree = find_candidate(
        textual_macros, query::MacroDesignCapability::token_tree_and_attribute_surface);
    ASSERT_NE(token_tree, nullptr);
    token_tree->non_goals.erase(std::remove(token_tree->non_goals.begin(), token_tree->non_goals.end(),
                                      "do_not_support_textual_macros"),
        token_tree->non_goals.end());
    EXPECT_FALSE(query::is_valid_m21a_macro_design_gate(textual_macros));

    query::MacroDesignGate runtime_macros = gate;
    query::MacroDesignCandidate* const trace = find_candidate(
        runtime_macros, query::MacroDesignCapability::expansion_source_map_and_debug_trace);
    ASSERT_NE(trace, nullptr);
    trace->non_goals.erase(std::remove(trace->non_goals.begin(), trace->non_goals.end(),
                                 "runtime_not_in_m21a"),
        trace->non_goals.end());
    EXPECT_FALSE(query::is_valid_m21a_macro_design_gate(runtime_macros));

    query::MacroDesignGate hidden_std = gate;
    query::MacroDesignCandidate* const attached = find_candidate(
        hidden_std, query::MacroDesignCapability::attached_item_codegen_surface);
    ASSERT_NE(attached, nullptr);
    attached->impact.standard_library_required = true;
    attached->decision = query::MacroDesignPolicyDecision::requires_later_language_surface;
    EXPECT_FALSE(query::is_valid_m21a_macro_design_gate(hidden_std));

    query::MacroDesignGate premature_typed_expr = gate;
    query::MacroDesignCandidate* const typed_expr = find_candidate(
        premature_typed_expr, query::MacroDesignCapability::typed_expression_macro_boundary);
    ASSERT_NE(typed_expr, nullptr);
    typed_expr->stage = query::MacroDesignGateStage::ready_for_implementation;
    typed_expr->decision = query::MacroDesignPolicyDecision::selected_m21_frontend_query_path;
    EXPECT_FALSE(query::is_valid_m21a_macro_design_gate(premature_typed_expr));

    query::MacroDesignGate unsandboxed_external = gate;
    query::MacroDesignCandidate* const external = find_candidate(
        unsandboxed_external, query::MacroDesignCapability::external_procedural_macro_sandbox);
    ASSERT_NE(external, nullptr);
    external->impact.external_process_required = false;
    external->decision = query::MacroDesignPolicyDecision::selected_m21_frontend_query_path;
    EXPECT_FALSE(query::is_valid_m21a_macro_design_gate(unsandboxed_external));

    query::MacroDesignGate duplicate_capability = gate;
    query::MacroDesignCandidate* const duplicate_external = find_candidate(
        duplicate_capability, query::MacroDesignCapability::external_procedural_macro_sandbox);
    ASSERT_NE(duplicate_external, nullptr);
    duplicate_external->capability = query::MacroDesignCapability::attached_item_codegen_surface;
    EXPECT_FALSE(query::is_valid_m21a_macro_design_gate(duplicate_capability));
}

TEST(QueryUnit, MacroDesignGateM21aSummaryAndDumpAreStable)
{
    const query::MacroDesignGate gate = query::m21a_macro_design_gate_baseline();
    query::MacroDesignGate changed = gate;
    ASSERT_FALSE(changed.candidates.empty());
    changed.candidates.front().required_facts.push_back("new fact changes m21a macro gate");

    EXPECT_NE(query::macro_design_gate_fingerprint(gate), query::macro_design_gate_fingerprint(changed));

    const std::string summary = query::summarize_macro_design_gate(gate);
    EXPECT_NE(summary.find("macro_design_gate name=M21a Macro System Design Gate"), std::string::npos)
        << summary;
    EXPECT_NE(summary.find("candidates=7"), std::string::npos) << summary;
    EXPECT_NE(summary.find("ready_for_implementation=5"), std::string::npos) << summary;
    EXPECT_NE(summary.find("blocked_by_dependency=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("future_stage=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("query_key_impact=7"), std::string::npos) << summary;
    EXPECT_NE(summary.find("hygiene_required=6"), std::string::npos) << summary;
    EXPECT_NE(summary.find("source_map_impact=7"), std::string::npos) << summary;
    EXPECT_NE(summary.find("external_process_required=1"), std::string::npos) << summary;

    const std::string dump = query::dump_macro_design_gate(gate);
    EXPECT_NE(dump.find("capability=token_tree_and_attribute_surface"), std::string::npos) << dump;
    EXPECT_NE(dump.find("selected_policy=token_tree_attribute_surface_v1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("required_fact=macro_expansion_trace_fact"), std::string::npos) << dump;
    EXPECT_NE(dump.find("required_fact=attached_macro_declared_name_fact"), std::string::npos) << dump;
    EXPECT_NE(dump.find("non_goal=standard_library_not_in_m21a"), std::string::npos) << dump;
    EXPECT_NE(dump.find("non_goal=do_not_expand_external_proc_macros_in_m21a"), std::string::npos) << dump;
}

} // namespace aurex::test
