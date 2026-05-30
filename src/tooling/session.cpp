#include <aurex/base/diagnostic.hpp>
#include <aurex/project/project_model.hpp>
#include <aurex/query/query_key.hpp>
#include <aurex/syntax/token.hpp>
#include <aurex/tooling/session.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace aurex::tooling {
namespace {

constexpr std::string_view TOOLING_DEFAULT_PACKAGE_IDENTITY = "ide";
constexpr std::string_view TOOLING_PROJECT_IMPORT_PACKAGE_PREFIX = "tooling-import-root:";
constexpr std::string_view TOOLING_PROJECT_TARGET_EMIT_KIND = "tooling-snapshot";
constexpr std::string_view TOOLING_PROJECT_TARGET_OPTIMIZATION = "none";
constexpr std::string_view TOOLING_PROJECT_DIAGNOSTIC_FORMAT = "tooling";
constexpr std::string_view TOOLING_FILE_URI_PREFIX = "file://";
constexpr std::string_view TOOLING_FILE_URI_LOCALHOST_PREFIX = "file://localhost";
constexpr std::string_view TOOLING_URI_HEX_DIGITS = "0123456789ABCDEF";
constexpr std::string_view TOOLING_INCREMENTAL_STATUS_CLEAN_BUILD = "clean_build";
constexpr std::string_view TOOLING_INCREMENTAL_STATUS_CACHED_SNAPSHOT = "cached_snapshot";
constexpr std::string_view TOOLING_INCREMENTAL_STATUS_PREVIOUS_CONTEXT = "previous_context";
constexpr std::string_view TOOLING_INCREMENTAL_STATUS_REJECTED_MISMATCHED_PREVIOUS = "rejected_mismatched_previous";
constexpr std::string_view TOOLING_INCREMENTAL_STATUS_REJECTED_STALE_PREVIOUS = "rejected_stale_previous";
constexpr std::string_view TOOLING_INCREMENTAL_STATUS_REJECTED_MALFORMED_PREVIOUS = "rejected_malformed_previous";
constexpr std::string_view TOOLING_INCREMENTAL_REASON_NO_PREVIOUS = "no previous snapshot context";
constexpr std::string_view TOOLING_INCREMENTAL_REASON_MISMATCHED_PREVIOUS =
    "previous snapshot context does not match document";
constexpr std::string_view TOOLING_INCREMENTAL_REASON_STALE_PREVIOUS =
    "previous snapshot version is not older than current version";
constexpr std::string_view TOOLING_INCREMENTAL_REASON_MALFORMED_PREVIOUS = "previous snapshot context is malformed";
constexpr std::string_view TOOLING_SYMBOL_KIND_KEYWORD = "keyword";
constexpr std::string_view TOOLING_CODE_ACTION_KIND_QUICKFIX = "quickfix";
constexpr std::string_view TOOLING_CODE_ACTION_DATA_LOOKUP_SUGGESTION = "lookup-suggestion";
constexpr std::string_view TOOLING_RENAME_CONFLICT_INVALID_IDENTIFIER = "new name is not a valid identifier";
constexpr std::string_view TOOLING_RENAME_CONFLICT_VISIBLE_SYMBOL = "new name conflicts with a visible symbol";
constexpr std::string_view TOOLING_RENAME_CONFLICT_KEYWORD = "new name is a reserved keyword";
constexpr std::string_view TOOLING_SUGGESTION_MESSAGE_PREFIX = "did you mean `";
constexpr char TOOLING_SUGGESTION_MESSAGE_END = '`';
constexpr char TOOLING_DOCUMENT_KEY_SEPARATOR = '\x1F';
constexpr char TOOLING_URI_PERCENT = '%';
constexpr char TOOLING_URI_SPACE = ' ';
constexpr char TOOLING_URI_SLASH = '/';
constexpr char TOOLING_URI_COLON = ':';
constexpr char TOOLING_URI_BACKSLASH = '\\';
constexpr base::usize TOOLING_URI_PERCENT_ESCAPE_LENGTH = 3;
constexpr base::usize TOOLING_HEX_DIGIT_COUNT = 16;
constexpr int TOOLING_DECIMAL_DIGIT_COUNT = 10;
constexpr base::usize TOOLING_FIRST_HEX_NIBBLE_SHIFT = 4;
constexpr base::u32 TOOLING_PRIMARY_PART_INDEX = 0;

constexpr auto TOOLING_RESERVED_WORDS = std::to_array<std::string_view>({
    "alignof",
    "as",
    "bitcast",
    "bool",
    "cast",
    "char",
    "const",
    "else",
    "enum",
    "extern",
    "f32",
    "f64",
    "false",
    "fn",
    "for",
    "i16",
    "i32",
    "i64",
    "i8",
    "if",
    "impl",
    "import",
    "in",
    "isize",
    "let",
    "match",
    "module",
    "null",
    "priv",
    "ptraddr",
    "ptrat",
    "ptrcast",
    "pub",
    "return",
    "sizeof",
    "slicelen",
    "sliceptr",
    "str",
    "struct",
    "true",
    "type",
    "u16",
    "u32",
    "u64",
    "u8",
    "unsafe",
    "usize",
    "var",
    "void",
    "while",
});

[[nodiscard]] std::string_view tooling_package_or_default(const std::string_view package) noexcept
{
    return package.empty() ? TOOLING_DEFAULT_PACKAGE_IDENTITY : package;
}

[[nodiscard]] query::PackageKey tooling_package_key(const std::string_view package) noexcept
{
    const std::array<std::string_view, 1> parts{tooling_package_or_default(package)};
    return query::package_key(parts);
}

[[nodiscard]] std::filesystem::path tooling_canonical_or_absolute(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical;
    }
    return std::filesystem::absolute(path);
}

[[nodiscard]] std::string tooling_normalize_path(const std::string_view path, const ToolingProjectConfig& config)
{
    std::filesystem::path candidate{std::string(path)};
    if (candidate.is_relative() && !config.root_path.empty()) {
        candidate = std::filesystem::path(config.root_path) / candidate;
    }
    return tooling_canonical_or_absolute(candidate).generic_string();
}

[[nodiscard]] std::filesystem::path tooling_project_root_path(const ToolingProjectConfig& config)
{
    if (!config.root_path.empty()) {
        return tooling_canonical_or_absolute(std::filesystem::path(config.root_path));
    }
    if (!config.source_root.empty()) {
        return tooling_canonical_or_absolute(std::filesystem::path(config.source_root));
    }
    return tooling_canonical_or_absolute(std::filesystem::current_path());
}

[[nodiscard]] std::filesystem::path tooling_project_source_root(const ToolingProjectConfig& config)
{
    if (!config.source_root.empty()) {
        std::filesystem::path source_root{config.source_root};
        if (source_root.is_relative() && !config.root_path.empty()) {
            source_root = std::filesystem::path(config.root_path) / source_root;
        }
        return tooling_canonical_or_absolute(source_root);
    }
    return tooling_project_root_path(config);
}

[[nodiscard]] project::ProjectTargetConfig tooling_project_target_config()
{
    return project::ProjectTargetConfig{
        std::string(TOOLING_PROJECT_TARGET_EMIT_KIND),
        std::string(TOOLING_PROJECT_TARGET_OPTIMIZATION),
        {},
        {},
    };
}

[[nodiscard]] project::ProjectCommandOptions tooling_project_command_options()
{
    return project::ProjectCommandOptions{
        std::string(TOOLING_PROJECT_DIAGNOSTIC_FORMAT),
        true,
    };
}

[[nodiscard]] std::vector<project::ProjectImportRoot> tooling_project_import_roots(const ToolingProjectConfig& config)
{
    std::vector<project::ProjectImportRoot> roots;
    roots.reserve(config.import_paths.size());
    for (const std::string& import_path : config.import_paths) {
        const std::filesystem::path normalized{tooling_normalize_path(import_path, config)};
        std::string identity;
        identity.reserve(TOOLING_PROJECT_IMPORT_PACKAGE_PREFIX.size() + normalized.generic_string().size());
        identity.append(TOOLING_PROJECT_IMPORT_PACKAGE_PREFIX);
        identity.append(normalized.generic_string());
        const std::array<std::string_view, 1> parts{std::string_view{identity}};
        roots.push_back(project::ProjectImportRoot{
            normalized,
            identity,
            query::package_key(parts),
        });
    }
    return roots;
}

[[nodiscard]] int tooling_hex_value(const char ch) noexcept
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + TOOLING_DECIMAL_DIGIT_COUNT;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + TOOLING_DECIMAL_DIGIT_COUNT;
    }
    return -1;
}

[[nodiscard]] std::string tooling_percent_decode(const std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (base::usize index = 0; index < text.size(); ++index) {
        if (text[index] == TOOLING_URI_PERCENT && index + TOOLING_URI_PERCENT_ESCAPE_LENGTH <= text.size()) {
            const int high = tooling_hex_value(text[index + 1U]);
            const int low = tooling_hex_value(text[index + 2U]);
            if (high >= 0 && low >= 0) {
                const int decoded = (high << TOOLING_FIRST_HEX_NIBBLE_SHIFT) | low;
                result.push_back(static_cast<char>(decoded));
                index += TOOLING_URI_PERCENT_ESCAPE_LENGTH - 1U;
                continue;
            }
        }
        result.push_back(text[index]);
    }
    return result;
}

void tooling_append_percent_encoded(std::string& out, const unsigned char ch)
{
    out.push_back(TOOLING_URI_PERCENT);
    out.push_back(TOOLING_URI_HEX_DIGITS[(ch >> TOOLING_FIRST_HEX_NIBBLE_SHIFT) % TOOLING_HEX_DIGIT_COUNT]);
    out.push_back(TOOLING_URI_HEX_DIGITS[ch % TOOLING_HEX_DIGIT_COUNT]);
}

[[nodiscard]] bool tooling_uri_char_is_plain(const unsigned char ch) noexcept
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '-'
        || ch == '_' || ch == '~' || ch == TOOLING_URI_SLASH || ch == TOOLING_URI_COLON;
}

[[nodiscard]] bool tooling_version_is_older(
    const ToolingDocumentVersion& previous, const ToolingDocumentVersion& current) noexcept
{
    if (previous.generation >= current.generation) {
        return false;
    }
    if (previous.client_version.has_value() && current.client_version.has_value()
        && *previous.client_version >= *current.client_version) {
        return false;
    }
    return true;
}

[[nodiscard]] bool tooling_incremental_input_shape_is_malformed(const ToolingIncrementalSnapshotInput& input) noexcept
{
    return input.previous_context_malformed
        || (input.previous_query_records == 0
            && (input.previous_dependency_edges != 0 || input.previous_semantic_facts != 0));
}

void tooling_fill_current_incremental_counts(ToolingIncrementalSnapshotResult& result, const IdeSnapshot& snapshot)
{
    result.current_query_records = snapshot.query.records.size();
    result.current_dependency_edges = snapshot.query.dependencies.size();
    result.current_semantic_facts = snapshot.query.semantic_facts.size();
}

[[nodiscard]] bool tooling_incremental_input_can_use_previous(const ToolingIncrementalSnapshotInput& input) noexcept
{
    return input.has_previous_snapshot && input.previous_document_matches && input.previous_version_is_older
        && !tooling_incremental_input_shape_is_malformed(input);
}

[[nodiscard]] IdeEditImpact tooling_full_document_edit_impact(const IdeSnapshot& snapshot)
{
    const base::SourceFile& file = snapshot.sources.get(snapshot.source_id);
    return edit_impact_for_range(snapshot, 0U, file.text().size());
}

[[nodiscard]] ToolingQueryReuseExecutionSummary tooling_reuse_execution_summary(const ToolingReusePlan& plan) noexcept
{
    ToolingQueryReuseExecutionSummary summary;
    summary.total_query_decisions = plan.query_plan.summary.total;
    summary.reused_query_records = plan.query_plan.reusable.size();
    summary.recomputed_query_records = plan.query_plan.recompute.size();
    summary.malformed_query_records = plan.query_plan.summary.malformed;
    summary.reused_semantic_facts = plan.summary.unchanged_facts;
    summary.recomputed_semantic_facts = plan.summary.recomputed_facts;
    summary.invalidated_semantic_facts = plan.summary.invalidated_facts;
    summary.malformed_semantic_facts = plan.summary.malformed_facts;
    summary.executed = plan.valid;
    summary.body_local = plan.summary.body_local;
    return summary;
}

void tooling_attach_reuse_execution(
    ToolingIncrementalSnapshotResult& result, const std::shared_ptr<const ToolingReusePlan>& plan)
{
    if (plan == nullptr) {
        return;
    }
    result.reuse_plan = plan;
    result.reuse_execution = tooling_reuse_execution_summary(*plan);
}

[[nodiscard]] ToolingSyntaxReuseExecutionSummary tooling_syntax_reuse_execution_summary(
    const syntax::LosslessSyntaxReuseStats& stats) noexcept
{
    return ToolingSyntaxReuseExecutionSummary{
        stats.previous_nodes,
        stats.current_nodes,
        stats.reused_nodes,
        stats.recomputed_nodes,
        stats.invalidated_nodes,
        stats.stable_key_collisions,
        stats.reused,
    };
}

void tooling_attach_syntax_reuse_execution(
    ToolingIncrementalSnapshotResult& result, const IdeSnapshot& previous, const IdeSnapshot& current) noexcept
{
    result.syntax_reuse = tooling_syntax_reuse_execution_summary(
        syntax::compare_lossless_stable_nodes(previous.lossless, current.lossless));
}

[[nodiscard]] bool tooling_text_edit_range_is_valid(
    const std::string_view text, const ToolingDocumentTextEdit& edit) noexcept
{
    return edit.begin <= text.size() && edit.removed_length <= text.size() - edit.begin;
}

[[nodiscard]] base::Result<std::string> tooling_apply_text_edit(
    const std::string_view text, const ToolingDocumentTextEdit& edit)
{
    if (!tooling_text_edit_range_is_valid(text, edit)) {
        return base::Result<std::string>::fail({base::ErrorCode::invalid_source, "tooling text edit is out of range"});
    }
    std::string result;
    result.reserve(text.size() - edit.removed_length + edit.inserted_text.size());
    result.append(text.substr(0U, edit.begin));
    result.append(edit.inserted_text);
    result.append(text.substr(edit.begin + edit.removed_length));
    return base::Result<std::string>::ok(std::move(result));
}

[[nodiscard]] ToolingDocumentTextEdit tooling_text_edit_from_replacement(const std::string_view before,
    const std::string_view after, const base::usize begin, const base::usize removed_length)
{
    ToolingDocumentTextEdit edit;
    edit.begin = begin;
    edit.removed_length = removed_length;
    if (!tooling_text_edit_range_is_valid(before, edit)) {
        return edit;
    }
    const base::usize prefix_size = begin;
    const base::usize suffix_size = before.size() - begin - removed_length;
    if (after.size() < prefix_size + suffix_size) {
        return edit;
    }
    const base::usize inserted_size = after.size() - prefix_size - suffix_size;
    edit.inserted_text = std::string(after.substr(begin, inserted_size));
    return edit;
}

[[nodiscard]] std::string tooling_kind_for_item(const syntax::ItemKind kind)
{
    switch (kind) {
        case syntax::ItemKind::const_decl:
            return "const";
        case syntax::ItemKind::type_alias:
            return "type_alias";
        case syntax::ItemKind::struct_decl:
            return "struct";
        case syntax::ItemKind::enum_decl:
            return "enum";
        case syntax::ItemKind::trait_decl:
            return "trait";
        case syntax::ItemKind::opaque_struct_decl:
            return "opaque_struct";
        case syntax::ItemKind::fn_decl:
            return "function";
        case syntax::ItemKind::extern_block:
            return "extern_block";
        case syntax::ItemKind::impl_block:
            return "impl_block";
    }
    return "item";
}

[[nodiscard]] std::string tooling_kind_for_definition(const query::DefKind kind)
{
    switch (kind) {
        case query::DefKind::function:
            return "function";
        case query::DefKind::method:
            return "method";
        case query::DefKind::value:
            return "value";
        case query::DefKind::const_:
            return "const";
        case query::DefKind::global:
            return "global";
        case query::DefKind::type_alias:
            return "type_alias";
        case query::DefKind::struct_:
            return "struct";
        case query::DefKind::enum_:
            return "enum";
        case query::DefKind::enum_case:
            return "enum_case";
        case query::DefKind::struct_field:
            return "struct_field";
        case query::DefKind::generic_template:
            return "generic_template";
        case query::DefKind::trait_:
            return "trait";
        case query::DefKind::trait_method:
            return "trait_method";
        case query::DefKind::associated_type:
            return "associated_type";
        case query::DefKind::associated_const:
            return "associated_const";
        case query::DefKind::synthetic:
            return "synthetic";
        case query::DefKind::invalid:
            return "invalid";
    }
    return "definition";
}

[[nodiscard]] const base::SourceFile* tooling_source_file_for_range(
    const IdeSnapshot& snapshot, const base::SourceRange& range) noexcept
{
    const std::span<const base::SourceFile> files = snapshot.sources.files();
    if (range.source.value >= files.size()) {
        return nullptr;
    }
    return &files[range.source.value];
}

[[nodiscard]] ToolingTextRange tooling_text_range_for_snapshot(
    const IdeSnapshot& snapshot, const base::SourceRange& range)
{
    ToolingTextRange result;
    result.range = range;
    const base::SourceFile* const file = tooling_source_file_for_range(snapshot, range);
    if (file == nullptr) {
        return result;
    }
    result.path = std::string(file->path());
    result.start = file->line_column(range.begin);
    result.end = file->line_column(range.end);
    return result;
}

[[nodiscard]] std::string tooling_stable_key_or_empty(const query::DefKey key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string tooling_stable_key_or_empty(const query::MemberKey key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string tooling_stable_key_or_empty(const query::QueryKey key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string tooling_stable_key_or_empty(const query::BodyKey key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string tooling_stable_key_or_empty(const query::GenericInstanceKey& key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] bool tooling_identifier_start(const char ch) noexcept
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

[[nodiscard]] bool tooling_identifier_continue(const char ch) noexcept
{
    return tooling_identifier_start(ch) || (ch >= '0' && ch <= '9');
}

[[nodiscard]] bool tooling_is_reserved_word(const std::string_view text) noexcept
{
    return std::ranges::find(TOOLING_RESERVED_WORDS, text) != TOOLING_RESERVED_WORDS.end();
}

[[nodiscard]] bool tooling_is_valid_identifier(const std::string_view text) noexcept
{
    if (text.empty() || !tooling_identifier_start(text.front()) || tooling_is_reserved_word(text)) {
        return false;
    }
    for (const char ch : text.substr(1U)) {
        if (!tooling_identifier_continue(ch)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] ToolingAstNode tooling_project_ast_node(const IdeSnapshot& snapshot, const IdeAstNodeInfo& node)
{
    return ToolingAstNode{
        node.kind,
        tooling_text_range_for_snapshot(snapshot, node.range),
        node.name,
        node.detail,
        tooling_stable_key_or_empty(node.definition),
        tooling_stable_key_or_empty(node.body),
        node.part_index,
        node.valid,
    };
}

[[nodiscard]] ToolingDiagnosticChild tooling_project_diagnostic_child(
    const IdeSnapshot& snapshot, const base::DiagnosticChild& child)
{
    return ToolingDiagnosticChild{
        child.severity,
        child.category,
        child.code,
        tooling_text_range_for_snapshot(snapshot, child.range),
        std::string(base::severity_name(child.severity)),
        std::string(base::diagnostic_category_name(child.category)),
        std::string(base::diagnostic_code_name(child.code)),
        child.message,
    };
}

[[nodiscard]] ToolingDefinition tooling_project_definition(const IdeSnapshot& snapshot, const IdeDefinition& definition)
{
    return ToolingDefinition{
        definition.key,
        definition.member,
        definition.generic_instance,
        tooling_text_range_for_snapshot(snapshot, definition.range),
        definition.name,
        definition.kind,
        tooling_stable_key_or_empty(definition.key),
        tooling_stable_key_or_empty(definition.member),
        tooling_stable_key_or_empty(definition.generic_instance),
        definition.part_index,
        definition.valid,
    };
}

[[nodiscard]] ToolingCompletionItem tooling_project_completion(
    const IdeSnapshot& snapshot, const IdeCompletionItem& item)
{
    return ToolingCompletionItem{
        item.context,
        tooling_text_range_for_snapshot(snapshot, item.replacement_range),
        item.label,
        item.kind,
        item.detail,
        tooling_stable_key_or_empty(item.definition),
        tooling_stable_key_or_empty(item.member),
        tooling_stable_key_or_empty(item.generic_instance),
        item.part_index,
        item.local,
        item.checked,
        false,
    };
}

[[nodiscard]] ToolingSemanticToken tooling_project_semantic_token(
    const IdeSnapshot& snapshot, const IdeSemanticToken& token)
{
    return ToolingSemanticToken{
        tooling_text_range_for_snapshot(snapshot, token.range),
        token.text,
        token.token_type,
        token.modifiers,
        tooling_stable_key_or_empty(token.definition),
        tooling_stable_key_or_empty(token.member),
        token.checked,
    };
}

[[nodiscard]] ToolingInlayHint tooling_project_inlay_hint(const IdeSnapshot& snapshot, const IdeInlayHint& hint)
{
    const std::string_view text = snapshot.sources.text(snapshot.source_id);
    return ToolingInlayHint{
        tooling_text_range_for_snapshot(snapshot, hint.position),
        tooling_position_for_offset(text, hint.position.begin),
        hint.label,
        hint.kind,
        hint.checked,
    };
}

[[nodiscard]] bool tooling_indexed_fact_is_definition_entry(const ToolingIndexedSemanticFact& fact) noexcept
{
    return fact.semantic_kind == IdeSemanticFactKind::item_signature
        || fact.semantic_kind == IdeSemanticFactKind::generic_template_signature;
}

[[nodiscard]] const ToolingIndexedSemanticFact* tooling_best_definition_fact(
    const std::vector<ToolingIndexedSemanticFact>& facts) noexcept
{
    for (const ToolingIndexedSemanticFact& fact : facts) {
        if (tooling_indexed_fact_is_definition_entry(fact)) {
            return &fact;
        }
    }
    return facts.empty() ? nullptr : &facts.front();
}

[[nodiscard]] ToolingDefinition tooling_project_definition_from_indexed_fact(const ToolingIndexedSemanticFact& fact)
{
    return ToolingDefinition{
        fact.definition,
        fact.member,
        fact.generic_instance,
        fact.range,
        fact.name,
        tooling_kind_for_definition(fact.definition.kind),
        fact.stable_definition_key,
        fact.stable_member_key,
        fact.stable_generic_instance_key,
        fact.part_index,
        true,
    };
}

[[nodiscard]] bool tooling_indexed_fact_matches_query(
    const ToolingIndexedSemanticFact& fact, const std::string_view query) noexcept
{
    return query.empty() || fact.name.find(query) != std::string::npos || fact.detail.find(query) != std::string::npos;
}

[[nodiscard]] ToolingWorkspaceSymbol tooling_project_workspace_symbol(const ToolingIndexedSemanticFact& fact)
{
    return ToolingWorkspaceSymbol{
        fact.document,
        fact.range,
        fact.name,
        tooling_kind_for_definition(fact.definition.kind),
        fact.detail,
        fact.document.path,
        fact.stable_query_key,
        fact.stable_definition_key,
        fact.stable_member_key,
        fact.stable_generic_instance_key,
        fact.part_index,
        fact.checked,
    };
}

[[nodiscard]] bool tooling_completion_label_matches_prefix(
    const std::string_view label, const std::string_view prefix) noexcept
{
    return prefix.empty() || (label.size() >= prefix.size() && label.substr(0U, prefix.size()) == prefix);
}

[[nodiscard]] std::string tooling_completion_identity(const ToolingCompletionItem& item)
{
    std::string identity;
    identity.reserve(item.label.size() + item.kind.size() + item.stable_definition_key.size()
        + item.stable_member_key.size() + item.stable_generic_instance_key.size());
    identity.append(item.label);
    identity.push_back(TOOLING_DOCUMENT_KEY_SEPARATOR);
    identity.append(item.kind);
    identity.push_back(TOOLING_DOCUMENT_KEY_SEPARATOR);
    identity.append(item.stable_definition_key);
    identity.push_back(TOOLING_DOCUMENT_KEY_SEPARATOR);
    identity.append(item.stable_member_key);
    identity.push_back(TOOLING_DOCUMENT_KEY_SEPARATOR);
    identity.append(item.stable_generic_instance_key);
    return identity;
}

void tooling_push_completion_if_new(
    std::vector<ToolingCompletionItem>& completions, std::vector<std::string>& seen, ToolingCompletionItem item)
{
    const std::string identity = tooling_completion_identity(item);
    if (std::ranges::find(seen, identity) != seen.end()) {
        return;
    }
    seen.push_back(identity);
    completions.push_back(std::move(item));
}

[[nodiscard]] ToolingCompletionItem tooling_workspace_completion_item(const ToolingIndexedSemanticFact& fact,
    const IdeCompletionContextKind context, const ToolingTextRange& replacement_range)
{
    return ToolingCompletionItem{
        context,
        replacement_range,
        fact.name,
        tooling_kind_for_definition(fact.definition.kind),
        fact.detail,
        fact.stable_definition_key,
        fact.stable_member_key,
        fact.stable_generic_instance_key,
        fact.part_index,
        false,
        fact.checked,
        true,
    };
}

void tooling_append_workspace_completions(std::vector<ToolingCompletionItem>& completions,
    std::vector<std::string>& seen, const std::vector<ToolingIndexedSemanticFact>& facts,
    const IdeCompletionContextKind context, const ToolingTextRange& replacement_range, const std::string_view prefix)
{
    if (context == IdeCompletionContextKind::member || context == IdeCompletionContextKind::module_path) {
        return;
    }
    for (const ToolingIndexedSemanticFact& fact : facts) {
        if (!tooling_indexed_fact_is_definition_entry(fact)
            || !tooling_completion_label_matches_prefix(fact.name, prefix)) {
            continue;
        }
        tooling_push_completion_if_new(
            completions, seen, tooling_workspace_completion_item(fact, context, replacement_range));
    }
}

[[nodiscard]] const IdeSemanticFact* tooling_semantic_fact_for_definition(
    const IdeSnapshot& snapshot, const IdeDefinition& definition) noexcept
{
    for (const IdeSemanticFact& fact : snapshot.query.semantic_facts) {
        if (query::is_valid(definition.key) && fact.definition == definition.key) {
            return &fact;
        }
        if (fact.name == definition.name && fact.range.source.value == definition.range.source.value
            && fact.range.begin == definition.range.begin && fact.range.end == definition.range.end) {
            return &fact;
        }
    }
    return nullptr;
}

[[nodiscard]] base::SourceRange tooling_name_range_in_item(
    const IdeSnapshot& snapshot, const syntax::ItemNode& item) noexcept
{
    for (const syntax::Token& token : snapshot.lossless.tokens()) {
        if (token.kind == syntax::TokenKind::identifier && token.text() == item.name
            && item.range.begin <= token.range.begin && token.range.end <= item.range.end) {
            return token.range;
        }
    }
    return item.range;
}

[[nodiscard]] ToolingDocumentSymbol tooling_symbol_from_fact(const IdeSnapshot& snapshot, const IdeSemanticFact& fact)
{
    ToolingDocumentSymbol symbol;
    symbol.name = fact.name;
    symbol.kind = tooling_kind_for_definition(fact.definition.kind);
    symbol.detail = fact.detail;
    symbol.range = tooling_text_range_for_snapshot(snapshot, fact.range);
    symbol.selection_range = symbol.range;
    symbol.query = fact.query;
    symbol.definition = fact.definition;
    symbol.stable_query_key = tooling_stable_key_or_empty(fact.query);
    symbol.stable_definition_key = tooling_stable_key_or_empty(fact.definition);
    symbol.part_index = fact.part_index;
    symbol.checked = fact.checked;
    return symbol;
}

[[nodiscard]] ToolingDocumentSymbol tooling_symbol_from_item(
    const IdeSnapshot& snapshot, const syntax::ItemNode& item, const base::u32 part_index)
{
    ToolingDocumentSymbol symbol;
    symbol.name = std::string(item.name);
    symbol.kind = tooling_kind_for_item(item.kind);
    symbol.detail = symbol.kind + " " + symbol.name;
    symbol.range = tooling_text_range_for_snapshot(snapshot, item.range);
    symbol.selection_range = tooling_text_range_for_snapshot(snapshot, tooling_name_range_in_item(snapshot, item));
    symbol.part_index = part_index;
    symbol.checked = false;
    return symbol;
}

[[nodiscard]] bool tooling_fact_is_document_symbol(const IdeSemanticFact& fact) noexcept
{
    return fact.kind == IdeSemanticFactKind::item_signature
        || fact.kind == IdeSemanticFactKind::generic_template_signature;
}

[[nodiscard]] base::u32 tooling_item_part_index(const IdeSnapshot& snapshot, const base::usize index) noexcept
{
    return index < snapshot.ast.item_part_indices.size() ? snapshot.ast.item_part_indices[index]
                                                         : TOOLING_PRIMARY_PART_INDEX;
}

[[nodiscard]] base::Result<void> tooling_missing_document_error()
{
    return base::Result<void>::fail({base::ErrorCode::invalid_source, "tooling document is not open"});
}

[[nodiscard]] bool tooling_same_reference_range(
    const ToolingReference& reference, const ToolingIndexedSemanticFact& fact) noexcept
{
    return reference.range.path == fact.range.path
        && reference.range.range.source.value == fact.range.range.source.value
        && reference.range.range.begin == fact.range.range.begin && reference.range.range.end == fact.range.range.end;
}

[[nodiscard]] bool tooling_same_range(const ToolingTextRange& lhs, const ToolingTextRange& rhs) noexcept
{
    return lhs.path == rhs.path && lhs.range.source.value == rhs.range.source.value
        && lhs.range.begin == rhs.range.begin && lhs.range.end == rhs.range.end;
}

void tooling_push_workspace_definition_references(
    std::vector<ToolingReference>& references, const std::vector<ToolingIndexedSemanticFact>& indexed_facts)
{
    for (const ToolingIndexedSemanticFact& fact : indexed_facts) {
        if (!tooling_indexed_fact_is_definition_entry(fact)) {
            continue;
        }
        const bool exists = std::ranges::any_of(references, [&fact](const ToolingReference& reference) {
            return tooling_same_reference_range(reference, fact);
        });
        if (exists) {
            continue;
        }
        references.push_back(ToolingReference{
            fact.range,
            fact.name,
            true,
        });
    }
}

[[nodiscard]] std::optional<std::string> tooling_lookup_suggestion_from_message(const std::string_view message)
{
    const base::usize prefix = message.find(TOOLING_SUGGESTION_MESSAGE_PREFIX);
    if (prefix == std::string_view::npos) {
        return std::nullopt;
    }
    const base::usize name_begin = prefix + TOOLING_SUGGESTION_MESSAGE_PREFIX.size();
    const base::usize name_end = message.find(TOOLING_SUGGESTION_MESSAGE_END, name_begin);
    if (name_end == std::string_view::npos || name_end == name_begin) {
        return std::nullopt;
    }
    return std::string(message.substr(name_begin, name_end - name_begin));
}

[[nodiscard]] std::string tooling_code_action_identity(const ToolingCodeAction& action)
{
    std::string identity;
    identity.reserve(action.kind.size() + action.title.size() + action.data.size());
    identity.append(action.kind);
    identity.push_back(TOOLING_DOCUMENT_KEY_SEPARATOR);
    identity.append(action.title);
    identity.push_back(TOOLING_DOCUMENT_KEY_SEPARATOR);
    identity.append(action.data);
    for (const ToolingCodeActionEdit& edit : action.edits) {
        identity.push_back(TOOLING_DOCUMENT_KEY_SEPARATOR);
        identity.append(tooling_document_store_key(edit.document));
        identity.push_back(TOOLING_DOCUMENT_KEY_SEPARATOR);
        identity.append(std::to_string(edit.range.range.begin));
        identity.push_back(TOOLING_DOCUMENT_KEY_SEPARATOR);
        identity.append(std::to_string(edit.range.range.end));
        identity.push_back(TOOLING_DOCUMENT_KEY_SEPARATOR);
        identity.append(edit.new_text);
    }
    return identity;
}

void tooling_push_code_action_if_new(
    std::vector<ToolingCodeAction>& actions, std::vector<std::string>& seen, ToolingCodeAction action)
{
    const std::string identity = tooling_code_action_identity(action);
    if (std::ranges::find(seen, identity) != seen.end()) {
        return;
    }
    seen.push_back(identity);
    actions.push_back(std::move(action));
}

void tooling_append_lookup_suggestion_action(std::vector<ToolingCodeAction>& actions, std::vector<std::string>& seen,
    const ToolingDocumentId& document, const ToolingTextRange& range, const base::Severity severity,
    const std::string_view message, const std::string_view code_name)
{
    if (severity != base::Severity::help) {
        return;
    }
    const std::optional<std::string> suggestion = tooling_lookup_suggestion_from_message(message);
    if (!suggestion.has_value()) {
        return;
    }
    ToolingCodeAction action;
    action.title = "Replace with `" + *suggestion + "`";
    action.kind = std::string(TOOLING_CODE_ACTION_KIND_QUICKFIX);
    action.diagnostic_code = std::string(code_name);
    action.data = std::string(TOOLING_CODE_ACTION_DATA_LOOKUP_SUGGESTION);
    action.edits.push_back(ToolingCodeActionEdit{
        document,
        range,
        *suggestion,
    });
    action.preferred = true;
    tooling_push_code_action_if_new(actions, seen, std::move(action));
}

[[nodiscard]] bool tooling_completion_matches_definition(
    const ToolingCompletionItem& item, const ToolingDefinition& definition) noexcept
{
    if (!definition.stable_member_key.empty()) {
        return item.stable_member_key == definition.stable_member_key;
    }
    if (!definition.stable_definition_key.empty()) {
        return item.stable_definition_key == definition.stable_definition_key;
    }
    return item.label == definition.name && item.kind == definition.kind;
}

void tooling_append_rename_conflict(std::vector<ToolingRenameConflict>& conflicts, const ToolingTextRange& range,
    const std::string_view name, const std::string_view kind, const std::string_view reason,
    const std::string_view stable_definition_key, const std::string_view stable_member_key)
{
    const bool exists = std::ranges::any_of(conflicts, [&range, name, kind, reason](const ToolingRenameConflict& item) {
        return tooling_same_range(item.range, range) && item.name == name && item.kind == kind && item.reason == reason;
    });
    if (exists) {
        return;
    }
    conflicts.push_back(ToolingRenameConflict{
        range,
        std::string(name),
        std::string(kind),
        std::string(reason),
        std::string(stable_definition_key),
        std::string(stable_member_key),
    });
}

void tooling_append_rename_conflicts_from_completion(std::vector<ToolingRenameConflict>& conflicts,
    const std::vector<ToolingCompletionItem>& completions, const ToolingDefinition& target,
    const std::string_view new_name)
{
    for (const ToolingCompletionItem& item : completions) {
        if (item.label != new_name || item.kind == TOOLING_SYMBOL_KIND_KEYWORD
            || tooling_completion_matches_definition(item, target)) {
            continue;
        }
        tooling_append_rename_conflict(conflicts, item.replacement_range, item.label, item.kind,
            TOOLING_RENAME_CONFLICT_VISIBLE_SYMBOL, item.stable_definition_key, item.stable_member_key);
    }
}

[[nodiscard]] ToolingDocumentId tooling_document_for_edit_range(
    const ToolingDocumentId& current_document, const ToolingTextRange& range, const ToolingProjectConfig& config)
{
    if (range.path.empty() || range.path == current_document.path) {
        return current_document;
    }
    return tooling_document_id_from_path(range.path, config);
}

void tooling_push_rename_edit_if_new(std::vector<ToolingRenameEdit>& edits, const ToolingDocumentId& document,
    const ToolingTextRange& range, const std::string_view new_name)
{
    const bool exists = std::ranges::any_of(edits, [&document, &range](const ToolingRenameEdit& edit) {
        return tooling_document_store_key(edit.document) == tooling_document_store_key(document)
            && tooling_same_range(edit.range, range);
    });
    if (exists) {
        return;
    }
    edits.push_back(ToolingRenameEdit{
        document,
        range,
        std::string(new_name),
    });
}

} // namespace

ToolingDocumentId tooling_document_id_from_path(const std::string_view path, const ToolingProjectConfig& config)
{
    ToolingDocumentId id;
    id.path = tooling_normalize_path(path, config);
    id.uri = tooling_file_uri_from_path(id.path);
    id.package_identity = std::string(tooling_package_or_default(config.package_identity));
    id.virtual_buffer_identity = id.uri;
    id.source_role = config.default_source_role;
    return id;
}

ToolingDocumentId tooling_document_id_from_uri(const std::string_view uri, const ToolingProjectConfig& config)
{
    const std::optional<std::string> path = tooling_path_from_file_uri(uri);
    ToolingDocumentId id = tooling_document_id_from_path(path.value_or(std::string(uri)), config);
    id.uri = std::string(uri);
    id.virtual_buffer_identity = id.uri;
    return id;
}

std::string tooling_document_store_key(const ToolingDocumentId& id)
{
    std::string key;
    key.reserve(id.package_identity.size() + id.path.size() + id.virtual_buffer_identity.size());
    key.append(id.package_identity);
    key.push_back(TOOLING_DOCUMENT_KEY_SEPARATOR);
    key.append(id.path);
    key.push_back(TOOLING_DOCUMENT_KEY_SEPARATOR);
    key.append(std::to_string(static_cast<base::u32>(id.source_role)));
    key.push_back(TOOLING_DOCUMENT_KEY_SEPARATOR);
    key.append(id.virtual_buffer_identity);
    return key;
}

std::string tooling_file_uri_from_path(const std::string_view path)
{
    std::string normalized = tooling_normalize_path(path, {});
    std::replace(normalized.begin(), normalized.end(), TOOLING_URI_BACKSLASH, TOOLING_URI_SLASH);
    std::string uri;
    uri.reserve(TOOLING_FILE_URI_PREFIX.size() + normalized.size());
    uri.append(TOOLING_FILE_URI_PREFIX);
    for (const unsigned char ch : normalized) {
        if (ch == TOOLING_URI_SPACE || !tooling_uri_char_is_plain(ch)) {
            tooling_append_percent_encoded(uri, ch);
            continue;
        }
        uri.push_back(static_cast<char>(ch));
    }
    return uri;
}

std::optional<std::string> tooling_path_from_file_uri(const std::string_view uri)
{
    std::string_view path = uri;
    if (uri.starts_with(TOOLING_FILE_URI_LOCALHOST_PREFIX)) {
        path = uri.substr(TOOLING_FILE_URI_LOCALHOST_PREFIX.size());
    } else if (uri.starts_with(TOOLING_FILE_URI_PREFIX)) {
        path = uri.substr(TOOLING_FILE_URI_PREFIX.size());
    } else {
        return std::nullopt;
    }
    return tooling_percent_decode(path);
}

base::usize tooling_offset_for_position(const std::string_view text, const ToolingSourcePosition position) noexcept
{
    base::usize line = 0;
    base::usize line_begin = 0;
    for (base::usize index = 0; index < text.size() && line < position.line; ++index) {
        if (text[index] == '\n') {
            ++line;
            line_begin = index + 1U;
        }
    }
    if (line < position.line) {
        return text.size();
    }
    base::usize line_end = text.size();
    for (base::usize index = line_begin; index < text.size(); ++index) {
        if (text[index] == '\n') {
            line_end = index;
            break;
        }
    }
    return std::min(line_begin + position.character, line_end);
}

ToolingSourcePosition tooling_position_for_offset(const std::string_view text, const base::usize offset) noexcept
{
    ToolingSourcePosition position;
    const base::usize limit = std::min(offset, text.size());
    base::usize line_begin = 0;
    for (base::usize index = 0; index < limit; ++index) {
        if (text[index] == '\n') {
            ++position.line;
            line_begin = index + 1U;
        }
    }
    position.character = limit - line_begin;
    return position;
}

std::string_view tooling_incremental_snapshot_status_name(const ToolingIncrementalSnapshotStatus status) noexcept
{
    switch (status) {
        case ToolingIncrementalSnapshotStatus::clean_build:
            return TOOLING_INCREMENTAL_STATUS_CLEAN_BUILD;
        case ToolingIncrementalSnapshotStatus::cached_snapshot:
            return TOOLING_INCREMENTAL_STATUS_CACHED_SNAPSHOT;
        case ToolingIncrementalSnapshotStatus::previous_context:
            return TOOLING_INCREMENTAL_STATUS_PREVIOUS_CONTEXT;
        case ToolingIncrementalSnapshotStatus::rejected_mismatched_previous:
            return TOOLING_INCREMENTAL_STATUS_REJECTED_MISMATCHED_PREVIOUS;
        case ToolingIncrementalSnapshotStatus::rejected_stale_previous:
            return TOOLING_INCREMENTAL_STATUS_REJECTED_STALE_PREVIOUS;
        case ToolingIncrementalSnapshotStatus::rejected_malformed_previous:
            return TOOLING_INCREMENTAL_STATUS_REJECTED_MALFORMED_PREVIOUS;
    }
    return TOOLING_INCREMENTAL_STATUS_REJECTED_MALFORMED_PREVIOUS;
}

ToolingIncrementalSnapshotResult tooling_incremental_snapshot_result(
    const ToolingIncrementalSnapshotInput& input, const IdeSnapshot& snapshot)
{
    ToolingIncrementalSnapshotResult result;
    result.input = input;
    tooling_fill_current_incremental_counts(result, snapshot);
    if (!input.has_previous_snapshot) {
        result.status = ToolingIncrementalSnapshotStatus::clean_build;
        result.fallback_reason = std::string(TOOLING_INCREMENTAL_REASON_NO_PREVIOUS);
        return result;
    }
    if (!input.previous_document_matches) {
        result.status = ToolingIncrementalSnapshotStatus::rejected_mismatched_previous;
        result.fallback_reason = std::string(TOOLING_INCREMENTAL_REASON_MISMATCHED_PREVIOUS);
        return result;
    }
    if (!input.previous_version_is_older) {
        result.status = ToolingIncrementalSnapshotStatus::rejected_stale_previous;
        result.fallback_reason = std::string(TOOLING_INCREMENTAL_REASON_STALE_PREVIOUS);
        return result;
    }
    if (tooling_incremental_input_shape_is_malformed(input)) {
        result.status = ToolingIncrementalSnapshotStatus::rejected_malformed_previous;
        result.fallback_reason = std::string(TOOLING_INCREMENTAL_REASON_MALFORMED_PREVIOUS);
        return result;
    }
    result.status = ToolingIncrementalSnapshotStatus::previous_context;
    result.used_previous_context = true;
    return result;
}

ToolingSession::ToolingSession() : ToolingSession(ToolingProjectConfig{})
{
}

ToolingSession::ToolingSession(ToolingProjectConfig config) : config_(std::move(config))
{
    if (this->config_.package_identity.empty()) {
        this->config_.package_identity = std::string(TOOLING_DEFAULT_PACKAGE_IDENTITY);
    }
    this->refresh_workspace_model();
}

const ToolingProjectConfig& ToolingSession::project_config() const noexcept
{
    return this->config_;
}

const project::WorkspaceModel& ToolingSession::workspace_model() const noexcept
{
    return this->workspace_model_;
}

bool ToolingSession::is_open(const ToolingDocumentId& id) const
{
    return this->find_slot(id) != this->documents_.end();
}

bool ToolingSession::is_generation_current(const ToolingDocumentId& id, const ToolingDocumentVersion version) const
{
    const auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return false;
    }
    return found->second.state.version == version;
}

std::optional<ToolingDocumentState> ToolingSession::document_state(const ToolingDocumentId& id) const
{
    const auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return std::nullopt;
    }
    return found->second.state;
}

base::Result<ToolingDocumentVersion> ToolingSession::open_document(
    ToolingDocumentId id, std::string text, const std::optional<base::i64> client_version)
{
    ToolingDocumentId normalized = this->normalize_document_id(id);
    const std::string key = tooling_document_store_key(normalized);
    if (this->documents_.contains(key)) {
        return base::Result<ToolingDocumentVersion>::fail(
            {base::ErrorCode::invalid_source, "tooling document is already open"});
    }
    const ToolingDocumentVersion version = this->next_version(client_version);
    DocumentSlot slot;
    slot.state.id = std::move(normalized);
    slot.state.version = version;
    slot.state.text = std::move(text);
    slot.state.open = true;
    slot.cached_version = {};
    this->documents_.emplace(key, std::move(slot));
    this->refresh_workspace_model();
    return base::Result<ToolingDocumentVersion>::ok(version);
}

base::Result<ToolingDocumentVersion> ToolingSession::change_document(
    const ToolingDocumentId& id, std::string text, const std::optional<base::i64> client_version)
{
    auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return base::Result<ToolingDocumentVersion>::fail(
            {base::ErrorCode::invalid_source, "tooling document is not open"});
    }
    if (client_version.has_value() && found->second.state.version.client_version.has_value()
        && *client_version <= *found->second.state.version.client_version) {
        return base::Result<ToolingDocumentVersion>::fail(
            {base::ErrorCode::invalid_source, "stale tooling document version"});
    }
    std::optional<IdeEditImpact> pending_edit_impact;
    if (found->second.cached_snapshot != nullptr && found->second.cached_version == found->second.state.version) {
        pending_edit_impact = tooling_full_document_edit_impact(*found->second.cached_snapshot);
        found->second.previous_cached_snapshot = found->second.cached_snapshot;
        found->second.previous_cached_version = found->second.cached_version;
    } else {
        found->second.previous_cached_snapshot.reset();
        found->second.previous_cached_version = {};
    }
    const ToolingDocumentVersion version = this->next_version(client_version);
    found->second.pending_workspace_facts = this->workspace_index_.facts_for_document(found->second.state.id);
    this->workspace_index_.remove_document(found->second.state.id);
    found->second.state.text = std::move(text);
    found->second.state.version = version;
    found->second.cached_snapshot.reset();
    found->second.cached_version = {};
    found->second.pending_edit_impact = std::move(pending_edit_impact);
    this->refresh_workspace_model();
    return base::Result<ToolingDocumentVersion>::ok(version);
}

base::Result<ToolingDocumentVersion> ToolingSession::change_document_range(
    const ToolingDocumentId& id, ToolingDocumentTextEdit edit, const std::optional<base::i64> client_version)
{
    auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return base::Result<ToolingDocumentVersion>::fail(
            {base::ErrorCode::invalid_source, "tooling document is not open"});
    }

    std::optional<IdeEditImpact> pending_edit_impact;
    if (found->second.cached_snapshot != nullptr && found->second.cached_version == found->second.state.version
        && tooling_text_edit_range_is_valid(found->second.state.text, edit)) {
        pending_edit_impact = edit_impact_for_range(*found->second.cached_snapshot, edit.begin, edit.removed_length);
    }
    base::Result<std::string> changed_text = tooling_apply_text_edit(found->second.state.text, edit);
    if (!changed_text) {
        return base::Result<ToolingDocumentVersion>::fail(changed_text.error());
    }
    base::Result<ToolingDocumentVersion> changed =
        this->change_document(id, std::move(changed_text.value()), client_version);
    if (!changed) {
        return changed;
    }
    auto changed_slot = this->find_slot(id);
    if (changed_slot != this->documents_.end() && pending_edit_impact.has_value()) {
        changed_slot->second.pending_edit_impact = *pending_edit_impact;
    }
    return changed;
}

base::Result<ToolingDocumentChangeResult> ToolingSession::change_document_with_reuse_plan(const ToolingDocumentId& id,
    std::string text, const base::usize edit_begin, const base::usize removed_length,
    const std::optional<base::i64> client_version)
{
    auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return base::Result<ToolingDocumentChangeResult>::fail(
            {base::ErrorCode::invalid_source, "tooling document is not open"});
    }

    base::Result<ToolingSnapshotHandle> before = this->snapshot(id);
    if (!before) {
        return base::Result<ToolingDocumentChangeResult>::fail(before.error());
    }
    const IdeSnapshot& before_snapshot = *before.value().snapshot;
    const IdeEditImpact impact = edit_impact_for_range(before_snapshot, edit_begin, removed_length);
    const std::string_view before_text = before_snapshot.sources.text(before_snapshot.source_id);
    ToolingDocumentTextEdit applied_edit =
        tooling_text_edit_from_replacement(before_text, text, edit_begin, removed_length);

    base::Result<ToolingDocumentVersion> changed = this->change_document(id, std::move(text), client_version);
    if (!changed) {
        return base::Result<ToolingDocumentChangeResult>::fail(changed.error());
    }
    auto changed_slot = this->find_slot(id);
    if (changed_slot != this->documents_.end()) {
        changed_slot->second.pending_edit_impact = impact;
    }
    base::Result<ToolingSnapshotHandle> after = this->snapshot(id);
    if (!after) {
        return base::Result<ToolingDocumentChangeResult>::fail(after.error());
    }
    ToolingReusePlan reuse_plan = after.value().incremental.reuse_plan == nullptr
        ? tooling_plan_reuse(before_snapshot, *after.value().snapshot, impact)
        : *after.value().incremental.reuse_plan;
    return base::Result<ToolingDocumentChangeResult>::ok(ToolingDocumentChangeResult{
        changed.value(),
        std::move(applied_edit),
        impact,
        std::move(reuse_plan),
        after.value().incremental,
    });
}

base::Result<ToolingDocumentChangeResult> ToolingSession::change_document_range_with_reuse_plan(
    const ToolingDocumentId& id, ToolingDocumentTextEdit edit, const std::optional<base::i64> client_version)
{
    auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return base::Result<ToolingDocumentChangeResult>::fail(
            {base::ErrorCode::invalid_source, "tooling document is not open"});
    }

    base::Result<ToolingSnapshotHandle> before = this->snapshot(id);
    if (!before) {
        return base::Result<ToolingDocumentChangeResult>::fail(before.error());
    }
    const IdeSnapshot& before_snapshot = *before.value().snapshot;
    const std::string_view before_text = before_snapshot.sources.text(before_snapshot.source_id);
    base::Result<std::string> changed_text = tooling_apply_text_edit(before_text, edit);
    if (!changed_text) {
        return base::Result<ToolingDocumentChangeResult>::fail(changed_text.error());
    }
    const IdeEditImpact impact = edit_impact_for_range(before_snapshot, edit.begin, edit.removed_length);

    base::Result<ToolingDocumentVersion> changed =
        this->change_document(id, std::move(changed_text.value()), client_version);
    if (!changed) {
        return base::Result<ToolingDocumentChangeResult>::fail(changed.error());
    }
    auto changed_slot = this->find_slot(id);
    if (changed_slot != this->documents_.end()) {
        changed_slot->second.pending_edit_impact = impact;
    }
    base::Result<ToolingSnapshotHandle> after = this->snapshot(id);
    if (!after) {
        return base::Result<ToolingDocumentChangeResult>::fail(after.error());
    }
    ToolingReusePlan reuse_plan = after.value().incremental.reuse_plan == nullptr
        ? tooling_plan_reuse(before_snapshot, *after.value().snapshot, impact)
        : *after.value().incremental.reuse_plan;
    return base::Result<ToolingDocumentChangeResult>::ok(ToolingDocumentChangeResult{
        changed.value(),
        std::move(edit),
        impact,
        std::move(reuse_plan),
        after.value().incremental,
    });
}

base::Result<void> ToolingSession::close_document(const ToolingDocumentId& id)
{
    auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return tooling_missing_document_error();
    }
    this->workspace_index_.remove_document(found->second.state.id);
    this->documents_.erase(found);
    this->refresh_workspace_model();
    return base::Result<void>::ok();
}

base::Result<ToolingSnapshotHandle> ToolingSession::snapshot(const ToolingDocumentId& id)
{
    auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return base::Result<ToolingSnapshotHandle>::fail(
            {base::ErrorCode::invalid_source, "tooling document is not open"});
    }
    DocumentSlot& slot = found->second;
    if (slot.cached_snapshot != nullptr && slot.cached_version == slot.state.version) {
        ToolingIncrementalSnapshotResult cached_result = slot.last_snapshot_result;
        cached_result.status = ToolingIncrementalSnapshotStatus::cached_snapshot;
        cached_result.from_cache = true;
        cached_result.used_previous_context = false;
        cached_result.fallback_reason.clear();
        ToolingSnapshotHandle handle{slot.state.id, slot.state.version, slot.cached_snapshot, std::move(cached_result)};
        if (slot.pending_workspace_facts.empty()) {
            handle.incremental.workspace_update = this->workspace_index_.update_snapshot(handle);
        } else {
            handle.incremental.workspace_update =
                this->workspace_index_.update_snapshot(handle, slot.pending_workspace_facts);
            slot.pending_workspace_facts.clear();
        }
        slot.last_snapshot_result.workspace_update = handle.incremental.workspace_update;
        return base::Result<ToolingSnapshotHandle>::ok(std::move(handle));
    }
    const ToolingIncrementalSnapshotInput incremental_input = this->incremental_input_for_slot(slot);
    IdeSnapshotRequest request;
    request.path = slot.state.id.path;
    request.text = slot.state.text;
    request.package_identity = std::string(tooling_package_or_default(slot.state.id.package_identity));
    request.virtual_buffer_identity =
        slot.state.id.virtual_buffer_identity.empty() ? slot.state.id.uri : slot.state.id.virtual_buffer_identity;
    request.source_role = slot.state.id.source_role;
    auto cached = std::make_shared<IdeSnapshot>();
    IdeIncrementalSnapshotInput ide_incremental;
    if (slot.previous_cached_snapshot != nullptr && tooling_incremental_input_can_use_previous(incremental_input)) {
        ide_incremental.previous_query = &slot.previous_cached_snapshot->query;
    }
    build_ide_snapshot_into(*cached, request, ide_incremental);
    slot.last_snapshot_result = tooling_incremental_snapshot_result(incremental_input, *cached);
    if (slot.last_snapshot_result.status == ToolingIncrementalSnapshotStatus::previous_context
        && slot.previous_cached_snapshot != nullptr) {
        const IdeEditImpact impact = slot.pending_edit_impact.has_value()
            ? *slot.pending_edit_impact
            : tooling_full_document_edit_impact(*slot.previous_cached_snapshot);
        std::shared_ptr<const ToolingReusePlan> reuse_plan =
            std::make_shared<ToolingReusePlan>(tooling_plan_reuse(*slot.previous_cached_snapshot, *cached, impact));
        tooling_attach_reuse_execution(slot.last_snapshot_result, reuse_plan);
        tooling_attach_syntax_reuse_execution(slot.last_snapshot_result, *slot.previous_cached_snapshot, *cached);
    }
    slot.pending_edit_impact.reset();
    slot.cached_snapshot = std::move(cached);
    slot.cached_version = slot.state.version;
    ToolingSnapshotHandle handle{slot.state.id, slot.state.version, slot.cached_snapshot, slot.last_snapshot_result};
    if (slot.pending_workspace_facts.empty()) {
        handle.incremental.workspace_update = this->workspace_index_.update_snapshot(handle);
    } else {
        handle.incremental.workspace_update =
            this->workspace_index_.update_snapshot(handle, slot.pending_workspace_facts);
        slot.pending_workspace_facts.clear();
    }
    slot.last_snapshot_result.workspace_update = handle.incremental.workspace_update;
    return base::Result<ToolingSnapshotHandle>::ok(std::move(handle));
}

const ToolingWorkspaceSemanticIndex& ToolingSession::workspace_index() const noexcept
{
    return this->workspace_index_;
}

base::Result<std::vector<ToolingDiagnostic>> ToolingSession::diagnostics(const ToolingDocumentId& id)
{
    base::Result<ToolingSnapshotHandle> handle = this->snapshot(id);
    if (!handle) {
        return base::Result<std::vector<ToolingDiagnostic>>::fail(handle.error());
    }
    const IdeSnapshot& snapshot = *handle.value().snapshot;
    std::vector<ToolingDiagnostic> result;
    result.reserve(snapshot.diagnostics.size());
    for (const IdeDiagnostic& diagnostic : snapshot.diagnostics) {
        std::vector<ToolingDiagnosticChild> children;
        children.reserve(diagnostic.children.size());
        for (const base::DiagnosticChild& child : diagnostic.children) {
            children.push_back(tooling_project_diagnostic_child(snapshot, child));
        }
        result.push_back(ToolingDiagnostic{
            diagnostic.severity,
            diagnostic.category,
            diagnostic.code,
            tooling_text_range_for_snapshot(snapshot, diagnostic.range),
            std::string(base::severity_name(diagnostic.severity)),
            std::string(base::diagnostic_category_name(diagnostic.category)),
            std::string(base::diagnostic_code_name(diagnostic.code)),
            diagnostic.message,
            std::move(children),
            diagnostic.owner_stages,
            diagnostic.source_part,
        });
    }
    return base::Result<std::vector<ToolingDiagnostic>>::ok(std::move(result));
}

base::Result<std::optional<ToolingHover>> ToolingSession::hover_at_offset(
    const ToolingDocumentId& id, const base::usize offset)
{
    base::Result<ToolingSnapshotHandle> handle = this->snapshot(id);
    if (!handle) {
        return base::Result<std::optional<ToolingHover>>::fail(handle.error());
    }
    const IdeSnapshot& snapshot = *handle.value().snapshot;
    const std::optional<IdeHoverInfo> hover = tooling::hover_at_offset(snapshot, offset);
    if (!hover.has_value()) {
        return base::Result<std::optional<ToolingHover>>::ok(std::nullopt);
    }

    ToolingHover projected;
    projected.range = tooling_text_range_for_snapshot(snapshot, hover->range);
    projected.label = hover->label;
    projected.valid = hover->valid;
    if (hover->definition.has_value()) {
        projected.definition = tooling_project_definition(snapshot, *hover->definition);
        const IdeSemanticFact* const fact = tooling_semantic_fact_for_definition(snapshot, *hover->definition);
        if (fact != nullptr) {
            projected.detail = fact->detail;
            projected.semantic_fact_detail = fact->detail;
            projected.semantic_fact_key = tooling_stable_key_or_empty(fact->query);
        }
    }
    return base::Result<std::optional<ToolingHover>>::ok(std::move(projected));
}

base::Result<std::optional<ToolingHover>> ToolingSession::hover_at_position(
    const ToolingDocumentId& id, const ToolingSourcePosition position)
{
    const auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return base::Result<std::optional<ToolingHover>>::fail(
            {base::ErrorCode::invalid_source, "tooling document is not open"});
    }
    return this->hover_at_offset(id, tooling_offset_for_position(found->second.state.text, position));
}

base::Result<std::optional<ToolingDefinition>> ToolingSession::definition_at_offset(
    const ToolingDocumentId& id, const base::usize offset)
{
    base::Result<ToolingSnapshotHandle> handle = this->snapshot(id);
    if (!handle) {
        return base::Result<std::optional<ToolingDefinition>>::fail(handle.error());
    }
    const IdeSnapshot& snapshot = *handle.value().snapshot;
    const std::optional<IdeDefinition> definition = tooling::definition_at_offset(snapshot, offset);
    if (!definition.has_value()) {
        return base::Result<std::optional<ToolingDefinition>>::ok(std::nullopt);
    }
    ToolingDefinition projected = tooling_project_definition(snapshot, *definition);
    std::vector<ToolingIndexedSemanticFact> indexed;
    if (query::is_valid(projected.member)) {
        indexed = this->workspace_index_.members(projected.member);
    }
    if (indexed.empty()) {
        indexed = this->workspace_index_.definitions(projected.key);
    }
    if (const ToolingIndexedSemanticFact* const fact = tooling_best_definition_fact(indexed); fact != nullptr) {
        projected = tooling_project_definition_from_indexed_fact(*fact);
    }
    return base::Result<std::optional<ToolingDefinition>>::ok(std::move(projected));
}

base::Result<std::optional<ToolingDefinition>> ToolingSession::definition_at_position(
    const ToolingDocumentId& id, const ToolingSourcePosition position)
{
    const auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return base::Result<std::optional<ToolingDefinition>>::fail(
            {base::ErrorCode::invalid_source, "tooling document is not open"});
    }
    return this->definition_at_offset(id, tooling_offset_for_position(found->second.state.text, position));
}

base::Result<std::optional<ToolingAstNode>> ToolingSession::ast_node_at_offset(
    const ToolingDocumentId& id, const base::usize offset)
{
    base::Result<ToolingSnapshotHandle> handle = this->snapshot(id);
    if (!handle) {
        return base::Result<std::optional<ToolingAstNode>>::fail(handle.error());
    }
    const IdeSnapshot& snapshot = *handle.value().snapshot;
    const std::optional<IdeAstNodeInfo> ast_node = tooling::ast_node_at_offset(snapshot, offset);
    if (!ast_node.has_value()) {
        return base::Result<std::optional<ToolingAstNode>>::ok(std::nullopt);
    }
    return base::Result<std::optional<ToolingAstNode>>::ok(tooling_project_ast_node(snapshot, *ast_node));
}

base::Result<std::optional<ToolingAstNode>> ToolingSession::ast_node_at_position(
    const ToolingDocumentId& id, const ToolingSourcePosition position)
{
    const auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return base::Result<std::optional<ToolingAstNode>>::fail(
            {base::ErrorCode::invalid_source, "tooling document is not open"});
    }
    return this->ast_node_at_offset(id, tooling_offset_for_position(found->second.state.text, position));
}

base::Result<std::vector<ToolingReference>> ToolingSession::references_at_offset(
    const ToolingDocumentId& id, const base::usize offset)
{
    base::Result<ToolingSnapshotHandle> handle = this->snapshot(id);
    if (!handle) {
        return base::Result<std::vector<ToolingReference>>::fail(handle.error());
    }
    const IdeSnapshot& snapshot = *handle.value().snapshot;
    const std::vector<IdeReference> references = tooling::references_at_offset(snapshot, offset);
    std::vector<ToolingReference> projected;
    projected.reserve(references.size());
    for (const IdeReference& reference : references) {
        projected.push_back(ToolingReference{
            tooling_text_range_for_snapshot(snapshot, reference.range),
            reference.name,
            reference.is_definition,
        });
    }
    const std::optional<IdeDefinition> definition = tooling::definition_at_offset(snapshot, offset);
    if (definition.has_value()) {
        std::vector<ToolingIndexedSemanticFact> indexed;
        if (query::is_valid(definition->member)) {
            indexed = this->workspace_index_.members(definition->member);
        }
        if (indexed.empty() && query::is_valid(definition->key)) {
            indexed = this->workspace_index_.definitions(definition->key);
        }
        tooling_push_workspace_definition_references(projected, indexed);
    }
    return base::Result<std::vector<ToolingReference>>::ok(std::move(projected));
}

base::Result<std::vector<ToolingReference>> ToolingSession::references_at_position(
    const ToolingDocumentId& id, const ToolingSourcePosition position)
{
    const auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return base::Result<std::vector<ToolingReference>>::fail(
            {base::ErrorCode::invalid_source, "tooling document is not open"});
    }
    return this->references_at_offset(id, tooling_offset_for_position(found->second.state.text, position));
}

base::Result<std::vector<ToolingDocumentSymbol>> ToolingSession::document_symbols(const ToolingDocumentId& id)
{
    base::Result<ToolingSnapshotHandle> handle = this->snapshot(id);
    if (!handle) {
        return base::Result<std::vector<ToolingDocumentSymbol>>::fail(handle.error());
    }
    const IdeSnapshot& snapshot = *handle.value().snapshot;
    std::vector<ToolingDocumentSymbol> symbols;
    symbols.reserve(snapshot.query.semantic_facts.size());
    for (const IdeSemanticFact& fact : snapshot.query.semantic_facts) {
        if (tooling_fact_is_document_symbol(fact)) {
            symbols.push_back(tooling_symbol_from_fact(snapshot, fact));
        }
    }
    if (symbols.empty() && snapshot.parsed) {
        symbols.reserve(snapshot.ast.items.size());
        for (base::usize index = 0; index < snapshot.ast.items.size(); ++index) {
            const syntax::ItemNode* const item = snapshot.ast.items.ptr(index);
            if (item == nullptr || item->name.empty()) {
                continue;
            }
            symbols.push_back(tooling_symbol_from_item(snapshot, *item, tooling_item_part_index(snapshot, index)));
        }
    }
    std::ranges::sort(symbols, [](const ToolingDocumentSymbol& lhs, const ToolingDocumentSymbol& rhs) {
        if (lhs.selection_range.range.begin == rhs.selection_range.range.begin) {
            return lhs.name < rhs.name;
        }
        return lhs.selection_range.range.begin < rhs.selection_range.range.begin;
    });
    return base::Result<std::vector<ToolingDocumentSymbol>>::ok(std::move(symbols));
}

base::Result<std::vector<ToolingCompletionItem>> ToolingSession::completion_at_offset(
    const ToolingDocumentId& id, const base::usize offset)
{
    base::Result<ToolingSnapshotHandle> handle = this->snapshot(id);
    if (!handle) {
        return base::Result<std::vector<ToolingCompletionItem>>::fail(handle.error());
    }
    const IdeSnapshot& snapshot = *handle.value().snapshot;
    const std::vector<IdeCompletionItem> ide_items = tooling::completion_items_at_offset(snapshot, offset);
    std::vector<ToolingCompletionItem> result;
    result.reserve(ide_items.size());
    std::vector<std::string> seen;
    seen.reserve(ide_items.size());
    for (const IdeCompletionItem& item : ide_items) {
        tooling_push_completion_if_new(result, seen, tooling_project_completion(snapshot, item));
    }
    if (!ide_items.empty()) {
        const IdeCompletionItem& first = ide_items.front();
        const std::string_view source = snapshot.sources.text(snapshot.source_id);
        const base::usize prefix_begin = std::min(first.replacement_range.begin, source.size());
        const base::usize prefix_end = std::min(offset, source.size());
        const std::string_view prefix =
            prefix_end >= prefix_begin ? source.substr(prefix_begin, prefix_end - prefix_begin) : std::string_view{};
        tooling_append_workspace_completions(result, seen, this->workspace_index_.all_facts(), first.context,
            tooling_text_range_for_snapshot(snapshot, first.replacement_range), prefix);
    }
    std::ranges::sort(result, [](const ToolingCompletionItem& lhs, const ToolingCompletionItem& rhs) {
        if (lhs.local != rhs.local) {
            return lhs.local && !rhs.local;
        }
        if (lhs.from_workspace != rhs.from_workspace) {
            return !lhs.from_workspace && rhs.from_workspace;
        }
        if (lhs.checked != rhs.checked) {
            return lhs.checked && !rhs.checked;
        }
        if (lhs.label == rhs.label) {
            return lhs.kind < rhs.kind;
        }
        return lhs.label < rhs.label;
    });
    return base::Result<std::vector<ToolingCompletionItem>>::ok(std::move(result));
}

base::Result<std::vector<ToolingCompletionItem>> ToolingSession::completion_at_position(
    const ToolingDocumentId& id, const ToolingSourcePosition position)
{
    const auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return base::Result<std::vector<ToolingCompletionItem>>::fail(
            {base::ErrorCode::invalid_source, "tooling document is not open"});
    }
    return this->completion_at_offset(id, tooling_offset_for_position(found->second.state.text, position));
}

base::Result<ToolingRenamePlan> ToolingSession::rename_at_offset(
    const ToolingDocumentId& id, const base::usize offset, std::string new_name)
{
    base::Result<ToolingSnapshotHandle> handle = this->snapshot(id);
    if (!handle) {
        return base::Result<ToolingRenamePlan>::fail(handle.error());
    }
    const IdeSnapshot& snapshot = *handle.value().snapshot;
    ToolingRenamePlan plan;
    plan.version = handle.value().version;
    plan.new_name = std::move(new_name);

    const std::optional<IdeDefinition> ide_definition = tooling::definition_at_offset(snapshot, offset);
    if (!ide_definition.has_value()) {
        return base::Result<ToolingRenamePlan>::ok(std::move(plan));
    }
    plan.target = tooling_project_definition(snapshot, *ide_definition);
    plan.old_name = plan.target.name;

    if (tooling_is_reserved_word(plan.new_name)) {
        tooling_append_rename_conflict(plan.conflicts, plan.target.range, plan.new_name, plan.target.kind,
            TOOLING_RENAME_CONFLICT_KEYWORD, plan.target.stable_definition_key, plan.target.stable_member_key);
    } else if (!tooling_is_valid_identifier(plan.new_name)) {
        tooling_append_rename_conflict(plan.conflicts, plan.target.range, plan.new_name, plan.target.kind,
            TOOLING_RENAME_CONFLICT_INVALID_IDENTIFIER, plan.target.stable_definition_key,
            plan.target.stable_member_key);
    }

    base::Result<std::vector<ToolingCompletionItem>> completions =
        this->completion_at_offset(id, ide_definition->range.begin);
    if (completions) {
        tooling_append_rename_conflicts_from_completion(
            plan.conflicts, completions.value(), plan.target, plan.new_name);
    }

    base::Result<std::vector<ToolingReference>> references = this->references_at_offset(id, offset);
    if (!references) {
        return base::Result<ToolingRenamePlan>::fail(references.error());
    }
    for (const ToolingReference& reference : references.value()) {
        const ToolingDocumentId edit_document =
            tooling_document_for_edit_range(handle.value().document, reference.range, this->config_);
        tooling_push_rename_edit_if_new(plan.edits, edit_document, reference.range, plan.new_name);
    }
    if (plan.edits.empty()) {
        tooling_push_rename_edit_if_new(plan.edits, handle.value().document, plan.target.range, plan.new_name);
    }
    std::ranges::sort(plan.edits, [](const ToolingRenameEdit& lhs, const ToolingRenameEdit& rhs) {
        const std::string lhs_key = tooling_document_store_key(lhs.document);
        const std::string rhs_key = tooling_document_store_key(rhs.document);
        if (lhs_key == rhs_key) {
            return lhs.range.range.begin < rhs.range.range.begin;
        }
        return lhs_key < rhs_key;
    });
    plan.valid = plan.conflicts.empty() && !plan.edits.empty();
    return base::Result<ToolingRenamePlan>::ok(std::move(plan));
}

base::Result<ToolingRenamePlan> ToolingSession::rename_at_position(
    const ToolingDocumentId& id, const ToolingSourcePosition position, std::string new_name)
{
    const auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return base::Result<ToolingRenamePlan>::fail({base::ErrorCode::invalid_source, "tooling document is not open"});
    }
    return this->rename_at_offset(
        id, tooling_offset_for_position(found->second.state.text, position), std::move(new_name));
}

base::Result<std::vector<ToolingSemanticToken>> ToolingSession::semantic_tokens(const ToolingDocumentId& id)
{
    base::Result<ToolingSnapshotHandle> handle = this->snapshot(id);
    if (!handle) {
        return base::Result<std::vector<ToolingSemanticToken>>::fail(handle.error());
    }
    const IdeSnapshot& snapshot = *handle.value().snapshot;
    const std::vector<IdeSemanticToken> ide_tokens = tooling::semantic_tokens(snapshot);
    std::vector<ToolingSemanticToken> result;
    result.reserve(ide_tokens.size());
    for (const IdeSemanticToken& token : ide_tokens) {
        result.push_back(tooling_project_semantic_token(snapshot, token));
    }
    return base::Result<std::vector<ToolingSemanticToken>>::ok(std::move(result));
}

base::Result<std::vector<ToolingInlayHint>> ToolingSession::inlay_hints(const ToolingDocumentId& id)
{
    base::Result<ToolingSnapshotHandle> handle = this->snapshot(id);
    if (!handle) {
        return base::Result<std::vector<ToolingInlayHint>>::fail(handle.error());
    }
    const IdeSnapshot& snapshot = *handle.value().snapshot;
    const std::vector<IdeInlayHint> ide_hints = tooling::inlay_hints(snapshot);
    std::vector<ToolingInlayHint> result;
    result.reserve(ide_hints.size());
    for (const IdeInlayHint& hint : ide_hints) {
        result.push_back(tooling_project_inlay_hint(snapshot, hint));
    }
    return base::Result<std::vector<ToolingInlayHint>>::ok(std::move(result));
}

base::Result<std::vector<ToolingCodeAction>> ToolingSession::code_actions(const ToolingDocumentId& id)
{
    base::Result<std::vector<ToolingDiagnostic>> diagnostics = this->diagnostics(id);
    if (!diagnostics) {
        return base::Result<std::vector<ToolingCodeAction>>::fail(diagnostics.error());
    }
    const auto found = this->find_slot(id);
    if (found == this->documents_.end()) {
        return base::Result<std::vector<ToolingCodeAction>>::fail(
            {base::ErrorCode::invalid_source, "tooling document is not open"});
    }
    std::vector<ToolingCodeAction> actions;
    std::vector<std::string> seen;
    for (const ToolingDiagnostic& diagnostic : diagnostics.value()) {
        tooling_append_lookup_suggestion_action(actions, seen, found->second.state.id, diagnostic.range,
            diagnostic.severity, diagnostic.message, diagnostic.code_name);
        for (const ToolingDiagnosticChild& child : diagnostic.children) {
            tooling_append_lookup_suggestion_action(
                actions, seen, found->second.state.id, child.range, child.severity, child.message, child.code_name);
        }
    }
    std::ranges::sort(actions, [](const ToolingCodeAction& lhs, const ToolingCodeAction& rhs) {
        if (lhs.preferred != rhs.preferred) {
            return lhs.preferred && !rhs.preferred;
        }
        return lhs.title < rhs.title;
    });
    return base::Result<std::vector<ToolingCodeAction>>::ok(std::move(actions));
}

base::Result<std::vector<ToolingWorkspaceSymbol>> ToolingSession::workspace_symbols(const std::string_view query)
{
    std::vector<ToolingDocumentId> documents;
    documents.reserve(this->documents_.size());
    for (const auto& entry : this->documents_) {
        documents.push_back(entry.second.state.id);
    }
    for (const ToolingDocumentId& document : documents) {
        base::Result<ToolingSnapshotHandle> handle = this->snapshot(document);
        if (!handle) {
            return base::Result<std::vector<ToolingWorkspaceSymbol>>::fail(handle.error());
        }
    }

    std::vector<ToolingWorkspaceSymbol> result;
    const std::vector<ToolingIndexedSemanticFact> facts = this->workspace_index_.all_facts();
    result.reserve(facts.size());
    for (const ToolingIndexedSemanticFact& fact : facts) {
        if (!tooling_indexed_fact_is_definition_entry(fact) || !tooling_indexed_fact_matches_query(fact, query)) {
            continue;
        }
        result.push_back(tooling_project_workspace_symbol(fact));
    }
    std::ranges::sort(result, [](const ToolingWorkspaceSymbol& lhs, const ToolingWorkspaceSymbol& rhs) {
        if (lhs.name == rhs.name) {
            if (lhs.document.uri == rhs.document.uri) {
                return lhs.range.range.begin < rhs.range.range.begin;
            }
            return lhs.document.uri < rhs.document.uri;
        }
        return lhs.name < rhs.name;
    });
    return base::Result<std::vector<ToolingWorkspaceSymbol>>::ok(std::move(result));
}

ToolingDocumentId ToolingSession::normalize_document_id(const ToolingDocumentId& id) const
{
    ToolingDocumentId normalized = id;
    if (normalized.path.empty()) {
        const std::optional<std::string> path = tooling_path_from_file_uri(normalized.uri);
        if (path.has_value()) {
            normalized.path = *path;
        }
    }
    normalized.path = tooling_normalize_path(normalized.path, this->config_);
    if (normalized.uri.empty()) {
        normalized.uri = tooling_file_uri_from_path(normalized.path);
    }
    if (normalized.package_identity.empty()) {
        normalized.package_identity = std::string(tooling_package_or_default(this->config_.package_identity));
    }
    if (normalized.virtual_buffer_identity.empty()) {
        normalized.virtual_buffer_identity = normalized.uri;
    }
    return normalized;
}

ToolingDocumentVersion ToolingSession::next_version(const std::optional<base::i64> client_version)
{
    ++this->next_generation_;
    return ToolingDocumentVersion{this->next_generation_, client_version};
}

ToolingIncrementalSnapshotInput ToolingSession::incremental_input_for_slot(const DocumentSlot& slot) const
{
    ToolingIncrementalSnapshotInput input;
    input.document = slot.state.id;
    input.current_version = slot.state.version;
    if (slot.previous_cached_snapshot == nullptr) {
        return input;
    }
    input.previous_version = slot.previous_cached_version;
    input.has_previous_snapshot = true;
    input.previous_document_matches = true;
    input.previous_version_is_older = tooling_version_is_older(input.previous_version, input.current_version);
    input.previous_query_records = slot.previous_cached_snapshot->query.records.size();
    input.previous_dependency_edges = slot.previous_cached_snapshot->query.dependencies.size();
    input.previous_semantic_facts = slot.previous_cached_snapshot->query.semantic_facts.size();
    input.previous_context_malformed = tooling_incremental_input_shape_is_malformed(input);
    return input;
}

void ToolingSession::refresh_workspace_model()
{
    std::vector<project::ProjectOpenBuffer> buffers;
    buffers.reserve(this->documents_.size());
    for (const auto& entry : this->documents_) {
        const DocumentSlot& slot = entry.second;
        buffers.push_back(project::ProjectOpenBuffer{
            std::filesystem::path(slot.state.id.path),
            slot.state.id.uri,
            std::string(tooling_package_or_default(slot.state.id.package_identity)),
            slot.state.id.virtual_buffer_identity.empty() ? slot.state.id.uri : slot.state.id.virtual_buffer_identity,
            slot.state.id.source_role,
            project::project_open_buffer_fingerprint(slot.state.text),
            static_cast<base::u64>(slot.state.text.size()),
            slot.state.version.generation,
        });
    }

    project::ProjectModel model = project::build_project_model(project::ProjectModelInput{
        project::ProjectSessionKind::tooling,
        tooling_project_root_path(this->config_),
        tooling_project_source_root(this->config_),
        std::string(tooling_package_or_default(this->config_.package_identity)),
        tooling_package_key(this->config_.package_identity),
        tooling_project_import_roots(this->config_),
        tooling_project_target_config(),
        tooling_project_command_options(),
        std::move(buffers),
    });
    const std::array<project::ProjectModel, 1> projects{std::move(model)};
    this->workspace_model_ =
        project::build_workspace_model(std::span<const project::ProjectModel>(projects.data(), projects.size()));
}

std::unordered_map<std::string, ToolingSession::DocumentSlot>::iterator ToolingSession::find_slot(
    const ToolingDocumentId& id)
{
    const ToolingDocumentId normalized = this->normalize_document_id(id);
    return this->documents_.find(tooling_document_store_key(normalized));
}

std::unordered_map<std::string, ToolingSession::DocumentSlot>::const_iterator ToolingSession::find_slot(
    const ToolingDocumentId& id) const
{
    const ToolingDocumentId normalized = this->normalize_document_id(id);
    return this->documents_.find(tooling_document_store_key(normalized));
}

} // namespace aurex::tooling
