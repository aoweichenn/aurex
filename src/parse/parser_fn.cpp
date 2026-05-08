#include "aurex/parse/parser_parts.hpp"

#include "aurex/parse/recovery.hpp"

#include <optional>
#include <string_view>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

constexpr base::usize kStringDelimiterSize = 1;
constexpr base::usize kStringDelimiterPairSize = kStringDelimiterSize * 2;

[[nodiscard]] std::string_view unquote_string_literal(const std::string_view text) noexcept {
    if (text.size() < kStringDelimiterPairSize) {
        return {};
    }
    return text.substr(kStringDelimiterSize, text.size() - kStringDelimiterPairSize);
}

} // namespace

syntax::ItemId ItemParser::parse_fn_decl(const bool is_export_c, const bool is_extern_c) {
    const syntax::Token& begin = this->expect(TokenKind::kw_fn, "expected 'fn'");
    const syntax::Token& name = this->expect(TokenKind::identifier, "expected function name");
    std::vector<std::string_view> generic_params;
    if (this->check(TokenKind::less)) {
        generic_params = this->parse_generic_param_list();
    }
    this->expect(TokenKind::l_paren, "expected '(' after function name");
    std::vector<syntax::ParamDecl> params;
    bool is_variadic = false;
    if (!this->check(TokenKind::r_paren)) {
        params = this->parse_param_list(is_variadic);
    }
    this->expect(TokenKind::r_paren, "expected ')' after parameter list");
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

    if (is_extern_c) {
        const syntax::Token& end = this->expect(TokenKind::semicolon, "expected ';' after extern function declaration");
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
    const syntax::Token& name = this->expect(TokenKind::identifier, "expected parameter name");
    this->expect(TokenKind::colon, "expected ':' after parameter name");
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
        return syntax::invalid_type_id;
    }
    return this->parse_type();
}

void ItemParser::parse_optional_abi_name(syntax::ItemNode& item) {
    if (!this->match(TokenKind::at)) {
        return;
    }
    const syntax::Token& attr = this->expect(TokenKind::identifier, "expected ABI attribute name");
    if (attr.text != "name") {
        this->report_at(attr, "expected ABI attribute 'name'");
    }
    this->expect(TokenKind::l_paren, "expected '(' after ABI attribute");
    this->parse_abi_name_argument(item);
    this->recover_abi_attribute_argument_end();
    this->reset_panic();
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
