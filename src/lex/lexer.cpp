#include "aurex/lex/lexer.hpp"

#include "aurex/base/config.hpp"
#include "char_class.hpp"
#include "keyword.hpp"
#include "lexeme.hpp"
#include "punctuator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace aurex::lex {

namespace {

constexpr base::usize estimated_bytes_per_token = 2;
constexpr base::usize max_estimated_token_capacity = 262'144;

enum class TokenStartAction : std::uint8_t {
    invalid,
    identifier,
    number,
    string,
    punctuator,
    c_string_or_identifier,
    byte_or_identifier,
};

constexpr void mark_token_start(
    std::array<TokenStartAction, byte_char_class_count>& table,
    const char c,
    const TokenStartAction action
) noexcept {
    table[char_class_index(c)] = action;
}

constexpr void mark_token_start_range(
    std::array<TokenStartAction, byte_char_class_count>& table,
    const char first,
    const char last,
    const TokenStartAction action
) noexcept {
    const std::size_t first_index = char_class_index(first);
    const std::size_t last_index = char_class_index(last);
    for (std::size_t index = first_index; index <= last_index; ++index) {
        table[index] = action;
    }
}

[[nodiscard]] consteval std::array<TokenStartAction, byte_char_class_count> build_token_start_actions() noexcept {
    std::array<TokenStartAction, byte_char_class_count> table {};

    mark_token_start_range(table, ascii_uppercase_first, ascii_uppercase_last, TokenStartAction::identifier);
    mark_token_start_range(table, ascii_lowercase_first, ascii_lowercase_last, TokenStartAction::identifier);
    mark_token_start(table, identifier_joiner, TokenStartAction::identifier);
    mark_token_start_range(table, ascii_digit_first, ascii_digit_last, TokenStartAction::number);

    mark_token_start(table, c_string_prefix.front(), TokenStartAction::c_string_or_identifier);
    mark_token_start(table, byte_literal_prefix.front(), TokenStartAction::byte_or_identifier);
    mark_token_start(table, lexeme_double_quote, TokenStartAction::string);

    mark_token_start(table, lexeme_dot, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_colon, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_minus, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_equal, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_bang, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_less, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_greater, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_amp, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_pipe, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_l_paren, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_r_paren, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_l_brace, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_r_brace, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_l_bracket, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_r_bracket, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_comma, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_semicolon, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_plus, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_star, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_slash, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_percent, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_caret, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_tilde, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_at, TokenStartAction::punctuator);
    mark_token_start(table, lexeme_question, TokenStartAction::punctuator);

    return table;
}

inline constexpr std::array token_start_actions = build_token_start_actions();

[[nodiscard]] base::usize initial_token_capacity(const base::usize source_size) noexcept {
    const base::usize configured_minimum = base::config::initial_token_capacity;
    const base::usize estimated_capacity = (source_size / estimated_bytes_per_token) + 1;
    return std::min(std::max(configured_minimum, estimated_capacity), max_estimated_token_capacity);
}

} // namespace

Lexer::Lexer(
    const base::SourceId source_id,
    const std::string_view source_text,
    base::DiagnosticSink& diagnostics,
    const LexerOptions options
) noexcept
    : source_id_(source_id),
      cursor_(source_text),
      diagnostics_(diagnostics),
      options_(options) {
    this->tokens_.reserve(initial_token_capacity(source_text.size()));
}

base::Result<std::vector<syntax::Token>> Lexer::tokenize() {
    while (!this->is_at_end()) {
        this->skip_trivia();
        if (!this->is_at_end()) {
            this->scan_token();
        }
    }

    this->add_token(syntax::TokenKind::eof, this->cursor_.source_size(), this->cursor_.source_size());
    if (this->diagnostics_.has_error()) {
        return base::Result<std::vector<syntax::Token>>::fail(
            {base::ErrorCode::lex_error, std::string(lexing_failed_message)}
        );
    }
    return base::Result<std::vector<syntax::Token>>::ok(std::move(this->tokens_));
}

bool Lexer::is_at_end() const noexcept {
    return this->cursor_.is_at_end();
}

bool Lexer::starts_with(const std::string_view text) const noexcept {
    return this->cursor_.starts_with(text);
}

char Lexer::peek() const noexcept {
    return this->cursor_.peek();
}

char Lexer::peek_next() const noexcept {
    return this->cursor_.peek_next();
}

char Lexer::advance() noexcept {
    return this->cursor_.advance();
}

void Lexer::advance_bytes(const base::usize byte_count) noexcept {
    this->cursor_.advance_bytes(byte_count);
}

bool Lexer::match(const char expected) noexcept {
    return this->cursor_.match(expected);
}

void Lexer::scan_token() {
    const base::usize begin = this->cursor_.offset();
    const char first = this->peek();

    switch (token_start_actions[char_class_index(first)]) {
    case TokenStartAction::identifier:
        this->scan_identifier();
        return;
    case TokenStartAction::number:
        this->scan_number();
        return;
    case TokenStartAction::string:
        this->advance();
        this->scan_string(begin);
        return;
    case TokenStartAction::punctuator:
        if (this->scan_punctuator(begin, first)) {
            return;
        }
        break;
    case TokenStartAction::c_string_or_identifier:
        if (this->peek_next() == c_string_prefix.back()) {
            this->scan_c_string(begin);
            return;
        }
        this->scan_identifier();
        return;
    case TokenStartAction::byte_or_identifier:
        if (this->peek_next() == byte_literal_prefix.back()) {
            this->scan_byte(begin);
            return;
        }
        this->scan_identifier();
        return;
    case TokenStartAction::invalid:
        break;
    }

    this->advance();
    this->report_current(begin, invalid_character_message);
    this->finish_invalid_token(begin);
}

bool Lexer::scan_punctuator(const base::usize begin, const char first) {
    const auto match = match_punctuator(
        first,
        this->peek_next(),
        this->cursor_.peek_at(detail::punctuator_third_byte_offset)
    );
    if (!match.matched()) {
        return false;
    }
    const base::usize end = begin + match.width;
    this->advance_bytes(match.width);
    this->add_nonempty_token(match.kind, begin, end);
    return true;
}

void Lexer::scan_identifier() {
    const base::usize begin = this->cursor_.offset();
    const std::string_view remaining = this->cursor_.remaining_text();
    base::usize width = 0;
    while (width < remaining.size() && is_ident_continue(remaining[width])) {
        ++width;
    }
    this->advance_bytes(width);
    const std::string_view text {remaining.data(), width};
    this->finish_token(keyword_kind(text), begin, text);
}

base::SourceRange Lexer::range(const base::usize begin, const base::usize end) const noexcept {
    return base::SourceRange {this->source_id_, begin, end};
}

base::SourceRange Lexer::current_range(const base::usize begin) const noexcept {
    return this->range(begin, this->cursor_.offset());
}

void Lexer::finish_token(const syntax::TokenKind kind, const base::usize begin) {
    this->tokens_.push_back(syntax::Token {
        kind,
        this->current_range(begin),
        this->cursor_.current_slice(begin),
    });
}

void Lexer::finish_token(const syntax::TokenKind kind, const base::usize begin, const std::string_view text) {
    this->tokens_.push_back(syntax::Token {
        kind,
        base::SourceRange {this->source_id_, begin, begin + text.size()},
        text,
    });
}

void Lexer::finish_invalid_token(const base::usize begin) {
    if (this->options_.emit_invalid_tokens) {
        this->finish_token(syntax::TokenKind::invalid, begin);
    }
}

void Lexer::add_nonempty_token(
    const syntax::TokenKind kind,
    const base::usize begin,
    const base::usize end
) {
    this->tokens_.push_back(syntax::Token {
        kind,
        base::SourceRange {this->source_id_, begin, end},
        this->cursor_.nonempty_slice(begin, end),
    });
}

void Lexer::add_token(const syntax::TokenKind kind, const base::usize begin, const base::usize end) {
    this->tokens_.push_back(syntax::Token {
        kind,
        this->range(begin, end),
        this->cursor_.slice(begin, end),
    });
}

void Lexer::report_current(const base::usize begin, const std::string_view message) const {
    this->diagnostics_.push(base::Diagnostic {
        base::Severity::error,
        this->current_range(begin),
        std::string(message),
    });
}

void Lexer::report(const base::usize begin, const base::usize end, const std::string_view message) const
{
    this->diagnostics_.push(base::Diagnostic {
        base::Severity::error,
        this->range(begin, end),
        std::string(message),
    });
}

} // namespace aurex::lex
