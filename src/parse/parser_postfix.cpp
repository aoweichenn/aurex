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

[[nodiscard]] bool is_primitive_type_token(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::kw_void:
    case TokenKind::kw_bool:
    case TokenKind::kw_i8:
    case TokenKind::kw_u8:
    case TokenKind::kw_i16:
    case TokenKind::kw_u16:
    case TokenKind::kw_i32:
    case TokenKind::kw_u32:
    case TokenKind::kw_i64:
    case TokenKind::kw_u64:
    case TokenKind::kw_isize:
    case TokenKind::kw_usize:
    case TokenKind::kw_f32:
    case TokenKind::kw_f64:
    case TokenKind::kw_str:
    case TokenKind::kw_char:
        return true;
    default:
        return false;
    }
}

} // namespace

syntax::ExprId PostfixExprParser::parse_postfix(const ExprContext context) {
    const syntax::ExprId base = PrimaryExprParser(this->parser_).parse_primary(context);
    syntax::AstArenaVector<syntax::PostfixOp> ops = this->session_.module.make_expr_list<syntax::PostfixOp>();
    while (std::optional<syntax::PostfixOp> next = this->parse_next_suffix(context)) {
        ops.push_back(std::move(next.value()));
        this->reset_panic();
    }
    if (ops.empty()) {
        return base;
    }

    const base::SourceRange last_op_range = ops.back().range;
    return this->session_.module.push_postfix_chain_expr(
        this->merge(this->expr_range_or(base, last_op_range), last_op_range),
        base,
        std::move(ops)
    );
}

std::optional<syntax::PostfixOp> PostfixExprParser::parse_next_suffix(const ExprContext context) {
    if (context == ExprContext::normal && this->check(TokenKind::l_brace)) {
        return this->parse_struct_literal_suffix(context);
    }
    if (this->match(TokenKind::l_bracket)) {
        return this->parse_bracket_suffix(context);
    }
    if (this->match(TokenKind::dot)) {
        return this->parse_field_suffix();
    }
    if (this->check(TokenKind::colon_colon)) {
        return this->parse_rejected_legacy_scope_suffix(this->peek().range);
    }
    if (is_leading_dot_numeric_field_token(this->peek())) {
        return this->parse_rejected_numeric_tuple_field_suffix(this->peek().range);
    }
    if (this->match(TokenKind::l_paren)) {
        return this->parse_call_suffix(context);
    }
    if (this->match(TokenKind::question)) {
        return this->parse_try_suffix();
    }
    if (this->check(TokenKind::plus_plus)) {
        return this->parse_rejected_update_suffix(
            this->peek().range,
            TokenKind::plus_plus,
            std::string(PARSER_INCREMENT_UNSUPPORTED)
        );
    }
    if (this->check(TokenKind::minus_minus)) {
        return this->parse_rejected_update_suffix(
            this->peek().range,
            TokenKind::minus_minus,
            std::string(PARSER_DECREMENT_UNSUPPORTED)
        );
    }
    return std::nullopt;
}

syntax::PostfixOp PostfixExprParser::parse_bracket_suffix(const ExprContext context) {
    const syntax::Token& begin = this->previous();
    syntax::PostfixOp op = this->session_.module.make_postfix_op();
    op.kind = syntax::PostfixOpKind::bracket;
    op.range = begin.range;

    if (this->check(TokenKind::r_bracket)) {
        this->report_here(std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT));
        const syntax::Token& end = this->expect_index_suffix_end();
        op.range = this->merge(begin.range, end.range);
        return op;
    }

    syntax::PostfixBracketArg first;
    if (!this->check(TokenKind::colon)) {
        first = this->parse_bracket_arg(context);
    }
    if (this->match(TokenKind::colon)) {
        op.bracket_is_slice = true;
        op.slice_start = first.expr;
        if (syntax::is_valid(first.type)) {
            this->report_here(std::string(PARSER_EXPECT_EXPRESSION));
        }
        if (!this->check(TokenKind::r_bracket)) {
            op.slice_end = this->parse_expr(context);
        }
        const syntax::Token& end = this->expect_slice_suffix_end();
        op.range = this->merge(begin.range, end.range);
        return op;
    }

    op.bracket_args.push_back(first);
    while (!this->is_eof() && !this->check(TokenKind::r_bracket)) {
        if (!this->recover_bracket_arg_separator()) {
            break;
        }
        op.bracket_args.push_back(this->parse_bracket_arg(context));
    }

    const syntax::Token& end = this->expect_index_suffix_end();
    op.range = this->merge(begin.range, end.range);
    return op;
}

syntax::PostfixBracketArg PostfixExprParser::parse_bracket_arg(const ExprContext context) {
    if (this->bracket_arg_starts_type_only()) {
        const syntax::TypeId type = this->parse_type();
        return syntax::PostfixBracketArg {
            syntax::INVALID_EXPR_ID,
            type,
            this->type_range_or(type, this->previous().range),
        };
    }

    const syntax::ExprId expr = this->parse_expr(context);
    return syntax::PostfixBracketArg {
        expr,
        syntax::INVALID_TYPE_ID,
        this->expr_range_or(expr, this->previous().range),
    };
}

bool PostfixExprParser::bracket_arg_starts_type_only() const noexcept {
    const TokenKind kind = this->peek().kind;
    if (is_primitive_type_token(kind) ||
        kind == TokenKind::kw_fn ||
        kind == TokenKind::kw_extern ||
        kind == TokenKind::kw_unsafe) {
        return true;
    }
    if (kind == TokenKind::star) {
        const TokenKind next = this->peek_at(1).kind;
        return next == TokenKind::kw_mut || next == TokenKind::kw_const;
    }
    if (kind == TokenKind::l_bracket) {
        const TokenKind next = this->peek_at(1).kind;
        return next == TokenKind::r_bracket || next == TokenKind::integer_literal;
    }
    if (kind == TokenKind::l_paren) {
        return this->bracket_parenthesized_arg_is_type();
    }
    return false;
}

bool PostfixExprParser::bracket_parenthesized_arg_is_type() const noexcept {
    base::usize depth = 0;
    bool saw_comma_at_outer_depth = false;
    for (base::usize offset = 0; true; ++offset) {
        const syntax::Token& token = this->peek_at(offset);
        if (token.kind == TokenKind::eof) {
            return false;
        }
        if (token.kind == TokenKind::l_paren) {
            ++depth;
            continue;
        }
        if (token.kind == TokenKind::r_paren) {
            if (depth == 0) {
                return false;
            }
            --depth;
            if (depth == 0) {
                return saw_comma_at_outer_depth;
            }
            continue;
        }
        if (token.kind == TokenKind::comma && depth == 1) {
            saw_comma_at_outer_depth = true;
        }
    }
}

bool PostfixExprParser::recover_bracket_arg_separator() {
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

syntax::PostfixOp PostfixExprParser::parse_field_suffix() {
    if (this->check(TokenKind::integer_literal)) {
        return this->parse_rejected_numeric_tuple_field_suffix(this->peek().range);
    }
    const syntax::Token& field = this->expect_identifier_recovered(std::string(PARSER_EXPECT_FIELD_AFTER_DOT));
    syntax::PostfixOp op = this->session_.module.make_postfix_op();
    op.kind = syntax::PostfixOpKind::select;
    op.name = field.text;
    op.range = field.range;
    return op;
}

syntax::PostfixOp PostfixExprParser::parse_rejected_legacy_scope_suffix(const base::SourceRange& fallback_range) {
    const syntax::Token& scope = this->advance();
    this->report_at(scope, std::string(PARSER_DOT_ONLY_SELECTOR));
    syntax::PostfixOp op = this->session_.module.make_postfix_op();
    op.kind = syntax::PostfixOpKind::select;
    op.range = this->merge(fallback_range, scope.range);
    return op;
}

syntax::PostfixOp PostfixExprParser::parse_struct_literal_suffix(const ExprContext context) {
    const syntax::Token& begin = this->expect(TokenKind::l_brace, std::string(PARSER_EXPECT_STRUCT_LITERAL_END));
    syntax::PostfixOp op = this->session_.module.make_postfix_op();
    op.kind = syntax::PostfixOpKind::struct_literal;
    op.range = begin.range;
    this->parse_struct_fields(op.field_inits, context);
    const syntax::Token& end = this->expect_recovered(
        TokenKind::r_brace,
        std::string(PARSER_EXPECT_STRUCT_LITERAL_END),
        RecoveryContext::struct_field
    );
    op.range = this->merge(begin.range, end.range);
    return op;
}

void PostfixExprParser::parse_struct_fields(
    syntax::AstArenaVector<syntax::FieldInit>& fields,
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

syntax::PostfixOp PostfixExprParser::parse_rejected_numeric_tuple_field_suffix(const base::SourceRange& fallback_range) {
    const syntax::Token& field = this->advance();
    this->report_at(field, std::string(PARSER_TUPLE_FIELD_ACCESS_UNSUPPORTED));
    syntax::PostfixOp op = this->session_.module.make_postfix_op();
    op.kind = syntax::PostfixOpKind::select;
    op.range = this->merge(fallback_range, field.range);
    return op;
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

syntax::PostfixOp PostfixExprParser::parse_call_suffix(const ExprContext context) {
    const syntax::Token& begin = this->previous();
    syntax::PostfixOp op = this->session_.module.make_postfix_op();
    op.kind = syntax::PostfixOpKind::call;
    this->parse_call_args(op.args, context);
    const syntax::Token& end = this->expect_recovered(
        TokenKind::r_paren,
        std::string(PARSER_EXPECT_CALL_ARGUMENTS_END),
        RecoveryContext::call_argument
    );
    op.range = this->merge(begin.range, end.range);
    return op;
}

void PostfixExprParser::parse_call_args(
    syntax::AstArenaVector<syntax::ExprId>& args,
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

syntax::PostfixOp PostfixExprParser::parse_try_suffix() const
{
    const syntax::Token& question = this->previous();
    syntax::PostfixOp op = this->session_.module.make_postfix_op();
    op.kind = syntax::PostfixOpKind::try_;
    op.range = question.range;
    return op;
}

syntax::PostfixOp PostfixExprParser::parse_rejected_update_suffix(
    const base::SourceRange& fallback_range,
    const TokenKind kind,
    std::string message
) {
    const syntax::Token& op_token = this->expect(kind, std::string(PARSER_EXPECT_UNSUPPORTED_UPDATE));
    this->report_at(op_token, std::move(message));

    syntax::PostfixOp op = this->session_.module.make_postfix_op();
    op.kind = syntax::PostfixOpKind::select;
    op.range = this->merge(fallback_range, op_token.range);
    return op;
}

} // namespace aurex::parse
