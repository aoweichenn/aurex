#pragma once

#include <aurex/application/tooling/ide.hpp>
#include <aurex/infrastructure/base/result.hpp>
#include <aurex/infrastructure/project/project_model.hpp>
#include <aurex/infrastructure/query/query_reuse.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aurex::tooling {

struct ToolingReusePlan;

struct ToolingProjectConfig {
    std::string root_path;
    std::string source_root;
    std::string package_identity = "ide";
    std::vector<std::string> import_paths;
    query::SourceRole default_source_role = query::SourceRole::virtual_buffer;
};

struct ToolingDocumentId {
    std::string uri;
    std::string path;
    std::string package_identity = "ide";
    std::string virtual_buffer_identity;
    query::SourceRole source_role = query::SourceRole::virtual_buffer;

    [[nodiscard]] friend bool operator==(const ToolingDocumentId& lhs, const ToolingDocumentId& rhs) = default;
};

struct ToolingDocumentVersion {
    base::u64 generation = 0;
    std::optional<base::i64> client_version;

    [[nodiscard]] friend bool operator==(
        const ToolingDocumentVersion& lhs, const ToolingDocumentVersion& rhs) = default;
};

struct ToolingDocumentState {
    ToolingDocumentId id;
    ToolingDocumentVersion version;
    std::string text;
    bool open = false;
};

enum class ToolingIncrementalSnapshotStatus : base::u8 {
    clean_build,
    cached_snapshot,
    previous_context,
    rejected_mismatched_previous,
    rejected_stale_previous,
    rejected_malformed_previous,
};

struct ToolingIncrementalSnapshotInput {
    ToolingDocumentId document;
    ToolingDocumentVersion current_version;
    ToolingDocumentVersion previous_version;
    bool has_previous_snapshot = false;
    bool previous_document_matches = false;
    bool previous_version_is_older = false;
    bool previous_context_malformed = false;
    base::usize previous_query_records = 0;
    base::usize previous_dependency_edges = 0;
    base::usize previous_semantic_facts = 0;
};

struct ToolingQueryReuseExecutionSummary {
    base::usize total_query_decisions = 0;
    base::usize reused_query_records = 0;
    base::usize recomputed_query_records = 0;
    base::usize malformed_query_records = 0;
    base::usize reused_semantic_facts = 0;
    base::usize recomputed_semantic_facts = 0;
    base::usize invalidated_semantic_facts = 0;
    base::usize malformed_semantic_facts = 0;
    bool executed = false;
    bool body_local = false;
};

struct ToolingWorkspaceIndexUpdateStats {
    base::usize previous_document_facts = 0;
    base::usize current_document_facts = 0;
    base::usize retained_facts = 0;
    base::usize replaced_facts = 0;
    base::usize removed_facts = 0;
    base::usize inserted_facts = 0;
    bool updated = false;
};

struct ToolingSyntaxReuseExecutionSummary {
    base::usize previous_nodes = 0;
    base::usize current_nodes = 0;
    base::usize reused_nodes = 0;
    base::usize recomputed_nodes = 0;
    base::usize invalidated_nodes = 0;
    base::usize stable_key_collisions = 0;
    bool executed = false;
};

struct ToolingIncrementalSnapshotResult {
    ToolingIncrementalSnapshotStatus status = ToolingIncrementalSnapshotStatus::clean_build;
    ToolingIncrementalSnapshotInput input;
    base::usize current_query_records = 0;
    base::usize current_dependency_edges = 0;
    base::usize current_semantic_facts = 0;
    std::shared_ptr<const ToolingReusePlan> reuse_plan;
    ToolingQueryReuseExecutionSummary reuse_execution;
    ToolingSyntaxReuseExecutionSummary syntax_reuse;
    ToolingWorkspaceIndexUpdateStats workspace_update;
    std::string fallback_reason;
    bool used_previous_context = false;
    bool from_cache = false;
};

struct ToolingSnapshotHandle {
    ToolingDocumentId document;
    ToolingDocumentVersion version;
    std::shared_ptr<const IdeSnapshot> snapshot;
    ToolingIncrementalSnapshotResult incremental;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return this->snapshot != nullptr;
    }
};

struct ToolingSourcePosition {
    // Zero-based line and byte character, matching LSP shape while remaining
    // protocol-neutral for the compiler-facing API.
    base::usize line = 0;
    base::usize character = 0;
};

struct ToolingTextRange {
    std::string path;
    base::SourceRange range{};
    base::LineColumn start{};
    base::LineColumn end{};
};

struct ToolingDiagnosticChild {
    base::Severity severity = base::Severity::note;
    base::DiagnosticCategory category = base::DiagnosticCategory::general;
    base::DiagnosticCode code = base::DiagnosticCode::none;
    ToolingTextRange range;
    std::string severity_name;
    std::string category_name;
    std::string code_name;
    std::string message;
};

struct ToolingDiagnostic {
    base::Severity severity = base::Severity::error;
    base::DiagnosticCategory category = base::DiagnosticCategory::general;
    base::DiagnosticCode code = base::DiagnosticCode::none;
    ToolingTextRange range;
    std::string severity_name;
    std::string category_name;
    std::string code_name;
    std::string message;
    std::vector<ToolingDiagnosticChild> children;
    std::vector<IdePipelineStageOwner> owner_stages;
    IdeModulePartContext source_part;
};

struct ToolingDefinition {
    query::DefKey key;
    query::MemberKey member;
    query::GenericInstanceKey generic_instance;
    ToolingTextRange range;
    std::string name;
    std::string kind;
    std::string stable_definition_key;
    std::string stable_member_key;
    std::string stable_generic_instance_key;
    base::u32 part_index = 0;
    bool valid = false;
};

struct ToolingReference {
    ToolingTextRange range;
    std::string name;
    bool is_definition = false;
};

struct ToolingHover {
    ToolingTextRange range;
    std::string label;
    std::string detail;
    std::string semantic_fact_key;
    std::string semantic_fact_detail;
    std::vector<IdePipelineStageOwner> owner_stages;
    std::optional<ToolingDefinition> definition;
    bool valid = false;
};

struct ToolingCompletionItem {
    IdeCompletionContextKind context = IdeCompletionContextKind::expression;
    ToolingTextRange replacement_range;
    std::string label;
    std::string kind;
    std::string detail;
    std::string stable_definition_key;
    std::string stable_member_key;
    std::string stable_generic_instance_key;
    base::u32 part_index = 0;
    bool local = false;
    bool checked = false;
    bool from_workspace = false;
};

struct ToolingSemanticToken {
    ToolingTextRange range;
    std::string text;
    std::string token_type;
    std::vector<std::string> modifiers;
    std::string stable_definition_key;
    std::string stable_member_key;
    bool checked = false;
};

struct ToolingInlayHint {
    ToolingTextRange range;
    ToolingSourcePosition position;
    std::string label;
    std::string kind;
    bool checked = false;
};

struct ToolingCodeActionEdit {
    ToolingDocumentId document;
    ToolingTextRange range;
    std::string new_text;
};

struct ToolingCodeAction {
    std::string title;
    std::string kind;
    std::string diagnostic_code;
    std::string data;
    std::vector<ToolingCodeActionEdit> edits;
    bool preferred = false;
};

struct ToolingRenameConflict {
    ToolingTextRange range;
    std::string name;
    std::string kind;
    std::string reason;
    std::string stable_definition_key;
    std::string stable_member_key;
};

struct ToolingRenameEdit {
    ToolingDocumentId document;
    ToolingTextRange range;
    std::string new_text;
};

struct ToolingRenamePlan {
    ToolingDocumentVersion version;
    ToolingDefinition target;
    std::string old_name;
    std::string new_name;
    std::vector<ToolingRenameEdit> edits;
    std::vector<ToolingRenameConflict> conflicts;
    bool valid = false;
};

struct ToolingWorkspaceSymbol {
    ToolingDocumentId document;
    ToolingTextRange range;
    std::string name;
    std::string kind;
    std::string detail;
    std::string container_name;
    std::string stable_query_key;
    std::string stable_definition_key;
    std::string stable_member_key;
    std::string stable_generic_instance_key;
    base::u32 part_index = 0;
    bool checked = false;
};

struct ToolingAstNode {
    IdeAstNodeKind kind = IdeAstNodeKind::item;
    ToolingTextRange range;
    std::string name;
    std::string detail;
    std::string stable_definition_key;
    std::string stable_body_key;
    base::u32 part_index = 0;
    bool valid = false;
};

struct ToolingDocumentSymbol {
    std::string name;
    std::string kind;
    std::string detail;
    ToolingTextRange range;
    ToolingTextRange selection_range;
    query::QueryKey query;
    query::DefKey definition;
    std::string stable_query_key;
    std::string stable_definition_key;
    base::u32 part_index = 0;
    bool checked = false;
};

enum class ToolingReuseFactStatus : base::u8 {
    unchanged,
    recomputed,
    invalidated,
    malformed,
};

struct ToolingReuseFact {
    ToolingReuseFactStatus status = ToolingReuseFactStatus::malformed;
    query::QueryRecordChangeStatus change_status = query::QueryRecordChangeStatus::malformed;
    query::QueryKey query;
    query::DefKey definition;
    query::MemberKey member;
    query::BodyKey body;
    query::GenericInstanceKey generic_instance;
    ToolingTextRange range;
    std::string name;
    std::string kind;
    std::string detail;
    std::string stable_query_key;
    std::string stable_definition_key;
    std::string stable_member_key;
    std::string stable_body_key;
    std::string stable_generic_instance_key;
    base::u32 part_index = 0;
};

struct ToolingInvalidationRoot {
    query::QueryKey query;
    ToolingTextRange range;
    std::string name;
    std::string kind;
    std::string reason;
    std::string stable_query_key;
};

struct ToolingDependencyDiff {
    base::usize unchanged = 0;
    base::usize added = 0;
    base::usize removed = 0;
};

struct ToolingReuseSummary {
    base::usize total_facts = 0;
    base::usize unchanged_facts = 0;
    base::usize recomputed_facts = 0;
    base::usize invalidated_facts = 0;
    base::usize malformed_facts = 0;
    bool body_local = false;
};

struct ToolingReusePlan {
    IdeEditImpact impact;
    ToolingTextRange impact_range;
    query::QueryReusePlan query_plan;
    ToolingDependencyDiff dependencies;
    ToolingReuseSummary summary;
    std::vector<ToolingInvalidationRoot> invalidation_roots;
    std::vector<ToolingReuseFact> facts;
    bool valid = false;
};

struct ToolingDocumentTextEdit {
    base::usize begin = 0;
    base::usize removed_length = 0;
    std::string inserted_text;
};

struct ToolingDocumentChangeResult {
    ToolingDocumentVersion version;
    ToolingDocumentTextEdit applied_edit;
    IdeEditImpact edit_impact;
    ToolingReusePlan reuse_plan;
    ToolingIncrementalSnapshotResult incremental;
};

struct ToolingIndexedSemanticFact {
    ToolingDocumentId document;
    ToolingDocumentVersion version;
    IdeSemanticFactKind semantic_kind = IdeSemanticFactKind::item_signature;
    query::QueryKey query;
    query::DefKey definition;
    query::MemberKey member;
    query::BodyKey body;
    query::GenericInstanceKey generic_instance;
    ToolingTextRange range;
    std::string name;
    std::string kind;
    std::string detail;
    std::string stable_query_key;
    std::string stable_definition_key;
    std::string stable_member_key;
    std::string stable_body_key;
    std::string stable_generic_instance_key;
    base::u32 part_index = 0;
    bool checked = false;
};

struct ToolingWorkspaceIndexStats {
    base::usize documents = 0;
    base::usize facts = 0;
    base::usize definitions = 0;
    base::usize members = 0;
    base::usize bodies = 0;
    base::usize generic_instances = 0;
};

class ToolingWorkspaceSemanticIndex {
public:
    void clear();
    void remove_document(const ToolingDocumentId& id);
    void index_snapshot(const ToolingSnapshotHandle& handle);
    [[nodiscard]] ToolingWorkspaceIndexUpdateStats update_snapshot(const ToolingSnapshotHandle& handle);
    [[nodiscard]] ToolingWorkspaceIndexUpdateStats update_snapshot(
        const ToolingSnapshotHandle& handle, const std::vector<ToolingIndexedSemanticFact>& previous_facts);

    [[nodiscard]] ToolingWorkspaceIndexStats stats() const noexcept;
    [[nodiscard]] std::vector<ToolingIndexedSemanticFact> all_facts() const;
    [[nodiscard]] std::vector<ToolingIndexedSemanticFact> facts_for_document(const ToolingDocumentId& id) const;
    [[nodiscard]] std::vector<ToolingIndexedSemanticFact> definitions(query::DefKey key) const;
    [[nodiscard]] std::vector<ToolingIndexedSemanticFact> members(query::MemberKey key) const;
    [[nodiscard]] std::vector<ToolingIndexedSemanticFact> bodies(query::BodyKey key) const;
    [[nodiscard]] std::vector<ToolingIndexedSemanticFact> generic_instances(const query::GenericInstanceKey& key) const;

private:
    using FactMap = std::unordered_map<std::string, std::vector<ToolingIndexedSemanticFact>>;

    static void erase_document_from_map(FactMap& map, std::string_view document_key);
    static std::vector<ToolingIndexedSemanticFact> facts_for_key(const FactMap& map, std::string_view stable_key);
    void rebuild_stats() noexcept;

    FactMap facts_by_document_;
    FactMap definitions_;
    FactMap members_;
    FactMap bodies_;
    FactMap generic_instances_;
    ToolingWorkspaceIndexStats stats_;
};

[[nodiscard]] ToolingDocumentId tooling_document_id_from_path(
    std::string_view path, const ToolingProjectConfig& config = {});
[[nodiscard]] ToolingDocumentId tooling_document_id_from_uri(
    std::string_view uri, const ToolingProjectConfig& config = {});
[[nodiscard]] std::string tooling_document_store_key(const ToolingDocumentId& id);
[[nodiscard]] std::string tooling_file_uri_from_path(std::string_view path);
[[nodiscard]] std::optional<std::string> tooling_path_from_file_uri(std::string_view uri);
[[nodiscard]] base::usize tooling_offset_for_position(std::string_view text, ToolingSourcePosition position) noexcept;
[[nodiscard]] ToolingSourcePosition tooling_position_for_offset(std::string_view text, base::usize offset) noexcept;
[[nodiscard]] std::string_view tooling_incremental_snapshot_status_name(
    ToolingIncrementalSnapshotStatus status) noexcept;
[[nodiscard]] ToolingIncrementalSnapshotResult tooling_incremental_snapshot_result(
    const ToolingIncrementalSnapshotInput& input, const IdeSnapshot& snapshot);
[[nodiscard]] std::string_view tooling_reuse_fact_status_name(ToolingReuseFactStatus status) noexcept;
[[nodiscard]] ToolingReusePlan tooling_plan_reuse(
    const IdeSnapshot& before, const IdeSnapshot& after, const IdeEditImpact& impact);

class ToolingSession {
public:
    ToolingSession();
    explicit ToolingSession(ToolingProjectConfig config);

    [[nodiscard]] const ToolingProjectConfig& project_config() const noexcept;
    [[nodiscard]] const project::WorkspaceModel& workspace_model() const noexcept;
    [[nodiscard]] bool is_open(const ToolingDocumentId& id) const;
    [[nodiscard]] bool is_generation_current(const ToolingDocumentId& id, ToolingDocumentVersion version) const;
    [[nodiscard]] std::optional<ToolingDocumentState> document_state(const ToolingDocumentId& id) const;

    [[nodiscard]] base::Result<ToolingDocumentVersion> open_document(
        ToolingDocumentId id, std::string text, std::optional<base::i64> client_version = std::nullopt);
    [[nodiscard]] base::Result<ToolingDocumentVersion> change_document(
        const ToolingDocumentId& id, std::string text, std::optional<base::i64> client_version = std::nullopt);
    [[nodiscard]] base::Result<ToolingDocumentVersion> change_document_range(const ToolingDocumentId& id,
        ToolingDocumentTextEdit edit, std::optional<base::i64> client_version = std::nullopt);
    [[nodiscard]] base::Result<ToolingDocumentChangeResult> change_document_with_reuse_plan(const ToolingDocumentId& id,
        std::string text, base::usize edit_begin, base::usize removed_length,
        std::optional<base::i64> client_version = std::nullopt);
    [[nodiscard]] base::Result<ToolingDocumentChangeResult> change_document_range_with_reuse_plan(
        const ToolingDocumentId& id, ToolingDocumentTextEdit edit,
        std::optional<base::i64> client_version = std::nullopt);
    [[nodiscard]] base::Result<void> close_document(const ToolingDocumentId& id);
    [[nodiscard]] base::Result<ToolingSnapshotHandle> snapshot(const ToolingDocumentId& id);
    [[nodiscard]] const ToolingWorkspaceSemanticIndex& workspace_index() const noexcept;

    [[nodiscard]] base::Result<std::vector<ToolingDiagnostic>> diagnostics(const ToolingDocumentId& id);
    [[nodiscard]] base::Result<std::optional<ToolingHover>> hover_at_offset(
        const ToolingDocumentId& id, base::usize offset);
    [[nodiscard]] base::Result<std::optional<ToolingHover>> hover_at_position(
        const ToolingDocumentId& id, ToolingSourcePosition position);
    [[nodiscard]] base::Result<std::optional<ToolingDefinition>> definition_at_offset(
        const ToolingDocumentId& id, base::usize offset);
    [[nodiscard]] base::Result<std::optional<ToolingDefinition>> definition_at_position(
        const ToolingDocumentId& id, ToolingSourcePosition position);
    [[nodiscard]] base::Result<std::optional<ToolingAstNode>> ast_node_at_offset(
        const ToolingDocumentId& id, base::usize offset);
    [[nodiscard]] base::Result<std::optional<ToolingAstNode>> ast_node_at_position(
        const ToolingDocumentId& id, ToolingSourcePosition position);
    [[nodiscard]] base::Result<std::vector<ToolingReference>> references_at_offset(
        const ToolingDocumentId& id, base::usize offset);
    [[nodiscard]] base::Result<std::vector<ToolingReference>> references_at_position(
        const ToolingDocumentId& id, ToolingSourcePosition position);
    [[nodiscard]] base::Result<std::vector<ToolingDocumentSymbol>> document_symbols(const ToolingDocumentId& id);
    [[nodiscard]] base::Result<std::vector<ToolingCompletionItem>> completion_at_offset(
        const ToolingDocumentId& id, base::usize offset);
    [[nodiscard]] base::Result<std::vector<ToolingCompletionItem>> completion_at_position(
        const ToolingDocumentId& id, ToolingSourcePosition position);
    [[nodiscard]] base::Result<ToolingRenamePlan> rename_at_offset(
        const ToolingDocumentId& id, base::usize offset, std::string new_name);
    [[nodiscard]] base::Result<ToolingRenamePlan> rename_at_position(
        const ToolingDocumentId& id, ToolingSourcePosition position, std::string new_name);
    [[nodiscard]] base::Result<std::vector<ToolingSemanticToken>> semantic_tokens(const ToolingDocumentId& id);
    [[nodiscard]] base::Result<std::vector<ToolingInlayHint>> inlay_hints(const ToolingDocumentId& id);
    [[nodiscard]] base::Result<std::vector<ToolingCodeAction>> code_actions(const ToolingDocumentId& id);
    [[nodiscard]] base::Result<std::vector<ToolingWorkspaceSymbol>> workspace_symbols(std::string_view query);

private:
    struct DocumentSlot {
        ToolingDocumentState state;
        ToolingDocumentVersion cached_version;
        std::shared_ptr<const IdeSnapshot> cached_snapshot;
        ToolingDocumentVersion previous_cached_version;
        std::shared_ptr<const IdeSnapshot> previous_cached_snapshot;
        std::optional<IdeEditImpact> pending_edit_impact;
        std::vector<ToolingIndexedSemanticFact> pending_workspace_facts;
        ToolingIncrementalSnapshotResult last_snapshot_result;
    };

    [[nodiscard]] ToolingDocumentId normalize_document_id(const ToolingDocumentId& id) const;
    [[nodiscard]] ToolingDocumentVersion next_version(std::optional<base::i64> client_version);
    [[nodiscard]] ToolingIncrementalSnapshotInput incremental_input_for_slot(const DocumentSlot& slot) const;
    void refresh_workspace_model();
    [[nodiscard]] std::unordered_map<std::string, DocumentSlot>::iterator find_slot(const ToolingDocumentId& id);
    [[nodiscard]] std::unordered_map<std::string, DocumentSlot>::const_iterator find_slot(
        const ToolingDocumentId& id) const;

    ToolingProjectConfig config_;
    project::WorkspaceModel workspace_model_;
    base::u64 next_generation_ = 0;
    std::unordered_map<std::string, DocumentSlot> documents_;
    ToolingWorkspaceSemanticIndex workspace_index_;
};

} // namespace aurex::tooling
