#include <aurex/driver/pipeline_stage.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/parse/lossless_parse.hpp>
#include <aurex/parse/parser.hpp>
#include <aurex/query/diagnostics_query.hpp>
#include <aurex/query/function_body_syntax_query.hpp>
#include <aurex/query/generic_template_signature_query.hpp>
#include <aurex/query/item_list_query.hpp>
#include <aurex/query/item_signature_query.hpp>
#include <aurex/query/module_exports_query.hpp>
#include <aurex/query/module_graph_query.hpp>
#include <aurex/query/module_part_query.hpp>
#include <aurex/query/stable_hash.hpp>
#include <aurex/query/type_check_body_query.hpp>
#include <aurex/sema/function.hpp>
#include <aurex/sema/sema.hpp>
#include <aurex/syntax/module.hpp>
#include <aurex/tooling/ide.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace aurex::tooling {
namespace {

constexpr std::string_view IDE_QUERY_PACKAGE_DEFAULT = "ide";
constexpr std::string_view IDE_CONTENT_RESULT_MARKER = "ide-content:v1";
constexpr std::string_view IDE_LEX_RESULT_MARKER = "ide-lex:v1";
constexpr std::string_view IDE_PARSE_RESULT_MARKER = "ide-parse:v1";
constexpr std::string_view IDE_DIAGNOSTICS_RESULT_MARKER = "ide-diagnostics:v1";
constexpr std::string_view IDE_MODULE_GRAPH_RESULT_MARKER = "module-graph:v1";
constexpr std::string_view IDE_MODULE_PART_RESULT_MARKER = "module-part:v1";
constexpr std::string_view IDE_MODULE_EXPORTS_RESULT_MARKER = "module-exports:v1";
constexpr std::string_view IDE_ITEM_LIST_RESULT_MARKER = "item-list:v1";
constexpr std::string_view IDE_FUNCTION_BODY_SYNTAX_RESULT_MARKER = "function-body-syntax:v1";
constexpr std::string_view IDE_TYPE_CHECK_BODY_RESULT_MARKER = "type-check-body:v1";
constexpr std::string_view IDE_PARSE_SKIPPED_MARKER = "parse-skipped";
constexpr std::string_view IDE_HOVER_IDENTIFIER_PREFIX = "identifier ";
constexpr std::string_view IDE_HOVER_TOKEN_PREFIX = "token ";
constexpr std::string_view IDE_SEMANTIC_FACT_ITEM_SIGNATURE = "item_signature";
constexpr std::string_view IDE_SEMANTIC_FACT_GENERIC_TEMPLATE_SIGNATURE = "generic_template_signature";
constexpr std::string_view IDE_SEMANTIC_FACT_FUNCTION_BODY_SYNTAX = "function_body_syntax";
constexpr std::string_view IDE_SEMANTIC_FACT_TYPE_CHECK_BODY = "type_check_body";
constexpr std::string_view IDE_SYMBOL_KIND_ENUM_CASE = "enum_case";
constexpr std::string_view IDE_SYMBOL_KIND_FUNCTION = "function";
constexpr std::string_view IDE_SYMBOL_KIND_GENERIC_TEMPLATE = "generic_template";
constexpr std::string_view IDE_SYMBOL_KIND_LOCAL = "local";
constexpr std::string_view IDE_SYMBOL_KIND_METHOD = "method";
constexpr std::string_view IDE_SYMBOL_KIND_OPAQUE_STRUCT = "opaque_struct";
constexpr std::string_view IDE_SYMBOL_KIND_PARAMETER = "parameter";
constexpr std::string_view IDE_SYMBOL_KIND_STRUCT = "struct";
constexpr std::string_view IDE_SYMBOL_KIND_STRUCT_FIELD = "struct_field";
constexpr std::string_view IDE_SYMBOL_KIND_TYPE_ALIAS = "type_alias";
constexpr std::string_view IDE_PRIMARY_PART_NAME = "<primary>";
constexpr base::u32 IDE_PRIMARY_PART_INDEX = 0;
constexpr base::u32 IDE_FIRST_NAMED_PART_INDEX = 1;

struct ItemDefinitionMetadata {
    query::DefNamespace namespace_ = query::DefNamespace::value;
    query::DefKind kind = query::DefKind::invalid;
    std::string_view label;
};

struct IdeSymbol {
    IdeSymbol() = default;
    IdeSymbol(query::DefKey key, base::SourceRange range, base::SourceRange scope_range, std::string name,
        std::string kind, std::string detail, base::u32 part_index, bool local, bool checked,
        query::MemberKey member = {}, query::GenericInstanceKey generic_instance = {})
        : key(key), range(range), scope_range(scope_range), name(std::move(name)), kind(std::move(kind)),
          detail(std::move(detail)), part_index(part_index), local(local), checked(checked), member(member),
          generic_instance(std::move(generic_instance))
    {
    }

    query::DefKey key;
    base::SourceRange range{};
    base::SourceRange scope_range{};
    std::string name;
    std::string kind;
    std::string detail;
    base::u32 part_index = 0;
    bool local = false;
    bool checked = false;
    query::MemberKey member;
    query::GenericInstanceKey generic_instance;
};

struct SymbolIndex {
    std::vector<IdeSymbol> symbols;
    std::unordered_map<std::string, std::vector<base::usize>> globals;
    std::unordered_map<std::string, std::vector<base::usize>> locals;
};

[[nodiscard]] std::string_view package_identity_or_default(const IdeSnapshotRequest& request) noexcept
{
    return request.package_identity.empty() ? IDE_QUERY_PACKAGE_DEFAULT : std::string_view{request.package_identity};
}

[[nodiscard]] query::PackageKey ide_package_key(const IdeSnapshotRequest& request) noexcept
{
    const std::array<std::string_view, 1> parts{package_identity_or_default(request)};
    return query::package_key(parts);
}

[[nodiscard]] query::FileKey ide_file_key(const IdeSnapshotRequest& request) noexcept
{
    return query::file_key(
        ide_package_key(request), request.path, request.source_role, request.virtual_buffer_identity);
}

void mix_source_range(query::StableHashBuilder& builder, const base::SourceRange& range) noexcept
{
    builder.mix_u64(range.source.value);
    builder.mix_u64(range.begin);
    builder.mix_u64(range.end);
}

void mix_token(query::StableHashBuilder& builder, const syntax::Token& token) noexcept
{
    builder.mix_u64(static_cast<base::u64>(token.kind));
    mix_source_range(builder, token.range);
    builder.mix_string(token.text());
}

[[nodiscard]] query::QueryResultFingerprint content_result_fingerprint(const std::string_view text) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(IDE_CONTENT_RESULT_MARKER);
    builder.mix_string(text);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint lex_result_fingerprint(
    const std::span<const syntax::Token> tokens, const std::span<const base::Diagnostic> diagnostics) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(IDE_LEX_RESULT_MARKER);
    builder.mix_u64(tokens.size());
    for (const syntax::Token& token : tokens) {
        mix_token(builder, token);
    }
    builder.mix_u64(diagnostics.size());
    for (const base::Diagnostic& diagnostic : diagnostics) {
        builder.mix_u64(static_cast<base::u64>(diagnostic.severity));
        builder.mix_u64(static_cast<base::u64>(diagnostic.category));
        builder.mix_u64(static_cast<base::u64>(diagnostic.code));
        mix_source_range(builder, diagnostic.range);
        builder.mix_string(diagnostic.message);
    }
    return query::query_result_fingerprint(builder.finish());
}

void mix_lossless_tree(query::StableHashBuilder& builder, const syntax::LosslessSyntaxTree& tree) noexcept
{
    builder.mix_bool(tree.is_structurally_valid());
    builder.mix_u64(tree.node_count());
    builder.mix_u64(tree.element_count());
    builder.mix_u64(tree.token_count());
    for (base::usize index = 0; index < tree.nodes().size(); ++index) {
        const syntax::LosslessNode& node = tree.nodes()[index];
        builder.mix_u64(index);
        builder.mix_u64(static_cast<base::u64>(node.kind));
        mix_source_range(builder, node.range);
        builder.mix_u64(node.parent.value);
        builder.mix_u64(node.first_child);
        builder.mix_u64(node.child_count);
        builder.mix_u64(node.first_token);
        builder.mix_u64(node.token_count);
    }
    for (base::usize index = 0; index < tree.elements().size(); ++index) {
        const syntax::LosslessElement& element = tree.elements()[index];
        builder.mix_u64(index);
        builder.mix_u64(static_cast<base::u64>(element.kind));
        builder.mix_u64(element.index);
    }
}

[[nodiscard]] query::QueryResultFingerprint parse_result_fingerprint(
    const syntax::LosslessSyntaxTree& tree, const bool parsed, const std::span<const base::Diagnostic> diagnostics)
{
    query::StableHashBuilder builder;
    builder.mix_string(IDE_PARSE_RESULT_MARKER);
    mix_lossless_tree(builder, tree);
    builder.mix_bool(parsed);
    builder.mix_u64(diagnostics.size());
    for (const base::Diagnostic& diagnostic : diagnostics) {
        builder.mix_u64(static_cast<base::u64>(diagnostic.severity));
        builder.mix_u64(static_cast<base::u64>(diagnostic.category));
        builder.mix_u64(static_cast<base::u64>(diagnostic.code));
        mix_source_range(builder, diagnostic.range);
        builder.mix_string(diagnostic.message);
    }
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint diagnostics_result_fingerprint(
    const std::span<const query::QueryDiagnosticEvent> events, const bool parsed, const bool checked)
{
    const std::array<std::string_view, 3> context{
        IDE_DIAGNOSTICS_RESULT_MARKER,
        parsed ? "parsed" : "parse-failed",
        checked ? "checked" : "check-failed",
    };
    return query::diagnostics_result_fingerprint(events, context);
}

[[nodiscard]] std::vector<IdePipelineStageOwner> ide_diagnostic_owner_stages(const base::DiagnosticCategory category)
{
    const std::span<const driver::PipelineStageId> owner_stage_ids =
        driver::pipeline_stage_ids_for_diagnostic_category(category);
    std::vector<IdePipelineStageOwner> owners;
    owners.reserve(owner_stage_ids.size());
    for (const driver::PipelineStageId stage_id : owner_stage_ids) {
        owners.push_back(driver::pipeline_stage_metadata(stage_id));
    }
    return owners;
}

[[nodiscard]] std::vector<IdeDiagnostic> collect_ide_diagnostics(const base::SourceManager& sources,
    const query::DiagnosticsEventStream& diagnostics, const IdeModulePartContext& source_part)
{
    std::vector<IdeDiagnostic> result;
    result.reserve(diagnostics.events.size());
    for (const query::QueryDiagnosticEvent& diagnostic : diagnostics.events) {
        const base::SourceFile& file = sources.get(diagnostic.range.source);
        result.push_back(IdeDiagnostic{
            diagnostic.severity,
            diagnostic.category,
            diagnostic.code,
            diagnostic.range,
            file.line_column(diagnostic.range.begin),
            file.line_column(diagnostic.range.end),
            std::string(file.path()),
            diagnostic.message,
            ide_diagnostic_owner_stages(diagnostic.category),
            source_part,
        });
    }
    return result;
}

[[nodiscard]] const query::QueryRecord* reusable_previous_query_record(
    const IdeQuerySnapshot& previous, const query::QueryRecord& current)
{
    for (const query::QueryRecord& record : previous.records) {
        if (record.key == current.key
            && query::query_record_change_status(&record, current) == query::QueryRecordChangeStatus::unchanged) {
            return &record;
        }
    }
    return nullptr;
}

[[nodiscard]] std::vector<query::QueryKey> previous_dependencies_for_query(
    const IdeQuerySnapshot& previous, const query::QueryKey key)
{
    std::vector<query::QueryKey> dependencies;
    for (const query::QueryDependencyEdge& edge : previous.dependencies) {
        if (edge.dependent == key) {
            dependencies.push_back(edge.dependency);
        }
    }
    return dependencies;
}

void seed_reusable_query_record(query::QueryContext& context, const IdeIncrementalSnapshotInput& incremental,
    const std::optional<query::QueryRecord>& current_record)
{
    if (incremental.previous_query == nullptr || !current_record.has_value()) {
        return;
    }
    const query::QueryRecord* const previous =
        reusable_previous_query_record(*incremental.previous_query, *current_record);
    if (previous == nullptr) {
        return;
    }
    static_cast<void>(context.seed_completed_record(
        *previous, previous_dependencies_for_query(*incremental.previous_query, previous->key)));
}

void evaluate_source_queries(IdeSnapshot& snapshot, const std::string_view source_text,
    const query::QueryResultFingerprint lex_result, const query::QueryResultFingerprint parse_result,
    const query::QueryResultFingerprint diagnostics_result, const query::DiagnosticsEventStream& diagnostics,
    const IdeIncrementalSnapshotInput& incremental)
{
    query::QueryContext context;
    const query::QueryResultFingerprint content_result = content_result_fingerprint(source_text);
    seed_reusable_query_record(
        context, incremental, query::file_content_query_record(snapshot.query.source_stage.file, content_result));
    static_cast<void>(context.evaluate_file_content(query::FileContentProviderInput{
        snapshot.query.source_stage.file,
        content_result,
    }));
    seed_reusable_query_record(
        context, incremental, query::lex_file_query_record(snapshot.query.source_stage.lex_file, lex_result));
    static_cast<void>(context.evaluate_lex_file(query::LexFileProviderInput{
        snapshot.query.source_stage.lex_file,
        lex_result,
    }));
    seed_reusable_query_record(
        context, incremental, query::parse_file_query_record(snapshot.query.source_stage.parse_file, parse_result));
    static_cast<void>(context.evaluate_parse_file(query::ParseFileProviderInput{
        snapshot.query.source_stage.parse_file,
        parse_result,
    }));
    if (const std::optional<query::QueryKey> parse_query =
            query::parse_file_query_key(snapshot.query.source_stage.parse_file)) {
        seed_reusable_query_record(
            context, incremental, query::diagnostics_query_record(*parse_query, diagnostics_result));
        static_cast<void>(context.evaluate_diagnostics(query::DiagnosticsProviderInput{
            *parse_query,
            diagnostics_result,
            diagnostics.events,
        }));
    }
    snapshot.query.records = context.completed_records();
    snapshot.query.dependencies = context.dependency_edges();
}

[[nodiscard]] bool token_contains_offset(const syntax::Token& token, const base::usize offset) noexcept
{
    if (token.range.empty()) {
        return offset == token.range.begin;
    }
    return token.range.begin <= offset && offset < token.range.end;
}

[[nodiscard]] const syntax::Token* token_at_offset(const IdeSnapshot& snapshot, const base::usize offset) noexcept
{
    const syntax::LosslessTokenId token_id = snapshot.lossless.token_at_offset(offset);
    return snapshot.lossless.token(token_id);
}

[[nodiscard]] std::optional<IdeTokenInfo> make_token_info(
    const IdeSnapshot& snapshot, const syntax::Token& token) noexcept
{
    syntax::LosslessNodeId node_id = snapshot.lossless.node_at_offset(token.range.begin);
    const syntax::LosslessNode* node = snapshot.lossless.node(node_id);
    if (node == nullptr) {
        node_id = snapshot.lossless.root_id();
        node = snapshot.lossless.root_node();
    }
    return IdeTokenInfo{
        token.kind,
        token.range,
        std::string(token.text()),
        node_id,
        node->kind,
        snapshot.lossless.node_key(node_id),
        true,
        syntax::is_trivia_token(token.kind),
    };
}

[[nodiscard]] std::optional<ItemDefinitionMetadata> item_definition_metadata(const syntax::ItemKind kind) noexcept
{
    switch (kind) {
        case syntax::ItemKind::const_decl:
            return ItemDefinitionMetadata{query::DefNamespace::value, query::DefKind::const_, "const"};
        case syntax::ItemKind::type_alias:
            return ItemDefinitionMetadata{query::DefNamespace::type, query::DefKind::type_alias, "type_alias"};
        case syntax::ItemKind::struct_decl:
            return ItemDefinitionMetadata{query::DefNamespace::type, query::DefKind::struct_, "struct"};
        case syntax::ItemKind::enum_decl:
            return ItemDefinitionMetadata{query::DefNamespace::type, query::DefKind::enum_, "enum"};
        case syntax::ItemKind::opaque_struct_decl:
            return ItemDefinitionMetadata{query::DefNamespace::type, query::DefKind::struct_, "opaque_struct"};
        case syntax::ItemKind::fn_decl:
            return ItemDefinitionMetadata{query::DefNamespace::value, query::DefKind::function, "function"};
        case syntax::ItemKind::extern_block:
        case syntax::ItemKind::impl_block:
            return std::nullopt;
    }
    return std::nullopt;
}

[[nodiscard]] query::StableSymbolKind item_stable_symbol_kind(const syntax::ItemKind kind) noexcept
{
    switch (kind) {
        case syntax::ItemKind::const_decl:
            return query::StableSymbolKind::value;
        case syntax::ItemKind::type_alias:
        case syntax::ItemKind::struct_decl:
        case syntax::ItemKind::enum_decl:
        case syntax::ItemKind::opaque_struct_decl:
            return query::StableSymbolKind::type;
        case syntax::ItemKind::fn_decl:
            return query::StableSymbolKind::function;
        case syntax::ItemKind::extern_block:
        case syntax::ItemKind::impl_block:
            return query::StableSymbolKind::invalid;
    }
    return query::StableSymbolKind::invalid;
}

[[nodiscard]] base::SourceRange definition_name_range(
    const syntax::LosslessSyntaxTree& tree, const syntax::ItemNode& item) noexcept
{
    for (const syntax::Token& token : tree.tokens()) {
        if (token.kind == syntax::TokenKind::identifier && token.text() == item.name
            && item.range.begin <= token.range.begin && token.range.end <= item.range.end) {
            return token.range;
        }
    }
    return item.range;
}

[[nodiscard]] base::SourceRange name_range_in_range(
    const syntax::LosslessSyntaxTree& tree, const std::string_view name, const base::SourceRange fallback) noexcept
{
    for (const syntax::Token& token : tree.tokens()) {
        if (token.kind == syntax::TokenKind::identifier && token.text() == name && fallback.begin <= token.range.begin
            && token.range.end <= fallback.end) {
            return token.range;
        }
    }
    return fallback;
}

[[nodiscard]] base::SourceRange item_name_range(const IdeSnapshot& snapshot, const syntax::ItemId item_id,
    const std::string_view name, const base::SourceRange fallback) noexcept
{
    if (syntax::is_valid(item_id)) {
        if (const syntax::ItemNode* const item = snapshot.ast.items.ptr(item_id.value); item != nullptr) {
            return definition_name_range(snapshot.lossless, *item);
        }
    }
    return name_range_in_range(snapshot.lossless, name, fallback);
}

[[nodiscard]] base::SourceRange first_item_name_range(
    const IdeSnapshot& snapshot, const std::string_view name, const base::SourceRange fallback) noexcept
{
    for (base::usize index = 0; index < snapshot.ast.items.size(); ++index) {
        const syntax::ItemNode* const item = snapshot.ast.items.ptr(index);
        if (item != nullptr && item->name == name) {
            return definition_name_range(snapshot.lossless, *item);
        }
    }
    return name_range_in_range(snapshot.lossless, name, fallback);
}

[[nodiscard]] query::ModuleKey module_key_for_snapshot(const IdeSnapshot& snapshot)
{
    std::vector<std::string_view> parts = snapshot.ast.module_path.parts;
    if (parts.empty()) {
        parts.push_back("ide");
    }
    return query::module_key_from_stable_id(snapshot.query.source_stage.file.package, query::stable_module_id(parts));
}

[[nodiscard]] std::string module_path_display_name(const syntax::ModulePath& path)
{
    std::string result;
    for (const std::string_view part : path.parts) {
        if (!result.empty()) {
            result.push_back('.');
        }
        result.append(part);
    }
    return result;
}

[[nodiscard]] std::filesystem::path ide_canonical_or_absolute(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical;
    }
    return std::filesystem::absolute(path);
}

[[nodiscard]] bool ide_path_exists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

[[nodiscard]] std::optional<std::filesystem::path> ide_owning_primary_for_part_file(
    const std::filesystem::path& part_file)
{
    const std::filesystem::path part_dir = part_file.parent_path();
    if (part_dir.extension() != ".parts") {
        return std::nullopt;
    }
    std::filesystem::path primary = part_dir.parent_path() / part_dir.stem();
    primary += ".ax";
    return primary;
}

[[nodiscard]] std::filesystem::path ide_module_part_file_path(
    const std::filesystem::path& primary_file, const std::string_view part_name)
{
    std::filesystem::path base = primary_file;
    base.replace_extension();
    base += ".parts";
    return base / (std::string(part_name) + ".ax");
}

[[nodiscard]] std::optional<std::string> read_ide_text_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

[[nodiscard]] std::optional<syntax::AstModule> parse_ide_module_file(const std::filesystem::path& path)
{
    std::optional<std::string> text = read_ide_text_file(path);
    if (!text.has_value()) {
        return std::nullopt;
    }
    base::SourceManager sources;
    base::DiagnosticSink diagnostics;
    const base::SourceId source_id = sources.add_source(path.string(), *text);
    lex::Lexer lexer(source_id, sources.text(source_id), diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        return std::nullopt;
    }
    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (!parsed) {
        return std::nullopt;
    }
    return parsed.take_value();
}

void recover_resolved_fragment_source_part(
    IdeModulePartContext& context, const IdeSnapshot& snapshot, const IdeSnapshotRequest& request)
{
    const std::filesystem::path part_path = ide_canonical_or_absolute(request.path);
    const std::optional<std::filesystem::path> primary_path = ide_owning_primary_for_part_file(part_path);
    if (!primary_path.has_value() || !ide_path_exists(*primary_path)) {
        return;
    }
    std::optional<syntax::AstModule> primary = parse_ide_module_file(*primary_path);
    if (!primary.has_value() || primary->file_kind != syntax::ModuleFileKind::primary
        || !syntax::module_paths_equal(primary->module_path, snapshot.ast.module_path)) {
        return;
    }
    for (base::u32 index = 0; index < primary->part_declarations.size(); ++index) {
        const syntax::ModulePartDecl& declaration = primary->part_declarations[index];
        if (declaration.name != context.part_name) {
            continue;
        }
        const std::filesystem::path expected_part =
            ide_canonical_or_absolute(ide_module_part_file_path(*primary_path, declaration.name));
        if (expected_part != part_path) {
            continue;
        }
        context.part_index = index + IDE_FIRST_NAMED_PART_INDEX;
        context.part_key = query::module_part_key(
            context.module_key, snapshot.query.source_stage.file, context.kind, context.part_name, context.part_index);
        context.resolved = query::is_valid(context.part_key);
        context.valid = context.resolved;
        return;
    }
}

[[nodiscard]] IdeModulePartContext source_part_context_for_snapshot(
    const IdeSnapshot& snapshot, const IdeSnapshotRequest& request)
{
    IdeModulePartContext context;
    if (!snapshot.parsed || snapshot.ast.module_path.parts.empty()) {
        return context;
    }

    context.module_key = module_key_for_snapshot(snapshot);
    context.module_range = snapshot.ast.module_path.range;
    context.module_name = module_path_display_name(snapshot.ast.module_path);
    if (snapshot.ast.file_kind == syntax::ModuleFileKind::part) {
        context.kind = query::ModulePartKind::fragment;
        context.part_range = snapshot.ast.part_header.range;
        context.part_name = std::string(snapshot.ast.part_header.name);
        context.valid = !context.module_name.empty() && !context.part_name.empty();
        recover_resolved_fragment_source_part(context, snapshot, request);
        return context;
    }

    context.kind = query::ModulePartKind::primary;
    context.part_range = snapshot.ast.module_path.range;
    context.part_name = std::string(IDE_PRIMARY_PART_NAME);
    context.part_index = IDE_PRIMARY_PART_INDEX;
    context.part_key = query::module_part_key(
        context.module_key, snapshot.query.source_stage.file, context.kind, IDE_PRIMARY_PART_NAME, context.part_index);
    context.resolved = query::is_valid(context.part_key);
    context.valid = context.resolved;
    return context;
}

[[nodiscard]] bool range_contains_offset(const base::SourceRange range, const base::usize offset) noexcept
{
    if (range.empty()) {
        return offset == range.begin;
    }
    return range.begin <= offset && offset < range.end;
}

[[nodiscard]] bool range_contains_range(const base::SourceRange outer, const base::SourceRange inner) noexcept
{
    return outer.source.value == inner.source.value && outer.begin <= inner.begin && inner.end <= outer.end;
}

[[nodiscard]] query::DefKind symbol_kind_to_def_kind(const query::StableSymbolKind kind) noexcept
{
    if (kind == query::StableSymbolKind::struct_field) {
        return query::DefKind::struct_field;
    }
    return query::DefKind::invalid;
}

[[nodiscard]] query::MemberKind symbol_kind_to_member_kind(const query::StableSymbolKind kind) noexcept
{
    if (kind == query::StableSymbolKind::enum_case) {
        return query::MemberKind::enum_case;
    }
    if (kind == query::StableSymbolKind::struct_field) {
        return query::MemberKind::struct_field;
    }
    return query::MemberKind::invalid;
}

[[nodiscard]] query::DefKey symbol_def_key(const IdeSnapshot& snapshot, const query::StableDefId stable_id,
    const query::DefNamespace name_space, const query::DefKind kind, const std::string_view fallback_name,
    const base::SourceRange fallback_range) noexcept
{
    if (query::is_valid(stable_id)) {
        return query::def_key_from_stable_id(module_key_for_snapshot(snapshot).package, stable_id, name_space, kind);
    }
    const std::array<std::string_view, 1> path{fallback_name};
    return query::def_key(
        module_key_for_snapshot(snapshot), name_space, kind, path, static_cast<base::u32>(fallback_range.begin));
}

[[nodiscard]] query::MemberKey symbol_member_key(const IdeSnapshot& snapshot, const query::StableMemberKey stable_key,
    const query::DefKind owner_kind, const std::string_view fallback_name)
{
    const query::MemberKind kind = symbol_kind_to_member_kind(stable_key.kind);
    if (!query::is_valid(stable_key) || kind == query::MemberKind::invalid || owner_kind == query::DefKind::invalid) {
        return {};
    }
    const query::DefKey owner = query::def_key_from_stable_id(
        module_key_for_snapshot(snapshot).package, stable_key.owner, query::DefNamespace::type, owner_kind);
    if (!query::is_valid(owner)) {
        return {};
    }
    const query::MemberKey member = query::member_key(owner, kind, fallback_name, stable_key.disambiguator);
    return query::is_valid(member) ? member : query::MemberKey{};
}

[[nodiscard]] std::optional<std::string_view> source_range_text(
    const base::SourceManager& sources, const base::SourceRange& range) noexcept
{
    const std::span<const base::SourceFile> files = sources.files();
    if (range.source.value >= files.size() || range.begin > range.end) {
        return std::nullopt;
    }
    const std::string_view text = files[range.source.value].text();
    if (range.end > text.size()) {
        return std::nullopt;
    }
    return std::string_view{text.data() + range.begin, range.end - range.begin};
}

[[nodiscard]] query::ModulePartKey ide_module_part_key_for_part_index(
    const IdeSnapshot& snapshot, const base::u32 part_index)
{
    if (snapshot.source_part.valid && snapshot.source_part.part_index == part_index
        && query::is_valid(snapshot.source_part.part_key)) {
        return snapshot.source_part.part_key;
    }
    const query::ModulePartKind kind =
        part_index == IDE_PRIMARY_PART_INDEX ? query::ModulePartKind::primary : query::ModulePartKind::fragment;
    if (part_index == IDE_PRIMARY_PART_INDEX) {
        return query::module_part_key(
            module_key_for_snapshot(snapshot), snapshot.query.source_stage.file, kind, IDE_PRIMARY_PART_NAME);
    }
    const std::string part_name = "<ide-part-" + std::to_string(part_index) + ">";
    return query::module_part_key(
        module_key_for_snapshot(snapshot), snapshot.query.source_stage.file, kind, part_name, part_index);
}

[[nodiscard]] query::DefKind function_signature_def_kind(const sema::FunctionSignature& signature) noexcept
{
    return signature.is_method ? query::DefKind::method : query::DefKind::function;
}

[[nodiscard]] query::DefKey function_signature_def_key(
    const IdeSnapshot& snapshot, const sema::FunctionSignature& signature)
{
    return symbol_def_key(snapshot, signature.stable_id, query::DefNamespace::value,
        function_signature_def_kind(signature), signature.name.view(), signature.range);
}

[[nodiscard]] query::DefKey ast_item_def_key(const IdeSnapshot& snapshot, const syntax::ItemNode& item)
{
    const std::optional<ItemDefinitionMetadata> metadata = item_definition_metadata(item.kind);
    const query::StableSymbolKind stable_kind = item_stable_symbol_kind(item.kind);
    if (!metadata.has_value() || stable_kind == query::StableSymbolKind::invalid || item.name.empty()) {
        return {};
    }
    std::vector<std::string_view> parts = snapshot.ast.module_path.parts;
    if (parts.empty()) {
        parts.push_back("ide");
    }
    const query::StableDefId stable_id =
        query::stable_definition_id(query::stable_module_id(parts), stable_kind, item.name);
    return query::def_key_from_stable_id(
        module_key_for_snapshot(snapshot).package, stable_id, metadata->namespace_, metadata->kind);
}

[[nodiscard]] std::optional<base::SourceRange> ast_item_body_range(
    const IdeSnapshot& snapshot, const syntax::ItemNode& item) noexcept
{
    if (item.kind != syntax::ItemKind::fn_decl || !syntax::is_valid(item.body)
        || item.body.value >= snapshot.ast.stmts.size()) {
        return std::nullopt;
    }
    return snapshot.ast.stmts.range(item.body.value);
}

[[nodiscard]] query::QueryResultFingerprint ide_module_graph_result_fingerprint(const IdeSnapshot& snapshot)
{
    query::StableHashBuilder builder;
    builder.mix_string(IDE_MODULE_GRAPH_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(module_key_for_snapshot(snapshot)));
    builder.mix_string(snapshot.source_part.module_name);
    builder.mix_u64(snapshot.ast.modules.size());
    builder.mix_u64(snapshot.ast.part_declarations.size());
    for (const syntax::ModulePartDecl& declaration : snapshot.ast.part_declarations) {
        builder.mix_string(declaration.name);
        mix_source_range(builder, declaration.range);
    }
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint ide_module_part_result_fingerprint(const IdeSnapshot& snapshot)
{
    query::StableHashBuilder builder;
    builder.mix_string(IDE_MODULE_PART_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(snapshot.source_part.part_key));
    builder.mix_string(snapshot.source_part.module_name);
    builder.mix_string(snapshot.source_part.part_name);
    builder.mix_u32(snapshot.source_part.part_index);
    builder.mix_u8(static_cast<base::u8>(snapshot.source_part.kind));
    mix_source_range(builder, snapshot.source_part.module_range);
    mix_source_range(builder, snapshot.source_part.part_range);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] base::u32 ide_item_part_index(const IdeSnapshot& snapshot, const base::usize item_index) noexcept
{
    return item_index < snapshot.ast.item_part_indices.size() ? snapshot.ast.item_part_indices[item_index]
                                                              : IDE_PRIMARY_PART_INDEX;
}

[[nodiscard]] query::QueryResultFingerprint ide_item_list_result_fingerprint(const IdeSnapshot& snapshot)
{
    query::StableHashBuilder builder;
    builder.mix_string(IDE_ITEM_LIST_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(module_key_for_snapshot(snapshot)));
    builder.mix_u64(snapshot.ast.items.size());
    for (base::usize index = 0; index < snapshot.ast.items.size(); ++index) {
        const syntax::ItemNode* const item = snapshot.ast.items.ptr(index);
        if (item == nullptr) {
            continue;
        }
        builder.mix_u64(index);
        builder.mix_u8(static_cast<base::u8>(item->kind));
        builder.mix_string(item->name);
        builder.mix_u32(ide_item_part_index(snapshot, index));
        builder.mix_u32(static_cast<base::u32>(item->generic_params.size()));
        mix_source_range(builder, item->range);
    }
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint ide_module_exports_result_fingerprint(
    const IdeSnapshot& snapshot, const query::QueryResultFingerprint item_list_result)
{
    query::StableHashBuilder builder;
    builder.mix_string(IDE_MODULE_EXPORTS_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(module_key_for_snapshot(snapshot)));
    builder.mix_u64(item_list_result.global_id);
    builder.mix_fingerprint(item_list_result.fingerprint);
    builder.mix_u64(snapshot.checked.functions.size());
    builder.mix_u64(snapshot.checked.structs.size());
    builder.mix_u64(snapshot.checked.enum_cases.size());
    builder.mix_u64(snapshot.checked.type_aliases.size());
    builder.mix_u64(snapshot.checked.generic_template_signatures.size());
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint function_body_syntax_content_fingerprint(
    const query::BodyKey key, const std::string_view body_text)
{
    query::StableHashBuilder builder;
    builder.mix_string(IDE_FUNCTION_BODY_SYNTAX_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_string(body_text);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint type_check_body_checked_result_fingerprint(const query::BodyKey key,
    const query::QueryResultFingerprint body_syntax_result, const query::QueryResultFingerprint signature_result)
{
    query::StableHashBuilder builder;
    builder.mix_string(IDE_TYPE_CHECK_BODY_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(body_syntax_result.global_id);
    builder.mix_fingerprint(body_syntax_result.fingerprint);
    builder.mix_u64(signature_result.global_id);
    builder.mix_fingerprint(signature_result.fingerprint);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] std::optional<base::SourceRange> function_signature_body_range(
    const IdeSnapshot& snapshot, const sema::FunctionSignature& signature) noexcept
{
    const syntax::ItemId item =
        syntax::is_valid(signature.definition_item) ? signature.definition_item : signature.prototype_item;
    if (!syntax::is_valid(item) || item.value >= snapshot.ast.items.size()) {
        return std::nullopt;
    }
    const syntax::ItemNode* const node = snapshot.ast.items.ptr(item.value);
    if (node == nullptr || node->kind != syntax::ItemKind::fn_decl || !syntax::is_valid(node->body)
        || node->body.value >= snapshot.ast.stmts.size()) {
        return std::nullopt;
    }
    return snapshot.ast.stmts.range(node->body.value);
}

[[nodiscard]] bool query_evaluation_completed(const query::QueryEvaluationResult& result) noexcept
{
    return result.status == query::QueryEvaluationStatus::computed
        || result.status == query::QueryEvaluationStatus::cached;
}

[[nodiscard]] std::string function_detail(const sema::CheckedModule& checked, const sema::FunctionSignature& signature)
{
    std::ostringstream label;
    label << (signature.is_method ? IDE_SYMBOL_KIND_METHOD : IDE_SYMBOL_KIND_FUNCTION) << ' '
          << sema::function_display_name(checked.types, signature) << '(';
    for (base::usize index = 0; index < signature.param_types.size(); ++index) {
        if (index != 0U) {
            label << ", ";
        }
        label << checked.types.display_name(signature.param_types[index]);
    }
    label << ") -> " << checked.types.display_name(signature.return_type);
    return label.str();
}

[[nodiscard]] std::string typed_detail(const sema::CheckedModule& checked, const std::string_view kind,
    const std::string_view name, const sema::TypeHandle type)
{
    std::ostringstream label;
    label << kind << ' ' << name;
    if (sema::is_valid(type)) {
        label << ": " << checked.types.display_name(type);
    }
    return label.str();
}

void push_global_symbol(SymbolIndex& index, IdeSymbol symbol)
{
    const base::usize symbol_index = index.symbols.size();
    const std::string name = symbol.name;
    index.symbols.push_back(std::move(symbol));
    index.globals[name].push_back(symbol_index);
}

void push_local_symbol(SymbolIndex& index, IdeSymbol symbol)
{
    const base::usize symbol_index = index.symbols.size();
    const std::string name = symbol.name;
    index.symbols.push_back(std::move(symbol));
    index.locals[name].push_back(symbol_index);
}

void push_checked_global_symbols(const IdeSnapshot& snapshot, SymbolIndex& index)
{
    for (const sema::GenericTemplateSignatureInfo& info : snapshot.checked.generic_template_signatures) {
        const base::SourceRange range =
            first_item_name_range(snapshot, info.name.view(), base::SourceRange{snapshot.source_id, 0U, 0U});
        push_global_symbol(index,
            IdeSymbol{
                symbol_def_key(snapshot, info.stable_id, info.name_space, query::DefKind::generic_template,
                    info.name.view(), range),
                range,
                range,
                std::string(info.name.view()),
                std::string(IDE_SYMBOL_KIND_GENERIC_TEMPLATE),
                std::string(IDE_SYMBOL_KIND_GENERIC_TEMPLATE) + " " + std::string(info.name.view()),
                info.part_index,
                false,
                true,
            });
    }
    for (const auto& entry : snapshot.checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        const query::DefKind kind = signature.is_method ? query::DefKind::method : query::DefKind::function;
        const base::SourceRange range =
            item_name_range(snapshot, signature.definition_item, signature.name.view(), signature.range);
        push_global_symbol(index,
            IdeSymbol{
                symbol_def_key(
                    snapshot, signature.stable_id, query::DefNamespace::value, kind, signature.name.view(), range),
                range,
                signature.range,
                std::string(signature.name.view()),
                std::string(signature.is_method ? IDE_SYMBOL_KIND_METHOD : IDE_SYMBOL_KIND_FUNCTION),
                function_detail(snapshot.checked, signature),
                signature.part_index,
                false,
                true,
                {},
                signature.generic_instance_key,
            });
    }
    for (const auto& entry : snapshot.checked.structs) {
        const sema::StructInfo& info = entry.second;
        const base::SourceRange range =
            first_item_name_range(snapshot, info.name.view(), base::SourceRange{snapshot.source_id, 0U, 0U});
        push_global_symbol(index,
            IdeSymbol{
                symbol_def_key(snapshot, info.stable_id, query::DefNamespace::type, query::DefKind::struct_,
                    info.name.view(), range),
                range,
                range,
                std::string(info.name.view()),
                std::string(info.is_opaque ? IDE_SYMBOL_KIND_OPAQUE_STRUCT : IDE_SYMBOL_KIND_STRUCT),
                typed_detail(snapshot.checked, info.is_opaque ? IDE_SYMBOL_KIND_OPAQUE_STRUCT : IDE_SYMBOL_KIND_STRUCT,
                    info.name.view(), info.type),
                info.part_index,
                false,
                true,
                {},
                info.generic_instance_key,
            });
        for (const sema::StructFieldInfo& field : info.fields) {
            const query::DefKind field_kind = symbol_kind_to_def_kind(field.stable_key.kind);
            const query::MemberKey member =
                symbol_member_key(snapshot, field.stable_key, query::DefKind::struct_, field.name.view());
            push_global_symbol(index,
                IdeSymbol{
                    symbol_def_key(snapshot, field.stable_key.owner, query::DefNamespace::member, field_kind,
                        field.name.view(), field.range),
                    name_range_in_range(snapshot.lossless, field.name.view(), field.range),
                    range,
                    std::string(field.name.view()),
                    std::string(IDE_SYMBOL_KIND_STRUCT_FIELD),
                    typed_detail(snapshot.checked, IDE_SYMBOL_KIND_STRUCT_FIELD, field.name.view(), field.type),
                    info.part_index,
                    false,
                    true,
                    member,
                    info.generic_instance_key,
                });
        }
    }
    for (const auto& entry : snapshot.checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        const base::SourceRange range = name_range_in_range(snapshot.lossless, info.case_name.view(), info.range);
        const query::MemberKey member =
            symbol_member_key(snapshot, info.stable_case_key, query::DefKind::enum_, info.case_name.view());
        push_global_symbol(index,
            IdeSymbol{
                symbol_def_key(snapshot, info.stable_id, query::DefNamespace::value, query::DefKind::enum_case,
                    info.case_name.view(), range),
                range,
                range,
                std::string(info.case_name.view()),
                std::string(IDE_SYMBOL_KIND_ENUM_CASE),
                sema::enum_case_display_name(snapshot.checked.types, info),
                info.part_index,
                false,
                true,
                member,
                info.generic_instance_key,
            });
    }
    for (const auto& entry : snapshot.checked.type_aliases) {
        const sema::TypeAliasInfo& info = entry.second;
        const base::SourceRange range = first_item_name_range(snapshot, info.name.view(), info.range);
        push_global_symbol(index,
            IdeSymbol{
                symbol_def_key(snapshot, info.stable_id, query::DefNamespace::type, query::DefKind::type_alias,
                    info.name.view(), range),
                range,
                range,
                std::string(info.name.view()),
                std::string(IDE_SYMBOL_KIND_TYPE_ALIAS),
                std::string(IDE_SYMBOL_KIND_TYPE_ALIAS) + " " + std::string(info.name.view()),
                info.part_index,
                false,
                true,
            });
    }
}

[[nodiscard]] query::ItemSignatureAuthority item_signature_authority_base(const IdeSnapshot& snapshot,
    const query::IncrementalKey incremental_key, const base::u32 part_index, const query::DefNamespace name_space,
    const query::DefKind kind, const syntax::Visibility visibility)
{
    return query::ItemSignatureAuthority{
        incremental_key,
        ide_module_part_key_for_part_index(snapshot, part_index),
        name_space,
        kind,
        syntax::visibility_rank(visibility),
    };
}

[[nodiscard]] query::ItemSignatureAuthority item_signature_authority(
    const IdeSnapshot& snapshot, const sema::FunctionSignature& signature)
{
    query::ItemSignatureAuthority authority = item_signature_authority_base(snapshot, signature.incremental_key,
        signature.part_index, query::DefNamespace::value, function_signature_def_kind(signature), signature.visibility);
    authority.value_component_count = static_cast<base::u32>(signature.param_types.size());
    authority.generic_param_count = static_cast<base::u32>(signature.generic_args.size());
    authority.has_return_type = sema::is_valid(signature.return_type);
    authority.has_receiver_type = sema::is_valid(signature.method_owner_type) || signature.has_self_param;
    authority.is_unsafe = signature.is_unsafe;
    authority.is_variadic = signature.is_variadic;
    authority.has_definition = signature.has_definition;
    return authority;
}

[[nodiscard]] query::ItemSignatureAuthority item_signature_authority(
    const IdeSnapshot& snapshot, const sema::StructInfo& info)
{
    query::ItemSignatureAuthority authority = item_signature_authority_base(snapshot, info.incremental_key,
        info.part_index, query::DefNamespace::type, query::DefKind::struct_, info.visibility);
    authority.value_component_count = static_cast<base::u32>(info.fields.size());
    authority.generic_param_count = static_cast<base::u32>(info.generic_instance_key.type_args.size());
    authority.has_return_type = sema::is_valid(info.type);
    authority.has_definition = !info.is_opaque;
    return authority;
}

[[nodiscard]] query::ItemSignatureAuthority item_signature_authority(
    const IdeSnapshot& snapshot, const sema::EnumCaseInfo& info)
{
    query::ItemSignatureAuthority authority = item_signature_authority_base(snapshot, info.incremental_key,
        info.part_index, query::DefNamespace::value, query::DefKind::enum_case, info.visibility);
    authority.value_component_count = static_cast<base::u32>(info.payload_types.size());
    authority.generic_param_count = static_cast<base::u32>(info.generic_instance_key.type_args.size());
    authority.has_return_type = sema::is_valid(info.type);
    authority.has_definition = true;
    return authority;
}

[[nodiscard]] query::ItemSignatureAuthority item_signature_authority(
    const IdeSnapshot& snapshot, const sema::TypeAliasInfo& info)
{
    query::ItemSignatureAuthority authority = item_signature_authority_base(snapshot, info.incremental_key,
        info.part_index, query::DefNamespace::type, query::DefKind::type_alias, info.visibility);
    authority.value_component_count = syntax::is_valid(info.target) ? 1U : 0U;
    authority.has_return_type = syntax::is_valid(info.target);
    authority.has_definition = true;
    return authority;
}

[[nodiscard]] query::GenericTemplateSignatureAuthority generic_template_signature_authority(
    const IdeSnapshot& snapshot, const sema::GenericTemplateSignatureInfo& info)
{
    return query::GenericTemplateSignatureAuthority{
        info.incremental_key,
        ide_module_part_key_for_part_index(snapshot, info.part_index),
        info.name_space,
        syntax::visibility_rank(info.visibility),
        info.param_count,
        info.constraint_count,
    };
}

void push_item_signature_fact(query::QueryContext& context, IdeSnapshot& snapshot, const query::DefKey key,
    const query::ItemSignatureAuthority& authority, IdeSemanticFact fact,
    const IdeIncrementalSnapshotInput& incremental)
{
    if (!query::is_valid(key) || !query::is_valid(authority)) {
        return;
    }
    seed_reusable_query_record(context, incremental,
        query::item_signature_query_record(key, query::item_signature_result_fingerprint(authority)));
    const query::QueryEvaluationResult result = context.evaluate_item_signature(query::ItemSignatureProviderInput{
        key,
        authority,
    });
    if (!query_evaluation_completed(result)) {
        return;
    }
    const std::optional<query::QueryKey> query_key = query::item_signature_query_key(key);
    if (!query_key.has_value()) {
        return;
    }
    fact.kind = IdeSemanticFactKind::item_signature;
    fact.query = *query_key;
    fact.definition = key;
    fact.checked = true;
    snapshot.query.semantic_facts.push_back(std::move(fact));
}

void push_generic_template_signature_fact(query::QueryContext& context, IdeSnapshot& snapshot, const query::DefKey key,
    const query::GenericTemplateSignatureAuthority& authority, IdeSemanticFact fact,
    const IdeIncrementalSnapshotInput& incremental)
{
    if (!query::is_valid(key) || !query::is_valid(authority)) {
        return;
    }
    seed_reusable_query_record(context, incremental,
        query::generic_template_signature_query_record(
            key, query::generic_template_signature_result_fingerprint(authority)));
    const query::QueryEvaluationResult result =
        context.evaluate_generic_template_signature(query::GenericTemplateSignatureProviderInput{
            key,
            authority,
        });
    if (!query_evaluation_completed(result)) {
        return;
    }
    const std::optional<query::QueryKey> query_key = query::generic_template_signature_query_key(key);
    if (!query_key.has_value()) {
        return;
    }
    fact.kind = IdeSemanticFactKind::generic_template_signature;
    fact.query = *query_key;
    fact.definition = key;
    fact.checked = true;
    snapshot.query.semantic_facts.push_back(std::move(fact));
}

void evaluate_function_item_signature_query(query::QueryContext& context, IdeSnapshot& snapshot,
    const sema::FunctionSignature& signature, const IdeIncrementalSnapshotInput& incremental)
{
    const query::DefKey key = function_signature_def_key(snapshot, signature);
    const base::SourceRange range =
        item_name_range(snapshot, signature.definition_item, signature.name.view(), signature.range);
    IdeSemanticFact fact;
    fact.range = range;
    fact.name = std::string(signature.name.view());
    fact.detail = std::string(IDE_SEMANTIC_FACT_ITEM_SIGNATURE);
    fact.part_index = signature.part_index;
    fact.generic_instance = signature.generic_instance_key;
    push_item_signature_fact(
        context, snapshot, key, item_signature_authority(snapshot, signature), std::move(fact), incremental);
}

void evaluate_struct_item_signature_query(query::QueryContext& context, IdeSnapshot& snapshot,
    const sema::StructInfo& info, const IdeIncrementalSnapshotInput& incremental)
{
    const base::SourceRange range =
        first_item_name_range(snapshot, info.name.view(), base::SourceRange{snapshot.source_id, 0U, 0U});
    const query::DefKey key = symbol_def_key(
        snapshot, info.stable_id, query::DefNamespace::type, query::DefKind::struct_, info.name.view(), range);
    IdeSemanticFact fact;
    fact.range = range;
    fact.name = std::string(info.name.view());
    fact.detail = std::string(IDE_SEMANTIC_FACT_ITEM_SIGNATURE);
    fact.part_index = info.part_index;
    fact.generic_instance = info.generic_instance_key;
    push_item_signature_fact(
        context, snapshot, key, item_signature_authority(snapshot, info), std::move(fact), incremental);
}

void evaluate_enum_case_item_signature_query(query::QueryContext& context, IdeSnapshot& snapshot,
    const sema::EnumCaseInfo& info, const IdeIncrementalSnapshotInput& incremental)
{
    const base::SourceRange range = name_range_in_range(snapshot.lossless, info.case_name.view(), info.range);
    const query::DefKey key = symbol_def_key(
        snapshot, info.stable_id, query::DefNamespace::value, query::DefKind::enum_case, info.case_name.view(), range);
    IdeSemanticFact fact;
    fact.range = range;
    fact.name = std::string(info.case_name.view());
    fact.detail = std::string(IDE_SEMANTIC_FACT_ITEM_SIGNATURE);
    fact.part_index = info.part_index;
    fact.generic_instance = info.generic_instance_key;
    push_item_signature_fact(
        context, snapshot, key, item_signature_authority(snapshot, info), std::move(fact), incremental);
}

void evaluate_type_alias_item_signature_query(query::QueryContext& context, IdeSnapshot& snapshot,
    const sema::TypeAliasInfo& info, const IdeIncrementalSnapshotInput& incremental)
{
    const base::SourceRange range = first_item_name_range(snapshot, info.name.view(), info.range);
    const query::DefKey key = symbol_def_key(
        snapshot, info.stable_id, query::DefNamespace::type, query::DefKind::type_alias, info.name.view(), range);
    IdeSemanticFact fact;
    fact.range = range;
    fact.name = std::string(info.name.view());
    fact.detail = std::string(IDE_SEMANTIC_FACT_ITEM_SIGNATURE);
    fact.part_index = info.part_index;
    push_item_signature_fact(
        context, snapshot, key, item_signature_authority(snapshot, info), std::move(fact), incremental);
}

void evaluate_generic_template_signature_query(query::QueryContext& context, IdeSnapshot& snapshot,
    const sema::GenericTemplateSignatureInfo& info, const IdeIncrementalSnapshotInput& incremental)
{
    const base::SourceRange range =
        first_item_name_range(snapshot, info.name.view(), base::SourceRange{snapshot.source_id, 0U, 0U});
    const query::DefKey key = symbol_def_key(
        snapshot, info.stable_id, info.name_space, query::DefKind::generic_template, info.name.view(), range);
    IdeSemanticFact fact;
    fact.range = range;
    fact.name = std::string(info.name.view());
    fact.detail = std::string(IDE_SEMANTIC_FACT_GENERIC_TEMPLATE_SIGNATURE);
    fact.part_index = info.part_index;
    push_generic_template_signature_fact(
        context, snapshot, key, generic_template_signature_authority(snapshot, info), std::move(fact), incremental);
}

void evaluate_checked_item_signature_queries(
    query::QueryContext& context, IdeSnapshot& snapshot, const IdeIncrementalSnapshotInput& incremental)
{
    for (const auto& entry : snapshot.checked.functions) {
        evaluate_function_item_signature_query(context, snapshot, entry.second, incremental);
    }
    for (const auto& entry : snapshot.checked.structs) {
        evaluate_struct_item_signature_query(context, snapshot, entry.second, incremental);
    }
    for (const auto& entry : snapshot.checked.enum_cases) {
        evaluate_enum_case_item_signature_query(context, snapshot, entry.second, incremental);
    }
    for (const auto& entry : snapshot.checked.type_aliases) {
        evaluate_type_alias_item_signature_query(context, snapshot, entry.second, incremental);
    }
    for (const sema::GenericTemplateSignatureInfo& info : snapshot.checked.generic_template_signatures) {
        evaluate_generic_template_signature_query(context, snapshot, info, incremental);
    }
}

[[nodiscard]] std::optional<query::FunctionBodySyntaxAuthority> function_body_syntax_authority(
    const IdeSnapshot& snapshot, const query::BodyKey key, const sema::FunctionSignature& signature)
{
    const std::optional<base::SourceRange> body_range = function_signature_body_range(snapshot, signature);
    if (!body_range.has_value()) {
        return std::nullopt;
    }
    const std::optional<std::string_view> body_text = source_range_text(snapshot.sources, *body_range);
    if (!body_text.has_value()) {
        return std::nullopt;
    }
    return query::FunctionBodySyntaxAuthority{
        function_body_syntax_content_fingerprint(key, *body_text),
        key.owner,
        ide_module_part_key_for_part_index(snapshot, signature.part_index),
        static_cast<base::u64>(body_range->begin),
        static_cast<base::u64>(body_range->end),
        key.slot,
        key.ordinal,
    };
}

[[nodiscard]] query::TypeCheckBodyAuthority type_check_body_authority(const query::BodyKey key,
    const query::QueryResultFingerprint body_syntax_result,
    const query::QueryResultFingerprint signature_result) noexcept
{
    query::TypeCheckBodyAuthority authority;
    authority.checked_body = type_check_body_checked_result_fingerprint(key, body_syntax_result, signature_result);
    authority.body_syntax_result = body_syntax_result;
    authority.signature_result = signature_result;
    return authority;
}

void push_function_body_syntax_fact(query::QueryContext& context, IdeSnapshot& snapshot, const query::BodyKey key,
    const query::FunctionBodySyntaxAuthority& authority, const sema::FunctionSignature& signature,
    const base::SourceRange range, const IdeIncrementalSnapshotInput& incremental)
{
    if (!query::is_valid(key) || !query::is_valid(authority)) {
        return;
    }
    seed_reusable_query_record(context, incremental,
        query::function_body_syntax_query_record(key, query::function_body_syntax_result_fingerprint(authority)));
    const query::QueryEvaluationResult result =
        context.evaluate_function_body_syntax(query::FunctionBodySyntaxProviderInput{
            key,
            authority,
        });
    if (!query_evaluation_completed(result)) {
        return;
    }
    const std::optional<query::QueryKey> query_key = query::function_body_syntax_query_key(key);
    if (!query_key.has_value()) {
        return;
    }
    IdeSemanticFact fact;
    fact.kind = IdeSemanticFactKind::function_body_syntax;
    fact.query = *query_key;
    fact.definition = key.owner;
    fact.body = key;
    fact.range = range;
    fact.name = std::string(signature.name.view());
    fact.detail = std::string(IDE_SEMANTIC_FACT_FUNCTION_BODY_SYNTAX);
    fact.part_index = signature.part_index;
    fact.generic_instance = signature.generic_instance_key;
    fact.checked = true;
    snapshot.query.semantic_facts.push_back(std::move(fact));
}

void push_type_check_body_fact(query::QueryContext& context, IdeSnapshot& snapshot, const query::BodyKey key,
    const query::TypeCheckBodyAuthority& authority, const sema::FunctionSignature& signature,
    const base::SourceRange range, const IdeIncrementalSnapshotInput& incremental)
{
    if (!query::is_valid(key) || !query::is_valid(authority)) {
        return;
    }
    seed_reusable_query_record(context, incremental,
        query::type_check_body_query_record(key, query::type_check_body_result_fingerprint(authority)));
    const query::QueryEvaluationResult result = context.evaluate_type_check_body(query::TypeCheckBodyProviderInput{
        key,
        authority,
    });
    if (!query_evaluation_completed(result)) {
        return;
    }
    const std::optional<query::QueryKey> query_key = query::type_check_body_query_key(key);
    if (!query_key.has_value()) {
        return;
    }
    IdeSemanticFact fact;
    fact.kind = IdeSemanticFactKind::type_check_body;
    fact.query = *query_key;
    fact.definition = key.owner;
    fact.body = key;
    fact.range = range;
    fact.name = std::string(signature.name.view());
    fact.detail = std::string(IDE_SEMANTIC_FACT_TYPE_CHECK_BODY);
    fact.part_index = signature.part_index;
    fact.generic_instance = signature.generic_instance_key;
    fact.checked = true;
    snapshot.query.semantic_facts.push_back(std::move(fact));
}

void evaluate_function_body_queries(query::QueryContext& context, IdeSnapshot& snapshot,
    const sema::FunctionSignature& signature, const IdeIncrementalSnapshotInput& incremental)
{
    if (!signature.has_definition || signature.has_conflict || !query::is_valid(signature.stable_id)
        || !query::is_valid(signature.incremental_key)) {
        return;
    }
    const query::DefKey def = function_signature_def_key(snapshot, signature);
    const query::BodyKey body = query::body_key(def, query::BodySlotKind::function_body);
    if (!query::is_valid(body)) {
        return;
    }
    const std::optional<base::SourceRange> range = function_signature_body_range(snapshot, signature);
    if (!range.has_value()) {
        return;
    }
    const std::optional<query::FunctionBodySyntaxAuthority> syntax_authority =
        function_body_syntax_authority(snapshot, body, signature);
    if (!syntax_authority.has_value() || !query::is_valid(*syntax_authority)) {
        return;
    }
    push_function_body_syntax_fact(context, snapshot, body, *syntax_authority, signature, *range, incremental);

    const query::ItemSignatureAuthority signature_authority = item_signature_authority(snapshot, signature);
    const query::QueryResultFingerprint signature_result =
        query::item_signature_result_fingerprint(signature_authority);
    const query::QueryResultFingerprint syntax_result =
        query::function_body_syntax_result_fingerprint(*syntax_authority);
    const query::TypeCheckBodyAuthority type_check_authority =
        type_check_body_authority(body, syntax_result, signature_result);
    push_type_check_body_fact(context, snapshot, body, type_check_authority, signature, *range, incremental);
}

void evaluate_checked_function_body_queries(
    query::QueryContext& context, IdeSnapshot& snapshot, const IdeIncrementalSnapshotInput& incremental)
{
    for (const auto& entry : snapshot.checked.functions) {
        evaluate_function_body_queries(context, snapshot, entry.second, incremental);
    }
}

void evaluate_module_query_surface(
    query::QueryContext& context, const IdeSnapshot& snapshot, const IdeIncrementalSnapshotInput& incremental)
{
    if (!snapshot.parsed || !snapshot.source_part.valid || !query::is_valid(snapshot.source_part.part_key)) {
        return;
    }
    const query::ModuleKey module = module_key_for_snapshot(snapshot);
    const query::QueryResultFingerprint graph_result = ide_module_graph_result_fingerprint(snapshot);
    const query::QueryResultFingerprint part_result = ide_module_part_result_fingerprint(snapshot);
    const query::QueryResultFingerprint item_list_result = ide_item_list_result_fingerprint(snapshot);
    const query::QueryResultFingerprint exports_result =
        ide_module_exports_result_fingerprint(snapshot, item_list_result);

    seed_reusable_query_record(context, incremental, query::module_graph_query_record(module, graph_result));
    static_cast<void>(context.evaluate_module_graph(query::ModuleGraphProviderInput{
        module,
        graph_result,
        {},
    }));
    seed_reusable_query_record(
        context, incremental, query::module_part_query_record(snapshot.source_part.part_key, part_result));
    static_cast<void>(context.evaluate_module_part(query::ModulePartProviderInput{
        snapshot.source_part.part_key,
        part_result,
    }));
    seed_reusable_query_record(context, incremental, query::item_list_query_record(module, item_list_result));
    static_cast<void>(context.evaluate_item_list(query::ItemListProviderInput{
        module,
        item_list_result,
    }));
    seed_reusable_query_record(context, incremental, query::module_exports_query_record(module, exports_result));
    static_cast<void>(context.evaluate_module_exports(query::ModuleExportsProviderInput{
        module,
        exports_result,
        {},
    }));
}

void append_semantic_query_surface(IdeSnapshot& snapshot, const query::QueryContext& context)
{
    std::vector<query::QueryRecord> records = context.completed_records();
    snapshot.query.records.insert(snapshot.query.records.end(), records.begin(), records.end());
    std::vector<query::QueryDependencyEdge> dependencies = context.dependency_edges();
    snapshot.query.dependencies.insert(snapshot.query.dependencies.end(), dependencies.begin(), dependencies.end());
}

void evaluate_semantic_queries(IdeSnapshot& snapshot, const IdeIncrementalSnapshotInput& incremental)
{
    query::QueryContext context;
    evaluate_module_query_surface(context, snapshot, incremental);
    if (snapshot.checked_semantics) {
        evaluate_checked_item_signature_queries(context, snapshot, incremental);
        evaluate_checked_function_body_queries(context, snapshot, incremental);
    }
    append_semantic_query_surface(snapshot, context);
}

void push_ast_global_symbol(const IdeSnapshot& snapshot, SymbolIndex& index, const syntax::ItemNode& item)
{
    const std::optional<ItemDefinitionMetadata> metadata = item_definition_metadata(item.kind);
    if (!metadata.has_value() || item.name.empty()) {
        return;
    }
    const std::array<std::string_view, 1> path{item.name};
    push_global_symbol(index,
        IdeSymbol{
            query::def_key(module_key_for_snapshot(snapshot), metadata->namespace_, metadata->kind, path),
            definition_name_range(snapshot.lossless, item),
            item.range,
            std::string(item.name),
            std::string(metadata->label),
            std::string(metadata->label) + " " + std::string(item.name),
            IDE_PRIMARY_PART_INDEX,
            false,
            false,
        });
}

[[nodiscard]] std::optional<sema::TypeHandle> checked_stmt_local_type(
    const IdeSnapshot& snapshot, const syntax::StmtId stmt) noexcept
{
    if (!syntax::is_valid(stmt) || stmt.value >= snapshot.checked.stmt_local_types.size()) {
        return std::nullopt;
    }
    const sema::TypeHandle type = snapshot.checked.stmt_local_types[stmt.value];
    return sema::is_valid(type) ? std::optional<sema::TypeHandle>{type} : std::nullopt;
}

[[nodiscard]] std::optional<sema::TypeHandle> checked_syntax_type(
    const IdeSnapshot& snapshot, const syntax::TypeId type) noexcept
{
    if (!syntax::is_valid(type) || type.value >= snapshot.checked.syntax_type_handles.size()) {
        return std::nullopt;
    }
    const sema::TypeHandle handle = snapshot.checked.syntax_type_handles[type.value];
    return sema::is_valid(handle) ? std::optional<sema::TypeHandle>{handle} : std::nullopt;
}

void push_parameter_symbols(const IdeSnapshot& snapshot, SymbolIndex& index, const syntax::ItemNode& function)
{
    for (const syntax::ParamDecl& param : function.params) {
        const base::SourceRange range = name_range_in_range(snapshot.lossless, param.name, param.range);
        std::string detail = std::string(IDE_SYMBOL_KIND_PARAMETER) + " " + std::string(param.name);
        if (const std::optional<sema::TypeHandle> type = checked_syntax_type(snapshot, param.type)) {
            detail += ": " + snapshot.checked.types.display_name(*type);
        }
        push_local_symbol(index,
            IdeSymbol{
                symbol_def_key(snapshot, {}, query::DefNamespace::value, query::DefKind::value, param.name, range),
                range,
                function.range,
                std::string(param.name),
                std::string(IDE_SYMBOL_KIND_PARAMETER),
                std::move(detail),
                IDE_PRIMARY_PART_INDEX,
                true,
                snapshot.checked_semantics,
            });
    }
}

void push_local_stmt_symbol(const IdeSnapshot& snapshot, SymbolIndex& index, const syntax::StmtNode& stmt,
    const syntax::StmtId stmt_id, const base::SourceRange scope_range)
{
    if (stmt.name.empty()) {
        return;
    }
    const base::SourceRange range = name_range_in_range(snapshot.lossless, stmt.name, stmt.range);
    std::string detail = std::string(IDE_SYMBOL_KIND_LOCAL) + " " + std::string(stmt.name);
    if (const std::optional<sema::TypeHandle> type = checked_stmt_local_type(snapshot, stmt_id)) {
        detail += ": " + snapshot.checked.types.display_name(*type);
    }
    push_local_symbol(index,
        IdeSymbol{
            symbol_def_key(snapshot, {}, query::DefNamespace::value, query::DefKind::value, stmt.name, range),
            range,
            scope_range,
            std::string(stmt.name),
            std::string(IDE_SYMBOL_KIND_LOCAL),
            std::move(detail),
            IDE_PRIMARY_PART_INDEX,
            true,
            snapshot.checked_semantics,
        });
}

void collect_local_symbols_from_function(
    const IdeSnapshot& snapshot, SymbolIndex& index, const syntax::ItemNode& function)
{
    if (!syntax::is_valid(function.body)) {
        return;
    }
    push_parameter_symbols(snapshot, index, function);

    std::vector<syntax::StmtId> stack;
    stack.push_back(function.body);
    while (!stack.empty()) {
        const syntax::StmtId stmt_id = stack.back();
        stack.pop_back();
        if (!syntax::is_valid(stmt_id) || stmt_id.value >= snapshot.ast.stmts.size()) {
            continue;
        }
        const syntax::StmtNode stmt = snapshot.ast.stmts[stmt_id.value];
        if (stmt.kind == syntax::StmtKind::let || stmt.kind == syntax::StmtKind::var
            || stmt.kind == syntax::StmtKind::for_range) {
            push_local_stmt_symbol(snapshot, index, stmt, stmt_id, function.range);
        }
        if (const syntax::AstArenaVector<syntax::StmtId>* statements =
                snapshot.ast.stmts.block_statements(stmt_id.value)) {
            for (base::usize statement_index = statements->size(); statement_index > 0U; --statement_index) {
                stack.push_back((*statements)[statement_index - 1U]);
            }
        }
        const std::array<syntax::StmtId, 6> children{
            stmt.then_block,
            stmt.else_block,
            stmt.else_if,
            stmt.body,
            stmt.for_init,
            stmt.for_update,
        };
        for (const syntax::StmtId child : children) {
            if (syntax::is_valid(child)) {
                stack.push_back(child);
            }
        }
    }
}

[[nodiscard]] SymbolIndex build_symbol_index(const IdeSnapshot& snapshot)
{
    SymbolIndex index;
    if (!snapshot.parsed) {
        return index;
    }
    if (snapshot.checked_semantics) {
        push_checked_global_symbols(snapshot, index);
    }
    for (base::usize item_index = 0; item_index < snapshot.ast.items.size(); ++item_index) {
        const syntax::ItemNode* const item = snapshot.ast.items.ptr(item_index);
        if (item != nullptr) {
            push_ast_global_symbol(snapshot, index, *item);
        }
    }
    for (base::usize item_index = 0; item_index < snapshot.ast.items.size(); ++item_index) {
        const syntax::ItemNode* const item = snapshot.ast.items.ptr(item_index);
        if (item != nullptr && item->kind == syntax::ItemKind::fn_decl) {
            collect_local_symbols_from_function(snapshot, index, *item);
        }
    }
    return index;
}

[[nodiscard]] const IdeSymbol* best_global_symbol(const SymbolIndex& index, const std::string_view name)
{
    const auto found = index.globals.find(std::string(name));
    if (found == index.globals.end()) {
        return nullptr;
    }
    for (const base::usize symbol_index : found->second) {
        if (symbol_index < index.symbols.size()) {
            return &index.symbols[symbol_index];
        }
    }
    return nullptr;
}

[[nodiscard]] const IdeSymbol* best_local_symbol(
    const SymbolIndex& index, const std::string_view name, const base::usize offset)
{
    const auto found = index.locals.find(std::string(name));
    if (found == index.locals.end()) {
        return nullptr;
    }
    const IdeSymbol* best = nullptr;
    for (const base::usize symbol_index : found->second) {
        if (symbol_index >= index.symbols.size()) {
            continue;
        }
        const IdeSymbol& symbol = index.symbols[symbol_index];
        if (!range_contains_offset(symbol.scope_range, offset) || symbol.range.begin > offset) {
            continue;
        }
        if (best == nullptr || symbol.range.begin >= best->range.begin) {
            best = &symbol;
        }
    }
    return best;
}

[[nodiscard]] const IdeSymbol* best_symbol_for_identifier(
    const SymbolIndex& index, const std::string_view name, const base::usize offset)
{
    if (const IdeSymbol* local = best_local_symbol(index, name, offset); local != nullptr) {
        return local;
    }
    return best_global_symbol(index, name);
}

[[nodiscard]] IdeDefinition make_definition_from_symbol(const IdeSymbol& symbol)
{
    return IdeDefinition{
        symbol.key,
        symbol.member,
        symbol.generic_instance,
        symbol.range,
        symbol.name,
        symbol.kind,
        symbol.part_index,
        true,
    };
}

[[nodiscard]] std::optional<IdeTokenInfo> identifier_info_at_offset(
    const IdeSnapshot& snapshot, const base::usize offset)
{
    std::optional<IdeTokenInfo> info = token_info_at_offset(snapshot, offset);
    if (!info.has_value() || info->kind != syntax::TokenKind::identifier) {
        return std::nullopt;
    }
    return info;
}

[[nodiscard]] bool same_range(const base::SourceRange lhs, const base::SourceRange rhs) noexcept
{
    return lhs.source.value == rhs.source.value && lhs.begin == rhs.begin && lhs.end == rhs.end;
}

[[nodiscard]] base::usize clamped_edit_probe_offset(
    const std::string_view text, const base::usize begin, const base::usize removed_length) noexcept
{
    if (text.empty()) {
        return 0U;
    }
    if (removed_length == 0U) {
        return begin >= text.size() ? text.size() - 1U : begin;
    }
    return std::min(text.size(), begin + removed_length) - 1U;
}

[[nodiscard]] base::usize node_depth(const syntax::LosslessSyntaxTree& tree, syntax::LosslessNodeId node) noexcept
{
    base::usize depth = 0;
    while (node != syntax::INVALID_LOSSLESS_NODE_ID) {
        const syntax::LosslessNodeId parent = tree.parent(node);
        if (parent == syntax::INVALID_LOSSLESS_NODE_ID) {
            return depth;
        }
        node = parent;
        ++depth;
    }
    return depth;
}

[[nodiscard]] syntax::LosslessNodeId parent_or_invalid(
    const syntax::LosslessSyntaxTree& tree, const syntax::LosslessNodeId node) noexcept
{
    return tree.parent(node);
}

[[nodiscard]] syntax::LosslessNodeId common_ancestor(
    const syntax::LosslessSyntaxTree& tree, syntax::LosslessNodeId lhs, syntax::LosslessNodeId rhs) noexcept
{
    base::usize lhs_depth = node_depth(tree, lhs);
    base::usize rhs_depth = node_depth(tree, rhs);
    while (lhs_depth > rhs_depth) {
        lhs = parent_or_invalid(tree, lhs);
        --lhs_depth;
    }
    while (rhs_depth > lhs_depth) {
        rhs = parent_or_invalid(tree, rhs);
        --rhs_depth;
    }
    while (lhs != rhs) {
        lhs = parent_or_invalid(tree, lhs);
        rhs = parent_or_invalid(tree, rhs);
        if (lhs == syntax::INVALID_LOSSLESS_NODE_ID || rhs == syntax::INVALID_LOSSLESS_NODE_ID) {
            return tree.root_id();
        }
    }
    return lhs;
}

} // namespace

IdeSnapshot build_ide_snapshot(const IdeSnapshotRequest& request, const IdeIncrementalSnapshotInput incremental)
{
    IdeSnapshot snapshot;
    build_ide_snapshot_into(snapshot, request, incremental);
    return snapshot;
}

void build_ide_snapshot_into(
    IdeSnapshot& snapshot, const IdeSnapshotRequest& request, const IdeIncrementalSnapshotInput incremental)
{
    snapshot = IdeSnapshot{};
    snapshot.source_id = snapshot.sources.add_source(request.path, request.text);
    const query::FileKey file = ide_file_key(request);
    if (const std::optional<query::QuerySourceStageKeys> keys =
            query::query_source_stage_keys(file, query::QuerySourceStageMode::lossless_tooling)) {
        snapshot.query.source_stage = *keys;
    }

    base::DiagnosticSink diagnostics;
    lex::LexerOptions lexer_options;
    lexer_options.emit_trivia_tokens = true;
    lex::Lexer lexer(snapshot.source_id, snapshot.sources.text(snapshot.source_id), diagnostics, lexer_options);
    auto token_result = lexer.tokenize();
    std::vector<syntax::Token> tokens;
    query::QueryResultFingerprint lex_result = {};
    query::QueryResultFingerprint parse_result = {};
    if (token_result) {
        snapshot.lexed = true;
        tokens.assign(token_result.value().begin(), token_result.value().end());
        lex_result = lex_result_fingerprint(tokens, diagnostics.diagnostics());
        snapshot.lossless = syntax::build_lossless_syntax_tree(tokens);
        auto parsed = parse::lower_lossless_syntax_to_ast(snapshot.lossless, diagnostics);
        if (parsed) {
            snapshot.parsed = true;
            snapshot.ast = parsed.take_value();
            parse_result = parse_result_fingerprint(snapshot.lossless, snapshot.parsed, diagnostics.diagnostics());
            sema::SemanticAnalyzer analyzer(snapshot.ast, diagnostics);
            auto checked = analyzer.analyze();
            if (checked) {
                snapshot.checked_semantics = true;
                snapshot.checked = checked.take_value();
            }
        } else {
            parse_result = parse_result_fingerprint(snapshot.lossless, snapshot.parsed, diagnostics.diagnostics());
        }
    } else {
        lex_result = lex_result_fingerprint(tokens, diagnostics.diagnostics());
        snapshot.lossless = syntax::build_lossless_syntax_tree(std::span<const syntax::Token>{});
        query::StableHashBuilder builder;
        builder.mix_string(IDE_PARSE_RESULT_MARKER);
        builder.mix_string(IDE_PARSE_SKIPPED_MARKER);
        parse_result = query::query_result_fingerprint(builder.finish());
    }
    const query::DiagnosticsEventStream diagnostic_stream =
        query::diagnostic_events_from_sink(diagnostics.diagnostics());
    const query::QueryResultFingerprint diagnostic_result =
        diagnostics_result_fingerprint(diagnostic_stream.events, snapshot.parsed, snapshot.checked_semantics);
    snapshot.source_part = source_part_context_for_snapshot(snapshot, request);
    evaluate_source_queries(
        snapshot, request.text, lex_result, parse_result, diagnostic_result, diagnostic_stream, incremental);
    evaluate_semantic_queries(snapshot, incremental);
    snapshot.has_errors = diagnostics.has_error();
    snapshot.diagnostics = collect_ide_diagnostics(snapshot.sources, diagnostic_stream, snapshot.source_part);
}

std::optional<IdeTokenInfo> token_info_at_offset(const IdeSnapshot& snapshot, const base::usize offset)
{
    const syntax::Token* token = token_at_offset(snapshot, offset);
    if (token == nullptr || !token_contains_offset(*token, offset)) {
        return std::nullopt;
    }
    return make_token_info(snapshot, *token);
}

std::optional<IdeDefinition> definition_at_offset(const IdeSnapshot& snapshot, const base::usize offset)
{
    const std::optional<IdeTokenInfo> info = identifier_info_at_offset(snapshot, offset);
    if (!info.has_value()) {
        return std::nullopt;
    }
    const SymbolIndex index = build_symbol_index(snapshot);
    const IdeSymbol* const symbol = best_symbol_for_identifier(index, info->text, offset);
    return symbol == nullptr ? std::nullopt : std::optional<IdeDefinition>{make_definition_from_symbol(*symbol)};
}

std::vector<IdeReference> references_at_offset(const IdeSnapshot& snapshot, const base::usize offset)
{
    const std::optional<IdeTokenInfo> info = identifier_info_at_offset(snapshot, offset);
    if (!info.has_value()) {
        return {};
    }

    const SymbolIndex index = build_symbol_index(snapshot);
    const IdeSymbol* const symbol = best_symbol_for_identifier(index, info->text, offset);
    std::vector<IdeReference> references;
    for (const syntax::Token& token : snapshot.lossless.tokens()) {
        if (token.kind != syntax::TokenKind::identifier || token.text() != info->text) {
            continue;
        }
        if (symbol != nullptr && symbol->local && !range_contains_range(symbol->scope_range, token.range)) {
            continue;
        }
        if (symbol == nullptr || best_symbol_for_identifier(index, token.text(), token.range.begin) == symbol) {
            references.push_back(IdeReference{
                token.range,
                info->text,
                symbol != nullptr && same_range(token.range, symbol->range),
            });
        }
    }
    return references;
}

std::optional<IdeHoverInfo> hover_at_offset(const IdeSnapshot& snapshot, const base::usize offset)
{
    std::optional<IdeTokenInfo> info = token_info_at_offset(snapshot, offset);
    if (!info.has_value()) {
        return std::nullopt;
    }

    IdeHoverInfo hover;
    hover.range = info->range;
    hover.valid = true;
    if (info->kind == syntax::TokenKind::identifier) {
        const SymbolIndex index = build_symbol_index(snapshot);
        const IdeSymbol* const symbol = best_symbol_for_identifier(index, info->text, offset);
        std::ostringstream label;
        label << IDE_HOVER_IDENTIFIER_PREFIX << '`' << info->text << '`';
        if (symbol != nullptr) {
            label << " -> " << symbol->kind;
            if (!symbol->detail.empty()) {
                label << " (" << symbol->detail << ")";
            }
            hover.definition = make_definition_from_symbol(*symbol);
        }
        hover.label = label.str();
        return hover;
    }

    std::ostringstream label;
    label << IDE_HOVER_TOKEN_PREFIX << syntax::token_kind_name(info->kind);
    hover.label = label.str();
    return hover;
}

std::optional<IdeAstNodeInfo> ast_node_at_offset(const IdeSnapshot& snapshot, const base::usize offset)
{
    if (!snapshot.parsed) {
        return std::nullopt;
    }

    for (base::usize index = 0; index < snapshot.ast.items.size(); ++index) {
        const syntax::ItemNode* const item = snapshot.ast.items.ptr(index);
        if (item == nullptr || !range_contains_offset(item->range, offset)) {
            continue;
        }

        const syntax::ItemId item_id{static_cast<base::u32>(index)};
        const query::DefKey definition = ast_item_def_key(snapshot, *item);
        const base::u32 part_index = ide_item_part_index(snapshot, index);
        if (const std::optional<base::SourceRange> body_range = ast_item_body_range(snapshot, *item);
            body_range.has_value() && range_contains_offset(*body_range, offset)) {
            const query::BodyKey body = query::body_key(definition, query::BodySlotKind::function_body);
            return IdeAstNodeInfo{
                IdeAstNodeKind::function_body,
                item_id,
                item->body,
                definition,
                body,
                *body_range,
                std::string(item->name),
                std::string(IDE_SEMANTIC_FACT_FUNCTION_BODY_SYNTAX),
                part_index,
                query::is_valid(definition) && query::is_valid(body),
            };
        }

        const std::optional<ItemDefinitionMetadata> metadata = item_definition_metadata(item->kind);
        return IdeAstNodeInfo{
            IdeAstNodeKind::item,
            item_id,
            syntax::INVALID_STMT_ID,
            definition,
            {},
            item->range,
            std::string(item->name),
            metadata.has_value() ? std::string(metadata->label) : std::string("item"),
            part_index,
            query::is_valid(definition),
        };
    }
    return std::nullopt;
}

IdeEditImpact edit_impact_for_range(
    const IdeSnapshot& snapshot, const base::usize begin, const base::usize removed_length)
{
    const base::SourceFile& file = snapshot.sources.get(snapshot.source_id);
    const std::string_view text = file.text();
    syntax::LosslessNodeId begin_node = snapshot.lossless.node_at_offset(clamped_edit_probe_offset(text, begin, 0U));
    syntax::LosslessNodeId end_node =
        snapshot.lossless.node_at_offset(clamped_edit_probe_offset(text, begin, removed_length));

    const syntax::LosslessNodeId node = common_ancestor(snapshot.lossless, begin_node, end_node);
    const syntax::LosslessNode& current = *snapshot.lossless.node(node);
    return IdeEditImpact{
        node,
        current.range,
        snapshot.lossless.node_key(node),
        current.first_token,
        current.token_count,
        true,
    };
}

} // namespace aurex::tooling
