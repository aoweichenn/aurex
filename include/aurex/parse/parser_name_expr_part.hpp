#pragma once

#include <aurex/parse/parser_part_base.hpp>

namespace aurex::parse {

class NameExprParser final : private ParserPartBase {
public:
    explicit NameExprParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ExprId parse_name_or_struct_literal(ExprContext context);

private:
    [[nodiscard]] syntax::ExprId make_name_expr(const syntax::Token& name) const;
};

} // namespace aurex::parse
