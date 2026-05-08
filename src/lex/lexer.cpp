#include "aurex/lex/lexer.hpp"

#include "aurex/base/config.hpp"
#include "char_class.hpp"
#include "keyword.hpp"
#include "lexeme.hpp"

#include <utility>

namespace aurex::lex {

Lexer::Lexer(
    const base::SourceId source_id,
    const std::string_view source_text,
    base::DiagnosticSink& diagnostics,
    const LexerOptions options
) noexcept
    : source_id_(source_id),
      source_text_(source_text),
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

    add_token(syntax::TokenKind::eof, source_text_.size(), source_text_.size());
    if (diagnostics_.has_error()) {
        return base::Result<std::vector<syntax::Token>>::fail(
            {base::ErrorCode::lex_error, "lexing failed"}
        );
    }
    return base::Result<std::vector<syntax::Token>>::ok(std::move(tokens_));
}

bool Lexer::is_at_end() const noexcept {
    return offset_ >= source_text_.size();
}

bool Lexer::starts_with(const std::string_view text) const noexcept {
    return source_text_.size() - offset_ >= text.size() &&
           source_text_.substr(offset_, text.size()) == text;
}

char Lexer::peek_at(const base::usize lookahead) const noexcept {
    const base::usize target = offset_ + lookahead;
    if (target >= source_text_.size()) {
        return eof_sentinel;
    }
    return source_text_[target];
}

char Lexer::peek() const noexcept {
    return peek_at(current_character_lookahead);
}

char Lexer::peek_next() const noexcept {
    return peek_at(next_character_lookahead);
}

char Lexer::advance() noexcept {
    if (is_at_end()) {
        return eof_sentinel;
    }
    const char c = source_text_[offset_];
    ++offset_;
    return c;
}

void Lexer::advance_bytes(const base::usize byte_count) noexcept {
    const base::usize remaining = source_text_.size() - offset_;
    offset_ += byte_count < remaining ? byte_count : remaining;
}

bool Lexer::match(const char expected) noexcept {
    if (peek() != expected) {
        return false;
    }
    ++offset_;
    return true;
}

void Lexer::scan_token() {
    const base::usize begin = offset_;

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

    using syntax::TokenKind;
    const char c = advance();
    switch (c) {
    case '"':
        scan_string(begin);
        break;
    case '(':
        add_token(TokenKind::l_paren, begin, offset_);
        break;
    case ')':
        add_token(TokenKind::r_paren, begin, offset_);
        break;
    case '{':
        add_token(TokenKind::l_brace, begin, offset_);
        break;
    case '}':
        add_token(TokenKind::r_brace, begin, offset_);
        break;
    case '[':
        add_token(TokenKind::l_bracket, begin, offset_);
        break;
    case ']':
        add_token(TokenKind::r_bracket, begin, offset_);
        break;
    case ',':
        add_token(TokenKind::comma, begin, offset_);
        break;
    case '.':
        if (starts_with(ellipsis_tail_after_dot)) {
            advance_bytes(ellipsis_tail_after_dot.size());
            add_token(TokenKind::ellipsis, begin, offset_);
        } else {
            add_token(TokenKind::dot, begin, offset_);
        }
        break;
    case ';':
        add_token(TokenKind::semicolon, begin, offset_);
        break;
    case ':':
        add_token(match(':') ? TokenKind::colon_colon : TokenKind::colon, begin, offset_);
        break;
    case '+':
        add_token(TokenKind::plus, begin, offset_);
        break;
    case '-': {
        const TokenKind kind = match('>') ? TokenKind::arrow : TokenKind::minus;
        add_token(kind, begin, offset_);
        break;
    }
    case '*':
        add_token(TokenKind::star, begin, offset_);
        break;
    case '/':
        add_token(TokenKind::slash, begin, offset_);
        break;
    case '%':
        add_token(TokenKind::percent, begin, offset_);
        break;
    case '&':
        add_token(match('&') ? TokenKind::amp_amp : TokenKind::amp, begin, offset_);
        break;
    case '|':
        add_token(match('|') ? TokenKind::pipe_pipe : TokenKind::pipe, begin, offset_);
        break;
    case '^':
        add_token(TokenKind::caret, begin, offset_);
        break;
    case '~':
        add_token(TokenKind::tilde, begin, offset_);
        break;
    case '!':
        add_token(match('=') ? TokenKind::bang_equal : TokenKind::bang, begin, offset_);
        break;
    case '=':
        if (match('=')) {
            add_token(TokenKind::equal_equal, begin, offset_);
        } else if (match('>')) {
            add_token(TokenKind::fat_arrow, begin, offset_);
        } else {
            add_token(TokenKind::equal, begin, offset_);
        }
        break;
    case '<':
        if (match('=')) {
            add_token(TokenKind::less_equal, begin, offset_);
        } else if (match('<')) {
            add_token(TokenKind::less_less, begin, offset_);
        } else {
            add_token(TokenKind::less, begin, offset_);
        }
        break;
    case '>':
        if (match('=')) {
            add_token(TokenKind::greater_equal, begin, offset_);
        } else if (match('>')) {
            add_token(TokenKind::greater_greater, begin, offset_);
        } else {
            add_token(TokenKind::greater, begin, offset_);
        }
        break;
    case '@':
        add_token(TokenKind::at, begin, offset_);
        break;
    case '?':
        add_token(TokenKind::question, begin, offset_);
        break;
    default:
        report(begin, offset_, "invalid character");
        if (options_.emit_invalid_tokens) {
            add_token(TokenKind::invalid, begin, offset_);
        }
        break;
    }
}

void Lexer::scan_identifier() {
    const base::usize begin = offset_;
    while (is_ident_continue(peek())) {
        advance();
    }
    const std::string_view text = source_text_.substr(begin, offset_ - begin);
    add_token(keyword_kind(text), begin, offset_);
}

void Lexer::add_token(const syntax::TokenKind kind, const base::usize begin, const base::usize end) {
    tokens_.push_back(syntax::Token {
        kind,
        base::SourceRange {source_id_, begin, end},
        source_text_.substr(begin, end - begin),
    });
}

void Lexer::report(const base::usize begin, const base::usize end, std::string message) const
{
    diagnostics_.push(base::Diagnostic {
        base::Severity::error,
        base::SourceRange {source_id_, begin, end},
        std::move(message),
    });
}

} // namespace aurex::lex
