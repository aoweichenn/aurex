#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_builtin_derive_support.hpp>
#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_contract_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionBuiltinDeriveM26DryRunAdmissionFactsAreCheckOnly)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult result = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(result));
    ASSERT_EQ(result.generated_parts.size(), 1U);
    ASSERT_EQ(result.builtin_derive_parser_dry_run_admission_gates.size(), 1U);
    ASSERT_EQ(result.builtin_derive_error_recovery_shadow_diagnostic_gates.size(), 1U);
    ASSERT_EQ(result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.size(), 1U);

    const frontend::macro::GeneratedModulePartPlaceholder& generated = result.generated_parts.front();
    const frontend::macro::BuiltinDeriveParserDryRunSessionBoundary* const session =
        builtin_derive_parser_dry_run_session_for_part(result, generated);
    ASSERT_NE(session, nullptr);
    const frontend::macro::BuiltinDeriveTokenCursorSnapshotRollbackProof* const proof =
        builtin_derive_token_cursor_snapshot_proof_for_part(result, generated);
    ASSERT_NE(proof, nullptr);
    const frontend::macro::BuiltinDeriveDiagnosticShadowNoAstMutationClosure* const shadow_closure =
        builtin_derive_diagnostic_shadow_no_ast_mutation_closure_for_part(result, generated);
    ASSERT_NE(shadow_closure, nullptr);
    const frontend::macro::BuiltinDeriveDryRunRollbackDiagnosticReplay* const replay =
        builtin_derive_dry_run_rollback_replay_for_part(result, generated);
    ASSERT_NE(replay, nullptr);
    const frontend::macro::ParserAdmissionDiagnosticReport* const report =
        parser_admission_report_for_part(result, generated);
    ASSERT_NE(report, nullptr);

    const frontend::macro::BuiltinDeriveParserDryRunAdmissionGate* const admission_gate =
        builtin_derive_parser_dry_run_admission_gate_for_part(result, generated);
    ASSERT_NE(admission_gate, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*admission_gate));
    EXPECT_EQ(admission_gate->admission_query_name,
        "m26a-builtin-derive-parser-dry-run-admission:0:0");
    EXPECT_EQ(admission_gate->dry_run_session_identity, session->dry_run_session_identity);
    EXPECT_EQ(admission_gate->cursor_snapshot_identity, proof->cursor_snapshot_identity);
    EXPECT_EQ(admission_gate->diagnostic_shadow_closure_identity, shadow_closure->closure_identity);
    EXPECT_EQ(admission_gate->generated_buffer_identity,
        result.generated_part_stubs.front().generated_buffer_identity);
    EXPECT_EQ(admission_gate->parse_config_fingerprint,
        result.generated_part_stubs.front().parse_config_fingerprint);
    EXPECT_EQ(admission_gate->dry_run_session_count, 1U);
    EXPECT_EQ(admission_gate->cursor_snapshot_proof_count, 1U);
    EXPECT_EQ(admission_gate->diagnostic_shadow_closure_count, 1U);
    EXPECT_EQ(admission_gate->admission_prerequisite_count, 5U);
    EXPECT_EQ(admission_gate->token_buffer_candidate_count, session->token_buffer_candidate_count);
    EXPECT_EQ(admission_gate->token_record_count, session->token_record_count);
    EXPECT_EQ(admission_gate->dry_run_execution_admitted_count, 0U);
    EXPECT_EQ(admission_gate->parser_consumable_case_count, 0U);
    EXPECT_TRUE(admission_gate->admission_gate_complete);
    EXPECT_FALSE(admission_gate->dry_run_execution_admitted);
    EXPECT_FALSE(admission_gate->dry_run_executed);
    EXPECT_FALSE(admission_gate->diagnostic_shadow_executed);
    EXPECT_FALSE(admission_gate->rollback_execution_enabled);
    EXPECT_FALSE(admission_gate->session_committed);
    EXPECT_FALSE(admission_gate->parser_cursor_advanced);
    EXPECT_FALSE(admission_gate->parser_consumption_enabled);
    EXPECT_FALSE(admission_gate->parser_admitted);
    EXPECT_FALSE(admission_gate->generated_part_parsed);
    EXPECT_FALSE(admission_gate->generated_part_merged);
    EXPECT_FALSE(admission_gate->ast_mutated);
    EXPECT_FALSE(admission_gate->sema_visible);
    EXPECT_FALSE(admission_gate->standard_library_required);
    EXPECT_FALSE(admission_gate->runtime_required);
    EXPECT_FALSE(admission_gate->external_process_required);
    EXPECT_FALSE(admission_gate->produced_user_generated_code);

    const frontend::macro::BuiltinDeriveErrorRecoveryShadowDiagnosticGate* const recovery_gate =
        builtin_derive_error_recovery_shadow_diagnostic_gate_for_part(result, generated);
    ASSERT_NE(recovery_gate, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*recovery_gate));
    EXPECT_EQ(recovery_gate->recovery_query_name,
        "m26b-builtin-derive-error-recovery-shadow-diagnostic:0:0");
    EXPECT_EQ(recovery_gate->dry_run_admission_gate_identity, admission_gate->admission_gate_identity);
    EXPECT_EQ(recovery_gate->diagnostic_shadow_closure_identity, shadow_closure->closure_identity);
    EXPECT_EQ(recovery_gate->rollback_replay_identity, replay->replay_protocol_identity);
    EXPECT_EQ(recovery_gate->parser_report_identity, report->report_identity);
    EXPECT_EQ(recovery_gate->diagnostic_shadow_count, shadow_closure->diagnostic_shadow_count);
    EXPECT_EQ(recovery_gate->report_entry_count, report->entry_count);
    EXPECT_EQ(recovery_gate->planned_recovery_count, report->blocked_entry_count);
    EXPECT_EQ(recovery_gate->executed_recovery_count, 0U);
    EXPECT_EQ(recovery_gate->emitted_diagnostic_count, 0U);
    EXPECT_TRUE(recovery_gate->recovery_shadow_complete);
    EXPECT_FALSE(recovery_gate->recovery_execution_enabled);
    EXPECT_FALSE(recovery_gate->diagnostic_emission_enabled);
    EXPECT_FALSE(recovery_gate->dry_run_execution_admitted);
    EXPECT_FALSE(recovery_gate->dry_run_executed);
    EXPECT_FALSE(recovery_gate->rollback_execution_enabled);
    EXPECT_FALSE(recovery_gate->parser_consumption_enabled);
    EXPECT_FALSE(recovery_gate->parser_admitted);
    EXPECT_FALSE(recovery_gate->ast_mutated);
    EXPECT_FALSE(recovery_gate->standard_library_required);
    EXPECT_FALSE(recovery_gate->runtime_required);
    EXPECT_FALSE(recovery_gate->external_process_required);
    EXPECT_FALSE(recovery_gate->produced_user_generated_code);

    const frontend::macro::BuiltinDeriveCursorRollbackAstMutationVerifierClosure* const verifier =
        builtin_derive_cursor_rollback_ast_mutation_verifier_closure_for_part(result, generated);
    ASSERT_NE(verifier, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*verifier));
    EXPECT_EQ(verifier->verifier_query_name,
        "m26c-builtin-derive-cursor-rollback-ast-verifier:0:0");
    EXPECT_EQ(verifier->dry_run_admission_gate_identity, admission_gate->admission_gate_identity);
    EXPECT_EQ(verifier->recovery_shadow_identity, recovery_gate->recovery_shadow_identity);
    EXPECT_EQ(verifier->cursor_snapshot_identity, proof->cursor_snapshot_identity);
    EXPECT_EQ(verifier->dry_run_session_identity, session->dry_run_session_identity);
    EXPECT_EQ(verifier->diagnostic_shadow_closure_identity, shadow_closure->closure_identity);
    EXPECT_EQ(verifier->cursor_snapshot_count, proof->cursor_snapshot_count);
    EXPECT_EQ(verifier->rollback_proof_count, proof->rollback_proof_count);
    EXPECT_EQ(verifier->recovery_shadow_count, 1U);
    EXPECT_EQ(verifier->ast_baseline_snapshot_count, 1U);
    EXPECT_EQ(verifier->ast_mutation_count, 0U);
    EXPECT_EQ(verifier->cursor_commit_count, 0U);
    EXPECT_EQ(verifier->session_commit_count, 0U);
    EXPECT_EQ(verifier->parser_consumable_case_count, 0U);
    EXPECT_TRUE(verifier->ast_mutation_verifier_complete);
    EXPECT_FALSE(verifier->rollback_execution_enabled);
    EXPECT_FALSE(verifier->recovery_execution_enabled);
    EXPECT_FALSE(verifier->diagnostic_emission_enabled);
    EXPECT_FALSE(verifier->dry_run_execution_admitted);
    EXPECT_FALSE(verifier->dry_run_executed);
    EXPECT_FALSE(verifier->session_committed);
    EXPECT_FALSE(verifier->parser_cursor_advanced);
    EXPECT_FALSE(verifier->parser_consumption_enabled);
    EXPECT_FALSE(verifier->parser_admitted);
    EXPECT_FALSE(verifier->generated_part_parsed);
    EXPECT_FALSE(verifier->generated_part_merged);
    EXPECT_FALSE(verifier->ast_mutated);
    EXPECT_FALSE(verifier->sema_visible);
    EXPECT_FALSE(verifier->standard_library_required);
    EXPECT_FALSE(verifier->runtime_required);
    EXPECT_FALSE(verifier->external_process_required);
    EXPECT_FALSE(verifier->produced_user_generated_code);

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "builtin_derive_parser_dry_run_admission_gate #0");
    expect_contains(dump, "dry_run_execution_admitted=no");
    expect_contains(dump, "builtin derive parser dry-run execution admission remains blocked in M26a");
    expect_contains(dump, "builtin_derive_error_recovery_shadow_diagnostic_gate #0");
    expect_contains(dump, "diagnostic_emission_enabled=no");
    expect_contains(dump, "builtin derive error recovery shadow diagnostics remain non-emitting in M26b");
    expect_contains(dump, "builtin_derive_cursor_rollback_ast_mutation_verifier_closure #0");
    expect_contains(dump, "ast_baseline_snapshots=1");
    expect_contains(dump,
        "builtin derive cursor rollback execution and AST mutation verifier remain check-only in M26c");
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsBuiltinDeriveM26DryRunAdmissionDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.builtin_derive_parser_dry_run_admission_gates.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_error_recovery_shadow_diagnostic_gates.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.size(), 1U);

    const auto expect_invalid = [](const frontend::macro::EarlyItemExpansionResult& result) {
        EXPECT_FALSE(frontend::macro::is_valid(result));
    };

    const frontend::macro::EarlyItemExpansionResult missing_admission_gates =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_admission_gates.clear();
        });
    EXPECT_EQ(missing_admission_gates.summary.builtin_derive_parser_dry_run_admission_gate_count, 0U);
    expect_invalid(missing_admission_gates);

    const frontend::macro::EarlyItemExpansionResult wrong_admission_identity =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_admission_gates.front().admission_gate_identity =
                query::stable_fingerprint("wrong m26a dry-run admission gate identity");
        });
    expect_invalid(wrong_admission_identity);

    const frontend::macro::EarlyItemExpansionResult wrong_admission_query =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_admission_gates.front().admission_query_name =
                "m26a-builtin-derive-parser-dry-run-admission:wrong";
        });
    expect_invalid(wrong_admission_query);

    const frontend::macro::EarlyItemExpansionResult admission_enabled =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_admission_gates.front()
                .dry_run_execution_admitted = true;
        });
    EXPECT_EQ(admission_enabled.summary
                  .builtin_derive_parser_dry_run_admission_gate_execution_admitted_count,
        1U);
    expect_invalid(admission_enabled);

    const frontend::macro::EarlyItemExpansionResult admission_executed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_admission_gates.front().dry_run_executed = true;
        });
    EXPECT_EQ(admission_executed.summary.builtin_derive_parser_dry_run_admission_gate_executed_count,
        1U);
    expect_invalid(admission_executed);

    const frontend::macro::EarlyItemExpansionResult admission_parser_consumable =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_admission_gates.front()
                .parser_consumable_case_count = 1U;
        });
    EXPECT_EQ(admission_parser_consumable.summary
                  .builtin_derive_parser_dry_run_admission_gate_parser_consumable_count,
        1U);
    expect_invalid(admission_parser_consumable);

    const frontend::macro::EarlyItemExpansionResult missing_recovery_gates =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_error_recovery_shadow_diagnostic_gates.clear();
        });
    EXPECT_EQ(missing_recovery_gates.summary.builtin_derive_error_recovery_shadow_diagnostic_gate_count,
        0U);
    expect_invalid(missing_recovery_gates);

    const frontend::macro::EarlyItemExpansionResult wrong_recovery_identity =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_error_recovery_shadow_diagnostic_gates.front()
                .recovery_shadow_identity =
                query::stable_fingerprint("wrong m26b recovery shadow identity");
        });
    expect_invalid(wrong_recovery_identity);

    const frontend::macro::EarlyItemExpansionResult recovery_executed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_error_recovery_shadow_diagnostic_gates.front()
                .executed_recovery_count = 1U;
        });
    EXPECT_EQ(recovery_executed.summary
                  .builtin_derive_error_recovery_shadow_diagnostic_gate_recovery_executed_count,
        1U);
    expect_invalid(recovery_executed);

    const frontend::macro::EarlyItemExpansionResult recovery_emitted =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_error_recovery_shadow_diagnostic_gates.front()
                .emitted_diagnostic_count = 1U;
        });
    EXPECT_EQ(recovery_emitted.summary
                  .builtin_derive_error_recovery_shadow_diagnostic_gate_diagnostic_emitted_count,
        1U);
    expect_invalid(recovery_emitted);

    const frontend::macro::EarlyItemExpansionResult missing_verifiers =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.clear();
        });
    EXPECT_EQ(missing_verifiers.summary
                  .builtin_derive_cursor_rollback_ast_mutation_verifier_closure_count,
        0U);
    expect_invalid(missing_verifiers);

    const frontend::macro::EarlyItemExpansionResult wrong_verifier_identity =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.front()
                .verifier_closure_identity =
                query::stable_fingerprint("wrong m26c verifier closure identity");
        });
    expect_invalid(wrong_verifier_identity);

    const frontend::macro::EarlyItemExpansionResult rollback_executed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.front()
                .rollback_execution_enabled = true;
        });
    EXPECT_EQ(rollback_executed.summary
                  .builtin_derive_cursor_rollback_ast_mutation_verifier_rollback_executed_count,
        1U);
    expect_invalid(rollback_executed);

    const frontend::macro::EarlyItemExpansionResult verifier_ast_mutated =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.front()
                .ast_mutation_count = 1U;
        });
    EXPECT_EQ(verifier_ast_mutated.summary
                  .builtin_derive_cursor_rollback_ast_mutation_verifier_ast_mutation_count,
        1U);
    EXPECT_EQ(verifier_ast_mutated.summary.ast_mutation_count, 1U);
    expect_invalid(verifier_ast_mutated);

    const frontend::macro::EarlyItemExpansionResult verifier_parser_consumable =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.front()
                .parser_consumable_case_count = 1U;
        });
    EXPECT_EQ(verifier_parser_consumable.summary
                  .builtin_derive_cursor_rollback_ast_mutation_verifier_parser_consumable_count,
        1U);
    expect_invalid(verifier_parser_consumable);

    const frontend::macro::EarlyItemExpansionResult verifier_standard_library =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.front()
                .standard_library_required = true;
        });
    EXPECT_EQ(verifier_standard_library.summary.standard_library_required_count, 1U);
    expect_invalid(verifier_standard_library);
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksBuiltinDeriveM26DryRunAdmissionFacts)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.builtin_derive_parser_dry_run_admission_gates.empty());
    ASSERT_FALSE(baseline.builtin_derive_error_recovery_shadow_diagnostic_gates.empty());
    ASSERT_FALSE(baseline.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.empty());

    const auto expect_fingerprint_drift =
        [&baseline](const frontend::macro::EarlyItemExpansionResult& result) {
            EXPECT_NE(result.fingerprint, baseline.fingerprint);
            EXPECT_FALSE(frontend::macro::is_valid(result));
        };

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_admission_gates.front().admission_gate_identity =
                query::stable_fingerprint("different m26a dry-run admission identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_parser_dry_run_admission_gates.front().dry_run_executed = true;
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_error_recovery_shadow_diagnostic_gates.front()
                .recovery_shadow_identity =
                query::stable_fingerprint("different m26b recovery shadow identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_error_recovery_shadow_diagnostic_gates.front()
                .diagnostic_emission_enabled = true;
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.front()
                .verifier_closure_identity =
                query::stable_fingerprint("different m26c verifier closure identity");
        }));
}

} // namespace aurex::test
