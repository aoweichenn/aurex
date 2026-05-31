#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/driver/pipeline_stage.hpp>
#include <aurex/query/generic_instance_key.hpp>
#include <aurex/query/query_context.hpp>
#include <aurex/sema/checked_module.hpp>
#include <aurex/syntax/ast.hpp>
#include <aurex/syntax/lossless.hpp>
#include <aurex/syntax/token.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::tooling {

struct IdeSnapshotRequest {
    std::string path;
    std::string text;
    std::string package_identity = "ide";
    std::string virtual_buffer_identity;
    query::SourceRole source_role = query::SourceRole::virtual_buffer;
};

using IdePipelineStageOwner = driver::PipelineStageMetadata;

struct IdeModulePartContext {
    query::ModuleKey module_key;
    query::ModulePartKey part_key;
    query::ModulePartKind kind = query::ModulePartKind::primary;
    base::SourceRange module_range{};
    base::SourceRange part_range{};
    std::string module_name;
    std::string part_name;
    base::u32 part_index = 0;
    bool resolved = false;
    bool valid = false;
};

struct IdeDiagnostic {
    base::Severity severity = base::Severity::error;
    base::DiagnosticCategory category = base::DiagnosticCategory::general;
    base::DiagnosticCode code = base::DiagnosticCode::none;
    base::SourceRange range{};
    base::LineColumn start{};
    base::LineColumn end{};
    std::string path;
    std::string message;
    std::vector<base::DiagnosticChild> children;
    std::vector<IdePipelineStageOwner> owner_stages;
    IdeModulePartContext source_part;
};

enum class IdeSemanticFactKind : base::u8 {
    item_signature,
    generic_template_signature,
    function_body_syntax,
    type_check_body,
};

struct IdeSemanticFact {
    IdeSemanticFactKind kind = IdeSemanticFactKind::item_signature;
    query::QueryKey query;
    query::DefKey definition;
    query::MemberKey member;
    query::BodyKey body;
    query::GenericInstanceKey generic_instance;
    base::SourceRange range{};
    std::string name;
    std::string detail;
    base::u32 part_index = 0;
    bool checked = false;
};

struct IdeQuerySnapshot {
    query::QuerySourceStageKeys source_stage;
    std::vector<query::QueryRecord> records;
    std::vector<query::QueryDependencyEdge> dependencies;
    std::vector<IdeSemanticFact> semantic_facts;
};

struct IdeIncrementalSnapshotInput {
    const IdeQuerySnapshot* previous_query = nullptr;
};

struct IdeSnapshot {
    base::SourceManager sources;
    base::SourceId source_id{};
    syntax::LosslessSyntaxTree lossless;
    syntax::AstModule ast;
    sema::CheckedModule checked;
    IdeQuerySnapshot query;
    std::vector<IdeDiagnostic> diagnostics;
    IdeModulePartContext source_part;
    bool lexed = false;
    bool parsed = false;
    bool checked_semantics = false;
    bool has_errors = false;
};

struct IdeTokenInfo {
    syntax::TokenKind kind = syntax::TokenKind::invalid;
    base::SourceRange range{};
    std::string text;
    syntax::LosslessNodeId node = syntax::INVALID_LOSSLESS_NODE_ID;
    syntax::LosslessNodeKind node_kind = syntax::LosslessNodeKind::source_file;
    std::optional<syntax::LosslessNodeKey> node_key;
    bool valid = false;
    bool trivia = false;
};

struct IdeDefinition {
    query::DefKey key;
    query::MemberKey member;
    query::GenericInstanceKey generic_instance;
    base::SourceRange range{};
    std::string name;
    std::string kind;
    base::u32 part_index = 0;
    bool valid = false;
};

struct IdeReference {
    base::SourceRange range{};
    std::string name;
    bool is_definition = false;
};

struct IdeHoverInfo {
    base::SourceRange range{};
    std::string label;
    std::optional<IdeDefinition> definition;
    bool valid = false;
};

enum class IdeCompletionContextKind : base::u8 {
    item,
    expression,
    member,
    module_path,
    trait_bound,
};

struct IdeCompletionItem {
    IdeCompletionContextKind context = IdeCompletionContextKind::expression;
    query::DefKey definition;
    query::MemberKey member;
    query::GenericInstanceKey generic_instance;
    base::SourceRange replacement_range{};
    std::string label;
    std::string kind;
    std::string detail;
    base::u32 part_index = 0;
    bool local = false;
    bool checked = false;
};

struct IdeSemanticToken {
    query::DefKey definition;
    query::MemberKey member;
    base::SourceRange range{};
    std::string text;
    std::string token_type;
    std::vector<std::string> modifiers;
    bool checked = false;
};

struct IdeInlayHint {
    base::SourceRange position{};
    std::string label;
    std::string kind;
    bool checked = false;
};

enum class IdeAstNodeKind : base::u8 {
    item,
    function_body,
};

struct IdeAstNodeInfo {
    IdeAstNodeKind kind = IdeAstNodeKind::item;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    syntax::StmtId body_stmt = syntax::INVALID_STMT_ID;
    query::DefKey definition;
    query::BodyKey body;
    base::SourceRange range{};
    std::string name;
    std::string detail;
    base::u32 part_index = 0;
    bool valid = false;
};

struct IdeEditImpact {
    syntax::LosslessNodeId node = syntax::INVALID_LOSSLESS_NODE_ID;
    base::SourceRange range{};
    std::optional<syntax::LosslessNodeKey> node_key;
    base::usize first_token = 0;
    base::usize token_count = 0;
    bool valid = false;
};

[[nodiscard]] IdeSnapshot build_ide_snapshot(
    const IdeSnapshotRequest& request, IdeIncrementalSnapshotInput incremental = {});
void build_ide_snapshot_into(
    IdeSnapshot& snapshot, const IdeSnapshotRequest& request, IdeIncrementalSnapshotInput incremental = {});
[[nodiscard]] std::optional<IdeTokenInfo> token_info_at_offset(const IdeSnapshot& snapshot, base::usize offset);
[[nodiscard]] std::optional<IdeDefinition> definition_at_offset(const IdeSnapshot& snapshot, base::usize offset);
[[nodiscard]] std::vector<IdeReference> references_at_offset(const IdeSnapshot& snapshot, base::usize offset);
[[nodiscard]] std::optional<IdeHoverInfo> hover_at_offset(const IdeSnapshot& snapshot, base::usize offset);
[[nodiscard]] std::vector<IdeCompletionItem> completion_items_at_offset(
    const IdeSnapshot& snapshot, base::usize offset);
[[nodiscard]] std::vector<IdeSemanticToken> semantic_tokens(const IdeSnapshot& snapshot);
[[nodiscard]] std::vector<IdeInlayHint> inlay_hints(const IdeSnapshot& snapshot);
[[nodiscard]] std::optional<IdeAstNodeInfo> ast_node_at_offset(const IdeSnapshot& snapshot, base::usize offset);
[[nodiscard]] IdeEditImpact edit_impact_for_range(
    const IdeSnapshot& snapshot, base::usize begin, base::usize removed_length);

} // namespace aurex::tooling
