#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::StmtId ControlStmtParser::parse_if_stmt() {
    const syntax::Token& begin = expect(TokenKind::kw_if, "expected 'if'");
    const syntax::ExprId condition = parse_expr(ExprContext::no_struct_literal);
    const syntax::StmtId then_block = parse_block();
    syntax::StmtId else_block = syntax::invalid_stmt_id;
    syntax::StmtId else_if = syntax::invalid_stmt_id;
    if (match(TokenKind::kw_else)) {
        if (check(TokenKind::kw_if)) {
            else_if = parse_if_stmt();
        } else {
            else_block = parse_block();
        }
    }

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::if_;
    if (syntax::is_valid(else_if)) {
        stmt.range = merge(begin.range, session_.module.stmts[else_if.value].range);
    } else if (syntax::is_valid(else_block)) {
        stmt.range = merge(begin.range, session_.module.stmts[else_block.value].range);
    } else {
        stmt.range = merge(begin.range, session_.module.stmts[then_block.value].range);
    }
    stmt.condition = condition;
    stmt.then_block = then_block;
    stmt.else_block = else_block;
    stmt.else_if = else_if;
    return session_.module.push_stmt(std::move(stmt));
}

syntax::StmtId ControlStmtParser::parse_while_stmt() {
    const syntax::Token& begin = expect(TokenKind::kw_while, "expected 'while'");
    const syntax::ExprId condition = parse_expr(ExprContext::no_struct_literal);
    const syntax::StmtId body = parse_block();

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::while_;
    stmt.range = merge(begin.range, session_.module.stmts[body.value].range);
    stmt.condition = condition;
    stmt.body = body;
    return session_.module.push_stmt(std::move(stmt));
}

syntax::StmtId ControlStmtParser::parse_for_stmt() {
    const syntax::Token& begin = expect(TokenKind::kw_for, "expected 'for'");
    syntax::StmtId init = syntax::invalid_stmt_id;
    if (match(TokenKind::semicolon)) {
        // Empty initializer.
    } else if (check(TokenKind::kw_let)) {
        init = StmtParser(parser_).parse_let_or_var_stmt(syntax::StmtKind::let);
    } else if (check(TokenKind::kw_var)) {
        init = StmtParser(parser_).parse_let_or_var_stmt(syntax::StmtKind::var);
    } else {
        init = StmtParser(parser_).parse_expr_or_assign_stmt(true);
    }

    syntax::ExprId condition = syntax::invalid_expr_id;
    if (!check(TokenKind::semicolon)) {
        condition = parse_expr(ExprContext::no_struct_literal);
    }
    expect(TokenKind::semicolon, "expected ';' after for condition");

    syntax::StmtId update = syntax::invalid_stmt_id;
    if (!check(TokenKind::l_brace)) {
        update = StmtParser(parser_).parse_expr_or_assign_stmt(false);
    }
    const syntax::StmtId body = parse_block();

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::for_;
    stmt.range = merge(begin.range, session_.module.stmts[body.value].range);
    stmt.for_init = init;
    stmt.condition = condition;
    stmt.for_update = update;
    stmt.body = body;
    return session_.module.push_stmt(std::move(stmt));
}

syntax::StmtId ControlStmtParser::parse_defer_stmt() {
    const syntax::Token& begin = expect(TokenKind::kw_defer, "expected 'defer'");
    const syntax::ExprId value = parse_expr();
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after defer statement");

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::defer;
    stmt.range = merge(begin.range, end.range);
    stmt.init = value;
    return session_.module.push_stmt(std::move(stmt));
}

syntax::StmtId ControlStmtParser::parse_return_stmt() {
    const syntax::Token& begin = expect(TokenKind::kw_return, "expected 'return'");
    syntax::ExprId value = syntax::invalid_expr_id;
    if (!check(TokenKind::semicolon)) {
        value = parse_expr();
    }
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after return");
    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::return_;
    stmt.range = merge(begin.range, end.range);
    stmt.return_value = value;
    return session_.module.push_stmt(std::move(stmt));
}

} // namespace aurex::parse
