#include <aurex/parse/parser_item_part.hpp>
#include <aurex/parse/parser_messages.hpp>
#include <aurex/parse/recovery.hpp>

#include <optional>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

constexpr base::usize PARSER_FN_STRING_DELIMITER_SIZE = 1;
constexpr base::usize PARSER_FN_STRING_DELIMITER_PAIR_SIZE = PARSER_FN_STRING_DELIMITER_SIZE * 2;

[[nodiscard]] std::string_view unquote_string_literal(const std::string_view text) noexcept
{
    if (text.size() < PARSER_FN_STRING_DELIMITER_PAIR_SIZE) {
        return {};
    }
    return text.substr(PARSER_FN_STRING_DELIMITER_SIZE, text.size() - PARSER_FN_STRING_DELIMITER_PAIR_SIZE);
}

} // namespace

syntax::ItemId ItemParser::parse_fn_decl(
    const bool is_export_c, const bool is_extern_c, const bool is_unsafe, const FunctionBodyPolicy body_policy)
{
    base::SourceRange begin_range = this->peek().range;
    if (is_unsafe) {
        const syntax::Token& unsafe = this->expect(TokenKind::kw_unsafe, std::string(PARSER_EXPECT_UNSAFE_KEYWORD));
        begin_range = unsafe.range;
    }
    const syntax::Token& begin = this->expect(TokenKind::kw_fn, std::string(PARSER_EXPECT_FN_KEYWORD));
    if (!is_unsafe) {
        begin_range = begin.range;
    }
    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_FN_NAME));
    std::vector<syntax::GenericParamDecl> generic_params = this->parse_optional_generic_params();
    this->expect_param_list_start(std::string(PARSER_EXPECT_FN_PARAM_LIST));
    std::vector<syntax::ParamDecl> params;
    bool is_variadic = false;
    if (!this->check(TokenKind::r_paren)) {
        params = this->parse_param_list(is_variadic);
    }
    this->expect_recovered(
        TokenKind::r_paren, std::string(PARSER_EXPECT_FN_PARAM_LIST_END), RecoveryContext::parameter);
    const syntax::TypeId return_type = this->parse_optional_return_type();

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::fn_decl;
    item.name = name.text();
    item.generic_params = std::move(generic_params);
    item.params = std::move(params);
    item.return_type = return_type;
    item.is_export_c = is_export_c;
    item.is_extern_c = is_extern_c;
    item.is_unsafe = is_unsafe;
    item.is_variadic = is_variadic;

    this->parse_optional_abi_name(item);
    item.where_constraints = this->parse_optional_where_constraints();

    if (is_extern_c) {
        const syntax::Token& end = this->expect_item_terminator(std::string(PARSER_EXPECT_EXTERN_FN_TERMINATOR));
        item.range = this->merge(begin_range, end.range);
    } else if (body_policy == FunctionBodyPolicy::require_prototype) {
        item.is_prototype = true;
        if (this->match(TokenKind::semicolon)) {
            item.range = this->merge(begin_range, this->previous().range);
        } else if (this->check(TokenKind::l_brace)) {
            this->report_here(std::string(PARSER_EXPECT_TRAIT_FN_TERMINATOR));
            const syntax::StmtId rejected_body = this->parse_block();
            item.range = this->merge(begin_range, this->stmt_range_or(rejected_body, begin.range));
        } else {
            const syntax::Token& end = this->expect_item_terminator(std::string(PARSER_EXPECT_TRAIT_FN_TERMINATOR));
            item.range = this->merge(begin_range, end.range);
        }
    } else if (this->match(TokenKind::semicolon)) {
        item.is_prototype = true;
        item.range = this->merge(begin_range, this->previous().range);
    } else {
        item.body = this->parse_block();
        item.range = this->merge(begin_range, this->stmt_range_or(item.body, begin.range));
    }

    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

std::vector<syntax::GenericParamDecl> ItemParser::parse_optional_generic_params()
{
    std::vector<syntax::GenericParamDecl> params;
    if (this->check(TokenKind::less)) {
        this->reject_legacy_angle_generic_params();
        return params;
    }
    if (!this->match(TokenKind::l_bracket)) {
        return params;
    }
    if (this->check(TokenKind::r_bracket)) {
        this->report_here(std::string(PARSER_EXPECT_GENERIC_TYPE_PARAMETER));
    }
    this->parse_generic_params(params);
    this->expect_recovered(
        TokenKind::r_bracket, std::string(PARSER_EXPECT_GENERIC_PARAM_LIST_END), RecoveryContext::generic_parameter);
    return params;
}

void ItemParser::reject_legacy_angle_generic_params() const
{
    const syntax::Token& begin = this->expect(TokenKind::less, std::string(PARSER_EXPECT_LEGACY_GENERIC_BEGIN));
    this->report_at(begin, std::string(PARSER_M2_LEGACY_ANGLE_GENERIC_UNSUPPORTED));
    while (!this->is_eof()) {
        if (this->match(TokenKind::greater)) {
            this->reset_panic();
            return;
        }
        if (this->check(TokenKind::l_paren) || this->check(TokenKind::l_brace) || this->check(TokenKind::equal)
            || this->check(TokenKind::semicolon)) {
            this->reset_panic();
            return;
        }
        this->advance();
    }
}

void ItemParser::parse_generic_params(std::vector<syntax::GenericParamDecl>& params)
{
    while (!this->is_eof() && !this->check(TokenKind::r_bracket)) {
        if (std::optional<syntax::GenericParamDecl> param = this->parse_generic_param()) {
            params.push_back(param.value());
        }
        this->reset_panic();
        if (!this->recover_generic_param_separator()) {
            break;
        }
    }
}

std::optional<syntax::GenericParamDecl> ItemParser::parse_generic_param()
{
    const syntax::Token& name =
        this->expect_identifier_recovered(std::string(PARSER_EXPECT_GENERIC_TYPE_PARAMETER_NAME));
    if (name.kind != TokenKind::identifier) {
        return std::nullopt;
    }
    if (this->check(TokenKind::colon)) {
        this->report_here(std::string(PARSER_M2_GENERIC_BOUNDS_UNSUPPORTED));
    }
    return syntax::GenericParamDecl{
        name.text(),
        name.range,
    };
}

bool ItemParser::recover_generic_param_separator() const
{
    if (this->check(TokenKind::r_bracket)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }

    this->report_here(std::string(PARSER_EXPECT_GENERIC_PARAM_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::generic_parameter)) {
        this->synchronize(RecoveryContext::generic_parameter);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }
    this->reset_panic();
    return false;
}

void ItemParser::expect_param_list_start(std::string message) const
{
    this->expect_recovered(TokenKind::l_paren, std::move(message), RecoveryContext::parameter_list_start);
}

std::vector<syntax::ParamDecl> ItemParser::parse_param_list(bool& is_variadic)
{
    std::vector<syntax::ParamDecl> params;
    while (!this->is_eof() && !this->check(TokenKind::r_paren)) {
        if (this->match(TokenKind::ellipsis)) {
            is_variadic = true;
            if (!this->check(TokenKind::r_paren)) {
                this->report_here(std::string(PARSER_VARIADIC_MARKER_MUST_BE_LAST));
                this->synchronize(RecoveryContext::parameter);
            }
            break;
        }
        if (std::optional<syntax::ParamDecl> param = this->parse_param()) {
            params.push_back(param.value());
        }
        this->reset_panic();
        if (!this->recover_param_separator(is_variadic)) {
            break;
        }
    }
    return params;
}

std::optional<syntax::ParamDecl> ItemParser::parse_param()
{
    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_PARAMETER_NAME));
    this->expect_type_annotation_colon(std::string(PARSER_EXPECT_PARAMETER_TYPE_COLON));
    const syntax::TypeId type = this->parse_type();
    if (name.kind != TokenKind::identifier) {
        return std::nullopt;
    }
    return syntax::ParamDecl{
        name.text(),
        type,
        this->merge(name.range, this->type_range_or(type, name.range)),
    };
}

bool ItemParser::recover_param_separator(bool& is_variadic) const
{
    if (this->check(TokenKind::r_paren)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        if (this->match(TokenKind::ellipsis)) {
            is_variadic = true;
            if (!this->check(TokenKind::r_paren)) {
                this->report_here(std::string(PARSER_VARIADIC_MARKER_MUST_BE_LAST));
                this->synchronize(RecoveryContext::parameter);
            }
            return false;
        }
        return !this->check(TokenKind::r_paren);
    }

    this->report_here(std::string(PARSER_EXPECT_PARAMETER_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::parameter)) {
        this->synchronize(RecoveryContext::parameter);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }
    this->reset_panic();
    return token_starts_parameter(this->peek().kind);
}

syntax::TypeId ItemParser::parse_optional_return_type() const
{
    if (!this->match(TokenKind::arrow)) {
        return syntax::INVALID_TYPE_ID;
    }
    return this->parse_type();
}

void ItemParser::parse_optional_abi_name(syntax::ItemNode& item)
{
    if (!this->match(TokenKind::at)) {
        return;
    }
    const syntax::Token& attr = this->expect_identifier_recovered(std::string(PARSER_EXPECT_ABI_NAME_ATTRIBUTE));
    if (attr.text() != "name") {
        this->report_at(attr, std::string(PARSER_EXPECT_ABI_NAME_ATTRIBUTE));
    }
    this->expect_abi_attribute_argument_start();
    this->parse_abi_name_argument(item);
    this->recover_abi_attribute_argument_end();
    this->reset_panic();
}

std::vector<syntax::GenericConstraintDecl> ItemParser::parse_optional_where_constraints()
{
    std::vector<syntax::GenericConstraintDecl> constraints;
    if (!this->match(TokenKind::kw_where)) {
        return constraints;
    }
    if (this->check(TokenKind::l_brace) || this->check(TokenKind::semicolon) || this->check(TokenKind::equal)) {
        this->report_here(std::string(PARSER_EXPECT_WHERE_GENERIC_PARAM));
        return constraints;
    }
    while (!this->is_eof() && !this->check(TokenKind::l_brace) && !this->check(TokenKind::semicolon)
        && !this->check(TokenKind::equal)) {
        if (std::optional<syntax::GenericConstraintDecl> constraint = this->parse_where_constraint()) {
            constraints.push_back(std::move(constraint.value()));
        }
        this->reset_panic();
        if (!this->recover_where_constraint_separator()) {
            break;
        }
    }
    return constraints;
}

std::optional<syntax::GenericConstraintDecl> ItemParser::parse_where_constraint()
{
    const syntax::Token& param = this->expect_identifier_recovered(std::string(PARSER_EXPECT_WHERE_GENERIC_PARAM));
    this->expect_recovered(
        TokenKind::colon, std::string(PARSER_EXPECT_WHERE_GENERIC_PARAM_COLON), RecoveryContext::item);
    if (param.kind != TokenKind::identifier) {
        return std::nullopt;
    }

    syntax::GenericConstraintDecl constraint;
    constraint.param_name = param.text();
    constraint.param_range = param.range;
    constraint.capability_names = this->session_.module.make_item_list<std::string_view>();
    constraint.capability_ranges = this->session_.module.make_item_list<base::SourceRange>();
    constraint.capability_name_ids = this->session_.module.make_item_list<syntax::IdentId>();
    constraint.range = param.range;
    this->parse_where_capabilities(constraint);
    if (!constraint.capability_ranges.empty()) {
        constraint.range = this->merge(param.range, constraint.capability_ranges.back());
    }
    return constraint;
}

void ItemParser::parse_where_capabilities(syntax::GenericConstraintDecl& constraint)
{
    while (!this->is_eof()) {
        const syntax::Token& capability =
            this->expect_identifier_recovered(std::string(PARSER_EXPECT_WHERE_CAPABILITY));
        std::vector<syntax::AssociatedTypeConstraintDecl> associated_constraints;
        if (capability.kind == TokenKind::identifier) {
            if (this->check(TokenKind::l_bracket)) {
                associated_constraints = this->parse_associated_type_constraints();
            }
            constraint.capability_names.push_back(capability.text());
            constraint.capability_ranges.push_back(capability.range);
            constraint.capability_associated_constraints.push_back(std::move(associated_constraints));
        }
        if (!this->match(TokenKind::plus)) {
            break;
        }
    }
}

std::vector<syntax::AssociatedTypeConstraintDecl> ItemParser::parse_associated_type_constraints()
{
    const syntax::Token& begin =
        this->expect(TokenKind::l_bracket, std::string(PARSER_EXPECT_ASSOCIATED_TYPE_CONSTRAINT_END));
    std::vector<syntax::AssociatedTypeConstraintDecl> constraints;
    while (!this->is_eof() && !this->check(TokenKind::r_bracket)) {
        if (std::optional<syntax::AssociatedTypeConstraintDecl> constraint = this->parse_associated_type_constraint()) {
            constraints.push_back(constraint.value());
        }
        this->reset_panic();
        if (!this->recover_associated_type_constraint_separator()) {
            break;
        }
    }
    static_cast<void>(this->expect_recovered_after(TokenKind::r_bracket,
        std::string(PARSER_EXPECT_ASSOCIATED_TYPE_CONSTRAINT_END), RecoveryContext::generic_type_argument, begin));
    return constraints;
}

std::optional<syntax::AssociatedTypeConstraintDecl> ItemParser::parse_associated_type_constraint()
{
    const syntax::Token& name =
        this->expect_identifier_recovered(std::string(PARSER_EXPECT_ASSOCIATED_TYPE_CONSTRAINT_NAME));
    this->expect_recovered(TokenKind::equal, std::string(PARSER_EXPECT_ASSOCIATED_TYPE_CONSTRAINT_EQUAL),
        RecoveryContext::type_annotation);
    const syntax::TypeId value_type = this->parse_type();
    if (name.kind != TokenKind::identifier) {
        return std::nullopt;
    }
    return syntax::AssociatedTypeConstraintDecl{
        name.text(),
        name.range,
        value_type,
        this->merge(name.range, this->type_range_or(value_type, name.range)),
    };
}

bool ItemParser::recover_associated_type_constraint_separator() const
{
    if (this->check(TokenKind::r_bracket)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }

    this->report_here(std::string(PARSER_EXPECT_ASSOCIATED_TYPE_CONSTRAINT_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::generic_type_argument)) {
        this->synchronize(RecoveryContext::generic_type_argument);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }
    this->reset_panic();
    return false;
}

bool ItemParser::recover_where_constraint_separator() const
{
    if (this->check(TokenKind::l_brace) || this->check(TokenKind::semicolon) || this->check(TokenKind::equal)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        if (this->check(TokenKind::l_brace) || this->check(TokenKind::semicolon) || this->check(TokenKind::equal)) {
            this->report_here(std::string(PARSER_EXPECT_WHERE_GENERIC_PARAM));
            return false;
        }
        return !this->check(TokenKind::l_brace) && !this->check(TokenKind::semicolon) && !this->check(TokenKind::equal);
    }

    this->report_here(std::string(PARSER_EXPECT_WHERE_SEPARATOR));
    this->synchronize(RecoveryContext::item);
    this->reset_panic();
    return false;
}

void ItemParser::expect_abi_attribute_argument_start() const
{
    this->expect_recovered(
        TokenKind::l_paren, std::string(PARSER_EXPECT_ABI_ATTRIBUTE_START), RecoveryContext::abi_attribute_start);
}

void ItemParser::parse_abi_name_argument(syntax::ItemNode& item) const
{
    const syntax::Token& value = this->expect(TokenKind::string_literal, std::string(PARSER_EXPECT_ABI_NAME_STRING));
    if (value.kind == TokenKind::string_literal) {
        item.abi_name = unquote_string_literal(value.text());
    }
}

void ItemParser::recover_abi_attribute_argument_end() const
{
    if (this->match(TokenKind::r_paren)) {
        return;
    }

    this->report_here(std::string(PARSER_EXPECT_ABI_ATTRIBUTE_END));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::abi_attribute_argument)) {
        this->synchronize(RecoveryContext::abi_attribute_argument);
    }
    this->match(TokenKind::r_paren);
    this->reset_panic();
}

} // namespace aurex::parse
