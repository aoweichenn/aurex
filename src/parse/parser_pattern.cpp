#include <aurex/parse/parser_pattern_part.hpp>

#include <aurex/parse/parser_messages.hpp>
#include <aurex/parse/recovery.hpp>

#include <utility>

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
    base::SourceRange range = this->pattern_range_or(first, this->previous().range);
    do {
        const syntax::PatternId alternative = this->parse_pattern_atom();
        pattern.alternatives.push_back(alternative);
        if (syntax::is_valid(alternative)) {
            range = this->merge(range, this->pattern_range_or(alternative, range));
        }
    } while (this->match(TokenKind::pipe));
    pattern.range = range;
    return this->session_.module.push_pattern(pattern);
}

syntax::PatternId PatternParser::parse_binding_pattern() {
    return this->parse_binding_pattern_atom();
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
            const syntax::Token& case_name =
                this->expect_identifier_recovered(std::string(PARSER_EXPECT_ENUM_CASE_AFTER_DOT));
            pattern.enum_name = first.text;
            pattern.case_name = case_name.text;
            pattern.scoped = true;
            pattern.range = this->merge(first.range, case_name.range);
        }
        if (this->match(TokenKind::l_paren)) {
            this->parse_payload_bindings(pattern);
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
        const syntax::Token& case_name =
            this->expect_identifier_recovered(std::string(PARSER_EXPECT_ENUM_CASE_AFTER_DOT));
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::enum_case;
        pattern.case_name = case_name.text;
        pattern.scoped = true;
        pattern.range = this->merge(dot.range, case_name.range);
        if (this->match(TokenKind::l_paren)) {
            this->parse_payload_bindings(pattern);
        }
        return this->session_.module.push_pattern(pattern);
    }
    this->report_here(std::string(PARSER_EXPECT_MATCH_PATTERN));
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::wildcard;
    pattern.range = this->peek().range;
    this->advance();
    return this->session_.module.push_pattern(pattern);
}

syntax::PatternId PatternParser::parse_binding_pattern_atom() {
    if (this->match(TokenKind::identifier)) {
        const syntax::Token& token = this->previous();
        syntax::PatternNode pattern;
        pattern.kind = token.text == "_" ? syntax::PatternKind::wildcard : syntax::PatternKind::binding;
        pattern.binding_name = token.text == "_" ? std::string_view {} : token.text;
        pattern.range = token.range;
        return this->session_.module.push_pattern(pattern);
    }
    if (this->check(TokenKind::l_paren)) {
        return this->parse_tuple_binding_pattern();
    }
    this->report_here(std::string(PARSER_EXPECT_MATCH_PATTERN));
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::wildcard;
    pattern.range = this->peek().range;
    this->advance();
    return this->session_.module.push_pattern(pattern);
}

syntax::PatternId PatternParser::parse_tuple_binding_pattern() {
    const syntax::Token& begin = this->expect(TokenKind::l_paren, std::string(PARSER_EXPECT_TUPLE_PATTERN_END));
    if (this->check(TokenKind::r_paren)) {
        this->report_here(std::string(PARSER_EMPTY_TUPLE_PATTERN_UNSUPPORTED));
        const syntax::Token& end = this->expect_tuple_pattern_end();
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::wildcard;
        pattern.range = this->merge(begin.range, end.range);
        return this->session_.module.push_pattern(pattern);
    }

    const syntax::PatternId first = this->parse_binding_pattern_atom();
    if (!this->match(TokenKind::comma)) {
        static_cast<void>(this->expect_tuple_pattern_end());
        return first;
    }

    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::tuple;
    pattern.elements.push_back(first);
    while (!this->is_eof() && !this->check(TokenKind::r_paren)) {
        pattern.elements.push_back(this->parse_binding_pattern_atom());
        this->reset_panic();
        if (!this->recover_tuple_pattern_separator()) {
            break;
        }
    }
    const syntax::Token& end = this->expect_tuple_pattern_end();
    pattern.range = this->merge(begin.range, end.range);
    return this->session_.module.push_pattern(std::move(pattern));
}

bool PatternParser::recover_tuple_pattern_separator() {
    if (this->check(TokenKind::r_paren)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }

    this->report_here(std::string(PARSER_EXPECT_TUPLE_PATTERN_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::pattern_payload)) {
        this->synchronize(RecoveryContext::pattern_payload);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }
    this->reset_panic();
    return false;
}

void PatternParser::parse_payload_bindings(syntax::PatternNode& pattern) {
    if (this->check(TokenKind::r_paren)) {
        this->report_here(std::string(PARSER_EXPECT_PAYLOAD_BINDING));
    } else {
        while (!this->is_eof()) {
            const syntax::Token& binding = this->expect_payload_binding_name();
            if (binding.kind == TokenKind::identifier) {
                pattern.binding_names.push_back(binding.text);
            }
            if (!this->match(TokenKind::comma)) {
                break;
            }
            if (this->check(TokenKind::r_paren)) {
                break;
            }
        }
    }
    const syntax::Token& end = this->expect_payload_binding_end();
    pattern.range = this->merge(pattern.range, end.range);
}

const syntax::Token& PatternParser::expect_tuple_pattern_end() {
    return this->expect_recovered(
        TokenKind::r_paren,
        std::string(PARSER_EXPECT_TUPLE_PATTERN_END),
        RecoveryContext::pattern_payload
    );
}

const syntax::Token& PatternParser::expect_payload_binding_name() {
    return this->expect_recovered(
        TokenKind::identifier,
        std::string(PARSER_EXPECT_PAYLOAD_BINDING),
        RecoveryContext::pattern_payload
    );
}

const syntax::Token& PatternParser::expect_payload_binding_end() {
    return this->expect_recovered(
        TokenKind::r_paren,
        std::string(PARSER_EXPECT_PAYLOAD_BINDING_END),
        RecoveryContext::pattern_payload
    );
}

} // namespace aurex::parse
