#include "aurex/parse/parser_parts.hpp"

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

syntax::PatternId PatternParser::parse_pattern() {
    const syntax::PatternId first = parse_pattern_atom();
    if (!match(TokenKind::pipe)) {
        return first;
    }
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::or_pattern;
    pattern.alternatives.push_back(first);
    base::SourceRange range = syntax::is_valid(first) && first.value < session_.module.patterns.size()
        ? session_.module.patterns[first.value].range
        : previous().range;
    do {
        const syntax::PatternId alternative = parse_pattern_atom();
        pattern.alternatives.push_back(alternative);
        if (syntax::is_valid(alternative) && alternative.value < session_.module.patterns.size()) {
            range = merge(range, session_.module.patterns[alternative.value].range);
        }
    } while (match(TokenKind::pipe));
    pattern.range = range;
    return session_.module.push_pattern(pattern);
}

syntax::PatternId PatternParser::parse_pattern_atom() {
    if (match(TokenKind::identifier)) {
        const syntax::Token& first = previous();
        if (first.text == "_") {
            syntax::PatternNode pattern;
            pattern.kind = syntax::PatternKind::wildcard;
            pattern.range = first.range;
            return session_.module.push_pattern(pattern);
        }
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::enum_case;
        pattern.case_name = first.text;
        pattern.range = first.range;
        if (match(TokenKind::dot)) {
            const syntax::Token& case_name = expect(TokenKind::identifier, "expected enum case name after '.'");
            pattern.enum_name = first.text;
            pattern.case_name = case_name.text;
            pattern.scoped = true;
            pattern.range = merge(first.range, case_name.range);
        }
        if (match(TokenKind::l_paren)) {
            const syntax::Token& binding = expect(TokenKind::identifier, "expected payload binding name");
            if (binding.kind == TokenKind::identifier) {
                pattern.binding_name = binding.text;
            }
            const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after payload binding");
            pattern.range = merge(pattern.range, end.range);
        }
        return session_.module.push_pattern(pattern);
    }
    if (match(TokenKind::integer_literal) || match(TokenKind::kw_true) || match(TokenKind::kw_false)) {
        const syntax::Token& token = previous();
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::literal;
        pattern.case_name = token.text;
        pattern.range = token.range;
        return session_.module.push_pattern(pattern);
    }
    if (match(TokenKind::dot)) {
        const syntax::Token& dot = previous();
        const syntax::Token& case_name = expect(TokenKind::identifier, "expected enum case name after '.'");
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::enum_case;
        pattern.case_name = case_name.text;
        pattern.scoped = true;
        pattern.range = merge(dot.range, case_name.range);
        if (match(TokenKind::l_paren)) {
            const syntax::Token& binding = expect(TokenKind::identifier, "expected payload binding name");
            if (binding.kind == TokenKind::identifier) {
                pattern.binding_name = binding.text;
            }
            const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after payload binding");
            pattern.range = merge(pattern.range, end.range);
        }
        return session_.module.push_pattern(pattern);
    }
    report_here("expected match pattern");
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::wildcard;
    pattern.range = peek().range;
    advance();
    return session_.module.push_pattern(pattern);
}

} // namespace aurex::parse
