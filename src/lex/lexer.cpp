#include "aurex/lex/lexer.hpp"

#include "aurex/base/config.hpp"
#include "char_class.hpp"
#include "keyword.hpp"
#include "lexeme.hpp"
#include "punctuator.hpp"

#include <string>
#include <utility>

namespace aurex::lex {

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
    this->tokens_.reserve(base::config::initial_token_capacity);
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

    if (first == c_string_prefix.front() && this->starts_with(c_string_prefix)) {
        this->scan_c_string(begin);
        return;
    }
    if (first == byte_literal_prefix.front() && this->starts_with(byte_literal_prefix)) {
        this->scan_byte(begin);
        return;
    }
    if (is_ident_start(first)) {
        this->scan_identifier();
        return;
    }
    if (is_decimal_digit(first)) {
        this->scan_number();
        return;
    }

    if (first == lexeme_double_quote) {
        this->advance();
        this->scan_string(begin);
        return;
    }

    if (this->scan_punctuator(begin)) {
        return;
    }

    this->advance();
    this->report_current(begin, invalid_character_message);
    this->finish_invalid_token(begin);
}

bool Lexer::scan_punctuator(const base::usize begin) {
    const auto match = match_punctuator(this->cursor_.remaining_text());
    if (!match.has_value()) {
        return false;
    }
    this->advance_bytes(match->width);
    this->finish_token(match->kind, begin);
    return true;
}

void Lexer::scan_identifier() {
    const base::usize begin = this->cursor_.offset();
    while (is_ident_continue(this->peek())) {
        this->advance();
    }
    const std::string_view text = this->cursor_.slice(begin, this->cursor_.offset());
    this->finish_token(keyword_kind(text), begin);
}

base::SourceRange Lexer::range(const base::usize begin, const base::usize end) const noexcept {
    return base::SourceRange {this->source_id_, begin, end};
}

base::SourceRange Lexer::current_range(const base::usize begin) const noexcept {
    return this->range(begin, this->cursor_.offset());
}

void Lexer::finish_token(const syntax::TokenKind kind, const base::usize begin) {
    this->add_token(kind, begin, this->cursor_.offset());
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
