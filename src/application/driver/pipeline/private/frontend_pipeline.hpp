#pragma once

#include <aurex/application/driver/module_record.hpp>
#include <aurex/frontend/macro/early_item_expansion.hpp>
#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/infrastructure/base/result.hpp>

#include <vector>

#include <application/driver/pipeline/private/compilation_session.hpp>

namespace aurex::ir {
struct Module;
} // namespace aurex::ir

namespace aurex::driver {

struct FrontendModuleOutput {
    syntax::AstModule ast;
    std::vector<ModuleRecord> modules;
    frontend::macro::EarlyItemExpansionResult early_item_expansion;
};

class FrontendPipeline final {
public:
    explicit FrontendPipeline(CompilationSession& session) noexcept;

    [[nodiscard]] base::Result<bool> try_reuse_check_cache();
    [[nodiscard]] base::Result<void> emit_token_or_lossless_output();
    [[nodiscard]] base::Result<FrontendModuleOutput> load_modules();
    [[nodiscard]] base::Result<void> dump_ast_output(const syntax::AstModule& ast);
    [[nodiscard]] base::Result<void> dump_module_graph_output(const std::vector<ModuleRecord>& modules);
    [[nodiscard]] base::Result<sema::CheckedModule> run_semantic_analysis(
        syntax::AstModule& ast, const std::vector<ModuleRecord>& modules);
    [[nodiscard]] base::Result<void> write_checked_incremental_cache(const std::vector<ModuleRecord>& modules,
        const syntax::AstModule& ast, const sema::CheckedModule& checked, const ir::Module* lowered_ir = nullptr);

private:
    CompilationSession& session_;
};

} // namespace aurex::driver
