#pragma once

#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/base/result.hpp>
#include <aurex/infrastructure/base/source.hpp>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::driver {

class CompilationProfiler;

struct LoadedModuleSource {
    base::SourceId source_id{};
    syntax::AstModule module;
};

struct ImportFileResolution {
    std::optional<std::filesystem::path> selected;
    std::optional<std::filesystem::path> selected_import_root;
    std::vector<std::filesystem::path> searched_candidates;
    std::vector<std::filesystem::path> matching_candidates;
};

[[nodiscard]] std::filesystem::path module_loader_canonical_or_absolute(const std::filesystem::path& path);

[[nodiscard]] std::filesystem::path module_part_base_dir(const std::filesystem::path& primary_file);

[[nodiscard]] std::filesystem::path module_part_file_path(
    const std::filesystem::path& primary_file, std::string_view part_name);

[[nodiscard]] std::optional<std::filesystem::path> owning_primary_for_part_file(const std::filesystem::path& part_file);

[[nodiscard]] std::string module_loader_case_fold(std::string_view text);

[[nodiscard]] bool module_loader_path_exists(const std::filesystem::path& path);

[[nodiscard]] std::optional<std::filesystem::path> find_import_file(const syntax::ModulePath& path,
    const std::filesystem::path& importer_dir, const std::vector<std::filesystem::path>& import_paths);

[[nodiscard]] ImportFileResolution resolve_import_file(const syntax::ModulePath& path,
    const std::filesystem::path& importer_dir, const std::vector<std::filesystem::path>& import_paths);

[[nodiscard]] ImportFileResolution resolve_import_file(const syntax::ModulePath& path,
    const std::filesystem::path& importer_dir, const std::optional<std::filesystem::path>& package_source_root,
    const std::vector<std::filesystem::path>& import_paths);

[[nodiscard]] std::vector<std::filesystem::path> import_candidates(const syntax::ModulePath& path,
    const std::filesystem::path& importer_dir, const std::vector<std::filesystem::path>& import_paths);

[[nodiscard]] std::string format_import_candidates(std::span<const std::filesystem::path> candidates);

[[nodiscard]] base::Result<LoadedModuleSource> load_module_source(const std::filesystem::path& canonical,
    base::SourceManager& sources, base::DiagnosticSink& diagnostics, CompilationProfiler* profiler,
    std::string_view profile_detail);

[[nodiscard]] base::Error module_loader_depth_exceeded_error();

[[nodiscard]] base::Result<void> report_cyclic_import(
    base::DiagnosticSink& diagnostics, const syntax::ModulePath* expected_module, std::string_view canonical_path);

[[nodiscard]] base::Result<void> report_import_resolution_failure(
    base::DiagnosticSink& diagnostics, const syntax::ModulePath& import_path, std::string_view formatted_candidates);

[[nodiscard]] base::Result<void> report_import_ambiguity(
    base::DiagnosticSink& diagnostics, const syntax::ModulePath& import_path, std::string_view formatted_candidates);

[[nodiscard]] base::Result<void> report_missing_module_part(base::DiagnosticSink& diagnostics,
    const syntax::ModulePath& module_path, const syntax::ModulePartDecl& part_decl,
    const std::filesystem::path& expected_path);

[[nodiscard]] base::Result<void> report_duplicate_module_part(
    base::DiagnosticSink& diagnostics, const syntax::ModulePath& module_path, const syntax::ModulePartDecl& part_decl);

[[nodiscard]] base::Result<void> report_module_part_case_collision(base::DiagnosticSink& diagnostics,
    const syntax::ModulePath& module_path, const syntax::ModulePartDecl& first_part,
    const syntax::ModulePartDecl& second_part);

[[nodiscard]] base::Result<void> report_module_part_imported(
    base::DiagnosticSink& diagnostics, const syntax::AstModule& module, const syntax::ModulePath* expected_module);

[[nodiscard]] base::Result<void> report_module_part_imported(base::DiagnosticSink& diagnostics,
    std::string_view module_name, std::string_view part_name, const base::SourceRange& range);

[[nodiscard]] base::Result<void> report_module_part_header_mismatch(base::DiagnosticSink& diagnostics,
    const syntax::ModulePath& expected_module, std::string_view expected_part, const syntax::AstModule& part_module);

[[nodiscard]] base::Result<void> report_module_part_root_owner_missing(base::DiagnosticSink& diagnostics,
    const syntax::AstModule& part_module, const std::filesystem::path& expected_primary);

[[nodiscard]] base::Result<void> report_module_part_unlisted_root(
    base::DiagnosticSink& diagnostics, const syntax::AstModule& part_module, const std::filesystem::path& primary);

[[nodiscard]] base::Result<void> report_module_part_artifact_root(
    base::DiagnosticSink& diagnostics, const syntax::AstModule& part_module, const std::filesystem::path& primary);

[[nodiscard]] base::Result<void> validate_cached_file_module_path(const syntax::AstModule& combined,
    syntax::ModuleId module_id, const syntax::ModulePath* expected_module, base::DiagnosticSink& diagnostics);

[[nodiscard]] base::Result<void> validate_importable_module_declaration(
    const syntax::AstModule& module, base::SourceId source_id, base::DiagnosticSink& diagnostics);

[[nodiscard]] base::Result<void> validate_module_import_path(const syntax::ModulePath& declared_module,
    const syntax::ModulePath* expected_module, base::DiagnosticSink& diagnostics);

[[nodiscard]] base::Result<void> validate_source_root_module_path(const syntax::ModulePath& declared_module,
    const std::filesystem::path& canonical_path, const std::filesystem::path& source_root,
    base::DiagnosticSink& diagnostics);

[[nodiscard]] base::Result<void> validate_unique_module_identity(std::string_view module_name,
    const std::filesystem::path& canonical_path, const std::filesystem::path& existing_path,
    const base::SourceRange& declaration_range, base::DiagnosticSink& diagnostics);

} // namespace aurex::driver
