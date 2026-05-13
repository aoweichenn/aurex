#pragma once

#include <aurex/parse/parser_part_base.hpp>

namespace aurex::parse {

enum class StatementTerminatorRecovery {
    direct,
    synchronize,
};

class StmtParser final : private ParserPartBase {
public:
    explicit StmtParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::StmtId parse_stmt();
    [[nodiscard]] syntax::StmtId parse_let_or_var_stmt(
        syntax::StmtKind kind,
        StatementTerminatorRecovery recovery = StatementTerminatorRecovery::direct
    );
    [[nodiscard]] syntax::StmtId parse_expr_or_assign_stmt();
    [[nodiscard]] syntax::StmtId parse_expr_or_assign_stmt(
        bool require_semicolon,
        StatementTerminatorRecovery recovery = StatementTerminatorRecovery::direct
    );
    [[nodiscard]] syntax::StmtId parse_expr_or_assign_stmt(
        ExprContext context,
        bool require_semicolon,
        StatementTerminatorRecovery recovery = StatementTerminatorRecovery::direct
    );

private:
    [[nodiscard]] syntax::StmtId parse_unsafe_block_stmt();
    [[nodiscard]] bool match_assignment_operator(syntax::AssignOp& op) noexcept;
    [[nodiscard]] syntax::StmtId parse_assignment_tail(
        syntax::ExprId lhs,
        ExprContext context,
        syntax::AssignOp op,
        bool require_semicolon,
        StatementTerminatorRecovery recovery
    );
    [[nodiscard]] const syntax::Token& expect_statement_semicolon(
        std::string message,
        StatementTerminatorRecovery recovery
    );
};

} // namespace aurex::parse
