#pragma once

#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/query_key.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::project {

inline constexpr base::u32 PROJECT_MODEL_SCHEMA_VERSION = 1;

enum class ProjectSessionKind : base::u8 {
    cli = 0,
    tooling,
};

struct ProjectTargetConfig {
    std::string emit_kind;
    std::string optimization_level;
    std::string clang_path;
    std::vector<std::string> clang_args;

    [[nodiscard]] friend bool operator==(const ProjectTargetConfig& lhs, const ProjectTargetConfig& rhs) = default;
};

struct ProjectCommandOptions {
    std::string diagnostic_format;
    bool query_pruning_enabled = true;

    [[nodiscard]] friend bool operator==(const ProjectCommandOptions& lhs, const ProjectCommandOptions& rhs) = default;
};

struct ProjectImportRoot {
    std::filesystem::path root;
    std::string package_identity;
    query::PackageKey package;

    [[nodiscard]] friend bool operator==(const ProjectImportRoot& lhs, const ProjectImportRoot& rhs) = default;
};

struct ProjectOpenBuffer {
    std::filesystem::path path;
    std::string uri;
    std::string package_identity;
    std::string virtual_buffer_identity;
    query::SourceRole source_role = query::SourceRole::virtual_buffer;
    query::StableFingerprint128 text_fingerprint;
    base::u64 text_size = 0;
    base::u64 generation = 0;

    [[nodiscard]] friend bool operator==(const ProjectOpenBuffer& lhs, const ProjectOpenBuffer& rhs) = default;
};

struct ProjectModelInput {
    ProjectSessionKind session_kind = ProjectSessionKind::cli;
    std::filesystem::path package_root;
    std::filesystem::path source_root;
    std::string package_identity;
    query::PackageKey package;
    std::vector<ProjectImportRoot> import_roots;
    ProjectTargetConfig target;
    ProjectCommandOptions command_options;
    std::vector<ProjectOpenBuffer> open_buffers;
};

struct ProjectModel {
    ProjectSessionKind session_kind = ProjectSessionKind::cli;
    std::filesystem::path package_root;
    std::filesystem::path source_root;
    std::string package_identity;
    query::PackageKey package;
    std::vector<ProjectImportRoot> import_roots;
    ProjectTargetConfig target;
    ProjectCommandOptions command_options;
    std::vector<ProjectOpenBuffer> open_buffers;
    query::ProjectKey key;
    query::StableFingerprint128 identity;
    base::u64 global_id = 0;
};

struct WorkspaceModel {
    std::vector<ProjectModel> projects;
    query::StableFingerprint128 identity;
    base::u64 global_id = 0;
};

[[nodiscard]] std::filesystem::path canonical_or_absolute_project_path(const std::filesystem::path& path);
[[nodiscard]] ProjectModel build_project_model(ProjectModelInput input);
[[nodiscard]] WorkspaceModel build_workspace_model(std::span<const ProjectModel> projects);
[[nodiscard]] query::StableFingerprint128 project_open_buffer_fingerprint(std::string_view text) noexcept;
[[nodiscard]] std::string stable_serialize(const ProjectTargetConfig& target);
[[nodiscard]] std::string stable_serialize(const ProjectCommandOptions& options);
[[nodiscard]] std::string stable_serialize(const ProjectModel& model);
[[nodiscard]] std::string stable_serialize(const WorkspaceModel& model);
[[nodiscard]] std::string debug_string(const ProjectModel& model);
[[nodiscard]] std::string debug_string(const WorkspaceModel& model);

} // namespace aurex::project
