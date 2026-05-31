#include <aurex/parse/parser_item_part.hpp>
#include <aurex/parse/parser_messages.hpp>

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

[[nodiscard]] syntax::Visibility trait_requirement_visibility(const ParsedVisibility visibility) noexcept
{
    if (visibility.explicit_visibility) {
        return visibility.visibility;
    }
    return syntax::Visibility::public_;
}

} // namespace

syntax::ItemId ItemParser::parse_trait_decl()
{
    const syntax::Token& begin = this->expect(TokenKind::kw_trait, std::string(PARSER_EXPECT_TRAIT_KEYWORD));
    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_TRAIT_NAME));
    std::vector<syntax::GenericParamDecl> generic_params = this->parse_optional_generic_params();
    std::vector<syntax::GenericConstraintDecl> where_constraints = this->parse_optional_where_constraints();
    this->expect_item_container_start(std::string(PARSER_EXPECT_TRAIT_BODY));

    syntax::ItemNode trait;
    trait.kind = syntax::ItemKind::trait_decl;
    trait.name = name.text();
    trait.generic_params = std::move(generic_params);
    trait.where_constraints = std::move(where_constraints);

    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        const ParsedVisibility visibility = this->parse_visibility();
        const bool is_unsafe = this->check(TokenKind::kw_unsafe);
        if (this->check(TokenKind::kw_type)) {
            const syntax::ItemId associated_type = this->parse_trait_associated_type_decl(visibility);
            if (syntax::is_valid(associated_type)) {
                trait.trait_items.push_back(associated_type);
            }
            this->reset_panic();
            continue;
        }
        if (!this->check(TokenKind::kw_fn) && !(is_unsafe && this->check_next(TokenKind::kw_fn))) {
            this->report_here(std::string(PARSER_EXPECT_TRAIT_REQUIREMENT));
            this->synchronize(RecoveryContext::item);
            this->reset_panic();
            continue;
        }

        const syntax::ItemId requirement =
            this->parse_fn_decl(false, false, is_unsafe, FunctionBodyPolicy::require_prototype);
        if (syntax::is_valid(requirement)) {
            syntax::ItemNode requirement_item = this->session_.module.items[requirement.value];
            requirement_item.visibility = trait_requirement_visibility(visibility);
            requirement_item.is_prototype = true;
            this->session_.module.set_item(requirement.value, std::move(requirement_item));
            trait.trait_items.push_back(requirement);
        }
        this->reset_panic();
    }

    const syntax::Token& end = this->expect_item_container_end(std::string(PARSER_EXPECT_TRAIT_END));
    trait.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_item(std::move(trait));
}

} // namespace aurex::parse
