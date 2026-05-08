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
    tokens_.reserve(base::config::initial_token_capacity);
}

base::Result<std::vector<syntax::Token>> Lexer::tokenize() {
    while (!is_at_end()) {
        skip_trivia();
        if (!is_at_end()) {
            scan_token();
        }
    }

    add_token(syntax::TokenKind::eof, cursor_.source_size(), cursor_.source_size());
    if (diagnostics_.has_error()) {
        return base::Result<std::vector<syntax::Token>>::fail(
            {base::ErrorCode::lex_error, std::string(lexing_failed_message)}
        );
    }
    return base::Result<std::vector<syntax::Token>>::ok(std::move(tokens_));
}

bool Lexer::is_at_end() const noexcept {
    return cursor_.is_at_end();
}

bool Lexer::starts_with(const std::string_view text) const noexcept {
    return cursor_.starts_with(text);
}

char Lexer::peek() const noexcept {
    return cursor_.peek();
}

char Lexer::peek_next() const noexcept {
    return cursor_.peek_next();
}

char Lexer::advance() noexcept {
    return cursor_.advance();
}

void Lexer::advance_bytes(const base::usize byte_count) noexcept {
    cursor_.advance_bytes(byte_count);
}

bool Lexer::match(const char expected) noexcept {
    return cursor_.match(expected);
}

void Lexer::scan_token() {
    const base::usize begin = cursor_.offset();

    if (starts_with(c_string_prefix)) {
        scan_c_string(begin);
        return;
    }
    if (starts_with(byte_literal_prefix)) {
        scan_byte(begin);
        return;
    }
    if (is_ident_start(peek())) {
        scan_identifier();
        return;
    }
    if (is_decimal_digit(peek())) {
        scan_number();
        return;
    }

    if (peek() == lexeme_double_quote) {
        advance();
        scan_string(begin);
        return;
    }

    if (scan_punctuator(begin)) {
        return;
    }

    advance();
    report(begin, cursor_.offset(), invalid_character_message);
    if (options_.emit_invalid_tokens) {
        finish_token(syntax::TokenKind::invalid, begin);
    }
}

bool Lexer::scan_punctuator(const base::usize begin) {
    const auto match = match_punctuator(cursor_.remaining_text());
    if (!match.has_value()) {
        return false;
    }
    advance_bytes(match->text.size());
    finish_token(match->kind, begin);
    return true;
}

void Lexer::scan_identifier() {
    const base::usize begin = cursor_.offset();
    while (is_ident_continue(peek())) {
        advance();
    }
    const std::string_view text = cursor_.slice(begin, cursor_.offset());
    finish_token(keyword_kind(text), begin);
}

void Lexer::finish_token(const syntax::TokenKind kind, const base::usize begin) {
    add_token(kind, begin, cursor_.offset());
}

void Lexer::add_token(const syntax::TokenKind kind, const base::usize begin, const base::usize end) {
    tokens_.push_back(syntax::Token {
        kind,
        base::SourceRange {source_id_, begin, end},
        cursor_.slice(begin, end),
    });
}

void Lexer::report(const base::usize begin, const base::usize end, const std::string_view message) const
{
    diagnostics_.push(base::Diagnostic {
        base::Severity::error,
        base::SourceRange {source_id_, begin, end},
        std::string(message),
    });
}

} // namespace aurex::lex
