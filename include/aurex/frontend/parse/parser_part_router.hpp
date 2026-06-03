#pragma once

#include <aurex/frontend/parse/expr_context.hpp>
#include <aurex/frontend/parse/parser_part_core.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>

namespace aurex::parse {

class ParserPartRouter : protected ParserPartCore {
protected:
    explicit ParserPartRouter(Parser& parser) noexcept : ParserPartCore(parser)
    {
    }

    [[nodiscard]] syntax::TypeId parse_type() const;
    [[nodiscard]] syntax::StmtId parse_block() const;
    [[nodiscard]] syntax::ExprId parse_block_expr(ExprContext context = ExprContext::normal) const;
    [[nodiscard]] syntax::StmtId parse_stmt() const;
    [[nodiscard]] syntax::ExprId parse_expr(ExprContext context = ExprContext::normal) const;
    [[nodiscard]] syntax::PatternId parse_pattern() const;
};

} // namespace aurex::parse
