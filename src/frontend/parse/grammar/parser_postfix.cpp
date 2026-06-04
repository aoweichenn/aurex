#include <aurex/frontend/parse/parser_messages.hpp>
#include <aurex/frontend/parse/parser_postfix_expr_part.hpp>
#include <aurex/frontend/parse/parser_primary_expr_part.hpp>
#include <aurex/frontend/parse/recovery.hpp>

#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include <frontend/parse/support/private/bracket_suffix_classifier.hpp>
#include <frontend/parse/support/private/type_arg_expr_converter.hpp>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

constexpr base::usize PARSER_TUPLE_FIELD_DOT_PREFIX_LENGTH = 1;
constexpr char PARSER_TUPLE_FIELD_DOT = '.';
constexpr char PARSER_TUPLE_FIELD_FIRST_DIGIT = '0';
constexpr char PARSER_TUPLE_FIELD_LAST_DIGIT = '9';

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
        return this->parse_rejected_numeric_tuple_field_suffix(base, this->peek().range);
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
    syntax::AstArenaVector<BracketArg> args = this->session_.module.make_expr_list<BracketArg>();

    if (this->check(TokenKind::r_bracket)) {
        this->report_here(std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT));
        const syntax::Token& end = this->expect_index_suffix_end(begin);
        const base::SourceRange range = this->merge(begin.range, end.range);
        const BracketSuffixDecision decision = BracketSuffixClassifier(this->parser_).classify_empty_suffix();
        if (decision.kind == BracketSuffixKind::generic_apply) {
            return this->session_.module.push_generic_apply_expr(this->merge(this->expr_range_or(base, range), range),
                base, this->session_.module.make_expr_list<syntax::TypeId>());
        }
        return this->session_.module.push_index_expr(
            this->merge(this->expr_range_or(base, range), range), base, syntax::INVALID_EXPR_ID);
    }

    BracketArg first;
    if (!this->check(TokenKind::colon)) {
        first = this->parse_bracket_arg(context);
    }
    if (this->match(TokenKind::colon)) {
        const BracketSuffixDecision decision =
            BracketSuffixClassifier(this->parser_).classify_slice_suffix(syntax::is_valid(first.type));
        if (decision.has_type_only_arg) {
            this->report_here(std::string(PARSER_EXPECT_EXPRESSION));
        }
        syntax::ExprId end_expr = syntax::INVALID_EXPR_ID;
        if (!this->check(TokenKind::r_bracket)) {
            end_expr = this->parse_expr(context);
        }
        const syntax::Token& end = this->expect_slice_suffix_end(begin);
        const base::SourceRange range = this->merge(begin.range, end.range);
        return this->session_.module.push_slice_expr(
            this->merge(this->expr_range_or(base, range), range), base, first.expr, end_expr);
    }

    args.push_back(first);
    while (!this->is_eof() && !this->check(TokenKind::r_bracket)) {
        if (!this->recover_bracket_arg_separator()) {
            break;
        }
        args.push_back(this->parse_bracket_arg(context));
    }

    const syntax::Token& end = this->expect_index_suffix_end(begin);
    const base::SourceRange bracket_range = this->merge(begin.range, end.range);
    const base::SourceRange range = this->merge(this->expr_range_or(base, bracket_range), bracket_range);
    const bool has_type_only_arg = this->bracket_args_contain_type_only(args);
    const BracketSuffixDecision decision = this->classify_bracket_suffix(base, args, has_type_only_arg, context);
    if (decision.kind == BracketSuffixKind::generic_apply) {
        std::optional<syntax::AstArenaVector<syntax::TypeId>> type_args =
            this->bracket_args_to_type_args(args, decision.report_type_arg_errors);
        if (type_args.has_value()) {
            return this->session_.module.push_generic_apply_expr(range, base, std::move(type_args.value()));
        }
        if (decision.report_type_arg_errors) {
            return this->session_.module.push_generic_apply_expr(
                range, base, this->session_.module.make_expr_list<syntax::TypeId>());
        }
    }

    syntax::ExprId index = syntax::INVALID_EXPR_ID;
    if (!args.empty()) {
        index = args.front().expr;
    }
    if (args.size() > 1) {
        this->session_.diagnostics.report_at(
            syntax::Token{TokenKind::invalid, bracket_range, {}}, std::string(PARSER_INDEX_EXPECTS_ONE_ARGUMENT));
    }
    return this->session_.module.push_index_expr(range, base, index);
}

PostfixExprParser::BracketArg PostfixExprParser::parse_bracket_arg(const ExprContext context)
{
    if (this->bracket_arg_starts_type_only()) {
        const syntax::TypeId type = this->parse_type();
        return BracketArg{
            syntax::INVALID_EXPR_ID,
            type,
            this->type_range_or(type, this->previous().range),
        };
    }

    const syntax::ExprId expr = this->parse_expr(context);
    return BracketArg{
        expr,
        syntax::INVALID_TYPE_ID,
        this->expr_range_or(expr, this->previous().range),
    };
}

bool PostfixExprParser::bracket_arg_starts_type_only() const noexcept
{
    return BracketSuffixClassifier(this->parser_).arg_starts_type_only();
}

bool PostfixExprParser::bracket_args_contain_type_only(const std::span<const BracketArg> args) const noexcept
{
    for (const BracketArg& arg : args) {
        if (syntax::is_valid(arg.type)) {
            return true;
        }
    }
    return false;
}

bool PostfixExprParser::bracket_args_are_type_like(const std::span<const BracketArg> args) const
{
    for (const BracketArg& arg : args) {
        if (syntax::is_valid(arg.type)) {
            continue;
        }
        if (!this->bracket_arg_expr_is_type_like(arg.expr)) {
            return false;
        }
    }
    return !args.empty();
}

bool PostfixExprParser::bracket_arg_expr_is_type_like(const syntax::ExprId expr) const
{
    return BracketSuffixClassifier(this->parser_).arg_expr_is_type_like(expr);
}

BracketSuffixDecision PostfixExprParser::classify_bracket_suffix(const syntax::ExprId base,
    const std::span<const BracketArg> args, const bool has_type_only_arg, const ExprContext context) const
{
    return BracketSuffixClassifier(this->parser_)
        .classify_after_expr(BracketSuffixClassificationInput{
            .base = base,
            .has_type_only_arg = has_type_only_arg,
            .args_are_type_like = this->bracket_args_are_type_like(args),
            .context = context,
        });
}

std::optional<syntax::AstArenaVector<syntax::TypeId>> PostfixExprParser::bracket_args_to_type_args(
    const std::span<const BracketArg> args, const bool report_errors)
{
    syntax::AstArenaVector<syntax::TypeId> type_args = this->session_.module.make_expr_list<syntax::TypeId>();
    type_args.reserve(args.size());
    for (const BracketArg& arg : args) {
        if (syntax::is_valid(arg.type)) {
            type_args.push_back(arg.type);
            continue;
        }
        const syntax::TypeId type = this->bracket_arg_expr_to_type(arg.expr, report_errors);
        if (!syntax::is_valid(type)) {
            return std::nullopt;
        }
        type_args.push_back(type);
    }
    return type_args;
}

syntax::TypeId PostfixExprParser::bracket_arg_expr_to_type(const syntax::ExprId expr, const bool report_errors)
{
    return TypeArgExprConverter(this->parser_).convert(expr, report_errors);
}

syntax::TypeId PostfixExprParser::append_type_selector(
    const syntax::TypeId base, const std::string_view name, const base::SourceRange& range, const bool report_errors)
{
    return TypeArgExprConverter(this->parser_).append_selector(base, name, range, report_errors);
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

syntax::ExprId PostfixExprParser::parse_field_suffix(const syntax::ExprId base)
{
    if (this->check(TokenKind::integer_literal)) {
        return this->parse_rejected_numeric_tuple_field_suffix(base, this->peek().range);
    }
    const syntax::Token& field = this->expect_identifier_recovered(std::string(PARSER_EXPECT_FIELD_AFTER_DOT));
    return this->session_.module.push_field_expr(
        this->merge(this->expr_range_or(base, field.range), field.range), base, field.text());
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

syntax::ExprId PostfixExprParser::parse_rejected_numeric_tuple_field_suffix(
    const syntax::ExprId base, const base::SourceRange& fallback_range)
{
    const syntax::Token& field = this->advance();
    this->report_at(field, std::string(PARSER_TUPLE_FIELD_ACCESS_UNSUPPORTED));
    const base::SourceRange range = this->merge(fallback_range, field.range);
    return this->session_.module.push_invalid_expr(this->merge(this->expr_range_or(base, range), range));
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
    syntax::AstArenaVector<syntax::ExprId> args = this->session_.module.make_expr_list<syntax::ExprId>();
    this->parse_call_args(args, context);
    const syntax::Token& end = this->expect_recovered_after(
        TokenKind::r_paren, std::string(PARSER_EXPECT_CALL_ARGUMENTS_END), RecoveryContext::call_argument, begin);
    const base::SourceRange call_range = this->merge(begin.range, end.range);
    return this->session_.module.push_call_expr(
        syntax::ExprKind::call, this->merge(this->expr_range_or(base, call_range), call_range), base, std::move(args));
}

void PostfixExprParser::parse_call_args(syntax::AstArenaVector<syntax::ExprId>& args, const ExprContext)
{
    while (!this->is_eof() && !this->check(TokenKind::r_paren)) {
        args.push_back(this->parse_expr(ExprContext::normal));
        this->reset_panic();
        if (!this->recover_call_arg_separator()) {
            break;
        }
    }
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
