#include "module_loader_support.hpp"

#include <aurex/driver/driver_messages.hpp>
#include <aurex/driver/file_cache.hpp>
#include <aurex/driver/pipeline_stage.hpp>
#include <aurex/driver/profile.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/parse/parser.hpp>
#include <aurex/syntax/module.hpp>

#include <sstream>
#include <string>
#include <utility>

namespace aurex::driver {
namespace {

[[nodiscard]] bool import_candidate_exists(const std::filesystem::path& candidate)
{
    std::error_code error;
    return std::filesystem::exists(candidate, error) && !error;
}

void push_module_loader_error(base::DiagnosticSink& diagnostics, const base::SourceRange& range, std::string message)
{
    diagnostics.push(base::Diagnostic{
        base::Severity::error,
        range,
        std::move(message),
        base::DiagnosticCategory::module,
        base::DiagnosticCode::module_error,
    });
}

[[nodiscard]] base::Error module_loader_failure(const base::ErrorCode code)
{
    return base::Error{code, std::string(DRIVER_MODULE_LOADING_FAILED)};
}

[[nodiscard]] base::Result<void> fail_module_loading(const base::ErrorCode code)
{
    return base::Result<void>::fail(module_loader_failure(code));
}

[[nodiscard]] base::Result<syntax::AstModule> lex_and_parse_module(const base::SourceId source_id,
    const std::string_view source_text, base::DiagnosticSink& diagnostics, CompilationProfiler* const profiler,
    const std::string_view detail)
{
    lex::Lexer lexer(source_id, source_text, diagnostics);
    auto token_result = [&] {
        ScopedCompilationPhase phase(profiler, PipelineStageId::module_lex, detail);
        return lexer.tokenize();
    }();
    if (!token_result) {
        return base::Result<syntax::AstModule>::fail(token_result.error());
    }

    parse::Parser parser(token_result.value(), diagnostics);
    auto ast_result = [&] {
        ScopedCompilationPhase phase(profiler, PipelineStageId::module_parse, detail);
        return parser.parse_module();
    }();
    if (!ast_result) {
        return base::Result<syntax::AstModule>::fail(ast_result.error());
    }
    return base::Result<syntax::AstModule>::ok(ast_result.take_value());
}

} // namespace

std::filesystem::path module_loader_canonical_or_absolute(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical;
    }
    return std::filesystem::absolute(path);
}

std::optional<std::filesystem::path> find_import_file(const syntax::ModulePath& path,
    const std::filesystem::path& importer_dir, const std::vector<std::filesystem::path>& import_paths)
{
    const std::filesystem::path relative = syntax::module_path_to_relative_file(path);

    const std::filesystem::path importer_candidate = importer_dir / relative;
    if (import_candidate_exists(importer_candidate)) {
        return importer_candidate;
    }
    for (const std::filesystem::path& import_path : import_paths) {
        const std::filesystem::path candidate = import_path / relative;
        if (import_candidate_exists(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::vector<std::filesystem::path> import_candidates(const syntax::ModulePath& path,
    const std::filesystem::path& importer_dir, const std::vector<std::filesystem::path>& import_paths)
{
    const std::filesystem::path relative = syntax::module_path_to_relative_file(path);
    std::vector<std::filesystem::path> candidates;
    candidates.reserve(import_paths.size() + 1);
    candidates.push_back(importer_dir / relative);
    for (const std::filesystem::path& import_path : import_paths) {
        candidates.push_back(import_path / relative);
    }
    return candidates;
}

std::string format_import_candidates(const std::span<const std::filesystem::path> candidates)
{
    std::ostringstream out;
    for (base::usize i = 0; i < candidates.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << candidates[i].string();
    }
    return out.str();
}

base::Result<LoadedModuleSource> load_module_source(const std::filesystem::path& canonical,
    base::SourceManager& sources, base::DiagnosticSink& diagnostics, CompilationProfiler* const profiler,
    const std::string_view profile_detail)
{
    auto source_result = [&] {
        ScopedCompilationPhase phase(profiler, PipelineStageId::module_read, profile_detail);
        return read_text_file(canonical);
    }();
    if (!source_result) {
        return base::Result<LoadedModuleSource>::fail(source_result.error());
    }

    const base::SourceId source_id = sources.add_source(canonical.string(), source_result.take_value());
    auto ast_result = lex_and_parse_module(source_id, sources.text(source_id), diagnostics, profiler, profile_detail);
    if (!ast_result) {
        return base::Result<LoadedModuleSource>::fail(ast_result.error());
    }
    return base::Result<LoadedModuleSource>::ok(LoadedModuleSource{
        source_id,
        ast_result.take_value(),
    });
}

base::Error module_loader_depth_exceeded_error()
{
    return base::Error{base::ErrorCode::invalid_source, std::string(DRIVER_IMPORT_DEPTH_EXCEEDED)};
}

base::Result<void> report_cyclic_import(base::DiagnosticSink& diagnostics,
    const syntax::ModulePath* const expected_module, const std::string_view canonical_path)
{
    push_module_loader_error(diagnostics, expected_module != nullptr ? expected_module->range : base::SourceRange{},
        driver_cyclic_import_message(canonical_path));
    return fail_module_loading(base::ErrorCode::parse_error);
}

base::Result<void> report_import_resolution_failure(base::DiagnosticSink& diagnostics,
    const syntax::ModulePath& import_path, const std::string_view formatted_candidates)
{
    push_module_loader_error(diagnostics, import_path.range,
        driver_import_resolve_failed_message(syntax::module_path_to_string(import_path), formatted_candidates));
    return fail_module_loading(base::ErrorCode::io_error);
}

base::Result<void> validate_cached_file_module_path(const syntax::AstModule& combined, const syntax::ModuleId module_id,
    const syntax::ModulePath* const expected_module, base::DiagnosticSink& diagnostics)
{
    if (expected_module == nullptr || !syntax::is_valid(module_id) || module_id.value >= combined.modules.size()) {
        return base::Result<void>::ok();
    }

    const syntax::ModulePath& loaded_module_path = combined.modules[module_id.value].path;
    if (syntax::module_paths_equal(loaded_module_path, *expected_module)) {
        return base::Result<void>::ok();
    }

    push_module_loader_error(diagnostics, expected_module->range,
        driver_module_import_mismatch_message(
            syntax::module_path_to_string(loaded_module_path), syntax::module_path_to_string(*expected_module)));
    return fail_module_loading(base::ErrorCode::parse_error);
}

base::Result<void> validate_importable_module_declaration(
    const syntax::AstModule& module, const base::SourceId source_id, base::DiagnosticSink& diagnostics)
{
    if (!module.module_path.parts.empty()) {
        return base::Result<void>::ok();
    }

    push_module_loader_error(
        diagnostics, base::SourceRange{source_id, 0, 0}, std::string(DRIVER_IMPORTABLE_MODULE_DECL_REQUIRED));
    return fail_module_loading(base::ErrorCode::parse_error);
}

base::Result<void> validate_module_import_path(const syntax::ModulePath& declared_module,
    const syntax::ModulePath* const expected_module, base::DiagnosticSink& diagnostics)
{
    if (expected_module == nullptr || syntax::module_paths_equal(declared_module, *expected_module)) {
        return base::Result<void>::ok();
    }

    push_module_loader_error(diagnostics, declared_module.range,
        driver_module_import_mismatch_message(
            syntax::module_path_to_string(declared_module), syntax::module_path_to_string(*expected_module)));
    return fail_module_loading(base::ErrorCode::parse_error);
}

base::Result<void> validate_unique_module_identity(const std::string_view module_name,
    const std::filesystem::path& canonical_path, const std::filesystem::path& existing_path,
    const base::SourceRange& declaration_range, base::DiagnosticSink& diagnostics)
{
    if (existing_path == canonical_path) {
        return base::Result<void>::ok();
    }

    push_module_loader_error(
        diagnostics, declaration_range, driver_duplicate_module_name_message(module_name, existing_path.string()));
    return fail_module_loading(base::ErrorCode::parse_error);
}

} // namespace aurex::driver
