#include "aurex/parse/parser_block_part.hpp"

#include "aurex/parse/recovery.hpp"
#include "aurex/parse/parser_stmt_part.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::StmtId BlockParser::parse_block() {
    const syntax::Token& begin = this->expect_block_start("expected block");
    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;

    while (!this->is_eof() && !this->at_block_recovery_boundary()) {
        const syntax::StmtId stmt = this->parse_stmt();
        if (syntax::is_valid(stmt)) {
            block.statements.push_back(stmt);
        } else {
            this->synchronize(RecoveryContext::statement);
        }
        this->reset_panic();
    }

    const syntax::Token& end = this->expect_block_end("expected '}' after block");
    block.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_stmt(std::move(block));
}

syntax::ExprId BlockParser::parse_block_expr(const ExprContext context) {
    const syntax::Token& begin = this->expect_block_start("expected block expression");
    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    syntax::ExprId result = syntax::invalid_expr_id;

    while (!this->is_eof() && !this->at_block_recovery_boundary()) {
        if (this->check(TokenKind::kw_let) || this->check(TokenKind::kw_var) || this->check(TokenKind::kw_defer)) {
            const syntax::StmtId stmt = this->parse_stmt();
            if (syntax::is_valid(stmt)) {
                block.statements.push_back(stmt);
            } else {
                this->synchronize(RecoveryContext::statement);
            }
            this->reset_panic();
            continue;
        }

        const syntax::StmtId stmt = StmtParser(this->parser_).parse_expr_or_assign_stmt(context, false);
        if (syntax::is_valid(stmt) &&
            stmt.value < this->session_.module.stmts.size() &&
            this->session_.module.stmts[stmt.value].kind == syntax::StmtKind::assign) {
            const syntax::Token& end = this->expect_recovered(
                TokenKind::semicolon,
                "expected ';' after assignment",
                RecoveryContext::statement_terminator
            );
            this->session_.module.stmts[stmt.value].range =
                this->merge(this->stmt_range_or(stmt, end.range), end.range);
            block.statements.push_back(stmt);
            this->reset_panic();
            continue;
        }
        if (this->match(TokenKind::semicolon)) {
            if (syntax::is_valid(stmt) &&
                stmt.value < this->session_.module.stmts.size()) {
                this->session_.module.stmts[stmt.value].range =
                    this->merge(this->stmt_range_or(stmt, this->previous().range), this->previous().range);
                block.statements.push_back(stmt);
            }
            this->reset_panic();
            continue;
        }

        if (syntax::is_valid(stmt) &&
            stmt.value < this->session_.module.stmts.size() &&
            this->session_.module.stmts[stmt.value].kind == syntax::StmtKind::expr) {
            result = this->session_.module.stmts[stmt.value].init;
        }
        break;
    }

    const syntax::Token& end = this->expect_block_end("expected '}' after block expression");
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

const syntax::Token& BlockParser::expect_block_start(std::string message) {
    return this->expect_recovered(
        TokenKind::l_brace,
        std::move(message),
        RecoveryContext::block_start
    );
}

const syntax::Token& BlockParser::expect_block_end(std::string message) {
    return this->expect_recovered(
        TokenKind::r_brace,
        std::move(message),
        RecoveryContext::block_end
    );
}

bool BlockParser::at_block_recovery_boundary() const noexcept {
    return token_matches_recovery_context(this->peek().kind, RecoveryContext::item);
}

} // namespace aurex::parse
