#include <aurex/parse/parser_primary_expr_part.hpp>

#include <aurex/parse/parser_builtin_expr_part.hpp>
#include <aurex/parse/parser_messages.hpp>
#include <aurex/parse/parser_name_expr_part.hpp>
#include <aurex/parse/recovery.hpp>

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

enum class BuiltinExprShape {
    CAST,
    TYPE,
    PTRADDR,
    PTRAT,
    STR_UNARY,
    STRRAW,
};

struct BuiltinExprSyntax {
    TokenKind token;
    BuiltinExprShape shape;
    syntax::ExprKind expr_kind;
};

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
    {TokenKind::kw_strptr, BuiltinExprShape::STR_UNARY, syntax::ExprKind::str_data},
    {TokenKind::kw_strblen, BuiltinExprShape::STR_UNARY, syntax::ExprKind::str_byte_len},
    {
        TokenKind::kw_strraw,
        BuiltinExprShape::STRRAW,
        syntax::ExprKind::str_from_bytes_unchecked,
    },
};

[[nodiscard]] const BuiltinExprSyntax* builtin_expr_for(const TokenKind kind) noexcept {
    for (const BuiltinExprSyntax& builtin : PARSER_PRIMARY_BUILTIN_EXPR_SYNTAX) {
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
    if (this->match(TokenKind::l_paren)) {
        const syntax::ExprId expr = this->parse_expr(context);
        this->expect_grouped_expression_end();
        return expr;
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

syntax::ExprId PrimaryExprParser::parse_builtin_expr(const ExprContext context) {
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
    case BuiltinExprShape::STR_UNARY:
        return parser.parse_str_unary(context);
    case BuiltinExprShape::STRRAW:
        return parser.parse_strraw(context);
    }
    return syntax::INVALID_EXPR_ID;
}

syntax::ExprId PrimaryExprParser::parse_unsafe_block_expr(const ExprContext context) {
    const syntax::Token& begin = this->expect(TokenKind::kw_unsafe, std::string(PARSER_EXPECT_UNSAFE_KEYWORD));
    if (!this->check(TokenKind::l_brace)) {
        this->report_here(std::string(PARSER_EXPECT_UNSAFE_BLOCK));
    }
    const syntax::ExprId block = this->parse_block_expr(context);
    if (syntax::is_valid(block) && block.value < this->session_.module.exprs.size()) {
        syntax::ExprNode& expr = this->session_.module.exprs[block.value];
        expr.kind = syntax::ExprKind::unsafe_block;
        expr.range = this->merge(begin.range, expr.range);
    }
    return block;
}

syntax::ExprId PrimaryExprParser::parse_array_literal(const ExprContext) {
    const syntax::Token& begin = this->expect(TokenKind::l_bracket, std::string(PARSER_EXPECT_ARRAY_LITERAL_START));
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::array_literal;

    if (this->check(TokenKind::r_bracket)) {
        const syntax::Token& end = this->expect_array_literal_end();
        expr.range = this->merge(begin.range, end.range);
        return this->session_.module.push_expr(std::move(expr));
    }

    const syntax::ExprId first = this->parse_expr(ExprContext::normal);
    if (this->match(TokenKind::semicolon)) {
        expr.array_repeat_value = first;
        if (this->check(TokenKind::r_bracket)) {
            this->report_here(std::string(PARSER_EXPECT_ARRAY_REPEAT_COUNT));
        } else {
            expr.array_repeat_count = this->parse_expr(ExprContext::normal);
        }
    } else {
        expr.array_elements.push_back(first);
        while (this->recover_array_literal_separator()) {
            expr.array_elements.push_back(this->parse_expr(ExprContext::normal));
            this->reset_panic();
        }
    }

    const syntax::Token& end = this->expect_array_literal_end();
    expr.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_expr(std::move(expr));
}

bool PrimaryExprParser::recover_array_literal_separator() {
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

const syntax::Token& PrimaryExprParser::expect_array_literal_end() {
    return this->expect_recovered(
        TokenKind::r_bracket,
        std::string(PARSER_EXPECT_ARRAY_LITERAL_END),
        RecoveryContext::array_literal
    );
}

void PrimaryExprParser::expect_grouped_expression_end() {
    this->expect_recovered(
        TokenKind::r_paren,
        std::string(PARSER_EXPECT_GROUPED_EXPR_END),
        RecoveryContext::grouped_expression
    );
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
