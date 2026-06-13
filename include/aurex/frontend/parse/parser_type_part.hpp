#pragma once

#include <aurex/frontend/parse/parser_part_base.hpp>

namespace aurex::parse {

class TypeParser final : private ParserPartBase {
public:
    explicit TypeParser(Parser& parser) noexcept : ParserPartBase(parser)
    {
    }

    [[nodiscard]] syntax::TypeId parse_type();

private:
    [[nodiscard]] syntax::TypeId parse_type_atom();
    [[nodiscard]] syntax::TypeId parse_named_type();
    [[nodiscard]] syntax::TypeId parse_dyn_trait_type();
    [[nodiscard]] syntax::TypeId parse_dyn_trait_principal_type(const base::SourceRange& begin_range);
    void parse_dyn_trait_args(syntax::TypeNode& type);
    [[nodiscard]] bool recover_dyn_trait_principal_separator() const;
    [[nodiscard]] bool recover_dyn_trait_arg_separator() const;
    [[nodiscard]] syntax::TypeId parse_tuple_or_parenthesized_type();
    [[nodiscard]] bool recover_tuple_type_separator() const;
    [[nodiscard]] const syntax::Token& expect_tuple_type_end(const syntax::Token& opening) const;
    [[nodiscard]] syntax::TypeId parse_function_type();
    [[nodiscard]] syntax::TypeId parse_function_type_after_fn(
        const base::SourceRange& begin_range, syntax::FunctionCallConv call_conv, bool is_unsafe);
    void parse_function_type_params(std::vector<syntax::TypeId>& params, bool& is_variadic);
    [[nodiscard]] bool recover_function_type_param_separator(bool& is_variadic) const;
    [[nodiscard]] syntax::TypeId parse_primitive_type() const;
    [[nodiscard]] syntax::ArrayLengthDecl parse_array_length();
    [[nodiscard]] syntax::ExprId parse_const_generic_expr_atom(std::string message) const;
    [[nodiscard]] syntax::GenericArgDecl parse_generic_arg();
    void parse_generic_type_args(std::vector<syntax::TypeId>& type_args, std::vector<syntax::GenericArgDecl>& args);
    [[nodiscard]] bool recover_generic_type_arg_separator() const;
    void reject_legacy_bracket_type_args() const;
    void expect_array_length_end() const;
};

} // namespace aurex::parse
