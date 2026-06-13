#include <aurex/frontend/parse/parser_messages.hpp>
#include <aurex/frontend/parse/parser_postfix_expr_part.hpp>
#include <aurex/frontend/parse/parser_primary_expr_part.hpp>
#include <aurex/frontend/parse/recovery.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

constexpr base::usize PARSER_TUPLE_FIELD_DOT_PREFIX_LENGTH = 1;
constexpr char PARSER_TUPLE_FIELD_DOT = '.';
constexpr char PARSER_TUPLE_FIELD_FIRST_DIGIT = '0';
constexpr char PARSER_TUPLE_FIELD_LAST_DIGIT = '9';
constexpr int PARSER_GENERIC_ANGLE_SINGLE_CLOSE_DEPTH = 1;
constexpr int PARSER_GENERIC_ANGLE_DOUBLE_CLOSE_DEPTH = 2;

[[nodiscard]] bool is_leading_dot_numeric_field_token(const syntax::Token& token) noexcept
{
    const std::string_view text = token.text();
    if (token.kind != TokenKind::float_literal || text.size() <= PARSER_TUPLE_FIELD_DOT_PREFIX_LENGTH
        || text.front() != PARSER_TUPLE_FIELD_DOT) {
        return false;
    }
    for (const char c : text.substr(PARSER_TUPLE_FIELD_DOT_PREFIX_LENGTH)) {
        if (c < PARSER_TUPLE_FIELD_FIRST_DIGIT || c > PARSER_TUPLE_FIELD_LAST_DIGIT) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool token_continues_generic_apply_suffix(const TokenKind kind, const ExprContext context) noexcept
{
    if (kind == TokenKind::l_paren || kind == TokenKind::dot) {
        return true;
    }
    return context == ExprContext::normal && kind == TokenKind::l_brace;
}

} // namespace

syntax::ExprId PostfixExprParser::parse_postfix(const ExprContext context)
{
    syntax::ExprId current = PrimaryExprParser(this->parser_).parse_primary(context);
    while (std::optional<syntax::ExprId> next = this->parse_next_suffix(current, context)) {
        current = next.value();
        this->reset_panic();
    }
    return current;
}

std::optional<syntax::ExprId> PostfixExprParser::parse_next_suffix(const syntax::ExprId base, const ExprContext context)
{
    if (context == ExprContext::normal && this->check(TokenKind::l_brace)) {
        return this->parse_struct_literal_suffix(base, context);
    }
    if (this->check_generic_left_angle()) {
        if (std::optional<syntax::ExprId> generic = this->parse_angle_generic_suffix(base, context)) {
            return generic.value();
        }
    }
    if (this->match(TokenKind::l_bracket)) {
        return this->parse_bracket_suffix(base, context);
    }
    if (this->match(TokenKind::dot)) {
        return this->parse_field_suffix(base);
    }
    if (this->check(TokenKind::colon_colon)) {
        return this->parse_rejected_legacy_scope_suffix(base, this->peek().range);
    }
    if (is_leading_dot_numeric_field_token(this->peek())) {
        return this->parse_numeric_tuple_field_suffix(base, this->peek().range);
    }
    if (this->match(TokenKind::l_paren)) {
        return this->parse_call_suffix(base, context);
    }
    if (this->match(TokenKind::question)) {
        return this->parse_try_suffix(base);
    }
    if (this->check(TokenKind::plus_plus)) {
        return this->parse_rejected_update_suffix(
            base, this->peek().range, TokenKind::plus_plus, std::string(PARSER_INCREMENT_UNSUPPORTED));
    }
    if (this->check(TokenKind::minus_minus)) {
        return this->parse_rejected_update_suffix(
            base, this->peek().range, TokenKind::minus_minus, std::string(PARSER_DECREMENT_UNSUPPORTED));
    }
    return std::nullopt;
}

syntax::ExprId PostfixExprParser::parse_bracket_suffix(const syntax::ExprId base, const ExprContext context)
{
    const syntax::Token& begin = this->previous();

    if (this->check(TokenKind::r_bracket)) {
        this->report_here(std::string(PARSER_EXPECT_EXPRESSION));
        const syntax::Token& end = this->expect_index_suffix_end(begin);
        const base::SourceRange range = this->merge(begin.range, end.range);
        return this->session_.module.push_index_expr(
            this->merge(this->expr_range_or(base, range), range), base, syntax::INVALID_EXPR_ID);
    }

    syntax::ExprId first = syntax::INVALID_EXPR_ID;
    if (!this->check(TokenKind::colon)) {
        first = this->parse_expr(context);
    }
    if (this->match(TokenKind::colon)) {
        syntax::ExprId end_expr = syntax::INVALID_EXPR_ID;
        if (!this->check(TokenKind::r_bracket)) {
            end_expr = this->parse_expr(context);
        }
        const syntax::Token& end = this->expect_slice_suffix_end(begin);
        const base::SourceRange range = this->merge(begin.range, end.range);
        return this->session_.module.push_slice_expr(
            this->merge(this->expr_range_or(base, range), range), base, first, end_expr);
    }

    std::vector<syntax::ExprId> args;
    args.push_back(first);
    while (!this->is_eof() && !this->check(TokenKind::r_bracket)) {
        if (!this->recover_bracket_arg_separator()) {
            break;
        }
        args.push_back(this->parse_expr(context));
    }

    const syntax::Token& end = this->expect_index_suffix_end(begin);
    const base::SourceRange bracket_range = this->merge(begin.range, end.range);
    const base::SourceRange range = this->merge(this->expr_range_or(base, bracket_range), bracket_range);
    syntax::ExprId index = syntax::INVALID_EXPR_ID;
    if (!args.empty()) {
        index = args.front();
    }
    if (args.size() > 1) {
        this->session_.diagnostics.report_at(
            syntax::Token{TokenKind::invalid, bracket_range, {}}, std::string(PARSER_INDEX_EXPECTS_ONE_ARGUMENT));
    }
    return this->session_.module.push_index_expr(range, base, index);
}

std::optional<syntax::ExprId> PostfixExprParser::parse_angle_generic_suffix(
    const syntax::ExprId base, const ExprContext context)
{
    if (!this->angle_generic_suffix_has_valid_continuation(context)) {
        return std::nullopt;
    }

    const syntax::Token& begin = this->expect_generic_left_angle_recovered(
        std::string(PARSER_EXPECT_GENERIC_APPLY_START), RecoveryContext::generic_type_argument);
    GenericSuffixArgs args = this->parse_angle_generic_args(begin);
    const syntax::Token& end = this->expect_generic_right_angle_recovered_after(
        std::string(PARSER_EXPECT_GENERIC_TYPE_ARGS_END), RecoveryContext::generic_type_argument, begin);

    syntax::GenericApplyExprPayload payload;
    payload.callee = base;
    payload.type_args = std::move(args.type_args);
    payload.generic_args = std::move(args.generic_args);

    const base::SourceRange suffix_range = this->merge(begin.range, end.range);
    return this->session_.module.push_generic_apply_expr(
        this->merge(this->expr_range_or(base, suffix_range), suffix_range), std::move(payload));
}

bool PostfixExprParser::angle_generic_suffix_has_valid_continuation(const ExprContext context) const noexcept
{
    int angle_depth = 0;
    base::usize offset = 0;
    while (!this->is_eof()) {
        const syntax::Token& token = this->peek_at(offset);
        switch (token.kind) {
            case TokenKind::less:
                ++angle_depth;
                break;
            case TokenKind::greater:
                --angle_depth;
                if (angle_depth == 0) {
                    return token_continues_generic_apply_suffix(this->peek_at(offset + 1U).kind, context);
                }
                break;
            case TokenKind::greater_greater:
                if (angle_depth == PARSER_GENERIC_ANGLE_SINGLE_CLOSE_DEPTH) {
                    return false;
                }
                angle_depth -= PARSER_GENERIC_ANGLE_DOUBLE_CLOSE_DEPTH;
                if (angle_depth == 0) {
                    return token_continues_generic_apply_suffix(this->peek_at(offset + 1U).kind, context);
                }
                if (angle_depth < 0) {
                    return false;
                }
                break;
            case TokenKind::greater_equal:
                if (angle_depth == PARSER_GENERIC_ANGLE_SINGLE_CLOSE_DEPTH) {
                    return token_continues_generic_apply_suffix(TokenKind::equal, context);
                }
                --angle_depth;
                break;
            case TokenKind::greater_greater_equal:
                if (angle_depth == PARSER_GENERIC_ANGLE_DOUBLE_CLOSE_DEPTH) {
                    return token_continues_generic_apply_suffix(TokenKind::equal, context);
                }
                return false;
            case TokenKind::eof:
            case TokenKind::semicolon:
                return false;
            default:
                break;
        }
        if (angle_depth <= 0) {
            return false;
        }
        ++offset;
    }
    return false;
}

PostfixExprParser::GenericSuffixArgs PostfixExprParser::parse_angle_generic_args(const syntax::Token& opening)
{
    GenericSuffixArgs args;
    args.type_args = this->session_.module.make_expr_list<syntax::TypeId>();
    args.generic_args = this->session_.module.make_expr_list<syntax::GenericArgDecl>();
    args.range = opening.range;
    if (this->check_generic_right_angle()) {
        this->report_here(std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT));
        return args;
    }
    while (!this->is_eof() && !this->check_generic_right_angle()) {
        syntax::GenericArgDecl arg = this->parse_angle_generic_arg();
        if (arg.kind == syntax::GenericArgKind::type && syntax::is_valid(arg.type)) {
            args.type_args.push_back(arg.type);
        }
        args.range = this->merge(args.range, arg.range);
        args.generic_args.push_back(arg);
        this->reset_panic();
        if (!this->recover_angle_generic_arg_separator()) {
            break;
        }
    }
    return args;
}

syntax::GenericArgDecl PostfixExprParser::parse_angle_generic_arg()
{
    if (this->check(TokenKind::integer_literal) || this->check(TokenKind::kw_true) || this->check(TokenKind::kw_false)
        || this->check(TokenKind::char_literal)) {
        const base::SourceRange range = this->peek().range;
        return syntax::GenericArgDecl{
            syntax::GenericArgKind::const_expr,
            syntax::INVALID_TYPE_ID,
            this->parse_const_generic_expr_atom(std::string(PARSER_EXPECT_GENERIC_ARGUMENT)),
            range,
        };
    }
    const syntax::TypeId type = this->parse_type();
    return syntax::GenericArgDecl{
        syntax::GenericArgKind::type,
        type,
        syntax::INVALID_EXPR_ID,
        this->type_range_or(type, this->previous().range),
    };
}

syntax::ExprId PostfixExprParser::parse_const_generic_expr_atom(std::string message)
{
    if (this->match(TokenKind::integer_literal)) {
        const syntax::Token& token = this->previous();
        return this->session_.module.push_literal_expr(syntax::ExprKind::integer_literal, token.range, token.text());
    }
    if (this->match(TokenKind::kw_true) || this->match(TokenKind::kw_false)) {
        const syntax::Token& token = this->previous();
        return this->session_.module.push_literal_expr(syntax::ExprKind::bool_literal, token.range, token.text());
    }
    if (this->match(TokenKind::char_literal)) {
        const syntax::Token& token = this->previous();
        return this->session_.module.push_literal_expr(syntax::ExprKind::char_literal, token.range, token.text());
    }
    this->report_here(std::move(message));
    return syntax::INVALID_EXPR_ID;
}

bool PostfixExprParser::recover_angle_generic_arg_separator() const
{
    if (this->check_generic_right_angle()) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check_generic_right_angle();
    }

    this->report_here(std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::generic_type_argument)) {
        this->synchronize(RecoveryContext::generic_type_argument);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check_generic_right_angle();
    }
    this->reset_panic();
    return false;
}

bool PostfixExprParser::recover_bracket_arg_separator()
{
    if (this->check(TokenKind::r_bracket)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }

    this->report_here(std::string(PARSER_EXPECT_INDEX_ARGUMENT_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::index_expression)) {
        this->synchronize(RecoveryContext::index_expression);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }
    this->reset_panic();
    return false;
}

syntax::ExprId PostfixExprParser::parse_field_suffix(const syntax::ExprId base)
{
    if (this->check(TokenKind::integer_literal)) {
        return this->parse_numeric_tuple_field_suffix(base, this->peek().range);
    }
    const syntax::Token& field = this->expect_identifier_recovered(std::string(PARSER_EXPECT_FIELD_AFTER_DOT));
    return this->session_.module.push_field_expr(
        this->merge(this->expr_range_or(base, field.range), field.range), base, field.text());
}

syntax::ExprId PostfixExprParser::parse_numeric_tuple_field_suffix(
    const syntax::ExprId base, const base::SourceRange& fallback_range)
{
    const syntax::Token& field = this->advance();
    std::string_view field_name = field.text();
    if (field.kind == TokenKind::float_literal && field_name.size() > PARSER_TUPLE_FIELD_DOT_PREFIX_LENGTH
        && field_name.front() == PARSER_TUPLE_FIELD_DOT) {
        field_name.remove_prefix(PARSER_TUPLE_FIELD_DOT_PREFIX_LENGTH);
    }
    const base::SourceRange range = this->merge(fallback_range, field.range);
    return this->session_.module.push_field_expr(this->merge(this->expr_range_or(base, range), range), base, field_name);
}

syntax::ExprId PostfixExprParser::parse_rejected_legacy_scope_suffix(
    const syntax::ExprId base, const base::SourceRange& fallback_range)
{
    const syntax::Token& scope = this->advance();
    this->report_at(scope, std::string(PARSER_DOT_ONLY_SELECTOR));
    const base::SourceRange range = this->merge(fallback_range, scope.range);
    return this->session_.module.push_invalid_expr(this->merge(this->expr_range_or(base, range), range));
}

syntax::ExprId PostfixExprParser::parse_struct_literal_suffix(const syntax::ExprId base, const ExprContext context)
{
    const syntax::Token& begin = this->expect(TokenKind::l_brace, std::string(PARSER_EXPECT_STRUCT_LITERAL_END));
    syntax::AstArenaVector<syntax::FieldInit> field_inits = this->session_.module.make_expr_list<syntax::FieldInit>();
    this->parse_struct_fields(field_inits, context);
    const syntax::Token& end = this->expect_recovered(
        TokenKind::r_brace, std::string(PARSER_EXPECT_STRUCT_LITERAL_END), RecoveryContext::struct_field);
    const base::SourceRange literal_range = this->merge(begin.range, end.range);
    return this->session_.module.push_struct_literal_expr(
        this->merge(this->expr_range_or(base, literal_range), literal_range), base, {}, {}, {},
        this->session_.module.make_expr_list<syntax::TypeId>(), std::move(field_inits));
}

void PostfixExprParser::parse_struct_fields(
    syntax::AstArenaVector<syntax::FieldInit>& fields, const ExprContext context)
{
    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        fields.push_back(this->parse_struct_field(context));
        this->reset_panic();
        if (!this->recover_struct_field_separator()) {
            break;
        }
    }
}

syntax::FieldInit PostfixExprParser::parse_struct_field(const ExprContext context)
{
    const syntax::Token& field = this->expect_identifier_recovered(std::string(PARSER_EXPECT_STRUCT_LITERAL_FIELD));
    this->expect_type_annotation_colon(std::string(PARSER_EXPECT_FIELD_TYPE_COLON));
    const syntax::ExprId value = this->parse_expr(context);
    return syntax::FieldInit{
        field.text(),
        value,
        this->merge(field.range, this->expr_range_or(value, field.range)),
    };
}

bool PostfixExprParser::recover_struct_field_separator()
{
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

const syntax::Token& PostfixExprParser::expect_index_suffix_end(const syntax::Token& opening)
{
    return this->expect_recovered_after(
        TokenKind::r_bracket, std::string(PARSER_EXPECT_INDEX_END), RecoveryContext::index_expression, opening);
}

const syntax::Token& PostfixExprParser::expect_slice_suffix_end(const syntax::Token& opening)
{
    return this->expect_recovered_after(
        TokenKind::r_bracket, std::string(PARSER_EXPECT_SLICE_END), RecoveryContext::index_expression, opening);
}

syntax::ExprId PostfixExprParser::parse_call_suffix(const syntax::ExprId base, const ExprContext context)
{
    const syntax::Token& begin = this->previous();
    syntax::CallExprPayload payload;
    payload.callee = base;
    payload.args = this->session_.module.make_expr_list<syntax::ExprId>();
    payload.arg_labels = this->session_.module.make_expr_list<syntax::CallArgLabelDecl>();
    this->parse_call_args(payload, context);
    const syntax::Token& end = this->expect_recovered_after(
        TokenKind::r_paren, std::string(PARSER_EXPECT_CALL_ARGUMENTS_END), RecoveryContext::call_argument, begin);
    const base::SourceRange call_range = this->merge(begin.range, end.range);
    return this->session_.module.push_call_expr(
        syntax::ExprKind::call, this->merge(this->expr_range_or(base, call_range), call_range), std::move(payload));
}

void PostfixExprParser::parse_call_args(syntax::CallExprPayload& payload, const ExprContext context)
{
    while (!this->is_eof() && !this->check(TokenKind::r_paren)) {
        this->parse_call_arg(payload, context);
        this->reset_panic();
        if (!this->recover_call_arg_separator()) {
            break;
        }
    }
}

void PostfixExprParser::parse_call_arg(syntax::CallExprPayload& payload, const ExprContext)
{
    syntax::CallArgLabelDecl label;
    if (this->check(TokenKind::identifier) && this->peek_at(1).kind == TokenKind::colon) {
        const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_EXPRESSION_NAME));
        static_cast<void>(this->expect(TokenKind::colon, std::string(PARSER_EXPECT_NAMED_ARGUMENT_VALUE)));
        label.name = name.text();
        label.range = name.range;
    }
    payload.args.push_back(this->parse_expr(ExprContext::normal));
    payload.arg_labels.push_back(label);
}

bool PostfixExprParser::recover_call_arg_separator()
{
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

syntax::ExprId PostfixExprParser::parse_try_suffix(const syntax::ExprId base)
{
    const syntax::Token& question = this->previous();
    return this->session_.module.push_try_expr(
        this->merge(this->expr_range_or(base, question.range), question.range), base);
}

syntax::ExprId PostfixExprParser::parse_rejected_update_suffix(
    const syntax::ExprId base, const base::SourceRange& fallback_range, const TokenKind kind, std::string message)
{
    const syntax::Token& op_token = this->expect(kind, std::string(PARSER_EXPECT_UNSUPPORTED_UPDATE));
    this->report_at(op_token, std::move(message));

    const base::SourceRange range = this->merge(fallback_range, op_token.range);
    return this->session_.module.push_invalid_expr(this->merge(this->expr_range_or(base, range), range));
}

} // namespace aurex::parse
