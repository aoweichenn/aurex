#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ItemId ItemParser::parse_impl_block() {
    const syntax::Token& begin = expect(TokenKind::kw_impl, "expected 'impl'");
    std::vector<std::string_view> generic_params;
    if (check(TokenKind::less)) {
        generic_params = parse_generic_param_list();
    }
    const syntax::TypeId impl_type = parse_type();
    expect(TokenKind::l_brace, "expected '{' after impl type");

    syntax::ItemNode block;
    block.kind = syntax::ItemKind::impl_block;
    block.generic_params = generic_params;
    block.impl_type = impl_type;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        const syntax::Visibility visibility = parse_visibility();
        if (!check(TokenKind::kw_fn)) {
            report_here("expected function declaration in impl block");
            synchronize();
            reset_panic();
            continue;
        }
        const syntax::ItemId method = parse_fn_decl(false, false);
        if (syntax::is_valid(method)) {
            std::vector<std::string_view> method_params = generic_params;
            method_params.insert(
                method_params.end(),
                session_.module.items[method.value].generic_params.begin(),
                session_.module.items[method.value].generic_params.end()
            );
            session_.module.items[method.value].visibility = visibility;
            session_.module.items[method.value].generic_params = std::move(method_params);
            session_.module.items[method.value].impl_generic_param_count = generic_params.size();
            session_.module.items[method.value].impl_type = impl_type;
            block.impl_items.push_back(method);
        }
        reset_panic();
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after impl block");
    block.range = merge(begin.range, end.range);
    reset_panic();
    return session_.module.push_item(std::move(block));
}

syntax::ItemId ItemParser::parse_extern_block() {
    const syntax::Token& begin = expect(TokenKind::kw_extern, "expected 'extern'");
    expect(TokenKind::kw_c, "expected 'c' after 'extern'");
    expect(TokenKind::l_brace, "expected '{' after 'extern c'");

    syntax::ItemNode block;
    block.kind = syntax::ItemKind::extern_block;
    block.is_extern_c = true;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        if (check(TokenKind::kw_fn)) {
            const syntax::ItemId item = parse_fn_decl(false, true);
            if (syntax::is_valid(item)) {
                block.extern_items.push_back(item);
            }
        } else if (check(TokenKind::kw_opaque)) {
            const syntax::ItemId item = parse_opaque_struct_decl();
            if (syntax::is_valid(item)) {
                block.extern_items.push_back(item);
            }
        } else {
            report_here("expected extern item");
            synchronize();
        }
        reset_panic();
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after extern block");
    block.range = merge(begin.range, end.range);
    reset_panic();
    return session_.module.push_item(std::move(block));
}

} // namespace aurex::parse
