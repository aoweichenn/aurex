#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_noop_fixture_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionNoopCollectsBuiltinDeriveDryRunContracts)
{
    const NoopAttributeExpansionFixture fixture = make_noop_attribute_expansion_fixture();
    const frontend::macro::EarlyItemExpansionResult& result = fixture.result;
    const NoopAttributeExpansionView view = inspect_noop_attribute_expansion(fixture);
    const auto* const generated_ptr = view.generated;
    const auto* const verification_closure = view.verification_closure;
    const auto* const admission_protocol = view.admission_protocol;
    const auto* const checkpoint_protocol = view.checkpoint_protocol;
    const auto* const rollback_gate = view.rollback_gate;
    ASSERT_NE(generated_ptr, nullptr);
    ASSERT_NE(verification_closure, nullptr);
    ASSERT_NE(admission_protocol, nullptr);
    ASSERT_NE(checkpoint_protocol, nullptr);
    ASSERT_NE(rollback_gate, nullptr);
    const auto& generated = *generated_ptr;

    const frontend::macro::BuiltinDeriveControlledParserDryRunAdapter* const dry_run_adapter =
        builtin_derive_controlled_dry_run_adapter_for_part(result, generated);
    ASSERT_NE(dry_run_adapter, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*dry_run_adapter));
    EXPECT_EQ(dry_run_adapter->module.value, generated.module.value);
    EXPECT_EQ(dry_run_adapter->source_part_index, generated.source_part_index);
    EXPECT_EQ(dry_run_adapter->attached_part, generated.source_part);
    EXPECT_EQ(dry_run_adapter->generated_part, generated.generated_part);
    EXPECT_EQ(dry_run_adapter->verification_closure_identity,
        verification_closure->verification_closure_identity);
    EXPECT_EQ(dry_run_adapter->admission_protocol_identity,
        admission_protocol->admission_protocol_identity);
    EXPECT_EQ(dry_run_adapter->checkpoint_protocol_identity,
        checkpoint_protocol->checkpoint_protocol_identity);
    EXPECT_GT(dry_run_adapter->dry_run_adapter_identity.byte_count, 0U);
    EXPECT_NE(dry_run_adapter->dry_run_adapter_identity,
        verification_closure->verification_closure_identity);
    EXPECT_EQ(dry_run_adapter->adapter_policy,
        "builtin_derive_controlled_parser_dry_run_adapter_v1");
    EXPECT_EQ(dry_run_adapter->adapter_query_name,
        "m24a-builtin-derive-controlled-parser-dry-run:0:0");
    expect_contains(dry_run_adapter->blocked_reason, "execution-blocked in M24a");
    EXPECT_EQ(dry_run_adapter->token_record_count, checkpoint_protocol->token_record_count);
    EXPECT_EQ(dry_run_adapter->diagnostic_anchor_count,
        checkpoint_protocol->diagnostic_anchor_count);
    EXPECT_EQ(dry_run_adapter->prerequisite_count, 5U);
    EXPECT_TRUE(dry_run_adapter->verification_closure_available);
    EXPECT_TRUE(dry_run_adapter->admission_protocol_available);
    EXPECT_TRUE(dry_run_adapter->checkpoint_protocol_available);
    EXPECT_TRUE(dry_run_adapter->compiler_owned_tokens_available);
    EXPECT_TRUE(dry_run_adapter->diagnostic_replay_available);
    EXPECT_TRUE(dry_run_adapter->dry_run_adapter_complete);
    EXPECT_FALSE(dry_run_adapter->dry_run_executed);
    EXPECT_FALSE(dry_run_adapter->parser_consumption_enabled);
    EXPECT_FALSE(dry_run_adapter->parser_admitted);
    EXPECT_FALSE(dry_run_adapter->generated_part_parsed);
    EXPECT_FALSE(dry_run_adapter->generated_part_merged);
    EXPECT_FALSE(dry_run_adapter->sema_visible);
    EXPECT_FALSE(dry_run_adapter->emit_expanded_available);
    EXPECT_FALSE(dry_run_adapter->debug_trace_available);
    EXPECT_FALSE(dry_run_adapter->source_map_available);
    EXPECT_FALSE(dry_run_adapter->standard_library_required);
    EXPECT_FALSE(dry_run_adapter->runtime_required);
    EXPECT_FALSE(dry_run_adapter->external_process_required);
    EXPECT_FALSE(dry_run_adapter->produced_user_generated_code);
    EXPECT_TRUE(dry_run_adapter->adapter_visible);
    EXPECT_TRUE(dry_run_adapter->query_reusable);

    const frontend::macro::BuiltinDeriveDryRunRollbackDiagnosticReplay* const dry_run_replay =
        builtin_derive_dry_run_rollback_replay_for_part(result, generated);
    ASSERT_NE(dry_run_replay, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*dry_run_replay));
    EXPECT_EQ(dry_run_replay->module.value, generated.module.value);
    EXPECT_EQ(dry_run_replay->source_part_index, generated.source_part_index);
    EXPECT_EQ(dry_run_replay->attached_part, generated.source_part);
    EXPECT_EQ(dry_run_replay->generated_part, generated.generated_part);
    EXPECT_EQ(dry_run_replay->dry_run_adapter_identity,
        dry_run_adapter->dry_run_adapter_identity);
    EXPECT_EQ(dry_run_replay->checkpoint_protocol_identity,
        checkpoint_protocol->checkpoint_protocol_identity);
    EXPECT_EQ(dry_run_replay->rollback_gate_identity, rollback_gate->rollback_gate_identity);
    EXPECT_GT(dry_run_replay->replay_protocol_identity.byte_count, 0U);
    EXPECT_NE(dry_run_replay->replay_protocol_identity,
        dry_run_adapter->dry_run_adapter_identity);
    EXPECT_EQ(dry_run_replay->replay_policy,
        "builtin_derive_dry_run_rollback_diagnostic_replay_v1");
    EXPECT_EQ(dry_run_replay->replay_query_name,
        "m24b-builtin-derive-dry-run-rollback-replay:0:0");
    expect_contains(dry_run_replay->blocked_reason, "execution-blocked in M24b");
    EXPECT_EQ(dry_run_replay->diagnostic_anchor_count,
        dry_run_adapter->diagnostic_anchor_count);
    EXPECT_EQ(dry_run_replay->report_entry_count,
        rollback_gate->diagnostic_report_entry_count);
    EXPECT_EQ(dry_run_replay->planned_replay_count,
        dry_run_replay->diagnostic_anchor_count);
    EXPECT_EQ(dry_run_replay->executed_replay_count, 0U);
    EXPECT_TRUE(dry_run_replay->dry_run_adapter_available);
    EXPECT_TRUE(dry_run_replay->checkpoint_protocol_available);
    EXPECT_TRUE(dry_run_replay->rollback_gate_available);
    EXPECT_TRUE(dry_run_replay->diagnostic_replay_plan_available);
    EXPECT_TRUE(dry_run_replay->replay_protocol_complete);
    EXPECT_FALSE(dry_run_replay->replay_execution_enabled);
    EXPECT_FALSE(dry_run_replay->dry_run_executed);
    EXPECT_FALSE(dry_run_replay->parser_consumption_enabled);
    EXPECT_FALSE(dry_run_replay->generated_part_parsed);
    EXPECT_FALSE(dry_run_replay->generated_part_merged);
    EXPECT_FALSE(dry_run_replay->sema_visible);
    EXPECT_FALSE(dry_run_replay->emit_expanded_available);
    EXPECT_FALSE(dry_run_replay->debug_trace_available);
    EXPECT_FALSE(dry_run_replay->source_map_available);
    EXPECT_FALSE(dry_run_replay->standard_library_required);
    EXPECT_FALSE(dry_run_replay->runtime_required);
    EXPECT_FALSE(dry_run_replay->external_process_required);
    EXPECT_FALSE(dry_run_replay->produced_user_generated_code);
    EXPECT_TRUE(dry_run_replay->replay_visible);
    EXPECT_TRUE(dry_run_replay->query_reusable);

    const frontend::macro::BuiltinDeriveDryRunNegativeMatrixClosure* const dry_run_matrix =
        builtin_derive_dry_run_negative_matrix_for_part(result, generated);
    ASSERT_NE(dry_run_matrix, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*dry_run_matrix));
    EXPECT_EQ(dry_run_matrix->module.value, generated.module.value);
    EXPECT_EQ(dry_run_matrix->source_part_index, generated.source_part_index);
    EXPECT_EQ(dry_run_matrix->attached_part, generated.source_part);
    EXPECT_EQ(dry_run_matrix->generated_part, generated.generated_part);
    EXPECT_EQ(dry_run_matrix->dry_run_adapter_identity,
        dry_run_adapter->dry_run_adapter_identity);
    EXPECT_EQ(dry_run_matrix->rollback_replay_identity,
        dry_run_replay->replay_protocol_identity);
    EXPECT_EQ(dry_run_matrix->verification_closure_identity,
        verification_closure->verification_closure_identity);
    EXPECT_GT(dry_run_matrix->negative_matrix_identity.byte_count, 0U);
    EXPECT_NE(dry_run_matrix->negative_matrix_identity,
        dry_run_replay->replay_protocol_identity);
    EXPECT_EQ(dry_run_matrix->matrix_policy,
        "builtin_derive_dry_run_negative_matrix_closure_v1");
    EXPECT_EQ(dry_run_matrix->matrix_query_name,
        "m24c-builtin-derive-dry-run-negative-matrix:0:0");
    expect_contains(dry_run_matrix->blocked_reason, "blocked in M24c");
    EXPECT_EQ(dry_run_matrix->dry_run_adapter_count, 1U);
    EXPECT_EQ(dry_run_matrix->rollback_replay_count, 1U);
    EXPECT_EQ(dry_run_matrix->verification_closure_count, 1U);
    EXPECT_EQ(dry_run_matrix->negative_case_count, 8U);
    EXPECT_EQ(dry_run_matrix->parser_consumable_case_count, 0U);
    EXPECT_TRUE(dry_run_matrix->dry_run_adapter_available);
    EXPECT_TRUE(dry_run_matrix->rollback_replay_available);
    EXPECT_TRUE(dry_run_matrix->verification_closure_available);
    EXPECT_TRUE(dry_run_matrix->negative_matrix_complete);
    EXPECT_FALSE(dry_run_matrix->dry_run_executed);
    EXPECT_FALSE(dry_run_matrix->parser_consumption_enabled);
    EXPECT_FALSE(dry_run_matrix->parser_admitted);
    EXPECT_FALSE(dry_run_matrix->generated_part_parsed);
    EXPECT_FALSE(dry_run_matrix->generated_part_merged);
    EXPECT_FALSE(dry_run_matrix->sema_visible);
    EXPECT_FALSE(dry_run_matrix->emit_expanded_available);
    EXPECT_FALSE(dry_run_matrix->debug_trace_available);
    EXPECT_FALSE(dry_run_matrix->source_map_available);
    EXPECT_FALSE(dry_run_matrix->standard_library_required);
    EXPECT_FALSE(dry_run_matrix->runtime_required);
    EXPECT_FALSE(dry_run_matrix->external_process_required);
    EXPECT_FALSE(dry_run_matrix->produced_user_generated_code);
    EXPECT_TRUE(dry_run_matrix->matrix_visible);
    EXPECT_TRUE(dry_run_matrix->query_reusable);

    const frontend::macro::BuiltinDeriveParserDryRunSessionBoundary* const dry_run_session =
        builtin_derive_parser_dry_run_session_for_part(result, generated);
    ASSERT_NE(dry_run_session, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*dry_run_session));
    EXPECT_EQ(dry_run_session->module.value, generated.module.value);
    EXPECT_EQ(dry_run_session->source_part_index, generated.source_part_index);
    EXPECT_EQ(dry_run_session->attached_part, generated.source_part);
    EXPECT_EQ(dry_run_session->generated_part, generated.generated_part);
    EXPECT_EQ(dry_run_session->dry_run_adapter_identity,
        dry_run_adapter->dry_run_adapter_identity);
    EXPECT_EQ(dry_run_session->negative_matrix_identity,
        dry_run_matrix->negative_matrix_identity);
    EXPECT_EQ(dry_run_session->generated_buffer_identity,
        result.generated_part_stubs.front().generated_buffer_identity);
    EXPECT_EQ(dry_run_session->parse_config_fingerprint,
        result.generated_part_stubs.front().parse_config_fingerprint);
    EXPECT_GT(dry_run_session->dry_run_session_identity.byte_count, 0U);
    EXPECT_NE(dry_run_session->dry_run_session_identity,
        dry_run_adapter->dry_run_adapter_identity);
    EXPECT_EQ(dry_run_session->session_policy,
        "builtin_derive_parser_dry_run_session_boundary_v1");
    EXPECT_EQ(dry_run_session->session_query_name,
        "m25a-builtin-derive-dry-run-session:0:0");
    expect_contains(dry_run_session->blocked_reason, "check-only and uncommitted in M25a");
    EXPECT_EQ(dry_run_session->token_buffer_candidate_count, 1U);
    EXPECT_EQ(dry_run_session->token_record_count, dry_run_adapter->token_record_count);
    EXPECT_EQ(dry_run_session->diagnostic_anchor_count,
        dry_run_adapter->diagnostic_anchor_count);
    EXPECT_EQ(dry_run_session->parser_state_snapshot_count, 1U);
    EXPECT_EQ(dry_run_session->committed_parse_count, 0U);
    EXPECT_TRUE(dry_run_session->dry_run_adapter_available);
    EXPECT_TRUE(dry_run_session->negative_matrix_available);
    EXPECT_TRUE(dry_run_session->compiler_owned_token_stream_available);
    EXPECT_TRUE(dry_run_session->sandbox_available);
    EXPECT_TRUE(dry_run_session->check_only);
    EXPECT_TRUE(dry_run_session->dry_run_session_complete);
    EXPECT_FALSE(dry_run_session->dry_run_executed);
    EXPECT_FALSE(dry_run_session->session_committed);
    EXPECT_FALSE(dry_run_session->parser_consumption_enabled);
    EXPECT_FALSE(dry_run_session->parser_admitted);
    EXPECT_FALSE(dry_run_session->parser_cursor_advanced);
    EXPECT_FALSE(dry_run_session->generated_part_parsed);
    EXPECT_FALSE(dry_run_session->generated_part_merged);
    EXPECT_FALSE(dry_run_session->ast_mutated);
    EXPECT_FALSE(dry_run_session->sema_visible);
    EXPECT_FALSE(dry_run_session->emit_expanded_available);
    EXPECT_FALSE(dry_run_session->debug_trace_available);
    EXPECT_FALSE(dry_run_session->source_map_available);
    EXPECT_FALSE(dry_run_session->standard_library_required);
    EXPECT_FALSE(dry_run_session->runtime_required);
    EXPECT_FALSE(dry_run_session->external_process_required);
    EXPECT_FALSE(dry_run_session->produced_user_generated_code);
    EXPECT_TRUE(dry_run_session->session_visible);
    EXPECT_TRUE(dry_run_session->query_reusable);

    const frontend::macro::BuiltinDeriveTokenCursorSnapshotRollbackProof* const cursor_proof =
        builtin_derive_token_cursor_snapshot_proof_for_part(result, generated);
    ASSERT_NE(cursor_proof, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*cursor_proof));
    EXPECT_EQ(cursor_proof->module.value, generated.module.value);
    EXPECT_EQ(cursor_proof->source_part_index, generated.source_part_index);
    EXPECT_EQ(cursor_proof->attached_part, generated.source_part);
    EXPECT_EQ(cursor_proof->generated_part, generated.generated_part);
    EXPECT_EQ(cursor_proof->dry_run_session_identity,
        dry_run_session->dry_run_session_identity);
    EXPECT_EQ(cursor_proof->checkpoint_protocol_identity,
        checkpoint_protocol->checkpoint_protocol_identity);
    EXPECT_EQ(cursor_proof->rollback_replay_identity,
        dry_run_replay->replay_protocol_identity);
    EXPECT_GT(cursor_proof->cursor_snapshot_identity.byte_count, 0U);
    EXPECT_NE(cursor_proof->cursor_snapshot_identity,
        dry_run_session->dry_run_session_identity);
    EXPECT_EQ(cursor_proof->snapshot_policy,
        "builtin_derive_token_cursor_snapshot_rollback_proof_v1");
    EXPECT_EQ(cursor_proof->snapshot_query_name,
        "m25b-builtin-derive-token-cursor-rollback-proof:0:0");
    expect_contains(cursor_proof->blocked_reason, "parser cursor unadvanced in M25b");
    EXPECT_EQ(cursor_proof->token_record_count, dry_run_session->token_record_count);
    EXPECT_EQ(cursor_proof->checkpoint_count, checkpoint_protocol->checkpoint_count);
    EXPECT_EQ(cursor_proof->cursor_snapshot_count, checkpoint_protocol->checkpoint_count);
    EXPECT_EQ(cursor_proof->parser_state_snapshot_count, checkpoint_protocol->checkpoint_count);
    EXPECT_EQ(cursor_proof->rollback_proof_count, checkpoint_protocol->rollback_plan_count);
    EXPECT_EQ(cursor_proof->cursor_commit_count, 0U);
    EXPECT_TRUE(cursor_proof->dry_run_session_available);
    EXPECT_TRUE(cursor_proof->checkpoint_protocol_available);
    EXPECT_TRUE(cursor_proof->rollback_replay_available);
    EXPECT_TRUE(cursor_proof->token_cursor_snapshot_available);
    EXPECT_TRUE(cursor_proof->parser_state_snapshot_available);
    EXPECT_TRUE(cursor_proof->rollback_proof_complete);
    EXPECT_FALSE(cursor_proof->replay_execution_enabled);
    EXPECT_FALSE(cursor_proof->rollback_execution_enabled);
    EXPECT_FALSE(cursor_proof->dry_run_executed);
    EXPECT_FALSE(cursor_proof->parser_cursor_advanced);
    EXPECT_FALSE(cursor_proof->session_committed);
    EXPECT_FALSE(cursor_proof->parser_consumption_enabled);
    EXPECT_FALSE(cursor_proof->parser_admitted);
    EXPECT_FALSE(cursor_proof->generated_part_parsed);
    EXPECT_FALSE(cursor_proof->generated_part_merged);
    EXPECT_FALSE(cursor_proof->ast_mutated);
    EXPECT_FALSE(cursor_proof->sema_visible);
    EXPECT_FALSE(cursor_proof->standard_library_required);
    EXPECT_FALSE(cursor_proof->runtime_required);
    EXPECT_FALSE(cursor_proof->external_process_required);
    EXPECT_FALSE(cursor_proof->produced_user_generated_code);
    EXPECT_TRUE(cursor_proof->proof_visible);
    EXPECT_TRUE(cursor_proof->query_reusable);

    const frontend::macro::BuiltinDeriveDiagnosticShadowNoAstMutationClosure* const shadow_closure =
        builtin_derive_diagnostic_shadow_no_ast_mutation_closure_for_part(result, generated);
    ASSERT_NE(shadow_closure, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*shadow_closure));
    EXPECT_EQ(shadow_closure->module.value, generated.module.value);
    EXPECT_EQ(shadow_closure->source_part_index, generated.source_part_index);
    EXPECT_EQ(shadow_closure->attached_part, generated.source_part);
    EXPECT_EQ(shadow_closure->generated_part, generated.generated_part);
    EXPECT_EQ(shadow_closure->dry_run_session_identity,
        dry_run_session->dry_run_session_identity);
    EXPECT_EQ(shadow_closure->cursor_snapshot_identity,
        cursor_proof->cursor_snapshot_identity);
    EXPECT_EQ(shadow_closure->rollback_replay_identity,
        dry_run_replay->replay_protocol_identity);
    EXPECT_EQ(shadow_closure->negative_matrix_identity,
        dry_run_matrix->negative_matrix_identity);
    EXPECT_GT(shadow_closure->closure_identity.byte_count, 0U);
    EXPECT_NE(shadow_closure->closure_identity, cursor_proof->cursor_snapshot_identity);
    EXPECT_EQ(shadow_closure->closure_policy,
        "builtin_derive_diagnostic_shadow_no_ast_mutation_closure_v1");
    EXPECT_EQ(shadow_closure->closure_query_name,
        "m25c-builtin-derive-diagnostic-shadow-no-ast-mutation:0:0");
    expect_contains(shadow_closure->blocked_reason, "no-AST-mutation in M25c");
    EXPECT_EQ(shadow_closure->dry_run_session_count, 1U);
    EXPECT_EQ(shadow_closure->cursor_snapshot_proof_count, 1U);
    EXPECT_EQ(shadow_closure->rollback_replay_count, 1U);
    EXPECT_EQ(shadow_closure->negative_matrix_count, 1U);
    EXPECT_EQ(shadow_closure->diagnostic_shadow_count,
        dry_run_replay->planned_replay_count);
    EXPECT_EQ(shadow_closure->executed_shadow_count, 0U);
    EXPECT_EQ(shadow_closure->ast_mutation_count, 0U);
    EXPECT_EQ(shadow_closure->parser_consumable_case_count, 0U);
    EXPECT_TRUE(shadow_closure->dry_run_session_available);
    EXPECT_TRUE(shadow_closure->cursor_snapshot_proof_available);
    EXPECT_TRUE(shadow_closure->rollback_replay_available);
    EXPECT_TRUE(shadow_closure->negative_matrix_available);
    EXPECT_TRUE(shadow_closure->diagnostic_shadow_available);
    EXPECT_TRUE(shadow_closure->no_ast_mutation_verified);
    EXPECT_TRUE(shadow_closure->closure_complete);
    EXPECT_FALSE(shadow_closure->dry_run_executed);
    EXPECT_FALSE(shadow_closure->replay_execution_enabled);
    EXPECT_FALSE(shadow_closure->session_committed);
    EXPECT_FALSE(shadow_closure->parser_cursor_advanced);
    EXPECT_FALSE(shadow_closure->parser_consumption_enabled);
    EXPECT_FALSE(shadow_closure->parser_admitted);
    EXPECT_FALSE(shadow_closure->generated_part_parsed);
    EXPECT_FALSE(shadow_closure->generated_part_merged);
    EXPECT_FALSE(shadow_closure->ast_mutated);
    EXPECT_FALSE(shadow_closure->sema_visible);
    EXPECT_FALSE(shadow_closure->emit_expanded_available);
    EXPECT_FALSE(shadow_closure->debug_trace_available);
    EXPECT_FALSE(shadow_closure->source_map_available);
    EXPECT_FALSE(shadow_closure->standard_library_required);
    EXPECT_FALSE(shadow_closure->runtime_required);
    EXPECT_FALSE(shadow_closure->external_process_required);
    EXPECT_FALSE(shadow_closure->produced_user_generated_code);
    EXPECT_TRUE(shadow_closure->closure_visible);
    EXPECT_TRUE(shadow_closure->query_reusable);
}

} // namespace aurex::test
