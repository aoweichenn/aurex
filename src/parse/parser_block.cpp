#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::StmtId BlockParser::parse_block() {
    const syntax::Token& begin = expect(TokenKind::l_brace, "expected block");
    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        const syntax::StmtId stmt = parse_stmt();
        if (syntax::is_valid(stmt)) {
            block.statements.push_back(stmt);
        } else {
            synchronize();
        }
        reset_panic();
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after block");
    block.range = merge(begin.range, end.range);
    reset_panic();
    return session_.module.push_stmt(std::move(block));
}

syntax::ExprId BlockParser::parse_block_expr(const ExprContext context) {
    const syntax::Token& begin = expect(TokenKind::l_brace, "expected block expression");
    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    syntax::ExprId result = syntax::invalid_expr_id;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        if (check(TokenKind::kw_let) || check(TokenKind::kw_var) || check(TokenKind::kw_defer)) {
            const syntax::StmtId stmt = parse_stmt();
            if (syntax::is_valid(stmt)) {
                block.statements.push_back(stmt);
            } else {
                synchronize();
            }
            reset_panic();
            continue;
        }

        const syntax::ExprId expr = parse_expr(context);
        if (match(TokenKind::equal)) {
            syntax::StmtNode stmt;
            stmt.kind = syntax::StmtKind::assign;
            stmt.lhs = expr;
            stmt.rhs = parse_expr(context);
            const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after assignment");
            stmt.range = syntax::is_valid(expr) ? merge(session_.module.exprs[expr.value].range, end.range) : end.range;
            block.statements.push_back(session_.module.push_stmt(std::move(stmt)));
            reset_panic();
            continue;
        }
        if (match(TokenKind::semicolon)) {
            syntax::StmtNode stmt;
            stmt.kind = syntax::StmtKind::expr;
            stmt.init = expr;
            stmt.range = syntax::is_valid(expr) ? merge(session_.module.exprs[expr.value].range, previous().range) : previous().range;
            block.statements.push_back(session_.module.push_stmt(std::move(stmt)));
            reset_panic();
            continue;
        }

        result = expr;
        break;
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after block expression");
    block.range = merge(begin.range, end.range);
    const syntax::StmtId block_id = session_.module.push_stmt(std::move(block));

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::block_expr;
    expr.range = merge(begin.range, end.range);
    expr.block = block_id;
    expr.block_result = result;
    reset_panic();
    return session_.module.push_expr(std::move(expr));
}

} // namespace aurex::parse
