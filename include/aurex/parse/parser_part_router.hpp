#pragma once

#include "aurex/parse/expr_context.hpp"
#include "aurex/parse/parser_part_core.hpp"
#include "aurex/syntax/ast.hpp"

#include <vector>

namespace aurex::parse {

class ParserPartRouter : protected ParserPartCore {
protected:
    explicit ParserPartRouter(Parser& parser) noexcept
        : ParserPartCore(parser) {}

    [[nodiscard]] syntax::TypeId parse_type();
    [[nodiscard]] std::vector<syntax::TypeId> parse_type_arg_list();
    [[nodiscard]] syntax::StmtId parse_block();
    [[nodiscard]] syntax::ExprId parse_block_expr(ExprContext context = ExprContext::normal);
    [[nodiscard]] syntax::StmtId parse_stmt();
    [[nodiscard]] syntax::ExprId parse_expr(ExprContext context = ExprContext::normal);
    [[nodiscard]] syntax::PatternId parse_pattern();
};

} // namespace aurex::parse
