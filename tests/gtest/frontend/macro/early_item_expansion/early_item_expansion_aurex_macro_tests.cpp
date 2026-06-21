#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_aurex_macro_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionCollectsAurexMacroSurfaceAdmissionGates)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n"
        "macro derive Inspect {\n"
        "  match item(target) -> { target }\n"
        "}\n"
        "macro const TokenBuild {\n"
        "  match tokens(input) -> { input }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult result = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(result));
    EXPECT_EQ(result.summary.macro_input_count, 0U);
    EXPECT_EQ(result.summary.attribute_input_count, 0U);
    EXPECT_EQ(result.summary.generated_part_placeholder_count, 0U);
    ASSERT_EQ(result.aurex_macro_surface_admission_gates.size(), 3U);
    EXPECT_EQ(result.summary.aurex_macro_surface_source_item_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_surface_admission_gate_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_declarative_surface_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_user_derive_surface_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_compile_time_surface_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_surface_visible_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_surface_query_reusable_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_surface_body_balanced_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_surface_match_clause_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_surface_expansion_enabled_count, 0U);
    EXPECT_EQ(result.summary.aurex_macro_surface_compile_time_execution_enabled_count, 0U);
    EXPECT_EQ(result.summary.aurex_macro_surface_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.parse_ready_token_buffer_count, 0U);
    EXPECT_EQ(result.summary.ast_mutation_count, 0U);
    EXPECT_EQ(result.summary.sema_visible_generated_part_count, 0U);
    EXPECT_EQ(result.summary.user_generated_code_count, 0U);
    EXPECT_EQ(result.summary.standard_library_required_count, 0U);
    EXPECT_EQ(result.summary.runtime_required_count, 0U);
    EXPECT_EQ(result.summary.external_process_required_count, 0U);

    const frontend::macro::AurexMacroSurfaceAdmissionGate* const build_vec =
        aurex_macro_surface_gate_by_name(result, "BuildVec");
    const frontend::macro::AurexMacroSurfaceAdmissionGate* const inspect =
        aurex_macro_surface_gate_by_name(result, "Inspect");
    const frontend::macro::AurexMacroSurfaceAdmissionGate* const token_build =
        aurex_macro_surface_gate_by_name(result, "TokenBuild");
    ASSERT_NE(build_vec, nullptr);
    ASSERT_NE(inspect, nullptr);
    ASSERT_NE(token_build, nullptr);

    EXPECT_TRUE(frontend::macro::is_valid(*build_vec));
    EXPECT_EQ(build_vec->macro_kind, syntax::MacroDeclKind::declarative);
    EXPECT_TRUE(build_vec->declarative_surface);
    EXPECT_FALSE(build_vec->user_derive_surface);
    EXPECT_FALSE(build_vec->compile_time_execution_surface);
    EXPECT_EQ(build_vec->query_name, "m27a-aurex-macro-surface:0:0:0:BuildVec");
    EXPECT_EQ(build_vec->blocker_reason, "Aurex declarative macro expansion is parser-blocked in M27a");
    EXPECT_EQ(build_vec->match_clause_count, 1U);
    EXPECT_TRUE(build_vec->body_balanced);
    EXPECT_FALSE(build_vec->expansion_enabled);
    EXPECT_FALSE(build_vec->compile_time_execution_enabled);
    EXPECT_FALSE(build_vec->parser_consumption_enabled);
    EXPECT_FALSE(build_vec->ast_mutated);
    EXPECT_FALSE(build_vec->sema_visible_generated_items);
    EXPECT_FALSE(build_vec->produced_user_generated_code);

    EXPECT_TRUE(frontend::macro::is_valid(*inspect));
    EXPECT_EQ(inspect->macro_kind, syntax::MacroDeclKind::derive);
    EXPECT_FALSE(inspect->declarative_surface);
    EXPECT_TRUE(inspect->user_derive_surface);
    EXPECT_FALSE(inspect->compile_time_execution_surface);
    EXPECT_EQ(inspect->query_name, "m27a-aurex-macro-surface:0:0:1:Inspect");
    EXPECT_EQ(inspect->blocker_reason, "Aurex user derive macro expansion is admission-only in M27b");

    EXPECT_TRUE(frontend::macro::is_valid(*token_build));
    EXPECT_EQ(token_build->macro_kind, syntax::MacroDeclKind::compile_time);
    EXPECT_FALSE(token_build->declarative_surface);
    EXPECT_FALSE(token_build->user_derive_surface);
    EXPECT_TRUE(token_build->compile_time_execution_surface);
    EXPECT_EQ(token_build->query_name, "m27a-aurex-macro-surface:0:0:2:TokenBuild");
    EXPECT_EQ(token_build->blocker_reason, "Aurex compile-time macro execution is admission-only in M27c");

    const std::string summary = frontend::macro::summarize_early_item_expansion(result);
    expect_contains(summary, "aurex_macro_surface_source_items=3");
    expect_contains(summary, "aurex_macro_surface_admissions=3");
    expect_contains(summary, "aurex_macro_declarative_surfaces=1");
    expect_contains(summary, "aurex_macro_user_derive_surfaces=1");
    expect_contains(summary, "aurex_macro_compile_time_surfaces=1");
    expect_contains(summary, "aurex_macro_surface_expansion_enabled=0");
    expect_contains(summary, "aurex_macro_surface_compile_time_execution_enabled=0");
    expect_contains(summary, "aurex_macro_surface_parser_consumable=0");

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "aurex_macro_surface_admission_gate #0");
    expect_contains(dump, "kind=declarative");
    expect_contains(dump, "kind=derive");
    expect_contains(dump, "kind=compile_time");
    expect_contains(dump, "body_balanced=yes");
    expect_contains(dump, "expansion_enabled=no");
    expect_contains(dump, "compile_time_execution_enabled=no");
    expect_contains(dump, "parser_consumption_enabled=no");
    expect_contains(dump, "user_generated_code=no");
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsAurexMacroSurfaceDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n"
        "macro derive Inspect {\n"
        "  match item(target) -> { target }\n"
        "}\n"
        "macro const TokenBuild {\n"
        "  match tokens(input) -> { input }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.aurex_macro_surface_admission_gates.size(), 3U);

    const frontend::macro::EarlyItemExpansionResult missing_gate =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.clear();
        });
    EXPECT_EQ(missing_gate.summary.aurex_macro_surface_source_item_count, 3U);
    EXPECT_EQ(missing_gate.summary.aurex_macro_surface_admission_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_gate));

    const frontend::macro::EarlyItemExpansionResult expansion_enabled =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().expansion_enabled = true;
        });
    EXPECT_EQ(expansion_enabled.summary.aurex_macro_surface_expansion_enabled_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(expansion_enabled));

    const frontend::macro::EarlyItemExpansionResult compile_time_execution_enabled =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.back().compile_time_execution_enabled = true;
        });
    EXPECT_EQ(compile_time_execution_enabled.summary
                  .aurex_macro_surface_compile_time_execution_enabled_count,
        1U);
    EXPECT_FALSE(frontend::macro::is_valid(compile_time_execution_enabled));

    const frontend::macro::EarlyItemExpansionResult parser_consumable =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().parser_consumption_enabled = true;
        });
    EXPECT_EQ(parser_consumable.summary.aurex_macro_surface_parser_consumable_count, 1U);
    EXPECT_EQ(parser_consumable.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_consumable));

    const frontend::macro::EarlyItemExpansionResult body_unbalanced =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().body_balanced = false;
        });
    EXPECT_EQ(body_unbalanced.summary.aurex_macro_surface_body_balanced_count, 2U);
    EXPECT_FALSE(frontend::macro::is_valid(body_unbalanced));

    const frontend::macro::EarlyItemExpansionResult user_code =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().produced_user_generated_code = true;
        });
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));

    const frontend::macro::EarlyItemExpansionResult wrong_surface =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().user_derive_surface = true;
        });
    EXPECT_EQ(wrong_surface.summary.aurex_macro_user_derive_surface_count, 2U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_surface));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksAurexMacroSurfaceAdmission)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.aurex_macro_surface_admission_gates.size(), 1U);

    const auto expect_fingerprint_drift =
        [&baseline](const frontend::macro::EarlyItemExpansionResult& result) {
            EXPECT_NE(result.fingerprint, baseline.fingerprint);
            EXPECT_FALSE(frontend::macro::is_valid(result));
        };

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().admission_identity =
                query::stable_fingerprint("different aurex macro surface admission identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().body_fingerprint =
                query::stable_fingerprint("different aurex macro body fingerprint");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().match_clause_count = 2U;
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().query_name =
                "m27a-aurex-macro-surface:wrong";
        }));
}

TEST(CoreUnit, EarlyItemExpansionCollectsAurexTypedMatcherAndDefinitionSiteHygieneGates)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n"
        "macro derive Inspect {\n"
        "  match item(target) -> { target }\n"
        "}\n"
        "macro const TokenBuild {\n"
        "  match tokens(input) -> { input }\n"
        "}\n"
        "macro Weird {\n"
        "  match unknown(input) -> { input }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult result = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(result));
    ASSERT_EQ(result.aurex_macro_surface_admission_gates.size(), 4U);
    ASSERT_EQ(result.aurex_macro_definition_site_hygiene_gates.size(), 4U);
    ASSERT_EQ(result.aurex_macro_typed_matcher_admission_gates.size(), 4U);
    EXPECT_EQ(result.summary.aurex_macro_definition_site_hygiene_gate_count, 4U);
    EXPECT_EQ(result.summary.aurex_macro_definition_site_scope_available_count, 4U);
    EXPECT_EQ(result.summary.aurex_macro_fresh_name_scope_reserved_count, 4U);
    EXPECT_EQ(result.summary.aurex_macro_diagnostic_anchor_available_count, 8U);
    EXPECT_EQ(result.summary.aurex_macro_hygiene_resolution_enabled_count, 0U);
    EXPECT_EQ(result.summary.aurex_macro_typed_matcher_admission_gate_count, 4U);
    EXPECT_EQ(result.summary.aurex_macro_typed_matcher_recognized_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_expr_list_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_item_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_token_stream_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_unknown_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_typed_matcher_execution_enabled_count, 0U);
    EXPECT_EQ(result.summary.parse_ready_token_buffer_count, 0U);
    EXPECT_EQ(result.summary.ast_mutation_count, 0U);
    EXPECT_EQ(result.summary.user_generated_code_count, 0U);
    EXPECT_EQ(result.summary.standard_library_required_count, 0U);
    EXPECT_EQ(result.summary.runtime_required_count, 0U);
    EXPECT_EQ(result.summary.external_process_required_count, 0U);

    const frontend::macro::AurexMacroDefinitionSiteHygieneAdmissionGate* const build_hygiene =
        aurex_macro_hygiene_gate_by_name(result, "BuildVec");
    ASSERT_NE(build_hygiene, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*build_hygiene));
    EXPECT_EQ(build_hygiene->query_name, "m27b-aurex-macro-definition-site-hygiene:0:0:0:BuildVec");
    EXPECT_TRUE(build_hygiene->definition_site_scope_available);
    EXPECT_TRUE(build_hygiene->fresh_name_scope_reserved);
    EXPECT_TRUE(build_hygiene->diagnostic_anchor_available);
    EXPECT_FALSE(build_hygiene->hygiene_resolution_enabled);
    EXPECT_FALSE(build_hygiene->declared_names_visible);
    EXPECT_FALSE(build_hygiene->produced_user_generated_code);

    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const expr_matcher =
        aurex_macro_matcher_gate_by_name_and_index(result, "BuildVec", 0U);
    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const item_matcher =
        aurex_macro_matcher_gate_by_name_and_index(result, "Inspect", 0U);
    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const token_matcher =
        aurex_macro_matcher_gate_by_name_and_index(result, "TokenBuild", 0U);
    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const unknown_matcher =
        aurex_macro_matcher_gate_by_name_and_index(result, "Weird", 0U);
    ASSERT_NE(expr_matcher, nullptr);
    ASSERT_NE(item_matcher, nullptr);
    ASSERT_NE(token_matcher, nullptr);
    ASSERT_NE(unknown_matcher, nullptr);

    EXPECT_TRUE(frontend::macro::is_valid(*expr_matcher));
    EXPECT_EQ(expr_matcher->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::expr_list);
    EXPECT_EQ(expr_matcher->matcher_head, "expr_list");
    EXPECT_EQ(expr_matcher->binding_name, "xs");
    EXPECT_TRUE(expr_matcher->matcher_shape_recognized);
    EXPECT_TRUE(expr_matcher->expr_list_matcher);
    EXPECT_FALSE(expr_matcher->matcher_execution_enabled);
    EXPECT_FALSE(expr_matcher->expansion_enabled);
    EXPECT_FALSE(expr_matcher->parser_consumption_enabled);
    EXPECT_EQ(expr_matcher->query_name, "m27b-aurex-macro-typed-matcher:0:0:0:0:BuildVec");

    EXPECT_TRUE(frontend::macro::is_valid(*item_matcher));
    EXPECT_EQ(item_matcher->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::item);
    EXPECT_EQ(item_matcher->binding_name, "target");
    EXPECT_TRUE(item_matcher->item_matcher);

    EXPECT_TRUE(frontend::macro::is_valid(*token_matcher));
    EXPECT_EQ(token_matcher->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::tokens);
    EXPECT_EQ(token_matcher->binding_name, "input");
    EXPECT_TRUE(token_matcher->token_stream_matcher);

    EXPECT_TRUE(frontend::macro::is_valid(*unknown_matcher));
    EXPECT_EQ(unknown_matcher->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::unknown);
    EXPECT_EQ(unknown_matcher->matcher_head, "unknown");
    EXPECT_TRUE(unknown_matcher->unknown_matcher);
    EXPECT_FALSE(unknown_matcher->matcher_shape_recognized);

    const std::string summary = frontend::macro::summarize_early_item_expansion(result);
    expect_contains(summary, "aurex_macro_definition_site_hygiene_gates=4");
    expect_contains(summary, "aurex_macro_typed_matcher_admissions=4");
    expect_contains(summary, "aurex_macro_typed_matchers_recognized=3");
    expect_contains(summary, "aurex_macro_expr_list_matchers=1");
    expect_contains(summary, "aurex_macro_unknown_matchers=1");
    expect_contains(summary, "aurex_macro_typed_matcher_execution_enabled=0");

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "aurex_macro_definition_site_hygiene_gate #0");
    expect_contains(dump, "definition_site_scope_available=yes");
    expect_contains(dump, "fresh_name_scope_reserved=yes");
    expect_contains(dump, "aurex_macro_typed_matcher_admission_gate #0");
    expect_contains(dump, "matcher_kind=expr_list");
    expect_contains(dump, "matcher_kind=item");
    expect_contains(dump, "matcher_kind=tokens");
    expect_contains(dump, "matcher_kind=unknown");
    expect_contains(dump, "matcher_execution_enabled=no");
}

TEST(CoreUnit, EarlyItemExpansionIndexesMultipleAndMalformedAurexTypedMatchers)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro Many {\n"
        "  match expr_list(xs) -> { xs }\n"
        "  match tokens(raw) -> { raw }\n"
        "  match item(target) -> { target }\n"
        "}\n"
        "macro Broken {\n"
        "  match expr_list xs -> { xs }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult result = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(result));
    ASSERT_EQ(result.aurex_macro_surface_admission_gates.size(), 2U);
    ASSERT_EQ(result.aurex_macro_definition_site_hygiene_gates.size(), 2U);
    ASSERT_EQ(result.aurex_macro_typed_matcher_admission_gates.size(), 4U);
    EXPECT_EQ(result.summary.aurex_macro_typed_matcher_admission_gate_count, 4U);
    EXPECT_EQ(result.summary.aurex_macro_typed_matcher_recognized_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_expr_list_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_item_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_token_stream_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_unknown_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_typed_matcher_execution_enabled_count, 0U);
    EXPECT_EQ(result.summary.aurex_macro_surface_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.parse_ready_token_buffer_count, 0U);
    EXPECT_EQ(result.summary.ast_mutation_count, 0U);
    EXPECT_EQ(result.summary.user_generated_code_count, 0U);

    const frontend::macro::AurexMacroSurfaceAdmissionGate* const many_surface =
        aurex_macro_surface_gate_by_name(result, "Many");
    const frontend::macro::AurexMacroSurfaceAdmissionGate* const broken_surface =
        aurex_macro_surface_gate_by_name(result, "Broken");
    ASSERT_NE(many_surface, nullptr);
    ASSERT_NE(broken_surface, nullptr);
    EXPECT_EQ(many_surface->match_clause_count, 3U);
    EXPECT_EQ(broken_surface->match_clause_count, 1U);

    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const many_expr =
        aurex_macro_matcher_gate_by_name_and_index(result, "Many", 0U);
    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const many_tokens =
        aurex_macro_matcher_gate_by_name_and_index(result, "Many", 1U);
    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const many_item =
        aurex_macro_matcher_gate_by_name_and_index(result, "Many", 2U);
    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const broken =
        aurex_macro_matcher_gate_by_name_and_index(result, "Broken", 0U);
    ASSERT_NE(many_expr, nullptr);
    ASSERT_NE(many_tokens, nullptr);
    ASSERT_NE(many_item, nullptr);
    ASSERT_NE(broken, nullptr);

    EXPECT_EQ(many_expr->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::expr_list);
    EXPECT_EQ(many_expr->binding_name, "xs");
    EXPECT_TRUE(many_expr->matcher_shape_recognized);
    EXPECT_EQ(many_expr->query_name, "m27b-aurex-macro-typed-matcher:0:0:0:0:Many");
    EXPECT_EQ(many_tokens->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::tokens);
    EXPECT_EQ(many_tokens->binding_name, "raw");
    EXPECT_EQ(many_tokens->query_name, "m27b-aurex-macro-typed-matcher:0:0:0:1:Many");
    EXPECT_EQ(many_item->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::item);
    EXPECT_EQ(many_item->binding_name, "target");
    EXPECT_EQ(many_item->query_name, "m27b-aurex-macro-typed-matcher:0:0:0:2:Many");

    EXPECT_EQ(broken->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::unknown);
    EXPECT_EQ(broken->matcher_head, "expr_list");
    EXPECT_TRUE(broken->binding_name.empty());
    EXPECT_TRUE(broken->unknown_matcher);
    EXPECT_FALSE(broken->matcher_shape_recognized);
    EXPECT_FALSE(broken->parser_consumption_enabled);
    EXPECT_FALSE(broken->ast_mutated);
    EXPECT_FALSE(broken->produced_user_generated_code);
}

TEST(CoreUnit, EarlyItemExpansionCollectsAurexMacroCallSitesAndUserDeriveSchemas)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n"
        "macro derive Inspect {\n"
        "  match item(target) -> { target }\n"
        "}\n"
        "macro call BuildVec {\n"
        "  1, 2, nested(3)\n"
        "}\n"
        "macro call Missing {\n"
        "  raw(tokens)\n"
        "}\n"
        "#[derive(Inspect)]\n"
        "struct Config { threads: i32; enabled: bool; }\n"
        "#[derive(Inspect)]\n"
        "enum Mode { fast, slow(i32), tuple(i32, bool) }\n";

    const frontend::macro::EarlyItemExpansionResult result = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(result));
    ASSERT_EQ(result.aurex_macro_call_site_admission_gates.size(), 2U);
    ASSERT_EQ(result.aurex_macro_matcher_to_call_binding_gates.size(), 1U);
    ASSERT_EQ(result.aurex_user_derive_target_schema_gates.size(), 2U);

    EXPECT_EQ(result.summary.aurex_macro_call_site_admission_gate_count, 2U);
    EXPECT_EQ(result.summary.aurex_macro_call_site_source_item_count, 2U);
    EXPECT_EQ(result.summary.aurex_macro_call_site_target_declared_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_call_site_visible_count, 2U);
    EXPECT_EQ(result.summary.aurex_macro_call_site_query_reusable_count, 2U);
    EXPECT_EQ(result.summary.aurex_macro_call_site_balanced_count, 2U);
    EXPECT_EQ(result.summary.aurex_macro_call_site_expansion_enabled_count, 0U);
    EXPECT_EQ(result.summary.aurex_macro_matcher_to_call_binding_gate_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_matcher_to_call_binding_admitted_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_matcher_to_call_binding_visible_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_matcher_to_call_binding_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_gate_count, 2U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_source_derive_count, 2U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_struct_count, 1U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_enum_count, 1U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_unsupported_count, 0U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_field_count, 2U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_enum_case_count, 3U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_enum_payload_count, 3U);
    EXPECT_EQ(result.summary.generated_source_text_count, 0U);
    EXPECT_EQ(result.summary.parse_ready_token_buffer_count, 0U);
    EXPECT_EQ(result.summary.ast_mutation_count, 0U);
    EXPECT_EQ(result.summary.sema_visible_generated_part_count, 0U);
    EXPECT_EQ(result.summary.user_generated_code_count, 0U);
    EXPECT_EQ(result.summary.standard_library_required_count, 0U);
    EXPECT_EQ(result.summary.runtime_required_count, 0U);
    EXPECT_EQ(result.summary.external_process_required_count, 0U);

    const frontend::macro::AurexMacroCallSiteAdmissionGate* const build_call =
        aurex_macro_call_site_gate_by_name(result, "BuildVec");
    const frontend::macro::AurexMacroCallSiteAdmissionGate* const missing_call =
        aurex_macro_call_site_gate_by_name(result, "Missing");
    ASSERT_NE(build_call, nullptr);
    ASSERT_NE(missing_call, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*build_call));
    EXPECT_TRUE(build_call->target_surface_declared);
    EXPECT_TRUE(build_call->token_tree_balanced);
    EXPECT_EQ(build_call->blocker_reason, "Aurex macro call-site expansion is admission-only in M27c");
    EXPECT_EQ(build_call->query_name, "m27c-aurex-macro-call-site:0:0:2:BuildVec");
    EXPECT_FALSE(build_call->expansion_enabled);
    EXPECT_FALSE(build_call->parser_consumption_enabled);
    EXPECT_FALSE(build_call->ast_mutated);
    EXPECT_FALSE(build_call->produced_user_generated_code);

    EXPECT_TRUE(frontend::macro::is_valid(*missing_call));
    EXPECT_FALSE(missing_call->target_surface_declared);
    EXPECT_EQ(missing_call->blocker_reason,
        "Aurex macro call-site target is not declared and remains blocked in M27c");

    const frontend::macro::AurexMacroMatcherToCallBindingAdmissionGate* const binding =
        aurex_macro_binding_gate_by_call_name(result, "BuildVec");
    ASSERT_NE(binding, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*binding));
    EXPECT_TRUE(binding->target_surface_declared);
    EXPECT_TRUE(binding->matcher_shape_recognized);
    EXPECT_TRUE(binding->binding_admitted);
    EXPECT_EQ(binding->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::expr_list);
    EXPECT_EQ(binding->matcher_head, "expr_list");
    EXPECT_EQ(binding->binding_name, "xs");
    EXPECT_EQ(binding->blocker_reason, "Aurex matcher-to-call binding is admission-only in M27c");
    EXPECT_FALSE(binding->matcher_execution_enabled);
    EXPECT_FALSE(binding->expansion_enabled);
    EXPECT_FALSE(binding->parser_consumption_enabled);
    EXPECT_FALSE(binding->ast_mutated);
    EXPECT_FALSE(binding->produced_user_generated_code);

    const frontend::macro::AurexUserDeriveTargetSchemaAdmissionGate* const config_schema =
        aurex_user_derive_schema_gate_by_target(result, "Config");
    const frontend::macro::AurexUserDeriveTargetSchemaAdmissionGate* const mode_schema =
        aurex_user_derive_schema_gate_by_target(result, "Mode");
    ASSERT_NE(config_schema, nullptr);
    ASSERT_NE(mode_schema, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*config_schema));
    EXPECT_EQ(config_schema->target_kind, frontend::macro::AurexUserDeriveTargetKind::struct_);
    EXPECT_EQ(config_schema->derive_name, "Inspect");
    EXPECT_EQ(config_schema->field_count, 2U);
    EXPECT_EQ(config_schema->enum_case_count, 0U);
    EXPECT_EQ(config_schema->blocker_reason,
        "Aurex user derive target schema is admission-only in M27c");
    EXPECT_FALSE(config_schema->expansion_enabled);
    EXPECT_FALSE(config_schema->parser_consumption_enabled);
    EXPECT_FALSE(config_schema->ast_mutated);
    EXPECT_FALSE(config_schema->produced_user_generated_code);

    EXPECT_TRUE(frontend::macro::is_valid(*mode_schema));
    EXPECT_EQ(mode_schema->target_kind, frontend::macro::AurexUserDeriveTargetKind::enum_);
    EXPECT_EQ(mode_schema->enum_case_count, 3U);
    EXPECT_EQ(mode_schema->enum_payload_count, 3U);
    EXPECT_FALSE(mode_schema->sema_visible_generated_items);

    const std::string summary = frontend::macro::summarize_early_item_expansion(result);
    expect_contains(summary, "aurex_macro_call_site_admissions=2");
    expect_contains(summary, "aurex_macro_call_site_source_items=2");
    expect_contains(summary, "aurex_macro_matcher_to_call_bindings_admitted=1");
    expect_contains(summary, "aurex_user_derive_target_schema_source_derives=2");
    expect_contains(summary, "aurex_user_derive_target_schemas=2");
    expect_contains(summary, "aurex_user_derive_target_schema_fields=2");
    expect_contains(summary, "parse_ready_token_buffers=0");
    expect_contains(summary, "user_generated_code=0");

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "aurex_macro_call_site_admission_gate #0");
    expect_contains(dump, "target_surface_declared=yes");
    expect_contains(dump, "target_surface_declared=no");
    expect_contains(dump, "aurex_macro_matcher_to_call_binding_gate #0");
    expect_contains(dump, "binding_admitted=yes");
    expect_contains(dump, "aurex_user_derive_target_schema_gate #0");
    expect_contains(dump, "target_kind=struct");
    expect_contains(dump, "target_kind=enum");
    expect_contains(dump, "parser_consumption_enabled=no");
    expect_contains(dump, "user_generated_code=no");
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsAurexMacroCallSiteAndUserDeriveDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n"
        "macro derive Inspect {\n"
        "  match item(target) -> { target }\n"
        "}\n"
        "macro call BuildVec {\n"
        "  1, 2, nested(3)\n"
        "}\n"
        "#[derive(Inspect)]\n"
        "struct Config { threads: i32; enabled: bool; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.aurex_macro_call_site_admission_gates.size(), 1U);
    ASSERT_EQ(baseline.aurex_macro_matcher_to_call_binding_gates.size(), 1U);
    ASSERT_EQ(baseline.aurex_user_derive_target_schema_gates.size(), 1U);
    EXPECT_EQ(baseline.summary.aurex_macro_call_site_source_item_count, 1U);
    EXPECT_EQ(baseline.summary.aurex_user_derive_target_schema_source_derive_count, 1U);

    const auto expect_invalid = [](const frontend::macro::EarlyItemExpansionResult& result) {
        EXPECT_FALSE(frontend::macro::is_valid(result));
    };

    const frontend::macro::EarlyItemExpansionResult missing_call_site =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_call_site_admission_gates.clear();
        });
    EXPECT_EQ(missing_call_site.summary.aurex_macro_call_site_admission_gate_count, 0U);
    expect_invalid(missing_call_site);

    const frontend::macro::EarlyItemExpansionResult executable_call_site =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_call_site_admission_gates.front().expansion_enabled = true;
        });
    EXPECT_EQ(executable_call_site.summary.aurex_macro_call_site_expansion_enabled_count, 1U);
    expect_invalid(executable_call_site);

    const frontend::macro::EarlyItemExpansionResult parser_consumable_call_site =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_call_site_admission_gates.front().parser_consumption_enabled = true;
        });
    EXPECT_EQ(parser_consumable_call_site.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(parser_consumable_call_site);

    const frontend::macro::EarlyItemExpansionResult missing_binding =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_matcher_to_call_binding_gates.clear();
        });
    EXPECT_EQ(missing_binding.summary.aurex_macro_matcher_to_call_binding_gate_count, 0U);
    expect_invalid(missing_binding);

    const frontend::macro::EarlyItemExpansionResult binding_executed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_matcher_to_call_binding_gates.front().matcher_execution_enabled = true;
        });
    EXPECT_EQ(binding_executed.summary.aurex_macro_typed_matcher_execution_enabled_count, 1U);
    expect_invalid(binding_executed);

    const frontend::macro::EarlyItemExpansionResult schema_parser_consumable =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_user_derive_target_schema_gates.front().parser_consumption_enabled = true;
        });
    EXPECT_EQ(schema_parser_consumable.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(schema_parser_consumable);

    const frontend::macro::EarlyItemExpansionResult missing_schema =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_user_derive_target_schema_gates.clear();
        });
    EXPECT_EQ(missing_schema.summary.aurex_user_derive_target_schema_gate_count, 0U);
    expect_invalid(missing_schema);
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsAurexTypedMatcherAndHygieneDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.aurex_macro_definition_site_hygiene_gates.size(), 1U);
    ASSERT_EQ(baseline.aurex_macro_typed_matcher_admission_gates.size(), 1U);

    const frontend::macro::EarlyItemExpansionResult missing_hygiene =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_definition_site_hygiene_gates.clear();
        });
    EXPECT_EQ(missing_hygiene.summary.aurex_macro_definition_site_hygiene_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_hygiene));

    const frontend::macro::EarlyItemExpansionResult hygiene_enabled =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_definition_site_hygiene_gates.front().hygiene_resolution_enabled = true;
        });
    EXPECT_EQ(hygiene_enabled.summary.aurex_macro_hygiene_resolution_enabled_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(hygiene_enabled));

    const frontend::macro::EarlyItemExpansionResult missing_matcher =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.clear();
        });
    EXPECT_EQ(missing_matcher.summary.aurex_macro_typed_matcher_admission_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_matcher));

    const frontend::macro::EarlyItemExpansionResult matcher_executed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().matcher_execution_enabled = true;
        });
    EXPECT_EQ(matcher_executed.summary.aurex_macro_typed_matcher_execution_enabled_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(matcher_executed));

    const frontend::macro::EarlyItemExpansionResult matcher_consumable =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().parser_consumption_enabled = true;
        });
    EXPECT_EQ(matcher_consumable.summary.aurex_macro_surface_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(matcher_consumable));

    const frontend::macro::EarlyItemExpansionResult wrong_kind_flags =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().item_matcher = true;
        });
    EXPECT_EQ(wrong_kind_flags.summary.aurex_macro_item_matcher_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_kind_flags));

    const frontend::macro::EarlyItemExpansionResult wrong_surface_identity =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().surface_admission_identity =
                query::stable_fingerprint("different m27b surface identity");
        });
    EXPECT_FALSE(frontend::macro::is_valid(wrong_surface_identity));

    const frontend::macro::EarlyItemExpansionResult wrong_hygiene_identity =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().definition_site_hygiene_identity =
                query::stable_fingerprint("different m27b definition-site hygiene identity");
        });
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_identity));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksAurexTypedMatcherAndHygieneAdmission)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.aurex_macro_definition_site_hygiene_gates.size(), 1U);
    ASSERT_EQ(baseline.aurex_macro_typed_matcher_admission_gates.size(), 1U);

    const auto expect_fingerprint_drift =
        [&baseline](const frontend::macro::EarlyItemExpansionResult& result) {
            EXPECT_NE(result.fingerprint, baseline.fingerprint);
            EXPECT_FALSE(frontend::macro::is_valid(result));
        };

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_definition_site_hygiene_gates.front().hygiene_identity =
                query::stable_fingerprint("different m27b hygiene identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().matcher_identity =
                query::stable_fingerprint("different m27b matcher identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().binding_name = "changed";
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().query_name =
                "m27b-aurex-macro-typed-matcher:wrong";
        }));
}
} // namespace aurex::test
