#include <aurex/infrastructure/query/macro_expansion_facts.hpp>

#include <algorithm>
#include <string>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u8 QUERY_TEST_INVALID_MACRO_EXPANSION_KIND = 231U;
constexpr base::u8 QUERY_TEST_INVALID_MACRO_EXPANSION_STAGE = 232U;
constexpr base::u8 QUERY_TEST_INVALID_MACRO_EXPANSION_POLICY = 233U;
constexpr base::usize QUERY_TEST_M21C_MACRO_EXPANSION_FACT_COUNT = 7U;
constexpr base::usize QUERY_TEST_M27_MACRO_EXPANSION_FACT_COUNT = 10U;
constexpr base::usize QUERY_TEST_M27B_MACRO_EXPANSION_FACT_COUNT = 13U;
constexpr base::usize QUERY_TEST_M27C_MACRO_EXPANSION_FACT_COUNT = 16U;
constexpr base::usize QUERY_TEST_M27D_MACRO_EXPANSION_FACT_COUNT = 19U;

[[nodiscard]] const query::MacroExpansionFact* find_fact(
    const query::MacroExpansionPlan& plan, const query::MacroExpansionFactKind kind) noexcept
{
    const auto found = std::find_if(plan.facts.begin(), plan.facts.end(),
        [kind](const query::MacroExpansionFact& fact) {
            return fact.kind == kind;
        });
    return found == plan.facts.end() ? nullptr : &*found;
}

[[nodiscard]] query::MacroExpansionFact* find_fact(
    query::MacroExpansionPlan& plan, const query::MacroExpansionFactKind kind) noexcept
{
    const auto found = std::find_if(plan.facts.begin(), plan.facts.end(),
        [kind](const query::MacroExpansionFact& fact) {
            return fact.kind == kind;
        });
    return found == plan.facts.end() ? nullptr : &*found;
}

} // namespace

TEST(QueryUnit, MacroExpansionFactsExposeEnumNamesAndInvalidFallbacks)
{
    EXPECT_EQ(query::macro_expansion_fact_kind_name(query::MacroExpansionFactKind::attribute_token_tree_input),
        "attribute_token_tree_input");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(query::MacroExpansionFactKind::builtin_derive_passthrough),
        "builtin_derive_passthrough");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(query::MacroExpansionFactKind::early_item_expansion_query_key),
        "early_item_expansion_query_key");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(query::MacroExpansionFactKind::generated_module_part_noop),
        "generated_module_part_noop");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(query::MacroExpansionFactKind::expansion_source_map_stub),
        "expansion_source_map_stub");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::unimplemented_item_attribute_blocker),
        "unimplemented_item_attribute_blocker");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::external_procedural_macro_blocked),
        "external_procedural_macro_blocked");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::aurex_declarative_macro_surface),
        "aurex_declarative_macro_surface");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::aurex_user_derive_macro_surface),
        "aurex_user_derive_macro_surface");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::aurex_compile_time_macro_execution_admission),
        "aurex_compile_time_macro_execution_admission");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::aurex_macro_typed_matcher_admission),
        "aurex_macro_typed_matcher_admission");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::aurex_macro_definition_site_hygiene_admission),
        "aurex_macro_definition_site_hygiene_admission");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::aurex_macro_debuggable_diagnostic_anchor),
        "aurex_macro_debuggable_diagnostic_anchor");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::aurex_macro_call_site_admission),
        "aurex_macro_call_site_admission");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::aurex_macro_matcher_to_call_binding_admission),
        "aurex_macro_matcher_to_call_binding_admission");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::aurex_user_derive_target_schema_admission),
        "aurex_user_derive_target_schema_admission");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::aurex_macro_output_contract_admission),
        "aurex_macro_output_contract_admission");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::aurex_macro_output_declared_name_policy_admission),
        "aurex_macro_output_declared_name_policy_admission");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  query::MacroExpansionFactKind::aurex_macro_output_diagnostic_projection_admission),
        "aurex_macro_output_diagnostic_projection_admission");
    EXPECT_EQ(query::macro_expansion_fact_kind_name(
                  static_cast<query::MacroExpansionFactKind>(QUERY_TEST_INVALID_MACRO_EXPANSION_KIND)),
        "invalid");

    EXPECT_EQ(query::macro_expansion_stage_name(query::MacroExpansionStage::parsed_attribute_surface),
        "parsed_attribute_surface");
    EXPECT_EQ(query::macro_expansion_stage_name(query::MacroExpansionStage::early_item_expansion),
        "early_item_expansion");
    EXPECT_EQ(query::macro_expansion_stage_name(query::MacroExpansionStage::generated_part_planning),
        "generated_part_planning");
    EXPECT_EQ(query::macro_expansion_stage_name(query::MacroExpansionStage::sema_blocker),
        "sema_blocker");
    EXPECT_EQ(query::macro_expansion_stage_name(query::MacroExpansionStage::future_stage),
        "future_stage");
    EXPECT_EQ(query::macro_expansion_stage_name(
                  static_cast<query::MacroExpansionStage>(QUERY_TEST_INVALID_MACRO_EXPANSION_STAGE)),
        "invalid");

    EXPECT_EQ(query::macro_expansion_policy_name(query::MacroExpansionPolicy::attribute_token_tree_v1),
        "attribute_token_tree_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(query::MacroExpansionPolicy::builtin_derive_passthrough_v1),
        "builtin_derive_passthrough_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(query::MacroExpansionPolicy::expansion_query_fingerprint_v1),
        "expansion_query_fingerprint_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(query::MacroExpansionPolicy::generated_module_part_noop_v1),
        "generated_module_part_noop_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(query::MacroExpansionPolicy::source_map_trace_stub_v1),
        "source_map_trace_stub_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(
                  query::MacroExpansionPolicy::unimplemented_item_attribute_blocker_v1),
        "unimplemented_item_attribute_blocker_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(
                  query::MacroExpansionPolicy::external_proc_macro_sandbox_future_v1),
        "external_proc_macro_sandbox_future_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(
                  query::MacroExpansionPolicy::aurex_declarative_macro_surface_v1),
        "aurex_declarative_macro_surface_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(
                  query::MacroExpansionPolicy::aurex_user_derive_macro_surface_v1),
        "aurex_user_derive_macro_surface_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(
                  query::MacroExpansionPolicy::aurex_compile_time_macro_execution_admission_v1),
        "aurex_compile_time_macro_execution_admission_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(
                  query::MacroExpansionPolicy::aurex_macro_typed_matcher_admission_v1),
        "aurex_macro_typed_matcher_admission_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(
                  query::MacroExpansionPolicy::aurex_macro_definition_site_hygiene_admission_v1),
        "aurex_macro_definition_site_hygiene_admission_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(
                  query::MacroExpansionPolicy::aurex_macro_debuggable_diagnostic_anchor_v1),
        "aurex_macro_debuggable_diagnostic_anchor_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(
                  query::MacroExpansionPolicy::aurex_macro_output_contract_admission_v1),
        "aurex_macro_output_contract_admission_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(
                  query::MacroExpansionPolicy::aurex_macro_output_declared_name_policy_admission_v1),
        "aurex_macro_output_declared_name_policy_admission_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(
                  query::MacroExpansionPolicy::aurex_macro_output_diagnostic_projection_admission_v1),
        "aurex_macro_output_diagnostic_projection_admission_v1");
    EXPECT_EQ(query::macro_expansion_policy_name(
                  static_cast<query::MacroExpansionPolicy>(QUERY_TEST_INVALID_MACRO_EXPANSION_POLICY)),
        "invalid");

    EXPECT_FALSE(query::is_valid(static_cast<query::MacroExpansionFactKind>(
        QUERY_TEST_INVALID_MACRO_EXPANSION_KIND)));
    EXPECT_FALSE(query::is_valid(static_cast<query::MacroExpansionStage>(
        QUERY_TEST_INVALID_MACRO_EXPANSION_STAGE)));
    EXPECT_FALSE(query::is_valid(static_cast<query::MacroExpansionPolicy>(
        QUERY_TEST_INVALID_MACRO_EXPANSION_POLICY)));
}

TEST(QueryUnit, MacroExpansionPlanM21cPinsEarlyItemExpansionNoopPipeline)
{
    const query::MacroExpansionPlan plan = query::m21c_macro_expansion_plan_baseline();

    ASSERT_EQ(plan.name, "M21c Early Item Macro Expansion Plan");
    ASSERT_EQ(plan.facts.size(), QUERY_TEST_M21C_MACRO_EXPANSION_FACT_COUNT);
    EXPECT_TRUE(query::is_valid(plan));
    EXPECT_TRUE(query::is_valid_m21c_macro_expansion_plan(plan));
    EXPECT_EQ(plan.fingerprint, query::macro_expansion_plan_fingerprint(plan));

    const query::MacroExpansionFact* attribute_input =
        find_fact(plan, query::MacroExpansionFactKind::attribute_token_tree_input);
    ASSERT_NE(attribute_input, nullptr);
    EXPECT_TRUE(attribute_input->consumes_attribute_decl);
    EXPECT_TRUE(attribute_input->consumes_attribute_token_tree);
    EXPECT_FALSE(attribute_input->requires_query_key);
    EXPECT_EQ(attribute_input->input_fact, "ItemNode::attributes");

    const query::MacroExpansionFact* derive =
        find_fact(plan, query::MacroExpansionFactKind::builtin_derive_passthrough);
    ASSERT_NE(derive, nullptr);
    EXPECT_TRUE(derive->preserves_builtin_derive);
    EXPECT_TRUE(derive->requires_query_key);
    EXPECT_FALSE(derive->blocks_unimplemented_item_attribute);

    const query::MacroExpansionFact* generated =
        find_fact(plan, query::MacroExpansionFactKind::generated_module_part_noop);
    ASSERT_NE(generated, nullptr);
    EXPECT_TRUE(generated->requires_generated_module_part);
    EXPECT_TRUE(generated->uses_generated_source_role);
    EXPECT_TRUE(generated->uses_generated_module_part_kind);
    EXPECT_EQ(generated->generated_source_role, query::SourceRole::generated);
    EXPECT_EQ(generated->generated_part_kind, query::ModulePartKind::generated);
    EXPECT_FALSE(generated->produces_user_generated_code);

    const query::MacroExpansionFact* blocker =
        find_fact(plan, query::MacroExpansionFactKind::unimplemented_item_attribute_blocker);
    ASSERT_NE(blocker, nullptr);
    EXPECT_EQ(blocker->stage, query::MacroExpansionStage::sema_blocker);
    EXPECT_TRUE(blocker->blocks_unimplemented_item_attribute);
    EXPECT_EQ(query::m21c_item_attribute_macro_unimplemented_message("builder"),
        "item attribute macros are parsed but macro expansion is not implemented yet: builder");
}

TEST(QueryUnit, MacroExpansionPlanM21cSummaryAndDumpAreStable)
{
    const query::MacroExpansionPlan plan = query::m21c_macro_expansion_plan_baseline();
    const query::MacroExpansionSummary summary = query::summarize_macro_expansion_plan_counts(plan);

    EXPECT_EQ(summary.fact_count, 7U);
    EXPECT_EQ(summary.attribute_input_count, 1U);
    EXPECT_EQ(summary.builtin_derive_passthrough_count, 1U);
    EXPECT_EQ(summary.query_key_count, 1U);
    EXPECT_EQ(summary.generated_part_count, 1U);
    EXPECT_EQ(summary.source_map_stub_count, 1U);
    EXPECT_EQ(summary.sema_blocker_count, 1U);
    EXPECT_EQ(summary.future_external_count, 1U);
    EXPECT_EQ(summary.user_generated_code_count, 0U);
    EXPECT_EQ(summary.standard_library_required_count, 0U);
    EXPECT_EQ(summary.runtime_required_count, 0U);
    EXPECT_EQ(summary.external_process_required_count, 1U);
    EXPECT_EQ(summary.unimplemented_item_attribute_blocker_count, 2U);

    query::MacroExpansionPlan changed = plan;
    ASSERT_FALSE(changed.facts.empty());
    changed.facts.front().output_fact = "changed output fact";
    changed.summary = query::summarize_macro_expansion_plan_counts(changed);
    changed.fingerprint = query::macro_expansion_plan_fingerprint(changed);
    EXPECT_NE(plan.fingerprint, changed.fingerprint);

    const std::string summary_text = query::summarize_macro_expansion_plan(plan);
    EXPECT_NE(summary_text.find("macro_expansion_plan name=M21c Early Item Macro Expansion Plan"),
        std::string::npos)
        << summary_text;
    EXPECT_NE(summary_text.find("facts=7"), std::string::npos) << summary_text;
    EXPECT_NE(summary_text.find("generated_parts=1"), std::string::npos) << summary_text;
    EXPECT_NE(summary_text.find("user_generated_code=0"), std::string::npos) << summary_text;
    EXPECT_NE(summary_text.find("standard_library_required=0"), std::string::npos) << summary_text;

    const std::string dump = query::dump_macro_expansion_plan(plan);
    EXPECT_NE(dump.find("kind=generated_module_part_noop"), std::string::npos) << dump;
    EXPECT_NE(dump.find("policy=expansion_query_fingerprint_v1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("output_fact=macro expansion query key fingerprint"), std::string::npos) << dump;
    EXPECT_NE(dump.find("blocker_fact=macro expansion output missing"), std::string::npos) << dump;
    EXPECT_NE(dump.find("external procedural macros are not executed in M21c"), std::string::npos) << dump;
}

TEST(QueryUnit, MacroExpansionPlanM21cValidationRejectsBoundaryDrift)
{
    const query::MacroExpansionPlan plan = query::m21c_macro_expansion_plan_baseline();
    ASSERT_TRUE(query::is_valid_m21c_macro_expansion_plan(plan));

    query::MacroExpansionPlan wrong_name = plan;
    wrong_name.name = "wrong macro expansion plan";
    wrong_name.fingerprint = query::macro_expansion_plan_fingerprint(wrong_name);
    EXPECT_FALSE(query::is_valid(wrong_name));

    query::MacroExpansionPlan generated_code = plan;
    query::MacroExpansionFact* const generated =
        find_fact(generated_code, query::MacroExpansionFactKind::generated_module_part_noop);
    ASSERT_NE(generated, nullptr);
    generated->produces_user_generated_code = true;
    generated_code.summary = query::summarize_macro_expansion_plan_counts(generated_code);
    generated_code.fingerprint = query::macro_expansion_plan_fingerprint(generated_code);
    EXPECT_FALSE(query::is_valid_m21c_macro_expansion_plan(generated_code));

    query::MacroExpansionPlan hidden_stdlib = plan;
    query::MacroExpansionFact* const blocker =
        find_fact(hidden_stdlib, query::MacroExpansionFactKind::unimplemented_item_attribute_blocker);
    ASSERT_NE(blocker, nullptr);
    blocker->standard_library_required = true;
    hidden_stdlib.summary = query::summarize_macro_expansion_plan_counts(hidden_stdlib);
    hidden_stdlib.fingerprint = query::macro_expansion_plan_fingerprint(hidden_stdlib);
    EXPECT_FALSE(query::is_valid_m21c_macro_expansion_plan(hidden_stdlib));

    query::MacroExpansionPlan wrong_generated_identity = plan;
    query::MacroExpansionFact* const generated_identity =
        find_fact(wrong_generated_identity, query::MacroExpansionFactKind::generated_module_part_noop);
    ASSERT_NE(generated_identity, nullptr);
    generated_identity->generated_part_kind = query::ModulePartKind::primary;
    wrong_generated_identity.summary = query::summarize_macro_expansion_plan_counts(wrong_generated_identity);
    wrong_generated_identity.fingerprint = query::macro_expansion_plan_fingerprint(wrong_generated_identity);
    EXPECT_FALSE(query::is_valid_m21c_macro_expansion_plan(wrong_generated_identity));

    query::MacroExpansionPlan stale_summary = plan;
    stale_summary.summary.fact_count += 1U;
    stale_summary.fingerprint = query::macro_expansion_plan_fingerprint(stale_summary);
    EXPECT_FALSE(query::is_valid_m21c_macro_expansion_plan(stale_summary));

    query::MacroExpansionPlan duplicate_kind = plan;
    query::MacroExpansionFact* const external =
        find_fact(duplicate_kind, query::MacroExpansionFactKind::external_procedural_macro_blocked);
    ASSERT_NE(external, nullptr);
    external->kind = query::MacroExpansionFactKind::generated_module_part_noop;
    duplicate_kind.summary = query::summarize_macro_expansion_plan_counts(duplicate_kind);
    duplicate_kind.fingerprint = query::macro_expansion_plan_fingerprint(duplicate_kind);
    EXPECT_FALSE(query::is_valid_m21c_macro_expansion_plan(duplicate_kind));
}

TEST(QueryUnit, MacroExpansionPlanM27PinsAurexMacroSurfaceAdmission)
{
    const query::MacroExpansionPlan plan = query::m27_macro_expansion_plan_baseline();

    ASSERT_EQ(plan.name, "M27 Aurex Macro Surface Admission Plan");
    ASSERT_EQ(plan.facts.size(), QUERY_TEST_M27_MACRO_EXPANSION_FACT_COUNT);
    EXPECT_FALSE(query::is_valid(plan));
    EXPECT_FALSE(query::is_valid_m21c_macro_expansion_plan(plan));
    EXPECT_TRUE(query::is_valid_m27_macro_expansion_plan(plan));
    EXPECT_EQ(plan.fingerprint, query::macro_expansion_plan_fingerprint(plan));

    const query::MacroExpansionSummary summary = query::summarize_macro_expansion_plan_counts(plan);
    EXPECT_EQ(summary.fact_count, QUERY_TEST_M27_MACRO_EXPANSION_FACT_COUNT);
    EXPECT_EQ(summary.attribute_input_count, 1U);
    EXPECT_EQ(summary.builtin_derive_passthrough_count, 1U);
    EXPECT_EQ(summary.generated_part_count, 1U);
    EXPECT_EQ(summary.aurex_declarative_macro_surface_count, 1U);
    EXPECT_EQ(summary.aurex_user_derive_macro_surface_count, 1U);
    EXPECT_EQ(summary.aurex_compile_time_macro_execution_admission_count, 1U);
    EXPECT_EQ(summary.user_generated_code_count, 0U);
    EXPECT_EQ(summary.standard_library_required_count, 0U);
    EXPECT_EQ(summary.runtime_required_count, 0U);
    EXPECT_EQ(summary.external_process_required_count, 1U);

    const std::array<query::MacroExpansionFactKind, 3U> aurex_surface_kinds{
        query::MacroExpansionFactKind::aurex_declarative_macro_surface,
        query::MacroExpansionFactKind::aurex_user_derive_macro_surface,
        query::MacroExpansionFactKind::aurex_compile_time_macro_execution_admission,
    };
    for (const query::MacroExpansionFactKind kind : aurex_surface_kinds) {
        const query::MacroExpansionFact* const fact = find_fact(plan, kind);
        ASSERT_NE(fact, nullptr);
        EXPECT_FALSE(fact->consumes_attribute_decl);
        EXPECT_TRUE(fact->consumes_attribute_token_tree);
        EXPECT_TRUE(fact->requires_query_key);
        EXPECT_FALSE(fact->requires_generated_module_part);
        EXPECT_TRUE(fact->requires_source_map);
        EXPECT_TRUE(fact->requires_hygiene);
        EXPECT_FALSE(fact->produces_user_generated_code);
        EXPECT_FALSE(fact->standard_library_required);
        EXPECT_FALSE(fact->runtime_required);
        EXPECT_FALSE(fact->external_process_required);
        EXPECT_TRUE(fact->blocks_unimplemented_item_attribute);
    }

    const std::string summary_text = query::summarize_macro_expansion_plan(plan);
    EXPECT_NE(summary_text.find("macro_expansion_plan name=M27 Aurex Macro Surface Admission Plan"),
        std::string::npos)
        << summary_text;
    EXPECT_NE(summary_text.find("facts=10"), std::string::npos) << summary_text;
    EXPECT_NE(summary_text.find("aurex_declarative_macro_surfaces=1"), std::string::npos)
        << summary_text;
    EXPECT_NE(summary_text.find("aurex_user_derive_macro_surfaces=1"), std::string::npos)
        << summary_text;
    EXPECT_NE(summary_text.find("aurex_compile_time_macro_execution_admissions=1"),
        std::string::npos)
        << summary_text;

    const std::string dump = query::dump_macro_expansion_plan(plan);
    EXPECT_NE(dump.find("kind=aurex_declarative_macro_surface"), std::string::npos) << dump;
    EXPECT_NE(dump.find("kind=aurex_user_derive_macro_surface"), std::string::npos) << dump;
    EXPECT_NE(dump.find("kind=aurex_compile_time_macro_execution_admission"), std::string::npos)
        << dump;
    EXPECT_NE(dump.find("output_fact=AurexMacroSurfaceAdmissionGate declarative surface"),
        std::string::npos)
        << dump;
}

TEST(QueryUnit, MacroExpansionPlanM27ValidationRejectsSurfaceBoundaryDrift)
{
    const query::MacroExpansionPlan plan = query::m27_macro_expansion_plan_baseline();
    ASSERT_TRUE(query::is_valid_m27_macro_expansion_plan(plan));

    query::MacroExpansionPlan missing_surface = plan;
    missing_surface.facts.pop_back();
    missing_surface.summary = query::summarize_macro_expansion_plan_counts(missing_surface);
    missing_surface.fingerprint = query::macro_expansion_plan_fingerprint(missing_surface);
    EXPECT_FALSE(query::is_valid_m27_macro_expansion_plan(missing_surface));

    query::MacroExpansionPlan external_surface = plan;
    query::MacroExpansionFact* const compile_time =
        find_fact(external_surface, query::MacroExpansionFactKind::aurex_compile_time_macro_execution_admission);
    ASSERT_NE(compile_time, nullptr);
    compile_time->external_process_required = true;
    external_surface.summary = query::summarize_macro_expansion_plan_counts(external_surface);
    external_surface.fingerprint = query::macro_expansion_plan_fingerprint(external_surface);
    EXPECT_FALSE(query::is_valid_m27_macro_expansion_plan(external_surface));

    query::MacroExpansionPlan generated_user_code = plan;
    query::MacroExpansionFact* const declarative =
        find_fact(generated_user_code, query::MacroExpansionFactKind::aurex_declarative_macro_surface);
    ASSERT_NE(declarative, nullptr);
    declarative->produces_user_generated_code = true;
    generated_user_code.summary = query::summarize_macro_expansion_plan_counts(generated_user_code);
    generated_user_code.fingerprint = query::macro_expansion_plan_fingerprint(generated_user_code);
    EXPECT_FALSE(query::is_valid_m27_macro_expansion_plan(generated_user_code));

    query::MacroExpansionPlan duplicate_kind = plan;
    query::MacroExpansionFact* const user_derive =
        find_fact(duplicate_kind, query::MacroExpansionFactKind::aurex_user_derive_macro_surface);
    ASSERT_NE(user_derive, nullptr);
    user_derive->kind = query::MacroExpansionFactKind::aurex_declarative_macro_surface;
    duplicate_kind.summary = query::summarize_macro_expansion_plan_counts(duplicate_kind);
    duplicate_kind.fingerprint = query::macro_expansion_plan_fingerprint(duplicate_kind);
    EXPECT_FALSE(query::is_valid_m27_macro_expansion_plan(duplicate_kind));
}

TEST(QueryUnit, MacroExpansionPlanM27bPinsTypedMatcherAndDefinitionSiteHygieneAdmission)
{
    const query::MacroExpansionPlan plan = query::m27b_macro_expansion_plan_baseline();

    ASSERT_EQ(plan.name, "M27b Aurex Typed Matcher And Definition-Site Hygiene Admission Plan");
    ASSERT_EQ(plan.facts.size(), QUERY_TEST_M27B_MACRO_EXPANSION_FACT_COUNT);
    EXPECT_FALSE(query::is_valid(plan));
    EXPECT_FALSE(query::is_valid_m21c_macro_expansion_plan(plan));
    EXPECT_FALSE(query::is_valid_m27_macro_expansion_plan(plan));
    EXPECT_TRUE(query::is_valid_m27b_macro_expansion_plan(plan));
    EXPECT_EQ(plan.fingerprint, query::macro_expansion_plan_fingerprint(plan));

    const query::MacroExpansionSummary summary = query::summarize_macro_expansion_plan_counts(plan);
    EXPECT_EQ(summary.fact_count, QUERY_TEST_M27B_MACRO_EXPANSION_FACT_COUNT);
    EXPECT_EQ(summary.aurex_declarative_macro_surface_count, 1U);
    EXPECT_EQ(summary.aurex_user_derive_macro_surface_count, 1U);
    EXPECT_EQ(summary.aurex_compile_time_macro_execution_admission_count, 1U);
    EXPECT_EQ(summary.aurex_macro_typed_matcher_admission_count, 1U);
    EXPECT_EQ(summary.aurex_macro_definition_site_hygiene_admission_count, 1U);
    EXPECT_EQ(summary.aurex_macro_debuggable_diagnostic_anchor_count, 1U);
    EXPECT_EQ(summary.user_generated_code_count, 0U);
    EXPECT_EQ(summary.standard_library_required_count, 0U);
    EXPECT_EQ(summary.runtime_required_count, 0U);
    EXPECT_EQ(summary.external_process_required_count, 1U);

    const std::array<query::MacroExpansionFactKind, 3U> m27b_kinds{
        query::MacroExpansionFactKind::aurex_macro_typed_matcher_admission,
        query::MacroExpansionFactKind::aurex_macro_definition_site_hygiene_admission,
        query::MacroExpansionFactKind::aurex_macro_debuggable_diagnostic_anchor,
    };
    for (const query::MacroExpansionFactKind kind : m27b_kinds) {
        const query::MacroExpansionFact* const fact = find_fact(plan, kind);
        ASSERT_NE(fact, nullptr);
        EXPECT_TRUE(fact->consumes_attribute_token_tree);
        EXPECT_TRUE(fact->requires_query_key);
        EXPECT_TRUE(fact->requires_source_map);
        EXPECT_TRUE(fact->requires_hygiene);
        EXPECT_FALSE(fact->produces_user_generated_code);
        EXPECT_FALSE(fact->standard_library_required);
        EXPECT_FALSE(fact->runtime_required);
        EXPECT_FALSE(fact->external_process_required);
        EXPECT_TRUE(fact->blocks_unimplemented_item_attribute);
    }

    const std::string summary_text = query::summarize_macro_expansion_plan(plan);
    EXPECT_NE(summary_text.find("facts=13"), std::string::npos) << summary_text;
    EXPECT_NE(summary_text.find("aurex_macro_typed_matcher_admissions=1"),
        std::string::npos)
        << summary_text;
    EXPECT_NE(summary_text.find("aurex_macro_definition_site_hygiene_admissions=1"),
        std::string::npos)
        << summary_text;
    EXPECT_NE(summary_text.find("aurex_macro_debuggable_diagnostic_anchors=1"),
        std::string::npos)
        << summary_text;

    const std::string dump = query::dump_macro_expansion_plan(plan);
    EXPECT_NE(dump.find("kind=aurex_macro_typed_matcher_admission"), std::string::npos)
        << dump;
    EXPECT_NE(dump.find("kind=aurex_macro_definition_site_hygiene_admission"),
        std::string::npos)
        << dump;
    EXPECT_NE(dump.find("kind=aurex_macro_debuggable_diagnostic_anchor"), std::string::npos)
        << dump;
}

TEST(QueryUnit, MacroExpansionPlanM27bValidationRejectsMatcherBoundaryDrift)
{
    const query::MacroExpansionPlan plan = query::m27b_macro_expansion_plan_baseline();
    ASSERT_TRUE(query::is_valid_m27b_macro_expansion_plan(plan));

    query::MacroExpansionPlan missing_matcher = plan;
    missing_matcher.facts.pop_back();
    missing_matcher.summary = query::summarize_macro_expansion_plan_counts(missing_matcher);
    missing_matcher.fingerprint = query::macro_expansion_plan_fingerprint(missing_matcher);
    EXPECT_FALSE(query::is_valid_m27b_macro_expansion_plan(missing_matcher));

    query::MacroExpansionPlan executable_matcher = plan;
    query::MacroExpansionFact* const matcher =
        find_fact(executable_matcher, query::MacroExpansionFactKind::aurex_macro_typed_matcher_admission);
    ASSERT_NE(matcher, nullptr);
    matcher->produces_user_generated_code = true;
    executable_matcher.summary = query::summarize_macro_expansion_plan_counts(executable_matcher);
    executable_matcher.fingerprint = query::macro_expansion_plan_fingerprint(executable_matcher);
    EXPECT_FALSE(query::is_valid_m27b_macro_expansion_plan(executable_matcher));

    query::MacroExpansionPlan runtime_hygiene = plan;
    query::MacroExpansionFact* const hygiene =
        find_fact(runtime_hygiene,
            query::MacroExpansionFactKind::aurex_macro_definition_site_hygiene_admission);
    ASSERT_NE(hygiene, nullptr);
    hygiene->runtime_required = true;
    runtime_hygiene.summary = query::summarize_macro_expansion_plan_counts(runtime_hygiene);
    runtime_hygiene.fingerprint = query::macro_expansion_plan_fingerprint(runtime_hygiene);
    EXPECT_FALSE(query::is_valid_m27b_macro_expansion_plan(runtime_hygiene));

    query::MacroExpansionPlan duplicate_kind = plan;
    query::MacroExpansionFact* const anchor =
        find_fact(duplicate_kind, query::MacroExpansionFactKind::aurex_macro_debuggable_diagnostic_anchor);
    ASSERT_NE(anchor, nullptr);
    anchor->kind = query::MacroExpansionFactKind::aurex_macro_typed_matcher_admission;
    duplicate_kind.summary = query::summarize_macro_expansion_plan_counts(duplicate_kind);
    duplicate_kind.fingerprint = query::macro_expansion_plan_fingerprint(duplicate_kind);
    EXPECT_FALSE(query::is_valid_m27b_macro_expansion_plan(duplicate_kind));
}

TEST(QueryUnit, MacroExpansionPlanM27cPinsCallSiteAndUserDeriveSchemaAdmission)
{
    const query::MacroExpansionPlan plan = query::m27c_macro_expansion_plan_baseline();

    ASSERT_EQ(plan.name,
        "M27c Aurex Macro Call-Site And User Derive Target Schema Admission Plan");
    ASSERT_EQ(plan.facts.size(), QUERY_TEST_M27C_MACRO_EXPANSION_FACT_COUNT);
    EXPECT_FALSE(query::is_valid(plan));
    EXPECT_FALSE(query::is_valid_m21c_macro_expansion_plan(plan));
    EXPECT_FALSE(query::is_valid_m27_macro_expansion_plan(plan));
    EXPECT_FALSE(query::is_valid_m27b_macro_expansion_plan(plan));
    EXPECT_TRUE(query::is_valid_m27c_macro_expansion_plan(plan));
    EXPECT_EQ(plan.fingerprint, query::macro_expansion_plan_fingerprint(plan));

    const query::MacroExpansionSummary summary = query::summarize_macro_expansion_plan_counts(plan);
    EXPECT_EQ(summary.fact_count, QUERY_TEST_M27C_MACRO_EXPANSION_FACT_COUNT);
    EXPECT_EQ(summary.aurex_macro_call_site_admission_count, 1U);
    EXPECT_EQ(summary.aurex_macro_matcher_to_call_binding_admission_count, 1U);
    EXPECT_EQ(summary.aurex_user_derive_target_schema_admission_count, 1U);
    EXPECT_EQ(summary.user_generated_code_count, 0U);
    EXPECT_EQ(summary.standard_library_required_count, 0U);
    EXPECT_EQ(summary.runtime_required_count, 0U);
    EXPECT_EQ(summary.external_process_required_count, 1U);

    const std::array<query::MacroExpansionFactKind, 3U> m27c_kinds{
        query::MacroExpansionFactKind::aurex_macro_call_site_admission,
        query::MacroExpansionFactKind::aurex_macro_matcher_to_call_binding_admission,
        query::MacroExpansionFactKind::aurex_user_derive_target_schema_admission,
    };
    for (const query::MacroExpansionFactKind kind : m27c_kinds) {
        const query::MacroExpansionFact* const fact = find_fact(plan, kind);
        ASSERT_NE(fact, nullptr);
        EXPECT_TRUE(fact->consumes_attribute_token_tree);
        EXPECT_TRUE(fact->requires_query_key);
        EXPECT_TRUE(fact->requires_source_map);
        EXPECT_TRUE(fact->requires_hygiene);
        EXPECT_FALSE(fact->requires_generated_module_part);
        EXPECT_FALSE(fact->produces_user_generated_code);
        EXPECT_FALSE(fact->standard_library_required);
        EXPECT_FALSE(fact->runtime_required);
        EXPECT_FALSE(fact->external_process_required);
        EXPECT_TRUE(fact->blocks_unimplemented_item_attribute);
    }

    const std::string summary_text = query::summarize_macro_expansion_plan(plan);
    EXPECT_NE(summary_text.find("facts=16"), std::string::npos) << summary_text;
    EXPECT_NE(summary_text.find("aurex_macro_call_site_admissions=1"), std::string::npos)
        << summary_text;
    EXPECT_NE(summary_text.find("aurex_macro_matcher_to_call_bindings=1"),
        std::string::npos)
        << summary_text;
    EXPECT_NE(summary_text.find("aurex_user_derive_target_schemas=1"), std::string::npos)
        << summary_text;

    const std::string dump = query::dump_macro_expansion_plan(plan);
    EXPECT_NE(dump.find("kind=aurex_macro_call_site_admission"), std::string::npos)
        << dump;
    EXPECT_NE(dump.find("kind=aurex_macro_matcher_to_call_binding_admission"),
        std::string::npos)
        << dump;
    EXPECT_NE(dump.find("kind=aurex_user_derive_target_schema_admission"),
        std::string::npos)
        << dump;
}

TEST(QueryUnit, MacroExpansionPlanM27cValidationRejectsCallSiteAdmissionDrift)
{
    const query::MacroExpansionPlan plan = query::m27c_macro_expansion_plan_baseline();
    ASSERT_TRUE(query::is_valid_m27c_macro_expansion_plan(plan));

    query::MacroExpansionPlan missing_schema = plan;
    missing_schema.facts.pop_back();
    missing_schema.summary = query::summarize_macro_expansion_plan_counts(missing_schema);
    missing_schema.fingerprint = query::macro_expansion_plan_fingerprint(missing_schema);
    EXPECT_FALSE(query::is_valid_m27c_macro_expansion_plan(missing_schema));

    query::MacroExpansionPlan executable_call = plan;
    query::MacroExpansionFact* const call_site =
        find_fact(executable_call, query::MacroExpansionFactKind::aurex_macro_call_site_admission);
    ASSERT_NE(call_site, nullptr);
    call_site->produces_user_generated_code = true;
    executable_call.summary = query::summarize_macro_expansion_plan_counts(executable_call);
    executable_call.fingerprint = query::macro_expansion_plan_fingerprint(executable_call);
    EXPECT_FALSE(query::is_valid_m27c_macro_expansion_plan(executable_call));

    query::MacroExpansionPlan runtime_schema = plan;
    query::MacroExpansionFact* const schema =
        find_fact(runtime_schema, query::MacroExpansionFactKind::aurex_user_derive_target_schema_admission);
    ASSERT_NE(schema, nullptr);
    schema->runtime_required = true;
    runtime_schema.summary = query::summarize_macro_expansion_plan_counts(runtime_schema);
    runtime_schema.fingerprint = query::macro_expansion_plan_fingerprint(runtime_schema);
    EXPECT_FALSE(query::is_valid_m27c_macro_expansion_plan(runtime_schema));

    query::MacroExpansionPlan duplicate_kind = plan;
    query::MacroExpansionFact* const binding =
        find_fact(duplicate_kind,
            query::MacroExpansionFactKind::aurex_macro_matcher_to_call_binding_admission);
    ASSERT_NE(binding, nullptr);
    binding->kind = query::MacroExpansionFactKind::aurex_macro_call_site_admission;
    duplicate_kind.summary = query::summarize_macro_expansion_plan_counts(duplicate_kind);
    duplicate_kind.fingerprint = query::macro_expansion_plan_fingerprint(duplicate_kind);
    EXPECT_FALSE(query::is_valid_m27c_macro_expansion_plan(duplicate_kind));
}

TEST(QueryUnit, MacroExpansionPlanM27dPinsOutputContractAdmission)
{
    const query::MacroExpansionPlan plan = query::m27d_macro_expansion_plan_baseline();

    ASSERT_EQ(plan.name, "M27d Aurex Macro Output Contract Admission Plan");
    ASSERT_EQ(plan.facts.size(), QUERY_TEST_M27D_MACRO_EXPANSION_FACT_COUNT);
    EXPECT_FALSE(query::is_valid(plan));
    EXPECT_FALSE(query::is_valid_m21c_macro_expansion_plan(plan));
    EXPECT_FALSE(query::is_valid_m27_macro_expansion_plan(plan));
    EXPECT_FALSE(query::is_valid_m27b_macro_expansion_plan(plan));
    EXPECT_FALSE(query::is_valid_m27c_macro_expansion_plan(plan));
    EXPECT_TRUE(query::is_valid_m27d_macro_expansion_plan(plan));
    EXPECT_EQ(plan.fingerprint, query::macro_expansion_plan_fingerprint(plan));

    const query::MacroExpansionSummary summary = query::summarize_macro_expansion_plan_counts(plan);
    EXPECT_EQ(summary.fact_count, QUERY_TEST_M27D_MACRO_EXPANSION_FACT_COUNT);
    EXPECT_EQ(summary.aurex_macro_call_site_admission_count, 1U);
    EXPECT_EQ(summary.aurex_macro_matcher_to_call_binding_admission_count, 1U);
    EXPECT_EQ(summary.aurex_user_derive_target_schema_admission_count, 1U);
    EXPECT_EQ(summary.aurex_macro_output_contract_admission_count, 1U);
    EXPECT_EQ(summary.aurex_macro_output_declared_name_policy_admission_count, 1U);
    EXPECT_EQ(summary.aurex_macro_output_diagnostic_projection_admission_count, 1U);
    EXPECT_EQ(summary.user_generated_code_count, 0U);
    EXPECT_EQ(summary.standard_library_required_count, 0U);
    EXPECT_EQ(summary.runtime_required_count, 0U);
    EXPECT_EQ(summary.external_process_required_count, 1U);

    const std::array<query::MacroExpansionFactKind, 3U> m27d_kinds{
        query::MacroExpansionFactKind::aurex_macro_output_contract_admission,
        query::MacroExpansionFactKind::aurex_macro_output_declared_name_policy_admission,
        query::MacroExpansionFactKind::aurex_macro_output_diagnostic_projection_admission,
    };
    for (const query::MacroExpansionFactKind kind : m27d_kinds) {
        const query::MacroExpansionFact* const fact = find_fact(plan, kind);
        ASSERT_NE(fact, nullptr);
        EXPECT_TRUE(fact->consumes_attribute_token_tree);
        EXPECT_TRUE(fact->requires_query_key);
        EXPECT_TRUE(fact->requires_generated_module_part);
        EXPECT_TRUE(fact->uses_generated_source_role);
        EXPECT_TRUE(fact->uses_generated_module_part_kind);
        EXPECT_TRUE(fact->requires_source_map);
        EXPECT_TRUE(fact->requires_hygiene);
        EXPECT_FALSE(fact->produces_user_generated_code);
        EXPECT_FALSE(fact->standard_library_required);
        EXPECT_FALSE(fact->runtime_required);
        EXPECT_FALSE(fact->external_process_required);
        EXPECT_TRUE(fact->blocks_unimplemented_item_attribute);
    }

    const std::string summary_text = query::summarize_macro_expansion_plan(plan);
    EXPECT_NE(summary_text.find("facts=19"), std::string::npos) << summary_text;
    EXPECT_NE(summary_text.find("aurex_macro_output_contracts=1"), std::string::npos)
        << summary_text;
    EXPECT_NE(summary_text.find("aurex_macro_output_declared_name_policies=1"),
        std::string::npos)
        << summary_text;
    EXPECT_NE(summary_text.find("aurex_macro_output_diagnostic_projections=1"),
        std::string::npos)
        << summary_text;

    const std::string dump = query::dump_macro_expansion_plan(plan);
    EXPECT_NE(dump.find("kind=aurex_macro_output_contract_admission"), std::string::npos)
        << dump;
    EXPECT_NE(dump.find("kind=aurex_macro_output_declared_name_policy_admission"),
        std::string::npos)
        << dump;
    EXPECT_NE(dump.find("kind=aurex_macro_output_diagnostic_projection_admission"),
        std::string::npos)
        << dump;
}

TEST(QueryUnit, MacroExpansionPlanM27dValidationRejectsOutputContractDrift)
{
    const query::MacroExpansionPlan plan = query::m27d_macro_expansion_plan_baseline();
    ASSERT_TRUE(query::is_valid_m27d_macro_expansion_plan(plan));

    query::MacroExpansionPlan missing_output_fact = plan;
    missing_output_fact.facts.pop_back();
    missing_output_fact.summary = query::summarize_macro_expansion_plan_counts(missing_output_fact);
    missing_output_fact.fingerprint = query::macro_expansion_plan_fingerprint(missing_output_fact);
    EXPECT_FALSE(query::is_valid_m27d_macro_expansion_plan(missing_output_fact));

    query::MacroExpansionPlan generated_user_code = plan;
    query::MacroExpansionFact* const output_contract =
        find_fact(generated_user_code,
            query::MacroExpansionFactKind::aurex_macro_output_contract_admission);
    ASSERT_NE(output_contract, nullptr);
    output_contract->produces_user_generated_code = true;
    generated_user_code.summary = query::summarize_macro_expansion_plan_counts(generated_user_code);
    generated_user_code.fingerprint = query::macro_expansion_plan_fingerprint(generated_user_code);
    EXPECT_FALSE(query::is_valid_m27d_macro_expansion_plan(generated_user_code));

    query::MacroExpansionPlan runtime_declared_names = plan;
    query::MacroExpansionFact* const declared_name_policy =
        find_fact(runtime_declared_names,
            query::MacroExpansionFactKind::aurex_macro_output_declared_name_policy_admission);
    ASSERT_NE(declared_name_policy, nullptr);
    declared_name_policy->runtime_required = true;
    runtime_declared_names.summary =
        query::summarize_macro_expansion_plan_counts(runtime_declared_names);
    runtime_declared_names.fingerprint =
        query::macro_expansion_plan_fingerprint(runtime_declared_names);
    EXPECT_FALSE(query::is_valid_m27d_macro_expansion_plan(runtime_declared_names));

    query::MacroExpansionPlan missing_generated_role = plan;
    query::MacroExpansionFact* const diagnostic_projection =
        find_fact(missing_generated_role,
            query::MacroExpansionFactKind::aurex_macro_output_diagnostic_projection_admission);
    ASSERT_NE(diagnostic_projection, nullptr);
    diagnostic_projection->uses_generated_source_role = false;
    missing_generated_role.summary =
        query::summarize_macro_expansion_plan_counts(missing_generated_role);
    missing_generated_role.fingerprint =
        query::macro_expansion_plan_fingerprint(missing_generated_role);
    EXPECT_FALSE(query::is_valid_m27d_macro_expansion_plan(missing_generated_role));

    query::MacroExpansionPlan duplicate_kind = plan;
    query::MacroExpansionFact* const diagnostic_duplicate =
        find_fact(duplicate_kind,
            query::MacroExpansionFactKind::aurex_macro_output_diagnostic_projection_admission);
    ASSERT_NE(diagnostic_duplicate, nullptr);
    diagnostic_duplicate->kind =
        query::MacroExpansionFactKind::aurex_macro_output_contract_admission;
    duplicate_kind.summary = query::summarize_macro_expansion_plan_counts(duplicate_kind);
    duplicate_kind.fingerprint = query::macro_expansion_plan_fingerprint(duplicate_kind);
    EXPECT_FALSE(query::is_valid_m27d_macro_expansion_plan(duplicate_kind));
}

} // namespace aurex::test
