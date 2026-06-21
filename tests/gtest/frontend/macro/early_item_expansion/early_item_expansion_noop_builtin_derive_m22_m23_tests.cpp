#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_noop_fixture_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionNoopCollectsBuiltinDeriveM22AndM23Contracts)
{
    const NoopAttributeExpansionFixture fixture = make_noop_attribute_expansion_fixture();
    const frontend::macro::EarlyItemExpansionResult& result = fixture.result;
    const NoopAttributeExpansionView view = inspect_noop_attribute_expansion(fixture);
    const auto* const builder = view.builder;
    const auto* const derive = view.derive;
    const auto* const generated_ptr = view.generated;
    const auto* const builder_buffer = view.builder_buffer;
    const auto* const derive_buffer = view.derive_buffer;
    const auto* const builder_gate = view.builder_gate;
    const auto* const derive_gate = view.derive_gate;
    const auto* const builder_diagnostic = view.builder_diagnostic;
    const auto* const derive_diagnostic = view.derive_diagnostic;
    const auto* const builder_preflight = view.builder_preflight;
    const auto* const derive_preflight = view.derive_preflight;
    const auto* const contract = view.contract;
    const auto* const closure = view.closure;
    ASSERT_NE(builder, nullptr);
    ASSERT_NE(derive, nullptr);
    ASSERT_NE(generated_ptr, nullptr);
    ASSERT_NE(builder_buffer, nullptr);
    ASSERT_NE(derive_buffer, nullptr);
    ASSERT_NE(builder_gate, nullptr);
    ASSERT_NE(derive_gate, nullptr);
    ASSERT_NE(builder_diagnostic, nullptr);
    ASSERT_NE(derive_diagnostic, nullptr);
    ASSERT_NE(builder_preflight, nullptr);
    ASSERT_NE(derive_preflight, nullptr);
    ASSERT_NE(contract, nullptr);
    ASSERT_NE(closure, nullptr);
    const auto& generated = *generated_ptr;

    ASSERT_EQ(result.builtin_derive_expansion_admissions.size(), 2U);
    ASSERT_EQ(result.builtin_derive_semantic_plans.size(), 2U);
    ASSERT_EQ(result.builtin_derive_parser_release_gates.size(), 1U);
    const frontend::macro::BuiltinDeriveExpansionAdmissionGate* const builder_derive_admission =
        builtin_derive_admission_for_input(result, *builder);
    ASSERT_NE(builder_derive_admission, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_derive_admission));
    EXPECT_EQ(builder_derive_admission->part_index, builder->part_index);
    EXPECT_EQ(builder_derive_admission->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_derive_admission->admission_index, 0U);
    EXPECT_EQ(builder_derive_admission->attached_part, builder->attached_part);
    EXPECT_EQ(builder_derive_admission->generated_part, generated.generated_part);
    EXPECT_EQ(builder_derive_admission->token_buffer_identity, builder_buffer->token_buffer_identity);
    EXPECT_EQ(builder_derive_admission->preflight_identity, builder_preflight->preflight_identity);
    EXPECT_EQ(builder_derive_admission->parse_gate_identity, builder_gate->parse_gate_identity);
    EXPECT_EQ(builder_derive_admission->diagnostic_identity, builder_diagnostic->diagnostic_identity);
    EXPECT_EQ(builder_derive_admission->closure_identity, closure->closure_identity);
    EXPECT_GT(builder_derive_admission->admission_identity.byte_count, 0U);
    EXPECT_EQ(builder_derive_admission->admission_policy,
        "builtin_derive_expansion_admission_gate_v1");
    EXPECT_EQ(builder_derive_admission->admission_kind,
        "non_derive_attribute_expansion_blocked");
    EXPECT_EQ(builder_derive_admission->query_name,
        "m22a-builtin-derive-admission:0:0:0:0:builder");
    expect_contains(builder_derive_admission->blocker_reason,
        "non-derive item attribute expansion remains blocked in M22a");
    EXPECT_EQ(builder_derive_admission->token_count, 0U);
    EXPECT_EQ(builder_derive_admission->capability_candidate_count, 0U);
    EXPECT_FALSE(builder_derive_admission->builtin_derive_input);
    EXPECT_TRUE(builder_derive_admission->compiler_owned);
    EXPECT_FALSE(builder_derive_admission->token_records_available);
    EXPECT_TRUE(builder_derive_admission->preflight_available);
    EXPECT_TRUE(builder_derive_admission->admission_visible);
    EXPECT_TRUE(builder_derive_admission->query_reusable);
    EXPECT_FALSE(builder_derive_admission->parser_consumption_enabled);
    EXPECT_FALSE(builder_derive_admission->external_process_required);
    EXPECT_FALSE(builder_derive_admission->standard_library_required);
    EXPECT_FALSE(builder_derive_admission->runtime_required);
    EXPECT_FALSE(builder_derive_admission->generated_source_text);
    EXPECT_FALSE(builder_derive_admission->produced_user_generated_code);

    const frontend::macro::BuiltinDeriveExpansionAdmissionGate* const derive_expansion_admission =
        builtin_derive_admission_for_input(result, *derive);
    ASSERT_NE(derive_expansion_admission, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_expansion_admission));
    EXPECT_EQ(derive_expansion_admission->admission_index, 1U);
    EXPECT_EQ(derive_expansion_admission->token_buffer_identity, derive_buffer->token_buffer_identity);
    EXPECT_EQ(derive_expansion_admission->preflight_identity, derive_preflight->preflight_identity);
    EXPECT_EQ(derive_expansion_admission->parse_gate_identity, derive_gate->parse_gate_identity);
    EXPECT_EQ(derive_expansion_admission->diagnostic_identity, derive_diagnostic->diagnostic_identity);
    EXPECT_EQ(derive_expansion_admission->closure_identity, closure->closure_identity);
    EXPECT_EQ(derive_expansion_admission->admission_kind,
        "builtin_derive_expansion_candidate");
    EXPECT_EQ(derive_expansion_admission->query_name,
        "m22a-builtin-derive-admission:0:0:0:1:derive");
    expect_contains(derive_expansion_admission->blocker_reason,
        "builtin derive expansion admission remains parser-blocked in M22a");
    EXPECT_EQ(derive_expansion_admission->token_count, derive_buffer->token_count);
    EXPECT_EQ(derive_expansion_admission->capability_candidate_count, 2U);
    EXPECT_EQ(derive_expansion_admission->unsupported_candidate_count, 0U);
    EXPECT_EQ(derive_expansion_admission->duplicate_candidate_count, 0U);
    EXPECT_TRUE(derive_expansion_admission->builtin_derive_input);
    EXPECT_TRUE(derive_expansion_admission->token_records_available);
    EXPECT_FALSE(derive_expansion_admission->parser_consumption_enabled);
    EXPECT_NE(builder_derive_admission->admission_identity,
        derive_expansion_admission->admission_identity);

    const frontend::macro::BuiltinDeriveSemanticExpansionPlan* const builder_semantic_plan =
        builtin_derive_semantic_plan_for_input(result, *builder);
    ASSERT_NE(builder_semantic_plan, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_semantic_plan));
    EXPECT_EQ(builder_semantic_plan->semantic_plan_index, 0U);
    EXPECT_EQ(builder_semantic_plan->admission_identity,
        builder_derive_admission->admission_identity);
    EXPECT_EQ(builder_semantic_plan->semantic_policy,
        "builtin_derive_semantic_expansion_plan_v1");
    EXPECT_EQ(builder_semantic_plan->target_kind, "struct");
    EXPECT_EQ(builder_semantic_plan->semantic_model, "capability_fact_lowering_plan");
    expect_contains(builder_semantic_plan->blocker_reason,
        "builtin derive semantic expansion remains capability-only and parser-blocked in M22b");
    EXPECT_EQ(builder_semantic_plan->capability_count, 0U);
    EXPECT_EQ(builder_semantic_plan->copy_capability_count, 0U);
    EXPECT_EQ(builder_semantic_plan->eq_capability_count, 0U);
    EXPECT_EQ(builder_semantic_plan->hash_capability_count, 0U);
    EXPECT_FALSE(builder_semantic_plan->builtin_derive_input);
    EXPECT_TRUE(builder_semantic_plan->target_struct_or_enum);
    EXPECT_FALSE(builder_semantic_plan->uses_existing_builtin_derive_capability_path);
    EXPECT_FALSE(builder_semantic_plan->requires_ast_mutation);
    EXPECT_FALSE(builder_semantic_plan->requires_generated_items);
    EXPECT_FALSE(builder_semantic_plan->requires_standard_library);
    EXPECT_FALSE(builder_semantic_plan->requires_runtime);
    EXPECT_FALSE(builder_semantic_plan->external_process_required);
    EXPECT_FALSE(builder_semantic_plan->parser_consumption_enabled);
    EXPECT_FALSE(builder_semantic_plan->produced_user_generated_code);
    EXPECT_TRUE(builder_semantic_plan->plan_visible);
    EXPECT_TRUE(builder_semantic_plan->query_reusable);

    const frontend::macro::BuiltinDeriveSemanticExpansionPlan* const derive_semantic_plan =
        builtin_derive_semantic_plan_for_input(result, *derive);
    ASSERT_NE(derive_semantic_plan, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_semantic_plan));
    EXPECT_EQ(derive_semantic_plan->semantic_plan_index, 1U);
    EXPECT_EQ(derive_semantic_plan->admission_identity,
        derive_expansion_admission->admission_identity);
    EXPECT_EQ(derive_semantic_plan->target_kind, "struct");
    EXPECT_EQ(derive_semantic_plan->capability_count, 2U);
    EXPECT_EQ(derive_semantic_plan->copy_capability_count, 1U);
    EXPECT_EQ(derive_semantic_plan->eq_capability_count, 1U);
    EXPECT_EQ(derive_semantic_plan->hash_capability_count, 0U);
    EXPECT_TRUE(derive_semantic_plan->builtin_derive_input);
    EXPECT_TRUE(derive_semantic_plan->target_struct_or_enum);
    EXPECT_TRUE(derive_semantic_plan->uses_existing_builtin_derive_capability_path);
    EXPECT_FALSE(derive_semantic_plan->requires_generated_items);
    EXPECT_FALSE(derive_semantic_plan->parser_consumption_enabled);
    EXPECT_NE(builder_semantic_plan->semantic_plan_identity,
        derive_semantic_plan->semantic_plan_identity);

    const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate* const release_gate =
        builtin_derive_parser_release_gate_for_part(result, generated);
    ASSERT_NE(release_gate, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*release_gate));
    EXPECT_EQ(release_gate->module.value, generated.module.value);
    EXPECT_EQ(release_gate->source_part_index, generated.source_part_index);
    EXPECT_EQ(release_gate->attached_part, generated.source_part);
    EXPECT_EQ(release_gate->generated_part, generated.generated_part);
    EXPECT_EQ(release_gate->contract_identity, contract->contract_identity);
    EXPECT_EQ(release_gate->closure_identity, closure->closure_identity);
    EXPECT_GT(release_gate->admission_group_identity.byte_count, 0U);
    EXPECT_GT(release_gate->semantic_plan_group_identity.byte_count, 0U);
    EXPECT_GT(release_gate->release_gate_identity.byte_count, 0U);
    EXPECT_EQ(release_gate->release_policy,
        "builtin_derive_parser_consumption_release_gate_v1");
    EXPECT_EQ(release_gate->release_query_name,
        "m22c-builtin-derive-parser-release:0:0");
    expect_contains(release_gate->blocked_reason,
        "builtin derive parser consumption release remains blocked in M22c");
    EXPECT_EQ(release_gate->admission_count, 2U);
    EXPECT_EQ(release_gate->derive_admission_count, 1U);
    EXPECT_EQ(release_gate->semantic_plan_count, 2U);
    EXPECT_EQ(release_gate->capability_total_count, 2U);
    EXPECT_EQ(release_gate->parser_consumable_contract_count, 0U);
    EXPECT_TRUE(release_gate->rollback_diagnostics_available);
    EXPECT_TRUE(release_gate->debug_trace_prerequisite_available);
    EXPECT_TRUE(release_gate->source_map_prerequisite_available);
    EXPECT_TRUE(release_gate->hygiene_prerequisite_available);
    EXPECT_FALSE(release_gate->parser_consumption_enabled);
    EXPECT_FALSE(release_gate->generated_part_parsed);
    EXPECT_FALSE(release_gate->generated_part_merged);
    EXPECT_FALSE(release_gate->emit_expanded_available);
    EXPECT_FALSE(release_gate->debug_trace_available);
    EXPECT_FALSE(release_gate->source_map_available);
    EXPECT_FALSE(release_gate->standard_library_required);
    EXPECT_FALSE(release_gate->runtime_required);
    EXPECT_FALSE(release_gate->external_process_required);
    EXPECT_FALSE(release_gate->produced_user_generated_code);
    EXPECT_TRUE(release_gate->release_visible);
    EXPECT_TRUE(release_gate->query_reusable);

    const frontend::macro::BuiltinDeriveReleaseHardeningMatrix* const hardening_matrix =
        builtin_derive_release_hardening_matrix_for_part(result, generated);
    ASSERT_NE(hardening_matrix, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*hardening_matrix));
    EXPECT_EQ(hardening_matrix->module.value, generated.module.value);
    EXPECT_EQ(hardening_matrix->source_part_index, generated.source_part_index);
    EXPECT_EQ(hardening_matrix->attached_part, generated.source_part);
    EXPECT_EQ(hardening_matrix->generated_part, generated.generated_part);
    EXPECT_EQ(hardening_matrix->release_gate_identity, release_gate->release_gate_identity);
    EXPECT_EQ(hardening_matrix->admission_group_identity, release_gate->admission_group_identity);
    EXPECT_EQ(hardening_matrix->semantic_plan_group_identity, release_gate->semantic_plan_group_identity);
    EXPECT_GT(hardening_matrix->hardening_matrix_identity.byte_count, 0U);
    EXPECT_EQ(hardening_matrix->hardening_policy, "builtin_derive_release_hardening_matrix_v1");
    EXPECT_EQ(hardening_matrix->hardening_query_name,
        "m22d-builtin-derive-release-hardening:0:0");
    expect_contains(hardening_matrix->blocked_reason,
        "builtin derive release hardening matrix keeps parser consumption blocked in M22d");
    EXPECT_EQ(hardening_matrix->part_local_admission_count, 2U);
    EXPECT_EQ(hardening_matrix->part_local_derive_admission_count, 1U);
    EXPECT_EQ(hardening_matrix->part_local_semantic_plan_count, 2U);
    EXPECT_EQ(hardening_matrix->part_local_release_gate_count, 1U);
    EXPECT_EQ(hardening_matrix->global_admission_count, 2U);
    EXPECT_EQ(hardening_matrix->global_semantic_plan_count, 2U);
    EXPECT_EQ(hardening_matrix->global_generated_part_count, 1U);
    EXPECT_EQ(hardening_matrix->cross_part_admission_count, 0U);
    EXPECT_EQ(hardening_matrix->cross_part_semantic_plan_count, 0U);
    EXPECT_TRUE(hardening_matrix->part_locality_preserved);
    EXPECT_TRUE(hardening_matrix->multi_item_matrix_available);
    EXPECT_TRUE(hardening_matrix->negative_matrix_complete);
    EXPECT_TRUE(hardening_matrix->release_remains_blocked);
    EXPECT_FALSE(hardening_matrix->parser_consumption_enabled);
    EXPECT_FALSE(hardening_matrix->generated_part_parsed);
    EXPECT_FALSE(hardening_matrix->generated_part_merged);
    EXPECT_FALSE(hardening_matrix->emit_expanded_available);
    EXPECT_FALSE(hardening_matrix->debug_trace_available);
    EXPECT_FALSE(hardening_matrix->source_map_available);
    EXPECT_FALSE(hardening_matrix->standard_library_required);
    EXPECT_FALSE(hardening_matrix->runtime_required);
    EXPECT_FALSE(hardening_matrix->external_process_required);
    EXPECT_FALSE(hardening_matrix->produced_user_generated_code);
    EXPECT_TRUE(hardening_matrix->matrix_visible);
    EXPECT_TRUE(hardening_matrix->query_reusable);

    const frontend::macro::BuiltinDeriveDebugDumpStabilityContract* const debug_contract =
        builtin_derive_debug_dump_contract_for_part(result, generated);
    ASSERT_NE(debug_contract, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*debug_contract));
    EXPECT_EQ(debug_contract->release_gate_identity, release_gate->release_gate_identity);
    EXPECT_EQ(debug_contract->hardening_matrix_identity, hardening_matrix->hardening_matrix_identity);
    EXPECT_GT(debug_contract->debug_dump_contract_identity.byte_count, 0U);
    EXPECT_EQ(debug_contract->debug_dump_policy,
        "builtin_derive_debug_dump_stability_contract_v1");
    EXPECT_EQ(debug_contract->debug_dump_query_name,
        "m22e-builtin-derive-debug-dump:0:0");
    expect_contains(debug_contract->blocked_reason,
        "builtin derive debug dump stability remains facts-only and parser-blocked in M22e");
    EXPECT_EQ(debug_contract->dump_section_count, 4U);
    EXPECT_TRUE(debug_contract->stable_ordering_available);
    EXPECT_TRUE(debug_contract->identity_projection_available);
    EXPECT_TRUE(debug_contract->summary_projection_available);
    EXPECT_TRUE(debug_contract->drift_debuggable);
    EXPECT_TRUE(debug_contract->debug_dump_contract_complete);
    EXPECT_FALSE(debug_contract->emit_expanded_available);
    EXPECT_FALSE(debug_contract->debug_trace_available);
    EXPECT_FALSE(debug_contract->source_map_available);
    EXPECT_FALSE(debug_contract->parser_consumption_enabled);
    EXPECT_FALSE(debug_contract->standard_library_required);
    EXPECT_FALSE(debug_contract->runtime_required);
    EXPECT_FALSE(debug_contract->external_process_required);
    EXPECT_FALSE(debug_contract->produced_user_generated_code);
    EXPECT_TRUE(debug_contract->contract_visible);
    EXPECT_TRUE(debug_contract->query_reusable);

    const frontend::macro::BuiltinDeriveRollbackDiagnosticDesignGate* const rollback_gate =
        builtin_derive_rollback_diagnostic_gate_for_part(result, generated);
    ASSERT_NE(rollback_gate, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*rollback_gate));
    EXPECT_EQ(rollback_gate->parser_consumption_contract_identity, contract->contract_identity);
    EXPECT_EQ(rollback_gate->release_gate_identity, release_gate->release_gate_identity);
    EXPECT_EQ(rollback_gate->hardening_matrix_identity, hardening_matrix->hardening_matrix_identity);
    EXPECT_EQ(rollback_gate->debug_dump_contract_identity, debug_contract->debug_dump_contract_identity);
    EXPECT_GT(rollback_gate->rollback_gate_identity.byte_count, 0U);
    EXPECT_EQ(rollback_gate->rollback_policy,
        "builtin_derive_rollback_diagnostic_design_gate_v1");
    EXPECT_EQ(rollback_gate->rollback_query_name,
        "m22f-builtin-derive-rollback-diagnostic:0:0");
    expect_contains(rollback_gate->blocked_reason,
        "builtin derive rollback diagnostics remain design-only and parser-blocked in M22f");
    EXPECT_EQ(rollback_gate->diagnostic_projection_count, 2U);
    EXPECT_EQ(rollback_gate->diagnostic_report_entry_count, 2U);
    EXPECT_EQ(rollback_gate->blocked_diagnostic_count, 2U);
    EXPECT_EQ(rollback_gate->derive_diagnostic_count, 1U);
    EXPECT_EQ(rollback_gate->empty_diagnostic_count, 1U);
    EXPECT_EQ(rollback_gate->parser_consumption_contract_count, 1U);
    EXPECT_TRUE(rollback_gate->rollback_diagnostic_design_available);
    EXPECT_TRUE(rollback_gate->diagnostic_grouping_available);
    EXPECT_TRUE(rollback_gate->source_anchor_available);
    EXPECT_TRUE(rollback_gate->token_tree_anchor_available);
    EXPECT_TRUE(rollback_gate->debug_dump_contract_available);
    EXPECT_TRUE(rollback_gate->release_rollback_plan_complete);
    EXPECT_FALSE(rollback_gate->rollback_execution_enabled);
    EXPECT_FALSE(rollback_gate->parser_consumption_enabled);
    EXPECT_FALSE(rollback_gate->generated_part_parsed);
    EXPECT_FALSE(rollback_gate->generated_part_merged);
    EXPECT_FALSE(rollback_gate->emit_expanded_available);
    EXPECT_FALSE(rollback_gate->debug_trace_available);
    EXPECT_FALSE(rollback_gate->source_map_available);
    EXPECT_FALSE(rollback_gate->standard_library_required);
    EXPECT_FALSE(rollback_gate->runtime_required);
    EXPECT_FALSE(rollback_gate->external_process_required);
    EXPECT_FALSE(rollback_gate->produced_user_generated_code);
    EXPECT_TRUE(rollback_gate->rollback_gate_visible);
    EXPECT_TRUE(rollback_gate->query_reusable);

    const frontend::macro::BuiltinDeriveParserConsumptionAdmissionProtocol* const admission_protocol =
        builtin_derive_parser_consumption_admission_protocol_for_part(result, generated);
    ASSERT_NE(admission_protocol, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*admission_protocol));
    EXPECT_EQ(admission_protocol->module.value, generated.module.value);
    EXPECT_EQ(admission_protocol->source_part_index, generated.source_part_index);
    EXPECT_EQ(admission_protocol->attached_part, generated.source_part);
    EXPECT_EQ(admission_protocol->generated_part, generated.generated_part);
    EXPECT_EQ(admission_protocol->parser_consumption_contract_identity, contract->contract_identity);
    EXPECT_EQ(admission_protocol->release_gate_identity, release_gate->release_gate_identity);
    EXPECT_EQ(admission_protocol->rollback_gate_identity, rollback_gate->rollback_gate_identity);
    EXPECT_GT(admission_protocol->admission_protocol_identity.byte_count, 0U);
    EXPECT_EQ(admission_protocol->admission_policy,
        "builtin_derive_parser_consumption_admission_protocol_v1");
    EXPECT_EQ(admission_protocol->admission_query_name,
        "m23a-builtin-derive-parser-consumption-admission:0:0");
    expect_contains(admission_protocol->blocked_reason, "no-parser-consumption in M23a");
    EXPECT_EQ(admission_protocol->token_buffer_count, 2U);
    EXPECT_EQ(admission_protocol->token_record_count, derive_buffer->token_count);
    EXPECT_EQ(admission_protocol->derive_candidate_count, 1U);
    EXPECT_EQ(admission_protocol->empty_candidate_count, 1U);
    EXPECT_EQ(admission_protocol->blocked_diagnostic_count, 2U);
    EXPECT_TRUE(admission_protocol->release_gate_available);
    EXPECT_TRUE(admission_protocol->rollback_gate_available);
    EXPECT_TRUE(admission_protocol->parser_contract_available);
    EXPECT_TRUE(admission_protocol->deterministic_order_available);
    EXPECT_TRUE(admission_protocol->generated_tokens_checkpointed);
    EXPECT_TRUE(admission_protocol->admission_protocol_complete);
    EXPECT_FALSE(admission_protocol->parser_consumption_enabled);
    EXPECT_FALSE(admission_protocol->parser_admitted);
    EXPECT_FALSE(admission_protocol->generated_part_parsed);
    EXPECT_FALSE(admission_protocol->generated_part_merged);
    EXPECT_FALSE(admission_protocol->emit_expanded_available);
    EXPECT_FALSE(admission_protocol->debug_trace_available);
    EXPECT_FALSE(admission_protocol->source_map_available);
    EXPECT_FALSE(admission_protocol->standard_library_required);
    EXPECT_FALSE(admission_protocol->runtime_required);
    EXPECT_FALSE(admission_protocol->external_process_required);
    EXPECT_FALSE(admission_protocol->produced_user_generated_code);
    EXPECT_TRUE(admission_protocol->protocol_visible);
    EXPECT_TRUE(admission_protocol->query_reusable);

    const frontend::macro::BuiltinDeriveParserConsumptionCheckpointRollbackProtocol* const
        checkpoint_protocol = builtin_derive_checkpoint_rollback_protocol_for_part(result, generated);
    ASSERT_NE(checkpoint_protocol, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*checkpoint_protocol));
    EXPECT_EQ(checkpoint_protocol->module.value, generated.module.value);
    EXPECT_EQ(checkpoint_protocol->source_part_index, generated.source_part_index);
    EXPECT_EQ(checkpoint_protocol->attached_part, generated.source_part);
    EXPECT_EQ(checkpoint_protocol->generated_part, generated.generated_part);
    EXPECT_EQ(checkpoint_protocol->admission_protocol_identity,
        admission_protocol->admission_protocol_identity);
    EXPECT_EQ(checkpoint_protocol->rollback_gate_identity, rollback_gate->rollback_gate_identity);
    EXPECT_GT(checkpoint_protocol->checkpoint_protocol_identity.byte_count, 0U);
    EXPECT_EQ(checkpoint_protocol->checkpoint_policy,
        "builtin_derive_parser_checkpoint_rollback_protocol_v1");
    EXPECT_EQ(checkpoint_protocol->checkpoint_query_name,
        "m23b-builtin-derive-checkpoint-rollback:0:0");
    expect_contains(checkpoint_protocol->blocked_reason, "parser-blocked in M23b");
    EXPECT_EQ(checkpoint_protocol->checkpoint_count, 3U);
    EXPECT_EQ(checkpoint_protocol->rollback_plan_count, 3U);
    EXPECT_EQ(checkpoint_protocol->token_record_count, admission_protocol->token_record_count);
    EXPECT_EQ(checkpoint_protocol->diagnostic_anchor_count,
        admission_protocol->blocked_diagnostic_count);
    EXPECT_TRUE(checkpoint_protocol->parser_state_checkpoint_available);
    EXPECT_TRUE(checkpoint_protocol->token_cursor_checkpoint_available);
    EXPECT_TRUE(checkpoint_protocol->generated_part_checkpoint_available);
    EXPECT_TRUE(checkpoint_protocol->diagnostic_replay_available);
    EXPECT_TRUE(checkpoint_protocol->rollback_protocol_complete);
    EXPECT_FALSE(checkpoint_protocol->rollback_execution_enabled);
    EXPECT_FALSE(checkpoint_protocol->parser_consumption_enabled);
    EXPECT_FALSE(checkpoint_protocol->generated_part_parsed);
    EXPECT_FALSE(checkpoint_protocol->generated_part_merged);
    EXPECT_FALSE(checkpoint_protocol->emit_expanded_available);
    EXPECT_FALSE(checkpoint_protocol->debug_trace_available);
    EXPECT_FALSE(checkpoint_protocol->source_map_available);
    EXPECT_FALSE(checkpoint_protocol->standard_library_required);
    EXPECT_FALSE(checkpoint_protocol->runtime_required);
    EXPECT_FALSE(checkpoint_protocol->external_process_required);
    EXPECT_FALSE(checkpoint_protocol->produced_user_generated_code);
    EXPECT_TRUE(checkpoint_protocol->protocol_visible);
    EXPECT_TRUE(checkpoint_protocol->query_reusable);

    const frontend::macro::BuiltinDeriveParserPreConsumptionVerificationClosure* const
        verification_closure = builtin_derive_preconsumption_verification_closure_for_part(result, generated);
    ASSERT_NE(verification_closure, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*verification_closure));
    EXPECT_EQ(verification_closure->module.value, generated.module.value);
    EXPECT_EQ(verification_closure->source_part_index, generated.source_part_index);
    EXPECT_EQ(verification_closure->attached_part, generated.source_part);
    EXPECT_EQ(verification_closure->generated_part, generated.generated_part);
    EXPECT_EQ(verification_closure->admission_protocol_identity,
        admission_protocol->admission_protocol_identity);
    EXPECT_EQ(verification_closure->checkpoint_protocol_identity,
        checkpoint_protocol->checkpoint_protocol_identity);
    EXPECT_EQ(verification_closure->debug_dump_contract_identity,
        debug_contract->debug_dump_contract_identity);
    EXPECT_GT(verification_closure->verification_closure_identity.byte_count, 0U);
    EXPECT_EQ(verification_closure->verification_policy,
        "builtin_derive_parser_preconsumption_verification_closure_v1");
    EXPECT_EQ(verification_closure->verification_query_name,
        "m23c-builtin-derive-preconsumption-verification:0:0");
    expect_contains(verification_closure->blocked_reason, "blocked in M23c");
    EXPECT_EQ(verification_closure->admission_protocol_count, 1U);
    EXPECT_EQ(verification_closure->checkpoint_protocol_count, 1U);
    EXPECT_EQ(verification_closure->hardening_matrix_count, 1U);
    EXPECT_EQ(verification_closure->debug_dump_contract_count, 1U);
    EXPECT_EQ(verification_closure->rollback_gate_count, 1U);
    EXPECT_TRUE(verification_closure->admission_protocol_available);
    EXPECT_TRUE(verification_closure->checkpoint_protocol_available);
    EXPECT_TRUE(verification_closure->release_hardening_available);
    EXPECT_TRUE(verification_closure->debug_dump_contract_available);
    EXPECT_TRUE(verification_closure->rollback_gate_available);
    EXPECT_TRUE(verification_closure->verification_closure_complete);
    EXPECT_FALSE(verification_closure->parser_consumption_enabled);
    EXPECT_FALSE(verification_closure->generated_part_parsed);
    EXPECT_FALSE(verification_closure->generated_part_merged);
    EXPECT_FALSE(verification_closure->sema_visible);
    EXPECT_FALSE(verification_closure->emit_expanded_available);
    EXPECT_FALSE(verification_closure->debug_trace_available);
    EXPECT_FALSE(verification_closure->source_map_available);
    EXPECT_FALSE(verification_closure->standard_library_required);
    EXPECT_FALSE(verification_closure->runtime_required);
    EXPECT_FALSE(verification_closure->external_process_required);
    EXPECT_FALSE(verification_closure->produced_user_generated_code);
    EXPECT_TRUE(verification_closure->closure_visible);
    EXPECT_TRUE(verification_closure->query_reusable);
}

} // namespace aurex::test
