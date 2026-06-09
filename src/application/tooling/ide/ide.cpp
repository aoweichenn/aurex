#include <aurex/application/tooling/ide.hpp>
#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/lossless_parse.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/function.hpp>
#include <aurex/frontend/sema/resource_semantics.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/frontend/syntax/core/module.hpp>
#include <aurex/infrastructure/pipeline/stage.hpp>
#include <aurex/infrastructure/query/diagnostics_query.hpp>
#include <aurex/infrastructure/query/function_body_syntax_query.hpp>
#include <aurex/infrastructure/query/generic_template_signature_query.hpp>
#include <aurex/infrastructure/query/item_list_query.hpp>
#include <aurex/infrastructure/query/item_signature_query.hpp>
#include <aurex/infrastructure/query/lower_function_ir_query.hpp>
#include <aurex/infrastructure/query/module_exports_query.hpp>
#include <aurex/infrastructure/query/module_graph_query.hpp>
#include <aurex/infrastructure/query/module_part_query.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#if defined(AUREX_TOOLING_ENABLE_IR_FACTS)
#include <aurex/midend/ir/ir_cleanup_marker_facts.hpp>
#include <aurex/midend/ir/ir_dyn_abi_facts.hpp>
#include <aurex/midend/ir/lower_ast.hpp>
#endif

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
constexpr std::string_view IDE_LOWER_FUNCTION_IR_RESULT_MARKER = "lower-function-ir:v1";
constexpr std::string_view IDE_PARSE_SKIPPED_MARKER = "parse-skipped";
constexpr std::string_view IDE_HOVER_IDENTIFIER_PREFIX = "identifier ";
constexpr std::string_view IDE_HOVER_TOKEN_PREFIX = "token ";
constexpr std::string_view IDE_SEMANTIC_FACT_ITEM_SIGNATURE = "item_signature";
constexpr std::string_view IDE_SEMANTIC_FACT_GENERIC_TEMPLATE_SIGNATURE = "generic_template_signature";
constexpr std::string_view IDE_SEMANTIC_FACT_FUNCTION_BODY_SYNTAX = "function_body_syntax";
constexpr std::string_view IDE_SEMANTIC_FACT_TYPE_CHECK_BODY = "type_check_body";
constexpr std::string_view IDE_SEMANTIC_FACT_BORROW_SUMMARY = "borrow_summary";
constexpr std::string_view IDE_SEMANTIC_FACT_BORROW_CONTRACT = "borrow_contract";
constexpr std::string_view IDE_SEMANTIC_FACT_LIFETIME_FACTS = "lifetime_facts";
constexpr std::string_view IDE_SEMANTIC_FACT_DROPCK_FACTS = "dropck_facts";
constexpr std::string_view IDE_SEMANTIC_FACT_PLACE_STATE = "place_state";
constexpr std::string_view IDE_SEMANTIC_FACT_BODY_LOAN_CHECK = "body_loan_check";
constexpr std::string_view IDE_SEMANTIC_FACT_DYN_ABI_FACTS = "dyn_abi_facts";
constexpr std::string_view IDE_SYMBOL_KIND_CONST = "const";
constexpr std::string_view IDE_SYMBOL_KIND_ENUM_CASE = "enum_case";
constexpr std::string_view IDE_SYMBOL_KIND_ENUM = "enum";
constexpr std::string_view IDE_SYMBOL_KIND_FUNCTION = "function";
constexpr std::string_view IDE_SYMBOL_KIND_GENERIC_TEMPLATE = "generic_template";
constexpr std::string_view IDE_SYMBOL_KIND_IMPL_METHOD = "impl_method";
constexpr std::string_view IDE_SYMBOL_KIND_LOCAL = "local";
constexpr std::string_view IDE_SYMBOL_KIND_METHOD = "method";
constexpr std::string_view IDE_SYMBOL_KIND_OPAQUE_STRUCT = "opaque_struct";
constexpr std::string_view IDE_SYMBOL_KIND_PARAMETER = "parameter";
constexpr std::string_view IDE_SYMBOL_KIND_STRUCT = "struct";
constexpr std::string_view IDE_SYMBOL_KIND_STRUCT_FIELD = "struct_field";
constexpr std::string_view IDE_SYMBOL_KIND_TRAIT = "trait";
constexpr std::string_view IDE_SYMBOL_KIND_TRAIT_METHOD = "trait_method";
constexpr std::string_view IDE_SYMBOL_KIND_ASSOCIATED_TYPE = "associated_type";
constexpr std::string_view IDE_SYMBOL_KIND_TYPE_ALIAS = "type_alias";
constexpr std::string_view IDE_COMPLETION_KIND_KEYWORD = "keyword";
constexpr std::string_view IDE_COMPLETION_DETAIL_KEYWORD = "keyword";
constexpr std::string_view IDE_TOKEN_TYPE_COMMENT = "comment";
constexpr std::string_view IDE_TOKEN_TYPE_ENUM = "enum";
constexpr std::string_view IDE_TOKEN_TYPE_ENUM_MEMBER = "enumMember";
constexpr std::string_view IDE_TOKEN_TYPE_FUNCTION = "function";
constexpr std::string_view IDE_TOKEN_TYPE_INTERFACE = "interface";
constexpr std::string_view IDE_TOKEN_TYPE_KEYWORD = "keyword";
constexpr std::string_view IDE_TOKEN_TYPE_METHOD = "method";
constexpr std::string_view IDE_TOKEN_TYPE_NUMBER = "number";
constexpr std::string_view IDE_TOKEN_TYPE_OPERATOR = "operator";
constexpr std::string_view IDE_TOKEN_TYPE_PARAMETER = "parameter";
constexpr std::string_view IDE_TOKEN_TYPE_PROPERTY = "property";
constexpr std::string_view IDE_TOKEN_TYPE_PUNCTUATION = "punctuation";
constexpr std::string_view IDE_TOKEN_TYPE_STRING = "string";
constexpr std::string_view IDE_TOKEN_TYPE_TYPE = "type";
constexpr std::string_view IDE_TOKEN_TYPE_TYPE_PARAMETER = "typeParameter";
constexpr std::string_view IDE_TOKEN_TYPE_VARIABLE = "variable";
constexpr std::string_view IDE_TOKEN_MODIFIER_DECLARATION = "declaration";
constexpr std::string_view IDE_TOKEN_MODIFIER_DEFINITION = "definition";
constexpr std::string_view IDE_TOKEN_MODIFIER_READONLY = "readonly";
constexpr std::string_view IDE_INLAY_HINT_KIND_TYPE = "type";
constexpr std::string_view IDE_DETAIL_TYPE_SEPARATOR = ": ";
constexpr std::string_view IDE_DETAIL_RESOURCE_SEPARATOR = " resource=";
constexpr std::string_view IDE_DETAIL_BORROW_SUMMARY_SEPARATOR = " borrow_summary=";
constexpr std::string_view IDE_DETAIL_BORROW_CONTRACT_SEPARATOR = " borrow_contract=";
constexpr std::string_view IDE_DETAIL_MOVE_REJECTION_SEPARATOR = " move_rejections=";
constexpr std::string_view IDE_DETAIL_LIFETIME_SEPARATOR = " lifetime=";
constexpr std::string_view IDE_DETAIL_PLACE_STATE_SEPARATOR = " place_state=";
constexpr std::string_view IDE_DETAIL_CLEANUP_MARKER_SEPARATOR = " cleanup_markers=";
constexpr std::string_view IDE_DETAIL_DYN_ABI_SEPARATOR = " dyn_abi=";
constexpr std::string_view IDE_DETAIL_DYN_DISPATCH_SEPARATOR = " dyn_dispatch=";
constexpr std::string_view IDE_PRIMARY_PART_NAME = "<primary>";
constexpr base::u32 IDE_PRIMARY_PART_INDEX = 0;
constexpr base::u32 IDE_FIRST_NAMED_PART_INDEX = 1;

constexpr std::array<std::string_view, 11> IDE_ITEM_COMPLETION_KEYWORDS{
    "fn",
    "struct",
    "enum",
    "trait",
    "type",
    "const",
    "impl",
    "extern",
    "import",
    "pub",
    "priv",
};

constexpr std::array<std::string_view, 24> IDE_EXPRESSION_COMPLETION_KEYWORDS{
    "let",
    "var",
    "return",
    "if",
    "else",
    "match",
    "while",
    "for",
    "in",
    "unsafe",
    "true",
    "false",
    "null",
    "sizeof",
    "alignof",
    "cast",
    "ptrcast",
    "bitcast",
    "ptraddr",
    "ptrat",
    "sliceptr",
    "slicelen",
    "strptr",
    "strblen",
};

struct ItemDefinitionMetadata {
    query::DefNamespace namespace_ = query::DefNamespace::value;
    query::DefKind kind = query::DefKind::invalid;
    std::string_view label;
};

struct IdeSymbol {
    IdeSymbol(query::DefKey symbol_key, base::SourceRange symbol_range, base::SourceRange symbol_scope_range,
        std::string symbol_name, std::string symbol_kind, std::string symbol_detail, base::u32 symbol_part_index,
        bool is_local, bool is_checked, query::MemberKey symbol_member = {},
        query::GenericInstanceKey symbol_generic_instance = {})
        : key(symbol_key), range(symbol_range), scope_range(symbol_scope_range), name(std::move(symbol_name)),
          kind(std::move(symbol_kind)), detail(std::move(symbol_detail)), part_index(symbol_part_index),
          local(is_local), checked(is_checked), member(symbol_member),
          generic_instance(std::move(symbol_generic_instance))
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
        const base::SourceFile* const file =
            diagnostic.range.well_formed() ? sources.try_get(diagnostic.range.source) : nullptr;
        if (file == nullptr) {
            continue;
        }
        result.push_back(IdeDiagnostic{
            diagnostic.severity,
            diagnostic.category,
            diagnostic.code,
            diagnostic.range,
            file->line_column(diagnostic.range.begin),
            file->line_column(diagnostic.range.end),
            std::string(file->path()),
            diagnostic.message,
            diagnostic.children,
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
        case syntax::ItemKind::trait_decl:
            return ItemDefinitionMetadata{query::DefNamespace::trait_, query::DefKind::trait_, "trait"};
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
        case syntax::ItemKind::trait_decl:
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
    if (kind == query::StableSymbolKind::method) {
        return query::MemberKind::trait_method;
    }
    if (kind == query::StableSymbolKind::type) {
        return query::MemberKind::associated_type;
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
    if (!range.well_formed()) {
        return std::nullopt;
    }
    const base::SourceFile* const file = sources.try_get(range.source);
    if (file == nullptr) {
        return std::nullopt;
    }
    const std::string_view text = file->text();
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

[[nodiscard]] query::BodySlotKind function_body_slot_kind(const sema::FunctionSignature& signature) noexcept
{
    return signature.is_trait_default_method_instance ? query::BodySlotKind::trait_default_method
                                                      : query::BodySlotKind::function_body;
}

[[nodiscard]] query::DefKey function_signature_def_key(
    const IdeSnapshot& snapshot, const sema::FunctionSignature& signature)
{
    return symbol_def_key(snapshot, signature.stable_id, query::DefNamespace::value,
        function_signature_def_kind(signature), signature.name.view(), signature.range);
}

[[nodiscard]] query::DefKey trait_signature_def_key(const IdeSnapshot& snapshot, const sema::TraitSignature& trait)
{
    return symbol_def_key(
        snapshot, trait.stable_id, query::DefNamespace::trait_, query::DefKind::trait_, trait.name.view(), trait.range);
}

[[nodiscard]] query::MemberKey trait_method_member_key(
    const IdeSnapshot& snapshot, const sema::TraitSignature& trait, const sema::TraitMethodRequirement& requirement)
{
    const query::DefKey owner = trait_signature_def_key(snapshot, trait);
    if (!query::is_valid(owner)) {
        return {};
    }
    const query::MemberKey member =
        query::member_key(owner, query::MemberKind::trait_method, requirement.name.view(), requirement.ordinal);
    return query::is_valid(member) ? member : query::MemberKey{};
}

[[nodiscard]] query::DefKey member_definition_key_from_member_key(const query::MemberKey member,
    const query::DefKind kind, const std::string_view fallback_name, const base::u32 ordinal) noexcept
{
    if (!query::is_valid(member.owner)) {
        return {};
    }
    const std::array<std::string_view, 1> path{fallback_name};
    return query::def_key(member.owner.module, query::DefNamespace::member, kind, path, ordinal);
}

[[nodiscard]] query::DefKey associated_type_definition_key(
    const IdeSnapshot& snapshot, const sema::TraitAssociatedTypeRequirement& associated_type)
{
    return symbol_def_key(snapshot, associated_type.stable_key.owner, query::DefNamespace::member,
        query::DefKind::associated_type, associated_type.name.view(), associated_type.range);
}

[[nodiscard]] query::DefKey associated_type_definition_key(const sema::TraitImplAssociatedTypeInfo& associated_type)
{
    return member_definition_key_from_member_key(associated_type.member_key, query::DefKind::associated_type,
        associated_type.name.view(), associated_type.requirement_ordinal);
}

[[nodiscard]] query::DefKey trait_method_definition_key(
    const IdeSnapshot& snapshot, const sema::TraitMethodRequirement& requirement)
{
    return symbol_def_key(snapshot, requirement.stable_key.owner, query::DefNamespace::member,
        query::DefKind::trait_method, requirement.name.view(), requirement.range);
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
    builder.mix_u64(snapshot.checked.traits.size());
    builder.mix_u64(snapshot.checked.trait_impls.size());
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

[[nodiscard]] query::QueryResultFingerprint lower_function_ir_input_fingerprint(const query::BodyKey key,
    const query::QueryResultFingerprint type_check_result, const query::FunctionCleanupMarkerFacts& cleanup,
    const query::FunctionDynAbiFacts& dyn_abi)
{
    query::StableHashBuilder builder;
    builder.mix_string(IDE_LOWER_FUNCTION_IR_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(type_check_result.global_id);
    builder.mix_fingerprint(type_check_result.fingerprint);
    builder.mix_string(cleanup.symbol);
    builder.mix_fingerprint(query::function_cleanup_marker_facts_fingerprint(cleanup));
    builder.mix_string(dyn_abi.symbol);
    builder.mix_fingerprint(query::function_dyn_abi_facts_fingerprint(dyn_abi));
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] std::string borrow_summary_detail(const sema::FunctionBorrowSummary& summary)
{
    std::ostringstream label;
    label << IDE_SEMANTIC_FACT_BORROW_SUMMARY << " origins=" << summary.origins.size()
          << " deps=" << summary.return_origins.size()
          << " storage_escapes=" << summary.storage_escapes.size()
          << " unknown=" << (summary.has_unknown_return_origin ? "true" : "false")
          << " local_escape=" << (summary.has_local_return_escape ? "true" : "false")
          << " fingerprint=" << query::debug_string(summary.fingerprint);
    return label.str();
}

[[nodiscard]] std::string borrow_contract_detail(const sema::FunctionBorrowContract& contract)
{
    std::ostringstream label;
    label << IDE_SEMANTIC_FACT_BORROW_CONTRACT
          << " source=" << sema::function_borrow_contract_source_name(contract.source)
          << " selectors=" << contract.return_selectors.size()
          << " unknown=" << (contract.unknown_return_allowed ? "true" : "false")
          << " local_escape=" << (contract.has_local_return_escape ? "true" : "false")
          << " mismatch=" << (contract.has_contract_mismatch ? "true" : "false")
          << " fingerprint=" << query::debug_string(sema::function_borrow_contract_fingerprint(contract));
    return label.str();
}

[[nodiscard]] std::string lifetime_facts_detail(const sema::FunctionLifetimeFacts& facts)
{
    const bool has_emitted_diagnostics =
        std::ranges::any_of(facts.violations, [](const sema::LifetimeViolation& violation) {
            return violation.diagnostic_emitted;
        });
    const base::usize local_escape_count =
        static_cast<base::usize>(std::ranges::count_if(facts.violations, [](const sema::LifetimeViolation& violation) {
            return violation.kind == sema::LifetimeViolationKind::local_escape;
        }));
    const base::usize unknown_escape_count =
        static_cast<base::usize>(std::ranges::count_if(facts.violations, [](const sema::LifetimeViolation& violation) {
            return violation.kind == sema::LifetimeViolationKind::unknown_escape;
        }));
    std::ostringstream label;
    label << IDE_SEMANTIC_FACT_LIFETIME_FACTS << " regions=" << facts.regions.size()
          << " outlives=" << facts.outlives_constraints.size()
          << " type_outlives=" << facts.type_outlives_constraints.size()
          << " live_ranges=" << facts.live_ranges.size() << " returns=" << facts.return_regions.size()
          << " violations=" << facts.violations.size()
          << " local_escapes=" << local_escape_count
          << " unknown_escapes=" << unknown_escape_count
          << " diagnostics=" << (has_emitted_diagnostics ? "true" : "false")
          << " fingerprint=" << query::debug_string(sema::function_lifetime_facts_fingerprint(facts));
    return label.str();
}

[[nodiscard]] std::string dropck_facts_detail(const sema::FunctionDropCheckFacts& facts)
{
    const bool has_emitted_diagnostics =
        std::ranges::any_of(facts.violations, [](const sema::DropCheckViolation& violation) {
            return violation.diagnostic_emitted;
        });
    base::u64 required_outlives_count = 0;
    for (const sema::DropCheckFact& fact : facts.facts) {
        required_outlives_count += static_cast<base::u64>(fact.required_outlives.size());
    }
    std::ostringstream label;
    label << IDE_SEMANTIC_FACT_DROPCK_FACTS << " facts=" << facts.facts.size()
          << " actions=" << facts.actions.size()
          << " required_outlives=" << required_outlives_count
          << " violations=" << facts.violations.size()
          << " diagnostics=" << (has_emitted_diagnostics ? "true" : "false")
          << " graph_missing=" << (facts.graph_missing ? "true" : "false")
          << " fingerprint=" << query::debug_string(sema::function_drop_check_facts_fingerprint(facts));
    return label.str();
}

[[nodiscard]] std::string place_state_detail(const sema::FunctionPlaceStateFacts& facts)
{
    const base::u64 partial_count =
        static_cast<base::u64>(std::ranges::count_if(facts.places, [](const sema::PlaceStateFact& fact) {
            return fact.has_partial_projection;
        }));
    const base::u64 move_place_count =
        static_cast<base::u64>(std::ranges::count_if(facts.places, [](const sema::PlaceStateFact& fact) {
            return fact.move_candidate_count != 0;
        }));
    const base::u64 drop_place_count =
        static_cast<base::u64>(std::ranges::count_if(facts.places, [](const sema::PlaceStateFact& fact) {
            return fact.drop_count != 0 || fact.cleanup_count != 0 || fact.drop_state == sema::PlaceStateDropState::dropped;
        }));
    const base::u64 borrow_event_count =
        static_cast<base::u64>(std::ranges::count_if(facts.events, [](const sema::PlaceStateEvent& event) {
            return event.kind == sema::PlaceStateEventKind::borrow_shared
                || event.kind == sema::PlaceStateEventKind::borrow_mutable;
        }));
    const base::u64 partial_move_count =
        static_cast<base::u64>(std::ranges::count_if(facts.places, [](const sema::PlaceStateFact& fact) {
            return fact.partial_move_count != 0 || fact.is_partially_moved;
        }));
    const base::u64 emitted_violation_count =
        static_cast<base::u64>(std::ranges::count_if(facts.violations, [](const sema::PlaceStateViolation& violation) {
            return violation.diagnostic_emitted;
        }));
    std::ostringstream label;
    label << IDE_SEMANTIC_FACT_PLACE_STATE << " places=" << facts.places.size()
          << " events=" << facts.events.size()
          << " partials=" << partial_count
          << " moves=" << move_place_count
          << " drops=" << drop_place_count
          << " borrows=" << borrow_event_count
          << " partial_moves=" << partial_move_count
          << " violations=" << facts.violations.size()
          << " diagnostics=" << emitted_violation_count
          << " enforced=" << (facts.diagnostic_mode_enforced ? "true" : "false")
          << " graph_missing=" << (facts.graph_missing ? "true" : "false")
          << " fingerprint=" << query::debug_string(sema::function_place_state_facts_fingerprint(facts));
    return label.str();
}

[[nodiscard]] std::string body_loan_check_detail(const sema::BodyLoanCheckResult& result)
{
    const base::usize reborrow_count =
        static_cast<base::usize>(std::ranges::count_if(result.loans, [](const sema::BodyLoan& loan) {
            return loan.parent_loan != sema::SEMA_BODY_LOAN_INVALID_INDEX;
        }));
    std::ostringstream label;
    label << IDE_SEMANTIC_FACT_BODY_LOAN_CHECK << " loans=" << result.loans.size() << " reborrows=" << reborrow_count
          << " two_phase=" << result.two_phase_borrows.size() << " conflicts=" << result.conflicts.size()
          << " mode=" << sema::body_loan_diagnostic_mode_name(result.diagnostic_mode)
          << " graph_missing=" << (result.graph_missing ? "true" : "false");
    if (!result.conflicts.empty()) {
        label << " first_conflict=" << sema::body_loan_conflict_kind_name(result.conflicts.front().kind);
    }
    label << " fingerprint=" << query::debug_string(sema::body_loan_check_fingerprint(result));
    return label.str();
}

[[nodiscard]] const query::FunctionCleanupMarkerFacts* cleanup_marker_facts_for_symbol(
    const IdeSnapshot& snapshot, const std::string_view symbol) noexcept
{
    for (const query::FunctionCleanupMarkerFacts& facts : snapshot.cleanup_marker_facts) {
        if (facts.symbol == symbol) {
            return &facts;
        }
    }
    return nullptr;
}

[[nodiscard]] std::string cleanup_marker_facts_detail(const query::FunctionCleanupMarkerFacts& facts)
{
    return query::summarize_function_cleanup_marker_facts(facts);
}

[[nodiscard]] const query::FunctionDynAbiFacts* dyn_abi_facts_for_symbol(
    const IdeSnapshot& snapshot, const std::string_view symbol) noexcept
{
    for (const query::FunctionDynAbiFacts& facts : snapshot.dyn_abi_facts) {
        if (facts.symbol == symbol) {
            return &facts;
        }
    }
    return nullptr;
}

[[nodiscard]] std::string dyn_abi_facts_detail(const query::FunctionDynAbiFacts& facts)
{
    std::string detail = query::summarize_function_dyn_abi_facts(facts);
    if (detail.rfind(IDE_SEMANTIC_FACT_DYN_ABI_FACTS, 0) == 0) {
        return detail;
    }
    return std::string(IDE_SEMANTIC_FACT_DYN_ABI_FACTS) + " " + detail;
}

[[nodiscard]] bool dyn_abi_facts_has_surface(const query::FunctionDynAbiFacts& facts) noexcept
{
    return !facts.objects.empty() || !facts.vtables.empty() || !facts.coercions.empty()
        || !facts.upcasts.empty() || !facts.principal_sets.empty()
        || !facts.composition_projections.empty() || !facts.composition_supertrait_chains.empty()
        || !facts.dispatches.empty();
}

void append_dyn_abi_hover_detail(std::ostringstream& label, const query::FunctionDynAbiFacts& facts)
{
    label << IDE_DETAIL_DYN_ABI_SEPARATOR
          << "abi=" << query::dyn_abi_policy_name(query::DynAbiPolicy::borrowed_view_v1)
          << "/metadata=" << query::dyn_metadata_policy_name(query::function_dyn_abi_metadata_policy(facts))
          << "/objects=" << facts.objects.size()
          << "/vtables=" << facts.vtables.size()
          << "/slots=" << facts.summary.slot_count
          << "/coercions=" << facts.coercions.size()
          << "/upcasts=" << facts.upcasts.size()
          << "/principal_sets=" << facts.principal_sets.size()
          << "/composition_projections=" << facts.composition_projections.size()
          << "/composition_supertrait_chains=" << facts.composition_supertrait_chains.size()
          << "/dispatches=" << facts.dispatches.size();
    if (!facts.composition_projections.empty()) {
        const query::DynCompositionProjectionAbiDescriptor& projection = facts.composition_projections.front();
        label << "/composition_projection="
              << (projection.source_reference_type_name.empty() ? "<unknown>" : projection.source_reference_type_name)
              << "->"
              << (projection.target_reference_type_name.empty() ? "<unknown>" : projection.target_reference_type_name)
              << "/composition_principal_index=" << projection.principal_index
              << "/composition_borrow=" << query::dyn_borrow_kind_name(projection.borrow_kind)
              << "/composition_metadata=" << query::dyn_metadata_policy_name(projection.metadata_policy);
    }
    if (!facts.composition_supertrait_chains.empty()) {
        const query::DynCompositionSupertraitChainAbiDescriptor& chain =
            facts.composition_supertrait_chains.front();
        label << "/composition_supertrait_chain="
              << (chain.source_reference_type_name.empty() ? "<unknown>" : chain.source_reference_type_name)
              << "->"
              << (chain.projected_reference_type_name.empty() ? "<unknown>"
                                                              : chain.projected_reference_type_name)
              << "->"
              << (chain.target_reference_type_name.empty() ? "<unknown>" : chain.target_reference_type_name)
              << "/chain_borrow=" << query::dyn_borrow_kind_name(chain.borrow_kind)
              << "/chain_composition_metadata="
              << query::dyn_metadata_policy_name(chain.composition_metadata_policy)
              << "/chain_upcast_metadata="
              << query::dyn_metadata_policy_name(chain.upcast_metadata_policy);
    }
    if (!facts.upcasts.empty()) {
        const query::DynUpcastAbiDescriptor& upcast = facts.upcasts.front();
        label << "/upcast="
              << (upcast.source_reference_type_name.empty() ? "<unknown>" : upcast.source_reference_type_name)
              << "->"
              << (upcast.target_reference_type_name.empty() ? "<unknown>" : upcast.target_reference_type_name)
              << "/upcast_borrow=" << query::dyn_borrow_kind_name(upcast.borrow_kind)
              << "/upcast_metadata=" << query::dyn_metadata_policy_name(upcast.metadata_policy);
    }
    if (!facts.dispatches.empty()) {
        label << "/dispatch=vtable_slot"
              << "/slot=" << facts.dispatches.front().slot;
    }
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

[[nodiscard]] std::string function_detail(const IdeSnapshot& snapshot, const sema::FunctionSignature& signature,
    const sema::FunctionLookupKey* const function_key = nullptr)
{
    const sema::CheckedModule& checked = snapshot.checked;
    std::ostringstream label;
    const std::string_view kind = signature.is_trait_impl_method
        ? IDE_SYMBOL_KIND_IMPL_METHOD
        : (signature.is_method ? IDE_SYMBOL_KIND_METHOD : IDE_SYMBOL_KIND_FUNCTION);
    label << kind << ' ' << sema::function_display_name(checked.types, signature) << '(';
    for (base::usize index = 0; index < signature.param_types.size(); ++index) {
        if (index != 0U) {
            label << ", ";
        }
        label << checked.types.display_name(signature.param_types[index]);
    }
    label << ") -> " << checked.types.display_name(signature.return_type);
    if (function_key != nullptr) {
        if (const auto summary = checked.borrow_summaries.find(*function_key);
            summary != checked.borrow_summaries.end()) {
            label << IDE_DETAIL_BORROW_SUMMARY_SEPARATOR << "deps=" << summary->second.return_origins.size()
                  << ", storage_escapes=" << summary->second.storage_escapes.size()
                  << ", unknown=" << (summary->second.has_unknown_return_origin ? "true" : "false")
                  << ", local_escape=" << (summary->second.has_local_return_escape ? "true" : "false");
        }
        if (const auto contract = checked.borrow_contracts.find(*function_key);
            contract != checked.borrow_contracts.end()) {
            label << IDE_DETAIL_BORROW_CONTRACT_SEPARATOR
                  << sema::function_borrow_contract_source_name(contract->second.source)
                  << "/selectors=" << contract->second.return_selectors.size()
                  << "/unknown=" << (contract->second.unknown_return_allowed ? "true" : "false")
                  << "/mismatch=" << (contract->second.has_contract_mismatch ? "true" : "false");
        }
        if (const auto lifetime = checked.lifetime_facts.find(*function_key);
            lifetime != checked.lifetime_facts.end()) {
            const base::usize local_escape_count = static_cast<base::usize>(
                std::ranges::count_if(lifetime->second.violations, [](const sema::LifetimeViolation& violation) {
                    return violation.kind == sema::LifetimeViolationKind::local_escape;
                }));
            const base::usize unknown_escape_count = static_cast<base::usize>(
                std::ranges::count_if(lifetime->second.violations, [](const sema::LifetimeViolation& violation) {
                    return violation.kind == sema::LifetimeViolationKind::unknown_escape;
                }));
            label << IDE_DETAIL_LIFETIME_SEPARATOR << "regions=" << lifetime->second.regions.size()
                  << "/returns=" << lifetime->second.return_regions.size()
                  << "/violations=" << lifetime->second.violations.size()
                  << "/local_escapes=" << local_escape_count
                  << "/unknown_escapes=" << unknown_escape_count;
        }
        if (const auto dropck = checked.dropck_facts.find(*function_key); dropck != checked.dropck_facts.end()) {
            base::u64 required_outlives_count = 0;
            for (const sema::DropCheckFact& fact : dropck->second.facts) {
                required_outlives_count += static_cast<base::u64>(fact.required_outlives.size());
            }
            label << " dropck=facts=" << dropck->second.facts.size()
                  << "/actions=" << dropck->second.actions.size()
                  << "/required_outlives=" << required_outlives_count
                  << "/violations=" << dropck->second.violations.size();
        }
        if (const auto place_state = checked.place_state_facts.find(*function_key);
            place_state != checked.place_state_facts.end()) {
            const base::u64 partial_count = static_cast<base::u64>(
                std::ranges::count_if(place_state->second.places, [](const sema::PlaceStateFact& fact) {
                    return fact.has_partial_projection;
                }));
            const base::u64 partial_move_count = static_cast<base::u64>(
                std::ranges::count_if(place_state->second.places, [](const sema::PlaceStateFact& fact) {
                    return fact.partial_move_count != 0 || fact.is_partially_moved;
                }));
            const base::u64 emitted_violation_count = static_cast<base::u64>(
                std::ranges::count_if(place_state->second.violations, [](const sema::PlaceStateViolation& violation) {
                    return violation.diagnostic_emitted;
                }));
            label << IDE_DETAIL_PLACE_STATE_SEPARATOR << "places=" << place_state->second.places.size()
                  << "/events=" << place_state->second.events.size()
                  << "/partials=" << partial_count
                  << "/partial_moves=" << partial_move_count
                  << "/violations=" << place_state->second.violations.size()
                  << "/diagnostics=" << emitted_violation_count
                  << "/enforced=" << (place_state->second.diagnostic_mode_enforced ? "true" : "false");
        }
        if (const auto move_rejections = checked.move_rejection_facts.find(*function_key);
            move_rejections != checked.move_rejection_facts.end()) {
            label << IDE_DETAIL_MOVE_REJECTION_SEPARATOR << "count=" << move_rejections->second.rejections.size();
            if (!move_rejections->second.rejections.empty()) {
                label << "/first=" << sema::move_rejection_kind_name(move_rejections->second.rejections.front().kind);
            }
        }
        if (const query::FunctionCleanupMarkerFacts* const cleanup =
                cleanup_marker_facts_for_symbol(snapshot, signature.c_name.view());
            cleanup != nullptr && !cleanup->markers.empty()) {
            label << IDE_DETAIL_CLEANUP_MARKER_SEPARATOR << "count=" << cleanup->markers.size()
                  << "/drop=" << cleanup->summary.drop_count
                  << "/drop_if=" << cleanup->summary.drop_if_count;
            if (!cleanup->markers.empty()) {
                label << "/first_policy=" << query::cleanup_marker_policy_name(cleanup->markers.front().policy);
            }
        }
        if (const query::FunctionDynAbiFacts* const dyn_abi =
                dyn_abi_facts_for_symbol(snapshot, signature.c_name.view());
            dyn_abi != nullptr && dyn_abi_facts_has_surface(*dyn_abi)) {
            append_dyn_abi_hover_detail(label, *dyn_abi);
        }
    }
    return label.str();
}

[[nodiscard]] std::string_view function_symbol_kind(const sema::FunctionSignature& signature) noexcept
{
    if (signature.is_trait_impl_method) {
        return IDE_SYMBOL_KIND_IMPL_METHOD;
    }
    return signature.is_method ? IDE_SYMBOL_KIND_METHOD : IDE_SYMBOL_KIND_FUNCTION;
}

[[nodiscard]] std::string typed_detail(const sema::CheckedModule& checked, const std::string_view kind,
    const std::string_view name, const sema::TypeHandle type)
{
    std::ostringstream label;
    label << kind << ' ' << name;
    if (sema::is_valid(type)) {
        label << ": " << checked.types.display_name(type);
        const sema::ResourceSemanticsSummary resource = sema::ResourceSemanticsClassifier(checked).classify(type);
        label << IDE_DETAIL_RESOURCE_SEPARATOR << sema::resource_semantics_debug_string(resource);
        if (checked.destructors.contains(type.value)) {
            label << " destructor=custom";
        }
    }
    return label.str();
}

[[nodiscard]] std::string trait_detail(const sema::TraitSignature& trait)
{
    std::ostringstream label;
    label << IDE_SYMBOL_KIND_TRAIT << ' ' << trait.name << " associated_types=" << trait.associated_types.size()
          << " requirements=" << trait.requirements.size();
    return label.str();
}

[[nodiscard]] std::string trait_method_detail(const sema::CheckedModule& checked, const sema::TraitSignature& trait,
    const sema::TraitMethodRequirement& requirement)
{
    std::ostringstream label;
    label << IDE_SYMBOL_KIND_TRAIT_METHOD << ' ' << trait.name << '.' << requirement.name << '(';
    for (base::usize index = 0; index < requirement.param_types.size(); ++index) {
        if (index != 0U) {
            label << ", ";
        }
        label << checked.types.display_name(requirement.param_types[index]);
    }
    label << ") -> " << checked.types.display_name(requirement.return_type);
    if (requirement.has_default_body) {
        label << " default";
    }
    return label.str();
}

void append_trait_method_dispatch_detail(std::ostringstream& label, const sema::TraitMethodCallBinding& binding)
{
    if (binding.dispatch != sema::TraitMethodDispatchKind::vtable_slot) {
        return;
    }
    label << IDE_DETAIL_DYN_DISPATCH_SEPARATOR
          << "dispatch=vtable_slot"
          << "/slot=" << binding.vtable_slot
          << "/abi=" << query::dyn_abi_policy_name(query::DynAbiPolicy::borrowed_view_v1)
          << "/metadata="
          << query::dyn_metadata_policy_name(query::DynMetadataPolicy::borrowed_methods_only_v1);
}

[[nodiscard]] std::string associated_type_detail(const sema::CheckedModule& checked, const std::string_view owner_name,
    const std::string_view name, const sema::TypeHandle value_type)
{
    std::ostringstream label;
    label << IDE_SYMBOL_KIND_ASSOCIATED_TYPE << ' ' << owner_name << '.' << name;
    if (sema::is_valid(value_type)) {
        label << IDE_DETAIL_TYPE_SEPARATOR << checked.types.display_name(value_type);
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

[[nodiscard]] const sema::TraitSignature* find_trait_signature(
    const IdeSnapshot& snapshot, const syntax::ModuleId module, const sema::IdentId name_id) noexcept
{
    for (const auto& entry : snapshot.checked.traits) {
        const sema::TraitSignature& trait = entry.second;
        if (trait.module.value == module.value && trait.name_id == name_id) {
            return &trait;
        }
    }
    return nullptr;
}

[[nodiscard]] const sema::TraitMethodRequirement* find_trait_method_requirement(
    const sema::TraitSignature& trait, const sema::IdentId name_id) noexcept
{
    for (const sema::TraitMethodRequirement& requirement : trait.requirements) {
        if (requirement.name_id == name_id) {
            return &requirement;
        }
    }
    return nullptr;
}

[[nodiscard]] query::MemberKey trait_impl_method_member_key(
    const IdeSnapshot& snapshot, const sema::FunctionSignature& signature)
{
    const sema::TraitSignature* const trait =
        find_trait_signature(snapshot, signature.trait_module, signature.trait_name_id);
    if (trait == nullptr) {
        return {};
    }
    const sema::TraitMethodRequirement* const requirement = find_trait_method_requirement(*trait, signature.name_id);
    return requirement == nullptr ? query::MemberKey{} : trait_method_member_key(snapshot, *trait, *requirement);
}

[[nodiscard]] base::SourceRange trait_impl_associated_type_name_range(const IdeSnapshot& snapshot,
    const sema::TraitImplAssociatedTypeInfo& associated_type, const base::SourceRange fallback) noexcept
{
    return item_name_range(snapshot, associated_type.item, associated_type.name.view(), fallback);
}

void push_checked_trait_symbols(const IdeSnapshot& snapshot, SymbolIndex& index, const sema::TraitSignature& trait)
{
    const base::SourceRange trait_name_range = item_name_range(snapshot, trait.item, trait.name.view(), trait.range);
    push_global_symbol(index,
        IdeSymbol{
            trait_signature_def_key(snapshot, trait),
            trait_name_range,
            trait.range,
            std::string(trait.name.view()),
            std::string(IDE_SYMBOL_KIND_TRAIT),
            trait_detail(trait),
            trait.part_index,
            false,
            true,
        });

    for (const sema::TraitAssociatedTypeRequirement& associated_type : trait.associated_types) {
        const base::SourceRange range =
            item_name_range(snapshot, associated_type.item, associated_type.name.view(), associated_type.range);
        push_global_symbol(index,
            IdeSymbol{
                associated_type_definition_key(snapshot, associated_type),
                range,
                trait.range,
                std::string(associated_type.name.view()),
                std::string(IDE_SYMBOL_KIND_ASSOCIATED_TYPE),
                associated_type_detail(
                    snapshot.checked, trait.name.view(), associated_type.name.view(), sema::INVALID_TYPE_HANDLE),
                trait.part_index,
                false,
                true,
                associated_type.member_key,
            });
    }

    for (const sema::TraitMethodRequirement& requirement : trait.requirements) {
        const base::SourceRange range =
            item_name_range(snapshot, requirement.item, requirement.name.view(), requirement.range);
        push_global_symbol(index,
            IdeSymbol{
                trait_method_definition_key(snapshot, requirement),
                range,
                trait.range,
                std::string(requirement.name.view()),
                std::string(IDE_SYMBOL_KIND_TRAIT_METHOD),
                trait_method_detail(snapshot.checked, trait, requirement),
                trait.part_index,
                false,
                true,
                trait_method_member_key(snapshot, trait, requirement),
            });
    }
}

void push_checked_trait_impl_symbols(const IdeSnapshot& snapshot, SymbolIndex& index, const sema::TraitImplInfo& impl)
{
    for (const sema::TraitImplAssociatedTypeInfo& associated_type : impl.associated_types) {
        const base::SourceRange range = trait_impl_associated_type_name_range(snapshot, associated_type, impl.range);
        push_global_symbol(index,
            IdeSymbol{
                associated_type_definition_key(associated_type),
                range,
                impl.range,
                std::string(associated_type.name.view()),
                std::string(IDE_SYMBOL_KIND_ASSOCIATED_TYPE),
                associated_type_detail(
                    snapshot.checked, impl.trait_name.view(), associated_type.name.view(), associated_type.value_type),
                impl.part_index,
                false,
                true,
                associated_type.member_key,
            });
    }
}

void push_checked_global_symbols(const IdeSnapshot& snapshot, SymbolIndex& index)
{
    for (const auto& entry : snapshot.checked.traits) {
        push_checked_trait_symbols(snapshot, index, entry.second);
    }
    for (const auto& entry : snapshot.checked.trait_impls) {
        push_checked_trait_impl_symbols(snapshot, index, entry.second);
    }
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
        if (signature.is_trait_default_method_instance) {
            continue;
        }
        const query::DefKind kind = signature.is_method ? query::DefKind::method : query::DefKind::function;
        const base::SourceRange range =
            item_name_range(snapshot, signature.definition_item, signature.name.view(), signature.range);
        const query::MemberKey member =
            signature.is_trait_impl_method ? trait_impl_method_member_key(snapshot, signature) : query::MemberKey{};
        push_global_symbol(index,
            IdeSymbol{
                symbol_def_key(
                    snapshot, signature.stable_id, query::DefNamespace::value, kind, signature.name.view(), range),
                range,
                signature.range,
                std::string(signature.name.view()),
                std::string(function_symbol_kind(signature)),
                function_detail(snapshot, signature, &entry.first),
                signature.part_index,
                false,
                true,
                member,
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
    authority.value_component_count = static_cast<base::u64>(signature.param_types.size());
    authority.generic_param_count = static_cast<base::u64>(signature.generic_args.size());
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
    authority.value_component_count = static_cast<base::u64>(info.fields.size());
    authority.generic_param_count = static_cast<base::u64>(info.generic_instance_key.type_args.size());
    authority.has_return_type = sema::is_valid(info.type);
    authority.has_definition = !info.is_opaque;
    return authority;
}

[[nodiscard]] query::ItemSignatureAuthority item_signature_authority(
    const IdeSnapshot& snapshot, const sema::EnumCaseInfo& info)
{
    query::ItemSignatureAuthority authority = item_signature_authority_base(snapshot, info.incremental_key,
        info.part_index, query::DefNamespace::value, query::DefKind::enum_case, info.visibility);
    authority.value_component_count = static_cast<base::u64>(info.payload_types.size());
    authority.generic_param_count = static_cast<base::u64>(info.generic_instance_key.type_args.size());
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

[[nodiscard]] query::ItemSignatureAuthority item_signature_authority(
    const IdeSnapshot& snapshot, const sema::TraitSignature& trait)
{
    query::ItemSignatureAuthority authority = item_signature_authority_base(snapshot, trait.incremental_key,
        trait.part_index, query::DefNamespace::trait_, query::DefKind::trait_, trait.visibility);
    authority.value_component_count = static_cast<base::u64>(trait.associated_types.size() + trait.requirements.size());
    authority.generic_param_count = static_cast<base::u64>(trait.generic_params.size());
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

void evaluate_trait_item_signature_query(query::QueryContext& context, IdeSnapshot& snapshot,
    const sema::TraitSignature& trait, const IdeIncrementalSnapshotInput& incremental)
{
    const base::SourceRange range = item_name_range(snapshot, trait.item, trait.name.view(), trait.range);
    const query::DefKey key = trait_signature_def_key(snapshot, trait);
    IdeSemanticFact fact;
    fact.range = range;
    fact.name = std::string(trait.name.view());
    fact.detail = std::string(IDE_SEMANTIC_FACT_ITEM_SIGNATURE);
    fact.part_index = trait.part_index;
    push_item_signature_fact(
        context, snapshot, key, item_signature_authority(snapshot, trait), std::move(fact), incremental);
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
    for (const auto& entry : snapshot.checked.traits) {
        evaluate_trait_item_signature_query(context, snapshot, entry.second, incremental);
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
    const query::QueryResultFingerprint body_syntax_result, const query::QueryResultFingerprint signature_result,
    const IdeSnapshot& snapshot, const sema::FunctionLookupKey function)
{
    query::TypeCheckBodyAuthority authority;
    authority.checked_body = type_check_body_checked_result_fingerprint(key, body_syntax_result, signature_result);
    authority.body_syntax_result = body_syntax_result;
    authority.signature_result = signature_result;
    sema::populate_type_check_body_borrow_authority(authority, snapshot.checked, function);
    return authority;
}

void push_borrow_summary_fact(IdeSnapshot& snapshot, const query::QueryKey query_key, const query::BodyKey body,
    const sema::FunctionSignature& signature, const sema::FunctionLookupKey function, const base::SourceRange range)
{
    const auto summary = snapshot.checked.borrow_summaries.find(function);
    if (summary == snapshot.checked.borrow_summaries.end()) {
        return;
    }
    IdeSemanticFact fact;
    fact.kind = IdeSemanticFactKind::borrow_summary;
    fact.query = query_key;
    fact.definition = body.owner;
    fact.body = body;
    fact.range = range;
    fact.name = std::string(signature.name.view());
    fact.detail = borrow_summary_detail(summary->second);
    fact.part_index = signature.part_index;
    fact.generic_instance = signature.generic_instance_key;
    fact.checked = true;
    snapshot.query.semantic_facts.push_back(std::move(fact));
}

void push_borrow_contract_fact(IdeSnapshot& snapshot, const query::QueryKey query_key, const query::BodyKey body,
    const sema::FunctionSignature& signature, const sema::FunctionLookupKey function, const base::SourceRange range)
{
    const auto contract = snapshot.checked.borrow_contracts.find(function);
    if (contract == snapshot.checked.borrow_contracts.end()) {
        return;
    }
    IdeSemanticFact fact;
    fact.kind = IdeSemanticFactKind::borrow_contract;
    fact.query = query_key;
    fact.definition = body.owner;
    fact.body = body;
    fact.range = range;
    fact.name = std::string(signature.name.view());
    fact.detail = borrow_contract_detail(contract->second);
    fact.part_index = signature.part_index;
    fact.generic_instance = signature.generic_instance_key;
    fact.checked = true;
    snapshot.query.semantic_facts.push_back(std::move(fact));
}

void push_lifetime_facts_fact(IdeSnapshot& snapshot, const query::QueryKey query_key, const query::BodyKey body,
    const sema::FunctionSignature& signature, const sema::FunctionLookupKey function, const base::SourceRange range)
{
    const auto facts = snapshot.checked.lifetime_facts.find(function);
    if (facts == snapshot.checked.lifetime_facts.end()) {
        return;
    }
    IdeSemanticFact fact;
    fact.kind = IdeSemanticFactKind::lifetime_facts;
    fact.query = query_key;
    fact.definition = body.owner;
    fact.body = body;
    fact.range = range;
    fact.name = std::string(signature.name.view());
    fact.detail = lifetime_facts_detail(facts->second);
    fact.part_index = signature.part_index;
    fact.generic_instance = signature.generic_instance_key;
    fact.checked = true;
    snapshot.query.semantic_facts.push_back(std::move(fact));
}

void push_move_rejection_facts_fact(IdeSnapshot& snapshot, const query::QueryKey query_key, const query::BodyKey body,
    const sema::FunctionSignature& signature, const sema::FunctionLookupKey function, const base::SourceRange range)
{
    const auto facts = snapshot.checked.move_rejection_facts.find(function);
    if (facts == snapshot.checked.move_rejection_facts.end()) {
        return;
    }
    IdeSemanticFact fact;
    fact.kind = IdeSemanticFactKind::move_rejection_facts;
    fact.query = query_key;
    fact.definition = body.owner;
    fact.body = body;
    fact.range = range;
    fact.name = std::string(signature.name.view());
    fact.detail = sema::summarize_function_move_rejection_facts(facts->second);
    fact.part_index = signature.part_index;
    fact.generic_instance = signature.generic_instance_key;
    fact.checked = true;
    snapshot.query.semantic_facts.push_back(std::move(fact));
}

void push_dropck_facts_fact(IdeSnapshot& snapshot, const query::QueryKey query_key, const query::BodyKey body,
    const sema::FunctionSignature& signature, const sema::FunctionLookupKey function, const base::SourceRange range)
{
    const auto facts = snapshot.checked.dropck_facts.find(function);
    if (facts == snapshot.checked.dropck_facts.end()) {
        return;
    }
    IdeSemanticFact fact;
    fact.kind = IdeSemanticFactKind::dropck_facts;
    fact.query = query_key;
    fact.definition = body.owner;
    fact.body = body;
    fact.range = range;
    fact.name = std::string(signature.name.view());
    fact.detail = dropck_facts_detail(facts->second);
    fact.part_index = signature.part_index;
    fact.generic_instance = signature.generic_instance_key;
    fact.checked = true;
    snapshot.query.semantic_facts.push_back(std::move(fact));
}

void push_place_state_fact(IdeSnapshot& snapshot, const query::QueryKey query_key, const query::BodyKey body,
    const sema::FunctionSignature& signature, const sema::FunctionLookupKey function, const base::SourceRange range)
{
    const auto facts = snapshot.checked.place_state_facts.find(function);
    if (facts == snapshot.checked.place_state_facts.end()) {
        return;
    }
    IdeSemanticFact fact;
    fact.kind = IdeSemanticFactKind::place_state;
    fact.query = query_key;
    fact.definition = body.owner;
    fact.body = body;
    fact.range = range;
    fact.name = std::string(signature.name.view());
    fact.detail = place_state_detail(facts->second);
    fact.part_index = signature.part_index;
    fact.generic_instance = signature.generic_instance_key;
    fact.checked = true;
    snapshot.query.semantic_facts.push_back(std::move(fact));
}

void push_body_loan_check_fact(IdeSnapshot& snapshot, const query::QueryKey query_key, const query::BodyKey body,
    const sema::FunctionSignature& signature, const sema::FunctionLookupKey function, const base::SourceRange range)
{
    const auto result = snapshot.checked.body_loan_checks.find(function);
    if (result == snapshot.checked.body_loan_checks.end()) {
        return;
    }
    IdeSemanticFact fact;
    fact.kind = IdeSemanticFactKind::body_loan_check;
    fact.query = query_key;
    fact.definition = body.owner;
    fact.body = body;
    fact.range = range;
    fact.name = std::string(signature.name.view());
    fact.detail = body_loan_check_detail(result->second);
    fact.part_index = signature.part_index;
    fact.generic_instance = signature.generic_instance_key;
    fact.checked = true;
    snapshot.query.semantic_facts.push_back(std::move(fact));
}

void push_cleanup_marker_facts_fact(IdeSnapshot& snapshot, const query::QueryKey query_key, const query::BodyKey body,
    const sema::FunctionSignature& signature, const base::SourceRange range)
{
    const query::FunctionCleanupMarkerFacts* const facts =
        cleanup_marker_facts_for_symbol(snapshot, signature.c_name.view());
    if (facts == nullptr || facts->markers.empty()) {
        return;
    }
    IdeSemanticFact fact;
    fact.kind = IdeSemanticFactKind::cleanup_marker_facts;
    fact.query = query_key;
    fact.definition = body.owner;
    fact.body = body;
    fact.range = range;
    fact.name = std::string(signature.name.view());
    fact.detail = cleanup_marker_facts_detail(*facts);
    fact.part_index = signature.part_index;
    fact.generic_instance = signature.generic_instance_key;
    fact.checked = true;
    snapshot.query.semantic_facts.push_back(std::move(fact));
}

void push_dyn_abi_facts_fact(IdeSnapshot& snapshot, const query::QueryKey query_key, const query::BodyKey body,
    const sema::FunctionSignature& signature, const base::SourceRange range)
{
    const query::FunctionDynAbiFacts* const facts = dyn_abi_facts_for_symbol(snapshot, signature.c_name.view());
    if (facts == nullptr || !dyn_abi_facts_has_surface(*facts)) {
        return;
    }
    IdeSemanticFact fact;
    fact.kind = IdeSemanticFactKind::dyn_abi_facts;
    fact.query = query_key;
    fact.definition = body.owner;
    fact.body = body;
    fact.range = range;
    fact.name = std::string(signature.name.view());
    fact.detail = dyn_abi_facts_detail(*facts);
    fact.part_index = signature.part_index;
    fact.generic_instance = signature.generic_instance_key;
    fact.checked = true;
    snapshot.query.semantic_facts.push_back(std::move(fact));
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
    const sema::FunctionLookupKey function, const base::SourceRange range,
    const IdeIncrementalSnapshotInput& incremental)
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
    push_borrow_summary_fact(snapshot, *query_key, key, signature, function, range);
    push_borrow_contract_fact(snapshot, *query_key, key, signature, function,
        item_name_range(snapshot, signature.definition_item, signature.name.view(), signature.range));
    push_move_rejection_facts_fact(snapshot, *query_key, key, signature, function, range);
    push_lifetime_facts_fact(snapshot, *query_key, key, signature, function, range);
    push_dropck_facts_fact(snapshot, *query_key, key, signature, function, range);
    push_place_state_fact(snapshot, *query_key, key, signature, function, range);
    push_body_loan_check_fact(snapshot, *query_key, key, signature, function, range);
}

void push_lower_function_ir_fact(query::QueryContext& context, IdeSnapshot& snapshot, const query::BodyKey key,
    const sema::FunctionSignature& signature, const query::QueryResultFingerprint type_check_result,
    const base::SourceRange range, const IdeIncrementalSnapshotInput& incremental)
{
    const query::FunctionCleanupMarkerFacts* const cleanup =
        cleanup_marker_facts_for_symbol(snapshot, signature.c_name.view());
    const query::FunctionCleanupMarkerFacts empty_cleanup;
    const query::FunctionCleanupMarkerFacts& cleanup_facts = cleanup == nullptr ? empty_cleanup : *cleanup;
    const query::FunctionDynAbiFacts* const dyn_abi =
        dyn_abi_facts_for_symbol(snapshot, signature.c_name.view());
    const query::FunctionDynAbiFacts empty_dyn_abi;
    const query::FunctionDynAbiFacts& dyn_abi_facts = dyn_abi == nullptr ? empty_dyn_abi : *dyn_abi;
    if ((!cleanup_facts.markers.empty() || dyn_abi_facts_has_surface(dyn_abi_facts))
        && query::is_valid(type_check_result)) {
        const query::QueryResultFingerprint ir_input =
            lower_function_ir_input_fingerprint(key, type_check_result, cleanup_facts, dyn_abi_facts);
        const query::QueryResultFingerprint ir_result =
            query::lower_function_ir_result_fingerprint(ir_input, cleanup_facts, dyn_abi_facts);
        seed_reusable_query_record(context, incremental, query::lower_function_ir_query_record(key, ir_result));
        const query::QueryEvaluationResult result =
            context.evaluate_lower_function_ir(query::LowerFunctionIRProviderInput{
                key,
                ir_input,
                cleanup_facts,
                dyn_abi_facts,
            });
        if (!query_evaluation_completed(result)) {
            return;
        }
        const std::optional<query::QueryKey> query_key = query::lower_function_ir_query_key(key);
        if (!query_key.has_value()) {
            return;
        }
        push_cleanup_marker_facts_fact(snapshot, *query_key, key, signature, range);
        push_dyn_abi_facts_fact(snapshot, *query_key, key, signature, range);
        return;
    }
}

void evaluate_function_body_queries(query::QueryContext& context, IdeSnapshot& snapshot,
    const sema::FunctionLookupKey function, const sema::FunctionSignature& signature,
    const IdeIncrementalSnapshotInput& incremental)
{
    if (!signature.has_definition || signature.has_conflict || !query::is_valid(signature.stable_id)
        || !query::is_valid(signature.incremental_key)) {
        return;
    }
    const query::DefKey def = function_signature_def_key(snapshot, signature);
    const query::BodyKey body = query::body_key(def, function_body_slot_kind(signature));
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
        type_check_body_authority(body, syntax_result, signature_result, snapshot, function);
    push_type_check_body_fact(context, snapshot, body, type_check_authority, signature, function, *range, incremental);
    push_lower_function_ir_fact(context, snapshot, body, signature,
        query::type_check_body_result_fingerprint(type_check_authority), *range, incremental);
}

void evaluate_checked_function_body_queries(
    query::QueryContext& context, IdeSnapshot& snapshot, const IdeIncrementalSnapshotInput& incremental)
{
    for (const auto& entry : snapshot.checked.functions) {
        evaluate_function_body_queries(context, snapshot, entry.first, entry.second, incremental);
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

#if defined(AUREX_TOOLING_ENABLE_IR_FACTS)
void populate_ir_facts(IdeSnapshot& snapshot)
{
    if (!snapshot.checked_semantics) {
        return;
    }
    auto lowered = ir::lower_ast(snapshot.ast, snapshot.checked);
    if (!lowered) {
        return;
    }
    snapshot.cleanup_marker_facts = ir::function_cleanup_marker_facts(lowered.value());
    snapshot.dyn_abi_facts = ir::function_dyn_abi_facts(lowered.value());
}
#else
void populate_ir_facts(IdeSnapshot& snapshot)
{
    static_cast<void>(snapshot);
}
#endif

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

[[nodiscard]] const sema::FunctionSignature* checked_function_signature_for_item(
    const IdeSnapshot& snapshot, const syntax::ItemId item) noexcept
{
    if (!snapshot.checked_semantics || !syntax::is_valid(item)) {
        return nullptr;
    }
    for (const auto& entry : snapshot.checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        if (signature.definition_item.value == item.value || signature.prototype_item.value == item.value) {
            return &signature;
        }
    }
    return nullptr;
}

[[nodiscard]] bool function_declares_generic_parameter(
    const syntax::ItemNode& function, const std::string_view name) noexcept
{
    for (const syntax::GenericParamDecl& param : function.generic_params) {
        if (param.name == name) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::optional<sema::TypeHandle> checked_generic_parameter_type(
    const IdeSnapshot& snapshot, const syntax::ItemNode& function, const syntax::TypeId type_id) noexcept
{
    if (!syntax::is_valid(type_id) || type_id.value >= snapshot.ast.types.size()) {
        return std::nullopt;
    }
    const syntax::TypeNode type = snapshot.ast.types[type_id.value];
    if (type.kind != syntax::TypeKind::named || !type.scope_name.empty() || !type.type_args.empty()
        || !function_declares_generic_parameter(function, type.name)) {
        return std::nullopt;
    }

    std::optional<sema::TypeHandle> match;
    for (base::usize index = 0; index < snapshot.checked.types.size(); ++index) {
        const sema::TypeHandle candidate{static_cast<base::u32>(index)};
        const sema::TypeInfo& info = snapshot.checked.types.get(candidate);
        if (info.kind != sema::TypeKind::generic_param || info.name.view() != type.name) {
            continue;
        }
        if (match.has_value()) {
            return std::nullopt;
        }
        match = candidate;
    }
    return match;
}

[[nodiscard]] std::optional<sema::TypeHandle> checked_parameter_type(const IdeSnapshot& snapshot,
    const syntax::ItemNode& function, const syntax::ParamDecl& param, const sema::FunctionSignature* const signature,
    const base::usize param_index) noexcept
{
    if (const std::optional<sema::TypeHandle> type = checked_syntax_type(snapshot, param.type)) {
        return type;
    }
    if (signature == nullptr || param_index >= signature->param_types.size()) {
        return checked_generic_parameter_type(snapshot, function, param.type);
    }
    const sema::TypeHandle type = signature->param_types[param_index];
    if (sema::is_valid(type)) {
        return type;
    }
    return checked_generic_parameter_type(snapshot, function, param.type);
}

void push_parameter_symbols(
    const IdeSnapshot& snapshot, SymbolIndex& index, const syntax::ItemNode& function, const syntax::ItemId function_id)
{
    const sema::FunctionSignature* const signature = checked_function_signature_for_item(snapshot, function_id);
    for (base::usize param_index = 0; param_index < function.params.size(); ++param_index) {
        const syntax::ParamDecl& param = function.params[param_index];
        const base::SourceRange range = name_range_in_range(snapshot.lossless, param.name, param.range);
        std::string detail = std::string(IDE_SYMBOL_KIND_PARAMETER) + " " + std::string(param.name);
        if (const std::optional<sema::TypeHandle> type =
                checked_parameter_type(snapshot, function, param, signature, param_index)) {
            detail = typed_detail(snapshot.checked, IDE_SYMBOL_KIND_PARAMETER, param.name, *type);
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
        detail = typed_detail(snapshot.checked, IDE_SYMBOL_KIND_LOCAL, stmt.name, *type);
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
    const IdeSnapshot& snapshot, SymbolIndex& index, const syntax::ItemNode& function, const syntax::ItemId function_id)
{
    if (!syntax::is_valid(function.body)) {
        return;
    }
    push_parameter_symbols(snapshot, index, function, function_id);

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
            collect_local_symbols_from_function(
                snapshot, index, *item, syntax::ItemId{static_cast<base::u32>(item_index)});
        }
    }
    return index;
}

[[nodiscard]] const IdeSymbol* best_global_symbol(
    const SymbolIndex& index, const std::string_view name, const base::usize offset)
{
    const auto found = index.globals.find(std::string(name));
    if (found == index.globals.end()) {
        return nullptr;
    }
    const IdeSymbol* fallback = nullptr;
    for (const base::usize symbol_index : found->second) {
        if (symbol_index < index.symbols.size()) {
            const IdeSymbol& symbol = index.symbols[symbol_index];
            if (range_contains_offset(symbol.range, offset)) {
                return &symbol;
            }
            if (fallback == nullptr || (!fallback->checked && symbol.checked)) {
                fallback = &symbol;
            }
        }
    }
    return fallback;
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
    return best_global_symbol(index, name, offset);
}

[[nodiscard]] bool ide_symbols_reference_same_entity(const IdeSymbol& lhs, const IdeSymbol& rhs) noexcept
{
    if (query::is_valid(lhs.member) && query::is_valid(rhs.member)) {
        return lhs.member == rhs.member;
    }
    if (query::is_valid(lhs.key) && query::is_valid(rhs.key)) {
        return lhs.key == rhs.key;
    }
    return lhs.name == rhs.name && lhs.kind == rhs.kind && lhs.range.source.value == rhs.range.source.value
        && lhs.range.begin == rhs.range.begin && lhs.range.end == rhs.range.end;
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

[[nodiscard]] const sema::TraitMethodRequirement* trait_method_call_requirement(
    const IdeSnapshot& snapshot, const sema::TraitMethodCallBinding& binding, const sema::TraitSignature*& trait)
{
    trait = find_trait_signature(snapshot, binding.trait_module, binding.trait_name_id);
    if (trait == nullptr) {
        return nullptr;
    }
    if (binding.requirement_ordinal < trait->requirements.size()) {
        const sema::TraitMethodRequirement& requirement = trait->requirements[binding.requirement_ordinal];
        if (requirement.name_id == binding.method_name_id) {
            return &requirement;
        }
    }
    return find_trait_method_requirement(*trait, binding.method_name_id);
}

[[nodiscard]] std::optional<IdeSymbol> trait_method_call_symbol(
    const IdeSnapshot& snapshot, const sema::TraitMethodCallBinding& binding)
{
    if (binding.dispatch == sema::TraitMethodDispatchKind::impl_override && sema::is_valid(binding.function_key)) {
        const auto found = snapshot.checked.functions.find(binding.function_key);
        if (found != snapshot.checked.functions.end() && !found->second.is_trait_default_method_instance) {
            const sema::FunctionSignature& signature = found->second;
            const base::SourceRange range =
                item_name_range(snapshot, signature.definition_item, signature.name.view(), signature.range);
            const query::MemberKey member =
                signature.is_trait_impl_method ? trait_impl_method_member_key(snapshot, signature) : query::MemberKey{};
            return IdeSymbol{
                function_signature_def_key(snapshot, signature),
                range,
                signature.range,
                std::string(signature.name.view()),
                std::string(function_symbol_kind(signature)),
                function_detail(snapshot, signature, &binding.function_key),
                signature.part_index,
                false,
                true,
                member,
                signature.generic_instance_key,
            };
        }
    }

    const sema::TraitSignature* trait = nullptr;
    const sema::TraitMethodRequirement* const requirement = trait_method_call_requirement(snapshot, binding, trait);
    if (trait == nullptr || requirement == nullptr) {
        return std::nullopt;
    }
    const base::SourceRange range =
        item_name_range(snapshot, requirement->item, requirement->name.view(), requirement->range);
    std::ostringstream detail;
    detail << trait_method_detail(snapshot.checked, *trait, *requirement);
    append_trait_method_dispatch_detail(detail, binding);
    return IdeSymbol{
        trait_method_definition_key(snapshot, *requirement),
        range,
        trait->range,
        std::string(requirement->name.view()),
        std::string(IDE_SYMBOL_KIND_TRAIT_METHOD),
        detail.str(),
        trait->part_index,
        false,
        true,
        trait_method_member_key(snapshot, *trait, *requirement),
    };
}

[[nodiscard]] std::optional<IdeSymbol> trait_method_call_symbol_at_offset(
    const IdeSnapshot& snapshot, const IdeTokenInfo& info, const base::usize offset)
{
    if (info.kind != syntax::TokenKind::identifier) {
        return std::nullopt;
    }
    for (const sema::TraitMethodCallBinding& binding : snapshot.checked.trait_method_calls) {
        if (binding.method_name.view() != info.text || !syntax::is_valid(binding.callee_expr)
            || binding.callee_expr.value >= snapshot.ast.exprs.size()) {
            continue;
        }
        if (!range_contains_offset(snapshot.ast.exprs.range(binding.callee_expr.value), offset)) {
            continue;
        }
        return trait_method_call_symbol(snapshot, binding);
    }
    return std::nullopt;
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

[[nodiscard]] bool ide_identifier_char(const char ch) noexcept
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
}

[[nodiscard]] bool ide_completion_label_matches_prefix(
    const std::string_view label, const std::string_view prefix) noexcept
{
    return prefix.empty() || (label.size() >= prefix.size() && label.substr(0, prefix.size()) == prefix);
}

struct IdeCompletionPrefix {
    base::SourceRange range{};
    std::string_view text;
};

[[nodiscard]] IdeCompletionPrefix ide_completion_prefix_at_offset(const IdeSnapshot& snapshot, const base::usize offset)
{
    const std::string_view text = snapshot.sources.text(snapshot.source_id);
    const base::usize clamped_offset = std::min(offset, text.size());
    base::usize begin = clamped_offset;
    while (begin > 0U && ide_identifier_char(text[begin - 1U])) {
        --begin;
    }
    base::usize end = clamped_offset;
    while (end < text.size() && ide_identifier_char(text[end])) {
        ++end;
    }
    return IdeCompletionPrefix{
        base::SourceRange{snapshot.source_id, begin, end},
        text.substr(begin, clamped_offset - begin),
    };
}

[[nodiscard]] std::optional<char> ide_previous_non_space(const std::string_view text, base::usize offset) noexcept
{
    offset = std::min(offset, text.size());
    while (offset > 0U) {
        --offset;
        const char ch = text[offset];
        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            return ch;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::string_view ide_line_prefix(const std::string_view text, const base::usize offset) noexcept
{
    const base::usize clamped_offset = std::min(offset, text.size());
    const base::usize line_begin = text.rfind('\n', clamped_offset == 0U ? 0U : clamped_offset - 1U);
    const base::usize begin = line_begin == std::string_view::npos ? 0U : line_begin + 1U;
    return text.substr(begin, clamped_offset - begin);
}

[[nodiscard]] bool ide_line_contains_module_path_keyword(const std::string_view line) noexcept
{
    return line.find("module") != std::string_view::npos || line.find("import") != std::string_view::npos;
}

[[nodiscard]] bool ide_token_is_trivia_for_completion(const syntax::Token& token) noexcept
{
    return token.kind == syntax::TokenKind::whitespace || token.kind == syntax::TokenKind::line_comment
        || token.kind == syntax::TokenKind::block_comment;
}

[[nodiscard]] bool ide_offset_is_trait_bound_completion_context(
    const IdeSnapshot& snapshot, const IdeCompletionPrefix& prefix) noexcept
{
    const base::usize begin = prefix.range.begin;
    bool saw_bound_colon = false;
    for (base::usize index = snapshot.lossless.tokens().size(); index > 0U; --index) {
        const syntax::Token& token = snapshot.lossless.tokens()[index - 1U];
        if (token.range.end > begin || ide_token_is_trivia_for_completion(token)) {
            continue;
        }
        if (!saw_bound_colon) {
            if (token.kind != syntax::TokenKind::colon) {
                return false;
            }
            saw_bound_colon = true;
            continue;
        }
        if (token.kind == syntax::TokenKind::kw_where) {
            return true;
        }
        if (token.kind == syntax::TokenKind::semicolon || token.kind == syntax::TokenKind::l_brace
            || token.kind == syntax::TokenKind::r_brace) {
            return false;
        }
    }
    return false;
}

[[nodiscard]] bool ide_offset_in_function_body(const IdeSnapshot& snapshot, const base::usize offset) noexcept
{
    if (!snapshot.parsed) {
        return false;
    }
    for (base::usize index = 0; index < snapshot.ast.items.size(); ++index) {
        const syntax::ItemNode* const item = snapshot.ast.items.ptr(index);
        if (item == nullptr || item->kind != syntax::ItemKind::fn_decl) {
            continue;
        }
        const std::optional<base::SourceRange> body_range = ast_item_body_range(snapshot, *item);
        if (body_range.has_value() && range_contains_offset(*body_range, offset)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] IdeCompletionContextKind ide_completion_context_at_offset(
    const IdeSnapshot& snapshot, const IdeCompletionPrefix& prefix, const base::usize offset)
{
    const std::string_view text = snapshot.sources.text(snapshot.source_id);
    if (const std::optional<char> previous = ide_previous_non_space(text, prefix.range.begin);
        previous.has_value() && *previous == '.') {
        return IdeCompletionContextKind::member;
    }
    if (ide_line_contains_module_path_keyword(ide_line_prefix(text, prefix.range.begin))) {
        return IdeCompletionContextKind::module_path;
    }
    if (ide_offset_is_trait_bound_completion_context(snapshot, prefix)) {
        return IdeCompletionContextKind::trait_bound;
    }
    return ide_offset_in_function_body(snapshot, offset) ? IdeCompletionContextKind::expression
                                                         : IdeCompletionContextKind::item;
}

[[nodiscard]] bool ide_completion_symbol_visible(
    const IdeSymbol& symbol, const IdeCompletionContextKind context, const base::usize offset) noexcept
{
    if (context == IdeCompletionContextKind::member) {
        return symbol.kind == IDE_SYMBOL_KIND_METHOD || symbol.kind == IDE_SYMBOL_KIND_STRUCT_FIELD
            || symbol.kind == IDE_SYMBOL_KIND_ENUM_CASE || symbol.kind == IDE_SYMBOL_KIND_IMPL_METHOD
            || symbol.kind == IDE_SYMBOL_KIND_TRAIT_METHOD || symbol.kind == IDE_SYMBOL_KIND_ASSOCIATED_TYPE;
    }
    if (context == IdeCompletionContextKind::trait_bound) {
        return !symbol.local && symbol.kind == IDE_SYMBOL_KIND_TRAIT;
    }
    if (symbol.kind == IDE_SYMBOL_KIND_STRUCT_FIELD && context != IdeCompletionContextKind::member) {
        return false;
    }
    if (symbol.kind == IDE_SYMBOL_KIND_TRAIT_METHOD || symbol.kind == IDE_SYMBOL_KIND_ASSOCIATED_TYPE) {
        return false;
    }
    if (!symbol.local) {
        return true;
    }
    return range_contains_offset(symbol.scope_range, offset) && symbol.range.begin <= offset;
}

[[nodiscard]] std::string ide_completion_identity(const IdeCompletionItem& item)
{
    std::string identity;
    identity.reserve(item.label.size() + item.kind.size() + IDE_DETAIL_TYPE_SEPARATOR.size());
    identity.append(item.label);
    identity.push_back('\x1F');
    identity.append(item.kind);
    identity.push_back('\x1F');
    identity.append(std::to_string(item.definition.global_id));
    identity.push_back('\x1F');
    identity.append(std::to_string(item.member.global_id));
    return identity;
}

void ide_push_completion_item(
    std::vector<IdeCompletionItem>& items, std::unordered_map<std::string, bool>& seen, IdeCompletionItem item)
{
    const std::string identity = ide_completion_identity(item);
    if (seen.contains(identity)) {
        return;
    }
    seen.emplace(identity, true);
    items.push_back(std::move(item));
}

void ide_append_keyword_completions(std::vector<IdeCompletionItem>& items, std::unordered_map<std::string, bool>& seen,
    const IdeCompletionContextKind context, const base::SourceRange replacement_range, const std::string_view prefix)
{
    const auto append_keywords = [&](const std::span<const std::string_view> keywords) {
        for (const std::string_view keyword : keywords) {
            if (!ide_completion_label_matches_prefix(keyword, prefix)) {
                continue;
            }
            ide_push_completion_item(items, seen,
                IdeCompletionItem{
                    context,
                    {},
                    {},
                    {},
                    replacement_range,
                    std::string(keyword),
                    std::string(IDE_COMPLETION_KIND_KEYWORD),
                    std::string(IDE_COMPLETION_DETAIL_KEYWORD),
                    IDE_PRIMARY_PART_INDEX,
                    false,
                    false,
                });
        }
    };

    if (context == IdeCompletionContextKind::item || context == IdeCompletionContextKind::module_path) {
        append_keywords(IDE_ITEM_COMPLETION_KEYWORDS);
    }
    if (context == IdeCompletionContextKind::expression) {
        append_keywords(IDE_EXPRESSION_COMPLETION_KEYWORDS);
    }
}

void ide_append_symbol_completions(std::vector<IdeCompletionItem>& items, std::unordered_map<std::string, bool>& seen,
    const SymbolIndex& index, const IdeCompletionContextKind context, const base::SourceRange replacement_range,
    const std::string_view prefix, const base::usize offset)
{
    for (const IdeSymbol& symbol : index.symbols) {
        if (!ide_completion_symbol_visible(symbol, context, offset)
            || !ide_completion_label_matches_prefix(symbol.name, prefix)) {
            continue;
        }
        ide_push_completion_item(items, seen,
            IdeCompletionItem{
                context,
                symbol.key,
                symbol.member,
                symbol.generic_instance,
                replacement_range,
                symbol.name,
                symbol.kind,
                symbol.detail,
                symbol.part_index,
                symbol.local,
                symbol.checked,
            });
    }
}

[[nodiscard]] bool ide_source_has_explicit_type_annotation_after(
    const IdeSnapshot& snapshot, const base::SourceRange range) noexcept
{
    const std::string_view text = snapshot.sources.text(snapshot.source_id);
    base::usize offset = std::min(range.end, text.size());
    while (offset < text.size() && (text[offset] == ' ' || text[offset] == '\t')) {
        ++offset;
    }
    return offset < text.size() && text[offset] == ':';
}

[[nodiscard]] std::optional<std::string> ide_type_hint_from_detail(const std::string_view detail)
{
    const base::usize separator = detail.find(IDE_DETAIL_TYPE_SEPARATOR);
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    const base::usize resource =
        detail.find(IDE_DETAIL_RESOURCE_SEPARATOR, separator + IDE_DETAIL_TYPE_SEPARATOR.size());
    std::string label;
    label.append(IDE_DETAIL_TYPE_SEPARATOR);
    label.append(detail.substr(separator + IDE_DETAIL_TYPE_SEPARATOR.size(),
        resource == std::string_view::npos ? std::string_view::npos
                                           : resource - separator - IDE_DETAIL_TYPE_SEPARATOR.size()));
    return label;
}

[[nodiscard]] bool ide_token_kind_is_keyword(const syntax::TokenKind kind) noexcept
{
    return kind >= syntax::TokenKind::kw_module && kind <= syntax::TokenKind::kw_strraw;
}

[[nodiscard]] bool ide_token_kind_is_builtin_type(const syntax::TokenKind kind) noexcept
{
    return kind == syntax::TokenKind::kw_void || kind == syntax::TokenKind::kw_bool || kind == syntax::TokenKind::kw_i8
        || kind == syntax::TokenKind::kw_u8 || kind == syntax::TokenKind::kw_i16 || kind == syntax::TokenKind::kw_u16
        || kind == syntax::TokenKind::kw_i32 || kind == syntax::TokenKind::kw_u32 || kind == syntax::TokenKind::kw_i64
        || kind == syntax::TokenKind::kw_u64 || kind == syntax::TokenKind::kw_isize
        || kind == syntax::TokenKind::kw_usize || kind == syntax::TokenKind::kw_f32 || kind == syntax::TokenKind::kw_f64
        || kind == syntax::TokenKind::kw_str || kind == syntax::TokenKind::kw_char;
}

[[nodiscard]] bool ide_token_kind_is_number(const syntax::TokenKind kind) noexcept
{
    return kind == syntax::TokenKind::integer_literal || kind == syntax::TokenKind::float_literal;
}

[[nodiscard]] bool ide_token_kind_is_string_like(const syntax::TokenKind kind) noexcept
{
    return kind == syntax::TokenKind::string_literal || kind == syntax::TokenKind::c_string_literal
        || kind == syntax::TokenKind::raw_string_literal || kind == syntax::TokenKind::byte_string_literal
        || kind == syntax::TokenKind::byte_literal || kind == syntax::TokenKind::char_literal;
}

[[nodiscard]] bool ide_token_kind_is_comment(const syntax::TokenKind kind) noexcept
{
    return kind == syntax::TokenKind::line_comment || kind == syntax::TokenKind::block_comment;
}

[[nodiscard]] bool ide_token_kind_is_punctuation(const syntax::TokenKind kind) noexcept
{
    return kind == syntax::TokenKind::l_paren || kind == syntax::TokenKind::r_paren
        || kind == syntax::TokenKind::l_brace || kind == syntax::TokenKind::r_brace
        || kind == syntax::TokenKind::l_bracket || kind == syntax::TokenKind::r_bracket
        || kind == syntax::TokenKind::comma || kind == syntax::TokenKind::dot || kind == syntax::TokenKind::semicolon
        || kind == syntax::TokenKind::colon || kind == syntax::TokenKind::colon_colon
        || kind == syntax::TokenKind::ellipsis;
}

[[nodiscard]] std::string_view ide_symbol_kind_token_type(const std::string_view kind) noexcept
{
    if (kind == IDE_SYMBOL_KIND_FUNCTION) {
        return IDE_TOKEN_TYPE_FUNCTION;
    }
    if (kind == IDE_SYMBOL_KIND_METHOD || kind == IDE_SYMBOL_KIND_IMPL_METHOD || kind == IDE_SYMBOL_KIND_TRAIT_METHOD) {
        return IDE_TOKEN_TYPE_METHOD;
    }
    if (kind == IDE_SYMBOL_KIND_PARAMETER) {
        return IDE_TOKEN_TYPE_PARAMETER;
    }
    if (kind == IDE_SYMBOL_KIND_TRAIT) {
        return IDE_TOKEN_TYPE_INTERFACE;
    }
    if (kind == IDE_SYMBOL_KIND_STRUCT || kind == IDE_SYMBOL_KIND_OPAQUE_STRUCT || kind == IDE_SYMBOL_KIND_TYPE_ALIAS
        || kind == IDE_SYMBOL_KIND_ASSOCIATED_TYPE) {
        return IDE_TOKEN_TYPE_TYPE;
    }
    if (kind == IDE_SYMBOL_KIND_ENUM) {
        return IDE_TOKEN_TYPE_ENUM;
    }
    if (kind == IDE_SYMBOL_KIND_ENUM_CASE) {
        return IDE_TOKEN_TYPE_ENUM_MEMBER;
    }
    if (kind == IDE_SYMBOL_KIND_GENERIC_TEMPLATE) {
        return IDE_TOKEN_TYPE_TYPE_PARAMETER;
    }
    if (kind == IDE_SYMBOL_KIND_STRUCT_FIELD) {
        return IDE_TOKEN_TYPE_PROPERTY;
    }
    return IDE_TOKEN_TYPE_VARIABLE;
}

[[nodiscard]] std::string_view ide_syntax_token_type(const syntax::TokenKind kind) noexcept
{
    if (ide_token_kind_is_comment(kind)) {
        return IDE_TOKEN_TYPE_COMMENT;
    }
    if (ide_token_kind_is_builtin_type(kind)) {
        return IDE_TOKEN_TYPE_TYPE;
    }
    if (ide_token_kind_is_keyword(kind)) {
        return IDE_TOKEN_TYPE_KEYWORD;
    }
    if (ide_token_kind_is_number(kind)) {
        return IDE_TOKEN_TYPE_NUMBER;
    }
    if (ide_token_kind_is_string_like(kind)) {
        return IDE_TOKEN_TYPE_STRING;
    }
    if (ide_token_kind_is_punctuation(kind)) {
        return IDE_TOKEN_TYPE_PUNCTUATION;
    }
    return IDE_TOKEN_TYPE_OPERATOR;
}

void ide_append_symbol_token_modifiers(
    std::vector<std::string>& modifiers, const IdeSymbol& symbol, const syntax::Token& token)
{
    if (same_range(symbol.range, token.range)) {
        modifiers.push_back(std::string(IDE_TOKEN_MODIFIER_DECLARATION));
        modifiers.push_back(std::string(IDE_TOKEN_MODIFIER_DEFINITION));
    }
    if (symbol.kind == IDE_SYMBOL_KIND_ENUM_CASE || symbol.kind == IDE_SYMBOL_KIND_CONST) {
        modifiers.push_back(std::string(IDE_TOKEN_MODIFIER_READONLY));
    }
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
                populate_ir_facts(snapshot);
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
    if (const std::optional<IdeSymbol> trait_method_symbol =
            trait_method_call_symbol_at_offset(snapshot, *info, offset)) {
        return make_definition_from_symbol(*trait_method_symbol);
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
        const IdeSymbol* const token_symbol = best_symbol_for_identifier(index, token.text(), token.range.begin);
        if (symbol == nullptr
            || (token_symbol != nullptr && ide_symbols_reference_same_entity(*token_symbol, *symbol))) {
            references.push_back(IdeReference{
                token.range,
                info->text,
                symbol != nullptr
                    && same_range(token.range, token_symbol != nullptr ? token_symbol->range : symbol->range),
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
        const std::optional<IdeSymbol> trait_method_symbol =
            trait_method_call_symbol_at_offset(snapshot, *info, offset);
        const IdeSymbol* const symbol = trait_method_symbol.has_value()
            ? &*trait_method_symbol
            : best_symbol_for_identifier(index, info->text, offset);
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

std::vector<IdeCompletionItem> completion_items_at_offset(const IdeSnapshot& snapshot, const base::usize offset)
{
    if (!snapshot.parsed) {
        return {};
    }
    const IdeCompletionPrefix prefix = ide_completion_prefix_at_offset(snapshot, offset);
    const IdeCompletionContextKind context = ide_completion_context_at_offset(snapshot, prefix, offset);
    const SymbolIndex index = build_symbol_index(snapshot);

    std::vector<IdeCompletionItem> items;
    std::unordered_map<std::string, bool> seen;
    ide_append_symbol_completions(items, seen, index, context, prefix.range, prefix.text, offset);
    ide_append_keyword_completions(items, seen, context, prefix.range, prefix.text);
    std::ranges::sort(items, [](const IdeCompletionItem& lhs, const IdeCompletionItem& rhs) {
        if (lhs.local != rhs.local) {
            return lhs.local && !rhs.local;
        }
        if (lhs.checked != rhs.checked) {
            return lhs.checked && !rhs.checked;
        }
        if (lhs.label == rhs.label) {
            return lhs.kind < rhs.kind;
        }
        return lhs.label < rhs.label;
    });
    return items;
}

std::vector<IdeSemanticToken> semantic_tokens(const IdeSnapshot& snapshot)
{
    if (!snapshot.lexed) {
        return {};
    }

    const SymbolIndex index = build_symbol_index(snapshot);
    std::vector<IdeSemanticToken> result;
    result.reserve(snapshot.lossless.token_count());
    for (const syntax::Token& token : snapshot.lossless.tokens()) {
        if (token.kind == syntax::TokenKind::eof || token.kind == syntax::TokenKind::whitespace) {
            continue;
        }

        IdeSemanticToken semantic;
        semantic.range = token.range;
        semantic.text = std::string(token.text());
        semantic.token_type = std::string(ide_syntax_token_type(token.kind));

        if (token.kind == syntax::TokenKind::identifier) {
            const IdeSymbol* const symbol = best_symbol_for_identifier(index, token.text(), token.range.begin);
            if (symbol != nullptr) {
                semantic.definition = symbol->key;
                semantic.member = symbol->member;
                semantic.token_type = std::string(ide_symbol_kind_token_type(symbol->kind));
                semantic.checked = symbol->checked;
                ide_append_symbol_token_modifiers(semantic.modifiers, *symbol, token);
            } else {
                semantic.token_type = std::string(IDE_TOKEN_TYPE_VARIABLE);
            }
        }

        result.push_back(std::move(semantic));
    }
    std::ranges::sort(result, [](const IdeSemanticToken& lhs, const IdeSemanticToken& rhs) {
        if (lhs.range.source.value == rhs.range.source.value) {
            return lhs.range.begin < rhs.range.begin;
        }
        return lhs.range.source.value < rhs.range.source.value;
    });
    return result;
}

std::vector<IdeInlayHint> inlay_hints(const IdeSnapshot& snapshot)
{
    if (!snapshot.checked_semantics) {
        return {};
    }

    const SymbolIndex index = build_symbol_index(snapshot);
    std::vector<IdeInlayHint> result;
    for (const IdeSymbol& symbol : index.symbols) {
        if (!symbol.checked || symbol.kind != IDE_SYMBOL_KIND_LOCAL
            || ide_source_has_explicit_type_annotation_after(snapshot, symbol.range)) {
            continue;
        }
        const std::optional<std::string> label = ide_type_hint_from_detail(symbol.detail);
        if (!label.has_value()) {
            continue;
        }
        result.push_back(IdeInlayHint{
            base::SourceRange{symbol.range.source, symbol.range.end, symbol.range.end},
            *label,
            std::string(IDE_INLAY_HINT_KIND_TYPE),
            true,
        });
    }
    std::ranges::sort(result, [](const IdeInlayHint& lhs, const IdeInlayHint& rhs) {
        if (lhs.position.source.value == rhs.position.source.value) {
            return lhs.position.begin < rhs.position.begin;
        }
        return lhs.position.source.value < rhs.position.source.value;
    });
    return result;
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
            const query::BodySlotKind slot = item->is_trait_default_method ? query::BodySlotKind::trait_default_method
                                                                           : query::BodySlotKind::function_body;
            const query::BodyKey body = query::body_key(definition, slot);
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
