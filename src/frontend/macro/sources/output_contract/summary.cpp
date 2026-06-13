#include <aurex/frontend/macro/output_contract_admission.hpp>

namespace aurex::frontend::macro {
namespace {

void add_counter(base::u64& counter, const bool condition) noexcept
{
    counter += static_cast<base::u64>(condition);
}

void accumulate_contract_summary(
    AurexMacroOutputContractSummary& summary,
    const AurexMacroOutputContractAdmissionGate& gate) noexcept
{
    add_counter(summary.contract_call_binding_count,
        gate.origin_kind == AurexMacroOutputContractOriginKind::matcher_to_call_binding);
    add_counter(summary.contract_user_derive_count,
        gate.origin_kind == AurexMacroOutputContractOriginKind::user_derive_target_schema);
    add_counter(summary.contract_visible_count, gate.gate_visible);
    add_counter(summary.contract_query_reusable_count, gate.query_reusable);
    add_counter(summary.contract_compiler_owned_count,
        gate.compiler_owned_output);
    add_counter(summary.contract_source_map_available_count,
        gate.source_map_available);
    add_counter(summary.contract_hygiene_mark_available_count,
        gate.hygiene_mark_available);
    add_counter(summary.contract_diagnostic_projection_available_count,
        gate.diagnostic_projection_available);
    add_counter(summary.contract_declared_name_policy_available_count,
        gate.declared_name_policy_available);
    summary.contract_planned_token_count += gate.planned_token_count;
    add_counter(summary.blocked_effects.generated_source_text_count, gate.generated_source_text);
    add_counter(summary.blocked_effects.parse_ready_token_buffer_count,
        gate.parse_ready | gate.parser_consumable | gate.parser_consumption_enabled);
    add_counter(summary.blocked_effects.ast_mutation_count, gate.ast_mutated);
    add_counter(summary.blocked_effects.sema_visible_generated_part_count,
        gate.sema_visible_generated_items);
    add_counter(summary.blocked_effects.standard_library_required_count,
        gate.standard_library_required);
    add_counter(summary.blocked_effects.runtime_required_count, gate.runtime_required);
    add_counter(summary.blocked_effects.external_process_required_count,
        gate.external_process_required);
    add_counter(summary.blocked_effects.user_generated_code_count,
        gate.produced_user_generated_code);
}

void accumulate_declared_name_summary(
    AurexMacroOutputContractSummary& summary,
    const AurexMacroOutputDeclaredNamePolicyAdmissionGate& gate) noexcept
{
    add_counter(summary.declared_name_policy_visible_count,
        gate.gate_visible);
    add_counter(summary.declared_name_policy_query_reusable_count,
        gate.query_reusable);
    add_counter(summary.declared_name_set_reserved_count,
        gate.declared_name_set_reserved);
    add_counter(summary.lookup_visible_declared_name_count,
        gate.lookup_visible);
    add_counter(summary.export_visible_declared_name_count,
        gate.export_visible);
    add_counter(summary.sema_visible_declared_name_count, gate.sema_visible);
    add_counter(summary.blocked_effects.sema_visible_generated_part_count, gate.sema_visible);
    add_counter(summary.blocked_effects.parse_ready_token_buffer_count, gate.parser_consumable);
    add_counter(summary.blocked_effects.ast_mutation_count, gate.ast_mutated);
    add_counter(summary.blocked_effects.standard_library_required_count,
        gate.standard_library_required);
    add_counter(summary.blocked_effects.runtime_required_count, gate.runtime_required);
    add_counter(summary.blocked_effects.external_process_required_count,
        gate.external_process_required);
    add_counter(summary.blocked_effects.user_generated_code_count,
        gate.produced_user_generated_code);
}

void accumulate_diagnostic_summary(
    AurexMacroOutputContractSummary& summary,
    const AurexMacroOutputDiagnosticProjectionAdmissionGate& gate) noexcept
{
    add_counter(summary.diagnostic_projection_visible_count,
        gate.gate_visible);
    add_counter(summary.diagnostic_projection_query_reusable_count,
        gate.query_reusable);
    add_counter(summary.diagnostic_projection_debuggable_count,
        gate.debug_projection_available);
    add_counter(summary.diagnostic_emission_enabled_count,
        gate.diagnostic_emission_enabled);
    add_counter(summary.blocked_effects.parse_ready_token_buffer_count, gate.parser_consumable);
    add_counter(summary.blocked_effects.ast_mutation_count, gate.ast_mutated);
    add_counter(summary.blocked_effects.sema_visible_generated_part_count,
        gate.sema_visible_generated_items);
    add_counter(summary.blocked_effects.standard_library_required_count,
        gate.standard_library_required);
    add_counter(summary.blocked_effects.runtime_required_count, gate.runtime_required);
    add_counter(summary.blocked_effects.external_process_required_count,
        gate.external_process_required);
    add_counter(summary.blocked_effects.user_generated_code_count,
        gate.produced_user_generated_code);
}

} // namespace

AurexMacroOutputContractSummary summarize_aurex_macro_output_contract_admissions(
    const std::span<const AurexMacroOutputContractAdmissionGate> contracts,
    const std::span<const AurexMacroOutputDeclaredNamePolicyAdmissionGate> declared_name_policies,
    const std::span<const AurexMacroOutputDiagnosticProjectionAdmissionGate> diagnostic_projections) noexcept
{
    AurexMacroOutputContractSummary summary;
    summary.contract_gate_count = static_cast<base::u64>(contracts.size());
    for (const AurexMacroOutputContractAdmissionGate& gate : contracts) {
        accumulate_contract_summary(summary, gate);
    }

    summary.declared_name_policy_gate_count =
        static_cast<base::u64>(declared_name_policies.size());
    for (const AurexMacroOutputDeclaredNamePolicyAdmissionGate& gate : declared_name_policies) {
        accumulate_declared_name_summary(summary, gate);
    }

    summary.diagnostic_projection_gate_count =
        static_cast<base::u64>(diagnostic_projections.size());
    for (const AurexMacroOutputDiagnosticProjectionAdmissionGate& gate : diagnostic_projections) {
        accumulate_diagnostic_summary(summary, gate);
    }
    return summary;
}

} // namespace aurex::frontend::macro
