#include <aurex/base/config.hpp>
#include <aurex/driver/driver_messages.hpp>
#include <aurex/driver/file_cache.hpp>
#include <aurex/driver/module_loader.hpp>
#include <aurex/driver/profile.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/parse/parser.hpp>
#include <aurex/syntax/module.hpp>

#include <filesystem>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>

#include "module_loader_remap.hpp"

namespace aurex::driver {

namespace {

[[nodiscard]] std::filesystem::path canonical_or_absolute(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical;
    }
    return std::filesystem::absolute(path);
}

void push_error(base::DiagnosticSink& diagnostics, const base::SourceRange& range, std::string message)
{
    diagnostics.push(base::Diagnostic{
        base::Severity::error,
        range,
        std::move(message),
        base::DiagnosticCategory::module,
        base::DiagnosticCode::module_error,
    });
}

[[nodiscard]] base::Result<syntax::AstModule> lex_and_parse_module(const base::SourceId source_id,
    const std::string_view source_text, base::DiagnosticSink& diagnostics, CompilationProfiler* const profiler,
    const std::string_view detail)
{
    lex::Lexer lexer(source_id, source_text, diagnostics);
    auto token_result = [&] {
        ScopedCompilationPhase phase(profiler, "module.lex", detail);
        return lexer.tokenize();
    }();
    if (!token_result) {
        return base::Result<syntax::AstModule>::fail(token_result.error());
    }

    parse::Parser parser(token_result.value(), diagnostics);
    auto ast_result = [&] {
        ScopedCompilationPhase phase(profiler, "module.parse", detail);
        return parser.parse_module();
    }();
    if (!ast_result) {
        return base::Result<syntax::AstModule>::fail(ast_result.error());
    }
    return base::Result<syntax::AstModule>::ok(ast_result.take_value());
}

[[nodiscard]] std::optional<std::filesystem::path> find_import_file(const syntax::ModulePath& path,
    const std::filesystem::path& importer_dir, const std::vector<std::filesystem::path>& import_paths)
{
    const std::filesystem::path relative = syntax::module_path_to_relative_file(path);
    const auto exists = [](const std::filesystem::path& candidate) {
        std::error_code error;
        return std::filesystem::exists(candidate, error) && !error;
    };

    std::filesystem::path importer_candidate = importer_dir / relative;
    if (exists(importer_candidate)) {
        return importer_candidate;
    }
    for (const std::filesystem::path& import_path : import_paths) {
        std::filesystem::path candidate = import_path / relative;
        if (exists(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<std::filesystem::path> import_candidates(const syntax::ModulePath& path,
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

[[nodiscard]] std::string format_import_candidates(const std::span<const std::filesystem::path> candidates)
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

} // namespace

ModuleLoader::ModuleLoader(const CompilerInvocation& invocation, base::SourceManager& sources,
    base::DiagnosticSink& diagnostics, CompilationProfiler* const profiler) noexcept
    : invocation_(invocation), sources_(sources), diagnostics_(diagnostics), profiler_(profiler),
      import_paths_(invocation.import_paths)
{
}

base::Result<syntax::AstModule> ModuleLoader::load_root()
{
    syntax::AstModule combined;
    const auto result =
        this->load_file(canonical_or_absolute(this->invocation_.input_path), combined, 0, true, nullptr);
    if (!result) {
        return base::Result<syntax::AstModule>::fail(result.error());
    }
    combined.finalize_identifiers();
    return base::Result<syntax::AstModule>::ok(std::move(combined));
}

std::span<const ModuleRecord> ModuleLoader::modules() const noexcept
{
    return this->modules_;
}

base::Result<syntax::ModuleId> ModuleLoader::load_file(const std::filesystem::path& path, syntax::AstModule& combined,
    const base::usize depth, const bool is_root, const syntax::ModulePath* expected_module)
{
    if (depth > base::config::AUREX_MAX_INCLUDE_DEPTH) {
        return base::Result<syntax::ModuleId>::fail(
            {base::ErrorCode::invalid_source, std::string(DRIVER_IMPORT_DEPTH_EXCEEDED)});
    }

    const std::filesystem::path canonical = canonical_or_absolute(path);
    const std::string key = canonical.string();
    if (this->loading_files_.contains(key)) {
        push_error(this->diagnostics_, expected_module != nullptr ? expected_module->range : base::SourceRange{},
            driver_cyclic_import_message(key));
        return base::Result<syntax::ModuleId>::fail(
            {base::ErrorCode::parse_error, std::string(DRIVER_MODULE_LOADING_FAILED)});
    }
    if (const auto loaded = this->loaded_file_modules_.find(key); loaded != this->loaded_file_modules_.end()) {
        const syntax::ModuleId module_id = loaded->second;
        if (expected_module != nullptr && syntax::is_valid(module_id) && module_id.value < combined.modules.size()
            && !syntax::module_paths_equal(combined.modules[module_id.value].path, *expected_module)) {
            push_error(this->diagnostics_, expected_module->range,
                driver_module_import_mismatch_message(
                    syntax::module_path_to_string(combined.modules[module_id.value].path),
                    syntax::module_path_to_string(*expected_module)));
            return base::Result<syntax::ModuleId>::fail(
                {base::ErrorCode::parse_error, std::string(DRIVER_MODULE_LOADING_FAILED)});
        }
        return base::Result<syntax::ModuleId>::ok(loaded->second);
    }
    this->loading_files_.insert(key);

    auto source_result = [&] {
        ScopedCompilationPhase phase(this->profiler_, "module.read", key);
        return read_text_file(canonical);
    }();
    if (!source_result) {
        this->loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail(source_result.error());
    }

    const base::SourceId source_id = this->sources_.add_source(canonical.string(), source_result.take_value());
    auto ast_result =
        lex_and_parse_module(source_id, this->sources_.text(source_id), this->diagnostics_, this->profiler_, key);
    if (!ast_result) {
        this->loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail(ast_result.error());
    }

    syntax::AstModule module = ast_result.take_value();
    if (module.module_path.parts.empty()) {
        push_error(this->diagnostics_, base::SourceRange{source_id, 0, 0},
            std::string(DRIVER_IMPORTABLE_MODULE_DECL_REQUIRED));
        this->loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail(
            {base::ErrorCode::parse_error, std::string(DRIVER_MODULE_LOADING_FAILED)});
    }
    if (expected_module != nullptr && !syntax::module_paths_equal(module.module_path, *expected_module)) {
        push_error(this->diagnostics_, module.module_path.range,
            driver_module_import_mismatch_message(
                syntax::module_path_to_string(module.module_path), syntax::module_path_to_string(*expected_module)));
        this->loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail(
            {base::ErrorCode::parse_error, std::string(DRIVER_MODULE_LOADING_FAILED)});
    }
    const std::string module_name = syntax::module_path_to_string(module.module_path);
    const auto module_inserted = this->loaded_modules_.emplace(
        module_name, LoadedModule{canonical, syntax::INVALID_MODULE_ID, module.module_path.range});
    if (!module_inserted.second && module_inserted.first->second.path != canonical) {
        push_error(this->diagnostics_, module.module_path.range,
            driver_duplicate_module_name_message(module_name, module_inserted.first->second.path.string()));
        this->loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail(
            {base::ErrorCode::parse_error, std::string(DRIVER_MODULE_LOADING_FAILED)});
    }

    syntax::ModuleId module_id = module_inserted.first->second.id;
    if (module_inserted.second) {
        module_id = syntax::ModuleId{static_cast<base::u32>(combined.modules.size())};
        module_inserted.first->second.id = module_id;
        syntax::ModuleInfo info;
        info.path = module.module_path;
        combined.intern_module_path(info.path);
        combined.modules.push_back(std::move(info));
        this->modules_.push_back(ModuleRecord{module_name, canonical});
    }
    if (is_root) {
        combined.module_path = module.module_path;
        combined.intern_module_path(combined.module_path);
    }

    const std::vector<syntax::ImportDecl> imports = module.imports;
    std::vector<syntax::ResolvedImport> direct_imports;
    direct_imports.reserve(imports.size());
    for (const syntax::ImportDecl& import : imports) {
        const auto import_file = find_import_file(import.path, canonical.parent_path(), this->import_paths_);
        if (!import_file) {
            const std::vector<std::filesystem::path> candidates =
                import_candidates(import.path, canonical.parent_path(), this->import_paths_);
            push_error(this->diagnostics_, import.path.range,
                driver_import_resolve_failed_message(
                    syntax::module_path_to_string(import.path), format_import_candidates(candidates)));
            this->loading_files_.erase(key);
            return base::Result<syntax::ModuleId>::fail(
                {base::ErrorCode::io_error, std::string(DRIVER_MODULE_LOADING_FAILED)});
        }
        auto import_result =
            this->load_file(canonical_or_absolute(*import_file), combined, depth + 1, false, &import.path);
        if (!import_result) {
            this->loading_files_.erase(key);
            return import_result;
        }
        syntax::ResolvedImport resolved{
            import_result.value(),
            import.alias,
            import.alias_range,
            import.visibility,
            import.alias_id,
        };
        combined.intern_resolved_import(resolved);
        direct_imports.push_back(resolved);
    }
    {
        ScopedCompilationPhase phase(this->profiler_, "module.append", module_name);
        if (is_root && imports.empty() && module_id.value == 0 && combined.modules.size() == 1
            && ast_payloads_empty(combined)) {
            move_root_module_into_empty_combined(combined, std::move(module), module_id);
        } else {
            if (syntax::is_valid(module_id) && module_id.value < combined.modules.size()) {
                combined.modules[module_id.value].imports = std::move(direct_imports);
            }
            append_module_into(combined, std::move(module), is_root, module_id);
        }
    }
    this->loading_files_.erase(key);
    this->loaded_file_modules_.emplace(key, module_id);
    return base::Result<syntax::ModuleId>::ok(module_id);
}

} // namespace aurex::driver
