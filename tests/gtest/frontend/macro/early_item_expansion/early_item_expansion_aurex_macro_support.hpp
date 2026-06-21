#pragma once

#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_common_support.hpp>

#include <algorithm>
#include <string_view>

namespace aurex::test::early_item_expansion_support {

[[nodiscard]] inline const frontend::macro::AurexMacroSurfaceAdmissionGate* aurex_macro_surface_gate_by_name(
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

[[nodiscard]] inline const frontend::macro::AurexMacroDefinitionSiteHygieneAdmissionGate*
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

[[nodiscard]] inline const frontend::macro::AurexMacroTypedMatcherAdmissionGate*
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

[[nodiscard]] inline const frontend::macro::AurexMacroCallSiteAdmissionGate*
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

[[nodiscard]] inline const frontend::macro::AurexMacroMatcherToCallBindingAdmissionGate*
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

[[nodiscard]] inline const frontend::macro::AurexUserDeriveTargetSchemaAdmissionGate*
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

} // namespace aurex::test::early_item_expansion_support
