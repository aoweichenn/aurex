#include "aurex/lex/lexer.hpp"

#include "aurex/base/config.hpp"
#include "char_class.hpp"
#include "keyword.hpp"
#include "lexeme.hpp"
#include "punctuator.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace aurex::lex {

namespace {

constexpr base::usize estimated_bytes_per_token = 2;
constexpr base::usize max_estimated_token_capacity = 262'144;

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
    const std::uint8_t first_classes = char_classes(first);

    if (first == c_string_prefix.front() && this->peek_next() == c_string_prefix.back()) {
        this->scan_c_string(begin);
        return;
    }
    if (first == byte_literal_prefix.front() && this->peek_next() == byte_literal_prefix.back()) {
        this->scan_byte(begin);
        return;
    }
    if (has_char_class_flags(first_classes, char_class_identifier_start)) {
        this->scan_identifier();
        return;
    }
    if (has_char_class_flags(first_classes, char_class_decimal_digit)) {
        this->scan_number();
        return;
    }

    if (first == lexeme_double_quote) {
        this->advance();
        this->scan_string(begin);
        return;
    }

    if (this->scan_punctuator(begin, first)) {
        return;
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
    this->advance_bytes(match.width);
    this->finish_token(match.kind, begin);
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
        this->current_range(begin),
        text,
    });
}

void Lexer::finish_invalid_token(const base::usize begin) {
    if (this->options_.emit_invalid_tokens) {
        this->finish_token(syntax::TokenKind::invalid, begin);
    }
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
