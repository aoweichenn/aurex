#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ItemId ItemParser::parse_impl_block() {
    const syntax::Token& begin = this->expect(TokenKind::kw_impl, "expected 'impl'");
    std::vector<std::string_view> generic_params;
    if (this->check(TokenKind::less)) {
        generic_params = this->parse_generic_param_list();
    }
    const syntax::TypeId impl_type = this->parse_type();
    this->expect(TokenKind::l_brace, "expected '{' after impl type");

    syntax::ItemNode block;
    block.kind = syntax::ItemKind::impl_block;
    block.generic_params = generic_params;
    block.impl_type = impl_type;

    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        const syntax::Visibility visibility = this->parse_visibility();
        if (!this->check(TokenKind::kw_fn)) {
            this->report_here("expected function declaration in impl block");
            this->synchronize(RecoveryContext::item);
            this->reset_panic();
            continue;
        }
        const syntax::ItemId method = this->parse_fn_decl(false, false);
        if (syntax::is_valid(method)) {
            std::vector<std::string_view> method_params = generic_params;
            method_params.insert(
                method_params.end(),
                this->session_.module.items[method.value].generic_params.begin(),
                this->session_.module.items[method.value].generic_params.end()
            );
            this->session_.module.items[method.value].visibility = visibility;
            this->session_.module.items[method.value].generic_params = std::move(method_params);
            this->session_.module.items[method.value].impl_generic_param_count = generic_params.size();
            this->session_.module.items[method.value].impl_type = impl_type;
            block.impl_items.push_back(method);
        }
        this->reset_panic();
    }

    const syntax::Token& end = this->expect(TokenKind::r_brace, "expected '}' after impl block");
    block.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_item(std::move(block));
}

syntax::ItemId ItemParser::parse_extern_block() {
    const syntax::Token& begin = this->expect(TokenKind::kw_extern, "expected 'extern'");
    this->expect(TokenKind::kw_c, "expected 'c' after 'extern'");
    this->expect(TokenKind::l_brace, "expected '{' after 'extern c'");

    syntax::ItemNode block;
    block.kind = syntax::ItemKind::extern_block;
    block.is_extern_c = true;

    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        if (this->check(TokenKind::kw_fn)) {
            const syntax::ItemId item = this->parse_fn_decl(false, true);
            if (syntax::is_valid(item)) {
                block.extern_items.push_back(item);
            }
        } else if (this->check(TokenKind::kw_opaque)) {
            const syntax::ItemId item = this->parse_opaque_struct_decl();
            if (syntax::is_valid(item)) {
                block.extern_items.push_back(item);
            }
        } else {
            this->report_here("expected extern item");
            this->synchronize(RecoveryContext::item);
        }
        this->reset_panic();
    }

    const syntax::Token& end = this->expect(TokenKind::r_brace, "expected '}' after extern block");
    block.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_item(std::move(block));
}

} // namespace aurex::parse
