#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_contract_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionValidationRejectsParserAdmissionGateDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.parser_admission_gates.size(), 1U);
    ASSERT_FALSE(baseline.generated_part_stubs.empty());

    frontend::macro::EarlyItemExpansionResult missing_gate = baseline;
    missing_gate.parser_admission_gates.clear();
    refresh_expansion_result(missing_gate);
    EXPECT_EQ(missing_gate.summary.parser_admission_gate_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_gate));

    frontend::macro::EarlyItemExpansionResult empty_parse_gate_identity = baseline;
    empty_parse_gate_identity.parser_admission_gates.front().parse_gate_identity = {};
    refresh_expansion_result(empty_parse_gate_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_parse_gate_identity));

    frontend::macro::EarlyItemExpansionResult wrong_parse_gate_identity = baseline;
    wrong_parse_gate_identity.parser_admission_gates.front().parse_gate_identity =
        query::stable_fingerprint("wrong parser gate identity");
    refresh_expansion_result(wrong_parse_gate_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_parse_gate_identity));

    frontend::macro::EarlyItemExpansionResult wrong_generated_buffer_identity = baseline;
    wrong_generated_buffer_identity.parser_admission_gates.front().generated_buffer_identity =
        query::stable_fingerprint("wrong parser gate generated buffer identity");
    refresh_expansion_result(wrong_generated_buffer_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_buffer_identity));

    frontend::macro::EarlyItemExpansionResult wrong_parse_config = baseline;
    wrong_parse_config.parser_admission_gates.front().parse_config_fingerprint =
        query::stable_fingerprint("wrong parser gate parse config");
    refresh_expansion_result(wrong_parse_config);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_parse_config));

    frontend::macro::EarlyItemExpansionResult wrong_token_buffer = baseline;
    wrong_token_buffer.parser_admission_gates.front().token_buffer_identity =
        query::stable_fingerprint("wrong parser gate token buffer");
    refresh_expansion_result(wrong_token_buffer);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_buffer));

    frontend::macro::EarlyItemExpansionResult wrong_materialization = baseline;
    wrong_materialization.parser_admission_gates.front().materialization_identity =
        query::stable_fingerprint("wrong parser gate materialization");
    refresh_expansion_result(wrong_materialization);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_materialization));

    frontend::macro::EarlyItemExpansionResult wrong_source_map = baseline;
    wrong_source_map.parser_admission_gates.front().source_map_identity =
        query::stable_fingerprint("wrong parser gate source map");
    refresh_expansion_result(wrong_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_source_map));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene = baseline;
    wrong_hygiene.parser_admission_gates.front().hygiene_mark =
        query::stable_fingerprint("wrong parser gate hygiene");
    refresh_expansion_result(wrong_hygiene);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene));

    frontend::macro::EarlyItemExpansionResult wrong_stream_name = baseline;
    wrong_stream_name.parser_admission_gates.front().token_stream_name =
        "m21h-token-stream:wrong-parser-gate";
    refresh_expansion_result(wrong_stream_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_stream_name));

    frontend::macro::EarlyItemExpansionResult wrong_policy = baseline;
    wrong_policy.parser_admission_gates.front().parser_gate_policy = "wrong_parser_gate_policy";
    refresh_expansion_result(wrong_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_policy));

    frontend::macro::EarlyItemExpansionResult wrong_blocker = baseline;
    wrong_blocker.parser_admission_gates.front().blocker_reason = "wrong parser gate blocker";
    refresh_expansion_result(wrong_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_blocker));

    frontend::macro::EarlyItemExpansionResult not_compiler_owned = baseline;
    not_compiler_owned.parser_admission_gates.front().compiler_owned = false;
    refresh_expansion_result(not_compiler_owned);
    EXPECT_EQ(not_compiler_owned.summary.compiler_owned_parser_admission_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(not_compiler_owned));

    frontend::macro::EarlyItemExpansionResult missing_records_available = baseline;
    missing_records_available.parser_admission_gates.front().token_records_available = false;
    refresh_expansion_result(missing_records_available);
    EXPECT_EQ(missing_records_available.summary.token_record_available_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_records_available));

    frontend::macro::EarlyItemExpansionResult non_materialized = baseline;
    non_materialized.parser_admission_gates.front().token_buffer_materialized = false;
    refresh_expansion_result(non_materialized);
    EXPECT_FALSE(frontend::macro::is_valid(non_materialized));

    frontend::macro::EarlyItemExpansionResult wrong_token_count = baseline;
    wrong_token_count.parser_admission_gates.front().token_count = 0U;
    refresh_expansion_result(wrong_token_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_count));

    frontend::macro::EarlyItemExpansionResult parser_admitted = baseline;
    parser_admitted.parser_admission_gates.front().parser_admitted = true;
    refresh_expansion_result(parser_admitted);
    EXPECT_EQ(parser_admitted.summary.parser_blocked_token_buffer_count, 0U);
    EXPECT_EQ(parser_admitted.summary.parser_admitted_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_admitted));

    frontend::macro::EarlyItemExpansionResult parse_ready = baseline;
    parse_ready.parser_admission_gates.front().parse_ready = true;
    refresh_expansion_result(parse_ready);
    EXPECT_EQ(parse_ready.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parse_ready));

    frontend::macro::EarlyItemExpansionResult parser_consumable = baseline;
    parser_consumable.parser_admission_gates.front().parser_consumable = true;
    refresh_expansion_result(parser_consumable);
    EXPECT_EQ(parser_consumable.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_consumable));

    frontend::macro::EarlyItemExpansionResult generated_text = baseline;
    generated_text.parser_admission_gates.front().generated_source_text = true;
    refresh_expansion_result(generated_text);
    EXPECT_EQ(generated_text.summary.generated_source_text_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_text));

    frontend::macro::EarlyItemExpansionResult generated_part_parsed = baseline;
    generated_part_parsed.parser_admission_gates.front().generated_part_parsed = true;
    refresh_expansion_result(generated_part_parsed);
    EXPECT_EQ(generated_part_parsed.summary.parsed_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_part_parsed));

    frontend::macro::EarlyItemExpansionResult generated_part_merged = baseline;
    generated_part_merged.parser_admission_gates.front().generated_part_merged = true;
    refresh_expansion_result(generated_part_merged);
    EXPECT_EQ(generated_part_merged.summary.merged_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_part_merged));

    frontend::macro::EarlyItemExpansionResult sema_visible = baseline;
    sema_visible.parser_admission_gates.front().sema_visible = true;
    refresh_expansion_result(sema_visible);
    EXPECT_EQ(sema_visible.summary.sema_visible_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(sema_visible));

    frontend::macro::EarlyItemExpansionResult user_code = baseline;
    user_code.parser_admission_gates.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code);
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));
}


TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParserAdmissionGateContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.parser_admission_gates.empty());

    frontend::macro::EarlyItemExpansionResult parse_gate_identity = baseline;
    parse_gate_identity.parser_admission_gates.front().parse_gate_identity =
        query::stable_fingerprint("different parser gate identity");
    refresh_expansion_result(parse_gate_identity);
    EXPECT_NE(parse_gate_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(parse_gate_identity));

    frontend::macro::EarlyItemExpansionResult parse_config = baseline;
    parse_config.parser_admission_gates.front().parse_config_fingerprint =
        query::stable_fingerprint("different parser gate parse config");
    refresh_expansion_result(parse_config);
    EXPECT_NE(parse_config.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(parse_config));

    frontend::macro::EarlyItemExpansionResult policy = baseline;
    policy.parser_admission_gates.front().parser_gate_policy = "different_parser_gate_policy";
    refresh_expansion_result(policy);
    EXPECT_NE(policy.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(policy));

    frontend::macro::EarlyItemExpansionResult availability = baseline;
    availability.parser_admission_gates.front().token_records_available = false;
    refresh_expansion_result(availability);
    EXPECT_NE(availability.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(availability));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsParserAdmissionDiagnosticProjectionDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.parser_admission_diagnostics.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_projection = baseline;
    missing_projection.parser_admission_diagnostics.clear();
    refresh_expansion_result(missing_projection);
    EXPECT_EQ(missing_projection.summary.parser_admission_diagnostic_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_projection));

    frontend::macro::EarlyItemExpansionResult empty_diagnostic_identity = baseline;
    empty_diagnostic_identity.parser_admission_diagnostics.front().diagnostic_identity = {};
    refresh_expansion_result(empty_diagnostic_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_diagnostic_identity));

    frontend::macro::EarlyItemExpansionResult wrong_diagnostic_identity = baseline;
    wrong_diagnostic_identity.parser_admission_diagnostics.front().diagnostic_identity =
        query::stable_fingerprint("wrong parser admission diagnostic identity");
    refresh_expansion_result(wrong_diagnostic_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_diagnostic_identity));

    frontend::macro::EarlyItemExpansionResult empty_anchor_identity = baseline;
    empty_anchor_identity.parser_admission_diagnostics.front().diagnostic_anchor_identity = {};
    refresh_expansion_result(empty_anchor_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_anchor_identity));

    frontend::macro::EarlyItemExpansionResult wrong_anchor_identity = baseline;
    wrong_anchor_identity.parser_admission_diagnostics.front().diagnostic_anchor_identity =
        query::stable_fingerprint("wrong parser admission diagnostic anchor");
    refresh_expansion_result(wrong_anchor_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_anchor_identity));

    frontend::macro::EarlyItemExpansionResult wrong_parse_gate_identity = baseline;
    wrong_parse_gate_identity.parser_admission_diagnostics.front().parse_gate_identity =
        query::stable_fingerprint("wrong parser admission diagnostic parse gate");
    refresh_expansion_result(wrong_parse_gate_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_parse_gate_identity));

    frontend::macro::EarlyItemExpansionResult wrong_token_plan = baseline;
    wrong_token_plan.parser_admission_diagnostics.front().token_plan_identity =
        query::stable_fingerprint("wrong parser admission diagnostic token plan");
    refresh_expansion_result(wrong_token_plan);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_plan));

    frontend::macro::EarlyItemExpansionResult wrong_token_buffer = baseline;
    wrong_token_buffer.parser_admission_diagnostics.front().token_buffer_identity =
        query::stable_fingerprint("wrong parser admission diagnostic token buffer");
    refresh_expansion_result(wrong_token_buffer);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_buffer));

    frontend::macro::EarlyItemExpansionResult wrong_materialization = baseline;
    wrong_materialization.parser_admission_diagnostics.front().materialization_identity =
        query::stable_fingerprint("wrong parser admission diagnostic materialization");
    refresh_expansion_result(wrong_materialization);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_materialization));

    frontend::macro::EarlyItemExpansionResult wrong_generated_buffer = baseline;
    wrong_generated_buffer.parser_admission_diagnostics.front().generated_buffer_identity =
        query::stable_fingerprint("wrong parser admission diagnostic generated buffer");
    refresh_expansion_result(wrong_generated_buffer);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_buffer));

    frontend::macro::EarlyItemExpansionResult wrong_parse_config = baseline;
    wrong_parse_config.parser_admission_diagnostics.front().parse_config_fingerprint =
        query::stable_fingerprint("wrong parser admission diagnostic parse config");
    refresh_expansion_result(wrong_parse_config);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_parse_config));

    frontend::macro::EarlyItemExpansionResult wrong_source_map = baseline;
    wrong_source_map.parser_admission_diagnostics.front().source_map_identity =
        query::stable_fingerprint("wrong parser admission diagnostic source map");
    refresh_expansion_result(wrong_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_source_map));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene = baseline;
    wrong_hygiene.parser_admission_diagnostics.front().hygiene_mark =
        query::stable_fingerprint("wrong parser admission diagnostic hygiene");
    refresh_expansion_result(wrong_hygiene);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene));

    frontend::macro::EarlyItemExpansionResult wrong_trace = baseline;
    wrong_trace.parser_admission_diagnostics.front().trace_identity =
        query::stable_fingerprint("wrong parser admission diagnostic trace");
    refresh_expansion_result(wrong_trace);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace));

    frontend::macro::EarlyItemExpansionResult wrong_policy = baseline;
    wrong_policy.parser_admission_diagnostics.front().diagnostic_policy = "wrong_diagnostic_policy";
    refresh_expansion_result(wrong_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_policy));

    frontend::macro::EarlyItemExpansionResult wrong_category = baseline;
    wrong_category.parser_admission_diagnostics.front().blocker_category =
        "empty_token_buffer_parser_admission_blocked";
    refresh_expansion_result(wrong_category);
    EXPECT_EQ(wrong_category.summary.derive_parser_admission_diagnostic_count, 0U);
    EXPECT_EQ(wrong_category.summary.empty_parser_admission_diagnostic_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_category));

    frontend::macro::EarlyItemExpansionResult wrong_token_buffer_blocker = baseline;
    wrong_token_buffer_blocker.parser_admission_diagnostics.front().token_buffer_blocker =
        "wrong token buffer blocker";
    refresh_expansion_result(wrong_token_buffer_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_buffer_blocker));

    frontend::macro::EarlyItemExpansionResult wrong_generated_part_blocker = baseline;
    wrong_generated_part_blocker.parser_admission_diagnostics.front().generated_part_parse_blocker =
        "wrong generated part parse blocker";
    refresh_expansion_result(wrong_generated_part_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_part_blocker));

    frontend::macro::EarlyItemExpansionResult wrong_user_message = baseline;
    wrong_user_message.parser_admission_diagnostics.front().user_message =
        "wrong parser admission diagnostic message";
    refresh_expansion_result(wrong_user_message);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_user_message));

    frontend::macro::EarlyItemExpansionResult wrong_debug_projection = baseline;
    wrong_debug_projection.parser_admission_diagnostics.front().debug_projection_name =
        "m21k-parser-admission:wrong";
    refresh_expansion_result(wrong_debug_projection);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_debug_projection));

    frontend::macro::EarlyItemExpansionResult wrong_primary_anchor = baseline;
    wrong_primary_anchor.parser_admission_diagnostics.front().primary_anchor.begin += 1U;
    refresh_expansion_result(wrong_primary_anchor);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_primary_anchor));

    frontend::macro::EarlyItemExpansionResult wrong_token_tree_anchor = baseline;
    wrong_token_tree_anchor.parser_admission_diagnostics.front().token_tree_anchor.end += 1U;
    refresh_expansion_result(wrong_token_tree_anchor);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_tree_anchor));

    frontend::macro::EarlyItemExpansionResult wrong_token_count = baseline;
    wrong_token_count.parser_admission_diagnostics.front().token_count = 0U;
    refresh_expansion_result(wrong_token_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_count));

    frontend::macro::EarlyItemExpansionResult missing_materialization = baseline;
    missing_materialization.parser_admission_diagnostics.front().token_buffer_materialized = false;
    refresh_expansion_result(missing_materialization);
    EXPECT_FALSE(frontend::macro::is_valid(missing_materialization));

    frontend::macro::EarlyItemExpansionResult missing_records = baseline;
    missing_records.parser_admission_diagnostics.front().token_records_available = false;
    refresh_expansion_result(missing_records);
    EXPECT_FALSE(frontend::macro::is_valid(missing_records));

    frontend::macro::EarlyItemExpansionResult parser_admitted = baseline;
    parser_admitted.parser_admission_diagnostics.front().parser_admitted = true;
    refresh_expansion_result(parser_admitted);
    EXPECT_EQ(parser_admitted.summary.parser_admission_diagnostic_blocked_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_admitted));

    frontend::macro::EarlyItemExpansionResult parse_ready = baseline;
    parse_ready.parser_admission_diagnostics.front().parse_ready = true;
    refresh_expansion_result(parse_ready);
    EXPECT_EQ(parse_ready.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parse_ready));

    frontend::macro::EarlyItemExpansionResult parser_consumable = baseline;
    parser_consumable.parser_admission_diagnostics.front().parser_consumable = true;
    refresh_expansion_result(parser_consumable);
    EXPECT_EQ(parser_consumable.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_consumable));

    frontend::macro::EarlyItemExpansionResult generated_part_parsed = baseline;
    generated_part_parsed.parser_admission_diagnostics.front().generated_part_parsed = true;
    refresh_expansion_result(generated_part_parsed);
    EXPECT_EQ(generated_part_parsed.summary.parsed_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_part_parsed));

    frontend::macro::EarlyItemExpansionResult generated_part_merged = baseline;
    generated_part_merged.parser_admission_diagnostics.front().generated_part_merged = true;
    refresh_expansion_result(generated_part_merged);
    EXPECT_EQ(generated_part_merged.summary.merged_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_part_merged));

    frontend::macro::EarlyItemExpansionResult emit_expanded = baseline;
    emit_expanded.parser_admission_diagnostics.front().emit_expanded_available = true;
    refresh_expansion_result(emit_expanded);
    EXPECT_EQ(emit_expanded.summary.emit_expanded_projection_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(emit_expanded));

    frontend::macro::EarlyItemExpansionResult debug_trace = baseline;
    debug_trace.parser_admission_diagnostics.front().debug_trace_available = true;
    refresh_expansion_result(debug_trace);
    EXPECT_EQ(debug_trace.summary.parser_admission_debug_trace_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(debug_trace));

    frontend::macro::EarlyItemExpansionResult source_map = baseline;
    source_map.parser_admission_diagnostics.front().source_map_available = true;
    refresh_expansion_result(source_map);
    EXPECT_EQ(source_map.summary.parser_admission_source_map_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(source_map));

    frontend::macro::EarlyItemExpansionResult user_code = baseline;
    user_code.parser_admission_diagnostics.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code);
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));
}
} // namespace aurex::test
