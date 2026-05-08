#include "aurex/parse/parser_parts.hpp"

namespace aurex::parse {

namespace {

using syntax::TokenKind;

enum class BuiltinExprShape {
    cast,
    type,
    ptr_addr,
    ptr_from_addr,
    move,
    str_unary,
    str_from_bytes_unchecked,
};

struct BuiltinExprSyntax {
    TokenKind token;
    BuiltinExprShape shape;
    syntax::ExprKind expr_kind;
};

constexpr BuiltinExprSyntax kBuiltinExprSyntax[] = {
    {TokenKind::kw_cast, BuiltinExprShape::cast, syntax::ExprKind::cast},
    {TokenKind::kw_ptr_cast, BuiltinExprShape::cast, syntax::ExprKind::ptr_cast},
    {TokenKind::kw_bit_cast, BuiltinExprShape::cast, syntax::ExprKind::bit_cast},
    {TokenKind::kw_size_of, BuiltinExprShape::type, syntax::ExprKind::size_of},
    {TokenKind::kw_align_of, BuiltinExprShape::type, syntax::ExprKind::align_of},
    {TokenKind::kw_ptr_addr, BuiltinExprShape::ptr_addr, syntax::ExprKind::ptr_addr},
    {
        TokenKind::kw_ptr_from_addr,
        BuiltinExprShape::ptr_from_addr,
        syntax::ExprKind::ptr_from_addr,
    },
    {TokenKind::kw_move, BuiltinExprShape::move, syntax::ExprKind::move_expr},
    {TokenKind::kw_str_data, BuiltinExprShape::str_unary, syntax::ExprKind::str_data},
    {TokenKind::kw_str_byte_len, BuiltinExprShape::str_unary, syntax::ExprKind::str_byte_len},
    {
        TokenKind::kw_str_from_bytes_unchecked,
        BuiltinExprShape::str_from_bytes_unchecked,
        syntax::ExprKind::str_from_bytes_unchecked,
    },
};

[[nodiscard]] const BuiltinExprSyntax* builtin_expr_for(const TokenKind kind) noexcept {
    for (const BuiltinExprSyntax& builtin : kBuiltinExprSyntax) {
        if (builtin.token == kind) {
            return &builtin;
        }
    }
    return nullptr;
}

} // namespace

syntax::ExprId PrimaryExprParser::parse_primary(const ExprContext context) {
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
    if (this->match(TokenKind::byte_literal)) {
        return this->parse_literal(syntax::ExprKind::byte_literal);
    }
    if (this->match(TokenKind::l_paren)) {
        const syntax::ExprId expr = this->parse_expr(context);
        this->expect(TokenKind::r_paren, "expected ')' after expression");
        return expr;
    }
    if (this->check(TokenKind::l_brace)) {
        return this->parse_block_expr(context);
    }
    const syntax::ExprId builtin = this->parse_builtin_expr(context);
    if (syntax::is_valid(builtin)) {
        return builtin;
    }

    this->report_here("expected expression");
    return this->make_invalid_expr();
}

syntax::ExprId PrimaryExprParser::parse_builtin_expr(const ExprContext context) {
    const BuiltinExprSyntax* builtin = builtin_expr_for(this->peek().kind);
    if (builtin == nullptr) {
        return syntax::invalid_expr_id;
    }

    this->advance();
    BuiltinExprParser parser(this->parser_);
    switch (builtin->shape) {
    case BuiltinExprShape::cast:
        return parser.parse_cast(builtin->expr_kind, context);
    case BuiltinExprShape::type:
        return parser.parse_type_builtin(builtin->expr_kind);
    case BuiltinExprShape::ptr_addr:
        return parser.parse_ptr_addr(context);
    case BuiltinExprShape::ptr_from_addr:
        return parser.parse_ptr_from_addr(context);
    case BuiltinExprShape::move:
        return parser.parse_move(context);
    case BuiltinExprShape::str_unary:
        return parser.parse_str_unary(context);
    case BuiltinExprShape::str_from_bytes_unchecked:
        return parser.parse_str_from_bytes_unchecked(context);
    }
    return syntax::invalid_expr_id;
}

syntax::ExprId PrimaryExprParser::parse_literal(const syntax::ExprKind kind) {
    const syntax::Token& token = this->previous();
    syntax::ExprNode expr;
    expr.kind = kind;
    expr.range = token.range;
    expr.text = token.text;
    return this->session_.module.push_expr(expr);
}

syntax::ExprId PrimaryExprParser::make_invalid_expr() {
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::invalid;
    expr.range = this->peek().range;
    if (!this->is_eof()) {
        this->advance();
    }
    return this->session_.module.push_expr(expr);
}

} // namespace aurex::parse
