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

[[nodiscard]] std::string_view unquote_string_literal(const std::string_view text) noexcept {
    if (text.size() < PARSER_FN_STRING_DELIMITER_PAIR_SIZE) {
        return {};
    }
    return text.substr(PARSER_FN_STRING_DELIMITER_SIZE, text.size() - PARSER_FN_STRING_DELIMITER_PAIR_SIZE);
}

} // namespace

syntax::ItemId ItemParser::parse_fn_decl(const bool is_export_c, const bool is_extern_c) {
    const syntax::Token& begin = this->expect(TokenKind::kw_fn, "expected 'fn'");
    const syntax::Token& name = this->expect_identifier_recovered("expected function name");
    std::vector<syntax::GenericParamDecl> generic_params = this->parse_optional_generic_params();
    this->expect_param_list_start("expected '(' after function name");
    std::vector<syntax::ParamDecl> params;
    bool is_variadic = false;
    if (!this->check(TokenKind::r_paren)) {
        params = this->parse_param_list(is_variadic);
    }
    this->expect_recovered(
        TokenKind::r_paren,
        "expected ')' after parameter list",
        RecoveryContext::parameter
    );
    const syntax::TypeId return_type = this->parse_optional_return_type();

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::fn_decl;
    item.name = name.text;
    item.generic_params = std::move(generic_params);
    item.params = std::move(params);
    item.return_type = return_type;
    item.is_export_c = is_export_c;
    item.is_extern_c = is_extern_c;
    item.is_variadic = is_variadic;

    this->parse_optional_abi_name(item);
    this->reject_optional_where_clause();

    if (is_extern_c) {
        const syntax::Token& end = this->expect_item_terminator(
            "expected ';' after extern function declaration"
        );
        item.range = this->merge(begin.range, end.range);
    } else if (this->match(TokenKind::semicolon)) {
        item.is_prototype = true;
        item.range = this->merge(begin.range, this->previous().range);
    } else {
        item.body = this->parse_block();
        item.range = this->merge(begin.range, this->stmt_range_or(item.body, begin.range));
    }

    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

std::vector<syntax::GenericParamDecl> ItemParser::parse_optional_generic_params() {
    std::vector<syntax::GenericParamDecl> params;
    if (this->check(TokenKind::less)) {
        this->reject_legacy_angle_generic_params();
        return params;
    }
    if (!this->match(TokenKind::l_bracket)) {
        return params;
    }
    if (this->check(TokenKind::r_bracket)) {
        this->report_here("expected generic type parameter");
    }
    this->parse_generic_params(params);
    this->expect_recovered(
        TokenKind::r_bracket,
        "expected ']' after generic parameter list",
        RecoveryContext::generic_parameter
    );
    return params;
}

void ItemParser::reject_legacy_angle_generic_params() {
    const syntax::Token& begin = this->expect(TokenKind::less, "expected '<'");
    this->report_at(begin, std::string(PARSER_M2_LEGACY_ANGLE_GENERIC_UNSUPPORTED));
    while (!this->is_eof()) {
        if (this->match(TokenKind::greater)) {
            this->reset_panic();
            return;
        }
        if (this->check(TokenKind::l_paren) ||
            this->check(TokenKind::l_brace) ||
            this->check(TokenKind::equal) ||
            this->check(TokenKind::semicolon)) {
            this->reset_panic();
            return;
        }
        this->advance();
    }
}

void ItemParser::parse_generic_params(std::vector<syntax::GenericParamDecl>& params) {
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

std::optional<syntax::GenericParamDecl> ItemParser::parse_generic_param() {
    const syntax::Token& name = this->expect_identifier_recovered("expected generic type parameter name");
    if (name.kind != TokenKind::identifier) {
        return std::nullopt;
    }
    if (this->check(TokenKind::colon)) {
        this->report_here(std::string(PARSER_M2_GENERIC_BOUNDS_UNSUPPORTED));
    }
    return syntax::GenericParamDecl {
        name.text,
        name.range,
    };
}

bool ItemParser::recover_generic_param_separator() {
    if (this->check(TokenKind::r_bracket)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }

    this->report_here("expected ',' or ']' after generic parameter");
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

void ItemParser::expect_param_list_start(std::string message) {
    this->expect_recovered(
        TokenKind::l_paren,
        std::move(message),
        RecoveryContext::parameter_list_start
    );
}

std::vector<syntax::ParamDecl> ItemParser::parse_param_list(bool& is_variadic) {
    std::vector<syntax::ParamDecl> params;
    while (!this->is_eof() && !this->check(TokenKind::r_paren)) {
        if (this->match(TokenKind::ellipsis)) {
            is_variadic = true;
            if (!this->check(TokenKind::r_paren)) {
                this->report_here("variadic marker must be last in parameter list");
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

std::optional<syntax::ParamDecl> ItemParser::parse_param() {
    const syntax::Token& name = this->expect_identifier_recovered("expected parameter name");
    this->expect_type_annotation_colon("expected ':' after parameter name");
    const syntax::TypeId type = this->parse_type();
    if (name.kind != TokenKind::identifier) {
        return std::nullopt;
    }
    return syntax::ParamDecl {
        name.text,
        type,
        this->merge(name.range, this->type_range_or(type, name.range)),
    };
}

bool ItemParser::recover_param_separator(bool& is_variadic) {
    if (this->check(TokenKind::r_paren)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        if (this->match(TokenKind::ellipsis)) {
            is_variadic = true;
            if (!this->check(TokenKind::r_paren)) {
                this->report_here("variadic marker must be last in parameter list");
                this->synchronize(RecoveryContext::parameter);
            }
            return false;
        }
        return !this->check(TokenKind::r_paren);
    }

    this->report_here("expected ',' or ')' after parameter");
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

syntax::TypeId ItemParser::parse_optional_return_type() {
    if (!this->match(TokenKind::arrow)) {
        return syntax::INVALID_TYPE_ID;
    }
    return this->parse_type();
}

void ItemParser::parse_optional_abi_name(syntax::ItemNode& item) {
    if (!this->match(TokenKind::at)) {
        return;
    }
    const syntax::Token& attr = this->expect_identifier_recovered("expected ABI attribute name");
    if (attr.text != "name") {
        this->report_at(attr, "expected ABI attribute 'name'");
    }
    this->expect_abi_attribute_argument_start();
    this->parse_abi_name_argument(item);
    this->recover_abi_attribute_argument_end();
    this->reset_panic();
}

void ItemParser::reject_optional_where_clause() {
    if (!this->check(TokenKind::identifier) || this->peek().text != "where") {
        return;
    }
    const syntax::Token& begin = this->advance();
    this->report_at(begin, std::string(PARSER_M2_GENERIC_WHERE_UNSUPPORTED));
    this->synchronize(RecoveryContext::item);
    this->reset_panic();
}

void ItemParser::expect_abi_attribute_argument_start() {
    this->expect_recovered(
        TokenKind::l_paren,
        "expected '(' after ABI attribute",
        RecoveryContext::abi_attribute_start
    );
}

void ItemParser::parse_abi_name_argument(syntax::ItemNode& item) {
    const syntax::Token& value = this->expect(TokenKind::string_literal, "expected string literal in ABI name");
    if (value.kind == TokenKind::string_literal) {
        item.abi_name = unquote_string_literal(value.text);
    }
}

void ItemParser::recover_abi_attribute_argument_end() {
    if (this->match(TokenKind::r_paren)) {
        return;
    }

    this->report_here("expected ')' after ABI attribute");
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::abi_attribute_argument)) {
        this->synchronize(RecoveryContext::abi_attribute_argument);
    }
    this->match(TokenKind::r_paren);
    this->reset_panic();
}

} // namespace aurex::parse
