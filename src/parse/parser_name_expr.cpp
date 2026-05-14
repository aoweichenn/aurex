#include <aurex/parse/parser_name_expr_part.hpp>

#include <aurex/parse/parser_messages.hpp>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ExprId NameExprParser::parse_name_or_struct_literal(const ExprContext context) {
    static_cast<void>(context);
    return this->make_name_expr(this->expect(TokenKind::identifier, std::string(PARSER_EXPECT_EXPRESSION_NAME)));
}

syntax::ExprId NameExprParser::make_name_expr(const syntax::Token& name) {
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::name;
    expr.range = name.range;
    expr.text = name.text;
    return this->session_.module.push_expr(expr);
}

} // namespace aurex::parse
