#pragma once

#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_common_support.hpp>

#include <algorithm>
#include <string_view>
#include <vector>

namespace aurex::test::early_item_expansion_support {

[[nodiscard]] inline const frontend::macro::ExpansionHygieneStub* hygiene_stub_for_input(
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

[[nodiscard]] inline const frontend::macro::ExpansionTraceStub* trace_stub_for_input(
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

[[nodiscard]] inline const frontend::macro::GeneratedItemDeclarationStub* generated_item_declaration_for_input(
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

[[nodiscard]] inline const frontend::macro::DeclaredGeneratedNameStub* declared_generated_name_for_input(
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

[[nodiscard]] inline const frontend::macro::TokenMaterializationAdmissionStub* token_admission_for_input(
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

[[nodiscard]] inline const frontend::macro::GeneratedTokenBufferStub* token_buffer_for_input(
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

[[nodiscard]] inline const frontend::macro::GeneratedTokenParserAdmissionGateStub* parser_admission_gate_for_input(
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

[[nodiscard]] inline const frontend::macro::ParserAdmissionDiagnosticProjectionStub* parser_admission_diagnostic_for_input(
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

[[nodiscard]] inline const frontend::macro::ParserAdmissionDiagnosticReportEntry*
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

[[nodiscard]] inline const frontend::macro::ParserAdmissionDiagnosticReport*
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

[[nodiscard]] inline const frontend::macro::GeneratedTokenParserReadinessPreflightEntry*
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

[[nodiscard]] inline const frontend::macro::GeneratedTokenParserConsumptionContractGate*
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

[[nodiscard]] inline std::vector<const frontend::macro::GeneratedTokenRecord*> token_records_for_input(
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

[[nodiscard]] inline base::usize first_record_index_for_attribute(
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

} // namespace aurex::test::early_item_expansion_support
