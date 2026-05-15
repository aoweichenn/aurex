#include <aurex/parse/parser_name_expr_part.hpp>

#include <aurex/parse/parser_messages.hpp>

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ExprId NameExprParser::parse_name_or_struct_literal(const ExprContext context) {
    static_cast<void>(context);
    return this->make_name_expr(this->expect(TokenKind::identifier, std::string(PARSER_EXPECT_EXPRESSION_NAME)));
}

syntax::ExprId NameExprParser::make_name_expr(const syntax::Token& name) {
    syntax::NameExprPayload payload;
    payload.text = name.text;
    return this->session_.module.push_name_expr(name.range, std::move(payload));
}

} // namespace aurex::parse
