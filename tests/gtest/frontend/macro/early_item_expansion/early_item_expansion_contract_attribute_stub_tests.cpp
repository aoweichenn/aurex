#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_contract_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionValidationRejectsHygieneAndTraceStubDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.source_maps.empty());
    ASSERT_FALSE(baseline.hygiene_stubs.empty());
    ASSERT_FALSE(baseline.trace_stubs.empty());

    frontend::macro::EarlyItemExpansionResult missing_source_map = baseline;
    missing_source_map.source_maps.clear();
    refresh_expansion_result(missing_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(missing_source_map));

    frontend::macro::EarlyItemExpansionResult missing_hygiene = baseline;
    missing_hygiene.hygiene_stubs.clear();
    refresh_expansion_result(missing_hygiene);
    EXPECT_EQ(missing_hygiene.summary.hygiene_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_hygiene));

    frontend::macro::EarlyItemExpansionResult empty_call_site_mark = baseline;
    empty_call_site_mark.hygiene_stubs.front().call_site_mark = {};
    refresh_expansion_result(empty_call_site_mark);
    EXPECT_FALSE(frontend::macro::is_valid(empty_call_site_mark));

    frontend::macro::EarlyItemExpansionResult empty_definition_site_mark = baseline;
    empty_definition_site_mark.hygiene_stubs.front().definition_site_mark = {};
    refresh_expansion_result(empty_definition_site_mark);
    EXPECT_FALSE(frontend::macro::is_valid(empty_definition_site_mark));

    frontend::macro::EarlyItemExpansionResult empty_generated_fresh_mark = baseline;
    empty_generated_fresh_mark.hygiene_stubs.front().generated_fresh_mark = {};
    refresh_expansion_result(empty_generated_fresh_mark);
    EXPECT_FALSE(frontend::macro::is_valid(empty_generated_fresh_mark));

    frontend::macro::EarlyItemExpansionResult empty_declared_name_set = baseline;
    empty_declared_name_set.hygiene_stubs.front().declared_name_set = {};
    refresh_expansion_result(empty_declared_name_set);
    EXPECT_EQ(empty_declared_name_set.summary.declared_name_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(empty_declared_name_set));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene_origin = baseline;
    wrong_hygiene_origin.hygiene_stubs.front().expansion_origin =
        query::stable_fingerprint("wrong hygiene origin");
    refresh_expansion_result(wrong_hygiene_origin);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_origin));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene_policy = baseline;
    wrong_hygiene_policy.hygiene_stubs.front().policy = "wrong_hygiene_policy";
    refresh_expansion_result(wrong_hygiene_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_policy));

    frontend::macro::EarlyItemExpansionResult resolved_hygiene = baseline;
    resolved_hygiene.hygiene_stubs.front().resolved = true;
    refresh_expansion_result(resolved_hygiene);
    EXPECT_EQ(resolved_hygiene.summary.unresolved_hygiene_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(resolved_hygiene));

    frontend::macro::EarlyItemExpansionResult visible_declared_names = baseline;
    visible_declared_names.hygiene_stubs.front().declared_names_visible = true;
    refresh_expansion_result(visible_declared_names);
    EXPECT_FALSE(frontend::macro::is_valid(visible_declared_names));

    frontend::macro::EarlyItemExpansionResult call_site_capture = baseline;
    call_site_capture.hygiene_stubs.front().captures_call_site_locals = true;
    refresh_expansion_result(call_site_capture);
    EXPECT_EQ(call_site_capture.summary.call_site_capture_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(call_site_capture));

    frontend::macro::EarlyItemExpansionResult missing_trace = baseline;
    missing_trace.trace_stubs.clear();
    refresh_expansion_result(missing_trace);
    EXPECT_EQ(missing_trace.summary.trace_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_trace));

    frontend::macro::EarlyItemExpansionResult empty_trace_identity = baseline;
    empty_trace_identity.trace_stubs.front().trace_identity = {};
    refresh_expansion_result(empty_trace_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_trace_identity));

    frontend::macro::EarlyItemExpansionResult empty_generated_source_map = baseline;
    empty_generated_source_map.trace_stubs.front().generated_source_map_identity = {};
    refresh_expansion_result(empty_generated_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(empty_generated_source_map));

    frontend::macro::EarlyItemExpansionResult empty_diagnostic_anchor = baseline;
    empty_diagnostic_anchor.trace_stubs.front().diagnostic_anchor = {};
    refresh_expansion_result(empty_diagnostic_anchor);
    EXPECT_FALSE(frontend::macro::is_valid(empty_diagnostic_anchor));

    frontend::macro::EarlyItemExpansionResult wrong_trace_policy = baseline;
    wrong_trace_policy.trace_stubs.front().trace_policy = "wrong_trace_policy";
    refresh_expansion_result(wrong_trace_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace_policy));

    frontend::macro::EarlyItemExpansionResult wrong_trace_blocker = baseline;
    wrong_trace_blocker.trace_stubs.front().blocker_reason = "wrong blocker";
    refresh_expansion_result(wrong_trace_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace_blocker));

    frontend::macro::EarlyItemExpansionResult trace_real_source_map = baseline;
    trace_real_source_map.trace_stubs.front().real_source_map = true;
    refresh_expansion_result(trace_real_source_map);
    EXPECT_EQ(trace_real_source_map.summary.real_source_map_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(trace_real_source_map));

    frontend::macro::EarlyItemExpansionResult trace_debug_available = baseline;
    trace_debug_available.trace_stubs.front().debug_trace_available = true;
    refresh_expansion_result(trace_debug_available);
    EXPECT_EQ(trace_debug_available.summary.debug_trace_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(trace_debug_available));

    frontend::macro::EarlyItemExpansionResult trace_cli_emit = baseline;
    trace_cli_emit.trace_stubs.front().cli_emit_expanded_available = true;
    refresh_expansion_result(trace_cli_emit);
    EXPECT_EQ(trace_cli_emit.summary.cli_emit_expanded_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(trace_cli_emit));

    frontend::macro::EarlyItemExpansionResult wrong_trace_origin = baseline;
    wrong_trace_origin.trace_stubs.front().expansion_origin =
        query::stable_fingerprint("wrong trace origin");
    refresh_expansion_result(wrong_trace_origin);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace_origin));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksHygieneAndTraceStubContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.hygiene_stubs.empty());
    ASSERT_FALSE(baseline.trace_stubs.empty());

    frontend::macro::EarlyItemExpansionResult hygiene_mark = baseline;
    hygiene_mark.hygiene_stubs.front().call_site_mark =
        query::stable_fingerprint("different call site mark");
    refresh_expansion_result(hygiene_mark);
    EXPECT_NE(hygiene_mark.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(hygiene_mark));

    frontend::macro::EarlyItemExpansionResult declared_names = baseline;
    declared_names.hygiene_stubs.front().declared_name_set =
        query::stable_fingerprint("different declared name set");
    refresh_expansion_result(declared_names);
    EXPECT_NE(declared_names.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(declared_names));

    frontend::macro::EarlyItemExpansionResult trace_identity = baseline;
    trace_identity.trace_stubs.front().trace_identity =
        query::stable_fingerprint("different trace identity");
    refresh_expansion_result(trace_identity);
    EXPECT_NE(trace_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(trace_identity));

    frontend::macro::EarlyItemExpansionResult source_map_identity = baseline;
    source_map_identity.trace_stubs.front().generated_source_map_identity =
        query::stable_fingerprint("different generated source map identity");
    refresh_expansion_result(source_map_identity);
    EXPECT_NE(source_map_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(source_map_identity));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsGeneratedItemAndDeclaredNameDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.generated_item_declarations.empty());
    ASSERT_FALSE(baseline.declared_generated_names.empty());

    frontend::macro::EarlyItemExpansionResult missing_declaration = baseline;
    missing_declaration.generated_item_declarations.clear();
    refresh_expansion_result(missing_declaration);
    EXPECT_EQ(missing_declaration.summary.generated_item_declaration_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_declaration));

    frontend::macro::EarlyItemExpansionResult missing_declared_name = baseline;
    missing_declared_name.declared_generated_names.clear();
    refresh_expansion_result(missing_declared_name);
    EXPECT_EQ(missing_declared_name.summary.declared_generated_name_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_declared_name));

    frontend::macro::EarlyItemExpansionResult empty_declaration_identity = baseline;
    empty_declaration_identity.generated_item_declarations.front().declaration_identity = {};
    refresh_expansion_result(empty_declaration_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_declaration_identity));

    frontend::macro::EarlyItemExpansionResult empty_generated_item_key = baseline;
    empty_generated_item_key.generated_item_declarations.front().generated_item_key = {};
    refresh_expansion_result(empty_generated_item_key);
    EXPECT_FALSE(frontend::macro::is_valid(empty_generated_item_key));

    frontend::macro::EarlyItemExpansionResult wrong_declaration_role = baseline;
    wrong_declaration_role.generated_item_declarations.front().declaration_role = "wrong_role";
    refresh_expansion_result(wrong_declaration_role);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declaration_role));

    frontend::macro::EarlyItemExpansionResult wrong_generated_item_name = baseline;
    wrong_generated_item_name.generated_item_declarations.front().generated_item_name =
        "__aurex_macro_declared:wrong";
    refresh_expansion_result(wrong_generated_item_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_item_name));

    frontend::macro::EarlyItemExpansionResult materialized_declaration = baseline;
    materialized_declaration.generated_item_declarations.front().materialized_tokens = true;
    refresh_expansion_result(materialized_declaration);
    EXPECT_EQ(materialized_declaration.summary.materialized_generated_item_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(materialized_declaration));

    frontend::macro::EarlyItemExpansionResult unplanned_declaration = baseline;
    unplanned_declaration.generated_item_declarations.front().planned = false;
    refresh_expansion_result(unplanned_declaration);
    EXPECT_EQ(unplanned_declaration.summary.planned_generated_item_declaration_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(unplanned_declaration));

    frontend::macro::EarlyItemExpansionResult parsed_declaration = baseline;
    parsed_declaration.generated_item_declarations.front().parsed = true;
    refresh_expansion_result(parsed_declaration);
    EXPECT_EQ(parsed_declaration.summary.parsed_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parsed_declaration));

    frontend::macro::EarlyItemExpansionResult sema_visible_declaration = baseline;
    sema_visible_declaration.generated_item_declarations.front().sema_visible = true;
    refresh_expansion_result(sema_visible_declaration);
    EXPECT_EQ(sema_visible_declaration.summary.sema_visible_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(sema_visible_declaration));

    frontend::macro::EarlyItemExpansionResult wrong_declaration_name_set = baseline;
    wrong_declaration_name_set.generated_item_declarations.front().declared_name_set =
        query::stable_fingerprint("wrong declaration name set");
    refresh_expansion_result(wrong_declaration_name_set);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declaration_name_set));

    frontend::macro::EarlyItemExpansionResult empty_declared_name_identity = baseline;
    empty_declared_name_identity.declared_generated_names.front().declared_name_identity = {};
    refresh_expansion_result(empty_declared_name_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_declared_name_identity));

    frontend::macro::EarlyItemExpansionResult empty_hygiene_mark = baseline;
    empty_hygiene_mark.declared_generated_names.front().hygiene_mark = {};
    refresh_expansion_result(empty_hygiene_mark);
    EXPECT_FALSE(frontend::macro::is_valid(empty_hygiene_mark));

    frontend::macro::EarlyItemExpansionResult wrong_namespace = baseline;
    wrong_namespace.declared_generated_names.front().namespace_kind = "value";
    refresh_expansion_result(wrong_namespace);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_namespace));

    frontend::macro::EarlyItemExpansionResult wrong_declared_name = baseline;
    wrong_declared_name.declared_generated_names.front().declared_name =
        "__aurex_macro_declared:wrong";
    refresh_expansion_result(wrong_declared_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declared_name));

    frontend::macro::EarlyItemExpansionResult lookup_visible = baseline;
    lookup_visible.declared_generated_names.front().lookup_visible = true;
    refresh_expansion_result(lookup_visible);
    EXPECT_EQ(lookup_visible.summary.lookup_visible_declared_name_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(lookup_visible));

    frontend::macro::EarlyItemExpansionResult export_visible = baseline;
    export_visible.declared_generated_names.front().export_visible = true;
    refresh_expansion_result(export_visible);
    EXPECT_EQ(export_visible.summary.export_visible_declared_name_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(export_visible));

    frontend::macro::EarlyItemExpansionResult sema_visible_name = baseline;
    sema_visible_name.declared_generated_names.front().sema_visible = true;
    refresh_expansion_result(sema_visible_name);
    EXPECT_EQ(sema_visible_name.summary.sema_visible_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(sema_visible_name));

    frontend::macro::EarlyItemExpansionResult user_code_name = baseline;
    user_code_name.declared_generated_names.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code_name);
    EXPECT_EQ(user_code_name.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code_name));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksGeneratedItemAndDeclaredNameContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.generated_item_declarations.empty());
    ASSERT_FALSE(baseline.declared_generated_names.empty());

    frontend::macro::EarlyItemExpansionResult declaration_identity = baseline;
    declaration_identity.generated_item_declarations.front().declaration_identity =
        query::stable_fingerprint("different declaration identity");
    refresh_expansion_result(declaration_identity);
    EXPECT_NE(declaration_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(declaration_identity));

    frontend::macro::EarlyItemExpansionResult generated_item_key = baseline;
    generated_item_key.generated_item_declarations.front().generated_item_key =
        query::stable_fingerprint("different generated item key");
    refresh_expansion_result(generated_item_key);
    EXPECT_NE(generated_item_key.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(generated_item_key));

    frontend::macro::EarlyItemExpansionResult declared_name_identity = baseline;
    declared_name_identity.declared_generated_names.front().declared_name_identity =
        query::stable_fingerprint("different declared name identity");
    refresh_expansion_result(declared_name_identity);
    EXPECT_NE(declared_name_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(declared_name_identity));

    frontend::macro::EarlyItemExpansionResult hygiene_mark = baseline;
    hygiene_mark.declared_generated_names.front().hygiene_mark =
        query::stable_fingerprint("different declared name hygiene mark");
    refresh_expansion_result(hygiene_mark);
    EXPECT_NE(hygiene_mark.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(hygiene_mark));
}
} // namespace aurex::test
