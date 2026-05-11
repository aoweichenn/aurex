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
    void expect_array_length_end();
};

} // namespace aurex::parse
