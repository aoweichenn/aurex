#include <aurex/parse/parser_item_part.hpp>

#include <aurex/parse/recovery.hpp>

#include <optional>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ItemId ItemParser::parse_struct_decl() {
    const syntax::Token& begin = this->expect(TokenKind::kw_struct, "expected 'struct'");
    const syntax::Token& name = this->expect_identifier_recovered("expected struct name");
    std::vector<syntax::GenericParamDecl> generic_params = this->parse_optional_generic_params();
    this->expect_item_container_start("expected '{' after struct name");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::struct_decl;
    item.name = name.text;
    item.generic_params = std::move(generic_params);

    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        if (std::optional<syntax::FieldDecl> field = this->parse_struct_field_decl()) {
            item.fields.push_back(field.value());
        }
        this->reset_panic();
        if (!this->recover_struct_field_decl_separator()) {
            break;
        }
    }

    const syntax::Token& end = this->expect_item_container_end("expected '}' after struct declaration");
    item.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

syntax::ItemId ItemParser::parse_enum_decl() {
    const syntax::Token& begin = this->expect(TokenKind::kw_enum, "expected 'enum'");
    const syntax::Token& name = this->expect_identifier_recovered("expected enum name");
    this->expect_type_annotation_colon("expected ':' after enum name");
    const syntax::TypeId base_type = this->parse_type();
    this->expect_item_container_start("expected '{' after enum base type");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::enum_decl;
    item.name = name.text;
    item.enum_base_type = base_type;

    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        if (std::optional<syntax::EnumCaseDecl> enum_case = this->parse_enum_case_decl()) {
            item.enum_cases.push_back(enum_case.value());
        }
        this->reset_panic();
        if (!this->recover_enum_case_separator()) {
            break;
        }
    }

    const syntax::Token& end = this->expect_item_container_end("expected '}' after enum declaration");
    item.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

std::optional<syntax::FieldDecl> ItemParser::parse_struct_field_decl() {
    const ParsedVisibility field_visibility = this->parse_visibility();
    const syntax::Token& field_name = this->expect_identifier_recovered("expected field name");
    this->expect_type_annotation_colon("expected ':' after field name");
    const syntax::TypeId field_type = this->parse_type();
    if (field_name.kind != TokenKind::identifier) {
        return std::nullopt;
    }
    const base::SourceRange end_range = this->check(TokenKind::semicolon) || this->check(TokenKind::comma)
        ? this->peek().range
        : this->type_range_or(field_type, field_name.range);
    return syntax::FieldDecl {
        field_name.text,
        field_type,
        this->merge(field_name.range, end_range),
        field_visibility.visibility,
    };
}

bool ItemParser::recover_struct_field_decl_separator() {
    if (this->check(TokenKind::r_brace)) {
        return false;
    }
    if (this->match(TokenKind::semicolon)) {
        this->reset_panic();
        return !this->check(TokenKind::r_brace);
    }

    this->report_here("expected ';' or '}' after field declaration");
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::struct_decl_field)) {
        this->synchronize(RecoveryContext::struct_decl_field);
    }
    if (this->match(TokenKind::semicolon) || this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_brace);
    }
    this->reset_panic();
    return token_starts_struct_decl_field(this->peek().kind);
}

std::optional<syntax::EnumCaseDecl> ItemParser::parse_enum_case_decl() {
    const syntax::Token& case_name = this->expect_identifier_recovered("expected enum case name");
    syntax::TypeId payload_type = syntax::INVALID_TYPE_ID;
    if (this->match(TokenKind::l_paren)) {
        payload_type = this->parse_type();
        this->expect_recovered(
            TokenKind::r_paren,
            "expected ')' after enum case payload type",
            RecoveryContext::enum_case_payload
        );
    }
    this->expect_initializer_equal("expected '=' after enum case name");
    const syntax::Token& value = this->expect(TokenKind::integer_literal, "expected integer literal enum value");
    if (case_name.kind != TokenKind::identifier) {
        return std::nullopt;
    }
    const base::SourceRange end_range = this->check(TokenKind::comma) || this->check(TokenKind::semicolon)
        ? this->peek().range
        : value.range;
    return syntax::EnumCaseDecl {
        case_name.text,
        payload_type,
        value.text,
        this->merge(case_name.range, end_range),
    };
}

bool ItemParser::recover_enum_case_separator() {
    if (this->check(TokenKind::r_brace)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_brace);
    }

    this->report_here("expected ',' or '}' after enum case");
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::enum_case)) {
        this->synchronize(RecoveryContext::enum_case);
    }
    if (this->match(TokenKind::comma) || this->match(TokenKind::semicolon)) {
        this->reset_panic();
        return !this->check(TokenKind::r_brace);
    }
    this->reset_panic();
    return token_starts_enum_case(this->peek().kind);
}

syntax::ItemId ItemParser::parse_opaque_struct_decl() {
    const syntax::Token& begin = this->expect(TokenKind::kw_opaque, "expected 'opaque'");
    this->expect(TokenKind::kw_struct, "expected 'struct' after 'opaque'");
    const syntax::Token& name = this->expect_identifier_recovered("expected opaque struct name");
    const syntax::Token& end = this->expect_item_terminator("expected ';' after opaque struct declaration");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::opaque_struct_decl;
    item.range = this->merge(begin.range, end.range);
    item.name = name.text;
    item.is_extern_c = true;
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

} // namespace aurex::parse
