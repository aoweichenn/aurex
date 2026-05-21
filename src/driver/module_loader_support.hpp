#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/result.hpp>
#include <aurex/base/source.hpp>
#include <aurex/syntax/ast.hpp>

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

[[nodiscard]] std::filesystem::path module_loader_canonical_or_absolute(const std::filesystem::path& path);

[[nodiscard]] std::optional<std::filesystem::path> find_import_file(const syntax::ModulePath& path,
    const std::filesystem::path& importer_dir, const std::vector<std::filesystem::path>& import_paths);

[[nodiscard]] std::vector<std::filesystem::path> import_candidates(const syntax::ModulePath& path,
    const std::filesystem::path& importer_dir, const std::vector<std::filesystem::path>& import_paths);

[[nodiscard]] std::string format_import_candidates(std::span<const std::filesystem::path> candidates);

[[nodiscard]] base::Result<LoadedModuleSource> load_module_source(const std::filesystem::path& canonical,
    base::SourceManager& sources, base::DiagnosticSink& diagnostics, CompilationProfiler* profiler,
    std::string_view profile_detail);

[[nodiscard]] base::Error module_loader_depth_exceeded_error();

[[nodiscard]] base::Result<void> report_cyclic_import(base::DiagnosticSink& diagnostics,
    const syntax::ModulePath* expected_module, std::string_view canonical_path);

[[nodiscard]] base::Result<void> report_import_resolution_failure(base::DiagnosticSink& diagnostics,
    const syntax::ModulePath& import_path, std::string_view formatted_candidates);

[[nodiscard]] base::Result<void> validate_cached_file_module_path(const syntax::AstModule& combined,
    syntax::ModuleId module_id, const syntax::ModulePath* expected_module, base::DiagnosticSink& diagnostics);

[[nodiscard]] base::Result<void> validate_importable_module_declaration(
    const syntax::AstModule& module, base::SourceId source_id, base::DiagnosticSink& diagnostics);

[[nodiscard]] base::Result<void> validate_module_import_path(const syntax::ModulePath& declared_module,
    const syntax::ModulePath* expected_module, base::DiagnosticSink& diagnostics);

[[nodiscard]] base::Result<void> validate_unique_module_identity(std::string_view module_name,
    const std::filesystem::path& canonical_path, const std::filesystem::path& existing_path,
    const base::SourceRange& declaration_range, base::DiagnosticSink& diagnostics);

} // namespace aurex::driver
