#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ExprId PostfixExprParser::parse_postfix(const ExprContext context) {
    syntax::ExprId expr = PrimaryExprParser(parser_).parse_primary(context);
    while (true) {
        if (next_angle_list_is_type_scope() && match(TokenKind::less)) {
            expr = parse_type_args_suffix(expr);
        } else if (match(TokenKind::dot)) {
            expr = parse_field_suffix(expr);
        } else if (match(TokenKind::l_bracket)) {
            expr = parse_index_suffix(expr, context);
        } else if (match(TokenKind::l_paren)) {
            expr = parse_call_suffix(expr, context);
        } else if (match(TokenKind::question)) {
            expr = parse_try_suffix(expr);
        } else {
            break;
        }
        reset_panic();
    }
    return expr;
}

syntax::ExprId PostfixExprParser::parse_type_args_suffix(const syntax::ExprId expr) {
    if (!syntax::is_valid(expr) || expr.value >= session_.module.exprs.size()) {
        return expr;
    }
    syntax::ExprNode& node = session_.module.exprs[expr.value];
    if (node.kind != syntax::ExprKind::name && node.kind != syntax::ExprKind::field) {
        report_at(previous(), "type arguments are only supported on named function calls, methods, or enum constructors");
    }
    if (!check_type_arg_list_end()) {
        do {
            node.type_args.push_back(parse_type());
            reset_panic();
            if (check_type_arg_list_end()) {
                break;
            }
        } while (match(TokenKind::comma) && !check_type_arg_list_end());
    }
    const syntax::Token& end = expect_type_arg_list_end("expected '>' after type argument list");
    node.range = merge(node.range, end.range);
    return expr;
}

syntax::ExprId PostfixExprParser::parse_field_suffix(const syntax::ExprId expr) {
    const syntax::Token& field = expect(TokenKind::identifier, "expected field name after '.'");
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::field;
    node.range = merge(session_.module.exprs[expr.value].range, field.range);
    node.object = expr;
    node.field_name = field.text;
    return session_.module.push_expr(std::move(node));
}

syntax::ExprId PostfixExprParser::parse_index_suffix(const syntax::ExprId expr, const ExprContext context) {
    const syntax::ExprId index = parse_expr(context);
    const syntax::Token& end = expect(TokenKind::r_bracket, "expected ']' after index");
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::index;
    node.range = merge(session_.module.exprs[expr.value].range, end.range);
    node.object = expr;
    node.index = index;
    return session_.module.push_expr(std::move(node));
}

syntax::ExprId PostfixExprParser::parse_call_suffix(const syntax::ExprId expr, const ExprContext context) {
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::call;
    node.callee = expr;
    if (!check(TokenKind::r_paren)) {
        do {
            node.args.push_back(parse_expr(context));
            if (check(TokenKind::r_paren)) {
                break;
            }
        } while (match(TokenKind::comma) && !check(TokenKind::r_paren));
    }
    const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after argument list");
    node.range = merge(session_.module.exprs[expr.value].range, end.range);
    return session_.module.push_expr(std::move(node));
}

syntax::ExprId PostfixExprParser::parse_try_suffix(const syntax::ExprId expr) {
    const syntax::Token& question = previous();
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::try_expr;
    node.unary_operand = expr;
    node.range = syntax::is_valid(expr) && expr.value < session_.module.exprs.size()
        ? merge(session_.module.exprs[expr.value].range, question.range)
        : question.range;
    return session_.module.push_expr(std::move(node));
}

} // namespace aurex::parse
