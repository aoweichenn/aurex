#include "aurex/parse/parser_parts.hpp"

#include <string_view>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

[[nodiscard]] syntax::BinaryOp binary_op_from_token(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::plus: return syntax::BinaryOp::add;
    case TokenKind::minus: return syntax::BinaryOp::sub;
    case TokenKind::star: return syntax::BinaryOp::mul;
    case TokenKind::slash: return syntax::BinaryOp::div;
    case TokenKind::percent: return syntax::BinaryOp::mod;
    case TokenKind::less_less: return syntax::BinaryOp::shl;
    case TokenKind::greater_greater: return syntax::BinaryOp::shr;
    case TokenKind::less: return syntax::BinaryOp::less;
    case TokenKind::less_equal: return syntax::BinaryOp::less_equal;
    case TokenKind::greater: return syntax::BinaryOp::greater;
    case TokenKind::greater_equal: return syntax::BinaryOp::greater_equal;
    case TokenKind::equal_equal: return syntax::BinaryOp::equal;
    case TokenKind::bang_equal: return syntax::BinaryOp::not_equal;
    case TokenKind::amp: return syntax::BinaryOp::bit_and;
    case TokenKind::caret: return syntax::BinaryOp::bit_xor;
    case TokenKind::pipe: return syntax::BinaryOp::bit_or;
    case TokenKind::amp_amp: return syntax::BinaryOp::logical_and;
    case TokenKind::pipe_pipe: return syntax::BinaryOp::logical_or;
    default: return syntax::BinaryOp::add;
    }
}

} // namespace

syntax::ExprId ExprParser::parse_expr(const ExprContext context) {
    if (check(TokenKind::kw_if)) {
        return parse_if_expr(context);
    }
    if (check(TokenKind::kw_match)) {
        return parse_match_expr(context);
    }
    return parse_logical_or(context);
}

syntax::ExprId ExprParser::parse_if_expr(const ExprContext context) {
    const syntax::Token& begin = expect(TokenKind::kw_if, "expected 'if'");
    const syntax::ExprId condition = parse_expr(ExprContext::no_struct_literal);

    const syntax::ExprId then_expr = parse_block_expr(context);
    expect(TokenKind::kw_else, "if expression requires else branch");
    const syntax::ExprId else_expr = check(TokenKind::kw_if) ? parse_if_expr(context) : parse_block_expr(context);

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::if_expr;
    expr.range = merge(begin.range, session_.module.exprs[else_expr.value].range);
    expr.condition = condition;
    expr.then_expr = then_expr;
    expr.else_expr = else_expr;
    return session_.module.push_expr(std::move(expr));
}

syntax::ExprId ExprParser::parse_match_expr(const ExprContext context) {
    const syntax::Token& begin = expect(TokenKind::kw_match, "expected 'match'");
    const syntax::ExprId value = parse_expr(ExprContext::no_struct_literal);
    expect(TokenKind::l_brace, "expected '{' after match value");

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::match_expr;
    expr.match_value = value;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        const syntax::PatternId pattern = parse_pattern();
        syntax::ExprId guard = syntax::invalid_expr_id;
        if (match(TokenKind::kw_if)) {
            guard = parse_expr(context);
        }
        expect(TokenKind::fat_arrow, "expected '=>' after match case");
        const syntax::ExprId arm_value = parse_expr(context);
        base::SourceRange pattern_range = {};
        if (syntax::is_valid(pattern) && pattern.value < session_.module.patterns.size()) {
            pattern_range = session_.module.patterns[pattern.value].range;
        }
        base::SourceRange arm_range = syntax::is_valid(arm_value)
            ? merge(pattern_range, session_.module.exprs[arm_value.value].range)
            : pattern_range;
        expr.match_arms.push_back(syntax::MatchArm {
            pattern,
            guard,
            arm_value,
            arm_range,
        });
        if (check(TokenKind::r_brace)) {
            break;
        }
        expect(TokenKind::comma, "expected ',' after match arm");
        reset_panic();
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after match expression");
    expr.range = merge(begin.range, end.range);
    reset_panic();
    return session_.module.push_expr(std::move(expr));
}

syntax::ExprId ExprParser::parse_logical_or(const ExprContext context) {
    syntax::ExprId expr = parse_logical_and(context);
    while (match(TokenKind::pipe_pipe)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_logical_and(context);
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(session_.module.exprs[expr.value].range, session_.module.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId ExprParser::parse_logical_and(const ExprContext context) {
    syntax::ExprId expr = parse_bit_or(context);
    while (match(TokenKind::amp_amp)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_bit_or(context);
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(session_.module.exprs[expr.value].range, session_.module.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId ExprParser::parse_bit_or(const ExprContext context) {
    syntax::ExprId expr = parse_bit_xor(context);
    while (match(TokenKind::pipe)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_bit_xor(context);
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(session_.module.exprs[expr.value].range, session_.module.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId ExprParser::parse_bit_xor(const ExprContext context) {
    syntax::ExprId expr = parse_bit_and(context);
    while (match(TokenKind::caret)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_bit_and(context);
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(session_.module.exprs[expr.value].range, session_.module.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId ExprParser::parse_bit_and(const ExprContext context) {
    syntax::ExprId expr = parse_equality(context);
    while (match(TokenKind::amp)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_equality(context);
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(session_.module.exprs[expr.value].range, session_.module.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId ExprParser::parse_equality(const ExprContext context) {
    syntax::ExprId expr = parse_compare(context);
    while (match(TokenKind::equal_equal) || match(TokenKind::bang_equal)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_compare(context);
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(session_.module.exprs[expr.value].range, session_.module.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId ExprParser::parse_compare(const ExprContext context) {
    syntax::ExprId expr = parse_shift(context);
    while (match(TokenKind::less) || match(TokenKind::less_equal) || match(TokenKind::greater) || match(TokenKind::greater_equal)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_shift(context);
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(session_.module.exprs[expr.value].range, session_.module.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId ExprParser::parse_shift(const ExprContext context) {
    syntax::ExprId expr = parse_add(context);
    while (match(TokenKind::less_less) || match(TokenKind::greater_greater)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_add(context);
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(session_.module.exprs[expr.value].range, session_.module.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId ExprParser::parse_add(const ExprContext context) {
    syntax::ExprId expr = parse_mul(context);
    while (match(TokenKind::plus) || match(TokenKind::minus)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_mul(context);
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(session_.module.exprs[expr.value].range, session_.module.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId ExprParser::parse_mul(const ExprContext context) {
    syntax::ExprId expr = parse_unary(context);
    while (match(TokenKind::star) || match(TokenKind::slash) || match(TokenKind::percent)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_unary(context);
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(session_.module.exprs[expr.value].range, session_.module.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId ExprParser::parse_unary(const ExprContext context) {
    if (match(TokenKind::bang) || match(TokenKind::minus) || match(TokenKind::tilde) || match(TokenKind::amp) || match(TokenKind::star)) {
        const syntax::Token& op = previous();
        const syntax::ExprId operand = parse_unary(context);
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::unary;
        expr.range = merge(op.range, session_.module.exprs[operand.value].range);
        expr.text = op.text;
        switch (op.kind) {
        case TokenKind::bang:
            expr.unary_op = syntax::UnaryOp::logical_not;
            break;
        case TokenKind::minus:
            expr.unary_op = syntax::UnaryOp::numeric_negate;
            break;
        case TokenKind::tilde:
            expr.unary_op = syntax::UnaryOp::bitwise_not;
            break;
        case TokenKind::amp:
            expr.unary_op = syntax::UnaryOp::address_of;
            break;
        case TokenKind::star:
            expr.unary_op = syntax::UnaryOp::dereference;
            break;
        default:
            break;
        }
        expr.unary_operand = operand;
        return session_.module.push_expr(std::move(expr));
    }
    return PrimaryExprParser(parser_).parse_postfix(context);
}

syntax::ExprId ExprParser::make_binary(const syntax::BinaryOp op, const syntax::ExprId lhs, const syntax::ExprId rhs, base::SourceRange range) {
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::binary;
    expr.range = range;
    expr.binary_op = op;
    expr.binary_lhs = lhs;
    expr.binary_rhs = rhs;
    return session_.module.push_expr(std::move(expr));
}

} // namespace aurex::parse
