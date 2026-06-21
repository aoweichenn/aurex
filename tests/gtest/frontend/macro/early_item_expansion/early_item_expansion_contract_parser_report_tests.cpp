#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_contract_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParserAdmissionDiagnosticProjectionContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.parser_admission_diagnostics.empty());

    frontend::macro::EarlyItemExpansionResult diagnostic_identity = baseline;
    diagnostic_identity.parser_admission_diagnostics.front().diagnostic_identity =
        query::stable_fingerprint("different diagnostic identity");
    refresh_expansion_result(diagnostic_identity);
    EXPECT_NE(diagnostic_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(diagnostic_identity));

    frontend::macro::EarlyItemExpansionResult category = baseline;
    category.parser_admission_diagnostics.front().blocker_category =
        "empty_token_buffer_parser_admission_blocked";
    refresh_expansion_result(category);
    EXPECT_NE(category.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(category));

    frontend::macro::EarlyItemExpansionResult debug_projection = baseline;
    debug_projection.parser_admission_diagnostics.front().debug_projection_name =
        "m21k-parser-admission:different";
    refresh_expansion_result(debug_projection);
    EXPECT_NE(debug_projection.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(debug_projection));

    frontend::macro::EarlyItemExpansionResult source_anchor = baseline;
    source_anchor.parser_admission_diagnostics.front().primary_anchor.end += 1U;
    refresh_expansion_result(source_anchor);
    EXPECT_NE(source_anchor.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(source_anchor));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsParserAdmissionReportProjectionDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.parser_admission_report_entries.size(), 2U);
    ASSERT_EQ(baseline.parser_admission_reports.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_entries = baseline;
    missing_entries.parser_admission_report_entries.clear();
    refresh_expansion_result(missing_entries);
    EXPECT_EQ(missing_entries.summary.parser_admission_report_entry_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_entries));

    frontend::macro::EarlyItemExpansionResult missing_reports = baseline;
    missing_reports.parser_admission_reports.clear();
    refresh_expansion_result(missing_reports);
    EXPECT_EQ(missing_reports.summary.parser_admission_report_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_reports));

    frontend::macro::EarlyItemExpansionResult wrong_entry_identity = baseline;
    wrong_entry_identity.parser_admission_report_entries.front().report_entry_identity =
        query::stable_fingerprint("wrong parser admission report entry identity");
    refresh_expansion_result(wrong_entry_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_entry_identity));

    frontend::macro::EarlyItemExpansionResult wrong_entry_anchor = baseline;
    wrong_entry_anchor.parser_admission_report_entries.front().diagnostic_anchor_identity =
        query::stable_fingerprint("wrong parser admission report entry anchor");
    refresh_expansion_result(wrong_entry_anchor);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_entry_anchor));

    frontend::macro::EarlyItemExpansionResult wrong_entry_category = baseline;
    wrong_entry_category.parser_admission_report_entries.back().blocker_category =
        "empty_token_buffer_parser_admission_blocked";
    refresh_expansion_result(wrong_entry_category);
    EXPECT_EQ(wrong_entry_category.summary.parser_admission_report_derive_entry_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_entry_category));

    frontend::macro::EarlyItemExpansionResult wrong_query_name = baseline;
    wrong_query_name.parser_admission_report_entries.front().query_projection_name =
        "m21l-parser-admission-report:wrong";
    refresh_expansion_result(wrong_query_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_query_name));

    frontend::macro::EarlyItemExpansionResult wrong_report_index = baseline;
    wrong_report_index.parser_admission_report_entries.back().report_index = 0U;
    refresh_expansion_result(wrong_report_index);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_index));

    frontend::macro::EarlyItemExpansionResult entry_parser_admitted = baseline;
    entry_parser_admitted.parser_admission_report_entries.front().parser_admitted = true;
    refresh_expansion_result(entry_parser_admitted);
    EXPECT_EQ(entry_parser_admitted.summary.parser_admission_report_blocked_entry_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(entry_parser_admitted));

    frontend::macro::EarlyItemExpansionResult entry_not_visible = baseline;
    entry_not_visible.parser_admission_report_entries.front().report_visible = false;
    refresh_expansion_result(entry_not_visible);
    EXPECT_FALSE(frontend::macro::is_valid(entry_not_visible));

    frontend::macro::EarlyItemExpansionResult entry_not_reusable = baseline;
    entry_not_reusable.parser_admission_report_entries.front().query_reusable = false;
    refresh_expansion_result(entry_not_reusable);
    EXPECT_FALSE(frontend::macro::is_valid(entry_not_reusable));

    frontend::macro::EarlyItemExpansionResult entry_parser_consumable = baseline;
    entry_parser_consumable.parser_admission_report_entries.front().parser_consumable = true;
    refresh_expansion_result(entry_parser_consumable);
    EXPECT_FALSE(frontend::macro::is_valid(entry_parser_consumable));

    frontend::macro::EarlyItemExpansionResult entry_emit_expanded = baseline;
    entry_emit_expanded.parser_admission_report_entries.front().emit_expanded_available = true;
    refresh_expansion_result(entry_emit_expanded);
    EXPECT_EQ(entry_emit_expanded.summary.emit_expanded_projection_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(entry_emit_expanded));

    frontend::macro::EarlyItemExpansionResult entry_user_code = baseline;
    entry_user_code.parser_admission_report_entries.front().produced_user_generated_code = true;
    refresh_expansion_result(entry_user_code);
    EXPECT_EQ(entry_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(entry_user_code));

    frontend::macro::EarlyItemExpansionResult wrong_report_identity = baseline;
    wrong_report_identity.parser_admission_reports.front().report_identity =
        query::stable_fingerprint("wrong parser admission report identity");
    refresh_expansion_result(wrong_report_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_identity));

    frontend::macro::EarlyItemExpansionResult wrong_report_group = baseline;
    wrong_report_group.parser_admission_reports.front().report_grouping_identity =
        query::stable_fingerprint("wrong parser admission report grouping");
    refresh_expansion_result(wrong_report_group);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_group));

    frontend::macro::EarlyItemExpansionResult wrong_report_anchor = baseline;
    wrong_report_anchor.parser_admission_reports.front().report_anchor_identity =
        query::stable_fingerprint("wrong parser admission report anchor");
    refresh_expansion_result(wrong_report_anchor);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_anchor));

    frontend::macro::EarlyItemExpansionResult wrong_parse_config = baseline;
    wrong_parse_config.parser_admission_reports.front().parse_config_fingerprint =
        query::stable_fingerprint("wrong parser admission report parse config");
    refresh_expansion_result(wrong_parse_config);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_parse_config));

    frontend::macro::EarlyItemExpansionResult wrong_generated_buffer = baseline;
    wrong_generated_buffer.parser_admission_reports.front().generated_buffer_identity =
        query::stable_fingerprint("wrong parser admission report generated buffer");
    refresh_expansion_result(wrong_generated_buffer);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_buffer));

    frontend::macro::EarlyItemExpansionResult wrong_policy = baseline;
    wrong_policy.parser_admission_reports.front().report_policy = "wrong_report_policy";
    refresh_expansion_result(wrong_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_policy));

    frontend::macro::EarlyItemExpansionResult wrong_report_query = baseline;
    wrong_report_query.parser_admission_reports.front().report_query_name =
        "m21l-parser-admission-report:wrong";
    refresh_expansion_result(wrong_report_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_query));

    frontend::macro::EarlyItemExpansionResult wrong_blocker = baseline;
    wrong_blocker.parser_admission_reports.front().blocked_reason = "wrong report blocker";
    refresh_expansion_result(wrong_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_blocker));

    frontend::macro::EarlyItemExpansionResult wrong_entry_count = baseline;
    wrong_entry_count.parser_admission_reports.front().entry_count = 1U;
    refresh_expansion_result(wrong_entry_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_entry_count));

    frontend::macro::EarlyItemExpansionResult wrong_category_totals = baseline;
    wrong_category_totals.parser_admission_reports.front().derive_entry_count = 0U;
    refresh_expansion_result(wrong_category_totals);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_category_totals));

    frontend::macro::EarlyItemExpansionResult report_not_visible = baseline;
    report_not_visible.parser_admission_reports.front().report_visible = false;
    refresh_expansion_result(report_not_visible);
    EXPECT_EQ(report_not_visible.summary.parser_admission_report_visible_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(report_not_visible));

    frontend::macro::EarlyItemExpansionResult report_not_reusable = baseline;
    report_not_reusable.parser_admission_reports.front().query_reusable = false;
    refresh_expansion_result(report_not_reusable);
    EXPECT_EQ(report_not_reusable.summary.parser_admission_report_query_reusable_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(report_not_reusable));

    frontend::macro::EarlyItemExpansionResult unordered_report = baseline;
    unordered_report.parser_admission_reports.front().source_anchor_ordered = false;
    refresh_expansion_result(unordered_report);
    EXPECT_EQ(unordered_report.summary.parser_admission_report_unordered_anchor_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(unordered_report));

    frontend::macro::EarlyItemExpansionResult report_parser_admitted = baseline;
    report_parser_admitted.parser_admission_reports.front().parser_admitted = true;
    refresh_expansion_result(report_parser_admitted);
    EXPECT_FALSE(frontend::macro::is_valid(report_parser_admitted));

    frontend::macro::EarlyItemExpansionResult report_parse_ready = baseline;
    report_parse_ready.parser_admission_reports.front().parse_ready = true;
    refresh_expansion_result(report_parse_ready);
    EXPECT_EQ(report_parse_ready.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_parse_ready));

    frontend::macro::EarlyItemExpansionResult report_parser_consumable = baseline;
    report_parser_consumable.parser_admission_reports.front().parser_consumable = true;
    refresh_expansion_result(report_parser_consumable);
    EXPECT_EQ(report_parser_consumable.summary.parser_admission_report_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_parser_consumable));

    frontend::macro::EarlyItemExpansionResult report_emit_expanded = baseline;
    report_emit_expanded.parser_admission_reports.front().emit_expanded_available = true;
    refresh_expansion_result(report_emit_expanded);
    EXPECT_EQ(report_emit_expanded.summary.emit_expanded_projection_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_emit_expanded));

    frontend::macro::EarlyItemExpansionResult report_debug_trace = baseline;
    report_debug_trace.parser_admission_reports.front().debug_trace_available = true;
    refresh_expansion_result(report_debug_trace);
    EXPECT_EQ(report_debug_trace.summary.parser_admission_debug_trace_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_debug_trace));

    frontend::macro::EarlyItemExpansionResult report_source_map = baseline;
    report_source_map.parser_admission_reports.front().source_map_available = true;
    refresh_expansion_result(report_source_map);
    EXPECT_EQ(report_source_map.summary.parser_admission_source_map_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_source_map));

    frontend::macro::EarlyItemExpansionResult report_user_code = baseline;
    report_user_code.parser_admission_reports.front().produced_user_generated_code = true;
    refresh_expansion_result(report_user_code);
    EXPECT_EQ(report_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_user_code));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParserAdmissionReportProjectionContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.parser_admission_report_entries.empty());
    ASSERT_FALSE(baseline.parser_admission_reports.empty());

    frontend::macro::EarlyItemExpansionResult entry_identity = baseline;
    entry_identity.parser_admission_report_entries.front().report_entry_identity =
        query::stable_fingerprint("different parser admission report entry identity");
    refresh_expansion_result(entry_identity);
    EXPECT_NE(entry_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(entry_identity));

    frontend::macro::EarlyItemExpansionResult entry_query = baseline;
    entry_query.parser_admission_report_entries.front().query_projection_name =
        "m21l-parser-admission-report:different";
    refresh_expansion_result(entry_query);
    EXPECT_NE(entry_query.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(entry_query));

    frontend::macro::EarlyItemExpansionResult report_identity = baseline;
    report_identity.parser_admission_reports.front().report_identity =
        query::stable_fingerprint("different parser admission report identity");
    refresh_expansion_result(report_identity);
    EXPECT_NE(report_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(report_identity));

    frontend::macro::EarlyItemExpansionResult report_totals = baseline;
    report_totals.parser_admission_reports.front().blocked_entry_count = 0U;
    refresh_expansion_result(report_totals);
    EXPECT_NE(report_totals.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(report_totals));
}
} // namespace aurex::test
