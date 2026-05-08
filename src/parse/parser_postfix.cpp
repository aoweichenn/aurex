#include "aurex/parse/parser_parts.hpp"

#include <optional>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ExprId PostfixExprParser::parse_postfix(const ExprContext context) {
    syntax::ExprId expr = PrimaryExprParser(this->parser_).parse_primary(context);
    while (std::optional<syntax::ExprId> next = this->parse_next_suffix(expr, context)) {
        expr = next.value();
        this->reset_panic();
    }
    return expr;
}

std::optional<syntax::ExprId> PostfixExprParser::parse_next_suffix(
    const syntax::ExprId expr,
    const ExprContext context
) {
    if (this->next_angle_list_is_type_scope() && this->match(TokenKind::less)) {
        return this->parse_type_args_suffix(expr);
    }
    if (this->match(TokenKind::dot)) {
        return this->parse_field_suffix(expr);
    }
    if (this->match(TokenKind::l_bracket)) {
        return this->parse_index_suffix(expr, context);
    }
    if (this->match(TokenKind::l_paren)) {
        return this->parse_call_suffix(expr, context);
    }
    if (this->match(TokenKind::question)) {
        return this->parse_try_suffix(expr);
    }
    return std::nullopt;
}

syntax::ExprId PostfixExprParser::parse_type_args_suffix(const syntax::ExprId expr) {
    if (!syntax::is_valid(expr) || expr.value >= this->session_.module.exprs.size()) {
        return expr;
    }
    syntax::ExprNode& node = this->session_.module.exprs[expr.value];
    if (node.kind != syntax::ExprKind::name && node.kind != syntax::ExprKind::field) {
        this->report_at(this->previous(), "type arguments are only supported on named function calls, methods, or enum constructors");
    }
    if (!this->check_type_arg_list_end()) {
        do {
            node.type_args.push_back(this->parse_type());
            this->reset_panic();
            if (this->check_type_arg_list_end()) {
                break;
            }
        } while (this->match(TokenKind::comma) && !this->check_type_arg_list_end());
    }
    const syntax::Token& end = this->expect_type_arg_list_end("expected '>' after type argument list");
    node.range = this->merge(node.range, end.range);
    return expr;
}

syntax::ExprId PostfixExprParser::parse_field_suffix(const syntax::ExprId expr) {
    const syntax::Token& field = this->expect(TokenKind::identifier, "expected field name after '.'");
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::field;
    node.range = this->merge(this->expr_range_or(expr, field.range), field.range);
    node.object = expr;
    node.field_name = field.text;
    return this->session_.module.push_expr(std::move(node));
}

syntax::ExprId PostfixExprParser::parse_index_suffix(const syntax::ExprId expr, const ExprContext context) {
    const syntax::ExprId index = this->parse_expr(context);
    const syntax::Token& end = this->expect(TokenKind::r_bracket, "expected ']' after index");
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::index;
    node.range = this->merge(this->expr_range_or(expr, end.range), end.range);
    node.object = expr;
    node.index = index;
    return this->session_.module.push_expr(std::move(node));
}

syntax::ExprId PostfixExprParser::parse_call_suffix(const syntax::ExprId expr, const ExprContext context) {
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::call;
    node.callee = expr;
    if (!this->check(TokenKind::r_paren)) {
        do {
            node.args.push_back(this->parse_expr(context));
            if (this->check(TokenKind::r_paren)) {
                break;
            }
        } while (this->match(TokenKind::comma) && !this->check(TokenKind::r_paren));
    }
    const syntax::Token& end = this->expect(TokenKind::r_paren, "expected ')' after argument list");
    node.range = this->merge(this->expr_range_or(expr, end.range), end.range);
    return this->session_.module.push_expr(std::move(node));
}

syntax::ExprId PostfixExprParser::parse_try_suffix(const syntax::ExprId expr) {
    const syntax::Token& question = this->previous();
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::try_expr;
    node.unary_operand = expr;
    node.range = this->merge(this->expr_range_or(expr, question.range), question.range);
    return this->session_.module.push_expr(std::move(node));
}

} // namespace aurex::parse
