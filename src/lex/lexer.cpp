#include <aurex/lex/lexer.hpp>

#include <aurex/base/config.hpp>
#include <lex/char_class.hpp>
#include <lex/keyword.hpp>
#include <lex/lexeme.hpp>
#include <lex/punctuator.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace aurex::lex {

namespace {

constexpr base::usize LEXER_ESTIMATED_BYTES_PER_TOKEN = 2;
constexpr base::usize LEXER_MAX_ESTIMATED_TOKEN_CAPACITY = 262'144;

enum class TokenStartAction : std::uint8_t {
    INVALID,
    IDENTIFIER,
    NUMBER,
    STRING,
    PUNCTUATOR,
    C_STRING_OR_IDENTIFIER,
    BYTE_OR_IDENTIFIER,
};

constexpr void mark_token_start(
    std::array<TokenStartAction, LEX_BYTE_CHAR_CLASS_COUNT>& table,
    const char c,
    const TokenStartAction action
) noexcept {
    table[char_class_index(c)] = action;
}

constexpr void mark_token_start_range(
    std::array<TokenStartAction, LEX_BYTE_CHAR_CLASS_COUNT>& table,
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

[[nodiscard]] consteval std::array<TokenStartAction, LEX_BYTE_CHAR_CLASS_COUNT> build_token_start_actions() noexcept {
    std::array<TokenStartAction, LEX_BYTE_CHAR_CLASS_COUNT> table {};

    mark_token_start_range(table, LEX_ASCII_UPPERCASE_FIRST, LEX_ASCII_UPPERCASE_LAST, TokenStartAction::IDENTIFIER);
    mark_token_start_range(table, LEX_ASCII_LOWERCASE_FIRST, LEX_ASCII_LOWERCASE_LAST, TokenStartAction::IDENTIFIER);
    mark_token_start(table, LEX_IDENTIFIER_JOINER, TokenStartAction::IDENTIFIER);
    mark_token_start_range(table, LEX_ASCII_DIGIT_FIRST, LEX_ASCII_DIGIT_LAST, TokenStartAction::NUMBER);

    mark_token_start(table, LEXEME_C_STRING_PREFIX.front(), TokenStartAction::C_STRING_OR_IDENTIFIER);
    mark_token_start(table, LEXEME_BYTE_LITERAL_PREFIX.front(), TokenStartAction::BYTE_OR_IDENTIFIER);
    mark_token_start(table, LEXEME_DOUBLE_QUOTE, TokenStartAction::STRING);

    mark_token_start(table, LEXEME_DOT, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_COLON, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_MINUS, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_EQUAL, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_BANG, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_LESS, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_GREATER, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_AMP, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_PIPE, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_L_PAREN, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_R_PAREN, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_L_BRACE, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_R_BRACE, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_L_BRACKET, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_R_BRACKET, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_COMMA, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_SEMICOLON, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_PLUS, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_STAR, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_SLASH, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_PERCENT, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_CARET, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_TILDE, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_AT, TokenStartAction::PUNCTUATOR);
    mark_token_start(table, LEXEME_QUESTION, TokenStartAction::PUNCTUATOR);

    return table;
}

inline constexpr std::array TOKEN_START_ACTIONS = build_token_start_actions();

[[nodiscard]] base::usize initial_token_capacity(const base::usize source_size) noexcept {
    const base::usize configured_minimum = base::config::AUREX_INITIAL_TOKEN_CAPACITY;
    const base::usize estimated_capacity = (source_size / LEXER_ESTIMATED_BYTES_PER_TOKEN) + 1;
    return std::min(std::max(configured_minimum, estimated_capacity), LEXER_MAX_ESTIMATED_TOKEN_CAPACITY);
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
            {base::ErrorCode::lex_error, std::string(LEXEME_LEXING_FAILED_MESSAGE)}
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

    switch (TOKEN_START_ACTIONS[char_class_index(first)]) {
    case TokenStartAction::IDENTIFIER:
        this->scan_identifier();
        return;
    case TokenStartAction::NUMBER:
        this->scan_number();
        return;
    case TokenStartAction::STRING:
        this->advance();
        this->scan_string(begin);
        return;
    case TokenStartAction::PUNCTUATOR:
        if (this->scan_punctuator(begin, first)) {
            return;
        }
        break;
    case TokenStartAction::C_STRING_OR_IDENTIFIER:
        if (this->peek_next() == LEXEME_C_STRING_PREFIX.back()) {
            this->scan_c_string(begin);
            return;
        }
        this->scan_identifier();
        return;
    case TokenStartAction::BYTE_OR_IDENTIFIER:
        if (this->peek_next() == LEXEME_BYTE_LITERAL_PREFIX.back()) {
            this->scan_byte(begin);
            return;
        }
        this->scan_identifier();
        return;
    case TokenStartAction::INVALID:
        break;
    }

    this->advance();
    this->report_current(begin, LEXEME_INVALID_CHARACTER_MESSAGE);
    this->finish_invalid_token(begin);
}

bool Lexer::scan_punctuator(const base::usize begin, const char first) {
    const auto match = match_punctuator(
        first,
        this->peek_next(),
        this->cursor_.peek_at(detail::PUNCTUATOR_THIRD_BYTE_OFFSET)
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
