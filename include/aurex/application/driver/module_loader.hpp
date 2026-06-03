#pragma once

#include <aurex/application/driver/invocation.hpp>
#include <aurex/application/driver/module_record.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/base/result.hpp>
#include <aurex/infrastructure/base/source.hpp>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aurex::driver {

class CompilationProfiler;

class ModuleLoader final {
public:
    ModuleLoader(const CompilerInvocation& invocation, base::SourceManager& sources, base::DiagnosticSink& diagnostics,
        CompilationProfiler* profiler = nullptr) noexcept;

    [[nodiscard]] base::Result<syntax::AstModule> load_root();
    [[nodiscard]] std::span<const ModuleRecord> modules() const noexcept;

private:
    struct LoadedModulePart {
        std::string name;
        std::filesystem::path path;
    };

    struct LoadedLogicalModule {
        std::filesystem::path primary_path;
        query::PackageKey package;
        syntax::ModuleId id = syntax::INVALID_MODULE_ID;
        std::vector<LoadedModulePart> parts;
    };

    struct LoadedPartFile {
        std::string module_name;
        std::string part_name;
    };

    struct LoadedModulePartAst {
        std::string name;
        std::filesystem::path path;
        base::u32 stable_index = 0;
        syntax::AstModule module;
        std::vector<syntax::ResolvedImport> imports;
        std::vector<syntax::ResolvedUse> reexports;
    };

    struct PackageLoadContext {
        query::PackageKey package;
        std::optional<std::filesystem::path> source_root;
    };

    [[nodiscard]] base::Result<syntax::ModuleId> load_file(const std::filesystem::path& path,
        syntax::AstModule& combined, base::usize depth, bool is_root, const syntax::ModulePath* expected_module,
        const PackageLoadContext& package_context);
    [[nodiscard]] base::Result<void> resolve_imports_for_file(const syntax::AstModule& module,
        const std::filesystem::path& canonical, syntax::AstModule& combined, base::usize depth,
        std::vector<syntax::ResolvedImport>& direct_imports, const PackageLoadContext& package_context);
    [[nodiscard]] base::Result<void> resolve_reexports_for_file(const syntax::AstModule& module,
        const std::filesystem::path& canonical, syntax::AstModule& combined, base::usize depth,
        std::vector<syntax::ResolvedUse>& direct_reexports, const PackageLoadContext& package_context);
    [[nodiscard]] base::Result<std::vector<LoadedModulePartAst>> load_declared_parts(
        const std::filesystem::path& primary_path, const std::string& module_name,
        const syntax::ModulePath& module_path, std::span<const syntax::ModulePartDecl> part_declarations,
        syntax::AstModule& combined, base::usize depth, syntax::ModuleId module_id,
        const PackageLoadContext& package_context);
    [[nodiscard]] base::Result<LoadedModulePartAst> load_module_part(const std::filesystem::path& part_path,
        const syntax::ModulePartDecl& part_decl, const syntax::ModulePath& expected_module, syntax::AstModule& combined,
        base::usize depth, syntax::ModuleId module_id, const PackageLoadContext& package_context);
    [[nodiscard]] base::Result<syntax::ModuleId> redirect_root_part(const std::filesystem::path& canonical,
        const std::string& key, const syntax::AstModule& module, syntax::AstModule& combined, base::usize depth,
        const PackageLoadContext& package_context);
    [[nodiscard]] PackageLoadContext package_context_for_import_resolution(const PackageLoadContext& importing_context,
        const std::optional<std::filesystem::path>& selected_import_root) const;
    void record_module_imports(syntax::ModuleId module_id, std::string_view owner_part, bool owner_is_primary,
        std::span<const syntax::ResolvedImport> imports, const syntax::AstModule& combined);
    void record_module_reexports(syntax::ModuleId module_id, std::string_view owner_part, bool owner_is_primary,
        std::span<const syntax::ResolvedUse> reexports, const syntax::AstModule& combined);
    void record_module_part(syntax::ModuleId module_id, std::string name, std::filesystem::path path,
        base::u32 stable_index, query::ModulePartKey key);

    const CompilerInvocation& invocation_;
    base::SourceManager& sources_;
    base::DiagnosticSink& diagnostics_;
    CompilationProfiler* profiler_ = nullptr;
    std::vector<std::filesystem::path> import_paths_;
    PackageLoadContext root_context_;
    std::unordered_set<std::string> loading_files_;
    std::unordered_map<std::string, LoadedPartFile> loaded_part_files_;
    std::unordered_map<std::string, syntax::ModuleId> loaded_file_modules_;
    std::unordered_map<std::string, LoadedLogicalModule> loaded_modules_;
    std::vector<ModuleRecord> modules_;
};

} // namespace aurex::driver
