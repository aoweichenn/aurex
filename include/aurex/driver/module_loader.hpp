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
    struct LoadedModule {
        std::filesystem::path path;
        syntax::ModuleId id = syntax::INVALID_MODULE_ID;
    };

    [[nodiscard]] base::Result<syntax::ModuleId> load_file(const std::filesystem::path& path,
        syntax::AstModule& combined, base::usize depth, bool is_root, const syntax::ModulePath* expected_module);

    const CompilerInvocation& invocation_;
    base::SourceManager& sources_;
    base::DiagnosticSink& diagnostics_;
    CompilationProfiler* profiler_ = nullptr;
    std::vector<std::filesystem::path> import_paths_;
    std::unordered_set<std::string> loading_files_;
    std::unordered_map<std::string, syntax::ModuleId> loaded_file_modules_;
    std::unordered_map<std::string, LoadedModule> loaded_modules_;
    std::vector<ModuleRecord> modules_;
};

} // namespace aurex::driver
