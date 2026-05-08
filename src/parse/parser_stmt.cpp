#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::StmtId StmtParser::parse_stmt() {
    this->reset_panic();
    if (this->check(TokenKind::kw_let)) {
        return this->parse_let_or_var_stmt(syntax::StmtKind::let);
    }
    if (this->check(TokenKind::kw_var)) {
        return this->parse_let_or_var_stmt(syntax::StmtKind::var);
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
    if (this->match(TokenKind::kw_break)) {
        const syntax::Token& begin = this->previous();
        const syntax::Token& end = this->expect(TokenKind::semicolon, "expected ';' after break");
        syntax::StmtNode stmt;
        stmt.kind = syntax::StmtKind::break_;
        stmt.range = this->merge(begin.range, end.range);
        return this->session_.module.push_stmt(stmt);
    }
    if (this->match(TokenKind::kw_continue)) {
        const syntax::Token& begin = this->previous();
        const syntax::Token& end = this->expect(TokenKind::semicolon, "expected ';' after continue");
        syntax::StmtNode stmt;
        stmt.kind = syntax::StmtKind::continue_;
        stmt.range = this->merge(begin.range, end.range);
        return this->session_.module.push_stmt(stmt);
    }
    if (this->check(TokenKind::kw_return)) {
        return ControlStmtParser(this->parser_).parse_return_stmt();
    }
    if (this->check(TokenKind::l_brace)) {
        return this->parse_block();
    }
    return this->parse_expr_or_assign_stmt();
}

syntax::StmtId StmtParser::parse_let_or_var_stmt(const syntax::StmtKind kind) {
    const syntax::Token& begin = this->advance();
    const syntax::Token& name = this->expect(TokenKind::identifier, "expected local name");
    syntax::TypeId type = syntax::invalid_type_id;
    if (this->match(TokenKind::colon)) {
        type = this->parse_type();
    }
    this->expect(TokenKind::equal, "expected initializer");
    const syntax::ExprId init = this->parse_expr();
    const syntax::Token& end = this->expect(TokenKind::semicolon, "expected ';' after local declaration");

    syntax::StmtNode stmt;
    stmt.kind = kind;
    stmt.range = this->merge(begin.range, end.range);
    stmt.name = name.text;
    stmt.declared_type = type;
    stmt.init = init;
    return this->session_.module.push_stmt(std::move(stmt));
}

syntax::StmtId StmtParser::parse_expr_or_assign_stmt() {
    return this->parse_expr_or_assign_stmt(true);
}

syntax::StmtId StmtParser::parse_expr_or_assign_stmt(const bool require_semicolon) {
    const syntax::ExprId lhs = this->parse_expr();
    syntax::StmtNode stmt;
    if (this->match(TokenKind::equal)) {
        stmt.kind = syntax::StmtKind::assign;
        stmt.lhs = lhs;
        stmt.rhs = this->parse_expr();
    } else {
        stmt.kind = syntax::StmtKind::expr;
        stmt.init = lhs;
    }
    base::SourceRange end_range = syntax::is_valid(lhs) ? this->session_.module.exprs[lhs.value].range : this->peek().range;
    if (require_semicolon) {
        const syntax::Token& end = this->expect(TokenKind::semicolon, "expected ';' after expression statement");
        end_range = end.range;
    } else if (stmt.kind == syntax::StmtKind::assign && syntax::is_valid(stmt.rhs) && stmt.rhs.value < this->session_.module.exprs.size()) {
        end_range = this->session_.module.exprs[stmt.rhs.value].range;
    } else if (stmt.kind == syntax::StmtKind::expr && syntax::is_valid(stmt.init) && stmt.init.value < this->session_.module.exprs.size()) {
        end_range = this->session_.module.exprs[stmt.init.value].range;
    }
    stmt.range = syntax::is_valid(lhs) ? this->merge(this->session_.module.exprs[lhs.value].range, end_range) : end_range;
    return this->session_.module.push_stmt(std::move(stmt));
}

} // namespace aurex::parse
