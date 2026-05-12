#pragma once

#include <aurex/parse/parser_part_base.hpp>

namespace aurex::parse {

class PatternParser final : private ParserPartBase {
public:
    explicit PatternParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::PatternId parse_pattern();

private:
    [[nodiscard]] syntax::PatternId parse_pattern_atom();
    void parse_payload_bindings(syntax::PatternNode& pattern);
    [[nodiscard]] const syntax::Token& expect_payload_binding_name();
    [[nodiscard]] const syntax::Token& expect_payload_binding_end();
};

} // namespace aurex::parse
