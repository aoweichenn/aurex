#pragma once

#include "aurex/syntax/ast.hpp"

namespace aurex::codegen_c {

[[nodiscard]] bool expr_contains_call(const syntax::AstModule& module, syntax::ExprId expr) noexcept;

} // namespace aurex::codegen_c
