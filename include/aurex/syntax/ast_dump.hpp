#pragma once

#include "aurex/syntax/ast.hpp"
#include "aurex/syntax/token.hpp"

#include <span>
#include <string>

namespace aurex::syntax {

[[nodiscard]] std::string dump_tokens(std::span<const Token> tokens);
[[nodiscard]] std::string dump_ast(const AstModule& module);

} // namespace aurex::syntax
