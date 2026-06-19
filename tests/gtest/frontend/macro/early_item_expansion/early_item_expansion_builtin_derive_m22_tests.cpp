#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_builtin_derive_support.hpp>
#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_contract_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionBuiltinDeriveM22CountsDuplicateEnumCapabilities)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq, Eq, Hash)]\n"
        "enum Mode { fast, slow }\n";

    const frontend::macro::EarlyItemExpansionResult result = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(result));
    ASSERT_EQ(result.inputs.size(), 1U);
    ASSERT_EQ(result.builtin_derive_expansion_admissions.size(), 1U);
    ASSERT_EQ(result.builtin_derive_semantic_plans.size(), 1U);
    ASSERT_EQ(result.builtin_derive_parser_release_gates.size(), 1U);
    ASSERT_EQ(result.builtin_derive_release_hardening_matrices.size(), 1U);
    ASSERT_EQ(result.builtin_derive_debug_dump_contracts.size(), 1U);
    ASSERT_EQ(result.builtin_derive_rollback_diagnostic_gates.size(), 1U);

    const frontend::macro::EarlyItemMacroInput& input = result.inputs.front();
    EXPECT_EQ(input.attribute_name, "derive");
    EXPECT_EQ(input.disposition,
        frontend::macro::EarlyItemExpansionDisposition::builtin_derive_passthrough);

    const frontend::macro::BuiltinDeriveExpansionAdmissionGate& admission =
        result.builtin_derive_expansion_admissions.front();
    EXPECT_EQ(admission.admission_kind, "builtin_derive_expansion_candidate");
    EXPECT_EQ(admission.query_name, "m22a-builtin-derive-admission:0:0:0:0:derive");
    EXPECT_EQ(admission.capability_candidate_count, 4U);
    EXPECT_EQ(admission.unsupported_candidate_count, 0U);
    EXPECT_EQ(admission.duplicate_candidate_count, 1U);
    EXPECT_TRUE(admission.builtin_derive_input);
    EXPECT_TRUE(admission.token_records_available);
    EXPECT_FALSE(admission.parser_consumption_enabled);
    EXPECT_FALSE(admission.produced_user_generated_code);

    const frontend::macro::BuiltinDeriveSemanticExpansionPlan& plan =
        result.builtin_derive_semantic_plans.front();
    EXPECT_EQ(plan.target_kind, "enum");
    EXPECT_TRUE(plan.target_struct_or_enum);
    EXPECT_TRUE(plan.uses_existing_builtin_derive_capability_path);
    EXPECT_EQ(plan.capability_count, 4U);
    EXPECT_EQ(plan.copy_capability_count, 1U);
    EXPECT_EQ(plan.eq_capability_count, 2U);
    EXPECT_EQ(plan.hash_capability_count, 1U);
    EXPECT_FALSE(plan.requires_generated_items);
    EXPECT_FALSE(plan.parser_consumption_enabled);

    const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate& release_gate =
        result.builtin_derive_parser_release_gates.front();
    EXPECT_EQ(release_gate.admission_count, 1U);
    EXPECT_EQ(release_gate.derive_admission_count, 1U);
    EXPECT_EQ(release_gate.semantic_plan_count, 1U);
    EXPECT_EQ(release_gate.capability_total_count, 4U);
    EXPECT_EQ(release_gate.parser_consumable_contract_count, 0U);
    EXPECT_TRUE(release_gate.rollback_diagnostics_available);
    EXPECT_FALSE(release_gate.parser_consumption_enabled);

    const frontend::macro::BuiltinDeriveReleaseHardeningMatrix& matrix =
        result.builtin_derive_release_hardening_matrices.front();
    EXPECT_EQ(matrix.part_local_admission_count, 1U);
    EXPECT_EQ(matrix.part_local_derive_admission_count, 1U);
    EXPECT_EQ(matrix.global_admission_count, 1U);
    EXPECT_EQ(matrix.cross_part_admission_count, 0U);
    EXPECT_TRUE(matrix.negative_matrix_complete);
    EXPECT_FALSE(matrix.parser_consumption_enabled);

    const frontend::macro::BuiltinDeriveDebugDumpStabilityContract& debug_contract =
        result.builtin_derive_debug_dump_contracts.front();
    EXPECT_EQ(debug_contract.dump_section_count, 4U);
    EXPECT_TRUE(debug_contract.debug_dump_contract_complete);
    EXPECT_FALSE(debug_contract.parser_consumption_enabled);

    const frontend::macro::BuiltinDeriveRollbackDiagnosticDesignGate& rollback_gate =
        result.builtin_derive_rollback_diagnostic_gates.front();
    EXPECT_EQ(rollback_gate.diagnostic_projection_count, 1U);
    EXPECT_EQ(rollback_gate.derive_diagnostic_count, 1U);
    EXPECT_EQ(rollback_gate.empty_diagnostic_count, 0U);
    EXPECT_TRUE(rollback_gate.release_rollback_plan_complete);
    EXPECT_FALSE(rollback_gate.rollback_execution_enabled);

    const frontend::macro::BuiltinDeriveParserConsumptionAdmissionProtocol& admission_protocol =
        result.builtin_derive_parser_consumption_admission_protocols.front();
    EXPECT_EQ(admission_protocol.admission_query_name,
        "m23a-builtin-derive-parser-consumption-admission:0:0");
    EXPECT_EQ(admission_protocol.token_buffer_count, 1U);
    EXPECT_EQ(admission_protocol.token_record_count, result.generated_token_records.size());
    EXPECT_EQ(admission_protocol.derive_candidate_count, 1U);
    EXPECT_EQ(admission_protocol.empty_candidate_count, 0U);
    EXPECT_EQ(admission_protocol.blocked_diagnostic_count, 1U);
    EXPECT_FALSE(admission_protocol.parser_consumption_enabled);
    EXPECT_FALSE(admission_protocol.parser_admitted);

    const frontend::macro::BuiltinDeriveParserConsumptionCheckpointRollbackProtocol&
        checkpoint_protocol = result.builtin_derive_checkpoint_rollback_protocols.front();
    EXPECT_EQ(checkpoint_protocol.checkpoint_query_name,
        "m23b-builtin-derive-checkpoint-rollback:0:0");
    EXPECT_EQ(checkpoint_protocol.admission_protocol_identity,
        admission_protocol.admission_protocol_identity);
    EXPECT_EQ(checkpoint_protocol.checkpoint_count, 3U);
    EXPECT_EQ(checkpoint_protocol.rollback_plan_count, 3U);
    EXPECT_EQ(checkpoint_protocol.token_record_count, admission_protocol.token_record_count);
    EXPECT_EQ(checkpoint_protocol.diagnostic_anchor_count,
        admission_protocol.blocked_diagnostic_count);
    EXPECT_FALSE(checkpoint_protocol.rollback_execution_enabled);
    EXPECT_FALSE(checkpoint_protocol.parser_consumption_enabled);

    const frontend::macro::BuiltinDeriveParserPreConsumptionVerificationClosure& verification_closure =
        result.builtin_derive_preconsumption_verification_closures.front();
    EXPECT_EQ(verification_closure.verification_query_name,
        "m23c-builtin-derive-preconsumption-verification:0:0");
    EXPECT_EQ(verification_closure.admission_protocol_identity,
        admission_protocol.admission_protocol_identity);
    EXPECT_EQ(verification_closure.checkpoint_protocol_identity,
        checkpoint_protocol.checkpoint_protocol_identity);
    EXPECT_EQ(verification_closure.admission_protocol_count, 1U);
    EXPECT_EQ(verification_closure.checkpoint_protocol_count, 1U);
    EXPECT_EQ(verification_closure.hardening_matrix_count, 1U);
    EXPECT_EQ(verification_closure.debug_dump_contract_count, 1U);
    EXPECT_EQ(verification_closure.rollback_gate_count, 1U);
    EXPECT_FALSE(verification_closure.parser_consumption_enabled);
    EXPECT_FALSE(verification_closure.sema_visible);

    EXPECT_EQ(result.summary.builtin_derive_expansion_capability_candidate_count, 4U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_capability_count, 4U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_eq_capability_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_hash_capability_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_gate_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_release_hardening_matrix_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_debug_dump_contract_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_rollback_diagnostic_gate_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_consumption_admission_protocol_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_checkpoint_rollback_protocol_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_preconsumption_verification_closure_count, 1U);
    EXPECT_EQ(result.summary.user_generated_code_count, 0U);

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "duplicate_candidates=1");
    expect_contains(dump, "target_kind=enum");
    expect_contains(dump, "capabilities=4");
    expect_contains(dump, "builtin derive parser consumption release remains blocked in M22c");
    expect_contains(dump, "builtin derive release hardening matrix keeps parser consumption blocked in M22d");
    expect_contains(dump, "builtin derive rollback diagnostics remain design-only and parser-blocked in M22f");
    expect_contains(dump, "query=m23a-builtin-derive-parser-consumption-admission:0:0");
    expect_contains(dump, "query=m23b-builtin-derive-checkpoint-rollback:0:0");
    expect_contains(dump, "query=m23c-builtin-derive-preconsumption-verification:0:0");
    expect_contains(dump, "query=m26a-builtin-derive-parser-dry-run-admission:0:0");
    expect_contains(dump, "query=m26b-builtin-derive-error-recovery-shadow-diagnostic:0:0");
    expect_contains(dump, "query=m26c-builtin-derive-cursor-rollback-ast-verifier:0:0");
}

TEST(CoreUnit, EarlyItemExpansionBuiltinDeriveM22ReleaseGatesStayPartLocal)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy)]\n"
        "struct Primary { value: i32; }\n"
        "#[builder(flag)]\n"
        "struct Secondary { value: i32; }\n";

    syntax::AstModule module = parse_success(source);
    assign_single_module_ownership(module);
    ASSERT_EQ(module.items.size(), 2U);
    ASSERT_EQ(module.item_part_indices.size(), module.items.size());
    module.item_part_indices[0] = 0U;
    module.item_part_indices[1] = 1U;
    std::vector<std::vector<query::ModulePartKey>> part_keys = part_key_table(2U);

    auto expanded = frontend::macro::expand_early_item_macros_noop(module, part_keys);
    ASSERT_TRUE(expanded) << expanded.error().message;
    const frontend::macro::EarlyItemExpansionResult result = expanded.take_value();

    ASSERT_TRUE(frontend::macro::is_valid(result));
    ASSERT_EQ(result.inputs.size(), 2U);
    ASSERT_EQ(result.generated_parts.size(), 2U);
    ASSERT_EQ(result.parser_consumption_contract_gates.size(), 2U);
    ASSERT_EQ(result.builtin_derive_expansion_admissions.size(), 2U);
    ASSERT_EQ(result.builtin_derive_semantic_plans.size(), 2U);
    ASSERT_EQ(result.builtin_derive_parser_release_gates.size(), 2U);
    ASSERT_EQ(result.builtin_derive_release_hardening_matrices.size(), 2U);
    ASSERT_EQ(result.builtin_derive_debug_dump_contracts.size(), 2U);
    ASSERT_EQ(result.builtin_derive_rollback_diagnostic_gates.size(), 2U);
    ASSERT_EQ(result.builtin_derive_parser_consumption_admission_protocols.size(), 2U);
    ASSERT_EQ(result.builtin_derive_checkpoint_rollback_protocols.size(), 2U);
    ASSERT_EQ(result.builtin_derive_preconsumption_verification_closures.size(), 2U);
    ASSERT_EQ(result.builtin_derive_controlled_dry_run_adapters.size(), 2U);
    ASSERT_EQ(result.builtin_derive_dry_run_rollback_replays.size(), 2U);
    ASSERT_EQ(result.builtin_derive_dry_run_negative_matrices.size(), 2U);
    ASSERT_EQ(result.builtin_derive_parser_dry_run_sessions.size(), 2U);
    ASSERT_EQ(result.builtin_derive_token_cursor_snapshot_proofs.size(), 2U);
    ASSERT_EQ(result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.size(), 2U);
    ASSERT_EQ(result.builtin_derive_parser_dry_run_admission_gates.size(), 2U);
    ASSERT_EQ(result.builtin_derive_error_recovery_shadow_diagnostic_gates.size(), 2U);
    ASSERT_EQ(result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.size(), 2U);

    EXPECT_EQ(result.generated_parts[0].source_part_index, 0U);
    EXPECT_EQ(result.generated_parts[0].source_part, part_keys[0][0]);
    EXPECT_EQ(result.generated_parts[1].source_part_index, 1U);
    EXPECT_EQ(result.generated_parts[1].source_part, part_keys[0][1]);

    const frontend::macro::EarlyItemMacroInput& derive_input = result.inputs[0];
    const frontend::macro::EarlyItemMacroInput& builder_input = result.inputs[1];
    EXPECT_EQ(derive_input.part_index, 0U);
    EXPECT_EQ(builder_input.part_index, 1U);
    EXPECT_EQ(derive_input.attribute_name, "derive");
    EXPECT_EQ(builder_input.attribute_name, "builder");

    const frontend::macro::BuiltinDeriveExpansionAdmissionGate* const derive_admission =
        builtin_derive_admission_for_input(result, derive_input);
    ASSERT_NE(derive_admission, nullptr);
    EXPECT_EQ(derive_admission->query_name,
        "m22a-builtin-derive-admission:0:0:0:0:derive");
    EXPECT_EQ(derive_admission->capability_candidate_count, 1U);
    EXPECT_TRUE(derive_admission->builtin_derive_input);

    const frontend::macro::BuiltinDeriveExpansionAdmissionGate* const builder_admission =
        builtin_derive_admission_for_input(result, builder_input);
    ASSERT_NE(builder_admission, nullptr);
    EXPECT_EQ(builder_admission->query_name,
        "m22a-builtin-derive-admission:0:1:1:0:builder");
    EXPECT_EQ(builder_admission->capability_candidate_count, 0U);
    EXPECT_FALSE(builder_admission->builtin_derive_input);

    const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate* const primary_release =
        builtin_derive_parser_release_gate_for_part(result, result.generated_parts[0]);
    ASSERT_NE(primary_release, nullptr);
    EXPECT_EQ(primary_release->release_query_name, "m22c-builtin-derive-parser-release:0:0");
    EXPECT_EQ(primary_release->admission_count, 1U);
    EXPECT_EQ(primary_release->derive_admission_count, 1U);
    EXPECT_EQ(primary_release->semantic_plan_count, 1U);
    EXPECT_EQ(primary_release->capability_total_count, 1U);
    EXPECT_FALSE(primary_release->parser_consumption_enabled);

    const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate* const secondary_release =
        builtin_derive_parser_release_gate_for_part(result, result.generated_parts[1]);
    ASSERT_NE(secondary_release, nullptr);
    EXPECT_EQ(secondary_release->release_query_name, "m22c-builtin-derive-parser-release:0:1");
    EXPECT_EQ(secondary_release->admission_count, 1U);
    EXPECT_EQ(secondary_release->derive_admission_count, 0U);
    EXPECT_EQ(secondary_release->semantic_plan_count, 1U);
    EXPECT_EQ(secondary_release->capability_total_count, 0U);
    EXPECT_FALSE(secondary_release->parser_consumption_enabled);
    EXPECT_NE(primary_release->admission_group_identity,
        secondary_release->admission_group_identity);
    EXPECT_NE(primary_release->semantic_plan_group_identity,
        secondary_release->semantic_plan_group_identity);
    EXPECT_NE(primary_release->release_gate_identity, secondary_release->release_gate_identity);

    const frontend::macro::BuiltinDeriveReleaseHardeningMatrix* const primary_matrix =
        builtin_derive_release_hardening_matrix_for_part(result, result.generated_parts[0]);
    ASSERT_NE(primary_matrix, nullptr);
    EXPECT_EQ(primary_matrix->hardening_query_name, "m22d-builtin-derive-release-hardening:0:0");
    EXPECT_EQ(primary_matrix->part_local_admission_count, 1U);
    EXPECT_EQ(primary_matrix->part_local_derive_admission_count, 1U);
    EXPECT_EQ(primary_matrix->global_admission_count, 2U);
    EXPECT_EQ(primary_matrix->cross_part_admission_count, 1U);
    EXPECT_EQ(primary_matrix->global_generated_part_count, 2U);
    EXPECT_TRUE(primary_matrix->part_locality_preserved);

    const frontend::macro::BuiltinDeriveReleaseHardeningMatrix* const secondary_matrix =
        builtin_derive_release_hardening_matrix_for_part(result, result.generated_parts[1]);
    ASSERT_NE(secondary_matrix, nullptr);
    EXPECT_EQ(secondary_matrix->hardening_query_name, "m22d-builtin-derive-release-hardening:0:1");
    EXPECT_EQ(secondary_matrix->part_local_admission_count, 1U);
    EXPECT_EQ(secondary_matrix->part_local_derive_admission_count, 0U);
    EXPECT_EQ(secondary_matrix->global_admission_count, 2U);
    EXPECT_EQ(secondary_matrix->cross_part_admission_count, 1U);
    EXPECT_NE(primary_matrix->hardening_matrix_identity, secondary_matrix->hardening_matrix_identity);

    const frontend::macro::BuiltinDeriveDebugDumpStabilityContract* const primary_debug =
        builtin_derive_debug_dump_contract_for_part(result, result.generated_parts[0]);
    ASSERT_NE(primary_debug, nullptr);
    EXPECT_EQ(primary_debug->debug_dump_query_name, "m22e-builtin-derive-debug-dump:0:0");

    const frontend::macro::BuiltinDeriveDebugDumpStabilityContract* const secondary_debug =
        builtin_derive_debug_dump_contract_for_part(result, result.generated_parts[1]);
    ASSERT_NE(secondary_debug, nullptr);
    EXPECT_EQ(secondary_debug->debug_dump_query_name, "m22e-builtin-derive-debug-dump:0:1");
    EXPECT_NE(primary_debug->debug_dump_contract_identity, secondary_debug->debug_dump_contract_identity);

    const frontend::macro::BuiltinDeriveRollbackDiagnosticDesignGate* const primary_rollback =
        builtin_derive_rollback_diagnostic_gate_for_part(result, result.generated_parts[0]);
    ASSERT_NE(primary_rollback, nullptr);
    EXPECT_EQ(primary_rollback->rollback_query_name, "m22f-builtin-derive-rollback-diagnostic:0:0");
    EXPECT_EQ(primary_rollback->diagnostic_projection_count, 1U);
    EXPECT_EQ(primary_rollback->derive_diagnostic_count, 1U);
    EXPECT_EQ(primary_rollback->empty_diagnostic_count, 0U);

    const frontend::macro::BuiltinDeriveRollbackDiagnosticDesignGate* const secondary_rollback =
        builtin_derive_rollback_diagnostic_gate_for_part(result, result.generated_parts[1]);
    ASSERT_NE(secondary_rollback, nullptr);
    EXPECT_EQ(secondary_rollback->rollback_query_name, "m22f-builtin-derive-rollback-diagnostic:0:1");
    EXPECT_EQ(secondary_rollback->diagnostic_projection_count, 1U);
    EXPECT_EQ(secondary_rollback->derive_diagnostic_count, 0U);
    EXPECT_EQ(secondary_rollback->empty_diagnostic_count, 1U);
    EXPECT_NE(primary_rollback->rollback_gate_identity, secondary_rollback->rollback_gate_identity);

    const frontend::macro::BuiltinDeriveParserConsumptionAdmissionProtocol* const primary_admission_protocol =
        builtin_derive_parser_consumption_admission_protocol_for_part(result, result.generated_parts[0]);
    ASSERT_NE(primary_admission_protocol, nullptr);
    EXPECT_EQ(primary_admission_protocol->admission_query_name,
        "m23a-builtin-derive-parser-consumption-admission:0:0");
    EXPECT_EQ(primary_admission_protocol->parser_consumption_contract_identity,
        result.parser_consumption_contract_gates[0].contract_identity);
    EXPECT_EQ(primary_admission_protocol->release_gate_identity, primary_release->release_gate_identity);
    EXPECT_EQ(primary_admission_protocol->rollback_gate_identity, primary_rollback->rollback_gate_identity);
    EXPECT_EQ(primary_admission_protocol->token_buffer_count, 1U);
    EXPECT_EQ(primary_admission_protocol->derive_candidate_count, 1U);
    EXPECT_EQ(primary_admission_protocol->empty_candidate_count, 0U);
    EXPECT_EQ(primary_admission_protocol->blocked_diagnostic_count, 1U);
    EXPECT_FALSE(primary_admission_protocol->parser_consumption_enabled);

    const frontend::macro::BuiltinDeriveParserConsumptionAdmissionProtocol* const secondary_admission_protocol =
        builtin_derive_parser_consumption_admission_protocol_for_part(result, result.generated_parts[1]);
    ASSERT_NE(secondary_admission_protocol, nullptr);
    EXPECT_EQ(secondary_admission_protocol->admission_query_name,
        "m23a-builtin-derive-parser-consumption-admission:0:1");
    EXPECT_EQ(secondary_admission_protocol->parser_consumption_contract_identity,
        result.parser_consumption_contract_gates[1].contract_identity);
    EXPECT_EQ(secondary_admission_protocol->release_gate_identity,
        secondary_release->release_gate_identity);
    EXPECT_EQ(secondary_admission_protocol->rollback_gate_identity,
        secondary_rollback->rollback_gate_identity);
    EXPECT_EQ(secondary_admission_protocol->token_buffer_count, 1U);
    EXPECT_EQ(secondary_admission_protocol->derive_candidate_count, 0U);
    EXPECT_EQ(secondary_admission_protocol->empty_candidate_count, 1U);
    EXPECT_EQ(secondary_admission_protocol->blocked_diagnostic_count, 1U);
    EXPECT_FALSE(secondary_admission_protocol->parser_consumption_enabled);
    EXPECT_NE(primary_admission_protocol->admission_protocol_identity,
        secondary_admission_protocol->admission_protocol_identity);

    const frontend::macro::BuiltinDeriveParserConsumptionCheckpointRollbackProtocol* const
        primary_checkpoint = builtin_derive_checkpoint_rollback_protocol_for_part(result, result.generated_parts[0]);
    ASSERT_NE(primary_checkpoint, nullptr);
    EXPECT_EQ(primary_checkpoint->checkpoint_query_name,
        "m23b-builtin-derive-checkpoint-rollback:0:0");
    EXPECT_EQ(primary_checkpoint->admission_protocol_identity,
        primary_admission_protocol->admission_protocol_identity);
    EXPECT_EQ(primary_checkpoint->checkpoint_count, 3U);
    EXPECT_EQ(primary_checkpoint->rollback_plan_count, 3U);
    EXPECT_EQ(primary_checkpoint->diagnostic_anchor_count, 1U);

    const frontend::macro::BuiltinDeriveParserConsumptionCheckpointRollbackProtocol* const
        secondary_checkpoint =
            builtin_derive_checkpoint_rollback_protocol_for_part(result, result.generated_parts[1]);
    ASSERT_NE(secondary_checkpoint, nullptr);
    EXPECT_EQ(secondary_checkpoint->checkpoint_query_name,
        "m23b-builtin-derive-checkpoint-rollback:0:1");
    EXPECT_EQ(secondary_checkpoint->admission_protocol_identity,
        secondary_admission_protocol->admission_protocol_identity);
    EXPECT_EQ(secondary_checkpoint->checkpoint_count, 3U);
    EXPECT_EQ(secondary_checkpoint->rollback_plan_count, 3U);
    EXPECT_EQ(secondary_checkpoint->diagnostic_anchor_count, 1U);
    EXPECT_NE(primary_checkpoint->checkpoint_protocol_identity,
        secondary_checkpoint->checkpoint_protocol_identity);

    const frontend::macro::BuiltinDeriveParserPreConsumptionVerificationClosure* const
        primary_verification =
            builtin_derive_preconsumption_verification_closure_for_part(result, result.generated_parts[0]);
    ASSERT_NE(primary_verification, nullptr);
    EXPECT_EQ(primary_verification->verification_query_name,
        "m23c-builtin-derive-preconsumption-verification:0:0");
    EXPECT_EQ(primary_verification->admission_protocol_identity,
        primary_admission_protocol->admission_protocol_identity);
    EXPECT_EQ(primary_verification->checkpoint_protocol_identity,
        primary_checkpoint->checkpoint_protocol_identity);
    EXPECT_EQ(primary_verification->debug_dump_contract_identity,
        primary_debug->debug_dump_contract_identity);
    EXPECT_FALSE(primary_verification->parser_consumption_enabled);
    EXPECT_FALSE(primary_verification->sema_visible);

    const frontend::macro::BuiltinDeriveParserPreConsumptionVerificationClosure* const
        secondary_verification =
            builtin_derive_preconsumption_verification_closure_for_part(result, result.generated_parts[1]);
    ASSERT_NE(secondary_verification, nullptr);
    EXPECT_EQ(secondary_verification->verification_query_name,
        "m23c-builtin-derive-preconsumption-verification:0:1");
    EXPECT_EQ(secondary_verification->admission_protocol_identity,
        secondary_admission_protocol->admission_protocol_identity);
    EXPECT_EQ(secondary_verification->checkpoint_protocol_identity,
        secondary_checkpoint->checkpoint_protocol_identity);
    EXPECT_EQ(secondary_verification->debug_dump_contract_identity,
        secondary_debug->debug_dump_contract_identity);
    EXPECT_FALSE(secondary_verification->parser_consumption_enabled);
    EXPECT_FALSE(secondary_verification->sema_visible);
    EXPECT_NE(primary_verification->verification_closure_identity,
        secondary_verification->verification_closure_identity);

    const frontend::macro::BuiltinDeriveControlledParserDryRunAdapter* const primary_adapter =
        builtin_derive_controlled_dry_run_adapter_for_part(result, result.generated_parts[0]);
    ASSERT_NE(primary_adapter, nullptr);
    EXPECT_EQ(primary_adapter->adapter_query_name,
        "m24a-builtin-derive-controlled-parser-dry-run:0:0");
    EXPECT_EQ(primary_adapter->verification_closure_identity,
        primary_verification->verification_closure_identity);
    EXPECT_EQ(primary_adapter->admission_protocol_identity,
        primary_admission_protocol->admission_protocol_identity);
    EXPECT_EQ(primary_adapter->checkpoint_protocol_identity,
        primary_checkpoint->checkpoint_protocol_identity);
    EXPECT_EQ(primary_adapter->token_record_count, primary_checkpoint->token_record_count);
    EXPECT_GT(primary_adapter->token_record_count, 0U);
    EXPECT_EQ(primary_adapter->diagnostic_anchor_count, 1U);
    EXPECT_TRUE(primary_adapter->dry_run_adapter_complete);
    EXPECT_FALSE(primary_adapter->dry_run_executed);
    EXPECT_FALSE(primary_adapter->parser_consumption_enabled);

    const frontend::macro::BuiltinDeriveControlledParserDryRunAdapter* const secondary_adapter =
        builtin_derive_controlled_dry_run_adapter_for_part(result, result.generated_parts[1]);
    ASSERT_NE(secondary_adapter, nullptr);
    EXPECT_EQ(secondary_adapter->adapter_query_name,
        "m24a-builtin-derive-controlled-parser-dry-run:0:1");
    EXPECT_EQ(secondary_adapter->verification_closure_identity,
        secondary_verification->verification_closure_identity);
    EXPECT_EQ(secondary_adapter->admission_protocol_identity,
        secondary_admission_protocol->admission_protocol_identity);
    EXPECT_EQ(secondary_adapter->checkpoint_protocol_identity,
        secondary_checkpoint->checkpoint_protocol_identity);
    EXPECT_EQ(secondary_adapter->token_record_count, secondary_checkpoint->token_record_count);
    EXPECT_EQ(secondary_adapter->token_record_count, 0U);
    EXPECT_EQ(secondary_adapter->diagnostic_anchor_count, 1U);
    EXPECT_TRUE(secondary_adapter->dry_run_adapter_complete);
    EXPECT_FALSE(secondary_adapter->dry_run_executed);
    EXPECT_FALSE(secondary_adapter->parser_consumption_enabled);
    EXPECT_NE(primary_adapter->dry_run_adapter_identity, secondary_adapter->dry_run_adapter_identity);

    const frontend::macro::BuiltinDeriveDryRunRollbackDiagnosticReplay* const primary_replay =
        builtin_derive_dry_run_rollback_replay_for_part(result, result.generated_parts[0]);
    ASSERT_NE(primary_replay, nullptr);
    EXPECT_EQ(primary_replay->replay_query_name,
        "m24b-builtin-derive-dry-run-rollback-replay:0:0");
    EXPECT_EQ(primary_replay->dry_run_adapter_identity, primary_adapter->dry_run_adapter_identity);
    EXPECT_EQ(primary_replay->checkpoint_protocol_identity,
        primary_checkpoint->checkpoint_protocol_identity);
    EXPECT_EQ(primary_replay->rollback_gate_identity, primary_rollback->rollback_gate_identity);
    EXPECT_EQ(primary_replay->planned_replay_count, primary_replay->diagnostic_anchor_count);
    EXPECT_EQ(primary_replay->executed_replay_count, 0U);
    EXPECT_FALSE(primary_replay->replay_execution_enabled);
    EXPECT_FALSE(primary_replay->dry_run_executed);

    const frontend::macro::BuiltinDeriveDryRunRollbackDiagnosticReplay* const secondary_replay =
        builtin_derive_dry_run_rollback_replay_for_part(result, result.generated_parts[1]);
    ASSERT_NE(secondary_replay, nullptr);
    EXPECT_EQ(secondary_replay->replay_query_name,
        "m24b-builtin-derive-dry-run-rollback-replay:0:1");
    EXPECT_EQ(secondary_replay->dry_run_adapter_identity, secondary_adapter->dry_run_adapter_identity);
    EXPECT_EQ(secondary_replay->checkpoint_protocol_identity,
        secondary_checkpoint->checkpoint_protocol_identity);
    EXPECT_EQ(secondary_replay->rollback_gate_identity, secondary_rollback->rollback_gate_identity);
    EXPECT_EQ(secondary_replay->planned_replay_count, secondary_replay->diagnostic_anchor_count);
    EXPECT_EQ(secondary_replay->executed_replay_count, 0U);
    EXPECT_FALSE(secondary_replay->replay_execution_enabled);
    EXPECT_FALSE(secondary_replay->dry_run_executed);
    EXPECT_NE(primary_replay->replay_protocol_identity, secondary_replay->replay_protocol_identity);

    const frontend::macro::BuiltinDeriveDryRunNegativeMatrixClosure* const primary_negative_matrix =
        builtin_derive_dry_run_negative_matrix_for_part(result, result.generated_parts[0]);
    ASSERT_NE(primary_negative_matrix, nullptr);
    EXPECT_EQ(primary_negative_matrix->matrix_query_name,
        "m24c-builtin-derive-dry-run-negative-matrix:0:0");
    EXPECT_EQ(primary_negative_matrix->dry_run_adapter_identity,
        primary_adapter->dry_run_adapter_identity);
    EXPECT_EQ(primary_negative_matrix->rollback_replay_identity,
        primary_replay->replay_protocol_identity);
    EXPECT_EQ(primary_negative_matrix->verification_closure_identity,
        primary_verification->verification_closure_identity);
    EXPECT_EQ(primary_negative_matrix->negative_case_count, 8U);
    EXPECT_EQ(primary_negative_matrix->parser_consumable_case_count, 0U);
    EXPECT_FALSE(primary_negative_matrix->dry_run_executed);
    EXPECT_FALSE(primary_negative_matrix->parser_consumption_enabled);

    const frontend::macro::BuiltinDeriveDryRunNegativeMatrixClosure* const secondary_negative_matrix =
        builtin_derive_dry_run_negative_matrix_for_part(result, result.generated_parts[1]);
    ASSERT_NE(secondary_negative_matrix, nullptr);
    EXPECT_EQ(secondary_negative_matrix->matrix_query_name,
        "m24c-builtin-derive-dry-run-negative-matrix:0:1");
    EXPECT_EQ(secondary_negative_matrix->dry_run_adapter_identity,
        secondary_adapter->dry_run_adapter_identity);
    EXPECT_EQ(secondary_negative_matrix->rollback_replay_identity,
        secondary_replay->replay_protocol_identity);
    EXPECT_EQ(secondary_negative_matrix->verification_closure_identity,
        secondary_verification->verification_closure_identity);
    EXPECT_EQ(secondary_negative_matrix->negative_case_count, 8U);
    EXPECT_EQ(secondary_negative_matrix->parser_consumable_case_count, 0U);
    EXPECT_FALSE(secondary_negative_matrix->dry_run_executed);
    EXPECT_FALSE(secondary_negative_matrix->parser_consumption_enabled);
    EXPECT_NE(primary_negative_matrix->negative_matrix_identity,
        secondary_negative_matrix->negative_matrix_identity);

    const frontend::macro::BuiltinDeriveParserDryRunSessionBoundary* const primary_session =
        builtin_derive_parser_dry_run_session_for_part(result, result.generated_parts[0]);
    ASSERT_NE(primary_session, nullptr);
    EXPECT_EQ(primary_session->session_query_name, "m25a-builtin-derive-dry-run-session:0:0");
    EXPECT_EQ(primary_session->dry_run_adapter_identity, primary_adapter->dry_run_adapter_identity);
    EXPECT_EQ(primary_session->negative_matrix_identity, primary_negative_matrix->negative_matrix_identity);
    EXPECT_EQ(primary_session->token_buffer_candidate_count, 1U);
    EXPECT_EQ(primary_session->token_record_count, primary_adapter->token_record_count);
    EXPECT_EQ(primary_session->diagnostic_anchor_count, primary_adapter->diagnostic_anchor_count);
    EXPECT_EQ(primary_session->committed_parse_count, 0U);
    EXPECT_TRUE(primary_session->sandbox_available);
    EXPECT_TRUE(primary_session->check_only);
    EXPECT_FALSE(primary_session->dry_run_executed);
    EXPECT_FALSE(primary_session->session_committed);
    EXPECT_FALSE(primary_session->parser_cursor_advanced);

    const frontend::macro::BuiltinDeriveParserDryRunSessionBoundary* const secondary_session =
        builtin_derive_parser_dry_run_session_for_part(result, result.generated_parts[1]);
    ASSERT_NE(secondary_session, nullptr);
    EXPECT_EQ(secondary_session->session_query_name, "m25a-builtin-derive-dry-run-session:0:1");
    EXPECT_EQ(secondary_session->dry_run_adapter_identity, secondary_adapter->dry_run_adapter_identity);
    EXPECT_EQ(secondary_session->negative_matrix_identity,
        secondary_negative_matrix->negative_matrix_identity);
    EXPECT_EQ(secondary_session->token_buffer_candidate_count, 0U);
    EXPECT_EQ(secondary_session->token_record_count, 0U);
    EXPECT_EQ(secondary_session->diagnostic_anchor_count, secondary_adapter->diagnostic_anchor_count);
    EXPECT_EQ(secondary_session->committed_parse_count, 0U);
    EXPECT_TRUE(secondary_session->sandbox_available);
    EXPECT_TRUE(secondary_session->check_only);
    EXPECT_FALSE(secondary_session->dry_run_executed);
    EXPECT_FALSE(secondary_session->session_committed);
    EXPECT_FALSE(secondary_session->parser_cursor_advanced);
    EXPECT_NE(primary_session->dry_run_session_identity, secondary_session->dry_run_session_identity);

    const frontend::macro::BuiltinDeriveTokenCursorSnapshotRollbackProof* const primary_cursor_proof =
        builtin_derive_token_cursor_snapshot_proof_for_part(result, result.generated_parts[0]);
    ASSERT_NE(primary_cursor_proof, nullptr);
    EXPECT_EQ(primary_cursor_proof->snapshot_query_name,
        "m25b-builtin-derive-token-cursor-rollback-proof:0:0");
    EXPECT_EQ(primary_cursor_proof->dry_run_session_identity, primary_session->dry_run_session_identity);
    EXPECT_EQ(primary_cursor_proof->rollback_replay_identity, primary_replay->replay_protocol_identity);
    EXPECT_EQ(primary_cursor_proof->cursor_snapshot_count, 3U);
    EXPECT_EQ(primary_cursor_proof->rollback_proof_count, 3U);
    EXPECT_EQ(primary_cursor_proof->cursor_commit_count, 0U);
    EXPECT_FALSE(primary_cursor_proof->parser_cursor_advanced);

    const frontend::macro::BuiltinDeriveTokenCursorSnapshotRollbackProof* const secondary_cursor_proof =
        builtin_derive_token_cursor_snapshot_proof_for_part(result, result.generated_parts[1]);
    ASSERT_NE(secondary_cursor_proof, nullptr);
    EXPECT_EQ(secondary_cursor_proof->snapshot_query_name,
        "m25b-builtin-derive-token-cursor-rollback-proof:0:1");
    EXPECT_EQ(secondary_cursor_proof->dry_run_session_identity,
        secondary_session->dry_run_session_identity);
    EXPECT_EQ(secondary_cursor_proof->rollback_replay_identity,
        secondary_replay->replay_protocol_identity);
    EXPECT_EQ(secondary_cursor_proof->token_record_count, 0U);
    EXPECT_EQ(secondary_cursor_proof->cursor_snapshot_count, 3U);
    EXPECT_EQ(secondary_cursor_proof->rollback_proof_count, 3U);
    EXPECT_EQ(secondary_cursor_proof->cursor_commit_count, 0U);
    EXPECT_FALSE(secondary_cursor_proof->parser_cursor_advanced);
    EXPECT_NE(primary_cursor_proof->cursor_snapshot_identity,
        secondary_cursor_proof->cursor_snapshot_identity);

    const frontend::macro::BuiltinDeriveDiagnosticShadowNoAstMutationClosure* const
        primary_shadow_closure =
            builtin_derive_diagnostic_shadow_no_ast_mutation_closure_for_part(
                result, result.generated_parts[0]);
    ASSERT_NE(primary_shadow_closure, nullptr);
    EXPECT_EQ(primary_shadow_closure->closure_query_name,
        "m25c-builtin-derive-diagnostic-shadow-no-ast-mutation:0:0");
    EXPECT_EQ(primary_shadow_closure->dry_run_session_identity, primary_session->dry_run_session_identity);
    EXPECT_EQ(primary_shadow_closure->cursor_snapshot_identity,
        primary_cursor_proof->cursor_snapshot_identity);
    EXPECT_EQ(primary_shadow_closure->rollback_replay_identity, primary_replay->replay_protocol_identity);
    EXPECT_EQ(primary_shadow_closure->diagnostic_shadow_count, primary_replay->planned_replay_count);
    EXPECT_EQ(primary_shadow_closure->executed_shadow_count, 0U);
    EXPECT_EQ(primary_shadow_closure->ast_mutation_count, 0U);
    EXPECT_TRUE(primary_shadow_closure->no_ast_mutation_verified);
    EXPECT_FALSE(primary_shadow_closure->dry_run_executed);
    EXPECT_FALSE(primary_shadow_closure->parser_consumption_enabled);

    const frontend::macro::BuiltinDeriveDiagnosticShadowNoAstMutationClosure* const
        secondary_shadow_closure =
            builtin_derive_diagnostic_shadow_no_ast_mutation_closure_for_part(
                result, result.generated_parts[1]);
    ASSERT_NE(secondary_shadow_closure, nullptr);
    EXPECT_EQ(secondary_shadow_closure->closure_query_name,
        "m25c-builtin-derive-diagnostic-shadow-no-ast-mutation:0:1");
    EXPECT_EQ(secondary_shadow_closure->dry_run_session_identity,
        secondary_session->dry_run_session_identity);
    EXPECT_EQ(secondary_shadow_closure->cursor_snapshot_identity,
        secondary_cursor_proof->cursor_snapshot_identity);
    EXPECT_EQ(secondary_shadow_closure->rollback_replay_identity,
        secondary_replay->replay_protocol_identity);
    EXPECT_EQ(secondary_shadow_closure->diagnostic_shadow_count,
        secondary_replay->planned_replay_count);
    EXPECT_EQ(secondary_shadow_closure->executed_shadow_count, 0U);
    EXPECT_EQ(secondary_shadow_closure->ast_mutation_count, 0U);
    EXPECT_TRUE(secondary_shadow_closure->no_ast_mutation_verified);
    EXPECT_FALSE(secondary_shadow_closure->dry_run_executed);
    EXPECT_FALSE(secondary_shadow_closure->parser_consumption_enabled);
    EXPECT_NE(primary_shadow_closure->closure_identity, secondary_shadow_closure->closure_identity);

    EXPECT_EQ(result.summary.builtin_derive_expansion_admission_gate_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_derive_admission_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_non_derive_blocked_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_plan_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_capability_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_gate_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_release_hardening_matrix_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_debug_dump_contract_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_rollback_diagnostic_gate_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_parser_consumption_admission_protocol_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_parser_consumption_admission_complete_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_parser_consumption_admission_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_checkpoint_rollback_protocol_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_checkpoint_rollback_complete_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_checkpoint_rollback_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_preconsumption_verification_closure_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_preconsumption_verification_complete_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_preconsumption_verification_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_controlled_dry_run_adapter_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_controlled_dry_run_adapter_visible_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_controlled_dry_run_adapter_query_reusable_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_controlled_dry_run_adapter_complete_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_controlled_dry_run_adapter_executed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_rollback_replay_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_rollback_replay_visible_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_rollback_replay_query_reusable_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_rollback_replay_complete_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_rollback_replay_executed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_negative_matrix_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_negative_matrix_visible_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_negative_matrix_query_reusable_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_negative_matrix_complete_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_negative_matrix_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_session_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_session_visible_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_session_query_reusable_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_session_complete_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_session_executed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_session_committed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_token_cursor_snapshot_proof_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_token_cursor_snapshot_proof_visible_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_token_cursor_snapshot_proof_query_reusable_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_token_cursor_snapshot_proof_complete_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_token_cursor_snapshot_proof_cursor_advanced_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_token_cursor_snapshot_proof_committed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_closure_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_visible_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_query_reusable_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_complete_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_executed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_ast_mutation_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_parser_consumable_count,
        0U);
    EXPECT_EQ(result.summary.ast_mutation_count, 0U);

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "query=m22c-builtin-derive-parser-release:0:0");
    expect_contains(dump, "query=m22c-builtin-derive-parser-release:0:1");
    expect_contains(dump, "query=m22d-builtin-derive-release-hardening:0:0");
    expect_contains(dump, "query=m22d-builtin-derive-release-hardening:0:1");
    expect_contains(dump, "cross_part_admissions=1");
    expect_contains(dump, "query=m22f-builtin-derive-rollback-diagnostic:0:1");
    expect_contains(dump, "query=m23a-builtin-derive-parser-consumption-admission:0:0");
    expect_contains(dump, "query=m23a-builtin-derive-parser-consumption-admission:0:1");
    expect_contains(dump, "query=m23b-builtin-derive-checkpoint-rollback:0:0");
    expect_contains(dump, "query=m23b-builtin-derive-checkpoint-rollback:0:1");
    expect_contains(dump, "query=m23c-builtin-derive-preconsumption-verification:0:0");
    expect_contains(dump, "query=m23c-builtin-derive-preconsumption-verification:0:1");
    expect_contains(dump, "query=m24a-builtin-derive-controlled-parser-dry-run:0:0");
    expect_contains(dump, "query=m24a-builtin-derive-controlled-parser-dry-run:0:1");
    expect_contains(dump, "query=m24b-builtin-derive-dry-run-rollback-replay:0:0");
    expect_contains(dump, "query=m24b-builtin-derive-dry-run-rollback-replay:0:1");
    expect_contains(dump, "query=m24c-builtin-derive-dry-run-negative-matrix:0:0");
    expect_contains(dump, "query=m24c-builtin-derive-dry-run-negative-matrix:0:1");
    expect_contains(dump, "source_part=1");
    expect_contains(dump, "part=1");
}
} // namespace aurex::test
