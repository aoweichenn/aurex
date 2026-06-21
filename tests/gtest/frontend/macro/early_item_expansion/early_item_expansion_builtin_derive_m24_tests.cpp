#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_builtin_derive_support.hpp>
#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_contract_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionValidationRejectsBuiltinDeriveM24DryRunDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.builtin_derive_controlled_dry_run_adapters.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_dry_run_rollback_replays.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_dry_run_negative_matrices.size(), 1U);

    const auto expect_invalid = [](const frontend::macro::EarlyItemExpansionResult& result) {
        EXPECT_FALSE(frontend::macro::is_valid(result));
    };

    const frontend::macro::EarlyItemExpansionResult missing_adapters =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.clear();
        });
    EXPECT_EQ(missing_adapters.summary.builtin_derive_controlled_dry_run_adapter_count, 0U);
    expect_invalid(missing_adapters);

    const frontend::macro::EarlyItemExpansionResult wrong_adapter_identity =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.front().dry_run_adapter_identity =
                query::stable_fingerprint("wrong m24a controlled dry-run adapter identity");
        });
    expect_invalid(wrong_adapter_identity);

    const frontend::macro::EarlyItemExpansionResult wrong_adapter_query =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.front().adapter_query_name =
                "m24a-builtin-derive-controlled-parser-dry-run:wrong";
        });
    expect_invalid(wrong_adapter_query);

    const frontend::macro::EarlyItemExpansionResult incomplete_adapter =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.front().dry_run_adapter_complete = false;
        });
    EXPECT_EQ(incomplete_adapter.summary.builtin_derive_controlled_dry_run_adapter_complete_count, 0U);
    expect_invalid(incomplete_adapter);

    const frontend::macro::EarlyItemExpansionResult adapter_dry_run_executed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.front().dry_run_executed = true;
        });
    EXPECT_EQ(adapter_dry_run_executed.summary.builtin_derive_controlled_dry_run_adapter_executed_count,
        1U);
    expect_invalid(adapter_dry_run_executed);

    const frontend::macro::EarlyItemExpansionResult adapter_parser_consumption =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.front().parser_consumption_enabled = true;
        });
    EXPECT_EQ(adapter_parser_consumption.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(adapter_parser_consumption);

    const frontend::macro::EarlyItemExpansionResult adapter_parser_admitted =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.front().parser_admitted = true;
        });
    EXPECT_EQ(adapter_parser_admitted.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_EQ(adapter_parser_admitted.summary.parser_admitted_token_buffer_count, 1U);
    expect_invalid(adapter_parser_admitted);

    const frontend::macro::EarlyItemExpansionResult adapter_standard_library =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.front().standard_library_required = true;
        });
    EXPECT_EQ(adapter_standard_library.summary.standard_library_required_count, 1U);
    expect_invalid(adapter_standard_library);

    const frontend::macro::EarlyItemExpansionResult adapter_runtime =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.front().runtime_required = true;
        });
    EXPECT_EQ(adapter_runtime.summary.runtime_required_count, 1U);
    expect_invalid(adapter_runtime);

    const frontend::macro::EarlyItemExpansionResult adapter_external =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.front().external_process_required = true;
        });
    EXPECT_EQ(adapter_external.summary.external_process_required_count, 1U);
    expect_invalid(adapter_external);

    const frontend::macro::EarlyItemExpansionResult adapter_user_code =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.front().produced_user_generated_code = true;
        });
    EXPECT_EQ(adapter_user_code.summary.user_generated_code_count, 1U);
    expect_invalid(adapter_user_code);

    const frontend::macro::EarlyItemExpansionResult missing_replays =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_rollback_replays.clear();
        });
    EXPECT_EQ(missing_replays.summary.builtin_derive_dry_run_rollback_replay_count, 0U);
    expect_invalid(missing_replays);

    const frontend::macro::EarlyItemExpansionResult wrong_replay_identity =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_rollback_replays.front().replay_protocol_identity =
                query::stable_fingerprint("wrong m24b rollback replay protocol identity");
        });
    expect_invalid(wrong_replay_identity);

    const frontend::macro::EarlyItemExpansionResult wrong_replay_query =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_rollback_replays.front().replay_query_name =
                "m24b-builtin-derive-dry-run-rollback-replay:wrong";
        });
    expect_invalid(wrong_replay_query);

    const frontend::macro::EarlyItemExpansionResult incomplete_replay =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_rollback_replays.front().replay_protocol_complete = false;
        });
    EXPECT_EQ(incomplete_replay.summary.builtin_derive_dry_run_rollback_replay_complete_count, 0U);
    expect_invalid(incomplete_replay);

    const frontend::macro::EarlyItemExpansionResult replay_execution_enabled =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_rollback_replays.front().replay_execution_enabled = true;
        });
    EXPECT_EQ(replay_execution_enabled.summary.builtin_derive_dry_run_rollback_replay_executed_count,
        1U);
    expect_invalid(replay_execution_enabled);

    const frontend::macro::EarlyItemExpansionResult replay_executed_count =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_rollback_replays.front().executed_replay_count = 1U;
        });
    EXPECT_EQ(replay_executed_count.summary.builtin_derive_dry_run_rollback_replay_executed_count, 1U);
    expect_invalid(replay_executed_count);

    const frontend::macro::EarlyItemExpansionResult replay_dry_run_executed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_rollback_replays.front().dry_run_executed = true;
        });
    EXPECT_EQ(replay_dry_run_executed.summary.builtin_derive_dry_run_rollback_replay_executed_count,
        1U);
    expect_invalid(replay_dry_run_executed);

    const frontend::macro::EarlyItemExpansionResult replay_parser_consumption =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_rollback_replays.front().parser_consumption_enabled = true;
        });
    EXPECT_EQ(replay_parser_consumption.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(replay_parser_consumption);

    const frontend::macro::EarlyItemExpansionResult missing_negative_matrices =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.clear();
        });
    EXPECT_EQ(missing_negative_matrices.summary.builtin_derive_dry_run_negative_matrix_count, 0U);
    expect_invalid(missing_negative_matrices);

    const frontend::macro::EarlyItemExpansionResult wrong_negative_identity =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().negative_matrix_identity =
                query::stable_fingerprint("wrong m24c dry-run negative matrix identity");
        });
    expect_invalid(wrong_negative_identity);

    const frontend::macro::EarlyItemExpansionResult wrong_negative_query =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().matrix_query_name =
                "m24c-builtin-derive-dry-run-negative-matrix:wrong";
        });
    expect_invalid(wrong_negative_query);

    const frontend::macro::EarlyItemExpansionResult incomplete_negative_matrix =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().negative_matrix_complete = false;
        });
    EXPECT_EQ(incomplete_negative_matrix.summary.builtin_derive_dry_run_negative_matrix_complete_count,
        0U);
    expect_invalid(incomplete_negative_matrix);

    const frontend::macro::EarlyItemExpansionResult parser_consumable_negative_case =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().parser_consumable_case_count = 1U;
        });
    EXPECT_EQ(parser_consumable_negative_case.summary.builtin_derive_dry_run_negative_matrix_parser_consumable_count,
        1U);
    EXPECT_EQ(parser_consumable_negative_case.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(parser_consumable_negative_case);

    const frontend::macro::EarlyItemExpansionResult negative_dry_run_executed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().dry_run_executed = true;
        });
    expect_invalid(negative_dry_run_executed);

    const frontend::macro::EarlyItemExpansionResult negative_parser_admitted =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().parser_admitted = true;
        });
    EXPECT_EQ(negative_parser_admitted.summary.builtin_derive_dry_run_negative_matrix_parser_consumable_count,
        1U);
    EXPECT_EQ(negative_parser_admitted.summary.parser_admitted_token_buffer_count, 1U);
    expect_invalid(negative_parser_admitted);

    const frontend::macro::EarlyItemExpansionResult negative_part_parsed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().generated_part_parsed = true;
        });
    EXPECT_EQ(negative_part_parsed.summary.parsed_generated_part_count, 1U);
    expect_invalid(negative_part_parsed);

    const frontend::macro::EarlyItemExpansionResult negative_sema_visible =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().sema_visible = true;
        });
    EXPECT_EQ(negative_sema_visible.summary.sema_visible_generated_part_count, 1U);
    expect_invalid(negative_sema_visible);

    const frontend::macro::EarlyItemExpansionResult negative_standard_library =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().standard_library_required = true;
        });
    EXPECT_EQ(negative_standard_library.summary.standard_library_required_count, 1U);
    expect_invalid(negative_standard_library);

    const frontend::macro::EarlyItemExpansionResult negative_runtime =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().runtime_required = true;
        });
    EXPECT_EQ(negative_runtime.summary.runtime_required_count, 1U);
    expect_invalid(negative_runtime);

    const frontend::macro::EarlyItemExpansionResult negative_external =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().external_process_required = true;
        });
    EXPECT_EQ(negative_external.summary.external_process_required_count, 1U);
    expect_invalid(negative_external);

    const frontend::macro::EarlyItemExpansionResult negative_user_code =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().produced_user_generated_code = true;
        });
    EXPECT_EQ(negative_user_code.summary.user_generated_code_count, 1U);
    expect_invalid(negative_user_code);
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksBuiltinDeriveM24DryRunFacts)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.builtin_derive_controlled_dry_run_adapters.empty());
    ASSERT_FALSE(baseline.builtin_derive_dry_run_rollback_replays.empty());
    ASSERT_FALSE(baseline.builtin_derive_dry_run_negative_matrices.empty());

    const auto expect_fingerprint_drift =
        [&baseline](const frontend::macro::EarlyItemExpansionResult& result) {
            EXPECT_NE(result.fingerprint, baseline.fingerprint);
            EXPECT_FALSE(frontend::macro::is_valid(result));
        };

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.front().dry_run_adapter_identity =
                query::stable_fingerprint("different m24a dry-run adapter identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.front().adapter_query_name =
                "m24a-builtin-derive-controlled-parser-dry-run:different";
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_controlled_dry_run_adapters.front().dry_run_executed = true;
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_rollback_replays.front().replay_protocol_identity =
                query::stable_fingerprint("different m24b replay identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_rollback_replays.front().planned_replay_count = 2U;
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().negative_matrix_identity =
                query::stable_fingerprint("different m24c negative matrix identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_dry_run_negative_matrices.front().parser_consumable_case_count = 1U;
        }));
}
} // namespace aurex::test
