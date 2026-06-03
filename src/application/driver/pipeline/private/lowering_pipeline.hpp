#pragma once

#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/infrastructure/base/result.hpp>
#include <aurex/midend/ir/ir.hpp>

#include <application/driver/pipeline/private/compilation_session.hpp>

namespace aurex::driver {

class LoweringPipeline final {
public:
    explicit LoweringPipeline(CompilationSession& session) noexcept;

    [[nodiscard]] base::Result<void> dump_checked_output(const sema::CheckedModule& checked);
    [[nodiscard]] base::Result<ir::Module> lower_and_optimize(
        const syntax::AstModule& ast, const sema::CheckedModule& checked);
    [[nodiscard]] base::Result<void> dump_ir_output(const ir::Module& module);

private:
    CompilationSession& session_;
};

} // namespace aurex::driver
