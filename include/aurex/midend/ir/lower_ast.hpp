#pragma once

#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/infrastructure/base/result.hpp>
#include <aurex/midend/ir/ir.hpp>

namespace aurex::ir {

[[nodiscard]] base::Result<Module> lower_ast(const syntax::AstModule& ast, const sema::CheckedModule& checked);

} // namespace aurex::ir
