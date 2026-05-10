#include <aurex/parse/parser_postfix_expr_part.hpp>

#include <aurex/parse/parser_primary_expr_part.hpp>
#include <aurex/parse/recovery.hpp>

#include <optional>
#include <utility>
#include <vector>

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
    if (this->next_angle_list_is_type_scope() && this->check(TokenKind::less)) {
        return this->parse_type_args_suffix(expr);
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

syntax::ExprId PostfixExprParser::parse_type_args_suffix(const syntax::ExprId expr) {
    const syntax::Token& begin = this->peek();
    std::vector<syntax::TypeId> type_args = this->parse_type_arg_list();
    if (!syntax::is_valid(expr) || expr.value >= this->session_.module.exprs.size()) {
        return expr;
    }
    syntax::ExprNode& node = this->session_.module.exprs[expr.value];
    if (node.kind != syntax::ExprKind::name && node.kind != syntax::ExprKind::field) {
        this->report_at(begin, "type arguments are only supported on named function calls, methods, or enum constructors");
    }
    node.type_args.insert(node.type_args.end(), type_args.begin(), type_args.end());
    node.range = this->merge(node.range, this->previous().range);
    return expr;
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
    const ExprContext context
) {
    while (!this->is_eof() && !this->check(TokenKind::r_paren)) {
        args.push_back(this->parse_expr(context));
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
