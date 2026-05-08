#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::StmtId StmtParser::parse_stmt() {
    reset_panic();
    if (check(TokenKind::kw_let)) {
        return parse_let_or_var_stmt(syntax::StmtKind::let);
    }
    if (check(TokenKind::kw_var)) {
        return parse_let_or_var_stmt(syntax::StmtKind::var);
    }
    if (check(TokenKind::kw_if)) {
        return ControlStmtParser(parser_).parse_if_stmt();
    }
    if (check(TokenKind::kw_for)) {
        return ControlStmtParser(parser_).parse_for_stmt();
    }
    if (check(TokenKind::kw_while)) {
        return ControlStmtParser(parser_).parse_while_stmt();
    }
    if (check(TokenKind::kw_defer)) {
        return ControlStmtParser(parser_).parse_defer_stmt();
    }
    if (match(TokenKind::kw_break)) {
        const syntax::Token& begin = previous();
        const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after break");
        syntax::StmtNode stmt;
        stmt.kind = syntax::StmtKind::break_;
        stmt.range = merge(begin.range, end.range);
        return session_.module.push_stmt(stmt);
    }
    if (match(TokenKind::kw_continue)) {
        const syntax::Token& begin = previous();
        const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after continue");
        syntax::StmtNode stmt;
        stmt.kind = syntax::StmtKind::continue_;
        stmt.range = merge(begin.range, end.range);
        return session_.module.push_stmt(stmt);
    }
    if (check(TokenKind::kw_return)) {
        return ControlStmtParser(parser_).parse_return_stmt();
    }
    if (check(TokenKind::l_brace)) {
        return parse_block();
    }
    return parse_expr_or_assign_stmt();
}

syntax::StmtId StmtParser::parse_let_or_var_stmt(const syntax::StmtKind kind) {
    const syntax::Token& begin = advance();
    const syntax::Token& name = expect(TokenKind::identifier, "expected local name");
    syntax::TypeId type = syntax::invalid_type_id;
    if (match(TokenKind::colon)) {
        type = parse_type();
    }
    expect(TokenKind::equal, "expected initializer");
    const syntax::ExprId init = parse_expr();
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after local declaration");

    syntax::StmtNode stmt;
    stmt.kind = kind;
    stmt.range = merge(begin.range, end.range);
    stmt.name = name.text;
    stmt.declared_type = type;
    stmt.init = init;
    return session_.module.push_stmt(std::move(stmt));
}

syntax::StmtId StmtParser::parse_expr_or_assign_stmt() {
    return parse_expr_or_assign_stmt(true);
}

syntax::StmtId StmtParser::parse_expr_or_assign_stmt(const bool require_semicolon) {
    const syntax::ExprId lhs = parse_expr();
    syntax::StmtNode stmt;
    if (match(TokenKind::equal)) {
        stmt.kind = syntax::StmtKind::assign;
        stmt.lhs = lhs;
        stmt.rhs = parse_expr();
    } else {
        stmt.kind = syntax::StmtKind::expr;
        stmt.init = lhs;
    }
    base::SourceRange end_range = syntax::is_valid(lhs) ? session_.module.exprs[lhs.value].range : peek().range;
    if (require_semicolon) {
        const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after expression statement");
        end_range = end.range;
    } else if (stmt.kind == syntax::StmtKind::assign && syntax::is_valid(stmt.rhs) && stmt.rhs.value < session_.module.exprs.size()) {
        end_range = session_.module.exprs[stmt.rhs.value].range;
    } else if (stmt.kind == syntax::StmtKind::expr && syntax::is_valid(stmt.init) && stmt.init.value < session_.module.exprs.size()) {
        end_range = session_.module.exprs[stmt.init.value].range;
    }
    stmt.range = syntax::is_valid(lhs) ? merge(session_.module.exprs[lhs.value].range, end_range) : end_range;
    return session_.module.push_stmt(std::move(stmt));
}

} // namespace aurex::parse
