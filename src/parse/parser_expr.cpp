#include "aurex/parse/parser_expr_part.hpp"

#include "aurex/parse/parser_postfix_expr_part.hpp"
#include "aurex/parse/recovery.hpp"

#include <string_view>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

enum class BinaryPrecedence {
    logical_or,
    logical_and,
    bit_or,
    bit_xor,
    bit_and,
    equality,
    comparison,
    shift,
    additive,
    multiplicative,
    after_multiplicative,
};

constexpr BinaryPrecedence kLowestBinaryPrecedence = BinaryPrecedence::logical_or;
constexpr int kLeftAssociativeRhsPrecedenceStep = 1;

struct BinaryOperatorSyntax {
    TokenKind token;
    syntax::BinaryOp op;
    BinaryPrecedence precedence;
};

constexpr BinaryOperatorSyntax kBinaryOperators[] = {
    {TokenKind::pipe_pipe, syntax::BinaryOp::logical_or, BinaryPrecedence::logical_or},
    {TokenKind::amp_amp, syntax::BinaryOp::logical_and, BinaryPrecedence::logical_and},
    {TokenKind::pipe, syntax::BinaryOp::bit_or, BinaryPrecedence::bit_or},
    {TokenKind::caret, syntax::BinaryOp::bit_xor, BinaryPrecedence::bit_xor},
    {TokenKind::amp, syntax::BinaryOp::bit_and, BinaryPrecedence::bit_and},
    {TokenKind::equal_equal, syntax::BinaryOp::equal, BinaryPrecedence::equality},
    {TokenKind::bang_equal, syntax::BinaryOp::not_equal, BinaryPrecedence::equality},
    {TokenKind::less, syntax::BinaryOp::less, BinaryPrecedence::comparison},
    {TokenKind::less_equal, syntax::BinaryOp::less_equal, BinaryPrecedence::comparison},
    {TokenKind::greater, syntax::BinaryOp::greater, BinaryPrecedence::comparison},
    {TokenKind::greater_equal, syntax::BinaryOp::greater_equal, BinaryPrecedence::comparison},
    {TokenKind::less_less, syntax::BinaryOp::shl, BinaryPrecedence::shift},
    {TokenKind::greater_greater, syntax::BinaryOp::shr, BinaryPrecedence::shift},
    {TokenKind::plus, syntax::BinaryOp::add, BinaryPrecedence::additive},
    {TokenKind::minus, syntax::BinaryOp::sub, BinaryPrecedence::additive},
    {TokenKind::star, syntax::BinaryOp::mul, BinaryPrecedence::multiplicative},
    {TokenKind::slash, syntax::BinaryOp::div, BinaryPrecedence::multiplicative},
    {TokenKind::percent, syntax::BinaryOp::mod, BinaryPrecedence::multiplicative},
};

[[nodiscard]] int precedence_rank(const BinaryPrecedence precedence) noexcept {
    return static_cast<int>(precedence);
}

[[nodiscard]] int next_stronger_precedence(const BinaryPrecedence precedence) noexcept {
    return precedence_rank(precedence) + kLeftAssociativeRhsPrecedenceStep;
}

[[nodiscard]] const BinaryOperatorSyntax* binary_operator_for(const TokenKind kind) noexcept {
    for (const BinaryOperatorSyntax& op : kBinaryOperators) {
        if (op.token == kind) {
            return &op;
        }
    }
    return nullptr;
}

} // namespace

syntax::ExprId ExprParser::parse_expr(const ExprContext context) {
    if (this->check(TokenKind::kw_if)) {
        return this->parse_if_expr(context);
    }
    if (this->check(TokenKind::kw_match)) {
        return this->parse_match_expr(context);
    }
    return this->parse_binary_expr(context, precedence_rank(kLowestBinaryPrecedence));
}

syntax::ExprId ExprParser::parse_if_expr(const ExprContext context) {
    const syntax::Token& begin = this->expect(TokenKind::kw_if, "expected 'if'");
    const syntax::ExprId condition = this->parse_expr(ExprContext::no_struct_literal);

    const syntax::ExprId then_expr = this->parse_block_expr(context);
    this->expect_recovered(
        TokenKind::kw_else,
        "if expression requires else branch",
        RecoveryContext::if_else
    );
    const syntax::ExprId else_expr = this->check(TokenKind::kw_if)
        ? this->parse_if_expr(context)
        : this->parse_block_expr(context);

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::if_expr;
    expr.range = this->merge(begin.range, this->expr_range_or(else_expr, begin.range));
    expr.condition = condition;
    expr.then_expr = then_expr;
    expr.else_expr = else_expr;
    return this->session_.module.push_expr(std::move(expr));
}

syntax::ExprId ExprParser::parse_match_expr(const ExprContext context) {
    const syntax::Token& begin = this->expect(TokenKind::kw_match, "expected 'match'");
    const syntax::ExprId value = this->parse_expr(ExprContext::no_struct_literal);
    this->expect_recovered(
        TokenKind::l_brace,
        "expected '{' after match value",
        RecoveryContext::block_start
    );

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::match_expr;
    expr.match_value = value;

    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        expr.match_arms.push_back(this->parse_match_arm(context, begin.range));
        if (!this->recover_match_arm_separator()) {
            break;
        }
    }

    const syntax::Token& end = this->expect_recovered(
        TokenKind::r_brace,
        "expected '}' after match expression",
        RecoveryContext::match_arm
    );
    expr.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_expr(std::move(expr));
}

syntax::MatchArm ExprParser::parse_match_arm(
    const ExprContext context,
    const base::SourceRange fallback_range
) {
    const syntax::PatternId pattern = this->parse_pattern();
    syntax::ExprId guard = syntax::invalid_expr_id;
    if (this->match(TokenKind::kw_if)) {
        guard = this->parse_expr(context);
    }
    this->expect_recovered(
        TokenKind::fat_arrow,
        "expected '=>' after match case",
        RecoveryContext::match_arm_arrow
    );
    const syntax::ExprId arm_value = this->parse_expr(context);
    const base::SourceRange pattern_range = this->pattern_range_or(pattern, fallback_range);
    const base::SourceRange arm_range = syntax::is_valid(arm_value)
        ? this->merge(pattern_range, this->expr_range_or(arm_value, pattern_range))
        : pattern_range;
    return syntax::MatchArm {
        pattern,
        guard,
        arm_value,
        arm_range,
    };
}

bool ExprParser::recover_match_arm_separator() {
    if (this->check(TokenKind::r_brace)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_brace);
    }

    this->report_here("expected ',' or '}' after match arm");
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::match_arm)) {
        this->synchronize(RecoveryContext::match_arm);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_brace);
    }
    this->reset_panic();
    return token_starts_match_arm(this->peek().kind);
}

syntax::ExprId ExprParser::parse_binary_expr(const ExprContext context, const int min_precedence) {
    syntax::ExprId expr = this->parse_unary(context);
    while (const BinaryOperatorSyntax* op = binary_operator_for(this->peek().kind)) {
        if (precedence_rank(op->precedence) < min_precedence) {
            break;
        }
        this->advance();
        const syntax::ExprId rhs = this->parse_binary_expr(context, next_stronger_precedence(op->precedence));
        expr = this->make_binary(op->op, expr, rhs);
    }
    return expr;
}

syntax::ExprId ExprParser::parse_unary(const ExprContext context) {
    if (this->check(TokenKind::plus_plus)) {
        return this->parse_rejected_update_operator_expr(
            TokenKind::plus_plus,
            "increment operator is not supported; use '+= 1'",
            context
        );
    }
    if (this->check(TokenKind::minus_minus)) {
        return this->parse_rejected_update_operator_expr(
            TokenKind::minus_minus,
            "decrement operator is not supported; use '-= 1'",
            context
        );
    }

    if (this->match(TokenKind::bang) ||
        this->match(TokenKind::minus) ||
        this->match(TokenKind::tilde) ||
        this->match(TokenKind::amp) ||
        this->match(TokenKind::star)) {
        const syntax::Token& op = this->previous();
        const syntax::ExprId operand = this->parse_unary(context);
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::unary;
        expr.range = this->merge(op.range, this->expr_range_or(operand, op.range));
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
        return this->session_.module.push_expr(std::move(expr));
    }
    return PostfixExprParser(this->parser_).parse_postfix(context);
}

syntax::ExprId ExprParser::parse_rejected_update_operator_expr(
    const TokenKind kind,
    std::string message,
    const ExprContext context
) {
    const syntax::Token& op = this->expect(kind, "expected unsupported update operator");
    this->report_at(op, std::move(message));
    const syntax::ExprId operand = this->parse_unary(context);

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::invalid;
    expr.range = this->merge(op.range, this->expr_range_or(operand, op.range));
    return this->session_.module.push_expr(std::move(expr));
}

syntax::ExprId ExprParser::make_binary(const syntax::BinaryOp op, const syntax::ExprId lhs, const syntax::ExprId rhs) {
    const base::SourceRange lhs_range = this->expr_range_or(lhs, this->expr_range_or(rhs, this->peek().range));
    const base::SourceRange rhs_range = this->expr_range_or(rhs, lhs_range);
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::binary;
    expr.range = this->merge(lhs_range, rhs_range);
    expr.binary_op = op;
    expr.binary_lhs = lhs;
    expr.binary_rhs = rhs;
    return this->session_.module.push_expr(std::move(expr));
}

} // namespace aurex::parse
