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
constexpr base::u32 EARLY_ITEM_EXPANSION_TEST_GENERATED_PART_INDEX_OFFSET = 100'000U;

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

[[nodiscard]] query::ModulePartKey primary_part_key()
{
    const query::PackageKey package = package_key();
    const query::ModuleKey module = module_key(package);
    const query::FileKey file = query::file_key(package, "/virtual/tests/macro/early_item_expansion.ax");
    return query::module_part_key(module, file, query::ModulePartKind::primary, "<primary>");
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

    EXPECT_EQ(result.name, "M21d No-op Early Item Macro Expansion Boundary");
    EXPECT_TRUE(frontend::macro::is_valid(result));
    EXPECT_EQ(result.fingerprint, frontend::macro::early_item_expansion_fingerprint(result));
    EXPECT_EQ(result.summary.macro_input_count, 2U);
    EXPECT_EQ(result.summary.attribute_input_count, 2U);
    EXPECT_EQ(result.summary.builtin_derive_passthrough_count, 1U);
    EXPECT_EQ(result.summary.blocked_attribute_count, 1U);
    EXPECT_EQ(result.summary.generated_part_placeholder_count, 1U);
    EXPECT_EQ(result.summary.source_map_placeholder_count, 2U);
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
    EXPECT_FALSE(generated.parsed);
    EXPECT_FALSE(generated.merged);
    EXPECT_FALSE(generated.produced_user_generated_code);

    ASSERT_EQ(result.source_maps.size(), 2U);
    for (const frontend::macro::ExpansionSourceMapPlaceholder& source_map : result.source_maps) {
        EXPECT_FALSE(source_map.real_source_map);
        EXPECT_FALSE(source_map.debug_trace_available);
        EXPECT_GT(source_map.expansion_origin.byte_count, 0U);
    }
    EXPECT_EQ(result.source_maps[0].expansion_origin, builder->query_key_fingerprint);
    EXPECT_EQ(result.source_maps[1].expansion_origin, derive->query_key_fingerprint);

    const std::string summary = frontend::macro::summarize_early_item_expansion(result);
    expect_contains(summary, "early_item_expansion name=M21d No-op Early Item Macro Expansion Boundary");
    expect_contains(summary, "attributes=2");
    expect_contains(summary, "blocked_attributes=1");
    expect_contains(summary, "user_generated_code=0");

    const std::string dump = frontend::macro::dump_early_item_expansion(result);
    expect_contains(dump, "input #0");
    expect_contains(dump, "attr=builder");
    expect_contains(dump, "disposition=blocked_unimplemented_attribute");
    expect_contains(dump, "generated_part #0");
    expect_contains(dump, "source_role=generated");
    expect_contains(dump, "kind=generated");
    expect_contains(dump, "source_map #1");
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
    generated_code.summary = frontend::macro::summarize_early_item_expansion_counts(generated_code);
    generated_code.fingerprint = frontend::macro::early_item_expansion_fingerprint(generated_code);
    EXPECT_FALSE(frontend::macro::is_valid(generated_code));

    frontend::macro::EarlyItemExpansionResult parsed_part = baseline;
    ASSERT_FALSE(parsed_part.generated_parts.empty());
    parsed_part.generated_parts.front().parsed = true;
    parsed_part.summary = frontend::macro::summarize_early_item_expansion_counts(parsed_part);
    parsed_part.fingerprint = frontend::macro::early_item_expansion_fingerprint(parsed_part);
    EXPECT_FALSE(frontend::macro::is_valid(parsed_part));

    frontend::macro::EarlyItemExpansionResult merged_part = baseline;
    ASSERT_FALSE(merged_part.generated_parts.empty());
    merged_part.generated_parts.front().merged = true;
    merged_part.summary = frontend::macro::summarize_early_item_expansion_counts(merged_part);
    merged_part.fingerprint = frontend::macro::early_item_expansion_fingerprint(merged_part);
    EXPECT_FALSE(frontend::macro::is_valid(merged_part));

    frontend::macro::EarlyItemExpansionResult real_source_map = baseline;
    ASSERT_FALSE(real_source_map.source_maps.empty());
    real_source_map.source_maps.front().real_source_map = true;
    real_source_map.summary = frontend::macro::summarize_early_item_expansion_counts(real_source_map);
    real_source_map.fingerprint = frontend::macro::early_item_expansion_fingerprint(real_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(real_source_map));

    frontend::macro::EarlyItemExpansionResult stale_summary = baseline;
    stale_summary.summary.macro_input_count += 1U;
    EXPECT_FALSE(frontend::macro::is_valid(stale_summary));

    frontend::macro::EarlyItemExpansionResult stale_fingerprint = baseline;
    stale_fingerprint.fingerprint = query::stable_fingerprint("stale early item expansion");
    EXPECT_FALSE(frontend::macro::is_valid(stale_fingerprint));
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
