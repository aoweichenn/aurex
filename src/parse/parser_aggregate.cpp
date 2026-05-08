#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ItemId ItemParser::parse_struct_decl() {
    const bool is_noncopy = match(TokenKind::kw_noncopy);
    const syntax::Token& begin = is_noncopy ? previous() : peek();
    expect(TokenKind::kw_struct, is_noncopy ? "expected 'struct' after 'noncopy'" : "expected 'struct'");
    const syntax::Token& name = expect(TokenKind::identifier, "expected struct name");
    std::vector<std::string_view> generic_params;
    if (check(TokenKind::less)) {
        generic_params = parse_generic_param_list();
    }
    expect(TokenKind::l_brace, "expected '{' after struct name");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::struct_decl;
    item.name = name.text;
    item.generic_params = std::move(generic_params);
    item.is_noncopy = is_noncopy;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        const syntax::Visibility field_visibility = parse_visibility();
        const syntax::Token& field_name = expect(TokenKind::identifier, "expected field name");
        expect(TokenKind::colon, "expected ':' after field name");
        const syntax::TypeId field_type = parse_type();
        const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after field declaration");
        if (field_name.kind == TokenKind::identifier) {
            item.fields.push_back(syntax::FieldDecl {
                field_name.text,
                field_type,
                merge(field_name.range, end.range),
                field_visibility,
            });
        }
        reset_panic();
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after struct declaration");
    item.range = merge(begin.range, end.range);
    reset_panic();
    return session_.module.push_item(std::move(item));
}

syntax::ItemId ItemParser::parse_enum_decl() {
    const syntax::Token& begin = expect(TokenKind::kw_enum, "expected 'enum'");
    const syntax::Token& name = expect(TokenKind::identifier, "expected enum name");
    std::vector<std::string_view> generic_params;
    if (check(TokenKind::less)) {
        generic_params = parse_generic_param_list();
    }
    expect(TokenKind::colon, "expected ':' after enum name");
    const syntax::TypeId base_type = parse_type();
    expect(TokenKind::l_brace, "expected '{' after enum base type");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::enum_decl;
    item.name = name.text;
    item.generic_params = std::move(generic_params);
    item.enum_base_type = base_type;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        const syntax::Token& case_name = expect(TokenKind::identifier, "expected enum case name");
        syntax::TypeId payload_type = syntax::invalid_type_id;
        if (match(TokenKind::l_paren)) {
            payload_type = parse_type();
            expect(TokenKind::r_paren, "expected ')' after enum case payload type");
        }
        expect(TokenKind::equal, "expected '=' after enum case name");
        const syntax::Token& value = expect(TokenKind::integer_literal, "expected integer literal enum value");
        const syntax::Token& comma = expect(TokenKind::comma, "expected ',' after enum case");
        if (case_name.kind == TokenKind::identifier) {
            item.enum_cases.push_back(syntax::EnumCaseDecl {
                case_name.text,
                payload_type,
                value.text,
                merge(case_name.range, comma.range),
            });
        }
        reset_panic();
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after enum declaration");
    item.range = merge(begin.range, end.range);
    reset_panic();
    return session_.module.push_item(std::move(item));
}

syntax::ItemId ItemParser::parse_opaque_struct_decl() {
    const syntax::Token& begin = expect(TokenKind::kw_opaque, "expected 'opaque'");
    expect(TokenKind::kw_struct, "expected 'struct' after 'opaque'");
    const syntax::Token& name = expect(TokenKind::identifier, "expected opaque struct name");
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after opaque struct declaration");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::opaque_struct_decl;
    item.range = merge(begin.range, end.range);
    item.name = name.text;
    item.is_extern_c = true;
    reset_panic();
    return session_.module.push_item(std::move(item));
}

} // namespace aurex::parse
