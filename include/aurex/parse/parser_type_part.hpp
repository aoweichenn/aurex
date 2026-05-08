#pragma once

#include "aurex/parse/parser_part_base.hpp"

namespace aurex::parse {

class TypeParser final : private ParserPartBase {
public:
    explicit TypeParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::TypeId parse_type();
    [[nodiscard]] std::vector<syntax::TypeId> parse_type_arg_list();

private:
    [[nodiscard]] syntax::TypeId parse_primitive_type();
    void expect_array_length_end();
    [[nodiscard]] bool recover_type_arg_separator();
};

} // namespace aurex::parse
