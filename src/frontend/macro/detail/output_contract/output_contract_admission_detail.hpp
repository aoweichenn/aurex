#pragma once

#include <aurex/frontend/macro/output_contract_types.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <string>
#include <string_view>

namespace aurex::frontend::macro::detail {

inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_CONTRACT_MARKER =
    "frontend.macro.m27d.output_contract_admission_gate.v1";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_TOKEN_BUFFER_MARKER =
    "frontend.macro.m27d.output_token_buffer.v1";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_POLICY_MARKER =
    "frontend.macro.m27d.output_declared_name_policy_admission_gate.v1";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_SET_MARKER =
    "frontend.macro.m27d.output_declared_name_set.v1";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_DIAGNOSTIC_PROJECTION_MARKER =
    "frontend.macro.m27d.output_diagnostic_projection_admission_gate.v1";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_SOURCE_MAP_MARKER =
    "frontend.macro.m27d.output_source_map.v1";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_HYGIENE_MARK_MARKER =
    "frontend.macro.m27d.output_hygiene_mark.v1";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_CONTRACT_POLICY =
    "aurex_macro_output_contract_admission_v1";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_POLICY =
    "aurex_macro_output_declared_name_policy_admission_v1";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_DIAGNOSTIC_POLICY =
    "aurex_macro_output_diagnostic_projection_admission_v1";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_CONTRACT_QUERY_PREFIX =
    "m27d-aurex-macro-output-contract:";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_QUERY_PREFIX =
    "m27d-aurex-macro-output-declared-name-policy:";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_DIAGNOSTIC_QUERY_PREFIX =
    "m27d-aurex-macro-output-diagnostic:";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_TOKEN_BUFFER_PREFIX =
    "m27d-output-buffer:";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_NAMESPACE =
    "macro_output_item";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_DIAGNOSTIC_CATEGORY =
    "macro_output_parser_blocked";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_CONTRACT_BLOCKER =
    "Aurex macro output contract is admission-only in M27d";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_PARSE_BLOCKER =
    "Aurex macro output parser consumption remains blocked in M27d";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_BLOCKER =
    "Aurex macro output declared names are hidden from lookup/export/sema in M27d";
inline constexpr std::string_view FRONTEND_MACRO_M27D_OUTPUT_DIAGNOSTIC_BLOCKER =
    "Aurex macro output diagnostics are projected but parser emission remains blocked in M27d";

[[nodiscard]] bool source_range_is_well_formed(const base::SourceRange& range) noexcept;
[[nodiscard]] bool is_nonzero_fingerprint(query::StableFingerprint128 fingerprint) noexcept;

[[nodiscard]] std::string macro_output_contract_query_name(
    syntax::ModuleId module,
    base::u32 part_index,
    syntax::ItemId consumer_item,
    syntax::ItemId macro_item,
    base::u32 output_index,
    std::string_view macro_name);
[[nodiscard]] std::string macro_output_declared_name_policy_query_name(
    syntax::ModuleId module,
    base::u32 part_index,
    syntax::ItemId consumer_item,
    syntax::ItemId macro_item,
    base::u32 output_index,
    std::string_view macro_name);
[[nodiscard]] std::string macro_output_diagnostic_projection_query_name(
    syntax::ModuleId module,
    base::u32 part_index,
    syntax::ItemId consumer_item,
    syntax::ItemId macro_item,
    base::u32 output_index,
    std::string_view macro_name);
[[nodiscard]] std::string macro_output_token_buffer_name(
    syntax::ModuleId module,
    base::u32 part_index,
    syntax::ItemId consumer_item,
    syntax::ItemId macro_item,
    base::u32 output_index,
    std::string_view macro_name);
[[nodiscard]] query::ModulePartKey macro_output_generated_module_part_key(
    query::ModulePartKey source_part,
    syntax::ModuleId module,
    base::u32 part_index,
    syntax::ItemId consumer_item,
    syntax::ItemId macro_item,
    base::u32 output_index,
    std::string_view macro_name);

[[nodiscard]] query::StableFingerprint128 macro_output_source_map_identity(
    syntax::ModuleId module,
    base::u32 part_index,
    syntax::ItemId consumer_item,
    syntax::ItemId macro_item,
    base::u32 output_index,
    query::StableFingerprint128 source_admission_identity,
    const base::SourceRange& source_range) noexcept;
[[nodiscard]] query::StableFingerprint128 macro_output_hygiene_mark_identity(
    syntax::ModuleId module,
    base::u32 part_index,
    syntax::ItemId consumer_item,
    syntax::ItemId macro_item,
    base::u32 output_index,
    query::StableFingerprint128 surface_admission_identity,
    query::StableFingerprint128 source_admission_identity,
    query::StableFingerprint128 matcher_identity,
    query::StableFingerprint128 target_schema_identity) noexcept;
[[nodiscard]] query::StableFingerprint128 macro_output_token_buffer_identity(
    const AurexMacroOutputContractAdmissionGate& gate) noexcept;
[[nodiscard]] query::StableFingerprint128 macro_output_declared_name_set_fingerprint(
    const AurexMacroOutputContractAdmissionGate& gate) noexcept;
[[nodiscard]] query::StableFingerprint128 macro_output_declared_name_policy_identity(
    const AurexMacroOutputDeclaredNamePolicyAdmissionGate& gate) noexcept;
[[nodiscard]] query::StableFingerprint128 macro_output_contract_identity(
    const AurexMacroOutputContractAdmissionGate& gate) noexcept;
[[nodiscard]] query::StableFingerprint128 macro_output_diagnostic_projection_identity(
    const AurexMacroOutputDiagnosticProjectionAdmissionGate& gate) noexcept;

[[nodiscard]] AurexMacroOutputDeclaredNamePolicyAdmissionGate
make_aurex_macro_output_declared_name_policy_gate(
    const AurexMacroOutputContractAdmissionGate& contract);
[[nodiscard]] AurexMacroOutputDiagnosticProjectionAdmissionGate
make_aurex_macro_output_diagnostic_projection_gate(
    const AurexMacroOutputContractAdmissionGate& contract);

void mix_aurex_macro_output_contract_gate(
    query::StableHashBuilder& builder,
    const AurexMacroOutputContractAdmissionGate& gate) noexcept;
void mix_aurex_macro_output_declared_name_policy_gate(
    query::StableHashBuilder& builder,
    const AurexMacroOutputDeclaredNamePolicyAdmissionGate& gate) noexcept;
void mix_aurex_macro_output_diagnostic_projection_gate(
    query::StableHashBuilder& builder,
    const AurexMacroOutputDiagnosticProjectionAdmissionGate& gate) noexcept;

} // namespace aurex::frontend::macro::detail
