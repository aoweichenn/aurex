#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::StmtId StmtParser::parse_block() {
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

syntax::ExprId StmtParser::parse_block_expr(const ExprContext context) {
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

syntax::StmtId StmtParser::parse_stmt() {
    reset_panic();
    if (check(TokenKind::kw_let)) {
        return parse_let_or_var_stmt(syntax::StmtKind::let);
    }
    if (check(TokenKind::kw_var)) {
        return parse_let_or_var_stmt(syntax::StmtKind::var);
    }
    if (check(TokenKind::kw_if)) {
        return parse_if_stmt();
    }
    if (check(TokenKind::kw_for)) {
        return parse_for_stmt();
    }
    if (check(TokenKind::kw_while)) {
        return parse_while_stmt();
    }
    if (check(TokenKind::kw_defer)) {
        return parse_defer_stmt();
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
        return parse_return_stmt();
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

syntax::StmtId StmtParser::parse_if_stmt() {
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

syntax::StmtId StmtParser::parse_while_stmt() {
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

syntax::StmtId StmtParser::parse_for_stmt() {
    const syntax::Token& begin = expect(TokenKind::kw_for, "expected 'for'");
    syntax::StmtId init = syntax::invalid_stmt_id;
    if (match(TokenKind::semicolon)) {
        // Empty initializer.
    } else if (check(TokenKind::kw_let)) {
        init = parse_let_or_var_stmt(syntax::StmtKind::let);
    } else if (check(TokenKind::kw_var)) {
        init = parse_let_or_var_stmt(syntax::StmtKind::var);
    } else {
        init = parse_expr_or_assign_stmt(true);
    }

    syntax::ExprId condition = syntax::invalid_expr_id;
    if (!check(TokenKind::semicolon)) {
        condition = parse_expr(ExprContext::no_struct_literal);
    }
    expect(TokenKind::semicolon, "expected ';' after for condition");

    syntax::StmtId update = syntax::invalid_stmt_id;
    if (!check(TokenKind::l_brace)) {
        update = parse_expr_or_assign_stmt(false);
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

syntax::StmtId StmtParser::parse_defer_stmt() {
    const syntax::Token& begin = expect(TokenKind::kw_defer, "expected 'defer'");
    const syntax::ExprId value = parse_expr();
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after defer statement");

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::defer;
    stmt.range = merge(begin.range, end.range);
    stmt.init = value;
    return session_.module.push_stmt(std::move(stmt));
}

syntax::StmtId StmtParser::parse_return_stmt() {
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
