#include <aurex/driver/pipeline_stage.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/parse/lossless_parse.hpp>
#include <aurex/parse/parser.hpp>
#include <aurex/query/diagnostics_query.hpp>
#include <aurex/query/stable_hash.hpp>
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
constexpr std::string_view IDE_PARSE_SKIPPED_MARKER = "parse-skipped";
constexpr std::string_view IDE_HOVER_IDENTIFIER_PREFIX = "identifier ";
constexpr std::string_view IDE_HOVER_TOKEN_PREFIX = "token ";
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
    query::DefKey key;
    base::SourceRange range{};
    base::SourceRange scope_range{};
    std::string name;
    std::string kind;
    std::string detail;
    base::u32 part_index = 0;
    bool local = false;
    bool checked = false;
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

void evaluate_source_queries(IdeSnapshot& snapshot, const std::string_view source_text,
    const query::QueryResultFingerprint lex_result, const query::QueryResultFingerprint parse_result,
    const query::QueryResultFingerprint diagnostics_result, const query::DiagnosticsEventStream& diagnostics)
{
    query::QueryContext context;
    const query::QueryResultFingerprint content_result = content_result_fingerprint(source_text);
    static_cast<void>(context.evaluate_file_content(query::FileContentProviderInput{
        snapshot.query.source_stage.file,
        content_result,
    }));
    static_cast<void>(context.evaluate_lex_file(query::LexFileProviderInput{
        snapshot.query.source_stage.lex_file,
        lex_result,
    }));
    static_cast<void>(context.evaluate_parse_file(query::ParseFileProviderInput{
        snapshot.query.source_stage.parse_file,
        parse_result,
    }));
    if (const std::optional<query::QueryKey> parse_query =
            query::parse_file_query_key(snapshot.query.source_stage.parse_file)) {
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
    return query::module_key(snapshot.query.source_stage.file.package, parts);
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
    switch (kind) {
        case query::StableSymbolKind::function:
            return query::DefKind::function;
        case query::StableSymbolKind::method:
            return query::DefKind::method;
        case query::StableSymbolKind::value:
            return query::DefKind::value;
        case query::StableSymbolKind::type:
            return query::DefKind::struct_;
        case query::StableSymbolKind::enum_case:
            return query::DefKind::enum_case;
        case query::StableSymbolKind::struct_field:
            return query::DefKind::struct_field;
        case query::StableSymbolKind::generic_template:
            return query::DefKind::generic_template;
        case query::StableSymbolKind::synthetic:
            return query::DefKind::synthetic;
        case query::StableSymbolKind::invalid:
            return query::DefKind::invalid;
    }
    return query::DefKind::invalid;
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
            });
        for (const sema::StructFieldInfo& field : info.fields) {
            const query::DefKind field_kind = symbol_kind_to_def_kind(field.stable_key.kind);
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
                });
        }
    }
    for (const auto& entry : snapshot.checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        const base::SourceRange range = name_range_in_range(snapshot.lossless, info.case_name.view(), info.range);
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

IdeSnapshot build_ide_snapshot(const IdeSnapshotRequest& request)
{
    IdeSnapshot snapshot;
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
    evaluate_source_queries(snapshot, request.text, lex_result, parse_result, diagnostic_result, diagnostic_stream);
    snapshot.source_part = source_part_context_for_snapshot(snapshot, request);
    snapshot.has_errors = diagnostics.has_error();
    snapshot.diagnostics = collect_ide_diagnostics(snapshot.sources, diagnostic_stream, snapshot.source_part);
    return snapshot;
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
