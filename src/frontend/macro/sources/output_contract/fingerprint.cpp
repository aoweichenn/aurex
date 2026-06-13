#include "frontend/macro/detail/output_contract/output_contract_admission_detail.hpp"

#include <aurex/frontend/macro/output_contract_admission.hpp>

namespace aurex::frontend::macro {

void mix_aurex_macro_output_contract_summary(
    query::StableHashBuilder& builder,
    const AurexMacroOutputContractSummary& summary) noexcept
{
    builder.mix_u64(summary.contract_gate_count);
    builder.mix_u64(summary.contract_call_binding_count);
    builder.mix_u64(summary.contract_user_derive_count);
    builder.mix_u64(summary.contract_visible_count);
    builder.mix_u64(summary.contract_query_reusable_count);
    builder.mix_u64(summary.contract_compiler_owned_count);
    builder.mix_u64(summary.contract_source_map_available_count);
    builder.mix_u64(summary.contract_hygiene_mark_available_count);
    builder.mix_u64(summary.contract_diagnostic_projection_available_count);
    builder.mix_u64(summary.contract_declared_name_policy_available_count);
    builder.mix_u64(summary.contract_planned_token_count);
    builder.mix_u64(summary.declared_name_policy_gate_count);
    builder.mix_u64(summary.declared_name_policy_visible_count);
    builder.mix_u64(summary.declared_name_policy_query_reusable_count);
    builder.mix_u64(summary.declared_name_set_reserved_count);
    builder.mix_u64(summary.lookup_visible_declared_name_count);
    builder.mix_u64(summary.export_visible_declared_name_count);
    builder.mix_u64(summary.sema_visible_declared_name_count);
    builder.mix_u64(summary.diagnostic_projection_gate_count);
    builder.mix_u64(summary.diagnostic_projection_visible_count);
    builder.mix_u64(summary.diagnostic_projection_query_reusable_count);
    builder.mix_u64(summary.diagnostic_projection_debuggable_count);
    builder.mix_u64(summary.diagnostic_emission_enabled_count);
    builder.mix_u64(summary.blocked_effects.generated_source_text_count);
    builder.mix_u64(summary.blocked_effects.parse_ready_token_buffer_count);
    builder.mix_u64(summary.blocked_effects.ast_mutation_count);
    builder.mix_u64(summary.blocked_effects.sema_visible_generated_part_count);
    builder.mix_u64(summary.blocked_effects.standard_library_required_count);
    builder.mix_u64(summary.blocked_effects.runtime_required_count);
    builder.mix_u64(summary.blocked_effects.external_process_required_count);
    builder.mix_u64(summary.blocked_effects.user_generated_code_count);
}

void mix_aurex_macro_output_contract_admissions(
    query::StableHashBuilder& builder,
    const std::span<const AurexMacroOutputContractAdmissionGate> contracts,
    const std::span<const AurexMacroOutputDeclaredNamePolicyAdmissionGate> declared_name_policies,
    const std::span<const AurexMacroOutputDiagnosticProjectionAdmissionGate> diagnostic_projections) noexcept
{
    builder.mix_u64(static_cast<base::u64>(contracts.size()));
    for (const AurexMacroOutputContractAdmissionGate& gate : contracts) {
        detail::mix_aurex_macro_output_contract_gate(builder, gate);
    }
    builder.mix_u64(static_cast<base::u64>(declared_name_policies.size()));
    for (const AurexMacroOutputDeclaredNamePolicyAdmissionGate& gate : declared_name_policies) {
        detail::mix_aurex_macro_output_declared_name_policy_gate(builder, gate);
    }
    builder.mix_u64(static_cast<base::u64>(diagnostic_projections.size()));
    for (const AurexMacroOutputDiagnosticProjectionAdmissionGate& gate : diagnostic_projections) {
        detail::mix_aurex_macro_output_diagnostic_projection_gate(builder, gate);
    }
}

} // namespace aurex::frontend::macro

namespace aurex::frontend::macro::detail {
namespace {

void mix_source_range(query::StableHashBuilder& builder, const base::SourceRange& range) noexcept
{
    builder.mix_u64(static_cast<base::u64>(range.source.value));
    builder.mix_u64(static_cast<base::u64>(range.begin));
    builder.mix_u64(static_cast<base::u64>(range.end));
}

} // namespace

void mix_aurex_macro_output_contract_gate(
    query::StableHashBuilder& builder,
    const AurexMacroOutputContractAdmissionGate& gate) noexcept
{
    builder.mix_u32(gate.consumer_item.value);
    builder.mix_u32(gate.macro_item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_u32(gate.output_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.generated_part));
    builder.mix_fingerprint(gate.surface_admission_identity);
    builder.mix_fingerprint(gate.source_admission_identity);
    builder.mix_fingerprint(gate.matcher_identity);
    builder.mix_fingerprint(gate.target_schema_identity);
    builder.mix_fingerprint(gate.output_contract_identity);
    builder.mix_fingerprint(gate.token_buffer_identity);
    builder.mix_fingerprint(gate.declared_name_policy_identity);
    builder.mix_fingerprint(gate.hygiene_mark);
    builder.mix_fingerprint(gate.diagnostic_anchor_identity);
    builder.mix_fingerprint(gate.source_map_identity);
    builder.mix_u8(static_cast<base::u8>(gate.origin_kind));
    builder.mix_u8(static_cast<base::u8>(gate.macro_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_string(gate.consumer_name);
    builder.mix_string(gate.output_policy);
    builder.mix_string(gate.query_name);
    builder.mix_string(gate.token_buffer_name);
    builder.mix_string(gate.blocker_reason);
    mix_source_range(builder, gate.source_range);
    mix_source_range(builder, gate.output_range);
    builder.mix_u64(gate.planned_token_count);
    builder.mix_bool(gate.compiler_owned_output);
    builder.mix_bool(gate.source_anchor_available);
    builder.mix_bool(gate.source_map_available);
    builder.mix_bool(gate.hygiene_mark_available);
    builder.mix_bool(gate.diagnostic_projection_available);
    builder.mix_bool(gate.declared_name_policy_available);
    builder.mix_bool(gate.token_buffer_materialized);
    builder.mix_bool(gate.generated_source_text);
    builder.mix_bool(gate.parse_ready);
    builder.mix_bool(gate.parser_consumable);
    builder.mix_bool(gate.parser_consumption_enabled);
    builder.mix_bool(gate.ast_mutated);
    builder.mix_bool(gate.sema_visible_generated_items);
    builder.mix_bool(gate.standard_library_required);
    builder.mix_bool(gate.runtime_required);
    builder.mix_bool(gate.external_process_required);
    builder.mix_bool(gate.produced_user_generated_code);
    builder.mix_bool(gate.gate_visible);
    builder.mix_bool(gate.query_reusable);
}

void mix_aurex_macro_output_declared_name_policy_gate(
    query::StableHashBuilder& builder,
    const AurexMacroOutputDeclaredNamePolicyAdmissionGate& gate) noexcept
{
    builder.mix_u32(gate.consumer_item.value);
    builder.mix_u32(gate.macro_item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_u32(gate.output_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.generated_part));
    builder.mix_fingerprint(gate.output_contract_identity);
    builder.mix_fingerprint(gate.declared_name_policy_identity);
    builder.mix_fingerprint(gate.declared_name_set_fingerprint);
    builder.mix_fingerprint(gate.hygiene_mark);
    builder.mix_fingerprint(gate.diagnostic_anchor_identity);
    builder.mix_u8(static_cast<base::u8>(gate.origin_kind));
    builder.mix_u8(static_cast<base::u8>(gate.macro_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_string(gate.consumer_name);
    builder.mix_string(gate.declared_name_policy);
    builder.mix_string(gate.declared_name_namespace);
    builder.mix_string(gate.query_name);
    builder.mix_string(gate.blocker_reason);
    builder.mix_u64(gate.planned_declared_name_count);
    builder.mix_bool(gate.compiler_owned_output);
    builder.mix_bool(gate.declared_name_set_reserved);
    builder.mix_bool(gate.lookup_visible);
    builder.mix_bool(gate.export_visible);
    builder.mix_bool(gate.sema_visible);
    builder.mix_bool(gate.parser_consumable);
    builder.mix_bool(gate.ast_mutated);
    builder.mix_bool(gate.standard_library_required);
    builder.mix_bool(gate.runtime_required);
    builder.mix_bool(gate.external_process_required);
    builder.mix_bool(gate.produced_user_generated_code);
    builder.mix_bool(gate.gate_visible);
    builder.mix_bool(gate.query_reusable);
}

void mix_aurex_macro_output_diagnostic_projection_gate(
    query::StableHashBuilder& builder,
    const AurexMacroOutputDiagnosticProjectionAdmissionGate& gate) noexcept
{
    builder.mix_u32(gate.consumer_item.value);
    builder.mix_u32(gate.macro_item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_u32(gate.output_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.generated_part));
    builder.mix_fingerprint(gate.output_contract_identity);
    builder.mix_fingerprint(gate.token_buffer_identity);
    builder.mix_fingerprint(gate.diagnostic_projection_identity);
    builder.mix_fingerprint(gate.diagnostic_anchor_identity);
    builder.mix_fingerprint(gate.source_map_identity);
    builder.mix_fingerprint(gate.hygiene_mark);
    builder.mix_u8(static_cast<base::u8>(gate.origin_kind));
    builder.mix_u8(static_cast<base::u8>(gate.macro_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_string(gate.consumer_name);
    builder.mix_string(gate.diagnostic_policy);
    builder.mix_string(gate.query_name);
    builder.mix_string(gate.blocker_category);
    builder.mix_string(gate.user_message);
    builder.mix_string(gate.blocker_reason);
    mix_source_range(builder, gate.primary_anchor);
    mix_source_range(builder, gate.output_anchor);
    builder.mix_bool(gate.compiler_owned_output);
    builder.mix_bool(gate.source_anchor_available);
    builder.mix_bool(gate.source_map_available);
    builder.mix_bool(gate.hygiene_mark_available);
    builder.mix_bool(gate.debug_projection_available);
    builder.mix_bool(gate.diagnostic_emission_enabled);
    builder.mix_bool(gate.parser_consumable);
    builder.mix_bool(gate.ast_mutated);
    builder.mix_bool(gate.sema_visible_generated_items);
    builder.mix_bool(gate.standard_library_required);
    builder.mix_bool(gate.runtime_required);
    builder.mix_bool(gate.external_process_required);
    builder.mix_bool(gate.produced_user_generated_code);
    builder.mix_bool(gate.gate_visible);
    builder.mix_bool(gate.query_reusable);
}

} // namespace aurex::frontend::macro::detail
