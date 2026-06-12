#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/macro/early_item_expansion.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/syntax/core/ast_dump.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>

#include <support/frontend_test_support.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::SourceId EARLY_ITEM_EXPANSION_TEST_SOURCE_ID{31U};
constexpr base::u8 EARLY_ITEM_EXPANSION_TEST_INVALID_DISPOSITION = 243U;
constexpr base::u8 EARLY_ITEM_EXPANSION_TEST_INVALID_LIFECYCLE_STATE = 242U;
constexpr base::u32 EARLY_ITEM_EXPANSION_TEST_GENERATED_PART_INDEX_OFFSET = 100'000U;
constexpr base::u64 EARLY_ITEM_EXPANSION_TEST_DERIVE_SENTINEL_TOKEN_COUNT = 2U;

[[nodiscard]] syntax::AstModule parse_success(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(EARLY_ITEM_EXPANSION_TEST_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        ADD_FAILURE() << tokens.error().message;
        return {};
    }

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (!parsed) {
        ADD_FAILURE() << parsed.error().message;
        return {};
    }
    if (diagnostics.has_error()) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        return {};
    }
    return parsed.take_value();
}

[[nodiscard]] const syntax::ItemNode* find_item(
    const syntax::AstModule& module, const std::string_view name) noexcept
{
    for (base::usize index = 0; index < module.items.size(); ++index) {
        const syntax::ItemNode* const item = module.items.ptr(index);
        if (item != nullptr && item->name == name) {
            return item;
        }
    }
    return nullptr;
}

void assign_single_module_ownership(syntax::AstModule& module)
{
    if (module.modules.empty()) {
        syntax::ModuleInfo module_info;
        module_info.path = module.module_path;
        module.modules.push_back(std::move(module_info));
    }
    module.item_modules.assign(module.items.size(), syntax::ModuleId{0U});
    module.item_part_indices.assign(module.items.size(), 0U);
}

[[nodiscard]] query::PackageKey package_key()
{
    const std::array<std::string_view, 1U> identity{"early-item-expansion-test-package"};
    return query::package_key(identity);
}

[[nodiscard]] query::ModuleKey module_key(const query::PackageKey package)
{
    const std::array<std::string_view, 2U> path{"macro", "early_item_expansion"};
    return query::module_key(package, path);
}

[[nodiscard]] query::ModulePartKey module_part_key_for_index(const base::u32 part_index)
{
    const query::PackageKey package = package_key();
    const query::ModuleKey module = module_key(package);
    if (part_index == 0U) {
        const query::FileKey file = query::file_key(package, "/virtual/tests/macro/early_item_expansion.ax");
        return query::module_part_key(module, file, query::ModulePartKind::primary, "<primary>");
    }
    const std::string part_name = "part" + std::to_string(part_index);
    const std::string source_path = "/virtual/tests/macro/early_item_expansion.parts/" + part_name + ".ax";
    const query::FileKey file = query::file_key(package, source_path);
    return query::module_part_key(module, file, query::ModulePartKind::fragment, part_name, part_index);
}

[[nodiscard]] query::ModulePartKey primary_part_key()
{
    return module_part_key_for_index(0U);
}

[[nodiscard]] std::vector<std::vector<query::ModulePartKey>> part_key_table(const base::u32 part_count)
{
    std::vector<std::vector<query::ModulePartKey>> keys(1U);
    keys.front().reserve(part_count);
    for (base::u32 part_index = 0U; part_index < part_count; ++part_index) {
        keys.front().push_back(module_part_key_for_index(part_index));
    }
    return keys;
}

[[nodiscard]] std::vector<std::vector<query::ModulePartKey>> single_part_key_table()
{
    return {{primary_part_key()}};
}

[[nodiscard]] frontend::macro::EarlyItemExpansionResult expand_source(const std::string_view source)
{
    syntax::AstModule module = parse_success(source);
    assign_single_module_ownership(module);
    std::vector<std::vector<query::ModulePartKey>> part_keys = single_part_key_table();
    auto expanded = frontend::macro::expand_early_item_macros_noop(module, part_keys);
    if (!expanded) {
        ADD_FAILURE() << expanded.error().message;
        return {};
    }
    return expanded.take_value();
}

[[nodiscard]] const frontend::macro::EarlyItemMacroInput* input_by_attribute(
    const frontend::macro::EarlyItemExpansionResult& result,
    const std::string_view attribute_name) noexcept
{
    const auto found = std::find_if(result.inputs.begin(), result.inputs.end(),
        [attribute_name](const frontend::macro::EarlyItemMacroInput& input) {
            return input.attribute_name == attribute_name;
        });
    return found == result.inputs.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::ExpansionHygieneStub* hygiene_stub_for_input(
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

[[nodiscard]] const frontend::macro::ExpansionTraceStub* trace_stub_for_input(
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

[[nodiscard]] const frontend::macro::GeneratedItemDeclarationStub* generated_item_declaration_for_input(
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

[[nodiscard]] const frontend::macro::DeclaredGeneratedNameStub* declared_generated_name_for_input(
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

[[nodiscard]] const frontend::macro::TokenMaterializationAdmissionStub* token_admission_for_input(
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

[[nodiscard]] const frontend::macro::GeneratedTokenBufferStub* token_buffer_for_input(
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

[[nodiscard]] const frontend::macro::GeneratedTokenParserAdmissionGateStub* parser_admission_gate_for_input(
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

[[nodiscard]] const frontend::macro::ParserAdmissionDiagnosticProjectionStub* parser_admission_diagnostic_for_input(
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

[[nodiscard]] const frontend::macro::ParserAdmissionDiagnosticReportEntry*
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

[[nodiscard]] const frontend::macro::ParserAdmissionDiagnosticReport*
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

[[nodiscard]] const frontend::macro::GeneratedTokenParserReadinessPreflightEntry*
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

[[nodiscard]] const frontend::macro::GeneratedTokenParserConsumptionContractGate*
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

[[nodiscard]] const frontend::macro::BuiltinDeriveExpansionAdmissionGate*
builtin_derive_admission_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.builtin_derive_expansion_admissions.begin(),
        result.builtin_derive_expansion_admissions.end(),
        [&input](const frontend::macro::BuiltinDeriveExpansionAdmissionGate& gate) {
            return gate.item.value == input.item.value
                && gate.module.value == input.module.value
                && gate.attribute_index == input.attribute_index;
        });
    return found == result.builtin_derive_expansion_admissions.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::BuiltinDeriveSemanticExpansionPlan*
builtin_derive_semantic_plan_for_input(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::EarlyItemMacroInput& input) noexcept
{
    const auto found = std::find_if(result.builtin_derive_semantic_plans.begin(),
        result.builtin_derive_semantic_plans.end(),
        [&input](const frontend::macro::BuiltinDeriveSemanticExpansionPlan& plan) {
            return plan.item.value == input.item.value
                && plan.module.value == input.module.value
                && plan.attribute_index == input.attribute_index;
        });
    return found == result.builtin_derive_semantic_plans.end() ? nullptr : &*found;
}

[[nodiscard]] const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate*
builtin_derive_parser_release_gate_for_part(
    const frontend::macro::EarlyItemExpansionResult& result,
    const frontend::macro::GeneratedModulePartPlaceholder& generated_part) noexcept
{
    const auto found = std::find_if(result.builtin_derive_parser_release_gates.begin(),
        result.builtin_derive_parser_release_gates.end(),
        [&generated_part](const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate& gate) {
            return gate.module.value == generated_part.module.value
                && gate.source_part_index == generated_part.source_part_index;
        });
    return found == result.builtin_derive_parser_release_gates.end() ? nullptr : &*found;
}

[[nodiscard]] std::vector<const frontend::macro::GeneratedTokenRecord*> token_records_for_input(
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

[[nodiscard]] base::usize first_record_index_for_attribute(
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

void refresh_expansion_result(frontend::macro::EarlyItemExpansionResult& result)
{
    result.summary = frontend::macro::summarize_early_item_expansion_counts(result);
    result.fingerprint = frontend::macro::early_item_expansion_fingerprint(result);
}

} // namespace

TEST(CoreUnit, EarlyItemExpansionDispositionNamesExposeInvalidFallback)
{
    using frontend::macro::EarlyItemExpansionDisposition;

    EXPECT_EQ(frontend::macro::early_item_expansion_disposition_name(
                  EarlyItemExpansionDisposition::builtin_derive_passthrough),
        "builtin_derive_passthrough");
    EXPECT_EQ(frontend::macro::early_item_expansion_disposition_name(
                  EarlyItemExpansionDisposition::blocked_unimplemented_attribute),
        "blocked_unimplemented_attribute");
    EXPECT_EQ(frontend::macro::early_item_expansion_disposition_name(
                  static_cast<EarlyItemExpansionDisposition>(EARLY_ITEM_EXPANSION_TEST_INVALID_DISPOSITION)),
        "invalid");
    EXPECT_TRUE(frontend::macro::is_valid(EarlyItemExpansionDisposition::builtin_derive_passthrough));
    EXPECT_FALSE(frontend::macro::is_valid(
        static_cast<EarlyItemExpansionDisposition>(EARLY_ITEM_EXPANSION_TEST_INVALID_DISPOSITION)));
}

TEST(CoreUnit, EarlyItemExpansionLifecycleNamesExposeInvalidFallback)
{
    using frontend::macro::GeneratedModulePartLifecycleState;

    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  GeneratedModulePartLifecycleState::planned),
        "planned");
    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  GeneratedModulePartLifecycleState::materialized_buffer_stub),
        "materialized_buffer_stub");
    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  GeneratedModulePartLifecycleState::parse_blocked),
        "parse_blocked");
    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  GeneratedModulePartLifecycleState::merge_blocked),
        "merge_blocked");
    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  static_cast<GeneratedModulePartLifecycleState>(
                      EARLY_ITEM_EXPANSION_TEST_INVALID_LIFECYCLE_STATE)),
        "invalid");
    EXPECT_TRUE(frontend::macro::is_valid(GeneratedModulePartLifecycleState::planned));
    EXPECT_TRUE(frontend::macro::is_valid(GeneratedModulePartLifecycleState::merge_blocked));
    EXPECT_FALSE(frontend::macro::is_valid(
        static_cast<GeneratedModulePartLifecycleState>(EARLY_ITEM_EXPANSION_TEST_INVALID_LIFECYCLE_STATE)));
}

TEST(CoreUnit, EarlyItemExpansionNoopCollectsAttributeInputsAndPlaceholders)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(defaults(threads = 4), flag, nested[a + b])]\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n"
        "fn main() -> i32 { return 0; }\n";

    syntax::AstModule module = parse_success(source);
    assign_single_module_ownership(module);
    const syntax::ItemNode* const config = find_item(module, "Config");
    ASSERT_NE(config, nullptr);
    ASSERT_EQ(config->attributes.size(), 2U);

    std::vector<std::vector<query::ModulePartKey>> part_keys = single_part_key_table();
    auto expanded = frontend::macro::expand_early_item_macros_noop(module, part_keys);
    ASSERT_TRUE(expanded) << expanded.error().message;
    const frontend::macro::EarlyItemExpansionResult result = expanded.take_value();

    EXPECT_EQ(result.name, "M22c Builtin Derive Parser Consumption Release Gate");
    EXPECT_TRUE(frontend::macro::is_valid(result));
    EXPECT_EQ(result.fingerprint, frontend::macro::early_item_expansion_fingerprint(result));
    EXPECT_EQ(result.summary.macro_input_count, 2U);
    EXPECT_EQ(result.summary.attribute_input_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_passthrough_count, 1U);
    EXPECT_EQ(result.summary.blocked_attribute_count, 1U);
    EXPECT_EQ(result.summary.generated_part_placeholder_count, 1U);
    EXPECT_EQ(result.summary.generated_part_stub_count, 1U);
    EXPECT_EQ(result.summary.materialized_buffer_stub_count, 1U);
    EXPECT_EQ(result.summary.parse_blocked_count, 1U);
    EXPECT_EQ(result.summary.merge_blocked_count, 1U);
    EXPECT_EQ(result.summary.sema_visible_generated_part_count, 0U);
    EXPECT_EQ(result.summary.source_map_placeholder_count, 2U);
    EXPECT_EQ(result.summary.hygiene_stub_count, 2U);
    EXPECT_EQ(result.summary.unresolved_hygiene_stub_count, 2U);
    EXPECT_EQ(result.summary.declared_name_stub_count, 2U);
    EXPECT_EQ(result.summary.call_site_capture_count, 0U);
    EXPECT_EQ(result.summary.trace_stub_count, 2U);
    EXPECT_EQ(result.summary.real_source_map_count, 0U);
    EXPECT_EQ(result.summary.debug_trace_available_count, 0U);
    EXPECT_EQ(result.summary.cli_emit_expanded_available_count, 0U);
    EXPECT_EQ(result.summary.generated_item_declaration_stub_count, 2U);
    EXPECT_EQ(result.summary.planned_generated_item_declaration_count, 2U);
    EXPECT_EQ(result.summary.materialized_generated_item_count, 0U);
    EXPECT_EQ(result.summary.declared_generated_name_stub_count, 2U);
    EXPECT_EQ(result.summary.lookup_visible_declared_name_count, 0U);
    EXPECT_EQ(result.summary.export_visible_declared_name_count, 0U);
    EXPECT_EQ(result.summary.token_materialization_admission_stub_count, 2U);
    EXPECT_EQ(result.summary.compiler_owned_admission_count, 2U);
    EXPECT_EQ(result.summary.admitted_token_materialization_count, 2U);
    EXPECT_EQ(result.summary.materialized_token_admission_count, 1U);
    EXPECT_EQ(result.summary.generated_token_buffer_stub_count, 2U);
    EXPECT_EQ(result.summary.empty_generated_token_buffer_count, 1U);
    EXPECT_EQ(result.summary.materialized_token_buffer_count, 1U);
    EXPECT_EQ(result.summary.compiler_owned_token_buffer_count, 2U);
    EXPECT_EQ(result.summary.generated_token_record_count,
        config->attributes[1].token_tree.size() + EARLY_ITEM_EXPANSION_TEST_DERIVE_SENTINEL_TOKEN_COUNT);
    EXPECT_EQ(result.summary.compiler_owned_generated_token_record_count,
        config->attributes[1].token_tree.size() + EARLY_ITEM_EXPANSION_TEST_DERIVE_SENTINEL_TOKEN_COUNT);
    EXPECT_EQ(result.summary.parser_visible_generated_token_count, 0U);
    EXPECT_EQ(result.summary.parser_admission_gate_stub_count, 2U);
    EXPECT_EQ(result.summary.compiler_owned_parser_admission_gate_count, 2U);
    EXPECT_EQ(result.summary.token_record_available_gate_count, 1U);
    EXPECT_EQ(result.summary.parser_blocked_token_buffer_count, 2U);
    EXPECT_EQ(result.summary.parser_admitted_token_buffer_count, 0U);
    EXPECT_EQ(result.summary.parser_admission_diagnostic_stub_count, 2U);
    EXPECT_EQ(result.summary.parser_admission_diagnostic_blocked_count, 2U);
    EXPECT_EQ(result.summary.derive_parser_admission_diagnostic_count, 1U);
    EXPECT_EQ(result.summary.empty_parser_admission_diagnostic_count, 1U);
    EXPECT_EQ(result.summary.emit_expanded_projection_available_count, 0U);
    EXPECT_EQ(result.summary.parser_admission_debug_trace_projection_count, 0U);
    EXPECT_EQ(result.summary.parser_admission_source_map_projection_count, 0U);
    EXPECT_EQ(result.summary.parser_admission_report_entry_count, 2U);
    EXPECT_EQ(result.summary.parser_admission_report_count, 1U);
    EXPECT_EQ(result.summary.parser_admission_report_blocked_entry_count, 2U);
    EXPECT_EQ(result.summary.parser_admission_report_derive_entry_count, 1U);
    EXPECT_EQ(result.summary.parser_admission_report_empty_entry_count, 1U);
    EXPECT_EQ(result.summary.parser_admission_report_token_record_available_entry_count, 1U);
    EXPECT_EQ(result.summary.parser_admission_report_visible_count, 1U);
    EXPECT_EQ(result.summary.parser_admission_report_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.parser_admission_report_unordered_anchor_count, 0U);
    EXPECT_EQ(result.summary.parser_admission_report_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_entry_count, 2U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_blocked_count, 2U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_derive_entry_count, 1U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_empty_entry_count, 1U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_contiguous_index_count, 2U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_delimiter_balanced_count, 2U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_source_anchor_covered_count, 2U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_parse_config_compatible_count, 2U);
    EXPECT_EQ(result.summary.parser_readiness_preflight_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.parser_consumption_contract_gate_count, 1U);
    EXPECT_EQ(result.summary.parser_consumption_contract_blocked_gate_count, 1U);
    EXPECT_EQ(result.summary.parser_consumption_contract_visible_count, 1U);
    EXPECT_EQ(result.summary.parser_consumption_contract_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.parser_consumption_contract_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.macro_boundary_closure_report_count, 1U);
    EXPECT_EQ(result.summary.macro_boundary_closure_visible_count, 1U);
    EXPECT_EQ(result.summary.macro_boundary_closure_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.macro_boundary_closure_complete_count, 1U);
    EXPECT_EQ(result.summary.macro_boundary_closure_parser_consumption_enabled_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_admission_gate_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_derive_admission_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_non_derive_blocked_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_visible_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_query_reusable_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_capability_candidate_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_plan_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_plan_visible_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_plan_query_reusable_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_capability_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_copy_capability_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_eq_capability_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_hash_capability_count, 0U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_gate_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_visible_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_query_reusable_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_parser_consumable_count, 0U);
    EXPECT_EQ(result.summary.generated_source_text_count, 0U);
    EXPECT_EQ(result.summary.parse_ready_token_buffer_count, 0U);
    EXPECT_EQ(result.summary.parsed_generated_part_count, 0U);
    EXPECT_EQ(result.summary.merged_generated_part_count, 0U);
    EXPECT_EQ(result.summary.user_generated_code_count, 0U);
    EXPECT_EQ(result.summary.standard_library_required_count, 0U);
    EXPECT_EQ(result.summary.runtime_required_count, 0U);
    EXPECT_EQ(result.summary.external_process_required_count, 0U);

    const frontend::macro::EarlyItemMacroInput* const builder = input_by_attribute(result, "builder");
    ASSERT_NE(builder, nullptr);
    EXPECT_EQ(builder->attribute_index, 0U);
    EXPECT_EQ(builder->part_index, 0U);
    EXPECT_EQ(builder->token_count, config->attributes[0].token_tree.size());
    EXPECT_EQ(builder->disposition,
        frontend::macro::EarlyItemExpansionDisposition::blocked_unimplemented_attribute);
    EXPECT_EQ(builder->attached_part, part_keys[0][0]);
    EXPECT_GT(builder->token_tree_fingerprint.byte_count, 0U);
    EXPECT_GT(builder->query_key_fingerprint.byte_count, 0U);

    const frontend::macro::EarlyItemMacroInput* const derive = input_by_attribute(result, "derive");
    ASSERT_NE(derive, nullptr);
    EXPECT_EQ(derive->attribute_index, 1U);
    EXPECT_EQ(derive->disposition,
        frontend::macro::EarlyItemExpansionDisposition::builtin_derive_passthrough);
    EXPECT_EQ(derive->token_count, config->attributes[1].token_tree.size());
    EXPECT_NE(derive->query_key_fingerprint, builder->query_key_fingerprint);

    ASSERT_EQ(result.generated_parts.size(), 1U);
    const frontend::macro::GeneratedModulePartPlaceholder& generated = result.generated_parts.front();
    EXPECT_EQ(generated.module.value, 0U);
    EXPECT_EQ(generated.source_part_index, 0U);
    EXPECT_EQ(generated.generated_stable_index, EARLY_ITEM_EXPANSION_TEST_GENERATED_PART_INDEX_OFFSET);
    EXPECT_EQ(generated.source_role, query::SourceRole::generated);
    EXPECT_EQ(generated.part_kind, query::ModulePartKind::generated);
    EXPECT_EQ(generated.source_part, part_keys[0][0]);
    EXPECT_EQ(generated.generated_part.kind, query::ModulePartKind::generated);
    EXPECT_EQ(generated.generated_part.file.role, query::SourceRole::generated);
    EXPECT_NE(generated.generated_part, generated.source_part);
    EXPECT_FALSE(generated.parsed);
    EXPECT_FALSE(generated.merged);
    EXPECT_FALSE(generated.produced_user_generated_code);

    ASSERT_EQ(result.generated_part_stubs.size(), 1U);
    const frontend::macro::GeneratedModulePartParseMergeStub& stub =
        result.generated_part_stubs.front();
    EXPECT_TRUE(frontend::macro::is_valid(stub));
    EXPECT_EQ(stub.module.value, generated.module.value);
    EXPECT_EQ(stub.source_part_index, generated.source_part_index);
    EXPECT_EQ(stub.generated_stable_index, generated.generated_stable_index);
    EXPECT_EQ(stub.source_part, generated.source_part);
    EXPECT_EQ(stub.generated_part, generated.generated_part);
    EXPECT_GT(stub.generated_buffer_identity.byte_count, 0U);
    EXPECT_GT(stub.parse_config_fingerprint.byte_count, 0U);
    EXPECT_GT(stub.merge_ordering_key.byte_count, 0U);
    EXPECT_EQ(stub.expansion_origin, generated.output_fingerprint);
    expect_contains(stub.generated_buffer_name, "m21e-noop-generated-buffer:");
    expect_contains(stub.blocker_reason, "parse and merge are blocked in M21e");
    EXPECT_EQ(stub.lifecycle_state,
        frontend::macro::GeneratedModulePartLifecycleState::merge_blocked);
    EXPECT_TRUE(stub.materialized_buffer);
    EXPECT_FALSE(stub.parsed);
    EXPECT_FALSE(stub.merged);
    EXPECT_FALSE(stub.sema_visible);
    EXPECT_FALSE(stub.produced_user_generated_code);

    ASSERT_EQ(result.source_maps.size(), 2U);
    for (const frontend::macro::ExpansionSourceMapPlaceholder& source_map : result.source_maps) {
        EXPECT_FALSE(source_map.real_source_map);
        EXPECT_FALSE(source_map.debug_trace_available);
        EXPECT_GT(source_map.expansion_origin.byte_count, 0U);
    }
    EXPECT_EQ(result.source_maps[0].expansion_origin, builder->query_key_fingerprint);
    EXPECT_EQ(result.source_maps[1].expansion_origin, derive->query_key_fingerprint);

    ASSERT_EQ(result.hygiene_stubs.size(), 2U);
    const frontend::macro::ExpansionHygieneStub* const builder_hygiene =
        hygiene_stub_for_input(result, *builder);
    ASSERT_NE(builder_hygiene, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_hygiene));
    EXPECT_EQ(builder_hygiene->part_index, builder->part_index);
    EXPECT_EQ(builder_hygiene->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_hygiene->attached_part, builder->attached_part);
    EXPECT_EQ(builder_hygiene->expansion_origin, builder->query_key_fingerprint);
    EXPECT_GT(builder_hygiene->call_site_mark.byte_count, 0U);
    EXPECT_GT(builder_hygiene->definition_site_mark.byte_count, 0U);
    EXPECT_GT(builder_hygiene->generated_fresh_mark.byte_count, 0U);
    EXPECT_GT(builder_hygiene->declared_name_set.byte_count, 0U);
    EXPECT_NE(builder_hygiene->call_site_mark, builder_hygiene->definition_site_mark);
    EXPECT_NE(builder_hygiene->definition_site_mark, builder_hygiene->generated_fresh_mark);
    EXPECT_EQ(builder_hygiene->policy, "origin_mark_hygiene_v1");
    EXPECT_FALSE(builder_hygiene->resolved);
    EXPECT_FALSE(builder_hygiene->declared_names_visible);
    EXPECT_FALSE(builder_hygiene->captures_call_site_locals);

    ASSERT_EQ(result.trace_stubs.size(), 2U);
    const frontend::macro::ExpansionTraceStub* const builder_trace =
        trace_stub_for_input(result, *builder);
    ASSERT_NE(builder_trace, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_trace));
    EXPECT_EQ(builder_trace->part_index, builder->part_index);
    EXPECT_EQ(builder_trace->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_trace->attached_part, builder->attached_part);
    EXPECT_EQ(builder_trace->attribute_range.source.value, builder->attribute_range.source.value);
    EXPECT_EQ(builder_trace->attribute_range.begin, builder->attribute_range.begin);
    EXPECT_EQ(builder_trace->attribute_range.end, builder->attribute_range.end);
    EXPECT_EQ(builder_trace->token_tree_range.source.value, builder->token_tree_range.source.value);
    EXPECT_EQ(builder_trace->token_tree_range.begin, builder->token_tree_range.begin);
    EXPECT_EQ(builder_trace->token_tree_range.end, builder->token_tree_range.end);
    EXPECT_EQ(builder_trace->expansion_origin, builder->query_key_fingerprint);
    EXPECT_GT(builder_trace->trace_identity.byte_count, 0U);
    EXPECT_GT(builder_trace->generated_source_map_identity.byte_count, 0U);
    EXPECT_GT(builder_trace->diagnostic_anchor.byte_count, 0U);
    EXPECT_NE(builder_trace->trace_identity, builder_trace->generated_source_map_identity);
    EXPECT_EQ(builder_trace->trace_policy, "expansion_source_map_debug_trace_v1");
    expect_contains(builder_trace->blocker_reason, "blocked in M21f");
    EXPECT_FALSE(builder_trace->real_source_map);
    EXPECT_FALSE(builder_trace->debug_trace_available);
    EXPECT_FALSE(builder_trace->cli_emit_expanded_available);

    ASSERT_EQ(result.generated_item_declarations.size(), 2U);
    const frontend::macro::GeneratedItemDeclarationStub* const builder_declaration =
        generated_item_declaration_for_input(result, *builder);
    ASSERT_NE(builder_declaration, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_declaration));
    EXPECT_EQ(builder_declaration->part_index, builder->part_index);
    EXPECT_EQ(builder_declaration->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_declaration->attached_part, builder->attached_part);
    EXPECT_EQ(builder_declaration->generated_part, generated.generated_part);
    EXPECT_EQ(builder_declaration->expansion_origin, builder->query_key_fingerprint);
    EXPECT_EQ(builder_declaration->declared_name_set, builder_hygiene->declared_name_set);
    EXPECT_GT(builder_declaration->declaration_identity.byte_count, 0U);
    EXPECT_GT(builder_declaration->generated_item_key.byte_count, 0U);
    EXPECT_NE(builder_declaration->declaration_identity, builder_declaration->generated_item_key);
    EXPECT_EQ(builder_declaration->declaration_role, "attached_item_codegen_declared_names_v1");
    expect_contains(builder_declaration->generated_item_name, "__aurex_macro_declared:0:0:0:0:builder");
    expect_contains(builder_declaration->blocker_reason, "blocked in M21g");
    EXPECT_TRUE(builder_declaration->planned);
    EXPECT_FALSE(builder_declaration->materialized_tokens);
    EXPECT_FALSE(builder_declaration->parsed);
    EXPECT_FALSE(builder_declaration->merged);
    EXPECT_FALSE(builder_declaration->sema_visible);
    EXPECT_FALSE(builder_declaration->produced_user_generated_code);

    const frontend::macro::GeneratedItemDeclarationStub* const derive_declaration =
        generated_item_declaration_for_input(result, *derive);
    ASSERT_NE(derive_declaration, nullptr);
    EXPECT_NE(derive_declaration->generated_item_name, builder_declaration->generated_item_name);
    expect_contains(derive_declaration->generated_item_name, "__aurex_macro_declared:0:0:0:1:derive");

    ASSERT_EQ(result.declared_generated_names.size(), 2U);
    const frontend::macro::DeclaredGeneratedNameStub* const builder_declared_name =
        declared_generated_name_for_input(result, *builder);
    ASSERT_NE(builder_declared_name, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_declared_name));
    EXPECT_EQ(builder_declared_name->part_index, builder->part_index);
    EXPECT_EQ(builder_declared_name->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_declared_name->attached_part, builder->attached_part);
    EXPECT_EQ(builder_declared_name->generated_part, generated.generated_part);
    EXPECT_EQ(builder_declared_name->expansion_origin, builder->query_key_fingerprint);
    EXPECT_EQ(builder_declared_name->declared_name_set, builder_hygiene->declared_name_set);
    EXPECT_EQ(builder_declared_name->hygiene_mark, builder_hygiene->generated_fresh_mark);
    EXPECT_EQ(builder_declared_name->declared_name, builder_declaration->generated_item_name);
    EXPECT_EQ(builder_declared_name->namespace_kind, "item");
    EXPECT_GT(builder_declared_name->declared_name_identity.byte_count, 0U);
    expect_contains(builder_declared_name->blocker_reason, "lookup is blocked in M21g");
    EXPECT_FALSE(builder_declared_name->lookup_visible);
    EXPECT_FALSE(builder_declared_name->export_visible);
    EXPECT_FALSE(builder_declared_name->sema_visible);
    EXPECT_FALSE(builder_declared_name->produced_user_generated_code);

    ASSERT_EQ(result.token_materialization_admissions.size(), 2U);
    const frontend::macro::TokenMaterializationAdmissionStub* const builder_admission =
        token_admission_for_input(result, *builder);
    ASSERT_NE(builder_admission, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_admission));
    EXPECT_EQ(builder_admission->part_index, builder->part_index);
    EXPECT_EQ(builder_admission->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_admission->attached_part, builder->attached_part);
    EXPECT_EQ(builder_admission->generated_part, generated.generated_part);
    EXPECT_EQ(builder_admission->expansion_origin, builder->query_key_fingerprint);
    EXPECT_EQ(builder_admission->declaration_identity, builder_declaration->declaration_identity);
    EXPECT_EQ(builder_admission->generated_item_key, builder_declaration->generated_item_key);
    EXPECT_EQ(builder_admission->declared_name_set, builder_hygiene->declared_name_set);
    EXPECT_EQ(builder_admission->declared_name_identity, builder_declared_name->declared_name_identity);
    EXPECT_EQ(builder_admission->hygiene_mark, builder_hygiene->generated_fresh_mark);
    EXPECT_EQ(builder_admission->source_map_identity, builder_trace->generated_source_map_identity);
    EXPECT_EQ(builder_admission->trace_identity, builder_trace->trace_identity);
    EXPECT_GT(builder_admission->token_plan_identity.byte_count, 0U);
    EXPECT_GT(builder_admission->token_buffer_identity.byte_count, 0U);
    EXPECT_NE(builder_admission->token_plan_identity, builder_admission->token_buffer_identity);
    EXPECT_EQ(builder_admission->admission_policy,
        "compiler_owned_attached_item_token_materialization_admission_v1");
    expect_contains(builder_admission->token_stream_name, "m21h-token-stream:0:0:0:0:builder");
    expect_contains(builder_admission->blocker_reason, "non-derive item attribute token materialization remains blocked in M21i");
    EXPECT_TRUE(builder_admission->compiler_owned);
    EXPECT_TRUE(builder_admission->admitted);
    EXPECT_FALSE(builder_admission->materialized_tokens);
    EXPECT_FALSE(builder_admission->generated_source_text);
    EXPECT_FALSE(builder_admission->parse_ready);
    EXPECT_FALSE(builder_admission->external_process_required);
    EXPECT_FALSE(builder_admission->standard_library_required);
    EXPECT_FALSE(builder_admission->runtime_required);
    EXPECT_FALSE(builder_admission->produced_user_generated_code);

    const frontend::macro::TokenMaterializationAdmissionStub* const derive_admission =
        token_admission_for_input(result, *derive);
    ASSERT_NE(derive_admission, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_admission));
    EXPECT_NE(derive_admission->token_stream_name, builder_admission->token_stream_name);
    expect_contains(derive_admission->token_stream_name, "m21h-token-stream:0:0:0:1:derive");
    expect_contains(derive_admission->blocker_reason, "derive token prototype remains parser-blocked in M21i");
    EXPECT_TRUE(derive_admission->compiler_owned);
    EXPECT_TRUE(derive_admission->admitted);
    EXPECT_TRUE(derive_admission->materialized_tokens);
    EXPECT_FALSE(derive_admission->generated_source_text);
    EXPECT_FALSE(derive_admission->parse_ready);
    EXPECT_FALSE(derive_admission->external_process_required);
    EXPECT_FALSE(derive_admission->standard_library_required);
    EXPECT_FALSE(derive_admission->runtime_required);
    EXPECT_FALSE(derive_admission->produced_user_generated_code);

    ASSERT_EQ(result.generated_token_buffers.size(), 2U);
    const frontend::macro::GeneratedTokenBufferStub* const builder_buffer =
        token_buffer_for_input(result, *builder);
    ASSERT_NE(builder_buffer, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_buffer));
    EXPECT_EQ(builder_buffer->part_index, builder->part_index);
    EXPECT_EQ(builder_buffer->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_buffer->attached_part, builder->attached_part);
    EXPECT_EQ(builder_buffer->generated_part, generated.generated_part);
    EXPECT_EQ(builder_buffer->token_plan_identity, builder_admission->token_plan_identity);
    EXPECT_EQ(builder_buffer->token_buffer_identity, builder_admission->token_buffer_identity);
    EXPECT_EQ(builder_buffer->source_map_identity, builder_trace->generated_source_map_identity);
    EXPECT_EQ(builder_buffer->hygiene_mark, builder_hygiene->generated_fresh_mark);
    EXPECT_EQ(builder_buffer->token_stream_name, builder_admission->token_stream_name);
    EXPECT_EQ(builder_buffer->token_buffer_kind, "compiler_owned_empty_token_stream");
    EXPECT_EQ(builder_buffer->token_producer_policy, "compiler_owned_blocked_empty_token_producer_v1");
    EXPECT_GT(builder_buffer->materialization_identity.byte_count, 0U);
    expect_contains(builder_buffer->blocker_reason, "empty and parser-blocked in M21i");
    EXPECT_EQ(builder_buffer->token_count, 0U);
    EXPECT_TRUE(builder_buffer->empty);
    EXPECT_FALSE(builder_buffer->materialized_tokens);
    EXPECT_FALSE(builder_buffer->generated_source_text);
    EXPECT_FALSE(builder_buffer->parser_consumable);
    EXPECT_FALSE(builder_buffer->produced_user_generated_code);

    const frontend::macro::GeneratedTokenBufferStub* const derive_buffer =
        token_buffer_for_input(result, *derive);
    ASSERT_NE(derive_buffer, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_buffer));
    EXPECT_EQ(derive_buffer->part_index, derive->part_index);
    EXPECT_EQ(derive_buffer->attribute_index, derive->attribute_index);
    EXPECT_EQ(derive_buffer->attached_part, derive->attached_part);
    EXPECT_EQ(derive_buffer->generated_part, generated.generated_part);
    EXPECT_EQ(derive_buffer->token_plan_identity, derive_admission->token_plan_identity);
    EXPECT_EQ(derive_buffer->token_buffer_identity, derive_admission->token_buffer_identity);
    EXPECT_GT(derive_buffer->materialization_identity.byte_count, 0U);
    EXPECT_EQ(derive_buffer->source_map_identity, result.trace_stubs[1].generated_source_map_identity);
    EXPECT_EQ(derive_buffer->token_stream_name, derive_admission->token_stream_name);
    EXPECT_EQ(derive_buffer->token_buffer_kind, "compiler_owned_builtin_derive_token_stream_prototype");
    EXPECT_EQ(derive_buffer->token_producer_policy,
        "compiler_owned_builtin_derive_token_producer_prototype_v1");
    expect_contains(derive_buffer->blocker_reason, "generated token buffer remains parser-blocked in M21i");
    EXPECT_EQ(derive_buffer->token_count,
        derive->token_count + EARLY_ITEM_EXPANSION_TEST_DERIVE_SENTINEL_TOKEN_COUNT);
    EXPECT_FALSE(derive_buffer->empty);
    EXPECT_TRUE(derive_buffer->materialized_tokens);
    EXPECT_FALSE(derive_buffer->generated_source_text);
    EXPECT_FALSE(derive_buffer->parser_consumable);
    EXPECT_FALSE(derive_buffer->produced_user_generated_code);

    ASSERT_EQ(result.parser_admission_gates.size(), 2U);
    ASSERT_EQ(result.parser_admission_diagnostics.size(), 2U);
    ASSERT_EQ(result.parser_admission_report_entries.size(), 2U);
    ASSERT_EQ(result.parser_admission_reports.size(), 1U);
    ASSERT_EQ(result.builtin_derive_expansion_admissions.size(), 2U);
    ASSERT_EQ(result.builtin_derive_semantic_plans.size(), 2U);
    ASSERT_EQ(result.builtin_derive_parser_release_gates.size(), 1U);
    ASSERT_EQ(result.parser_readiness_preflight_entries.size(), 2U);
    ASSERT_EQ(result.parser_consumption_contract_gates.size(), 1U);
    ASSERT_EQ(result.macro_boundary_closure_reports.size(), 1U);
    const frontend::macro::GeneratedTokenParserAdmissionGateStub* const builder_gate =
        parser_admission_gate_for_input(result, *builder);
    ASSERT_NE(builder_gate, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_gate));
    EXPECT_EQ(builder_gate->part_index, builder->part_index);
    EXPECT_EQ(builder_gate->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_gate->attached_part, builder->attached_part);
    EXPECT_EQ(builder_gate->generated_part, generated.generated_part);
    EXPECT_EQ(builder_gate->token_plan_identity, builder_buffer->token_plan_identity);
    EXPECT_EQ(builder_gate->token_buffer_identity, builder_buffer->token_buffer_identity);
    EXPECT_EQ(builder_gate->materialization_identity, builder_buffer->materialization_identity);
    EXPECT_EQ(builder_gate->source_map_identity, builder_buffer->source_map_identity);
    EXPECT_EQ(builder_gate->hygiene_mark, builder_buffer->hygiene_mark);
    EXPECT_EQ(builder_gate->generated_buffer_identity, result.generated_part_stubs.front().generated_buffer_identity);
    EXPECT_EQ(builder_gate->parse_config_fingerprint, result.generated_part_stubs.front().parse_config_fingerprint);
    EXPECT_GT(builder_gate->parse_gate_identity.byte_count, 0U);
    EXPECT_EQ(builder_gate->token_stream_name, builder_buffer->token_stream_name);
    EXPECT_EQ(builder_gate->parser_gate_policy,
        "compiler_owned_generated_token_parser_admission_gate_v1");
    expect_contains(builder_gate->blocker_reason,
        "empty or non-derive generated token buffer parser admission remains blocked in M21j");
    EXPECT_EQ(builder_gate->token_count, 0U);
    EXPECT_TRUE(builder_gate->compiler_owned);
    EXPECT_FALSE(builder_gate->token_buffer_materialized);
    EXPECT_FALSE(builder_gate->token_records_available);
    EXPECT_FALSE(builder_gate->parser_admitted);
    EXPECT_FALSE(builder_gate->parse_ready);
    EXPECT_FALSE(builder_gate->parser_consumable);
    EXPECT_FALSE(builder_gate->generated_source_text);
    EXPECT_FALSE(builder_gate->generated_part_parsed);
    EXPECT_FALSE(builder_gate->generated_part_merged);
    EXPECT_FALSE(builder_gate->sema_visible);
    EXPECT_FALSE(builder_gate->produced_user_generated_code);

    const frontend::macro::GeneratedTokenParserAdmissionGateStub* const derive_gate =
        parser_admission_gate_for_input(result, *derive);
    ASSERT_NE(derive_gate, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_gate));
    EXPECT_EQ(derive_gate->token_plan_identity, derive_buffer->token_plan_identity);
    EXPECT_EQ(derive_gate->token_buffer_identity, derive_buffer->token_buffer_identity);
    EXPECT_EQ(derive_gate->materialization_identity, derive_buffer->materialization_identity);
    EXPECT_EQ(derive_gate->source_map_identity, derive_buffer->source_map_identity);
    EXPECT_EQ(derive_gate->hygiene_mark, derive_buffer->hygiene_mark);
    EXPECT_EQ(derive_gate->token_stream_name, derive_buffer->token_stream_name);
    expect_contains(derive_gate->blocker_reason,
        "compiler-owned derive generated token buffer parser admission remains blocked in M21j");
    EXPECT_EQ(derive_gate->token_count, derive_buffer->token_count);
    EXPECT_TRUE(derive_gate->compiler_owned);
    EXPECT_TRUE(derive_gate->token_buffer_materialized);
    EXPECT_TRUE(derive_gate->token_records_available);
    EXPECT_FALSE(derive_gate->parser_admitted);
    EXPECT_FALSE(derive_gate->parse_ready);
    EXPECT_FALSE(derive_gate->parser_consumable);
    EXPECT_FALSE(derive_gate->generated_source_text);
    EXPECT_FALSE(derive_gate->generated_part_parsed);
    EXPECT_FALSE(derive_gate->generated_part_merged);
    EXPECT_FALSE(derive_gate->sema_visible);
    EXPECT_FALSE(derive_gate->produced_user_generated_code);
    EXPECT_NE(builder_gate->parse_gate_identity, derive_gate->parse_gate_identity);

    const frontend::macro::ParserAdmissionDiagnosticProjectionStub* const builder_diagnostic =
        parser_admission_diagnostic_for_input(result, *builder);
    ASSERT_NE(builder_diagnostic, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_diagnostic));
    EXPECT_EQ(builder_diagnostic->part_index, builder->part_index);
    EXPECT_EQ(builder_diagnostic->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_diagnostic->attached_part, builder->attached_part);
    EXPECT_EQ(builder_diagnostic->generated_part, generated.generated_part);
    EXPECT_EQ(builder_diagnostic->primary_anchor.source.value, builder->attribute_range.source.value);
    EXPECT_EQ(builder_diagnostic->primary_anchor.begin, builder->attribute_range.begin);
    EXPECT_EQ(builder_diagnostic->primary_anchor.end, builder->attribute_range.end);
    EXPECT_EQ(builder_diagnostic->token_tree_anchor.source.value, builder->token_tree_range.source.value);
    EXPECT_EQ(builder_diagnostic->token_tree_anchor.begin, builder->token_tree_range.begin);
    EXPECT_EQ(builder_diagnostic->token_tree_anchor.end, builder->token_tree_range.end);
    EXPECT_EQ(builder_diagnostic->parse_gate_identity, builder_gate->parse_gate_identity);
    EXPECT_GT(builder_diagnostic->diagnostic_identity.byte_count, 0U);
    EXPECT_GT(builder_diagnostic->diagnostic_anchor_identity.byte_count, 0U);
    EXPECT_NE(builder_diagnostic->diagnostic_identity, builder_diagnostic->parse_gate_identity);
    EXPECT_EQ(builder_diagnostic->token_plan_identity, builder_gate->token_plan_identity);
    EXPECT_EQ(builder_diagnostic->token_buffer_identity, builder_gate->token_buffer_identity);
    EXPECT_EQ(builder_diagnostic->materialization_identity, builder_gate->materialization_identity);
    EXPECT_EQ(builder_diagnostic->generated_buffer_identity, stub.generated_buffer_identity);
    EXPECT_EQ(builder_diagnostic->parse_config_fingerprint, stub.parse_config_fingerprint);
    EXPECT_EQ(builder_diagnostic->source_map_identity, builder_buffer->source_map_identity);
    EXPECT_EQ(builder_diagnostic->hygiene_mark, builder_buffer->hygiene_mark);
    EXPECT_EQ(builder_diagnostic->trace_identity, builder_trace->trace_identity);
    EXPECT_EQ(builder_diagnostic->diagnostic_policy,
        "parser_admission_blocked_diagnostic_projection_v1");
    EXPECT_EQ(builder_diagnostic->blocker_category, "empty_token_buffer_parser_admission_blocked");
    EXPECT_EQ(builder_diagnostic->token_buffer_blocker,
        "empty or non-derive generated token buffer parser admission remains blocked in M21j");
    expect_contains(builder_diagnostic->generated_part_parse_blocker,
        "generated module part parse remains blocked before parser admission diagnostics in M21k");
    expect_contains(builder_diagnostic->user_message,
        "generated token buffer is empty and parser admission remains blocked in M21k");
    expect_contains(builder_diagnostic->debug_projection_name,
        "m21k-parser-admission:0:0:0:0:builder");
    EXPECT_EQ(builder_diagnostic->token_count, 0U);
    EXPECT_FALSE(builder_diagnostic->token_buffer_materialized);
    EXPECT_FALSE(builder_diagnostic->token_records_available);
    EXPECT_FALSE(builder_diagnostic->parser_admitted);
    EXPECT_FALSE(builder_diagnostic->parse_ready);
    EXPECT_FALSE(builder_diagnostic->parser_consumable);
    EXPECT_FALSE(builder_diagnostic->generated_part_parsed);
    EXPECT_FALSE(builder_diagnostic->generated_part_merged);
    EXPECT_FALSE(builder_diagnostic->emit_expanded_available);
    EXPECT_FALSE(builder_diagnostic->debug_trace_available);
    EXPECT_FALSE(builder_diagnostic->source_map_available);
    EXPECT_FALSE(builder_diagnostic->produced_user_generated_code);

    const frontend::macro::ParserAdmissionDiagnosticProjectionStub* const derive_diagnostic =
        parser_admission_diagnostic_for_input(result, *derive);
    ASSERT_NE(derive_diagnostic, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_diagnostic));
    EXPECT_EQ(derive_diagnostic->parse_gate_identity, derive_gate->parse_gate_identity);
    EXPECT_EQ(derive_diagnostic->token_count, derive_gate->token_count);
    EXPECT_TRUE(derive_diagnostic->token_buffer_materialized);
    EXPECT_TRUE(derive_diagnostic->token_records_available);
    EXPECT_EQ(derive_diagnostic->blocker_category, "derive_token_buffer_parser_admission_blocked");
    EXPECT_EQ(derive_diagnostic->token_buffer_blocker,
        "compiler-owned derive generated token buffer parser admission remains blocked in M21j");
    expect_contains(derive_diagnostic->user_message,
        "generated derive token buffer is compiler-owned but parser admission remains blocked in M21k");
    expect_contains(derive_diagnostic->debug_projection_name,
        "m21k-parser-admission:0:0:0:1:derive");
    EXPECT_FALSE(derive_diagnostic->emit_expanded_available);
    EXPECT_FALSE(derive_diagnostic->debug_trace_available);
    EXPECT_FALSE(derive_diagnostic->source_map_available);
    EXPECT_NE(builder_diagnostic->diagnostic_identity, derive_diagnostic->diagnostic_identity);

    const frontend::macro::ParserAdmissionDiagnosticReportEntry* const builder_report_entry =
        parser_admission_report_entry_for_input(result, *builder);
    ASSERT_NE(builder_report_entry, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_report_entry));
    EXPECT_EQ(builder_report_entry->part_index, builder->part_index);
    EXPECT_EQ(builder_report_entry->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_report_entry->report_index, 0U);
    EXPECT_EQ(builder_report_entry->attached_part, builder->attached_part);
    EXPECT_EQ(builder_report_entry->generated_part, generated.generated_part);
    EXPECT_EQ(builder_report_entry->diagnostic_identity, builder_diagnostic->diagnostic_identity);
    EXPECT_EQ(builder_report_entry->diagnostic_anchor_identity,
        builder_diagnostic->diagnostic_anchor_identity);
    EXPECT_GT(builder_report_entry->report_entry_identity.byte_count, 0U);
    EXPECT_NE(builder_report_entry->report_entry_identity, builder_report_entry->diagnostic_identity);
    EXPECT_EQ(builder_report_entry->parse_gate_identity, builder_gate->parse_gate_identity);
    EXPECT_EQ(builder_report_entry->blocker_category, builder_diagnostic->blocker_category);
    EXPECT_EQ(builder_report_entry->debug_projection_name, builder_diagnostic->debug_projection_name);
    EXPECT_EQ(builder_report_entry->query_projection_name, "m21l-parser-admission-report:0:0");
    EXPECT_EQ(builder_report_entry->token_count, builder_diagnostic->token_count);
    EXPECT_FALSE(builder_report_entry->token_records_available);
    EXPECT_FALSE(builder_report_entry->parser_admitted);
    EXPECT_TRUE(builder_report_entry->report_visible);
    EXPECT_TRUE(builder_report_entry->query_reusable);
    EXPECT_FALSE(builder_report_entry->parser_consumable);
    EXPECT_FALSE(builder_report_entry->emit_expanded_available);
    EXPECT_FALSE(builder_report_entry->produced_user_generated_code);

    const frontend::macro::ParserAdmissionDiagnosticReportEntry* const derive_report_entry =
        parser_admission_report_entry_for_input(result, *derive);
    ASSERT_NE(derive_report_entry, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_report_entry));
    EXPECT_EQ(derive_report_entry->report_index, 1U);
    EXPECT_EQ(derive_report_entry->diagnostic_identity, derive_diagnostic->diagnostic_identity);
    EXPECT_EQ(derive_report_entry->diagnostic_anchor_identity,
        derive_diagnostic->diagnostic_anchor_identity);
    EXPECT_EQ(derive_report_entry->parse_gate_identity, derive_gate->parse_gate_identity);
    EXPECT_EQ(derive_report_entry->blocker_category, "derive_token_buffer_parser_admission_blocked");
    EXPECT_EQ(derive_report_entry->query_projection_name, builder_report_entry->query_projection_name);
    EXPECT_TRUE(derive_report_entry->token_records_available);
    EXPECT_FALSE(derive_report_entry->parser_admitted);
    EXPECT_NE(builder_report_entry->report_entry_identity, derive_report_entry->report_entry_identity);

    const frontend::macro::ParserAdmissionDiagnosticReport* const report =
        parser_admission_report_for_part(result, generated);
    ASSERT_NE(report, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*report));
    EXPECT_EQ(report->module.value, generated.module.value);
    EXPECT_EQ(report->source_part_index, generated.source_part_index);
    EXPECT_EQ(report->attached_part, generated.source_part);
    EXPECT_EQ(report->generated_part, generated.generated_part);
    EXPECT_GT(report->report_identity.byte_count, 0U);
    EXPECT_GT(report->report_anchor_identity.byte_count, 0U);
    EXPECT_GT(report->report_grouping_identity.byte_count, 0U);
    EXPECT_NE(report->report_identity, report->report_anchor_identity);
    EXPECT_NE(report->report_identity, report->report_grouping_identity);
    EXPECT_EQ(report->parse_config_fingerprint, stub.parse_config_fingerprint);
    EXPECT_EQ(report->generated_buffer_identity, stub.generated_buffer_identity);
    EXPECT_EQ(report->report_policy, "parser_admission_blocked_report_query_projection_v1");
    EXPECT_EQ(report->report_query_name, "m21l-parser-admission-report:0:0");
    expect_contains(report->blocked_reason,
        "parser admission diagnostic report remains parser-blocked in M21l");
    EXPECT_EQ(report->entry_count, 2U);
    EXPECT_EQ(report->blocked_entry_count, 2U);
    EXPECT_EQ(report->derive_entry_count, 1U);
    EXPECT_EQ(report->empty_entry_count, 1U);
    EXPECT_EQ(report->token_record_available_entry_count, 1U);
    EXPECT_TRUE(report->query_reusable);
    EXPECT_TRUE(report->report_visible);
    EXPECT_TRUE(report->source_anchor_ordered);
    EXPECT_FALSE(report->parser_admitted);
    EXPECT_FALSE(report->parse_ready);
    EXPECT_FALSE(report->parser_consumable);
    EXPECT_FALSE(report->emit_expanded_available);
    EXPECT_FALSE(report->debug_trace_available);
    EXPECT_FALSE(report->source_map_available);
    EXPECT_FALSE(report->produced_user_generated_code);

    const frontend::macro::GeneratedTokenParserReadinessPreflightEntry* const builder_preflight =
        parser_readiness_preflight_entry_for_input(result, *builder);
    ASSERT_NE(builder_preflight, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_preflight));
    EXPECT_EQ(builder_preflight->part_index, builder->part_index);
    EXPECT_EQ(builder_preflight->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_preflight->attached_part, builder->attached_part);
    EXPECT_EQ(builder_preflight->generated_part, generated.generated_part);
    EXPECT_EQ(builder_preflight->token_buffer_identity, builder_buffer->token_buffer_identity);
    EXPECT_EQ(builder_preflight->materialization_identity, builder_buffer->materialization_identity);
    EXPECT_EQ(builder_preflight->generated_buffer_identity, builder_gate->generated_buffer_identity);
    EXPECT_EQ(builder_preflight->parse_config_fingerprint, builder_gate->parse_config_fingerprint);
    EXPECT_EQ(builder_preflight->parse_gate_identity, builder_gate->parse_gate_identity);
    EXPECT_EQ(builder_preflight->diagnostic_identity, builder_diagnostic->diagnostic_identity);
    EXPECT_EQ(builder_preflight->diagnostic_anchor_identity, builder_diagnostic->diagnostic_anchor_identity);
    EXPECT_EQ(builder_preflight->report_entry_identity, builder_report_entry->report_entry_identity);
    EXPECT_EQ(builder_preflight->source_map_identity, builder_buffer->source_map_identity);
    EXPECT_EQ(builder_preflight->hygiene_mark, builder_buffer->hygiene_mark);
    EXPECT_EQ(builder_preflight->trace_identity, builder_diagnostic->trace_identity);
    EXPECT_GT(builder_preflight->preflight_identity.byte_count, 0U);
    EXPECT_EQ(builder_preflight->token_stream_name, builder_buffer->token_stream_name);
    EXPECT_EQ(builder_preflight->token_stream_shape, "empty_token_stream_parser_input_blocked");
    EXPECT_EQ(builder_preflight->delimiter_balance_state, "balanced");
    EXPECT_EQ(builder_preflight->source_anchor_coverage_state, "covered");
    EXPECT_EQ(builder_preflight->readiness_policy,
        "generated_token_parser_consumption_readiness_preflight_v1");
    expect_contains(builder_preflight->blocker_reason,
        "parser consumption readiness preflight remains parser-blocked in M21m");
    EXPECT_EQ(builder_preflight->token_count, 0U);
    EXPECT_FALSE(builder_preflight->token_records_available);
    EXPECT_TRUE(builder_preflight->token_indices_contiguous);
    EXPECT_TRUE(builder_preflight->delimiter_balanced);
    EXPECT_TRUE(builder_preflight->source_anchors_covered);
    EXPECT_TRUE(builder_preflight->parse_config_compatible);
    EXPECT_TRUE(builder_preflight->hygiene_prerequisite_available);
    EXPECT_TRUE(builder_preflight->source_map_prerequisite_available);
    EXPECT_TRUE(builder_preflight->diagnostic_projection_available);
    EXPECT_FALSE(builder_preflight->parser_admitted);
    EXPECT_FALSE(builder_preflight->parse_ready);
    EXPECT_FALSE(builder_preflight->parser_consumable);
    EXPECT_FALSE(builder_preflight->generated_part_parsed);
    EXPECT_FALSE(builder_preflight->generated_part_merged);
    EXPECT_FALSE(builder_preflight->produced_user_generated_code);

    const frontend::macro::GeneratedTokenParserReadinessPreflightEntry* const derive_preflight =
        parser_readiness_preflight_entry_for_input(result, *derive);
    ASSERT_NE(derive_preflight, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_preflight));
    EXPECT_EQ(derive_preflight->token_stream_shape, "derive_token_buffer_parser_input_candidate");
    EXPECT_EQ(derive_preflight->token_count, derive_buffer->token_count);
    EXPECT_TRUE(derive_preflight->token_records_available);
    EXPECT_TRUE(derive_preflight->token_indices_contiguous);
    EXPECT_TRUE(derive_preflight->delimiter_balanced);
    EXPECT_TRUE(derive_preflight->source_anchors_covered);
    EXPECT_FALSE(derive_preflight->parser_consumable);

    const frontend::macro::GeneratedTokenParserConsumptionContractGate* const contract =
        parser_consumption_contract_gate_for_part(result, generated);
    ASSERT_NE(contract, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*contract));
    EXPECT_EQ(contract->module.value, generated.module.value);
    EXPECT_EQ(contract->source_part_index, generated.source_part_index);
    EXPECT_EQ(contract->attached_part, generated.source_part);
    EXPECT_EQ(contract->generated_part, generated.generated_part);
    EXPECT_EQ(contract->generated_buffer_identity, stub.generated_buffer_identity);
    EXPECT_EQ(contract->parse_config_fingerprint, stub.parse_config_fingerprint);
    EXPECT_EQ(contract->report_identity, report->report_identity);
    EXPECT_GT(contract->contract_identity.byte_count, 0U);
    EXPECT_GT(contract->contract_grouping_identity.byte_count, 0U);
    EXPECT_GT(contract->contract_anchor_identity.byte_count, 0U);
    EXPECT_EQ(contract->contract_policy, "generated_token_parser_consumption_contract_gate_v1");
    EXPECT_EQ(contract->contract_query_name, "m21n-parser-consumption-contract:0:0");
    expect_contains(contract->blocked_reason,
        "parser consumption contract remains parser-blocked in M21n");
    EXPECT_EQ(contract->preflight_entry_count, 2U);
    EXPECT_EQ(contract->blocked_entry_count, 2U);
    EXPECT_EQ(contract->derive_entry_count, 1U);
    EXPECT_EQ(contract->empty_entry_count, 1U);
    EXPECT_EQ(contract->contiguous_index_entry_count, 2U);
    EXPECT_EQ(contract->delimiter_balanced_entry_count, 2U);
    EXPECT_EQ(contract->source_anchor_covered_entry_count, 2U);
    EXPECT_EQ(contract->parse_config_compatible_entry_count, 2U);
    EXPECT_EQ(contract->diagnostic_projection_entry_count, 2U);
    EXPECT_TRUE(contract->query_reusable);
    EXPECT_TRUE(contract->contract_visible);
    EXPECT_TRUE(contract->all_entries_structurally_checked);
    EXPECT_FALSE(contract->parser_admitted);
    EXPECT_FALSE(contract->parse_ready);
    EXPECT_FALSE(contract->parser_consumable);
    EXPECT_FALSE(contract->generated_part_parsed);
    EXPECT_FALSE(contract->generated_part_merged);
    EXPECT_FALSE(contract->sema_visible);
    EXPECT_FALSE(contract->emit_expanded_available);
    EXPECT_FALSE(contract->debug_trace_available);
    EXPECT_FALSE(contract->source_map_available);
    EXPECT_FALSE(contract->produced_user_generated_code);

    const frontend::macro::MacroExpansionBoundaryClosureReport& closure =
        result.macro_boundary_closure_reports.front();
    EXPECT_TRUE(frontend::macro::is_valid(closure));
    EXPECT_GT(closure.closure_identity.byte_count, 0U);
    EXPECT_GT(closure.closure_grouping_identity.byte_count, 0U);
    EXPECT_NE(closure.closure_identity, closure.closure_grouping_identity);
    EXPECT_EQ(closure.closure_policy, "m21_macro_expansion_boundary_release_closure_v1");
    EXPECT_EQ(closure.closure_query_name, "m21o-macro-boundary-closure");
    expect_contains(closure.blocked_reason,
        "M21 macro expansion boundary remains parser-blocked after M21o closure");
    EXPECT_EQ(closure.macro_input_count, 2U);
    EXPECT_EQ(closure.generated_part_count, 1U);
    EXPECT_EQ(closure.parser_admission_report_count, 1U);
    EXPECT_EQ(closure.parser_readiness_preflight_entry_count, 2U);
    EXPECT_EQ(closure.parser_consumption_contract_gate_count, 1U);
    EXPECT_EQ(closure.blocked_contract_gate_count, 1U);
    EXPECT_EQ(closure.parser_consumable_contract_gate_count, 0U);
    EXPECT_TRUE(closure.m21m_preflight_available);
    EXPECT_TRUE(closure.m21n_contract_available);
    EXPECT_TRUE(closure.release_closure_complete);
    EXPECT_TRUE(closure.query_reusable);
    EXPECT_TRUE(closure.closure_visible);
    EXPECT_FALSE(closure.parser_consumption_enabled);
    EXPECT_FALSE(closure.emit_expanded_available);
    EXPECT_FALSE(closure.debug_trace_available);
    EXPECT_FALSE(closure.source_map_available);
    EXPECT_FALSE(closure.standard_library_required);
    EXPECT_FALSE(closure.runtime_required);
    EXPECT_FALSE(closure.external_process_required);
    EXPECT_FALSE(closure.produced_user_generated_code);

    ASSERT_EQ(result.builtin_derive_expansion_admissions.size(), 2U);
    ASSERT_EQ(result.builtin_derive_semantic_plans.size(), 2U);
    ASSERT_EQ(result.builtin_derive_parser_release_gates.size(), 1U);
    const frontend::macro::BuiltinDeriveExpansionAdmissionGate* const builder_derive_admission =
        builtin_derive_admission_for_input(result, *builder);
    ASSERT_NE(builder_derive_admission, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_derive_admission));
    EXPECT_EQ(builder_derive_admission->part_index, builder->part_index);
    EXPECT_EQ(builder_derive_admission->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_derive_admission->admission_index, 0U);
    EXPECT_EQ(builder_derive_admission->attached_part, builder->attached_part);
    EXPECT_EQ(builder_derive_admission->generated_part, generated.generated_part);
    EXPECT_EQ(builder_derive_admission->token_buffer_identity, builder_buffer->token_buffer_identity);
    EXPECT_EQ(builder_derive_admission->preflight_identity, builder_preflight->preflight_identity);
    EXPECT_EQ(builder_derive_admission->parse_gate_identity, builder_gate->parse_gate_identity);
    EXPECT_EQ(builder_derive_admission->diagnostic_identity, builder_diagnostic->diagnostic_identity);
    EXPECT_EQ(builder_derive_admission->closure_identity, closure.closure_identity);
    EXPECT_GT(builder_derive_admission->admission_identity.byte_count, 0U);
    EXPECT_EQ(builder_derive_admission->admission_policy,
        "builtin_derive_expansion_admission_gate_v1");
    EXPECT_EQ(builder_derive_admission->admission_kind,
        "non_derive_attribute_expansion_blocked");
    EXPECT_EQ(builder_derive_admission->query_name,
        "m22a-builtin-derive-admission:0:0:0:0:builder");
    expect_contains(builder_derive_admission->blocker_reason,
        "non-derive item attribute expansion remains blocked in M22a");
    EXPECT_EQ(builder_derive_admission->token_count, 0U);
    EXPECT_EQ(builder_derive_admission->capability_candidate_count, 0U);
    EXPECT_FALSE(builder_derive_admission->builtin_derive_input);
    EXPECT_TRUE(builder_derive_admission->compiler_owned);
    EXPECT_FALSE(builder_derive_admission->token_records_available);
    EXPECT_TRUE(builder_derive_admission->preflight_available);
    EXPECT_TRUE(builder_derive_admission->admission_visible);
    EXPECT_TRUE(builder_derive_admission->query_reusable);
    EXPECT_FALSE(builder_derive_admission->parser_consumption_enabled);
    EXPECT_FALSE(builder_derive_admission->external_process_required);
    EXPECT_FALSE(builder_derive_admission->standard_library_required);
    EXPECT_FALSE(builder_derive_admission->runtime_required);
    EXPECT_FALSE(builder_derive_admission->generated_source_text);
    EXPECT_FALSE(builder_derive_admission->produced_user_generated_code);

    const frontend::macro::BuiltinDeriveExpansionAdmissionGate* const derive_expansion_admission =
        builtin_derive_admission_for_input(result, *derive);
    ASSERT_NE(derive_expansion_admission, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_expansion_admission));
    EXPECT_EQ(derive_expansion_admission->admission_index, 1U);
    EXPECT_EQ(derive_expansion_admission->token_buffer_identity, derive_buffer->token_buffer_identity);
    EXPECT_EQ(derive_expansion_admission->preflight_identity, derive_preflight->preflight_identity);
    EXPECT_EQ(derive_expansion_admission->parse_gate_identity, derive_gate->parse_gate_identity);
    EXPECT_EQ(derive_expansion_admission->diagnostic_identity, derive_diagnostic->diagnostic_identity);
    EXPECT_EQ(derive_expansion_admission->closure_identity, closure.closure_identity);
    EXPECT_EQ(derive_expansion_admission->admission_kind,
        "builtin_derive_expansion_candidate");
    EXPECT_EQ(derive_expansion_admission->query_name,
        "m22a-builtin-derive-admission:0:0:0:1:derive");
    expect_contains(derive_expansion_admission->blocker_reason,
        "builtin derive expansion admission remains parser-blocked in M22a");
    EXPECT_EQ(derive_expansion_admission->token_count, derive_buffer->token_count);
    EXPECT_EQ(derive_expansion_admission->capability_candidate_count, 2U);
    EXPECT_EQ(derive_expansion_admission->unsupported_candidate_count, 0U);
    EXPECT_EQ(derive_expansion_admission->duplicate_candidate_count, 0U);
    EXPECT_TRUE(derive_expansion_admission->builtin_derive_input);
    EXPECT_TRUE(derive_expansion_admission->token_records_available);
    EXPECT_FALSE(derive_expansion_admission->parser_consumption_enabled);
    EXPECT_NE(builder_derive_admission->admission_identity,
        derive_expansion_admission->admission_identity);

    const frontend::macro::BuiltinDeriveSemanticExpansionPlan* const builder_semantic_plan =
        builtin_derive_semantic_plan_for_input(result, *builder);
    ASSERT_NE(builder_semantic_plan, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_semantic_plan));
    EXPECT_EQ(builder_semantic_plan->semantic_plan_index, 0U);
    EXPECT_EQ(builder_semantic_plan->admission_identity,
        builder_derive_admission->admission_identity);
    EXPECT_EQ(builder_semantic_plan->semantic_policy,
        "builtin_derive_semantic_expansion_plan_v1");
    EXPECT_EQ(builder_semantic_plan->target_kind, "struct");
    EXPECT_EQ(builder_semantic_plan->semantic_model, "capability_fact_lowering_plan");
    expect_contains(builder_semantic_plan->blocker_reason,
        "builtin derive semantic expansion remains capability-only and parser-blocked in M22b");
    EXPECT_EQ(builder_semantic_plan->capability_count, 0U);
    EXPECT_EQ(builder_semantic_plan->copy_capability_count, 0U);
    EXPECT_EQ(builder_semantic_plan->eq_capability_count, 0U);
    EXPECT_EQ(builder_semantic_plan->hash_capability_count, 0U);
    EXPECT_FALSE(builder_semantic_plan->builtin_derive_input);
    EXPECT_TRUE(builder_semantic_plan->target_struct_or_enum);
    EXPECT_FALSE(builder_semantic_plan->uses_existing_builtin_derive_capability_path);
    EXPECT_FALSE(builder_semantic_plan->requires_ast_mutation);
    EXPECT_FALSE(builder_semantic_plan->requires_generated_items);
    EXPECT_FALSE(builder_semantic_plan->requires_standard_library);
    EXPECT_FALSE(builder_semantic_plan->requires_runtime);
    EXPECT_FALSE(builder_semantic_plan->external_process_required);
    EXPECT_FALSE(builder_semantic_plan->parser_consumption_enabled);
    EXPECT_FALSE(builder_semantic_plan->produced_user_generated_code);
    EXPECT_TRUE(builder_semantic_plan->plan_visible);
    EXPECT_TRUE(builder_semantic_plan->query_reusable);

    const frontend::macro::BuiltinDeriveSemanticExpansionPlan* const derive_semantic_plan =
        builtin_derive_semantic_plan_for_input(result, *derive);
    ASSERT_NE(derive_semantic_plan, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_semantic_plan));
    EXPECT_EQ(derive_semantic_plan->semantic_plan_index, 1U);
    EXPECT_EQ(derive_semantic_plan->admission_identity,
        derive_expansion_admission->admission_identity);
    EXPECT_EQ(derive_semantic_plan->target_kind, "struct");
    EXPECT_EQ(derive_semantic_plan->capability_count, 2U);
    EXPECT_EQ(derive_semantic_plan->copy_capability_count, 1U);
    EXPECT_EQ(derive_semantic_plan->eq_capability_count, 1U);
    EXPECT_EQ(derive_semantic_plan->hash_capability_count, 0U);
    EXPECT_TRUE(derive_semantic_plan->builtin_derive_input);
    EXPECT_TRUE(derive_semantic_plan->target_struct_or_enum);
    EXPECT_TRUE(derive_semantic_plan->uses_existing_builtin_derive_capability_path);
    EXPECT_FALSE(derive_semantic_plan->requires_generated_items);
    EXPECT_FALSE(derive_semantic_plan->parser_consumption_enabled);
    EXPECT_NE(builder_semantic_plan->semantic_plan_identity,
        derive_semantic_plan->semantic_plan_identity);

    const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate* const release_gate =
        builtin_derive_parser_release_gate_for_part(result, generated);
    ASSERT_NE(release_gate, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*release_gate));
    EXPECT_EQ(release_gate->module.value, generated.module.value);
    EXPECT_EQ(release_gate->source_part_index, generated.source_part_index);
    EXPECT_EQ(release_gate->attached_part, generated.source_part);
    EXPECT_EQ(release_gate->generated_part, generated.generated_part);
    EXPECT_EQ(release_gate->contract_identity, contract->contract_identity);
    EXPECT_EQ(release_gate->closure_identity, closure.closure_identity);
    EXPECT_GT(release_gate->admission_group_identity.byte_count, 0U);
    EXPECT_GT(release_gate->semantic_plan_group_identity.byte_count, 0U);
    EXPECT_GT(release_gate->release_gate_identity.byte_count, 0U);
    EXPECT_EQ(release_gate->release_policy,
        "builtin_derive_parser_consumption_release_gate_v1");
    EXPECT_EQ(release_gate->release_query_name,
        "m22c-builtin-derive-parser-release:0:0");
    expect_contains(release_gate->blocked_reason,
        "builtin derive parser consumption release remains blocked in M22c");
    EXPECT_EQ(release_gate->admission_count, 2U);
    EXPECT_EQ(release_gate->derive_admission_count, 1U);
    EXPECT_EQ(release_gate->semantic_plan_count, 2U);
    EXPECT_EQ(release_gate->capability_total_count, 2U);
    EXPECT_EQ(release_gate->parser_consumable_contract_count, 0U);
    EXPECT_TRUE(release_gate->rollback_diagnostics_available);
    EXPECT_TRUE(release_gate->debug_trace_prerequisite_available);
    EXPECT_TRUE(release_gate->source_map_prerequisite_available);
    EXPECT_TRUE(release_gate->hygiene_prerequisite_available);
    EXPECT_FALSE(release_gate->parser_consumption_enabled);
    EXPECT_FALSE(release_gate->generated_part_parsed);
    EXPECT_FALSE(release_gate->generated_part_merged);
    EXPECT_FALSE(release_gate->emit_expanded_available);
    EXPECT_FALSE(release_gate->debug_trace_available);
    EXPECT_FALSE(release_gate->source_map_available);
    EXPECT_FALSE(release_gate->standard_library_required);
    EXPECT_FALSE(release_gate->runtime_required);
    EXPECT_FALSE(release_gate->external_process_required);
    EXPECT_FALSE(release_gate->produced_user_generated_code);
    EXPECT_TRUE(release_gate->release_visible);
    EXPECT_TRUE(release_gate->query_reusable);

    const std::vector<const frontend::macro::GeneratedTokenRecord*> builder_records =
        token_records_for_input(result, *builder);
    EXPECT_TRUE(builder_records.empty());
    const std::vector<const frontend::macro::GeneratedTokenRecord*> derive_records =
        token_records_for_input(result, *derive);
    ASSERT_EQ(derive_records.size(), derive_buffer->token_count);
    const frontend::macro::GeneratedTokenRecord& begin_record = *derive_records.front();
    EXPECT_TRUE(frontend::macro::is_valid(begin_record));
    EXPECT_EQ(begin_record.token_buffer_identity, derive_buffer->token_buffer_identity);
    EXPECT_EQ(begin_record.token_index, 0U);
    EXPECT_EQ(begin_record.kind, syntax::TokenKind::identifier);
    EXPECT_EQ(begin_record.text, "__aurex_builtin_derive_begin");
    EXPECT_EQ(begin_record.token_role, "derive_codegen_begin");
    EXPECT_FALSE(begin_record.parser_visible);
    EXPECT_FALSE(begin_record.produced_user_generated_code);
    ASSERT_GE(derive_records.size(), 3U);
    const frontend::macro::GeneratedTokenRecord& first_source_record = *derive_records[1];
    EXPECT_EQ(first_source_record.kind, config->attributes[1].token_tree[0].kind);
    EXPECT_EQ(first_source_record.text, "__aurex_builtin_derive_source_token_1");
    EXPECT_EQ(first_source_record.token_role, "derive_source_token_placeholder");
    EXPECT_EQ(first_source_record.anchor_range.source.value,
        config->attributes[1].token_tree[0].range.source.value);
    EXPECT_EQ(first_source_record.anchor_range.begin, config->attributes[1].token_tree[0].range.begin);
    EXPECT_EQ(first_source_record.anchor_range.end, config->attributes[1].token_tree[0].range.end);
    const frontend::macro::GeneratedTokenRecord& end_record = *derive_records.back();
    EXPECT_EQ(end_record.token_index, derive_buffer->token_count - 1U);
    EXPECT_EQ(end_record.kind, syntax::TokenKind::identifier);
    EXPECT_EQ(end_record.text, "__aurex_builtin_derive_end");
    EXPECT_EQ(end_record.token_role, "derive_codegen_end");
    EXPECT_EQ(end_record.token_buffer_identity, derive_buffer->token_buffer_identity);
    EXPECT_NE(begin_record.token_identity, first_source_record.token_identity);
    EXPECT_NE(first_source_record.token_identity, end_record.token_identity);

    const std::string summary = frontend::macro::summarize_early_item_expansion(result);
    expect_contains(summary, "early_item_expansion name=M22c Builtin Derive Parser Consumption Release Gate");
    expect_contains(summary, "attributes=2");
    expect_contains(summary, "blocked_attributes=1");
    expect_contains(summary, "generated_part_stubs=1");
    expect_contains(summary, "parse_blocked=1");
    expect_contains(summary, "merge_blocked=1");
    expect_contains(summary, "sema_visible_generated_parts=0");
    expect_contains(summary, "hygiene_stubs=2");
    expect_contains(summary, "unresolved_hygiene_stubs=2");
    expect_contains(summary, "declared_name_stubs=2");
    expect_contains(summary, "trace_stubs=2");
    expect_contains(summary, "real_source_maps=0");
    expect_contains(summary, "debug_traces=0");
    expect_contains(summary, "cli_emit_expanded=0");
    expect_contains(summary, "generated_item_declarations=2");
    expect_contains(summary, "planned_generated_item_declarations=2");
    expect_contains(summary, "materialized_generated_items=0");
    expect_contains(summary, "declared_generated_names=2");
    expect_contains(summary, "lookup_visible_declared_names=0");
    expect_contains(summary, "export_visible_declared_names=0");
    expect_contains(summary, "token_materialization_admissions=2");
    expect_contains(summary, "compiler_owned_admissions=2");
    expect_contains(summary, "admitted_token_materializations=2");
    expect_contains(summary, "materialized_token_admissions=1");
    expect_contains(summary, "generated_token_buffers=2");
    expect_contains(summary, "empty_generated_token_buffers=1");
    expect_contains(summary, "materialized_token_buffers=1");
    expect_contains(summary, "compiler_owned_token_buffers=2");
    expect_contains(summary, "generated_token_records=7");
    expect_contains(summary, "compiler_owned_generated_token_records=7");
    expect_contains(summary, "parser_visible_generated_tokens=0");
    expect_contains(summary, "parser_admission_gates=2");
    expect_contains(summary, "compiler_owned_parser_admission_gates=2");
    expect_contains(summary, "token_record_available_gates=1");
    expect_contains(summary, "parser_blocked_token_buffers=2");
    expect_contains(summary, "parser_admitted_token_buffers=0");
    expect_contains(summary, "parser_admission_diagnostics=2");
    expect_contains(summary, "parser_admission_diagnostics_blocked=2");
    expect_contains(summary, "derive_parser_admission_diagnostics=1");
    expect_contains(summary, "empty_parser_admission_diagnostics=1");
    expect_contains(summary, "emit_expanded_projections=0");
    expect_contains(summary, "parser_admission_debug_trace_projections=0");
    expect_contains(summary, "parser_admission_source_map_projections=0");
    expect_contains(summary, "parser_admission_report_entries=2");
    expect_contains(summary, "parser_admission_reports=1");
    expect_contains(summary, "parser_admission_report_blocked_entries=2");
    expect_contains(summary, "derive_parser_admission_report_entries=1");
    expect_contains(summary, "empty_parser_admission_report_entries=1");
    expect_contains(summary, "parser_admission_report_token_record_available_entries=1");
    expect_contains(summary, "parser_admission_report_visible=1");
    expect_contains(summary, "parser_admission_report_query_reusable=1");
    expect_contains(summary, "parser_admission_report_unordered_anchors=0");
    expect_contains(summary, "parser_admission_report_parser_consumable=0");
    expect_contains(summary, "parser_readiness_preflight_entries=2");
    expect_contains(summary, "parser_readiness_preflight_blocked=2");
    expect_contains(summary, "derive_parser_readiness_preflight_entries=1");
    expect_contains(summary, "empty_parser_readiness_preflight_entries=1");
    expect_contains(summary, "parser_readiness_preflight_contiguous_indices=2");
    expect_contains(summary, "parser_readiness_preflight_delimiter_balanced=2");
    expect_contains(summary, "parser_readiness_preflight_source_anchor_covered=2");
    expect_contains(summary, "parser_readiness_preflight_parse_config_compatible=2");
    expect_contains(summary, "parser_readiness_preflight_parser_consumable=0");
    expect_contains(summary, "parser_consumption_contract_gates=1");
    expect_contains(summary, "parser_consumption_contract_blocked_gates=1");
    expect_contains(summary, "parser_consumption_contract_visible=1");
    expect_contains(summary, "parser_consumption_contract_query_reusable=1");
    expect_contains(summary, "parser_consumption_contract_parser_consumable=0");
    expect_contains(summary, "macro_boundary_closure_reports=1");
    expect_contains(summary, "macro_boundary_closure_visible=1");
    expect_contains(summary, "macro_boundary_closure_query_reusable=1");
    expect_contains(summary, "macro_boundary_closure_complete=1");
    expect_contains(summary, "macro_boundary_closure_parser_consumption_enabled=0");
    expect_contains(summary, "builtin_derive_expansion_admissions=2");
    expect_contains(summary, "builtin_derive_expansion_derive_admissions=1");
    expect_contains(summary, "builtin_derive_expansion_non_derive_blocked=1");
    expect_contains(summary, "builtin_derive_expansion_visible=2");
    expect_contains(summary, "builtin_derive_expansion_query_reusable=2");
    expect_contains(summary, "builtin_derive_expansion_capability_candidates=2");
    expect_contains(summary, "builtin_derive_semantic_plans=2");
    expect_contains(summary, "builtin_derive_semantic_plan_visible=2");
    expect_contains(summary, "builtin_derive_semantic_plan_query_reusable=2");
    expect_contains(summary, "builtin_derive_semantic_capabilities=2");
    expect_contains(summary, "builtin_derive_semantic_copy_capabilities=1");
    expect_contains(summary, "builtin_derive_semantic_eq_capabilities=1");
    expect_contains(summary, "builtin_derive_semantic_hash_capabilities=0");
    expect_contains(summary, "builtin_derive_parser_release_gates=1");
    expect_contains(summary, "builtin_derive_parser_release_visible=1");
    expect_contains(summary, "builtin_derive_parser_release_query_reusable=1");
    expect_contains(summary, "builtin_derive_parser_release_parser_consumable=0");
    expect_contains(summary, "generated_source_text=0");
    expect_contains(summary, "parse_ready_token_buffers=0");
    expect_contains(summary, "user_generated_code=0");

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "input #0");
    expect_contains(dump, "attr=builder");
    expect_contains(dump, "disposition=blocked_unimplemented_attribute");
    expect_contains(dump, "generated_part #0");
    expect_contains(dump, "source_role=generated");
    expect_contains(dump, "kind=generated");
    expect_contains(dump, "parse_merge_stub #0");
    expect_contains(dump, "lifecycle=merge_blocked");
    expect_contains(dump, "materialized_buffer=yes");
    expect_contains(dump, "sema_visible=no");
    expect_contains(dump, "parse and merge are blocked in M21e");
    expect_contains(dump, "buffer_identity=");
    expect_contains(dump, "parse_config=");
    expect_contains(dump, "merge_ordering=");
    expect_contains(dump, "source_map #1");
    expect_contains(dump, "hygiene_stub #0");
    expect_contains(dump, "policy=origin_mark_hygiene_v1");
    expect_contains(dump, "resolved=no");
    expect_contains(dump, "declared_names_visible=no");
    expect_contains(dump, "captures_call_site_locals=no");
    expect_contains(dump, "call_site_mark=");
    expect_contains(dump, "definition_site_mark=");
    expect_contains(dump, "generated_fresh_mark=");
    expect_contains(dump, "declared_name_set=");
    expect_contains(dump, "trace_stub #0");
    expect_contains(dump, "policy=expansion_source_map_debug_trace_v1");
    expect_contains(dump, "cli_emit_expanded=no");
    expect_contains(dump, "real macro source map and debug trace are blocked in M21f");
    expect_contains(dump, "trace_identity=");
    expect_contains(dump, "generated_source_map=");
    expect_contains(dump, "diagnostic_anchor=");
    expect_contains(dump, "generated_item_declaration_stub #0");
    expect_contains(dump, "role=attached_item_codegen_declared_names_v1");
    expect_contains(dump, "name=__aurex_macro_declared:0:0:0:0:builder");
    expect_contains(dump, "planned=yes");
    expect_contains(dump, "materialized_tokens=no");
    expect_contains(dump, "generated item declaration materialization is blocked in M21g");
    expect_contains(dump, "declaration_identity=");
    expect_contains(dump, "generated_item_key=");
    expect_contains(dump, "declared_generated_name_stub #0");
    expect_contains(dump, "namespace=item");
    expect_contains(dump, "lookup_visible=no");
    expect_contains(dump, "export_visible=no");
    expect_contains(dump, "declared generated name lookup is blocked in M21g");
    expect_contains(dump, "declared_name_identity=");
    expect_contains(dump, "hygiene_mark=");
    expect_contains(dump, "token_materialization_admission_stub #0");
    expect_contains(dump, "policy=compiler_owned_attached_item_token_materialization_admission_v1");
    expect_contains(dump, "token_stream=m21h-token-stream:0:0:0:0:builder");
    expect_contains(dump, "compiler_owned=yes");
    expect_contains(dump, "admitted=yes");
    expect_contains(dump, "generated_source_text=no");
    expect_contains(dump, "parse_ready=no");
    expect_contains(dump, "external_process_required=no");
    expect_contains(dump, "standard_library_required=no");
    expect_contains(dump, "runtime_required=no");
    expect_contains(dump, "non-derive item attribute token materialization remains blocked in M21i");
    expect_contains(dump, "source_map_identity=");
    expect_contains(dump, "token_plan_identity=");
    expect_contains(dump, "token_buffer_identity=");
    expect_contains(dump, "generated_token_buffer_stub #0");
    expect_contains(dump, "kind=compiler_owned_empty_token_stream");
    expect_contains(dump, "producer=compiler_owned_blocked_empty_token_producer_v1");
    expect_contains(dump, "token_count=0");
    expect_contains(dump, "empty=yes");
    expect_contains(dump, "parser_consumable=no");
    expect_contains(dump, "materialization_identity=");
    expect_contains(dump, "generated token buffer remains empty and parser-blocked in M21i");
    expect_contains(dump, "kind=compiler_owned_builtin_derive_token_stream_prototype");
    expect_contains(dump, "producer=compiler_owned_builtin_derive_token_producer_prototype_v1");
    expect_contains(dump, "generated_token_record #0");
    expect_contains(dump, "text=__aurex_builtin_derive_begin");
    expect_contains(dump, "role=derive_codegen_begin");
    expect_contains(dump, "role=derive_source_token_placeholder");
    expect_contains(dump, "text=__aurex_builtin_derive_end");
    expect_contains(dump, "parser_visible=no");
    expect_contains(dump, "token_identity=");
    expect_contains(dump, "generated_token_parser_admission_gate_stub #0");
    expect_contains(dump, "policy=compiler_owned_generated_token_parser_admission_gate_v1");
    expect_contains(dump, "token_buffer_materialized=no");
    expect_contains(dump, "token_records_available=no");
    expect_contains(dump, "parser_admitted=no");
    expect_contains(dump, "generated_part_parsed=no");
    expect_contains(dump, "generated_part_merged=no");
    expect_contains(dump, "empty or non-derive generated token buffer parser admission remains blocked in M21j");
    expect_contains(dump, "compiler-owned derive generated token buffer parser admission remains blocked in M21j");
    expect_contains(dump, "generated_buffer_identity=");
    expect_contains(dump, "parse_gate_identity=");
    expect_contains(dump, "parser_admission_diagnostic_projection_stub #0");
    expect_contains(dump, "policy=parser_admission_blocked_diagnostic_projection_v1");
    expect_contains(dump, "category=empty_token_buffer_parser_admission_blocked");
    expect_contains(dump, "category=derive_token_buffer_parser_admission_blocked");
    expect_contains(dump, "debug_projection=m21k-parser-admission:0:0:0:0:builder");
    expect_contains(dump, "debug_projection=m21k-parser-admission:0:0:0:1:derive");
    expect_contains(dump, "primary_anchor=");
    expect_contains(dump, "token_tree_anchor=");
    expect_contains(dump, "emit_expanded_available=no");
    expect_contains(dump, "debug_trace_available=no");
    expect_contains(dump, "source_map_available=no");
    expect_contains(dump, "token_buffer_blocker=empty or non-derive generated token buffer parser admission remains blocked in M21j");
    expect_contains(dump, "generated_part_parse_blocker=generated module part parse remains blocked before parser admission diagnostics in M21k");
    expect_contains(dump, "message=generated token buffer is empty and parser admission remains blocked in M21k");
    expect_contains(dump, "message=generated derive token buffer is compiler-owned but parser admission remains blocked in M21k");
    expect_contains(dump, "diagnostic_identity=");
    expect_contains(dump, "diagnostic_anchor=");
    expect_contains(dump, "trace_identity=");
    expect_contains(dump, "parser_admission_report_entry #0");
    expect_contains(dump, "report_index=0");
    expect_contains(dump, "report_index=1");
    expect_contains(dump, "query_projection=m21l-parser-admission-report:0:0");
    expect_contains(dump, "report_visible=yes");
    expect_contains(dump, "query_reusable=yes");
    expect_contains(dump, "report_entry_identity=");
    expect_contains(dump, "parser_admission_diagnostic_report #0");
    expect_contains(dump, "policy=parser_admission_blocked_report_query_projection_v1");
    expect_contains(dump, "query=m21l-parser-admission-report:0:0");
    expect_contains(dump, "entries=2");
    expect_contains(dump, "blocked_entries=2");
    expect_contains(dump, "derive_entries=1");
    expect_contains(dump, "empty_entries=1");
    expect_contains(dump, "token_record_available_entries=1");
    expect_contains(dump, "source_anchor_ordered=yes");
    expect_contains(dump, "parser admission diagnostic report remains parser-blocked in M21l");
    expect_contains(dump, "report_identity=");
    expect_contains(dump, "report_anchor_identity=");
    expect_contains(dump, "report_grouping_identity=");
    expect_contains(dump, "parser_readiness_preflight_entry #0");
    expect_contains(dump, "shape=empty_token_stream_parser_input_blocked");
    expect_contains(dump, "shape=derive_token_buffer_parser_input_candidate");
    expect_contains(dump, "token_indices_contiguous=yes");
    expect_contains(dump, "delimiter_balance=balanced");
    expect_contains(dump, "source_anchor_coverage=covered");
    expect_contains(dump, "parse_config_compatible=yes");
    expect_contains(dump, "hygiene_prerequisite_available=yes");
    expect_contains(dump, "source_map_prerequisite_available=yes");
    expect_contains(dump, "diagnostic_projection_available=yes");
    expect_contains(dump, "policy=generated_token_parser_consumption_readiness_preflight_v1");
    expect_contains(dump, "parser consumption readiness preflight remains parser-blocked in M21m");
    expect_contains(dump, "preflight_identity=");
    expect_contains(dump, "parser_consumption_contract_gate #0");
    expect_contains(dump, "policy=generated_token_parser_consumption_contract_gate_v1");
    expect_contains(dump, "query=m21n-parser-consumption-contract:0:0");
    expect_contains(dump, "preflight_entries=2");
    expect_contains(dump, "all_entries_structurally_checked=yes");
    expect_contains(dump, "parser consumption contract remains parser-blocked in M21n");
    expect_contains(dump, "contract_identity=");
    expect_contains(dump, "contract_grouping_identity=");
    expect_contains(dump, "contract_anchor_identity=");
    expect_contains(dump, "macro_boundary_closure_report #0");
    expect_contains(dump, "policy=m21_macro_expansion_boundary_release_closure_v1");
    expect_contains(dump, "query=m21o-macro-boundary-closure");
    expect_contains(dump, "release_closure_complete=yes");
    expect_contains(dump, "parser_consumption_enabled=no");
    expect_contains(dump, "M21 macro expansion boundary remains parser-blocked after M21o closure");
    expect_contains(dump, "closure_identity=");
    expect_contains(dump, "closure_grouping_identity=");
    expect_contains(dump, "builtin_derive_expansion_admission_gate #0");
    expect_contains(dump, "policy=builtin_derive_expansion_admission_gate_v1");
    expect_contains(dump, "kind=non_derive_attribute_expansion_blocked");
    expect_contains(dump, "kind=builtin_derive_expansion_candidate");
    expect_contains(dump, "query=m22a-builtin-derive-admission:0:0:0:1:derive");
    expect_contains(dump, "capability_candidates=2");
    expect_contains(dump, "builtin derive expansion admission remains parser-blocked in M22a");
    expect_contains(dump, "admission_identity=");
    expect_contains(dump, "builtin_derive_semantic_expansion_plan #0");
    expect_contains(dump, "policy=builtin_derive_semantic_expansion_plan_v1");
    expect_contains(dump, "target_kind=struct");
    expect_contains(dump, "semantic_model=capability_fact_lowering_plan");
    expect_contains(dump, "copy=1");
    expect_contains(dump, "eq=1");
    expect_contains(dump, "hash=0");
    expect_contains(dump, "requires_generated_items=no");
    expect_contains(dump, "requires_standard_library=no");
    expect_contains(dump, "requires_runtime=no");
    expect_contains(dump, "builtin derive semantic expansion remains capability-only and parser-blocked in M22b");
    expect_contains(dump, "capability_set_identity=");
    expect_contains(dump, "semantic_plan_identity=");
    expect_contains(dump, "builtin_derive_parser_consumption_release_gate #0");
    expect_contains(dump, "policy=builtin_derive_parser_consumption_release_gate_v1");
    expect_contains(dump, "query=m22c-builtin-derive-parser-release:0:0");
    expect_contains(dump, "admissions=2");
    expect_contains(dump, "derive_admissions=1");
    expect_contains(dump, "semantic_plans=2");
    expect_contains(dump, "capabilities=2");
    expect_contains(dump, "parser_consumable_contracts=0");
    expect_contains(dump, "rollback_diagnostics_available=yes");
    expect_contains(dump, "builtin derive parser consumption release remains blocked in M22c");
    expect_contains(dump, "release_gate_identity=");
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksAttributeTokenTree)
{
    constexpr std::string_view first_source =
        "module macro.early_item_expansion;\n"
        "#[builder(defaults(threads = 4))]\n"
        "struct Config { threads: i32; }\n";
    constexpr std::string_view second_source =
        "module macro.early_item_expansion;\n"
        "#[builder(defaults(threads = 8))]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult first = expand_source(first_source);
    const frontend::macro::EarlyItemExpansionResult second = expand_source(second_source);

    ASSERT_EQ(first.inputs.size(), 1U);
    ASSERT_EQ(second.inputs.size(), 1U);
    EXPECT_NE(first.inputs.front().token_tree_fingerprint, second.inputs.front().token_tree_fingerprint);
    EXPECT_NE(first.inputs.front().query_key_fingerprint, second.inputs.front().query_key_fingerprint);
    EXPECT_NE(first.fingerprint, second.fingerprint);

    frontend::macro::EarlyItemExpansionResult stale = first;
    stale.inputs.front().token_count += 1U;
    EXPECT_FALSE(frontend::macro::is_valid(stale));
}

TEST(CoreUnit, EarlyItemExpansionGeneratedItemNamesIncludeItemIdentity)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n"
        "#[builder(flag)]\n"
        "struct Other { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult result = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(result));
    ASSERT_EQ(result.inputs.size(), 2U);
    ASSERT_EQ(result.generated_item_declarations.size(), 2U);
    ASSERT_EQ(result.declared_generated_names.size(), 2U);
    ASSERT_EQ(result.token_materialization_admissions.size(), 2U);
    ASSERT_EQ(result.generated_token_buffers.size(), 2U);
    ASSERT_EQ(result.parser_admission_gates.size(), 2U);
    ASSERT_EQ(result.parser_admission_diagnostics.size(), 2U);
    ASSERT_EQ(result.parser_admission_report_entries.size(), 2U);
    ASSERT_EQ(result.parser_admission_reports.size(), 1U);

    EXPECT_NE(result.inputs[0].item.value, result.inputs[1].item.value);
    EXPECT_EQ(result.inputs[0].attribute_index, 0U);
    EXPECT_EQ(result.inputs[1].attribute_index, 0U);
    EXPECT_NE(result.generated_item_declarations[0].generated_item_name,
        result.generated_item_declarations[1].generated_item_name);
    expect_contains(result.generated_item_declarations[0].generated_item_name,
        "__aurex_macro_declared:0:0:0:0:builder");
    expect_contains(result.generated_item_declarations[1].generated_item_name,
        "__aurex_macro_declared:0:0:1:0:builder");
    EXPECT_EQ(result.declared_generated_names[0].declared_name,
        result.generated_item_declarations[0].generated_item_name);
    EXPECT_EQ(result.declared_generated_names[1].declared_name,
        result.generated_item_declarations[1].generated_item_name);
    EXPECT_NE(result.token_materialization_admissions[0].token_stream_name,
        result.token_materialization_admissions[1].token_stream_name);
    expect_contains(result.token_materialization_admissions[0].token_stream_name,
        "m21h-token-stream:0:0:0:0:builder");
    expect_contains(result.token_materialization_admissions[1].token_stream_name,
        "m21h-token-stream:0:0:1:0:builder");
    EXPECT_EQ(result.generated_token_buffers[0].token_stream_name,
        result.token_materialization_admissions[0].token_stream_name);
    EXPECT_EQ(result.generated_token_buffers[1].token_stream_name,
        result.token_materialization_admissions[1].token_stream_name);
    EXPECT_EQ(result.parser_admission_gates[0].token_stream_name,
        result.generated_token_buffers[0].token_stream_name);
    EXPECT_EQ(result.parser_admission_gates[1].token_stream_name,
        result.generated_token_buffers[1].token_stream_name);
    EXPECT_NE(result.parser_admission_gates[0].parse_gate_identity,
        result.parser_admission_gates[1].parse_gate_identity);
    EXPECT_NE(result.parser_admission_diagnostics[0].debug_projection_name,
        result.parser_admission_diagnostics[1].debug_projection_name);
    expect_contains(result.parser_admission_diagnostics[0].debug_projection_name,
        "m21k-parser-admission:0:0:0:0:builder");
    expect_contains(result.parser_admission_diagnostics[1].debug_projection_name,
        "m21k-parser-admission:0:0:1:0:builder");
    EXPECT_NE(result.parser_admission_diagnostics[0].diagnostic_identity,
        result.parser_admission_diagnostics[1].diagnostic_identity);
    EXPECT_NE(result.parser_admission_report_entries[0].report_entry_identity,
        result.parser_admission_report_entries[1].report_entry_identity);
    EXPECT_EQ(result.parser_admission_report_entries[0].query_projection_name,
        "m21l-parser-admission-report:0:0");
    EXPECT_EQ(result.parser_admission_report_entries[1].query_projection_name,
        result.parser_admission_report_entries[0].query_projection_name);
    EXPECT_EQ(result.parser_admission_reports.front().entry_count, 2U);
    EXPECT_EQ(result.parser_admission_reports.front().blocked_entry_count, 2U);
    EXPECT_EQ(result.parser_admission_reports.front().empty_entry_count, 2U);
    EXPECT_EQ(result.parser_admission_reports.front().derive_entry_count, 0U);
    EXPECT_EQ(result.builtin_derive_expansion_admissions.front().query_name,
        "m22a-builtin-derive-admission:0:0:0:0:builder");
    EXPECT_EQ(result.builtin_derive_expansion_admissions.back().query_name,
        "m22a-builtin-derive-admission:0:0:1:0:builder");
    EXPECT_EQ(result.builtin_derive_parser_release_gates.front().admission_count, 2U);
    EXPECT_EQ(result.builtin_derive_parser_release_gates.front().derive_admission_count, 0U);
}

TEST(CoreUnit, EarlyItemExpansionBuiltinDeriveM22CountsDuplicateEnumCapabilities)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq, Eq, Hash)]\n"
        "enum Mode { fast, slow }\n";

    const frontend::macro::EarlyItemExpansionResult result = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(result));
    ASSERT_EQ(result.inputs.size(), 1U);
    ASSERT_EQ(result.builtin_derive_expansion_admissions.size(), 1U);
    ASSERT_EQ(result.builtin_derive_semantic_plans.size(), 1U);
    ASSERT_EQ(result.builtin_derive_parser_release_gates.size(), 1U);

    const frontend::macro::EarlyItemMacroInput& input = result.inputs.front();
    EXPECT_EQ(input.attribute_name, "derive");
    EXPECT_EQ(input.disposition,
        frontend::macro::EarlyItemExpansionDisposition::builtin_derive_passthrough);

    const frontend::macro::BuiltinDeriveExpansionAdmissionGate& admission =
        result.builtin_derive_expansion_admissions.front();
    EXPECT_EQ(admission.admission_kind, "builtin_derive_expansion_candidate");
    EXPECT_EQ(admission.query_name, "m22a-builtin-derive-admission:0:0:0:0:derive");
    EXPECT_EQ(admission.capability_candidate_count, 4U);
    EXPECT_EQ(admission.unsupported_candidate_count, 0U);
    EXPECT_EQ(admission.duplicate_candidate_count, 1U);
    EXPECT_TRUE(admission.builtin_derive_input);
    EXPECT_TRUE(admission.token_records_available);
    EXPECT_FALSE(admission.parser_consumption_enabled);
    EXPECT_FALSE(admission.produced_user_generated_code);

    const frontend::macro::BuiltinDeriveSemanticExpansionPlan& plan =
        result.builtin_derive_semantic_plans.front();
    EXPECT_EQ(plan.target_kind, "enum");
    EXPECT_TRUE(plan.target_struct_or_enum);
    EXPECT_TRUE(plan.uses_existing_builtin_derive_capability_path);
    EXPECT_EQ(plan.capability_count, 4U);
    EXPECT_EQ(plan.copy_capability_count, 1U);
    EXPECT_EQ(plan.eq_capability_count, 2U);
    EXPECT_EQ(plan.hash_capability_count, 1U);
    EXPECT_FALSE(plan.requires_generated_items);
    EXPECT_FALSE(plan.parser_consumption_enabled);

    const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate& release_gate =
        result.builtin_derive_parser_release_gates.front();
    EXPECT_EQ(release_gate.admission_count, 1U);
    EXPECT_EQ(release_gate.derive_admission_count, 1U);
    EXPECT_EQ(release_gate.semantic_plan_count, 1U);
    EXPECT_EQ(release_gate.capability_total_count, 4U);
    EXPECT_EQ(release_gate.parser_consumable_contract_count, 0U);
    EXPECT_TRUE(release_gate.rollback_diagnostics_available);
    EXPECT_FALSE(release_gate.parser_consumption_enabled);

    EXPECT_EQ(result.summary.builtin_derive_expansion_capability_candidate_count, 4U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_capability_count, 4U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_eq_capability_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_hash_capability_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_gate_count, 1U);
    EXPECT_EQ(result.summary.user_generated_code_count, 0U);

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "duplicate_candidates=1");
    expect_contains(dump, "target_kind=enum");
    expect_contains(dump, "capabilities=4");
    expect_contains(dump, "builtin derive parser consumption release remains blocked in M22c");
}

TEST(CoreUnit, EarlyItemExpansionBuiltinDeriveM22ReleaseGatesStayPartLocal)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy)]\n"
        "struct Primary { value: i32; }\n"
        "#[builder(flag)]\n"
        "struct Secondary { value: i32; }\n";

    syntax::AstModule module = parse_success(source);
    assign_single_module_ownership(module);
    ASSERT_EQ(module.items.size(), 2U);
    ASSERT_EQ(module.item_part_indices.size(), module.items.size());
    module.item_part_indices[0] = 0U;
    module.item_part_indices[1] = 1U;
    std::vector<std::vector<query::ModulePartKey>> part_keys = part_key_table(2U);

    auto expanded = frontend::macro::expand_early_item_macros_noop(module, part_keys);
    ASSERT_TRUE(expanded) << expanded.error().message;
    const frontend::macro::EarlyItemExpansionResult result = expanded.take_value();

    ASSERT_TRUE(frontend::macro::is_valid(result));
    ASSERT_EQ(result.inputs.size(), 2U);
    ASSERT_EQ(result.generated_parts.size(), 2U);
    ASSERT_EQ(result.parser_consumption_contract_gates.size(), 2U);
    ASSERT_EQ(result.builtin_derive_expansion_admissions.size(), 2U);
    ASSERT_EQ(result.builtin_derive_semantic_plans.size(), 2U);
    ASSERT_EQ(result.builtin_derive_parser_release_gates.size(), 2U);

    EXPECT_EQ(result.generated_parts[0].source_part_index, 0U);
    EXPECT_EQ(result.generated_parts[0].source_part, part_keys[0][0]);
    EXPECT_EQ(result.generated_parts[1].source_part_index, 1U);
    EXPECT_EQ(result.generated_parts[1].source_part, part_keys[0][1]);

    const frontend::macro::EarlyItemMacroInput& derive_input = result.inputs[0];
    const frontend::macro::EarlyItemMacroInput& builder_input = result.inputs[1];
    EXPECT_EQ(derive_input.part_index, 0U);
    EXPECT_EQ(builder_input.part_index, 1U);
    EXPECT_EQ(derive_input.attribute_name, "derive");
    EXPECT_EQ(builder_input.attribute_name, "builder");

    const frontend::macro::BuiltinDeriveExpansionAdmissionGate* const derive_admission =
        builtin_derive_admission_for_input(result, derive_input);
    ASSERT_NE(derive_admission, nullptr);
    EXPECT_EQ(derive_admission->query_name,
        "m22a-builtin-derive-admission:0:0:0:0:derive");
    EXPECT_EQ(derive_admission->capability_candidate_count, 1U);
    EXPECT_TRUE(derive_admission->builtin_derive_input);

    const frontend::macro::BuiltinDeriveExpansionAdmissionGate* const builder_admission =
        builtin_derive_admission_for_input(result, builder_input);
    ASSERT_NE(builder_admission, nullptr);
    EXPECT_EQ(builder_admission->query_name,
        "m22a-builtin-derive-admission:0:1:1:0:builder");
    EXPECT_EQ(builder_admission->capability_candidate_count, 0U);
    EXPECT_FALSE(builder_admission->builtin_derive_input);

    const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate* const primary_release =
        builtin_derive_parser_release_gate_for_part(result, result.generated_parts[0]);
    ASSERT_NE(primary_release, nullptr);
    EXPECT_EQ(primary_release->release_query_name, "m22c-builtin-derive-parser-release:0:0");
    EXPECT_EQ(primary_release->admission_count, 1U);
    EXPECT_EQ(primary_release->derive_admission_count, 1U);
    EXPECT_EQ(primary_release->semantic_plan_count, 1U);
    EXPECT_EQ(primary_release->capability_total_count, 1U);
    EXPECT_FALSE(primary_release->parser_consumption_enabled);

    const frontend::macro::BuiltinDeriveParserConsumptionReleaseGate* const secondary_release =
        builtin_derive_parser_release_gate_for_part(result, result.generated_parts[1]);
    ASSERT_NE(secondary_release, nullptr);
    EXPECT_EQ(secondary_release->release_query_name, "m22c-builtin-derive-parser-release:0:1");
    EXPECT_EQ(secondary_release->admission_count, 1U);
    EXPECT_EQ(secondary_release->derive_admission_count, 0U);
    EXPECT_EQ(secondary_release->semantic_plan_count, 1U);
    EXPECT_EQ(secondary_release->capability_total_count, 0U);
    EXPECT_FALSE(secondary_release->parser_consumption_enabled);
    EXPECT_NE(primary_release->admission_group_identity,
        secondary_release->admission_group_identity);
    EXPECT_NE(primary_release->semantic_plan_group_identity,
        secondary_release->semantic_plan_group_identity);
    EXPECT_NE(primary_release->release_gate_identity, secondary_release->release_gate_identity);

    EXPECT_EQ(result.summary.builtin_derive_expansion_admission_gate_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_derive_admission_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_expansion_non_derive_blocked_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_plan_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_semantic_capability_count, 1U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_gate_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_parser_release_parser_consumable_count, 0U);

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "query=m22c-builtin-derive-parser-release:0:0");
    expect_contains(dump, "query=m22c-builtin-derive-parser-release:0:1");
    expect_contains(dump, "source_part=1");
    expect_contains(dump, "part=1");
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsNoopBoundaryDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));

    frontend::macro::EarlyItemExpansionResult generated_code = baseline;
    ASSERT_FALSE(generated_code.generated_parts.empty());
    generated_code.generated_parts.front().produced_user_generated_code = true;
    refresh_expansion_result(generated_code);
    EXPECT_FALSE(frontend::macro::is_valid(generated_code));

    frontend::macro::EarlyItemExpansionResult parsed_part = baseline;
    ASSERT_FALSE(parsed_part.generated_parts.empty());
    parsed_part.generated_parts.front().parsed = true;
    refresh_expansion_result(parsed_part);
    EXPECT_FALSE(frontend::macro::is_valid(parsed_part));

    frontend::macro::EarlyItemExpansionResult merged_part = baseline;
    ASSERT_FALSE(merged_part.generated_parts.empty());
    merged_part.generated_parts.front().merged = true;
    refresh_expansion_result(merged_part);
    EXPECT_FALSE(frontend::macro::is_valid(merged_part));

    frontend::macro::EarlyItemExpansionResult missing_stub = baseline;
    ASSERT_FALSE(missing_stub.generated_part_stubs.empty());
    missing_stub.generated_part_stubs.clear();
    refresh_expansion_result(missing_stub);
    EXPECT_FALSE(frontend::macro::is_valid(missing_stub));

    frontend::macro::EarlyItemExpansionResult parse_blocked_stub = baseline;
    ASSERT_FALSE(parse_blocked_stub.generated_part_stubs.empty());
    parse_blocked_stub.generated_part_stubs.front().lifecycle_state =
        frontend::macro::GeneratedModulePartLifecycleState::parse_blocked;
    refresh_expansion_result(parse_blocked_stub);
    EXPECT_EQ(parse_blocked_stub.summary.parse_blocked_count, 1U);
    EXPECT_EQ(parse_blocked_stub.summary.merge_blocked_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(parse_blocked_stub));

    frontend::macro::EarlyItemExpansionResult invalid_lifecycle_stub = baseline;
    ASSERT_FALSE(invalid_lifecycle_stub.generated_part_stubs.empty());
    invalid_lifecycle_stub.generated_part_stubs.front().lifecycle_state =
        static_cast<frontend::macro::GeneratedModulePartLifecycleState>(
            EARLY_ITEM_EXPANSION_TEST_INVALID_LIFECYCLE_STATE);
    refresh_expansion_result(invalid_lifecycle_stub);
    EXPECT_FALSE(frontend::macro::is_valid(invalid_lifecycle_stub));

    frontend::macro::EarlyItemExpansionResult non_materialized_stub = baseline;
    ASSERT_FALSE(non_materialized_stub.generated_part_stubs.empty());
    non_materialized_stub.generated_part_stubs.front().materialized_buffer = false;
    refresh_expansion_result(non_materialized_stub);
    EXPECT_EQ(non_materialized_stub.summary.materialized_buffer_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(non_materialized_stub));

    frontend::macro::EarlyItemExpansionResult parsed_stub = baseline;
    ASSERT_FALSE(parsed_stub.generated_part_stubs.empty());
    parsed_stub.generated_part_stubs.front().parsed = true;
    refresh_expansion_result(parsed_stub);
    EXPECT_EQ(parsed_stub.summary.parsed_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parsed_stub));

    frontend::macro::EarlyItemExpansionResult merged_stub = baseline;
    ASSERT_FALSE(merged_stub.generated_part_stubs.empty());
    merged_stub.generated_part_stubs.front().merged = true;
    refresh_expansion_result(merged_stub);
    EXPECT_EQ(merged_stub.summary.merged_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(merged_stub));

    frontend::macro::EarlyItemExpansionResult sema_visible_stub = baseline;
    ASSERT_FALSE(sema_visible_stub.generated_part_stubs.empty());
    sema_visible_stub.generated_part_stubs.front().sema_visible = true;
    refresh_expansion_result(sema_visible_stub);
    EXPECT_EQ(sema_visible_stub.summary.sema_visible_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(sema_visible_stub));

    frontend::macro::EarlyItemExpansionResult generated_code_stub = baseline;
    ASSERT_FALSE(generated_code_stub.generated_part_stubs.empty());
    generated_code_stub.generated_part_stubs.front().produced_user_generated_code = true;
    refresh_expansion_result(generated_code_stub);
    EXPECT_EQ(generated_code_stub.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_code_stub));

    frontend::macro::EarlyItemExpansionResult empty_buffer_identity = baseline;
    ASSERT_FALSE(empty_buffer_identity.generated_part_stubs.empty());
    empty_buffer_identity.generated_part_stubs.front().generated_buffer_identity = {};
    refresh_expansion_result(empty_buffer_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_buffer_identity));

    frontend::macro::EarlyItemExpansionResult empty_parse_config = baseline;
    ASSERT_FALSE(empty_parse_config.generated_part_stubs.empty());
    empty_parse_config.generated_part_stubs.front().parse_config_fingerprint = {};
    refresh_expansion_result(empty_parse_config);
    EXPECT_FALSE(frontend::macro::is_valid(empty_parse_config));

    frontend::macro::EarlyItemExpansionResult empty_merge_ordering = baseline;
    ASSERT_FALSE(empty_merge_ordering.generated_part_stubs.empty());
    empty_merge_ordering.generated_part_stubs.front().merge_ordering_key = {};
    refresh_expansion_result(empty_merge_ordering);
    EXPECT_FALSE(frontend::macro::is_valid(empty_merge_ordering));

    frontend::macro::EarlyItemExpansionResult empty_origin = baseline;
    ASSERT_FALSE(empty_origin.generated_part_stubs.empty());
    empty_origin.generated_part_stubs.front().expansion_origin = {};
    refresh_expansion_result(empty_origin);
    EXPECT_FALSE(frontend::macro::is_valid(empty_origin));

    frontend::macro::EarlyItemExpansionResult empty_buffer_name = baseline;
    ASSERT_FALSE(empty_buffer_name.generated_part_stubs.empty());
    empty_buffer_name.generated_part_stubs.front().generated_buffer_name.clear();
    refresh_expansion_result(empty_buffer_name);
    EXPECT_FALSE(frontend::macro::is_valid(empty_buffer_name));

    frontend::macro::EarlyItemExpansionResult empty_blocker = baseline;
    ASSERT_FALSE(empty_blocker.generated_part_stubs.empty());
    empty_blocker.generated_part_stubs.front().blocker_reason.clear();
    refresh_expansion_result(empty_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(empty_blocker));

    frontend::macro::EarlyItemExpansionResult wrong_source_part = baseline;
    ASSERT_FALSE(wrong_source_part.generated_part_stubs.empty());
    wrong_source_part.generated_part_stubs.front().source_part =
        wrong_source_part.generated_part_stubs.front().generated_part;
    refresh_expansion_result(wrong_source_part);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_source_part));

    frontend::macro::EarlyItemExpansionResult real_source_map = baseline;
    ASSERT_FALSE(real_source_map.source_maps.empty());
    real_source_map.source_maps.front().real_source_map = true;
    refresh_expansion_result(real_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(real_source_map));

    frontend::macro::EarlyItemExpansionResult source_map_debug_trace = baseline;
    ASSERT_FALSE(source_map_debug_trace.source_maps.empty());
    source_map_debug_trace.source_maps.front().debug_trace_available = true;
    refresh_expansion_result(source_map_debug_trace);
    EXPECT_FALSE(frontend::macro::is_valid(source_map_debug_trace));

    frontend::macro::EarlyItemExpansionResult stale_summary = baseline;
    stale_summary.summary.macro_input_count += 1U;
    EXPECT_FALSE(frontend::macro::is_valid(stale_summary));

    frontend::macro::EarlyItemExpansionResult stale_fingerprint = baseline;
    stale_fingerprint.fingerprint = query::stable_fingerprint("stale early item expansion");
    EXPECT_FALSE(frontend::macro::is_valid(stale_fingerprint));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParseMergeStubContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.generated_part_stubs.empty());

    frontend::macro::EarlyItemExpansionResult buffer_identity = baseline;
    buffer_identity.generated_part_stubs.front().generated_buffer_identity =
        query::stable_fingerprint("different generated buffer identity");
    refresh_expansion_result(buffer_identity);
    EXPECT_NE(buffer_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(buffer_identity));

    frontend::macro::EarlyItemExpansionResult merge_order = baseline;
    merge_order.generated_part_stubs.front().merge_ordering_key =
        query::stable_fingerprint("different merge ordering");
    refresh_expansion_result(merge_order);
    EXPECT_NE(merge_order.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(merge_order));

    frontend::macro::EarlyItemExpansionResult blocker = baseline;
    blocker.generated_part_stubs.front().blocker_reason = "different blocker";
    refresh_expansion_result(blocker);
    EXPECT_NE(blocker.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(blocker));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsHygieneAndTraceStubDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.source_maps.empty());
    ASSERT_FALSE(baseline.hygiene_stubs.empty());
    ASSERT_FALSE(baseline.trace_stubs.empty());

    frontend::macro::EarlyItemExpansionResult missing_source_map = baseline;
    missing_source_map.source_maps.clear();
    refresh_expansion_result(missing_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(missing_source_map));

    frontend::macro::EarlyItemExpansionResult missing_hygiene = baseline;
    missing_hygiene.hygiene_stubs.clear();
    refresh_expansion_result(missing_hygiene);
    EXPECT_EQ(missing_hygiene.summary.hygiene_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_hygiene));

    frontend::macro::EarlyItemExpansionResult empty_call_site_mark = baseline;
    empty_call_site_mark.hygiene_stubs.front().call_site_mark = {};
    refresh_expansion_result(empty_call_site_mark);
    EXPECT_FALSE(frontend::macro::is_valid(empty_call_site_mark));

    frontend::macro::EarlyItemExpansionResult empty_definition_site_mark = baseline;
    empty_definition_site_mark.hygiene_stubs.front().definition_site_mark = {};
    refresh_expansion_result(empty_definition_site_mark);
    EXPECT_FALSE(frontend::macro::is_valid(empty_definition_site_mark));

    frontend::macro::EarlyItemExpansionResult empty_generated_fresh_mark = baseline;
    empty_generated_fresh_mark.hygiene_stubs.front().generated_fresh_mark = {};
    refresh_expansion_result(empty_generated_fresh_mark);
    EXPECT_FALSE(frontend::macro::is_valid(empty_generated_fresh_mark));

    frontend::macro::EarlyItemExpansionResult empty_declared_name_set = baseline;
    empty_declared_name_set.hygiene_stubs.front().declared_name_set = {};
    refresh_expansion_result(empty_declared_name_set);
    EXPECT_EQ(empty_declared_name_set.summary.declared_name_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(empty_declared_name_set));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene_origin = baseline;
    wrong_hygiene_origin.hygiene_stubs.front().expansion_origin =
        query::stable_fingerprint("wrong hygiene origin");
    refresh_expansion_result(wrong_hygiene_origin);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_origin));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene_policy = baseline;
    wrong_hygiene_policy.hygiene_stubs.front().policy = "wrong_hygiene_policy";
    refresh_expansion_result(wrong_hygiene_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_policy));

    frontend::macro::EarlyItemExpansionResult resolved_hygiene = baseline;
    resolved_hygiene.hygiene_stubs.front().resolved = true;
    refresh_expansion_result(resolved_hygiene);
    EXPECT_EQ(resolved_hygiene.summary.unresolved_hygiene_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(resolved_hygiene));

    frontend::macro::EarlyItemExpansionResult visible_declared_names = baseline;
    visible_declared_names.hygiene_stubs.front().declared_names_visible = true;
    refresh_expansion_result(visible_declared_names);
    EXPECT_FALSE(frontend::macro::is_valid(visible_declared_names));

    frontend::macro::EarlyItemExpansionResult call_site_capture = baseline;
    call_site_capture.hygiene_stubs.front().captures_call_site_locals = true;
    refresh_expansion_result(call_site_capture);
    EXPECT_EQ(call_site_capture.summary.call_site_capture_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(call_site_capture));

    frontend::macro::EarlyItemExpansionResult missing_trace = baseline;
    missing_trace.trace_stubs.clear();
    refresh_expansion_result(missing_trace);
    EXPECT_EQ(missing_trace.summary.trace_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_trace));

    frontend::macro::EarlyItemExpansionResult empty_trace_identity = baseline;
    empty_trace_identity.trace_stubs.front().trace_identity = {};
    refresh_expansion_result(empty_trace_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_trace_identity));

    frontend::macro::EarlyItemExpansionResult empty_generated_source_map = baseline;
    empty_generated_source_map.trace_stubs.front().generated_source_map_identity = {};
    refresh_expansion_result(empty_generated_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(empty_generated_source_map));

    frontend::macro::EarlyItemExpansionResult empty_diagnostic_anchor = baseline;
    empty_diagnostic_anchor.trace_stubs.front().diagnostic_anchor = {};
    refresh_expansion_result(empty_diagnostic_anchor);
    EXPECT_FALSE(frontend::macro::is_valid(empty_diagnostic_anchor));

    frontend::macro::EarlyItemExpansionResult wrong_trace_policy = baseline;
    wrong_trace_policy.trace_stubs.front().trace_policy = "wrong_trace_policy";
    refresh_expansion_result(wrong_trace_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace_policy));

    frontend::macro::EarlyItemExpansionResult wrong_trace_blocker = baseline;
    wrong_trace_blocker.trace_stubs.front().blocker_reason = "wrong blocker";
    refresh_expansion_result(wrong_trace_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace_blocker));

    frontend::macro::EarlyItemExpansionResult trace_real_source_map = baseline;
    trace_real_source_map.trace_stubs.front().real_source_map = true;
    refresh_expansion_result(trace_real_source_map);
    EXPECT_EQ(trace_real_source_map.summary.real_source_map_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(trace_real_source_map));

    frontend::macro::EarlyItemExpansionResult trace_debug_available = baseline;
    trace_debug_available.trace_stubs.front().debug_trace_available = true;
    refresh_expansion_result(trace_debug_available);
    EXPECT_EQ(trace_debug_available.summary.debug_trace_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(trace_debug_available));

    frontend::macro::EarlyItemExpansionResult trace_cli_emit = baseline;
    trace_cli_emit.trace_stubs.front().cli_emit_expanded_available = true;
    refresh_expansion_result(trace_cli_emit);
    EXPECT_EQ(trace_cli_emit.summary.cli_emit_expanded_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(trace_cli_emit));

    frontend::macro::EarlyItemExpansionResult wrong_trace_origin = baseline;
    wrong_trace_origin.trace_stubs.front().expansion_origin =
        query::stable_fingerprint("wrong trace origin");
    refresh_expansion_result(wrong_trace_origin);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace_origin));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksHygieneAndTraceStubContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.hygiene_stubs.empty());
    ASSERT_FALSE(baseline.trace_stubs.empty());

    frontend::macro::EarlyItemExpansionResult hygiene_mark = baseline;
    hygiene_mark.hygiene_stubs.front().call_site_mark =
        query::stable_fingerprint("different call site mark");
    refresh_expansion_result(hygiene_mark);
    EXPECT_NE(hygiene_mark.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(hygiene_mark));

    frontend::macro::EarlyItemExpansionResult declared_names = baseline;
    declared_names.hygiene_stubs.front().declared_name_set =
        query::stable_fingerprint("different declared name set");
    refresh_expansion_result(declared_names);
    EXPECT_NE(declared_names.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(declared_names));

    frontend::macro::EarlyItemExpansionResult trace_identity = baseline;
    trace_identity.trace_stubs.front().trace_identity =
        query::stable_fingerprint("different trace identity");
    refresh_expansion_result(trace_identity);
    EXPECT_NE(trace_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(trace_identity));

    frontend::macro::EarlyItemExpansionResult source_map_identity = baseline;
    source_map_identity.trace_stubs.front().generated_source_map_identity =
        query::stable_fingerprint("different generated source map identity");
    refresh_expansion_result(source_map_identity);
    EXPECT_NE(source_map_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(source_map_identity));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsGeneratedItemAndDeclaredNameDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.generated_item_declarations.empty());
    ASSERT_FALSE(baseline.declared_generated_names.empty());

    frontend::macro::EarlyItemExpansionResult missing_declaration = baseline;
    missing_declaration.generated_item_declarations.clear();
    refresh_expansion_result(missing_declaration);
    EXPECT_EQ(missing_declaration.summary.generated_item_declaration_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_declaration));

    frontend::macro::EarlyItemExpansionResult missing_declared_name = baseline;
    missing_declared_name.declared_generated_names.clear();
    refresh_expansion_result(missing_declared_name);
    EXPECT_EQ(missing_declared_name.summary.declared_generated_name_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_declared_name));

    frontend::macro::EarlyItemExpansionResult empty_declaration_identity = baseline;
    empty_declaration_identity.generated_item_declarations.front().declaration_identity = {};
    refresh_expansion_result(empty_declaration_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_declaration_identity));

    frontend::macro::EarlyItemExpansionResult empty_generated_item_key = baseline;
    empty_generated_item_key.generated_item_declarations.front().generated_item_key = {};
    refresh_expansion_result(empty_generated_item_key);
    EXPECT_FALSE(frontend::macro::is_valid(empty_generated_item_key));

    frontend::macro::EarlyItemExpansionResult wrong_declaration_role = baseline;
    wrong_declaration_role.generated_item_declarations.front().declaration_role = "wrong_role";
    refresh_expansion_result(wrong_declaration_role);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declaration_role));

    frontend::macro::EarlyItemExpansionResult wrong_generated_item_name = baseline;
    wrong_generated_item_name.generated_item_declarations.front().generated_item_name =
        "__aurex_macro_declared:wrong";
    refresh_expansion_result(wrong_generated_item_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_item_name));

    frontend::macro::EarlyItemExpansionResult materialized_declaration = baseline;
    materialized_declaration.generated_item_declarations.front().materialized_tokens = true;
    refresh_expansion_result(materialized_declaration);
    EXPECT_EQ(materialized_declaration.summary.materialized_generated_item_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(materialized_declaration));

    frontend::macro::EarlyItemExpansionResult unplanned_declaration = baseline;
    unplanned_declaration.generated_item_declarations.front().planned = false;
    refresh_expansion_result(unplanned_declaration);
    EXPECT_EQ(unplanned_declaration.summary.planned_generated_item_declaration_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(unplanned_declaration));

    frontend::macro::EarlyItemExpansionResult parsed_declaration = baseline;
    parsed_declaration.generated_item_declarations.front().parsed = true;
    refresh_expansion_result(parsed_declaration);
    EXPECT_EQ(parsed_declaration.summary.parsed_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parsed_declaration));

    frontend::macro::EarlyItemExpansionResult sema_visible_declaration = baseline;
    sema_visible_declaration.generated_item_declarations.front().sema_visible = true;
    refresh_expansion_result(sema_visible_declaration);
    EXPECT_EQ(sema_visible_declaration.summary.sema_visible_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(sema_visible_declaration));

    frontend::macro::EarlyItemExpansionResult wrong_declaration_name_set = baseline;
    wrong_declaration_name_set.generated_item_declarations.front().declared_name_set =
        query::stable_fingerprint("wrong declaration name set");
    refresh_expansion_result(wrong_declaration_name_set);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declaration_name_set));

    frontend::macro::EarlyItemExpansionResult empty_declared_name_identity = baseline;
    empty_declared_name_identity.declared_generated_names.front().declared_name_identity = {};
    refresh_expansion_result(empty_declared_name_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_declared_name_identity));

    frontend::macro::EarlyItemExpansionResult empty_hygiene_mark = baseline;
    empty_hygiene_mark.declared_generated_names.front().hygiene_mark = {};
    refresh_expansion_result(empty_hygiene_mark);
    EXPECT_FALSE(frontend::macro::is_valid(empty_hygiene_mark));

    frontend::macro::EarlyItemExpansionResult wrong_namespace = baseline;
    wrong_namespace.declared_generated_names.front().namespace_kind = "value";
    refresh_expansion_result(wrong_namespace);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_namespace));

    frontend::macro::EarlyItemExpansionResult wrong_declared_name = baseline;
    wrong_declared_name.declared_generated_names.front().declared_name =
        "__aurex_macro_declared:wrong";
    refresh_expansion_result(wrong_declared_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declared_name));

    frontend::macro::EarlyItemExpansionResult lookup_visible = baseline;
    lookup_visible.declared_generated_names.front().lookup_visible = true;
    refresh_expansion_result(lookup_visible);
    EXPECT_EQ(lookup_visible.summary.lookup_visible_declared_name_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(lookup_visible));

    frontend::macro::EarlyItemExpansionResult export_visible = baseline;
    export_visible.declared_generated_names.front().export_visible = true;
    refresh_expansion_result(export_visible);
    EXPECT_EQ(export_visible.summary.export_visible_declared_name_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(export_visible));

    frontend::macro::EarlyItemExpansionResult sema_visible_name = baseline;
    sema_visible_name.declared_generated_names.front().sema_visible = true;
    refresh_expansion_result(sema_visible_name);
    EXPECT_EQ(sema_visible_name.summary.sema_visible_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(sema_visible_name));

    frontend::macro::EarlyItemExpansionResult user_code_name = baseline;
    user_code_name.declared_generated_names.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code_name);
    EXPECT_EQ(user_code_name.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code_name));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksGeneratedItemAndDeclaredNameContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.generated_item_declarations.empty());
    ASSERT_FALSE(baseline.declared_generated_names.empty());

    frontend::macro::EarlyItemExpansionResult declaration_identity = baseline;
    declaration_identity.generated_item_declarations.front().declaration_identity =
        query::stable_fingerprint("different declaration identity");
    refresh_expansion_result(declaration_identity);
    EXPECT_NE(declaration_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(declaration_identity));

    frontend::macro::EarlyItemExpansionResult generated_item_key = baseline;
    generated_item_key.generated_item_declarations.front().generated_item_key =
        query::stable_fingerprint("different generated item key");
    refresh_expansion_result(generated_item_key);
    EXPECT_NE(generated_item_key.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(generated_item_key));

    frontend::macro::EarlyItemExpansionResult declared_name_identity = baseline;
    declared_name_identity.declared_generated_names.front().declared_name_identity =
        query::stable_fingerprint("different declared name identity");
    refresh_expansion_result(declared_name_identity);
    EXPECT_NE(declared_name_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(declared_name_identity));

    frontend::macro::EarlyItemExpansionResult hygiene_mark = baseline;
    hygiene_mark.declared_generated_names.front().hygiene_mark =
        query::stable_fingerprint("different declared name hygiene mark");
    refresh_expansion_result(hygiene_mark);
    EXPECT_NE(hygiene_mark.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(hygiene_mark));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsTokenMaterializationAdmissionDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.token_materialization_admissions.empty());
    ASSERT_FALSE(baseline.generated_token_buffers.empty());

    frontend::macro::EarlyItemExpansionResult missing_admission = baseline;
    missing_admission.token_materialization_admissions.clear();
    refresh_expansion_result(missing_admission);
    EXPECT_EQ(missing_admission.summary.token_materialization_admission_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_admission));

    frontend::macro::EarlyItemExpansionResult missing_buffer = baseline;
    missing_buffer.generated_token_buffers.clear();
    refresh_expansion_result(missing_buffer);
    EXPECT_EQ(missing_buffer.summary.generated_token_buffer_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_buffer));

    frontend::macro::EarlyItemExpansionResult empty_token_plan = baseline;
    empty_token_plan.token_materialization_admissions.front().token_plan_identity = {};
    refresh_expansion_result(empty_token_plan);
    EXPECT_FALSE(frontend::macro::is_valid(empty_token_plan));

    frontend::macro::EarlyItemExpansionResult empty_token_buffer_identity = baseline;
    empty_token_buffer_identity.token_materialization_admissions.front().token_buffer_identity = {};
    refresh_expansion_result(empty_token_buffer_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_token_buffer_identity));

    frontend::macro::EarlyItemExpansionResult wrong_declaration_identity = baseline;
    wrong_declaration_identity.token_materialization_admissions.front().declaration_identity =
        query::stable_fingerprint("wrong admission declaration identity");
    refresh_expansion_result(wrong_declaration_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declaration_identity));

    frontend::macro::EarlyItemExpansionResult wrong_generated_item_key = baseline;
    wrong_generated_item_key.token_materialization_admissions.front().generated_item_key =
        query::stable_fingerprint("wrong admission generated item key");
    refresh_expansion_result(wrong_generated_item_key);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_item_key));

    frontend::macro::EarlyItemExpansionResult wrong_declared_name_identity = baseline;
    wrong_declared_name_identity.token_materialization_admissions.front().declared_name_identity =
        query::stable_fingerprint("wrong admission declared name identity");
    refresh_expansion_result(wrong_declared_name_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declared_name_identity));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene_mark = baseline;
    wrong_hygiene_mark.token_materialization_admissions.front().hygiene_mark =
        query::stable_fingerprint("wrong admission hygiene mark");
    refresh_expansion_result(wrong_hygiene_mark);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_mark));

    frontend::macro::EarlyItemExpansionResult wrong_source_map = baseline;
    wrong_source_map.token_materialization_admissions.front().source_map_identity =
        query::stable_fingerprint("wrong admission source map");
    refresh_expansion_result(wrong_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_source_map));

    frontend::macro::EarlyItemExpansionResult wrong_trace_identity = baseline;
    wrong_trace_identity.token_materialization_admissions.front().trace_identity =
        query::stable_fingerprint("wrong admission trace identity");
    refresh_expansion_result(wrong_trace_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace_identity));

    frontend::macro::EarlyItemExpansionResult wrong_policy = baseline;
    wrong_policy.token_materialization_admissions.front().admission_policy = "wrong_admission_policy";
    refresh_expansion_result(wrong_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_policy));

    frontend::macro::EarlyItemExpansionResult wrong_stream_name = baseline;
    wrong_stream_name.token_materialization_admissions.front().token_stream_name =
        "m21h-token-stream:wrong";
    refresh_expansion_result(wrong_stream_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_stream_name));

    frontend::macro::EarlyItemExpansionResult wrong_blocker = baseline;
    wrong_blocker.token_materialization_admissions.front().blocker_reason = "wrong blocker";
    refresh_expansion_result(wrong_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_blocker));

    frontend::macro::EarlyItemExpansionResult not_compiler_owned = baseline;
    not_compiler_owned.token_materialization_admissions.front().compiler_owned = false;
    refresh_expansion_result(not_compiler_owned);
    EXPECT_EQ(not_compiler_owned.summary.compiler_owned_admission_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(not_compiler_owned));

    frontend::macro::EarlyItemExpansionResult not_admitted = baseline;
    not_admitted.token_materialization_admissions.front().admitted = false;
    refresh_expansion_result(not_admitted);
    EXPECT_EQ(not_admitted.summary.admitted_token_materialization_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(not_admitted));

    frontend::macro::EarlyItemExpansionResult materialized_admission = baseline;
    materialized_admission.token_materialization_admissions.front().materialized_tokens = true;
    refresh_expansion_result(materialized_admission);
    EXPECT_EQ(materialized_admission.summary.materialized_token_admission_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(materialized_admission));

    frontend::macro::EarlyItemExpansionResult generated_text = baseline;
    generated_text.token_materialization_admissions.front().generated_source_text = true;
    refresh_expansion_result(generated_text);
    EXPECT_EQ(generated_text.summary.generated_source_text_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_text));

    frontend::macro::EarlyItemExpansionResult parse_ready = baseline;
    parse_ready.token_materialization_admissions.front().parse_ready = true;
    refresh_expansion_result(parse_ready);
    EXPECT_EQ(parse_ready.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parse_ready));

    frontend::macro::EarlyItemExpansionResult external_process = baseline;
    external_process.token_materialization_admissions.front().external_process_required = true;
    refresh_expansion_result(external_process);
    EXPECT_EQ(external_process.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(external_process));

    frontend::macro::EarlyItemExpansionResult standard_library = baseline;
    standard_library.token_materialization_admissions.front().standard_library_required = true;
    refresh_expansion_result(standard_library);
    EXPECT_EQ(standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(standard_library));

    frontend::macro::EarlyItemExpansionResult runtime = baseline;
    runtime.token_materialization_admissions.front().runtime_required = true;
    refresh_expansion_result(runtime);
    EXPECT_EQ(runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(runtime));

    frontend::macro::EarlyItemExpansionResult user_code = baseline;
    user_code.token_materialization_admissions.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code);
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsGeneratedTokenBufferDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.generated_token_buffers.empty());

    frontend::macro::EarlyItemExpansionResult empty_token_plan = baseline;
    empty_token_plan.generated_token_buffers.front().token_plan_identity = {};
    refresh_expansion_result(empty_token_plan);
    EXPECT_FALSE(frontend::macro::is_valid(empty_token_plan));

    frontend::macro::EarlyItemExpansionResult empty_token_buffer_identity = baseline;
    empty_token_buffer_identity.generated_token_buffers.front().token_buffer_identity = {};
    refresh_expansion_result(empty_token_buffer_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_token_buffer_identity));

    frontend::macro::EarlyItemExpansionResult wrong_source_map = baseline;
    wrong_source_map.generated_token_buffers.front().source_map_identity =
        query::stable_fingerprint("wrong token buffer source map");
    refresh_expansion_result(wrong_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_source_map));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene_mark = baseline;
    wrong_hygiene_mark.generated_token_buffers.front().hygiene_mark =
        query::stable_fingerprint("wrong token buffer hygiene mark");
    refresh_expansion_result(wrong_hygiene_mark);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_mark));

    frontend::macro::EarlyItemExpansionResult wrong_stream_name = baseline;
    wrong_stream_name.generated_token_buffers.front().token_stream_name =
        "m21h-token-stream:wrong";
    refresh_expansion_result(wrong_stream_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_stream_name));

    frontend::macro::EarlyItemExpansionResult wrong_kind = baseline;
    wrong_kind.generated_token_buffers.front().token_buffer_kind = "wrong_token_buffer_kind";
    refresh_expansion_result(wrong_kind);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_kind));

    frontend::macro::EarlyItemExpansionResult wrong_producer_policy = baseline;
    wrong_producer_policy.generated_token_buffers.front().token_producer_policy =
        "wrong_token_producer_policy";
    refresh_expansion_result(wrong_producer_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_producer_policy));

    frontend::macro::EarlyItemExpansionResult wrong_blocker = baseline;
    wrong_blocker.generated_token_buffers.front().blocker_reason = "wrong blocker";
    refresh_expansion_result(wrong_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_blocker));

    frontend::macro::EarlyItemExpansionResult empty_materialization_identity = baseline;
    empty_materialization_identity.generated_token_buffers.front().materialization_identity = {};
    refresh_expansion_result(empty_materialization_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_materialization_identity));

    frontend::macro::EarlyItemExpansionResult non_empty_token_count = baseline;
    non_empty_token_count.generated_token_buffers.front().token_count = 1U;
    refresh_expansion_result(non_empty_token_count);
    EXPECT_FALSE(frontend::macro::is_valid(non_empty_token_count));

    frontend::macro::EarlyItemExpansionResult non_empty_buffer = baseline;
    non_empty_buffer.generated_token_buffers.front().empty = false;
    refresh_expansion_result(non_empty_buffer);
    EXPECT_EQ(non_empty_buffer.summary.empty_generated_token_buffer_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(non_empty_buffer));

    frontend::macro::EarlyItemExpansionResult materialized_tokens = baseline;
    materialized_tokens.generated_token_buffers.front().materialized_tokens = true;
    refresh_expansion_result(materialized_tokens);
    EXPECT_EQ(materialized_tokens.summary.materialized_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(materialized_tokens));

    frontend::macro::EarlyItemExpansionResult generated_text = baseline;
    generated_text.generated_token_buffers.front().generated_source_text = true;
    refresh_expansion_result(generated_text);
    EXPECT_EQ(generated_text.summary.generated_source_text_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_text));

    frontend::macro::EarlyItemExpansionResult parser_consumable = baseline;
    parser_consumable.generated_token_buffers.front().parser_consumable = true;
    refresh_expansion_result(parser_consumable);
    EXPECT_EQ(parser_consumable.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_consumable));

    frontend::macro::EarlyItemExpansionResult user_code = baseline;
    user_code.generated_token_buffers.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code);
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsDeriveGeneratedTokenPrototypeDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.inputs.size(), 1U);
    ASSERT_EQ(baseline.token_materialization_admissions.size(), 1U);
    ASSERT_EQ(baseline.generated_token_buffers.size(), 1U);
    ASSERT_FALSE(baseline.generated_token_records.empty());
    EXPECT_TRUE(baseline.token_materialization_admissions.front().materialized_tokens);
    EXPECT_TRUE(baseline.generated_token_buffers.front().materialized_tokens);
    EXPECT_FALSE(baseline.generated_token_buffers.front().empty);
    EXPECT_EQ(baseline.summary.generated_token_record_count,
        baseline.inputs.front().token_count + EARLY_ITEM_EXPANSION_TEST_DERIVE_SENTINEL_TOKEN_COUNT);

    frontend::macro::EarlyItemExpansionResult non_materialized_admission = baseline;
    non_materialized_admission.token_materialization_admissions.front().materialized_tokens = false;
    refresh_expansion_result(non_materialized_admission);
    EXPECT_EQ(non_materialized_admission.summary.materialized_token_admission_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(non_materialized_admission));

    frontend::macro::EarlyItemExpansionResult empty_derive_buffer = baseline;
    empty_derive_buffer.generated_token_buffers.front().empty = true;
    refresh_expansion_result(empty_derive_buffer);
    EXPECT_EQ(empty_derive_buffer.summary.empty_generated_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(empty_derive_buffer));

    frontend::macro::EarlyItemExpansionResult non_materialized_buffer = baseline;
    non_materialized_buffer.generated_token_buffers.front().materialized_tokens = false;
    refresh_expansion_result(non_materialized_buffer);
    EXPECT_EQ(non_materialized_buffer.summary.materialized_token_buffer_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(non_materialized_buffer));

    frontend::macro::EarlyItemExpansionResult wrong_token_count = baseline;
    wrong_token_count.generated_token_buffers.front().token_count = 0U;
    refresh_expansion_result(wrong_token_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_count));

    frontend::macro::EarlyItemExpansionResult wrong_derive_policy = baseline;
    wrong_derive_policy.generated_token_buffers.front().token_producer_policy =
        "compiler_owned_blocked_empty_token_producer_v1";
    refresh_expansion_result(wrong_derive_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_derive_policy));

    frontend::macro::EarlyItemExpansionResult wrong_materialization_identity = baseline;
    wrong_materialization_identity.generated_token_buffers.front().materialization_identity =
        query::stable_fingerprint("wrong derive materialization identity");
    refresh_expansion_result(wrong_materialization_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_materialization_identity));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsGeneratedTokenRecordDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    const base::usize derive_record_index = first_record_index_for_attribute(baseline, "derive");
    ASSERT_LT(derive_record_index, baseline.generated_token_records.size());

    frontend::macro::EarlyItemExpansionResult missing_records = baseline;
    missing_records.generated_token_records.clear();
    refresh_expansion_result(missing_records);
    EXPECT_EQ(missing_records.summary.generated_token_record_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_records));

    frontend::macro::EarlyItemExpansionResult parser_visible = baseline;
    parser_visible.generated_token_records[derive_record_index].parser_visible = true;
    refresh_expansion_result(parser_visible);
    EXPECT_EQ(parser_visible.summary.parser_visible_generated_token_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_visible));

    frontend::macro::EarlyItemExpansionResult user_generated_code = baseline;
    user_generated_code.generated_token_records[derive_record_index].produced_user_generated_code = true;
    refresh_expansion_result(user_generated_code);
    EXPECT_EQ(user_generated_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_generated_code));

    frontend::macro::EarlyItemExpansionResult wrong_token_identity = baseline;
    wrong_token_identity.generated_token_records[derive_record_index].token_identity =
        query::stable_fingerprint("wrong generated token identity");
    refresh_expansion_result(wrong_token_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_identity));

    frontend::macro::EarlyItemExpansionResult wrong_begin_text = baseline;
    wrong_begin_text.generated_token_records[derive_record_index].text =
        "__aurex_builtin_derive_wrong_begin";
    refresh_expansion_result(wrong_begin_text);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_begin_text));

    frontend::macro::EarlyItemExpansionResult wrong_begin_role = baseline;
    wrong_begin_role.generated_token_records[derive_record_index].token_role =
        "wrong_generated_token_role";
    refresh_expansion_result(wrong_begin_role);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_begin_role));

    frontend::macro::EarlyItemExpansionResult wrong_begin_kind = baseline;
    wrong_begin_kind.generated_token_records[derive_record_index].kind = syntax::TokenKind::integer_literal;
    refresh_expansion_result(wrong_begin_kind);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_begin_kind));

    frontend::macro::EarlyItemExpansionResult invalid_record_kind = baseline;
    invalid_record_kind.generated_token_records[derive_record_index].kind = syntax::TokenKind::invalid;
    refresh_expansion_result(invalid_record_kind);
    EXPECT_FALSE(frontend::macro::is_valid(invalid_record_kind));

    frontend::macro::EarlyItemExpansionResult wrong_record_buffer = baseline;
    wrong_record_buffer.generated_token_records[derive_record_index].token_buffer_identity =
        query::stable_fingerprint("wrong generated token buffer identity");
    refresh_expansion_result(wrong_record_buffer);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_record_buffer));

    frontend::macro::EarlyItemExpansionResult stale_record_fingerprint = baseline;
    stale_record_fingerprint.generated_token_records[derive_record_index].text =
        "__aurex_builtin_derive_wrong_begin";
    refresh_expansion_result(stale_record_fingerprint);
    EXPECT_NE(stale_record_fingerprint.fingerprint, baseline.fingerprint);
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsParserAdmissionGateDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.parser_admission_gates.size(), 1U);
    ASSERT_FALSE(baseline.generated_part_stubs.empty());

    frontend::macro::EarlyItemExpansionResult missing_gate = baseline;
    missing_gate.parser_admission_gates.clear();
    refresh_expansion_result(missing_gate);
    EXPECT_EQ(missing_gate.summary.parser_admission_gate_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_gate));

    frontend::macro::EarlyItemExpansionResult empty_parse_gate_identity = baseline;
    empty_parse_gate_identity.parser_admission_gates.front().parse_gate_identity = {};
    refresh_expansion_result(empty_parse_gate_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_parse_gate_identity));

    frontend::macro::EarlyItemExpansionResult wrong_parse_gate_identity = baseline;
    wrong_parse_gate_identity.parser_admission_gates.front().parse_gate_identity =
        query::stable_fingerprint("wrong parser gate identity");
    refresh_expansion_result(wrong_parse_gate_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_parse_gate_identity));

    frontend::macro::EarlyItemExpansionResult wrong_generated_buffer_identity = baseline;
    wrong_generated_buffer_identity.parser_admission_gates.front().generated_buffer_identity =
        query::stable_fingerprint("wrong parser gate generated buffer identity");
    refresh_expansion_result(wrong_generated_buffer_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_buffer_identity));

    frontend::macro::EarlyItemExpansionResult wrong_parse_config = baseline;
    wrong_parse_config.parser_admission_gates.front().parse_config_fingerprint =
        query::stable_fingerprint("wrong parser gate parse config");
    refresh_expansion_result(wrong_parse_config);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_parse_config));

    frontend::macro::EarlyItemExpansionResult wrong_token_buffer = baseline;
    wrong_token_buffer.parser_admission_gates.front().token_buffer_identity =
        query::stable_fingerprint("wrong parser gate token buffer");
    refresh_expansion_result(wrong_token_buffer);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_buffer));

    frontend::macro::EarlyItemExpansionResult wrong_materialization = baseline;
    wrong_materialization.parser_admission_gates.front().materialization_identity =
        query::stable_fingerprint("wrong parser gate materialization");
    refresh_expansion_result(wrong_materialization);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_materialization));

    frontend::macro::EarlyItemExpansionResult wrong_source_map = baseline;
    wrong_source_map.parser_admission_gates.front().source_map_identity =
        query::stable_fingerprint("wrong parser gate source map");
    refresh_expansion_result(wrong_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_source_map));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene = baseline;
    wrong_hygiene.parser_admission_gates.front().hygiene_mark =
        query::stable_fingerprint("wrong parser gate hygiene");
    refresh_expansion_result(wrong_hygiene);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene));

    frontend::macro::EarlyItemExpansionResult wrong_stream_name = baseline;
    wrong_stream_name.parser_admission_gates.front().token_stream_name =
        "m21h-token-stream:wrong-parser-gate";
    refresh_expansion_result(wrong_stream_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_stream_name));

    frontend::macro::EarlyItemExpansionResult wrong_policy = baseline;
    wrong_policy.parser_admission_gates.front().parser_gate_policy = "wrong_parser_gate_policy";
    refresh_expansion_result(wrong_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_policy));

    frontend::macro::EarlyItemExpansionResult wrong_blocker = baseline;
    wrong_blocker.parser_admission_gates.front().blocker_reason = "wrong parser gate blocker";
    refresh_expansion_result(wrong_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_blocker));

    frontend::macro::EarlyItemExpansionResult not_compiler_owned = baseline;
    not_compiler_owned.parser_admission_gates.front().compiler_owned = false;
    refresh_expansion_result(not_compiler_owned);
    EXPECT_EQ(not_compiler_owned.summary.compiler_owned_parser_admission_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(not_compiler_owned));

    frontend::macro::EarlyItemExpansionResult missing_records_available = baseline;
    missing_records_available.parser_admission_gates.front().token_records_available = false;
    refresh_expansion_result(missing_records_available);
    EXPECT_EQ(missing_records_available.summary.token_record_available_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_records_available));

    frontend::macro::EarlyItemExpansionResult non_materialized = baseline;
    non_materialized.parser_admission_gates.front().token_buffer_materialized = false;
    refresh_expansion_result(non_materialized);
    EXPECT_FALSE(frontend::macro::is_valid(non_materialized));

    frontend::macro::EarlyItemExpansionResult wrong_token_count = baseline;
    wrong_token_count.parser_admission_gates.front().token_count = 0U;
    refresh_expansion_result(wrong_token_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_count));

    frontend::macro::EarlyItemExpansionResult parser_admitted = baseline;
    parser_admitted.parser_admission_gates.front().parser_admitted = true;
    refresh_expansion_result(parser_admitted);
    EXPECT_EQ(parser_admitted.summary.parser_blocked_token_buffer_count, 0U);
    EXPECT_EQ(parser_admitted.summary.parser_admitted_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_admitted));

    frontend::macro::EarlyItemExpansionResult parse_ready = baseline;
    parse_ready.parser_admission_gates.front().parse_ready = true;
    refresh_expansion_result(parse_ready);
    EXPECT_EQ(parse_ready.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parse_ready));

    frontend::macro::EarlyItemExpansionResult parser_consumable = baseline;
    parser_consumable.parser_admission_gates.front().parser_consumable = true;
    refresh_expansion_result(parser_consumable);
    EXPECT_EQ(parser_consumable.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_consumable));

    frontend::macro::EarlyItemExpansionResult generated_text = baseline;
    generated_text.parser_admission_gates.front().generated_source_text = true;
    refresh_expansion_result(generated_text);
    EXPECT_EQ(generated_text.summary.generated_source_text_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_text));

    frontend::macro::EarlyItemExpansionResult generated_part_parsed = baseline;
    generated_part_parsed.parser_admission_gates.front().generated_part_parsed = true;
    refresh_expansion_result(generated_part_parsed);
    EXPECT_EQ(generated_part_parsed.summary.parsed_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_part_parsed));

    frontend::macro::EarlyItemExpansionResult generated_part_merged = baseline;
    generated_part_merged.parser_admission_gates.front().generated_part_merged = true;
    refresh_expansion_result(generated_part_merged);
    EXPECT_EQ(generated_part_merged.summary.merged_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_part_merged));

    frontend::macro::EarlyItemExpansionResult sema_visible = baseline;
    sema_visible.parser_admission_gates.front().sema_visible = true;
    refresh_expansion_result(sema_visible);
    EXPECT_EQ(sema_visible.summary.sema_visible_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(sema_visible));

    frontend::macro::EarlyItemExpansionResult user_code = baseline;
    user_code.parser_admission_gates.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code);
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksTokenMaterializationAdmissionContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.token_materialization_admissions.empty());
    ASSERT_FALSE(baseline.generated_token_buffers.empty());

    frontend::macro::EarlyItemExpansionResult token_plan = baseline;
    token_plan.token_materialization_admissions.front().token_plan_identity =
        query::stable_fingerprint("different token plan identity");
    refresh_expansion_result(token_plan);
    EXPECT_NE(token_plan.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(token_plan));

    frontend::macro::EarlyItemExpansionResult token_buffer_identity = baseline;
    token_buffer_identity.token_materialization_admissions.front().token_buffer_identity =
        query::stable_fingerprint("different token buffer identity");
    refresh_expansion_result(token_buffer_identity);
    EXPECT_NE(token_buffer_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(token_buffer_identity));

    frontend::macro::EarlyItemExpansionResult buffer_stream = baseline;
    buffer_stream.generated_token_buffers.front().token_stream_name =
        "m21h-token-stream:different";
    refresh_expansion_result(buffer_stream);
    EXPECT_NE(buffer_stream.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(buffer_stream));

    frontend::macro::EarlyItemExpansionResult buffer_kind = baseline;
    buffer_kind.generated_token_buffers.front().token_buffer_kind = "different_buffer_kind";
    refresh_expansion_result(buffer_kind);
    EXPECT_NE(buffer_kind.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(buffer_kind));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParserAdmissionGateContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.parser_admission_gates.empty());

    frontend::macro::EarlyItemExpansionResult parse_gate_identity = baseline;
    parse_gate_identity.parser_admission_gates.front().parse_gate_identity =
        query::stable_fingerprint("different parser gate identity");
    refresh_expansion_result(parse_gate_identity);
    EXPECT_NE(parse_gate_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(parse_gate_identity));

    frontend::macro::EarlyItemExpansionResult parse_config = baseline;
    parse_config.parser_admission_gates.front().parse_config_fingerprint =
        query::stable_fingerprint("different parser gate parse config");
    refresh_expansion_result(parse_config);
    EXPECT_NE(parse_config.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(parse_config));

    frontend::macro::EarlyItemExpansionResult policy = baseline;
    policy.parser_admission_gates.front().parser_gate_policy = "different_parser_gate_policy";
    refresh_expansion_result(policy);
    EXPECT_NE(policy.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(policy));

    frontend::macro::EarlyItemExpansionResult availability = baseline;
    availability.parser_admission_gates.front().token_records_available = false;
    refresh_expansion_result(availability);
    EXPECT_NE(availability.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(availability));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsParserAdmissionDiagnosticProjectionDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.parser_admission_diagnostics.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_projection = baseline;
    missing_projection.parser_admission_diagnostics.clear();
    refresh_expansion_result(missing_projection);
    EXPECT_EQ(missing_projection.summary.parser_admission_diagnostic_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_projection));

    frontend::macro::EarlyItemExpansionResult empty_diagnostic_identity = baseline;
    empty_diagnostic_identity.parser_admission_diagnostics.front().diagnostic_identity = {};
    refresh_expansion_result(empty_diagnostic_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_diagnostic_identity));

    frontend::macro::EarlyItemExpansionResult wrong_diagnostic_identity = baseline;
    wrong_diagnostic_identity.parser_admission_diagnostics.front().diagnostic_identity =
        query::stable_fingerprint("wrong parser admission diagnostic identity");
    refresh_expansion_result(wrong_diagnostic_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_diagnostic_identity));

    frontend::macro::EarlyItemExpansionResult empty_anchor_identity = baseline;
    empty_anchor_identity.parser_admission_diagnostics.front().diagnostic_anchor_identity = {};
    refresh_expansion_result(empty_anchor_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_anchor_identity));

    frontend::macro::EarlyItemExpansionResult wrong_anchor_identity = baseline;
    wrong_anchor_identity.parser_admission_diagnostics.front().diagnostic_anchor_identity =
        query::stable_fingerprint("wrong parser admission diagnostic anchor");
    refresh_expansion_result(wrong_anchor_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_anchor_identity));

    frontend::macro::EarlyItemExpansionResult wrong_parse_gate_identity = baseline;
    wrong_parse_gate_identity.parser_admission_diagnostics.front().parse_gate_identity =
        query::stable_fingerprint("wrong parser admission diagnostic parse gate");
    refresh_expansion_result(wrong_parse_gate_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_parse_gate_identity));

    frontend::macro::EarlyItemExpansionResult wrong_token_plan = baseline;
    wrong_token_plan.parser_admission_diagnostics.front().token_plan_identity =
        query::stable_fingerprint("wrong parser admission diagnostic token plan");
    refresh_expansion_result(wrong_token_plan);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_plan));

    frontend::macro::EarlyItemExpansionResult wrong_token_buffer = baseline;
    wrong_token_buffer.parser_admission_diagnostics.front().token_buffer_identity =
        query::stable_fingerprint("wrong parser admission diagnostic token buffer");
    refresh_expansion_result(wrong_token_buffer);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_buffer));

    frontend::macro::EarlyItemExpansionResult wrong_materialization = baseline;
    wrong_materialization.parser_admission_diagnostics.front().materialization_identity =
        query::stable_fingerprint("wrong parser admission diagnostic materialization");
    refresh_expansion_result(wrong_materialization);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_materialization));

    frontend::macro::EarlyItemExpansionResult wrong_generated_buffer = baseline;
    wrong_generated_buffer.parser_admission_diagnostics.front().generated_buffer_identity =
        query::stable_fingerprint("wrong parser admission diagnostic generated buffer");
    refresh_expansion_result(wrong_generated_buffer);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_buffer));

    frontend::macro::EarlyItemExpansionResult wrong_parse_config = baseline;
    wrong_parse_config.parser_admission_diagnostics.front().parse_config_fingerprint =
        query::stable_fingerprint("wrong parser admission diagnostic parse config");
    refresh_expansion_result(wrong_parse_config);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_parse_config));

    frontend::macro::EarlyItemExpansionResult wrong_source_map = baseline;
    wrong_source_map.parser_admission_diagnostics.front().source_map_identity =
        query::stable_fingerprint("wrong parser admission diagnostic source map");
    refresh_expansion_result(wrong_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_source_map));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene = baseline;
    wrong_hygiene.parser_admission_diagnostics.front().hygiene_mark =
        query::stable_fingerprint("wrong parser admission diagnostic hygiene");
    refresh_expansion_result(wrong_hygiene);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene));

    frontend::macro::EarlyItemExpansionResult wrong_trace = baseline;
    wrong_trace.parser_admission_diagnostics.front().trace_identity =
        query::stable_fingerprint("wrong parser admission diagnostic trace");
    refresh_expansion_result(wrong_trace);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace));

    frontend::macro::EarlyItemExpansionResult wrong_policy = baseline;
    wrong_policy.parser_admission_diagnostics.front().diagnostic_policy = "wrong_diagnostic_policy";
    refresh_expansion_result(wrong_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_policy));

    frontend::macro::EarlyItemExpansionResult wrong_category = baseline;
    wrong_category.parser_admission_diagnostics.front().blocker_category =
        "empty_token_buffer_parser_admission_blocked";
    refresh_expansion_result(wrong_category);
    EXPECT_EQ(wrong_category.summary.derive_parser_admission_diagnostic_count, 0U);
    EXPECT_EQ(wrong_category.summary.empty_parser_admission_diagnostic_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_category));

    frontend::macro::EarlyItemExpansionResult wrong_token_buffer_blocker = baseline;
    wrong_token_buffer_blocker.parser_admission_diagnostics.front().token_buffer_blocker =
        "wrong token buffer blocker";
    refresh_expansion_result(wrong_token_buffer_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_buffer_blocker));

    frontend::macro::EarlyItemExpansionResult wrong_generated_part_blocker = baseline;
    wrong_generated_part_blocker.parser_admission_diagnostics.front().generated_part_parse_blocker =
        "wrong generated part parse blocker";
    refresh_expansion_result(wrong_generated_part_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_part_blocker));

    frontend::macro::EarlyItemExpansionResult wrong_user_message = baseline;
    wrong_user_message.parser_admission_diagnostics.front().user_message =
        "wrong parser admission diagnostic message";
    refresh_expansion_result(wrong_user_message);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_user_message));

    frontend::macro::EarlyItemExpansionResult wrong_debug_projection = baseline;
    wrong_debug_projection.parser_admission_diagnostics.front().debug_projection_name =
        "m21k-parser-admission:wrong";
    refresh_expansion_result(wrong_debug_projection);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_debug_projection));

    frontend::macro::EarlyItemExpansionResult wrong_primary_anchor = baseline;
    wrong_primary_anchor.parser_admission_diagnostics.front().primary_anchor.begin += 1U;
    refresh_expansion_result(wrong_primary_anchor);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_primary_anchor));

    frontend::macro::EarlyItemExpansionResult wrong_token_tree_anchor = baseline;
    wrong_token_tree_anchor.parser_admission_diagnostics.front().token_tree_anchor.end += 1U;
    refresh_expansion_result(wrong_token_tree_anchor);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_tree_anchor));

    frontend::macro::EarlyItemExpansionResult wrong_token_count = baseline;
    wrong_token_count.parser_admission_diagnostics.front().token_count = 0U;
    refresh_expansion_result(wrong_token_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_count));

    frontend::macro::EarlyItemExpansionResult missing_materialization = baseline;
    missing_materialization.parser_admission_diagnostics.front().token_buffer_materialized = false;
    refresh_expansion_result(missing_materialization);
    EXPECT_FALSE(frontend::macro::is_valid(missing_materialization));

    frontend::macro::EarlyItemExpansionResult missing_records = baseline;
    missing_records.parser_admission_diagnostics.front().token_records_available = false;
    refresh_expansion_result(missing_records);
    EXPECT_FALSE(frontend::macro::is_valid(missing_records));

    frontend::macro::EarlyItemExpansionResult parser_admitted = baseline;
    parser_admitted.parser_admission_diagnostics.front().parser_admitted = true;
    refresh_expansion_result(parser_admitted);
    EXPECT_EQ(parser_admitted.summary.parser_admission_diagnostic_blocked_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_admitted));

    frontend::macro::EarlyItemExpansionResult parse_ready = baseline;
    parse_ready.parser_admission_diagnostics.front().parse_ready = true;
    refresh_expansion_result(parse_ready);
    EXPECT_EQ(parse_ready.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parse_ready));

    frontend::macro::EarlyItemExpansionResult parser_consumable = baseline;
    parser_consumable.parser_admission_diagnostics.front().parser_consumable = true;
    refresh_expansion_result(parser_consumable);
    EXPECT_EQ(parser_consumable.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_consumable));

    frontend::macro::EarlyItemExpansionResult generated_part_parsed = baseline;
    generated_part_parsed.parser_admission_diagnostics.front().generated_part_parsed = true;
    refresh_expansion_result(generated_part_parsed);
    EXPECT_EQ(generated_part_parsed.summary.parsed_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_part_parsed));

    frontend::macro::EarlyItemExpansionResult generated_part_merged = baseline;
    generated_part_merged.parser_admission_diagnostics.front().generated_part_merged = true;
    refresh_expansion_result(generated_part_merged);
    EXPECT_EQ(generated_part_merged.summary.merged_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_part_merged));

    frontend::macro::EarlyItemExpansionResult emit_expanded = baseline;
    emit_expanded.parser_admission_diagnostics.front().emit_expanded_available = true;
    refresh_expansion_result(emit_expanded);
    EXPECT_EQ(emit_expanded.summary.emit_expanded_projection_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(emit_expanded));

    frontend::macro::EarlyItemExpansionResult debug_trace = baseline;
    debug_trace.parser_admission_diagnostics.front().debug_trace_available = true;
    refresh_expansion_result(debug_trace);
    EXPECT_EQ(debug_trace.summary.parser_admission_debug_trace_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(debug_trace));

    frontend::macro::EarlyItemExpansionResult source_map = baseline;
    source_map.parser_admission_diagnostics.front().source_map_available = true;
    refresh_expansion_result(source_map);
    EXPECT_EQ(source_map.summary.parser_admission_source_map_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(source_map));

    frontend::macro::EarlyItemExpansionResult user_code = baseline;
    user_code.parser_admission_diagnostics.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code);
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParserAdmissionDiagnosticProjectionContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.parser_admission_diagnostics.empty());

    frontend::macro::EarlyItemExpansionResult diagnostic_identity = baseline;
    diagnostic_identity.parser_admission_diagnostics.front().diagnostic_identity =
        query::stable_fingerprint("different diagnostic identity");
    refresh_expansion_result(diagnostic_identity);
    EXPECT_NE(diagnostic_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(diagnostic_identity));

    frontend::macro::EarlyItemExpansionResult category = baseline;
    category.parser_admission_diagnostics.front().blocker_category =
        "empty_token_buffer_parser_admission_blocked";
    refresh_expansion_result(category);
    EXPECT_NE(category.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(category));

    frontend::macro::EarlyItemExpansionResult debug_projection = baseline;
    debug_projection.parser_admission_diagnostics.front().debug_projection_name =
        "m21k-parser-admission:different";
    refresh_expansion_result(debug_projection);
    EXPECT_NE(debug_projection.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(debug_projection));

    frontend::macro::EarlyItemExpansionResult source_anchor = baseline;
    source_anchor.parser_admission_diagnostics.front().primary_anchor.end += 1U;
    refresh_expansion_result(source_anchor);
    EXPECT_NE(source_anchor.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(source_anchor));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsParserAdmissionReportProjectionDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.parser_admission_report_entries.size(), 2U);
    ASSERT_EQ(baseline.parser_admission_reports.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_entries = baseline;
    missing_entries.parser_admission_report_entries.clear();
    refresh_expansion_result(missing_entries);
    EXPECT_EQ(missing_entries.summary.parser_admission_report_entry_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_entries));

    frontend::macro::EarlyItemExpansionResult missing_reports = baseline;
    missing_reports.parser_admission_reports.clear();
    refresh_expansion_result(missing_reports);
    EXPECT_EQ(missing_reports.summary.parser_admission_report_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_reports));

    frontend::macro::EarlyItemExpansionResult wrong_entry_identity = baseline;
    wrong_entry_identity.parser_admission_report_entries.front().report_entry_identity =
        query::stable_fingerprint("wrong parser admission report entry identity");
    refresh_expansion_result(wrong_entry_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_entry_identity));

    frontend::macro::EarlyItemExpansionResult wrong_entry_anchor = baseline;
    wrong_entry_anchor.parser_admission_report_entries.front().diagnostic_anchor_identity =
        query::stable_fingerprint("wrong parser admission report entry anchor");
    refresh_expansion_result(wrong_entry_anchor);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_entry_anchor));

    frontend::macro::EarlyItemExpansionResult wrong_entry_category = baseline;
    wrong_entry_category.parser_admission_report_entries.back().blocker_category =
        "empty_token_buffer_parser_admission_blocked";
    refresh_expansion_result(wrong_entry_category);
    EXPECT_EQ(wrong_entry_category.summary.parser_admission_report_derive_entry_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_entry_category));

    frontend::macro::EarlyItemExpansionResult wrong_query_name = baseline;
    wrong_query_name.parser_admission_report_entries.front().query_projection_name =
        "m21l-parser-admission-report:wrong";
    refresh_expansion_result(wrong_query_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_query_name));

    frontend::macro::EarlyItemExpansionResult wrong_report_index = baseline;
    wrong_report_index.parser_admission_report_entries.back().report_index = 0U;
    refresh_expansion_result(wrong_report_index);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_index));

    frontend::macro::EarlyItemExpansionResult entry_parser_admitted = baseline;
    entry_parser_admitted.parser_admission_report_entries.front().parser_admitted = true;
    refresh_expansion_result(entry_parser_admitted);
    EXPECT_EQ(entry_parser_admitted.summary.parser_admission_report_blocked_entry_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(entry_parser_admitted));

    frontend::macro::EarlyItemExpansionResult entry_not_visible = baseline;
    entry_not_visible.parser_admission_report_entries.front().report_visible = false;
    refresh_expansion_result(entry_not_visible);
    EXPECT_FALSE(frontend::macro::is_valid(entry_not_visible));

    frontend::macro::EarlyItemExpansionResult entry_not_reusable = baseline;
    entry_not_reusable.parser_admission_report_entries.front().query_reusable = false;
    refresh_expansion_result(entry_not_reusable);
    EXPECT_FALSE(frontend::macro::is_valid(entry_not_reusable));

    frontend::macro::EarlyItemExpansionResult entry_parser_consumable = baseline;
    entry_parser_consumable.parser_admission_report_entries.front().parser_consumable = true;
    refresh_expansion_result(entry_parser_consumable);
    EXPECT_FALSE(frontend::macro::is_valid(entry_parser_consumable));

    frontend::macro::EarlyItemExpansionResult entry_emit_expanded = baseline;
    entry_emit_expanded.parser_admission_report_entries.front().emit_expanded_available = true;
    refresh_expansion_result(entry_emit_expanded);
    EXPECT_EQ(entry_emit_expanded.summary.emit_expanded_projection_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(entry_emit_expanded));

    frontend::macro::EarlyItemExpansionResult entry_user_code = baseline;
    entry_user_code.parser_admission_report_entries.front().produced_user_generated_code = true;
    refresh_expansion_result(entry_user_code);
    EXPECT_EQ(entry_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(entry_user_code));

    frontend::macro::EarlyItemExpansionResult wrong_report_identity = baseline;
    wrong_report_identity.parser_admission_reports.front().report_identity =
        query::stable_fingerprint("wrong parser admission report identity");
    refresh_expansion_result(wrong_report_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_identity));

    frontend::macro::EarlyItemExpansionResult wrong_report_group = baseline;
    wrong_report_group.parser_admission_reports.front().report_grouping_identity =
        query::stable_fingerprint("wrong parser admission report grouping");
    refresh_expansion_result(wrong_report_group);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_group));

    frontend::macro::EarlyItemExpansionResult wrong_report_anchor = baseline;
    wrong_report_anchor.parser_admission_reports.front().report_anchor_identity =
        query::stable_fingerprint("wrong parser admission report anchor");
    refresh_expansion_result(wrong_report_anchor);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_anchor));

    frontend::macro::EarlyItemExpansionResult wrong_parse_config = baseline;
    wrong_parse_config.parser_admission_reports.front().parse_config_fingerprint =
        query::stable_fingerprint("wrong parser admission report parse config");
    refresh_expansion_result(wrong_parse_config);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_parse_config));

    frontend::macro::EarlyItemExpansionResult wrong_generated_buffer = baseline;
    wrong_generated_buffer.parser_admission_reports.front().generated_buffer_identity =
        query::stable_fingerprint("wrong parser admission report generated buffer");
    refresh_expansion_result(wrong_generated_buffer);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_buffer));

    frontend::macro::EarlyItemExpansionResult wrong_policy = baseline;
    wrong_policy.parser_admission_reports.front().report_policy = "wrong_report_policy";
    refresh_expansion_result(wrong_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_policy));

    frontend::macro::EarlyItemExpansionResult wrong_report_query = baseline;
    wrong_report_query.parser_admission_reports.front().report_query_name =
        "m21l-parser-admission-report:wrong";
    refresh_expansion_result(wrong_report_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_report_query));

    frontend::macro::EarlyItemExpansionResult wrong_blocker = baseline;
    wrong_blocker.parser_admission_reports.front().blocked_reason = "wrong report blocker";
    refresh_expansion_result(wrong_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_blocker));

    frontend::macro::EarlyItemExpansionResult wrong_entry_count = baseline;
    wrong_entry_count.parser_admission_reports.front().entry_count = 1U;
    refresh_expansion_result(wrong_entry_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_entry_count));

    frontend::macro::EarlyItemExpansionResult wrong_category_totals = baseline;
    wrong_category_totals.parser_admission_reports.front().derive_entry_count = 0U;
    refresh_expansion_result(wrong_category_totals);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_category_totals));

    frontend::macro::EarlyItemExpansionResult report_not_visible = baseline;
    report_not_visible.parser_admission_reports.front().report_visible = false;
    refresh_expansion_result(report_not_visible);
    EXPECT_EQ(report_not_visible.summary.parser_admission_report_visible_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(report_not_visible));

    frontend::macro::EarlyItemExpansionResult report_not_reusable = baseline;
    report_not_reusable.parser_admission_reports.front().query_reusable = false;
    refresh_expansion_result(report_not_reusable);
    EXPECT_EQ(report_not_reusable.summary.parser_admission_report_query_reusable_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(report_not_reusable));

    frontend::macro::EarlyItemExpansionResult unordered_report = baseline;
    unordered_report.parser_admission_reports.front().source_anchor_ordered = false;
    refresh_expansion_result(unordered_report);
    EXPECT_EQ(unordered_report.summary.parser_admission_report_unordered_anchor_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(unordered_report));

    frontend::macro::EarlyItemExpansionResult report_parser_admitted = baseline;
    report_parser_admitted.parser_admission_reports.front().parser_admitted = true;
    refresh_expansion_result(report_parser_admitted);
    EXPECT_FALSE(frontend::macro::is_valid(report_parser_admitted));

    frontend::macro::EarlyItemExpansionResult report_parse_ready = baseline;
    report_parse_ready.parser_admission_reports.front().parse_ready = true;
    refresh_expansion_result(report_parse_ready);
    EXPECT_EQ(report_parse_ready.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_parse_ready));

    frontend::macro::EarlyItemExpansionResult report_parser_consumable = baseline;
    report_parser_consumable.parser_admission_reports.front().parser_consumable = true;
    refresh_expansion_result(report_parser_consumable);
    EXPECT_EQ(report_parser_consumable.summary.parser_admission_report_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_parser_consumable));

    frontend::macro::EarlyItemExpansionResult report_emit_expanded = baseline;
    report_emit_expanded.parser_admission_reports.front().emit_expanded_available = true;
    refresh_expansion_result(report_emit_expanded);
    EXPECT_EQ(report_emit_expanded.summary.emit_expanded_projection_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_emit_expanded));

    frontend::macro::EarlyItemExpansionResult report_debug_trace = baseline;
    report_debug_trace.parser_admission_reports.front().debug_trace_available = true;
    refresh_expansion_result(report_debug_trace);
    EXPECT_EQ(report_debug_trace.summary.parser_admission_debug_trace_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_debug_trace));

    frontend::macro::EarlyItemExpansionResult report_source_map = baseline;
    report_source_map.parser_admission_reports.front().source_map_available = true;
    refresh_expansion_result(report_source_map);
    EXPECT_EQ(report_source_map.summary.parser_admission_source_map_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_source_map));

    frontend::macro::EarlyItemExpansionResult report_user_code = baseline;
    report_user_code.parser_admission_reports.front().produced_user_generated_code = true;
    refresh_expansion_result(report_user_code);
    EXPECT_EQ(report_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(report_user_code));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParserAdmissionReportProjectionContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.parser_admission_report_entries.empty());
    ASSERT_FALSE(baseline.parser_admission_reports.empty());

    frontend::macro::EarlyItemExpansionResult entry_identity = baseline;
    entry_identity.parser_admission_report_entries.front().report_entry_identity =
        query::stable_fingerprint("different parser admission report entry identity");
    refresh_expansion_result(entry_identity);
    EXPECT_NE(entry_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(entry_identity));

    frontend::macro::EarlyItemExpansionResult entry_query = baseline;
    entry_query.parser_admission_report_entries.front().query_projection_name =
        "m21l-parser-admission-report:different";
    refresh_expansion_result(entry_query);
    EXPECT_NE(entry_query.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(entry_query));

    frontend::macro::EarlyItemExpansionResult report_identity = baseline;
    report_identity.parser_admission_reports.front().report_identity =
        query::stable_fingerprint("different parser admission report identity");
    refresh_expansion_result(report_identity);
    EXPECT_NE(report_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(report_identity));

    frontend::macro::EarlyItemExpansionResult report_totals = baseline;
    report_totals.parser_admission_reports.front().blocked_entry_count = 0U;
    refresh_expansion_result(report_totals);
    EXPECT_NE(report_totals.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(report_totals));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsParserReadinessAndContractDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.parser_readiness_preflight_entries.size(), 2U);
    ASSERT_EQ(baseline.parser_consumption_contract_gates.size(), 1U);
    ASSERT_EQ(baseline.macro_boundary_closure_reports.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_preflight = baseline;
    missing_preflight.parser_readiness_preflight_entries.clear();
    refresh_expansion_result(missing_preflight);
    EXPECT_EQ(missing_preflight.summary.parser_readiness_preflight_entry_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_preflight));

    frontend::macro::EarlyItemExpansionResult wrong_preflight_identity = baseline;
    wrong_preflight_identity.parser_readiness_preflight_entries.front().preflight_identity =
        query::stable_fingerprint("wrong parser readiness preflight identity");
    refresh_expansion_result(wrong_preflight_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_preflight_identity));

    frontend::macro::EarlyItemExpansionResult wrong_preflight_shape = baseline;
    wrong_preflight_shape.parser_readiness_preflight_entries.back().token_stream_shape =
        "empty_token_stream_parser_input_blocked";
    refresh_expansion_result(wrong_preflight_shape);
    EXPECT_EQ(wrong_preflight_shape.summary.parser_readiness_preflight_derive_entry_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_preflight_shape));

    frontend::macro::EarlyItemExpansionResult non_contiguous = baseline;
    non_contiguous.parser_readiness_preflight_entries.back().token_indices_contiguous = false;
    refresh_expansion_result(non_contiguous);
    EXPECT_EQ(non_contiguous.summary.parser_readiness_preflight_contiguous_index_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(non_contiguous));

    frontend::macro::EarlyItemExpansionResult unbalanced_delimiter = baseline;
    unbalanced_delimiter.parser_readiness_preflight_entries.back().delimiter_balanced = false;
    refresh_expansion_result(unbalanced_delimiter);
    EXPECT_EQ(unbalanced_delimiter.summary.parser_readiness_preflight_delimiter_balanced_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(unbalanced_delimiter));

    frontend::macro::EarlyItemExpansionResult uncovered_anchor = baseline;
    uncovered_anchor.parser_readiness_preflight_entries.back().source_anchors_covered = false;
    refresh_expansion_result(uncovered_anchor);
    EXPECT_EQ(uncovered_anchor.summary.parser_readiness_preflight_source_anchor_covered_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(uncovered_anchor));

    frontend::macro::EarlyItemExpansionResult incompatible_parse_config = baseline;
    incompatible_parse_config.parser_readiness_preflight_entries.back().parse_config_compatible = false;
    refresh_expansion_result(incompatible_parse_config);
    EXPECT_EQ(incompatible_parse_config.summary.parser_readiness_preflight_parse_config_compatible_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(incompatible_parse_config));

    frontend::macro::EarlyItemExpansionResult preflight_parser_consumable = baseline;
    preflight_parser_consumable.parser_readiness_preflight_entries.back().parser_consumable = true;
    refresh_expansion_result(preflight_parser_consumable);
    EXPECT_EQ(preflight_parser_consumable.summary.parser_readiness_preflight_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(preflight_parser_consumable));

    frontend::macro::EarlyItemExpansionResult missing_contract = baseline;
    missing_contract.parser_consumption_contract_gates.clear();
    refresh_expansion_result(missing_contract);
    EXPECT_EQ(missing_contract.summary.parser_consumption_contract_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_contract));

    frontend::macro::EarlyItemExpansionResult wrong_contract_identity = baseline;
    wrong_contract_identity.parser_consumption_contract_gates.front().contract_identity =
        query::stable_fingerprint("wrong parser consumption contract identity");
    refresh_expansion_result(wrong_contract_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_contract_identity));

    frontend::macro::EarlyItemExpansionResult wrong_contract_counts = baseline;
    wrong_contract_counts.parser_consumption_contract_gates.front().preflight_entry_count = 1U;
    refresh_expansion_result(wrong_contract_counts);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_contract_counts));

    frontend::macro::EarlyItemExpansionResult contract_not_visible = baseline;
    contract_not_visible.parser_consumption_contract_gates.front().contract_visible = false;
    refresh_expansion_result(contract_not_visible);
    EXPECT_EQ(contract_not_visible.summary.parser_consumption_contract_visible_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_not_visible));

    frontend::macro::EarlyItemExpansionResult contract_parser_consumable = baseline;
    contract_parser_consumable.parser_consumption_contract_gates.front().parser_consumable = true;
    refresh_expansion_result(contract_parser_consumable);
    EXPECT_EQ(contract_parser_consumable.summary.parser_consumption_contract_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_parser_consumable));

    frontend::macro::EarlyItemExpansionResult contract_emit_expanded = baseline;
    contract_emit_expanded.parser_consumption_contract_gates.front().emit_expanded_available = true;
    refresh_expansion_result(contract_emit_expanded);
    EXPECT_EQ(contract_emit_expanded.summary.emit_expanded_projection_available_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_emit_expanded));

    frontend::macro::EarlyItemExpansionResult contract_source_map = baseline;
    contract_source_map.parser_consumption_contract_gates.front().source_map_available = true;
    refresh_expansion_result(contract_source_map);
    EXPECT_EQ(contract_source_map.summary.parser_admission_source_map_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_source_map));

    frontend::macro::EarlyItemExpansionResult contract_user_code = baseline;
    contract_user_code.parser_consumption_contract_gates.front().produced_user_generated_code = true;
    refresh_expansion_result(contract_user_code);
    EXPECT_EQ(contract_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(contract_user_code));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsMacroBoundaryClosureDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.macro_boundary_closure_reports.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_closure = baseline;
    missing_closure.macro_boundary_closure_reports.clear();
    refresh_expansion_result(missing_closure);
    EXPECT_EQ(missing_closure.summary.macro_boundary_closure_report_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_closure));

    frontend::macro::EarlyItemExpansionResult wrong_identity = baseline;
    wrong_identity.macro_boundary_closure_reports.front().closure_identity =
        query::stable_fingerprint("wrong macro boundary closure identity");
    refresh_expansion_result(wrong_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_identity));

    frontend::macro::EarlyItemExpansionResult wrong_counts = baseline;
    wrong_counts.macro_boundary_closure_reports.front().parser_consumption_contract_gate_count = 0U;
    refresh_expansion_result(wrong_counts);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_counts));

    frontend::macro::EarlyItemExpansionResult not_complete = baseline;
    not_complete.macro_boundary_closure_reports.front().release_closure_complete = false;
    refresh_expansion_result(not_complete);
    EXPECT_EQ(not_complete.summary.macro_boundary_closure_complete_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(not_complete));

    frontend::macro::EarlyItemExpansionResult parser_consumption_enabled = baseline;
    parser_consumption_enabled.macro_boundary_closure_reports.front().parser_consumption_enabled = true;
    refresh_expansion_result(parser_consumption_enabled);
    EXPECT_EQ(parser_consumption_enabled.summary.macro_boundary_closure_parser_consumption_enabled_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_consumption_enabled));

    frontend::macro::EarlyItemExpansionResult standard_library = baseline;
    standard_library.macro_boundary_closure_reports.front().standard_library_required = true;
    refresh_expansion_result(standard_library);
    EXPECT_EQ(standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(standard_library));

    frontend::macro::EarlyItemExpansionResult runtime = baseline;
    runtime.macro_boundary_closure_reports.front().runtime_required = true;
    refresh_expansion_result(runtime);
    EXPECT_EQ(runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(runtime));

    frontend::macro::EarlyItemExpansionResult external_process = baseline;
    external_process.macro_boundary_closure_reports.front().external_process_required = true;
    refresh_expansion_result(external_process);
    EXPECT_EQ(external_process.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(external_process));

    frontend::macro::EarlyItemExpansionResult user_code = baseline;
    user_code.macro_boundary_closure_reports.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code);
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParserReadinessContractAndClosure)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.parser_readiness_preflight_entries.empty());
    ASSERT_FALSE(baseline.parser_consumption_contract_gates.empty());
    ASSERT_FALSE(baseline.macro_boundary_closure_reports.empty());

    frontend::macro::EarlyItemExpansionResult preflight_identity = baseline;
    preflight_identity.parser_readiness_preflight_entries.front().preflight_identity =
        query::stable_fingerprint("different parser readiness preflight identity");
    refresh_expansion_result(preflight_identity);
    EXPECT_NE(preflight_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(preflight_identity));

    frontend::macro::EarlyItemExpansionResult preflight_shape = baseline;
    preflight_shape.parser_readiness_preflight_entries.front().token_stream_shape =
        "empty_token_stream_parser_input_blocked";
    refresh_expansion_result(preflight_shape);
    EXPECT_NE(preflight_shape.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(preflight_shape));

    frontend::macro::EarlyItemExpansionResult contract_identity = baseline;
    contract_identity.parser_consumption_contract_gates.front().contract_identity =
        query::stable_fingerprint("different parser consumption contract identity");
    refresh_expansion_result(contract_identity);
    EXPECT_NE(contract_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(contract_identity));

    frontend::macro::EarlyItemExpansionResult contract_totals = baseline;
    contract_totals.parser_consumption_contract_gates.front().delimiter_balanced_entry_count = 0U;
    refresh_expansion_result(contract_totals);
    EXPECT_NE(contract_totals.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(contract_totals));

    frontend::macro::EarlyItemExpansionResult closure_identity = baseline;
    closure_identity.macro_boundary_closure_reports.front().closure_identity =
        query::stable_fingerprint("different macro boundary closure identity");
    refresh_expansion_result(closure_identity);
    EXPECT_NE(closure_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(closure_identity));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsBuiltinDeriveM22Drift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.builtin_derive_expansion_admissions.size(), 2U);
    ASSERT_EQ(baseline.builtin_derive_semantic_plans.size(), 2U);
    ASSERT_EQ(baseline.builtin_derive_parser_release_gates.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_admissions = baseline;
    missing_admissions.builtin_derive_expansion_admissions.clear();
    refresh_expansion_result(missing_admissions);
    EXPECT_EQ(missing_admissions.summary.builtin_derive_expansion_admission_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_admissions));

    frontend::macro::EarlyItemExpansionResult wrong_admission_identity = baseline;
    wrong_admission_identity.builtin_derive_expansion_admissions.back().admission_identity =
        query::stable_fingerprint("wrong builtin derive admission identity");
    refresh_expansion_result(wrong_admission_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_admission_identity));

    frontend::macro::EarlyItemExpansionResult wrong_admission_kind = baseline;
    wrong_admission_kind.builtin_derive_expansion_admissions.back().admission_kind =
        "non_derive_attribute_expansion_blocked";
    refresh_expansion_result(wrong_admission_kind);
    EXPECT_EQ(wrong_admission_kind.summary.builtin_derive_expansion_derive_admission_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_admission_kind));

    frontend::macro::EarlyItemExpansionResult wrong_admission_query = baseline;
    wrong_admission_query.builtin_derive_expansion_admissions.back().query_name =
        "m22a-builtin-derive-admission:wrong";
    refresh_expansion_result(wrong_admission_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_admission_query));

    frontend::macro::EarlyItemExpansionResult wrong_candidate_count = baseline;
    wrong_candidate_count.builtin_derive_expansion_admissions.back().capability_candidate_count = 1U;
    refresh_expansion_result(wrong_candidate_count);
    EXPECT_NE(wrong_candidate_count.summary.builtin_derive_expansion_capability_candidate_count,
        baseline.summary.builtin_derive_expansion_capability_candidate_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_candidate_count));

    frontend::macro::EarlyItemExpansionResult admission_parser_enabled = baseline;
    admission_parser_enabled.builtin_derive_expansion_admissions.back().parser_consumption_enabled = true;
    refresh_expansion_result(admission_parser_enabled);
    EXPECT_EQ(admission_parser_enabled.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_parser_enabled));

    frontend::macro::EarlyItemExpansionResult admission_standard_library = baseline;
    admission_standard_library.builtin_derive_expansion_admissions.back().standard_library_required = true;
    refresh_expansion_result(admission_standard_library);
    EXPECT_EQ(admission_standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_standard_library));

    frontend::macro::EarlyItemExpansionResult missing_plans = baseline;
    missing_plans.builtin_derive_semantic_plans.clear();
    refresh_expansion_result(missing_plans);
    EXPECT_EQ(missing_plans.summary.builtin_derive_semantic_plan_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_plans));

    frontend::macro::EarlyItemExpansionResult wrong_semantic_identity = baseline;
    wrong_semantic_identity.builtin_derive_semantic_plans.back().semantic_plan_identity =
        query::stable_fingerprint("wrong builtin derive semantic plan");
    refresh_expansion_result(wrong_semantic_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_semantic_identity));

    frontend::macro::EarlyItemExpansionResult wrong_target_kind = baseline;
    wrong_target_kind.builtin_derive_semantic_plans.back().target_kind = "enum";
    refresh_expansion_result(wrong_target_kind);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_target_kind));

    frontend::macro::EarlyItemExpansionResult wrong_capability_total = baseline;
    wrong_capability_total.builtin_derive_semantic_plans.back().capability_count = 2U;
    refresh_expansion_result(wrong_capability_total);
    EXPECT_EQ(wrong_capability_total.summary.builtin_derive_semantic_capability_count, 2U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_capability_total));

    frontend::macro::EarlyItemExpansionResult semantic_requires_generated_items = baseline;
    semantic_requires_generated_items.builtin_derive_semantic_plans.back().requires_generated_items = true;
    refresh_expansion_result(semantic_requires_generated_items);
    EXPECT_FALSE(frontend::macro::is_valid(semantic_requires_generated_items));

    frontend::macro::EarlyItemExpansionResult semantic_runtime = baseline;
    semantic_runtime.builtin_derive_semantic_plans.back().requires_runtime = true;
    refresh_expansion_result(semantic_runtime);
    EXPECT_EQ(semantic_runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(semantic_runtime));

    frontend::macro::EarlyItemExpansionResult missing_release = baseline;
    missing_release.builtin_derive_parser_release_gates.clear();
    refresh_expansion_result(missing_release);
    EXPECT_EQ(missing_release.summary.builtin_derive_parser_release_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_release));

    frontend::macro::EarlyItemExpansionResult wrong_release_identity = baseline;
    wrong_release_identity.builtin_derive_parser_release_gates.front().release_gate_identity =
        query::stable_fingerprint("wrong builtin derive parser release gate");
    refresh_expansion_result(wrong_release_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_release_identity));

    frontend::macro::EarlyItemExpansionResult wrong_release_counts = baseline;
    wrong_release_counts.builtin_derive_parser_release_gates.front().derive_admission_count = 0U;
    refresh_expansion_result(wrong_release_counts);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_release_counts));

    frontend::macro::EarlyItemExpansionResult release_parser_enabled = baseline;
    release_parser_enabled.builtin_derive_parser_release_gates.front().parser_consumption_enabled = true;
    refresh_expansion_result(release_parser_enabled);
    EXPECT_EQ(release_parser_enabled.summary.builtin_derive_parser_release_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(release_parser_enabled));

    frontend::macro::EarlyItemExpansionResult release_debug_trace = baseline;
    release_debug_trace.builtin_derive_parser_release_gates.front().debug_trace_available = true;
    refresh_expansion_result(release_debug_trace);
    EXPECT_EQ(release_debug_trace.summary.parser_admission_debug_trace_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(release_debug_trace));

    frontend::macro::EarlyItemExpansionResult release_external_process = baseline;
    release_external_process.builtin_derive_parser_release_gates.front().external_process_required = true;
    refresh_expansion_result(release_external_process);
    EXPECT_EQ(release_external_process.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(release_external_process));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksBuiltinDeriveM22Facts)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.builtin_derive_expansion_admissions.empty());
    ASSERT_FALSE(baseline.builtin_derive_semantic_plans.empty());
    ASSERT_FALSE(baseline.builtin_derive_parser_release_gates.empty());

    frontend::macro::EarlyItemExpansionResult admission_identity = baseline;
    admission_identity.builtin_derive_expansion_admissions.front().admission_identity =
        query::stable_fingerprint("different builtin derive admission identity");
    refresh_expansion_result(admission_identity);
    EXPECT_NE(admission_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(admission_identity));

    frontend::macro::EarlyItemExpansionResult admission_query = baseline;
    admission_query.builtin_derive_expansion_admissions.front().query_name =
        "m22a-builtin-derive-admission:different";
    refresh_expansion_result(admission_query);
    EXPECT_NE(admission_query.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(admission_query));

    frontend::macro::EarlyItemExpansionResult semantic_identity = baseline;
    semantic_identity.builtin_derive_semantic_plans.front().semantic_plan_identity =
        query::stable_fingerprint("different builtin derive semantic plan identity");
    refresh_expansion_result(semantic_identity);
    EXPECT_NE(semantic_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(semantic_identity));

    frontend::macro::EarlyItemExpansionResult semantic_capabilities = baseline;
    semantic_capabilities.builtin_derive_semantic_plans.front().hash_capability_count = 0U;
    refresh_expansion_result(semantic_capabilities);
    EXPECT_NE(semantic_capabilities.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(semantic_capabilities));

    frontend::macro::EarlyItemExpansionResult release_identity = baseline;
    release_identity.builtin_derive_parser_release_gates.front().release_gate_identity =
        query::stable_fingerprint("different builtin derive parser release identity");
    refresh_expansion_result(release_identity);
    EXPECT_NE(release_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(release_identity));
}

TEST(CoreUnit, EarlyItemExpansionRejectsInvalidInputs)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    syntax::AstModule module = parse_success(source);
    assign_single_module_ownership(module);
    std::vector<std::vector<query::ModulePartKey>> part_keys = single_part_key_table();

    query::MacroExpansionPlan invalid_plan = query::m21c_macro_expansion_plan_baseline();
    invalid_plan.name = "wrong plan";
    invalid_plan.fingerprint = query::macro_expansion_plan_fingerprint(invalid_plan);
    auto invalid_plan_result =
        frontend::macro::expand_early_item_macros_noop(module, part_keys, invalid_plan);
    ASSERT_FALSE(invalid_plan_result);
    EXPECT_EQ(invalid_plan_result.error().code, base::ErrorCode::internal_error);
    expect_contains(invalid_plan_result.error().message, "valid M21c macro expansion plan");

    syntax::AstModule missing_module_owner = module;
    missing_module_owner.item_modules.pop_back();
    auto missing_module_result =
        frontend::macro::expand_early_item_macros_noop(missing_module_owner, part_keys);
    ASSERT_FALSE(missing_module_result);
    EXPECT_EQ(missing_module_result.error().code, base::ErrorCode::internal_error);
    expect_contains(missing_module_result.error().message, "one module owner per item");

    syntax::AstModule missing_part_owner = module;
    missing_part_owner.item_part_indices.pop_back();
    auto missing_part_result =
        frontend::macro::expand_early_item_macros_noop(missing_part_owner, part_keys);
    ASSERT_FALSE(missing_part_result);
    EXPECT_EQ(missing_part_result.error().code, base::ErrorCode::internal_error);
    expect_contains(missing_part_result.error().message, "one module part index per item");

    std::vector<std::vector<query::ModulePartKey>> missing_key = {{}};
    auto missing_key_result =
        frontend::macro::expand_early_item_macros_noop(module, missing_key);
    ASSERT_FALSE(missing_key_result);
    EXPECT_EQ(missing_key_result.error().code, base::ErrorCode::internal_error);
    expect_contains(missing_key_result.error().message, "missing module part key");

    std::vector<std::vector<query::ModulePartKey>> invalid_key = {{query::ModulePartKey{}}};
    auto invalid_key_result =
        frontend::macro::expand_early_item_macros_noop(module, invalid_key);
    ASSERT_FALSE(invalid_key_result);
    EXPECT_EQ(invalid_key_result.error().code, base::ErrorCode::internal_error);
    expect_contains(invalid_key_result.error().message, "missing module part key");
}

} // namespace aurex::test
