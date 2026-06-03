#include <aurex/parse/parser_item_part.hpp>
#include <aurex/parse/parser_messages.hpp>

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ItemId ItemParser::parse_impl_block()
{
    const syntax::Token& begin = this->expect(TokenKind::kw_impl, std::string(PARSER_EXPECT_IMPL_KEYWORD));
    std::vector<syntax::GenericParamDecl> generic_params = this->parse_optional_generic_params();
    const syntax::TypeId first_type = this->parse_type();
    syntax::TypeId trait_type = syntax::INVALID_TYPE_ID;
    syntax::TypeId impl_type = first_type;
    if (this->match(TokenKind::kw_for)) {
        trait_type = first_type;
        impl_type = this->parse_type();
    }
    std::vector<syntax::GenericConstraintDecl> where_constraints = this->parse_optional_where_constraints();
    this->expect_item_container_start(std::string(PARSER_EXPECT_IMPL_BODY));

    syntax::ItemNode block;
    block.kind = syntax::ItemKind::impl_block;
    block.generic_params = generic_params;
    block.where_constraints = where_constraints;
    block.impl_type = impl_type;
    block.trait_type = trait_type;

    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        ParsedFunctionAttributes function_attributes;
        this->parse_optional_function_decorators(function_attributes);
        const ParsedVisibility visibility = this->parse_visibility();
        this->parse_optional_function_decorators(function_attributes);
        const bool is_unsafe = this->check(TokenKind::kw_unsafe);
        if (this->check(TokenKind::kw_type) && syntax::is_valid(trait_type)) {
            this->report_misplaced_function_decorators(function_attributes);
            const syntax::ItemId associated_type =
                this->parse_impl_associated_type_decl(visibility, impl_type, trait_type);
            if (syntax::is_valid(associated_type)) {
                block.impl_items.push_back(associated_type);
            }
            this->reset_panic();
            continue;
        }
        if (!this->check(TokenKind::kw_fn) && !(is_unsafe && this->check_next(TokenKind::kw_fn))) {
            this->report_misplaced_function_decorators(function_attributes);
            this->report_here(
                std::string(syntax::is_valid(trait_type) ? PARSER_EXPECT_IMPL_ITEM : PARSER_EXPECT_IMPL_FN));
            this->synchronize(RecoveryContext::item);
            this->reset_panic();
            continue;
        }
        const syntax::ItemId method = this->parse_fn_decl(false, false, is_unsafe, std::move(function_attributes));
        if (syntax::is_valid(method)) {
            syntax::ItemNode method_item = this->session_.module.items[method.value];
            method_item.visibility = syntax::is_valid(trait_type) && !visibility.explicit_visibility
                ? syntax::Visibility::public_
                : visibility.visibility;
            method_item.impl_type = impl_type;
            method_item.trait_type = trait_type;
            if (!generic_params.empty()) {
                method_item.generic_params.insert(
                    method_item.generic_params.begin(), generic_params.begin(), generic_params.end());
            }
            if (!where_constraints.empty()) {
                method_item.where_constraints.insert(
                    method_item.where_constraints.begin(), where_constraints.begin(), where_constraints.end());
            }
            this->session_.module.set_item(method.value, std::move(method_item));
            block.impl_items.push_back(method);
        }
        this->reset_panic();
    }

    const syntax::Token& end = this->expect_item_container_end(std::string(PARSER_EXPECT_IMPL_END));
    block.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_item(std::move(block));
}

syntax::ItemId ItemParser::parse_extern_block()
{
    const syntax::Token& begin = this->expect(TokenKind::kw_extern, std::string(PARSER_EXPECT_EXTERN_KEYWORD));
    this->expect_contextual_c_keyword(std::string(PARSER_EXPECT_C_AFTER_EXTERN));
    this->expect_item_container_start(std::string(PARSER_EXPECT_EXTERN_BODY));

    syntax::ItemNode block;
    block.kind = syntax::ItemKind::extern_block;
    block.is_extern_c = true;

    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        ParsedFunctionAttributes function_attributes;
        this->parse_optional_function_decorators(function_attributes);
        const bool is_unsafe = this->check(TokenKind::kw_unsafe);
        if (this->check(TokenKind::kw_fn) || (is_unsafe && this->check_next(TokenKind::kw_fn))) {
            const syntax::ItemId item = this->parse_fn_decl(false, true, is_unsafe, std::move(function_attributes));
            if (syntax::is_valid(item)) {
                block.extern_items.push_back(item);
            }
        } else if (this->check(TokenKind::kw_opaque)) {
            this->report_misplaced_function_decorators(function_attributes);
            const syntax::ItemId item = this->parse_opaque_struct_decl();
            if (syntax::is_valid(item)) {
                block.extern_items.push_back(item);
            }
        } else {
            this->report_misplaced_function_decorators(function_attributes);
            this->report_here(std::string(PARSER_EXPECT_EXTERN_ITEM));
            this->synchronize(RecoveryContext::item);
        }
        this->reset_panic();
    }

    const syntax::Token& end = this->expect_item_container_end(std::string(PARSER_EXPECT_EXTERN_END));
    block.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_item(std::move(block));
}

} // namespace aurex::parse
