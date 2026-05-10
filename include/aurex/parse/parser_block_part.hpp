#pragma once

#include <aurex/parse/parser_part_base.hpp>

#include <string_view>

namespace aurex::parse {

class BlockParser final : private ParserPartBase {
public:
    explicit BlockParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::StmtId parse_block();
    [[nodiscard]] syntax::ExprId parse_block_expr(ExprContext context = ExprContext::normal);

private:
    enum class BlockBodyMode {
        statement,
        expression,
    };

    struct BlockBody {
        syntax::StmtId block = syntax::invalid_stmt_id;
        syntax::ExprId result = syntax::invalid_expr_id;
    };

    [[nodiscard]] BlockBody parse_block_body(
        BlockBodyMode mode,
        ExprContext context,
        std::string_view start_message,
        std::string_view end_message
    );
    [[nodiscard]] bool token_starts_tail_expression() const noexcept;
    [[nodiscard]] bool token_starts_required_statement() const noexcept;
    [[nodiscard]] bool next_if_is_tail_expression() const noexcept;
    [[nodiscard]] bool next_block_is_tail_expression() const noexcept;
    [[nodiscard]] const syntax::Token& expect_block_start(std::string message);
    [[nodiscard]] const syntax::Token& expect_block_end(std::string message);
    [[nodiscard]] bool at_block_recovery_boundary() const noexcept;
};

} // namespace aurex::parse
