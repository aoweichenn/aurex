#include <aurex/parse/parser_expr_part.hpp>

#include <aurex/parse/parser_messages.hpp>
#include <aurex/parse/parser_pattern_part.hpp>
#include <aurex/parse/parser_postfix_expr_part.hpp>
#include <aurex/parse/recovery.hpp>

#include <cassert>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

enum class BinaryPrecedence {
    LOGICAL_OR,
    LOGICAL_AND,
    BIT_OR,
    BIT_XOR,
    BIT_AND,
    EQUALITY,
    COMPARISON,
    SHIFT,
    ADDITIVE,
    MULTIPLICATIVE,
};

constexpr base::usize PARSER_EXPR_OPERATOR_STACK_INITIAL_CAPACITY = 16;
constexpr base::usize PARSER_EXPR_OPERAND_STACK_INITIAL_CAPACITY = 16;
constexpr base::usize PARSER_EXPR_PREFIX_STACK_INITIAL_CAPACITY = 8;

struct BinaryOperatorSyntax {
    TokenKind token;
    syntax::BinaryOp op;
    BinaryPrecedence precedence;
};

constexpr BinaryOperatorSyntax PARSER_EXPR_BINARY_OPERATORS[] = {
    {TokenKind::pipe_pipe, syntax::BinaryOp::logical_or, BinaryPrecedence::LOGICAL_OR},
    {TokenKind::amp_amp, syntax::BinaryOp::logical_and, BinaryPrecedence::LOGICAL_AND},
    {TokenKind::pipe, syntax::BinaryOp::bit_or, BinaryPrecedence::BIT_OR},
    {TokenKind::caret, syntax::BinaryOp::bit_xor, BinaryPrecedence::BIT_XOR},
    {TokenKind::amp, syntax::BinaryOp::bit_and, BinaryPrecedence::BIT_AND},
    {TokenKind::equal_equal, syntax::BinaryOp::equal, BinaryPrecedence::EQUALITY},
    {TokenKind::bang_equal, syntax::BinaryOp::not_equal, BinaryPrecedence::EQUALITY},
    {TokenKind::less, syntax::BinaryOp::less, BinaryPrecedence::COMPARISON},
    {TokenKind::less_equal, syntax::BinaryOp::less_equal, BinaryPrecedence::COMPARISON},
    {TokenKind::greater, syntax::BinaryOp::greater, BinaryPrecedence::COMPARISON},
    {TokenKind::greater_equal, syntax::BinaryOp::greater_equal, BinaryPrecedence::COMPARISON},
    {TokenKind::less_less, syntax::BinaryOp::shl, BinaryPrecedence::SHIFT},
    {TokenKind::greater_greater, syntax::BinaryOp::shr, BinaryPrecedence::SHIFT},
    {TokenKind::plus, syntax::BinaryOp::add, BinaryPrecedence::ADDITIVE},
    {TokenKind::minus, syntax::BinaryOp::sub, BinaryPrecedence::ADDITIVE},
    {TokenKind::star, syntax::BinaryOp::mul, BinaryPrecedence::MULTIPLICATIVE},
    {TokenKind::slash, syntax::BinaryOp::div, BinaryPrecedence::MULTIPLICATIVE},
    {TokenKind::percent, syntax::BinaryOp::mod, BinaryPrecedence::MULTIPLICATIVE},
};

[[nodiscard]] int precedence_rank(const BinaryPrecedence precedence) noexcept {
    return static_cast<int>(precedence);
}

[[nodiscard]] const BinaryOperatorSyntax* binary_operator_for(const TokenKind kind) noexcept {
    for (const BinaryOperatorSyntax& op : PARSER_EXPR_BINARY_OPERATORS) {
        if (op.token == kind) {
            return &op;
        }
    }
    return nullptr;
}

[[nodiscard]] std::optional<syntax::UnaryOp> unary_op_for(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::bang:
        return syntax::UnaryOp::logical_not;
    case TokenKind::minus:
        return syntax::UnaryOp::numeric_negate;
    case TokenKind::tilde:
        return syntax::UnaryOp::bitwise_not;
    case TokenKind::amp:
        return syntax::UnaryOp::address_of;
    case TokenKind::star:
        return syntax::UnaryOp::dereference;
    default:
        return std::nullopt;
    }
}

} // namespace

syntax::ExprId ExprParser::parse_expr(const ExprContext context) {
    if (this->check(TokenKind::kw_if)) {
        return this->parse_if_expr(context);
    }
    if (this->check(TokenKind::kw_match)) {
        return this->parse_match_expr(context);
    }
    return this->parse_binary_expr(context);
}

syntax::ExprId ExprParser::parse_if_expr(const ExprContext context) {
    const syntax::Token& begin = this->expect(TokenKind::kw_if, std::string(PARSER_EXPECT_IF));
    const syntax::ExprId condition = this->parse_expr(ExprContext::no_struct_literal);
    syntax::PatternId condition_pattern = syntax::INVALID_PATTERN_ID;
    if (this->match(TokenKind::kw_is)) {
        condition_pattern = PatternParser(this->parser_).parse_pattern();
    }

    const syntax::ExprId then_expr = this->parse_block_expr(context);
    this->expect_recovered(
        TokenKind::kw_else,
        std::string(PARSER_M2_IF_EXPR_REQUIRES_ELSE),
        RecoveryContext::if_else
    );
    const syntax::ExprId else_expr = this->check(TokenKind::kw_if)
        ? this->parse_if_expr(context)
        : this->parse_block_expr(context);

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::if_expr;
    expr.range = this->merge(begin.range, this->expr_range_or(else_expr, begin.range));
    expr.condition = condition;
    expr.condition_pattern = condition_pattern;
    expr.then_expr = then_expr;
    expr.else_expr = else_expr;
    return this->session_.module.push_expr(std::move(expr));
}

syntax::ExprId ExprParser::parse_match_expr(const ExprContext context) {
    const syntax::Token& begin = this->expect(TokenKind::kw_match, std::string(PARSER_EXPECT_MATCH));
    const syntax::ExprId value = this->parse_expr(ExprContext::no_struct_literal);
    this->expect_recovered(
        TokenKind::l_brace,
        std::string(PARSER_EXPECT_MATCH_BODY),
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
        std::string(PARSER_EXPECT_MATCH_END),
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
    syntax::ExprId guard = syntax::INVALID_EXPR_ID;
    if (this->match(TokenKind::kw_if)) {
        guard = this->parse_expr(context);
    }
    this->expect_recovered(
        TokenKind::fat_arrow,
        std::string(PARSER_EXPECT_MATCH_ARM_ARROW),
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

    this->report_here(std::string(PARSER_EXPECT_MATCH_ARM_SEPARATOR));
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

syntax::ExprId ExprParser::parse_binary_expr(const ExprContext context) {
    std::vector<syntax::ExprId> operands;
    std::vector<const BinaryOperatorSyntax*> operators;
    operands.reserve(PARSER_EXPR_OPERAND_STACK_INITIAL_CAPACITY);
    operators.reserve(PARSER_EXPR_OPERATOR_STACK_INITIAL_CAPACITY);

    const auto reduce_top_operator = [&]() {
        assert(!operators.empty());
        assert(operands.size() >= 2);
        const BinaryOperatorSyntax* const op = operators.back();
        operators.pop_back();
        const syntax::ExprId rhs = operands.back();
        operands.pop_back();
        const syntax::ExprId lhs = operands.back();
        operands.pop_back();
        operands.push_back(this->make_binary(op->op, lhs, rhs));
    };

    operands.push_back(this->parse_unary(context));
    while (const BinaryOperatorSyntax* op = binary_operator_for(this->peek().kind)) {
        this->advance();
        while (!operators.empty() &&
               precedence_rank(operators.back()->precedence) >= precedence_rank(op->precedence)) {
            reduce_top_operator();
        }
        operators.push_back(op);
        operands.push_back(this->parse_unary(context));
    }
    while (!operators.empty()) {
        reduce_top_operator();
    }
    return operands.back();
}

syntax::ExprId ExprParser::parse_unary(const ExprContext context) {
    struct PrefixOperator {
        syntax::TokenKind token_kind = TokenKind::invalid;
        syntax::UnaryOp unary_op = syntax::UnaryOp::logical_not;
        std::string_view text;
        base::SourceRange range {};
    };

    std::vector<PrefixOperator> prefixes;
    prefixes.reserve(PARSER_EXPR_PREFIX_STACK_INITIAL_CAPACITY);
    while (true) {
        if (this->check(TokenKind::plus_plus) || this->check(TokenKind::minus_minus)) {
            const syntax::Token& op = this->advance();
            const bool increment = op.kind == TokenKind::plus_plus;
            this->report_at(
                op,
                increment
                    ? std::string(PARSER_INCREMENT_UNSUPPORTED)
                    : std::string(PARSER_DECREMENT_UNSUPPORTED)
            );
            prefixes.push_back(PrefixOperator {
                op.kind,
                syntax::UnaryOp::logical_not,
                op.text,
                op.range,
            });
            continue;
        }

        if (this->check(TokenKind::amp)) {
            const syntax::Token& op = this->advance();
            syntax::UnaryOp unary_op = syntax::UnaryOp::address_of;
            base::SourceRange range = op.range;
            if (this->match(TokenKind::kw_mut)) {
                unary_op = syntax::UnaryOp::address_of_mut;
                range = this->merge(op.range, this->previous().range);
            }
            prefixes.push_back(PrefixOperator {
                op.kind,
                unary_op,
                op.text,
                range,
            });
            continue;
        }

        const std::optional<syntax::UnaryOp> unary_op = unary_op_for(this->peek().kind);
        if (!unary_op.has_value()) {
            break;
        }
        const syntax::Token& op = this->advance();
        prefixes.push_back(PrefixOperator {
            op.kind,
            unary_op.value(),
            op.text,
            op.range,
        });
    }

    syntax::ExprId expr = PostfixExprParser(this->parser_).parse_postfix(context);
    for (base::usize index = prefixes.size(); index > 0; --index) {
        const PrefixOperator& prefix = prefixes[index - 1];
        syntax::ExprNode node;
        node.kind = prefix.token_kind == TokenKind::plus_plus || prefix.token_kind == TokenKind::minus_minus
            ? syntax::ExprKind::invalid
            : syntax::ExprKind::unary;
        node.range = this->merge(prefix.range, this->expr_range_or(expr, prefix.range));
        node.text = prefix.text;
        node.unary_op = prefix.unary_op;
        node.unary_operand = expr;
        expr = this->session_.module.push_expr(std::move(node));
    }
    return expr;
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
