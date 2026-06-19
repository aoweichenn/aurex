#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_builtin_derive_support.hpp>
#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_contract_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionValidationRejectsBuiltinDeriveM25DryRunSandboxDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.builtin_derive_parser_dry_run_sessions.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_token_cursor_snapshot_proofs.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.size(), 1U);

    const auto expect_invalid = [](const frontend::macro::EarlyItemExpansionResult& result) {
        EXPECT_FALSE(frontend::macro::is_valid(result));
    };

    const frontend::macro::EarlyItemExpansionResult missing_sessions =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_sessions.clear();
        });
    EXPECT_EQ(missing_sessions.summary.builtin_derive_parser_dry_run_session_count, 0U);
    expect_invalid(missing_sessions);

    const frontend::macro::EarlyItemExpansionResult wrong_session_identity =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_sessions.front().dry_run_session_identity =
                query::stable_fingerprint("wrong m25a dry-run session identity");
        });
    expect_invalid(wrong_session_identity);

    const frontend::macro::EarlyItemExpansionResult wrong_session_query =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_sessions.front().session_query_name =
                "m25a-builtin-derive-dry-run-session:wrong";
        });
    expect_invalid(wrong_session_query);

    const frontend::macro::EarlyItemExpansionResult incomplete_session =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_sessions.front().dry_run_session_complete = false;
        });
    EXPECT_EQ(incomplete_session.summary.builtin_derive_parser_dry_run_session_complete_count, 0U);
    expect_invalid(incomplete_session);

    const frontend::macro::EarlyItemExpansionResult session_executed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_sessions.front().dry_run_executed = true;
        });
    EXPECT_EQ(session_executed.summary.builtin_derive_parser_dry_run_session_executed_count, 1U);
    expect_invalid(session_executed);

    const frontend::macro::EarlyItemExpansionResult session_committed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_sessions.front().session_committed = true;
        });
    EXPECT_EQ(session_committed.summary.builtin_derive_parser_dry_run_session_committed_count, 1U);
    EXPECT_EQ(session_committed.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(session_committed);

    const frontend::macro::EarlyItemExpansionResult session_cursor_advanced =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_sessions.front().parser_cursor_advanced = true;
        });
    EXPECT_EQ(session_cursor_advanced.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(session_cursor_advanced);

    const frontend::macro::EarlyItemExpansionResult session_ast_mutated =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_sessions.front().ast_mutated = true;
        });
    EXPECT_EQ(session_ast_mutated.summary.ast_mutation_count, 1U);
    expect_invalid(session_ast_mutated);

    const frontend::macro::EarlyItemExpansionResult session_standard_library =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_sessions.front().standard_library_required = true;
        });
    EXPECT_EQ(session_standard_library.summary.standard_library_required_count, 1U);
    expect_invalid(session_standard_library);

    const frontend::macro::EarlyItemExpansionResult missing_proofs =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_token_cursor_snapshot_proofs.clear();
        });
    EXPECT_EQ(missing_proofs.summary.builtin_derive_token_cursor_snapshot_proof_count, 0U);
    expect_invalid(missing_proofs);

    const frontend::macro::EarlyItemExpansionResult wrong_proof_identity =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_token_cursor_snapshot_proofs.front().cursor_snapshot_identity =
                query::stable_fingerprint("wrong m25b token cursor snapshot proof identity");
        });
    expect_invalid(wrong_proof_identity);

    const frontend::macro::EarlyItemExpansionResult wrong_proof_query =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_token_cursor_snapshot_proofs.front().snapshot_query_name =
                "m25b-builtin-derive-token-cursor-rollback-proof:wrong";
        });
    expect_invalid(wrong_proof_query);

    const frontend::macro::EarlyItemExpansionResult incomplete_proof =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_token_cursor_snapshot_proofs.front().rollback_proof_complete = false;
        });
    EXPECT_EQ(incomplete_proof.summary.builtin_derive_token_cursor_snapshot_proof_complete_count, 0U);
    expect_invalid(incomplete_proof);

    const frontend::macro::EarlyItemExpansionResult proof_cursor_advanced =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_token_cursor_snapshot_proofs.front().parser_cursor_advanced = true;
        });
    EXPECT_EQ(proof_cursor_advanced.summary.builtin_derive_token_cursor_snapshot_proof_cursor_advanced_count,
        1U);
    EXPECT_EQ(proof_cursor_advanced.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(proof_cursor_advanced);

    const frontend::macro::EarlyItemExpansionResult proof_committed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_token_cursor_snapshot_proofs.front().cursor_commit_count = 1U;
        });
    EXPECT_EQ(proof_committed.summary.builtin_derive_token_cursor_snapshot_proof_committed_count, 1U);
    EXPECT_EQ(proof_committed.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(proof_committed);

    const frontend::macro::EarlyItemExpansionResult proof_replay_execution =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_token_cursor_snapshot_proofs.front().replay_execution_enabled = true;
        });
    expect_invalid(proof_replay_execution);

    const frontend::macro::EarlyItemExpansionResult proof_runtime =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_token_cursor_snapshot_proofs.front().runtime_required = true;
        });
    EXPECT_EQ(proof_runtime.summary.runtime_required_count, 1U);
    expect_invalid(proof_runtime);

    const frontend::macro::EarlyItemExpansionResult missing_shadow_closures =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.clear();
        });
    EXPECT_EQ(missing_shadow_closures.summary
                  .builtin_derive_diagnostic_shadow_no_ast_mutation_closure_count,
        0U);
    expect_invalid(missing_shadow_closures);

    const frontend::macro::EarlyItemExpansionResult wrong_shadow_identity =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.front()
                .closure_identity =
                query::stable_fingerprint("wrong m25c diagnostic shadow closure identity");
        });
    expect_invalid(wrong_shadow_identity);

    const frontend::macro::EarlyItemExpansionResult wrong_shadow_query =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.front()
                .closure_query_name =
                "m25c-builtin-derive-diagnostic-shadow-no-ast-mutation:wrong";
        });
    expect_invalid(wrong_shadow_query);

    const frontend::macro::EarlyItemExpansionResult incomplete_shadow_closure =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.front()
                .closure_complete = false;
        });
    EXPECT_EQ(incomplete_shadow_closure.summary
                  .builtin_derive_diagnostic_shadow_no_ast_mutation_complete_count,
        0U);
    expect_invalid(incomplete_shadow_closure);

    const frontend::macro::EarlyItemExpansionResult shadow_executed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.front()
                .executed_shadow_count = 1U;
        });
    EXPECT_EQ(shadow_executed.summary
                  .builtin_derive_diagnostic_shadow_no_ast_mutation_executed_count,
        1U);
    expect_invalid(shadow_executed);

    const frontend::macro::EarlyItemExpansionResult shadow_ast_mutated =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.front()
                .ast_mutated = true;
        });
    EXPECT_EQ(shadow_ast_mutated.summary
                  .builtin_derive_diagnostic_shadow_no_ast_mutation_ast_mutation_count,
        1U);
    EXPECT_EQ(shadow_ast_mutated.summary.ast_mutation_count, 1U);
    expect_invalid(shadow_ast_mutated);

    const frontend::macro::EarlyItemExpansionResult shadow_parser_consumable =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.front()
                .parser_consumable_case_count = 1U;
        });
    EXPECT_EQ(shadow_parser_consumable.summary
                  .builtin_derive_diagnostic_shadow_no_ast_mutation_parser_consumable_count,
        1U);
    EXPECT_EQ(shadow_parser_consumable.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(shadow_parser_consumable);

    const frontend::macro::EarlyItemExpansionResult shadow_external =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.front()
                .external_process_required = true;
        });
    EXPECT_EQ(shadow_external.summary.external_process_required_count, 1U);
    expect_invalid(shadow_external);

    const frontend::macro::EarlyItemExpansionResult shadow_user_code =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.front()
                .produced_user_generated_code = true;
        });
    EXPECT_EQ(shadow_user_code.summary.user_generated_code_count, 1U);
    expect_invalid(shadow_user_code);
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksBuiltinDeriveM25DryRunSandboxFacts)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.builtin_derive_parser_dry_run_sessions.empty());
    ASSERT_FALSE(baseline.builtin_derive_token_cursor_snapshot_proofs.empty());
    ASSERT_FALSE(baseline.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.empty());

    const auto expect_fingerprint_drift =
        [&baseline](const frontend::macro::EarlyItemExpansionResult& result) {
            EXPECT_NE(result.fingerprint, baseline.fingerprint);
            EXPECT_FALSE(frontend::macro::is_valid(result));
        };

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_sessions.front().dry_run_session_identity =
                query::stable_fingerprint("different m25a dry-run session identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_sessions.front().session_query_name =
                "m25a-builtin-derive-dry-run-session:different";
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_sessions.front().parser_cursor_advanced = true;
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_token_cursor_snapshot_proofs.front().cursor_snapshot_identity =
                query::stable_fingerprint("different m25b cursor snapshot identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_token_cursor_snapshot_proofs.front().cursor_commit_count = 1U;
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.front()
                .closure_identity =
                query::stable_fingerprint("different m25c diagnostic shadow closure identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.front()
                .ast_mutation_count = 1U;
        }));
}
} // namespace aurex::test
