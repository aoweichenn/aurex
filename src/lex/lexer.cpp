#include "aurex/lex/lexer.hpp"

#include "aurex/base/config.hpp"
#include "char_class.hpp"
#include "keyword.hpp"
#include "lexeme.hpp"

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

    using syntax::TokenKind;
    const char c = advance();
    switch (c) {
    case lexeme_double_quote:
        scan_string(begin);
        break;
    case lexeme_l_paren:
        add_token(TokenKind::l_paren, begin, cursor_.offset());
        break;
    case lexeme_r_paren:
        add_token(TokenKind::r_paren, begin, cursor_.offset());
        break;
    case lexeme_l_brace:
        add_token(TokenKind::l_brace, begin, cursor_.offset());
        break;
    case lexeme_r_brace:
        add_token(TokenKind::r_brace, begin, cursor_.offset());
        break;
    case lexeme_l_bracket:
        add_token(TokenKind::l_bracket, begin, cursor_.offset());
        break;
    case lexeme_r_bracket:
        add_token(TokenKind::r_bracket, begin, cursor_.offset());
        break;
    case lexeme_comma:
        add_token(TokenKind::comma, begin, cursor_.offset());
        break;
    case lexeme_dot:
        if (starts_with(ellipsis_tail_after_dot)) {
            advance_bytes(ellipsis_tail_after_dot.size());
            add_token(TokenKind::ellipsis, begin, cursor_.offset());
        } else {
            add_token(TokenKind::dot, begin, cursor_.offset());
        }
        break;
    case lexeme_semicolon:
        add_token(TokenKind::semicolon, begin, cursor_.offset());
        break;
    case lexeme_colon:
        add_token(match(lexeme_colon) ? TokenKind::colon_colon : TokenKind::colon, begin, cursor_.offset());
        break;
    case lexeme_plus:
        add_token(TokenKind::plus, begin, cursor_.offset());
        break;
    case lexeme_minus: {
        const TokenKind kind = match(lexeme_greater) ? TokenKind::arrow : TokenKind::minus;
        add_token(kind, begin, cursor_.offset());
        break;
    }
    case lexeme_star:
        add_token(TokenKind::star, begin, cursor_.offset());
        break;
    case lexeme_slash:
        add_token(TokenKind::slash, begin, cursor_.offset());
        break;
    case lexeme_percent:
        add_token(TokenKind::percent, begin, cursor_.offset());
        break;
    case lexeme_amp:
        add_token(match(lexeme_amp) ? TokenKind::amp_amp : TokenKind::amp, begin, cursor_.offset());
        break;
    case lexeme_pipe:
        add_token(match(lexeme_pipe) ? TokenKind::pipe_pipe : TokenKind::pipe, begin, cursor_.offset());
        break;
    case lexeme_caret:
        add_token(TokenKind::caret, begin, cursor_.offset());
        break;
    case lexeme_tilde:
        add_token(TokenKind::tilde, begin, cursor_.offset());
        break;
    case lexeme_bang:
        add_token(match(lexeme_equal) ? TokenKind::bang_equal : TokenKind::bang, begin, cursor_.offset());
        break;
    case lexeme_equal:
        if (match(lexeme_equal)) {
            add_token(TokenKind::equal_equal, begin, cursor_.offset());
        } else if (match(lexeme_greater)) {
            add_token(TokenKind::fat_arrow, begin, cursor_.offset());
        } else {
            add_token(TokenKind::equal, begin, cursor_.offset());
        }
        break;
    case lexeme_less:
        if (match(lexeme_equal)) {
            add_token(TokenKind::less_equal, begin, cursor_.offset());
        } else if (match(lexeme_less)) {
            add_token(TokenKind::less_less, begin, cursor_.offset());
        } else {
            add_token(TokenKind::less, begin, cursor_.offset());
        }
        break;
    case lexeme_greater:
        if (match(lexeme_equal)) {
            add_token(TokenKind::greater_equal, begin, cursor_.offset());
        } else if (match(lexeme_greater)) {
            add_token(TokenKind::greater_greater, begin, cursor_.offset());
        } else {
            add_token(TokenKind::greater, begin, cursor_.offset());
        }
        break;
    case lexeme_at:
        add_token(TokenKind::at, begin, cursor_.offset());
        break;
    case lexeme_question:
        add_token(TokenKind::question, begin, cursor_.offset());
        break;
    default:
        report(begin, cursor_.offset(), invalid_character_message);
        if (options_.emit_invalid_tokens) {
            add_token(TokenKind::invalid, begin, cursor_.offset());
        }
        break;
    }
}

void Lexer::scan_identifier() {
    const base::usize begin = cursor_.offset();
    while (is_ident_continue(peek())) {
        advance();
    }
    const std::string_view text = cursor_.slice(begin, cursor_.offset());
    add_token(keyword_kind(text), begin, cursor_.offset());
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
