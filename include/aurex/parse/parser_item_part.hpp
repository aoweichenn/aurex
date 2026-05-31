#pragma once

#include <aurex/parse/parser_part_base.hpp>

#include <optional>
#include <vector>

namespace aurex::parse {

struct ParsedVisibility {
    syntax::Visibility visibility = syntax::Visibility::private_;
    bool explicit_visibility = false;
};

enum class FunctionBodyPolicy {
    allow_body_or_prototype,
    require_prototype,
};

class ItemParser final : private ParserPartBase {
public:
    explicit ItemParser(Parser& parser) noexcept : ParserPartBase(parser)
    {
    }

    [[nodiscard]] syntax::ModulePath parse_path() const;
    [[nodiscard]] syntax::ModulePartDecl parse_module_part_decl();
    [[nodiscard]] syntax::ModulePartHeader parse_module_part_header();
    [[nodiscard]] syntax::ImportDecl parse_import_decl();
    [[nodiscard]] syntax::UseDecl parse_use_decl();
    [[nodiscard]] syntax::ItemId parse_item();

private:
    [[nodiscard]] const syntax::Token& expect_item_terminator(std::string message) const;
    void expect_item_container_start(std::string message) const;
    [[nodiscard]] const syntax::Token& expect_item_container_end(std::string message) const;
    [[nodiscard]] std::optional<syntax::Token> parse_path_segment(std::string message) const;
    void parse_import_alias(syntax::ImportDecl& import);
    void recover_import_alias() const;
    [[nodiscard]] ParsedVisibility parse_visibility() const;
    void parse_use_alias(syntax::UseDecl& use);
    [[nodiscard]] std::vector<syntax::GenericParamDecl> parse_optional_generic_params();
    void reject_legacy_angle_generic_params() const;
    void parse_generic_params(std::vector<syntax::GenericParamDecl>& params);
    [[nodiscard]] std::optional<syntax::GenericParamDecl> parse_generic_param();
    [[nodiscard]] bool recover_generic_param_separator() const;
    [[nodiscard]] std::vector<syntax::GenericConstraintDecl> parse_optional_where_constraints();
    [[nodiscard]] std::optional<syntax::GenericConstraintDecl> parse_where_constraint();
    void parse_where_capabilities(syntax::GenericConstraintDecl& constraint);
    [[nodiscard]] std::vector<syntax::AssociatedTypeConstraintDecl> parse_associated_type_constraints();
    [[nodiscard]] std::optional<syntax::AssociatedTypeConstraintDecl> parse_associated_type_constraint();
    [[nodiscard]] bool recover_associated_type_constraint_separator() const;
    [[nodiscard]] bool recover_where_constraint_separator() const;
    [[nodiscard]] syntax::ItemId parse_const_decl();
    [[nodiscard]] syntax::ItemId parse_type_alias_decl();
    [[nodiscard]] syntax::ItemId parse_trait_associated_type_decl(ParsedVisibility visibility);
    [[nodiscard]] syntax::ItemId parse_impl_associated_type_decl(
        ParsedVisibility visibility, syntax::TypeId impl_type, syntax::TypeId trait_type);
    [[nodiscard]] syntax::ItemId parse_struct_decl();
    [[nodiscard]] std::optional<syntax::FieldDecl> parse_struct_field_decl();
    [[nodiscard]] bool recover_struct_field_decl_separator() const;
    [[nodiscard]] syntax::ItemId parse_enum_decl();
    [[nodiscard]] std::optional<syntax::EnumCaseDecl> parse_enum_case_decl();
    [[nodiscard]] bool recover_enum_case_separator() const;
    [[nodiscard]] syntax::ItemId parse_trait_decl();
    [[nodiscard]] syntax::ItemId parse_impl_block();
    [[nodiscard]] syntax::ItemId parse_extern_block();
    [[nodiscard]] syntax::ItemId parse_opaque_struct_decl();
    [[nodiscard]] syntax::ItemId parse_fn_decl(bool is_export_c, bool is_extern_c, bool is_unsafe = false,
        FunctionBodyPolicy body_policy = FunctionBodyPolicy::allow_body_or_prototype);
    void expect_param_list_start(std::string message) const;
    [[nodiscard]] std::vector<syntax::ParamDecl> parse_param_list(bool& is_variadic);
    [[nodiscard]] std::optional<syntax::ParamDecl> parse_param();
    [[nodiscard]] bool recover_param_separator(bool& is_variadic) const;
    [[nodiscard]] syntax::TypeId parse_optional_return_type() const;
    void parse_optional_abi_name(syntax::ItemNode& item);
    void expect_abi_attribute_argument_start() const;
    void parse_abi_name_argument(syntax::ItemNode& item) const;
    void recover_abi_attribute_argument_end() const;
};

} // namespace aurex::parse
