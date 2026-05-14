#include <aurex/parse/parser_postfix_expr_part.hpp>

#include <aurex/parse/parser_messages.hpp>
#include <aurex/parse/parser_primary_expr_part.hpp>
#include <aurex/parse/recovery.hpp>

#include <optional>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

constexpr base::usize PARSER_TUPLE_FIELD_DOT_PREFIX_LENGTH = 1;
constexpr char PARSER_TUPLE_FIELD_DOT = '.';
constexpr char PARSER_TUPLE_FIELD_FIRST_DIGIT = '0';
constexpr char PARSER_TUPLE_FIELD_LAST_DIGIT = '9';

[[nodiscard]] bool is_leading_dot_numeric_field_token(const syntax::Token& token) noexcept {
    if (token.kind != TokenKind::float_literal ||
        token.text.size() <= PARSER_TUPLE_FIELD_DOT_PREFIX_LENGTH ||
        token.text.front() != PARSER_TUPLE_FIELD_DOT) {
        return false;
    }
    for (const char c : token.text.substr(PARSER_TUPLE_FIELD_DOT_PREFIX_LENGTH)) {
        if (c < PARSER_TUPLE_FIELD_FIRST_DIGIT || c > PARSER_TUPLE_FIELD_LAST_DIGIT) {
            return false;
        }
    }
    return true;
}

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
    if (context == ExprContext::normal && this->check(TokenKind::l_brace)) {
        return this->parse_struct_literal_suffix(expr, context);
    }
    if (this->check(TokenKind::l_bracket) && this->bracketed_type_args_are_postfix_generic_apply(expr)) {
        return this->parse_generic_apply_suffix(expr);
    }
    if (this->match(TokenKind::dot)) {
        return this->parse_field_suffix(expr);
    }
    if (this->check(TokenKind::colon_colon)) {
        return this->parse_rejected_legacy_scope_suffix(expr);
    }
    if (is_leading_dot_numeric_field_token(this->peek())) {
        return this->parse_rejected_numeric_tuple_field_suffix(expr);
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
            std::string(PARSER_INCREMENT_UNSUPPORTED)
        );
    }
    if (this->check(TokenKind::minus_minus)) {
        return this->parse_rejected_update_suffix(
            expr,
            TokenKind::minus_minus,
            std::string(PARSER_DECREMENT_UNSUPPORTED)
        );
    }
    return std::nullopt;
}

bool PostfixExprParser::bracketed_type_args_are_postfix_generic_apply(
    const syntax::ExprId expr
) const noexcept {
    if (!syntax::is_valid(expr) || expr.value >= this->session_.module.exprs.size()) {
        return false;
    }
    const syntax::ExprKind kind = this->session_.module.exprs[expr.value].kind;
    if (kind != syntax::ExprKind::name && kind != syntax::ExprKind::field) {
        return false;
    }
    base::usize depth = 0;
    for (base::usize offset = 0; true; ++offset) {
        const syntax::Token& token = this->peek_at(offset);
        if (token.kind == TokenKind::eof) {
            return false;
        }
        if (token.kind == TokenKind::l_bracket) {
            ++depth;
            continue;
        }
        if (token.kind != TokenKind::r_bracket) {
            continue;
        }
        if (depth == 0) {
            return false;
        }
        --depth;
        if (depth != 0) {
            continue;
        }
        const TokenKind next = this->peek_at(offset + 1).kind;
        return next == TokenKind::dot || next == TokenKind::l_paren || next == TokenKind::l_brace;
    }
}

syntax::ExprId PostfixExprParser::parse_generic_apply_suffix(const syntax::ExprId expr) {
    const syntax::Token& begin = this->expect_recovered(
        TokenKind::l_bracket,
        std::string(PARSER_EXPECT_GENERIC_APPLY_START),
        RecoveryContext::generic_type_argument
    );
    std::vector<syntax::TypeId> type_args;
    if (this->check(TokenKind::r_bracket)) {
        this->report_here(std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT));
    }
    this->parse_generic_type_args(type_args);
    const syntax::Token& end = this->expect_recovered(
        TokenKind::r_bracket,
        std::string(PARSER_EXPECT_GENERIC_TYPE_ARGS_END),
        RecoveryContext::generic_type_argument
    );

    syntax::ExprNode node;
    node.kind = syntax::ExprKind::generic_apply;
    node.range = this->merge(this->expr_range_or(expr, begin.range), end.range);
    node.callee = expr;
    node.type_args = std::move(type_args);
    if (syntax::is_valid(expr) && expr.value < this->session_.module.exprs.size()) {
        const syntax::ExprNode& callee = this->session_.module.exprs[expr.value];
        if (callee.kind == syntax::ExprKind::name || callee.kind == syntax::ExprKind::field) {
            return this->session_.module.push_expr(std::move(node));
        } else {
            this->report_at(begin, std::string(PARSER_M2_EXPLICIT_GENERIC_CALL_SYNTAX));
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

    this->report_here(std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT_SEPARATOR));
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
    if (this->check(TokenKind::integer_literal)) {
        return this->parse_rejected_numeric_tuple_field_suffix(expr);
    }
    const syntax::Token& field = this->expect_identifier_recovered(std::string(PARSER_EXPECT_FIELD_AFTER_DOT));
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::field;
    node.range = this->merge(this->expr_range_or(expr, field.range), field.range);
    node.object = expr;
    node.field_name = field.text;
    return this->session_.module.push_expr(std::move(node));
}

syntax::ExprId PostfixExprParser::parse_rejected_legacy_scope_suffix(const syntax::ExprId expr) {
    const syntax::Token& scope = this->advance();
    this->report_at(scope, std::string(PARSER_DOT_ONLY_SELECTOR));
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::invalid;
    node.range = this->merge(this->expr_range_or(expr, scope.range), scope.range);
    return this->session_.module.push_expr(node);
}

syntax::ExprId PostfixExprParser::parse_struct_literal_suffix(
    const syntax::ExprId type_expr,
    const ExprContext context
) {
    const syntax::Token& begin = this->expect(TokenKind::l_brace, std::string(PARSER_EXPECT_STRUCT_LITERAL_END));
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::struct_literal;
    node.object = type_expr;
    node.range = this->merge(this->expr_range_or(type_expr, begin.range), begin.range);
    this->parse_struct_fields(node.field_inits, context);
    const syntax::Token& end = this->expect_recovered(
        TokenKind::r_brace,
        std::string(PARSER_EXPECT_STRUCT_LITERAL_END),
        RecoveryContext::struct_field
    );
    node.range = this->merge(node.range, end.range);
    return this->session_.module.push_expr(std::move(node));
}

void PostfixExprParser::parse_struct_fields(
    std::vector<syntax::FieldInit>& fields,
    const ExprContext context
) {
    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        fields.push_back(this->parse_struct_field(context));
        this->reset_panic();
        if (!this->recover_struct_field_separator()) {
            break;
        }
    }
}

syntax::FieldInit PostfixExprParser::parse_struct_field(const ExprContext context) {
    const syntax::Token& field =
        this->expect_identifier_recovered(std::string(PARSER_EXPECT_STRUCT_LITERAL_FIELD));
    this->expect_type_annotation_colon(std::string(PARSER_EXPECT_FIELD_TYPE_COLON));
    const syntax::ExprId value = this->parse_expr(context);
    return syntax::FieldInit {
        field.text,
        value,
        this->merge(field.range, this->expr_range_or(value, field.range)),
    };
}

bool PostfixExprParser::recover_struct_field_separator() {
    if (this->check(TokenKind::r_brace)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_brace);
    }

    this->report_here(std::string(PARSER_EXPECT_STRUCT_LITERAL_FIELD_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::struct_field)) {
        this->synchronize(RecoveryContext::struct_field);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_brace);
    }
    this->reset_panic();
    return token_starts_struct_field(this->peek().kind);
}

syntax::ExprId PostfixExprParser::parse_rejected_numeric_tuple_field_suffix(const syntax::ExprId expr) {
    const syntax::Token& field = this->advance();
    this->report_at(field, std::string(PARSER_TUPLE_FIELD_ACCESS_UNSUPPORTED));
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::invalid;
    node.range = this->merge(this->expr_range_or(expr, field.range), field.range);
    return this->session_.module.push_expr(std::move(node));
}

syntax::ExprId PostfixExprParser::parse_index_suffix(const syntax::ExprId expr, const ExprContext context) {
    syntax::ExprId first = syntax::INVALID_EXPR_ID;
    if (!this->check(TokenKind::colon)) {
        first = this->parse_expr(context);
    }
    if (this->match(TokenKind::colon)) {
        syntax::ExprId end_bound = syntax::INVALID_EXPR_ID;
        if (!this->check(TokenKind::r_bracket)) {
            end_bound = this->parse_expr(context);
        }
        const syntax::Token& end = this->expect_slice_suffix_end();
        syntax::ExprNode node;
        node.kind = syntax::ExprKind::slice;
        node.range = this->merge(this->expr_range_or(expr, end.range), end.range);
        node.object = expr;
        node.slice_start = first;
        node.slice_end = end_bound;
        return this->session_.module.push_expr(std::move(node));
    }
    const syntax::Token& end = this->expect_index_suffix_end();
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::index;
    node.range = this->merge(this->expr_range_or(expr, end.range), end.range);
    node.object = expr;
    node.index = first;
    return this->session_.module.push_expr(std::move(node));
}

const syntax::Token& PostfixExprParser::expect_index_suffix_end() {
    return this->expect_recovered(
        TokenKind::r_bracket,
        std::string(PARSER_EXPECT_INDEX_END),
        RecoveryContext::index_expression
    );
}

const syntax::Token& PostfixExprParser::expect_slice_suffix_end() {
    return this->expect_recovered(
        TokenKind::r_bracket,
        std::string(PARSER_EXPECT_SLICE_END),
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
        std::string(PARSER_EXPECT_CALL_ARGUMENTS_END),
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

    this->report_here(std::string(PARSER_EXPECT_CALL_ARGUMENT_SEPARATOR));
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
    const syntax::Token& op = this->expect(kind, std::string(PARSER_EXPECT_UNSUPPORTED_UPDATE));
    this->report_at(op, std::move(message));

    syntax::ExprNode node;
    node.kind = syntax::ExprKind::invalid;
    node.range = this->merge(this->expr_range_or(expr, op.range), op.range);
    return this->session_.module.push_expr(std::move(node));
}

} // namespace aurex::parse
