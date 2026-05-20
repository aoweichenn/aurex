#include <aurex/parse/lossless_parse.hpp>
#include <aurex/parse/parser.hpp>

namespace aurex::parse {
namespace {

[[nodiscard]] syntax::Token make_synthetic_eof(const syntax::LosslessSyntaxTree& tree) noexcept
{
    const base::SourceRange range = tree.range();
    return syntax::Token{syntax::TokenKind::eof, base::SourceRange{range.source, range.end, range.end}, {}};
}

} // namespace

std::vector<syntax::Token> semantic_tokens_from_lossless_tree(const syntax::LosslessSyntaxTree& tree)
{
    std::vector<syntax::Token> tokens;
    tokens.reserve(tree.semantic_token_count() + 1U);
    for (const syntax::Token& token : tree.tokens()) {
        if (!syntax::is_trivia_token(token.kind)) {
            tokens.push_back(token);
        }
    }
    if (tokens.empty() || tokens.back().kind != syntax::TokenKind::eof) {
        tokens.push_back(make_synthetic_eof(tree));
    }
    return tokens;
}

base::Result<syntax::AstModule> lower_lossless_syntax_to_ast(
    const syntax::LosslessSyntaxTree& tree, base::DiagnosticSink& diagnostics)
{
    std::vector<syntax::Token> tokens = semantic_tokens_from_lossless_tree(tree);
    Parser parser(tokens, diagnostics);
    return parser.parse_module();
}

} // namespace aurex::parse
