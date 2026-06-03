#pragma once

#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/frontend/syntax/core/token.hpp>

#include <span>
#include <string>

namespace aurex::syntax {

[[nodiscard]] std::string dump_tokens(std::span<const Token> tokens);
[[nodiscard]] std::string dump_ast(const AstModule& module);

} // namespace aurex::syntax
