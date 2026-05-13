#pragma once

#include <aurex/parse/parser_part_base.hpp>

namespace aurex::parse {

class TypeParser final : private ParserPartBase {
public:
    explicit TypeParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::TypeId parse_type();

private:
    [[nodiscard]] syntax::TypeId parse_type_atom();
    [[nodiscard]] syntax::TypeId parse_tuple_or_parenthesized_type();
    [[nodiscard]] bool recover_tuple_type_separator();
    [[nodiscard]] const syntax::Token& expect_tuple_type_end();
    [[nodiscard]] syntax::TypeId parse_function_type();
    [[nodiscard]] syntax::TypeId parse_function_type_after_fn(
        base::SourceRange begin_range,
        syntax::FunctionCallConv call_conv,
        bool is_unsafe
    );
    void parse_function_type_params(std::vector<syntax::TypeId>& params, bool& is_variadic);
    [[nodiscard]] bool recover_function_type_param_separator(bool& is_variadic);
    [[nodiscard]] syntax::TypeId parse_primitive_type();
    void parse_generic_type_args(std::vector<syntax::TypeId>& args);
    [[nodiscard]] bool recover_generic_type_arg_separator();
    void reject_legacy_angle_type_args();
    void expect_array_length_end();
};

} // namespace aurex::parse
