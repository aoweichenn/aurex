#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/macro/early_item_expansion.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/syntax/core/ast_dump.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>

#include <support/frontend_test_support.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::SourceId EARLY_ITEM_EXPANSION_TEST_SOURCE_ID{31U};
constexpr base::u8 EARLY_ITEM_EXPANSION_TEST_INVALID_DISPOSITION = 243U;
constexpr base::u8 EARLY_ITEM_EXPANSION_TEST_INVALID_LIFECYCLE_STATE = 242U;
constexpr base::u32 EARLY_ITEM_EXPANSION_TEST_GENERATED_PART_INDEX_OFFSET = 100'000U;
constexpr base::u64 EARLY_ITEM_EXPANSION_TEST_DERIVE_SENTINEL_TOKEN_COUNT = 2U;

[[nodiscard]] syntax::AstModule parse_success(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(EARLY_ITEM_EXPANSION_TEST_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        ADD_FAILURE() << tokens.error().message;
        return {};
    }

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (!parsed) {
        ADD_FAILURE() << parsed.error().message;
        return {};
    }
    if (diagnostics.has_error()) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        return {};
    }
    return parsed.take_value();
}

[[nodiscard]] const syntax::ItemNode* find_item(
    const syntax::AstModule& module, const std::string_view name) noexcept
{
    for (base::usize index = 0; index < module.items.size(); ++index) {
        const syntax::ItemNode* const item = module.items.ptr(index);
        if (item != nullptr && item->name == name) {
            return item;
        }
    }
    return nullptr;
}

void assign_single_module_ownership(syntax::AstModule& module)
{
    if (module.modules.empty()) {
        syntax::ModuleInfo module_info;
        module_info.path = module.module_path;
        module.modules.push_back(std::move(module_info));
    }
    module.item_modules.assign(module.items.size(), syntax::ModuleId{0U});
    module.item_part_indices.assign(module.items.size(), 0U);
}

[[nodiscard]] query::PackageKey package_key()
{
    const std::array<std::string_view, 1U> identity{"early-item-expansion-test-package"};
    return query::package_key(identity);
}

[[nodiscard]] query::ModuleKey module_key(const query::PackageKey package)
{
    const std::array<std::string_view, 2U> path{"macro", "early_item_expansion"};
    return query::module_key(package, path);
}

[[nodiscard]] query::ModulePartKey module_part_key_for_index(const base::u32 part_index)
{
    const query::PackageKey package = package_key();
    const query::ModuleKey module = module_key(package);
    if (part_index == 0U) {
        const query::FileKey file = query::file_key(package, "/virtual/tests/macro/early_item_expansion.ax");
        return query::module_part_key(module, file, query::ModulePartKind::primary, "<primary>");
    }
    const std::string part_name = "part" + std::to_string(part_index);
    const std::string source_path = "/virtual/tests/macro/early_item_expansion.parts/" + part_name + ".ax";
    const query::FileKey file = query::file_key(package, source_path);
    return query::module_part_key(module, file, query::ModulePartKind::fragment, part_name, part_index);
}

[[nodiscard]] query::ModulePartKey primary_part_key()
{
    return module_part_key_for_index(0U);
}

[[nodiscard]] std::vector<std::vector<query::ModulePartKey>> part_key_table(const base::u32 part_count)
{
    std::vector<std::vector<query::ModulePartKey>> keys(1U);
    keys.front().reserve(part_count);
    for (base::u32 part_index = 0U; part_index < part_count; ++part_index) {
        keys.front().push_back(module_part_key_for_index(part_index));
    }
    return keys;
}

[[nodiscard]] std::vector<std::vector<query::ModulePartKey>> single_part_key_table()
{
    return {{primary_part_key()}};
}

[[nodiscard]] frontend::macro::EarlyItemExpansionResult expand_source(const std::string_view source)
{
    syntax::AstModule module = parse_success(source);
    assign_single_module_ownership(module);
    std::vector<std::vector<query::ModulePartKey>> part_keys = single_part_key_table();
    auto expanded = frontend::macro::expand_early_item_macros_noop(module, part_keys);
    if (!expanded) {
        ADD_FAILURE() << expanded.error().message;
        return {};
    }
    return expanded.take_value();
}

[[nodiscard]] const frontend::macro::EarlyItemMacroInput* input_by_attribute(
    const frontend::macro::EarlyItemExpansionResult& result,
    const std::string_view attribute_name) noexcept
{
    const auto found = std::find_if(result.inputs.begin(), result.inputs.end(),
        [attribute_name](const frontend::macro::EarlyItemMacroInput& input) {
            return input.attribute_name == attribute_name;
        });
    return found == result.inputs.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::ExpansionHygieneStub* hygiene_stub_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.hygiene_stubs.begin(), result.hygiene_stubs.end(),
        [&input](const frontend::macro::ExpansionHygieneStub& stub) {
            return stub.item.value == input.item.value
                && stub.module.value == input.module.value
                && stub.attribute_index == input.attribute_index;
        });
    return found == result.hygiene_stubs.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::ExpansionTraceStub* trace_stub_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.trace_stubs.begin(), result.trace_stubs.end(),
        [&input](const frontend::macro::ExpansionTraceStub& stub) {
            return stub.item.value == input.item.value
                && stub.module.value == input.module.value
                && stub.attribute_index == input.attribute_index;
        });
    return found == result.trace_stubs.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::GeneratedItemDeclarationStub* generated_item_declaration_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.generated_item_declarations.begin(),
        result.generated_item_declarations.end(),
        [&input](const frontend::macro::GeneratedItemDeclarationStub& stub) {
            return stub.item.value == input.item.value
                && stub.module.value == input.module.value
                && stub.attribute_index == input.attribute_index;
        });
    return found == result.generated_item_declarations.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::DeclaredGeneratedNameStub* declared_generated_name_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.declared_generated_names.begin(),
        result.declared_generated_names.end(),
        [&input](const frontend::macro::DeclaredGeneratedNameStub& stub) {
            return stub.item.value == input.item.value
                && stub.module.value == input.module.value
                && stub.attribute_index == input.attribute_index;
        });
    return found == result.declared_generated_names.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::TokenMaterializationAdmissionStub* token_admission_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.token_materialization_admissions.begin(),
        result.token_materialization_admissions.end(),
        [&input](const frontend::macro::TokenMaterializationAdmissionStub& stub) {
            return stub.item.value == input.item.value
                && stub.module.value == input.module.value
                && stub.attribute_index == input.attribute_index;
        });
    return found == result.token_materialization_admissions.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::GeneratedTokenBufferStub* token_buffer_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.generated_token_buffers.begin(),
        result.generated_token_buffers.end(),
        [&input](const frontend::macro::GeneratedTokenBufferStub& stub) {
            return stub.item.value == input.item.value
                && stub.module.value == input.module.value
                && stub.attribute_index == input.attribute_index;
        });
    return found == result.generated_token_buffers.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::GeneratedTokenParserAdmissionGateStub* parser_admission_gate_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.parser_admission_gates.begin(),
        result.parser_admission_gates.end(),
        [&input](const frontend::macro::GeneratedTokenParserAdmissionGateStub& stub) {
            return stub.item.value == input.item.value
                && stub.module.value == input.module.value
                && stub.attribute_index == input.attribute_index;
        });
    return found == result.parser_admission_gates.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::ParserAdmissionDiagnosticProjectionStub* parser_admission_diagnostic_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.parser_admission_diagnostics.begin(),
        result.parser_admission_diagnostics.end(),
        [&input](const frontend::macro::ParserAdmissionDiagnosticProjectionStub& stub) {
            return stub.item.value == input.item.value
                && stub.module.value == input.module.value
                && stub.attribute_index == input.attribute_index;
        });
    return found == result.parser_admission_diagnostics.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::ParserAdmissionDiagnosticReportEntry*
parser_admission_report_entry_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.parser_admission_report_entries.begin(),
        result.parser_admission_report_entries.end(),
        [&input](const frontend::macro::ParserAdmissionDiagnosticReportEntry& entry) {
            return entry.item.value == input.item.value
                && entry.module.value == input.module.value
                && entry.attribute_index == input.attribute_index;
        });
    return found == result.parser_admission_report_entries.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::ParserAdmissionDiagnosticReport*
parser_admission_report_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.parser_admission_reports.begin(),
        result.parser_admission_reports.end(),
        [&generated_part](const frontend::macro::ParserAdmissionDiagnosticReport& report) {
            return report.module.value == generated_part.module.value
                && report.source_part_index == generated_part.source_part_index;
        });
    return found == result.parser_admission_reports.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::GeneratedTokenParserReadinessPreflightEntry*
parser_readiness_preflight_entry_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.parser_readiness_preflight_entries.begin(),
        result.parser_readiness_preflight_entries.end(),
        [&input](const frontend::macro::GeneratedTokenParserReadinessPreflightEntry& entry) {
            return entry.item.value == input.item.value
                && entry.module.value == input.module.value
                && entry.attribute_index == input.attribute_index;
        });
    return found == result.parser_readiness_preflight_entries.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::GeneratedTokenParserConsumptionContractGate*
parser_consumption_contract_gate_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.parser_consumption_contract_gates.begin(),
        result.parser_consumption_contract_gates.end(),
        [&generated_part](const frontend::macro::GeneratedTokenParserConsumptionContractGate& gate) {
            return gate.module.value == generated_part.module.value
                && gate.source_part_index == generated_part.source_part_index;
        });
    return found == result.parser_consumption_contract_gates.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::BuiltinDeriveExpansionAdmissionGate*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveSemanticExpansionPlan*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveReleaseHardeningMatrix*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveDebugDumpStabilityContract*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveRollbackDiagnosticDesignGate*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveParserConsumptionAdmissionProtocol*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveParserConsumptionCheckpointRollbackProtocol*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveParserPreConsumptionVerificationClosure*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveControlledParserDryRunAdapter*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveDryRunRollbackDiagnosticReplay*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveDryRunNegativeMatrixClosure*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveParserDryRunSessionBoundary*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveTokenCursorSnapshotRollbackProof*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveDiagnosticShadowNoAstMutationClosure*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveParserDryRunAdmissionGate*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveErrorRecoveryShadowDiagnosticGate*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveCursorRollbackAstMutationVerifierClosure*
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

[[nodiscard]] std::vector<const frontend::macro::GeneratedTokenRecord*> token_records_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input)
{
    std::vector<const frontend::macro::GeneratedTokenRecord*> records;
    for (const frontend::macro::GeneratedTokenRecord& record : result.generated_token_records) {
        if (record.item.value == input.item.value
            && record.module.value == input.module.value
            && record.attribute_index == input.attribute_index) {
            records.push_back(&record);
        }
    }
    return records;
}

[[nodiscard]] base::usize first_record_index_for_attribute(
    const frontend::macro::EarlyItemExpansionResult& result,
    const std::string_view attribute_name)
{
    for (base::usize index = 0; index < result.generated_token_records.size(); ++index) {
        const frontend::macro::GeneratedTokenRecord& record = result.generated_token_records[index];
        const auto input_found = std::find_if(result.inputs.begin(), result.inputs.end(),
            [&record, attribute_name](const frontend::macro::EarlyItemMacroInput& input) {
                return input.item.value == record.item.value
                    && input.module.value == record.module.value
                    && input.attribute_index == record.attribute_index
                    && input.attribute_name == attribute_name;
            });
        if (input_found != result.inputs.end()) {
            return index;
        }
    }
    return result.generated_token_records.size();
}

void refresh_expansion_result(frontend::macro::EarlyItemExpansionResult& result)
{
    result.summary = frontend::macro::summarize_early_item_expansion_counts(result);
    result.fingerprint = frontend::macro::early_item_expansion_fingerprint(result);
}

template <typename Mutator>
[[nodiscard]] frontend::macro::EarlyItemExpansionResult mutated_expansion_result(
    const frontend::macro::EarlyItemExpansionResult& baseline,
    Mutator&& mutator)
{
    frontend::macro::EarlyItemExpansionResult result = baseline;
    std::forward<Mutator>(mutator)(result);
    refresh_expansion_result(result);
    return result;
}

[[nodiscard]] const frontend::macro::AurexMacroSurfaceAdmissionGate* aurex_macro_surface_gate_by_name(
    const frontend::macro::EarlyItemExpansionResult& result,
    const std::string_view macro_name) noexcept
{
    const auto found = std::find_if(result.aurex_macro_surface_admission_gates.begin(),
        result.aurex_macro_surface_admission_gates.end(),
        [macro_name](const frontend::macro::AurexMacroSurfaceAdmissionGate& gate) {
            return gate.macro_name == macro_name;
        });
    return found == result.aurex_macro_surface_admission_gates.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::AurexMacroDefinitionSiteHygieneAdmissionGate*
aurex_macro_hygiene_gate_by_name(
    const frontend::macro::EarlyItemExpansionResult& result,
    const std::string_view macro_name) noexcept
{
    const auto found = std::find_if(result.aurex_macro_definition_site_hygiene_gates.begin(),
        result.aurex_macro_definition_site_hygiene_gates.end(),
        [macro_name](const frontend::macro::AurexMacroDefinitionSiteHygieneAdmissionGate& gate) {
            return gate.macro_name == macro_name;
        });
    return found == result.aurex_macro_definition_site_hygiene_gates.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::AurexMacroTypedMatcherAdmissionGate*
aurex_macro_matcher_gate_by_name_and_index(
    const frontend::macro::EarlyItemExpansionResult& result,
    const std::string_view macro_name,
    const base::u32 matcher_index) noexcept
{
    const auto found = std::find_if(result.aurex_macro_typed_matcher_admission_gates.begin(),
        result.aurex_macro_typed_matcher_admission_gates.end(),
        [macro_name, matcher_index](const frontend::macro::AurexMacroTypedMatcherAdmissionGate& gate) {
            return gate.macro_name == macro_name && gate.matcher_index == matcher_index;
        });
    return found == result.aurex_macro_typed_matcher_admission_gates.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::AurexMacroCallSiteAdmissionGate*
aurex_macro_call_site_gate_by_name(
    const frontend::macro::EarlyItemExpansionResult& result,
    const std::string_view macro_name) noexcept
{
    const auto found = std::find_if(result.aurex_macro_call_site_admission_gates.begin(),
        result.aurex_macro_call_site_admission_gates.end(),
        [macro_name](const frontend::macro::AurexMacroCallSiteAdmissionGate& gate) {
            return gate.macro_name == macro_name;
        });
    return found == result.aurex_macro_call_site_admission_gates.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::AurexMacroMatcherToCallBindingAdmissionGate*
aurex_macro_binding_gate_by_call_name(
    const frontend::macro::EarlyItemExpansionResult& result,
    const std::string_view macro_name) noexcept
{
    const auto found = std::find_if(result.aurex_macro_matcher_to_call_binding_gates.begin(),
        result.aurex_macro_matcher_to_call_binding_gates.end(),
        [macro_name](const frontend::macro::AurexMacroMatcherToCallBindingAdmissionGate& gate) {
            return gate.macro_name == macro_name;
        });
    return found == result.aurex_macro_matcher_to_call_binding_gates.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::AurexUserDeriveTargetSchemaAdmissionGate*
aurex_user_derive_schema_gate_by_target(
    const frontend::macro::EarlyItemExpansionResult& result,
    const std::string_view target_name) noexcept
{
    const auto found = std::find_if(result.aurex_user_derive_target_schema_gates.begin(),
        result.aurex_user_derive_target_schema_gates.end(),
        [target_name](const frontend::macro::AurexUserDeriveTargetSchemaAdmissionGate& gate) {
            return gate.target_name == target_name;
        });
    return found == result.aurex_user_derive_target_schema_gates.end() ? nullptr : &*found;
}

} // namespace

TEST(CoreUnit, EarlyItemExpansionDispositionNamesExposeInvalidFallback)
{
    using frontend::macro::EarlyItemExpansionDisposition;

    EXPECT_EQ(frontend::macro::early_item_expansion_disposition_name(
                  EarlyItemExpansionDisposition::builtin_derive_passthrough),
        "builtin_derive_passthrough");
    EXPECT_EQ(frontend::macro::early_item_expansion_disposition_name(
                  EarlyItemExpansionDisposition::blocked_unimplemented_attribute),
        "blocked_unimplemented_attribute");
    EXPECT_EQ(frontend::macro::early_item_expansion_disposition_name(
                  static_cast<EarlyItemExpansionDisposition>(EARLY_ITEM_EXPANSION_TEST_INVALID_DISPOSITION)),
        "invalid");
    EXPECT_TRUE(frontend::macro::is_valid(EarlyItemExpansionDisposition::builtin_derive_passthrough));
    EXPECT_FALSE(frontend::macro::is_valid(
        static_cast<EarlyItemExpansionDisposition>(EARLY_ITEM_EXPANSION_TEST_INVALID_DISPOSITION)));
}

TEST(CoreUnit, EarlyItemExpansionLifecycleNamesExposeInvalidFallback)
{
    using frontend::macro::GeneratedModulePartLifecycleState;

    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  GeneratedModulePartLifecycleState::planned),
        "planned");
    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  GeneratedModulePartLifecycleState::materialized_buffer_stub),
        "materialized_buffer_stub");
    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  GeneratedModulePartLifecycleState::parse_blocked),
        "parse_blocked");
    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  GeneratedModulePartLifecycleState::merge_blocked),
        "merge_blocked");
    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  static_cast<GeneratedModulePartLifecycleState>(
                      EARLY_ITEM_EXPANSION_TEST_INVALID_LIFECYCLE_STATE)),
        "invalid");
    EXPECT_TRUE(frontend::macro::is_valid(GeneratedModulePartLifecycleState::planned));
    EXPECT_TRUE(frontend::macro::is_valid(GeneratedModulePartLifecycleState::merge_blocked));
    EXPECT_FALSE(frontend::macro::is_valid(
        static_cast<GeneratedModulePartLifecycleState>(EARLY_ITEM_EXPANSION_TEST_INVALID_LIFECYCLE_STATE)));
}

TEST(CoreUnit, EarlyItemExpansionNoopCollectsAttributeInputsAndPlaceholders)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(defaults(threads = 4), flag, nested[a + b])]\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n"
        "fn main() -> i32 { return 0; }\n";

    syntax::AstModule module = parse_success(source);
    assign_single_module_ownership(module);
    const syntax::ItemNode* const config = find_item(module, "Config");
    ASSERT_NE(config, nullptr);
    ASSERT_EQ(config->attributes.size(), 2U);

    std::vector<std::vector<query::ModulePartKey>> part_keys = single_part_key_table();
    auto expanded = frontend::macro::expand_early_item_macros_noop(module, part_keys);
    ASSERT_TRUE(expanded) << expanded.error().message;
    const frontend::macro::EarlyItemExpansionResult result = expanded.take_value();

    EXPECT_EQ(result.name, "M26c Builtin Derive Cursor Rollback AST Mutation Verifier Closure");
    EXPECT_TRUE(frontend::macro::is_valid(result));
    EXPECT_EQ(result.fingerprint, frontend::macro::early_item_expansion_fingerprint(result));
    EXPECT_EQ(result.summary.macro_input_count, 2U);
    EXPECT_EQ(result.summary.attribute_input_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_passthrough_count, 1U);
    EXPECT_EQ(result.summary.blocked_attribute_count, 1U);
    EXPECT_EQ(result.summary.generated_part_placeholder_count, 1U);
    EXPECT_EQ(result.summary.generated_part_stub_count, 1U);
    EXPECT_EQ(result.summary.materialized_buffer_stub_count, 1U);
    EXPECT_EQ(result.summary.parse_blocked_count, 1U);
    EXPECT_EQ(result.summary.merge_blocked_count, 1U);
    EXPECT_EQ(result.summary.sema_visible_generated_part_count, 0U);
    EXPECT_EQ(result.summary.source_map_placeholder_count, 2U);
    EXPECT_EQ(result.summary.hygiene_stub_count, 2U);
    EXPECT_EQ(result.summary.unresolved_hygiene_stub_count, 2U);
    EXPECT_EQ(result.summary.declared_name_stub_count, 2U);
    EXPECT_EQ(result.summary.call_site_capture_count, 0U);
    EXPECT_EQ(result.summary.trace_stub_count, 2U);
    EXPECT_EQ(result.summary.real_source_map_count, 0U);
    EXPECT_EQ(result.summary.debug_trace_available_count, 0U);
    EXPECT_EQ(result.summary.cli_emit_expanded_available_count, 0U);
    EXPECT_EQ(result.summary.generated_item_declaration_stub_count, 2U);
    EXPECT_EQ(result.summary.planned_generated_item_declaration_count, 2U);
    EXPECT_EQ(result.summary.materialized_generated_item_count, 0U);
    EXPECT_EQ(result.summary.declared_generated_name_stub_count, 2U);
    EXPECT_EQ(result.summary.lookup_visible_declared_name_count, 0U);
    EXPECT_EQ(result.summary.export_visible_declared_name_count, 0U);
    EXPECT_EQ(result.summary.token_materialization_admission_stub_count, 2U);
    EXPECT_EQ(result.summary.compiler_owned_admission_count, 2U);
    EXPECT_EQ(result.summary.admitted_token_materialization_count, 2U);
    EXPECT_EQ(result.summary.materialized_token_admission_count, 1U);
    EXPECT_EQ(result.summary.generated_token_buffer_stub_count, 2U);
    EXPECT_EQ(result.summary.empty_generated_token_buffer_count, 1U);
    EXPECT_EQ(result.summary.materialized_token_buffer_count, 1U);
    EXPECT_EQ(result.summary.compiler_owned_token_buffer_count, 2U);
    EXPECT_EQ(result.summary.generated_token_record_count,
        config->attributes[1].token_tree.size() + EARLY_ITEM_EXPANSION_TEST_DERIVE_SENTINEL_TOKEN_COUNT);
    EXPECT_EQ(result.summary.compiler_owned_generated_token_record_count,
        config->attributes[1].token_tree.size() + EARLY_ITEM_EXPANSION_TEST_DERIVE_SENTINEL_TOKEN_COUNT);
    EXPECT_EQ(result.summary.parser_visible_generated_token_count, 0U);
    EXPECT_EQ(result.summary.parser_admission_gate_stub_count, 2U);
    EXPECT_EQ(result.summary.compiler_owned_parser_admission_gate_count, 2U);
    EXPECT_EQ(result.summary.token_record_available_gate_count, 1U);
    EXPECT_EQ(result.summary.parser_blocked_token_buffer_count, 2U);
    EXPECT_EQ(result.summary.parser_admitted_token_buffer_count, 0U);
    EXPECT_EQ(result.summary.parser_admission_diagnostic_stub_count, 2U);
    EXPECT_EQ(result.summary.parser_admission_diagnostic_blocked_count, 2U);
    EXPECT_EQ(result.summary.derive_parser_admission_diagnostic_count, 1U);
    EXPECT_EQ(result.summary.empty_parser_admission_diagnostic_count, 1U);
    EXPECT_EQ(result.summary.emit_expanded_projection_available_count, 0U);
    EXPECT_EQ(result.summary.parser_admission_debug_trace_projection_count, 0U);
    EXPECT_EQ(result.summary.parser_admission_source_map_projection_count, 0U);
    EXPECT_EQ(result.summary.parser_admission_report_entry_count, 2U);
    EXPECT_EQ(result.summary.parser_admission_report_count, 1U);
    EXPECT_EQ(result.summary.parser_admission_report_blocked_entry_count, 2U);
    EXPECT_EQ(result.summary.parser_admission_report_derive_entry_count, 1U);
    EXPECT_EQ(result.summary.parser_admission_report_empty_entry_count, 1U);
    EXPECT_EQ(result.summary.parser_admission_report_token_record_available_entry_count, 1U);
    EXPECT_EQ(result.summary.parser_admission_report_visible_count, 1U);
    EXPECT_EQ(result.summary.parser_admission_report_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.parser_admission_report_unordered_anchor_count, 0U);
    EXPECT_EQ(result.summary.parser_admission_report_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_entry_count, 2U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_blocked_count, 2U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_derive_entry_count, 1U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_empty_entry_count, 1U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_contiguous_index_count, 2U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_delimiter_balanced_count, 2U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_source_anchor_covered_count, 2U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_parse_config_compatible_count, 2U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.parser_consumption_contract_gate_count, 1U);
    EXPECT_EQ(result.summary.parser_consumption_contract_blocked_gate_count, 1U);
    EXPECT_EQ(result.summary.parser_consumption_contract_visible_count, 1U);
    EXPECT_EQ(result.summary.parser_consumption_contract_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.parser_consumption_contract_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.macro_boundary_closure_report_count, 1U);
    EXPECT_EQ(result.summary.macro_boundary_closure_visible_count, 1U);
    EXPECT_EQ(result.summary.macro_boundary_closure_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.macro_boundary_closure_complete_count, 1U);
    EXPECT_EQ(result.summary.macro_boundary_closure_parser_consumption_enabled_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_admission_gate_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_derive_admission_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_non_derive_blocked_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_visible_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_query_reusable_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_capability_candidate_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_plan_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_plan_visible_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_plan_query_reusable_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_capability_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_copy_capability_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_eq_capability_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_hash_capability_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_gate_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_release_hardening_matrix_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_release_hardening_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_release_hardening_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_release_hardening_negative_matrix_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_release_hardening_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_debug_dump_contract_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_debug_dump_contract_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_debug_dump_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_debug_dump_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_debug_dump_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_rollback_diagnostic_gate_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_rollback_diagnostic_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_rollback_diagnostic_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_rollback_diagnostic_design_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_rollback_diagnostic_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_parser_consumption_admission_protocol_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_consumption_admission_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_consumption_admission_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_consumption_admission_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_consumption_admission_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_checkpoint_rollback_protocol_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_checkpoint_rollback_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_checkpoint_rollback_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_checkpoint_rollback_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_checkpoint_rollback_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_preconsumption_verification_closure_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_preconsumption_verification_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_preconsumption_verification_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_preconsumption_verification_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_preconsumption_verification_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_controlled_dry_run_adapter_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_controlled_dry_run_adapter_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_controlled_dry_run_adapter_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_controlled_dry_run_adapter_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_controlled_dry_run_adapter_executed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_rollback_replay_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_rollback_replay_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_rollback_replay_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_rollback_replay_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_rollback_replay_executed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_negative_matrix_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_negative_matrix_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_negative_matrix_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_negative_matrix_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_dry_run_negative_matrix_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_session_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_session_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_session_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_session_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_session_executed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_session_committed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_token_cursor_snapshot_proof_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_token_cursor_snapshot_proof_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_token_cursor_snapshot_proof_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_token_cursor_snapshot_proof_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_token_cursor_snapshot_proof_cursor_advanced_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_token_cursor_snapshot_proof_committed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_closure_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_executed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_ast_mutation_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_diagnostic_shadow_no_ast_mutation_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_admission_gate_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_admission_gate_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_admission_gate_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_admission_gate_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_admission_gate_execution_admitted_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_admission_gate_executed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_parser_dry_run_admission_gate_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_error_recovery_shadow_diagnostic_gate_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_error_recovery_shadow_diagnostic_gate_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_error_recovery_shadow_diagnostic_gate_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_error_recovery_shadow_diagnostic_gate_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_error_recovery_shadow_diagnostic_gate_recovery_executed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_error_recovery_shadow_diagnostic_gate_diagnostic_emitted_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_error_recovery_shadow_diagnostic_gate_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_cursor_rollback_ast_mutation_verifier_closure_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_cursor_rollback_ast_mutation_verifier_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_cursor_rollback_ast_mutation_verifier_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_cursor_rollback_ast_mutation_verifier_complete_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_cursor_rollback_ast_mutation_verifier_rollback_executed_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_cursor_rollback_ast_mutation_verifier_ast_mutation_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_cursor_rollback_ast_mutation_verifier_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.generated_source_text_count, 0U);
    EXPECT_EQ(result.summary.parse_ready_token_buffer_count, 0U);
    EXPECT_EQ(result.summary.parsed_generated_part_count, 0U);
    EXPECT_EQ(result.summary.merged_generated_part_count, 0U);
    EXPECT_EQ(result.summary.ast_mutation_count, 0U);
    EXPECT_EQ(result.summary.user_generated_code_count, 0U);
    EXPECT_EQ(result.summary.standard_library_required_count, 0U);
    EXPECT_EQ(result.summary.runtime_required_count, 0U);
    EXPECT_EQ(result.summary.external_process_required_count, 0U);

    const frontend::macro::EarlyItemMacroInput* const builder = input_by_attribute(result, "builder");
    ASSERT_NE(builder, nullptr);
    EXPECT_EQ(builder->attribute_index, 0U);
    EXPECT_EQ(builder->part_index, 0U);
    EXPECT_EQ(builder->token_count, config->attributes[0].token_tree.size());
    EXPECT_EQ(builder->disposition,
        frontend::macro::EarlyItemExpansionDisposition::blocked_unimplemented_attribute);
    EXPECT_EQ(builder->attached_part, part_keys[0][0]);
    EXPECT_GT(builder->token_tree_fingerprint.byte_count, 0U);
    EXPECT_GT(builder->query_key_fingerprint.byte_count, 0U);

    const frontend::macro::EarlyItemMacroInput* const derive = input_by_attribute(result, "derive");
    ASSERT_NE(derive, nullptr);
    EXPECT_EQ(derive->attribute_index, 1U);
    EXPECT_EQ(derive->disposition,
        frontend::macro::EarlyItemExpansionDisposition::builtin_derive_passthrough);
    EXPECT_EQ(derive->token_count, config->attributes[1].token_tree.size());
    EXPECT_NE(derive->query_key_fingerprint, builder->query_key_fingerprint);

    ASSERT_EQ(result.generated_parts.size(), 1U);
    const frontend::macro::GeneratedModulePartPlaceholder& generated = result.generated_parts.front();
    EXPECT_EQ(generated.module.value, 0U);
    EXPECT_EQ(generated.source_part_index, 0U);
    EXPECT_EQ(generated.generated_stable_index, EARLY_ITEM_EXPANSION_TEST_GENERATED_PART_INDEX_OFFSET);
    EXPECT_EQ(generated.source_role, query::SourceRole::generated);
    EXPECT_EQ(generated.part_kind, query::ModulePartKind::generated);
    EXPECT_EQ(generated.source_part, part_keys[0][0]);
    EXPECT_EQ(generated.generated_part.kind, query::ModulePartKind::generated);
    EXPECT_EQ(generated.generated_part.file.role, query::SourceRole::generated);
    EXPECT_NE(generated.generated_part, generated.source_part);
    EXPECT_FALSE(generated.parsed);
    EXPECT_FALSE(generated.merged);
    EXPECT_FALSE(generated.produced_user_generated_code);

    ASSERT_EQ(result.generated_part_stubs.size(), 1U);
    const frontend::macro::GeneratedModulePartParseMergeStub& stub =
        result.generated_part_stubs.front();
    EXPECT_TRUE(frontend::macro::is_valid(stub));
    EXPECT_EQ(stub.module.value, generated.module.value);
    EXPECT_EQ(stub.source_part_index, generated.source_part_index);
    EXPECT_EQ(stub.generated_stable_index, generated.generated_stable_index);
    EXPECT_EQ(stub.source_part, generated.source_part);
    EXPECT_EQ(stub.generated_part, generated.generated_part);
    EXPECT_GT(stub.generated_buffer_identity.byte_count, 0U);
    EXPECT_GT(stub.parse_config_fingerprint.byte_count, 0U);
    EXPECT_GT(stub.merge_ordering_key.byte_count, 0U);
    EXPECT_EQ(stub.expansion_origin, generated.output_fingerprint);
    expect_contains(stub.generated_buffer_name, "m21e-noop-generated-buffer:");
    expect_contains(stub.blocker_reason, "parse and merge are blocked in M21e");
    EXPECT_EQ(stub.lifecycle_state,
        frontend::macro::GeneratedModulePartLifecycleState::merge_blocked);
    EXPECT_TRUE(stub.materialized_buffer);
    EXPECT_FALSE(stub.parsed);
    EXPECT_FALSE(stub.merged);
    EXPECT_FALSE(stub.sema_visible);
    EXPECT_FALSE(stub.produced_user_generated_code);

    ASSERT_EQ(result.source_maps.size(), 2U);
    for (const frontend::macro::ExpansionSourceMapPlaceholder& source_map : result.source_maps) {
        EXPECT_FALSE(source_map.real_source_map);
        EXPECT_FALSE(source_map.debug_trace_available);
        EXPECT_GT(source_map.expansion_origin.byte_count, 0U);
    }
    EXPECT_EQ(result.source_maps[0].expansion_origin, builder->query_key_fingerprint);
    EXPECT_EQ(result.source_maps[1].expansion_origin, derive->query_key_fingerprint);

    ASSERT_EQ(result.hygiene_stubs.size(), 2U);
    const frontend::macro::ExpansionHygieneStub* const builder_hygiene =
        hygiene_stub_for_input(result, *builder);
    ASSERT_NE(builder_hygiene, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_hygiene));
    EXPECT_EQ(builder_hygiene->part_index, builder->part_index);
    EXPECT_EQ(builder_hygiene->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_hygiene->attached_part, builder->attached_part);
    EXPECT_EQ(builder_hygiene->expansion_origin, builder->query_key_fingerprint);
    EXPECT_GT(builder_hygiene->call_site_mark.byte_count, 0U);
    EXPECT_GT(builder_hygiene->definition_site_mark.byte_count, 0U);
    EXPECT_GT(builder_hygiene->generated_fresh_mark.byte_count, 0U);
    EXPECT_GT(builder_hygiene->declared_name_set.byte_count, 0U);
    EXPECT_NE(builder_hygiene->call_site_mark, builder_hygiene->definition_site_mark);
    EXPECT_NE(builder_hygiene->definition_site_mark, builder_hygiene->generated_fresh_mark);
    EXPECT_EQ(builder_hygiene->policy, "origin_mark_hygiene_v1");
    EXPECT_FALSE(builder_hygiene->resolved);
    EXPECT_FALSE(builder_hygiene->declared_names_visible);
    EXPECT_FALSE(builder_hygiene->captures_call_site_locals);

    ASSERT_EQ(result.trace_stubs.size(), 2U);
    const frontend::macro::ExpansionTraceStub* const builder_trace =
        trace_stub_for_input(result, *builder);
    ASSERT_NE(builder_trace, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_trace));
    EXPECT_EQ(builder_trace->part_index, builder->part_index);
    EXPECT_EQ(builder_trace->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_trace->attached_part, builder->attached_part);
    EXPECT_EQ(builder_trace->attribute_range.source.value, builder->attribute_range.source.value);
    EXPECT_EQ(builder_trace->attribute_range.begin, builder->attribute_range.begin);
    EXPECT_EQ(builder_trace->attribute_range.end, builder->attribute_range.end);
    EXPECT_EQ(builder_trace->token_tree_range.source.value, builder->token_tree_range.source.value);
    EXPECT_EQ(builder_trace->token_tree_range.begin, builder->token_tree_range.begin);
    EXPECT_EQ(builder_trace->token_tree_range.end, builder->token_tree_range.end);
    EXPECT_EQ(builder_trace->expansion_origin, builder->query_key_fingerprint);
    EXPECT_GT(builder_trace->trace_identity.byte_count, 0U);
    EXPECT_GT(builder_trace->generated_source_map_identity.byte_count, 0U);
    EXPECT_GT(builder_trace->diagnostic_anchor.byte_count, 0U);
    EXPECT_NE(builder_trace->trace_identity, builder_trace->generated_source_map_identity);
    EXPECT_EQ(builder_trace->trace_policy, "expansion_source_map_debug_trace_v1");
    expect_contains(builder_trace->blocker_reason, "blocked in M21f");
    EXPECT_FALSE(builder_trace->real_source_map);
    EXPECT_FALSE(builder_trace->debug_trace_available);
    EXPECT_FALSE(builder_trace->cli_emit_expanded_available);

    ASSERT_EQ(result.generated_item_declarations.size(), 2U);
    const frontend::macro::GeneratedItemDeclarationStub* const builder_declaration =
        generated_item_declaration_for_input(result, *builder);
    ASSERT_NE(builder_declaration, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_declaration));
    EXPECT_EQ(builder_declaration->part_index, builder->part_index);
    EXPECT_EQ(builder_declaration->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_declaration->attached_part, builder->attached_part);
    EXPECT_EQ(builder_declaration->generated_part, generated.generated_part);
    EXPECT_EQ(builder_declaration->expansion_origin, builder->query_key_fingerprint);
    EXPECT_EQ(builder_declaration->declared_name_set, builder_hygiene->declared_name_set);
    EXPECT_GT(builder_declaration->declaration_identity.byte_count, 0U);
    EXPECT_GT(builder_declaration->generated_item_key.byte_count, 0U);
    EXPECT_NE(builder_declaration->declaration_identity, builder_declaration->generated_item_key);
    EXPECT_EQ(builder_declaration->declaration_role, "attached_item_codegen_declared_names_v1");
    expect_contains(builder_declaration->generated_item_name, "__aurex_macro_declared:0:0:0:0:builder");
    expect_contains(builder_declaration->blocker_reason, "blocked in M21g");
    EXPECT_TRUE(builder_declaration->planned);
    EXPECT_FALSE(builder_declaration->materialized_tokens);
    EXPECT_FALSE(builder_declaration->parsed);
    EXPECT_FALSE(builder_declaration->merged);
    EXPECT_FALSE(builder_declaration->sema_visible);
    EXPECT_FALSE(builder_declaration->produced_user_generated_code);

    const frontend::macro::GeneratedItemDeclarationStub* const derive_declaration =
        generated_item_declaration_for_input(result, *derive);
    ASSERT_NE(derive_declaration, nullptr);
    EXPECT_NE(derive_declaration->generated_item_name, builder_declaration->generated_item_name);
    expect_contains(derive_declaration->generated_item_name, "__aurex_macro_declared:0:0:0:1:derive");

    ASSERT_EQ(result.declared_generated_names.size(), 2U);
    const frontend::macro::DeclaredGeneratedNameStub* const builder_declared_name =
        declared_generated_name_for_input(result, *builder);
    ASSERT_NE(builder_declared_name, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_declared_name));
    EXPECT_EQ(builder_declared_name->part_index, builder->part_index);
    EXPECT_EQ(builder_declared_name->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_declared_name->attached_part, builder->attached_part);
    EXPECT_EQ(builder_declared_name->generated_part, generated.generated_part);
    EXPECT_EQ(builder_declared_name->expansion_origin, builder->query_key_fingerprint);
    EXPECT_EQ(builder_declared_name->declared_name_set, builder_hygiene->declared_name_set);
    EXPECT_EQ(builder_declared_name->hygiene_mark, builder_hygiene->generated_fresh_mark);
    EXPECT_EQ(builder_declared_name->declared_name, builder_declaration->generated_item_name);
    EXPECT_EQ(builder_declared_name->namespace_kind, "item");
    EXPECT_GT(builder_declared_name->declared_name_identity.byte_count, 0U);
    expect_contains(builder_declared_name->blocker_reason, "lookup is blocked in M21g");
    EXPECT_FALSE(builder_declared_name->lookup_visible);
    EXPECT_FALSE(builder_declared_name->export_visible);
    EXPECT_FALSE(builder_declared_name->sema_visible);
    EXPECT_FALSE(builder_declared_name->produced_user_generated_code);

    ASSERT_EQ(result.token_materialization_admissions.size(), 2U);
    const frontend::macro::TokenMaterializationAdmissionStub* const builder_admission =
        token_admission_for_input(result, *builder);
    ASSERT_NE(builder_admission, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_admission));
    EXPECT_EQ(builder_admission->part_index, builder->part_index);
    EXPECT_EQ(builder_admission->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_admission->attached_part, builder->attached_part);
    EXPECT_EQ(builder_admission->generated_part, generated.generated_part);
    EXPECT_EQ(builder_admission->expansion_origin, builder->query_key_fingerprint);
    EXPECT_EQ(builder_admission->declaration_identity, builder_declaration->declaration_identity);
    EXPECT_EQ(builder_admission->generated_item_key, builder_declaration->generated_item_key);
    EXPECT_EQ(builder_admission->declared_name_set, builder_hygiene->declared_name_set);
    EXPECT_EQ(builder_admission->declared_name_identity, builder_declared_name->declared_name_identity);
    EXPECT_EQ(builder_admission->hygiene_mark, builder_hygiene->generated_fresh_mark);
    EXPECT_EQ(builder_admission->source_map_identity, builder_trace->generated_source_map_identity);
    EXPECT_EQ(builder_admission->trace_identity, builder_trace->trace_identity);
    EXPECT_GT(builder_admission->token_plan_identity.byte_count, 0U);
    EXPECT_GT(builder_admission->token_buffer_identity.byte_count, 0U);
    EXPECT_NE(builder_admission->token_plan_identity, builder_admission->token_buffer_identity);
    EXPECT_EQ(builder_admission->admission_policy,
        "compiler_owned_attached_item_token_materialization_admission_v1");
    expect_contains(builder_admission->token_stream_name, "m21h-token-stream:0:0:0:0:builder");
    expect_contains(builder_admission->blocker_reason, "non-derive item attribute token materialization remains blocked in M21i");
    EXPECT_TRUE(builder_admission->compiler_owned);
    EXPECT_TRUE(builder_admission->admitted);
    EXPECT_FALSE(builder_admission->materialized_tokens);
    EXPECT_FALSE(builder_admission->generated_source_text);
    EXPECT_FALSE(builder_admission->parse_ready);
    EXPECT_FALSE(builder_admission->external_process_required);
    EXPECT_FALSE(builder_admission->standard_library_required);
    EXPECT_FALSE(builder_admission->runtime_required);
    EXPECT_FALSE(builder_admission->produced_user_generated_code);

    const frontend::macro::TokenMaterializationAdmissionStub* const derive_admission =
        token_admission_for_input(result, *derive);
    ASSERT_NE(derive_admission, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_admission));
    EXPECT_NE(derive_admission->token_stream_name, builder_admission->token_stream_name);
    expect_contains(derive_admission->token_stream_name, "m21h-token-stream:0:0:0:1:derive");
    expect_contains(derive_admission->blocker_reason, "derive token prototype remains parser-blocked in M21i");
    EXPECT_TRUE(derive_admission->compiler_owned);
    EXPECT_TRUE(derive_admission->admitted);
    EXPECT_TRUE(derive_admission->materialized_tokens);
    EXPECT_FALSE(derive_admission->generated_source_text);
    EXPECT_FALSE(derive_admission->parse_ready);
    EXPECT_FALSE(derive_admission->external_process_required);
    EXPECT_FALSE(derive_admission->standard_library_required);
    EXPECT_FALSE(derive_admission->runtime_required);
    EXPECT_FALSE(derive_admission->produced_user_generated_code);

    ASSERT_EQ(result.generated_token_buffers.size(), 2U);
    const frontend::macro::GeneratedTokenBufferStub* const builder_buffer =
        token_buffer_for_input(result, *builder);
    ASSERT_NE(builder_buffer, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_buffer));
    EXPECT_EQ(builder_buffer->part_index, builder->part_index);
    EXPECT_EQ(builder_buffer->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_buffer->attached_part, builder->attached_part);
    EXPECT_EQ(builder_buffer->generated_part, generated.generated_part);
    EXPECT_EQ(builder_buffer->token_plan_identity, builder_admission->token_plan_identity);
    EXPECT_EQ(builder_buffer->token_buffer_identity, builder_admission->token_buffer_identity);
    EXPECT_EQ(builder_buffer->source_map_identity, builder_trace->generated_source_map_identity);
    EXPECT_EQ(builder_buffer->hygiene_mark, builder_hygiene->generated_fresh_mark);
    EXPECT_EQ(builder_buffer->token_stream_name, builder_admission->token_stream_name);
    EXPECT_EQ(builder_buffer->token_buffer_kind, "compiler_owned_empty_token_stream");
    EXPECT_EQ(builder_buffer->token_producer_policy, "compiler_owned_blocked_empty_token_producer_v1");
    EXPECT_GT(builder_buffer->materialization_identity.byte_count, 0U);
    expect_contains(builder_buffer->blocker_reason, "empty and parser-blocked in M21i");
    EXPECT_EQ(builder_buffer->token_count, 0U);
    EXPECT_TRUE(builder_buffer->empty);
    EXPECT_FALSE(builder_buffer->materialized_tokens);
    EXPECT_FALSE(builder_buffer->generated_source_text);
    EXPECT_FALSE(builder_buffer->parser_consumable);
    EXPECT_FALSE(builder_buffer->produced_user_generated_code);

    const frontend::macro::GeneratedTokenBufferStub* const derive_buffer =
        token_buffer_for_input(result, *derive);
    ASSERT_NE(derive_buffer, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_buffer));
    EXPECT_EQ(derive_buffer->part_index, derive->part_index);
    EXPECT_EQ(derive_buffer->attribute_index, derive->attribute_index);
    EXPECT_EQ(derive_buffer->attached_part, derive->attached_part);
    EXPECT_EQ(derive_buffer->generated_part, generated.generated_part);
    EXPECT_EQ(derive_buffer->token_plan_identity, derive_admission->token_plan_identity);
    EXPECT_EQ(derive_buffer->token_buffer_identity, derive_admission->token_buffer_identity);
    EXPECT_GT(derive_buffer->materialization_identity.byte_count, 0U);
    EXPECT_EQ(derive_buffer->source_map_identity, result.trace_stubs[1].generated_source_map_identity);
    EXPECT_EQ(derive_buffer->token_stream_name, derive_admission->token_stream_name);
    EXPECT_EQ(derive_buffer->token_buffer_kind, "compiler_owned_builtin_derive_token_stream_prototype");
    EXPECT_EQ(derive_buffer->token_producer_policy,
        "compiler_owned_builtin_derive_token_producer_prototype_v1");
    expect_contains(derive_buffer->blocker_reason, "generated token buffer remains parser-blocked in M21i");
    EXPECT_EQ(derive_buffer->token_count,
        derive->token_count + EARLY_ITEM_EXPANSION_TEST_DERIVE_SENTINEL_TOKEN_COUNT);
    EXPECT_FALSE(derive_buffer->empty);
    EXPECT_TRUE(derive_buffer->materialized_tokens);
    EXPECT_FALSE(derive_buffer->generated_source_text);
    EXPECT_FALSE(derive_buffer->parser_consumable);
    EXPECT_FALSE(derive_buffer->produced_user_generated_code);

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
    EXPECT_EQ(builder_derive_admission->closure_identity, closure.closure_identity);
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
    EXPECT_EQ(derive_expansion_admission->closure_identity, closure.closure_identity);
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
    EXPECT_EQ(release_gate->closure_identity, closure.closure_identity);
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

    const std::vector<const frontend::macro::GeneratedTokenRecord*> builder_records =
        token_records_for_input(result, *builder);
    EXPECT_TRUE(builder_records.empty());
    const std::vector<const frontend::macro::GeneratedTokenRecord*> derive_records =
        token_records_for_input(result, *derive);
    ASSERT_EQ(derive_records.size(), derive_buffer->token_count);
    const frontend::macro::GeneratedTokenRecord& begin_record = *derive_records.front();
    EXPECT_TRUE(frontend::macro::is_valid(begin_record));
    EXPECT_EQ(begin_record.token_buffer_identity, derive_buffer->token_buffer_identity);
    EXPECT_EQ(begin_record.token_index, 0U);
    EXPECT_EQ(begin_record.kind, syntax::TokenKind::identifier);
    EXPECT_EQ(begin_record.text, "__aurex_builtin_derive_begin");
    EXPECT_EQ(begin_record.token_role, "derive_codegen_begin");
    EXPECT_FALSE(begin_record.parser_visible);
    EXPECT_FALSE(begin_record.produced_user_generated_code);
    ASSERT_GE(derive_records.size(), 3U);
    const frontend::macro::GeneratedTokenRecord& first_source_record = *derive_records[1];
    EXPECT_EQ(first_source_record.kind, config->attributes[1].token_tree[0].kind);
    EXPECT_EQ(first_source_record.text, "__aurex_builtin_derive_source_token_1");
    EXPECT_EQ(first_source_record.token_role, "derive_source_token_placeholder");
    EXPECT_EQ(first_source_record.anchor_range.source.value,
        config->attributes[1].token_tree[0].range.source.value);
    EXPECT_EQ(first_source_record.anchor_range.begin, config->attributes[1].token_tree[0].range.begin);
    EXPECT_EQ(first_source_record.anchor_range.end, config->attributes[1].token_tree[0].range.end);
    const frontend::macro::GeneratedTokenRecord& end_record = *derive_records.back();
    EXPECT_EQ(end_record.token_index, derive_buffer->token_count - 1U);
    EXPECT_EQ(end_record.kind, syntax::TokenKind::identifier);
    EXPECT_EQ(end_record.text, "__aurex_builtin_derive_end");
    EXPECT_EQ(end_record.token_role, "derive_codegen_end");
    EXPECT_EQ(end_record.token_buffer_identity, derive_buffer->token_buffer_identity);
    EXPECT_NE(begin_record.token_identity, first_source_record.token_identity);
    EXPECT_NE(first_source_record.token_identity, end_record.token_identity);

    const std::string summary = frontend::macro::summarize_early_item_expansion(result);
    expect_contains(summary,
        "early_item_expansion name=M26c Builtin Derive Cursor Rollback AST Mutation Verifier Closure");
    expect_contains(summary, "attributes=2");
    expect_contains(summary, "blocked_attributes=1");
    expect_contains(summary, "generated_part_stubs=1");
    expect_contains(summary, "parse_blocked=1");
    expect_contains(summary, "merge_blocked=1");
    expect_contains(summary, "sema_visible_generated_parts=0");
    expect_contains(summary, "hygiene_stubs=2");
    expect_contains(summary, "unresolved_hygiene_stubs=2");
    expect_contains(summary, "declared_name_stubs=2");
    expect_contains(summary, "trace_stubs=2");
    expect_contains(summary, "real_source_maps=0");
    expect_contains(summary, "debug_traces=0");
    expect_contains(summary, "cli_emit_expanded=0");
    expect_contains(summary, "generated_item_declarations=2");
    expect_contains(summary, "planned_generated_item_declarations=2");
    expect_contains(summary, "materialized_generated_items=0");
    expect_contains(summary, "declared_generated_names=2");
    expect_contains(summary, "lookup_visible_declared_names=0");
    expect_contains(summary, "export_visible_declared_names=0");
    expect_contains(summary, "token_materialization_admissions=2");
    expect_contains(summary, "compiler_owned_admissions=2");
    expect_contains(summary, "admitted_token_materializations=2");
    expect_contains(summary, "materialized_token_admissions=1");
    expect_contains(summary, "generated_token_buffers=2");
    expect_contains(summary, "empty_generated_token_buffers=1");
    expect_contains(summary, "materialized_token_buffers=1");
    expect_contains(summary, "compiler_owned_token_buffers=2");
    expect_contains(summary, "generated_token_records=7");
    expect_contains(summary, "compiler_owned_generated_token_records=7");
    expect_contains(summary, "parser_visible_generated_tokens=0");
    expect_contains(summary, "parser_admission_gates=2");
    expect_contains(summary, "compiler_owned_parser_admission_gates=2");
    expect_contains(summary, "token_record_available_gates=1");
    expect_contains(summary, "parser_blocked_token_buffers=2");
    expect_contains(summary, "parser_admitted_token_buffers=0");
    expect_contains(summary, "parser_admission_diagnostics=2");
    expect_contains(summary, "parser_admission_diagnostics_blocked=2");
    expect_contains(summary, "derive_parser_admission_diagnostics=1");
    expect_contains(summary, "empty_parser_admission_diagnostics=1");
    expect_contains(summary, "emit_expanded_projections=0");
    expect_contains(summary, "parser_admission_debug_trace_projections=0");
    expect_contains(summary, "parser_admission_source_map_projections=0");
    expect_contains(summary, "parser_admission_report_entries=2");
    expect_contains(summary, "parser_admission_reports=1");
    expect_contains(summary, "parser_admission_report_blocked_entries=2");
    expect_contains(summary, "derive_parser_admission_report_entries=1");
    expect_contains(summary, "empty_parser_admission_report_entries=1");
    expect_contains(summary, "parser_admission_report_token_record_available_entries=1");
    expect_contains(summary, "parser_admission_report_visible=1");
    expect_contains(summary, "parser_admission_report_query_reusable=1");
    expect_contains(summary, "parser_admission_report_unordered_anchors=0");
    expect_contains(summary, "parser_admission_report_parser_consumable=0");
    expect_contains(summary, "parser_readiness_preflight_entries=2");
    expect_contains(summary, "parser_readiness_preflight_blocked=2");
    expect_contains(summary, "derive_parser_readiness_preflight_entries=1");
    expect_contains(summary, "empty_parser_readiness_preflight_entries=1");
    expect_contains(summary, "parser_readiness_preflight_contiguous_indices=2");
    expect_contains(summary, "parser_readiness_preflight_delimiter_balanced=2");
    expect_contains(summary, "parser_readiness_preflight_source_anchor_covered=2");
    expect_contains(summary, "parser_readiness_preflight_parse_config_compatible=2");
    expect_contains(summary, "parser_readiness_preflight_parser_consumable=0");
    expect_contains(summary, "parser_consumption_contract_gates=1");
    expect_contains(summary, "parser_consumption_contract_blocked_gates=1");
    expect_contains(summary, "parser_consumption_contract_visible=1");
    expect_contains(summary, "parser_consumption_contract_query_reusable=1");
    expect_contains(summary, "parser_consumption_contract_parser_consumable=0");
    expect_contains(summary, "macro_boundary_closure_reports=1");
    expect_contains(summary, "macro_boundary_closure_visible=1");
    expect_contains(summary, "macro_boundary_closure_query_reusable=1");
    expect_contains(summary, "macro_boundary_closure_complete=1");
    expect_contains(summary, "macro_boundary_closure_parser_consumption_enabled=0");
    expect_contains(summary, "builtin_derive_expansion_admissions=2");
    expect_contains(summary, "builtin_derive_expansion_derive_admissions=1");
    expect_contains(summary, "builtin_derive_expansion_non_derive_blocked=1");
    expect_contains(summary, "builtin_derive_expansion_visible=2");
    expect_contains(summary, "builtin_derive_expansion_query_reusable=2");
    expect_contains(summary, "builtin_derive_expansion_capability_candidates=2");
    expect_contains(summary, "builtin_derive_semantic_plans=2");
    expect_contains(summary, "builtin_derive_semantic_plan_visible=2");
    expect_contains(summary, "builtin_derive_semantic_plan_query_reusable=2");
    expect_contains(summary, "builtin_derive_semantic_capabilities=2");
    expect_contains(summary, "builtin_derive_semantic_copy_capabilities=1");
    expect_contains(summary, "builtin_derive_semantic_eq_capabilities=1");
    expect_contains(summary, "builtin_derive_semantic_hash_capabilities=0");
    expect_contains(summary, "builtin_derive_parser_release_gates=1");
    expect_contains(summary, "builtin_derive_parser_release_visible=1");
    expect_contains(summary, "builtin_derive_parser_release_query_reusable=1");
    expect_contains(summary, "builtin_derive_parser_release_parser_consumable=0");
    expect_contains(summary, "builtin_derive_release_hardening_matrices=1");
    expect_contains(summary, "builtin_derive_release_hardening_visible=1");
    expect_contains(summary, "builtin_derive_release_hardening_query_reusable=1");
    expect_contains(summary, "builtin_derive_release_hardening_negative_matrix_complete=1");
    expect_contains(summary, "builtin_derive_release_hardening_parser_consumable=0");
    expect_contains(summary, "builtin_derive_debug_dump_contracts=1");
    expect_contains(summary, "builtin_derive_debug_dump_visible=1");
    expect_contains(summary, "builtin_derive_debug_dump_query_reusable=1");
    expect_contains(summary, "builtin_derive_debug_dump_complete=1");
    expect_contains(summary, "builtin_derive_debug_dump_parser_consumable=0");
    expect_contains(summary, "builtin_derive_rollback_diagnostic_gates=1");
    expect_contains(summary, "builtin_derive_rollback_diagnostic_visible=1");
    expect_contains(summary, "builtin_derive_rollback_diagnostic_query_reusable=1");
    expect_contains(summary, "builtin_derive_rollback_diagnostic_design_complete=1");
    expect_contains(summary, "builtin_derive_rollback_diagnostic_parser_consumable=0");
    expect_contains(summary, "builtin_derive_parser_consumption_admission_protocols=1");
    expect_contains(summary, "builtin_derive_parser_consumption_admission_visible=1");
    expect_contains(summary, "builtin_derive_parser_consumption_admission_query_reusable=1");
    expect_contains(summary, "builtin_derive_parser_consumption_admission_complete=1");
    expect_contains(summary, "builtin_derive_parser_consumption_admission_parser_consumable=0");
    expect_contains(summary, "builtin_derive_checkpoint_rollback_protocols=1");
    expect_contains(summary, "builtin_derive_checkpoint_rollback_visible=1");
    expect_contains(summary, "builtin_derive_checkpoint_rollback_query_reusable=1");
    expect_contains(summary, "builtin_derive_checkpoint_rollback_complete=1");
    expect_contains(summary, "builtin_derive_checkpoint_rollback_parser_consumable=0");
    expect_contains(summary, "builtin_derive_preconsumption_verification_closures=1");
    expect_contains(summary, "builtin_derive_preconsumption_verification_visible=1");
    expect_contains(summary, "builtin_derive_preconsumption_verification_query_reusable=1");
    expect_contains(summary, "builtin_derive_preconsumption_verification_complete=1");
    expect_contains(summary, "builtin_derive_preconsumption_verification_parser_consumable=0");
    expect_contains(summary, "builtin_derive_controlled_dry_run_adapters=1");
    expect_contains(summary, "builtin_derive_controlled_dry_run_adapter_visible=1");
    expect_contains(summary, "builtin_derive_controlled_dry_run_adapter_query_reusable=1");
    expect_contains(summary, "builtin_derive_controlled_dry_run_adapter_complete=1");
    expect_contains(summary, "builtin_derive_controlled_dry_run_adapter_executed=0");
    expect_contains(summary, "builtin_derive_dry_run_rollback_replays=1");
    expect_contains(summary, "builtin_derive_dry_run_rollback_replay_visible=1");
    expect_contains(summary, "builtin_derive_dry_run_rollback_replay_query_reusable=1");
    expect_contains(summary, "builtin_derive_dry_run_rollback_replay_complete=1");
    expect_contains(summary, "builtin_derive_dry_run_rollback_replay_executed=0");
    expect_contains(summary, "builtin_derive_dry_run_negative_matrices=1");
    expect_contains(summary, "builtin_derive_dry_run_negative_matrix_visible=1");
    expect_contains(summary, "builtin_derive_dry_run_negative_matrix_query_reusable=1");
    expect_contains(summary, "builtin_derive_dry_run_negative_matrix_complete=1");
    expect_contains(summary, "builtin_derive_dry_run_negative_matrix_parser_consumable=0");
    expect_contains(summary, "builtin_derive_parser_dry_run_sessions=1");
    expect_contains(summary, "builtin_derive_parser_dry_run_session_visible=1");
    expect_contains(summary, "builtin_derive_parser_dry_run_session_query_reusable=1");
    expect_contains(summary, "builtin_derive_parser_dry_run_session_complete=1");
    expect_contains(summary, "builtin_derive_parser_dry_run_session_executed=0");
    expect_contains(summary, "builtin_derive_parser_dry_run_session_committed=0");
    expect_contains(summary, "builtin_derive_token_cursor_snapshot_proofs=1");
    expect_contains(summary, "builtin_derive_token_cursor_snapshot_proof_visible=1");
    expect_contains(summary, "builtin_derive_token_cursor_snapshot_proof_query_reusable=1");
    expect_contains(summary, "builtin_derive_token_cursor_snapshot_proof_complete=1");
    expect_contains(summary, "builtin_derive_token_cursor_snapshot_proof_cursor_advanced=0");
    expect_contains(summary, "builtin_derive_token_cursor_snapshot_proof_committed=0");
    expect_contains(summary, "builtin_derive_diagnostic_shadow_no_ast_mutation_closures=1");
    expect_contains(summary, "builtin_derive_diagnostic_shadow_no_ast_mutation_visible=1");
    expect_contains(summary, "builtin_derive_diagnostic_shadow_no_ast_mutation_query_reusable=1");
    expect_contains(summary, "builtin_derive_diagnostic_shadow_no_ast_mutation_complete=1");
    expect_contains(summary, "builtin_derive_diagnostic_shadow_no_ast_mutation_executed=0");
    expect_contains(summary, "builtin_derive_diagnostic_shadow_no_ast_mutation_ast_mutation=0");
    expect_contains(summary, "builtin_derive_diagnostic_shadow_no_ast_mutation_parser_consumable=0");
    expect_contains(summary, "builtin_derive_parser_dry_run_admission_gates=1");
    expect_contains(summary, "builtin_derive_parser_dry_run_admission_gate_visible=1");
    expect_contains(summary, "builtin_derive_parser_dry_run_admission_gate_query_reusable=1");
    expect_contains(summary, "builtin_derive_parser_dry_run_admission_gate_complete=1");
    expect_contains(summary, "builtin_derive_parser_dry_run_admission_gate_execution_admitted=0");
    expect_contains(summary, "builtin_derive_parser_dry_run_admission_gate_executed=0");
    expect_contains(summary, "builtin_derive_parser_dry_run_admission_gate_parser_consumable=0");
    expect_contains(summary, "builtin_derive_error_recovery_shadow_diagnostic_gates=1");
    expect_contains(summary, "builtin_derive_error_recovery_shadow_diagnostic_gate_visible=1");
    expect_contains(summary, "builtin_derive_error_recovery_shadow_diagnostic_gate_query_reusable=1");
    expect_contains(summary, "builtin_derive_error_recovery_shadow_diagnostic_gate_complete=1");
    expect_contains(summary, "builtin_derive_error_recovery_shadow_diagnostic_gate_recovery_executed=0");
    expect_contains(summary, "builtin_derive_error_recovery_shadow_diagnostic_gate_diagnostic_emitted=0");
    expect_contains(summary, "builtin_derive_error_recovery_shadow_diagnostic_gate_parser_consumable=0");
    expect_contains(summary, "builtin_derive_cursor_rollback_ast_mutation_verifier_closures=1");
    expect_contains(summary, "builtin_derive_cursor_rollback_ast_mutation_verifier_visible=1");
    expect_contains(summary, "builtin_derive_cursor_rollback_ast_mutation_verifier_query_reusable=1");
    expect_contains(summary, "builtin_derive_cursor_rollback_ast_mutation_verifier_complete=1");
    expect_contains(summary, "builtin_derive_cursor_rollback_ast_mutation_verifier_rollback_executed=0");
    expect_contains(summary, "builtin_derive_cursor_rollback_ast_mutation_verifier_ast_mutation=0");
    expect_contains(summary, "builtin_derive_cursor_rollback_ast_mutation_verifier_parser_consumable=0");
    expect_contains(summary, "generated_source_text=0");
    expect_contains(summary, "parse_ready_token_buffers=0");
    expect_contains(summary, "ast_mutations=0");
    expect_contains(summary, "user_generated_code=0");

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "input #0");
    expect_contains(dump, "attr=builder");
    expect_contains(dump, "disposition=blocked_unimplemented_attribute");
    expect_contains(dump, "generated_part #0");
    expect_contains(dump, "source_role=generated");
    expect_contains(dump, "kind=generated");
    expect_contains(dump, "parse_merge_stub #0");
    expect_contains(dump, "lifecycle=merge_blocked");
    expect_contains(dump, "materialized_buffer=yes");
    expect_contains(dump, "sema_visible=no");
    expect_contains(dump, "parse and merge are blocked in M21e");
    expect_contains(dump, "buffer_identity=");
    expect_contains(dump, "parse_config=");
    expect_contains(dump, "merge_ordering=");
    expect_contains(dump, "source_map #1");
    expect_contains(dump, "hygiene_stub #0");
    expect_contains(dump, "policy=origin_mark_hygiene_v1");
    expect_contains(dump, "resolved=no");
    expect_contains(dump, "declared_names_visible=no");
    expect_contains(dump, "captures_call_site_locals=no");
    expect_contains(dump, "call_site_mark=");
    expect_contains(dump, "definition_site_mark=");
    expect_contains(dump, "generated_fresh_mark=");
    expect_contains(dump, "declared_name_set=");
    expect_contains(dump, "trace_stub #0");
    expect_contains(dump, "policy=expansion_source_map_debug_trace_v1");
    expect_contains(dump, "cli_emit_expanded=no");
    expect_contains(dump, "real macro source map and debug trace are blocked in M21f");
    expect_contains(dump, "trace_identity=");
    expect_contains(dump, "generated_source_map=");
    expect_contains(dump, "diagnostic_anchor=");
    expect_contains(dump, "generated_item_declaration_stub #0");
    expect_contains(dump, "role=attached_item_codegen_declared_names_v1");
    expect_contains(dump, "name=__aurex_macro_declared:0:0:0:0:builder");
    expect_contains(dump, "planned=yes");
    expect_contains(dump, "materialized_tokens=no");
    expect_contains(dump, "generated item declaration materialization is blocked in M21g");
    expect_contains(dump, "declaration_identity=");
    expect_contains(dump, "generated_item_key=");
    expect_contains(dump, "declared_generated_name_stub #0");
    expect_contains(dump, "namespace=item");
    expect_contains(dump, "lookup_visible=no");
    expect_contains(dump, "export_visible=no");
    expect_contains(dump, "declared generated name lookup is blocked in M21g");
    expect_contains(dump, "declared_name_identity=");
    expect_contains(dump, "hygiene_mark=");
    expect_contains(dump, "token_materialization_admission_stub #0");
    expect_contains(dump, "policy=compiler_owned_attached_item_token_materialization_admission_v1");
    expect_contains(dump, "token_stream=m21h-token-stream:0:0:0:0:builder");
    expect_contains(dump, "compiler_owned=yes");
    expect_contains(dump, "admitted=yes");
    expect_contains(dump, "generated_source_text=no");
    expect_contains(dump, "parse_ready=no");
    expect_contains(dump, "external_process_required=no");
    expect_contains(dump, "standard_library_required=no");
    expect_contains(dump, "runtime_required=no");
    expect_contains(dump, "non-derive item attribute token materialization remains blocked in M21i");
    expect_contains(dump, "source_map_identity=");
    expect_contains(dump, "token_plan_identity=");
    expect_contains(dump, "token_buffer_identity=");
    expect_contains(dump, "generated_token_buffer_stub #0");
    expect_contains(dump, "kind=compiler_owned_empty_token_stream");
    expect_contains(dump, "producer=compiler_owned_blocked_empty_token_producer_v1");
    expect_contains(dump, "token_count=0");
    expect_contains(dump, "empty=yes");
    expect_contains(dump, "parser_consumable=no");
    expect_contains(dump, "materialization_identity=");
    expect_contains(dump, "generated token buffer remains empty and parser-blocked in M21i");
    expect_contains(dump, "kind=compiler_owned_builtin_derive_token_stream_prototype");
    expect_contains(dump, "producer=compiler_owned_builtin_derive_token_producer_prototype_v1");
    expect_contains(dump, "generated_token_record #0");
    expect_contains(dump, "text=__aurex_builtin_derive_begin");
    expect_contains(dump, "role=derive_codegen_begin");
    expect_contains(dump, "role=derive_source_token_placeholder");
    expect_contains(dump, "text=__aurex_builtin_derive_end");
    expect_contains(dump, "parser_visible=no");
    expect_contains(dump, "token_identity=");
    expect_contains(dump, "generated_token_parser_admission_gate_stub #0");
    expect_contains(dump, "policy=compiler_owned_generated_token_parser_admission_gate_v1");
    expect_contains(dump, "token_buffer_materialized=no");
    expect_contains(dump, "token_records_available=no");
    expect_contains(dump, "parser_admitted=no");
    expect_contains(dump, "generated_part_parsed=no");
    expect_contains(dump, "generated_part_merged=no");
    expect_contains(dump, "empty or non-derive generated token buffer parser admission remains blocked in M21j");
    expect_contains(dump, "compiler-owned derive generated token buffer parser admission remains blocked in M21j");
    expect_contains(dump, "generated_buffer_identity=");
    expect_contains(dump, "parse_gate_identity=");
    expect_contains(dump, "parser_admission_diagnostic_projection_stub #0");
    expect_contains(dump, "policy=parser_admission_blocked_diagnostic_projection_v1");
    expect_contains(dump, "category=empty_token_buffer_parser_admission_blocked");
    expect_contains(dump, "category=derive_token_buffer_parser_admission_blocked");
    expect_contains(dump, "debug_projection=m21k-parser-admission:0:0:0:0:builder");
    expect_contains(dump, "debug_projection=m21k-parser-admission:0:0:0:1:derive");
    expect_contains(dump, "primary_anchor=");
    expect_contains(dump, "token_tree_anchor=");
    expect_contains(dump, "emit_expanded_available=no");
    expect_contains(dump, "debug_trace_available=no");
    expect_contains(dump, "source_map_available=no");
    expect_contains(dump, "token_buffer_blocker=empty or non-derive generated token buffer parser admission remains blocked in M21j");
    expect_contains(dump, "generated_part_parse_blocker=generated module part parse remains blocked before parser admission diagnostics in M21k");
    expect_contains(dump, "message=generated token buffer is empty and parser admission remains blocked in M21k");
    expect_contains(dump, "message=generated derive token buffer is compiler-owned but parser admission remains blocked in M21k");
    expect_contains(dump, "diagnostic_identity=");
    expect_contains(dump, "diagnostic_anchor=");
    expect_contains(dump, "trace_identity=");
    expect_contains(dump, "parser_admission_report_entry #0");
    expect_contains(dump, "report_index=0");
    expect_contains(dump, "report_index=1");
    expect_contains(dump, "query_projection=m21l-parser-admission-report:0:0");
    expect_contains(dump, "report_visible=yes");
    expect_contains(dump, "query_reusable=yes");
    expect_contains(dump, "report_entry_identity=");
    expect_contains(dump, "parser_admission_diagnostic_report #0");
    expect_contains(dump, "policy=parser_admission_blocked_report_query_projection_v1");
    expect_contains(dump, "query=m21l-parser-admission-report:0:0");
    expect_contains(dump, "entries=2");
    expect_contains(dump, "blocked_entries=2");
    expect_contains(dump, "derive_entries=1");
    expect_contains(dump, "empty_entries=1");
    expect_contains(dump, "token_record_available_entries=1");
    expect_contains(dump, "source_anchor_ordered=yes");
    expect_contains(dump, "parser admission diagnostic report remains parser-blocked in M21l");
    expect_contains(dump, "report_identity=");
    expect_contains(dump, "report_anchor_identity=");
    expect_contains(dump, "report_grouping_identity=");
    expect_contains(dump, "parser_readiness_preflight_entry #0");
    expect_contains(dump, "shape=empty_token_stream_parser_input_blocked");
    expect_contains(dump, "shape=derive_token_buffer_parser_input_candidate");
    expect_contains(dump, "token_indices_contiguous=yes");
    expect_contains(dump, "delimiter_balance=balanced");
    expect_contains(dump, "source_anchor_coverage=covered");
    expect_contains(dump, "parse_config_compatible=yes");
    expect_contains(dump, "hygiene_prerequisite_available=yes");
    expect_contains(dump, "source_map_prerequisite_available=yes");
    expect_contains(dump, "diagnostic_projection_available=yes");
    expect_contains(dump, "policy=generated_token_parser_consumption_readiness_preflight_v1");
    expect_contains(dump, "parser consumption readiness preflight remains parser-blocked in M21m");
    expect_contains(dump, "preflight_identity=");
    expect_contains(dump, "parser_consumption_contract_gate #0");
    expect_contains(dump, "policy=generated_token_parser_consumption_contract_gate_v1");
    expect_contains(dump, "query=m21n-parser-consumption-contract:0:0");
    expect_contains(dump, "preflight_entries=2");
    expect_contains(dump, "all_entries_structurally_checked=yes");
    expect_contains(dump, "parser consumption contract remains parser-blocked in M21n");
    expect_contains(dump, "contract_identity=");
    expect_contains(dump, "contract_grouping_identity=");
    expect_contains(dump, "contract_anchor_identity=");
    expect_contains(dump, "macro_boundary_closure_report #0");
    expect_contains(dump, "policy=m21_macro_expansion_boundary_release_closure_v1");
    expect_contains(dump, "query=m21o-macro-boundary-closure");
    expect_contains(dump, "release_closure_complete=yes");
    expect_contains(dump, "parser_consumption_enabled=no");
    expect_contains(dump, "M21 macro expansion boundary remains parser-blocked after M21o closure");
    expect_contains(dump, "closure_identity=");
    expect_contains(dump, "closure_grouping_identity=");
    expect_contains(dump, "builtin_derive_expansion_admission_gate #0");
    expect_contains(dump, "policy=builtin_derive_expansion_admission_gate_v1");
    expect_contains(dump, "kind=non_derive_attribute_expansion_blocked");
    expect_contains(dump, "kind=builtin_derive_expansion_candidate");
    expect_contains(dump, "query=m22a-builtin-derive-admission:0:0:0:1:derive");
    expect_contains(dump, "capability_candidates=2");
    expect_contains(dump, "builtin derive expansion admission remains parser-blocked in M22a");
    expect_contains(dump, "admission_identity=");
    expect_contains(dump, "builtin_derive_semantic_expansion_plan #0");
    expect_contains(dump, "policy=builtin_derive_semantic_expansion_plan_v1");
    expect_contains(dump, "target_kind=struct");
    expect_contains(dump, "semantic_model=capability_fact_lowering_plan");
    expect_contains(dump, "copy=1");
    expect_contains(dump, "eq=1");
    expect_contains(dump, "hash=0");
    expect_contains(dump, "requires_generated_items=no");
    expect_contains(dump, "requires_standard_library=no");
    expect_contains(dump, "requires_runtime=no");
    expect_contains(dump, "builtin derive semantic expansion remains capability-only and parser-blocked in M22b");
    expect_contains(dump, "capability_set_identity=");
    expect_contains(dump, "semantic_plan_identity=");
    expect_contains(dump, "builtin_derive_parser_consumption_release_gate #0");
    expect_contains(dump, "policy=builtin_derive_parser_consumption_release_gate_v1");
    expect_contains(dump, "query=m22c-builtin-derive-parser-release:0:0");
    expect_contains(dump, "admissions=2");
    expect_contains(dump, "derive_admissions=1");
    expect_contains(dump, "semantic_plans=2");
    expect_contains(dump, "capabilities=2");
    expect_contains(dump, "parser_consumable_contracts=0");
    expect_contains(dump, "rollback_diagnostics_available=yes");
    expect_contains(dump, "builtin derive parser consumption release remains blocked in M22c");
    expect_contains(dump, "release_gate_identity=");
    expect_contains(dump, "builtin_derive_release_hardening_matrix #0");
    expect_contains(dump, "policy=builtin_derive_release_hardening_matrix_v1");
    expect_contains(dump, "query=m22d-builtin-derive-release-hardening:0:0");
    expect_contains(dump, "part_local_admissions=2");
    expect_contains(dump, "global_admissions=2");
    expect_contains(dump, "cross_part_admissions=0");
    expect_contains(dump, "negative_matrix_complete=yes");
    expect_contains(dump, "release_remains_blocked=yes");
    expect_contains(dump, "builtin derive release hardening matrix keeps parser consumption blocked in M22d");
    expect_contains(dump, "hardening_matrix_identity=");
    expect_contains(dump, "builtin_derive_debug_dump_stability_contract #0");
    expect_contains(dump, "policy=builtin_derive_debug_dump_stability_contract_v1");
    expect_contains(dump, "query=m22e-builtin-derive-debug-dump:0:0");
    expect_contains(dump, "dump_sections=4");
    expect_contains(dump, "stable_ordering_available=yes");
    expect_contains(dump, "identity_projection_available=yes");
    expect_contains(dump, "debug_dump_contract_complete=yes");
    expect_contains(dump, "builtin derive debug dump stability remains facts-only and parser-blocked in M22e");
    expect_contains(dump, "debug_dump_contract_identity=");
    expect_contains(dump, "builtin_derive_rollback_diagnostic_design_gate #0");
    expect_contains(dump, "policy=builtin_derive_rollback_diagnostic_design_gate_v1");
    expect_contains(dump, "query=m22f-builtin-derive-rollback-diagnostic:0:0");
    expect_contains(dump, "diagnostic_projections=2");
    expect_contains(dump, "blocked_diagnostics=2");
    expect_contains(dump, "derive_diagnostics=1");
    expect_contains(dump, "empty_diagnostics=1");
    expect_contains(dump, "rollback_execution_enabled=no");
    expect_contains(dump, "builtin derive rollback diagnostics remain design-only and parser-blocked in M22f");
    expect_contains(dump, "rollback_gate_identity=");
    expect_contains(dump, "builtin_derive_parser_consumption_admission_protocol #0");
    expect_contains(dump, "policy=builtin_derive_parser_consumption_admission_protocol_v1");
    expect_contains(dump, "query=m23a-builtin-derive-parser-consumption-admission:0:0");
    expect_contains(dump, "token_buffers=2");
    expect_contains(dump, "derive_candidates=1");
    expect_contains(dump, "empty_candidates=1");
    expect_contains(dump, "parser_admitted=no");
    expect_contains(dump,
        "builtin derive parser consumption admission protocol remains no-parser-consumption in M23a");
    expect_contains(dump, "admission_protocol_identity=");
    expect_contains(dump, "builtin_derive_checkpoint_rollback_protocol #0");
    expect_contains(dump, "policy=builtin_derive_parser_checkpoint_rollback_protocol_v1");
    expect_contains(dump, "query=m23b-builtin-derive-checkpoint-rollback:0:0");
    expect_contains(dump, "checkpoints=3");
    expect_contains(dump, "rollback_plans=3");
    expect_contains(dump, "diagnostic_replay_available=yes");
    expect_contains(dump,
        "builtin derive checkpoint rollback protocol remains design-only and parser-blocked in M23b");
    expect_contains(dump, "checkpoint_protocol_identity=");
    expect_contains(dump, "builtin_derive_preconsumption_verification_closure #0");
    expect_contains(dump, "policy=builtin_derive_parser_preconsumption_verification_closure_v1");
    expect_contains(dump, "query=m23c-builtin-derive-preconsumption-verification:0:0");
    expect_contains(dump, "verification_closure_complete=yes");
    expect_contains(dump, "sema_visible=no");
    expect_contains(dump,
        "builtin derive pre-consumption verification closure keeps parser consumption blocked in M23c");
    expect_contains(dump, "verification_closure_identity=");
    expect_contains(dump, "builtin_derive_controlled_dry_run_adapter #0");
    expect_contains(dump, "policy=builtin_derive_controlled_parser_dry_run_adapter_v1");
    expect_contains(dump, "query=m24a-builtin-derive-controlled-parser-dry-run:0:0");
    expect_contains(dump, "prerequisites=5");
    expect_contains(dump, "dry_run_adapter_complete=yes");
    expect_contains(dump, "dry_run_executed=no");
    expect_contains(dump,
        "builtin derive controlled parser dry-run adapter remains execution-blocked in M24a");
    expect_contains(dump, "dry_run_adapter_identity=");
    expect_contains(dump, "builtin_derive_dry_run_rollback_replay #0");
    expect_contains(dump, "policy=builtin_derive_dry_run_rollback_diagnostic_replay_v1");
    expect_contains(dump, "query=m24b-builtin-derive-dry-run-rollback-replay:0:0");
    expect_contains(dump, "planned_replays=2");
    expect_contains(dump, "executed_replays=0");
    expect_contains(dump, "replay_execution_enabled=no");
    expect_contains(dump,
        "builtin derive dry-run rollback diagnostic replay remains execution-blocked in M24b");
    expect_contains(dump, "replay_protocol_identity=");
    expect_contains(dump, "builtin_derive_dry_run_negative_matrix #0");
    expect_contains(dump, "policy=builtin_derive_dry_run_negative_matrix_closure_v1");
    expect_contains(dump, "query=m24c-builtin-derive-dry-run-negative-matrix:0:0");
    expect_contains(dump, "negative_cases=8");
    expect_contains(dump, "parser_consumable_cases=0");
    expect_contains(dump, "negative_matrix_complete=yes");
    expect_contains(dump,
        "builtin derive dry-run negative matrix keeps parser consumption blocked in M24c");
    expect_contains(dump, "negative_matrix_identity=");
    expect_contains(dump, "builtin_derive_parser_dry_run_session #0");
    expect_contains(dump, "policy=builtin_derive_parser_dry_run_session_boundary_v1");
    expect_contains(dump, "query=m25a-builtin-derive-dry-run-session:0:0");
    expect_contains(dump, "token_buffer_candidates=1");
    expect_contains(dump, "parser_state_snapshots=1");
    expect_contains(dump, "committed_parses=0");
    expect_contains(dump, "sandbox_available=yes");
    expect_contains(dump, "check_only=yes");
    expect_contains(dump, "session_committed=no");
    expect_contains(dump,
        "builtin derive parser dry-run session remains check-only and uncommitted in M25a");
    expect_contains(dump, "dry_run_session_identity=");
    expect_contains(dump, "builtin_derive_token_cursor_snapshot_proof #0");
    expect_contains(dump, "policy=builtin_derive_token_cursor_snapshot_rollback_proof_v1");
    expect_contains(dump, "query=m25b-builtin-derive-token-cursor-rollback-proof:0:0");
    expect_contains(dump, "cursor_snapshots=3");
    expect_contains(dump, "parser_state_snapshots=3");
    expect_contains(dump, "rollback_proofs=3");
    expect_contains(dump, "cursor_commits=0");
    expect_contains(dump, "parser_cursor_advanced=no");
    expect_contains(dump,
        "builtin derive token cursor snapshot rollback proof keeps parser cursor unadvanced in M25b");
    expect_contains(dump, "cursor_snapshot_identity=");
    expect_contains(dump, "builtin_derive_diagnostic_shadow_no_ast_mutation_closure #0");
    expect_contains(dump, "policy=builtin_derive_diagnostic_shadow_no_ast_mutation_closure_v1");
    expect_contains(dump, "query=m25c-builtin-derive-diagnostic-shadow-no-ast-mutation:0:0");
    expect_contains(dump, "diagnostic_shadows=2");
    expect_contains(dump, "executed_shadows=0");
    expect_contains(dump, "ast_mutations=0");
    expect_contains(dump, "no_ast_mutation_verified=yes");
    expect_contains(dump,
        "builtin derive diagnostic shadow replay remains non-executing and no-AST-mutation in M25c");
    expect_contains(dump, "closure_identity=");
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksAttributeTokenTree)
{
    constexpr std::string_view first_source =
        "module macro.early_item_expansion;\n"
        "#[builder(defaults(threads = 4))]\n"
        "struct Config { threads: i32; }\n";
    constexpr std::string_view second_source =
        "module macro.early_item_expansion;\n"
        "#[builder(defaults(threads = 8))]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult first = expand_source(first_source);
    const frontend::macro::EarlyItemExpansionResult second = expand_source(second_source);

    ASSERT_EQ(first.inputs.size(), 1U);
    ASSERT_EQ(second.inputs.size(), 1U);
    EXPECT_NE(first.inputs.front().token_tree_fingerprint, second.inputs.front().token_tree_fingerprint);
    EXPECT_NE(first.inputs.front().query_key_fingerprint, second.inputs.front().query_key_fingerprint);
    EXPECT_NE(first.fingerprint, second.fingerprint);

    frontend::macro::EarlyItemExpansionResult stale = first;
    stale.inputs.front().token_count += 1U;
    EXPECT_FALSE(frontend::macro::is_valid(stale));
}

TEST(CoreUnit, EarlyItemExpansionGeneratedItemNamesIncludeItemIdentity)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n"
        "#[builder(flag)]\n"
        "struct Other { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult result = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(result));
    ASSERT_EQ(result.inputs.size(), 2U);
    ASSERT_EQ(result.generated_item_declarations.size(), 2U);
    ASSERT_EQ(result.declared_generated_names.size(), 2U);
    ASSERT_EQ(result.token_materialization_admissions.size(), 2U);
    ASSERT_EQ(result.generated_token_buffers.size(), 2U);
    ASSERT_EQ(result.parser_admission_gates.size(), 2U);
    ASSERT_EQ(result.parser_admission_diagnostics.size(), 2U);
    ASSERT_EQ(result.parser_admission_report_entries.size(), 2U);
    ASSERT_EQ(result.parser_admission_reports.size(), 1U);

    EXPECT_NE(result.inputs[0].item.value, result.inputs[1].item.value);
    EXPECT_EQ(result.inputs[0].attribute_index, 0U);
    EXPECT_EQ(result.inputs[1].attribute_index, 0U);
    EXPECT_NE(result.generated_item_declarations[0].generated_item_name,
        result.generated_item_declarations[1].generated_item_name);
    expect_contains(result.generated_item_declarations[0].generated_item_name,
        "__aurex_macro_declared:0:0:0:0:builder");
    expect_contains(result.generated_item_declarations[1].generated_item_name,
        "__aurex_macro_declared:0:0:1:0:builder");
    EXPECT_EQ(result.declared_generated_names[0].declared_name,
        result.generated_item_declarations[0].generated_item_name);
    EXPECT_EQ(result.declared_generated_names[1].declared_name,
        result.generated_item_declarations[1].generated_item_name);
    EXPECT_NE(result.token_materialization_admissions[0].token_stream_name,
        result.token_materialization_admissions[1].token_stream_name);
    expect_contains(result.token_materialization_admissions[0].token_stream_name,
        "m21h-token-stream:0:0:0:0:builder");
    expect_contains(result.token_materialization_admissions[1].token_stream_name,
        "m21h-token-stream:0:0:1:0:builder");
    EXPECT_EQ(result.generated_token_buffers[0].token_stream_name,
        result.token_materialization_admissions[0].token_stream_name);
    EXPECT_EQ(result.generated_token_buffers[1].token_stream_name,
        result.token_materialization_admissions[1].token_stream_name);
    EXPECT_EQ(result.parser_admission_gates[0].token_stream_name,
        result.generated_token_buffers[0].token_stream_name);
    EXPECT_EQ(result.parser_admission_gates[1].token_stream_name,
        result.generated_token_buffers[1].token_stream_name);
    EXPECT_NE(result.parser_admission_gates[0].parse_gate_identity,
        result.parser_admission_gates[1].parse_gate_identity);
    EXPECT_NE(result.parser_admission_diagnostics[0].debug_projection_name,
        result.parser_admission_diagnostics[1].debug_projection_name);
    expect_contains(result.parser_admission_diagnostics[0].debug_projection_name,
        "m21k-parser-admission:0:0:0:0:builder");
    expect_contains(result.parser_admission_diagnostics[1].debug_projection_name,
        "m21k-parser-admission:0:0:1:0:builder");
    EXPECT_NE(result.parser_admission_diagnostics[0].diagnostic_identity,
        result.parser_admission_diagnostics[1].diagnostic_identity);
    EXPECT_NE(result.parser_admission_report_entries[0].report_entry_identity,
        result.parser_admission_report_entries[1].report_entry_identity);
    EXPECT_EQ(result.parser_admission_report_entries[0].query_projection_name,
        "m21l-parser-admission-report:0:0");
    EXPECT_EQ(result.parser_admission_report_entries[1].query_projection_name,
        result.parser_admission_report_entries[0].query_projection_name);
    EXPECT_EQ(result.parser_admission_reports.front().entry_count, 2U);
    EXPECT_EQ(result.parser_admission_reports.front().blocked_entry_count, 2U);
    EXPECT_EQ(result.parser_admission_reports.front().empty_entry_count, 2U);
    EXPECT_EQ(result.parser_admission_reports.front().derive_entry_count, 0U);
    EXPECT_EQ(result.builtin_derive_expansion_admissions.front().query_name,
        "m22a-builtin-derive-admission:0:0:0:0:builder");
    EXPECT_EQ(result.builtin_derive_expansion_admissions.back().query_name,
        "m22a-builtin-derive-admission:0:0:1:0:builder");
    EXPECT_EQ(result.builtin_derive_parser_release_gates.front().admission_count, 2U);
    EXPECT_EQ(result.builtin_derive_parser_release_gates.front().derive_admission_count, 0U);
}

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

TEST(CoreUnit, EarlyItemExpansionValidationRejectsNoopBoundaryDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));

    frontend::macro::EarlyItemExpansionResult generated_code = baseline;
    ASSERT_FALSE(generated_code.generated_parts.empty());
    generated_code.generated_parts.front().produced_user_generated_code = true;
    refresh_expansion_result(generated_code);
    EXPECT_FALSE(frontend::macro::is_valid(generated_code));

    frontend::macro::EarlyItemExpansionResult parsed_part = baseline;
    ASSERT_FALSE(parsed_part.generated_parts.empty());
    parsed_part.generated_parts.front().parsed = true;
    refresh_expansion_result(parsed_part);
    EXPECT_FALSE(frontend::macro::is_valid(parsed_part));

    frontend::macro::EarlyItemExpansionResult merged_part = baseline;
    ASSERT_FALSE(merged_part.generated_parts.empty());
    merged_part.generated_parts.front().merged = true;
    refresh_expansion_result(merged_part);
    EXPECT_FALSE(frontend::macro::is_valid(merged_part));

    frontend::macro::EarlyItemExpansionResult missing_stub = baseline;
    ASSERT_FALSE(missing_stub.generated_part_stubs.empty());
    missing_stub.generated_part_stubs.clear();
    refresh_expansion_result(missing_stub);
    EXPECT_FALSE(frontend::macro::is_valid(missing_stub));

    frontend::macro::EarlyItemExpansionResult parse_blocked_stub = baseline;
    ASSERT_FALSE(parse_blocked_stub.generated_part_stubs.empty());
    parse_blocked_stub.generated_part_stubs.front().lifecycle_state =
        frontend::macro::GeneratedModulePartLifecycleState::parse_blocked;
    refresh_expansion_result(parse_blocked_stub);
    EXPECT_EQ(parse_blocked_stub.summary.parse_blocked_count, 1U);
    EXPECT_EQ(parse_blocked_stub.summary.merge_blocked_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(parse_blocked_stub));

    frontend::macro::EarlyItemExpansionResult invalid_lifecycle_stub = baseline;
    ASSERT_FALSE(invalid_lifecycle_stub.generated_part_stubs.empty());
    invalid_lifecycle_stub.generated_part_stubs.front().lifecycle_state =
        static_cast<frontend::macro::GeneratedModulePartLifecycleState>(
            EARLY_ITEM_EXPANSION_TEST_INVALID_LIFECYCLE_STATE);
    refresh_expansion_result(invalid_lifecycle_stub);
    EXPECT_FALSE(frontend::macro::is_valid(invalid_lifecycle_stub));

    frontend::macro::EarlyItemExpansionResult non_materialized_stub = baseline;
    ASSERT_FALSE(non_materialized_stub.generated_part_stubs.empty());
    non_materialized_stub.generated_part_stubs.front().materialized_buffer = false;
    refresh_expansion_result(non_materialized_stub);
    EXPECT_EQ(non_materialized_stub.summary.materialized_buffer_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(non_materialized_stub));

    frontend::macro::EarlyItemExpansionResult parsed_stub = baseline;
    ASSERT_FALSE(parsed_stub.generated_part_stubs.empty());
    parsed_stub.generated_part_stubs.front().parsed = true;
    refresh_expansion_result(parsed_stub);
    EXPECT_EQ(parsed_stub.summary.parsed_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parsed_stub));

    frontend::macro::EarlyItemExpansionResult merged_stub = baseline;
    ASSERT_FALSE(merged_stub.generated_part_stubs.empty());
    merged_stub.generated_part_stubs.front().merged = true;
    refresh_expansion_result(merged_stub);
    EXPECT_EQ(merged_stub.summary.merged_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(merged_stub));

    frontend::macro::EarlyItemExpansionResult sema_visible_stub = baseline;
    ASSERT_FALSE(sema_visible_stub.generated_part_stubs.empty());
    sema_visible_stub.generated_part_stubs.front().sema_visible = true;
    refresh_expansion_result(sema_visible_stub);
    EXPECT_EQ(sema_visible_stub.summary.sema_visible_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(sema_visible_stub));

    frontend::macro::EarlyItemExpansionResult generated_code_stub = baseline;
    ASSERT_FALSE(generated_code_stub.generated_part_stubs.empty());
    generated_code_stub.generated_part_stubs.front().produced_user_generated_code = true;
    refresh_expansion_result(generated_code_stub);
    EXPECT_EQ(generated_code_stub.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_code_stub));

    frontend::macro::EarlyItemExpansionResult empty_buffer_identity = baseline;
    ASSERT_FALSE(empty_buffer_identity.generated_part_stubs.empty());
    empty_buffer_identity.generated_part_stubs.front().generated_buffer_identity = {};
    refresh_expansion_result(empty_buffer_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_buffer_identity));

    frontend::macro::EarlyItemExpansionResult empty_parse_config = baseline;
    ASSERT_FALSE(empty_parse_config.generated_part_stubs.empty());
    empty_parse_config.generated_part_stubs.front().parse_config_fingerprint = {};
    refresh_expansion_result(empty_parse_config);
    EXPECT_FALSE(frontend::macro::is_valid(empty_parse_config));

    frontend::macro::EarlyItemExpansionResult empty_merge_ordering = baseline;
    ASSERT_FALSE(empty_merge_ordering.generated_part_stubs.empty());
    empty_merge_ordering.generated_part_stubs.front().merge_ordering_key = {};
    refresh_expansion_result(empty_merge_ordering);
    EXPECT_FALSE(frontend::macro::is_valid(empty_merge_ordering));

    frontend::macro::EarlyItemExpansionResult empty_origin = baseline;
    ASSERT_FALSE(empty_origin.generated_part_stubs.empty());
    empty_origin.generated_part_stubs.front().expansion_origin = {};
    refresh_expansion_result(empty_origin);
    EXPECT_FALSE(frontend::macro::is_valid(empty_origin));

    frontend::macro::EarlyItemExpansionResult empty_buffer_name = baseline;
    ASSERT_FALSE(empty_buffer_name.generated_part_stubs.empty());
    empty_buffer_name.generated_part_stubs.front().generated_buffer_name.clear();
    refresh_expansion_result(empty_buffer_name);
    EXPECT_FALSE(frontend::macro::is_valid(empty_buffer_name));

    frontend::macro::EarlyItemExpansionResult empty_blocker = baseline;
    ASSERT_FALSE(empty_blocker.generated_part_stubs.empty());
    empty_blocker.generated_part_stubs.front().blocker_reason.clear();
    refresh_expansion_result(empty_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(empty_blocker));

    frontend::macro::EarlyItemExpansionResult wrong_source_part = baseline;
    ASSERT_FALSE(wrong_source_part.generated_part_stubs.empty());
    wrong_source_part.generated_part_stubs.front().source_part =
        wrong_source_part.generated_part_stubs.front().generated_part;
    refresh_expansion_result(wrong_source_part);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_source_part));

    frontend::macro::EarlyItemExpansionResult real_source_map = baseline;
    ASSERT_FALSE(real_source_map.source_maps.empty());
    real_source_map.source_maps.front().real_source_map = true;
    refresh_expansion_result(real_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(real_source_map));

    frontend::macro::EarlyItemExpansionResult source_map_debug_trace = baseline;
    ASSERT_FALSE(source_map_debug_trace.source_maps.empty());
    source_map_debug_trace.source_maps.front().debug_trace_available = true;
    refresh_expansion_result(source_map_debug_trace);
    EXPECT_FALSE(frontend::macro::is_valid(source_map_debug_trace));

    frontend::macro::EarlyItemExpansionResult stale_summary = baseline;
    stale_summary.summary.macro_input_count += 1U;
    EXPECT_FALSE(frontend::macro::is_valid(stale_summary));

    frontend::macro::EarlyItemExpansionResult stale_fingerprint = baseline;
    stale_fingerprint.fingerprint = query::stable_fingerprint("stale early item expansion");
    EXPECT_FALSE(frontend::macro::is_valid(stale_fingerprint));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParseMergeStubContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.generated_part_stubs.empty());

    frontend::macro::EarlyItemExpansionResult buffer_identity = baseline;
    buffer_identity.generated_part_stubs.front().generated_buffer_identity =
        query::stable_fingerprint("different generated buffer identity");
    refresh_expansion_result(buffer_identity);
    EXPECT_NE(buffer_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(buffer_identity));

    frontend::macro::EarlyItemExpansionResult merge_order = baseline;
    merge_order.generated_part_stubs.front().merge_ordering_key =
        query::stable_fingerprint("different merge ordering");
    refresh_expansion_result(merge_order);
    EXPECT_NE(merge_order.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(merge_order));

    frontend::macro::EarlyItemExpansionResult blocker = baseline;
    blocker.generated_part_stubs.front().blocker_reason = "different blocker";
    refresh_expansion_result(blocker);
    EXPECT_NE(blocker.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(blocker));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsHygieneAndTraceStubDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.source_maps.empty());
    ASSERT_FALSE(baseline.hygiene_stubs.empty());
    ASSERT_FALSE(baseline.trace_stubs.empty());

    frontend::macro::EarlyItemExpansionResult missing_source_map = baseline;
    missing_source_map.source_maps.clear();
    refresh_expansion_result(missing_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(missing_source_map));

    frontend::macro::EarlyItemExpansionResult missing_hygiene = baseline;
    missing_hygiene.hygiene_stubs.clear();
    refresh_expansion_result(missing_hygiene);
    EXPECT_EQ(missing_hygiene.summary.hygiene_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_hygiene));

    frontend::macro::EarlyItemExpansionResult empty_call_site_mark = baseline;
    empty_call_site_mark.hygiene_stubs.front().call_site_mark = {};
    refresh_expansion_result(empty_call_site_mark);
    EXPECT_FALSE(frontend::macro::is_valid(empty_call_site_mark));

    frontend::macro::EarlyItemExpansionResult empty_definition_site_mark = baseline;
    empty_definition_site_mark.hygiene_stubs.front().definition_site_mark = {};
    refresh_expansion_result(empty_definition_site_mark);
    EXPECT_FALSE(frontend::macro::is_valid(empty_definition_site_mark));

    frontend::macro::EarlyItemExpansionResult empty_generated_fresh_mark = baseline;
    empty_generated_fresh_mark.hygiene_stubs.front().generated_fresh_mark = {};
    refresh_expansion_result(empty_generated_fresh_mark);
    EXPECT_FALSE(frontend::macro::is_valid(empty_generated_fresh_mark));

    frontend::macro::EarlyItemExpansionResult empty_declared_name_set = baseline;
    empty_declared_name_set.hygiene_stubs.front().declared_name_set = {};
    refresh_expansion_result(empty_declared_name_set);
    EXPECT_EQ(empty_declared_name_set.summary.declared_name_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(empty_declared_name_set));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene_origin = baseline;
    wrong_hygiene_origin.hygiene_stubs.front().expansion_origin =
        query::stable_fingerprint("wrong hygiene origin");
    refresh_expansion_result(wrong_hygiene_origin);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_origin));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene_policy = baseline;
    wrong_hygiene_policy.hygiene_stubs.front().policy = "wrong_hygiene_policy";
    refresh_expansion_result(wrong_hygiene_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_policy));

    frontend::macro::EarlyItemExpansionResult resolved_hygiene = baseline;
    resolved_hygiene.hygiene_stubs.front().resolved = true;
    refresh_expansion_result(resolved_hygiene);
    EXPECT_EQ(resolved_hygiene.summary.unresolved_hygiene_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(resolved_hygiene));

    frontend::macro::EarlyItemExpansionResult visible_declared_names = baseline;
    visible_declared_names.hygiene_stubs.front().declared_names_visible = true;
    refresh_expansion_result(visible_declared_names);
    EXPECT_FALSE(frontend::macro::is_valid(visible_declared_names));

    frontend::macro::EarlyItemExpansionResult call_site_capture = baseline;
    call_site_capture.hygiene_stubs.front().captures_call_site_locals = true;
    refresh_expansion_result(call_site_capture);
    EXPECT_EQ(call_site_capture.summary.call_site_capture_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(call_site_capture));

    frontend::macro::EarlyItemExpansionResult missing_trace = baseline;
    missing_trace.trace_stubs.clear();
    refresh_expansion_result(missing_trace);
    EXPECT_EQ(missing_trace.summary.trace_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_trace));

    frontend::macro::EarlyItemExpansionResult empty_trace_identity = baseline;
    empty_trace_identity.trace_stubs.front().trace_identity = {};
    refresh_expansion_result(empty_trace_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_trace_identity));

    frontend::macro::EarlyItemExpansionResult empty_generated_source_map = baseline;
    empty_generated_source_map.trace_stubs.front().generated_source_map_identity = {};
    refresh_expansion_result(empty_generated_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(empty_generated_source_map));

    frontend::macro::EarlyItemExpansionResult empty_diagnostic_anchor = baseline;
    empty_diagnostic_anchor.trace_stubs.front().diagnostic_anchor = {};
    refresh_expansion_result(empty_diagnostic_anchor);
    EXPECT_FALSE(frontend::macro::is_valid(empty_diagnostic_anchor));

    frontend::macro::EarlyItemExpansionResult wrong_trace_policy = baseline;
    wrong_trace_policy.trace_stubs.front().trace_policy = "wrong_trace_policy";
    refresh_expansion_result(wrong_trace_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace_policy));

    frontend::macro::EarlyItemExpansionResult wrong_trace_blocker = baseline;
    wrong_trace_blocker.trace_stubs.front().blocker_reason = "wrong blocker";
    refresh_expansion_result(wrong_trace_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace_blocker));

    frontend::macro::EarlyItemExpansionResult trace_real_source_map = baseline;
    trace_real_source_map.trace_stubs.front().real_source_map = true;
    refresh_expansion_result(trace_real_source_map);
    EXPECT_EQ(trace_real_source_map.summary.real_source_map_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(trace_real_source_map));

    frontend::macro::EarlyItemExpansionResult trace_debug_available = baseline;
    trace_debug_available.trace_stubs.front().debug_trace_available = true;
    refresh_expansion_result(trace_debug_available);
    EXPECT_EQ(trace_debug_available.summary.debug_trace_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(trace_debug_available));

    frontend::macro::EarlyItemExpansionResult trace_cli_emit = baseline;
    trace_cli_emit.trace_stubs.front().cli_emit_expanded_available = true;
    refresh_expansion_result(trace_cli_emit);
    EXPECT_EQ(trace_cli_emit.summary.cli_emit_expanded_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(trace_cli_emit));

    frontend::macro::EarlyItemExpansionResult wrong_trace_origin = baseline;
    wrong_trace_origin.trace_stubs.front().expansion_origin =
        query::stable_fingerprint("wrong trace origin");
    refresh_expansion_result(wrong_trace_origin);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace_origin));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksHygieneAndTraceStubContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.hygiene_stubs.empty());
    ASSERT_FALSE(baseline.trace_stubs.empty());

    frontend::macro::EarlyItemExpansionResult hygiene_mark = baseline;
    hygiene_mark.hygiene_stubs.front().call_site_mark =
        query::stable_fingerprint("different call site mark");
    refresh_expansion_result(hygiene_mark);
    EXPECT_NE(hygiene_mark.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(hygiene_mark));

    frontend::macro::EarlyItemExpansionResult declared_names = baseline;
    declared_names.hygiene_stubs.front().declared_name_set =
        query::stable_fingerprint("different declared name set");
    refresh_expansion_result(declared_names);
    EXPECT_NE(declared_names.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(declared_names));

    frontend::macro::EarlyItemExpansionResult trace_identity = baseline;
    trace_identity.trace_stubs.front().trace_identity =
        query::stable_fingerprint("different trace identity");
    refresh_expansion_result(trace_identity);
    EXPECT_NE(trace_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(trace_identity));

    frontend::macro::EarlyItemExpansionResult source_map_identity = baseline;
    source_map_identity.trace_stubs.front().generated_source_map_identity =
        query::stable_fingerprint("different generated source map identity");
    refresh_expansion_result(source_map_identity);
    EXPECT_NE(source_map_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(source_map_identity));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsGeneratedItemAndDeclaredNameDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.generated_item_declarations.empty());
    ASSERT_FALSE(baseline.declared_generated_names.empty());

    frontend::macro::EarlyItemExpansionResult missing_declaration = baseline;
    missing_declaration.generated_item_declarations.clear();
    refresh_expansion_result(missing_declaration);
    EXPECT_EQ(missing_declaration.summary.generated_item_declaration_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_declaration));

    frontend::macro::EarlyItemExpansionResult missing_declared_name = baseline;
    missing_declared_name.declared_generated_names.clear();
    refresh_expansion_result(missing_declared_name);
    EXPECT_EQ(missing_declared_name.summary.declared_generated_name_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_declared_name));

    frontend::macro::EarlyItemExpansionResult empty_declaration_identity = baseline;
    empty_declaration_identity.generated_item_declarations.front().declaration_identity = {};
    refresh_expansion_result(empty_declaration_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_declaration_identity));

    frontend::macro::EarlyItemExpansionResult empty_generated_item_key = baseline;
    empty_generated_item_key.generated_item_declarations.front().generated_item_key = {};
    refresh_expansion_result(empty_generated_item_key);
    EXPECT_FALSE(frontend::macro::is_valid(empty_generated_item_key));

    frontend::macro::EarlyItemExpansionResult wrong_declaration_role = baseline;
    wrong_declaration_role.generated_item_declarations.front().declaration_role = "wrong_role";
    refresh_expansion_result(wrong_declaration_role);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declaration_role));

    frontend::macro::EarlyItemExpansionResult wrong_generated_item_name = baseline;
    wrong_generated_item_name.generated_item_declarations.front().generated_item_name =
        "__aurex_macro_declared:wrong";
    refresh_expansion_result(wrong_generated_item_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_item_name));

    frontend::macro::EarlyItemExpansionResult materialized_declaration = baseline;
    materialized_declaration.generated_item_declarations.front().materialized_tokens = true;
    refresh_expansion_result(materialized_declaration);
    EXPECT_EQ(materialized_declaration.summary.materialized_generated_item_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(materialized_declaration));

    frontend::macro::EarlyItemExpansionResult unplanned_declaration = baseline;
    unplanned_declaration.generated_item_declarations.front().planned = false;
    refresh_expansion_result(unplanned_declaration);
    EXPECT_EQ(unplanned_declaration.summary.planned_generated_item_declaration_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(unplanned_declaration));

    frontend::macro::EarlyItemExpansionResult parsed_declaration = baseline;
    parsed_declaration.generated_item_declarations.front().parsed = true;
    refresh_expansion_result(parsed_declaration);
    EXPECT_EQ(parsed_declaration.summary.parsed_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parsed_declaration));

    frontend::macro::EarlyItemExpansionResult sema_visible_declaration = baseline;
    sema_visible_declaration.generated_item_declarations.front().sema_visible = true;
    refresh_expansion_result(sema_visible_declaration);
    EXPECT_EQ(sema_visible_declaration.summary.sema_visible_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(sema_visible_declaration));

    frontend::macro::EarlyItemExpansionResult wrong_declaration_name_set = baseline;
    wrong_declaration_name_set.generated_item_declarations.front().declared_name_set =
        query::stable_fingerprint("wrong declaration name set");
    refresh_expansion_result(wrong_declaration_name_set);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declaration_name_set));

    frontend::macro::EarlyItemExpansionResult empty_declared_name_identity = baseline;
    empty_declared_name_identity.declared_generated_names.front().declared_name_identity = {};
    refresh_expansion_result(empty_declared_name_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_declared_name_identity));

    frontend::macro::EarlyItemExpansionResult empty_hygiene_mark = baseline;
    empty_hygiene_mark.declared_generated_names.front().hygiene_mark = {};
    refresh_expansion_result(empty_hygiene_mark);
    EXPECT_FALSE(frontend::macro::is_valid(empty_hygiene_mark));

    frontend::macro::EarlyItemExpansionResult wrong_namespace = baseline;
    wrong_namespace.declared_generated_names.front().namespace_kind = "value";
    refresh_expansion_result(wrong_namespace);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_namespace));

    frontend::macro::EarlyItemExpansionResult wrong_declared_name = baseline;
    wrong_declared_name.declared_generated_names.front().declared_name =
        "__aurex_macro_declared:wrong";
    refresh_expansion_result(wrong_declared_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declared_name));

    frontend::macro::EarlyItemExpansionResult lookup_visible = baseline;
    lookup_visible.declared_generated_names.front().lookup_visible = true;
    refresh_expansion_result(lookup_visible);
    EXPECT_EQ(lookup_visible.summary.lookup_visible_declared_name_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(lookup_visible));

    frontend::macro::EarlyItemExpansionResult export_visible = baseline;
    export_visible.declared_generated_names.front().export_visible = true;
    refresh_expansion_result(export_visible);
    EXPECT_EQ(export_visible.summary.export_visible_declared_name_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(export_visible));

    frontend::macro::EarlyItemExpansionResult sema_visible_name = baseline;
    sema_visible_name.declared_generated_names.front().sema_visible = true;
    refresh_expansion_result(sema_visible_name);
    EXPECT_EQ(sema_visible_name.summary.sema_visible_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(sema_visible_name));

    frontend::macro::EarlyItemExpansionResult user_code_name = baseline;
    user_code_name.declared_generated_names.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code_name);
    EXPECT_EQ(user_code_name.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code_name));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksGeneratedItemAndDeclaredNameContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.generated_item_declarations.empty());
    ASSERT_FALSE(baseline.declared_generated_names.empty());

    frontend::macro::EarlyItemExpansionResult declaration_identity = baseline;
    declaration_identity.generated_item_declarations.front().declaration_identity =
        query::stable_fingerprint("different declaration identity");
    refresh_expansion_result(declaration_identity);
    EXPECT_NE(declaration_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(declaration_identity));

    frontend::macro::EarlyItemExpansionResult generated_item_key = baseline;
    generated_item_key.generated_item_declarations.front().generated_item_key =
        query::stable_fingerprint("different generated item key");
    refresh_expansion_result(generated_item_key);
    EXPECT_NE(generated_item_key.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(generated_item_key));

    frontend::macro::EarlyItemExpansionResult declared_name_identity = baseline;
    declared_name_identity.declared_generated_names.front().declared_name_identity =
        query::stable_fingerprint("different declared name identity");
    refresh_expansion_result(declared_name_identity);
    EXPECT_NE(declared_name_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(declared_name_identity));

    frontend::macro::EarlyItemExpansionResult hygiene_mark = baseline;
    hygiene_mark.declared_generated_names.front().hygiene_mark =
        query::stable_fingerprint("different declared name hygiene mark");
    refresh_expansion_result(hygiene_mark);
    EXPECT_NE(hygiene_mark.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(hygiene_mark));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsTokenMaterializationAdmissionDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.token_materialization_admissions.empty());
    ASSERT_FALSE(baseline.generated_token_buffers.empty());

    frontend::macro::EarlyItemExpansionResult missing_admission = baseline;
    missing_admission.token_materialization_admissions.clear();
    refresh_expansion_result(missing_admission);
    EXPECT_EQ(missing_admission.summary.token_materialization_admission_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_admission));

    frontend::macro::EarlyItemExpansionResult missing_buffer = baseline;
    missing_buffer.generated_token_buffers.clear();
    refresh_expansion_result(missing_buffer);
    EXPECT_EQ(missing_buffer.summary.generated_token_buffer_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_buffer));

    frontend::macro::EarlyItemExpansionResult empty_token_plan = baseline;
    empty_token_plan.token_materialization_admissions.front().token_plan_identity = {};
    refresh_expansion_result(empty_token_plan);
    EXPECT_FALSE(frontend::macro::is_valid(empty_token_plan));

    frontend::macro::EarlyItemExpansionResult empty_token_buffer_identity = baseline;
    empty_token_buffer_identity.token_materialization_admissions.front().token_buffer_identity = {};
    refresh_expansion_result(empty_token_buffer_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_token_buffer_identity));

    frontend::macro::EarlyItemExpansionResult wrong_declaration_identity = baseline;
    wrong_declaration_identity.token_materialization_admissions.front().declaration_identity =
        query::stable_fingerprint("wrong admission declaration identity");
    refresh_expansion_result(wrong_declaration_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declaration_identity));

    frontend::macro::EarlyItemExpansionResult wrong_generated_item_key = baseline;
    wrong_generated_item_key.token_materialization_admissions.front().generated_item_key =
        query::stable_fingerprint("wrong admission generated item key");
    refresh_expansion_result(wrong_generated_item_key);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_item_key));

    frontend::macro::EarlyItemExpansionResult wrong_declared_name_identity = baseline;
    wrong_declared_name_identity.token_materialization_admissions.front().declared_name_identity =
        query::stable_fingerprint("wrong admission declared name identity");
    refresh_expansion_result(wrong_declared_name_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declared_name_identity));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene_mark = baseline;
    wrong_hygiene_mark.token_materialization_admissions.front().hygiene_mark =
        query::stable_fingerprint("wrong admission hygiene mark");
    refresh_expansion_result(wrong_hygiene_mark);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_mark));

    frontend::macro::EarlyItemExpansionResult wrong_source_map = baseline;
    wrong_source_map.token_materialization_admissions.front().source_map_identity =
        query::stable_fingerprint("wrong admission source map");
    refresh_expansion_result(wrong_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_source_map));

    frontend::macro::EarlyItemExpansionResult wrong_trace_identity = baseline;
    wrong_trace_identity.token_materialization_admissions.front().trace_identity =
        query::stable_fingerprint("wrong admission trace identity");
    refresh_expansion_result(wrong_trace_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace_identity));

    frontend::macro::EarlyItemExpansionResult wrong_policy = baseline;
    wrong_policy.token_materialization_admissions.front().admission_policy = "wrong_admission_policy";
    refresh_expansion_result(wrong_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_policy));

    frontend::macro::EarlyItemExpansionResult wrong_stream_name = baseline;
    wrong_stream_name.token_materialization_admissions.front().token_stream_name =
        "m21h-token-stream:wrong";
    refresh_expansion_result(wrong_stream_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_stream_name));

    frontend::macro::EarlyItemExpansionResult wrong_blocker = baseline;
    wrong_blocker.token_materialization_admissions.front().blocker_reason = "wrong blocker";
    refresh_expansion_result(wrong_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_blocker));

    frontend::macro::EarlyItemExpansionResult not_compiler_owned = baseline;
    not_compiler_owned.token_materialization_admissions.front().compiler_owned = false;
    refresh_expansion_result(not_compiler_owned);
    EXPECT_EQ(not_compiler_owned.summary.compiler_owned_admission_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(not_compiler_owned));

    frontend::macro::EarlyItemExpansionResult not_admitted = baseline;
    not_admitted.token_materialization_admissions.front().admitted = false;
    refresh_expansion_result(not_admitted);
    EXPECT_EQ(not_admitted.summary.admitted_token_materialization_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(not_admitted));

    frontend::macro::EarlyItemExpansionResult materialized_admission = baseline;
    materialized_admission.token_materialization_admissions.front().materialized_tokens = true;
    refresh_expansion_result(materialized_admission);
    EXPECT_EQ(materialized_admission.summary.materialized_token_admission_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(materialized_admission));

    frontend::macro::EarlyItemExpansionResult generated_text = baseline;
    generated_text.token_materialization_admissions.front().generated_source_text = true;
    refresh_expansion_result(generated_text);
    EXPECT_EQ(generated_text.summary.generated_source_text_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_text));

    frontend::macro::EarlyItemExpansionResult parse_ready = baseline;
    parse_ready.token_materialization_admissions.front().parse_ready = true;
    refresh_expansion_result(parse_ready);
    EXPECT_EQ(parse_ready.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parse_ready));

    frontend::macro::EarlyItemExpansionResult external_process = baseline;
    external_process.token_materialization_admissions.front().external_process_required = true;
    refresh_expansion_result(external_process);
    EXPECT_EQ(external_process.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(external_process));

    frontend::macro::EarlyItemExpansionResult standard_library = baseline;
    standard_library.token_materialization_admissions.front().standard_library_required = true;
    refresh_expansion_result(standard_library);
    EXPECT_EQ(standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(standard_library));

    frontend::macro::EarlyItemExpansionResult runtime = baseline;
    runtime.token_materialization_admissions.front().runtime_required = true;
    refresh_expansion_result(runtime);
    EXPECT_EQ(runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(runtime));

    frontend::macro::EarlyItemExpansionResult user_code = baseline;
    user_code.token_materialization_admissions.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code);
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsGeneratedTokenBufferDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.generated_token_buffers.empty());

    frontend::macro::EarlyItemExpansionResult empty_token_plan = baseline;
    empty_token_plan.generated_token_buffers.front().token_plan_identity = {};
    refresh_expansion_result(empty_token_plan);
    EXPECT_FALSE(frontend::macro::is_valid(empty_token_plan));

    frontend::macro::EarlyItemExpansionResult empty_token_buffer_identity = baseline;
    empty_token_buffer_identity.generated_token_buffers.front().token_buffer_identity = {};
    refresh_expansion_result(empty_token_buffer_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_token_buffer_identity));

    frontend::macro::EarlyItemExpansionResult wrong_source_map = baseline;
    wrong_source_map.generated_token_buffers.front().source_map_identity =
        query::stable_fingerprint("wrong token buffer source map");
    refresh_expansion_result(wrong_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_source_map));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene_mark = baseline;
    wrong_hygiene_mark.generated_token_buffers.front().hygiene_mark =
        query::stable_fingerprint("wrong token buffer hygiene mark");
    refresh_expansion_result(wrong_hygiene_mark);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_mark));

    frontend::macro::EarlyItemExpansionResult wrong_stream_name = baseline;
    wrong_stream_name.generated_token_buffers.front().token_stream_name =
        "m21h-token-stream:wrong";
    refresh_expansion_result(wrong_stream_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_stream_name));

    frontend::macro::EarlyItemExpansionResult wrong_kind = baseline;
    wrong_kind.generated_token_buffers.front().token_buffer_kind = "wrong_token_buffer_kind";
    refresh_expansion_result(wrong_kind);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_kind));

    frontend::macro::EarlyItemExpansionResult wrong_producer_policy = baseline;
    wrong_producer_policy.generated_token_buffers.front().token_producer_policy =
        "wrong_token_producer_policy";
    refresh_expansion_result(wrong_producer_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_producer_policy));

    frontend::macro::EarlyItemExpansionResult wrong_blocker = baseline;
    wrong_blocker.generated_token_buffers.front().blocker_reason = "wrong blocker";
    refresh_expansion_result(wrong_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_blocker));

    frontend::macro::EarlyItemExpansionResult empty_materialization_identity = baseline;
    empty_materialization_identity.generated_token_buffers.front().materialization_identity = {};
    refresh_expansion_result(empty_materialization_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_materialization_identity));

    frontend::macro::EarlyItemExpansionResult non_empty_token_count = baseline;
    non_empty_token_count.generated_token_buffers.front().token_count = 1U;
    refresh_expansion_result(non_empty_token_count);
    EXPECT_FALSE(frontend::macro::is_valid(non_empty_token_count));

    frontend::macro::EarlyItemExpansionResult non_empty_buffer = baseline;
    non_empty_buffer.generated_token_buffers.front().empty = false;
    refresh_expansion_result(non_empty_buffer);
    EXPECT_EQ(non_empty_buffer.summary.empty_generated_token_buffer_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(non_empty_buffer));

    frontend::macro::EarlyItemExpansionResult materialized_tokens = baseline;
    materialized_tokens.generated_token_buffers.front().materialized_tokens = true;
    refresh_expansion_result(materialized_tokens);
    EXPECT_EQ(materialized_tokens.summary.materialized_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(materialized_tokens));

    frontend::macro::EarlyItemExpansionResult generated_text = baseline;
    generated_text.generated_token_buffers.front().generated_source_text = true;
    refresh_expansion_result(generated_text);
    EXPECT_EQ(generated_text.summary.generated_source_text_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_text));

    frontend::macro::EarlyItemExpansionResult parser_consumable = baseline;
    parser_consumable.generated_token_buffers.front().parser_consumable = true;
    refresh_expansion_result(parser_consumable);
    EXPECT_EQ(parser_consumable.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_consumable));

    frontend::macro::EarlyItemExpansionResult user_code = baseline;
    user_code.generated_token_buffers.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code);
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsDeriveGeneratedTokenPrototypeDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.inputs.size(), 1U);
    ASSERT_EQ(baseline.token_materialization_admissions.size(), 1U);
    ASSERT_EQ(baseline.generated_token_buffers.size(), 1U);
    ASSERT_FALSE(baseline.generated_token_records.empty());
    EXPECT_TRUE(baseline.token_materialization_admissions.front().materialized_tokens);
    EXPECT_TRUE(baseline.generated_token_buffers.front().materialized_tokens);
    EXPECT_FALSE(baseline.generated_token_buffers.front().empty);
    EXPECT_EQ(baseline.summary.generated_token_record_count,
        baseline.inputs.front().token_count + EARLY_ITEM_EXPANSION_TEST_DERIVE_SENTINEL_TOKEN_COUNT);

    frontend::macro::EarlyItemExpansionResult non_materialized_admission = baseline;
    non_materialized_admission.token_materialization_admissions.front().materialized_tokens = false;
    refresh_expansion_result(non_materialized_admission);
    EXPECT_EQ(non_materialized_admission.summary.materialized_token_admission_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(non_materialized_admission));

    frontend::macro::EarlyItemExpansionResult empty_derive_buffer = baseline;
    empty_derive_buffer.generated_token_buffers.front().empty = true;
    refresh_expansion_result(empty_derive_buffer);
    EXPECT_EQ(empty_derive_buffer.summary.empty_generated_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(empty_derive_buffer));

    frontend::macro::EarlyItemExpansionResult non_materialized_buffer = baseline;
    non_materialized_buffer.generated_token_buffers.front().materialized_tokens = false;
    refresh_expansion_result(non_materialized_buffer);
    EXPECT_EQ(non_materialized_buffer.summary.materialized_token_buffer_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(non_materialized_buffer));

    frontend::macro::EarlyItemExpansionResult wrong_token_count = baseline;
    wrong_token_count.generated_token_buffers.front().token_count = 0U;
    refresh_expansion_result(wrong_token_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_count));

    frontend::macro::EarlyItemExpansionResult wrong_derive_policy = baseline;
    wrong_derive_policy.generated_token_buffers.front().token_producer_policy =
        "compiler_owned_blocked_empty_token_producer_v1";
    refresh_expansion_result(wrong_derive_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_derive_policy));

    frontend::macro::EarlyItemExpansionResult wrong_materialization_identity = baseline;
    wrong_materialization_identity.generated_token_buffers.front().materialization_identity =
        query::stable_fingerprint("wrong derive materialization identity");
    refresh_expansion_result(wrong_materialization_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_materialization_identity));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsGeneratedTokenRecordDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    const base::usize derive_record_index = first_record_index_for_attribute(baseline, "derive");
    ASSERT_LT(derive_record_index, baseline.generated_token_records.size());

    frontend::macro::EarlyItemExpansionResult missing_records = baseline;
    missing_records.generated_token_records.clear();
    refresh_expansion_result(missing_records);
    EXPECT_EQ(missing_records.summary.generated_token_record_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_records));

    frontend::macro::EarlyItemExpansionResult parser_visible = baseline;
    parser_visible.generated_token_records[derive_record_index].parser_visible = true;
    refresh_expansion_result(parser_visible);
    EXPECT_EQ(parser_visible.summary.parser_visible_generated_token_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_visible));

    frontend::macro::EarlyItemExpansionResult user_generated_code = baseline;
    user_generated_code.generated_token_records[derive_record_index].produced_user_generated_code = true;
    refresh_expansion_result(user_generated_code);
    EXPECT_EQ(user_generated_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_generated_code));

    frontend::macro::EarlyItemExpansionResult wrong_token_identity = baseline;
    wrong_token_identity.generated_token_records[derive_record_index].token_identity =
        query::stable_fingerprint("wrong generated token identity");
    refresh_expansion_result(wrong_token_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_identity));

    frontend::macro::EarlyItemExpansionResult wrong_begin_text = baseline;
    wrong_begin_text.generated_token_records[derive_record_index].text =
        "__aurex_builtin_derive_wrong_begin";
    refresh_expansion_result(wrong_begin_text);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_begin_text));

    frontend::macro::EarlyItemExpansionResult wrong_begin_role = baseline;
    wrong_begin_role.generated_token_records[derive_record_index].token_role =
        "wrong_generated_token_role";
    refresh_expansion_result(wrong_begin_role);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_begin_role));

    frontend::macro::EarlyItemExpansionResult wrong_begin_kind = baseline;
    wrong_begin_kind.generated_token_records[derive_record_index].kind = syntax::TokenKind::integer_literal;
    refresh_expansion_result(wrong_begin_kind);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_begin_kind));

    frontend::macro::EarlyItemExpansionResult invalid_record_kind = baseline;
    invalid_record_kind.generated_token_records[derive_record_index].kind = syntax::TokenKind::invalid;
    refresh_expansion_result(invalid_record_kind);
    EXPECT_FALSE(frontend::macro::is_valid(invalid_record_kind));

    frontend::macro::EarlyItemExpansionResult wrong_record_buffer = baseline;
    wrong_record_buffer.generated_token_records[derive_record_index].token_buffer_identity =
        query::stable_fingerprint("wrong generated token buffer identity");
    refresh_expansion_result(wrong_record_buffer);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_record_buffer));

    frontend::macro::EarlyItemExpansionResult stale_record_fingerprint = baseline;
    stale_record_fingerprint.generated_token_records[derive_record_index].text =
        "__aurex_builtin_derive_wrong_begin";
    refresh_expansion_result(stale_record_fingerprint);
    EXPECT_NE(stale_record_fingerprint.fingerprint, baseline.fingerprint);
}

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

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksTokenMaterializationAdmissionContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.token_materialization_admissions.empty());
    ASSERT_FALSE(baseline.generated_token_buffers.empty());

    frontend::macro::EarlyItemExpansionResult token_plan = baseline;
    token_plan.token_materialization_admissions.front().token_plan_identity =
        query::stable_fingerprint("different token plan identity");
    refresh_expansion_result(token_plan);
    EXPECT_NE(token_plan.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(token_plan));

    frontend::macro::EarlyItemExpansionResult token_buffer_identity = baseline;
    token_buffer_identity.token_materialization_admissions.front().token_buffer_identity =
        query::stable_fingerprint("different token buffer identity");
    refresh_expansion_result(token_buffer_identity);
    EXPECT_NE(token_buffer_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(token_buffer_identity));

    frontend::macro::EarlyItemExpansionResult buffer_stream = baseline;
    buffer_stream.generated_token_buffers.front().token_stream_name =
        "m21h-token-stream:different";
    refresh_expansion_result(buffer_stream);
    EXPECT_NE(buffer_stream.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(buffer_stream));

    frontend::macro::EarlyItemExpansionResult buffer_kind = baseline;
    buffer_kind.generated_token_buffers.front().token_buffer_kind = "different_buffer_kind";
    refresh_expansion_result(buffer_kind);
    EXPECT_NE(buffer_kind.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(buffer_kind));
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

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParserAdmissionDiagnosticProjectionContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.parser_admission_diagnostics.empty());

    frontend::macro::EarlyItemExpansionResult diagnostic_identity = baseline;
    diagnostic_identity.parser_admission_diagnostics.front().diagnostic_identity =
        query::stable_fingerprint("different diagnostic identity");
    refresh_expansion_result(diagnostic_identity);
    EXPECT_NE(diagnostic_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(diagnostic_identity));

    frontend::macro::EarlyItemExpansionResult category = baseline;
    category.parser_admission_diagnostics.front().blocker_category =
        "empty_token_buffer_parser_admission_blocked";
    refresh_expansion_result(category);
    EXPECT_NE(category.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(category));

    frontend::macro::EarlyItemExpansionResult debug_projection = baseline;
    debug_projection.parser_admission_diagnostics.front().debug_projection_name =
        "m21k-parser-admission:different";
    refresh_expansion_result(debug_projection);
    EXPECT_NE(debug_projection.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(debug_projection));

    frontend::macro::EarlyItemExpansionResult source_anchor = baseline;
    source_anchor.parser_admission_diagnostics.front().primary_anchor.end += 1U;
    refresh_expansion_result(source_anchor);
    EXPECT_NE(source_anchor.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(source_anchor));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsParserAdmissionReportProjectionDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.parser_admission_report_entries.size(), 2U);
    ASSERT_EQ(baseline.parser_admission_reports.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_entries = baseline;
    missing_entries.parser_admission_report_entries.clear();
    refresh_expansion_result(missing_entries);
    EXPECT_EQ(missing_entries.summary.parser_admission_report_entry_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_entries));

    frontend::macro::EarlyItemExpansionResult missing_reports = baseline;
    missing_reports.parser_admission_reports.clear();
    refresh_expansion_result(missing_reports);
    EXPECT_EQ(missing_reports.summary.parser_admission_report_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_reports));

    frontend::macro::EarlyItemExpansionResult wrong_entry_identity = baseline;
    wrong_entry_identity.parser_admission_report_entries.front().report_entry_identity =
        query::stable_fingerprint("wrong parser admission report entry identity");
    refresh_expansion_result(wrong_entry_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_entry_identity));

    frontend::macro::EarlyItemExpansionResult wrong_entry_anchor = baseline;
    wrong_entry_anchor.parser_admission_report_entries.front().diagnostic_anchor_identity =
        query::stable_fingerprint("wrong parser admission report entry anchor");
    refresh_expansion_result(wrong_entry_anchor);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_entry_anchor));

    frontend::macro::EarlyItemExpansionResult wrong_entry_category = baseline;
    wrong_entry_category.parser_admission_report_entries.back().blocker_category =
        "empty_token_buffer_parser_admission_blocked";
    refresh_expansion_result(wrong_entry_category);
    EXPECT_EQ(wrong_entry_category.summary.parser_admission_report_derive_entry_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_entry_category));

    frontend::macro::EarlyItemExpansionResult wrong_query_name = baseline;
    wrong_query_name.parser_admission_report_entries.front().query_projection_name =
        "m21l-parser-admission-report:wrong";
    refresh_expansion_result(wrong_query_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_query_name));

    frontend::macro::EarlyItemExpansionResult wrong_report_index = baseline;
    wrong_report_index.parser_admission_report_entries.back().report_index = 0U;
    refresh_expansion_result(wrong_report_index);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_index));

    frontend::macro::EarlyItemExpansionResult entry_parser_admitted = baseline;
    entry_parser_admitted.parser_admission_report_entries.front().parser_admitted = true;
    refresh_expansion_result(entry_parser_admitted);
    EXPECT_EQ(entry_parser_admitted.summary.parser_admission_report_blocked_entry_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(entry_parser_admitted));

    frontend::macro::EarlyItemExpansionResult entry_not_visible = baseline;
    entry_not_visible.parser_admission_report_entries.front().report_visible = false;
    refresh_expansion_result(entry_not_visible);
    EXPECT_FALSE(frontend::macro::is_valid(entry_not_visible));

    frontend::macro::EarlyItemExpansionResult entry_not_reusable = baseline;
    entry_not_reusable.parser_admission_report_entries.front().query_reusable = false;
    refresh_expansion_result(entry_not_reusable);
    EXPECT_FALSE(frontend::macro::is_valid(entry_not_reusable));

    frontend::macro::EarlyItemExpansionResult entry_parser_consumable = baseline;
    entry_parser_consumable.parser_admission_report_entries.front().parser_consumable = true;
    refresh_expansion_result(entry_parser_consumable);
    EXPECT_FALSE(frontend::macro::is_valid(entry_parser_consumable));

    frontend::macro::EarlyItemExpansionResult entry_emit_expanded = baseline;
    entry_emit_expanded.parser_admission_report_entries.front().emit_expanded_available = true;
    refresh_expansion_result(entry_emit_expanded);
    EXPECT_EQ(entry_emit_expanded.summary.emit_expanded_projection_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(entry_emit_expanded));

    frontend::macro::EarlyItemExpansionResult entry_user_code = baseline;
    entry_user_code.parser_admission_report_entries.front().produced_user_generated_code = true;
    refresh_expansion_result(entry_user_code);
    EXPECT_EQ(entry_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(entry_user_code));

    frontend::macro::EarlyItemExpansionResult wrong_report_identity = baseline;
    wrong_report_identity.parser_admission_reports.front().report_identity =
        query::stable_fingerprint("wrong parser admission report identity");
    refresh_expansion_result(wrong_report_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_identity));

    frontend::macro::EarlyItemExpansionResult wrong_report_group = baseline;
    wrong_report_group.parser_admission_reports.front().report_grouping_identity =
        query::stable_fingerprint("wrong parser admission report grouping");
    refresh_expansion_result(wrong_report_group);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_group));

    frontend::macro::EarlyItemExpansionResult wrong_report_anchor = baseline;
    wrong_report_anchor.parser_admission_reports.front().report_anchor_identity =
        query::stable_fingerprint("wrong parser admission report anchor");
    refresh_expansion_result(wrong_report_anchor);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_anchor));

    frontend::macro::EarlyItemExpansionResult wrong_parse_config = baseline;
    wrong_parse_config.parser_admission_reports.front().parse_config_fingerprint =
        query::stable_fingerprint("wrong parser admission report parse config");
    refresh_expansion_result(wrong_parse_config);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_parse_config));

    frontend::macro::EarlyItemExpansionResult wrong_generated_buffer = baseline;
    wrong_generated_buffer.parser_admission_reports.front().generated_buffer_identity =
        query::stable_fingerprint("wrong parser admission report generated buffer");
    refresh_expansion_result(wrong_generated_buffer);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_buffer));

    frontend::macro::EarlyItemExpansionResult wrong_policy = baseline;
    wrong_policy.parser_admission_reports.front().report_policy = "wrong_report_policy";
    refresh_expansion_result(wrong_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_policy));

    frontend::macro::EarlyItemExpansionResult wrong_report_query = baseline;
    wrong_report_query.parser_admission_reports.front().report_query_name =
        "m21l-parser-admission-report:wrong";
    refresh_expansion_result(wrong_report_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_query));

    frontend::macro::EarlyItemExpansionResult wrong_blocker = baseline;
    wrong_blocker.parser_admission_reports.front().blocked_reason = "wrong report blocker";
    refresh_expansion_result(wrong_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_blocker));

    frontend::macro::EarlyItemExpansionResult wrong_entry_count = baseline;
    wrong_entry_count.parser_admission_reports.front().entry_count = 1U;
    refresh_expansion_result(wrong_entry_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_entry_count));

    frontend::macro::EarlyItemExpansionResult wrong_category_totals = baseline;
    wrong_category_totals.parser_admission_reports.front().derive_entry_count = 0U;
    refresh_expansion_result(wrong_category_totals);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_category_totals));

    frontend::macro::EarlyItemExpansionResult report_not_visible = baseline;
    report_not_visible.parser_admission_reports.front().report_visible = false;
    refresh_expansion_result(report_not_visible);
    EXPECT_EQ(report_not_visible.summary.parser_admission_report_visible_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(report_not_visible));

    frontend::macro::EarlyItemExpansionResult report_not_reusable = baseline;
    report_not_reusable.parser_admission_reports.front().query_reusable = false;
    refresh_expansion_result(report_not_reusable);
    EXPECT_EQ(report_not_reusable.summary.parser_admission_report_query_reusable_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(report_not_reusable));

    frontend::macro::EarlyItemExpansionResult unordered_report = baseline;
    unordered_report.parser_admission_reports.front().source_anchor_ordered = false;
    refresh_expansion_result(unordered_report);
    EXPECT_EQ(unordered_report.summary.parser_admission_report_unordered_anchor_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(unordered_report));

    frontend::macro::EarlyItemExpansionResult report_parser_admitted = baseline;
    report_parser_admitted.parser_admission_reports.front().parser_admitted = true;
    refresh_expansion_result(report_parser_admitted);
    EXPECT_FALSE(frontend::macro::is_valid(report_parser_admitted));

    frontend::macro::EarlyItemExpansionResult report_parse_ready = baseline;
    report_parse_ready.parser_admission_reports.front().parse_ready = true;
    refresh_expansion_result(report_parse_ready);
    EXPECT_EQ(report_parse_ready.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_parse_ready));

    frontend::macro::EarlyItemExpansionResult report_parser_consumable = baseline;
    report_parser_consumable.parser_admission_reports.front().parser_consumable = true;
    refresh_expansion_result(report_parser_consumable);
    EXPECT_EQ(report_parser_consumable.summary.parser_admission_report_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_parser_consumable));

    frontend::macro::EarlyItemExpansionResult report_emit_expanded = baseline;
    report_emit_expanded.parser_admission_reports.front().emit_expanded_available = true;
    refresh_expansion_result(report_emit_expanded);
    EXPECT_EQ(report_emit_expanded.summary.emit_expanded_projection_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_emit_expanded));

    frontend::macro::EarlyItemExpansionResult report_debug_trace = baseline;
    report_debug_trace.parser_admission_reports.front().debug_trace_available = true;
    refresh_expansion_result(report_debug_trace);
    EXPECT_EQ(report_debug_trace.summary.parser_admission_debug_trace_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_debug_trace));

    frontend::macro::EarlyItemExpansionResult report_source_map = baseline;
    report_source_map.parser_admission_reports.front().source_map_available = true;
    refresh_expansion_result(report_source_map);
    EXPECT_EQ(report_source_map.summary.parser_admission_source_map_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_source_map));

    frontend::macro::EarlyItemExpansionResult report_user_code = baseline;
    report_user_code.parser_admission_reports.front().produced_user_generated_code = true;
    refresh_expansion_result(report_user_code);
    EXPECT_EQ(report_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_user_code));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParserAdmissionReportProjectionContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.parser_admission_report_entries.empty());
    ASSERT_FALSE(baseline.parser_admission_reports.empty());

    frontend::macro::EarlyItemExpansionResult entry_identity = baseline;
    entry_identity.parser_admission_report_entries.front().report_entry_identity =
        query::stable_fingerprint("different parser admission report entry identity");
    refresh_expansion_result(entry_identity);
    EXPECT_NE(entry_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(entry_identity));

    frontend::macro::EarlyItemExpansionResult entry_query = baseline;
    entry_query.parser_admission_report_entries.front().query_projection_name =
        "m21l-parser-admission-report:different";
    refresh_expansion_result(entry_query);
    EXPECT_NE(entry_query.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(entry_query));

    frontend::macro::EarlyItemExpansionResult report_identity = baseline;
    report_identity.parser_admission_reports.front().report_identity =
        query::stable_fingerprint("different parser admission report identity");
    refresh_expansion_result(report_identity);
    EXPECT_NE(report_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(report_identity));

    frontend::macro::EarlyItemExpansionResult report_totals = baseline;
    report_totals.parser_admission_reports.front().blocked_entry_count = 0U;
    refresh_expansion_result(report_totals);
    EXPECT_NE(report_totals.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(report_totals));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsParserReadinessAndContractDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.parser_readiness_preflight_entries.size(), 2U);
    ASSERT_EQ(baseline.parser_consumption_contract_gates.size(), 1U);
    ASSERT_EQ(baseline.macro_boundary_closure_reports.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_preflight = baseline;
    missing_preflight.parser_readiness_preflight_entries.clear();
    refresh_expansion_result(missing_preflight);
    EXPECT_EQ(missing_preflight.summary.parser_readiness_preflight_entry_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_preflight));

    frontend::macro::EarlyItemExpansionResult wrong_preflight_identity = baseline;
    wrong_preflight_identity.parser_readiness_preflight_entries.front().preflight_identity =
        query::stable_fingerprint("wrong parser readiness preflight identity");
    refresh_expansion_result(wrong_preflight_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_preflight_identity));

    frontend::macro::EarlyItemExpansionResult wrong_preflight_shape = baseline;
    wrong_preflight_shape.parser_readiness_preflight_entries.back().token_stream_shape =
        "empty_token_stream_parser_input_blocked";
    refresh_expansion_result(wrong_preflight_shape);
    EXPECT_EQ(wrong_preflight_shape.summary.parser_readiness_preflight_derive_entry_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_preflight_shape));

    frontend::macro::EarlyItemExpansionResult non_contiguous = baseline;
    non_contiguous.parser_readiness_preflight_entries.back().token_indices_contiguous = false;
    refresh_expansion_result(non_contiguous);
    EXPECT_EQ(non_contiguous.summary.parser_readiness_preflight_contiguous_index_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(non_contiguous));

    frontend::macro::EarlyItemExpansionResult unbalanced_delimiter = baseline;
    unbalanced_delimiter.parser_readiness_preflight_entries.back().delimiter_balanced = false;
    refresh_expansion_result(unbalanced_delimiter);
    EXPECT_EQ(unbalanced_delimiter.summary.parser_readiness_preflight_delimiter_balanced_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(unbalanced_delimiter));

    frontend::macro::EarlyItemExpansionResult uncovered_anchor = baseline;
    uncovered_anchor.parser_readiness_preflight_entries.back().source_anchors_covered = false;
    refresh_expansion_result(uncovered_anchor);
    EXPECT_EQ(uncovered_anchor.summary.parser_readiness_preflight_source_anchor_covered_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(uncovered_anchor));

    frontend::macro::EarlyItemExpansionResult incompatible_parse_config = baseline;
    incompatible_parse_config.parser_readiness_preflight_entries.back().parse_config_compatible = false;
    refresh_expansion_result(incompatible_parse_config);
    EXPECT_EQ(incompatible_parse_config.summary.parser_readiness_preflight_parse_config_compatible_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(incompatible_parse_config));

    frontend::macro::EarlyItemExpansionResult preflight_parser_consumable = baseline;
    preflight_parser_consumable.parser_readiness_preflight_entries.back().parser_consumable = true;
    refresh_expansion_result(preflight_parser_consumable);
    EXPECT_EQ(preflight_parser_consumable.summary.parser_readiness_preflight_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(preflight_parser_consumable));

    frontend::macro::EarlyItemExpansionResult missing_contract = baseline;
    missing_contract.parser_consumption_contract_gates.clear();
    refresh_expansion_result(missing_contract);
    EXPECT_EQ(missing_contract.summary.parser_consumption_contract_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_contract));

    frontend::macro::EarlyItemExpansionResult wrong_contract_identity = baseline;
    wrong_contract_identity.parser_consumption_contract_gates.front().contract_identity =
        query::stable_fingerprint("wrong parser consumption contract identity");
    refresh_expansion_result(wrong_contract_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_contract_identity));

    frontend::macro::EarlyItemExpansionResult wrong_contract_counts = baseline;
    wrong_contract_counts.parser_consumption_contract_gates.front().preflight_entry_count = 1U;
    refresh_expansion_result(wrong_contract_counts);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_contract_counts));

    frontend::macro::EarlyItemExpansionResult contract_not_visible = baseline;
    contract_not_visible.parser_consumption_contract_gates.front().contract_visible = false;
    refresh_expansion_result(contract_not_visible);
    EXPECT_EQ(contract_not_visible.summary.parser_consumption_contract_visible_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_not_visible));

    frontend::macro::EarlyItemExpansionResult contract_parser_consumable = baseline;
    contract_parser_consumable.parser_consumption_contract_gates.front().parser_consumable = true;
    refresh_expansion_result(contract_parser_consumable);
    EXPECT_EQ(contract_parser_consumable.summary.parser_consumption_contract_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_parser_consumable));

    frontend::macro::EarlyItemExpansionResult contract_emit_expanded = baseline;
    contract_emit_expanded.parser_consumption_contract_gates.front().emit_expanded_available = true;
    refresh_expansion_result(contract_emit_expanded);
    EXPECT_EQ(contract_emit_expanded.summary.emit_expanded_projection_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_emit_expanded));

    frontend::macro::EarlyItemExpansionResult contract_source_map = baseline;
    contract_source_map.parser_consumption_contract_gates.front().source_map_available = true;
    refresh_expansion_result(contract_source_map);
    EXPECT_EQ(contract_source_map.summary.parser_admission_source_map_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_source_map));

    frontend::macro::EarlyItemExpansionResult contract_user_code = baseline;
    contract_user_code.parser_consumption_contract_gates.front().produced_user_generated_code = true;
    refresh_expansion_result(contract_user_code);
    EXPECT_EQ(contract_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_user_code));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsMacroBoundaryClosureDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.macro_boundary_closure_reports.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_closure = baseline;
    missing_closure.macro_boundary_closure_reports.clear();
    refresh_expansion_result(missing_closure);
    EXPECT_EQ(missing_closure.summary.macro_boundary_closure_report_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_closure));

    frontend::macro::EarlyItemExpansionResult wrong_identity = baseline;
    wrong_identity.macro_boundary_closure_reports.front().closure_identity =
        query::stable_fingerprint("wrong macro boundary closure identity");
    refresh_expansion_result(wrong_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_identity));

    frontend::macro::EarlyItemExpansionResult wrong_counts = baseline;
    wrong_counts.macro_boundary_closure_reports.front().parser_consumption_contract_gate_count = 0U;
    refresh_expansion_result(wrong_counts);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_counts));

    frontend::macro::EarlyItemExpansionResult not_complete = baseline;
    not_complete.macro_boundary_closure_reports.front().release_closure_complete = false;
    refresh_expansion_result(not_complete);
    EXPECT_EQ(not_complete.summary.macro_boundary_closure_complete_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(not_complete));

    frontend::macro::EarlyItemExpansionResult parser_consumption_enabled = baseline;
    parser_consumption_enabled.macro_boundary_closure_reports.front().parser_consumption_enabled = true;
    refresh_expansion_result(parser_consumption_enabled);
    EXPECT_EQ(parser_consumption_enabled.summary.macro_boundary_closure_parser_consumption_enabled_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_consumption_enabled));

    frontend::macro::EarlyItemExpansionResult standard_library = baseline;
    standard_library.macro_boundary_closure_reports.front().standard_library_required = true;
    refresh_expansion_result(standard_library);
    EXPECT_EQ(standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(standard_library));

    frontend::macro::EarlyItemExpansionResult runtime = baseline;
    runtime.macro_boundary_closure_reports.front().runtime_required = true;
    refresh_expansion_result(runtime);
    EXPECT_EQ(runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(runtime));

    frontend::macro::EarlyItemExpansionResult external_process = baseline;
    external_process.macro_boundary_closure_reports.front().external_process_required = true;
    refresh_expansion_result(external_process);
    EXPECT_EQ(external_process.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(external_process));

    frontend::macro::EarlyItemExpansionResult user_code = baseline;
    user_code.macro_boundary_closure_reports.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code);
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParserReadinessContractAndClosure)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.parser_readiness_preflight_entries.empty());
    ASSERT_FALSE(baseline.parser_consumption_contract_gates.empty());
    ASSERT_FALSE(baseline.macro_boundary_closure_reports.empty());

    frontend::macro::EarlyItemExpansionResult preflight_identity = baseline;
    preflight_identity.parser_readiness_preflight_entries.front().preflight_identity =
        query::stable_fingerprint("different parser readiness preflight identity");
    refresh_expansion_result(preflight_identity);
    EXPECT_NE(preflight_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(preflight_identity));

    frontend::macro::EarlyItemExpansionResult preflight_shape = baseline;
    preflight_shape.parser_readiness_preflight_entries.front().token_stream_shape =
        "empty_token_stream_parser_input_blocked";
    refresh_expansion_result(preflight_shape);
    EXPECT_NE(preflight_shape.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(preflight_shape));

    frontend::macro::EarlyItemExpansionResult contract_identity = baseline;
    contract_identity.parser_consumption_contract_gates.front().contract_identity =
        query::stable_fingerprint("different parser consumption contract identity");
    refresh_expansion_result(contract_identity);
    EXPECT_NE(contract_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(contract_identity));

    frontend::macro::EarlyItemExpansionResult contract_totals = baseline;
    contract_totals.parser_consumption_contract_gates.front().delimiter_balanced_entry_count = 0U;
    refresh_expansion_result(contract_totals);
    EXPECT_NE(contract_totals.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(contract_totals));

    frontend::macro::EarlyItemExpansionResult closure_identity = baseline;
    closure_identity.macro_boundary_closure_reports.front().closure_identity =
        query::stable_fingerprint("different macro boundary closure identity");
    refresh_expansion_result(closure_identity);
    EXPECT_NE(closure_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(closure_identity));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsBuiltinDeriveM22Drift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.builtin_derive_expansion_admissions.size(), 2U);
    ASSERT_EQ(baseline.builtin_derive_semantic_plans.size(), 2U);
    ASSERT_EQ(baseline.builtin_derive_parser_release_gates.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_admissions = baseline;
    missing_admissions.builtin_derive_expansion_admissions.clear();
    refresh_expansion_result(missing_admissions);
    EXPECT_EQ(missing_admissions.summary.builtin_derive_expansion_admission_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_admissions));

    frontend::macro::EarlyItemExpansionResult wrong_admission_identity = baseline;
    wrong_admission_identity.builtin_derive_expansion_admissions.back().admission_identity =
        query::stable_fingerprint("wrong builtin derive admission identity");
    refresh_expansion_result(wrong_admission_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_admission_identity));

    frontend::macro::EarlyItemExpansionResult wrong_admission_kind = baseline;
    wrong_admission_kind.builtin_derive_expansion_admissions.back().admission_kind =
        "non_derive_attribute_expansion_blocked";
    refresh_expansion_result(wrong_admission_kind);
    EXPECT_EQ(wrong_admission_kind.summary.builtin_derive_expansion_derive_admission_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_admission_kind));

    frontend::macro::EarlyItemExpansionResult wrong_admission_query = baseline;
    wrong_admission_query.builtin_derive_expansion_admissions.back().query_name =
        "m22a-builtin-derive-admission:wrong";
    refresh_expansion_result(wrong_admission_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_admission_query));

    frontend::macro::EarlyItemExpansionResult wrong_candidate_count = baseline;
    wrong_candidate_count.builtin_derive_expansion_admissions.back().capability_candidate_count = 1U;
    refresh_expansion_result(wrong_candidate_count);
    EXPECT_NE(wrong_candidate_count.summary.builtin_derive_expansion_capability_candidate_count,
        baseline.summary.builtin_derive_expansion_capability_candidate_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_candidate_count));

    frontend::macro::EarlyItemExpansionResult admission_parser_enabled = baseline;
    admission_parser_enabled.builtin_derive_expansion_admissions.back().parser_consumption_enabled = true;
    refresh_expansion_result(admission_parser_enabled);
    EXPECT_EQ(admission_parser_enabled.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_parser_enabled));

    frontend::macro::EarlyItemExpansionResult admission_standard_library = baseline;
    admission_standard_library.builtin_derive_expansion_admissions.back().standard_library_required = true;
    refresh_expansion_result(admission_standard_library);
    EXPECT_EQ(admission_standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_standard_library));

    frontend::macro::EarlyItemExpansionResult missing_plans = baseline;
    missing_plans.builtin_derive_semantic_plans.clear();
    refresh_expansion_result(missing_plans);
    EXPECT_EQ(missing_plans.summary.builtin_derive_semantic_plan_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_plans));

    frontend::macro::EarlyItemExpansionResult wrong_semantic_identity = baseline;
    wrong_semantic_identity.builtin_derive_semantic_plans.back().semantic_plan_identity =
        query::stable_fingerprint("wrong builtin derive semantic plan");
    refresh_expansion_result(wrong_semantic_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_semantic_identity));

    frontend::macro::EarlyItemExpansionResult wrong_target_kind = baseline;
    wrong_target_kind.builtin_derive_semantic_plans.back().target_kind = "enum";
    refresh_expansion_result(wrong_target_kind);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_target_kind));

    frontend::macro::EarlyItemExpansionResult wrong_capability_total = baseline;
    wrong_capability_total.builtin_derive_semantic_plans.back().capability_count = 2U;
    refresh_expansion_result(wrong_capability_total);
    EXPECT_EQ(wrong_capability_total.summary.builtin_derive_semantic_capability_count, 2U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_capability_total));

    frontend::macro::EarlyItemExpansionResult semantic_requires_generated_items = baseline;
    semantic_requires_generated_items.builtin_derive_semantic_plans.back().requires_generated_items = true;
    refresh_expansion_result(semantic_requires_generated_items);
    EXPECT_FALSE(frontend::macro::is_valid(semantic_requires_generated_items));

    frontend::macro::EarlyItemExpansionResult semantic_runtime = baseline;
    semantic_runtime.builtin_derive_semantic_plans.back().requires_runtime = true;
    refresh_expansion_result(semantic_runtime);
    EXPECT_EQ(semantic_runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(semantic_runtime));

    frontend::macro::EarlyItemExpansionResult missing_release = baseline;
    missing_release.builtin_derive_parser_release_gates.clear();
    refresh_expansion_result(missing_release);
    EXPECT_EQ(missing_release.summary.builtin_derive_parser_release_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_release));

    frontend::macro::EarlyItemExpansionResult wrong_release_identity = baseline;
    wrong_release_identity.builtin_derive_parser_release_gates.front().release_gate_identity =
        query::stable_fingerprint("wrong builtin derive parser release gate");
    refresh_expansion_result(wrong_release_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_release_identity));

    frontend::macro::EarlyItemExpansionResult wrong_release_counts = baseline;
    wrong_release_counts.builtin_derive_parser_release_gates.front().derive_admission_count = 0U;
    refresh_expansion_result(wrong_release_counts);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_release_counts));

    frontend::macro::EarlyItemExpansionResult release_parser_enabled = baseline;
    release_parser_enabled.builtin_derive_parser_release_gates.front().parser_consumption_enabled = true;
    refresh_expansion_result(release_parser_enabled);
    EXPECT_EQ(release_parser_enabled.summary.builtin_derive_parser_release_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(release_parser_enabled));

    frontend::macro::EarlyItemExpansionResult release_debug_trace = baseline;
    release_debug_trace.builtin_derive_parser_release_gates.front().debug_trace_available = true;
    refresh_expansion_result(release_debug_trace);
    EXPECT_EQ(release_debug_trace.summary.parser_admission_debug_trace_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(release_debug_trace));

    frontend::macro::EarlyItemExpansionResult release_external_process = baseline;
    release_external_process.builtin_derive_parser_release_gates.front().external_process_required = true;
    refresh_expansion_result(release_external_process);
    EXPECT_EQ(release_external_process.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(release_external_process));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksBuiltinDeriveM22Facts)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.builtin_derive_expansion_admissions.empty());
    ASSERT_FALSE(baseline.builtin_derive_semantic_plans.empty());
    ASSERT_FALSE(baseline.builtin_derive_parser_release_gates.empty());

    frontend::macro::EarlyItemExpansionResult admission_identity = baseline;
    admission_identity.builtin_derive_expansion_admissions.front().admission_identity =
        query::stable_fingerprint("different builtin derive admission identity");
    refresh_expansion_result(admission_identity);
    EXPECT_NE(admission_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(admission_identity));

    frontend::macro::EarlyItemExpansionResult admission_query = baseline;
    admission_query.builtin_derive_expansion_admissions.front().query_name =
        "m22a-builtin-derive-admission:different";
    refresh_expansion_result(admission_query);
    EXPECT_NE(admission_query.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(admission_query));

    frontend::macro::EarlyItemExpansionResult semantic_identity = baseline;
    semantic_identity.builtin_derive_semantic_plans.front().semantic_plan_identity =
        query::stable_fingerprint("different builtin derive semantic plan identity");
    refresh_expansion_result(semantic_identity);
    EXPECT_NE(semantic_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(semantic_identity));

    frontend::macro::EarlyItemExpansionResult semantic_capabilities = baseline;
    semantic_capabilities.builtin_derive_semantic_plans.front().hash_capability_count = 0U;
    refresh_expansion_result(semantic_capabilities);
    EXPECT_NE(semantic_capabilities.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(semantic_capabilities));

    frontend::macro::EarlyItemExpansionResult release_identity = baseline;
    release_identity.builtin_derive_parser_release_gates.front().release_gate_identity =
        query::stable_fingerprint("different builtin derive parser release identity");
    refresh_expansion_result(release_identity);
    EXPECT_NE(release_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(release_identity));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsBuiltinDeriveM22ReleaseHardeningDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.builtin_derive_release_hardening_matrices.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_debug_dump_contracts.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_rollback_diagnostic_gates.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_matrix = baseline;
    missing_matrix.builtin_derive_release_hardening_matrices.clear();
    refresh_expansion_result(missing_matrix);
    EXPECT_EQ(missing_matrix.summary.builtin_derive_release_hardening_matrix_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_matrix));

    frontend::macro::EarlyItemExpansionResult wrong_matrix_identity = baseline;
    wrong_matrix_identity.builtin_derive_release_hardening_matrices.front().hardening_matrix_identity =
        query::stable_fingerprint("wrong builtin derive release hardening matrix");
    refresh_expansion_result(wrong_matrix_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_matrix_identity));

    frontend::macro::EarlyItemExpansionResult wrong_matrix_query = baseline;
    wrong_matrix_query.builtin_derive_release_hardening_matrices.front().hardening_query_name =
        "m22d-builtin-derive-release-hardening:wrong";
    refresh_expansion_result(wrong_matrix_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_matrix_query));

    frontend::macro::EarlyItemExpansionResult wrong_cross_part_count = baseline;
    wrong_cross_part_count.builtin_derive_release_hardening_matrices.front().cross_part_admission_count = 1U;
    refresh_expansion_result(wrong_cross_part_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_cross_part_count));

    frontend::macro::EarlyItemExpansionResult matrix_not_complete = baseline;
    matrix_not_complete.builtin_derive_release_hardening_matrices.front().negative_matrix_complete = false;
    refresh_expansion_result(matrix_not_complete);
    EXPECT_EQ(matrix_not_complete.summary.builtin_derive_release_hardening_negative_matrix_complete_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(matrix_not_complete));

    frontend::macro::EarlyItemExpansionResult matrix_parser_enabled = baseline;
    matrix_parser_enabled.builtin_derive_release_hardening_matrices.front().parser_consumption_enabled = true;
    refresh_expansion_result(matrix_parser_enabled);
    EXPECT_EQ(matrix_parser_enabled.summary.builtin_derive_release_hardening_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(matrix_parser_enabled));

    frontend::macro::EarlyItemExpansionResult matrix_standard_library = baseline;
    matrix_standard_library.builtin_derive_release_hardening_matrices.front().standard_library_required = true;
    refresh_expansion_result(matrix_standard_library);
    EXPECT_EQ(matrix_standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(matrix_standard_library));

    frontend::macro::EarlyItemExpansionResult missing_debug_contract = baseline;
    missing_debug_contract.builtin_derive_debug_dump_contracts.clear();
    refresh_expansion_result(missing_debug_contract);
    EXPECT_EQ(missing_debug_contract.summary.builtin_derive_debug_dump_contract_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_debug_contract));

    frontend::macro::EarlyItemExpansionResult wrong_debug_identity = baseline;
    wrong_debug_identity.builtin_derive_debug_dump_contracts.front().debug_dump_contract_identity =
        query::stable_fingerprint("wrong builtin derive debug dump contract");
    refresh_expansion_result(wrong_debug_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_debug_identity));

    frontend::macro::EarlyItemExpansionResult wrong_debug_query = baseline;
    wrong_debug_query.builtin_derive_debug_dump_contracts.front().debug_dump_query_name =
        "m22e-builtin-derive-debug-dump:wrong";
    refresh_expansion_result(wrong_debug_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_debug_query));

    frontend::macro::EarlyItemExpansionResult debug_unstable_order = baseline;
    debug_unstable_order.builtin_derive_debug_dump_contracts.front().stable_ordering_available = false;
    refresh_expansion_result(debug_unstable_order);
    EXPECT_FALSE(frontend::macro::is_valid(debug_unstable_order));

    frontend::macro::EarlyItemExpansionResult debug_incomplete = baseline;
    debug_incomplete.builtin_derive_debug_dump_contracts.front().debug_dump_contract_complete = false;
    refresh_expansion_result(debug_incomplete);
    EXPECT_EQ(debug_incomplete.summary.builtin_derive_debug_dump_complete_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(debug_incomplete));

    frontend::macro::EarlyItemExpansionResult debug_parser_enabled = baseline;
    debug_parser_enabled.builtin_derive_debug_dump_contracts.front().parser_consumption_enabled = true;
    refresh_expansion_result(debug_parser_enabled);
    EXPECT_EQ(debug_parser_enabled.summary.builtin_derive_debug_dump_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(debug_parser_enabled));

    frontend::macro::EarlyItemExpansionResult debug_runtime = baseline;
    debug_runtime.builtin_derive_debug_dump_contracts.front().runtime_required = true;
    refresh_expansion_result(debug_runtime);
    EXPECT_EQ(debug_runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(debug_runtime));

    frontend::macro::EarlyItemExpansionResult missing_rollback = baseline;
    missing_rollback.builtin_derive_rollback_diagnostic_gates.clear();
    refresh_expansion_result(missing_rollback);
    EXPECT_EQ(missing_rollback.summary.builtin_derive_rollback_diagnostic_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_rollback));

    frontend::macro::EarlyItemExpansionResult wrong_rollback_identity = baseline;
    wrong_rollback_identity.builtin_derive_rollback_diagnostic_gates.front().rollback_gate_identity =
        query::stable_fingerprint("wrong builtin derive rollback diagnostic gate");
    refresh_expansion_result(wrong_rollback_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_rollback_identity));

    frontend::macro::EarlyItemExpansionResult wrong_rollback_query = baseline;
    wrong_rollback_query.builtin_derive_rollback_diagnostic_gates.front().rollback_query_name =
        "m22f-builtin-derive-rollback-diagnostic:wrong";
    refresh_expansion_result(wrong_rollback_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_rollback_query));

    frontend::macro::EarlyItemExpansionResult wrong_diagnostic_count = baseline;
    wrong_diagnostic_count.builtin_derive_rollback_diagnostic_gates.front().derive_diagnostic_count = 0U;
    refresh_expansion_result(wrong_diagnostic_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_diagnostic_count));

    frontend::macro::EarlyItemExpansionResult rollback_prerequisite_missing = baseline;
    rollback_prerequisite_missing.builtin_derive_rollback_diagnostic_gates.front()
        .release_rollback_plan_complete = false;
    refresh_expansion_result(rollback_prerequisite_missing);
    EXPECT_EQ(rollback_prerequisite_missing.summary.builtin_derive_rollback_diagnostic_design_complete_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_prerequisite_missing));

    frontend::macro::EarlyItemExpansionResult rollback_execution_enabled = baseline;
    rollback_execution_enabled.builtin_derive_rollback_diagnostic_gates.front().rollback_execution_enabled = true;
    refresh_expansion_result(rollback_execution_enabled);
    EXPECT_EQ(rollback_execution_enabled.summary.builtin_derive_rollback_diagnostic_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_execution_enabled));

    frontend::macro::EarlyItemExpansionResult rollback_parser_enabled = baseline;
    rollback_parser_enabled.builtin_derive_rollback_diagnostic_gates.front().parser_consumption_enabled = true;
    refresh_expansion_result(rollback_parser_enabled);
    EXPECT_EQ(rollback_parser_enabled.summary.builtin_derive_rollback_diagnostic_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_parser_enabled));

    frontend::macro::EarlyItemExpansionResult rollback_external = baseline;
    rollback_external.builtin_derive_rollback_diagnostic_gates.front().external_process_required = true;
    refresh_expansion_result(rollback_external);
    EXPECT_EQ(rollback_external.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_external));

    frontend::macro::EarlyItemExpansionResult rollback_user_code = baseline;
    rollback_user_code.builtin_derive_rollback_diagnostic_gates.front().produced_user_generated_code = true;
    refresh_expansion_result(rollback_user_code);
    EXPECT_EQ(rollback_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_user_code));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksBuiltinDeriveM22ReleaseHardeningFacts)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.builtin_derive_release_hardening_matrices.empty());
    ASSERT_FALSE(baseline.builtin_derive_debug_dump_contracts.empty());
    ASSERT_FALSE(baseline.builtin_derive_rollback_diagnostic_gates.empty());

    frontend::macro::EarlyItemExpansionResult matrix_identity = baseline;
    matrix_identity.builtin_derive_release_hardening_matrices.front().hardening_matrix_identity =
        query::stable_fingerprint("different builtin derive release hardening matrix identity");
    refresh_expansion_result(matrix_identity);
    EXPECT_NE(matrix_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(matrix_identity));

    frontend::macro::EarlyItemExpansionResult matrix_counts = baseline;
    matrix_counts.builtin_derive_release_hardening_matrices.front().part_local_derive_admission_count = 0U;
    refresh_expansion_result(matrix_counts);
    EXPECT_NE(matrix_counts.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(matrix_counts));

    frontend::macro::EarlyItemExpansionResult debug_identity = baseline;
    debug_identity.builtin_derive_debug_dump_contracts.front().debug_dump_contract_identity =
        query::stable_fingerprint("different builtin derive debug dump identity");
    refresh_expansion_result(debug_identity);
    EXPECT_NE(debug_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(debug_identity));

    frontend::macro::EarlyItemExpansionResult debug_complete = baseline;
    debug_complete.builtin_derive_debug_dump_contracts.front().debug_dump_contract_complete = false;
    refresh_expansion_result(debug_complete);
    EXPECT_NE(debug_complete.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(debug_complete));

    frontend::macro::EarlyItemExpansionResult rollback_identity = baseline;
    rollback_identity.builtin_derive_rollback_diagnostic_gates.front().rollback_gate_identity =
        query::stable_fingerprint("different builtin derive rollback diagnostic identity");
    refresh_expansion_result(rollback_identity);
    EXPECT_NE(rollback_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_identity));

    frontend::macro::EarlyItemExpansionResult rollback_design = baseline;
    rollback_design.builtin_derive_rollback_diagnostic_gates.front()
        .diagnostic_grouping_available = false;
    refresh_expansion_result(rollback_design);
    EXPECT_NE(rollback_design.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_design));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsBuiltinDeriveM23ParserConsumptionAdmissionDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.builtin_derive_parser_consumption_admission_protocols.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_checkpoint_rollback_protocols.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_preconsumption_verification_closures.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_admission = baseline;
    missing_admission.builtin_derive_parser_consumption_admission_protocols.clear();
    refresh_expansion_result(missing_admission);
    EXPECT_EQ(missing_admission.summary.builtin_derive_parser_consumption_admission_protocol_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_admission));

    frontend::macro::EarlyItemExpansionResult wrong_admission_identity = baseline;
    wrong_admission_identity.builtin_derive_parser_consumption_admission_protocols.front()
        .admission_protocol_identity = query::stable_fingerprint("wrong m23a admission protocol identity");
    refresh_expansion_result(wrong_admission_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_admission_identity));

    frontend::macro::EarlyItemExpansionResult wrong_admission_query = baseline;
    wrong_admission_query.builtin_derive_parser_consumption_admission_protocols.front()
        .admission_query_name = "m23a-builtin-derive-parser-consumption-admission:wrong";
    refresh_expansion_result(wrong_admission_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_admission_query));

    frontend::macro::EarlyItemExpansionResult admission_parser_admitted = baseline;
    admission_parser_admitted.builtin_derive_parser_consumption_admission_protocols.front()
        .parser_admitted = true;
    refresh_expansion_result(admission_parser_admitted);
    EXPECT_EQ(admission_parser_admitted.summary.builtin_derive_parser_consumption_admission_parser_consumable_count,
        1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_parser_admitted));

    frontend::macro::EarlyItemExpansionResult admission_parser_enabled = baseline;
    admission_parser_enabled.builtin_derive_parser_consumption_admission_protocols.front()
        .parser_consumption_enabled = true;
    refresh_expansion_result(admission_parser_enabled);
    EXPECT_EQ(admission_parser_enabled.summary.builtin_derive_parser_consumption_admission_parser_consumable_count,
        1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_parser_enabled));

    frontend::macro::EarlyItemExpansionResult admission_standard_library = baseline;
    admission_standard_library.builtin_derive_parser_consumption_admission_protocols.front()
        .standard_library_required = true;
    refresh_expansion_result(admission_standard_library);
    EXPECT_EQ(admission_standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_standard_library));

    frontend::macro::EarlyItemExpansionResult admission_runtime = baseline;
    admission_runtime.builtin_derive_parser_consumption_admission_protocols.front().runtime_required = true;
    refresh_expansion_result(admission_runtime);
    EXPECT_EQ(admission_runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_runtime));

    frontend::macro::EarlyItemExpansionResult admission_external = baseline;
    admission_external.builtin_derive_parser_consumption_admission_protocols.front()
        .external_process_required = true;
    refresh_expansion_result(admission_external);
    EXPECT_EQ(admission_external.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_external));

    frontend::macro::EarlyItemExpansionResult admission_user_code = baseline;
    admission_user_code.builtin_derive_parser_consumption_admission_protocols.front()
        .produced_user_generated_code = true;
    refresh_expansion_result(admission_user_code);
    EXPECT_EQ(admission_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_user_code));

    frontend::macro::EarlyItemExpansionResult missing_checkpoint = baseline;
    missing_checkpoint.builtin_derive_checkpoint_rollback_protocols.clear();
    refresh_expansion_result(missing_checkpoint);
    EXPECT_EQ(missing_checkpoint.summary.builtin_derive_checkpoint_rollback_protocol_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_checkpoint));

    frontend::macro::EarlyItemExpansionResult wrong_checkpoint_identity = baseline;
    wrong_checkpoint_identity.builtin_derive_checkpoint_rollback_protocols.front()
        .checkpoint_protocol_identity = query::stable_fingerprint("wrong m23b checkpoint protocol identity");
    refresh_expansion_result(wrong_checkpoint_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_checkpoint_identity));

    frontend::macro::EarlyItemExpansionResult wrong_checkpoint_query = baseline;
    wrong_checkpoint_query.builtin_derive_checkpoint_rollback_protocols.front()
        .checkpoint_query_name = "m23b-builtin-derive-checkpoint-rollback:wrong";
    refresh_expansion_result(wrong_checkpoint_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_checkpoint_query));

    frontend::macro::EarlyItemExpansionResult wrong_checkpoint_count = baseline;
    wrong_checkpoint_count.builtin_derive_checkpoint_rollback_protocols.front().checkpoint_count = 2U;
    refresh_expansion_result(wrong_checkpoint_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_checkpoint_count));

    frontend::macro::EarlyItemExpansionResult checkpoint_replay_missing = baseline;
    checkpoint_replay_missing.builtin_derive_checkpoint_rollback_protocols.front()
        .diagnostic_replay_available = false;
    refresh_expansion_result(checkpoint_replay_missing);
    EXPECT_FALSE(frontend::macro::is_valid(checkpoint_replay_missing));

    frontend::macro::EarlyItemExpansionResult checkpoint_rollback_enabled = baseline;
    checkpoint_rollback_enabled.builtin_derive_checkpoint_rollback_protocols.front()
        .rollback_execution_enabled = true;
    refresh_expansion_result(checkpoint_rollback_enabled);
    EXPECT_EQ(checkpoint_rollback_enabled.summary.builtin_derive_checkpoint_rollback_parser_consumable_count,
        1U);
    EXPECT_FALSE(frontend::macro::is_valid(checkpoint_rollback_enabled));

    frontend::macro::EarlyItemExpansionResult checkpoint_parser_enabled = baseline;
    checkpoint_parser_enabled.builtin_derive_checkpoint_rollback_protocols.front()
        .parser_consumption_enabled = true;
    refresh_expansion_result(checkpoint_parser_enabled);
    EXPECT_EQ(checkpoint_parser_enabled.summary.builtin_derive_checkpoint_rollback_parser_consumable_count,
        1U);
    EXPECT_FALSE(frontend::macro::is_valid(checkpoint_parser_enabled));

    frontend::macro::EarlyItemExpansionResult missing_closure = baseline;
    missing_closure.builtin_derive_preconsumption_verification_closures.clear();
    refresh_expansion_result(missing_closure);
    EXPECT_EQ(missing_closure.summary.builtin_derive_preconsumption_verification_closure_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_closure));

    frontend::macro::EarlyItemExpansionResult wrong_closure_identity = baseline;
    wrong_closure_identity.builtin_derive_preconsumption_verification_closures.front()
        .verification_closure_identity = query::stable_fingerprint("wrong m23c verification closure identity");
    refresh_expansion_result(wrong_closure_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_closure_identity));

    frontend::macro::EarlyItemExpansionResult wrong_closure_query = baseline;
    wrong_closure_query.builtin_derive_preconsumption_verification_closures.front()
        .verification_query_name = "m23c-builtin-derive-preconsumption-verification:wrong";
    refresh_expansion_result(wrong_closure_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_closure_query));

    frontend::macro::EarlyItemExpansionResult closure_incomplete = baseline;
    closure_incomplete.builtin_derive_preconsumption_verification_closures.front()
        .verification_closure_complete = false;
    refresh_expansion_result(closure_incomplete);
    EXPECT_EQ(closure_incomplete.summary.builtin_derive_preconsumption_verification_complete_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_incomplete));

    frontend::macro::EarlyItemExpansionResult closure_parser_enabled = baseline;
    closure_parser_enabled.builtin_derive_preconsumption_verification_closures.front()
        .parser_consumption_enabled = true;
    refresh_expansion_result(closure_parser_enabled);
    EXPECT_EQ(closure_parser_enabled.summary.builtin_derive_preconsumption_verification_parser_consumable_count,
        1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_parser_enabled));

    frontend::macro::EarlyItemExpansionResult closure_sema_visible = baseline;
    closure_sema_visible.builtin_derive_preconsumption_verification_closures.front().sema_visible = true;
    refresh_expansion_result(closure_sema_visible);
    EXPECT_EQ(closure_sema_visible.summary.sema_visible_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_sema_visible));

    frontend::macro::EarlyItemExpansionResult closure_part_parsed = baseline;
    closure_part_parsed.builtin_derive_preconsumption_verification_closures.front().generated_part_parsed =
        true;
    refresh_expansion_result(closure_part_parsed);
    EXPECT_EQ(closure_part_parsed.summary.parsed_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_part_parsed));

    frontend::macro::EarlyItemExpansionResult closure_standard_library = baseline;
    closure_standard_library.builtin_derive_preconsumption_verification_closures.front()
        .standard_library_required = true;
    refresh_expansion_result(closure_standard_library);
    EXPECT_EQ(closure_standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_standard_library));

    frontend::macro::EarlyItemExpansionResult closure_runtime = baseline;
    closure_runtime.builtin_derive_preconsumption_verification_closures.front().runtime_required = true;
    refresh_expansion_result(closure_runtime);
    EXPECT_EQ(closure_runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_runtime));

    frontend::macro::EarlyItemExpansionResult closure_external = baseline;
    closure_external.builtin_derive_preconsumption_verification_closures.front()
        .external_process_required = true;
    refresh_expansion_result(closure_external);
    EXPECT_EQ(closure_external.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_external));

    frontend::macro::EarlyItemExpansionResult closure_user_code = baseline;
    closure_user_code.builtin_derive_preconsumption_verification_closures.front()
        .produced_user_generated_code = true;
    refresh_expansion_result(closure_user_code);
    EXPECT_EQ(closure_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_user_code));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksBuiltinDeriveM23ParserConsumptionAdmissionFacts)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.builtin_derive_parser_consumption_admission_protocols.empty());
    ASSERT_FALSE(baseline.builtin_derive_checkpoint_rollback_protocols.empty());
    ASSERT_FALSE(baseline.builtin_derive_preconsumption_verification_closures.empty());

    frontend::macro::EarlyItemExpansionResult admission_identity = baseline;
    admission_identity.builtin_derive_parser_consumption_admission_protocols.front()
        .admission_protocol_identity =
        query::stable_fingerprint("different m23a admission protocol identity");
    refresh_expansion_result(admission_identity);
    EXPECT_NE(admission_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(admission_identity));

    frontend::macro::EarlyItemExpansionResult admission_counts = baseline;
    admission_counts.builtin_derive_parser_consumption_admission_protocols.front().derive_candidate_count =
        0U;
    refresh_expansion_result(admission_counts);
    EXPECT_NE(admission_counts.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(admission_counts));

    frontend::macro::EarlyItemExpansionResult checkpoint_identity = baseline;
    checkpoint_identity.builtin_derive_checkpoint_rollback_protocols.front()
        .checkpoint_protocol_identity =
        query::stable_fingerprint("different m23b checkpoint rollback identity");
    refresh_expansion_result(checkpoint_identity);
    EXPECT_NE(checkpoint_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(checkpoint_identity));

    frontend::macro::EarlyItemExpansionResult checkpoint_count = baseline;
    checkpoint_count.builtin_derive_checkpoint_rollback_protocols.front().rollback_plan_count = 2U;
    refresh_expansion_result(checkpoint_count);
    EXPECT_NE(checkpoint_count.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(checkpoint_count));

    frontend::macro::EarlyItemExpansionResult closure_identity = baseline;
    closure_identity.builtin_derive_preconsumption_verification_closures.front()
        .verification_closure_identity =
        query::stable_fingerprint("different m23c verification closure identity");
    refresh_expansion_result(closure_identity);
    EXPECT_NE(closure_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(closure_identity));

    frontend::macro::EarlyItemExpansionResult closure_query = baseline;
    closure_query.builtin_derive_preconsumption_verification_closures.front()
        .verification_query_name = "m23c-builtin-derive-preconsumption-verification:different";
    refresh_expansion_result(closure_query);
    EXPECT_NE(closure_query.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(closure_query));
}

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

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.front()
                .parser_cursor_advanced = true;
        }));
}

TEST(CoreUnit, EarlyItemExpansionCollectsAurexMacroSurfaceAdmissionGates)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n"
        "macro derive Inspect {\n"
        "  match item(target) -> { target }\n"
        "}\n"
        "macro const TokenBuild {\n"
        "  match tokens(input) -> { input }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult result = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(result));
    EXPECT_EQ(result.summary.macro_input_count, 0U);
    EXPECT_EQ(result.summary.attribute_input_count, 0U);
    EXPECT_EQ(result.summary.generated_part_placeholder_count, 0U);
    ASSERT_EQ(result.aurex_macro_surface_admission_gates.size(), 3U);
    EXPECT_EQ(result.summary.aurex_macro_surface_source_item_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_surface_admission_gate_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_declarative_surface_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_user_derive_surface_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_compile_time_surface_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_surface_visible_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_surface_query_reusable_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_surface_body_balanced_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_surface_match_clause_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_surface_expansion_enabled_count, 0U);
    EXPECT_EQ(result.summary.aurex_macro_surface_compile_time_execution_enabled_count, 0U);
    EXPECT_EQ(result.summary.aurex_macro_surface_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.parse_ready_token_buffer_count, 0U);
    EXPECT_EQ(result.summary.ast_mutation_count, 0U);
    EXPECT_EQ(result.summary.sema_visible_generated_part_count, 0U);
    EXPECT_EQ(result.summary.user_generated_code_count, 0U);
    EXPECT_EQ(result.summary.standard_library_required_count, 0U);
    EXPECT_EQ(result.summary.runtime_required_count, 0U);
    EXPECT_EQ(result.summary.external_process_required_count, 0U);

    const frontend::macro::AurexMacroSurfaceAdmissionGate* const build_vec =
        aurex_macro_surface_gate_by_name(result, "BuildVec");
    const frontend::macro::AurexMacroSurfaceAdmissionGate* const inspect =
        aurex_macro_surface_gate_by_name(result, "Inspect");
    const frontend::macro::AurexMacroSurfaceAdmissionGate* const token_build =
        aurex_macro_surface_gate_by_name(result, "TokenBuild");
    ASSERT_NE(build_vec, nullptr);
    ASSERT_NE(inspect, nullptr);
    ASSERT_NE(token_build, nullptr);

    EXPECT_TRUE(frontend::macro::is_valid(*build_vec));
    EXPECT_EQ(build_vec->macro_kind, syntax::MacroDeclKind::declarative);
    EXPECT_TRUE(build_vec->declarative_surface);
    EXPECT_FALSE(build_vec->user_derive_surface);
    EXPECT_FALSE(build_vec->compile_time_execution_surface);
    EXPECT_EQ(build_vec->query_name, "m27a-aurex-macro-surface:0:0:0:BuildVec");
    EXPECT_EQ(build_vec->blocker_reason, "Aurex declarative macro expansion is parser-blocked in M27a");
    EXPECT_EQ(build_vec->match_clause_count, 1U);
    EXPECT_TRUE(build_vec->body_balanced);
    EXPECT_FALSE(build_vec->expansion_enabled);
    EXPECT_FALSE(build_vec->compile_time_execution_enabled);
    EXPECT_FALSE(build_vec->parser_consumption_enabled);
    EXPECT_FALSE(build_vec->ast_mutated);
    EXPECT_FALSE(build_vec->sema_visible_generated_items);
    EXPECT_FALSE(build_vec->produced_user_generated_code);

    EXPECT_TRUE(frontend::macro::is_valid(*inspect));
    EXPECT_EQ(inspect->macro_kind, syntax::MacroDeclKind::derive);
    EXPECT_FALSE(inspect->declarative_surface);
    EXPECT_TRUE(inspect->user_derive_surface);
    EXPECT_FALSE(inspect->compile_time_execution_surface);
    EXPECT_EQ(inspect->query_name, "m27a-aurex-macro-surface:0:0:1:Inspect");
    EXPECT_EQ(inspect->blocker_reason, "Aurex user derive macro expansion is admission-only in M27b");

    EXPECT_TRUE(frontend::macro::is_valid(*token_build));
    EXPECT_EQ(token_build->macro_kind, syntax::MacroDeclKind::compile_time);
    EXPECT_FALSE(token_build->declarative_surface);
    EXPECT_FALSE(token_build->user_derive_surface);
    EXPECT_TRUE(token_build->compile_time_execution_surface);
    EXPECT_EQ(token_build->query_name, "m27a-aurex-macro-surface:0:0:2:TokenBuild");
    EXPECT_EQ(token_build->blocker_reason, "Aurex compile-time macro execution is admission-only in M27c");

    const std::string summary = frontend::macro::summarize_early_item_expansion(result);
    expect_contains(summary, "aurex_macro_surface_source_items=3");
    expect_contains(summary, "aurex_macro_surface_admissions=3");
    expect_contains(summary, "aurex_macro_declarative_surfaces=1");
    expect_contains(summary, "aurex_macro_user_derive_surfaces=1");
    expect_contains(summary, "aurex_macro_compile_time_surfaces=1");
    expect_contains(summary, "aurex_macro_surface_expansion_enabled=0");
    expect_contains(summary, "aurex_macro_surface_compile_time_execution_enabled=0");
    expect_contains(summary, "aurex_macro_surface_parser_consumable=0");

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "aurex_macro_surface_admission_gate #0");
    expect_contains(dump, "kind=declarative");
    expect_contains(dump, "kind=derive");
    expect_contains(dump, "kind=compile_time");
    expect_contains(dump, "body_balanced=yes");
    expect_contains(dump, "expansion_enabled=no");
    expect_contains(dump, "compile_time_execution_enabled=no");
    expect_contains(dump, "parser_consumption_enabled=no");
    expect_contains(dump, "user_generated_code=no");
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsAurexMacroSurfaceDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n"
        "macro derive Inspect {\n"
        "  match item(target) -> { target }\n"
        "}\n"
        "macro const TokenBuild {\n"
        "  match tokens(input) -> { input }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.aurex_macro_surface_admission_gates.size(), 3U);

    const frontend::macro::EarlyItemExpansionResult missing_gate =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.clear();
        });
    EXPECT_EQ(missing_gate.summary.aurex_macro_surface_source_item_count, 3U);
    EXPECT_EQ(missing_gate.summary.aurex_macro_surface_admission_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_gate));

    const frontend::macro::EarlyItemExpansionResult expansion_enabled =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().expansion_enabled = true;
        });
    EXPECT_EQ(expansion_enabled.summary.aurex_macro_surface_expansion_enabled_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(expansion_enabled));

    const frontend::macro::EarlyItemExpansionResult compile_time_execution_enabled =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.back().compile_time_execution_enabled = true;
        });
    EXPECT_EQ(compile_time_execution_enabled.summary
                  .aurex_macro_surface_compile_time_execution_enabled_count,
        1U);
    EXPECT_FALSE(frontend::macro::is_valid(compile_time_execution_enabled));

    const frontend::macro::EarlyItemExpansionResult parser_consumable =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().parser_consumption_enabled = true;
        });
    EXPECT_EQ(parser_consumable.summary.aurex_macro_surface_parser_consumable_count, 1U);
    EXPECT_EQ(parser_consumable.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_consumable));

    const frontend::macro::EarlyItemExpansionResult body_unbalanced =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().body_balanced = false;
        });
    EXPECT_EQ(body_unbalanced.summary.aurex_macro_surface_body_balanced_count, 2U);
    EXPECT_FALSE(frontend::macro::is_valid(body_unbalanced));

    const frontend::macro::EarlyItemExpansionResult user_code =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().produced_user_generated_code = true;
        });
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));

    const frontend::macro::EarlyItemExpansionResult wrong_surface =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().user_derive_surface = true;
        });
    EXPECT_EQ(wrong_surface.summary.aurex_macro_user_derive_surface_count, 2U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_surface));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksAurexMacroSurfaceAdmission)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.aurex_macro_surface_admission_gates.size(), 1U);

    const auto expect_fingerprint_drift =
        [&baseline](const frontend::macro::EarlyItemExpansionResult& result) {
            EXPECT_NE(result.fingerprint, baseline.fingerprint);
            EXPECT_FALSE(frontend::macro::is_valid(result));
        };

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().admission_identity =
                query::stable_fingerprint("different aurex macro surface admission identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().body_fingerprint =
                query::stable_fingerprint("different aurex macro body fingerprint");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().match_clause_count = 2U;
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_surface_admission_gates.front().query_name =
                "m27a-aurex-macro-surface:wrong";
        }));
}

TEST(CoreUnit, EarlyItemExpansionCollectsAurexTypedMatcherAndDefinitionSiteHygieneGates)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n"
        "macro derive Inspect {\n"
        "  match item(target) -> { target }\n"
        "}\n"
        "macro const TokenBuild {\n"
        "  match tokens(input) -> { input }\n"
        "}\n"
        "macro Weird {\n"
        "  match unknown(input) -> { input }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult result = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(result));
    ASSERT_EQ(result.aurex_macro_surface_admission_gates.size(), 4U);
    ASSERT_EQ(result.aurex_macro_definition_site_hygiene_gates.size(), 4U);
    ASSERT_EQ(result.aurex_macro_typed_matcher_admission_gates.size(), 4U);
    EXPECT_EQ(result.summary.aurex_macro_definition_site_hygiene_gate_count, 4U);
    EXPECT_EQ(result.summary.aurex_macro_definition_site_scope_available_count, 4U);
    EXPECT_EQ(result.summary.aurex_macro_fresh_name_scope_reserved_count, 4U);
    EXPECT_EQ(result.summary.aurex_macro_diagnostic_anchor_available_count, 8U);
    EXPECT_EQ(result.summary.aurex_macro_hygiene_resolution_enabled_count, 0U);
    EXPECT_EQ(result.summary.aurex_macro_typed_matcher_admission_gate_count, 4U);
    EXPECT_EQ(result.summary.aurex_macro_typed_matcher_recognized_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_expr_list_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_item_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_token_stream_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_unknown_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_typed_matcher_execution_enabled_count, 0U);
    EXPECT_EQ(result.summary.parse_ready_token_buffer_count, 0U);
    EXPECT_EQ(result.summary.ast_mutation_count, 0U);
    EXPECT_EQ(result.summary.user_generated_code_count, 0U);
    EXPECT_EQ(result.summary.standard_library_required_count, 0U);
    EXPECT_EQ(result.summary.runtime_required_count, 0U);
    EXPECT_EQ(result.summary.external_process_required_count, 0U);

    const frontend::macro::AurexMacroDefinitionSiteHygieneAdmissionGate* const build_hygiene =
        aurex_macro_hygiene_gate_by_name(result, "BuildVec");
    ASSERT_NE(build_hygiene, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*build_hygiene));
    EXPECT_EQ(build_hygiene->query_name, "m27b-aurex-macro-definition-site-hygiene:0:0:0:BuildVec");
    EXPECT_TRUE(build_hygiene->definition_site_scope_available);
    EXPECT_TRUE(build_hygiene->fresh_name_scope_reserved);
    EXPECT_TRUE(build_hygiene->diagnostic_anchor_available);
    EXPECT_FALSE(build_hygiene->hygiene_resolution_enabled);
    EXPECT_FALSE(build_hygiene->declared_names_visible);
    EXPECT_FALSE(build_hygiene->produced_user_generated_code);

    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const expr_matcher =
        aurex_macro_matcher_gate_by_name_and_index(result, "BuildVec", 0U);
    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const item_matcher =
        aurex_macro_matcher_gate_by_name_and_index(result, "Inspect", 0U);
    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const token_matcher =
        aurex_macro_matcher_gate_by_name_and_index(result, "TokenBuild", 0U);
    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const unknown_matcher =
        aurex_macro_matcher_gate_by_name_and_index(result, "Weird", 0U);
    ASSERT_NE(expr_matcher, nullptr);
    ASSERT_NE(item_matcher, nullptr);
    ASSERT_NE(token_matcher, nullptr);
    ASSERT_NE(unknown_matcher, nullptr);

    EXPECT_TRUE(frontend::macro::is_valid(*expr_matcher));
    EXPECT_EQ(expr_matcher->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::expr_list);
    EXPECT_EQ(expr_matcher->matcher_head, "expr_list");
    EXPECT_EQ(expr_matcher->binding_name, "xs");
    EXPECT_TRUE(expr_matcher->matcher_shape_recognized);
    EXPECT_TRUE(expr_matcher->expr_list_matcher);
    EXPECT_FALSE(expr_matcher->matcher_execution_enabled);
    EXPECT_FALSE(expr_matcher->expansion_enabled);
    EXPECT_FALSE(expr_matcher->parser_consumption_enabled);
    EXPECT_EQ(expr_matcher->query_name, "m27b-aurex-macro-typed-matcher:0:0:0:0:BuildVec");

    EXPECT_TRUE(frontend::macro::is_valid(*item_matcher));
    EXPECT_EQ(item_matcher->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::item);
    EXPECT_EQ(item_matcher->binding_name, "target");
    EXPECT_TRUE(item_matcher->item_matcher);

    EXPECT_TRUE(frontend::macro::is_valid(*token_matcher));
    EXPECT_EQ(token_matcher->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::tokens);
    EXPECT_EQ(token_matcher->binding_name, "input");
    EXPECT_TRUE(token_matcher->token_stream_matcher);

    EXPECT_TRUE(frontend::macro::is_valid(*unknown_matcher));
    EXPECT_EQ(unknown_matcher->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::unknown);
    EXPECT_EQ(unknown_matcher->matcher_head, "unknown");
    EXPECT_TRUE(unknown_matcher->unknown_matcher);
    EXPECT_FALSE(unknown_matcher->matcher_shape_recognized);

    const std::string summary = frontend::macro::summarize_early_item_expansion(result);
    expect_contains(summary, "aurex_macro_definition_site_hygiene_gates=4");
    expect_contains(summary, "aurex_macro_typed_matcher_admissions=4");
    expect_contains(summary, "aurex_macro_typed_matchers_recognized=3");
    expect_contains(summary, "aurex_macro_expr_list_matchers=1");
    expect_contains(summary, "aurex_macro_unknown_matchers=1");
    expect_contains(summary, "aurex_macro_typed_matcher_execution_enabled=0");

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "aurex_macro_definition_site_hygiene_gate #0");
    expect_contains(dump, "definition_site_scope_available=yes");
    expect_contains(dump, "fresh_name_scope_reserved=yes");
    expect_contains(dump, "aurex_macro_typed_matcher_admission_gate #0");
    expect_contains(dump, "matcher_kind=expr_list");
    expect_contains(dump, "matcher_kind=item");
    expect_contains(dump, "matcher_kind=tokens");
    expect_contains(dump, "matcher_kind=unknown");
    expect_contains(dump, "matcher_execution_enabled=no");
}

TEST(CoreUnit, EarlyItemExpansionIndexesMultipleAndMalformedAurexTypedMatchers)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro Many {\n"
        "  match expr_list(xs) -> { xs }\n"
        "  match tokens(raw) -> { raw }\n"
        "  match item(target) -> { target }\n"
        "}\n"
        "macro Broken {\n"
        "  match expr_list xs -> { xs }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult result = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(result));
    ASSERT_EQ(result.aurex_macro_surface_admission_gates.size(), 2U);
    ASSERT_EQ(result.aurex_macro_definition_site_hygiene_gates.size(), 2U);
    ASSERT_EQ(result.aurex_macro_typed_matcher_admission_gates.size(), 4U);
    EXPECT_EQ(result.summary.aurex_macro_typed_matcher_admission_gate_count, 4U);
    EXPECT_EQ(result.summary.aurex_macro_typed_matcher_recognized_count, 3U);
    EXPECT_EQ(result.summary.aurex_macro_expr_list_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_item_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_token_stream_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_unknown_matcher_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_typed_matcher_execution_enabled_count, 0U);
    EXPECT_EQ(result.summary.aurex_macro_surface_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.parse_ready_token_buffer_count, 0U);
    EXPECT_EQ(result.summary.ast_mutation_count, 0U);
    EXPECT_EQ(result.summary.user_generated_code_count, 0U);

    const frontend::macro::AurexMacroSurfaceAdmissionGate* const many_surface =
        aurex_macro_surface_gate_by_name(result, "Many");
    const frontend::macro::AurexMacroSurfaceAdmissionGate* const broken_surface =
        aurex_macro_surface_gate_by_name(result, "Broken");
    ASSERT_NE(many_surface, nullptr);
    ASSERT_NE(broken_surface, nullptr);
    EXPECT_EQ(many_surface->match_clause_count, 3U);
    EXPECT_EQ(broken_surface->match_clause_count, 1U);

    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const many_expr =
        aurex_macro_matcher_gate_by_name_and_index(result, "Many", 0U);
    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const many_tokens =
        aurex_macro_matcher_gate_by_name_and_index(result, "Many", 1U);
    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const many_item =
        aurex_macro_matcher_gate_by_name_and_index(result, "Many", 2U);
    const frontend::macro::AurexMacroTypedMatcherAdmissionGate* const broken =
        aurex_macro_matcher_gate_by_name_and_index(result, "Broken", 0U);
    ASSERT_NE(many_expr, nullptr);
    ASSERT_NE(many_tokens, nullptr);
    ASSERT_NE(many_item, nullptr);
    ASSERT_NE(broken, nullptr);

    EXPECT_EQ(many_expr->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::expr_list);
    EXPECT_EQ(many_expr->binding_name, "xs");
    EXPECT_TRUE(many_expr->matcher_shape_recognized);
    EXPECT_EQ(many_expr->query_name, "m27b-aurex-macro-typed-matcher:0:0:0:0:Many");
    EXPECT_EQ(many_tokens->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::tokens);
    EXPECT_EQ(many_tokens->binding_name, "raw");
    EXPECT_EQ(many_tokens->query_name, "m27b-aurex-macro-typed-matcher:0:0:0:1:Many");
    EXPECT_EQ(many_item->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::item);
    EXPECT_EQ(many_item->binding_name, "target");
    EXPECT_EQ(many_item->query_name, "m27b-aurex-macro-typed-matcher:0:0:0:2:Many");

    EXPECT_EQ(broken->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::unknown);
    EXPECT_EQ(broken->matcher_head, "expr_list");
    EXPECT_TRUE(broken->binding_name.empty());
    EXPECT_TRUE(broken->unknown_matcher);
    EXPECT_FALSE(broken->matcher_shape_recognized);
    EXPECT_FALSE(broken->parser_consumption_enabled);
    EXPECT_FALSE(broken->ast_mutated);
    EXPECT_FALSE(broken->produced_user_generated_code);
}

TEST(CoreUnit, EarlyItemExpansionCollectsAurexMacroCallSitesAndUserDeriveSchemas)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n"
        "macro derive Inspect {\n"
        "  match item(target) -> { target }\n"
        "}\n"
        "macro call BuildVec {\n"
        "  1, 2, nested(3)\n"
        "}\n"
        "macro call Missing {\n"
        "  raw(tokens)\n"
        "}\n"
        "#[derive(Inspect)]\n"
        "struct Config { threads: i32; enabled: bool; }\n"
        "#[derive(Inspect)]\n"
        "enum Mode { fast, slow(i32), tuple(i32, bool) }\n";

    const frontend::macro::EarlyItemExpansionResult result = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(result));
    ASSERT_EQ(result.aurex_macro_call_site_admission_gates.size(), 2U);
    ASSERT_EQ(result.aurex_macro_matcher_to_call_binding_gates.size(), 1U);
    ASSERT_EQ(result.aurex_user_derive_target_schema_gates.size(), 2U);

    EXPECT_EQ(result.summary.aurex_macro_call_site_admission_gate_count, 2U);
    EXPECT_EQ(result.summary.aurex_macro_call_site_source_item_count, 2U);
    EXPECT_EQ(result.summary.aurex_macro_call_site_target_declared_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_call_site_visible_count, 2U);
    EXPECT_EQ(result.summary.aurex_macro_call_site_query_reusable_count, 2U);
    EXPECT_EQ(result.summary.aurex_macro_call_site_balanced_count, 2U);
    EXPECT_EQ(result.summary.aurex_macro_call_site_expansion_enabled_count, 0U);
    EXPECT_EQ(result.summary.aurex_macro_matcher_to_call_binding_gate_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_matcher_to_call_binding_admitted_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_matcher_to_call_binding_visible_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_matcher_to_call_binding_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_gate_count, 2U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_source_derive_count, 2U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_struct_count, 1U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_enum_count, 1U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_unsupported_count, 0U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_field_count, 2U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_enum_case_count, 3U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_enum_payload_count, 3U);
    EXPECT_EQ(result.summary.generated_source_text_count, 0U);
    EXPECT_EQ(result.summary.parse_ready_token_buffer_count, 0U);
    EXPECT_EQ(result.summary.ast_mutation_count, 0U);
    EXPECT_EQ(result.summary.sema_visible_generated_part_count, 0U);
    EXPECT_EQ(result.summary.user_generated_code_count, 0U);
    EXPECT_EQ(result.summary.standard_library_required_count, 0U);
    EXPECT_EQ(result.summary.runtime_required_count, 0U);
    EXPECT_EQ(result.summary.external_process_required_count, 0U);

    const frontend::macro::AurexMacroCallSiteAdmissionGate* const build_call =
        aurex_macro_call_site_gate_by_name(result, "BuildVec");
    const frontend::macro::AurexMacroCallSiteAdmissionGate* const missing_call =
        aurex_macro_call_site_gate_by_name(result, "Missing");
    ASSERT_NE(build_call, nullptr);
    ASSERT_NE(missing_call, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*build_call));
    EXPECT_TRUE(build_call->target_surface_declared);
    EXPECT_TRUE(build_call->token_tree_balanced);
    EXPECT_EQ(build_call->blocker_reason, "Aurex macro call-site expansion is admission-only in M27c");
    EXPECT_EQ(build_call->query_name, "m27c-aurex-macro-call-site:0:0:2:BuildVec");
    EXPECT_FALSE(build_call->expansion_enabled);
    EXPECT_FALSE(build_call->parser_consumption_enabled);
    EXPECT_FALSE(build_call->ast_mutated);
    EXPECT_FALSE(build_call->produced_user_generated_code);

    EXPECT_TRUE(frontend::macro::is_valid(*missing_call));
    EXPECT_FALSE(missing_call->target_surface_declared);
    EXPECT_EQ(missing_call->blocker_reason,
        "Aurex macro call-site target is not declared and remains blocked in M27c");

    const frontend::macro::AurexMacroMatcherToCallBindingAdmissionGate* const binding =
        aurex_macro_binding_gate_by_call_name(result, "BuildVec");
    ASSERT_NE(binding, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*binding));
    EXPECT_TRUE(binding->target_surface_declared);
    EXPECT_TRUE(binding->matcher_shape_recognized);
    EXPECT_TRUE(binding->binding_admitted);
    EXPECT_EQ(binding->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::expr_list);
    EXPECT_EQ(binding->matcher_head, "expr_list");
    EXPECT_EQ(binding->binding_name, "xs");
    EXPECT_EQ(binding->blocker_reason, "Aurex matcher-to-call binding is admission-only in M27c");
    EXPECT_FALSE(binding->matcher_execution_enabled);
    EXPECT_FALSE(binding->expansion_enabled);
    EXPECT_FALSE(binding->parser_consumption_enabled);
    EXPECT_FALSE(binding->ast_mutated);
    EXPECT_FALSE(binding->produced_user_generated_code);

    const frontend::macro::AurexUserDeriveTargetSchemaAdmissionGate* const config_schema =
        aurex_user_derive_schema_gate_by_target(result, "Config");
    const frontend::macro::AurexUserDeriveTargetSchemaAdmissionGate* const mode_schema =
        aurex_user_derive_schema_gate_by_target(result, "Mode");
    ASSERT_NE(config_schema, nullptr);
    ASSERT_NE(mode_schema, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*config_schema));
    EXPECT_EQ(config_schema->target_kind, frontend::macro::AurexUserDeriveTargetKind::struct_);
    EXPECT_EQ(config_schema->derive_name, "Inspect");
    EXPECT_EQ(config_schema->field_count, 2U);
    EXPECT_EQ(config_schema->enum_case_count, 0U);
    EXPECT_EQ(config_schema->blocker_reason,
        "Aurex user derive target schema is admission-only in M27c");
    EXPECT_FALSE(config_schema->expansion_enabled);
    EXPECT_FALSE(config_schema->parser_consumption_enabled);
    EXPECT_FALSE(config_schema->ast_mutated);
    EXPECT_FALSE(config_schema->produced_user_generated_code);

    EXPECT_TRUE(frontend::macro::is_valid(*mode_schema));
    EXPECT_EQ(mode_schema->target_kind, frontend::macro::AurexUserDeriveTargetKind::enum_);
    EXPECT_EQ(mode_schema->enum_case_count, 3U);
    EXPECT_EQ(mode_schema->enum_payload_count, 3U);
    EXPECT_FALSE(mode_schema->sema_visible_generated_items);

    const std::string summary = frontend::macro::summarize_early_item_expansion(result);
    expect_contains(summary, "aurex_macro_call_site_admissions=2");
    expect_contains(summary, "aurex_macro_call_site_source_items=2");
    expect_contains(summary, "aurex_macro_matcher_to_call_bindings_admitted=1");
    expect_contains(summary, "aurex_user_derive_target_schema_source_derives=2");
    expect_contains(summary, "aurex_user_derive_target_schemas=2");
    expect_contains(summary, "aurex_user_derive_target_schema_fields=2");
    expect_contains(summary, "parse_ready_token_buffers=0");
    expect_contains(summary, "user_generated_code=0");

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "aurex_macro_call_site_admission_gate #0");
    expect_contains(dump, "target_surface_declared=yes");
    expect_contains(dump, "target_surface_declared=no");
    expect_contains(dump, "aurex_macro_matcher_to_call_binding_gate #0");
    expect_contains(dump, "binding_admitted=yes");
    expect_contains(dump, "aurex_user_derive_target_schema_gate #0");
    expect_contains(dump, "target_kind=struct");
    expect_contains(dump, "target_kind=enum");
    expect_contains(dump, "parser_consumption_enabled=no");
    expect_contains(dump, "user_generated_code=no");
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsAurexMacroCallSiteAndUserDeriveDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n"
        "macro derive Inspect {\n"
        "  match item(target) -> { target }\n"
        "}\n"
        "macro call BuildVec {\n"
        "  1, 2, nested(3)\n"
        "}\n"
        "#[derive(Inspect)]\n"
        "struct Config { threads: i32; enabled: bool; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.aurex_macro_call_site_admission_gates.size(), 1U);
    ASSERT_EQ(baseline.aurex_macro_matcher_to_call_binding_gates.size(), 1U);
    ASSERT_EQ(baseline.aurex_user_derive_target_schema_gates.size(), 1U);
    EXPECT_EQ(baseline.summary.aurex_macro_call_site_source_item_count, 1U);
    EXPECT_EQ(baseline.summary.aurex_user_derive_target_schema_source_derive_count, 1U);

    const auto expect_invalid = [](const frontend::macro::EarlyItemExpansionResult& result) {
        EXPECT_FALSE(frontend::macro::is_valid(result));
    };

    const frontend::macro::EarlyItemExpansionResult missing_call_site =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_call_site_admission_gates.clear();
        });
    EXPECT_EQ(missing_call_site.summary.aurex_macro_call_site_admission_gate_count, 0U);
    expect_invalid(missing_call_site);

    const frontend::macro::EarlyItemExpansionResult executable_call_site =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_call_site_admission_gates.front().expansion_enabled = true;
        });
    EXPECT_EQ(executable_call_site.summary.aurex_macro_call_site_expansion_enabled_count, 1U);
    expect_invalid(executable_call_site);

    const frontend::macro::EarlyItemExpansionResult parser_consumable_call_site =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_call_site_admission_gates.front().parser_consumption_enabled = true;
        });
    EXPECT_EQ(parser_consumable_call_site.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(parser_consumable_call_site);

    const frontend::macro::EarlyItemExpansionResult missing_binding =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_matcher_to_call_binding_gates.clear();
        });
    EXPECT_EQ(missing_binding.summary.aurex_macro_matcher_to_call_binding_gate_count, 0U);
    expect_invalid(missing_binding);

    const frontend::macro::EarlyItemExpansionResult binding_executed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_matcher_to_call_binding_gates.front().matcher_execution_enabled = true;
        });
    EXPECT_EQ(binding_executed.summary.aurex_macro_typed_matcher_execution_enabled_count, 1U);
    expect_invalid(binding_executed);

    const frontend::macro::EarlyItemExpansionResult schema_parser_consumable =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_user_derive_target_schema_gates.front().parser_consumption_enabled = true;
        });
    EXPECT_EQ(schema_parser_consumable.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(schema_parser_consumable);

    const frontend::macro::EarlyItemExpansionResult missing_schema =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_user_derive_target_schema_gates.clear();
        });
    EXPECT_EQ(missing_schema.summary.aurex_user_derive_target_schema_gate_count, 0U);
    expect_invalid(missing_schema);
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsAurexTypedMatcherAndHygieneDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.aurex_macro_definition_site_hygiene_gates.size(), 1U);
    ASSERT_EQ(baseline.aurex_macro_typed_matcher_admission_gates.size(), 1U);

    const frontend::macro::EarlyItemExpansionResult missing_hygiene =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_definition_site_hygiene_gates.clear();
        });
    EXPECT_EQ(missing_hygiene.summary.aurex_macro_definition_site_hygiene_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_hygiene));

    const frontend::macro::EarlyItemExpansionResult hygiene_enabled =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_definition_site_hygiene_gates.front().hygiene_resolution_enabled = true;
        });
    EXPECT_EQ(hygiene_enabled.summary.aurex_macro_hygiene_resolution_enabled_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(hygiene_enabled));

    const frontend::macro::EarlyItemExpansionResult missing_matcher =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.clear();
        });
    EXPECT_EQ(missing_matcher.summary.aurex_macro_typed_matcher_admission_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_matcher));

    const frontend::macro::EarlyItemExpansionResult matcher_executed =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().matcher_execution_enabled = true;
        });
    EXPECT_EQ(matcher_executed.summary.aurex_macro_typed_matcher_execution_enabled_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(matcher_executed));

    const frontend::macro::EarlyItemExpansionResult matcher_consumable =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().parser_consumption_enabled = true;
        });
    EXPECT_EQ(matcher_consumable.summary.aurex_macro_surface_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(matcher_consumable));

    const frontend::macro::EarlyItemExpansionResult wrong_kind_flags =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().item_matcher = true;
        });
    EXPECT_EQ(wrong_kind_flags.summary.aurex_macro_item_matcher_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_kind_flags));

    const frontend::macro::EarlyItemExpansionResult wrong_surface_identity =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().surface_admission_identity =
                query::stable_fingerprint("different m27b surface identity");
        });
    EXPECT_FALSE(frontend::macro::is_valid(wrong_surface_identity));

    const frontend::macro::EarlyItemExpansionResult wrong_hygiene_identity =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().definition_site_hygiene_identity =
                query::stable_fingerprint("different m27b definition-site hygiene identity");
        });
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_identity));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksAurexTypedMatcherAndHygieneAdmission)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "macro BuildVec {\n"
        "  match expr_list(xs) -> { xs }\n"
        "}\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.aurex_macro_definition_site_hygiene_gates.size(), 1U);
    ASSERT_EQ(baseline.aurex_macro_typed_matcher_admission_gates.size(), 1U);

    const auto expect_fingerprint_drift =
        [&baseline](const frontend::macro::EarlyItemExpansionResult& result) {
            EXPECT_NE(result.fingerprint, baseline.fingerprint);
            EXPECT_FALSE(frontend::macro::is_valid(result));
        };

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_definition_site_hygiene_gates.front().hygiene_identity =
                query::stable_fingerprint("different m27b hygiene identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().matcher_identity =
                query::stable_fingerprint("different m27b matcher identity");
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().binding_name = "changed";
        }));

    expect_fingerprint_drift(
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_typed_matcher_admission_gates.front().query_name =
                "m27b-aurex-macro-typed-matcher:wrong";
        }));
}

TEST(CoreUnit, EarlyItemExpansionRejectsInvalidInputs)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    syntax::AstModule module = parse_success(source);
    assign_single_module_ownership(module);
    std::vector<std::vector<query::ModulePartKey>> part_keys = single_part_key_table();

    query::MacroExpansionPlan invalid_plan = query::m21c_macro_expansion_plan_baseline();
    invalid_plan.name = "wrong plan";
    invalid_plan.fingerprint = query::macro_expansion_plan_fingerprint(invalid_plan);
    auto invalid_plan_result =
        frontend::macro::expand_early_item_macros_noop(module, part_keys, invalid_plan);
    ASSERT_FALSE(invalid_plan_result);
    EXPECT_EQ(invalid_plan_result.error().code, base::ErrorCode::internal_error);
    expect_contains(invalid_plan_result.error().message, "valid M21c macro expansion plan");

    syntax::AstModule missing_module_owner = module;
    missing_module_owner.item_modules.pop_back();
    auto missing_module_result =
        frontend::macro::expand_early_item_macros_noop(missing_module_owner, part_keys);
    ASSERT_FALSE(missing_module_result);
    EXPECT_EQ(missing_module_result.error().code, base::ErrorCode::internal_error);
    expect_contains(missing_module_result.error().message, "one module owner per item");

    syntax::AstModule missing_part_owner = module;
    missing_part_owner.item_part_indices.pop_back();
    auto missing_part_result =
        frontend::macro::expand_early_item_macros_noop(missing_part_owner, part_keys);
    ASSERT_FALSE(missing_part_result);
    EXPECT_EQ(missing_part_result.error().code, base::ErrorCode::internal_error);
    expect_contains(missing_part_result.error().message, "one module part index per item");

    std::vector<std::vector<query::ModulePartKey>> missing_key = {{}};
    auto missing_key_result =
        frontend::macro::expand_early_item_macros_noop(module, missing_key);
    ASSERT_FALSE(missing_key_result);
    EXPECT_EQ(missing_key_result.error().code, base::ErrorCode::internal_error);
    expect_contains(missing_key_result.error().message, "missing module part key");

    std::vector<std::vector<query::ModulePartKey>> invalid_key = {{query::ModulePartKey{}}};
    auto invalid_key_result =
        frontend::macro::expand_early_item_macros_noop(module, invalid_key);
    ASSERT_FALSE(invalid_key_result);
    EXPECT_EQ(invalid_key_result.error().code, base::ErrorCode::internal_error);
    expect_contains(invalid_key_result.error().message, "missing module part key");
}

} // namespace aurex::test
