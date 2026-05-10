#include "aurex/parse/parser_stmt_part.hpp"

#include "aurex/parse/parser_control_stmt_part.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::StmtId StmtParser::parse_stmt() {
    this->reset_panic();
    if (this->check(TokenKind::kw_let)) {
        return this->parse_let_or_var_stmt(
            syntax::StmtKind::let,
            StatementTerminatorRecovery::synchronize
        );
    }
    if (this->check(TokenKind::kw_var)) {
        return this->parse_let_or_var_stmt(
            syntax::StmtKind::var,
            StatementTerminatorRecovery::synchronize
        );
    }
    if (this->check(TokenKind::kw_if)) {
        return ControlStmtParser(this->parser_).parse_if_stmt();
    }
    if (this->check(TokenKind::kw_for)) {
        return ControlStmtParser(this->parser_).parse_for_stmt();
    }
    if (this->check(TokenKind::kw_while)) {
        return ControlStmtParser(this->parser_).parse_while_stmt();
    }
    if (this->check(TokenKind::kw_defer)) {
        return ControlStmtParser(this->parser_).parse_defer_stmt();
    }
    if (this->check(TokenKind::kw_break)) {
        return ControlStmtParser(this->parser_).parse_break_stmt();
    }
    if (this->check(TokenKind::kw_continue)) {
        return ControlStmtParser(this->parser_).parse_continue_stmt();
    }
    if (this->check(TokenKind::kw_return)) {
        return ControlStmtParser(this->parser_).parse_return_stmt();
    }
    if (this->check(TokenKind::l_brace)) {
        return this->parse_block();
    }
    return this->parse_expr_or_assign_stmt();
}

syntax::StmtId StmtParser::parse_let_or_var_stmt(
    const syntax::StmtKind kind,
    const StatementTerminatorRecovery recovery
) {
    const syntax::Token& begin = this->advance();
    const syntax::Token& name = this->expect_identifier_recovered("expected local name");
    syntax::TypeId type = syntax::invalid_type_id;
    if (this->match(TokenKind::colon)) {
        type = this->parse_type();
    }
    this->expect_initializer_equal("expected initializer");
    const syntax::ExprId init = this->parse_expr();
    const syntax::Token& end = this->expect_statement_semicolon(
        "expected ';' after local declaration",
        recovery
    );

    syntax::StmtNode stmt;
    stmt.kind = kind;
    stmt.range = this->merge(begin.range, end.range);
    stmt.name = name.text;
    stmt.declared_type = type;
    stmt.init = init;
    return this->session_.module.push_stmt(std::move(stmt));
}

syntax::StmtId StmtParser::parse_expr_or_assign_stmt() {
    return this->parse_expr_or_assign_stmt(
        true,
        StatementTerminatorRecovery::synchronize
    );
}

syntax::StmtId StmtParser::parse_expr_or_assign_stmt(
    const bool require_semicolon,
    const StatementTerminatorRecovery recovery
) {
    return this->parse_expr_or_assign_stmt(ExprContext::normal, require_semicolon, recovery);
}

syntax::StmtId StmtParser::parse_expr_or_assign_stmt(
    const ExprContext context,
    const bool require_semicolon,
    const StatementTerminatorRecovery recovery
) {
    const syntax::ExprId lhs = this->parse_expr(context);
    syntax::AssignOp op = syntax::AssignOp::assign;
    if (this->match_assignment_operator(op)) {
        return this->parse_assignment_tail(lhs, context, op, require_semicolon, recovery);
    }

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::expr;
    stmt.init = lhs;
    base::SourceRange end_range = this->expr_range_or(lhs, this->peek().range);
    if (require_semicolon) {
        const syntax::Token& end = this->expect_statement_semicolon(
            "expected ';' after expression statement",
            recovery
        );
        end_range = end.range;
    } else {
        end_range = this->expr_range_or(stmt.init, end_range);
    }
    stmt.range = this->merge(this->expr_range_or(lhs, end_range), end_range);
    return this->session_.module.push_stmt(std::move(stmt));
}

bool StmtParser::match_assignment_operator(syntax::AssignOp& op) noexcept {
    if (this->match(TokenKind::equal)) {
        op = syntax::AssignOp::assign;
        return true;
    }
    if (this->match(TokenKind::plus_equal)) {
        op = syntax::AssignOp::add;
        return true;
    }
    if (this->match(TokenKind::minus_equal)) {
        op = syntax::AssignOp::sub;
        return true;
    }
    if (this->match(TokenKind::star_equal)) {
        op = syntax::AssignOp::mul;
        return true;
    }
    if (this->match(TokenKind::slash_equal)) {
        op = syntax::AssignOp::div;
        return true;
    }
    if (this->match(TokenKind::percent_equal)) {
        op = syntax::AssignOp::mod;
        return true;
    }
    if (this->match(TokenKind::less_less_equal)) {
        op = syntax::AssignOp::shl;
        return true;
    }
    if (this->match(TokenKind::greater_greater_equal)) {
        op = syntax::AssignOp::shr;
        return true;
    }
    if (this->match(TokenKind::amp_equal)) {
        op = syntax::AssignOp::bit_and;
        return true;
    }
    if (this->match(TokenKind::caret_equal)) {
        op = syntax::AssignOp::bit_xor;
        return true;
    }
    if (this->match(TokenKind::pipe_equal)) {
        op = syntax::AssignOp::bit_or;
        return true;
    }
    return false;
}

syntax::StmtId StmtParser::parse_assignment_tail(
    const syntax::ExprId lhs,
    const ExprContext context,
    const syntax::AssignOp op,
    const bool require_semicolon,
    const StatementTerminatorRecovery recovery
) {
    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::assign;
    stmt.assign_op = op;
    stmt.lhs = lhs;
    stmt.rhs = this->parse_expr(context);

    base::SourceRange end_range = this->expr_range_or(stmt.rhs, this->expr_range_or(lhs, this->peek().range));
    if (require_semicolon) {
        const syntax::Token& end = this->expect_statement_semicolon(
            "expected ';' after assignment",
            recovery
        );
        end_range = end.range;
    }
    stmt.range = this->merge(this->expr_range_or(lhs, end_range), end_range);
    return this->session_.module.push_stmt(std::move(stmt));
}

const syntax::Token& StmtParser::expect_statement_semicolon(
    std::string message,
    const StatementTerminatorRecovery recovery
) {
    switch (recovery) {
    case StatementTerminatorRecovery::direct:
        return this->expect(TokenKind::semicolon, std::move(message));
    case StatementTerminatorRecovery::synchronize:
        return this->expect_recovered(
            TokenKind::semicolon,
            std::move(message),
            RecoveryContext::statement_terminator
        );
    }
    return this->expect(TokenKind::semicolon, std::move(message));
}

} // namespace aurex::parse
