#include <aurex/parse/parser_control_stmt_part.hpp>

#include <aurex/parse/parser_stmt_part.hpp>

#include <string_view>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

constexpr std::string_view PARSER_FOR_RANGE_CALLEE = "range";
constexpr base::usize PARSER_FOR_RANGE_MIN_ARG_COUNT = 1;
constexpr base::usize PARSER_FOR_RANGE_MAX_ARG_COUNT = 2;

} // namespace

syntax::StmtId ControlStmtParser::parse_if_stmt() {
    const syntax::Token& begin = this->expect(TokenKind::kw_if, "expected 'if'");
    const syntax::ExprId condition = this->parse_expr(ExprContext::no_struct_literal);
    const syntax::StmtId then_block = this->parse_block();
    syntax::StmtId else_block = syntax::INVALID_STMT_ID;
    syntax::StmtId else_if = syntax::INVALID_STMT_ID;
    if (this->match(TokenKind::kw_else)) {
        if (this->check(TokenKind::kw_if)) {
            else_if = this->parse_if_stmt();
        } else {
            else_block = this->parse_block();
        }
    }

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::if_;
    if (syntax::is_valid(else_if)) {
        stmt.range = this->merge(begin.range, this->stmt_range_or(else_if, begin.range));
    } else if (syntax::is_valid(else_block)) {
        stmt.range = this->merge(begin.range, this->stmt_range_or(else_block, begin.range));
    } else {
        stmt.range = this->merge(begin.range, this->stmt_range_or(then_block, begin.range));
    }
    stmt.condition = condition;
    stmt.then_block = then_block;
    stmt.else_block = else_block;
    stmt.else_if = else_if;
    return this->session_.module.push_stmt(std::move(stmt));
}

syntax::StmtId ControlStmtParser::parse_while_stmt() {
    const syntax::Token& begin = this->expect(TokenKind::kw_while, "expected 'while'");
    const syntax::ExprId condition = this->parse_expr(ExprContext::no_struct_literal);
    const syntax::StmtId body = this->parse_block();

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::while_;
    stmt.range = this->merge(begin.range, this->stmt_range_or(body, begin.range));
    stmt.condition = condition;
    stmt.body = body;
    return this->session_.module.push_stmt(std::move(stmt));
}

syntax::StmtId ControlStmtParser::parse_for_stmt() {
    const syntax::Token& begin = this->expect(TokenKind::kw_for, "expected 'for'");
    if (this->next_for_is_range_loop()) {
        return this->parse_for_range_stmt(begin);
    }

    const syntax::StmtId init = this->parse_for_init_clause();
    syntax::ExprId condition = syntax::INVALID_EXPR_ID;
    if (!this->check(TokenKind::semicolon)) {
        condition = this->parse_expr(ExprContext::no_struct_literal);
    }
    this->expect_recovered(
        TokenKind::semicolon,
        "expected ';' after for condition",
        RecoveryContext::for_clause_separator
    );

    const syntax::StmtId update = this->parse_for_update_clause();
    const syntax::StmtId body = this->parse_block();

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::for_;
    stmt.range = this->merge(begin.range, this->stmt_range_or(body, begin.range));
    stmt.for_init = init;
    stmt.condition = condition;
    stmt.for_update = update;
    stmt.body = body;
    return this->session_.module.push_stmt(std::move(stmt));
}

bool ControlStmtParser::next_for_is_range_loop() const noexcept {
    return this->check(TokenKind::identifier) && this->check_next(TokenKind::kw_in);
}

syntax::StmtId ControlStmtParser::parse_for_range_stmt(const syntax::Token& begin) {
    const syntax::Token& name = this->expect(TokenKind::identifier, "expected loop variable after 'for'");
    this->expect(TokenKind::kw_in, "expected 'in' after loop variable");
    const syntax::Token& callee = this->expect_identifier_recovered("expected range after 'in'");
    if (callee.text != PARSER_FOR_RANGE_CALLEE) {
        this->report_at(callee, "for-in currently supports range(...) only");
    }
    std::vector<syntax::ExprId> args;
    const bool has_range_arguments = this->check(TokenKind::l_paren);
    this->expect(TokenKind::l_paren, "expected '(' after range");
    const syntax::Token* end = &callee;
    if (has_range_arguments) {
        this->parse_range_args(args);
        end = &this->expect_recovered(
            TokenKind::r_paren,
            "expected ')' after range arguments",
            RecoveryContext::call_argument
        );
    }
    if (args.size() < PARSER_FOR_RANGE_MIN_ARG_COUNT || args.size() > PARSER_FOR_RANGE_MAX_ARG_COUNT) {
        this->report_at(callee, "range expects 1 or 2 arguments");
    }

    const syntax::StmtId body = this->parse_block();

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::for_range;
    stmt.range = this->merge(begin.range, this->stmt_range_or(body, end->range));
    stmt.name = name.text;
    if (args.size() == PARSER_FOR_RANGE_MIN_ARG_COUNT) {
        stmt.range_end = args[0];
    } else if (args.size() >= PARSER_FOR_RANGE_MAX_ARG_COUNT) {
        stmt.range_start = args[0];
        stmt.range_end = args[1];
    }
    stmt.body = body;
    return this->session_.module.push_stmt(std::move(stmt));
}

void ControlStmtParser::parse_range_args(std::vector<syntax::ExprId>& args) {
    while (!this->is_eof() && !this->check(TokenKind::r_paren)) {
        args.push_back(this->parse_expr(ExprContext::no_struct_literal));
        this->reset_panic();
        if (!this->recover_range_arg_separator()) {
            break;
        }
    }
}

bool ControlStmtParser::recover_range_arg_separator() {
    if (this->check(TokenKind::r_paren)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }

    this->report_here("expected ',' or ')' after range argument");
    this->synchronize(RecoveryContext::call_argument);
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }
    this->reset_panic();
    return false;
}

syntax::StmtId ControlStmtParser::parse_break_stmt() {
    const syntax::Token& begin = this->expect(TokenKind::kw_break, "expected 'break'");
    const syntax::Token& end = this->expect_recovered(
        TokenKind::semicolon,
        "expected ';' after break",
        RecoveryContext::statement_terminator
    );
    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::break_;
    stmt.range = this->merge(begin.range, end.range);
    return this->session_.module.push_stmt(stmt);
}

syntax::StmtId ControlStmtParser::parse_continue_stmt() {
    const syntax::Token& begin = this->expect(TokenKind::kw_continue, "expected 'continue'");
    const syntax::Token& end = this->expect_recovered(
        TokenKind::semicolon,
        "expected ';' after continue",
        RecoveryContext::statement_terminator
    );
    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::continue_;
    stmt.range = this->merge(begin.range, end.range);
    return this->session_.module.push_stmt(stmt);
}

syntax::StmtId ControlStmtParser::parse_defer_stmt() {
    const syntax::Token& begin = this->expect(TokenKind::kw_defer, "expected 'defer'");
    const syntax::ExprId value = this->parse_expr();
    const syntax::Token& end = this->expect_recovered(
        TokenKind::semicolon,
        "expected ';' after defer statement",
        RecoveryContext::statement_terminator
    );

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::defer;
    stmt.range = this->merge(begin.range, end.range);
    stmt.init = value;
    return this->session_.module.push_stmt(std::move(stmt));
}

syntax::StmtId ControlStmtParser::parse_return_stmt() {
    const syntax::Token& begin = this->expect(TokenKind::kw_return, "expected 'return'");
    syntax::ExprId value = syntax::INVALID_EXPR_ID;
    if (!this->check(TokenKind::semicolon)) {
        value = this->parse_expr();
    }
    const syntax::Token& end = this->expect_recovered(
        TokenKind::semicolon,
        "expected ';' after return",
        RecoveryContext::statement_terminator
    );
    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::return_;
    stmt.range = this->merge(begin.range, end.range);
    stmt.return_value = value;
    return this->session_.module.push_stmt(std::move(stmt));
}

syntax::StmtId ControlStmtParser::parse_for_init_clause() {
    if (this->match(TokenKind::semicolon)) {
        return syntax::INVALID_STMT_ID;
    }
    if (this->check(TokenKind::kw_let)) {
        return StmtParser(this->parser_).parse_let_or_var_stmt(syntax::StmtKind::let);
    }
    if (this->check(TokenKind::kw_var)) {
        return StmtParser(this->parser_).parse_let_or_var_stmt(syntax::StmtKind::var);
    }
    return StmtParser(this->parser_).parse_expr_or_assign_stmt(true);
}

syntax::StmtId ControlStmtParser::parse_for_update_clause() {
    if (this->check(TokenKind::l_brace)) {
        return syntax::INVALID_STMT_ID;
    }
    return StmtParser(this->parser_).parse_expr_or_assign_stmt(false);
}

} // namespace aurex::parse
