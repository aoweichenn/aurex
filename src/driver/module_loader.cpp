#include <aurex/base/config.hpp>
#include <aurex/driver/module_loader.hpp>
#include <aurex/driver/pipeline_stage.hpp>
#include <aurex/driver/profile.hpp>
#include <aurex/syntax/module.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "module_loader_remap.hpp"
#include "module_loader_support.hpp"

namespace aurex::driver {

namespace {

[[nodiscard]] bool emit_kind_produces_artifact(const EmitKind emit_kind) noexcept
{
    return emit_kind == EmitKind::ir || emit_kind == EmitKind::llvm_ir || emit_kind == EmitKind::assembly
        || emit_kind == EmitKind::object || emit_kind == EmitKind::executable;
}

[[nodiscard]] bool module_part_name_matches(
    const syntax::ModulePartHeader& header, const syntax::ModulePartDecl& declaration) noexcept
{
    return header.name == declaration.name;
}

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
    if (const auto loaded_part = this->loaded_part_files_.find(key); loaded_part != this->loaded_part_files_.end()) {
        const base::SourceRange range = expected_module != nullptr ? expected_module->range : base::SourceRange{};
        return base::Result<syntax::ModuleId>::fail(report_module_part_imported(
            this->diagnostics_, loaded_part->second.module_name, loaded_part->second.part_name, range)
                .error());
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

    auto loaded_source = load_module_source(canonical, this->sources_, this->diagnostics_, this->profiler_, key);
    if (!loaded_source) {
        return base::Result<syntax::ModuleId>::fail(loaded_source.error());
    }

    LoadedModuleSource loaded = loaded_source.take_value();
    syntax::AstModule module = std::move(loaded.module);
    if (module.file_kind == syntax::ModuleFileKind::part) {
        if (is_root) {
            return this->redirect_root_part(canonical, key, module, combined, depth);
        }
        return base::Result<syntax::ModuleId>::fail(
            report_module_part_imported(this->diagnostics_, module, expected_module).error());
    }

    LoadingFileScope loading_file_scope{this->loading_files_, key};
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
        this->loaded_modules_.emplace(module_name, LoadedLogicalModule{canonical, syntax::INVALID_MODULE_ID, {}});
    if (!module_inserted.second) {
        const auto identity_validation = validate_unique_module_identity(module_name, canonical,
            module_inserted.first->second.primary_path, module.module_path.range, this->diagnostics_);
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

    std::vector<syntax::ResolvedImport> direct_imports;
    direct_imports.reserve(module.imports.size());
    const auto import_result = this->resolve_imports_for_file(module, canonical, combined, depth, direct_imports);
    if (!import_result) {
        return base::Result<syntax::ModuleId>::fail(import_result.error());
    }

    auto parts_result = this->load_declared_parts(canonical, module_name, module.module_path, module.part_declarations,
        combined, depth, module_id, direct_imports);
    if (!parts_result) {
        return base::Result<syntax::ModuleId>::fail(parts_result.error());
    }
    std::vector<syntax::AstModule> part_modules = parts_result.take_value();

    {
        ScopedCompilationPhase phase(this->profiler_, PipelineStageId::module_append, module_name);
        if (is_root && module.imports.empty() && module.part_declarations.empty() && module_id.value == 0
            && combined.modules.size() == 1 && ast_payloads_empty(combined)) {
            move_root_module_into_empty_combined(combined, std::move(module), module_id);
        } else {
            if (syntax::is_valid(module_id) && module_id.value < combined.modules.size()) {
                combined.modules[module_id.value].imports = std::move(direct_imports);
            }
            append_module_into(combined, std::move(module), is_root, module_id);
            for (syntax::AstModule& part_module : part_modules) {
                append_module_into(combined, std::move(part_module), false, module_id);
            }
        }
    }
    this->loaded_file_modules_.emplace(key, module_id);
    return base::Result<syntax::ModuleId>::ok(module_id);
}

base::Result<void> ModuleLoader::resolve_imports_for_file(const syntax::AstModule& module,
    const std::filesystem::path& canonical, syntax::AstModule& combined, const base::usize depth,
    std::vector<syntax::ResolvedImport>& direct_imports)
{
    direct_imports.reserve(direct_imports.size() + module.imports.size());
    for (const syntax::ImportDecl& import : module.imports) {
        const ImportFileResolution resolution =
            resolve_import_file(import.path, canonical.parent_path(), this->import_paths_);
        if (resolution.matching_candidates.empty()) {
            return report_import_resolution_failure(
                this->diagnostics_, import.path, format_import_candidates(resolution.searched_candidates));
        }
        if (resolution.matching_candidates.size() > 1) {
            return report_import_ambiguity(
                this->diagnostics_, import.path, format_import_candidates(resolution.matching_candidates));
        }

        auto import_result = this->load_file(*resolution.selected, combined, depth + 1, false, &import.path);
        if (!import_result) {
            return base::Result<void>::fail(import_result.error());
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
    return base::Result<void>::ok();
}

base::Result<std::vector<syntax::AstModule>> ModuleLoader::load_declared_parts(
    const std::filesystem::path& primary_path, const std::string& module_name, const syntax::ModulePath& module_path,
    const std::span<const syntax::ModulePartDecl> part_declarations, syntax::AstModule& combined,
    const base::usize depth, const syntax::ModuleId module_id, std::vector<syntax::ResolvedImport>& direct_imports)
{
    std::vector<syntax::AstModule> part_modules;
    part_modules.reserve(part_declarations.size());
    std::unordered_map<std::string, const syntax::ModulePartDecl*> parts_by_name;
    std::unordered_map<std::string, const syntax::ModulePartDecl*> parts_by_folded_name;
    parts_by_name.reserve(part_declarations.size());
    parts_by_folded_name.reserve(part_declarations.size());

    for (const syntax::ModulePartDecl& part_decl : part_declarations) {
        const std::string part_name{part_decl.name};
        const auto name_inserted = parts_by_name.emplace(part_name, &part_decl);
        if (!name_inserted.second) {
            return base::Result<std::vector<syntax::AstModule>>::fail(
                report_duplicate_module_part(this->diagnostics_, module_path, part_decl).error());
        }

        const std::string folded_name = module_loader_case_fold(part_decl.name);
        const auto folded_inserted = parts_by_folded_name.emplace(folded_name, &part_decl);
        if (!folded_inserted.second && folded_inserted.first->second->name != part_decl.name) {
            return base::Result<std::vector<syntax::AstModule>>::fail(report_module_part_case_collision(
                this->diagnostics_, module_path, *folded_inserted.first->second, part_decl)
                    .error());
        }
    }

    for (const syntax::ModulePartDecl& part_decl : part_declarations) {
        const std::string part_name{part_decl.name};
        const std::filesystem::path part_path = module_part_file_path(primary_path, part_decl.name);
        if (!module_loader_path_exists(part_path)) {
            return base::Result<std::vector<syntax::AstModule>>::fail(
                report_missing_module_part(this->diagnostics_, module_path, part_decl, part_path).error());
        }

        auto part_result =
            this->load_module_part(part_path, part_decl, module_path, combined, depth, module_id, direct_imports);
        if (!part_result) {
            return base::Result<std::vector<syntax::AstModule>>::fail(part_result.error());
        }
        syntax::AstModule part_module = part_result.take_value();
        const std::filesystem::path canonical_part_path = module_loader_canonical_or_absolute(part_path);
        if (auto logical = this->loaded_modules_.find(module_name); logical != this->loaded_modules_.end()) {
            logical->second.parts.push_back(LoadedModulePart{part_name, canonical_part_path});
        }
        part_modules.push_back(std::move(part_module));
    }

    return base::Result<std::vector<syntax::AstModule>>::ok(std::move(part_modules));
}

base::Result<syntax::AstModule> ModuleLoader::load_module_part(const std::filesystem::path& part_path,
    const syntax::ModulePartDecl& part_decl, const syntax::ModulePath& expected_module, syntax::AstModule& combined,
    const base::usize depth, const syntax::ModuleId module_id, std::vector<syntax::ResolvedImport>& direct_imports)
{
    if (depth > base::config::AUREX_MAX_INCLUDE_DEPTH) {
        return base::Result<syntax::AstModule>::fail(module_loader_depth_exceeded_error());
    }

    const std::filesystem::path canonical = module_loader_canonical_or_absolute(part_path);
    const std::string key = canonical.string();
    if (this->loading_files_.contains(key)) {
        return base::Result<syntax::AstModule>::fail(
            report_cyclic_import(this->diagnostics_, &expected_module, key).error());
    }
    LoadingFileScope loading_file_scope{this->loading_files_, key};

    auto loaded_source = load_module_source(canonical, this->sources_, this->diagnostics_, this->profiler_, key);
    if (!loaded_source) {
        return base::Result<syntax::AstModule>::fail(loaded_source.error());
    }

    LoadedModuleSource loaded = loaded_source.take_value();
    syntax::AstModule part_module = std::move(loaded.module);
    if (part_module.file_kind != syntax::ModuleFileKind::part
        || !syntax::module_paths_equal(part_module.module_path, expected_module)
        || !module_part_name_matches(part_module.part_header, part_decl)) {
        return base::Result<syntax::AstModule>::fail(
            report_module_part_header_mismatch(this->diagnostics_, expected_module, part_decl.name, part_module)
                .error());
    }

    const auto import_result = this->resolve_imports_for_file(part_module, canonical, combined, depth, direct_imports);
    if (!import_result) {
        return base::Result<syntax::AstModule>::fail(import_result.error());
    }

    this->loaded_part_files_.emplace(key,
        LoadedPartFile{
            syntax::module_path_to_string(part_module.module_path),
            std::string(part_module.part_header.name),
        });
    this->loaded_file_modules_.emplace(key, module_id);
    return base::Result<syntax::AstModule>::ok(std::move(part_module));
}

base::Result<syntax::ModuleId> ModuleLoader::redirect_root_part(const std::filesystem::path& canonical,
    const std::string& key, const syntax::AstModule& module, syntax::AstModule& combined, const base::usize depth)
{
    const std::optional<std::filesystem::path> primary_path = owning_primary_for_part_file(canonical);
    if (!primary_path.has_value() || !module_loader_path_exists(*primary_path)) {
        const std::filesystem::path expected_primary = primary_path.value_or(std::filesystem::path{});
        return base::Result<syntax::ModuleId>::fail(
            report_module_part_root_owner_missing(this->diagnostics_, module, expected_primary).error());
    }

    const std::filesystem::path canonical_primary = module_loader_canonical_or_absolute(*primary_path);
    if (emit_kind_produces_artifact(this->invocation_.emit_kind)) {
        return base::Result<syntax::ModuleId>::fail(
            report_module_part_artifact_root(this->diagnostics_, module, canonical_primary).error());
    }

    auto owner_result = this->load_file(canonical_primary, combined, depth, true, nullptr);
    if (!owner_result) {
        return owner_result;
    }
    if (!this->loaded_part_files_.contains(key)) {
        return base::Result<syntax::ModuleId>::fail(
            report_module_part_unlisted_root(this->diagnostics_, module, canonical_primary).error());
    }
    return owner_result;
}

} // namespace aurex::driver
