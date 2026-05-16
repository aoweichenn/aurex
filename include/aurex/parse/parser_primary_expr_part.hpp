#pragma once

#include <aurex/parse/parser_part_base.hpp>

namespace aurex::parse {

class PrimaryExprParser final : private ParserPartBase {
public:
    explicit PrimaryExprParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ExprId parse_primary(ExprContext context);

private:
    [[nodiscard]] syntax::ExprId parse_unsafe_block_expr(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_array_literal(ExprContext context);
    bool recover_array_literal_separator();
    const syntax::Token& expect_array_literal_end();
    [[nodiscard]] syntax::ExprId parse_tuple_or_grouped_expr(ExprContext context);
    bool recover_tuple_literal_separator();
    const syntax::Token& expect_tuple_literal_end();
    [[nodiscard]] syntax::ExprId parse_builtin_expr(ExprContext context);
    void expect_grouped_expression_end();
    void skip_grouped_expression_remainder();
    [[nodiscard]] syntax::ExprId parse_literal(syntax::ExprKind kind) const;
    [[nodiscard]] syntax::ExprId make_invalid_expr();
};

} // namespace aurex::parse
