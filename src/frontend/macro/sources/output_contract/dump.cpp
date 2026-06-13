#include <aurex/frontend/macro/output_contract_admission.hpp>

#include <ostream>

namespace aurex::frontend::macro {
namespace {

std::string_view yes_no(const bool value) noexcept
{
    static constexpr std::string_view OUTPUT_CONTRACT_DUMP_BOOL_TEXT[] = {
        "no",
        "yes",
    };
    return OUTPUT_CONTRACT_DUMP_BOOL_TEXT[static_cast<base::usize>(value)];
}

} // namespace

std::string_view aurex_macro_output_contract_origin_kind_name(
    const AurexMacroOutputContractOriginKind kind) noexcept
{
    switch (kind) {
        case AurexMacroOutputContractOriginKind::matcher_to_call_binding:
            return "matcher_to_call_binding";
        case AurexMacroOutputContractOriginKind::user_derive_target_schema:
            return "user_derive_target_schema";
    }
    return "matcher_to_call_binding";
}

void append_aurex_macro_output_contract_summary(
    std::ostream& stream,
    const AurexMacroOutputContractSummary& summary)
{
    stream << " aurex_macro_output_contracts=" << summary.contract_gate_count
           << " aurex_macro_output_contract_call_bindings="
           << summary.contract_call_binding_count
           << " aurex_macro_output_contract_user_derives=" << summary.contract_user_derive_count
           << " aurex_macro_output_contract_compiler_owned="
           << summary.contract_compiler_owned_count
           << " aurex_macro_output_contract_source_maps="
           << summary.contract_source_map_available_count
           << " aurex_macro_output_contract_hygiene_marks="
           << summary.contract_hygiene_mark_available_count
           << " aurex_macro_output_contract_diagnostic_projections="
           << summary.contract_diagnostic_projection_available_count
           << " aurex_macro_output_contract_declared_name_policies="
           << summary.contract_declared_name_policy_available_count
           << " aurex_macro_output_declared_name_policies="
           << summary.declared_name_policy_gate_count
           << " aurex_macro_output_declared_name_sets_reserved="
           << summary.declared_name_set_reserved_count
           << " aurex_macro_output_lookup_visible_declared_names="
           << summary.lookup_visible_declared_name_count
           << " aurex_macro_output_diagnostic_projections="
           << summary.diagnostic_projection_gate_count
           << " aurex_macro_output_diagnostic_debuggable="
           << summary.diagnostic_projection_debuggable_count
           << " aurex_macro_output_diagnostic_emission_enabled="
           << summary.diagnostic_emission_enabled_count;
}

void dump_aurex_macro_output_contract_admissions(
    std::ostream& stream,
    const std::span<const AurexMacroOutputContractAdmissionGate> contracts,
    const std::span<const AurexMacroOutputDeclaredNamePolicyAdmissionGate> declared_name_policies,
    const std::span<const AurexMacroOutputDiagnosticProjectionAdmissionGate> diagnostic_projections)
{
    for (base::usize index = 0; index < contracts.size(); ++index) {
        const AurexMacroOutputContractAdmissionGate& gate = contracts[index];
        stream << "  aurex_macro_output_contract_gate #" << index
               << " consumer_item=" << gate.consumer_item.value
               << " macro_item=" << gate.macro_item.value
               << " module=" << gate.module.value
               << " part=" << gate.part_index
               << " output=" << gate.output_index
               << " origin=" << aurex_macro_output_contract_origin_kind_name(gate.origin_kind)
               << " macro_kind=" << static_cast<base::u32>(gate.macro_kind)
               << " macro_name=" << gate.macro_name
               << " consumer_name=" << gate.consumer_name
               << " policy=" << gate.output_policy
               << " query=" << gate.query_name
               << " token_buffer=" << gate.token_buffer_name
               << " planned_tokens=" << gate.planned_token_count
               << " compiler_owned=" << yes_no(gate.compiler_owned_output)
               << " source_map_available=" << yes_no(gate.source_map_available)
               << " hygiene_mark_available=" << yes_no(gate.hygiene_mark_available)
               << " diagnostic_projection_available="
               << yes_no(gate.diagnostic_projection_available)
               << " declared_name_policy_available="
               << yes_no(gate.declared_name_policy_available)
               << " token_buffer_materialized=" << yes_no(gate.token_buffer_materialized)
               << " generated_source_text=" << yes_no(gate.generated_source_text)
               << " parse_ready=" << yes_no(gate.parse_ready)
               << " parser_consumable=" << yes_no(gate.parser_consumable)
               << " parser_consumption_enabled="
               << yes_no(gate.parser_consumption_enabled)
               << " ast_mutated=" << yes_no(gate.ast_mutated)
               << " sema_visible_generated_items="
               << yes_no(gate.sema_visible_generated_items)
               << " user_generated_code=" << yes_no(gate.produced_user_generated_code)
               << " gate_visible=" << yes_no(gate.gate_visible)
               << " query_reusable=" << yes_no(gate.query_reusable)
               << " blocker=" << gate.blocker_reason
               << " source_identity=" << query::debug_string(gate.source_admission_identity)
               << " output_contract_identity="
               << query::debug_string(gate.output_contract_identity)
               << " token_buffer_identity=" << query::debug_string(gate.token_buffer_identity)
               << " declared_name_policy_identity="
               << query::debug_string(gate.declared_name_policy_identity)
               << " source_map_identity=" << query::debug_string(gate.source_map_identity)
               << " hygiene_mark=" << query::debug_string(gate.hygiene_mark)
               << " diagnostic_anchor=" << query::debug_string(gate.diagnostic_anchor_identity)
               << '\n';
    }
    for (base::usize index = 0; index < declared_name_policies.size(); ++index) {
        const AurexMacroOutputDeclaredNamePolicyAdmissionGate& gate =
            declared_name_policies[index];
        stream << "  aurex_macro_output_declared_name_policy_gate #" << index
               << " consumer_item=" << gate.consumer_item.value
               << " macro_item=" << gate.macro_item.value
               << " module=" << gate.module.value
               << " part=" << gate.part_index
               << " output=" << gate.output_index
               << " origin=" << aurex_macro_output_contract_origin_kind_name(gate.origin_kind)
               << " macro_name=" << gate.macro_name
               << " consumer_name=" << gate.consumer_name
               << " policy=" << gate.declared_name_policy
               << " namespace=" << gate.declared_name_namespace
               << " query=" << gate.query_name
               << " planned_declared_names=" << gate.planned_declared_name_count
               << " declared_name_set_reserved=" << yes_no(gate.declared_name_set_reserved)
               << " lookup_visible=" << yes_no(gate.lookup_visible)
               << " export_visible=" << yes_no(gate.export_visible)
               << " sema_visible=" << yes_no(gate.sema_visible)
               << " parser_consumable=" << yes_no(gate.parser_consumable)
               << " ast_mutated=" << yes_no(gate.ast_mutated)
               << " user_generated_code=" << yes_no(gate.produced_user_generated_code)
               << " gate_visible=" << yes_no(gate.gate_visible)
               << " query_reusable=" << yes_no(gate.query_reusable)
               << " blocker=" << gate.blocker_reason
               << " output_contract_identity="
               << query::debug_string(gate.output_contract_identity)
               << " declared_name_policy_identity="
               << query::debug_string(gate.declared_name_policy_identity)
               << " declared_name_set="
               << query::debug_string(gate.declared_name_set_fingerprint)
               << " hygiene_mark=" << query::debug_string(gate.hygiene_mark)
               << '\n';
    }
    for (base::usize index = 0; index < diagnostic_projections.size(); ++index) {
        const AurexMacroOutputDiagnosticProjectionAdmissionGate& gate =
            diagnostic_projections[index];
        stream << "  aurex_macro_output_diagnostic_projection_gate #" << index
               << " consumer_item=" << gate.consumer_item.value
               << " macro_item=" << gate.macro_item.value
               << " module=" << gate.module.value
               << " part=" << gate.part_index
               << " output=" << gate.output_index
               << " origin=" << aurex_macro_output_contract_origin_kind_name(gate.origin_kind)
               << " macro_name=" << gate.macro_name
               << " consumer_name=" << gate.consumer_name
               << " policy=" << gate.diagnostic_policy
               << " query=" << gate.query_name
               << " category=" << gate.blocker_category
               << " debug_projection_available=" << yes_no(gate.debug_projection_available)
               << " diagnostic_emission_enabled="
               << yes_no(gate.diagnostic_emission_enabled)
               << " parser_consumable=" << yes_no(gate.parser_consumable)
               << " ast_mutated=" << yes_no(gate.ast_mutated)
               << " sema_visible_generated_items="
               << yes_no(gate.sema_visible_generated_items)
               << " user_generated_code=" << yes_no(gate.produced_user_generated_code)
               << " gate_visible=" << yes_no(gate.gate_visible)
               << " query_reusable=" << yes_no(gate.query_reusable)
               << " message=" << gate.user_message
               << " blocker=" << gate.blocker_reason
               << " output_contract_identity="
               << query::debug_string(gate.output_contract_identity)
               << " token_buffer_identity=" << query::debug_string(gate.token_buffer_identity)
               << " diagnostic_projection_identity="
               << query::debug_string(gate.diagnostic_projection_identity)
               << " source_map_identity=" << query::debug_string(gate.source_map_identity)
               << " hygiene_mark=" << query::debug_string(gate.hygiene_mark)
               << '\n';
    }
}

} // namespace aurex::frontend::macro
