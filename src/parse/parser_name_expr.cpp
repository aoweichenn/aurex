#include <aurex/parse/parser_name_expr_part.hpp>

#include <aurex/parse/recovery.hpp>

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
    if (this->check(TokenKind::colon_colon) && this->peek_at(1).kind == TokenKind::identifier) {
        this->advance();
        scope_name = first.text;
        scope_range = first.range;
        name = &this->expect_identifier_recovered("expected item name after '::'");
    }

    std::vector<syntax::TypeId> type_args;
    base::SourceRange type_name_range = scope_name.empty() ? name->range : this->merge(scope_range, name->range);
    if (context == ExprContext::normal &&
        this->check(TokenKind::l_bracket) &&
        this->try_parse_struct_literal_type_args(type_args, type_name_range)) {
        return this->parse_struct_literal(
            scope_name,
            scope_range,
            *name,
            std::move(type_args),
            type_name_range,
            context
        );
    }

    if (context == ExprContext::normal && this->check(TokenKind::l_brace)) {
        return this->parse_struct_literal(
            scope_name,
            scope_range,
            *name,
            {},
            type_name_range,
            context
        );
    }

    return this->make_name_expr(scope_name, scope_range, *name);
}

syntax::ExprId NameExprParser::parse_struct_literal(
    const std::string_view scope_name,
    const base::SourceRange scope_range,
    const syntax::Token& name,
    std::vector<syntax::TypeId> type_args,
    const base::SourceRange type_name_range,
    const ExprContext context
) {
    this->advance();
    syntax::ExprNode node;
    node.kind = syntax::ExprKind::struct_literal;
    node.scope_name = scope_name;
    node.scope_range = scope_range;
    node.struct_name = name.text;
    node.type_args = std::move(type_args);
    node.range = type_name_range;
    this->parse_struct_fields(node.field_inits, context);
    const syntax::Token& end = this->expect_recovered(
        TokenKind::r_brace,
        "expected '}' after struct literal",
        RecoveryContext::struct_field
    );
    node.range = this->merge(node.range, end.range);
    return this->session_.module.push_expr(std::move(node));
}

bool NameExprParser::try_parse_struct_literal_type_args(
    std::vector<syntax::TypeId>& type_args,
    base::SourceRange& end_range
) {
    if (!this->bracketed_type_args_followed_by_struct_body()) {
        return false;
    }
    const base::usize checkpoint = this->mark();
    this->advance();
    this->parse_struct_literal_type_args(type_args);
    if (!this->match(TokenKind::r_bracket)) {
        this->rewind(checkpoint);
        type_args.clear();
        return false;
    }
    const syntax::Token& end = this->previous();
    if (!this->check(TokenKind::l_brace)) {
        this->rewind(checkpoint);
        type_args.clear();
        return false;
    }
    end_range = this->merge(end_range, end.range);
    return true;
}

bool NameExprParser::bracketed_type_args_followed_by_struct_body() const noexcept {
    if (!this->check(TokenKind::l_bracket)) {
        return false;
    }
    base::usize depth = 0;
    for (base::usize offset = 0; true; ++offset) {
        const syntax::Token& token = this->peek_at(offset);
        if (token.kind == TokenKind::eof) {
            return false;
        }
        if (token.kind == TokenKind::l_bracket) {
            ++depth;
            continue;
        }
        if (token.kind == TokenKind::r_bracket) {
            if (depth == 0) {
                return false;
            }
            --depth;
            if (depth == 0) {
                return this->peek_at(offset + 1).kind == TokenKind::l_brace;
            }
        }
    }
}

void NameExprParser::parse_struct_literal_type_args(std::vector<syntax::TypeId>& type_args) {
    if (this->check(TokenKind::r_bracket)) {
        this->report_here("expected generic type argument");
    }
    while (!this->is_eof() && !this->check(TokenKind::r_bracket)) {
        type_args.push_back(this->parse_type());
        this->reset_panic();
        if (!this->recover_struct_literal_type_arg_separator()) {
            break;
        }
    }
}

bool NameExprParser::recover_struct_literal_type_arg_separator() {
    if (this->check(TokenKind::r_bracket)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }

    this->report_here("expected ',' or ']' after generic type argument");
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

void NameExprParser::parse_struct_fields(
    std::vector<syntax::FieldInit>& fields,
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

syntax::FieldInit NameExprParser::parse_struct_field(const ExprContext context) {
    const syntax::Token& field =
        this->expect_identifier_recovered("expected field name in struct literal");
    this->expect_type_annotation_colon("expected ':' after field name");
    const syntax::ExprId value = this->parse_expr(context);
    return syntax::FieldInit {
        field.text,
        value,
        this->merge(field.range, this->expr_range_or(value, field.range)),
    };
}

bool NameExprParser::recover_struct_field_separator() {
    if (this->check(TokenKind::r_brace)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_brace);
    }

    this->report_here("expected ',' or '}' after struct literal field");
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
