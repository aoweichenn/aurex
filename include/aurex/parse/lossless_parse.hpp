#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/result.hpp>
#include <aurex/syntax/ast.hpp>
#include <aurex/syntax/lossless.hpp>
#include <aurex/syntax/token.hpp>

#include <vector>

namespace aurex::parse {

[[nodiscard]] std::vector<syntax::Token> semantic_tokens_from_lossless_tree(const syntax::LosslessSyntaxTree& tree);
[[nodiscard]] base::Result<syntax::AstModule> lower_lossless_syntax_to_ast(
    const syntax::LosslessSyntaxTree& tree, base::DiagnosticSink& diagnostics);

} // namespace aurex::parse
