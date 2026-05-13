#pragma once

#include <aurex/parse/parser_part_base.hpp>

namespace aurex::parse {

class PatternParser final : private ParserPartBase {
public:
    explicit PatternParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::PatternId parse_pattern();
    [[nodiscard]] syntax::PatternId parse_binding_pattern();

private:
    [[nodiscard]] syntax::PatternId parse_pattern_atom();
    [[nodiscard]] syntax::PatternId parse_binding_pattern_atom();
    [[nodiscard]] syntax::PatternId parse_tuple_binding_pattern();
    [[nodiscard]] bool recover_tuple_pattern_separator();
    void parse_payload_bindings(syntax::PatternNode& pattern);
    [[nodiscard]] const syntax::Token& expect_tuple_pattern_end();
    [[nodiscard]] const syntax::Token& expect_payload_binding_name();
    [[nodiscard]] const syntax::Token& expect_payload_binding_end();
};

} // namespace aurex::parse
