#include "frontend/macro/detail/output_contract/output_contract_admission_detail.hpp"

#include <aurex/frontend/macro/early_item_expansion.hpp>
#include <aurex/frontend/macro/output_contract_admission.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace aurex::frontend::macro {
namespace {

void finalize_aurex_macro_output_contract_gate(AurexMacroOutputContractAdmissionGate& gate)
{
    gate.source_map_identity = detail::macro_output_source_map_identity(gate.module, gate.part_index,
        gate.consumer_item, gate.macro_item, gate.output_index, gate.source_admission_identity,
        gate.source_range);
    gate.hygiene_mark = detail::macro_output_hygiene_mark_identity(gate.module, gate.part_index,
        gate.consumer_item, gate.macro_item, gate.output_index, gate.surface_admission_identity,
        gate.source_admission_identity, gate.matcher_identity, gate.target_schema_identity);
    gate.token_buffer_identity = detail::macro_output_token_buffer_identity(gate);
    gate.output_contract_identity = detail::macro_output_contract_identity(gate);
    gate.declared_name_policy_identity =
        detail::macro_output_declared_name_policy_identity(
            detail::make_aurex_macro_output_declared_name_policy_gate(gate));
}

[[nodiscard]] AurexMacroOutputContractAdmissionGate make_aurex_macro_output_contract_gate(
    const AurexMacroMatcherToCallBindingAdmissionGate& binding,
    const base::u32 output_index)
{
    AurexMacroOutputContractAdmissionGate gate;
    gate.consumer_item = binding.call_item;
    gate.macro_item = binding.macro_item;
    gate.module = binding.module;
    gate.part_index = binding.part_index;
    gate.output_index = output_index;
    gate.attached_part = binding.attached_part;
    gate.generated_part = detail::macro_output_generated_module_part_key(binding.attached_part,
        binding.module, binding.part_index, binding.call_item, binding.macro_item, output_index,
        binding.macro_name);
    gate.surface_admission_identity = binding.surface_admission_identity;
    gate.source_admission_identity = binding.binding_identity;
    gate.matcher_identity = binding.matcher_identity;
    gate.origin_kind = AurexMacroOutputContractOriginKind::matcher_to_call_binding;
    gate.macro_kind = binding.macro_kind;
    gate.macro_name = binding.macro_name;
    gate.consumer_name = binding.macro_name;
    gate.output_policy = std::string(detail::FRONTEND_MACRO_M27D_OUTPUT_CONTRACT_POLICY);
    gate.query_name = detail::macro_output_contract_query_name(binding.module, binding.part_index,
        binding.call_item, binding.macro_item, output_index, binding.macro_name);
    gate.token_buffer_name = detail::macro_output_token_buffer_name(binding.module,
        binding.part_index, binding.call_item, binding.macro_item, output_index, binding.macro_name);
    gate.blocker_reason = std::string(detail::FRONTEND_MACRO_M27D_OUTPUT_CONTRACT_BLOCKER);
    gate.source_range = binding.call_range;
    gate.output_range = binding.matcher_range;
    gate.planned_token_count = 0U;
    gate.diagnostic_anchor_identity = binding.diagnostic_anchor_identity;
    finalize_aurex_macro_output_contract_gate(gate);
    return gate;
}

[[nodiscard]] AurexMacroOutputContractAdmissionGate make_aurex_macro_output_contract_gate(
    const AurexUserDeriveTargetSchemaAdmissionGate& schema,
    const AurexMacroSurfaceAdmissionGate& surface,
    const base::u32 output_index)
{
    AurexMacroOutputContractAdmissionGate gate;
    gate.consumer_item = schema.target_item;
    gate.macro_item = schema.macro_item;
    gate.module = schema.module;
    gate.part_index = schema.part_index;
    gate.output_index = output_index;
    gate.attached_part = schema.attached_part;
    gate.generated_part = detail::macro_output_generated_module_part_key(schema.attached_part,
        schema.module, schema.part_index, schema.target_item, schema.macro_item, output_index,
        schema.derive_name);
    gate.surface_admission_identity = schema.surface_admission_identity;
    gate.source_admission_identity = schema.schema_identity;
    gate.target_schema_identity = schema.schema_identity;
    gate.origin_kind = AurexMacroOutputContractOriginKind::user_derive_target_schema;
    gate.macro_kind = surface.macro_kind;
    gate.macro_name = schema.derive_name;
    gate.consumer_name = schema.target_name;
    gate.output_policy = std::string(detail::FRONTEND_MACRO_M27D_OUTPUT_CONTRACT_POLICY);
    gate.query_name = detail::macro_output_contract_query_name(schema.module, schema.part_index,
        schema.target_item, schema.macro_item, output_index, schema.derive_name);
    gate.token_buffer_name = detail::macro_output_token_buffer_name(schema.module,
        schema.part_index, schema.target_item, schema.macro_item, output_index, schema.derive_name);
    gate.blocker_reason = std::string(detail::FRONTEND_MACRO_M27D_OUTPUT_CONTRACT_BLOCKER);
    gate.source_range = schema.derive_range;
    gate.output_range = surface.body_range;
    gate.planned_token_count = 0U;
    gate.diagnostic_anchor_identity = schema.diagnostic_anchor_identity;
    finalize_aurex_macro_output_contract_gate(gate);
    return gate;
}

[[nodiscard]] const AurexMacroSurfaceAdmissionGate* find_user_derive_surface(
    const std::span<const AurexMacroSurfaceAdmissionGate> surfaces,
    const AurexUserDeriveTargetSchemaAdmissionGate& schema) noexcept
{
    const auto surface = std::find_if(surfaces.begin(), surfaces.end(),
        [&schema](const AurexMacroSurfaceAdmissionGate& gate) {
            return gate.item.value == schema.macro_item.value
                && gate.macro_kind == syntax::MacroDeclKind::derive
                && gate.admission_identity == schema.surface_admission_identity
                && gate.macro_name == schema.derive_name;
        });
    return surface == surfaces.end() ? nullptr : std::addressof(*surface);
}

void append_aurex_macro_output_contract(
    AurexMacroOutputContractAdmissionSet& admissions,
    AurexMacroOutputContractAdmissionGate contract)
{
    admissions.declared_name_policies.push_back(
        detail::make_aurex_macro_output_declared_name_policy_gate(contract));
    admissions.diagnostic_projections.push_back(
        detail::make_aurex_macro_output_diagnostic_projection_gate(contract));
    admissions.output_contracts.push_back(std::move(contract));
}

} // namespace

AurexMacroOutputContractAdmissionSet collect_aurex_macro_output_contract_admissions(
    const std::span<const AurexMacroSurfaceAdmissionGate> surfaces,
    const std::span<const AurexMacroMatcherToCallBindingAdmissionGate> bindings,
    const std::span<const AurexUserDeriveTargetSchemaAdmissionGate> schemas)
{
    AurexMacroOutputContractAdmissionSet admissions;
    const base::usize expected_count = bindings.size() + schemas.size();
    admissions.output_contracts.reserve(expected_count);
    admissions.declared_name_policies.reserve(expected_count);
    admissions.diagnostic_projections.reserve(expected_count);

    base::u32 output_index = 0U;
    for (const AurexMacroMatcherToCallBindingAdmissionGate& binding : bindings) {
        append_aurex_macro_output_contract(
            admissions, make_aurex_macro_output_contract_gate(binding, output_index));
        ++output_index;
    }
    for (const AurexUserDeriveTargetSchemaAdmissionGate& schema : schemas) {
        const AurexMacroSurfaceAdmissionGate* const surface = find_user_derive_surface(surfaces, schema);
        if (surface == nullptr) {
            continue;
        }
        append_aurex_macro_output_contract(
            admissions, make_aurex_macro_output_contract_gate(schema, *surface, output_index));
        ++output_index;
    }
    return admissions;
}

} // namespace aurex::frontend::macro

namespace aurex::frontend::macro::detail {

AurexMacroOutputDeclaredNamePolicyAdmissionGate make_aurex_macro_output_declared_name_policy_gate(
    const AurexMacroOutputContractAdmissionGate& contract)
{
    const std::string query_name = macro_output_declared_name_policy_query_name(contract.module,
        contract.part_index, contract.consumer_item, contract.macro_item, contract.output_index,
        contract.macro_name);

    AurexMacroOutputDeclaredNamePolicyAdmissionGate gate;
    gate.consumer_item = contract.consumer_item;
    gate.macro_item = contract.macro_item;
    gate.module = contract.module;
    gate.part_index = contract.part_index;
    gate.output_index = contract.output_index;
    gate.attached_part = contract.attached_part;
    gate.generated_part = contract.generated_part;
    gate.output_contract_identity = contract.output_contract_identity;
    gate.declared_name_set_fingerprint = macro_output_declared_name_set_fingerprint(contract);
    gate.hygiene_mark = contract.hygiene_mark;
    gate.diagnostic_anchor_identity = contract.diagnostic_anchor_identity;
    gate.origin_kind = contract.origin_kind;
    gate.macro_kind = contract.macro_kind;
    gate.macro_name = contract.macro_name;
    gate.consumer_name = contract.consumer_name;
    gate.declared_name_policy = std::string(FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_POLICY);
    gate.declared_name_namespace = std::string(FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_NAMESPACE);
    gate.query_name = query_name;
    gate.blocker_reason = std::string(FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_BLOCKER);
    gate.planned_declared_name_count = 0U;
    gate.declared_name_policy_identity = macro_output_declared_name_policy_identity(gate);
    return gate;
}

AurexMacroOutputDiagnosticProjectionAdmissionGate
make_aurex_macro_output_diagnostic_projection_gate(
    const AurexMacroOutputContractAdmissionGate& contract)
{
    const std::string query_name = macro_output_diagnostic_projection_query_name(contract.module,
        contract.part_index, contract.consumer_item, contract.macro_item, contract.output_index,
        contract.macro_name);

    AurexMacroOutputDiagnosticProjectionAdmissionGate gate;
    gate.consumer_item = contract.consumer_item;
    gate.macro_item = contract.macro_item;
    gate.module = contract.module;
    gate.part_index = contract.part_index;
    gate.output_index = contract.output_index;
    gate.attached_part = contract.attached_part;
    gate.generated_part = contract.generated_part;
    gate.output_contract_identity = contract.output_contract_identity;
    gate.token_buffer_identity = contract.token_buffer_identity;
    gate.diagnostic_anchor_identity = contract.diagnostic_anchor_identity;
    gate.source_map_identity = contract.source_map_identity;
    gate.hygiene_mark = contract.hygiene_mark;
    gate.origin_kind = contract.origin_kind;
    gate.macro_kind = contract.macro_kind;
    gate.macro_name = contract.macro_name;
    gate.consumer_name = contract.consumer_name;
    gate.diagnostic_policy = std::string(FRONTEND_MACRO_M27D_OUTPUT_DIAGNOSTIC_POLICY);
    gate.query_name = query_name;
    gate.blocker_category = std::string(FRONTEND_MACRO_M27D_OUTPUT_DIAGNOSTIC_CATEGORY);
    gate.user_message = std::string(FRONTEND_MACRO_M27D_OUTPUT_PARSE_BLOCKER);
    gate.blocker_reason = std::string(FRONTEND_MACRO_M27D_OUTPUT_DIAGNOSTIC_BLOCKER);
    gate.primary_anchor = contract.source_range;
    gate.output_anchor = contract.output_range;
    gate.diagnostic_projection_identity = macro_output_diagnostic_projection_identity(gate);
    return gate;
}

} // namespace aurex::frontend::macro::detail
