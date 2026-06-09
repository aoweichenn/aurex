#include <aurex/application/tooling/ide.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

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

constexpr std::string_view IDE_TOOLING_TRAIT_SOURCE = "module ide.traits;\n"
                                                      "trait Source {\n"
                                                      "  type Item;\n"
                                                      "  fn get(self: &Self) -> Self.Item;\n"
                                                      "}\n"
                                                      "struct Bytes { value: i32; }\n"
                                                      "impl Source for Bytes {\n"
                                                      "  type Item = i32;\n"
                                                      "  fn get(self: &Bytes) -> i32 { return self.value; }\n"
                                                      "}\n"
                                                      "fn read_i32[T](value: &T) -> i32 where T: Source[Item = i32] {\n"
                                                      "  return value.get();\n"
                                                      "}\n"
                                                      "fn main() -> i32 {\n"
                                                      "  let bytes = Bytes { value: 7 };\n"
                                                      "  return read_i32[Bytes](&bytes) - 7;\n"
                                                      "}\n";

constexpr std::string_view IDE_TOOLING_TRAIT_DEFAULT_SOURCE = "module ide.trait_defaults;\n"
                                                              "trait Reader {\n"
                                                              "  fn read(self: &Self) -> i32;\n"
                                                              "  fn is_empty(self: &Self) -> bool {\n"
                                                              "    return self.read() == 0;\n"
                                                              "  }\n"
                                                              "}\n"
                                                              "struct File { value: i32; }\n"
                                                              "struct Buffer { value: i32; }\n"
                                                              "impl Reader for File {\n"
                                                              "  fn read(self: &File) -> i32 { return self.value; }\n"
                                                              "}\n"
                                                              "impl Reader for Buffer {\n"
                                                              "  fn read(self: &Buffer) -> i32 { return self.value; }\n"
                                                              "  fn is_empty(self: &Buffer) -> bool { return false; }\n"
                                                              "}\n"
                                                              "fn main() -> i32 {\n"
                                                              "  let file = File { value: 0 };\n"
                                                              "  let buffer = Buffer { value: 1 };\n"
                                                              "  if file.is_empty() { return 0; }\n"
                                                              "  if buffer.is_empty() { return 1; }\n"
                                                              "  return 2;\n"
                                                              "}\n";

constexpr base::usize IDE_TOOLING_LARGE_FUNCTION_COUNT = 128;
constexpr base::u32 IDE_TOOLING_PRIMARY_PART_INDEX = 0;
constexpr base::u32 IDE_TOOLING_FUNCTION_PART_INDEX = 3;
constexpr base::u32 IDE_TOOLING_STRUCT_PART_INDEX = 4;
constexpr base::u32 IDE_TOOLING_ALIAS_PART_INDEX = 5;
constexpr base::u32 IDE_TOOLING_ENUM_CASE_PART_INDEX = 6;
constexpr base::u32 IDE_TOOLING_TEMPLATE_PART_INDEX = 7;
constexpr base::u32 IDE_TOOLING_RECOVERED_PARSER_PART_INDEX = 2;
constexpr base::u32 IDE_TOOLING_REQUIREMENT_ORDINAL_FALLBACK_OFFSET = 100U;
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

[[nodiscard]] const tooling::IdeSemanticFact* find_semantic_fact(const tooling::IdeSnapshot& snapshot,
    const tooling::IdeSemanticFactKind kind, const query::QueryKind query_kind, const std::string_view name)
{
    for (const tooling::IdeSemanticFact& fact : snapshot.query.semantic_facts) {
        if (fact.kind == kind && fact.query.kind == query_kind && fact.name == name && fact.checked) {
            return &fact;
        }
    }
    return nullptr;
}

[[nodiscard]] std::optional<sema::FunctionLookupKey> find_function_key(
    const sema::CheckedModule& checked, const std::string_view name)
{
    for (const auto& entry : checked.functions) {
        if (entry.second.name.view() == name) {
            return entry.first;
        }
    }
    return std::nullopt;
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

[[nodiscard]] bool has_semantic_token(
    const std::vector<tooling::IdeSemanticToken>& tokens, const std::string_view text,
    const std::string_view token_type)
{
    return std::ranges::any_of(tokens, [text, token_type](const tooling::IdeSemanticToken& token) {
        return token.text == text && token.token_type == token_type;
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

TEST(CoreUnit, IdeToolingProjectsBorrowSummaryAndLoanFacts)
{
    constexpr std::string_view source = "module ide.borrow_facts;\n"
                                        "fn id_ref(value: &i32) -> &i32 {\n"
                                        "  return value;\n"
                                        "}\n"
                                        "fn read(value: i32) -> i32 {\n"
                                        "  let ref_value: &i32 = &value;\n"
                                        "  return *ref_value;\n"
                                        "}\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));
    ASSERT_TRUE(snapshot.checked_semantics);
    EXPECT_FALSE(snapshot.has_errors);

    const tooling::IdeSemanticFact* const summary_fact = find_semantic_fact(
        snapshot, tooling::IdeSemanticFactKind::borrow_summary, query::QueryKind::type_check_body, "id_ref");
    ASSERT_NE(summary_fact, nullptr);
    EXPECT_NE(summary_fact->detail.find("deps=1"), std::string::npos) << summary_fact->detail;
    EXPECT_NE(summary_fact->detail.find("storage_escapes=0"), std::string::npos) << summary_fact->detail;
    EXPECT_NE(summary_fact->detail.find("unknown=false"), std::string::npos) << summary_fact->detail;

    const tooling::IdeSemanticFact* const contract_fact = find_semantic_fact(
        snapshot, tooling::IdeSemanticFactKind::borrow_contract, query::QueryKind::type_check_body, "id_ref");
    ASSERT_NE(contract_fact, nullptr);
    EXPECT_NE(contract_fact->detail.find("source=inferred"), std::string::npos) << contract_fact->detail;
    EXPECT_NE(contract_fact->detail.find("selectors=1"), std::string::npos) << contract_fact->detail;

    const tooling::IdeSemanticFact* const lifetime_fact = find_semantic_fact(
        snapshot, tooling::IdeSemanticFactKind::lifetime_facts, query::QueryKind::type_check_body, "id_ref");
    ASSERT_NE(lifetime_fact, nullptr);
    EXPECT_NE(lifetime_fact->detail.find("live_ranges="), std::string::npos) << lifetime_fact->detail;
    EXPECT_NE(lifetime_fact->detail.find("returns=1"), std::string::npos) << lifetime_fact->detail;
    EXPECT_NE(lifetime_fact->detail.find("violations=0"), std::string::npos) << lifetime_fact->detail;
    EXPECT_NE(lifetime_fact->detail.find("local_escapes=0"), std::string::npos) << lifetime_fact->detail;
    EXPECT_NE(lifetime_fact->detail.find("unknown_escapes=0"), std::string::npos) << lifetime_fact->detail;

    const tooling::IdeSemanticFact* const loan_fact = find_semantic_fact(
        snapshot, tooling::IdeSemanticFactKind::body_loan_check, query::QueryKind::type_check_body, "read");
    ASSERT_NE(loan_fact, nullptr);
    EXPECT_NE(loan_fact->detail.find("loans=1"), std::string::npos) << loan_fact->detail;
    EXPECT_NE(loan_fact->detail.find("reborrows=0"), std::string::npos) << loan_fact->detail;
    EXPECT_NE(loan_fact->detail.find("two_phase=0"), std::string::npos) << loan_fact->detail;
    EXPECT_NE(loan_fact->detail.find("conflicts=0"), std::string::npos) << loan_fact->detail;

    const base::usize id_ref_offset = source.find("id_ref");
    ASSERT_NE(id_ref_offset, std::string_view::npos);
    const std::optional<tooling::IdeHoverInfo> hover = tooling::hover_at_offset(snapshot, id_ref_offset);
    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->label.find("borrow_summary=deps=1"), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("storage_escapes=0"), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("borrow_contract=inferred/selectors=1/unknown=false/mismatch=false"), std::string::npos)
        << hover->label;
    EXPECT_NE(hover->label.find("lifetime=regions="), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("/returns=1/violations=0/local_escapes=0/unknown_escapes=0"), std::string::npos)
        << hover->label;
}

TEST(CoreUnit, IdeToolingProjectsCleanupMarkerFactsWhenIrFactsAreEnabled)
{
    constexpr std::string_view source = "module ide.cleanup_marker_facts;\n"
                                        "struct File { fd: i32; }\n"
                                        "impl Drop for File {\n"
                                        "  fn drop(self: deinit File) -> void {}\n"
                                        "}\n"
                                        "fn consume(value: File) -> void {}\n"
                                        "fn main() -> void {}\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));
    ASSERT_TRUE(snapshot.checked_semantics);
    EXPECT_FALSE(snapshot.has_errors);

    if (snapshot.cleanup_marker_facts.empty()) {
        EXPECT_FALSE(has_semantic_fact_kind(snapshot, tooling::IdeSemanticFactKind::cleanup_marker_facts,
            query::QueryKind::lower_function_ir, "consume"));
        return;
    }

    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::lower_function_ir));
    EXPECT_TRUE(has_dependency_kind(snapshot, query::QueryKind::lower_function_ir, query::QueryKind::type_check_body));
    const tooling::IdeSemanticFact* const cleanup_fact = find_semantic_fact(
        snapshot, tooling::IdeSemanticFactKind::cleanup_marker_facts, query::QueryKind::lower_function_ir, "consume");
    ASSERT_NE(cleanup_fact, nullptr);
    EXPECT_NE(cleanup_fact->detail.find("cleanup_marker_facts markers="), std::string::npos)
        << cleanup_fact->detail;
    EXPECT_NE(cleanup_fact->detail.find("static_custom_destructor="), std::string::npos)
        << cleanup_fact->detail;
    EXPECT_NE(cleanup_fact->detail.find("fingerprint="), std::string::npos)
        << cleanup_fact->detail;

    const base::usize consume_offset = source.find("consume");
    ASSERT_NE(consume_offset, std::string_view::npos);
    const std::optional<tooling::IdeHoverInfo> hover = tooling::hover_at_offset(snapshot, consume_offset);
    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->label.find("cleanup_markers=count="), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("/first_policy="), std::string::npos) << hover->label;
}

TEST(CoreUnit, IdeToolingProjectsDynAbiFactsAndDynDispatchHover)
{
    constexpr std::string_view source =
        "module ide.dyn_abi_facts;\n"
        "trait Draw {\n"
        "  fn draw(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Draw for File {\n"
        "  fn draw(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "fn render(drawable: &dyn Draw) -> i32 {\n"
        "  return drawable.draw();\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 11 };\n"
        "  let drawable: &dyn Draw = &file;\n"
        "  return render(drawable);\n"
        "}\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));
    ASSERT_TRUE(snapshot.checked_semantics);
    EXPECT_FALSE(snapshot.has_errors);

    if (snapshot.dyn_abi_facts.empty()) {
        EXPECT_FALSE(has_semantic_fact_kind(snapshot, tooling::IdeSemanticFactKind::dyn_abi_facts,
            query::QueryKind::lower_function_ir, "render"));
    } else {
        EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::lower_function_ir));
        EXPECT_TRUE(has_dependency_kind(snapshot, query::QueryKind::lower_function_ir,
            query::QueryKind::type_check_body));
        const tooling::IdeSemanticFact* const dyn_fact = find_semantic_fact(
            snapshot, tooling::IdeSemanticFactKind::dyn_abi_facts, query::QueryKind::lower_function_ir, "render");
        ASSERT_NE(dyn_fact, nullptr);
        EXPECT_NE(dyn_fact->detail.find("dyn_abi_facts objects="), std::string::npos)
            << dyn_fact->detail;
        EXPECT_NE(dyn_fact->detail.find("abi=borrowed_view_v1"), std::string::npos)
            << dyn_fact->detail;
        EXPECT_NE(dyn_fact->detail.find("metadata=borrowed_methods_only_v1"), std::string::npos)
            << dyn_fact->detail;
        EXPECT_NE(dyn_fact->detail.find("first_dispatch=vtable_slot slot=0"), std::string::npos)
            << dyn_fact->detail;

        const base::usize render_offset = source.find("render(drawable");
        ASSERT_NE(render_offset, std::string_view::npos);
        const std::optional<tooling::IdeHoverInfo> render_hover =
            tooling::hover_at_offset(snapshot, render_offset);
        ASSERT_TRUE(render_hover.has_value());
        EXPECT_NE(render_hover->label.find("dyn_abi=abi=borrowed_view_v1"), std::string::npos)
            << render_hover->label;
        EXPECT_NE(render_hover->label.find("/metadata=borrowed_methods_only_v1"), std::string::npos)
            << render_hover->label;
    }

    const base::usize draw_offset = source.find("draw();");
    ASSERT_NE(draw_offset, std::string_view::npos);
    const std::optional<tooling::IdeHoverInfo> draw_hover = tooling::hover_at_offset(snapshot, draw_offset);
    ASSERT_TRUE(draw_hover.has_value());
    EXPECT_NE(draw_hover->label.find("identifier `draw` -> trait_method"), std::string::npos)
        << draw_hover->label;
    EXPECT_NE(draw_hover->label.find("dyn_dispatch=dispatch=vtable_slot/slot=0"), std::string::npos)
        << draw_hover->label;
    EXPECT_NE(draw_hover->label.find("/abi=borrowed_view_v1"), std::string::npos) << draw_hover->label;
    EXPECT_NE(draw_hover->label.find("/metadata=borrowed_methods_only_v1"), std::string::npos)
        << draw_hover->label;
}

TEST(CoreUnit, IdeToolingProjectsSupertraitUpcastDynAbiFactsAndHover)
{
    constexpr std::string_view source =
        "module ide.dyn_supertrait_upcast_facts;\n"
        "trait Parent {\n"
        "  fn parent(self: &Self) -> i32;\n"
        "}\n"
        "trait Child: Parent {\n"
        "  fn child(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Parent for File {\n"
        "  fn parent(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "impl Child for File {\n"
        "  fn child(self: &File) -> i32 { return self.value + 1; }\n"
        "}\n"
        "fn widen(child: &dyn Child) -> &dyn Parent {\n"
        "  let parent: &dyn Parent = child;\n"
        "  return parent;\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 19 };\n"
        "  let child: &dyn Child = &file;\n"
        "  let parent: &dyn Parent = widen(child);\n"
        "  return parent.parent();\n"
        "}\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));
    ASSERT_TRUE(snapshot.checked_semantics);
    EXPECT_FALSE(snapshot.has_errors);
    EXPECT_FALSE(snapshot.dyn_abi_facts.empty());
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::lower_function_ir));
    EXPECT_TRUE(has_dependency_kind(snapshot, query::QueryKind::lower_function_ir,
        query::QueryKind::type_check_body));

    const tooling::IdeSemanticFact* const dyn_fact = find_semantic_fact(
        snapshot, tooling::IdeSemanticFactKind::dyn_abi_facts, query::QueryKind::lower_function_ir, "widen");
    ASSERT_NE(dyn_fact, nullptr);
    EXPECT_NE(dyn_fact->detail.find("dyn_abi_facts objects="), std::string::npos) << dyn_fact->detail;
    EXPECT_NE(dyn_fact->detail.find("upcasts=1"), std::string::npos) << dyn_fact->detail;
    EXPECT_NE(dyn_fact->detail.find("metadata=supertrait_vptr_metadata_v1"), std::string::npos)
        << dyn_fact->detail;
    EXPECT_NE(dyn_fact->detail.find("first_upcast=&dyn Child->&dyn Parent"), std::string::npos)
        << dyn_fact->detail;

    const base::usize widen_offset = source.find("widen(child");
    ASSERT_NE(widen_offset, std::string_view::npos);
    const std::optional<tooling::IdeHoverInfo> widen_hover = tooling::hover_at_offset(snapshot, widen_offset);
    ASSERT_TRUE(widen_hover.has_value());
    EXPECT_NE(widen_hover->label.find("dyn_abi=abi=borrowed_view_v1"), std::string::npos)
        << widen_hover->label;
    EXPECT_NE(widen_hover->label.find("/metadata=supertrait_vptr_metadata_v1"), std::string::npos)
        << widen_hover->label;
    EXPECT_NE(widen_hover->label.find("/upcasts=1"), std::string::npos) << widen_hover->label;
    EXPECT_NE(widen_hover->label.find("/upcast=&dyn Child->&dyn Parent"), std::string::npos)
        << widen_hover->label;
    EXPECT_NE(widen_hover->label.find("/upcast_borrow=shared"), std::string::npos)
        << widen_hover->label;
}

TEST(CoreUnit, IdeToolingProjectsDynTraitCompositionRuntimeFactsAndHover)
{
    constexpr std::string_view source =
        "module ide.dyn_trait_composition_facts;\n"
        "trait Draw {\n"
        "  fn draw(self: &Self) -> i32;\n"
        "}\n"
        "trait Debug {\n"
        "  fn debug(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Draw for File {\n"
        "  fn draw(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "impl Debug for File {\n"
        "  fn debug(self: &File) -> i32 { return self.value + 1; }\n"
        "}\n"
        "fn score(combo: &dyn (Debug + Draw)) -> i32 {\n"
        "  let draw: &dyn Draw = combo;\n"
        "  return draw.draw();\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 5 };\n"
        "  return score(&file);\n"
        "}\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));
    ASSERT_TRUE(snapshot.checked_semantics);
    EXPECT_FALSE(snapshot.has_errors);
    EXPECT_FALSE(snapshot.dyn_abi_facts.empty());
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::lower_function_ir));
    EXPECT_TRUE(has_dependency_kind(snapshot, query::QueryKind::lower_function_ir,
        query::QueryKind::type_check_body));

    const tooling::IdeSemanticFact* const score_fact = find_semantic_fact(
        snapshot, tooling::IdeSemanticFactKind::dyn_abi_facts, query::QueryKind::lower_function_ir, "score");
    ASSERT_NE(score_fact, nullptr);
    EXPECT_NE(score_fact->detail.find("dyn_abi_facts objects="), std::string::npos)
        << score_fact->detail;
    EXPECT_NE(score_fact->detail.find("principal_sets=0"), std::string::npos) << score_fact->detail;
    EXPECT_NE(score_fact->detail.find("composition_projections=1"), std::string::npos)
        << score_fact->detail;
    EXPECT_NE(score_fact->detail.find("metadata=principal_set_metadata_v1"), std::string::npos)
        << score_fact->detail;
    EXPECT_NE(score_fact->detail.find("first_composition_projection=&dyn ("), std::string::npos)
        << score_fact->detail;

    const tooling::IdeSemanticFact* const main_fact = find_semantic_fact(
        snapshot, tooling::IdeSemanticFactKind::dyn_abi_facts, query::QueryKind::lower_function_ir, "main");
    ASSERT_NE(main_fact, nullptr);
    EXPECT_NE(main_fact->detail.find("principal_sets=1"), std::string::npos) << main_fact->detail;
    EXPECT_NE(main_fact->detail.find("composition_projections=0"), std::string::npos)
        << main_fact->detail;
    EXPECT_NE(main_fact->detail.find("metadata=principal_set_metadata_v1"), std::string::npos)
        << main_fact->detail;

    const base::usize score_offset = source.find("score(combo");
    ASSERT_NE(score_offset, std::string_view::npos);
    const std::optional<tooling::IdeHoverInfo> score_hover = tooling::hover_at_offset(snapshot, score_offset);
    ASSERT_TRUE(score_hover.has_value());
    EXPECT_NE(score_hover->label.find("dyn_abi=abi=borrowed_view_v1"), std::string::npos)
        << score_hover->label;
    EXPECT_NE(score_hover->label.find("/metadata=principal_set_metadata_v1"), std::string::npos)
        << score_hover->label;
    EXPECT_NE(score_hover->label.find("/principal_sets=0"), std::string::npos) << score_hover->label;
    EXPECT_NE(score_hover->label.find("/composition_projections=1"), std::string::npos)
        << score_hover->label;
    EXPECT_NE(score_hover->label.find("/composition_projection=&dyn ("), std::string::npos)
        << score_hover->label;
    EXPECT_NE(score_hover->label.find("/composition_borrow=shared"), std::string::npos)
        << score_hover->label;
    EXPECT_NE(score_hover->label.find("/composition_metadata=principal_set_metadata_v1"), std::string::npos)
        << score_hover->label;

    const base::usize draw_offset = source.find("draw();");
    ASSERT_NE(draw_offset, std::string_view::npos);
    const std::optional<tooling::IdeHoverInfo> draw_hover = tooling::hover_at_offset(snapshot, draw_offset);
    ASSERT_TRUE(draw_hover.has_value());
    EXPECT_NE(draw_hover->label.find("identifier `draw` -> trait_method"), std::string::npos)
        << draw_hover->label;
    EXPECT_NE(draw_hover->label.find("dyn_dispatch=dispatch=vtable_slot/slot=0"), std::string::npos)
        << draw_hover->label;
    EXPECT_NE(draw_hover->label.find("/metadata=borrowed_methods_only_v1"), std::string::npos)
        << draw_hover->label;
}

TEST(CoreUnit, IdeToolingProjectsDeclaredUnknownBorrowBoundaryFacts)
{
    constexpr std::string_view source = "module ide.unknown_borrow_facts;\n"
                                        "extern c {\n"
                                        "  @borrow(return = [unknown])\n"
                                        "  fn ext(value: &i32) -> &i32;\n"
                                        "}\n"
                                        "@borrow(return = [unknown])\n"
                                        "fn wrap(value: &i32) -> &i32 {\n"
                                        "  return ext(value);\n"
                                        "}\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));
    ASSERT_TRUE(snapshot.checked_semantics);
    EXPECT_FALSE(snapshot.has_errors);

    const tooling::IdeSemanticFact* const summary_fact = find_semantic_fact(
        snapshot, tooling::IdeSemanticFactKind::borrow_summary, query::QueryKind::type_check_body, "wrap");
    ASSERT_NE(summary_fact, nullptr);
    EXPECT_NE(summary_fact->detail.find("unknown=true"), std::string::npos) << summary_fact->detail;
    EXPECT_NE(summary_fact->detail.find("local_escape=false"), std::string::npos) << summary_fact->detail;

    const tooling::IdeSemanticFact* const contract_fact = find_semantic_fact(
        snapshot, tooling::IdeSemanticFactKind::borrow_contract, query::QueryKind::type_check_body, "wrap");
    ASSERT_NE(contract_fact, nullptr);
    EXPECT_NE(contract_fact->detail.find("source=declared"), std::string::npos) << contract_fact->detail;
    EXPECT_NE(contract_fact->detail.find("unknown=true"), std::string::npos) << contract_fact->detail;
    EXPECT_NE(contract_fact->detail.find("mismatch=false"), std::string::npos) << contract_fact->detail;

    const base::usize wrap_offset = source.find("wrap(value");
    ASSERT_NE(wrap_offset, std::string_view::npos);
    const std::optional<tooling::IdeHoverInfo> hover = tooling::hover_at_offset(snapshot, wrap_offset);
    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->label.find("borrow_summary=deps="), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("unknown=true"), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("borrow_contract=declared/selectors=1/unknown=true/mismatch=false"),
        std::string::npos)
        << hover->label;
}

TEST(CoreUnit, IdeToolingHoverReflectsCheckedBorrowLifetimeAndDropFactDetails)
{
    const tooling::IdeSnapshot base_snapshot = tooling::build_ide_snapshot(request_for(IDE_TOOLING_SOURCE));
    tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(
        "module ide.hover_facts;\n"
        "fn id_ref(value: &i32) -> &i32 {\n"
        "  return value;\n"
        "}\n"
        "fn main() -> void {}\n"));
    ASSERT_TRUE(snapshot.checked_semantics);
    EXPECT_FALSE(snapshot.has_errors);
    EXPECT_TRUE(base_snapshot.checked_semantics);

    const std::optional<sema::FunctionLookupKey> key = find_function_key(snapshot.checked, "id_ref");
    ASSERT_TRUE(key.has_value());
    sema::FunctionBorrowSummary& summary = snapshot.checked.borrow_summaries[*key];
    summary.function = *key;
    summary.has_unknown_return_origin = true;
    summary.has_local_return_escape = true;
    summary.storage_escapes.push_back(sema::FunctionBorrowStorageEscape{});

    sema::FunctionBorrowContract& contract = snapshot.checked.borrow_contracts[*key];
    contract.function = *key;
    contract.source = sema::FunctionBorrowContractSource::declared;
    contract.return_selectors.push_back(sema::BorrowContractSelector{
        .kind = sema::BorrowContractSelectorKind::unknown,
        .param_index = sema::SEMA_BORROW_SUMMARY_INVALID_INDEX,
    });
    contract.unknown_return_allowed = true;
    contract.has_contract_mismatch = true;

    sema::FunctionLifetimeFacts& lifetime = snapshot.checked.lifetime_facts[*key];
    lifetime.function = *key;
    sema::LifetimeRegion local_region;
    local_region.kind = sema::LifetimeRegionKind::local;
    lifetime.regions.push_back(local_region);
    lifetime.return_regions.push_back(sema::LifetimeReturnRegion{.region = 0U});
    lifetime.violations.push_back(sema::LifetimeViolation{
        .kind = sema::LifetimeViolationKind::local_escape,
        .region = 0U,
        .diagnostic_emitted = true,
    });
    lifetime.violations.push_back(sema::LifetimeViolation{
        .kind = sema::LifetimeViolationKind::unknown_escape,
        .region = 0U,
        .diagnostic_emitted = true,
    });

    sema::DropCheckFact drop_fact;
    drop_fact.required_outlives.push_back(sema::DropCheckRequiredOutlives{
        .reason = sema::LifetimeConstraintReason::dropck,
    });
    sema::FunctionDropCheckFacts& dropck = snapshot.checked.dropck_facts[*key];
    dropck.function = *key;
    dropck.facts.push_back(std::move(drop_fact));
    dropck.actions.push_back(sema::DropActionFact{});
    dropck.violations.push_back(sema::DropCheckViolation{
        .kind = sema::DropCheckViolationKind::generic_type_outlives,
        .diagnostic_emitted = true,
    });

    sema::FunctionMoveRejectionFacts& move_rejections = snapshot.checked.move_rejection_facts[*key];
    move_rejections.function = *key;
    move_rejections.rejections.push_back(sema::MoveRejectionFact{
        .kind = sema::MoveRejectionKind::try_payload,
        .tracked_type = snapshot.checked.types.builtin(sema::BuiltinType::i32),
        .resource_fingerprint = query::stable_fingerprint("ide-hover-move-rejection"),
        .diagnostic_emitted = true,
    });
    move_rejections.fingerprint = sema::function_move_rejection_facts_fingerprint(move_rejections);

    constexpr std::string_view source = "module ide.hover_facts;\n"
                                        "fn id_ref(value: &i32) -> &i32 {\n"
                                        "  return value;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    const base::usize id_ref_offset = source.find("id_ref");
    ASSERT_NE(id_ref_offset, std::string_view::npos);
    const std::optional<tooling::IdeHoverInfo> hover = tooling::hover_at_offset(snapshot, id_ref_offset);
    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->label.find("storage_escapes=1"), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("unknown=true"), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("local_escape=true"), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("borrow_contract=declared/selectors="), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("/unknown=true/mismatch=true"), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("lifetime=regions="), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("/violations=2/local_escapes=1/unknown_escapes=1"), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("dropck=facts=1/actions=1/required_outlives=1/violations=1"), std::string::npos)
        << hover->label;
    EXPECT_NE(hover->label.find("move_rejections=count=1/first=try_payload"), std::string::npos) << hover->label;
}

TEST(CoreUnit, IdeToolingReferencesAstFallbackSymbolsWithoutCheckedKeys)
{
    tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(IDE_TOOLING_SOURCE));
    ASSERT_TRUE(snapshot.parsed);
    ASSERT_TRUE(snapshot.checked_semantics);

    snapshot.checked_semantics = false;
    snapshot.checked = sema::CheckedModule{};
    snapshot.query.source_stage = {};

    const base::usize call_offset = IDE_TOOLING_SOURCE.find("add(1");
    ASSERT_NE(call_offset, std::string_view::npos);
    const std::optional<tooling::IdeDefinition> definition = tooling::definition_at_offset(snapshot, call_offset);
    ASSERT_TRUE(definition.has_value());
    EXPECT_TRUE(definition->valid);
    EXPECT_EQ(definition->name, "add");
    EXPECT_EQ(definition->kind, "function");
    EXPECT_FALSE(query::is_valid(definition->key));

    const std::vector<tooling::IdeReference> references = tooling::references_at_offset(snapshot, call_offset);
    ASSERT_GE(references.size(), 2U);
    EXPECT_TRUE(std::ranges::any_of(references, [](const tooling::IdeReference& reference) {
        return reference.name == "add" && reference.is_definition;
    }));
    EXPECT_TRUE(std::ranges::any_of(references, [](const tooling::IdeReference& reference) {
        return reference.name == "add" && !reference.is_definition;
    }));
}

TEST(CoreUnit, IdeToolingRecordsPrimaryModulePartDeclarations)
{
    constexpr std::string_view SOURCE = "module ide.parts;\n"
                                        "part worker;\n"
                                        "fn main() -> i32 { return 0; }\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(SOURCE));

    EXPECT_TRUE(snapshot.parsed);
    EXPECT_EQ(snapshot.ast.part_declarations.size(), 1U);
    EXPECT_EQ(snapshot.ast.part_declarations.front().name, "worker");
    expect_primary_source_part(snapshot.source_part, "ide.parts");
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::module_graph));
    EXPECT_TRUE(has_record_kind(snapshot, query::QueryKind::module_part));
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
    EXPECT_NE(parameter_hover->label.find("resource=Copy/Discard/Trivial/OwnedValue"), std::string::npos);
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

TEST(CoreUnit, IdeToolingHoverExposesMoveOnlyNeedsDropResources)
{
    constexpr std::string_view SOURCE = "module ide.resources;\n"
                                        "fn hold[T](value: T) {\n"
                                        "}\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(SOURCE));
    ASSERT_TRUE(snapshot.checked_semantics);

    const base::usize value_offset = SOURCE.find("value: T");
    ASSERT_NE(value_offset, std::string_view::npos);
    const std::optional<tooling::IdeHoverInfo> hover = tooling::hover_at_offset(snapshot, value_offset);
    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->label.find("identifier `value` -> parameter"), std::string::npos);
    EXPECT_NE(hover->label.find("resource=MoveOnly/Discard/NeedsDrop/OwnedValue"), std::string::npos) << hover->label;
}

TEST(CoreUnit, IdeToolingHoverMarksCustomDestructorResources)
{
    constexpr std::string_view SOURCE = "module ide.drop_resource;\n"
                                        "struct File { fd: i32; }\n"
                                        "impl Drop for File {\n"
                                        "  fn drop(self: deinit File) -> void {}\n"
                                        "}\n"
                                        "fn use_file(value: File) -> void {}\n"
                                        "fn main() -> void {}\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(SOURCE));
    ASSERT_TRUE(snapshot.checked_semantics);
    EXPECT_FALSE(snapshot.has_errors);

    const base::usize value_offset = SOURCE.find("value: File");
    ASSERT_NE(value_offset, std::string_view::npos);
    const std::optional<tooling::IdeHoverInfo> hover = tooling::hover_at_offset(snapshot, value_offset);
    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->label.find("identifier `value` -> parameter"), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("resource=MoveOnly/Discard/NeedsDrop/OwnedValue"), std::string::npos)
        << hover->label;
    EXPECT_NE(hover->label.find("destructor=custom"), std::string::npos) << hover->label;
}

TEST(CoreUnit, IdeToolingServesCompletionSemanticTokensAndInlayHints)
{
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(IDE_TOOLING_SOURCE));
    ASSERT_TRUE(snapshot.checked_semantics);

    const base::usize return_offset = IDE_TOOLING_SOURCE.find("return value");
    ASSERT_NE(return_offset, std::string_view::npos);
    const std::vector<tooling::IdeCompletionItem> keyword_completions =
        tooling::completion_items_at_offset(snapshot, return_offset + std::string_view{"retu"}.size());
    EXPECT_TRUE(std::ranges::any_of(keyword_completions, [](const tooling::IdeCompletionItem& item) {
        return item.label == "return" && item.kind == "keyword"
            && item.context == tooling::IdeCompletionContextKind::expression;
    }));

    const base::usize value_offset = IDE_TOOLING_SOURCE.find("value;");
    ASSERT_NE(value_offset, std::string_view::npos);
    const std::vector<tooling::IdeCompletionItem> local_completions =
        tooling::completion_items_at_offset(snapshot, value_offset + std::string_view{"va"}.size());
    EXPECT_TRUE(std::ranges::any_of(local_completions, [](const tooling::IdeCompletionItem& item) {
        return item.label == "value" && item.kind == "local" && item.local && item.checked;
    }));

    const base::usize call_offset = IDE_TOOLING_SOURCE.find("add(1");
    ASSERT_NE(call_offset, std::string_view::npos);
    const std::vector<tooling::IdeCompletionItem> function_completions =
        tooling::completion_items_at_offset(snapshot, call_offset + std::string_view{"ad"}.size());
    EXPECT_TRUE(std::ranges::any_of(function_completions, [](const tooling::IdeCompletionItem& item) {
        return item.label == "add" && item.kind == "function" && item.checked;
    }));

    const base::usize top_level_offset = IDE_TOOLING_SOURCE.find("fn add");
    ASSERT_NE(top_level_offset, std::string_view::npos);
    const std::vector<tooling::IdeCompletionItem> item_completions =
        tooling::completion_items_at_offset(snapshot, top_level_offset + std::string_view{"fn"}.size());
    EXPECT_TRUE(std::ranges::any_of(item_completions, [](const tooling::IdeCompletionItem& item) {
        return item.label == "fn" && item.kind == "keyword" && item.context == tooling::IdeCompletionContextKind::item;
    }));

    const base::usize before_local_offset = IDE_TOOLING_SOURCE.find("let value");
    ASSERT_NE(before_local_offset, std::string_view::npos);
    const std::vector<tooling::IdeCompletionItem> before_local_completions =
        tooling::completion_items_at_offset(snapshot, before_local_offset);
    EXPECT_TRUE(std::ranges::none_of(before_local_completions, [](const tooling::IdeCompletionItem& item) {
        return item.label == "value" && item.kind == "local";
    }));

    constexpr std::string_view member_source = "module ide.member;\n"
                                               "struct Point { count: i32; }\n"
                                               "fn main() -> i32 {\n"
                                               "  let point = Point { count: 1 };\n"
                                               "  return point.count;\n"
                                               "}\n";
    const tooling::IdeSnapshot member_snapshot = tooling::build_ide_snapshot(request_for(member_source));
    const base::usize member_offset = member_source.find("point.count");
    ASSERT_NE(member_offset, std::string_view::npos);
    const std::vector<tooling::IdeCompletionItem> member_completions =
        tooling::completion_items_at_offset(member_snapshot, member_offset + std::string_view{"point."}.size());
    EXPECT_TRUE(std::ranges::any_of(member_completions, [](const tooling::IdeCompletionItem& item) {
        return item.label == "count" && item.kind == "struct_field"
            && item.context == tooling::IdeCompletionContextKind::member;
    }));
    const std::vector<tooling::IdeSemanticToken> member_tokens = tooling::semantic_tokens(member_snapshot);
    EXPECT_TRUE(std::ranges::any_of(member_tokens, [](const tooling::IdeSemanticToken& token) {
        return token.text == "count" && token.token_type == "property" && token.checked;
    }));
    const auto expect_mutated_field_kind_is_still_tokenizable = [&](const query::StableSymbolKind kind) {
        tooling::IdeSnapshot mutated_snapshot = member_snapshot;
        ASSERT_FALSE(mutated_snapshot.checked.structs.empty());
        sema::StructInfo& mutated_struct = mutated_snapshot.checked.structs.begin()->second;
        ASSERT_FALSE(mutated_struct.fields.empty());
        mutated_struct.fields.front().stable_key.kind = kind;
        const std::vector<tooling::IdeSemanticToken> mutated_tokens = tooling::semantic_tokens(mutated_snapshot);
        EXPECT_TRUE(std::ranges::any_of(mutated_tokens, [](const tooling::IdeSemanticToken& token) {
            return token.text == "count" && token.token_type == "property";
        }));
    };
    expect_mutated_field_kind_is_still_tokenizable(query::StableSymbolKind::method);
    expect_mutated_field_kind_is_still_tokenizable(query::StableSymbolKind::type);
    expect_mutated_field_kind_is_still_tokenizable(query::StableSymbolKind::invalid);

    constexpr std::string_view method_source = "module ide.method_tokens;\n"
                                               "struct Counter { value: i32; }\n"
                                               "impl Counter {\n"
                                               "  fn read(self: &Counter) -> i32 { return self.value; }\n"
                                               "}\n"
                                               "fn main() -> i32 {\n"
                                               "  let counter = Counter { value: 1 };\n"
                                               "  return counter.read();\n"
                                               "}\n";
    const tooling::IdeSnapshot method_snapshot = tooling::build_ide_snapshot(request_for(method_source));
    const std::vector<tooling::IdeSemanticToken> method_tokens = tooling::semantic_tokens(method_snapshot);
    EXPECT_TRUE(std::ranges::any_of(method_tokens, [](const tooling::IdeSemanticToken& token) {
        return token.text == "read" && token.token_type == "method" && token.checked;
    }));

    constexpr std::string_view module_path_source = "module ide.module_path;\n"
                                                    "import ide.member;\n"
                                                    "fn main() -> i32 { return 0; }\n";
    const tooling::IdeSnapshot module_path_snapshot = tooling::build_ide_snapshot(request_for(module_path_source));
    const base::usize import_offset = module_path_source.find("import ");
    ASSERT_NE(import_offset, std::string_view::npos);
    const base::usize module_path_offset = import_offset + std::string_view{"import "}.size();
    const std::vector<tooling::IdeCompletionItem> module_path_completions =
        tooling::completion_items_at_offset(module_path_snapshot, module_path_offset);
    EXPECT_TRUE(std::ranges::any_of(module_path_completions, [](const tooling::IdeCompletionItem& item) {
        return item.label == "import" && item.context == tooling::IdeCompletionContextKind::module_path;
    }));

    const std::vector<tooling::IdeSemanticToken> tokens = tooling::semantic_tokens(snapshot);
    EXPECT_TRUE(std::ranges::any_of(tokens, [](const tooling::IdeSemanticToken& token) {
        return token.text == "add" && token.token_type == "function" && token.checked;
    }));
    EXPECT_TRUE(std::ranges::any_of(tokens, [](const tooling::IdeSemanticToken& token) {
        return token.text == "return" && token.token_type == "keyword";
    }));
    EXPECT_TRUE(std::ranges::any_of(tokens, [](const tooling::IdeSemanticToken& token) {
        return token.text == "value" && token.token_type == "variable"
            && std::ranges::find(token.modifiers, "declaration") != token.modifiers.end();
    }));

    constexpr std::string_view readonly_source = "module ide.readonly;\n"
                                                 "const answer: i32 = 1;\n"
                                                 "enum Mode { fast }\n"
                                                 "fn main() -> i32 { return answer; }\n";
    const tooling::IdeSnapshot readonly_snapshot = tooling::build_ide_snapshot(request_for(readonly_source));
    const std::vector<tooling::IdeSemanticToken> readonly_tokens = tooling::semantic_tokens(readonly_snapshot);
    EXPECT_TRUE(std::ranges::any_of(readonly_tokens, [](const tooling::IdeSemanticToken& token) {
        return (token.text == "answer" || token.text == "fast")
            && std::ranges::find(token.modifiers, "readonly") != token.modifiers.end();
    }));

    constexpr std::string_view lexical_token_source = "module ide.lexical_tokens;\n"
                                                      "// token comment\n"
                                                      "fn main() -> str { return \"ok\"; }\n";
    const tooling::IdeSnapshot lexical_token_snapshot = tooling::build_ide_snapshot(request_for(lexical_token_source));
    const std::vector<tooling::IdeSemanticToken> lexical_tokens = tooling::semantic_tokens(lexical_token_snapshot);
    EXPECT_TRUE(std::ranges::any_of(lexical_tokens, [](const tooling::IdeSemanticToken& token) {
        return token.text.find("token comment") != std::string::npos && token.token_type == "comment";
    }));
    EXPECT_TRUE(std::ranges::any_of(lexical_tokens, [](const tooling::IdeSemanticToken& token) {
        return token.text == "\"ok\"" && token.token_type == "string";
    }));

    const std::vector<tooling::IdeInlayHint> hints = tooling::inlay_hints(snapshot);
    EXPECT_TRUE(std::ranges::any_of(hints, [](const tooling::IdeInlayHint& hint) {
        return hint.label == ": i32" && hint.kind == "type" && hint.checked;
    }));

    constexpr std::string_view multiple_hint_source = "module ide.hints;\n"
                                                      "fn main() -> i32 {\n"
                                                      "  let first = 1;\n"
                                                      "  let second = 2;\n"
                                                      "  return first + second;\n"
                                                      "}\n";
    const tooling::IdeSnapshot multiple_hint_snapshot = tooling::build_ide_snapshot(request_for(multiple_hint_source));
    const std::vector<tooling::IdeInlayHint> multiple_hints = tooling::inlay_hints(multiple_hint_snapshot);
    ASSERT_GE(multiple_hints.size(), 2U);
    EXPECT_LE(multiple_hints.front().position.begin, multiple_hints.back().position.begin);

    constexpr std::string_view explicit_type_source = "module ide.snapshot;\n"
                                                      "fn add(a: i32, b: i32) -> i32 {\n"
                                                      "  return a + b;\n"
                                                      "}\n"
                                                      "fn main() -> i32 {\n"
                                                      "  let value: i32 = add(1, 2);\n"
                                                      "  return value;\n"
                                                      "}\n";
    const tooling::IdeSnapshot explicit_snapshot = tooling::build_ide_snapshot(request_for(explicit_type_source));
    const std::vector<tooling::IdeInlayHint> explicit_hints = tooling::inlay_hints(explicit_snapshot);
    EXPECT_TRUE(std::ranges::none_of(explicit_hints, [](const tooling::IdeInlayHint& hint) {
        return hint.label == ": i32";
    }));
}

TEST(CoreUnit, IdeToolingClassifiesLexicalSemanticTokenSurface)
{
    constexpr std::string_view source = "module ide.token_surface;\n"
                                        "// line comment\n"
                                        "/* block comment */\n"
                                        "void bool i8 u8 i16 u16 i32 u32 i64 u64 isize usize f32 f64 str char\n"
                                        "123 1.5 \"text\" c\"ffi\" r\"raw\" b\"bytes\" b'a' 'z'\n"
                                        "() {} [] , . ; : :: ... + - * / % ^ << >> | & ~ ! = == != < <= > >= ? @\n";
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(source));
    ASSERT_TRUE(snapshot.lexed);

    const std::vector<tooling::IdeSemanticToken> tokens = tooling::semantic_tokens(snapshot);
    struct ExpectedToken {
        std::string_view text;
        std::string_view token_type;
    };
    constexpr std::array<ExpectedToken, 31> EXPECTED_TOKENS{{
        {"// line comment", "comment"},
        {"/* block comment */", "comment"},
        {"module", "keyword"},
        {"void", "type"},
        {"bool", "type"},
        {"i8", "type"},
        {"u8", "type"},
        {"i16", "type"},
        {"u16", "type"},
        {"i32", "type"},
        {"u32", "type"},
        {"i64", "type"},
        {"u64", "type"},
        {"isize", "type"},
        {"usize", "type"},
        {"f32", "type"},
        {"f64", "type"},
        {"str", "type"},
        {"char", "type"},
        {"123", "number"},
        {"1.5", "number"},
        {"\"text\"", "string"},
        {"c\"ffi\"", "string"},
        {"r\"raw\"", "string"},
        {"b\"bytes\"", "string"},
        {"b'a'", "string"},
        {"'z'", "string"},
        {"(", "punctuation"},
        {"::", "punctuation"},
        {"...", "punctuation"},
        {"+", "operator"},
    }};

    for (const ExpectedToken& expected : EXPECTED_TOKENS) {
        SCOPED_TRACE(std::string(expected.text));
        EXPECT_TRUE(has_semantic_token(tokens, expected.text, expected.token_type));
    }
}

TEST(CoreUnit, IdeToolingExposesM4TraitFactsForWp7)
{
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(IDE_TOOLING_TRAIT_SOURCE));
    ASSERT_TRUE(snapshot.parsed);
    ASSERT_TRUE(snapshot.checked_semantics);
    EXPECT_TRUE(has_semantic_fact_kind(
        snapshot, tooling::IdeSemanticFactKind::item_signature, query::QueryKind::item_signature, "Source"));

    const base::usize trait_bound_offset = IDE_TOOLING_TRAIT_SOURCE.find("where T: Source");
    ASSERT_NE(trait_bound_offset, std::string_view::npos);
    const std::vector<tooling::IdeCompletionItem> trait_completions =
        tooling::completion_items_at_offset(snapshot, trait_bound_offset + std::string_view{"where T: So"}.size());
    EXPECT_TRUE(std::ranges::any_of(trait_completions, [](const tooling::IdeCompletionItem& item) {
        return item.label == "Source" && item.kind == "trait" && item.checked
            && item.context == tooling::IdeCompletionContextKind::trait_bound
            && item.definition.kind == query::DefKind::trait_;
    }));
    EXPECT_TRUE(std::ranges::none_of(trait_completions, [](const tooling::IdeCompletionItem& item) {
        return item.kind == "struct" || item.kind == "function";
    }));

    const base::usize trait_use_offset = IDE_TOOLING_TRAIT_SOURCE.find("Source[Item");
    ASSERT_NE(trait_use_offset, std::string_view::npos);
    const std::optional<tooling::IdeDefinition> trait_definition =
        tooling::definition_at_offset(snapshot, trait_use_offset);
    ASSERT_TRUE(trait_definition.has_value());
    EXPECT_EQ(trait_definition->name, "Source");
    EXPECT_EQ(trait_definition->kind, "trait");
    EXPECT_EQ(trait_definition->key.kind, query::DefKind::trait_);

    const base::usize requirement_method_offset = IDE_TOOLING_TRAIT_SOURCE.find("fn get(self: &Self)");
    ASSERT_NE(requirement_method_offset, std::string_view::npos);
    const std::optional<tooling::IdeDefinition> requirement_method =
        tooling::definition_at_offset(snapshot, requirement_method_offset + std::string_view{"fn "}.size());
    ASSERT_TRUE(requirement_method.has_value());
    EXPECT_EQ(requirement_method->name, "get");
    EXPECT_EQ(requirement_method->kind, "trait_method");
    EXPECT_EQ(requirement_method->key.kind, query::DefKind::trait_method);
    EXPECT_TRUE(query::is_valid(requirement_method->member));

    const base::usize impl_method_offset = IDE_TOOLING_TRAIT_SOURCE.rfind("fn get(self: &Bytes)");
    ASSERT_NE(impl_method_offset, std::string_view::npos);
    const std::optional<tooling::IdeDefinition> impl_method =
        tooling::definition_at_offset(snapshot, impl_method_offset + std::string_view{"fn "}.size());
    ASSERT_TRUE(impl_method.has_value());
    EXPECT_EQ(impl_method->name, "get");
    EXPECT_EQ(impl_method->kind, "impl_method");
    EXPECT_EQ(impl_method->key.kind, query::DefKind::method);
    EXPECT_TRUE(query::is_valid(impl_method->member));

    const base::usize impl_associated_offset = IDE_TOOLING_TRAIT_SOURCE.find("Item = i32");
    ASSERT_NE(impl_associated_offset, std::string_view::npos);
    const std::optional<tooling::IdeDefinition> associated_type =
        tooling::definition_at_offset(snapshot, impl_associated_offset);
    ASSERT_TRUE(associated_type.has_value());
    EXPECT_EQ(associated_type->name, "Item");
    EXPECT_EQ(associated_type->kind, "associated_type");
    EXPECT_EQ(associated_type->key.kind, query::DefKind::associated_type);
    EXPECT_TRUE(query::is_valid(associated_type->member));

    const std::optional<tooling::IdeHoverInfo> associated_hover =
        tooling::hover_at_offset(snapshot, impl_associated_offset);
    ASSERT_TRUE(associated_hover.has_value());
    EXPECT_NE(associated_hover->label.find("identifier `Item` -> associated_type"), std::string::npos);
    ASSERT_TRUE(associated_hover->definition.has_value());
    EXPECT_TRUE(query::is_valid(associated_hover->definition->member));

    const std::vector<tooling::IdeReference> associated_references =
        tooling::references_at_offset(snapshot, impl_associated_offset);
    EXPECT_GE(associated_references.size(), 3U);
    EXPECT_TRUE(std::ranges::any_of(associated_references, [](const tooling::IdeReference& reference) {
        return reference.is_definition;
    }));

    const std::vector<tooling::IdeSemanticToken> tokens = tooling::semantic_tokens(snapshot);
    EXPECT_TRUE(std::ranges::any_of(tokens, [](const tooling::IdeSemanticToken& token) {
        return token.text == "Source" && token.token_type == "interface" && token.checked;
    }));
    EXPECT_TRUE(std::ranges::any_of(tokens, [](const tooling::IdeSemanticToken& token) {
        return token.text == "Item" && token.token_type == "type" && token.checked && query::is_valid(token.member);
    }));
    EXPECT_TRUE(std::ranges::any_of(tokens, [](const tooling::IdeSemanticToken& token) {
        return token.text == "get" && token.token_type == "method" && token.checked && query::is_valid(token.member);
    }));
}

TEST(CoreUnit, IdeToolingResolvesM5TraitDefaultMethodOrigins)
{
    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request_for(IDE_TOOLING_TRAIT_DEFAULT_SOURCE));
    ASSERT_TRUE(snapshot.parsed);
    ASSERT_TRUE(snapshot.checked_semantics);
    EXPECT_TRUE(has_semantic_fact_kind(snapshot, tooling::IdeSemanticFactKind::function_body_syntax,
        query::QueryKind::function_body_syntax, "is_empty"));
    EXPECT_TRUE(has_semantic_fact_kind(
        snapshot, tooling::IdeSemanticFactKind::type_check_body, query::QueryKind::type_check_body, "is_empty"));

    const base::usize default_body_offset = IDE_TOOLING_TRAIT_DEFAULT_SOURCE.find("self.read");
    ASSERT_NE(default_body_offset, std::string_view::npos);
    const std::optional<tooling::IdeAstNodeInfo> default_body =
        tooling::ast_node_at_offset(snapshot, default_body_offset);
    ASSERT_TRUE(default_body.has_value());
    EXPECT_TRUE(default_body->valid);
    EXPECT_EQ(default_body->kind, tooling::IdeAstNodeKind::function_body);
    EXPECT_EQ(default_body->name, "is_empty");
    EXPECT_EQ(default_body->detail, "function_body_syntax");
    EXPECT_EQ(default_body->body.slot, query::BodySlotKind::trait_default_method);

    const base::usize inherited_call = IDE_TOOLING_TRAIT_DEFAULT_SOURCE.find("file.is_empty");
    ASSERT_NE(inherited_call, std::string_view::npos);
    const base::usize inherited_method = inherited_call + std::string_view{"file."}.size();
    const std::optional<tooling::IdeDefinition> inherited_definition =
        tooling::definition_at_offset(snapshot, inherited_method);
    ASSERT_TRUE(inherited_definition.has_value());
    EXPECT_EQ(inherited_definition->name, "is_empty");
    EXPECT_EQ(inherited_definition->kind, "trait_method");
    EXPECT_EQ(inherited_definition->key.kind, query::DefKind::trait_method);
    EXPECT_TRUE(query::is_valid(inherited_definition->member));

    const std::optional<tooling::IdeHoverInfo> inherited_hover = tooling::hover_at_offset(snapshot, inherited_method);
    ASSERT_TRUE(inherited_hover.has_value());
    EXPECT_NE(inherited_hover->label.find("identifier `is_empty` -> trait_method"), std::string::npos);
    EXPECT_NE(inherited_hover->label.find("Reader.is_empty(&Self) -> bool default"), std::string::npos);
    ASSERT_TRUE(inherited_hover->definition.has_value());
    EXPECT_EQ(inherited_hover->definition->kind, "trait_method");

    tooling::IdeSnapshot ordinal_fallback_snapshot = snapshot;
    bool mutated_default_binding = false;
    for (sema::TraitMethodCallBinding& binding : ordinal_fallback_snapshot.checked.trait_method_calls) {
        if (binding.method_name == "is_empty" && binding.dispatch == sema::TraitMethodDispatchKind::trait_default) {
            binding.requirement_ordinal += IDE_TOOLING_REQUIREMENT_ORDINAL_FALLBACK_OFFSET;
            mutated_default_binding = true;
            break;
        }
    }
    ASSERT_TRUE(mutated_default_binding);
    const std::optional<tooling::IdeDefinition> ordinal_fallback_definition =
        tooling::definition_at_offset(ordinal_fallback_snapshot, inherited_method);
    ASSERT_TRUE(ordinal_fallback_definition.has_value());
    EXPECT_EQ(ordinal_fallback_definition->kind, "trait_method");

    const base::usize override_call = IDE_TOOLING_TRAIT_DEFAULT_SOURCE.find("buffer.is_empty");
    ASSERT_NE(override_call, std::string_view::npos);
    const base::usize override_method = override_call + std::string_view{"buffer."}.size();
    const std::optional<tooling::IdeDefinition> override_definition =
        tooling::definition_at_offset(snapshot, override_method);
    ASSERT_TRUE(override_definition.has_value());
    EXPECT_EQ(override_definition->name, "is_empty");
    EXPECT_EQ(override_definition->kind, "impl_method");
    EXPECT_EQ(override_definition->key.kind, query::DefKind::method);
    EXPECT_TRUE(query::is_valid(override_definition->member));

    const std::optional<tooling::IdeHoverInfo> override_hover = tooling::hover_at_offset(snapshot, override_method);
    ASSERT_TRUE(override_hover.has_value());
    EXPECT_NE(override_hover->label.find("identifier `is_empty` -> impl_method"), std::string::npos);
    ASSERT_TRUE(override_hover->definition.has_value());
    EXPECT_EQ(override_hover->definition->kind, "impl_method");

    ASSERT_FALSE(snapshot.checked.trait_default_method_instances.empty());
    tooling::IdeSnapshot synthetic_override_snapshot = snapshot;
    bool redirected_override_binding = false;
    for (sema::TraitMethodCallBinding& binding : synthetic_override_snapshot.checked.trait_method_calls) {
        if (binding.method_name == "is_empty" && binding.dispatch == sema::TraitMethodDispatchKind::impl_override) {
            binding.function_key = snapshot.checked.trait_default_method_instances.front().key;
            redirected_override_binding = true;
            break;
        }
    }
    ASSERT_TRUE(redirected_override_binding);
    const std::optional<tooling::IdeDefinition> synthetic_override_definition =
        tooling::definition_at_offset(synthetic_override_snapshot, override_method);
    ASSERT_TRUE(synthetic_override_definition.has_value());
    EXPECT_EQ(synthetic_override_definition->kind, "trait_method");
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
                                        "extern c { @name(\"native\") fn native(value: i32) -> i32; }\n"
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

    const std::vector<tooling::IdeSemanticToken> tokens = tooling::semantic_tokens(snapshot);
    EXPECT_TRUE(has_semantic_token(tokens, "Handle", "type"));

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
    const auto type_mismatch = std::ranges::find_if(snapshot.diagnostics, [](const tooling::IdeDiagnostic& diagnostic) {
        return diagnostic.category == base::DiagnosticCategory::type
            && diagnostic.code == base::DiagnosticCode::semantic_type_mismatch;
    });
    ASSERT_NE(type_mismatch, snapshot.diagnostics.end());
    ASSERT_GE(type_mismatch->children.size(), 2U);
    EXPECT_TRUE(std::ranges::any_of(type_mismatch->children, [](const base::DiagnosticChild& child) {
        return child.severity == base::Severity::note
            && child.message.find("expected type: i32") != std::string::npos;
    }));
    EXPECT_TRUE(std::ranges::any_of(type_mismatch->children, [](const base::DiagnosticChild& child) {
        return child.severity == base::Severity::note && child.message.find("actual type: bool") != std::string::npos;
    }));
    EXPECT_TRUE(
        has_diagnostic_owner_stage_profile(snapshot, base::DiagnosticCategory::type, IDE_TOOLING_STAGE_SEMA_ANALYZE));
}

TEST(CoreUnit, IdeToolingReportsTraitCandidateDiagnosticsForWp7)
{
    constexpr std::string_view equality_source = "module ide.trait_diag_equality;\n"
                                                 "trait Source { type Item; fn get(self: &Self) -> Self.Item; }\n"
                                                 "struct Bytes { value: bool; }\n"
                                                 "impl Source for Bytes {\n"
                                                 "  type Item = bool;\n"
                                                 "  fn get(self: &Bytes) -> bool { return self.value; }\n"
                                                 "}\n"
                                                 "fn read_i32[T](value: &T) -> i32 where T: Source[Item = i32] {\n"
                                                 "  return value.get();\n"
                                                 "}\n"
                                                 "fn main() -> i32 {\n"
                                                 "  let bytes = Bytes { value: true };\n"
                                                 "  return read_i32[Bytes](&bytes);\n"
                                                 "}\n";
    const tooling::IdeSnapshot equality_snapshot = tooling::build_ide_snapshot(request_for(equality_source));
    EXPECT_TRUE(equality_snapshot.has_errors);
    EXPECT_FALSE(equality_snapshot.checked_semantics);
    EXPECT_TRUE(std::ranges::any_of(equality_snapshot.diagnostics, [](const tooling::IdeDiagnostic& diagnostic) {
        return diagnostic.message.find("trait associated type equality is not satisfied") != std::string::npos;
    }));
    EXPECT_TRUE(std::ranges::any_of(equality_snapshot.diagnostics, [](const tooling::IdeDiagnostic& diagnostic) {
        return diagnostic.severity == base::Severity::note
            && diagnostic.message.find("candidate trait impl") != std::string::npos;
    }));
    EXPECT_TRUE(std::ranges::any_of(equality_snapshot.diagnostics, [](const tooling::IdeDiagnostic& diagnostic) {
        return diagnostic.severity == base::Severity::note
            && diagnostic.message.find("candidate associated type `Item` resolves to bool") != std::string::npos;
    }));

    constexpr std::string_view rejected_source = "module ide.trait_diag_rejected;\n"
                                                 "trait Source { type Item; fn get(self: &Self) -> Self.Item; }\n"
                                                 "struct Bytes { value: i32; }\n"
                                                 "struct Text { value: i32; }\n"
                                                 "impl Source for Text {\n"
                                                 "  type Item = i32;\n"
                                                 "  fn get(self: &Text) -> i32 { return self.value; }\n"
                                                 "}\n"
                                                 "fn read_i32[T](value: &T) -> i32 where T: Source[Item = i32] {\n"
                                                 "  return value.get();\n"
                                                 "}\n"
                                                 "fn main() -> i32 {\n"
                                                 "  let bytes = Bytes { value: 1 };\n"
                                                 "  return read_i32[Bytes](&bytes);\n"
                                                 "}\n";
    const tooling::IdeSnapshot rejected_snapshot = tooling::build_ide_snapshot(request_for(rejected_source));
    EXPECT_TRUE(rejected_snapshot.has_errors);
    EXPECT_TRUE(std::ranges::any_of(rejected_snapshot.diagnostics, [](const tooling::IdeDiagnostic& diagnostic) {
        return diagnostic.message.find("does not satisfy trait predicate") != std::string::npos;
    }));
    EXPECT_TRUE(std::ranges::any_of(rejected_snapshot.diagnostics, [](const tooling::IdeDiagnostic& diagnostic) {
        return diagnostic.severity == base::Severity::note
            && diagnostic.message.find("rejected trait impl candidate") != std::string::npos
            && diagnostic.message.find("self type mismatch") != std::string::npos;
    }));
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
    EXPECT_TRUE(tooling::semantic_tokens(lex_snapshot).empty());

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
    EXPECT_TRUE(tooling::completion_items_at_offset(parse_snapshot, parse_error_source.find("top_level")).empty());
    EXPECT_TRUE(tooling::inlay_hints(parse_snapshot).empty());
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
