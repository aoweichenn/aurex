#include <aurex/frontend/parse/parser_builtin_expr_part.hpp>
#include <aurex/frontend/parse/parser_messages.hpp>
#include <aurex/frontend/parse/recovery.hpp>

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

constexpr base::usize PARSER_STRRAW_ARG_COUNT = 2;

[[nodiscard]] std::string parser_builtin_type_start_message(const std::string& name)
{
    return std::string(PARSER_EXPECT_BUILTIN_TYPE_START_PREFIX) + name
        + std::string(PARSER_EXPECT_BUILTIN_TYPE_START_SUFFIX);
}

[[nodiscard]] std::string parser_builtin_type_end_message(const std::string& name)
{
    return std::string(PARSER_EXPECT_BUILTIN_TYPE_END_PREFIX) + name
        + std::string(PARSER_EXPECT_BUILTIN_TYPE_END_SUFFIX);
}

[[nodiscard]] std::string parser_builtin_expr_start_message(const std::string& name)
{
    return std::string(PARSER_EXPECT_BUILTIN_EXPR_START_PREFIX) + name
        + std::string(PARSER_EXPECT_BUILTIN_EXPR_START_SUFFIX);
}

[[nodiscard]] std::string parser_builtin_expr_end_message(const std::string& name)
{
    return std::string(PARSER_EXPECT_BUILTIN_EXPR_END_PREFIX) + name
        + std::string(PARSER_EXPECT_BUILTIN_EXPR_END_SUFFIX);
}

[[nodiscard]] std::string parser_builtin_arg_start_message(const std::string& name)
{
    return std::string(PARSER_EXPECT_BUILTIN_ARG_START_PREFIX) + name;
}

[[nodiscard]] std::string parser_builtin_arg_end_message(const std::string& name)
{
    return std::string(PARSER_EXPECT_BUILTIN_ARG_END_PREFIX) + name + std::string(PARSER_EXPECT_BUILTIN_ARG_END_SUFFIX);
}

} // namespace

syntax::ExprId BuiltinExprParser::parse_cast(const syntax::ExprKind kind, const ExprContext context)
{
    const syntax::Token& begin = this->previous();
    const std::string name{begin.text()};
    this->expect_builtin_type_arg_list_start(parser_builtin_type_start_message(name));
    const syntax::TypeId type = this->parse_type();
    [[maybe_unused]] const syntax::Token& type_end =
        this->expect_builtin_type_arg_list_end(parser_builtin_type_end_message(name));
    this->expect_builtin_arg_list_start(parser_builtin_expr_start_message(name));
    const syntax::ExprId value = this->parse_expr(context);
    const syntax::Token& end = this->expect_builtin_arg_list_end(parser_builtin_expr_end_message(name));

    return this->session_.module.push_cast_like_expr(kind, this->merge(begin.range, end.range), type, value);
}

syntax::ExprId BuiltinExprParser::parse_type_builtin(const syntax::ExprKind kind)
{
    const syntax::Token& begin = this->previous();
    const std::string name{begin.text()};
    this->expect_builtin_type_arg_list_start(parser_builtin_type_start_message(name));
    const syntax::TypeId type = this->parse_type();
    const syntax::Token& end = this->expect_builtin_type_arg_list_end(parser_builtin_type_end_message(name));

    return this->session_.module.push_cast_like_expr(
        kind, this->merge(begin.range, end.range), type, syntax::INVALID_EXPR_ID);
}

syntax::ExprId BuiltinExprParser::parse_ptraddr(const ExprContext context)
{
    const syntax::Token& begin = this->previous();
    this->expect_builtin_arg_list_start(std::string(PARSER_EXPECT_BUILTIN_PTRADDR_START));
    const syntax::ExprId value = this->parse_expr(context);
    const syntax::Token& end = this->expect_builtin_arg_list_end(std::string(PARSER_EXPECT_BUILTIN_PTRADDR_END));

    return this->session_.module.push_cast_like_expr(
        syntax::ExprKind::ptr_addr, this->merge(begin.range, end.range), syntax::INVALID_TYPE_ID, value);
}

syntax::ExprId BuiltinExprParser::parse_ptrat(const ExprContext context)
{
    const syntax::Token& begin = this->previous();
    this->expect_builtin_type_arg_list_start(std::string(PARSER_EXPECT_BUILTIN_PTRAT_TYPE_START));
    const syntax::TypeId type = this->parse_type();
    [[maybe_unused]] const syntax::Token& type_end =
        this->expect_builtin_type_arg_list_end(std::string(PARSER_EXPECT_BUILTIN_PTRAT_TYPE_END));
    this->expect_builtin_arg_list_start(std::string(PARSER_EXPECT_BUILTIN_PTRAT_START));
    const syntax::ExprId value = this->parse_expr(context);
    const syntax::Token& end = this->expect_builtin_arg_list_end(std::string(PARSER_EXPECT_BUILTIN_PTRAT_END));

    return this->session_.module.push_cast_like_expr(
        syntax::ExprKind::paddr, this->merge(begin.range, end.range), type, value);
}

syntax::ExprId BuiltinExprParser::parse_slice_unary(const syntax::ExprKind kind, const ExprContext context)
{
    const syntax::Token& begin = this->previous();
    const std::string name{begin.text()};
    this->expect_builtin_arg_list_start(parser_builtin_arg_start_message(name));
    const syntax::ExprId value = this->parse_expr(context);
    const syntax::Token& end = this->expect_builtin_arg_list_end(parser_builtin_arg_end_message(name));

    return this->session_.module.push_cast_like_expr(
        kind, this->merge(begin.range, end.range), syntax::INVALID_TYPE_ID, value);
}

syntax::ExprId BuiltinExprParser::parse_str_unary(const ExprContext context)
{
    const syntax::Token& begin = this->previous();
    const std::string name{begin.text()};
    const syntax::ExprKind kind =
        begin.kind == TokenKind::kw_strptr ? syntax::ExprKind::str_data : syntax::ExprKind::str_byte_len;
    this->expect_builtin_arg_list_start(parser_builtin_arg_start_message(name));
    const syntax::ExprId value = this->parse_expr(context);
    const syntax::Token& end = this->expect_builtin_arg_list_end(parser_builtin_arg_end_message(name));

    return this->session_.module.push_cast_like_expr(
        kind, this->merge(begin.range, end.range), syntax::INVALID_TYPE_ID, value);
}

syntax::ExprId BuiltinExprParser::parse_str_slice_unary(const syntax::ExprKind kind, const ExprContext context)
{
    const syntax::Token& begin = this->previous();
    const std::string name{begin.text()};
    this->expect_builtin_arg_list_start(parser_builtin_arg_start_message(name));
    const syntax::ExprId value = this->parse_expr(context);
    const syntax::Token& end = this->expect_builtin_arg_list_end(parser_builtin_arg_end_message(name));

    return this->session_.module.push_cast_like_expr(
        kind, this->merge(begin.range, end.range), syntax::INVALID_TYPE_ID, value);
}

syntax::ExprId BuiltinExprParser::parse_strraw(const ExprContext context)
{
    const syntax::Token& begin = this->previous();
    this->expect_builtin_arg_list_start(std::string(PARSER_EXPECT_BUILTIN_STRRAW_START));
    const syntax::ExprId data = this->parse_expr(context);
    this->recover_builtin_arg_separator(std::string(PARSER_EXPECT_BUILTIN_STRRAW_DATA_SEPARATOR));
    const syntax::ExprId len = this->parse_expr(context);
    const syntax::Token& end = this->expect_builtin_arg_list_end(std::string(PARSER_EXPECT_BUILTIN_STRRAW_END));

    syntax::AstArenaVector<syntax::ExprId> args = this->session_.module.make_expr_list<syntax::ExprId>();
    args.reserve(PARSER_STRRAW_ARG_COUNT);
    args.push_back(data);
    args.push_back(len);
    return this->session_.module.push_call_expr(syntax::ExprKind::str_from_bytes_unchecked,
        this->merge(begin.range, end.range), syntax::INVALID_EXPR_ID, std::move(args));
}

void BuiltinExprParser::expect_builtin_arg_list_start(std::string message)
{
    this->expect_recovered(TokenKind::l_paren, std::move(message), RecoveryContext::builtin_argument_list_start);
}

void BuiltinExprParser::expect_builtin_type_arg_list_start(std::string message)
{
    this->expect_recovered(TokenKind::l_bracket, std::move(message), RecoveryContext::builtin_argument_list_start);
}

void BuiltinExprParser::recover_builtin_arg_separator(std::string message)
{
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return;
    }

    this->report_here(std::move(message));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::builtin_argument)) {
        this->synchronize(RecoveryContext::builtin_argument);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return;
    }
    this->reset_panic();
}

const syntax::Token& BuiltinExprParser::expect_builtin_arg_list_end(std::string message)
{
    return this->expect_recovered(TokenKind::r_paren, std::move(message), RecoveryContext::builtin_argument);
}

const syntax::Token& BuiltinExprParser::expect_builtin_type_arg_list_end(std::string message)
{
    return this->expect_recovered(TokenKind::r_bracket, std::move(message), RecoveryContext::array_type_length);
}

} // namespace aurex::parse
