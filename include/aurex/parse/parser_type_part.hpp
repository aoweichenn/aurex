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
    [[nodiscard]] syntax::TypeId parse_primitive_type();
    void parse_generic_type_args(std::vector<syntax::TypeId>& args);
    [[nodiscard]] bool recover_generic_type_arg_separator();
    void expect_array_length_end();
};

} // namespace aurex::parse
