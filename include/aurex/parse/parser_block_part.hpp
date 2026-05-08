#pragma once

#include "aurex/parse/parser_part_base.hpp"

namespace aurex::parse {

class BlockParser final : private ParserPartBase {
public:
    explicit BlockParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::StmtId parse_block();
    [[nodiscard]] syntax::ExprId parse_block_expr(ExprContext context = ExprContext::normal);

private:
    [[nodiscard]] const syntax::Token& expect_block_start(std::string message);
    [[nodiscard]] const syntax::Token& expect_block_end(std::string message);
    [[nodiscard]] bool at_block_recovery_boundary() const noexcept;
};

} // namespace aurex::parse
