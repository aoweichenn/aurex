#pragma once

#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_common_support.hpp>

#include <algorithm>

namespace aurex::test::early_item_expansion_support {

[[nodiscard]] inline const frontend::macro::BuiltinDeriveExpansionAdmissionGate*
builtin_derive_admission_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.builtin_derive_expansion_admissions.begin(),
        result.builtin_derive_expansion_admissions.end(),
        [&input](const frontend::macro::BuiltinDeriveExpansionAdmissionGate& gate) {
            return gate.item.value == input.item.value
                && gate.module.value == input.module.value
                && gate.attribute_index == input.attribute_index;
        });
    return found == result.builtin_derive_expansion_admissions.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveSemanticExpansionPlan*
builtin_derive_semantic_plan_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.builtin_derive_semantic_plans.begin(),
        result.builtin_derive_semantic_plans.end(),
        [&input](const frontend::macro::BuiltinDeriveSemanticExpansionPlan& plan) {
            return plan.item.value == input.item.value
                && plan.module.value == input.module.value
                && plan.attribute_index == input.attribute_index;
        });
    return found == result.builtin_derive_semantic_plans.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate*
builtin_derive_parser_release_gate_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_parser_release_gates.begin(),
        result.builtin_derive_parser_release_gates.end(),
        [&generated_part](const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate& gate) {
            return gate.module.value == generated_part.module.value
                && gate.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_parser_release_gates.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveReleaseHardeningMatrix*
builtin_derive_release_hardening_matrix_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_release_hardening_matrices.begin(),
        result.builtin_derive_release_hardening_matrices.end(),
        [&generated_part](const frontend::macro::BuiltinDeriveReleaseHardeningMatrix& matrix) {
            return matrix.module.value == generated_part.module.value
                && matrix.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_release_hardening_matrices.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveDebugDumpStabilityContract*
builtin_derive_debug_dump_contract_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_debug_dump_contracts.begin(),
        result.builtin_derive_debug_dump_contracts.end(),
        [&generated_part](const frontend::macro::BuiltinDeriveDebugDumpStabilityContract& contract) {
            return contract.module.value == generated_part.module.value
                && contract.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_debug_dump_contracts.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveRollbackDiagnosticDesignGate*
builtin_derive_rollback_diagnostic_gate_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_rollback_diagnostic_gates.begin(),
        result.builtin_derive_rollback_diagnostic_gates.end(),
        [&generated_part](const frontend::macro::BuiltinDeriveRollbackDiagnosticDesignGate& gate) {
            return gate.module.value == generated_part.module.value
                && gate.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_rollback_diagnostic_gates.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveParserConsumptionAdmissionProtocol*
builtin_derive_parser_consumption_admission_protocol_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_parser_consumption_admission_protocols.begin(),
        result.builtin_derive_parser_consumption_admission_protocols.end(),
        [&generated_part](const frontend::macro::BuiltinDeriveParserConsumptionAdmissionProtocol& protocol) {
            return protocol.module.value == generated_part.module.value
                && protocol.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_parser_consumption_admission_protocols.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveParserConsumptionCheckpointRollbackProtocol*
builtin_derive_checkpoint_rollback_protocol_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_checkpoint_rollback_protocols.begin(),
        result.builtin_derive_checkpoint_rollback_protocols.end(),
        [&generated_part](
            const frontend::macro::BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& protocol) {
            return protocol.module.value == generated_part.module.value
                && protocol.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_checkpoint_rollback_protocols.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveParserPreConsumptionVerificationClosure*
builtin_derive_preconsumption_verification_closure_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_preconsumption_verification_closures.begin(),
        result.builtin_derive_preconsumption_verification_closures.end(),
        [&generated_part](const frontend::macro::BuiltinDeriveParserPreConsumptionVerificationClosure& closure) {
            return closure.module.value == generated_part.module.value
                && closure.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_preconsumption_verification_closures.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveControlledParserDryRunAdapter*
builtin_derive_controlled_dry_run_adapter_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_controlled_dry_run_adapters.begin(),
        result.builtin_derive_controlled_dry_run_adapters.end(),
        [&generated_part](const frontend::macro::BuiltinDeriveControlledParserDryRunAdapter& adapter) {
            return adapter.module.value == generated_part.module.value
                && adapter.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_controlled_dry_run_adapters.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveDryRunRollbackDiagnosticReplay*
builtin_derive_dry_run_rollback_replay_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_dry_run_rollback_replays.begin(),
        result.builtin_derive_dry_run_rollback_replays.end(),
        [&generated_part](const frontend::macro::BuiltinDeriveDryRunRollbackDiagnosticReplay& replay) {
            return replay.module.value == generated_part.module.value
                && replay.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_dry_run_rollback_replays.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveDryRunNegativeMatrixClosure*
builtin_derive_dry_run_negative_matrix_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_dry_run_negative_matrices.begin(),
        result.builtin_derive_dry_run_negative_matrices.end(),
        [&generated_part](const frontend::macro::BuiltinDeriveDryRunNegativeMatrixClosure& matrix) {
            return matrix.module.value == generated_part.module.value
                && matrix.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_dry_run_negative_matrices.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveParserDryRunSessionBoundary*
builtin_derive_parser_dry_run_session_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_parser_dry_run_sessions.begin(),
        result.builtin_derive_parser_dry_run_sessions.end(),
        [&generated_part](const frontend::macro::BuiltinDeriveParserDryRunSessionBoundary& session) {
            return session.module.value == generated_part.module.value
                && session.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_parser_dry_run_sessions.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveTokenCursorSnapshotRollbackProof*
builtin_derive_token_cursor_snapshot_proof_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_token_cursor_snapshot_proofs.begin(),
        result.builtin_derive_token_cursor_snapshot_proofs.end(),
        [&generated_part](const frontend::macro::BuiltinDeriveTokenCursorSnapshotRollbackProof& proof) {
            return proof.module.value == generated_part.module.value
                && proof.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_token_cursor_snapshot_proofs.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveDiagnosticShadowNoAstMutationClosure*
builtin_derive_diagnostic_shadow_no_ast_mutation_closure_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found =
        std::find_if(result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.begin(),
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.end(),
            [&generated_part](
                const frontend::macro::BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure) {
                return closure.module.value == generated_part.module.value
                    && closure.source_part_index == generated_part.source_part_index;
            });
    return found == result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.end()
        ? nullptr
        : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveParserDryRunAdmissionGate*
builtin_derive_parser_dry_run_admission_gate_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_parser_dry_run_admission_gates.begin(),
        result.builtin_derive_parser_dry_run_admission_gates.end(),
        [&generated_part](const frontend::macro::BuiltinDeriveParserDryRunAdmissionGate& gate) {
            return gate.module.value == generated_part.module.value
                && gate.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_parser_dry_run_admission_gates.end() ? nullptr : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveErrorRecoveryShadowDiagnosticGate*
builtin_derive_error_recovery_shadow_diagnostic_gate_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(
        result.builtin_derive_error_recovery_shadow_diagnostic_gates.begin(),
        result.builtin_derive_error_recovery_shadow_diagnostic_gates.end(),
        [&generated_part](
            const frontend::macro::BuiltinDeriveErrorRecoveryShadowDiagnosticGate& gate) {
            return gate.module.value == generated_part.module.value
                && gate.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_error_recovery_shadow_diagnostic_gates.end()
        ? nullptr
        : &*found;
}

[[nodiscard]] inline const frontend::macro::BuiltinDeriveCursorRollbackAstMutationVerifierClosure*
builtin_derive_cursor_rollback_ast_mutation_verifier_closure_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(
        result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.begin(),
        result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.end(),
        [&generated_part](
            const frontend::macro::BuiltinDeriveCursorRollbackAstMutationVerifierClosure& closure) {
            return closure.module.value == generated_part.module.value
                && closure.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.end()
        ? nullptr
        : &*found;
}

} // namespace aurex::test::early_item_expansion_support
