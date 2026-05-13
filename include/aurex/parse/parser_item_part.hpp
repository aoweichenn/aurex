#pragma once

#include <aurex/parse/parser_part_base.hpp>

#include <optional>
#include <vector>

namespace aurex::parse {

struct ParsedVisibility {
    syntax::Visibility visibility = syntax::Visibility::private_;
    bool explicit_visibility = false;
};

class ItemParser final : private ParserPartBase {
public:
    explicit ItemParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ModulePath parse_path();
    [[nodiscard]] syntax::ImportDecl parse_import_decl();
    [[nodiscard]] syntax::ItemId parse_item();

private:
    [[nodiscard]] const syntax::Token& expect_item_terminator(std::string message);
    void expect_item_container_start(std::string message);
    [[nodiscard]] const syntax::Token& expect_item_container_end(std::string message);
    [[nodiscard]] std::optional<syntax::Token> parse_path_segment(std::string message);
    void parse_import_alias(syntax::ImportDecl& import);
    void recover_import_alias();
    [[nodiscard]] ParsedVisibility parse_visibility();
    [[nodiscard]] std::vector<syntax::GenericParamDecl> parse_optional_generic_params();
    void reject_legacy_angle_generic_params();
    void parse_generic_params(std::vector<syntax::GenericParamDecl>& params);
    [[nodiscard]] std::optional<syntax::GenericParamDecl> parse_generic_param();
    [[nodiscard]] bool recover_generic_param_separator();
    [[nodiscard]] syntax::ItemId parse_const_decl();
    [[nodiscard]] syntax::ItemId parse_type_alias_decl();
    [[nodiscard]] syntax::ItemId parse_struct_decl();
    [[nodiscard]] std::optional<syntax::FieldDecl> parse_struct_field_decl();
    [[nodiscard]] bool recover_struct_field_decl_separator();
    [[nodiscard]] syntax::ItemId parse_enum_decl();
    [[nodiscard]] std::optional<syntax::EnumCaseDecl> parse_enum_case_decl();
    [[nodiscard]] bool recover_enum_case_separator();
    [[nodiscard]] syntax::ItemId parse_impl_block();
    [[nodiscard]] syntax::ItemId parse_extern_block();
    [[nodiscard]] syntax::ItemId parse_opaque_struct_decl();
    [[nodiscard]] syntax::ItemId parse_fn_decl(bool is_export_c, bool is_extern_c, bool is_unsafe = false);
    void expect_param_list_start(std::string message);
    [[nodiscard]] std::vector<syntax::ParamDecl> parse_param_list(bool& is_variadic);
    [[nodiscard]] std::optional<syntax::ParamDecl> parse_param();
    [[nodiscard]] bool recover_param_separator(bool& is_variadic);
    [[nodiscard]] syntax::TypeId parse_optional_return_type();
    void parse_optional_abi_name(syntax::ItemNode& item);
    void reject_optional_where_clause();
    void expect_abi_attribute_argument_start();
    void parse_abi_name_argument(syntax::ItemNode& item);
    void recover_abi_attribute_argument_end();
};

} // namespace aurex::parse
