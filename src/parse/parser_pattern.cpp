#include <aurex/parse/parser_pattern_part.hpp>

#include <aurex/parse/parser_messages.hpp>
#include <aurex/parse/recovery.hpp>

#include <utility>
#include <vector>

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
    return this->session_.module.push_pattern(std::move(pattern));
}

syntax::PatternId PatternParser::parse_pattern_atom() {
    if (this->check(TokenKind::l_paren)) {
        return this->parse_tuple_pattern();
    }
    if (this->check(TokenKind::l_bracket)) {
        return this->parse_slice_pattern();
    }
    if (this->match(TokenKind::kw_const)) {
        const syntax::Token& keyword = this->previous();
        const syntax::Token& name =
            this->expect_identifier_recovered(std::string(PARSER_EXPECT_CONST_NAME));
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::const_;
        pattern.binding_name = name.text;
        pattern.range = this->merge(keyword.range, name.range);
        return this->session_.module.push_pattern(pattern);
    }
    if (this->match(TokenKind::identifier)) {
        return this->parse_identifier_pattern(this->previous());
    }
    if (this->match(TokenKind::integer_literal) || this->match(TokenKind::kw_true) || this->match(TokenKind::kw_false)) {
        return this->parse_literal_pattern(this->previous());
    }
    if (this->match(TokenKind::dot)) {
        return this->parse_shorthand_enum_case_pattern(this->previous());
    }
    return this->parse_fallback_wildcard_pattern();
}

syntax::PatternId PatternParser::parse_destructure_pattern_atom() {
    return this->parse_pattern_atom();
}

syntax::PatternId PatternParser::parse_identifier_pattern(const syntax::Token& first) {
    if (first.text == "_") {
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::wildcard;
        pattern.range = first.range;
        return this->session_.module.push_pattern(pattern);
    }
    if (this->check(TokenKind::l_brace)) {
        return this->parse_struct_pattern(first);
    }
    if (this->check(TokenKind::l_paren)) {
        this->report_here(std::string(PARSER_BARE_ENUM_CASE_PATTERN_UNSUPPORTED));
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::binding;
        pattern.binding_name = first.text;
        pattern.range = first.range;
        this->consume_bare_enum_case_payload_recovery(first, pattern.range);
        return this->session_.module.push_pattern(std::move(pattern));
    }
    if (this->check(TokenKind::dot) || this->check(TokenKind::l_bracket)) {
        return this->parse_explicit_enum_case_pattern(first);
    }

    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::binding;
    pattern.binding_name = first.text;
    pattern.range = first.range;
    return this->session_.module.push_pattern(std::move(pattern));
}

syntax::PatternId PatternParser::parse_explicit_enum_case_pattern(const syntax::Token& first) {
    std::vector<syntax::Token> parts;
    parts.push_back(first);

    const auto make_pattern = [&](syntax::TypeId enum_type, const syntax::Token& case_name) {
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::enum_case;
        pattern.enum_type = enum_type;
        pattern.case_name = case_name.text;
        pattern.scoped = true;
        pattern.range = this->merge(first.range, case_name.range);
        if (this->match(TokenKind::l_paren)) {
            this->parse_payload_patterns(pattern);
        }
        return this->session_.module.push_pattern(std::move(pattern));
    };

    if (this->match(TokenKind::l_bracket)) {
        std::vector<syntax::TypeId> type_args;
        this->parse_pattern_generic_type_args(type_args);
        const syntax::Token& end = this->expect_generic_type_args_end();
        const syntax::TypeId enum_type = this->push_explicit_enum_case_type(
            parts,
            parts.size(),
            std::move(type_args),
            this->merge(first.range, end.range)
        );
        if (!this->match(TokenKind::dot)) {
            this->report_here(std::string(PARSER_EXPECT_ENUM_CASE_PATTERN_DOT));
            syntax::PatternNode pattern;
            pattern.kind = syntax::PatternKind::binding;
            pattern.binding_name = first.text;
            pattern.range = this->merge(first.range, end.range);
            return this->session_.module.push_pattern(std::move(pattern));
        }
        const syntax::Token& case_name =
            this->expect_identifier_recovered(std::string(PARSER_EXPECT_ENUM_CASE_AFTER_DOT));
        return make_pattern(enum_type, case_name);
    }

    while (this->match(TokenKind::dot)) {
        parts.push_back(this->expect_identifier_recovered(std::string(PARSER_EXPECT_ENUM_CASE_AFTER_DOT)));
        if (this->match(TokenKind::l_bracket)) {
            std::vector<syntax::TypeId> type_args;
            this->parse_pattern_generic_type_args(type_args);
            const syntax::Token& end = this->expect_generic_type_args_end();
            const syntax::TypeId enum_type = this->push_explicit_enum_case_type(
                parts,
                parts.size(),
                std::move(type_args),
                this->merge(parts.front().range, end.range)
            );
            if (!this->match(TokenKind::dot)) {
                this->report_here(std::string(PARSER_EXPECT_ENUM_CASE_PATTERN_DOT));
                syntax::PatternNode pattern;
                pattern.kind = syntax::PatternKind::binding;
                pattern.binding_name = first.text;
                pattern.range = this->merge(first.range, end.range);
                return this->session_.module.push_pattern(std::move(pattern));
            }
            const syntax::Token& case_name =
                this->expect_identifier_recovered(std::string(PARSER_EXPECT_ENUM_CASE_AFTER_DOT));
            return make_pattern(enum_type, case_name);
        }
    }

    const base::usize type_part_count = parts.size() > 1 ? parts.size() - 1 : parts.size();
    const syntax::TypeId enum_type = this->push_explicit_enum_case_type(
        parts,
        type_part_count,
        {},
        this->merge(parts.front().range, parts[type_part_count - 1].range)
    );
    const syntax::Token& case_name = parts.back();
    return make_pattern(enum_type, case_name);
}

syntax::PatternId PatternParser::parse_shorthand_enum_case_pattern(const syntax::Token& dot) {
    const syntax::Token& case_name =
        this->expect_identifier_recovered(std::string(PARSER_EXPECT_ENUM_CASE_AFTER_DOT));
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::enum_case;
    pattern.case_name = case_name.text;
    pattern.scoped = true;
    pattern.range = this->merge(dot.range, case_name.range);
    if (this->match(TokenKind::l_paren)) {
        this->parse_payload_patterns(pattern);
    }
    return this->session_.module.push_pattern(std::move(pattern));
}

syntax::PatternId PatternParser::parse_literal_pattern(const syntax::Token& token) const
{
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::literal;
    pattern.case_name = token.text;
    pattern.range = token.range;
    return this->session_.module.push_pattern(pattern);
}

syntax::PatternId PatternParser::parse_fallback_wildcard_pattern() const
{
    this->report_here(std::string(PARSER_EXPECT_MATCH_PATTERN));
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::wildcard;
    pattern.range = this->peek().range;
    this->advance();
    return this->session_.module.push_pattern(pattern);
}

syntax::TypeId PatternParser::push_explicit_enum_case_type(
    const std::vector<syntax::Token>& parts,
    const base::usize type_part_count,
    std::vector<syntax::TypeId> type_args,
    const base::SourceRange& type_range
) const
{
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::named;
    type.range = type_range;
    type.name = parts[type_part_count - 1].text;
    type.type_args = std::move(type_args);
    if (type_part_count > 1) {
        type.scope_range = this->merge(parts.front().range, parts[type_part_count - 2].range);
        type.scope_parts.reserve(type_part_count - 1);
        for (base::usize i = 0; i + 1 < type_part_count; ++i) {
            type.scope_parts.push_back(parts[i].text);
        }
        type.scope_name = type.scope_parts.front();
    }
    return this->session_.module.push_type(std::move(type));
}

syntax::PatternId PatternParser::parse_tuple_pattern() {
    const syntax::Token& begin = this->expect(TokenKind::l_paren, std::string(PARSER_EXPECT_TUPLE_PATTERN_END));
    if (this->check(TokenKind::r_paren)) {
        this->report_here(std::string(PARSER_EMPTY_TUPLE_PATTERN_UNSUPPORTED));
        const syntax::Token& end = this->expect_tuple_pattern_end();
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::wildcard;
        pattern.range = this->merge(begin.range, end.range);
        return this->session_.module.push_pattern(pattern);
    }

    const syntax::PatternId first = this->parse_destructure_pattern_atom();
    if (!this->match(TokenKind::comma)) {
        static_cast<void>(this->expect_tuple_pattern_end());
        return first;
    }

    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::tuple;
    pattern.elements.push_back(first);
    while (!this->is_eof() && !this->check(TokenKind::r_paren)) {
        pattern.elements.push_back(this->parse_destructure_pattern_atom());
        this->reset_panic();
        if (!this->recover_tuple_pattern_separator()) {
            break;
        }
    }
    const syntax::Token& end = this->expect_tuple_pattern_end();
    pattern.range = this->merge(begin.range, end.range);
    return this->session_.module.push_pattern(std::move(pattern));
}

syntax::PatternId PatternParser::parse_slice_pattern() {
    const syntax::Token& begin = this->expect(TokenKind::l_bracket, std::string(PARSER_EXPECT_SLICE_PATTERN_END));
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::slice;
    pattern.range = begin.range;

    while (!this->is_eof() && !this->check(TokenKind::r_bracket)) {
        if (this->match_slice_rest_marker()) {
            if (pattern.has_slice_rest) {
                this->report_at(this->previous(), std::string(PARSER_DUPLICATE_SLICE_PATTERN_REST));
            } else {
                pattern.has_slice_rest = true;
                pattern.slice_rest_index = pattern.elements.size();
            }
        } else if (this->match(TokenKind::ellipsis)) {
            this->report_at(this->previous(), std::string(PARSER_EXPECT_SLICE_PATTERN_REST));
            if (!pattern.has_slice_rest) {
                pattern.has_slice_rest = true;
                pattern.slice_rest_index = pattern.elements.size();
            }
        } else {
            pattern.elements.push_back(this->parse_destructure_pattern_atom());
        }
        this->reset_panic();
        if (!this->recover_slice_pattern_separator()) {
            break;
        }
    }

    const syntax::Token& end = this->expect_slice_pattern_end();
    pattern.range = this->merge(begin.range, end.range);
    return this->session_.module.push_pattern(std::move(pattern));
}

syntax::PatternId PatternParser::parse_struct_pattern(const syntax::Token& name) {
    const syntax::Token& begin = this->expect(TokenKind::l_brace, std::string(PARSER_EXPECT_STRUCT_PATTERN_END));
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::struct_;
    pattern.struct_name = name.text;
    pattern.range = this->merge(name.range, begin.range);
    while (!this->is_eof() && !this->check(TokenKind::r_brace)) {
        const syntax::Token& field_name =
            this->expect_identifier_recovered(std::string(PARSER_EXPECT_STRUCT_PATTERN_FIELD));
        syntax::PatternId field_pattern = syntax::INVALID_PATTERN_ID;
        if (this->match(TokenKind::colon)) {
            field_pattern = this->parse_destructure_pattern_atom();
        } else {
            syntax::PatternNode binding;
            binding.kind = field_name.text == "_" ? syntax::PatternKind::wildcard : syntax::PatternKind::binding;
            binding.binding_name = field_name.text == "_" ? std::string_view {} : field_name.text;
            binding.range = field_name.range;
            field_pattern = this->session_.module.push_pattern(binding);
        }
        pattern.field_patterns.push_back(syntax::FieldPattern {
            field_name.text,
            field_pattern,
            this->merge(field_name.range, this->pattern_range_or(field_pattern, field_name.range)),
        });
        this->reset_panic();
        if (!this->recover_struct_pattern_separator()) {
            break;
        }
    }
    const syntax::Token& end = this->expect_struct_pattern_end();
    pattern.range = this->merge(name.range, end.range);
    return this->session_.module.push_pattern(std::move(pattern));
}

bool PatternParser::recover_tuple_pattern_separator() const
{
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

bool PatternParser::recover_slice_pattern_separator() const
{
    if (this->check(TokenKind::r_bracket)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }

    this->report_here(std::string(PARSER_EXPECT_SLICE_PATTERN_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::pattern_payload)) {
        this->synchronize(RecoveryContext::pattern_payload);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }
    this->reset_panic();
    return false;
}

bool PatternParser::recover_struct_pattern_separator() const
{
    if (this->check(TokenKind::r_brace)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_brace);
    }

    this->report_here(std::string(PARSER_EXPECT_STRUCT_PATTERN_FIELD_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::pattern_payload)) {
        this->synchronize(RecoveryContext::pattern_payload);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_brace);
    }
    this->reset_panic();
    return false;
}

bool PatternParser::recover_generic_type_arg_separator() const
{
    if (this->check(TokenKind::r_bracket)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }

    this->report_here(std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::generic_type_argument)) {
        this->synchronize(RecoveryContext::generic_type_argument);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }
    this->reset_panic();
    return false;
}

bool PatternParser::match_slice_rest_marker() const
{
    if (!this->check(TokenKind::dot) || !this->check_next(TokenKind::dot)) {
        return false;
    }
    this->advance();
    this->advance();
    return true;
}

void PatternParser::parse_pattern_generic_type_args(std::vector<syntax::TypeId>& args) {
    if (this->check(TokenKind::r_bracket)) {
        this->report_here(std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT));
        return;
    }
    while (!this->is_eof() && !this->check(TokenKind::r_bracket)) {
        args.push_back(this->parse_type());
        this->reset_panic();
        if (!this->recover_generic_type_arg_separator()) {
            break;
        }
    }
}

void PatternParser::parse_payload_patterns(syntax::PatternNode& pattern) {
    if (this->check(TokenKind::r_paren)) {
        this->report_here(std::string(PARSER_EXPECT_PAYLOAD_BINDING));
    } else {
        while (!this->is_eof()) {
            const syntax::PatternId payload = this->parse_destructure_pattern_atom();
            pattern.payload_patterns.push_back(payload);
            if (syntax::is_valid(payload) && payload.value < this->session_.module.patterns.size()) {
                const syntax::PatternNode& payload_node = this->session_.module.patterns[payload.value];
                if (payload_node.kind == syntax::PatternKind::binding) {
                    pattern.binding_names.push_back(payload_node.binding_name);
                }
            }
            if (!this->match(TokenKind::comma)) {
                break;
            }
            if (this->check(TokenKind::r_paren)) {
                break;
            }
        }
    }
    const syntax::Token& end = this->expect_payload_pattern_end();
    pattern.range = this->merge(pattern.range, end.range);
}

void PatternParser::consume_bare_enum_case_payload_recovery(
    const syntax::Token& first,
    base::SourceRange& range
) {
    syntax::PatternNode recovery;
    recovery.kind = syntax::PatternKind::enum_case;
    recovery.case_name = first.text;
    recovery.range = first.range;
    this->advance();
    this->parse_payload_patterns(recovery);
    range = this->merge(range, recovery.range);
}

const syntax::Token& PatternParser::expect_generic_type_args_end() const
{
    return this->expect_recovered(
        TokenKind::r_bracket,
        std::string(PARSER_EXPECT_GENERIC_TYPE_ARGS_END),
        RecoveryContext::generic_type_argument
    );
}

const syntax::Token& PatternParser::expect_tuple_pattern_end() const
{
    return this->expect_recovered(
        TokenKind::r_paren,
        std::string(PARSER_EXPECT_TUPLE_PATTERN_END),
        RecoveryContext::pattern_payload
    );
}

const syntax::Token& PatternParser::expect_slice_pattern_end() const
{
    return this->expect_recovered(
        TokenKind::r_bracket,
        std::string(PARSER_EXPECT_SLICE_PATTERN_END),
        RecoveryContext::pattern_payload
    );
}

const syntax::Token& PatternParser::expect_payload_pattern_end() const
{
    return this->expect_recovered(
        TokenKind::r_paren,
        std::string(PARSER_EXPECT_PAYLOAD_BINDING_END),
        RecoveryContext::pattern_payload
    );
}

const syntax::Token& PatternParser::expect_struct_pattern_end() const
{
    return this->expect_recovered(
        TokenKind::r_brace,
        std::string(PARSER_EXPECT_STRUCT_PATTERN_END),
        RecoveryContext::pattern_payload
    );
}

} // namespace aurex::parse
