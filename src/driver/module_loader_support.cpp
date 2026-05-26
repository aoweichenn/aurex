#include "module_loader_support.hpp"

#include <aurex/driver/driver_messages.hpp>
#include <aurex/driver/file_cache.hpp>
#include <aurex/driver/package_identity.hpp>
#include <aurex/driver/pipeline_stage.hpp>
#include <aurex/driver/profile.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/parse/parser.hpp>
#include <aurex/syntax/module.hpp>

#include <cctype>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

namespace aurex::driver {
namespace {

struct ImportSearchCandidate {
    std::filesystem::path path;
    std::optional<std::filesystem::path> selected_import_root;
};

[[nodiscard]] bool import_candidate_exists(const std::filesystem::path& candidate)
{
    std::error_code error;
    return std::filesystem::exists(candidate, error) && !error;
}

[[nodiscard]] bool path_is_within_or_equal(
    const std::filesystem::path& child, const std::filesystem::path& parent) noexcept
{
    auto child_part = child.begin();
    auto parent_part = parent.begin();
    while (parent_part != parent.end()) {
        if (child_part == child.end() || *child_part != *parent_part) {
            return false;
        }
        ++child_part;
        ++parent_part;
    }
    return true;
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

[[nodiscard]] std::vector<ImportSearchCandidate> import_search_candidates(const syntax::ModulePath& path,
    const std::filesystem::path& importer_dir, const std::optional<std::filesystem::path>& package_source_root,
    const std::vector<std::filesystem::path>& import_paths)
{
    const std::filesystem::path relative = syntax::module_path_to_relative_file(path);
    std::vector<ImportSearchCandidate> candidates;
    candidates.reserve(import_paths.size() + 1);
    candidates.push_back(ImportSearchCandidate{
        package_source_root.value_or(importer_dir) / relative,
        std::nullopt,
    });
    for (const std::filesystem::path& import_path : import_paths) {
        const std::filesystem::path canonical_import_root = module_loader_canonical_or_absolute(import_path);
        const std::optional<std::filesystem::path> import_source_root =
            package_source_root_for_import_root(canonical_import_root.string());
        candidates.push_back(ImportSearchCandidate{
            import_source_root.value_or(import_path) / relative,
            canonical_import_root,
        });
    }
    return candidates;
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

std::filesystem::path module_part_base_dir(const std::filesystem::path& primary_file)
{
    std::filesystem::path base = primary_file;
    base.replace_extension();
    base += ".parts";
    return base;
}

std::filesystem::path module_part_file_path(const std::filesystem::path& primary_file, const std::string_view part_name)
{
    return module_part_base_dir(primary_file) / (std::string(part_name) + ".ax");
}

std::optional<std::filesystem::path> owning_primary_for_part_file(const std::filesystem::path& part_file)
{
    const std::filesystem::path part_dir = part_file.parent_path();
    if (part_dir.extension() != ".parts") {
        return std::nullopt;
    }

    std::filesystem::path primary = part_dir.parent_path() / part_dir.stem();
    primary += ".ax";
    return primary;
}

std::string module_loader_case_fold(const std::string_view text)
{
    std::string folded;
    folded.reserve(text.size());
    for (const char ch : text) {
        folded.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return folded;
}

bool module_loader_path_exists(const std::filesystem::path& path)
{
    return import_candidate_exists(path);
}

std::optional<std::filesystem::path> find_import_file(const syntax::ModulePath& path,
    const std::filesystem::path& importer_dir, const std::vector<std::filesystem::path>& import_paths)
{
    return resolve_import_file(path, importer_dir, import_paths).selected;
}

ImportFileResolution resolve_import_file(const syntax::ModulePath& path, const std::filesystem::path& importer_dir,
    const std::vector<std::filesystem::path>& import_paths)
{
    return resolve_import_file(path, importer_dir, std::nullopt, import_paths);
}

ImportFileResolution resolve_import_file(const syntax::ModulePath& path, const std::filesystem::path& importer_dir,
    const std::optional<std::filesystem::path>& package_source_root,
    const std::vector<std::filesystem::path>& import_paths)
{
    ImportFileResolution resolution;
    const std::vector<ImportSearchCandidate> candidates =
        import_search_candidates(path, importer_dir, package_source_root, import_paths);
    resolution.searched_candidates.reserve(candidates.size());
    for (const ImportSearchCandidate& candidate : candidates) {
        resolution.searched_candidates.push_back(candidate.path);
    }

    std::unordered_set<std::string> seen_canonical_paths;
    for (const ImportSearchCandidate& candidate : candidates) {
        if (!import_candidate_exists(candidate.path)) {
            continue;
        }

        const std::filesystem::path canonical = module_loader_canonical_or_absolute(candidate.path);
        if (seen_canonical_paths.insert(canonical.string()).second) {
            resolution.matching_candidates.push_back(canonical);
        }
    }

    if (resolution.matching_candidates.size() == 1) {
        resolution.selected = resolution.matching_candidates.front();
        for (const ImportSearchCandidate& candidate : candidates) {
            const std::filesystem::path canonical = module_loader_canonical_or_absolute(candidate.path);
            if (canonical != *resolution.selected) {
                continue;
            }
            resolution.selected_import_root = candidate.selected_import_root;
            break;
        }
    }
    return resolution;
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

base::Result<void> report_import_ambiguity(base::DiagnosticSink& diagnostics, const syntax::ModulePath& import_path,
    const std::string_view formatted_candidates)
{
    push_module_loader_error(diagnostics, import_path.range,
        driver_import_ambiguous_message(syntax::module_path_to_string(import_path), formatted_candidates));
    return fail_module_loading(base::ErrorCode::parse_error);
}

base::Result<void> report_missing_module_part(base::DiagnosticSink& diagnostics, const syntax::ModulePath& module_path,
    const syntax::ModulePartDecl& part_decl, const std::filesystem::path& expected_path)
{
    push_module_loader_error(diagnostics, part_decl.range,
        driver_missing_module_part_message(
            syntax::module_path_to_string(module_path), part_decl.name, expected_path.string()));
    return fail_module_loading(base::ErrorCode::io_error);
}

base::Result<void> report_duplicate_module_part(
    base::DiagnosticSink& diagnostics, const syntax::ModulePath& module_path, const syntax::ModulePartDecl& part_decl)
{
    push_module_loader_error(diagnostics, part_decl.range,
        driver_duplicate_module_part_message(syntax::module_path_to_string(module_path), part_decl.name));
    return fail_module_loading(base::ErrorCode::parse_error);
}

base::Result<void> report_module_part_case_collision(base::DiagnosticSink& diagnostics,
    const syntax::ModulePath& module_path, const syntax::ModulePartDecl& first_part,
    const syntax::ModulePartDecl& second_part)
{
    push_module_loader_error(diagnostics, second_part.range,
        driver_module_part_case_collision_message(
            syntax::module_path_to_string(module_path), first_part.name, second_part.name));
    return fail_module_loading(base::ErrorCode::parse_error);
}

base::Result<void> report_module_part_imported(
    base::DiagnosticSink& diagnostics, const syntax::AstModule& module, const syntax::ModulePath* const expected_module)
{
    const base::SourceRange range = expected_module != nullptr ? expected_module->range : module.part_header.range;
    return report_module_part_imported(
        diagnostics, syntax::module_path_to_string(module.module_path), module.part_header.name, range);
}

base::Result<void> report_module_part_imported(base::DiagnosticSink& diagnostics, const std::string_view module_name,
    const std::string_view part_name, const base::SourceRange& range)
{
    push_module_loader_error(diagnostics, range, driver_module_part_imported_message(module_name, part_name));
    return fail_module_loading(base::ErrorCode::parse_error);
}

base::Result<void> report_module_part_header_mismatch(base::DiagnosticSink& diagnostics,
    const syntax::ModulePath& expected_module, const std::string_view expected_part,
    const syntax::AstModule& part_module)
{
    push_module_loader_error(diagnostics, part_module.part_header.range,
        driver_module_part_header_mismatch_message(syntax::module_path_to_string(expected_module), expected_part,
            syntax::module_path_to_string(part_module.module_path), part_module.part_header.name));
    return fail_module_loading(base::ErrorCode::parse_error);
}

base::Result<void> report_module_part_root_owner_missing(base::DiagnosticSink& diagnostics,
    const syntax::AstModule& part_module, const std::filesystem::path& expected_primary)
{
    push_module_loader_error(diagnostics, part_module.part_header.range,
        driver_module_part_root_owner_missing_message(part_module.part_header.name, expected_primary.string()));
    return fail_module_loading(base::ErrorCode::io_error);
}

base::Result<void> report_module_part_unlisted_root(
    base::DiagnosticSink& diagnostics, const syntax::AstModule& part_module, const std::filesystem::path& primary)
{
    push_module_loader_error(diagnostics, part_module.part_header.range,
        driver_module_part_unlisted_root_message(part_module.part_header.name, primary.string()));
    return fail_module_loading(base::ErrorCode::parse_error);
}

base::Result<void> report_module_part_artifact_root(
    base::DiagnosticSink& diagnostics, const syntax::AstModule& part_module, const std::filesystem::path& primary)
{
    push_module_loader_error(diagnostics, part_module.part_header.range,
        driver_module_part_artifact_root_message(part_module.part_header.name, primary.string()));
    return fail_module_loading(base::ErrorCode::parse_error);
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

base::Result<void> validate_source_root_module_path(const syntax::ModulePath& declared_module,
    const std::filesystem::path& canonical_path, const std::filesystem::path& source_root,
    base::DiagnosticSink& diagnostics)
{
    const std::filesystem::path canonical_source_root = module_loader_canonical_or_absolute(source_root);
    if (!path_is_within_or_equal(canonical_path, canonical_source_root)) {
        push_module_loader_error(diagnostics, declared_module.range,
            driver_module_source_root_outside_message(canonical_path.string(), canonical_source_root.string()));
        return fail_module_loading(base::ErrorCode::parse_error);
    }

    const std::filesystem::path expected_path = module_loader_canonical_or_absolute(
        canonical_source_root / syntax::module_path_to_relative_file(declared_module));
    if (canonical_path == expected_path) {
        return base::Result<void>::ok();
    }

    push_module_loader_error(diagnostics, declared_module.range,
        driver_module_source_root_mismatch_message(
            syntax::module_path_to_string(declared_module), canonical_path.string(), expected_path.string()));
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
