#include "aurex/parse/parser_parts.hpp"

#include <string_view>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ExprId PrimaryExprParser::parse_primary(const ExprContext context) {
    if (check(TokenKind::identifier)) {
        return parse_name_or_struct_literal(context);
    }
    if (match(TokenKind::integer_literal)) {
        return parse_literal(syntax::ExprKind::integer_literal);
    }
    if (match(TokenKind::float_literal)) {
        return parse_literal(syntax::ExprKind::float_literal);
    }
    if (match(TokenKind::kw_true) || match(TokenKind::kw_false)) {
        return parse_literal(syntax::ExprKind::bool_literal);
    }
    if (match(TokenKind::kw_null)) {
        return parse_literal(syntax::ExprKind::null_literal);
    }
    if (match(TokenKind::string_literal)) {
        return parse_literal(syntax::ExprKind::string_literal);
    }
    if (match(TokenKind::c_string_literal)) {
        return parse_literal(syntax::ExprKind::c_string_literal);
    }
    if (match(TokenKind::byte_literal)) {
        return parse_literal(syntax::ExprKind::byte_literal);
    }
    if (match(TokenKind::l_paren)) {
        const syntax::ExprId expr = parse_expr(context);
        expect(TokenKind::r_paren, "expected ')' after expression");
        return expr;
    }
    if (check(TokenKind::l_brace)) {
        return parse_block_expr(context);
    }
    if (match(TokenKind::kw_cast)) {
        return BuiltinExprParser(parser_).parse_cast(syntax::ExprKind::cast, context);
    }
    if (match(TokenKind::kw_ptr_cast)) {
        return BuiltinExprParser(parser_).parse_cast(syntax::ExprKind::ptr_cast, context);
    }
    if (match(TokenKind::kw_bit_cast)) {
        return BuiltinExprParser(parser_).parse_cast(syntax::ExprKind::bit_cast, context);
    }
    if (match(TokenKind::kw_size_of)) {
        return BuiltinExprParser(parser_).parse_type_builtin(syntax::ExprKind::size_of);
    }
    if (match(TokenKind::kw_align_of)) {
        return BuiltinExprParser(parser_).parse_type_builtin(syntax::ExprKind::align_of);
    }
    if (match(TokenKind::kw_ptr_addr)) {
        return BuiltinExprParser(parser_).parse_ptr_addr(context);
    }
    if (match(TokenKind::kw_ptr_from_addr)) {
        return BuiltinExprParser(parser_).parse_ptr_from_addr(context);
    }
    if (match(TokenKind::kw_move)) {
        return BuiltinExprParser(parser_).parse_move(context);
    }
    if (match(TokenKind::kw_str_data) || match(TokenKind::kw_str_byte_len)) {
        return BuiltinExprParser(parser_).parse_str_unary(context);
    }
    if (match(TokenKind::kw_str_from_bytes_unchecked)) {
        return BuiltinExprParser(parser_).parse_str_from_bytes_unchecked(context);
    }

    report_here("expected expression");
    return make_invalid_expr();
}

syntax::ExprId PrimaryExprParser::parse_name_or_struct_literal(const ExprContext context) {
    const syntax::Token& first = expect(TokenKind::identifier, "expected expression name");
    const syntax::Token* name = &first;
    std::string_view scope_name;
    base::SourceRange scope_range {};
    if (match(TokenKind::colon_colon)) {
        scope_name = first.text;
        scope_range = first.range;
        name = &expect(TokenKind::identifier, "expected item name after '::'");
    }

    std::vector<syntax::TypeId> struct_type_args;
    if (context == ExprContext::normal && next_angle_list_is_struct_literal()) {
        struct_type_args = parse_type_arg_list();
    }
    if (context == ExprContext::normal && check(TokenKind::l_brace)) {
        advance();
        syntax::ExprNode node;
        node.kind = syntax::ExprKind::struct_literal;
        node.scope_name = scope_name;
        node.scope_range = scope_range;
        node.struct_name = name->text;
        node.struct_type_args = std::move(struct_type_args);
        node.range = scope_name.empty() ? name->range : merge(scope_range, name->range);
        if (!check(TokenKind::r_brace)) {
            do {
                const syntax::Token& field = expect(TokenKind::identifier, "expected field name in struct literal");
                expect(TokenKind::colon, "expected ':' after field name");
                const syntax::ExprId value = parse_expr(context);
                node.field_inits.push_back(syntax::FieldInit {
                    field.text,
                    value,
                    merge(field.range, session_.module.exprs[value.value].range),
                });
                if (check(TokenKind::r_brace)) {
                    break;
                }
            } while (match(TokenKind::comma) && !check(TokenKind::r_brace));
        }
        const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after struct literal");
        node.range = merge(node.range, end.range);
        return session_.module.push_expr(std::move(node));
    }
    if (!struct_type_args.empty()) {
        report_at(*name, "type arguments in expressions are only supported on struct literals, function calls, or scoped enum constructors");
    }

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::name;
    expr.scope_name = scope_name;
    expr.scope_range = scope_range;
    expr.range = scope_name.empty() ? name->range : merge(scope_range, name->range);
    expr.text = name->text;
    return session_.module.push_expr(expr);
}

syntax::ExprId PrimaryExprParser::parse_literal(const syntax::ExprKind kind) {
    const syntax::Token& token = previous();
    syntax::ExprNode expr;
    expr.kind = kind;
    expr.range = token.range;
    expr.text = token.text;
    return session_.module.push_expr(expr);
}

syntax::ExprId PrimaryExprParser::make_invalid_expr() {
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::invalid;
    expr.range = peek().range;
    if (!is_eof()) {
        advance();
    }
    return session_.module.push_expr(expr);
}

} // namespace aurex::parse
