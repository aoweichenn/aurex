#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::StmtId BlockParser::parse_block() {
    const syntax::Token& begin = this->expect(TokenKind::l_brace, "expected block");
    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;

    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        const syntax::StmtId stmt = this->parse_stmt();
        if (syntax::is_valid(stmt)) {
            block.statements.push_back(stmt);
        } else {
            this->synchronize();
        }
        this->reset_panic();
    }

    const syntax::Token& end = this->expect(TokenKind::r_brace, "expected '}' after block");
    block.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_stmt(std::move(block));
}

syntax::ExprId BlockParser::parse_block_expr(const ExprContext context) {
    const syntax::Token& begin = this->expect(TokenKind::l_brace, "expected block expression");
    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    syntax::ExprId result = syntax::invalid_expr_id;

    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        if (this->check(TokenKind::kw_let) || this->check(TokenKind::kw_var) || this->check(TokenKind::kw_defer)) {
            const syntax::StmtId stmt = this->parse_stmt();
            if (syntax::is_valid(stmt)) {
                block.statements.push_back(stmt);
            } else {
                this->synchronize();
            }
            this->reset_panic();
            continue;
        }

        const syntax::ExprId expr = this->parse_expr(context);
        if (this->match(TokenKind::equal)) {
            syntax::StmtNode stmt;
            stmt.kind = syntax::StmtKind::assign;
            stmt.lhs = expr;
            stmt.rhs = this->parse_expr(context);
            const syntax::Token& end = this->expect(TokenKind::semicolon, "expected ';' after assignment");
            stmt.range = this->merge(this->expr_range_or(expr, end.range), end.range);
            block.statements.push_back(this->session_.module.push_stmt(std::move(stmt)));
            this->reset_panic();
            continue;
        }
        if (this->match(TokenKind::semicolon)) {
            syntax::StmtNode stmt;
            stmt.kind = syntax::StmtKind::expr;
            stmt.init = expr;
            stmt.range = this->merge(this->expr_range_or(expr, this->previous().range), this->previous().range);
            block.statements.push_back(this->session_.module.push_stmt(std::move(stmt)));
            this->reset_panic();
            continue;
        }

        result = expr;
        break;
    }

    const syntax::Token& end = this->expect(TokenKind::r_brace, "expected '}' after block expression");
    block.range = this->merge(begin.range, end.range);
    const syntax::StmtId block_id = this->session_.module.push_stmt(std::move(block));

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::block_expr;
    expr.range = this->merge(begin.range, end.range);
    expr.block = block_id;
    expr.block_result = result;
    this->reset_panic();
    return this->session_.module.push_expr(std::move(expr));
}

} // namespace aurex::parse
