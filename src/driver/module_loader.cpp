#include <aurex/base/config.hpp>
#include <aurex/driver/module_loader.hpp>
#include <aurex/driver/package_identity.hpp>
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

[[nodiscard]] ModuleRecord make_module_record(const std::string& module_name, const std::filesystem::path& primary_path,
    const query::PackageKey package, const syntax::ModuleId id)
{
    ModuleRecord record;
    record.name = module_name;
    record.path = primary_path;
    record.package = package;
    record.id = id;
    record.parts.push_back(ModulePartRecord{
        {},
        primary_path,
        0,
        ModulePartRecordKind::primary,
    });
    return record;
}

constexpr char MODULE_LOADER_LOGICAL_MODULE_KEY_SEPARATOR = '\x1f';

[[nodiscard]] std::string logical_module_key(const query::PackageKey package, const std::string_view module_name)
{
    std::string key = std::to_string(package.global_id);
    key.push_back(MODULE_LOADER_LOGICAL_MODULE_KEY_SEPARATOR);
    key.append(module_name.data(), module_name.size());
    return key;
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
      import_paths_(invocation.import_paths), root_context_(PackageLoadContext{
                                                  package_key_for_invocation(invocation),
                                                  package_source_root_for_invocation(invocation),
                                              })
{
}

base::Result<syntax::AstModule> ModuleLoader::load_root()
{
    syntax::AstModule combined;
    const auto result = this->load_file(module_loader_canonical_or_absolute(this->invocation_.input_path), combined, 0,
        true, nullptr, this->root_context_);
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

void ModuleLoader::record_module_imports(const syntax::ModuleId module_id, const std::string_view owner_part,
    const bool owner_is_primary, const std::span<const syntax::ResolvedImport> imports,
    const syntax::AstModule& combined)
{
    if (!syntax::is_valid(module_id) || module_id.value >= this->modules_.size()) {
        return;
    }
    ModuleRecord& record = this->modules_[module_id.value];
    record.imports.reserve(record.imports.size() + imports.size());
    for (const syntax::ResolvedImport& import : imports) {
        if (!syntax::is_valid(import.module) || import.module.value >= combined.modules.size()) {
            continue;
        }
        record.imports.push_back(ModuleImportRecord{
            std::string(owner_part),
            syntax::module_path_to_string(combined.modules[import.module.value].path),
            std::string(import.alias),
            this->modules_[import.module.value].package,
            owner_is_primary,
            import.visibility,
            syntax::visibility_is_public(import.visibility),
        });
    }
}

void ModuleLoader::record_module_part(
    const syntax::ModuleId module_id, std::string name, std::filesystem::path path, const base::u32 stable_index)
{
    if (!syntax::is_valid(module_id) || module_id.value >= this->modules_.size()) {
        return;
    }
    this->modules_[module_id.value].parts.push_back(ModulePartRecord{
        std::move(name),
        std::move(path),
        stable_index,
        ModulePartRecordKind::named,
    });
}

base::Result<syntax::ModuleId> ModuleLoader::load_file(const std::filesystem::path& path, syntax::AstModule& combined,
    const base::usize depth, const bool is_root, const syntax::ModulePath* expected_module,
    const PackageLoadContext& package_context)
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
            return this->redirect_root_part(canonical, key, module, combined, depth, package_context);
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
    const std::string logical_key = logical_module_key(package_context.package, module_name);
    const auto module_inserted = this->loaded_modules_.emplace(
        logical_key, LoadedLogicalModule{canonical, package_context.package, syntax::INVALID_MODULE_ID, {}});
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
        this->modules_.push_back(make_module_record(module_name, canonical, package_context.package, module_id));
    }
    if (is_root) {
        combined.module_path = module.module_path;
        combined.intern_module_path(combined.module_path);
    }

    std::vector<syntax::ResolvedImport> direct_imports;
    direct_imports.reserve(module.imports.size());
    const auto import_result =
        this->resolve_imports_for_file(module, canonical, combined, depth, direct_imports, package_context);
    if (!import_result) {
        return base::Result<syntax::ModuleId>::fail(import_result.error());
    }
    this->record_module_imports(module_id, {}, true, direct_imports, combined);

    auto parts_result = this->load_declared_parts(canonical, module_name, module.module_path, module.part_declarations,
        combined, depth, module_id, package_context);
    if (!parts_result) {
        return base::Result<syntax::ModuleId>::fail(parts_result.error());
    }
    std::vector<LoadedModulePartAst> part_modules = parts_result.take_value();

    {
        ScopedCompilationPhase phase(this->profiler_, PipelineStageId::module_append, module_name);
        if (is_root && module.imports.empty() && module.part_declarations.empty() && module_id.value == 0
            && combined.modules.size() == 1 && ast_payloads_empty(combined)) {
            move_root_module_into_empty_combined(combined, std::move(module), module_id);
        } else {
            append_module_into(combined, std::move(module), is_root, module_id, direct_imports);
            if (syntax::is_valid(module_id) && module_id.value < combined.modules.size()) {
                combined.modules[module_id.value].imports = std::move(direct_imports);
            }
            for (LoadedModulePartAst& part_module : part_modules) {
                append_module_into(combined, std::move(part_module.module), false, module_id, part_module.imports);
            }
        }
    }
    this->loaded_file_modules_.emplace(key, module_id);
    return base::Result<syntax::ModuleId>::ok(module_id);
}

base::Result<void> ModuleLoader::resolve_imports_for_file(const syntax::AstModule& module,
    const std::filesystem::path& canonical, syntax::AstModule& combined, const base::usize depth,
    std::vector<syntax::ResolvedImport>& direct_imports, const PackageLoadContext& package_context)
{
    direct_imports.reserve(direct_imports.size() + module.imports.size());
    for (const syntax::ImportDecl& import : module.imports) {
        const ImportFileResolution resolution =
            resolve_import_file(import.path, canonical.parent_path(), package_context.source_root, this->import_paths_);
        if (resolution.matching_candidates.empty()) {
            return report_import_resolution_failure(
                this->diagnostics_, import.path, format_import_candidates(resolution.searched_candidates));
        }
        if (resolution.matching_candidates.size() > 1) {
            return report_import_ambiguity(
                this->diagnostics_, import.path, format_import_candidates(resolution.matching_candidates));
        }

        const PackageLoadContext import_context =
            this->package_context_for_import_resolution(package_context, resolution.selected_import_root);
        auto import_result =
            this->load_file(*resolution.selected, combined, depth + 1, false, &import.path, import_context);
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

base::Result<std::vector<ModuleLoader::LoadedModulePartAst>> ModuleLoader::load_declared_parts(
    const std::filesystem::path& primary_path, const std::string& module_name, const syntax::ModulePath& module_path,
    const std::span<const syntax::ModulePartDecl> part_declarations, syntax::AstModule& combined,
    const base::usize depth, const syntax::ModuleId module_id, const PackageLoadContext& package_context)
{
    std::vector<LoadedModulePartAst> part_modules;
    part_modules.reserve(part_declarations.size());
    std::unordered_map<std::string, const syntax::ModulePartDecl*> parts_by_name;
    std::unordered_map<std::string, const syntax::ModulePartDecl*> parts_by_folded_name;
    parts_by_name.reserve(part_declarations.size());
    parts_by_folded_name.reserve(part_declarations.size());

    for (const syntax::ModulePartDecl& part_decl : part_declarations) {
        const std::string part_name{part_decl.name};
        const auto name_inserted = parts_by_name.emplace(part_name, &part_decl);
        if (!name_inserted.second) {
            return base::Result<std::vector<LoadedModulePartAst>>::fail(
                report_duplicate_module_part(this->diagnostics_, module_path, part_decl).error());
        }

        const std::string folded_name = module_loader_case_fold(part_decl.name);
        const auto folded_inserted = parts_by_folded_name.emplace(folded_name, &part_decl);
        if (!folded_inserted.second && folded_inserted.first->second->name != part_decl.name) {
            return base::Result<std::vector<LoadedModulePartAst>>::fail(report_module_part_case_collision(
                this->diagnostics_, module_path, *folded_inserted.first->second, part_decl)
                    .error());
        }
    }

    base::u32 stable_part_index = 1;
    for (const syntax::ModulePartDecl& part_decl : part_declarations) {
        const std::string part_name{part_decl.name};
        const std::filesystem::path part_path = module_part_file_path(primary_path, part_decl.name);
        if (!module_loader_path_exists(part_path)) {
            return base::Result<std::vector<LoadedModulePartAst>>::fail(
                report_missing_module_part(this->diagnostics_, module_path, part_decl, part_path).error());
        }

        auto part_result =
            this->load_module_part(part_path, part_decl, module_path, combined, depth, module_id, package_context);
        if (!part_result) {
            return base::Result<std::vector<LoadedModulePartAst>>::fail(part_result.error());
        }
        LoadedModulePartAst part_module = part_result.take_value();
        const std::filesystem::path canonical_part_path = module_loader_canonical_or_absolute(part_path);
        if (auto logical = this->loaded_modules_.find(logical_module_key(package_context.package, module_name));
            logical != this->loaded_modules_.end()) {
            logical->second.parts.push_back(LoadedModulePart{part_name, canonical_part_path});
        }
        this->record_module_part(module_id, part_name, canonical_part_path, stable_part_index);
        this->record_module_imports(module_id, part_name, false, part_module.imports, combined);
        ++stable_part_index;
        part_modules.push_back(std::move(part_module));
    }

    return base::Result<std::vector<LoadedModulePartAst>>::ok(std::move(part_modules));
}

base::Result<ModuleLoader::LoadedModulePartAst> ModuleLoader::load_module_part(const std::filesystem::path& part_path,
    const syntax::ModulePartDecl& part_decl, const syntax::ModulePath& expected_module, syntax::AstModule& combined,
    const base::usize depth, const syntax::ModuleId module_id, const PackageLoadContext& package_context)
{
    if (depth > base::config::AUREX_MAX_INCLUDE_DEPTH) {
        return base::Result<LoadedModulePartAst>::fail(module_loader_depth_exceeded_error());
    }

    const std::filesystem::path canonical = module_loader_canonical_or_absolute(part_path);
    const std::string key = canonical.string();
    if (this->loading_files_.contains(key)) {
        return base::Result<LoadedModulePartAst>::fail(
            report_cyclic_import(this->diagnostics_, &expected_module, key).error());
    }
    LoadingFileScope loading_file_scope{this->loading_files_, key};

    auto loaded_source = load_module_source(canonical, this->sources_, this->diagnostics_, this->profiler_, key);
    if (!loaded_source) {
        return base::Result<LoadedModulePartAst>::fail(loaded_source.error());
    }

    LoadedModuleSource loaded = loaded_source.take_value();
    syntax::AstModule part_module = std::move(loaded.module);
    if (part_module.file_kind != syntax::ModuleFileKind::part
        || !syntax::module_paths_equal(part_module.module_path, expected_module)
        || !module_part_name_matches(part_module.part_header, part_decl)) {
        return base::Result<LoadedModulePartAst>::fail(
            report_module_part_header_mismatch(this->diagnostics_, expected_module, part_decl.name, part_module)
                .error());
    }

    std::vector<syntax::ResolvedImport> part_imports;
    part_imports.reserve(part_module.imports.size());
    const auto import_result =
        this->resolve_imports_for_file(part_module, canonical, combined, depth, part_imports, package_context);
    if (!import_result) {
        return base::Result<LoadedModulePartAst>::fail(import_result.error());
    }

    this->loaded_part_files_.emplace(key,
        LoadedPartFile{
            syntax::module_path_to_string(part_module.module_path),
            std::string(part_module.part_header.name),
        });
    this->loaded_file_modules_.emplace(key, module_id);
    return base::Result<LoadedModulePartAst>::ok(LoadedModulePartAst{
        std::string(part_decl.name),
        canonical,
        std::move(part_module),
        std::move(part_imports),
    });
}

base::Result<syntax::ModuleId> ModuleLoader::redirect_root_part(const std::filesystem::path& canonical,
    const std::string& key, const syntax::AstModule& module, syntax::AstModule& combined, const base::usize depth,
    const PackageLoadContext& package_context)
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

    auto owner_result = this->load_file(canonical_primary, combined, depth, true, nullptr, package_context);
    if (!owner_result) {
        return owner_result;
    }
    if (!this->loaded_part_files_.contains(key)) {
        return base::Result<syntax::ModuleId>::fail(
            report_module_part_unlisted_root(this->diagnostics_, module, canonical_primary).error());
    }
    return owner_result;
}

ModuleLoader::PackageLoadContext ModuleLoader::package_context_for_import_resolution(
    const PackageLoadContext& importing_context, const std::optional<std::filesystem::path>& selected_import_root) const
{
    if (!selected_import_root.has_value()) {
        return importing_context;
    }
    const std::string import_root = module_loader_canonical_or_absolute(*selected_import_root).string();
    return PackageLoadContext{
        package_key_for_import_root(import_root),
        package_source_root_for_import_root(import_root),
    };
}

} // namespace aurex::driver
