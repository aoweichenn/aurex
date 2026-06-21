#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_contract_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionValidationRejectsParserReadinessAndContractDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.parser_readiness_preflight_entries.size(), 2U);
    ASSERT_EQ(baseline.parser_consumption_contract_gates.size(), 1U);
    ASSERT_EQ(baseline.macro_boundary_closure_reports.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_preflight = baseline;
    missing_preflight.parser_readiness_preflight_entries.clear();
    refresh_expansion_result(missing_preflight);
    EXPECT_EQ(missing_preflight.summary.parser_readiness_preflight_entry_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_preflight));

    frontend::macro::EarlyItemExpansionResult wrong_preflight_identity = baseline;
    wrong_preflight_identity.parser_readiness_preflight_entries.front().preflight_identity =
        query::stable_fingerprint("wrong parser readiness preflight identity");
    refresh_expansion_result(wrong_preflight_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_preflight_identity));

    frontend::macro::EarlyItemExpansionResult wrong_preflight_shape = baseline;
    wrong_preflight_shape.parser_readiness_preflight_entries.back().token_stream_shape =
        "empty_token_stream_parser_input_blocked";
    refresh_expansion_result(wrong_preflight_shape);
    EXPECT_EQ(wrong_preflight_shape.summary.parser_readiness_preflight_derive_entry_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_preflight_shape));

    frontend::macro::EarlyItemExpansionResult non_contiguous = baseline;
    non_contiguous.parser_readiness_preflight_entries.back().token_indices_contiguous = false;
    refresh_expansion_result(non_contiguous);
    EXPECT_EQ(non_contiguous.summary.parser_readiness_preflight_contiguous_index_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(non_contiguous));

    frontend::macro::EarlyItemExpansionResult unbalanced_delimiter = baseline;
    unbalanced_delimiter.parser_readiness_preflight_entries.back().delimiter_balanced = false;
    refresh_expansion_result(unbalanced_delimiter);
    EXPECT_EQ(unbalanced_delimiter.summary.parser_readiness_preflight_delimiter_balanced_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(unbalanced_delimiter));

    frontend::macro::EarlyItemExpansionResult uncovered_anchor = baseline;
    uncovered_anchor.parser_readiness_preflight_entries.back().source_anchors_covered = false;
    refresh_expansion_result(uncovered_anchor);
    EXPECT_EQ(uncovered_anchor.summary.parser_readiness_preflight_source_anchor_covered_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(uncovered_anchor));

    frontend::macro::EarlyItemExpansionResult incompatible_parse_config = baseline;
    incompatible_parse_config.parser_readiness_preflight_entries.back().parse_config_compatible = false;
    refresh_expansion_result(incompatible_parse_config);
    EXPECT_EQ(incompatible_parse_config.summary.parser_readiness_preflight_parse_config_compatible_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(incompatible_parse_config));

    frontend::macro::EarlyItemExpansionResult preflight_parser_consumable = baseline;
    preflight_parser_consumable.parser_readiness_preflight_entries.back().parser_consumable = true;
    refresh_expansion_result(preflight_parser_consumable);
    EXPECT_EQ(preflight_parser_consumable.summary.parser_readiness_preflight_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(preflight_parser_consumable));

    frontend::macro::EarlyItemExpansionResult missing_contract = baseline;
    missing_contract.parser_consumption_contract_gates.clear();
    refresh_expansion_result(missing_contract);
    EXPECT_EQ(missing_contract.summary.parser_consumption_contract_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_contract));

    frontend::macro::EarlyItemExpansionResult wrong_contract_identity = baseline;
    wrong_contract_identity.parser_consumption_contract_gates.front().contract_identity =
        query::stable_fingerprint("wrong parser consumption contract identity");
    refresh_expansion_result(wrong_contract_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_contract_identity));

    frontend::macro::EarlyItemExpansionResult wrong_contract_counts = baseline;
    wrong_contract_counts.parser_consumption_contract_gates.front().preflight_entry_count = 1U;
    refresh_expansion_result(wrong_contract_counts);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_contract_counts));

    frontend::macro::EarlyItemExpansionResult contract_not_visible = baseline;
    contract_not_visible.parser_consumption_contract_gates.front().contract_visible = false;
    refresh_expansion_result(contract_not_visible);
    EXPECT_EQ(contract_not_visible.summary.parser_consumption_contract_visible_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_not_visible));

    frontend::macro::EarlyItemExpansionResult contract_parser_consumable = baseline;
    contract_parser_consumable.parser_consumption_contract_gates.front().parser_consumable = true;
    refresh_expansion_result(contract_parser_consumable);
    EXPECT_EQ(contract_parser_consumable.summary.parser_consumption_contract_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_parser_consumable));

    frontend::macro::EarlyItemExpansionResult contract_emit_expanded = baseline;
    contract_emit_expanded.parser_consumption_contract_gates.front().emit_expanded_available = true;
    refresh_expansion_result(contract_emit_expanded);
    EXPECT_EQ(contract_emit_expanded.summary.emit_expanded_projection_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_emit_expanded));

    frontend::macro::EarlyItemExpansionResult contract_source_map = baseline;
    contract_source_map.parser_consumption_contract_gates.front().source_map_available = true;
    refresh_expansion_result(contract_source_map);
    EXPECT_EQ(contract_source_map.summary.parser_admission_source_map_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_source_map));

    frontend::macro::EarlyItemExpansionResult contract_user_code = baseline;
    contract_user_code.parser_consumption_contract_gates.front().produced_user_generated_code = true;
    refresh_expansion_result(contract_user_code);
    EXPECT_EQ(contract_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_user_code));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsMacroBoundaryClosureDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.macro_boundary_closure_reports.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_closure = baseline;
    missing_closure.macro_boundary_closure_reports.clear();
    refresh_expansion_result(missing_closure);
    EXPECT_EQ(missing_closure.summary.macro_boundary_closure_report_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_closure));

    frontend::macro::EarlyItemExpansionResult wrong_identity = baseline;
    wrong_identity.macro_boundary_closure_reports.front().closure_identity =
        query::stable_fingerprint("wrong macro boundary closure identity");
    refresh_expansion_result(wrong_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_identity));

    frontend::macro::EarlyItemExpansionResult wrong_counts = baseline;
    wrong_counts.macro_boundary_closure_reports.front().parser_consumption_contract_gate_count = 0U;
    refresh_expansion_result(wrong_counts);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_counts));

    frontend::macro::EarlyItemExpansionResult not_complete = baseline;
    not_complete.macro_boundary_closure_reports.front().release_closure_complete = false;
    refresh_expansion_result(not_complete);
    EXPECT_EQ(not_complete.summary.macro_boundary_closure_complete_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(not_complete));

    frontend::macro::EarlyItemExpansionResult parser_consumption_enabled = baseline;
    parser_consumption_enabled.macro_boundary_closure_reports.front().parser_consumption_enabled = true;
    refresh_expansion_result(parser_consumption_enabled);
    EXPECT_EQ(parser_consumption_enabled.summary.macro_boundary_closure_parser_consumption_enabled_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_consumption_enabled));

    frontend::macro::EarlyItemExpansionResult standard_library = baseline;
    standard_library.macro_boundary_closure_reports.front().standard_library_required = true;
    refresh_expansion_result(standard_library);
    EXPECT_EQ(standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(standard_library));

    frontend::macro::EarlyItemExpansionResult runtime = baseline;
    runtime.macro_boundary_closure_reports.front().runtime_required = true;
    refresh_expansion_result(runtime);
    EXPECT_EQ(runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(runtime));

    frontend::macro::EarlyItemExpansionResult external_process = baseline;
    external_process.macro_boundary_closure_reports.front().external_process_required = true;
    refresh_expansion_result(external_process);
    EXPECT_EQ(external_process.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(external_process));

    frontend::macro::EarlyItemExpansionResult user_code = baseline;
    user_code.macro_boundary_closure_reports.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code);
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParserReadinessContractAndClosure)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.parser_readiness_preflight_entries.empty());
    ASSERT_FALSE(baseline.parser_consumption_contract_gates.empty());
    ASSERT_FALSE(baseline.macro_boundary_closure_reports.empty());

    frontend::macro::EarlyItemExpansionResult preflight_identity = baseline;
    preflight_identity.parser_readiness_preflight_entries.front().preflight_identity =
        query::stable_fingerprint("different parser readiness preflight identity");
    refresh_expansion_result(preflight_identity);
    EXPECT_NE(preflight_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(preflight_identity));

    frontend::macro::EarlyItemExpansionResult preflight_shape = baseline;
    preflight_shape.parser_readiness_preflight_entries.front().token_stream_shape =
        "empty_token_stream_parser_input_blocked";
    refresh_expansion_result(preflight_shape);
    EXPECT_NE(preflight_shape.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(preflight_shape));

    frontend::macro::EarlyItemExpansionResult contract_identity = baseline;
    contract_identity.parser_consumption_contract_gates.front().contract_identity =
        query::stable_fingerprint("different parser consumption contract identity");
    refresh_expansion_result(contract_identity);
    EXPECT_NE(contract_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(contract_identity));

    frontend::macro::EarlyItemExpansionResult contract_totals = baseline;
    contract_totals.parser_consumption_contract_gates.front().delimiter_balanced_entry_count = 0U;
    refresh_expansion_result(contract_totals);
    EXPECT_NE(contract_totals.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(contract_totals));

    frontend::macro::EarlyItemExpansionResult closure_identity = baseline;
    closure_identity.macro_boundary_closure_reports.front().closure_identity =
        query::stable_fingerprint("different macro boundary closure identity");
    refresh_expansion_result(closure_identity);
    EXPECT_NE(closure_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(closure_identity));
}

} // namespace aurex::test
