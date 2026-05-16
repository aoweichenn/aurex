#pragma once

#include <aurex/parse/parser_part_base.hpp>

#include <vector>

namespace aurex::parse {

class PatternParser final : private ParserPartBase {
public:
    explicit PatternParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::PatternId parse_pattern();

private:
    [[nodiscard]] syntax::PatternId parse_pattern_atom();
    [[nodiscard]] syntax::PatternId parse_destructure_pattern_atom();
    [[nodiscard]] syntax::PatternId parse_identifier_pattern(const syntax::Token& first);
    [[nodiscard]] syntax::PatternId parse_explicit_enum_case_pattern(const syntax::Token& first);
    [[nodiscard]] syntax::PatternId parse_shorthand_enum_case_pattern(const syntax::Token& dot);
    [[nodiscard]] syntax::PatternId parse_literal_pattern(const syntax::Token& token);
    [[nodiscard]] syntax::PatternId parse_fallback_wildcard_pattern();
    [[nodiscard]] syntax::TypeId push_explicit_enum_case_type(
        const std::vector<syntax::Token>& parts,
        base::usize type_part_count,
        std::vector<syntax::TypeId> type_args,
        const base::SourceRange& type_range
    );
    [[nodiscard]] syntax::PatternId parse_tuple_pattern();
    [[nodiscard]] syntax::PatternId parse_slice_pattern();
    [[nodiscard]] syntax::PatternId parse_struct_pattern(const syntax::Token& name);
    [[nodiscard]] bool recover_tuple_pattern_separator();
    [[nodiscard]] bool recover_slice_pattern_separator();
    [[nodiscard]] bool recover_struct_pattern_separator();
    [[nodiscard]] bool recover_generic_type_arg_separator();
    [[nodiscard]] bool match_slice_rest_marker();
    void parse_pattern_generic_type_args(std::vector<syntax::TypeId>& args);
    void parse_payload_patterns(syntax::PatternNode& pattern);
    void consume_bare_enum_case_payload_recovery(const syntax::Token& first, base::SourceRange& range);
    [[nodiscard]] const syntax::Token& expect_generic_type_args_end();
    [[nodiscard]] const syntax::Token& expect_tuple_pattern_end();
    [[nodiscard]] const syntax::Token& expect_slice_pattern_end();
    [[nodiscard]] const syntax::Token& expect_payload_pattern_end();
    [[nodiscard]] const syntax::Token& expect_struct_pattern_end();
};

} // namespace aurex::parse
