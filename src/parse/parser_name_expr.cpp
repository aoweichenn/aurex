#include "aurex/parse/parser_parts.hpp"

#include <string_view>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ExprId NameExprParser::parse_name_or_struct_literal(const ExprContext context) {
    const syntax::Token& first = this->expect(TokenKind::identifier, "expected expression name");
    const syntax::Token* name = &first;
    std::string_view scope_name;
    base::SourceRange scope_range {};
    if (this->match(TokenKind::colon_colon)) {
        scope_name = first.text;
        scope_range = first.range;
        name = &this->expect(TokenKind::identifier, "expected item name after '::'");
    }

    std::vector<syntax::TypeId> struct_type_args;
    if (context == ExprContext::normal && this->next_angle_list_is_struct_literal()) {
        struct_type_args = this->parse_type_arg_list();
    }
    if (context == ExprContext::normal && this->check(TokenKind::l_brace)) {
        return this->parse_struct_literal(
            scope_name,
            scope_range,
            *name,
            std::move(struct_type_args),
            context
        );
    }
    if (!struct_type_args.empty()) {
        this->report_at(
            *name,
            "type arguments in expressions are only supported on struct literals, "
            "function calls, or scoped enum constructors"
        );
    }

    return this->make_name_expr(scope_name, scope_range, *name);
}

syntax::ExprId NameExprParser::parse_struct_literal(
    const std::string_view scope_name,
    const base::SourceRange scope_range,
    const syntax::Token& name,
    std::vector<syntax::TypeId> struct_type_args,
    const ExprContext context
) {
    this->advance();
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::struct_literal;
    node.scope_name = scope_name;
    node.scope_range = scope_range;
    node.struct_name = name.text;
    node.struct_type_args = std::move(struct_type_args);
    node.range = scope_name.empty() ? name.range : this->merge(scope_range, name.range);
    if (!this->check(TokenKind::r_brace)) {
        do {
            const syntax::Token& field = this->expect(
                TokenKind::identifier,
                "expected field name in struct literal"
            );
            this->expect(TokenKind::colon, "expected ':' after field name");
            const syntax::ExprId value = this->parse_expr(context);
            node.field_inits.push_back(syntax::FieldInit {
                field.text,
                value,
                this->merge(field.range, this->expr_range_or(value, field.range)),
            });
            if (this->check(TokenKind::r_brace)) {
                break;
            }
        } while (this->match(TokenKind::comma) && !this->check(TokenKind::r_brace));
    }
    const syntax::Token& end = this->expect(TokenKind::r_brace, "expected '}' after struct literal");
    node.range = this->merge(node.range, end.range);
    return this->session_.module.push_expr(std::move(node));
}

syntax::ExprId NameExprParser::make_name_expr(
    const std::string_view scope_name,
    const base::SourceRange scope_range,
    const syntax::Token& name
) {
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::name;
    expr.scope_name = scope_name;
    expr.scope_range = scope_range;
    expr.range = scope_name.empty() ? name.range : this->merge(scope_range, name.range);
    expr.text = name.text;
    return this->session_.module.push_expr(expr);
}

} // namespace aurex::parse
