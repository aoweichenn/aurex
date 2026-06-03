#include <aurex/frontend/parse/parser_messages.hpp>
#include <aurex/frontend/parse/parser_name_expr_part.hpp>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ExprId NameExprParser::parse_name_or_struct_literal(const ExprContext context)
{
    static_cast<void>(context);
    return this->make_name_expr(this->expect(TokenKind::identifier, std::string(PARSER_EXPECT_EXPRESSION_NAME)));
}

syntax::ExprId NameExprParser::make_name_expr(const syntax::Token& name) const
{
    return this->session_.module.push_name_expr(name.range, name.text());
}

} // namespace aurex::parse
