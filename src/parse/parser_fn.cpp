#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ItemId ItemParser::parse_fn_decl(const bool is_export_c, const bool is_extern_c) {
    const syntax::Token& begin = expect(TokenKind::kw_fn, "expected 'fn'");
    const syntax::Token& name = expect(TokenKind::identifier, "expected function name");
    std::vector<std::string_view> generic_params;
    if (check(TokenKind::less)) {
        generic_params = parse_generic_param_list();
    }
    expect(TokenKind::l_paren, "expected '(' after function name");
    std::vector<syntax::ParamDecl> params;
    bool is_variadic = false;
    if (!check(TokenKind::r_paren)) {
        params = parse_param_list(is_variadic);
    }
    expect(TokenKind::r_paren, "expected ')' after parameter list");
    const syntax::TypeId return_type = parse_optional_return_type();

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::fn_decl;
    item.name = name.text;
    item.generic_params = std::move(generic_params);
    item.params = std::move(params);
    item.return_type = return_type;
    item.is_export_c = is_export_c;
    item.is_extern_c = is_extern_c;
    item.is_variadic = is_variadic;

    parse_optional_abi_name(item);

    if (is_extern_c) {
        const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after extern function declaration");
        item.range = merge(begin.range, end.range);
    } else if (match(TokenKind::semicolon)) {
        item.is_prototype = true;
        item.range = merge(begin.range, previous().range);
    } else {
        item.body = parse_block();
        item.range = syntax::is_valid(item.body) ? merge(begin.range, session_.module.stmts[item.body.value].range) : begin.range;
    }

    reset_panic();
    return session_.module.push_item(std::move(item));
}

std::vector<syntax::ParamDecl> ItemParser::parse_param_list(bool& is_variadic) {
    std::vector<syntax::ParamDecl> params;
    do {
        if (match(TokenKind::ellipsis)) {
            is_variadic = true;
            if (!check(TokenKind::r_paren)) {
                report_here("variadic marker must be last in parameter list");
                while (!is_eof() && !check(TokenKind::r_paren)) {
                    advance();
                }
            }
            break;
        }
        const syntax::Token& name = expect(TokenKind::identifier, "expected parameter name");
        expect(TokenKind::colon, "expected ':' after parameter name");
        const syntax::TypeId type = parse_type();
        if (name.kind == TokenKind::identifier) {
            params.push_back(syntax::ParamDecl {name.text, type, merge(name.range, session_.module.types[type.value].range)});
        }
        reset_panic();
        if (check(TokenKind::r_paren)) {
            break;
        }
    } while (match(TokenKind::comma) && !check(TokenKind::r_paren));
    return params;
}

syntax::TypeId ItemParser::parse_optional_return_type() {
    if (!match(TokenKind::arrow)) {
        return syntax::invalid_type_id;
    }
    return parse_type();
}

void ItemParser::parse_optional_abi_name(syntax::ItemNode& item) {
    if (!match(TokenKind::at)) {
        return;
    }
    const syntax::Token& attr = expect(TokenKind::identifier, "expected ABI attribute name");
    if (attr.text != "name") {
        report_at(attr, "expected ABI attribute 'name'");
    }
    expect(TokenKind::l_paren, "expected '(' after ABI attribute");
    const syntax::Token& value = expect(TokenKind::string_literal, "expected string literal in ABI name");
    if (value.text.size() >= 2) {
        item.abi_name = value.text.substr(1, value.text.size() - 2);
    }
    expect(TokenKind::r_paren, "expected ')' after ABI attribute");
    reset_panic();
}

} // namespace aurex::parse
