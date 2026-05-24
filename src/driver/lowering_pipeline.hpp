#pragma once

#include <aurex/base/result.hpp>
#include <aurex/ir/ir.hpp>
#include <aurex/sema/checked_module.hpp>
#include <aurex/syntax/ast.hpp>

#include "compilation_session.hpp"

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
