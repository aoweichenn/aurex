#pragma once

#include <aurex/base/diagnostic.hpp>
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

struct IdePipelineStageOwner {
    std::string id;
    std::string profile_name;
    std::string input;
    std::string output;
    std::string diagnostic_ownership;
    std::string cache_query_impact;
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
    std::vector<IdePipelineStageOwner> owner_stages;
};

struct IdeQuerySnapshot {
    query::QuerySourceStageKeys source_stage;
    std::vector<query::QueryRecord> records;
    std::vector<query::QueryDependencyEdge> dependencies;
};

struct IdeSnapshot {
    base::SourceManager sources;
    base::SourceId source_id{};
    syntax::LosslessSyntaxTree lossless;
    syntax::AstModule ast;
    sema::CheckedModule checked;
    IdeQuerySnapshot query;
    std::vector<IdeDiagnostic> diagnostics;
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
    base::SourceRange range{};
    std::string name;
    std::string kind;
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

struct IdeEditImpact {
    syntax::LosslessNodeId node = syntax::INVALID_LOSSLESS_NODE_ID;
    base::SourceRange range{};
    std::optional<syntax::LosslessNodeKey> node_key;
    base::usize first_token = 0;
    base::usize token_count = 0;
    bool valid = false;
};

[[nodiscard]] IdeSnapshot build_ide_snapshot(const IdeSnapshotRequest& request);
[[nodiscard]] std::optional<IdeTokenInfo> token_info_at_offset(const IdeSnapshot& snapshot, base::usize offset);
[[nodiscard]] std::optional<IdeDefinition> definition_at_offset(const IdeSnapshot& snapshot, base::usize offset);
[[nodiscard]] std::vector<IdeReference> references_at_offset(const IdeSnapshot& snapshot, base::usize offset);
[[nodiscard]] std::optional<IdeHoverInfo> hover_at_offset(const IdeSnapshot& snapshot, base::usize offset);
[[nodiscard]] IdeEditImpact edit_impact_for_range(
    const IdeSnapshot& snapshot, base::usize begin, base::usize removed_length);

} // namespace aurex::tooling
