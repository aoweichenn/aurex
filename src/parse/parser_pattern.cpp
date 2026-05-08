#include "aurex/parse/parser_parts.hpp"

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::PatternId PatternParser::parse_pattern() {
    const syntax::PatternId first = this->parse_pattern_atom();
    if (!this->match(TokenKind::pipe)) {
        return first;
    }
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::or_pattern;
    pattern.alternatives.push_back(first);
    base::SourceRange range = syntax::is_valid(first) && first.value < this->session_.module.patterns.size()
        ? this->session_.module.patterns[first.value].range
        : this->previous().range;
    do {
        const syntax::PatternId alternative = this->parse_pattern_atom();
        pattern.alternatives.push_back(alternative);
        if (syntax::is_valid(alternative) && alternative.value < this->session_.module.patterns.size()) {
            range = this->merge(range, this->session_.module.patterns[alternative.value].range);
        }
    } while (this->match(TokenKind::pipe));
    pattern.range = range;
    return this->session_.module.push_pattern(pattern);
}

syntax::PatternId PatternParser::parse_pattern_atom() {
    if (this->match(TokenKind::identifier)) {
        const syntax::Token& first = this->previous();
        if (first.text == "_") {
            syntax::PatternNode pattern;
            pattern.kind = syntax::PatternKind::wildcard;
            pattern.range = first.range;
            return this->session_.module.push_pattern(pattern);
        }
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::enum_case;
        pattern.case_name = first.text;
        pattern.range = first.range;
        if (this->match(TokenKind::dot)) {
            const syntax::Token& case_name = this->expect(TokenKind::identifier, "expected enum case name after '.'");
            pattern.enum_name = first.text;
            pattern.case_name = case_name.text;
            pattern.scoped = true;
            pattern.range = this->merge(first.range, case_name.range);
        }
        if (this->match(TokenKind::l_paren)) {
            const syntax::Token& binding = this->expect(TokenKind::identifier, "expected payload binding name");
            if (binding.kind == TokenKind::identifier) {
                pattern.binding_name = binding.text;
            }
            const syntax::Token& end = this->expect(TokenKind::r_paren, "expected ')' after payload binding");
            pattern.range = this->merge(pattern.range, end.range);
        }
        return this->session_.module.push_pattern(pattern);
    }
    if (this->match(TokenKind::integer_literal) || this->match(TokenKind::kw_true) || this->match(TokenKind::kw_false)) {
        const syntax::Token& token = this->previous();
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::literal;
        pattern.case_name = token.text;
        pattern.range = token.range;
        return this->session_.module.push_pattern(pattern);
    }
    if (this->match(TokenKind::dot)) {
        const syntax::Token& dot = this->previous();
        const syntax::Token& case_name = this->expect(TokenKind::identifier, "expected enum case name after '.'");
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::enum_case;
        pattern.case_name = case_name.text;
        pattern.scoped = true;
        pattern.range = this->merge(dot.range, case_name.range);
        if (this->match(TokenKind::l_paren)) {
            const syntax::Token& binding = this->expect(TokenKind::identifier, "expected payload binding name");
            if (binding.kind == TokenKind::identifier) {
                pattern.binding_name = binding.text;
            }
            const syntax::Token& end = this->expect(TokenKind::r_paren, "expected ')' after payload binding");
            pattern.range = this->merge(pattern.range, end.range);
        }
        return this->session_.module.push_pattern(pattern);
    }
    this->report_here("expected match pattern");
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::wildcard;
    pattern.range = this->peek().range;
    this->advance();
    return this->session_.module.push_pattern(pattern);
}

} // namespace aurex::parse
