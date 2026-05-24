#include <aurex/base/config.hpp>
#include <aurex/driver/module_loader.hpp>
#include <aurex/driver/pipeline_stage.hpp>
#include <aurex/driver/profile.hpp>
#include <aurex/syntax/module.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "module_loader_remap.hpp"
#include "module_loader_support.hpp"

namespace aurex::driver {

namespace {

class LoadingFileScope final {
public:
    LoadingFileScope(std::unordered_set<std::string>& loading_files, std::string key)
        : loading_files_(&loading_files), key_(std::move(key))
    {
        this->loading_files_->insert(this->key_);
    }

    LoadingFileScope(const LoadingFileScope&) = delete;
    LoadingFileScope& operator=(const LoadingFileScope&) = delete;
    LoadingFileScope(LoadingFileScope&&) = delete;
    LoadingFileScope& operator=(LoadingFileScope&&) = delete;

    ~LoadingFileScope()
    {
        if (this->loading_files_ != nullptr) {
            this->loading_files_->erase(this->key_);
        }
    }

private:
    std::unordered_set<std::string>* loading_files_ = nullptr;
    std::string key_;
};

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
        this->load_file(module_loader_canonical_or_absolute(this->invocation_.input_path), combined, 0, true, nullptr);
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
        return base::Result<syntax::ModuleId>::fail(module_loader_depth_exceeded_error());
    }

    const std::filesystem::path canonical = module_loader_canonical_or_absolute(path);
    const std::string key = canonical.string();
    if (this->loading_files_.contains(key)) {
        return base::Result<syntax::ModuleId>::fail(
            report_cyclic_import(this->diagnostics_, expected_module, key).error());
    }
    if (const auto loaded = this->loaded_file_modules_.find(key); loaded != this->loaded_file_modules_.end()) {
        const syntax::ModuleId module_id = loaded->second;
        const auto validation =
            validate_cached_file_module_path(combined, module_id, expected_module, this->diagnostics_);
        if (!validation) {
            return base::Result<syntax::ModuleId>::fail(validation.error());
        }
        return base::Result<syntax::ModuleId>::ok(loaded->second);
    }
    LoadingFileScope loading_file_scope{this->loading_files_, key};

    auto loaded_source = load_module_source(canonical, this->sources_, this->diagnostics_, this->profiler_, key);
    if (!loaded_source) {
        return base::Result<syntax::ModuleId>::fail(loaded_source.error());
    }

    LoadedModuleSource loaded = loaded_source.take_value();
    syntax::AstModule module = std::move(loaded.module);
    const auto declaration_validation =
        validate_importable_module_declaration(module, loaded.source_id, this->diagnostics_);
    if (!declaration_validation) {
        return base::Result<syntax::ModuleId>::fail(declaration_validation.error());
    }
    const auto import_path_validation =
        validate_module_import_path(module.module_path, expected_module, this->diagnostics_);
    if (!import_path_validation) {
        return base::Result<syntax::ModuleId>::fail(import_path_validation.error());
    }
    const std::string module_name = syntax::module_path_to_string(module.module_path);
    const auto module_inserted =
        this->loaded_modules_.emplace(module_name, LoadedModule{canonical, syntax::INVALID_MODULE_ID});
    if (!module_inserted.second) {
        const auto identity_validation = validate_unique_module_identity(
            module_name, canonical, module_inserted.first->second.path, module.module_path.range, this->diagnostics_);
        if (!identity_validation) {
            return base::Result<syntax::ModuleId>::fail(identity_validation.error());
        }
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
            return base::Result<syntax::ModuleId>::fail(
                report_import_resolution_failure(this->diagnostics_, import.path, format_import_candidates(candidates))
                    .error());
        }
        auto import_result = this->load_file(
            module_loader_canonical_or_absolute(*import_file), combined, depth + 1, false, &import.path);
        if (!import_result) {
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
        ScopedCompilationPhase phase(
            this->profiler_, pipeline_stage_profile_name(PipelineStageId::module_append), module_name);
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
    this->loaded_file_modules_.emplace(key, module_id);
    return base::Result<syntax::ModuleId>::ok(module_id);
}

} // namespace aurex::driver
