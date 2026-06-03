#pragma once

#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/frontend/syntax/core/lossless.hpp>
#include <aurex/frontend/syntax/core/token.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/base/result.hpp>

#include <vector>

namespace aurex::parse {

[[nodiscard]] std::vector<syntax::Token> semantic_tokens_from_lossless_tree(const syntax::LosslessSyntaxTree& tree);
[[nodiscard]] base::Result<syntax::AstModule> lower_lossless_syntax_to_ast(
    const syntax::LosslessSyntaxTree& tree, base::DiagnosticSink& diagnostics);

} // namespace aurex::parse
