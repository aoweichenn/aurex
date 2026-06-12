#include <aurex/frontend/parse/parser_item_part.hpp>
#include <aurex/frontend/parse/parser_messages.hpp>
#include <aurex/frontend/parse/recovery.hpp>

#include <algorithm>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

constexpr std::string_view PARSER_MACRO_CONTEXTUAL_KEYWORD_TEXT = "macro";
constexpr std::string_view PARSER_MACRO_DERIVE_CONTEXTUAL_KEYWORD_TEXT = "derive";
constexpr std::string_view PARSER_MACRO_MATCH_CLAUSE_KEYWORD_TEXT = "match";

[[nodiscard]] syntax::AttributeTokenTreeGroupKind attribute_group_kind(const TokenKind kind) noexcept
{
    switch (kind) {
        case TokenKind::l_paren:
        case TokenKind::r_paren:
            return syntax::AttributeTokenTreeGroupKind::paren;
        case TokenKind::l_bracket:
        case TokenKind::r_bracket:
            return syntax::AttributeTokenTreeGroupKind::bracket;
        case TokenKind::l_brace:
        case TokenKind::r_brace:
            return syntax::AttributeTokenTreeGroupKind::brace;
        default:
            return syntax::AttributeTokenTreeGroupKind::none;
    }
}

[[nodiscard]] bool token_opens_attribute_group(const TokenKind kind) noexcept
{
    return kind == TokenKind::l_paren || kind == TokenKind::l_bracket || kind == TokenKind::l_brace;
}

[[nodiscard]] bool token_closes_attribute_group(const TokenKind kind) noexcept
{
    return kind == TokenKind::r_paren || kind == TokenKind::r_bracket || kind == TokenKind::r_brace;
}

[[nodiscard]] bool token_closes_attribute_group(
    const TokenKind kind, const syntax::AttributeTokenTreeGroupKind group) noexcept
{
    switch (group) {
        case syntax::AttributeTokenTreeGroupKind::paren:
            return kind == TokenKind::r_paren;
        case syntax::AttributeTokenTreeGroupKind::bracket:
            return kind == TokenKind::r_bracket;
        case syntax::AttributeTokenTreeGroupKind::brace:
            return kind == TokenKind::r_brace;
        case syntax::AttributeTokenTreeGroupKind::none:
            return false;
    }
    return false;
}

[[nodiscard]] bool token_is_contextual_keyword(const syntax::Token& token, const std::string_view text) noexcept
{
    return token.kind == TokenKind::identifier && token.text() == text;
}

[[nodiscard]] bool token_is_macro_item_start(const syntax::Token& token) noexcept
{
    return token_is_contextual_keyword(token, PARSER_MACRO_CONTEXTUAL_KEYWORD_TEXT);
}

void include_item_attribute_range(ParsedItemAttributes& attributes, const base::SourceRange& range) noexcept
{
    if (!attributes.present) {
        attributes.range = range;
        attributes.present = true;
        return;
    }
    attributes.range.begin = std::min(attributes.range.begin, range.begin);
    attributes.range.end = std::max(attributes.range.end, range.end);
}

} // namespace

syntax::ItemId ItemParser::parse_item()
{
    this->reset_panic();
    ParsedItemAttributes item_attributes;
    this->parse_optional_item_attributes(item_attributes);
    ParsedFunctionAttributes function_attributes;
    this->parse_optional_function_decorators(function_attributes);
    const ParsedVisibility visibility = this->parse_visibility();
    this->parse_optional_item_attributes(item_attributes);
    this->parse_optional_function_decorators(function_attributes);
    if (this->check(TokenKind::kw_const)) {
        this->report_misplaced_function_decorators(function_attributes);
        const syntax::ItemId id = this->parse_const_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
            this->apply_item_attributes(id, item_attributes);
        }
        return id;
    }
    if (this->check(TokenKind::kw_type)) {
        this->report_misplaced_function_decorators(function_attributes);
        const syntax::ItemId id = this->parse_type_alias_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
            this->apply_item_attributes(id, item_attributes);
        }
        return id;
    }
    if (this->check(TokenKind::kw_struct)) {
        this->report_misplaced_function_decorators(function_attributes);
        const syntax::ItemId id = this->parse_struct_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
            this->apply_item_attributes(id, item_attributes);
        }
        return id;
    }
    if (this->check(TokenKind::kw_enum)) {
        this->report_misplaced_function_decorators(function_attributes);
        const syntax::ItemId id = this->parse_enum_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
            this->apply_item_attributes(id, item_attributes);
        }
        return id;
    }
    if (this->check(TokenKind::kw_trait)) {
        this->report_misplaced_function_decorators(function_attributes);
        const syntax::ItemId id = this->parse_trait_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
            this->apply_item_attributes(id, item_attributes);
        }
        return id;
    }
    if (this->check(TokenKind::kw_impl)) {
        this->report_misplaced_function_decorators(function_attributes);
        if (visibility.explicit_visibility && syntax::visibility_is_module_private(visibility.visibility)) {
            this->report_here(std::string(PARSER_IMPL_PRIVATE_UNSUPPORTED));
        }
        const syntax::ItemId id = this->parse_impl_block();
        if (syntax::is_valid(id)) {
            this->apply_item_attributes(id, item_attributes);
        }
        return id;
    }
    if (this->check(TokenKind::kw_opaque)) {
        this->report_misplaced_function_decorators(function_attributes);
        const syntax::ItemId id = this->parse_opaque_struct_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
            this->apply_item_attributes(id, item_attributes);
        }
        return id;
    }
    if (this->check(TokenKind::kw_extern)) {
        this->report_misplaced_function_decorators(function_attributes);
        if (visibility.explicit_visibility && syntax::visibility_is_module_private(visibility.visibility)) {
            this->report_here(std::string(PARSER_EXTERN_PRIVATE_UNSUPPORTED));
        }
        const syntax::ItemId id = this->parse_extern_block();
        if (syntax::is_valid(id)) {
            this->apply_item_attributes(id, item_attributes);
        }
        return id;
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
            this->apply_item_attributes(id, item_attributes);
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
            this->apply_item_attributes(id, item_attributes);
        }
        return id;
    }
    if (this->check(TokenKind::kw_fn)) {
        const syntax::ItemId id = this->parse_fn_decl(false, false, false, std::move(function_attributes));
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
            this->apply_item_attributes(id, item_attributes);
        }
        return id;
    }
    if (token_is_macro_item_start(this->peek())) {
        this->report_misplaced_function_decorators(function_attributes);
        const syntax::ItemId id = this->parse_macro_decl();
        if (syntax::is_valid(id)) {
            this->session_.module.items.set_visibility(id.value, visibility.visibility);
            this->apply_item_attributes(id, item_attributes);
        }
        return id;
    }

    this->report_misplaced_function_decorators(function_attributes);
    this->report_here(std::string(PARSER_EXPECT_ITEM_DECLARATION));
    return syntax::INVALID_ITEM_ID;
}

void ItemParser::parse_optional_item_attributes(ParsedItemAttributes& attributes)
{
    while (this->check(TokenKind::hash)) {
        const syntax::Token& attribute_start = this->advance();
        this->parse_item_attribute(attributes, attribute_start);
        include_item_attribute_range(attributes, this->merge(attribute_start.range, this->previous().range));
        this->reset_panic();
    }
}

void ItemParser::parse_item_attribute(ParsedItemAttributes& attributes, const syntax::Token& attribute_start)
{
    const syntax::Token& list_start =
        this->expect_recovered(TokenKind::l_bracket, std::string(PARSER_EXPECT_ITEM_ATTRIBUTE_START),
            RecoveryContext::item);
    const syntax::Token& attr = this->expect_identifier_recovered(std::string(PARSER_EXPECT_ITEM_ATTRIBUTE_NAME));
    syntax::AttributeDecl attribute;
    attribute.name = attr.text();
    attribute.name_id = syntax::INVALID_IDENT_ID;
    attribute.range = this->merge(attribute_start.range, attr.range);
    attribute.token_tree = this->session_.module.make_item_list<syntax::AttributeTokenDecl>();
    if (attr.text() == "derive") {
        this->parse_derive_attribute(attributes, attribute);
        attribute.range.end = this->previous().range.end;
    } else if (token_opens_attribute_group(this->peek().kind)) {
        const syntax::Token& opening = this->advance();
        this->parse_attribute_token_tree(attribute, opening);
    } else if (!this->check(TokenKind::r_bracket)) {
        this->report_here(std::string(PARSER_EXPECT_ITEM_ATTRIBUTE_END));
        if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::item)) {
            this->synchronize(RecoveryContext::item);
        }
    }
    if (list_start.kind == TokenKind::l_bracket) {
        this->expect_recovered_after(TokenKind::r_bracket, std::string(PARSER_EXPECT_ITEM_ATTRIBUTE_END),
            RecoveryContext::item, list_start);
    } else {
        this->expect_recovered(
            TokenKind::r_bracket, std::string(PARSER_EXPECT_ITEM_ATTRIBUTE_END), RecoveryContext::item);
    }
    attribute.range = this->merge(attribute_start.range, this->previous().range);
    attributes.attributes.push_back(std::move(attribute));
}

void ItemParser::parse_attribute_token_tree(syntax::AttributeDecl& attribute, const syntax::Token& opening)
{
    const syntax::AttributeTokenTreeGroupKind root_group = attribute_group_kind(opening.kind);
    attribute.has_token_tree = true;
    attribute.token_tree_range = opening.range;

    base::u32 depth = 0;
    std::vector<syntax::AttributeTokenTreeGroupKind> groups;
    groups.push_back(root_group);
    attribute.token_tree.push_back(
        syntax::AttributeTokenDecl{opening.kind, opening.text(), opening.range, depth, root_group});
    ++depth;

    while (!this->is_eof() && !groups.empty()) {
        const syntax::Token& token = this->advance();
        const syntax::AttributeTokenTreeGroupKind current_group = groups.back();
        const bool closing_current_group = token_closes_attribute_group(token.kind, current_group);
        const base::u32 token_depth = closing_current_group && depth > 0 ? depth - 1U : depth;
        attribute.token_tree.push_back(
            syntax::AttributeTokenDecl{token.kind, token.text(), token.range, token_depth, attribute_group_kind(token.kind)});
        attribute.token_tree_range = this->merge(opening.range, token.range);

        if (token_opens_attribute_group(token.kind)) {
            groups.push_back(attribute_group_kind(token.kind));
            ++depth;
            continue;
        }
        if (closing_current_group) {
            groups.pop_back();
            depth = token_depth;
            continue;
        }
        if (token_closes_attribute_group(token.kind)) {
            this->report_at(token, std::string(PARSER_EXPECT_ITEM_ATTRIBUTE_END));
            break;
        }
    }
}

void ItemParser::parse_derive_attribute(ParsedItemAttributes& attributes, syntax::AttributeDecl& attribute)
{
    const syntax::Token& argument_start =
        this->expect_recovered(TokenKind::l_paren, std::string(PARSER_EXPECT_DERIVE_ARGUMENT_START),
            RecoveryContext::item);
    attribute.has_token_tree = argument_start.kind == TokenKind::l_paren;
    if (attribute.has_token_tree) {
        attribute.token_tree_range = argument_start.range;
        attribute.token_tree.push_back(syntax::AttributeTokenDecl{argument_start.kind,
            argument_start.text(),
            argument_start.range,
            0,
            syntax::AttributeTokenTreeGroupKind::paren});
    }
    if (this->check(TokenKind::r_paren)) {
        this->report_here(std::string(PARSER_EXPECT_DERIVE_NAME));
    }
    while (!this->is_eof() && !this->check(TokenKind::r_paren) && !this->check(TokenKind::r_bracket)) {
        const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_DERIVE_NAME));
        if (attribute.has_token_tree && name.kind != TokenKind::eof) {
            attribute.token_tree.push_back(syntax::AttributeTokenDecl{
                name.kind, name.text(), name.range, 1, syntax::AttributeTokenTreeGroupKind::none});
            attribute.token_tree_range = this->merge(argument_start.range, name.range);
        }
        if (name.kind == TokenKind::identifier) {
            attributes.derives.push_back(syntax::DeriveDecl{name.text(), syntax::INVALID_IDENT_ID, name.range});
        }
        this->reset_panic();
        if (this->check(TokenKind::r_paren)) {
            break;
        }
        if (this->check(TokenKind::comma)) {
            const syntax::Token& separator = this->peek();
            const bool trailing_separator =
                this->check_next(TokenKind::r_paren) || this->check_next(TokenKind::r_bracket);
            if (!this->recover_derive_separator()) {
                if (trailing_separator && attribute.has_token_tree) {
                    attribute.token_tree.push_back(syntax::AttributeTokenDecl{separator.kind,
                        separator.text(),
                        separator.range,
                        1,
                        syntax::AttributeTokenTreeGroupKind::none});
                    attribute.token_tree_range = this->merge(argument_start.range, separator.range);
                }
                break;
            }
            if (attribute.has_token_tree) {
                attribute.token_tree.push_back(syntax::AttributeTokenDecl{separator.kind,
                    separator.text(),
                    separator.range,
                    1,
                    syntax::AttributeTokenTreeGroupKind::none});
                attribute.token_tree_range = this->merge(argument_start.range, separator.range);
            }
            continue;
        }
        if (!this->recover_derive_separator()) {
            break;
        }
    }
    if (argument_start.kind == TokenKind::l_paren) {
        const syntax::Token& argument_end = this->expect_recovered_after(TokenKind::r_paren,
            std::string(PARSER_EXPECT_DERIVE_ARGUMENT_END),
            RecoveryContext::item, argument_start);
        if (argument_end.kind == TokenKind::r_paren) {
            attribute.token_tree.push_back(syntax::AttributeTokenDecl{argument_end.kind,
                argument_end.text(),
                argument_end.range,
                0,
                syntax::AttributeTokenTreeGroupKind::paren});
            attribute.token_tree_range = this->merge(argument_start.range, argument_end.range);
        }
        return;
    }
    const syntax::Token& argument_end =
        this->expect_recovered(TokenKind::r_paren, std::string(PARSER_EXPECT_DERIVE_ARGUMENT_END),
            RecoveryContext::item);
    if (attribute.has_token_tree && argument_end.kind == TokenKind::r_paren) {
        attribute.token_tree.push_back(syntax::AttributeTokenDecl{argument_end.kind,
            argument_end.text(),
            argument_end.range,
            0,
            syntax::AttributeTokenTreeGroupKind::paren});
        attribute.token_tree_range = this->merge(argument_start.range, argument_end.range);
    }
}

bool ItemParser::recover_derive_separator() const
{
    if (this->check(TokenKind::r_paren)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }
    this->report_here(std::string(PARSER_EXPECT_DERIVE_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::item)) {
        this->synchronize(RecoveryContext::item);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }
    this->reset_panic();
    return this->check(TokenKind::identifier);
}

void ItemParser::apply_item_attributes(const syntax::ItemId item_id, const ParsedItemAttributes& attributes) const
{
    if (!attributes.present || !syntax::is_valid(item_id)) {
        return;
    }
    syntax::ItemNode item = this->session_.module.items.take(item_id.value);
    item.attributes.insert(item.attributes.end(), attributes.attributes.begin(), attributes.attributes.end());
    item.derives.insert(item.derives.end(), attributes.derives.begin(), attributes.derives.end());
    item.range.begin = std::min(item.range.begin, attributes.range.begin);
    this->session_.module.items.set(item_id.value, std::move(item));
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

syntax::ItemId ItemParser::parse_macro_decl()
{
    const syntax::Token& begin = this->advance();
    syntax::MacroDeclKind macro_kind = syntax::MacroDeclKind::declarative;
    if (token_is_contextual_keyword(this->peek(), PARSER_MACRO_DERIVE_CONTEXTUAL_KEYWORD_TEXT)) {
        macro_kind = syntax::MacroDeclKind::derive;
        this->advance();
    } else if (this->check(TokenKind::kw_const)) {
        macro_kind = syntax::MacroDeclKind::compile_time;
        this->advance();
    }

    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_MACRO_NAME));
    const syntax::Token& body_start =
        this->expect_recovered(TokenKind::l_brace, std::string(PARSER_EXPECT_MACRO_BODY), RecoveryContext::block_start);

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::macro_decl;
    item.name = name.text();
    item.macro_kind = macro_kind;
    item.macro_body_tokens = this->session_.module.items.make_list<syntax::AttributeTokenDecl>();
    item.macro_body_range = body_start.range;

    if (body_start.kind == TokenKind::l_brace) {
        this->parse_macro_body_token_tree(item, body_start);
    }

    item.range = this->merge(begin.range, this->previous().range);
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

void ItemParser::parse_macro_body_token_tree(syntax::ItemNode& item, const syntax::Token& opening)
{
    const syntax::AttributeTokenTreeGroupKind root_group = attribute_group_kind(opening.kind);
    base::u32 depth = 0;
    std::vector<syntax::AttributeTokenTreeGroupKind> groups;
    groups.push_back(root_group);
    item.macro_body_tokens.push_back(
        syntax::AttributeTokenDecl{opening.kind, opening.text(), opening.range, depth, root_group});
    ++depth;

    while (!this->is_eof() && !groups.empty()) {
        const syntax::Token& token = this->advance();
        const syntax::AttributeTokenTreeGroupKind current_group = groups.back();
        const bool closing_current_group = token_closes_attribute_group(token.kind, current_group);
        const base::u32 token_depth = closing_current_group && depth > 0 ? depth - 1U : depth;
        item.macro_body_tokens.push_back(
            syntax::AttributeTokenDecl{token.kind, token.text(), token.range, token_depth, attribute_group_kind(token.kind)});
        item.macro_body_range = this->merge(opening.range, token.range);

        if (token.kind == TokenKind::kw_match && token_depth == 1U) {
            ++item.macro_match_clause_count;
        } else if (token_is_contextual_keyword(token, PARSER_MACRO_MATCH_CLAUSE_KEYWORD_TEXT) && token_depth == 1U) {
            ++item.macro_match_clause_count;
        }

        if (token_opens_attribute_group(token.kind)) {
            groups.push_back(attribute_group_kind(token.kind));
            ++depth;
            continue;
        }
        if (closing_current_group) {
            groups.pop_back();
            depth = token_depth;
            continue;
        }
        if (token_closes_attribute_group(token.kind)) {
            this->report_at(token, std::string(PARSER_EXPECT_MACRO_BODY_END));
            break;
        }
    }

    item.macro_body_balanced = groups.empty();
    if (!item.macro_body_balanced) {
        this->report_at(opening, std::string(PARSER_EXPECT_MACRO_BODY_END));
    }
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
