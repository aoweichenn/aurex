#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ItemId ItemParser::parse_item() {
    reset_panic();
    const syntax::Visibility visibility = parse_visibility();
    if (check(TokenKind::kw_const)) {
        const syntax::ItemId id = parse_const_decl();
        if (syntax::is_valid(id)) {
            session_.module.items[id.value].visibility = visibility;
        }
        return id;
    }
    if (check(TokenKind::kw_type)) {
        const syntax::ItemId id = parse_type_alias_decl();
        if (syntax::is_valid(id)) {
            session_.module.items[id.value].visibility = visibility;
        }
        return id;
    }
    if (check(TokenKind::kw_struct) || check(TokenKind::kw_noncopy)) {
        const syntax::ItemId id = parse_struct_decl();
        if (syntax::is_valid(id)) {
            session_.module.items[id.value].visibility = visibility;
        }
        return id;
    }
    if (check(TokenKind::kw_enum)) {
        const syntax::ItemId id = parse_enum_decl();
        if (syntax::is_valid(id)) {
            session_.module.items[id.value].visibility = visibility;
        }
        return id;
    }
    if (check(TokenKind::kw_impl)) {
        if (visibility == syntax::Visibility::private_) {
            report_here("impl block cannot be private");
        }
        return parse_impl_block();
    }
    if (check(TokenKind::kw_opaque)) {
        const syntax::ItemId id = parse_opaque_struct_decl();
        if (syntax::is_valid(id)) {
            session_.module.items[id.value].visibility = visibility;
        }
        return id;
    }
    if (check(TokenKind::kw_extern)) {
        if (visibility == syntax::Visibility::private_) {
            report_here("extern block cannot be private");
        }
        return parse_extern_block();
    }
    if (check(TokenKind::kw_export)) {
        if (visibility == syntax::Visibility::private_) {
            report_here("exported C function cannot be private");
        }
        const syntax::Token& begin = advance();
        expect(TokenKind::kw_c, "expected 'c' after 'export'");
        if (!check(TokenKind::kw_fn)) {
            report_here("expected function declaration after 'export c'");
            return syntax::invalid_item_id;
        }
        syntax::ItemId id = parse_fn_decl(true, false);
        if (syntax::is_valid(id)) {
            session_.module.items[id.value].range.begin = begin.range.begin;
            session_.module.items[id.value].visibility = syntax::Visibility::public_;
        }
        return id;
    }
    if (check(TokenKind::kw_fn)) {
        const syntax::ItemId id = parse_fn_decl(false, false);
        if (syntax::is_valid(id)) {
            session_.module.items[id.value].visibility = visibility;
        }
        return id;
    }

    report_here("expected item declaration");
    return syntax::invalid_item_id;
}

syntax::ItemId ItemParser::parse_const_decl() {
    const syntax::Token& begin = expect(TokenKind::kw_const, "expected 'const'");
    const syntax::Token& name = expect(TokenKind::identifier, "expected const name");
    expect(TokenKind::colon, "expected ':' after const name");
    const syntax::TypeId type = parse_type();
    expect(TokenKind::equal, "expected '=' in const declaration");
    const syntax::ExprId value = parse_expr();
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after const declaration");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::const_decl;
    item.range = merge(begin.range, end.range);
    item.name = name.text;
    item.const_type = type;
    item.const_value = value;
    reset_panic();
    return session_.module.push_item(std::move(item));
}

syntax::ItemId ItemParser::parse_type_alias_decl() {
    const syntax::Token& begin = expect(TokenKind::kw_type, "expected 'type'");
    const syntax::Token& name = expect(TokenKind::identifier, "expected type alias name");
    expect(TokenKind::equal, "expected '=' in type alias declaration");
    const syntax::TypeId target = parse_type();
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after type alias declaration");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::type_alias;
    item.range = merge(begin.range, end.range);
    item.name = name.text;
    item.alias_type = target;
    reset_panic();
    return session_.module.push_item(std::move(item));
}

std::vector<std::string_view> ItemParser::parse_generic_param_list() {
    std::vector<std::string_view> params;
    expect(TokenKind::less, "expected '<' before generic parameter list");
    if (!check(TokenKind::greater)) {
        do {
            const syntax::Token& name = expect(TokenKind::identifier, "expected generic parameter name");
            if (name.kind == TokenKind::identifier) {
                params.push_back(name.text);
            }
            reset_panic();
            if (check(TokenKind::greater)) {
                break;
            }
        } while (match(TokenKind::comma) && !check(TokenKind::greater));
    }
    expect(TokenKind::greater, "expected '>' after generic parameter list");
    reset_panic();
    return params;
}

} // namespace aurex::parse
