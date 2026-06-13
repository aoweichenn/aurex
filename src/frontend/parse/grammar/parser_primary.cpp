#include <aurex/frontend/parse/parser_builtin_expr_part.hpp>
#include <aurex/frontend/parse/parser_messages.hpp>
#include <aurex/frontend/parse/parser_name_expr_part.hpp>
#include <aurex/frontend/parse/parser_primary_expr_part.hpp>
#include <aurex/frontend/parse/recovery.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

enum class BuiltinExprShape {
    CAST,
    TYPE,
    PTRADDR,
    PTRAT,
    SLICE_UNARY,
    STR_UNARY,
    STR_SLICE_UNARY,
    STRRAW,
};

struct BuiltinExprSyntax {
    TokenKind token;
    BuiltinExprShape shape;
    syntax::ExprKind expr_kind;
};

constexpr base::usize PARSER_MAX_EXPRESSION_NESTING_DEPTH = 512;
constexpr base::usize PARSER_GROUPED_RECOVERY_STACK_INITIAL_CAPACITY = 16;

constexpr BuiltinExprSyntax PARSER_PRIMARY_BUILTIN_EXPR_SYNTAX[] = {
    {TokenKind::kw_cast, BuiltinExprShape::CAST, syntax::ExprKind::cast},
    {TokenKind::kw_ptrcast, BuiltinExprShape::CAST, syntax::ExprKind::pcast},
    {TokenKind::kw_bitcast, BuiltinExprShape::CAST, syntax::ExprKind::bcast},
    {TokenKind::kw_sizeof, BuiltinExprShape::TYPE, syntax::ExprKind::size_of},
    {TokenKind::kw_alignof, BuiltinExprShape::TYPE, syntax::ExprKind::align_of},
    {TokenKind::kw_ptraddr, BuiltinExprShape::PTRADDR, syntax::ExprKind::ptr_addr},
    {
        TokenKind::kw_ptrat,
        BuiltinExprShape::PTRAT,
        syntax::ExprKind::paddr,
    },
    {TokenKind::kw_sliceptr, BuiltinExprShape::SLICE_UNARY, syntax::ExprKind::slice_data},
    {TokenKind::kw_slicelen, BuiltinExprShape::SLICE_UNARY, syntax::ExprKind::slice_len},
    {TokenKind::kw_strptr, BuiltinExprShape::STR_UNARY, syntax::ExprKind::str_data},
    {TokenKind::kw_strblen, BuiltinExprShape::STR_UNARY, syntax::ExprKind::str_byte_len},
    {TokenKind::kw_strvalid, BuiltinExprShape::STR_SLICE_UNARY, syntax::ExprKind::str_is_valid_utf8},
    {
        TokenKind::kw_strfromutf8,
        BuiltinExprShape::STR_SLICE_UNARY,
        syntax::ExprKind::str_from_utf8_checked,
    },
    {
        TokenKind::kw_strraw,
        BuiltinExprShape::STRRAW,
        syntax::ExprKind::str_from_bytes_unchecked,
    },
};

[[nodiscard]] const BuiltinExprSyntax* builtin_expr_for(const TokenKind kind) noexcept
{
    for (const BuiltinExprSyntax& builtin : PARSER_PRIMARY_BUILTIN_EXPR_SYNTAX) {
        if (builtin.token == kind) {
            return &builtin;
        }
    }
    return nullptr;
}

[[nodiscard]] std::optional<TokenKind> closing_delimiter_for(const TokenKind kind) noexcept
{
    switch (kind) {
        case TokenKind::l_paren:
            return TokenKind::r_paren;
        case TokenKind::l_bracket:
            return TokenKind::r_bracket;
        case TokenKind::l_brace:
            return TokenKind::r_brace;
        default:
            return std::nullopt;
    }
}

[[nodiscard]] bool token_is_closing_delimiter(const TokenKind kind) noexcept
{
    return kind == TokenKind::r_paren || kind == TokenKind::r_bracket || kind == TokenKind::r_brace;
}

class ExpressionNestingGuard final {
public:
    explicit ExpressionNestingGuard(ParseSession& session) noexcept : session_(&session)
    {
        ++this->session_->expression_nesting_depth;
    }

    ~ExpressionNestingGuard() noexcept
    {
        --this->session_->expression_nesting_depth;
    }

    ExpressionNestingGuard(const ExpressionNestingGuard&) = delete;
    ExpressionNestingGuard& operator=(const ExpressionNestingGuard&) = delete;
    ExpressionNestingGuard(ExpressionNestingGuard&&) = delete;
    ExpressionNestingGuard& operator=(ExpressionNestingGuard&&) = delete;

private:
    ParseSession* session_;
};

} // namespace

syntax::ExprId PrimaryExprParser::parse_primary(const ExprContext context)
{
    if (this->check(TokenKind::pipe) || this->check(TokenKind::pipe_pipe)) {
        return this->parse_lambda_expr(context);
    }
    if (this->check(TokenKind::kw_fn)) {
        return this->parse_legacy_lambda_expr(context);
    }
    if (this->check(TokenKind::identifier)) {
        return NameExprParser(this->parser_).parse_name_or_struct_literal(context);
    }
    if (this->match(TokenKind::integer_literal)) {
        return this->parse_literal(syntax::ExprKind::integer_literal);
    }
    if (this->match(TokenKind::float_literal)) {
        return this->parse_literal(syntax::ExprKind::float_literal);
    }
    if (this->match(TokenKind::kw_true) || this->match(TokenKind::kw_false)) {
        return this->parse_literal(syntax::ExprKind::bool_literal);
    }
    if (this->match(TokenKind::kw_null)) {
        return this->parse_literal(syntax::ExprKind::null_literal);
    }
    if (this->match(TokenKind::string_literal)) {
        return this->parse_literal(syntax::ExprKind::string_literal);
    }
    if (this->match(TokenKind::c_string_literal)) {
        return this->parse_literal(syntax::ExprKind::c_string_literal);
    }
    if (this->match(TokenKind::raw_string_literal)) {
        return this->parse_literal(syntax::ExprKind::raw_string_literal);
    }
    if (this->match(TokenKind::byte_string_literal)) {
        return this->parse_literal(syntax::ExprKind::byte_string_literal);
    }
    if (this->match(TokenKind::byte_literal)) {
        return this->parse_literal(syntax::ExprKind::byte_literal);
    }
    if (this->match(TokenKind::char_literal)) {
        return this->parse_literal(syntax::ExprKind::char_literal);
    }
    if (this->check(TokenKind::l_paren)) {
        return this->parse_tuple_or_grouped_expr(context);
    }
    if (this->check(TokenKind::kw_unsafe)) {
        return this->parse_unsafe_block_expr(context);
    }
    if (this->check(TokenKind::l_bracket)) {
        return this->parse_array_literal(context);
    }
    if (this->check(TokenKind::l_brace)) {
        return this->parse_block_expr(context);
    }
    const syntax::ExprId builtin = this->parse_builtin_expr(context);
    if (syntax::is_valid(builtin)) {
        return builtin;
    }

    this->report_here(std::string(PARSER_EXPECT_EXPRESSION));
    return this->make_invalid_expr();
}

syntax::ExprId PrimaryExprParser::parse_builtin_expr(const ExprContext context)
{
    const BuiltinExprSyntax* builtin = builtin_expr_for(this->peek().kind);
    if (builtin == nullptr) {
        return syntax::INVALID_EXPR_ID;
    }

    this->advance();
    BuiltinExprParser parser(this->parser_);
    switch (builtin->shape) {
        case BuiltinExprShape::CAST:
            return parser.parse_cast(builtin->expr_kind, context);
        case BuiltinExprShape::TYPE:
            return parser.parse_type_builtin(builtin->expr_kind);
        case BuiltinExprShape::PTRADDR:
            return parser.parse_ptraddr(context);
        case BuiltinExprShape::PTRAT:
            return parser.parse_ptrat(context);
        case BuiltinExprShape::SLICE_UNARY:
            return parser.parse_slice_unary(builtin->expr_kind, context);
        case BuiltinExprShape::STR_UNARY:
            return parser.parse_str_unary(context);
        case BuiltinExprShape::STR_SLICE_UNARY:
            return parser.parse_str_slice_unary(builtin->expr_kind, context);
        case BuiltinExprShape::STRRAW:
            return parser.parse_strraw(context);
    }
    return syntax::INVALID_EXPR_ID;
}

syntax::ExprId PrimaryExprParser::parse_lambda_expr(const ExprContext context)
{
    std::vector<syntax::ParamDecl> params;
    if (this->match(TokenKind::pipe_pipe)) {
        return this->finish_lambda_expr(LambdaHead{this->previous(), std::move(params)}, context);
    } else {
        const syntax::Token& begin = this->expect(TokenKind::pipe, std::string(PARSER_EXPECT_CLOSURE_PARAM_LIST));
        if (!this->check(TokenKind::pipe)) {
            params = this->parse_lambda_param_list(TokenKind::pipe);
        }
        this->expect_recovered(
            TokenKind::pipe, std::string(PARSER_EXPECT_CLOSURE_PARAM_LIST_END), RecoveryContext::parameter);
        return this->finish_lambda_expr(LambdaHead{begin, std::move(params)}, context);
    }
}

syntax::ExprId PrimaryExprParser::parse_legacy_lambda_expr(const ExprContext context)
{
    const syntax::Token& begin = this->expect(TokenKind::kw_fn, std::string(PARSER_EXPECT_FN_KEYWORD));
    this->report_at(begin, std::string(PARSER_LEGACY_FN_CLOSURE_LITERAL));
    this->expect_recovered(
        TokenKind::l_paren, std::string(PARSER_LEGACY_FN_CLOSURE_LITERAL), RecoveryContext::parameter_list_start);
    std::vector<syntax::ParamDecl> params;
    if (!this->check(TokenKind::r_paren)) {
        params = this->parse_lambda_param_list(TokenKind::r_paren);
    }
    this->expect_recovered(TokenKind::r_paren, std::string(PARSER_LEGACY_FN_CLOSURE_LITERAL),
        RecoveryContext::parameter);
    return this->finish_lambda_expr(LambdaHead{begin, std::move(params)}, context);
}

syntax::ExprId PrimaryExprParser::finish_lambda_expr(LambdaHead head, const ExprContext context)
{
    if (!this->match(TokenKind::arrow)) {
        this->report_here(std::string(PARSER_EXPECT_CLOSURE_RETURN_ARROW));
    }
    const syntax::TypeId return_type = this->parse_type();

    syntax::StmtId body = syntax::INVALID_STMT_ID;
    base::SourceRange end_range = this->type_range_or(return_type, head.begin.range);
    if (this->match(TokenKind::fat_arrow)) {
        const syntax::Token& body_begin = this->previous();
        const syntax::ExprId value = this->parse_expr(context);
        const base::SourceRange body_range = this->merge(body_begin.range, this->expr_range_or(value, body_begin.range));
        body = this->make_lambda_return_body(head.begin, value, body_range);
        end_range = this->stmt_range_or(body, body_range);
    } else if (this->check(TokenKind::l_brace)) {
        body = this->parse_block();
        end_range = this->stmt_range_or(body, end_range);
    } else {
        this->report_here(std::string(PARSER_EXPECT_CLOSURE_BODY));
    }

    this->reset_panic();
    return this->session_.module.push_lambda_expr(this->merge(head.begin.range, end_range), std::move(head.params),
        return_type, body);
}

std::vector<syntax::ParamDecl> PrimaryExprParser::parse_lambda_param_list(const TokenKind terminator)
{
    std::vector<syntax::ParamDecl> params;
    while (!this->is_eof() && !this->check(terminator)) {
        if (std::optional<syntax::ParamDecl> param = this->parse_lambda_param()) {
            params.push_back(param.value());
        }
        this->reset_panic();
        if (!this->recover_lambda_param_separator(terminator)) {
            break;
        }
    }
    return params;
}

std::optional<syntax::ParamDecl> PrimaryExprParser::parse_lambda_param()
{
    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_PARAMETER_NAME));
    this->expect_recovered(
        TokenKind::colon, std::string(PARSER_EXPECT_PARAMETER_TYPE_COLON), RecoveryContext::type_annotation);
    const syntax::TypeId type = this->parse_type();
    if (name.kind != TokenKind::identifier) {
        return std::nullopt;
    }
    return syntax::ParamDecl{
        name.text(),
        type,
        this->merge(name.range, this->type_range_or(type, name.range)),
        syntax::INVALID_IDENT_ID,
        false,
        syntax::INVALID_EXPR_ID,
    };
}

bool PrimaryExprParser::recover_lambda_param_separator(const TokenKind terminator)
{
    if (this->check(terminator)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(terminator);
    }

    this->report_here(std::string(terminator == TokenKind::pipe ? PARSER_EXPECT_CLOSURE_PARAM_SEPARATOR
                                                                : PARSER_EXPECT_PARAMETER_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::parameter)) {
        this->synchronize(RecoveryContext::parameter);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(terminator);
    }
    this->reset_panic();
    return false;
}

syntax::StmtId PrimaryExprParser::make_lambda_return_body(
    const syntax::Token& begin, const syntax::ExprId value, const base::SourceRange& body_range)
{
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = value;
    return_stmt.range = body_range;
    const syntax::StmtId return_id = this->session_.module.push_stmt(std::move(return_stmt));

    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    block.range = this->merge(begin.range, body_range);
    block.statements.push_back(return_id);
    return this->session_.module.push_stmt(std::move(block));
}

syntax::ExprId PrimaryExprParser::parse_unsafe_block_expr(const ExprContext context)
{
    const syntax::Token& begin = this->expect(TokenKind::kw_unsafe, std::string(PARSER_EXPECT_UNSAFE_KEYWORD));
    if (!this->check(TokenKind::l_brace)) {
        this->report_here(std::string(PARSER_EXPECT_UNSAFE_BLOCK));
    }
    const syntax::ExprId block = this->parse_block_expr(context);
    if (syntax::is_valid(block) && block.value < this->session_.module.exprs.size()) {
        const base::SourceRange range = this->merge(begin.range, this->session_.module.exprs.range(block.value));
        static_cast<void>(
            this->session_.module.exprs.retag_block_expr(block.value, syntax::ExprKind::unsafe_block, range));
    }
    return block;
}

syntax::ExprId PrimaryExprParser::parse_array_literal(const ExprContext)
{
    const syntax::Token& begin = this->expect(TokenKind::l_bracket, std::string(PARSER_EXPECT_ARRAY_LITERAL_START));
    syntax::AstArenaVector<syntax::ExprId> elements = this->session_.module.make_expr_list<syntax::ExprId>();
    syntax::ExprId repeat_value = syntax::INVALID_EXPR_ID;
    syntax::ExprId repeat_count = syntax::INVALID_EXPR_ID;

    if (this->check(TokenKind::r_bracket)) {
        const syntax::Token& end = this->expect_array_literal_end(begin);
        return this->session_.module.push_array_expr(this->merge(begin.range, end.range), std::move(elements));
    }

    const syntax::ExprId first = this->parse_expr(ExprContext::normal);
    if (this->match(TokenKind::semicolon)) {
        repeat_value = first;
        if (this->check(TokenKind::r_bracket)) {
            this->report_here(std::string(PARSER_EXPECT_ARRAY_REPEAT_COUNT));
        } else {
            repeat_count = this->parse_expr(ExprContext::normal);
        }
    } else {
        elements.push_back(first);
        while (this->recover_array_literal_separator()) {
            elements.push_back(this->parse_expr(ExprContext::normal));
            this->reset_panic();
        }
    }

    const syntax::Token& end = this->expect_array_literal_end(begin);
    this->reset_panic();
    return this->session_.module.push_array_expr(
        this->merge(begin.range, end.range), std::move(elements), repeat_value, repeat_count);
}

bool PrimaryExprParser::recover_array_literal_separator()
{
    if (this->check(TokenKind::r_bracket)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }

    this->report_here(std::string(PARSER_EXPECT_ARRAY_ELEMENT_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::array_literal)) {
        this->synchronize(RecoveryContext::array_literal);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }
    this->reset_panic();
    return false;
}

const syntax::Token& PrimaryExprParser::expect_array_literal_end(const syntax::Token& opening)
{
    return this->expect_recovered_after(
        TokenKind::r_bracket, std::string(PARSER_EXPECT_ARRAY_LITERAL_END), RecoveryContext::array_literal, opening);
}

syntax::ExprId PrimaryExprParser::parse_tuple_or_grouped_expr(const ExprContext context)
{
    const syntax::Token& begin = this->expect(TokenKind::l_paren, std::string(PARSER_EXPECT_GROUPED_EXPR_END));
    if (this->session_.expression_nesting_depth >= PARSER_MAX_EXPRESSION_NESTING_DEPTH) {
        this->report_at(begin, std::string(PARSER_EXPRESSION_NESTING_LIMIT));
        this->skip_grouped_expression_remainder();
        return this->session_.module.push_invalid_expr(this->merge(begin.range, this->previous().range));
    }

    const ExpressionNestingGuard nesting_guard(this->session_);
    if (this->check(TokenKind::r_paren)) {
        this->report_here(std::string(PARSER_EMPTY_TUPLE_LITERAL_UNSUPPORTED));
        const syntax::Token& end = this->expect_tuple_literal_end(begin);
        return this->session_.module.push_invalid_expr(this->merge(begin.range, end.range));
    }

    const syntax::ExprId first = this->parse_expr(context);
    if (!this->match(TokenKind::comma)) {
        this->expect_grouped_expression_end(begin);
        return first;
    }

    syntax::AstArenaVector<syntax::ExprId> elements = this->session_.module.make_expr_list<syntax::ExprId>();
    elements.push_back(first);
    while (!this->is_eof() && !this->check(TokenKind::r_paren)) {
        elements.push_back(this->parse_expr(ExprContext::normal));
        this->reset_panic();
        if (!this->recover_tuple_literal_separator()) {
            break;
        }
    }

    const syntax::Token& end = this->expect_tuple_literal_end(begin);
    this->reset_panic();
    return this->session_.module.push_tuple_expr(this->merge(begin.range, end.range), std::move(elements));
}

bool PrimaryExprParser::recover_tuple_literal_separator()
{
    if (this->check(TokenKind::r_paren)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }

    this->report_here(std::string(PARSER_EXPECT_TUPLE_ELEMENT_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::grouped_expression)) {
        this->synchronize(RecoveryContext::grouped_expression);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }
    this->reset_panic();
    return false;
}

const syntax::Token& PrimaryExprParser::expect_tuple_literal_end(const syntax::Token& opening)
{
    return this->expect_recovered_after(
        TokenKind::r_paren, std::string(PARSER_EXPECT_TUPLE_LITERAL_END), RecoveryContext::grouped_expression, opening);
}

void PrimaryExprParser::expect_grouped_expression_end(const syntax::Token& opening)
{
    this->expect_recovered_after(
        TokenKind::r_paren, std::string(PARSER_EXPECT_GROUPED_EXPR_END), RecoveryContext::grouped_expression, opening);
}

void PrimaryExprParser::skip_grouped_expression_remainder()
{
    std::vector<TokenKind> expected_closers;
    expected_closers.reserve(PARSER_GROUPED_RECOVERY_STACK_INITIAL_CAPACITY);
    expected_closers.push_back(TokenKind::r_paren);

    while (!this->is_eof() && !expected_closers.empty()) {
        const TokenKind kind = this->peek().kind;
        const std::optional<TokenKind> closer = closing_delimiter_for(kind);
        if (closer.has_value()) {
            expected_closers.push_back(closer.value());
            this->advance();
            continue;
        }

        if (kind == expected_closers.back()) {
            expected_closers.pop_back();
            this->advance();
            continue;
        }

        if (token_is_closing_delimiter(kind)) {
            this->advance();
            continue;
        }

        this->advance();
    }
}

syntax::ExprId PrimaryExprParser::parse_literal(const syntax::ExprKind kind) const
{
    const syntax::Token& token = this->previous();
    return this->session_.module.push_literal_expr(kind, token.range, token.text());
}

syntax::ExprId PrimaryExprParser::make_invalid_expr()
{
    const base::SourceRange range = this->peek().range;
    if (!this->is_eof()) {
        this->advance();
    }
    return this->session_.module.push_invalid_expr(range);
}

} // namespace aurex::parse
