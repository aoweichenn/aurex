#pragma once

#include <aurex/parse/parser_part_base.hpp>

namespace aurex::parse {

class PrimaryExprParser final : private ParserPartBase {
public:
    explicit PrimaryExprParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ExprId parse_primary(ExprContext context);

private:
    [[nodiscard]] syntax::ExprId parse_builtin_expr(ExprContext context);
    void expect_grouped_expression_end();
    [[nodiscard]] syntax::ExprId parse_literal(syntax::ExprKind kind);
    [[nodiscard]] syntax::ExprId make_invalid_expr();
};

} // namespace aurex::parse
