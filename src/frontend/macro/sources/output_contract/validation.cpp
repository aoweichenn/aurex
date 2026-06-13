#include "frontend/macro/detail/output_contract/output_contract_admission_detail.hpp"

#include <aurex/frontend/macro/early_item_expansion.hpp>
#include <aurex/frontend/macro/output_contract_admission.hpp>

#include <algorithm>

namespace aurex::frontend::macro {

bool is_valid(const AurexMacroOutputContractAdmissionGate& gate) noexcept
{
    const bool call_binding_origin =
        gate.origin_kind == AurexMacroOutputContractOriginKind::matcher_to_call_binding;
    const bool derive_origin =
        gate.origin_kind == AurexMacroOutputContractOriginKind::user_derive_target_schema;
    return syntax::is_valid(gate.consumer_item)
        && syntax::is_valid(gate.macro_item)
        && syntax::is_valid(gate.module)
        && query::is_valid(gate.attached_part)
        && query::is_valid(gate.generated_part)
        && gate.generated_part.kind == query::ModulePartKind::generated
        && gate.generated_part.file.role == query::SourceRole::generated
        && detail::is_nonzero_fingerprint(gate.surface_admission_identity)
        && detail::is_nonzero_fingerprint(gate.source_admission_identity)
        && (call_binding_origin ? detail::is_nonzero_fingerprint(gate.matcher_identity)
                                : gate.matcher_identity == query::StableFingerprint128{})
        && (derive_origin ? detail::is_nonzero_fingerprint(gate.target_schema_identity)
                          : gate.target_schema_identity == query::StableFingerprint128{})
        && detail::is_nonzero_fingerprint(gate.output_contract_identity)
        && detail::is_nonzero_fingerprint(gate.token_buffer_identity)
        && detail::is_nonzero_fingerprint(gate.declared_name_policy_identity)
        && detail::is_nonzero_fingerprint(gate.hygiene_mark)
        && detail::is_nonzero_fingerprint(gate.diagnostic_anchor_identity)
        && detail::is_nonzero_fingerprint(gate.source_map_identity)
        && !gate.macro_name.empty()
        && !gate.consumer_name.empty()
        && gate.output_policy == detail::FRONTEND_MACRO_M27D_OUTPUT_CONTRACT_POLICY
        && gate.query_name == detail::macro_output_contract_query_name(gate.module, gate.part_index,
               gate.consumer_item, gate.macro_item, gate.output_index, gate.macro_name)
        && gate.token_buffer_name == detail::macro_output_token_buffer_name(gate.module,
               gate.part_index, gate.consumer_item, gate.macro_item, gate.output_index,
               gate.macro_name)
        && gate.generated_part == detail::macro_output_generated_module_part_key(gate.attached_part,
               gate.module, gate.part_index, gate.consumer_item, gate.macro_item, gate.output_index,
               gate.macro_name)
        && gate.source_map_identity == detail::macro_output_source_map_identity(gate.module,
               gate.part_index, gate.consumer_item, gate.macro_item, gate.output_index,
               gate.source_admission_identity, gate.source_range)
        && gate.hygiene_mark == detail::macro_output_hygiene_mark_identity(gate.module,
               gate.part_index, gate.consumer_item, gate.macro_item, gate.output_index,
               gate.surface_admission_identity, gate.source_admission_identity, gate.matcher_identity,
               gate.target_schema_identity)
        && gate.token_buffer_identity == detail::macro_output_token_buffer_identity(gate)
        && gate.output_contract_identity == detail::macro_output_contract_identity(gate)
        && gate.blocker_reason == detail::FRONTEND_MACRO_M27D_OUTPUT_CONTRACT_BLOCKER
        && detail::source_range_is_well_formed(gate.source_range)
        && detail::source_range_is_well_formed(gate.output_range)
        && gate.compiler_owned_output
        && gate.source_anchor_available
        && gate.source_map_available
        && gate.hygiene_mark_available
        && gate.diagnostic_projection_available
        && gate.declared_name_policy_available
        && !gate.token_buffer_materialized
        && !gate.generated_source_text
        && !gate.parse_ready
        && !gate.parser_consumable
        && !gate.parser_consumption_enabled
        && !gate.ast_mutated
        && !gate.sema_visible_generated_items
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.gate_visible
        && gate.query_reusable;
}

bool is_valid(const AurexMacroOutputDeclaredNamePolicyAdmissionGate& gate) noexcept
{
    return syntax::is_valid(gate.consumer_item)
        && syntax::is_valid(gate.macro_item)
        && syntax::is_valid(gate.module)
        && query::is_valid(gate.attached_part)
        && query::is_valid(gate.generated_part)
        && gate.generated_part.kind == query::ModulePartKind::generated
        && gate.generated_part.file.role == query::SourceRole::generated
        && detail::is_nonzero_fingerprint(gate.output_contract_identity)
        && detail::is_nonzero_fingerprint(gate.declared_name_policy_identity)
        && detail::is_nonzero_fingerprint(gate.declared_name_set_fingerprint)
        && detail::is_nonzero_fingerprint(gate.hygiene_mark)
        && detail::is_nonzero_fingerprint(gate.diagnostic_anchor_identity)
        && !gate.macro_name.empty()
        && !gate.consumer_name.empty()
        && gate.declared_name_policy == detail::FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_POLICY
        && gate.declared_name_namespace == detail::FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_NAMESPACE
        && gate.query_name == detail::macro_output_declared_name_policy_query_name(gate.module,
               gate.part_index, gate.consumer_item, gate.macro_item, gate.output_index,
               gate.macro_name)
        && gate.blocker_reason == detail::FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_BLOCKER
        && gate.declared_name_policy_identity == detail::macro_output_declared_name_policy_identity(gate)
        && gate.compiler_owned_output
        && gate.declared_name_set_reserved
        && gate.planned_declared_name_count == 0U
        && !gate.lookup_visible
        && !gate.export_visible
        && !gate.sema_visible
        && !gate.parser_consumable
        && !gate.ast_mutated
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.gate_visible
        && gate.query_reusable;
}

bool is_valid(const AurexMacroOutputDiagnosticProjectionAdmissionGate& gate) noexcept
{
    return syntax::is_valid(gate.consumer_item)
        && syntax::is_valid(gate.macro_item)
        && syntax::is_valid(gate.module)
        && query::is_valid(gate.attached_part)
        && query::is_valid(gate.generated_part)
        && gate.generated_part.kind == query::ModulePartKind::generated
        && gate.generated_part.file.role == query::SourceRole::generated
        && detail::is_nonzero_fingerprint(gate.output_contract_identity)
        && detail::is_nonzero_fingerprint(gate.token_buffer_identity)
        && detail::is_nonzero_fingerprint(gate.diagnostic_projection_identity)
        && detail::is_nonzero_fingerprint(gate.diagnostic_anchor_identity)
        && detail::is_nonzero_fingerprint(gate.source_map_identity)
        && detail::is_nonzero_fingerprint(gate.hygiene_mark)
        && !gate.macro_name.empty()
        && !gate.consumer_name.empty()
        && gate.diagnostic_policy == detail::FRONTEND_MACRO_M27D_OUTPUT_DIAGNOSTIC_POLICY
        && gate.query_name == detail::macro_output_diagnostic_projection_query_name(gate.module,
               gate.part_index, gate.consumer_item, gate.macro_item, gate.output_index,
               gate.macro_name)
        && gate.blocker_category == detail::FRONTEND_MACRO_M27D_OUTPUT_DIAGNOSTIC_CATEGORY
        && gate.user_message == detail::FRONTEND_MACRO_M27D_OUTPUT_PARSE_BLOCKER
        && gate.blocker_reason == detail::FRONTEND_MACRO_M27D_OUTPUT_DIAGNOSTIC_BLOCKER
        && gate.diagnostic_projection_identity
            == detail::macro_output_diagnostic_projection_identity(gate)
        && detail::source_range_is_well_formed(gate.primary_anchor)
        && detail::source_range_is_well_formed(gate.output_anchor)
        && gate.compiler_owned_output
        && gate.source_anchor_available
        && gate.source_map_available
        && gate.hygiene_mark_available
        && gate.debug_projection_available
        && !gate.diagnostic_emission_enabled
        && !gate.parser_consumable
        && !gate.ast_mutated
        && !gate.sema_visible_generated_items
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.gate_visible
        && gate.query_reusable;
}

bool aurex_macro_output_contracts_match_inputs(
    const std::span<const AurexMacroMatcherToCallBindingAdmissionGate> bindings,
    const std::span<const AurexUserDeriveTargetSchemaAdmissionGate> schemas,
    const std::span<const AurexMacroOutputContractAdmissionGate> contracts,
    const std::span<const AurexMacroOutputDeclaredNamePolicyAdmissionGate> declared_name_policies,
    const std::span<const AurexMacroOutputDiagnosticProjectionAdmissionGate> diagnostic_projections) noexcept
{
    const base::u64 expected_contract_count =
        static_cast<base::u64>(bindings.size()) + static_cast<base::u64>(schemas.size());
    if (expected_contract_count != static_cast<base::u64>(contracts.size())
        || declared_name_policies.size() != contracts.size()
        || diagnostic_projections.size() != contracts.size()) {
        return false;
    }

    base::u64 output_index = 0U;
    for (const AurexMacroMatcherToCallBindingAdmissionGate& binding : bindings) {
        const auto contract = std::find_if(contracts.begin(), contracts.end(),
            [&binding, output_index](const AurexMacroOutputContractAdmissionGate& gate) {
                return gate.origin_kind == AurexMacroOutputContractOriginKind::matcher_to_call_binding
                    && gate.consumer_item.value == binding.call_item.value
                    && gate.macro_item.value == binding.macro_item.value
                    && gate.source_admission_identity == binding.binding_identity
                    && gate.surface_admission_identity == binding.surface_admission_identity
                    && gate.matcher_identity == binding.matcher_identity
                    && gate.output_index == output_index;
            });
        if (contract == contracts.end()) {
            return false;
        }
        ++output_index;
    }
    for (const AurexUserDeriveTargetSchemaAdmissionGate& schema : schemas) {
        const auto contract = std::find_if(contracts.begin(), contracts.end(),
            [&schema, output_index](const AurexMacroOutputContractAdmissionGate& gate) {
                return gate.origin_kind == AurexMacroOutputContractOriginKind::user_derive_target_schema
                    && gate.consumer_item.value == schema.target_item.value
                    && gate.macro_item.value == schema.macro_item.value
                    && gate.source_admission_identity == schema.schema_identity
                    && gate.surface_admission_identity == schema.surface_admission_identity
                    && gate.target_schema_identity == schema.schema_identity
                    && gate.output_index == output_index;
            });
        if (contract == contracts.end()) {
            return false;
        }
        ++output_index;
    }

    for (const AurexMacroOutputContractAdmissionGate& contract : contracts) {
        const auto declared_name = std::find_if(declared_name_policies.begin(),
            declared_name_policies.end(),
            [&contract](const AurexMacroOutputDeclaredNamePolicyAdmissionGate& gate) {
                return gate.output_contract_identity == contract.output_contract_identity
                    && gate.declared_name_policy_identity == contract.declared_name_policy_identity
                    && gate.hygiene_mark == contract.hygiene_mark
                    && gate.diagnostic_anchor_identity == contract.diagnostic_anchor_identity;
            });
        if (declared_name == declared_name_policies.end()) {
            return false;
        }
        const auto diagnostic = std::find_if(diagnostic_projections.begin(),
            diagnostic_projections.end(),
            [&contract](const AurexMacroOutputDiagnosticProjectionAdmissionGate& gate) {
                return gate.output_contract_identity == contract.output_contract_identity
                    && gate.token_buffer_identity == contract.token_buffer_identity
                    && gate.diagnostic_anchor_identity == contract.diagnostic_anchor_identity
                    && gate.source_map_identity == contract.source_map_identity
                    && gate.hygiene_mark == contract.hygiene_mark;
            });
        if (diagnostic == diagnostic_projections.end()) {
            return false;
        }
    }
    return true;
}

bool aurex_macro_output_contract_admissions_are_valid(
    const std::span<const AurexMacroMatcherToCallBindingAdmissionGate> bindings,
    const std::span<const AurexUserDeriveTargetSchemaAdmissionGate> schemas,
    const std::span<const AurexMacroOutputContractAdmissionGate> contracts,
    const std::span<const AurexMacroOutputDeclaredNamePolicyAdmissionGate> declared_name_policies,
    const std::span<const AurexMacroOutputDiagnosticProjectionAdmissionGate> diagnostic_projections) noexcept
{
    return std::all_of(contracts.begin(), contracts.end(),
               [](const AurexMacroOutputContractAdmissionGate& gate) {
                   return is_valid(gate);
               })
        && std::all_of(declared_name_policies.begin(), declared_name_policies.end(),
               [](const AurexMacroOutputDeclaredNamePolicyAdmissionGate& gate) {
                   return is_valid(gate);
               })
        && std::all_of(diagnostic_projections.begin(), diagnostic_projections.end(),
               [](const AurexMacroOutputDiagnosticProjectionAdmissionGate& gate) {
                   return is_valid(gate);
               })
        && aurex_macro_output_contracts_match_inputs(
            bindings, schemas, contracts, declared_name_policies, diagnostic_projections);
}

} // namespace aurex::frontend::macro
