#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_common_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

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
