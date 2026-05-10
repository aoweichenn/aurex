#pragma once

#include "aurex/parse/parser_part_base.hpp"

namespace aurex::parse {

class ExprParser final : private ParserPartBase {
public:
    explicit ExprParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ExprId parse_expr(ExprContext context = ExprContext::normal);

private:
    [[nodiscard]] syntax::ExprId parse_if_expr(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_match_expr(ExprContext context);
    [[nodiscard]] syntax::MatchArm parse_match_arm(ExprContext context, base::SourceRange fallback_range);
    [[nodiscard]] bool recover_match_arm_separator();
    [[nodiscard]] syntax::ExprId parse_binary_expr(ExprContext context, int min_precedence);
    [[nodiscard]] syntax::ExprId parse_unary(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_rejected_update_operator_expr(
        syntax::TokenKind kind,
        std::string message,
        ExprContext context
    );
    [[nodiscard]] syntax::ExprId make_binary(
        syntax::BinaryOp op,
        syntax::ExprId lhs,
        syntax::ExprId rhs
    );
};

} // namespace aurex::parse
