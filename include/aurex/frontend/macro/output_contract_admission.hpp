#pragma once

#include <aurex/frontend/macro/output_contract_summary.hpp>
#include <aurex/frontend/macro/output_contract_types.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <iosfwd>
#include <span>
#include <string_view>
#include <vector>

namespace aurex::frontend::macro {

struct AurexMacroCallSiteAdmissionGate;
struct AurexMacroMatcherToCallBindingAdmissionGate;
struct AurexMacroSurfaceAdmissionGate;
struct AurexUserDeriveTargetSchemaAdmissionGate;

struct AurexMacroOutputContractAdmissionSet {
    std::vector<AurexMacroOutputContractAdmissionGate> output_contracts;
    std::vector<AurexMacroOutputDeclaredNamePolicyAdmissionGate> declared_name_policies;
    std::vector<AurexMacroOutputDiagnosticProjectionAdmissionGate> diagnostic_projections;
};

[[nodiscard]] std::string_view aurex_macro_output_contract_origin_kind_name(
    AurexMacroOutputContractOriginKind kind) noexcept;

[[nodiscard]] bool is_valid(const AurexMacroOutputContractAdmissionGate& gate) noexcept;
[[nodiscard]] bool is_valid(const AurexMacroOutputDeclaredNamePolicyAdmissionGate& gate) noexcept;
[[nodiscard]] bool is_valid(const AurexMacroOutputDiagnosticProjectionAdmissionGate& gate) noexcept;

[[nodiscard]] AurexMacroOutputContractAdmissionSet collect_aurex_macro_output_contract_admissions(
    std::span<const AurexMacroSurfaceAdmissionGate> surfaces,
    std::span<const AurexMacroMatcherToCallBindingAdmissionGate> bindings,
    std::span<const AurexUserDeriveTargetSchemaAdmissionGate> schemas);

[[nodiscard]] bool aurex_macro_output_contracts_match_inputs(
    std::span<const AurexMacroMatcherToCallBindingAdmissionGate> bindings,
    std::span<const AurexUserDeriveTargetSchemaAdmissionGate> schemas,
    std::span<const AurexMacroOutputContractAdmissionGate> contracts,
    std::span<const AurexMacroOutputDeclaredNamePolicyAdmissionGate> declared_name_policies,
    std::span<const AurexMacroOutputDiagnosticProjectionAdmissionGate> diagnostic_projections) noexcept;

[[nodiscard]] bool aurex_macro_output_contract_admissions_are_valid(
    std::span<const AurexMacroMatcherToCallBindingAdmissionGate> bindings,
    std::span<const AurexUserDeriveTargetSchemaAdmissionGate> schemas,
    std::span<const AurexMacroOutputContractAdmissionGate> contracts,
    std::span<const AurexMacroOutputDeclaredNamePolicyAdmissionGate> declared_name_policies,
    std::span<const AurexMacroOutputDiagnosticProjectionAdmissionGate> diagnostic_projections) noexcept;

[[nodiscard]] AurexMacroOutputContractSummary summarize_aurex_macro_output_contract_admissions(
    std::span<const AurexMacroOutputContractAdmissionGate> contracts,
    std::span<const AurexMacroOutputDeclaredNamePolicyAdmissionGate> declared_name_policies,
    std::span<const AurexMacroOutputDiagnosticProjectionAdmissionGate> diagnostic_projections) noexcept;

void mix_aurex_macro_output_contract_summary(
    query::StableHashBuilder& builder,
    const AurexMacroOutputContractSummary& summary) noexcept;

void mix_aurex_macro_output_contract_admissions(
    query::StableHashBuilder& builder,
    std::span<const AurexMacroOutputContractAdmissionGate> contracts,
    std::span<const AurexMacroOutputDeclaredNamePolicyAdmissionGate> declared_name_policies,
    std::span<const AurexMacroOutputDiagnosticProjectionAdmissionGate> diagnostic_projections) noexcept;

void append_aurex_macro_output_contract_summary(
    std::ostream& stream,
    const AurexMacroOutputContractSummary& summary);

void dump_aurex_macro_output_contract_admissions(
    std::ostream& stream,
    std::span<const AurexMacroOutputContractAdmissionGate> contracts,
    std::span<const AurexMacroOutputDeclaredNamePolicyAdmissionGate> declared_name_policies,
    std::span<const AurexMacroOutputDiagnosticProjectionAdmissionGate> diagnostic_projections);

} // namespace aurex::frontend::macro
