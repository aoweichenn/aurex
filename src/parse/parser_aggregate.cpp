#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ItemId ItemParser::parse_struct_decl() {
    const bool is_noncopy = this->match(TokenKind::kw_noncopy);
    const syntax::Token& begin = is_noncopy ? this->previous() : this->peek();
    this->expect(TokenKind::kw_struct, is_noncopy ? "expected 'struct' after 'noncopy'" : "expected 'struct'");
    const syntax::Token& name = this->expect(TokenKind::identifier, "expected struct name");
    std::vector<std::string_view> generic_params;
    if (this->check(TokenKind::less)) {
        generic_params = this->parse_generic_param_list();
    }
    this->expect(TokenKind::l_brace, "expected '{' after struct name");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::struct_decl;
    item.name = name.text;
    item.generic_params = std::move(generic_params);
    item.is_noncopy = is_noncopy;

    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        const syntax::Visibility field_visibility = this->parse_visibility();
        const syntax::Token& field_name = this->expect(TokenKind::identifier, "expected field name");
        this->expect(TokenKind::colon, "expected ':' after field name");
        const syntax::TypeId field_type = this->parse_type();
        const syntax::Token& end = this->expect(TokenKind::semicolon, "expected ';' after field declaration");
        if (field_name.kind == TokenKind::identifier) {
            item.fields.push_back(syntax::FieldDecl {
                field_name.text,
                field_type,
                this->merge(field_name.range, end.range),
                field_visibility,
            });
        }
        this->reset_panic();
    }

    const syntax::Token& end = this->expect(TokenKind::r_brace, "expected '}' after struct declaration");
    item.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

syntax::ItemId ItemParser::parse_enum_decl() {
    const syntax::Token& begin = this->expect(TokenKind::kw_enum, "expected 'enum'");
    const syntax::Token& name = this->expect(TokenKind::identifier, "expected enum name");
    std::vector<std::string_view> generic_params;
    if (this->check(TokenKind::less)) {
        generic_params = this->parse_generic_param_list();
    }
    this->expect(TokenKind::colon, "expected ':' after enum name");
    const syntax::TypeId base_type = this->parse_type();
    this->expect(TokenKind::l_brace, "expected '{' after enum base type");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::enum_decl;
    item.name = name.text;
    item.generic_params = std::move(generic_params);
    item.enum_base_type = base_type;

    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        const syntax::Token& case_name = this->expect(TokenKind::identifier, "expected enum case name");
        syntax::TypeId payload_type = syntax::invalid_type_id;
        if (this->match(TokenKind::l_paren)) {
            payload_type = this->parse_type();
            this->expect(TokenKind::r_paren, "expected ')' after enum case payload type");
        }
        this->expect(TokenKind::equal, "expected '=' after enum case name");
        const syntax::Token& value = this->expect(TokenKind::integer_literal, "expected integer literal enum value");
        const syntax::Token& comma = this->expect(TokenKind::comma, "expected ',' after enum case");
        if (case_name.kind == TokenKind::identifier) {
            item.enum_cases.push_back(syntax::EnumCaseDecl {
                case_name.text,
                payload_type,
                value.text,
                this->merge(case_name.range, comma.range),
            });
        }
        this->reset_panic();
    }

    const syntax::Token& end = this->expect(TokenKind::r_brace, "expected '}' after enum declaration");
    item.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

syntax::ItemId ItemParser::parse_opaque_struct_decl() {
    const syntax::Token& begin = this->expect(TokenKind::kw_opaque, "expected 'opaque'");
    this->expect(TokenKind::kw_struct, "expected 'struct' after 'opaque'");
    const syntax::Token& name = this->expect(TokenKind::identifier, "expected opaque struct name");
    const syntax::Token& end = this->expect(TokenKind::semicolon, "expected ';' after opaque struct declaration");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::opaque_struct_decl;
    item.range = this->merge(begin.range, end.range);
    item.name = name.text;
    item.is_extern_c = true;
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

} // namespace aurex::parse
