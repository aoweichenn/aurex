#pragma once

#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_builtin_derive_support.hpp>
#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_contract_support.hpp>

#include <string_view>
#include <utility>
#include <vector>

namespace aurex::test::early_item_expansion_support {

inline constexpr std::string_view EARLY_ITEM_EXPANSION_TEST_NOOP_ATTRIBUTE_SOURCE =
    "module macro.early_item_expansion;\n"
    "#[builder(defaults(threads = 4), flag, nested[a + b])]\n"
    "#[derive(Copy, Eq)]\n"
    "struct Config { threads: i32; }\n"
    "fn main() -> i32 { return 0; }\n";

struct NoopAttributeExpansionFixture {
    syntax::AstModule module;
    std::vector<std::vector<query::ModulePartKey>> part_keys;
    frontend::macro::EarlyItemExpansionResult result;
};

struct NoopAttributeExpansionView {
    const syntax::ItemNode* config = nullptr;
    const frontend::macro::EarlyItemMacroInput* builder = nullptr;
    const frontend::macro::EarlyItemMacroInput* derive = nullptr;
    const frontend::macro::GeneratedModulePartPlaceholder* generated = nullptr;
    const frontend::macro::GeneratedModulePartParseMergeStub* stub = nullptr;
    const frontend::macro::ExpansionHygieneStub* builder_hygiene = nullptr;
    const frontend::macro::ExpansionTraceStub* builder_trace = nullptr;
    const frontend::macro::GeneratedItemDeclarationStub* builder_declaration = nullptr;
    const frontend::macro::GeneratedItemDeclarationStub* derive_declaration = nullptr;
    const frontend::macro::DeclaredGeneratedNameStub* builder_declared_name = nullptr;
    const frontend::macro::TokenMaterializationAdmissionStub* builder_admission = nullptr;
    const frontend::macro::TokenMaterializationAdmissionStub* derive_admission = nullptr;
    const frontend::macro::GeneratedTokenBufferStub* builder_buffer = nullptr;
    const frontend::macro::GeneratedTokenBufferStub* derive_buffer = nullptr;
    const frontend::macro::GeneratedTokenParserAdmissionGateStub* builder_gate = nullptr;
    const frontend::macro::GeneratedTokenParserAdmissionGateStub* derive_gate = nullptr;
    const frontend::macro::ParserAdmissionDiagnosticProjectionStub* builder_diagnostic = nullptr;
    const frontend::macro::ParserAdmissionDiagnosticProjectionStub* derive_diagnostic = nullptr;
    const frontend::macro::ParserAdmissionDiagnosticReportEntry* builder_report_entry = nullptr;
    const frontend::macro::ParserAdmissionDiagnosticReportEntry* derive_report_entry = nullptr;
    const frontend::macro::ParserAdmissionDiagnosticReport* report = nullptr;
    const frontend::macro::GeneratedTokenParserReadinessPreflightEntry* builder_preflight = nullptr;
    const frontend::macro::GeneratedTokenParserReadinessPreflightEntry* derive_preflight = nullptr;
    const frontend::macro::GeneratedTokenParserConsumptionContractGate* contract = nullptr;
    const frontend::macro::MacroExpansionBoundaryClosureReport* closure = nullptr;
    const frontend::macro::BuiltinDeriveExpansionAdmissionGate* builder_derive_admission = nullptr;
    const frontend::macro::BuiltinDeriveExpansionAdmissionGate* derive_expansion_admission = nullptr;
    const frontend::macro::BuiltinDeriveSemanticExpansionPlan* builder_semantic_plan = nullptr;
    const frontend::macro::BuiltinDeriveSemanticExpansionPlan* derive_semantic_plan = nullptr;
    const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate* release_gate = nullptr;
    const frontend::macro::BuiltinDeriveReleaseHardeningMatrix* hardening_matrix = nullptr;
    const frontend::macro::BuiltinDeriveDebugDumpStabilityContract* debug_contract = nullptr;
    const frontend::macro::BuiltinDeriveRollbackDiagnosticDesignGate* rollback_gate = nullptr;
    const frontend::macro::BuiltinDeriveParserConsumptionAdmissionProtocol* admission_protocol = nullptr;
    const frontend::macro::BuiltinDeriveParserConsumptionCheckpointRollbackProtocol* checkpoint_protocol = nullptr;
    const frontend::macro::BuiltinDeriveParserPreConsumptionVerificationClosure* verification_closure = nullptr;
    const frontend::macro::BuiltinDeriveControlledParserDryRunAdapter* dry_run_adapter = nullptr;
    const frontend::macro::BuiltinDeriveDryRunRollbackDiagnosticReplay* dry_run_replay = nullptr;
    const frontend::macro::BuiltinDeriveDryRunNegativeMatrixClosure* dry_run_matrix = nullptr;
    const frontend::macro::BuiltinDeriveParserDryRunSessionBoundary* dry_run_session = nullptr;
    const frontend::macro::BuiltinDeriveTokenCursorSnapshotRollbackProof* cursor_proof = nullptr;
    const frontend::macro::BuiltinDeriveDiagnosticShadowNoAstMutationClosure* shadow_closure = nullptr;
};

[[nodiscard]] inline NoopAttributeExpansionFixture make_noop_attribute_expansion_fixture()
{
    NoopAttributeExpansionFixture fixture;
    fixture.module = parse_success(EARLY_ITEM_EXPANSION_TEST_NOOP_ATTRIBUTE_SOURCE);
    assign_single_module_ownership(fixture.module);
    fixture.part_keys = single_part_key_table();

    auto expanded = frontend::macro::expand_early_item_macros_noop(fixture.module, fixture.part_keys);
    if (!expanded) {
        ADD_FAILURE() << expanded.error().message;
        return fixture;
    }
    fixture.result = expanded.take_value();
    return fixture;
}

[[nodiscard]] inline NoopAttributeExpansionView inspect_noop_attribute_expansion(
    const NoopAttributeExpansionFixture& fixture) noexcept
{
    NoopAttributeExpansionView view;
    const frontend::macro::EarlyItemExpansionResult& result = fixture.result;
    view.config = find_item(fixture.module, "Config");
    view.builder = input_by_attribute(result, "builder");
    view.derive = input_by_attribute(result, "derive");

    if (!result.generated_parts.empty()) {
        view.generated = &result.generated_parts.front();
        view.report = parser_admission_report_for_part(result, *view.generated);
        view.contract = parser_consumption_contract_gate_for_part(result, *view.generated);
        view.release_gate = builtin_derive_parser_release_gate_for_part(result, *view.generated);
        view.hardening_matrix = builtin_derive_release_hardening_matrix_for_part(result, *view.generated);
        view.debug_contract = builtin_derive_debug_dump_contract_for_part(result, *view.generated);
        view.rollback_gate = builtin_derive_rollback_diagnostic_gate_for_part(result, *view.generated);
        view.admission_protocol = builtin_derive_parser_consumption_admission_protocol_for_part(result, *view.generated);
        view.checkpoint_protocol = builtin_derive_checkpoint_rollback_protocol_for_part(result, *view.generated);
        view.verification_closure = builtin_derive_preconsumption_verification_closure_for_part(result, *view.generated);
        view.dry_run_adapter = builtin_derive_controlled_dry_run_adapter_for_part(result, *view.generated);
        view.dry_run_replay = builtin_derive_dry_run_rollback_replay_for_part(result, *view.generated);
        view.dry_run_matrix = builtin_derive_dry_run_negative_matrix_for_part(result, *view.generated);
        view.dry_run_session = builtin_derive_parser_dry_run_session_for_part(result, *view.generated);
        view.cursor_proof = builtin_derive_token_cursor_snapshot_proof_for_part(result, *view.generated);
        view.shadow_closure = builtin_derive_diagnostic_shadow_no_ast_mutation_closure_for_part(result, *view.generated);
    }
    if (!result.generated_part_stubs.empty()) {
        view.stub = &result.generated_part_stubs.front();
    }
    if (!result.macro_boundary_closure_reports.empty()) {
        view.closure = &result.macro_boundary_closure_reports.front();
    }
    if (view.builder != nullptr) {
        view.builder_hygiene = hygiene_stub_for_input(result, *view.builder);
        view.builder_trace = trace_stub_for_input(result, *view.builder);
        view.builder_declaration = generated_item_declaration_for_input(result, *view.builder);
        view.builder_declared_name = declared_generated_name_for_input(result, *view.builder);
        view.builder_admission = token_admission_for_input(result, *view.builder);
        view.builder_buffer = token_buffer_for_input(result, *view.builder);
        view.builder_gate = parser_admission_gate_for_input(result, *view.builder);
        view.builder_diagnostic = parser_admission_diagnostic_for_input(result, *view.builder);
        view.builder_report_entry = parser_admission_report_entry_for_input(result, *view.builder);
        view.builder_preflight = parser_readiness_preflight_entry_for_input(result, *view.builder);
        view.builder_derive_admission = builtin_derive_admission_for_input(result, *view.builder);
        view.builder_semantic_plan = builtin_derive_semantic_plan_for_input(result, *view.builder);
    }
    if (view.derive != nullptr) {
        view.derive_declaration = generated_item_declaration_for_input(result, *view.derive);
        view.derive_admission = token_admission_for_input(result, *view.derive);
        view.derive_buffer = token_buffer_for_input(result, *view.derive);
        view.derive_gate = parser_admission_gate_for_input(result, *view.derive);
        view.derive_diagnostic = parser_admission_diagnostic_for_input(result, *view.derive);
        view.derive_report_entry = parser_admission_report_entry_for_input(result, *view.derive);
        view.derive_preflight = parser_readiness_preflight_entry_for_input(result, *view.derive);
        view.derive_expansion_admission = builtin_derive_admission_for_input(result, *view.derive);
        view.derive_semantic_plan = builtin_derive_semantic_plan_for_input(result, *view.derive);
    }
    return view;
}

} // namespace aurex::test::early_item_expansion_support
