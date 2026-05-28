#include <aurex/tooling/ide.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view IDE_TOOLING_SOURCE = "module ide.snapshot;\n"
                                                "fn add(a: i32, b: i32) -> i32 {\n"
                                                "  return a + b;\n"
                                                "}\n"
                                                "fn main() -> i32 {\n"
                                                "  let value = add(1, 2);\n"
                                                "  return value;\n"
                                                "}\n";

constexpr base::usize IDE_TOOLING_LARGE_FUNCTION_COUNT = 128;
constexpr base::u32 IDE_TOOLING_PRIMARY_PART_INDEX = 0;
constexpr base::u32 IDE_TOOLING_FUNCTION_PART_INDEX = 3;
constexpr base::u32 IDE_TOOLING_STRUCT_PART_INDEX = 4;
constexpr base::u32 IDE_TOOLING_ALIAS_PART_INDEX = 5;
constexpr base::u32 IDE_TOOLING_ENUM_CASE_PART_INDEX = 6;
constexpr base::u32 IDE_TOOLING_TEMPLATE_PART_INDEX = 7;
constexpr base::u32 IDE_TOOLING_RECOVERED_PARSER_PART_INDEX = 2;
constexpr std::string_view IDE_TOOLING_PRIMARY_PART_NAME = "<primary>";
constexpr std::string_view IDE_TOOLING_STAGE_TOKENS_LEX = "tokens.lex";
constexpr std::string_view IDE_TOOLING_STAGE_MODULE_LEX = "module.lex";
constexpr std::string_view IDE_TOOLING_STAGE_MODULE_PARSE = "module.parse";
constexpr std::string_view IDE_TOOLING_STAGE_SEMA_ANALYZE = "sema.analyze";

std::atomic<std::uint64_t> ide_tooling_temp_counter{0};

[[nodiscard]] std::string ide_tooling_sanitize_test_name(const std::string_view name)
{
    std::string result;
    result.reserve(name.size());
    for (const char ch : name) {
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
        result.push_back(ok ? ch : '_');
    }
    return result;
}

[[nodiscard]] fs::path ide_tooling_tmp_root()
{
    const ::testing::TestInfo* const info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string test_name = "suite";
    if (info != nullptr) {
        test_name = std::string(info->test_suite_name()) + "_" + std::string(info->name());
    }
    const auto tick = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::uint64_t seq = ide_tooling_temp_counter.fetch_add(1U, std::memory_order_relaxed);
    return fs::temp_directory_path()
        / ("aurex-ide-tooling-" + ide_tooling_sanitize_test_name(test_name) + "-" + std::to_string(tick) + "-"
            + std::to_string(seq));
}

[[nodiscard]] tooling::IdeSnapshotRequest request_for(const std::string_view source)
{
    tooling::IdeSnapshotRequest request;
    request.path = "/workspace/ide_snapshot.ax";
    request.text = std::string(source);
    request.package_identity = "tooling-test";
    request.virtual_buffer_identity = "buffer:1";
    return request;
}

[[nodiscard]] tooling::IdeSnapshotRequest request_for_path(const std::string_view source, const fs::path& path)
{
    tooling::IdeSnapshotRequest request = request_for(source);
    request.path = path.string();
    return request;
}

fs::path write_ide_tooling_source(const fs::path& path, const std::string_view text)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to open " + path.string());
    }
    out << text;
    return path;
}

[[nodiscard]] bool has_record_kind(const tooling::IdeSnapshot& snapshot, const query::QueryKind kind)
{
    return std::ranges::any_of(snapshot.query.records, [kind](const query::QueryRecord& record) {
        return record.key.kind == kind;
    });
}

[[nodiscard]] bool has_dependency_kind(
    const tooling::IdeSnapshot& snapshot, const query::QueryKind dependent, const query::QueryKind dependency)
{
    return std::ranges::any_of(
        snapshot.query.dependencies, [dependent, dependency](const query::QueryDependencyEdge edge) {
            return edge.dependent.kind == dependent && edge.dependency.kind == dependency;
        });
}

[[nodiscard]] bool has_semantic_fact_query_kind(
    const tooling::IdeSnapshot& snapshot, const query::QueryKind kind, const std::string_view name)
{
    return std::ranges::any_of(snapshot.query.semantic_facts, [kind, name](const tooling::IdeSemanticFact& fact) {
        return fact.query.kind == kind && fact.name == name && fact.checked;
    });
}

[[nodiscard]] bool has_semantic_fact_kind(const tooling::IdeSnapshot& snapshot, const tooling::IdeSemanticFactKind kind,
    const query::QueryKind query_kind, const std::string_view name)
{
    return std::ranges::any_of(
        snapshot.query.semantic_facts, [kind, query_kind, name](const tooling::IdeSemanticFact& fact) {
            return fact.kind == kind && fact.query.kind == query_kind && fact.name == name && fact.checked;
        });
}

[[nodiscard]] bool has_diagnostic_category(
    const tooling::IdeSnapshot& snapshot, const base::DiagnosticCategory category)
{
    return std::ranges::any_of(snapshot.diagnostics, [category](const tooling::IdeDiagnostic& diagnostic) {
        return diagnostic.category == category;
    });
}

[[nodiscard]] bool diagnostic_has_owner_stage_profile(
    const tooling::IdeDiagnostic& diagnostic, const std::string_view profile_name)
{
    return std::ranges::any_of(diagnostic.owner_stages, [profile_name](const tooling::IdePipelineStageOwner& owner) {
        return owner.profile_name == profile_name && !owner.id.empty() && !owner.diagnostic_ownership.empty();
    });
}

[[nodiscard]] bool has_diagnostic_owner_stage_profile(
    const tooling::IdeSnapshot& snapshot, const base::DiagnosticCategory category, const std::string_view profile_name)
{
    return std::ranges::any_of(
        snapshot.diagnostics, [category, profile_name](const tooling::IdeDiagnostic& diagnostic) {
            return diagnostic.category == category && diagnostic_has_owner_stage_profile(diagnostic, profile_name);
        });
}

void expect_definition_kind(const tooling::IdeSnapshot& snapshot, const std::string_view source,
    const std::string_view name, const std::string_view kind, const query::DefKind def_kind)
{
    const base::usize offset = source.find(name);
    ASSERT_NE(offset, std::string_view::npos) << name;
    const std::optional<tooling::IdeDefinition> definition = tooling::definition_at_offset(snapshot, offset);
    ASSERT_TRUE(definition.has_value()) << name;
    EXPECT_TRUE(definition->valid);
    EXPECT_EQ(definition->name, name);
    EXPECT_EQ(definition->kind, kind);
    EXPECT_EQ(definition->key.kind, def_kind);
    EXPECT_EQ(definition->part_index, IDE_TOOLING_PRIMARY_PART_INDEX);
    EXPECT_EQ(source.substr(definition->range.begin, definition->range.length()), name);
}

void expect_primary_source_part(const tooling::IdeModulePartContext& context, const std::string_view module_name)
{
    EXPECT_TRUE(context.valid);
    EXPECT_TRUE(context.resolved);
    EXPECT_TRUE(query::is_valid(context.module_key));
    EXPECT_TRUE(query::is_valid(context.part_key));
    EXPECT_EQ(context.kind, query::ModulePartKind::primary);
    EXPECT_EQ(context.module_name, module_name);
    EXPECT_EQ(context.part_name, IDE_TOOLING_PRIMARY_PART_NAME);
    EXPECT_EQ(context.part_index, IDE_TOOLING_PRIMARY_PART_INDEX);
    EXPECT_EQ(context.part_key.kind, query::ModulePartKind::primary);
    EXPECT_EQ(context.part_key.stable_index, IDE_TOOLING_PRIMARY_PART_INDEX);
}

void expect_resolved_fragment_source_part(const tooling::IdeModulePartContext& context,
    const std::string_view module_name, const std::string_view part_name, const base::u32 part_index)
{
    EXPECT_TRUE(context.valid);
    EXPECT_TRUE(context.resolved);
    EXPECT_TRUE(query::is_valid(context.module_key));
    EXPECT_TRUE(query::is_valid(context.part_key));
    EXPECT_EQ(context.kind, query::ModulePartKind::fragment);
    EXPECT_EQ(context.module_name, module_name);
    EXPECT_EQ(context.part_name, part_name);
    EXPECT_EQ(context.part_index, part_index);
    EXPECT_EQ(context.part_key.kind, query::ModulePartKind::fragment);
    EXPECT_EQ(context.part_key.stable_index, part_index);
}

void expect_unresolved_fragment_source_part(
    const tooling::IdeModulePartContext& context, const std::string_view module_name, const std::string_view part_name)
{
    EXPECT_TRUE(context.valid);
    EXPECT_FALSE(context.resolved);
    EXPECT_TRUE(query::is_valid(context.module_key));
    EXPECT_FALSE(query::is_valid(context.part_key));
    EXPECT_EQ(context.kind, query::ModulePartKind::fragment);
    EXPECT_EQ(context.module_name, module_name);
    EXPECT_EQ(context.part_name, part_name);
    EXPECT_EQ(context.part_index, IDE_TOOLING_PRIMARY_PART_INDEX);
}

void mark_checked_function_part(tooling::IdeSnapshot& snapshot, const std::string_view name, const base::u32 part_index)
{
    bool updated = false;
    for (auto& entry : snapshot.checked.functions) {
        sema::FunctionSignature& signature = entry.second;
        if (signature.name == name) {
            signature.part_index = part_index;
            updated = true;
        }
    }
    ASSERT_TRUE(updated) << name;
}

void mark_checked_struct_part(tooling::IdeSnapshot& snapshot, const std::string_view name, const base::u32 part_index)
{
    bool updated = false;
    for (auto& entry : snapshot.checked.structs) {
        sema::StructInfo& info = entry.second;
        if (info.name == name) {
            info.part_index = part_index;
            updated = true;
        }
    }
    ASSERT_TRUE(updated) << name;
}

void mark_checked_type_alias_part(
    tooling::IdeSnapshot& snapshot, const std::string_view name, const base::u32 part_index)
{
    bool updated = false;
    for (auto& entry : snapshot.checked.type_aliases) {
        sema::TypeAliasInfo& info = entry.second;
        if (info.name == name) {
            info.part_index = part_index;
            updated = true;
        }
    }
    ASSERT_TRUE(updated) << name;
}

void mark_checked_enum_case_part(tooling::IdeSnapshot& snapshot, const std::string_view enum_name,
    const std::string_view case_name, const base::u32 part_index)
{
    bool updated = false;
    for (auto& entry : snapshot.checked.enum_cases) {
        sema::EnumCaseInfo& info = entry.second;
        if (info.enum_name == enum_name && info.case_name == case_name) {
            info.part_index = part_index;
            updated = true;
        }
    }
    ASSERT_TRUE(updated) << enum_name << "." << case_name;
}

void mark_checked_generic_template_part(
    tooling::IdeSnapshot& snapshot, const std::string_view name, const base::u32 part_index)
{
    bool updated = false;
    for (sema::GenericTemplateSignatureInfo& info : snapshot.checked.generic_template_signatures) {
        if (info.name == name) {
            info.part_index = part_index;
            updated = true;
        }
    }
    ASSERT_TRUE(updated) << name;
}

[[nodiscard]] std::string large_tooling_source()
{
    std::string source = "module ide.large;\n";
    for (base::usize index = 0; index < IDE_TOOLING_LARGE_FUNCTION_COUNT; ++index) {
        source += "fn f_";
        source += std::to_string(index);
        source += "() -> i32 { return ";
        source += std::to_string(index);
        source += "; }\n";
    }
    source += "fn main() -> i32 { return f_";
    source += std::to_string(IDE_TOOLING_LARGE_FUNCTION_COUNT - 1U);
    source += "(); }\n";
    return source;
}

} // namespace

TEST(CoreUnit, IdeToolingBuildsQueryBackedLosslessSnapshot)
{
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(IDE_TOOLING_SOURCE));

    EXPECT_TRUE(snapshot.lexed);
    EXPECT_TRUE(snapshot.parsed);
    EXPECT_TRUE(snapshot.checked_semantics);
    EXPECT_GT(snapshot.checked.functions.size(), 0U);
    EXPECT_FALSE(snapshot.has_errors);
    EXPECT_TRUE(snapshot.diagnostics.empty());
    EXPECT_EQ(snapshot.sources.text(snapshot.source_id), IDE_TOOLING_SOURCE);
    EXPECT_EQ(snapshot.lossless.reconstruct_text(), IDE_TOOLING_SOURCE);
    EXPECT_TRUE(snapshot.lossless.is_structurally_valid());
    EXPECT_TRUE(snapshot.query.source_stage.lex_config.retain_trivia);
    EXPECT_TRUE(snapshot.query.source_stage.parser_config.build_lossless_tree);
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::file_content));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::lex_file));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::parse_file));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::module_graph));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::module_part));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::item_list));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::module_exports));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::item_signature));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::function_body_syntax));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::type_check_body));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::diagnostics));
    EXPECT_TRUE(has_dependency_kind(snapshot, query::QueryKind::lex_file, query::QueryKind::file_content));
    EXPECT_TRUE(has_dependency_kind(snapshot, query::QueryKind::parse_file, query::QueryKind::lex_file));
    EXPECT_TRUE(has_dependency_kind(snapshot, query::QueryKind::module_part, query::QueryKind::parse_file));
    EXPECT_TRUE(has_dependency_kind(snapshot, query::QueryKind::item_list, query::QueryKind::module_graph));
    EXPECT_TRUE(has_dependency_kind(snapshot, query::QueryKind::module_exports, query::QueryKind::item_list));
    EXPECT_TRUE(has_dependency_kind(snapshot, query::QueryKind::item_signature, query::QueryKind::module_exports));
    EXPECT_TRUE(
        has_dependency_kind(snapshot, query::QueryKind::type_check_body, query::QueryKind::function_body_syntax));
    EXPECT_TRUE(has_dependency_kind(snapshot, query::QueryKind::type_check_body, query::QueryKind::item_signature));
    EXPECT_TRUE(has_dependency_kind(snapshot, query::QueryKind::diagnostics, query::QueryKind::parse_file));
    EXPECT_TRUE(has_semantic_fact_query_kind(snapshot, query::QueryKind::item_signature, "add"));
    EXPECT_TRUE(has_semantic_fact_kind(
        snapshot, tooling::IdeSemanticFactKind::function_body_syntax, query::QueryKind::function_body_syntax, "add"));
    EXPECT_TRUE(has_semantic_fact_kind(
        snapshot, tooling::IdeSemanticFactKind::type_check_body, query::QueryKind::type_check_body, "main"));
    expect_primary_source_part(snapshot.source_part, "ide.snapshot");
    EXPECT_EQ(
        IDE_TOOLING_SOURCE.substr(snapshot.source_part.module_range.begin, snapshot.source_part.module_range.length()),
        "ide.snapshot");

    const std::optional<tooling::IdeTokenInfo> eof_token =
        tooling::token_info_at_offset(snapshot, IDE_TOOLING_SOURCE.size());
    ASSERT_TRUE(eof_token.has_value());
    EXPECT_TRUE(eof_token->valid);
    EXPECT_EQ(eof_token->kind, syntax::TokenKind::eof);
    EXPECT_EQ(eof_token->node, snapshot.lossless.root_id());
}

TEST(CoreUnit, IdeToolingHandlesDefaultPackageEmptyBuffersAndInvalidOffsets)
{
    tooling::IdeSnapshotRequest request = request_for("");
    request.package_identity.clear();
    request.virtual_buffer_identity.clear();
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request);

    constexpr std::array<std::string_view, 1> default_package{"ide"};
    EXPECT_EQ(snapshot.query.source_stage.file.package, query::package_key(default_package));
    EXPECT_TRUE(snapshot.lexed);
    EXPECT_FALSE(snapshot.has_errors);
    EXPECT_TRUE(snapshot.lossless.is_structurally_valid());
    EXPECT_EQ(snapshot.lossless.reconstruct_text(), "");

    const std::optional<tooling::IdeTokenInfo> eof_token = tooling::token_info_at_offset(snapshot, 0U);
    ASSERT_TRUE(eof_token.has_value());
    EXPECT_TRUE(eof_token->valid);
    EXPECT_EQ(eof_token->kind, syntax::TokenKind::eof);
    EXPECT_EQ(eof_token->text, "");
    EXPECT_FALSE(eof_token->trivia);
    EXPECT_TRUE(eof_token->node_key.has_value());

    EXPECT_FALSE(tooling::token_info_at_offset(snapshot, 1U).has_value());
    EXPECT_FALSE(tooling::definition_at_offset(snapshot, 0U).has_value());
    EXPECT_TRUE(tooling::references_at_offset(snapshot, 0U).empty());
    EXPECT_FALSE(tooling::hover_at_offset(snapshot, 1U).has_value());
    EXPECT_FALSE(snapshot.source_part.valid);
    EXPECT_FALSE(snapshot.source_part.resolved);
    EXPECT_FALSE(query::is_valid(snapshot.source_part.module_key));
    EXPECT_FALSE(query::is_valid(snapshot.source_part.part_key));

    const std::optional<tooling::IdeHoverInfo> eof_hover = tooling::hover_at_offset(snapshot, 0U);
    ASSERT_TRUE(eof_hover.has_value());
    EXPECT_TRUE(eof_hover->valid);
    EXPECT_NE(eof_hover->label.find("token eof"), std::string::npos);

    const tooling::IdeEditImpact empty_delete = tooling::edit_impact_for_range(snapshot, 0U, 1U);
    EXPECT_TRUE(empty_delete.valid);
    EXPECT_TRUE(empty_delete.node_key.has_value());
    EXPECT_EQ(empty_delete.range.begin, 0U);
    EXPECT_EQ(empty_delete.range.end, 0U);
}

TEST(CoreUnit, IdeToolingServesTokenHoverDefinitionReferencesAndEditImpact)
{
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(IDE_TOOLING_SOURCE));
    ASSERT_TRUE(snapshot.parsed);

    const base::usize call_offset = IDE_TOOLING_SOURCE.find("add(1");
    ASSERT_NE(call_offset, std::string_view::npos);
    const std::optional<tooling::IdeTokenInfo> token = tooling::token_info_at_offset(snapshot, call_offset);
    ASSERT_TRUE(token.has_value());
    EXPECT_TRUE(token->valid);
    EXPECT_EQ(token->kind, syntax::TokenKind::identifier);
    EXPECT_EQ(token->text, "add");
    EXPECT_FALSE(token->trivia);
    EXPECT_TRUE(token->node_key.has_value());

    const std::optional<tooling::IdeDefinition> definition = tooling::definition_at_offset(snapshot, call_offset);
    ASSERT_TRUE(definition.has_value());
    EXPECT_TRUE(definition->valid);
    EXPECT_EQ(definition->name, "add");
    EXPECT_EQ(definition->kind, "function");
    EXPECT_EQ(IDE_TOOLING_SOURCE.substr(definition->range.begin, definition->range.length()), "add");
    EXPECT_EQ(definition->key.kind, query::DefKind::function);
    EXPECT_EQ(definition->part_index, IDE_TOOLING_PRIMARY_PART_INDEX);

    const std::vector<tooling::IdeReference> references = tooling::references_at_offset(snapshot, call_offset);
    ASSERT_GE(references.size(), 2U);
    EXPECT_TRUE(std::ranges::any_of(references, [](const tooling::IdeReference& reference) {
        return reference.is_definition;
    }));
    EXPECT_TRUE(std::ranges::any_of(references, [](const tooling::IdeReference& reference) {
        return reference.range.begin == IDE_TOOLING_SOURCE.find("add(1") && !reference.is_definition;
    }));

    const std::optional<tooling::IdeHoverInfo> hover = tooling::hover_at_offset(snapshot, call_offset);
    ASSERT_TRUE(hover.has_value());
    EXPECT_TRUE(hover->valid);
    EXPECT_NE(hover->label.find("identifier `add` -> function"), std::string::npos);
    ASSERT_TRUE(hover->definition.has_value());
    EXPECT_EQ(hover->definition->name, "add");
    EXPECT_EQ(hover->definition->part_index, IDE_TOOLING_PRIMARY_PART_INDEX);

    const base::usize parameter_offset = IDE_TOOLING_SOURCE.find("a: i32");
    ASSERT_NE(parameter_offset, std::string_view::npos);
    const std::optional<tooling::IdeDefinition> parameter_definition =
        tooling::definition_at_offset(snapshot, parameter_offset);
    ASSERT_TRUE(parameter_definition.has_value());
    EXPECT_TRUE(parameter_definition->valid);
    EXPECT_EQ(parameter_definition->name, "a");
    EXPECT_EQ(parameter_definition->kind, "parameter");
    EXPECT_EQ(IDE_TOOLING_SOURCE.substr(parameter_definition->range.begin, parameter_definition->range.length()), "a");
    EXPECT_EQ(parameter_definition->key.kind, query::DefKind::value);
    EXPECT_EQ(parameter_definition->part_index, IDE_TOOLING_PRIMARY_PART_INDEX);

    const std::vector<tooling::IdeReference> parameter_references =
        tooling::references_at_offset(snapshot, parameter_offset);
    ASSERT_GE(parameter_references.size(), 2U);
    EXPECT_TRUE(std::ranges::any_of(parameter_references, [](const tooling::IdeReference& reference) {
        return reference.is_definition;
    }));
    const std::optional<tooling::IdeHoverInfo> parameter_hover = tooling::hover_at_offset(snapshot, parameter_offset);
    ASSERT_TRUE(parameter_hover.has_value());
    EXPECT_TRUE(parameter_hover->valid);
    EXPECT_NE(parameter_hover->label.find("identifier `a` -> parameter"), std::string::npos);
    ASSERT_TRUE(parameter_hover->definition.has_value());
    EXPECT_EQ(parameter_hover->definition->name, "a");
    EXPECT_EQ(parameter_hover->definition->part_index, IDE_TOOLING_PRIMARY_PART_INDEX);

    const base::usize edit_offset = IDE_TOOLING_SOURCE.find("return value");
    ASSERT_NE(edit_offset, std::string_view::npos);
    const tooling::IdeEditImpact impact =
        tooling::edit_impact_for_range(snapshot, edit_offset, std::string_view{"return"}.size());
    EXPECT_TRUE(impact.valid);
    EXPECT_TRUE(impact.node_key.has_value());
    EXPECT_GT(impact.token_count, 0U);
    EXPECT_EQ(impact.node, snapshot.lossless.node_at_offset(edit_offset));
    EXPECT_NE(impact.node, snapshot.lossless.root_id());
    EXPECT_LE(impact.range.begin, edit_offset);
    EXPECT_GT(impact.range.end, edit_offset);
}

TEST(CoreUnit, IdeToolingExposesCheckedModulePartOriginsThroughDefinitions)
{
    constexpr std::string_view source = "module ide.parts;\n"
                                        "fn from_part() -> i32 { return 1; }\n"
                                        "struct Carrier { count: i32; }\n"
                                        "fn use(carrier: Carrier) -> i32 {\n"
                                        "  return from_part() + carrier.count;\n"
                                        "}\n";
    tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));
    ASSERT_TRUE(snapshot.parsed);
    ASSERT_TRUE(snapshot.checked_semantics);

    mark_checked_function_part(snapshot, "from_part", IDE_TOOLING_FUNCTION_PART_INDEX);
    mark_checked_struct_part(snapshot, "Carrier", IDE_TOOLING_STRUCT_PART_INDEX);

    const base::usize function_call_offset = source.find("from_part() +");
    ASSERT_NE(function_call_offset, std::string_view::npos);
    const std::optional<tooling::IdeDefinition> function_definition =
        tooling::definition_at_offset(snapshot, function_call_offset);
    ASSERT_TRUE(function_definition.has_value());
    EXPECT_TRUE(function_definition->valid);
    EXPECT_EQ(function_definition->name, "from_part");
    EXPECT_EQ(function_definition->kind, "function");
    EXPECT_EQ(function_definition->part_index, IDE_TOOLING_FUNCTION_PART_INDEX);

    const std::optional<tooling::IdeHoverInfo> function_hover =
        tooling::hover_at_offset(snapshot, function_call_offset);
    ASSERT_TRUE(function_hover.has_value());
    ASSERT_TRUE(function_hover->definition.has_value());
    EXPECT_EQ(function_hover->definition->part_index, IDE_TOOLING_FUNCTION_PART_INDEX);
    EXPECT_NE(function_hover->label.find("identifier `from_part` -> function"), std::string::npos);

    const base::usize field_use_offset = source.rfind("count");
    ASSERT_NE(field_use_offset, std::string_view::npos);
    const std::optional<tooling::IdeDefinition> field_definition =
        tooling::definition_at_offset(snapshot, field_use_offset);
    ASSERT_TRUE(field_definition.has_value());
    EXPECT_TRUE(field_definition->valid);
    EXPECT_EQ(field_definition->name, "count");
    EXPECT_EQ(field_definition->kind, "struct_field");
    EXPECT_EQ(field_definition->part_index, IDE_TOOLING_STRUCT_PART_INDEX);

    const std::optional<tooling::IdeHoverInfo> field_hover = tooling::hover_at_offset(snapshot, field_use_offset);
    ASSERT_TRUE(field_hover.has_value());
    ASSERT_TRUE(field_hover->definition.has_value());
    EXPECT_EQ(field_hover->definition->part_index, IDE_TOOLING_STRUCT_PART_INDEX);
    EXPECT_NE(field_hover->label.find("identifier `count` -> struct_field"), std::string::npos);
}

TEST(CoreUnit, IdeToolingExposesCheckedPartOriginsForTemplatesAliasesAndEnumCases)
{
    constexpr std::string_view source = "module ide.more_parts;\n"
                                        "type Count = i32;\n"
                                        "enum Mode: u8 { fast = 1, slow = 2 }\n"
                                        "struct Box[T] { value: T; }\n"
                                        "fn keep(value: Count) -> Count { return value; }\n";
    tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));
    ASSERT_TRUE(snapshot.parsed);
    ASSERT_TRUE(snapshot.checked_semantics);
    EXPECT_TRUE(has_semantic_fact_kind(snapshot, tooling::IdeSemanticFactKind::generic_template_signature,
        query::QueryKind::generic_template_signature, "Box"));

    mark_checked_type_alias_part(snapshot, "Count", IDE_TOOLING_ALIAS_PART_INDEX);
    mark_checked_enum_case_part(snapshot, "Mode", "fast", IDE_TOOLING_ENUM_CASE_PART_INDEX);
    mark_checked_generic_template_part(snapshot, "Box", IDE_TOOLING_TEMPLATE_PART_INDEX);

    const base::usize alias_use_offset = source.find("Count) ->");
    ASSERT_NE(alias_use_offset, std::string_view::npos);
    const std::optional<tooling::IdeDefinition> alias_definition =
        tooling::definition_at_offset(snapshot, alias_use_offset);
    ASSERT_TRUE(alias_definition.has_value());
    EXPECT_EQ(alias_definition->name, "Count");
    EXPECT_EQ(alias_definition->kind, "type_alias");
    EXPECT_EQ(alias_definition->part_index, IDE_TOOLING_ALIAS_PART_INDEX);

    const base::usize enum_case_offset = source.find("fast");
    ASSERT_NE(enum_case_offset, std::string_view::npos);
    const std::optional<tooling::IdeDefinition> enum_case_definition =
        tooling::definition_at_offset(snapshot, enum_case_offset);
    ASSERT_TRUE(enum_case_definition.has_value());
    EXPECT_EQ(enum_case_definition->name, "fast");
    EXPECT_EQ(enum_case_definition->kind, "enum_case");
    EXPECT_EQ(enum_case_definition->part_index, IDE_TOOLING_ENUM_CASE_PART_INDEX);

    const base::usize template_offset = source.find("Box[T]");
    ASSERT_NE(template_offset, std::string_view::npos);
    const std::optional<tooling::IdeDefinition> template_definition =
        tooling::definition_at_offset(snapshot, template_offset);
    ASSERT_TRUE(template_definition.has_value());
    EXPECT_EQ(template_definition->name, "Box");
    EXPECT_EQ(template_definition->kind, "generic_template");
    EXPECT_EQ(template_definition->part_index, IDE_TOOLING_TEMPLATE_PART_INDEX);
}

TEST(CoreUnit, IdeToolingReportsKeywordTriviaUndefinedReferenceAndCrossNodeEdit)
{
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(IDE_TOOLING_SOURCE));
    ASSERT_TRUE(snapshot.parsed);

    const base::usize function_keyword_offset = IDE_TOOLING_SOURCE.find("fn add");
    ASSERT_NE(function_keyword_offset, std::string_view::npos);
    const std::optional<tooling::IdeHoverInfo> keyword_hover =
        tooling::hover_at_offset(snapshot, function_keyword_offset);
    ASSERT_TRUE(keyword_hover.has_value());
    EXPECT_TRUE(keyword_hover->valid);
    EXPECT_NE(keyword_hover->label.find("token kw_fn"), std::string::npos);
    EXPECT_FALSE(tooling::definition_at_offset(snapshot, function_keyword_offset).has_value());
    EXPECT_TRUE(tooling::references_at_offset(snapshot, function_keyword_offset).empty());

    const base::usize trivia_offset = IDE_TOOLING_SOURCE.find("  return a");
    ASSERT_NE(trivia_offset, std::string_view::npos);
    const std::optional<tooling::IdeTokenInfo> trivia = tooling::token_info_at_offset(snapshot, trivia_offset);
    ASSERT_TRUE(trivia.has_value());
    EXPECT_TRUE(trivia->trivia);

    const base::usize local_value_offset = IDE_TOOLING_SOURCE.find("value =");
    ASSERT_NE(local_value_offset, std::string_view::npos);
    const std::optional<tooling::IdeDefinition> local_definition =
        tooling::definition_at_offset(snapshot, local_value_offset);
    ASSERT_TRUE(local_definition.has_value());
    EXPECT_TRUE(local_definition->valid);
    EXPECT_EQ(local_definition->name, "value");
    EXPECT_EQ(local_definition->kind, "local");
    EXPECT_EQ(IDE_TOOLING_SOURCE.substr(local_definition->range.begin, local_definition->range.length()), "value");
    EXPECT_EQ(local_definition->part_index, IDE_TOOLING_PRIMARY_PART_INDEX);
    const std::optional<tooling::IdeHoverInfo> local_hover = tooling::hover_at_offset(snapshot, local_value_offset);
    ASSERT_TRUE(local_hover.has_value());
    EXPECT_TRUE(local_hover->valid);
    EXPECT_NE(local_hover->label.find("identifier `value` -> local"), std::string::npos);
    ASSERT_TRUE(local_hover->definition.has_value());
    EXPECT_EQ(local_hover->definition->name, "value");
    EXPECT_EQ(local_hover->definition->part_index, IDE_TOOLING_PRIMARY_PART_INDEX);
    const std::vector<tooling::IdeReference> local_references =
        tooling::references_at_offset(snapshot, local_value_offset);
    ASSERT_GE(local_references.size(), 2U);
    EXPECT_TRUE(std::ranges::any_of(local_references, [](const tooling::IdeReference& reference) {
        return reference.is_definition;
    }));
    EXPECT_TRUE(std::ranges::any_of(local_references, [](const tooling::IdeReference& reference) {
        return !reference.is_definition;
    }));

    const base::usize first_function_offset = IDE_TOOLING_SOURCE.find("fn add");
    const base::usize second_function_offset = IDE_TOOLING_SOURCE.find("fn main");
    const base::usize first_body_offset = IDE_TOOLING_SOURCE.find("return a + b");
    const base::usize second_body_offset = IDE_TOOLING_SOURCE.find("return value");
    ASSERT_NE(first_function_offset, std::string_view::npos);
    ASSERT_NE(second_function_offset, std::string_view::npos);
    ASSERT_NE(first_body_offset, std::string_view::npos);
    ASSERT_NE(second_body_offset, std::string_view::npos);
    ASSERT_GT(second_function_offset, first_function_offset);
    ASSERT_GT(second_body_offset, first_body_offset);
    const tooling::IdeEditImpact cross_function_impact =
        tooling::edit_impact_for_range(snapshot, first_function_offset, second_function_offset - first_function_offset);
    EXPECT_TRUE(cross_function_impact.valid);
    EXPECT_TRUE(cross_function_impact.node_key.has_value());
    EXPECT_EQ(cross_function_impact.node, snapshot.lossless.root_id());

    const tooling::IdeEditImpact cross_body_impact =
        tooling::edit_impact_for_range(snapshot, first_body_offset, second_body_offset - first_body_offset);
    EXPECT_TRUE(cross_body_impact.valid);
    EXPECT_TRUE(cross_body_impact.node_key.has_value());
    EXPECT_EQ(cross_body_impact.node, snapshot.lossless.root_id());

    const tooling::IdeEditImpact module_to_body_impact =
        tooling::edit_impact_for_range(snapshot, 0U, first_body_offset + 1U);
    EXPECT_TRUE(module_to_body_impact.valid);
    EXPECT_TRUE(module_to_body_impact.node_key.has_value());
    EXPECT_EQ(module_to_body_impact.node, snapshot.lossless.root_id());

    const tooling::IdeEditImpact eof_insert =
        tooling::edit_impact_for_range(snapshot, IDE_TOOLING_SOURCE.size() + 1U, 0U);
    EXPECT_TRUE(eof_insert.valid);
    EXPECT_TRUE(eof_insert.node_key.has_value());
}

TEST(CoreUnit, IdeToolingRestrictsLocalReferencesToDeclaringFunction)
{
    constexpr std::string_view source = "module ide.local_scope;\n"
                                        "fn first() -> i32 {\n"
                                        "  let value = 1;\n"
                                        "  return value;\n"
                                        "}\n"
                                        "fn second() -> i32 {\n"
                                        "  let value = 2;\n"
                                        "  return value;\n"
                                        "}\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));
    ASSERT_TRUE(snapshot.parsed);

    const base::usize first_value_offset = source.find("value = 1");
    const base::usize second_function_offset = source.find("fn second");
    ASSERT_NE(first_value_offset, std::string_view::npos);
    ASSERT_NE(second_function_offset, std::string_view::npos);

    const std::vector<tooling::IdeReference> references = tooling::references_at_offset(snapshot, first_value_offset);
    ASSERT_EQ(references.size(), 2U);
    for (const tooling::IdeReference& reference : references) {
        EXPECT_LT(reference.range.begin, second_function_offset);
    }
}

TEST(CoreUnit, IdeToolingResolvesSupportedTopLevelDefinitionKinds)
{
    constexpr std::string_view source = "module ide.defs;\n"
                                        "pub const answer: i32 = 42;\n"
                                        "type Count = i32;\n"
                                        "pub opaque struct Handle;\n"
                                        "pub struct Point { pub x: i32; }\n"
                                        "enum Mode { fast, slow }\n"
                                        "impl Point { fn read(self: *const Point) -> i32 { return self.x; } }\n"
                                        "extern c { fn native(value: i32) -> i32 @name(\"native\"); }\n"
                                        "fn use_defs() -> i32 { return answer; }\n";
    tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));
    ASSERT_TRUE(snapshot.parsed);
    ASSERT_TRUE(snapshot.lossless.is_structurally_valid());

    expect_definition_kind(snapshot, source, "answer", "const", query::DefKind::const_);
    expect_definition_kind(snapshot, source, "Count", "type_alias", query::DefKind::type_alias);
    expect_definition_kind(snapshot, source, "Handle", "opaque_struct", query::DefKind::struct_);
    expect_definition_kind(snapshot, source, "Point", "struct", query::DefKind::struct_);
    expect_definition_kind(snapshot, source, "Mode", "enum", query::DefKind::enum_);
    expect_definition_kind(snapshot, source, "use_defs", "function", query::DefKind::function);

    snapshot.ast.module_path.parts.clear();
    expect_definition_kind(snapshot, source, "answer", "const", query::DefKind::const_);

    for (base::usize index = 0; index < snapshot.ast.items.size(); ++index) {
        const syntax::ItemNode* item = snapshot.ast.items.ptr(index);
        if (item != nullptr && item->name == "answer") {
            syntax::ItemNode moved_item = snapshot.ast.items.take(index);
            moved_item.range = base::SourceRange{snapshot.source_id, 0U, 0U};
            snapshot.ast.items.set(index, std::move(moved_item));
            break;
        }
    }
    const std::optional<tooling::IdeDefinition> fallback_range_definition =
        tooling::definition_at_offset(snapshot, source.find("answer"));
    ASSERT_TRUE(fallback_range_definition.has_value());
    EXPECT_EQ(fallback_range_definition->range.begin, 0U);
    EXPECT_EQ(fallback_range_definition->range.end, 0U);
}

TEST(CoreUnit, IdeToolingReturnsStructuredDiagnosticsForInvalidSource)
{
    constexpr std::string_view source = "module ide.invalid;\n"
                                        "fn main() -> i32 {\n"
                                        "  let value: i32 = true;\n"
                                        "  return value;\n"
                                        "}\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));

    EXPECT_TRUE(snapshot.lexed);
    EXPECT_TRUE(snapshot.parsed);
    EXPECT_FALSE(snapshot.checked_semantics);
    EXPECT_TRUE(snapshot.has_errors);
    expect_primary_source_part(snapshot.source_part, "ide.invalid");
    ASSERT_FALSE(snapshot.diagnostics.empty());
    expect_primary_source_part(snapshot.diagnostics.front().source_part, "ide.invalid");
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::diagnostics));
    EXPECT_TRUE(has_dependency_kind(snapshot, query::QueryKind::diagnostics, query::QueryKind::parse_file));
    EXPECT_TRUE(std::ranges::any_of(snapshot.diagnostics, [](const tooling::IdeDiagnostic& diagnostic) {
        return diagnostic.category == base::DiagnosticCategory::type
            && diagnostic.code == base::DiagnosticCode::semantic_type_mismatch && diagnostic.start.line == 3U
            && diagnostic.path == "/workspace/ide_snapshot.ax";
    }));
    EXPECT_TRUE(
        has_diagnostic_owner_stage_profile(snapshot, base::DiagnosticCategory::type, IDE_TOOLING_STAGE_SEMA_ANALYZE));
}

TEST(CoreUnit, IdeToolingCarriesUnresolvedFragmentPartContextOnDiagnostics)
{
    constexpr std::string_view source = "module ide.invalid_part part parser;\n"
                                        "fn main() -> i32 {\n"
                                        "  let value: i32 = true;\n"
                                        "  return value;\n"
                                        "}\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));

    EXPECT_TRUE(snapshot.lexed);
    EXPECT_TRUE(snapshot.parsed);
    EXPECT_FALSE(snapshot.checked_semantics);
    EXPECT_TRUE(snapshot.has_errors);
    expect_unresolved_fragment_source_part(snapshot.source_part, "ide.invalid_part", "parser");
    EXPECT_EQ(
        source.substr(snapshot.source_part.part_range.begin, snapshot.source_part.part_range.length()), "part parser");

    ASSERT_FALSE(snapshot.diagnostics.empty());
    expect_unresolved_fragment_source_part(snapshot.diagnostics.front().source_part, "ide.invalid_part", "parser");
    EXPECT_TRUE(std::ranges::any_of(snapshot.diagnostics, [](const tooling::IdeDiagnostic& diagnostic) {
        return diagnostic.category == base::DiagnosticCategory::type
            && diagnostic.code == base::DiagnosticCode::semantic_type_mismatch && diagnostic.source_part.valid
            && !diagnostic.source_part.resolved && diagnostic.source_part.kind == query::ModulePartKind::fragment;
    }));
}

TEST(CoreUnit, IdeToolingRecoversFragmentPartContextFromOwningPrimary)
{
    const fs::path work = ide_tooling_tmp_root() / "ide-owned-part-context";
    static_cast<void>(write_ide_tooling_source(work / "vm.ax",
        "module ide.owned_part;\n"
        "part lexer;\n"
        "part parser;\n"
        "fn primary() -> i32 { return 0; }\n"));
    static_cast<void>(write_ide_tooling_source(work / "vm.parts" / "lexer.ax",
        "module ide.owned_part part lexer;\n"
        "fn scan() -> i32 { return 1; }\n"));
    const fs::path parser_path = write_ide_tooling_source(work / "vm.parts" / "parser.ax",
        "module ide.owned_part part parser;\n"
        "fn main() -> i32 {\n"
        "  let value: i32 = true;\n"
        "  return value;\n"
        "}\n");
    const std::string part_source = "module ide.owned_part part parser;\n"
                                    "fn main() -> i32 {\n"
                                    "  let value: i32 = true;\n"
                                    "  return value;\n"
                                    "}\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for_path(part_source, parser_path));

    EXPECT_TRUE(snapshot.lexed);
    EXPECT_TRUE(snapshot.parsed);
    EXPECT_FALSE(snapshot.checked_semantics);
    EXPECT_TRUE(snapshot.has_errors);
    expect_resolved_fragment_source_part(
        snapshot.source_part, "ide.owned_part", "parser", IDE_TOOLING_RECOVERED_PARSER_PART_INDEX);
    EXPECT_EQ(part_source.substr(snapshot.source_part.part_range.begin, snapshot.source_part.part_range.length()),
        "part parser");

    ASSERT_FALSE(snapshot.diagnostics.empty());
    expect_resolved_fragment_source_part(
        snapshot.diagnostics.front().source_part, "ide.owned_part", "parser", IDE_TOOLING_RECOVERED_PARSER_PART_INDEX);
    EXPECT_TRUE(std::ranges::any_of(snapshot.diagnostics, [](const tooling::IdeDiagnostic& diagnostic) {
        return diagnostic.category == base::DiagnosticCategory::type
            && diagnostic.code == base::DiagnosticCode::semantic_type_mismatch && diagnostic.source_part.valid
            && diagnostic.source_part.resolved && diagnostic.source_part.kind == query::ModulePartKind::fragment
            && diagnostic.source_part.part_index == IDE_TOOLING_RECOVERED_PARSER_PART_INDEX;
    }));
}

TEST(CoreUnit, IdeToolingLeavesFragmentContextUnresolvedWhenPrimaryDoesNotOwnPart)
{
    const fs::path mismatch_work = ide_tooling_tmp_root() / "ide-mismatched-part-context";
    static_cast<void>(write_ide_tooling_source(mismatch_work / "vm.ax",
        "module ide.other_owner;\n"
        "part parser;\n"));
    const fs::path mismatch_parser_path = write_ide_tooling_source(mismatch_work / "vm.parts" / "parser.ax",
        "module ide.requested_owner part parser;\n"
        "fn main() -> i32 { return 0; }\n");
    const tooling::IdeSnapshot mismatch_snapshot =
        tooling::build_ide_snapshot(request_for_path("module ide.requested_owner part parser;\n"
                                                     "fn main() -> i32 { return 0; }\n",
            mismatch_parser_path));

    ASSERT_TRUE(mismatch_snapshot.parsed);
    expect_unresolved_fragment_source_part(mismatch_snapshot.source_part, "ide.requested_owner", "parser");

    const fs::path unlisted_work = ide_tooling_tmp_root() / "ide-unlisted-part-context";
    static_cast<void>(write_ide_tooling_source(unlisted_work / "vm.ax",
        "module ide.unlisted_owner;\n"
        "part lexer;\n"));
    const fs::path unlisted_parser_path = write_ide_tooling_source(unlisted_work / "vm.parts" / "parser.ax",
        "module ide.unlisted_owner part parser;\n"
        "fn main() -> i32 { return 0; }\n");
    const tooling::IdeSnapshot unlisted_snapshot =
        tooling::build_ide_snapshot(request_for_path("module ide.unlisted_owner part parser;\n"
                                                     "fn main() -> i32 { return 0; }\n",
            unlisted_parser_path));

    ASSERT_TRUE(unlisted_snapshot.parsed);
    expect_unresolved_fragment_source_part(unlisted_snapshot.source_part, "ide.unlisted_owner", "parser");
}

TEST(CoreUnit, IdeToolingSeparatesLexAndParseFailuresIntoQuerySnapshots)
{
    constexpr std::string_view lex_error_source = "module ide.lex;\n"
                                                  "fn main() -> i32 { return \"unterminated\n"
                                                  "}\n";
    const tooling::IdeSnapshot lex_snapshot = tooling::build_ide_snapshot(request_for(lex_error_source));
    EXPECT_FALSE(lex_snapshot.lexed);
    EXPECT_FALSE(lex_snapshot.parsed);
    EXPECT_FALSE(lex_snapshot.checked_semantics);
    EXPECT_TRUE(lex_snapshot.has_errors);
    EXPECT_TRUE(lex_snapshot.lossless.is_structurally_valid());
    EXPECT_TRUE(lex_snapshot.lossless.tokens().empty());
    EXPECT_TRUE(has_diagnostic_category(lex_snapshot, base::DiagnosticCategory::lexer));
    EXPECT_TRUE(has_diagnostic_owner_stage_profile(
        lex_snapshot, base::DiagnosticCategory::lexer, IDE_TOOLING_STAGE_TOKENS_LEX));
    EXPECT_TRUE(has_diagnostic_owner_stage_profile(
        lex_snapshot, base::DiagnosticCategory::lexer, IDE_TOOLING_STAGE_MODULE_LEX));
    EXPECT_TRUE(has_record_kind(lex_snapshot, query::QueryKind::lex_file));
    EXPECT_TRUE(has_record_kind(lex_snapshot, query::QueryKind::parse_file));
    EXPECT_TRUE(has_record_kind(lex_snapshot, query::QueryKind::diagnostics));

    constexpr std::string_view parse_error_source = "module ide.parse;\n"
                                                    "let top_level = 1;\n"
                                                    "fn ok() -> i32 { return 0; }\n";
    const tooling::IdeSnapshot parse_snapshot = tooling::build_ide_snapshot(request_for(parse_error_source));
    EXPECT_TRUE(parse_snapshot.lexed);
    EXPECT_FALSE(parse_snapshot.parsed);
    EXPECT_FALSE(parse_snapshot.checked_semantics);
    EXPECT_TRUE(parse_snapshot.has_errors);
    EXPECT_TRUE(parse_snapshot.lossless.is_structurally_valid());
    EXPECT_TRUE(has_diagnostic_category(parse_snapshot, base::DiagnosticCategory::parser));
    EXPECT_TRUE(has_diagnostic_owner_stage_profile(
        parse_snapshot, base::DiagnosticCategory::parser, IDE_TOOLING_STAGE_MODULE_PARSE));
    EXPECT_FALSE(tooling::definition_at_offset(parse_snapshot, parse_error_source.find("top_level")).has_value());
    EXPECT_TRUE(has_record_kind(parse_snapshot, query::QueryKind::parse_file));
    EXPECT_TRUE(has_dependency_kind(parse_snapshot, query::QueryKind::parse_file, query::QueryKind::lex_file));
}

TEST(CoreUnit, IdeToolingHandlesLargeSnapshotStressWithoutBreakingQueryRecords)
{
    const std::string source = large_tooling_source();
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));
    ASSERT_TRUE(snapshot.lexed);
    ASSERT_TRUE(snapshot.parsed);
    EXPECT_FALSE(snapshot.has_errors);
    EXPECT_EQ(snapshot.lossless.reconstruct_text(), source);
    EXPECT_TRUE(snapshot.lossless.is_structurally_valid());
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::file_content));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::lex_file));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::parse_file));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::diagnostics));

    const std::string last_function_name = "f_" + std::to_string(IDE_TOOLING_LARGE_FUNCTION_COUNT - 1U);
    const base::usize call_offset = source.rfind(last_function_name + "()");
    ASSERT_NE(call_offset, std::string::npos);
    const std::optional<tooling::IdeDefinition> definition = tooling::definition_at_offset(snapshot, call_offset);
    ASSERT_TRUE(definition.has_value());
    EXPECT_EQ(definition->name, last_function_name);
    EXPECT_EQ(definition->kind, "function");

    const std::vector<tooling::IdeReference> references = tooling::references_at_offset(snapshot, call_offset);
    ASSERT_EQ(references.size(), 2U);
    EXPECT_TRUE(std::ranges::any_of(references, [](const tooling::IdeReference& reference) {
        return reference.is_definition;
    }));

    const tooling::IdeEditImpact broad_impact = tooling::edit_impact_for_range(snapshot, 0U, source.size());
    EXPECT_TRUE(broad_impact.valid);
    EXPECT_EQ(broad_impact.node, snapshot.lossless.root_id());
}

} // namespace aurex::test
