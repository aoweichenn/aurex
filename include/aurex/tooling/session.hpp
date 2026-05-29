#pragma once

#include <aurex/base/result.hpp>
#include <aurex/tooling/ide.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aurex::tooling {

struct ToolingProjectConfig {
    std::string root_path;
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

struct ToolingSnapshotHandle {
    ToolingDocumentId document;
    ToolingDocumentVersion version;
    std::shared_ptr<const IdeSnapshot> snapshot;

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

struct ToolingDiagnostic {
    base::Severity severity = base::Severity::error;
    base::DiagnosticCategory category = base::DiagnosticCategory::general;
    base::DiagnosticCode code = base::DiagnosticCode::none;
    ToolingTextRange range;
    std::string severity_name;
    std::string category_name;
    std::string code_name;
    std::string message;
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

[[nodiscard]] ToolingDocumentId tooling_document_id_from_path(
    std::string_view path, const ToolingProjectConfig& config = {});
[[nodiscard]] ToolingDocumentId tooling_document_id_from_uri(
    std::string_view uri, const ToolingProjectConfig& config = {});
[[nodiscard]] std::string tooling_document_store_key(const ToolingDocumentId& id);
[[nodiscard]] std::string tooling_file_uri_from_path(std::string_view path);
[[nodiscard]] std::optional<std::string> tooling_path_from_file_uri(std::string_view uri);
[[nodiscard]] base::usize tooling_offset_for_position(
    std::string_view text, ToolingSourcePosition position) noexcept;
[[nodiscard]] ToolingSourcePosition tooling_position_for_offset(std::string_view text, base::usize offset) noexcept;

class ToolingSession {
public:
    ToolingSession();
    explicit ToolingSession(ToolingProjectConfig config);

    [[nodiscard]] const ToolingProjectConfig& project_config() const noexcept;
    [[nodiscard]] bool is_open(const ToolingDocumentId& id) const;
    [[nodiscard]] std::optional<ToolingDocumentState> document_state(const ToolingDocumentId& id) const;

    [[nodiscard]] base::Result<ToolingDocumentVersion> open_document(
        ToolingDocumentId id, std::string text, std::optional<base::i64> client_version = std::nullopt);
    [[nodiscard]] base::Result<ToolingDocumentVersion> change_document(
        const ToolingDocumentId& id, std::string text, std::optional<base::i64> client_version = std::nullopt);
    [[nodiscard]] base::Result<void> close_document(const ToolingDocumentId& id);
    [[nodiscard]] base::Result<ToolingSnapshotHandle> snapshot(const ToolingDocumentId& id);

    [[nodiscard]] base::Result<std::vector<ToolingDiagnostic>> diagnostics(const ToolingDocumentId& id);
    [[nodiscard]] base::Result<std::optional<ToolingHover>> hover_at_offset(
        const ToolingDocumentId& id, base::usize offset);
    [[nodiscard]] base::Result<std::optional<ToolingHover>> hover_at_position(
        const ToolingDocumentId& id, ToolingSourcePosition position);
    [[nodiscard]] base::Result<std::optional<ToolingDefinition>> definition_at_offset(
        const ToolingDocumentId& id, base::usize offset);
    [[nodiscard]] base::Result<std::optional<ToolingDefinition>> definition_at_position(
        const ToolingDocumentId& id, ToolingSourcePosition position);
    [[nodiscard]] base::Result<std::vector<ToolingReference>> references_at_offset(
        const ToolingDocumentId& id, base::usize offset);
    [[nodiscard]] base::Result<std::vector<ToolingReference>> references_at_position(
        const ToolingDocumentId& id, ToolingSourcePosition position);
    [[nodiscard]] base::Result<std::vector<ToolingDocumentSymbol>> document_symbols(const ToolingDocumentId& id);

private:
    struct DocumentSlot {
        ToolingDocumentState state;
        ToolingDocumentVersion cached_version;
        std::shared_ptr<const IdeSnapshot> cached_snapshot;
    };

    [[nodiscard]] ToolingDocumentId normalize_document_id(const ToolingDocumentId& id) const;
    [[nodiscard]] ToolingDocumentVersion next_version(std::optional<base::i64> client_version);
    [[nodiscard]] std::unordered_map<std::string, DocumentSlot>::iterator find_slot(const ToolingDocumentId& id);
    [[nodiscard]] std::unordered_map<std::string, DocumentSlot>::const_iterator find_slot(
        const ToolingDocumentId& id) const;

    ToolingProjectConfig config_;
    base::u64 next_generation_ = 0;
    std::unordered_map<std::string, DocumentSlot> documents_;
};

} // namespace aurex::tooling
