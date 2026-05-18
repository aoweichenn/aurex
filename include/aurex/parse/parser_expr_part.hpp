#pragma once

#include <aurex/parse/parser_part_base.hpp>

namespace aurex::parse {

class ExprParser final : private ParserPartBase {
public:
    explicit ExprParser(Parser& parser) noexcept : ParserPartBase(parser)
    {
    }

    [[nodiscard]] syntax::ExprId parse_expr(ExprContext context = ExprContext::normal);

private:
    [[nodiscard]] syntax::ExprId parse_if_expr(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_match_expr(ExprContext context);
    [[nodiscard]] syntax::MatchArm parse_match_arm(ExprContext context, const base::SourceRange& fallback_range);
    [[nodiscard]] bool recover_match_arm_separator() const;
    [[nodiscard]] syntax::ExprId parse_binary_expr(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_unary(ExprContext context) const;
    [[nodiscard]] syntax::ExprId make_binary(syntax::BinaryOp op, syntax::ExprId lhs, syntax::ExprId rhs) const;
};

} // namespace aurex::parse
