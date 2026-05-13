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
    [[nodiscard]] syntax::PatternId parse_destructure_pattern_atom();
    [[nodiscard]] syntax::PatternId parse_tuple_pattern(bool destructure_context);
    [[nodiscard]] syntax::PatternId parse_slice_pattern();
    [[nodiscard]] syntax::PatternId parse_struct_pattern(const syntax::Token& name);
    [[nodiscard]] bool recover_tuple_pattern_separator();
    [[nodiscard]] bool recover_slice_pattern_separator();
    [[nodiscard]] bool recover_struct_pattern_separator();
    [[nodiscard]] bool match_slice_rest_marker();
    void parse_payload_patterns(syntax::PatternNode& pattern);
    [[nodiscard]] const syntax::Token& expect_tuple_pattern_end();
    [[nodiscard]] const syntax::Token& expect_slice_pattern_end();
    [[nodiscard]] const syntax::Token& expect_payload_pattern_end();
    [[nodiscard]] const syntax::Token& expect_struct_pattern_end();
};

} // namespace aurex::parse
