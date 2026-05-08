#include "aurex/parse/parser_parts.hpp"

#include "aurex/parse/recovery.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ExprId BuiltinExprParser::parse_cast(const syntax::ExprKind kind, const ExprContext context) {
    const syntax::Token& begin = this->previous();
    this->expect(TokenKind::l_paren, "expected '(' after cast builtin");
    const syntax::TypeId type = this->parse_type();
    this->recover_builtin_arg_separator("expected ',' after cast type");
    const syntax::ExprId value = this->parse_expr(context);
    const syntax::Token& end = this->expect(TokenKind::r_paren, "expected ')' after cast expression");

    syntax::ExprNode expr;
    expr.kind = kind;
    expr.range = this->merge(begin.range, end.range);
    expr.cast_type = type;
    expr.cast_expr = value;
    return this->session_.module.push_expr(std::move(expr));
}

syntax::ExprId BuiltinExprParser::parse_type_builtin(const syntax::ExprKind kind) {
    const syntax::Token& begin = this->previous();
    this->expect(TokenKind::l_paren, "expected '(' after type builtin");
    const syntax::TypeId type = this->parse_type();
    const syntax::Token& end = this->expect(TokenKind::r_paren, "expected ')' after type builtin");

    syntax::ExprNode expr;
    expr.kind = kind;
    expr.range = this->merge(begin.range, end.range);
    expr.cast_type = type;
    return this->session_.module.push_expr(std::move(expr));
}

syntax::ExprId BuiltinExprParser::parse_ptr_addr(const ExprContext context) {
    const syntax::Token& begin = this->previous();
    this->expect(TokenKind::l_paren, "expected '(' after ptr_addr");
    const syntax::ExprId value = this->parse_expr(context);
    const syntax::Token& end = this->expect(TokenKind::r_paren, "expected ')' after ptr_addr argument");

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::ptr_addr;
    expr.range = this->merge(begin.range, end.range);
    expr.cast_expr = value;
    return this->session_.module.push_expr(std::move(expr));
}

syntax::ExprId BuiltinExprParser::parse_ptr_from_addr(const ExprContext context) {
    const syntax::Token& begin = this->previous();
    this->expect(TokenKind::l_paren, "expected '(' after ptr_from_addr");
    const syntax::TypeId type = this->parse_type();
    this->recover_builtin_arg_separator("expected ',' after ptr_from_addr type");
    const syntax::ExprId value = this->parse_expr(context);
    const syntax::Token& end = this->expect(TokenKind::r_paren, "expected ')' after ptr_from_addr argument");

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::ptr_from_addr;
    expr.range = this->merge(begin.range, end.range);
    expr.cast_type = type;
    expr.cast_expr = value;
    return this->session_.module.push_expr(std::move(expr));
}

syntax::ExprId BuiltinExprParser::parse_move(const ExprContext context) {
    const syntax::Token& begin = this->previous();
    this->expect(TokenKind::l_paren, "expected '(' after move");
    const syntax::ExprId value = this->parse_expr(context);
    const syntax::Token& end = this->expect(TokenKind::r_paren, "expected ')' after move argument");

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::move_expr;
    expr.range = this->merge(begin.range, end.range);
    expr.unary_operand = value;
    return this->session_.module.push_expr(std::move(expr));
}

syntax::ExprId BuiltinExprParser::parse_str_unary(const ExprContext context) {
    const syntax::Token& begin = this->previous();
    const syntax::ExprKind kind = begin.kind == TokenKind::kw_str_data
        ? syntax::ExprKind::str_data
        : syntax::ExprKind::str_byte_len;
    this->expect(TokenKind::l_paren, "expected '(' after str builtin");
    const syntax::ExprId value = this->parse_expr(context);
    const syntax::Token& end = this->expect(TokenKind::r_paren, "expected ')' after str builtin argument");

    syntax::ExprNode expr;
    expr.kind = kind;
    expr.range = this->merge(begin.range, end.range);
    expr.cast_expr = value;
    return this->session_.module.push_expr(std::move(expr));
}

syntax::ExprId BuiltinExprParser::parse_str_from_bytes_unchecked(const ExprContext context) {
    const syntax::Token& begin = this->previous();
    this->expect(TokenKind::l_paren, "expected '(' after str_from_bytes_unchecked");
    const syntax::ExprId data = this->parse_expr(context);
    this->recover_builtin_arg_separator("expected ',' after str_from_bytes_unchecked data");
    const syntax::ExprId len = this->parse_expr(context);
    const syntax::Token& end = this->expect(TokenKind::r_paren, "expected ')' after str_from_bytes_unchecked length");

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::str_from_bytes_unchecked;
    expr.range = this->merge(begin.range, end.range);
    expr.args.push_back(data);
    expr.args.push_back(len);
    return this->session_.module.push_expr(std::move(expr));
}

void BuiltinExprParser::recover_builtin_arg_separator(std::string message) {
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

} // namespace aurex::parse
