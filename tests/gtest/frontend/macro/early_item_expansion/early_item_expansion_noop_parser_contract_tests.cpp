#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_noop_fixture_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionNoopCollectsParserAdmissionContracts)
{
    const NoopAttributeExpansionFixture fixture = make_noop_attribute_expansion_fixture();
    const frontend::macro::EarlyItemExpansionResult& result = fixture.result;
    const NoopAttributeExpansionView view = inspect_noop_attribute_expansion(fixture);
    const auto* const builder = view.builder;
    const auto* const derive = view.derive;
    const auto* const generated_ptr = view.generated;
    const auto* const stub_ptr = view.stub;
    const auto* const builder_buffer = view.builder_buffer;
    const auto* const derive_buffer = view.derive_buffer;
    const auto* const builder_trace = view.builder_trace;
    ASSERT_NE(builder, nullptr);
    ASSERT_NE(derive, nullptr);
    ASSERT_NE(generated_ptr, nullptr);
    ASSERT_NE(stub_ptr, nullptr);
    ASSERT_NE(builder_buffer, nullptr);
    ASSERT_NE(derive_buffer, nullptr);
    ASSERT_NE(builder_trace, nullptr);
    const auto& generated = *generated_ptr;
    const auto& stub = *stub_ptr;

    ASSERT_EQ(result.parser_admission_gates.size(), 2U);
    ASSERT_EQ(result.parser_admission_diagnostics.size(), 2U);
    ASSERT_EQ(result.parser_admission_report_entries.size(), 2U);
    ASSERT_EQ(result.parser_admission_reports.size(), 1U);
    ASSERT_EQ(result.builtin_derive_expansion_admissions.size(), 2U);
    ASSERT_EQ(result.builtin_derive_semantic_plans.size(), 2U);
    ASSERT_EQ(result.builtin_derive_parser_release_gates.size(), 1U);
    ASSERT_EQ(result.builtin_derive_release_hardening_matrices.size(), 1U);
    ASSERT_EQ(result.builtin_derive_debug_dump_contracts.size(), 1U);
    ASSERT_EQ(result.builtin_derive_rollback_diagnostic_gates.size(), 1U);
    ASSERT_EQ(result.builtin_derive_parser_consumption_admission_protocols.size(), 1U);
    ASSERT_EQ(result.builtin_derive_checkpoint_rollback_protocols.size(), 1U);
    ASSERT_EQ(result.builtin_derive_preconsumption_verification_closures.size(), 1U);
    ASSERT_EQ(result.builtin_derive_controlled_dry_run_adapters.size(), 1U);
    ASSERT_EQ(result.builtin_derive_dry_run_rollback_replays.size(), 1U);
    ASSERT_EQ(result.builtin_derive_dry_run_negative_matrices.size(), 1U);
    ASSERT_EQ(result.builtin_derive_parser_dry_run_sessions.size(), 1U);
    ASSERT_EQ(result.builtin_derive_token_cursor_snapshot_proofs.size(), 1U);
    ASSERT_EQ(result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.size(), 1U);
    ASSERT_EQ(result.builtin_derive_parser_dry_run_admission_gates.size(), 1U);
    ASSERT_EQ(result.builtin_derive_error_recovery_shadow_diagnostic_gates.size(), 1U);
    ASSERT_EQ(result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.size(), 1U);
    ASSERT_EQ(result.parser_readiness_preflight_entries.size(), 2U);
    ASSERT_EQ(result.parser_consumption_contract_gates.size(), 1U);
    ASSERT_EQ(result.macro_boundary_closure_reports.size(), 1U);
    const frontend::macro::GeneratedTokenParserAdmissionGateStub* const builder_gate =
        parser_admission_gate_for_input(result, *builder);
    ASSERT_NE(builder_gate, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_gate));
    EXPECT_EQ(builder_gate->part_index, builder->part_index);
    EXPECT_EQ(builder_gate->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_gate->attached_part, builder->attached_part);
    EXPECT_EQ(builder_gate->generated_part, generated.generated_part);
    EXPECT_EQ(builder_gate->token_plan_identity, builder_buffer->token_plan_identity);
    EXPECT_EQ(builder_gate->token_buffer_identity, builder_buffer->token_buffer_identity);
    EXPECT_EQ(builder_gate->materialization_identity, builder_buffer->materialization_identity);
    EXPECT_EQ(builder_gate->source_map_identity, builder_buffer->source_map_identity);
    EXPECT_EQ(builder_gate->hygiene_mark, builder_buffer->hygiene_mark);
    EXPECT_EQ(builder_gate->generated_buffer_identity, result.generated_part_stubs.front().generated_buffer_identity);
    EXPECT_EQ(builder_gate->parse_config_fingerprint, result.generated_part_stubs.front().parse_config_fingerprint);
    EXPECT_GT(builder_gate->parse_gate_identity.byte_count, 0U);
    EXPECT_EQ(builder_gate->token_stream_name, builder_buffer->token_stream_name);
    EXPECT_EQ(builder_gate->parser_gate_policy,
        "compiler_owned_generated_token_parser_admission_gate_v1");
    expect_contains(builder_gate->blocker_reason,
        "empty or non-derive generated token buffer parser admission remains blocked in M21j");
    EXPECT_EQ(builder_gate->token_count, 0U);
    EXPECT_TRUE(builder_gate->compiler_owned);
    EXPECT_FALSE(builder_gate->token_buffer_materialized);
    EXPECT_FALSE(builder_gate->token_records_available);
    EXPECT_FALSE(builder_gate->parser_admitted);
    EXPECT_FALSE(builder_gate->parse_ready);
    EXPECT_FALSE(builder_gate->parser_consumable);
    EXPECT_FALSE(builder_gate->generated_source_text);
    EXPECT_FALSE(builder_gate->generated_part_parsed);
    EXPECT_FALSE(builder_gate->generated_part_merged);
    EXPECT_FALSE(builder_gate->sema_visible);
    EXPECT_FALSE(builder_gate->produced_user_generated_code);

    const frontend::macro::GeneratedTokenParserAdmissionGateStub* const derive_gate =
        parser_admission_gate_for_input(result, *derive);
    ASSERT_NE(derive_gate, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_gate));
    EXPECT_EQ(derive_gate->token_plan_identity, derive_buffer->token_plan_identity);
    EXPECT_EQ(derive_gate->token_buffer_identity, derive_buffer->token_buffer_identity);
    EXPECT_EQ(derive_gate->materialization_identity, derive_buffer->materialization_identity);
    EXPECT_EQ(derive_gate->source_map_identity, derive_buffer->source_map_identity);
    EXPECT_EQ(derive_gate->hygiene_mark, derive_buffer->hygiene_mark);
    EXPECT_EQ(derive_gate->token_stream_name, derive_buffer->token_stream_name);
    expect_contains(derive_gate->blocker_reason,
        "compiler-owned derive generated token buffer parser admission remains blocked in M21j");
    EXPECT_EQ(derive_gate->token_count, derive_buffer->token_count);
    EXPECT_TRUE(derive_gate->compiler_owned);
    EXPECT_TRUE(derive_gate->token_buffer_materialized);
    EXPECT_TRUE(derive_gate->token_records_available);
    EXPECT_FALSE(derive_gate->parser_admitted);
    EXPECT_FALSE(derive_gate->parse_ready);
    EXPECT_FALSE(derive_gate->parser_consumable);
    EXPECT_FALSE(derive_gate->generated_source_text);
    EXPECT_FALSE(derive_gate->generated_part_parsed);
    EXPECT_FALSE(derive_gate->generated_part_merged);
    EXPECT_FALSE(derive_gate->sema_visible);
    EXPECT_FALSE(derive_gate->produced_user_generated_code);
    EXPECT_NE(builder_gate->parse_gate_identity, derive_gate->parse_gate_identity);

    const frontend::macro::ParserAdmissionDiagnosticProjectionStub* const builder_diagnostic =
        parser_admission_diagnostic_for_input(result, *builder);
    ASSERT_NE(builder_diagnostic, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_diagnostic));
    EXPECT_EQ(builder_diagnostic->part_index, builder->part_index);
    EXPECT_EQ(builder_diagnostic->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_diagnostic->attached_part, builder->attached_part);
    EXPECT_EQ(builder_diagnostic->generated_part, generated.generated_part);
    EXPECT_EQ(builder_diagnostic->primary_anchor.source.value, builder->attribute_range.source.value);
    EXPECT_EQ(builder_diagnostic->primary_anchor.begin, builder->attribute_range.begin);
    EXPECT_EQ(builder_diagnostic->primary_anchor.end, builder->attribute_range.end);
    EXPECT_EQ(builder_diagnostic->token_tree_anchor.source.value, builder->token_tree_range.source.value);
    EXPECT_EQ(builder_diagnostic->token_tree_anchor.begin, builder->token_tree_range.begin);
    EXPECT_EQ(builder_diagnostic->token_tree_anchor.end, builder->token_tree_range.end);
    EXPECT_EQ(builder_diagnostic->parse_gate_identity, builder_gate->parse_gate_identity);
    EXPECT_GT(builder_diagnostic->diagnostic_identity.byte_count, 0U);
    EXPECT_GT(builder_diagnostic->diagnostic_anchor_identity.byte_count, 0U);
    EXPECT_NE(builder_diagnostic->diagnostic_identity, builder_diagnostic->parse_gate_identity);
    EXPECT_EQ(builder_diagnostic->token_plan_identity, builder_gate->token_plan_identity);
    EXPECT_EQ(builder_diagnostic->token_buffer_identity, builder_gate->token_buffer_identity);
    EXPECT_EQ(builder_diagnostic->materialization_identity, builder_gate->materialization_identity);
    EXPECT_EQ(builder_diagnostic->generated_buffer_identity, stub.generated_buffer_identity);
    EXPECT_EQ(builder_diagnostic->parse_config_fingerprint, stub.parse_config_fingerprint);
    EXPECT_EQ(builder_diagnostic->source_map_identity, builder_buffer->source_map_identity);
    EXPECT_EQ(builder_diagnostic->hygiene_mark, builder_buffer->hygiene_mark);
    EXPECT_EQ(builder_diagnostic->trace_identity, builder_trace->trace_identity);
    EXPECT_EQ(builder_diagnostic->diagnostic_policy,
        "parser_admission_blocked_diagnostic_projection_v1");
    EXPECT_EQ(builder_diagnostic->blocker_category, "empty_token_buffer_parser_admission_blocked");
    EXPECT_EQ(builder_diagnostic->token_buffer_blocker,
        "empty or non-derive generated token buffer parser admission remains blocked in M21j");
    expect_contains(builder_diagnostic->generated_part_parse_blocker,
        "generated module part parse remains blocked before parser admission diagnostics in M21k");
    expect_contains(builder_diagnostic->user_message,
        "generated token buffer is empty and parser admission remains blocked in M21k");
    expect_contains(builder_diagnostic->debug_projection_name,
        "m21k-parser-admission:0:0:0:0:builder");
    EXPECT_EQ(builder_diagnostic->token_count, 0U);
    EXPECT_FALSE(builder_diagnostic->token_buffer_materialized);
    EXPECT_FALSE(builder_diagnostic->token_records_available);
    EXPECT_FALSE(builder_diagnostic->parser_admitted);
    EXPECT_FALSE(builder_diagnostic->parse_ready);
    EXPECT_FALSE(builder_diagnostic->parser_consumable);
    EXPECT_FALSE(builder_diagnostic->generated_part_parsed);
    EXPECT_FALSE(builder_diagnostic->generated_part_merged);
    EXPECT_FALSE(builder_diagnostic->emit_expanded_available);
    EXPECT_FALSE(builder_diagnostic->debug_trace_available);
    EXPECT_FALSE(builder_diagnostic->source_map_available);
    EXPECT_FALSE(builder_diagnostic->produced_user_generated_code);

    const frontend::macro::ParserAdmissionDiagnosticProjectionStub* const derive_diagnostic =
        parser_admission_diagnostic_for_input(result, *derive);
    ASSERT_NE(derive_diagnostic, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_diagnostic));
    EXPECT_EQ(derive_diagnostic->parse_gate_identity, derive_gate->parse_gate_identity);
    EXPECT_EQ(derive_diagnostic->token_count, derive_gate->token_count);
    EXPECT_TRUE(derive_diagnostic->token_buffer_materialized);
    EXPECT_TRUE(derive_diagnostic->token_records_available);
    EXPECT_EQ(derive_diagnostic->blocker_category, "derive_token_buffer_parser_admission_blocked");
    EXPECT_EQ(derive_diagnostic->token_buffer_blocker,
        "compiler-owned derive generated token buffer parser admission remains blocked in M21j");
    expect_contains(derive_diagnostic->user_message,
        "generated derive token buffer is compiler-owned but parser admission remains blocked in M21k");
    expect_contains(derive_diagnostic->debug_projection_name,
        "m21k-parser-admission:0:0:0:1:derive");
    EXPECT_FALSE(derive_diagnostic->emit_expanded_available);
    EXPECT_FALSE(derive_diagnostic->debug_trace_available);
    EXPECT_FALSE(derive_diagnostic->source_map_available);
    EXPECT_NE(builder_diagnostic->diagnostic_identity, derive_diagnostic->diagnostic_identity);

    const frontend::macro::ParserAdmissionDiagnosticReportEntry* const builder_report_entry =
        parser_admission_report_entry_for_input(result, *builder);
    ASSERT_NE(builder_report_entry, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_report_entry));
    EXPECT_EQ(builder_report_entry->part_index, builder->part_index);
    EXPECT_EQ(builder_report_entry->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_report_entry->report_index, 0U);
    EXPECT_EQ(builder_report_entry->attached_part, builder->attached_part);
    EXPECT_EQ(builder_report_entry->generated_part, generated.generated_part);
    EXPECT_EQ(builder_report_entry->diagnostic_identity, builder_diagnostic->diagnostic_identity);
    EXPECT_EQ(builder_report_entry->diagnostic_anchor_identity,
        builder_diagnostic->diagnostic_anchor_identity);
    EXPECT_GT(builder_report_entry->report_entry_identity.byte_count, 0U);
    EXPECT_NE(builder_report_entry->report_entry_identity, builder_report_entry->diagnostic_identity);
    EXPECT_EQ(builder_report_entry->parse_gate_identity, builder_gate->parse_gate_identity);
    EXPECT_EQ(builder_report_entry->blocker_category, builder_diagnostic->blocker_category);
    EXPECT_EQ(builder_report_entry->debug_projection_name, builder_diagnostic->debug_projection_name);
    EXPECT_EQ(builder_report_entry->query_projection_name, "m21l-parser-admission-report:0:0");
    EXPECT_EQ(builder_report_entry->token_count, builder_diagnostic->token_count);
    EXPECT_FALSE(builder_report_entry->token_records_available);
    EXPECT_FALSE(builder_report_entry->parser_admitted);
    EXPECT_TRUE(builder_report_entry->report_visible);
    EXPECT_TRUE(builder_report_entry->query_reusable);
    EXPECT_FALSE(builder_report_entry->parser_consumable);
    EXPECT_FALSE(builder_report_entry->emit_expanded_available);
    EXPECT_FALSE(builder_report_entry->produced_user_generated_code);

    const frontend::macro::ParserAdmissionDiagnosticReportEntry* const derive_report_entry =
        parser_admission_report_entry_for_input(result, *derive);
    ASSERT_NE(derive_report_entry, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_report_entry));
    EXPECT_EQ(derive_report_entry->report_index, 1U);
    EXPECT_EQ(derive_report_entry->diagnostic_identity, derive_diagnostic->diagnostic_identity);
    EXPECT_EQ(derive_report_entry->diagnostic_anchor_identity,
        derive_diagnostic->diagnostic_anchor_identity);
    EXPECT_EQ(derive_report_entry->parse_gate_identity, derive_gate->parse_gate_identity);
    EXPECT_EQ(derive_report_entry->blocker_category, "derive_token_buffer_parser_admission_blocked");
    EXPECT_EQ(derive_report_entry->query_projection_name, builder_report_entry->query_projection_name);
    EXPECT_TRUE(derive_report_entry->token_records_available);
    EXPECT_FALSE(derive_report_entry->parser_admitted);
    EXPECT_NE(builder_report_entry->report_entry_identity, derive_report_entry->report_entry_identity);

    const frontend::macro::ParserAdmissionDiagnosticReport* const report =
        parser_admission_report_for_part(result, generated);
    ASSERT_NE(report, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*report));
    EXPECT_EQ(report->module.value, generated.module.value);
    EXPECT_EQ(report->source_part_index, generated.source_part_index);
    EXPECT_EQ(report->attached_part, generated.source_part);
    EXPECT_EQ(report->generated_part, generated.generated_part);
    EXPECT_GT(report->report_identity.byte_count, 0U);
    EXPECT_GT(report->report_anchor_identity.byte_count, 0U);
    EXPECT_GT(report->report_grouping_identity.byte_count, 0U);
    EXPECT_NE(report->report_identity, report->report_anchor_identity);
    EXPECT_NE(report->report_identity, report->report_grouping_identity);
    EXPECT_EQ(report->parse_config_fingerprint, stub.parse_config_fingerprint);
    EXPECT_EQ(report->generated_buffer_identity, stub.generated_buffer_identity);
    EXPECT_EQ(report->report_policy, "parser_admission_blocked_report_query_projection_v1");
    EXPECT_EQ(report->report_query_name, "m21l-parser-admission-report:0:0");
    expect_contains(report->blocked_reason,
        "parser admission diagnostic report remains parser-blocked in M21l");
    EXPECT_EQ(report->entry_count, 2U);
    EXPECT_EQ(report->blocked_entry_count, 2U);
    EXPECT_EQ(report->derive_entry_count, 1U);
    EXPECT_EQ(report->empty_entry_count, 1U);
    EXPECT_EQ(report->token_record_available_entry_count, 1U);
    EXPECT_TRUE(report->query_reusable);
    EXPECT_TRUE(report->report_visible);
    EXPECT_TRUE(report->source_anchor_ordered);
    EXPECT_FALSE(report->parser_admitted);
    EXPECT_FALSE(report->parse_ready);
    EXPECT_FALSE(report->parser_consumable);
    EXPECT_FALSE(report->emit_expanded_available);
    EXPECT_FALSE(report->debug_trace_available);
    EXPECT_FALSE(report->source_map_available);
    EXPECT_FALSE(report->produced_user_generated_code);

    const frontend::macro::GeneratedTokenParserReadinessPreflightEntry* const builder_preflight =
        parser_readiness_preflight_entry_for_input(result, *builder);
    ASSERT_NE(builder_preflight, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_preflight));
    EXPECT_EQ(builder_preflight->part_index, builder->part_index);
    EXPECT_EQ(builder_preflight->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_preflight->attached_part, builder->attached_part);
    EXPECT_EQ(builder_preflight->generated_part, generated.generated_part);
    EXPECT_EQ(builder_preflight->token_buffer_identity, builder_buffer->token_buffer_identity);
    EXPECT_EQ(builder_preflight->materialization_identity, builder_buffer->materialization_identity);
    EXPECT_EQ(builder_preflight->generated_buffer_identity, builder_gate->generated_buffer_identity);
    EXPECT_EQ(builder_preflight->parse_config_fingerprint, builder_gate->parse_config_fingerprint);
    EXPECT_EQ(builder_preflight->parse_gate_identity, builder_gate->parse_gate_identity);
    EXPECT_EQ(builder_preflight->diagnostic_identity, builder_diagnostic->diagnostic_identity);
    EXPECT_EQ(builder_preflight->diagnostic_anchor_identity, builder_diagnostic->diagnostic_anchor_identity);
    EXPECT_EQ(builder_preflight->report_entry_identity, builder_report_entry->report_entry_identity);
    EXPECT_EQ(builder_preflight->source_map_identity, builder_buffer->source_map_identity);
    EXPECT_EQ(builder_preflight->hygiene_mark, builder_buffer->hygiene_mark);
    EXPECT_EQ(builder_preflight->trace_identity, builder_diagnostic->trace_identity);
    EXPECT_GT(builder_preflight->preflight_identity.byte_count, 0U);
    EXPECT_EQ(builder_preflight->token_stream_name, builder_buffer->token_stream_name);
    EXPECT_EQ(builder_preflight->token_stream_shape, "empty_token_stream_parser_input_blocked");
    EXPECT_EQ(builder_preflight->delimiter_balance_state, "balanced");
    EXPECT_EQ(builder_preflight->source_anchor_coverage_state, "covered");
    EXPECT_EQ(builder_preflight->readiness_policy,
        "generated_token_parser_consumption_readiness_preflight_v1");
    expect_contains(builder_preflight->blocker_reason,
        "parser consumption readiness preflight remains parser-blocked in M21m");
    EXPECT_EQ(builder_preflight->token_count, 0U);
    EXPECT_FALSE(builder_preflight->token_records_available);
    EXPECT_TRUE(builder_preflight->token_indices_contiguous);
    EXPECT_TRUE(builder_preflight->delimiter_balanced);
    EXPECT_TRUE(builder_preflight->source_anchors_covered);
    EXPECT_TRUE(builder_preflight->parse_config_compatible);
    EXPECT_TRUE(builder_preflight->hygiene_prerequisite_available);
    EXPECT_TRUE(builder_preflight->source_map_prerequisite_available);
    EXPECT_TRUE(builder_preflight->diagnostic_projection_available);
    EXPECT_FALSE(builder_preflight->parser_admitted);
    EXPECT_FALSE(builder_preflight->parse_ready);
    EXPECT_FALSE(builder_preflight->parser_consumable);
    EXPECT_FALSE(builder_preflight->generated_part_parsed);
    EXPECT_FALSE(builder_preflight->generated_part_merged);
    EXPECT_FALSE(builder_preflight->produced_user_generated_code);

    const frontend::macro::GeneratedTokenParserReadinessPreflightEntry* const derive_preflight =
        parser_readiness_preflight_entry_for_input(result, *derive);
    ASSERT_NE(derive_preflight, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_preflight));
    EXPECT_EQ(derive_preflight->token_stream_shape, "derive_token_buffer_parser_input_candidate");
    EXPECT_EQ(derive_preflight->token_count, derive_buffer->token_count);
    EXPECT_TRUE(derive_preflight->token_records_available);
    EXPECT_TRUE(derive_preflight->token_indices_contiguous);
    EXPECT_TRUE(derive_preflight->delimiter_balanced);
    EXPECT_TRUE(derive_preflight->source_anchors_covered);
    EXPECT_FALSE(derive_preflight->parser_consumable);

    const frontend::macro::GeneratedTokenParserConsumptionContractGate* const contract =
        parser_consumption_contract_gate_for_part(result, generated);
    ASSERT_NE(contract, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*contract));
    EXPECT_EQ(contract->module.value, generated.module.value);
    EXPECT_EQ(contract->source_part_index, generated.source_part_index);
    EXPECT_EQ(contract->attached_part, generated.source_part);
    EXPECT_EQ(contract->generated_part, generated.generated_part);
    EXPECT_EQ(contract->generated_buffer_identity, stub.generated_buffer_identity);
    EXPECT_EQ(contract->parse_config_fingerprint, stub.parse_config_fingerprint);
    EXPECT_EQ(contract->report_identity, report->report_identity);
    EXPECT_GT(contract->contract_identity.byte_count, 0U);
    EXPECT_GT(contract->contract_grouping_identity.byte_count, 0U);
    EXPECT_GT(contract->contract_anchor_identity.byte_count, 0U);
    EXPECT_EQ(contract->contract_policy, "generated_token_parser_consumption_contract_gate_v1");
    EXPECT_EQ(contract->contract_query_name, "m21n-parser-consumption-contract:0:0");
    expect_contains(contract->blocked_reason,
        "parser consumption contract remains parser-blocked in M21n");
    EXPECT_EQ(contract->preflight_entry_count, 2U);
    EXPECT_EQ(contract->blocked_entry_count, 2U);
    EXPECT_EQ(contract->derive_entry_count, 1U);
    EXPECT_EQ(contract->empty_entry_count, 1U);
    EXPECT_EQ(contract->contiguous_index_entry_count, 2U);
    EXPECT_EQ(contract->delimiter_balanced_entry_count, 2U);
    EXPECT_EQ(contract->source_anchor_covered_entry_count, 2U);
    EXPECT_EQ(contract->parse_config_compatible_entry_count, 2U);
    EXPECT_EQ(contract->diagnostic_projection_entry_count, 2U);
    EXPECT_TRUE(contract->query_reusable);
    EXPECT_TRUE(contract->contract_visible);
    EXPECT_TRUE(contract->all_entries_structurally_checked);
    EXPECT_FALSE(contract->parser_admitted);
    EXPECT_FALSE(contract->parse_ready);
    EXPECT_FALSE(contract->parser_consumable);
    EXPECT_FALSE(contract->generated_part_parsed);
    EXPECT_FALSE(contract->generated_part_merged);
    EXPECT_FALSE(contract->sema_visible);
    EXPECT_FALSE(contract->emit_expanded_available);
    EXPECT_FALSE(contract->debug_trace_available);
    EXPECT_FALSE(contract->source_map_available);
    EXPECT_FALSE(contract->produced_user_generated_code);

    const frontend::macro::MacroExpansionBoundaryClosureReport& closure =
        result.macro_boundary_closure_reports.front();
    EXPECT_TRUE(frontend::macro::is_valid(closure));
    EXPECT_GT(closure.closure_identity.byte_count, 0U);
    EXPECT_GT(closure.closure_grouping_identity.byte_count, 0U);
    EXPECT_NE(closure.closure_identity, closure.closure_grouping_identity);
    EXPECT_EQ(closure.closure_policy, "m21_macro_expansion_boundary_release_closure_v1");
    EXPECT_EQ(closure.closure_query_name, "m21o-macro-boundary-closure");
    expect_contains(closure.blocked_reason,
        "M21 macro expansion boundary remains parser-blocked after M21o closure");
    EXPECT_EQ(closure.macro_input_count, 2U);
    EXPECT_EQ(closure.generated_part_count, 1U);
    EXPECT_EQ(closure.parser_admission_report_count, 1U);
    EXPECT_EQ(closure.parser_readiness_preflight_entry_count, 2U);
    EXPECT_EQ(closure.parser_consumption_contract_gate_count, 1U);
    EXPECT_EQ(closure.blocked_contract_gate_count, 1U);
    EXPECT_EQ(closure.parser_consumable_contract_gate_count, 0U);
    EXPECT_TRUE(closure.m21m_preflight_available);
    EXPECT_TRUE(closure.m21n_contract_available);
    EXPECT_TRUE(closure.release_closure_complete);
    EXPECT_TRUE(closure.query_reusable);
    EXPECT_TRUE(closure.closure_visible);
    EXPECT_FALSE(closure.parser_consumption_enabled);
    EXPECT_FALSE(closure.emit_expanded_available);
    EXPECT_FALSE(closure.debug_trace_available);
    EXPECT_FALSE(closure.source_map_available);
    EXPECT_FALSE(closure.standard_library_required);
    EXPECT_FALSE(closure.runtime_required);
    EXPECT_FALSE(closure.external_process_required);
    EXPECT_FALSE(closure.produced_user_generated_code);
}

} // namespace aurex::test
