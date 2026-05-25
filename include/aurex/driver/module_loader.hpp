#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/result.hpp>
#include <aurex/base/source.hpp>
#include <aurex/driver/invocation.hpp>
#include <aurex/driver/module_record.hpp>
#include <aurex/syntax/ast.hpp>

#include <filesystem>
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
        syntax::ModuleId id = syntax::INVALID_MODULE_ID;
        std::vector<LoadedModulePart> parts;
    };

    struct LoadedPartFile {
        std::string module_name;
        std::string part_name;
    };

    [[nodiscard]] base::Result<syntax::ModuleId> load_file(const std::filesystem::path& path,
        syntax::AstModule& combined, base::usize depth, bool is_root, const syntax::ModulePath* expected_module);
    [[nodiscard]] base::Result<void> resolve_imports_for_file(const syntax::AstModule& module,
        const std::filesystem::path& canonical, syntax::AstModule& combined, base::usize depth,
        std::vector<syntax::ResolvedImport>& direct_imports);
    [[nodiscard]] base::Result<std::vector<syntax::AstModule>> load_declared_parts(
        const std::filesystem::path& primary_path, const std::string& module_name,
        const syntax::ModulePath& module_path, std::span<const syntax::ModulePartDecl> part_declarations,
        syntax::AstModule& combined, base::usize depth, syntax::ModuleId module_id,
        std::vector<syntax::ResolvedImport>& direct_imports);
    [[nodiscard]] base::Result<syntax::AstModule> load_module_part(const std::filesystem::path& part_path,
        const syntax::ModulePartDecl& part_decl, const syntax::ModulePath& expected_module, syntax::AstModule& combined,
        base::usize depth, syntax::ModuleId module_id, std::vector<syntax::ResolvedImport>& direct_imports);
    [[nodiscard]] base::Result<syntax::ModuleId> redirect_root_part(const std::filesystem::path& canonical,
        const std::string& key, const syntax::AstModule& module, syntax::AstModule& combined, base::usize depth);

    const CompilerInvocation& invocation_;
    base::SourceManager& sources_;
    base::DiagnosticSink& diagnostics_;
    CompilationProfiler* profiler_ = nullptr;
    std::vector<std::filesystem::path> import_paths_;
    std::unordered_set<std::string> loading_files_;
    std::unordered_map<std::string, LoadedPartFile> loaded_part_files_;
    std::unordered_map<std::string, syntax::ModuleId> loaded_file_modules_;
    std::unordered_map<std::string, LoadedLogicalModule> loaded_modules_;
    std::vector<ModuleRecord> modules_;
};

} // namespace aurex::driver
