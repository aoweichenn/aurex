#include <aurex/parse/parser_item_part.hpp>
#include <aurex/parse/parser_messages.hpp>
#include <aurex/parse/recovery.hpp>

#include <optional>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::ItemId ItemParser::parse_struct_decl()
{
    const syntax::Token& begin = this->expect(TokenKind::kw_struct, std::string(PARSER_EXPECT_STRUCT_KEYWORD));
    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_STRUCT_NAME));
    std::vector<syntax::GenericParamDecl> generic_params = this->parse_optional_generic_params();
    std::vector<syntax::GenericConstraintDecl> where_constraints = this->parse_optional_where_constraints();
    this->expect_item_container_start(std::string(PARSER_EXPECT_STRUCT_BODY));

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::struct_decl;
    item.name = name.text();
    item.generic_params = std::move(generic_params);
    item.where_constraints = std::move(where_constraints);

    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        if (std::optional<syntax::FieldDecl> field = this->parse_struct_field_decl()) {
            item.fields.push_back(field.value());
        }
        this->reset_panic();
        if (!this->recover_struct_field_decl_separator()) {
            break;
        }
    }

    const syntax::Token& end = this->expect_item_container_end(std::string(PARSER_EXPECT_STRUCT_END));
    item.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

syntax::ItemId ItemParser::parse_enum_decl()
{
    const syntax::Token& begin = this->expect(TokenKind::kw_enum, std::string(PARSER_EXPECT_ENUM_KEYWORD));
    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_ENUM_NAME));
    std::vector<syntax::GenericParamDecl> generic_params = this->parse_optional_generic_params();
    syntax::TypeId base_type = syntax::INVALID_TYPE_ID;
    if (this->match(TokenKind::colon)) {
        base_type = this->parse_type();
    }
    std::vector<syntax::GenericConstraintDecl> where_constraints = this->parse_optional_where_constraints();
    if (syntax::is_valid(base_type)) {
        this->expect_item_container_start(std::string(PARSER_EXPECT_ENUM_BODY_AFTER_BASE));
    } else {
        this->expect_item_container_start(std::string(PARSER_EXPECT_ENUM_BODY_AFTER_NAME));
    }

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::enum_decl;
    item.name = name.text();
    item.generic_params = std::move(generic_params);
    item.where_constraints = std::move(where_constraints);
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

    const syntax::Token& end = this->expect_item_container_end(std::string(PARSER_EXPECT_ENUM_END));
    item.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

std::optional<syntax::FieldDecl> ItemParser::parse_struct_field_decl()
{
    const ParsedVisibility field_visibility = this->parse_visibility();
    const syntax::Token& field_name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_FIELD_NAME));
    this->expect_type_annotation_colon(std::string(PARSER_EXPECT_FIELD_TYPE_COLON));
    const syntax::TypeId field_type = this->parse_type();
    if (field_name.kind != TokenKind::identifier) {
        return std::nullopt;
    }
    const base::SourceRange end_range = this->check(TokenKind::semicolon) || this->check(TokenKind::comma)
        ? this->peek().range
        : this->type_range_or(field_type, field_name.range);
    return syntax::FieldDecl{
        field_name.text(),
        field_type,
        this->merge(field_name.range, end_range),
        field_visibility.visibility,
    };
}

bool ItemParser::recover_struct_field_decl_separator() const
{
    if (this->check(TokenKind::r_brace)) {
        return false;
    }
    if (this->match(TokenKind::semicolon)) {
        this->reset_panic();
        return !this->check(TokenKind::r_brace);
    }

    this->report_here(std::string(PARSER_EXPECT_FIELD_SEPARATOR));
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

std::optional<syntax::EnumCaseDecl> ItemParser::parse_enum_case_decl()
{
    const syntax::Token& case_name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_ENUM_CASE_NAME));
    syntax::TypeId payload_type = syntax::INVALID_TYPE_ID;
    syntax::AstArenaVector<syntax::TypeId> payload_types = this->session_.module.make_item_list<syntax::TypeId>();
    base::SourceRange payload_end_range = case_name.range;
    if (this->match(TokenKind::l_paren)) {
        if (this->check(TokenKind::r_paren)) {
            this->report_here(std::string(PARSER_EXPECT_ENUM_CASE_PAYLOAD_TYPE));
        } else {
            payload_type = this->parse_type();
            payload_types.push_back(payload_type);
            payload_end_range = this->type_range_or(payload_type, case_name.range);
            while (this->match(TokenKind::comma)) {
                if (this->check(TokenKind::r_paren)) {
                    break;
                }
                const syntax::TypeId next_payload_type = this->parse_type();
                payload_types.push_back(next_payload_type);
                payload_end_range = this->type_range_or(next_payload_type, payload_end_range);
            }
        }
        this->expect_recovered(
            TokenKind::r_paren, std::string(PARSER_EXPECT_ENUM_CASE_PAYLOAD_END), RecoveryContext::enum_case_payload);
    }
    std::string_view value_text;
    base::SourceRange value_range = payload_types.empty() ? case_name.range : payload_end_range;
    if (this->match(TokenKind::equal)) {
        const syntax::Token& value = this->expect(TokenKind::integer_literal, std::string(PARSER_EXPECT_ENUM_VALUE));
        value_text = value.text();
        value_range = value.range;
    }
    if (case_name.kind != TokenKind::identifier) {
        return std::nullopt;
    }
    const base::SourceRange end_range =
        this->check(TokenKind::comma) || this->check(TokenKind::semicolon) ? this->peek().range : value_range;
    return syntax::EnumCaseDecl{
        case_name.text(),
        payload_type,
        std::move(payload_types),
        value_text,
        this->merge(case_name.range, end_range),
    };
}

bool ItemParser::recover_enum_case_separator() const
{
    if (this->check(TokenKind::r_brace)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_brace);
    }

    this->report_here(std::string(PARSER_EXPECT_ENUM_CASE_SEPARATOR));
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

syntax::ItemId ItemParser::parse_opaque_struct_decl()
{
    const syntax::Token& begin = this->expect(TokenKind::kw_opaque, std::string(PARSER_EXPECT_OPAQUE_KEYWORD));
    this->expect(TokenKind::kw_struct, std::string(PARSER_EXPECT_STRUCT_AFTER_OPAQUE));
    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_OPAQUE_STRUCT_NAME));
    const syntax::Token& end = this->expect_item_terminator(std::string(PARSER_EXPECT_OPAQUE_STRUCT_TERMINATOR));

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::opaque_struct_decl;
    item.range = this->merge(begin.range, end.range);
    item.name = name.text();
    item.is_extern_c = true;
    this->reset_panic();
    return this->session_.module.push_item(std::move(item));
}

} // namespace aurex::parse
