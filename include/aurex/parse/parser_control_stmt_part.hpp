#pragma once

#include <aurex/parse/parser_part_base.hpp>

#include <vector>

namespace aurex::parse {

class ControlStmtParser final : private ParserPartBase {
public:
    explicit ControlStmtParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::StmtId parse_if_stmt();
    [[nodiscard]] syntax::StmtId parse_for_stmt();
    [[nodiscard]] syntax::StmtId parse_while_stmt();
    [[nodiscard]] syntax::StmtId parse_break_stmt();
    [[nodiscard]] syntax::StmtId parse_continue_stmt();
    [[nodiscard]] syntax::StmtId parse_defer_stmt();
    [[nodiscard]] syntax::StmtId parse_return_stmt();

private:
    [[nodiscard]] bool next_for_is_range_loop() const noexcept;
    [[nodiscard]] syntax::StmtId parse_for_range_stmt(const syntax::Token& begin);
    void parse_range_args(std::vector<syntax::ExprId>& args);
    [[nodiscard]] bool recover_range_arg_separator();
    [[nodiscard]] syntax::StmtId parse_for_init_clause();
    [[nodiscard]] syntax::StmtId parse_for_update_clause();
};

} // namespace aurex::parse
