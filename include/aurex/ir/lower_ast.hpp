#pragma once

#include <aurex/base/result.hpp>
#include <aurex/ir/ir.hpp>
#include <aurex/sema/checked_module.hpp>
#include <aurex/syntax/ast.hpp>

namespace aurex::ir {

[[nodiscard]] base::Result<Module> lower_ast(const syntax::AstModule& ast, const sema::CheckedModule& checked);

} // namespace aurex::ir
