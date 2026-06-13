#include <aurex/frontend/macro/early_item_expansion.hpp>
#include <aurex/frontend/macro/output_contract_admission.hpp>

#include <support/frontend_macro_test_support.hpp>
#include <support/frontend_test_support.hpp>

#include <algorithm>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

using frontend_macro_support::expand_source;
using frontend_macro_support::mutated_expansion_result;

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

TEST(CoreUnit, OutputContractAdmissionCollectsCallAndDeriveContracts)
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
    ASSERT_EQ(result.aurex_macro_output_contract_gates.size(), 3U);
    ASSERT_EQ(result.aurex_macro_output_declared_name_policy_gates.size(), 3U);
    ASSERT_EQ(result.aurex_macro_output_diagnostic_projection_gates.size(), 3U);

    EXPECT_EQ(result.summary.aurex_macro_call_site_admission_gate_count, 2U);
    EXPECT_EQ(result.summary.aurex_macro_call_site_target_declared_count, 1U);
    EXPECT_EQ(result.summary.aurex_macro_matcher_to_call_binding_admitted_count, 1U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_source_derive_count, 2U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_struct_count, 1U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_enum_count, 1U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_field_count, 2U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_enum_case_count, 3U);
    EXPECT_EQ(result.summary.aurex_user_derive_target_schema_enum_payload_count, 3U);

    const frontend::macro::AurexMacroOutputContractSummary& output_summary =
        result.summary.aurex_macro_output_contracts;
    EXPECT_EQ(output_summary.contract_gate_count, 3U);
    EXPECT_EQ(output_summary.contract_call_binding_count, 1U);
    EXPECT_EQ(output_summary.contract_user_derive_count, 2U);
    EXPECT_EQ(output_summary.contract_compiler_owned_count, 3U);
    EXPECT_EQ(output_summary.contract_source_map_available_count, 3U);
    EXPECT_EQ(output_summary.contract_hygiene_mark_available_count, 3U);
    EXPECT_EQ(output_summary.contract_diagnostic_projection_available_count, 3U);
    EXPECT_EQ(output_summary.contract_declared_name_policy_available_count, 3U);
    EXPECT_EQ(output_summary.declared_name_policy_gate_count, 3U);
    EXPECT_EQ(output_summary.declared_name_set_reserved_count, 3U);
    EXPECT_EQ(output_summary.lookup_visible_declared_name_count, 0U);
    EXPECT_EQ(output_summary.export_visible_declared_name_count, 0U);
    EXPECT_EQ(output_summary.sema_visible_declared_name_count, 0U);
    EXPECT_EQ(output_summary.diagnostic_projection_gate_count, 3U);
    EXPECT_EQ(output_summary.diagnostic_projection_debuggable_count, 3U);
    EXPECT_EQ(output_summary.diagnostic_emission_enabled_count, 0U);
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
    EXPECT_TRUE(build_call->target_surface_declared);
    EXPECT_TRUE(build_call->token_tree_balanced);
    EXPECT_FALSE(missing_call->target_surface_declared);

    const frontend::macro::AurexMacroMatcherToCallBindingAdmissionGate* const binding =
        aurex_macro_binding_gate_by_call_name(result, "BuildVec");
    ASSERT_NE(binding, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*binding));
    EXPECT_TRUE(binding->binding_admitted);
    EXPECT_EQ(binding->matcher_kind, frontend::macro::AurexMacroTypedMatcherKind::expr_list);
    EXPECT_EQ(binding->binding_name, "xs");
    EXPECT_FALSE(binding->matcher_execution_enabled);
    EXPECT_FALSE(binding->parser_consumption_enabled);
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
    EXPECT_EQ(mode_schema->target_kind, frontend::macro::AurexUserDeriveTargetKind::enum_);
    EXPECT_EQ(mode_schema->enum_case_count, 3U);
    EXPECT_EQ(mode_schema->enum_payload_count, 3U);

    const frontend::macro::AurexMacroOutputContractAdmissionGate& call_output =
        result.aurex_macro_output_contract_gates.front();
    EXPECT_TRUE(frontend::macro::is_valid(call_output));
    EXPECT_EQ(call_output.origin_kind,
        frontend::macro::AurexMacroOutputContractOriginKind::matcher_to_call_binding);
    EXPECT_EQ(call_output.consumer_item.value, binding->call_item.value);
    EXPECT_EQ(call_output.macro_item.value, binding->macro_item.value);
    EXPECT_EQ(call_output.source_admission_identity, binding->binding_identity);
    EXPECT_EQ(call_output.matcher_identity, binding->matcher_identity);
    EXPECT_EQ(call_output.target_schema_identity, query::StableFingerprint128{});
    EXPECT_EQ(call_output.output_index, 0U);
    EXPECT_TRUE(call_output.compiler_owned_output);
    EXPECT_TRUE(call_output.source_map_available);
    EXPECT_TRUE(call_output.hygiene_mark_available);
    EXPECT_TRUE(call_output.diagnostic_projection_available);
    EXPECT_TRUE(call_output.declared_name_policy_available);
    EXPECT_FALSE(call_output.token_buffer_materialized);
    EXPECT_FALSE(call_output.generated_source_text);
    EXPECT_FALSE(call_output.parse_ready);
    EXPECT_FALSE(call_output.parser_consumable);
    EXPECT_FALSE(call_output.ast_mutated);
    EXPECT_FALSE(call_output.sema_visible_generated_items);
    EXPECT_FALSE(call_output.standard_library_required);
    EXPECT_FALSE(call_output.runtime_required);
    EXPECT_FALSE(call_output.external_process_required);
    EXPECT_FALSE(call_output.produced_user_generated_code);

    const frontend::macro::AurexMacroOutputContractAdmissionGate& struct_output =
        result.aurex_macro_output_contract_gates[1U];
    EXPECT_TRUE(frontend::macro::is_valid(struct_output));
    EXPECT_EQ(struct_output.origin_kind,
        frontend::macro::AurexMacroOutputContractOriginKind::user_derive_target_schema);
    EXPECT_EQ(struct_output.consumer_item.value, config_schema->target_item.value);
    EXPECT_EQ(struct_output.macro_item.value, config_schema->macro_item.value);
    EXPECT_EQ(struct_output.source_admission_identity, config_schema->schema_identity);
    EXPECT_EQ(struct_output.target_schema_identity, config_schema->schema_identity);
    EXPECT_EQ(struct_output.matcher_identity, query::StableFingerprint128{});
    EXPECT_EQ(struct_output.output_index, 1U);
    EXPECT_EQ(struct_output.consumer_name, "Config");

    const frontend::macro::AurexMacroOutputDeclaredNamePolicyAdmissionGate& declared_names =
        result.aurex_macro_output_declared_name_policy_gates.front();
    EXPECT_TRUE(frontend::macro::is_valid(declared_names));
    EXPECT_EQ(declared_names.output_contract_identity, call_output.output_contract_identity);
    EXPECT_EQ(declared_names.declared_name_policy_identity,
        call_output.declared_name_policy_identity);
    EXPECT_TRUE(declared_names.declared_name_set_reserved);
    EXPECT_FALSE(declared_names.lookup_visible);
    EXPECT_FALSE(declared_names.export_visible);
    EXPECT_FALSE(declared_names.sema_visible);
    EXPECT_FALSE(declared_names.parser_consumable);

    const frontend::macro::AurexMacroOutputDiagnosticProjectionAdmissionGate& diagnostic =
        result.aurex_macro_output_diagnostic_projection_gates.front();
    EXPECT_TRUE(frontend::macro::is_valid(diagnostic));
    EXPECT_EQ(diagnostic.output_contract_identity, call_output.output_contract_identity);
    EXPECT_EQ(diagnostic.token_buffer_identity, call_output.token_buffer_identity);
    EXPECT_EQ(diagnostic.diagnostic_anchor_identity, call_output.diagnostic_anchor_identity);
    EXPECT_TRUE(diagnostic.debug_projection_available);
    EXPECT_FALSE(diagnostic.diagnostic_emission_enabled);
    EXPECT_FALSE(diagnostic.parser_consumable);

    const std::string summary = frontend::macro::summarize_early_item_expansion(result);
    expect_contains(summary, "aurex_macro_output_contracts=3");
    expect_contains(summary, "aurex_macro_output_contract_call_bindings=1");
    expect_contains(summary, "aurex_macro_output_contract_user_derives=2");
    expect_contains(summary, "aurex_macro_output_declared_name_policies=3");
    expect_contains(summary, "aurex_macro_output_diagnostic_projections=3");

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "aurex_macro_output_contract_gate #0");
    expect_contains(dump, "origin=matcher_to_call_binding");
    expect_contains(dump, "origin=user_derive_target_schema");
    expect_contains(dump, "aurex_macro_output_declared_name_policy_gate #0");
    expect_contains(dump, "lookup_visible=no");
    expect_contains(dump, "aurex_macro_output_diagnostic_projection_gate #0");
    expect_contains(dump, "diagnostic_emission_enabled=no");
    expect_contains(dump, "parser_consumption_enabled=no");
    expect_contains(dump, "user_generated_code=no");
}

TEST(CoreUnit, OutputContractAdmissionValidationRejectsDrift)
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
    ASSERT_EQ(baseline.aurex_macro_matcher_to_call_binding_gates.size(), 1U);
    ASSERT_EQ(baseline.aurex_user_derive_target_schema_gates.size(), 1U);
    ASSERT_EQ(baseline.aurex_macro_output_contract_gates.size(), 2U);
    ASSERT_EQ(baseline.aurex_macro_output_declared_name_policy_gates.size(), 2U);
    ASSERT_EQ(baseline.aurex_macro_output_diagnostic_projection_gates.size(), 2U);

    const auto expect_invalid = [](const frontend::macro::EarlyItemExpansionResult& result) {
        EXPECT_FALSE(frontend::macro::is_valid(result));
    };

    const frontend::macro::EarlyItemExpansionResult missing_contract =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_output_contract_gates.pop_back();
        });
    EXPECT_EQ(missing_contract.summary.aurex_macro_output_contracts.contract_gate_count, 1U);
    expect_invalid(missing_contract);

    const frontend::macro::EarlyItemExpansionResult missing_declared_name_policy =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_output_declared_name_policy_gates.clear();
        });
    EXPECT_EQ(missing_declared_name_policy.summary.aurex_macro_output_contracts
                  .declared_name_policy_gate_count,
        0U);
    expect_invalid(missing_declared_name_policy);

    const frontend::macro::EarlyItemExpansionResult missing_diagnostic =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_output_diagnostic_projection_gates.clear();
        });
    EXPECT_EQ(
        missing_diagnostic.summary.aurex_macro_output_contracts.diagnostic_projection_gate_count,
        0U);
    expect_invalid(missing_diagnostic);

    const frontend::macro::EarlyItemExpansionResult parser_consumable_contract =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_output_contract_gates.front().parser_consumable = true;
        });
    EXPECT_EQ(parser_consumable_contract.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(parser_consumable_contract);

    const frontend::macro::EarlyItemExpansionResult generated_source_text =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_output_contract_gates.front().generated_source_text = true;
        });
    EXPECT_EQ(generated_source_text.summary.generated_source_text_count, 1U);
    expect_invalid(generated_source_text);

    const frontend::macro::EarlyItemExpansionResult visible_declared_name =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_output_declared_name_policy_gates.front().lookup_visible = true;
        });
    EXPECT_EQ(visible_declared_name.summary.aurex_macro_output_contracts
                  .lookup_visible_declared_name_count,
        1U);
    expect_invalid(visible_declared_name);

    const frontend::macro::EarlyItemExpansionResult sema_visible_declared_name =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_output_declared_name_policy_gates.front().sema_visible = true;
        });
    EXPECT_EQ(sema_visible_declared_name.summary.aurex_macro_output_contracts
                  .sema_visible_declared_name_count,
        1U);
    EXPECT_EQ(sema_visible_declared_name.summary.sema_visible_generated_part_count, 1U);
    expect_invalid(sema_visible_declared_name);

    const frontend::macro::EarlyItemExpansionResult emitted_diagnostic =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_output_diagnostic_projection_gates.front()
                .diagnostic_emission_enabled = true;
        });
    EXPECT_EQ(
        emitted_diagnostic.summary.aurex_macro_output_contracts.diagnostic_emission_enabled_count,
        1U);
    expect_invalid(emitted_diagnostic);

    const frontend::macro::EarlyItemExpansionResult diagnostic_parser_consumable =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_output_diagnostic_projection_gates.front().parser_consumable = true;
        });
    EXPECT_EQ(diagnostic_parser_consumable.summary.parse_ready_token_buffer_count, 1U);
    expect_invalid(diagnostic_parser_consumable);

    const frontend::macro::EarlyItemExpansionResult wrong_token_buffer =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_output_contract_gates.front().token_buffer_identity =
                query::stable_fingerprint("wrong m27d output token buffer identity");
        });
    expect_invalid(wrong_token_buffer);

    const frontend::macro::EarlyItemExpansionResult broken_contract_link =
        mutated_expansion_result(baseline, [](frontend::macro::EarlyItemExpansionResult& result) {
            result.aurex_macro_output_declared_name_policy_gates.front().output_contract_identity =
                query::stable_fingerprint("wrong m27d output contract link");
        });
    expect_invalid(broken_contract_link);
}

} // namespace aurex::test
