#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ItemId ItemParser::parse_item() {
    this->reset_panic();
    const syntax::Visibility visibility = this->parse_visibility();
    if (this->check(TokenKind::kw_const)) {
        const syntax::ItemId id = this->parse_const_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items[id.value].visibility = visibility;
        }
        return id;
    }
    if (this->check(TokenKind::kw_type)) {
        const syntax::ItemId id = this->parse_type_alias_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items[id.value].visibility = visibility;
        }
        return id;
    }
    if (this->check(TokenKind::kw_struct) || this->check(TokenKind::kw_noncopy)) {
        const syntax::ItemId id = this->parse_struct_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items[id.value].visibility = visibility;
        }
        return id;
    }
    if (this->check(TokenKind::kw_enum)) {
        const syntax::ItemId id = this->parse_enum_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items[id.value].visibility = visibility;
        }
        return id;
    }
    if (this->check(TokenKind::kw_impl)) {
        if (visibility == syntax::Visibility::private_) {
            this->report_here("impl block cannot be private");
        }
        return this->parse_impl_block();
    }
    if (this->check(TokenKind::kw_opaque)) {
        const syntax::ItemId id = this->parse_opaque_struct_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items[id.value].visibility = visibility;
        }
        return id;
    }
    if (this->check(TokenKind::kw_extern)) {
        if (visibility == syntax::Visibility::private_) {
            this->report_here("extern block cannot be private");
        }
        return this->parse_extern_block();
    }
    if (this->check(TokenKind::kw_export)) {
        if (visibility == syntax::Visibility::private_) {
            this->report_here("exported C function cannot be private");
        }
        const syntax::Token& begin = this->advance();
        this->expect(TokenKind::kw_c, "expected 'c' after 'export'");
        if (!this->check(TokenKind::kw_fn)) {
            this->report_here("expected function declaration after 'export c'");
            return syntax::invalid_item_id;
        }
        syntax::ItemId id = this->parse_fn_decl(true, false);
        if (syntax::is_valid(id)) {
            this->session_.module.items[id.value].range.begin = begin.range.begin;
            this->session_.module.items[id.value].visibility = syntax::Visibility::public_;
        }
        return id;
    }
    if (this->check(TokenKind::kw_fn)) {
        const syntax::ItemId id = this->parse_fn_decl(false, false);
        if (syntax::is_valid(id)) {
            this->session_.module.items[id.value].visibility = visibility;
        }
        return id;
    }

    this->report_here("expected item declaration");
    return syntax::invalid_item_id;
}

syntax::ItemId ItemParser::parse_const_decl() {
    const syntax::Token& begin = this->expect(TokenKind::kw_const, "expected 'const'");
    const syntax::Token& name = this->expect(TokenKind::identifier, "expected const name");
    this->expect(TokenKind::colon, "expected ':' after const name");
    const syntax::TypeId type = this->parse_type();
    this->expect(TokenKind::equal, "expected '=' in const declaration");
    const syntax::ExprId value = this->parse_expr();
    const syntax::Token& end = this->expect(TokenKind::semicolon, "expected ';' after const declaration");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::const_decl;
    item.range = this->merge(begin.range, end.range);
    item.name = name.text;
    item.const_type = type;
    item.const_value = value;
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

syntax::ItemId ItemParser::parse_type_alias_decl() {
    const syntax::Token& begin = this->expect(TokenKind::kw_type, "expected 'type'");
    const syntax::Token& name = this->expect(TokenKind::identifier, "expected type alias name");
    this->expect(TokenKind::equal, "expected '=' in type alias declaration");
    const syntax::TypeId target = this->parse_type();
    const syntax::Token& end = this->expect(TokenKind::semicolon, "expected ';' after type alias declaration");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::type_alias;
    item.range = this->merge(begin.range, end.range);
    item.name = name.text;
    item.alias_type = target;
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

std::vector<std::string_view> ItemParser::parse_generic_param_list() {
    std::vector<std::string_view> params;
    this->expect(TokenKind::less, "expected '<' before generic parameter list");
    if (!this->check(TokenKind::greater)) {
        do {
            const syntax::Token& name = this->expect(TokenKind::identifier, "expected generic parameter name");
            if (name.kind == TokenKind::identifier) {
                params.push_back(name.text);
            }
            this->reset_panic();
            if (this->check(TokenKind::greater)) {
                break;
            }
        } while (this->match(TokenKind::comma) && !this->check(TokenKind::greater));
    }
    this->expect(TokenKind::greater, "expected '>' after generic parameter list");
    this->reset_panic();
    return params;
}

} // namespace aurex::parse
