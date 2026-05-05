#include "aurex/parse/parser.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

[[nodiscard]] syntax::BinaryOp binary_op_from_token(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::plus: return syntax::BinaryOp::add;
    case TokenKind::minus: return syntax::BinaryOp::sub;
    case TokenKind::star: return syntax::BinaryOp::mul;
    case TokenKind::slash: return syntax::BinaryOp::div;
    case TokenKind::percent: return syntax::BinaryOp::mod;
    case TokenKind::less_less: return syntax::BinaryOp::shl;
    case TokenKind::greater_greater: return syntax::BinaryOp::shr;
    case TokenKind::less: return syntax::BinaryOp::less;
    case TokenKind::less_equal: return syntax::BinaryOp::less_equal;
    case TokenKind::greater: return syntax::BinaryOp::greater;
    case TokenKind::greater_equal: return syntax::BinaryOp::greater_equal;
    case TokenKind::equal_equal: return syntax::BinaryOp::equal;
    case TokenKind::bang_equal: return syntax::BinaryOp::not_equal;
    case TokenKind::amp: return syntax::BinaryOp::bit_and;
    case TokenKind::caret: return syntax::BinaryOp::bit_xor;
    case TokenKind::pipe: return syntax::BinaryOp::bit_or;
    case TokenKind::amp_amp: return syntax::BinaryOp::logical_and;
    case TokenKind::pipe_pipe: return syntax::BinaryOp::logical_or;
    default: return syntax::BinaryOp::add;
    }
}

} // namespace

syntax::ExprId Parser::parse_block_expr() {
    const syntax::Token& begin = expect(TokenKind::l_brace, "expected block expression");
    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    syntax::ExprId result = syntax::invalid_expr_id;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        if (check(TokenKind::kw_let) || check(TokenKind::kw_var)) {
            const syntax::StmtId stmt = parse_stmt();
            if (syntax::is_valid(stmt)) {
                block.statements.push_back(stmt);
            } else {
                synchronize();
            }
            panic_ = false;
            continue;
        }

        const syntax::ExprId expr = parse_expr();
        if (match(TokenKind::equal)) {
            syntax::StmtNode stmt;
            stmt.kind = syntax::StmtKind::assign;
            stmt.lhs = expr;
            stmt.rhs = parse_expr();
            const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after assignment");
            stmt.range = syntax::is_valid(expr) ? merge(module_.exprs[expr.value].range, end.range) : end.range;
            block.statements.push_back(module_.push_stmt(std::move(stmt)));
            panic_ = false;
            continue;
        }
        if (match(TokenKind::semicolon)) {
            syntax::StmtNode stmt;
            stmt.kind = syntax::StmtKind::expr;
            stmt.init = expr;
            stmt.range = syntax::is_valid(expr) ? merge(module_.exprs[expr.value].range, previous().range) : previous().range;
            block.statements.push_back(module_.push_stmt(std::move(stmt)));
            panic_ = false;
            continue;
        }

        result = expr;
        break;
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after block expression");
    block.range = merge(begin.range, end.range);
    const syntax::StmtId block_id = module_.push_stmt(std::move(block));

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::block_expr;
    expr.range = merge(begin.range, end.range);
    expr.block = block_id;
    expr.block_result = result;
    panic_ = false;
    return module_.push_expr(std::move(expr));
}

syntax::StmtId Parser::parse_stmt() {
    panic_ = false;
    if (check(TokenKind::kw_let)) {
        return parse_let_or_var_stmt(syntax::StmtKind::let);
    }
    if (check(TokenKind::kw_var)) {
        return parse_let_or_var_stmt(syntax::StmtKind::var);
    }
    if (check(TokenKind::kw_if)) {
        return parse_if_stmt();
    }
    if (check(TokenKind::kw_while)) {
        return parse_while_stmt();
    }
    if (match(TokenKind::kw_break)) {
        const syntax::Token& begin = previous();
        const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after break");
        syntax::StmtNode stmt;
        stmt.kind = syntax::StmtKind::break_;
        stmt.range = merge(begin.range, end.range);
        return module_.push_stmt(stmt);
    }
    if (match(TokenKind::kw_continue)) {
        const syntax::Token& begin = previous();
        const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after continue");
        syntax::StmtNode stmt;
        stmt.kind = syntax::StmtKind::continue_;
        stmt.range = merge(begin.range, end.range);
        return module_.push_stmt(stmt);
    }
    if (check(TokenKind::kw_return)) {
        return parse_return_stmt();
    }
    if (check(TokenKind::l_brace)) {
        return parse_block();
    }
    return parse_expr_or_assign_stmt();
}

syntax::StmtId Parser::parse_let_or_var_stmt(const syntax::StmtKind kind) {
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
    return module_.push_stmt(std::move(stmt));
}

syntax::StmtId Parser::parse_if_stmt() {
    const syntax::Token& begin = expect(TokenKind::kw_if, "expected 'if'");
    const bool previous_struct_literal_mode = allow_struct_literal_;
    allow_struct_literal_ = false;
    const syntax::ExprId condition = parse_expr();
    allow_struct_literal_ = previous_struct_literal_mode;
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
        stmt.range = merge(begin.range, module_.stmts[else_if.value].range);
    } else if (syntax::is_valid(else_block)) {
        stmt.range = merge(begin.range, module_.stmts[else_block.value].range);
    } else {
        stmt.range = merge(begin.range, module_.stmts[then_block.value].range);
    }
    stmt.condition = condition;
    stmt.then_block = then_block;
    stmt.else_block = else_block;
    stmt.else_if = else_if;
    return module_.push_stmt(std::move(stmt));
}

syntax::StmtId Parser::parse_while_stmt() {
    const syntax::Token& begin = expect(TokenKind::kw_while, "expected 'while'");
    const bool previous_struct_literal_mode = allow_struct_literal_;
    allow_struct_literal_ = false;
    const syntax::ExprId condition = parse_expr();
    allow_struct_literal_ = previous_struct_literal_mode;
    const syntax::StmtId body = parse_block();

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::while_;
    stmt.range = merge(begin.range, module_.stmts[body.value].range);
    stmt.condition = condition;
    stmt.body = body;
    return module_.push_stmt(std::move(stmt));
}

syntax::StmtId Parser::parse_return_stmt() {
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
    return module_.push_stmt(std::move(stmt));
}

syntax::StmtId Parser::parse_expr_or_assign_stmt() {
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
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after expression statement");
    stmt.range = syntax::is_valid(lhs) ? merge(module_.exprs[lhs.value].range, end.range) : end.range;
    return module_.push_stmt(std::move(stmt));
}

syntax::ExprId Parser::parse_expr() {
    if (check(TokenKind::kw_if)) {
        return parse_if_expr();
    }
    if (check(TokenKind::kw_match)) {
        return parse_match_expr();
    }
    return parse_logical_or();
}

syntax::ExprId Parser::parse_if_expr() {
    const syntax::Token& begin = expect(TokenKind::kw_if, "expected 'if'");
    const bool previous_struct_literal_mode = allow_struct_literal_;
    allow_struct_literal_ = false;
    const syntax::ExprId condition = parse_expr();
    allow_struct_literal_ = previous_struct_literal_mode;

    const syntax::ExprId then_expr = parse_block_expr();
    expect(TokenKind::kw_else, "if expression requires else branch");
    const syntax::ExprId else_expr = parse_block_expr();

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::if_expr;
    expr.range = merge(begin.range, module_.exprs[else_expr.value].range);
    expr.condition = condition;
    expr.then_expr = then_expr;
    expr.else_expr = else_expr;
    return module_.push_expr(std::move(expr));
}

syntax::ExprId Parser::parse_match_expr() {
    const syntax::Token& begin = expect(TokenKind::kw_match, "expected 'match'");
    const bool previous_struct_literal_mode = allow_struct_literal_;
    allow_struct_literal_ = false;
    const syntax::ExprId value = parse_expr();
    allow_struct_literal_ = previous_struct_literal_mode;
    expect(TokenKind::l_brace, "expected '{' after match value");

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::match_expr;
    expr.match_value = value;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        const syntax::PatternId pattern = parse_pattern();
        syntax::ExprId guard = syntax::invalid_expr_id;
        if (match(TokenKind::kw_if)) {
            guard = parse_expr();
        }
        expect(TokenKind::fat_arrow, "expected '=>' after match case");
        const syntax::ExprId arm_value = parse_expr();
        base::SourceRange pattern_range = {};
        if (syntax::is_valid(pattern) && pattern.value < module_.patterns.size()) {
            pattern_range = module_.patterns[pattern.value].range;
        }
        base::SourceRange arm_range = syntax::is_valid(arm_value)
            ? merge(pattern_range, module_.exprs[arm_value.value].range)
            : pattern_range;
        expr.match_arms.push_back(syntax::MatchArm {
            pattern,
            guard,
            arm_value,
            arm_range,
        });
        if (check(TokenKind::r_brace)) {
            break;
        }
        expect(TokenKind::comma, "expected ',' after match arm");
        panic_ = false;
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after match expression");
    expr.range = merge(begin.range, end.range);
    panic_ = false;
    return module_.push_expr(std::move(expr));
}

syntax::PatternId Parser::parse_pattern() {
    const syntax::PatternId first = parse_pattern_atom();
    if (!match(TokenKind::pipe)) {
        return first;
    }
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::or_pattern;
    pattern.alternatives.push_back(first);
    base::SourceRange range = syntax::is_valid(first) && first.value < module_.patterns.size()
        ? module_.patterns[first.value].range
        : previous().range;
    do {
        const syntax::PatternId alternative = parse_pattern_atom();
        pattern.alternatives.push_back(alternative);
        if (syntax::is_valid(alternative) && alternative.value < module_.patterns.size()) {
            range = merge(range, module_.patterns[alternative.value].range);
        }
    } while (match(TokenKind::pipe));
    pattern.range = range;
    return module_.push_pattern(pattern);
}

syntax::PatternId Parser::parse_pattern_atom() {
    if (match(TokenKind::identifier)) {
        const syntax::Token& first = previous();
        if (first.text == "_") {
            syntax::PatternNode pattern;
            pattern.kind = syntax::PatternKind::wildcard;
            pattern.range = first.range;
            return module_.push_pattern(pattern);
        }
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::enum_case;
        pattern.case_name = first.text;
        pattern.range = first.range;
        if (match(TokenKind::dot)) {
            const syntax::Token& case_name = expect(TokenKind::identifier, "expected enum case name after '.'");
            pattern.enum_name = first.text;
            pattern.case_name = case_name.text;
            pattern.scoped = true;
            pattern.range = merge(first.range, case_name.range);
        }
        if (match(TokenKind::l_paren)) {
            const syntax::Token& binding = expect(TokenKind::identifier, "expected payload binding name");
            if (binding.kind == TokenKind::identifier) {
                pattern.binding_name = binding.text;
            }
            const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after payload binding");
            pattern.range = merge(pattern.range, end.range);
        }
        return module_.push_pattern(pattern);
    }
    if (match(TokenKind::integer_literal) || match(TokenKind::kw_true) || match(TokenKind::kw_false)) {
        const syntax::Token& token = previous();
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::literal;
        pattern.case_name = token.text;
        pattern.range = token.range;
        return module_.push_pattern(pattern);
    }
    if (match(TokenKind::dot)) {
        const syntax::Token& dot = previous();
        const syntax::Token& case_name = expect(TokenKind::identifier, "expected enum case name after '.'");
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::enum_case;
        pattern.case_name = case_name.text;
        pattern.scoped = true;
        pattern.range = merge(dot.range, case_name.range);
        if (match(TokenKind::l_paren)) {
            const syntax::Token& binding = expect(TokenKind::identifier, "expected payload binding name");
            if (binding.kind == TokenKind::identifier) {
                pattern.binding_name = binding.text;
            }
            const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after payload binding");
            pattern.range = merge(pattern.range, end.range);
        }
        return module_.push_pattern(pattern);
    }
    report_here("expected match pattern");
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::wildcard;
    pattern.range = peek().range;
    advance();
    return module_.push_pattern(pattern);
}

syntax::ExprId Parser::parse_logical_or() {
    syntax::ExprId expr = parse_logical_and();
    while (match(TokenKind::pipe_pipe)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_logical_and();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_logical_and() {
    syntax::ExprId expr = parse_bit_or();
    while (match(TokenKind::amp_amp)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_bit_or();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_bit_or() {
    syntax::ExprId expr = parse_bit_xor();
    while (match(TokenKind::pipe)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_bit_xor();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_bit_xor() {
    syntax::ExprId expr = parse_bit_and();
    while (match(TokenKind::caret)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_bit_and();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_bit_and() {
    syntax::ExprId expr = parse_equality();
    while (match(TokenKind::amp)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_equality();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_equality() {
    syntax::ExprId expr = parse_compare();
    while (match(TokenKind::equal_equal) || match(TokenKind::bang_equal)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_compare();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_compare() {
    syntax::ExprId expr = parse_shift();
    while (match(TokenKind::less) || match(TokenKind::less_equal) || match(TokenKind::greater) || match(TokenKind::greater_equal)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_shift();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_shift() {
    syntax::ExprId expr = parse_add();
    while (match(TokenKind::less_less) || match(TokenKind::greater_greater)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_add();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_add() {
    syntax::ExprId expr = parse_mul();
    while (match(TokenKind::plus) || match(TokenKind::minus)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_mul();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_mul() {
    syntax::ExprId expr = parse_unary();
    while (match(TokenKind::star) || match(TokenKind::slash) || match(TokenKind::percent)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_unary();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_unary() {
    if (match(TokenKind::bang) || match(TokenKind::minus) || match(TokenKind::tilde) || match(TokenKind::amp) || match(TokenKind::star)) {
        const syntax::Token& op = previous();
        const syntax::ExprId operand = parse_unary();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::unary;
        expr.range = merge(op.range, module_.exprs[operand.value].range);
        expr.text = op.text;
        switch (op.kind) {
        case TokenKind::bang:
            expr.unary_op = syntax::UnaryOp::logical_not;
            break;
        case TokenKind::minus:
            expr.unary_op = syntax::UnaryOp::numeric_negate;
            break;
        case TokenKind::tilde:
            expr.unary_op = syntax::UnaryOp::bitwise_not;
            break;
        case TokenKind::amp:
            expr.unary_op = syntax::UnaryOp::address_of;
            break;
        case TokenKind::star:
            expr.unary_op = syntax::UnaryOp::dereference;
            break;
        default:
            break;
        }
        expr.unary_operand = operand;
        return module_.push_expr(std::move(expr));
    }
    return parse_postfix();
}

syntax::ExprId Parser::parse_postfix() {
    syntax::ExprId expr = parse_primary();
    while (true) {
        if (next_angle_list_is_type_scope() && match(TokenKind::less)) {
            if (!syntax::is_valid(expr) || expr.value >= module_.exprs.size()) {
                continue;
            }
            syntax::ExprNode& node = module_.exprs[expr.value];
            if (node.kind != syntax::ExprKind::name) {
                report_at(previous(), "type arguments are only supported on named enum constructors");
            }
            if (!check(TokenKind::greater)) {
                do {
                    node.type_args.push_back(parse_type());
                    panic_ = false;
                    if (check(TokenKind::greater)) {
                        break;
                    }
                } while (match(TokenKind::comma) && !check(TokenKind::greater));
            }
            const syntax::Token& end = expect(TokenKind::greater, "expected '>' after type argument list");
            node.range = merge(node.range, end.range);
        } else if (match(TokenKind::dot)) {
            const syntax::Token& field = expect(TokenKind::identifier, "expected field name after '.'");
            syntax::ExprNode node;
            node.kind = syntax::ExprKind::field;
            node.range = merge(module_.exprs[expr.value].range, field.range);
            node.object = expr;
            node.field_name = field.text;
            expr = module_.push_expr(std::move(node));
        } else if (match(TokenKind::l_bracket)) {
            const syntax::ExprId index = parse_expr();
            const syntax::Token& end = expect(TokenKind::r_bracket, "expected ']' after index");
            syntax::ExprNode node;
            node.kind = syntax::ExprKind::index;
            node.range = merge(module_.exprs[expr.value].range, end.range);
            node.object = expr;
            node.index = index;
            expr = module_.push_expr(std::move(node));
        } else if (match(TokenKind::l_paren)) {
            syntax::ExprNode node;
            node.kind = syntax::ExprKind::call;
            node.callee = expr;
            if (!check(TokenKind::r_paren)) {
                do {
                    node.args.push_back(parse_expr());
                    if (check(TokenKind::r_paren)) {
                        break;
                    }
                } while (match(TokenKind::comma) && !check(TokenKind::r_paren));
            }
            const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after argument list");
            node.range = merge(module_.exprs[expr.value].range, end.range);
            expr = module_.push_expr(std::move(node));
        } else if (match(TokenKind::question)) {
            const syntax::Token& question = previous();
            syntax::ExprNode node;
            node.kind = syntax::ExprKind::try_expr;
            node.unary_operand = expr;
            node.range = syntax::is_valid(expr) && expr.value < module_.exprs.size()
                ? merge(module_.exprs[expr.value].range, question.range)
                : question.range;
            expr = module_.push_expr(std::move(node));
        } else {
            break;
        }
        panic_ = false;
    }
    return expr;
}

syntax::ExprId Parser::parse_primary() {
    if (match(TokenKind::identifier)) {
        const syntax::Token& name = previous();
        std::vector<syntax::TypeId> struct_type_args;
        if (allow_struct_literal_ && next_angle_list_is_struct_literal()) {
            struct_type_args = parse_type_arg_list();
        }
        if (allow_struct_literal_ && check(TokenKind::l_brace)) {
            advance();
            syntax::ExprNode node;
            node.kind = syntax::ExprKind::struct_literal;
            node.struct_name = name.text;
            node.struct_type_args = std::move(struct_type_args);
            node.range = name.range;
            if (!check(TokenKind::r_brace)) {
                do {
                    const syntax::Token& field = expect(TokenKind::identifier, "expected field name in struct literal");
                    expect(TokenKind::colon, "expected ':' after field name");
                    const syntax::ExprId value = parse_expr();
                    node.field_inits.push_back(syntax::FieldInit {field.text, value, merge(field.range, module_.exprs[value.value].range)});
                    if (check(TokenKind::r_brace)) {
                        break;
                    }
                } while (match(TokenKind::comma) && !check(TokenKind::r_brace));
            }
            const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after struct literal");
            node.range = merge(name.range, end.range);
            return module_.push_expr(std::move(node));
        }
        if (!struct_type_args.empty()) {
            report_at(name, "type arguments in expressions are only supported on struct literals or scoped enum constructors");
        }
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::name;
        expr.range = name.range;
        expr.text = name.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::integer_literal)) {
        const syntax::Token& token = previous();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::integer_literal;
        expr.range = token.range;
        expr.text = token.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::kw_true) || match(TokenKind::kw_false)) {
        const syntax::Token& token = previous();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::bool_literal;
        expr.range = token.range;
        expr.text = token.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::kw_null)) {
        const syntax::Token& token = previous();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::null_literal;
        expr.range = token.range;
        expr.text = token.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::string_literal)) {
        const syntax::Token& token = previous();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::string_literal;
        expr.range = token.range;
        expr.text = token.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::c_string_literal)) {
        const syntax::Token& token = previous();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::c_string_literal;
        expr.range = token.range;
        expr.text = token.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::byte_literal)) {
        const syntax::Token& token = previous();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::byte_literal;
        expr.range = token.range;
        expr.text = token.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::l_paren)) {
        const syntax::ExprId expr = parse_expr();
        expect(TokenKind::r_paren, "expected ')' after expression");
        return expr;
    }
    if (check(TokenKind::l_brace)) {
        return parse_block_expr();
    }
    if (match(TokenKind::kw_cast)) {
        return parse_builtin_cast(syntax::ExprKind::cast);
    }
    if (match(TokenKind::kw_ptr_cast)) {
        return parse_builtin_cast(syntax::ExprKind::ptr_cast);
    }
    if (match(TokenKind::kw_bit_cast)) {
        return parse_builtin_cast(syntax::ExprKind::bit_cast);
    }
    if (match(TokenKind::kw_size_of)) {
        return parse_type_builtin(syntax::ExprKind::size_of);
    }
    if (match(TokenKind::kw_align_of)) {
        return parse_type_builtin(syntax::ExprKind::align_of);
    }
    if (match(TokenKind::kw_ptr_addr)) {
        const syntax::Token& begin = previous();
        expect(TokenKind::l_paren, "expected '(' after ptr_addr");
        const syntax::ExprId value = parse_expr();
        const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after ptr_addr argument");
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::ptr_addr;
        expr.range = merge(begin.range, end.range);
        expr.cast_expr = value;
        return module_.push_expr(std::move(expr));
    }
    if (match(TokenKind::kw_ptr_from_addr)) {
        const syntax::Token& begin = previous();
        expect(TokenKind::l_paren, "expected '(' after ptr_from_addr");
        const syntax::TypeId type = parse_type();
        expect(TokenKind::comma, "expected ',' after ptr_from_addr type");
        const syntax::ExprId value = parse_expr();
        const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after ptr_from_addr argument");
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::ptr_from_addr;
        expr.range = merge(begin.range, end.range);
        expr.cast_type = type;
        expr.cast_expr = value;
        return module_.push_expr(std::move(expr));
    }

    report_here("expected expression");
    return make_invalid_expr();
}

syntax::ExprId Parser::parse_builtin_cast(const syntax::ExprKind kind) {
    const syntax::Token& begin = previous();
    expect(TokenKind::l_paren, "expected '(' after cast builtin");
    const syntax::TypeId type = parse_type();
    expect(TokenKind::comma, "expected ',' after cast type");
    const syntax::ExprId value = parse_expr();
    const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after cast expression");

    syntax::ExprNode expr;
    expr.kind = kind;
    expr.range = merge(begin.range, end.range);
    expr.cast_type = type;
    expr.cast_expr = value;
    return module_.push_expr(std::move(expr));
}

syntax::ExprId Parser::parse_type_builtin(const syntax::ExprKind kind) {
    const syntax::Token& begin = previous();
    expect(TokenKind::l_paren, "expected '(' after type builtin");
    const syntax::TypeId type = parse_type();
    const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after type builtin");

    syntax::ExprNode expr;
    expr.kind = kind;
    expr.range = merge(begin.range, end.range);
    expr.cast_type = type;
    return module_.push_expr(std::move(expr));
}

syntax::ExprId Parser::make_binary(const syntax::BinaryOp op, const syntax::ExprId lhs, const syntax::ExprId rhs, base::SourceRange range) {
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::binary;
    expr.range = range;
    expr.binary_op = op;
    expr.binary_lhs = lhs;
    expr.binary_rhs = rhs;
    return module_.push_expr(std::move(expr));
}

syntax::ExprId Parser::make_invalid_expr() {
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::invalid;
    expr.range = peek().range;
    if (!is_eof()) {
        advance();
    }
    return module_.push_expr(expr);
}

} // namespace aurex::parse
