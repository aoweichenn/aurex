#include <aurex/frontend/parse/parser_item_part.hpp>
#include <aurex/frontend/parse/parser_messages.hpp>
#include <aurex/frontend/parse/recovery.hpp>

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ItemId ItemParser::parse_item()
{
    this->reset_panic();
    ParsedFunctionAttributes function_attributes;
    this->parse_optional_function_decorators(function_attributes);
    const ParsedVisibility visibility = this->parse_visibility();
    this->parse_optional_function_decorators(function_attributes);
    if (this->check(TokenKind::kw_const)) {
        this->report_misplaced_function_decorators(function_attributes);
        const syntax::ItemId id = this->parse_const_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
        }
        return id;
    }
    if (this->check(TokenKind::kw_type)) {
        this->report_misplaced_function_decorators(function_attributes);
        const syntax::ItemId id = this->parse_type_alias_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
        }
        return id;
    }
    if (this->check(TokenKind::kw_struct)) {
        this->report_misplaced_function_decorators(function_attributes);
        const syntax::ItemId id = this->parse_struct_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
        }
        return id;
    }
    if (this->check(TokenKind::kw_enum)) {
        this->report_misplaced_function_decorators(function_attributes);
        const syntax::ItemId id = this->parse_enum_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
        }
        return id;
    }
    if (this->check(TokenKind::kw_trait)) {
        this->report_misplaced_function_decorators(function_attributes);
        const syntax::ItemId id = this->parse_trait_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
        }
        return id;
    }
    if (this->check(TokenKind::kw_impl)) {
        this->report_misplaced_function_decorators(function_attributes);
        if (visibility.explicit_visibility && syntax::visibility_is_module_private(visibility.visibility)) {
            this->report_here(std::string(PARSER_IMPL_PRIVATE_UNSUPPORTED));
        }
        return this->parse_impl_block();
    }
    if (this->check(TokenKind::kw_opaque)) {
        this->report_misplaced_function_decorators(function_attributes);
        const syntax::ItemId id = this->parse_opaque_struct_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
        }
        return id;
    }
    if (this->check(TokenKind::kw_extern)) {
        this->report_misplaced_function_decorators(function_attributes);
        if (visibility.explicit_visibility && syntax::visibility_is_module_private(visibility.visibility)) {
            this->report_here(std::string(PARSER_EXTERN_PRIVATE_UNSUPPORTED));
        }
        return this->parse_extern_block();
    }
    if (this->check(TokenKind::kw_export)) {
        if (visibility.explicit_visibility && syntax::visibility_is_module_private(visibility.visibility)) {
            this->report_here(std::string(PARSER_EXPORT_C_PRIVATE_UNSUPPORTED));
        }
        const syntax::Token& begin = this->advance();
        const base::usize range_begin =
            function_attributes.present ? function_attributes.range.begin : begin.range.begin;
        this->expect_contextual_c_keyword(std::string(PARSER_EXPECT_EXPORT_C_KEYWORD));
        const bool is_unsafe = this->check(TokenKind::kw_unsafe);
        if (!this->check(TokenKind::kw_fn) && !(is_unsafe && this->check_next(TokenKind::kw_fn))) {
            this->report_here(std::string(PARSER_EXPECT_EXPORT_C_FN));
            return syntax::INVALID_ITEM_ID;
        }
        const syntax::ItemId id = this->parse_fn_decl(true, false, is_unsafe, std::move(function_attributes));
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_range_begin(id.value, range_begin);
            this->session_.module.items.set_visibility(id.value, syntax::Visibility::public_);
        }
        return id;
    }
    if (this->check(TokenKind::kw_unsafe)) {
        if (!this->check_next(TokenKind::kw_fn)) {
            this->advance();
            this->report_here(std::string(PARSER_EXPECT_FN_AFTER_UNSAFE));
            this->synchronize(RecoveryContext::item);
            return syntax::INVALID_ITEM_ID;
        }
        const syntax::ItemId id = this->parse_fn_decl(false, false, true, std::move(function_attributes));
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
        }
        return id;
    }
    if (this->check(TokenKind::kw_fn)) {
        const syntax::ItemId id = this->parse_fn_decl(false, false, false, std::move(function_attributes));
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
        }
        return id;
    }

    this->report_misplaced_function_decorators(function_attributes);
    this->report_here(std::string(PARSER_EXPECT_ITEM_DECLARATION));
    return syntax::INVALID_ITEM_ID;
}

syntax::ItemId ItemParser::parse_const_decl()
{
    const syntax::Token& begin = this->expect(TokenKind::kw_const, std::string(PARSER_EXPECT_CONST_KEYWORD));
    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_CONST_NAME));
    this->expect_type_annotation_colon(std::string(PARSER_EXPECT_CONST_TYPE_COLON));
    const syntax::TypeId type = this->parse_type();
    this->expect_initializer_equal(std::string(PARSER_EXPECT_CONST_INITIALIZER_EQUAL));
    const syntax::ExprId value = this->parse_expr();
    const syntax::Token& end = this->expect_item_terminator(std::string(PARSER_EXPECT_CONST_TERMINATOR));

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::const_decl;
    item.range = this->merge(begin.range, end.range);
    item.name = name.text();
    item.const_type = type;
    item.const_value = value;
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

syntax::ItemId ItemParser::parse_type_alias_decl()
{
    const syntax::Token& begin = this->expect(TokenKind::kw_type, std::string(PARSER_EXPECT_TYPE_KEYWORD));
    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_TYPE_ALIAS_NAME));
    std::vector<syntax::GenericParamDecl> generic_params = this->parse_optional_generic_params();
    std::vector<syntax::GenericConstraintDecl> where_constraints = this->parse_optional_where_constraints();
    this->expect_initializer_equal(std::string(PARSER_EXPECT_TYPE_ALIAS_INITIALIZER_EQUAL));
    const syntax::TypeId target = this->parse_type();
    const syntax::Token& end = this->expect_item_terminator(std::string(PARSER_EXPECT_TYPE_ALIAS_TERMINATOR));

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::type_alias;
    item.range = this->merge(begin.range, end.range);
    item.name = name.text();
    item.generic_params = std::move(generic_params);
    item.where_constraints = std::move(where_constraints);
    item.alias_type = target;
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

syntax::ItemId ItemParser::parse_trait_associated_type_decl(const ParsedVisibility visibility)
{
    const syntax::Token& begin = this->expect(TokenKind::kw_type, std::string(PARSER_EXPECT_TYPE_KEYWORD));
    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_TYPE_ALIAS_NAME));
    std::vector<syntax::GenericParamDecl> generic_params = this->parse_optional_generic_params();
    std::vector<syntax::GenericConstraintDecl> where_constraints = this->parse_optional_where_constraints();
    syntax::TypeId target = syntax::INVALID_TYPE_ID;
    if (this->match(TokenKind::equal)) {
        target = this->parse_type();
    }
    const syntax::Token& end =
        this->expect_item_terminator(std::string(PARSER_EXPECT_TRAIT_ASSOCIATED_TYPE_TERMINATOR));

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::type_alias;
    item.range = this->merge(begin.range, end.range);
    item.name = name.text();
    item.generic_params = std::move(generic_params);
    item.where_constraints = std::move(where_constraints);
    item.alias_type = target;
    item.visibility = visibility.explicit_visibility ? visibility.visibility : syntax::Visibility::public_;
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

syntax::ItemId ItemParser::parse_impl_associated_type_decl(
    const ParsedVisibility visibility, const syntax::TypeId impl_type, const syntax::TypeId trait_type)
{
    const syntax::Token& begin = this->expect(TokenKind::kw_type, std::string(PARSER_EXPECT_TYPE_KEYWORD));
    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_TYPE_ALIAS_NAME));
    std::vector<syntax::GenericParamDecl> generic_params = this->parse_optional_generic_params();
    std::vector<syntax::GenericConstraintDecl> where_constraints = this->parse_optional_where_constraints();
    this->expect_initializer_equal(std::string(PARSER_EXPECT_TYPE_ALIAS_INITIALIZER_EQUAL));
    const syntax::TypeId target = this->parse_type();
    const syntax::Token& end = this->expect_item_terminator(std::string(PARSER_EXPECT_IMPL_ASSOCIATED_TYPE_TERMINATOR));

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::type_alias;
    item.range = this->merge(begin.range, end.range);
    item.name = name.text();
    item.generic_params = std::move(generic_params);
    item.where_constraints = std::move(where_constraints);
    item.alias_type = target;
    item.impl_type = impl_type;
    item.trait_type = trait_type;
    item.visibility = syntax::is_valid(trait_type) && !visibility.explicit_visibility ? syntax::Visibility::public_
                                                                                      : visibility.visibility;
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

} // namespace aurex::parse
