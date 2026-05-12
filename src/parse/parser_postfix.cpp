#include <aurex/parse/parser_postfix_expr_part.hpp>

#include <aurex/parse/parser_primary_expr_part.hpp>
#include <aurex/parse/recovery.hpp>

#include <optional>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ExprId PostfixExprParser::parse_postfix(const ExprContext context) {
    syntax::ExprId expr = PrimaryExprParser(this->parser_).parse_primary(context);
    while (std::optional<syntax::ExprId> next = this->parse_next_suffix(expr, context)) {
        expr = next.value();
        this->reset_panic();
    }
    return expr;
}

std::optional<syntax::ExprId> PostfixExprParser::parse_next_suffix(
    const syntax::ExprId expr,
    const ExprContext context
) {
    if (this->check(TokenKind::colon_colon) && this->peek_at(1).kind == TokenKind::l_bracket) {
        return this->parse_generic_apply_suffix(expr);
    }
    if (this->match(TokenKind::dot)) {
        return this->parse_field_suffix(expr);
    }
    if (this->match(TokenKind::l_bracket)) {
        return this->parse_index_suffix(expr, context);
    }
    if (this->match(TokenKind::l_paren)) {
        return this->parse_call_suffix(expr, context);
    }
    if (this->match(TokenKind::question)) {
        return this->parse_try_suffix(expr);
    }
    if (this->check(TokenKind::plus_plus)) {
        return this->parse_rejected_update_suffix(
            expr,
            TokenKind::plus_plus,
            "increment operator is not supported; use '+= 1'"
        );
    }
    if (this->check(TokenKind::minus_minus)) {
        return this->parse_rejected_update_suffix(
            expr,
            TokenKind::minus_minus,
            "decrement operator is not supported; use '-= 1'"
        );
    }
    return std::nullopt;
}

syntax::ExprId PostfixExprParser::parse_generic_apply_suffix(const syntax::ExprId expr) {
    const syntax::Token& begin = this->expect(TokenKind::colon_colon, "expected '::' before generic type arguments");
    this->expect_recovered(
        TokenKind::l_bracket,
        "expected '[' after '::'",
        RecoveryContext::generic_type_argument
    );
    std::vector<syntax::TypeId> type_args;
    if (this->check(TokenKind::r_bracket)) {
        this->report_here("expected generic type argument");
    }
    this->parse_generic_type_args(type_args);
    const syntax::Token& end = this->expect_recovered(
        TokenKind::r_bracket,
        "expected ']' after generic type arguments",
        RecoveryContext::generic_type_argument
    );

    syntax::ExprNode node;
    node.kind = syntax::ExprKind::generic_apply;
    node.range = this->merge(this->expr_range_or(expr, begin.range), end.range);
    node.callee = expr;
    node.type_args = std::move(type_args);
    if (syntax::is_valid(expr) && expr.value < this->session_.module.exprs.size()) {
        const syntax::ExprNode& callee = this->session_.module.exprs[expr.value];
        if (callee.kind == syntax::ExprKind::name) {
            return this->session_.module.push_expr(std::move(node));
        } else {
            this->report_at(begin, "explicit generic arguments require a function name");
            node.kind = syntax::ExprKind::invalid;
        }
    }
    return this->session_.module.push_expr(std::move(node));
}

void PostfixExprParser::parse_generic_type_args(std::vector<syntax::TypeId>& args) {
    while (!this->is_eof() && !this->check(TokenKind::r_bracket)) {
        args.push_back(this->parse_type());
        this->reset_panic();
        if (!this->recover_generic_type_arg_separator()) {
            break;
        }
    }
}

bool PostfixExprParser::recover_generic_type_arg_separator() {
    if (this->check(TokenKind::r_bracket)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }

    this->report_here("expected ',' or ']' after generic type argument");
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::generic_type_argument)) {
        this->synchronize(RecoveryContext::generic_type_argument);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }
    this->reset_panic();
    return false;
}

syntax::ExprId PostfixExprParser::parse_field_suffix(const syntax::ExprId expr) {
    const syntax::Token& field = this->expect_identifier_recovered("expected field name after '.'");
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::field;
    node.range = this->merge(this->expr_range_or(expr, field.range), field.range);
    node.object = expr;
    node.field_name = field.text;
    return this->session_.module.push_expr(std::move(node));
}

syntax::ExprId PostfixExprParser::parse_index_suffix(const syntax::ExprId expr, const ExprContext context) {
    const syntax::ExprId index = this->parse_expr(context);
    const syntax::Token& end = this->expect_index_suffix_end();
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::index;
    node.range = this->merge(this->expr_range_or(expr, end.range), end.range);
    node.object = expr;
    node.index = index;
    return this->session_.module.push_expr(std::move(node));
}

const syntax::Token& PostfixExprParser::expect_index_suffix_end() {
    return this->expect_recovered(
        TokenKind::r_bracket,
        "expected ']' after index",
        RecoveryContext::index_expression
    );
}

syntax::ExprId PostfixExprParser::parse_call_suffix(const syntax::ExprId expr, const ExprContext context) {
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::call;
    node.callee = expr;
    this->parse_call_args(node.args, context);
    const syntax::Token& end = this->expect_recovered(
        TokenKind::r_paren,
        "expected ')' after argument list",
        RecoveryContext::call_argument
    );
    node.range = this->merge(this->expr_range_or(expr, end.range), end.range);
    return this->session_.module.push_expr(std::move(node));
}

void PostfixExprParser::parse_call_args(
    std::vector<syntax::ExprId>& args,
    const ExprContext
) {
    while (!this->is_eof() && !this->check(TokenKind::r_paren)) {
        args.push_back(this->parse_expr(ExprContext::normal));
        this->reset_panic();
        if (!this->recover_call_arg_separator()) {
            break;
        }
    }
}

bool PostfixExprParser::recover_call_arg_separator() {
    if (this->check(TokenKind::r_paren)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }

    this->report_here("expected ',' or ')' after argument");
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::call_argument)) {
        this->synchronize(RecoveryContext::call_argument);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }
    this->reset_panic();
    return false;
}

syntax::ExprId PostfixExprParser::parse_try_suffix(const syntax::ExprId expr) {
    const syntax::Token& question = this->previous();
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::try_expr;
    node.unary_operand = expr;
    node.range = this->merge(this->expr_range_or(expr, question.range), question.range);
    return this->session_.module.push_expr(std::move(node));
}

syntax::ExprId PostfixExprParser::parse_rejected_update_suffix(
    const syntax::ExprId expr,
    const TokenKind kind,
    std::string message
) {
    const syntax::Token& op = this->expect(kind, "expected unsupported update operator");
    this->report_at(op, std::move(message));

    syntax::ExprNode node;
    node.kind = syntax::ExprKind::invalid;
    node.range = this->merge(this->expr_range_or(expr, op.range), op.range);
    return this->session_.module.push_expr(std::move(node));
}

} // namespace aurex::parse
