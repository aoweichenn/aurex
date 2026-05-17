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

constexpr base::usize LEXER_TOKEN_ESTIMATE_EOF_TOKEN = 1;
constexpr base::usize LEXER_TOKEN_PREFIX_TAIL_OFFSET = LEXEME_SINGLE_BYTE_WIDTH;
constexpr base::usize LEXER_MAX_ERROR_DIAGNOSTICS = 128;
constexpr std::string_view LEXER_ERROR_BUDGET_EXHAUSTED_MESSAGE =
    "too many lexical errors; suppressing further lexer diagnostics";

enum class TokenStartAction : std::uint8_t {
    INVALID,
    IDENTIFIER,
    NUMBER,
    STRING,
    PUNCTUATOR,
    C_STRING_OR_IDENTIFIER,
    BYTE_OR_IDENTIFIER,
    RAW_STRING_OR_IDENTIFIER,
    CHAR,
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
    mark_token_start(table, LEXEME_RAW_STRING_PREFIX.front(), TokenStartAction::RAW_STRING_OR_IDENTIFIER);
    mark_token_start(table, LEXEME_DOUBLE_QUOTE, TokenStartAction::STRING);
    mark_token_start(table, LEXEME_SINGLE_QUOTE, TokenStartAction::CHAR);

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

[[nodiscard]] bool estimate_two_byte_prefix(
    const std::string_view source,
    const base::usize index,
    const std::string_view prefix
) noexcept {
    return index + prefix.size() <= source.size() &&
           source.substr(index, prefix.size()) == prefix;
}

[[nodiscard]] base::usize estimate_quoted_token_end(
    const std::string_view source,
    base::usize index,
    const char quote
) noexcept {
    while (index < source.size()) {
        const char current = source[index];
        ++index;
        if (current == LEXEME_ESCAPE && index < source.size()) {
            ++index;
            continue;
        }
        if (current == quote) {
            break;
        }
    }
    return index;
}

[[nodiscard]] base::usize estimate_line_comment_end(
    const std::string_view source,
    base::usize index
) noexcept {
    while (index < source.size() && source[index] != LEXEME_LINE_FEED) {
        ++index;
    }
    return index;
}

[[nodiscard]] base::usize estimate_block_comment_end(
    const std::string_view source,
    base::usize index
) noexcept {
    while (index + LEXEME_BLOCK_COMMENT_SUFFIX.size() <= source.size()) {
        if (source.substr(index, LEXEME_BLOCK_COMMENT_SUFFIX.size()) == LEXEME_BLOCK_COMMENT_SUFFIX) {
            return index + LEXEME_BLOCK_COMMENT_SUFFIX.size();
        }
        ++index;
    }
    return source.size();
}

[[nodiscard]] base::usize estimate_identifier_token_end(
    const std::string_view source,
    base::usize index
) noexcept {
    while (index < source.size() && is_ident_continue(source[index])) {
        ++index;
    }
    return index;
}

[[nodiscard]] base::usize estimate_number_token_end(
    const std::string_view source,
    base::usize index
) noexcept {
    while (index < source.size() && is_ident_continue(source[index])) {
        ++index;
    }
    return index;
}

[[nodiscard]] bool estimate_prefixed_quote(
    const std::string_view source,
    const base::usize index,
    const std::string_view prefix
) noexcept {
    return source[index] == prefix.front() &&
           index + LEXER_TOKEN_PREFIX_TAIL_OFFSET < source.size() &&
           source[index + LEXER_TOKEN_PREFIX_TAIL_OFFSET] == prefix.back();
}

[[nodiscard]] base::usize estimate_token_capacity(const std::string_view source) noexcept {
    constexpr base::usize configured_minimum = base::config::AUREX_INITIAL_TOKEN_CAPACITY;
    base::usize tokens = LEXER_TOKEN_ESTIMATE_EOF_TOKEN;
    base::usize index = 0;
    while (index < source.size()) {
        const char current = source[index];
        if (is_trivia_space(current)) {
            ++index;
            continue;
        }
        if (estimate_two_byte_prefix(source, index, LEXEME_LINE_COMMENT_PREFIX)) {
            index = estimate_line_comment_end(source, index + LEXEME_LINE_COMMENT_PREFIX.size());
            continue;
        }
        if (estimate_two_byte_prefix(source, index, LEXEME_BLOCK_COMMENT_PREFIX)) {
            index = estimate_block_comment_end(source, index + LEXEME_BLOCK_COMMENT_PREFIX.size());
            continue;
        }
        ++tokens;
        if (current == LEXEME_DOUBLE_QUOTE || current == LEXEME_SINGLE_QUOTE) {
            index = estimate_quoted_token_end(source, index + 1U, current);
            continue;
        }
        if (estimate_prefixed_quote(source, index, LEXEME_C_STRING_PREFIX) ||
            estimate_prefixed_quote(source, index, LEXEME_RAW_STRING_PREFIX) ||
            estimate_prefixed_quote(source, index, LEXEME_BYTE_STRING_PREFIX)) {
            index = estimate_quoted_token_end(source, index + LEXEME_C_STRING_PREFIX.size(), LEXEME_DOUBLE_QUOTE);
            continue;
        }
        if (estimate_prefixed_quote(source, index, LEXEME_BYTE_LITERAL_PREFIX)) {
            index = estimate_quoted_token_end(source, index + LEXEME_BYTE_LITERAL_PREFIX.size(), LEXEME_SINGLE_QUOTE);
            continue;
        }
        if (is_ident_start(current)) {
            index = estimate_identifier_token_end(source, index + LEXEME_SINGLE_BYTE_WIDTH);
            continue;
        }
        if (is_decimal_digit(current) ||
            (current == LEXEME_DOT &&
             index + LEXEME_SINGLE_BYTE_WIDTH < source.size() &&
             is_decimal_digit(source[index + LEXEME_SINGLE_BYTE_WIDTH]))) {
            index = estimate_number_token_end(source, index + LEXEME_SINGLE_BYTE_WIDTH);
            continue;
        }
        ++index;
    }
    return std::max(configured_minimum, tokens);
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
    this->tokens_.reserve(estimate_token_capacity(source_text));
}

base::Result<TokenBuffer> Lexer::tokenize() {
    while (!this->is_at_end()) {
        this->skip_trivia();
        if (!this->is_at_end()) {
            this->scan_token();
        }
    }

    this->add_token(syntax::TokenKind::eof, this->cursor_.source_size(), this->cursor_.source_size());
    if (this->diagnostics_.has_error()) {
        return base::Result<TokenBuffer>::fail(
            {base::ErrorCode::lex_error, std::string(LEXEME_LEXING_FAILED_MESSAGE)}
        );
    }
    return base::Result<TokenBuffer>::ok(std::move(this->tokens_));
}

bool Lexer::is_at_end() const noexcept {
    return this->cursor_.is_at_end();
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
        if (first == LEXEME_DOT && is_decimal_digit(this->peek_next())) {
            this->scan_leading_dot_float(begin);
            return;
        }
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
        if (this->peek_next() == LEXEME_BYTE_STRING_PREFIX.back()) {
            this->scan_byte_string(begin);
            return;
        }
        this->scan_identifier();
        return;
    case TokenStartAction::RAW_STRING_OR_IDENTIFIER:
        if (this->peek_next() == LEXEME_RAW_STRING_PREFIX.back()) {
            this->scan_raw_string(begin);
            return;
        }
        this->scan_identifier();
        return;
    case TokenStartAction::CHAR:
        this->advance();
        this->scan_char(begin);
        return;
    case TokenStartAction::INVALID:
        break;
    }

    this->advance();
    this->scan_invalid_run(begin);
}

void Lexer::scan_invalid_run(const base::usize begin) {
    while (!this->is_at_end() &&
           TOKEN_START_ACTIONS[char_class_index(this->peek())] == TokenStartAction::INVALID) {
        this->advance();
    }
    this->report(begin, this->cursor_.offset(), LEXEME_INVALID_CHARACTER_MESSAGE);
    this->finish_invalid_token(begin, this->cursor_.offset());
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

void Lexer::finish_invalid_token(const base::usize begin, const base::usize end) {
    if (this->options_.emit_invalid_tokens) {
        this->add_nonempty_token(syntax::TokenKind::invalid, begin, end);
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

void Lexer::report_current(const base::usize begin, const std::string_view message) {
    this->report(begin, this->cursor_.offset(), message);
}

void Lexer::report(const base::usize begin, const base::usize end, const std::string_view message)
{
    if (this->lexical_error_count_ >= LEXER_MAX_ERROR_DIAGNOSTICS) {
        if (!this->lexical_error_budget_reported_) {
            this->diagnostics_.push(base::Diagnostic {
                base::Severity::error,
                this->range(begin, end),
                std::string(LEXER_ERROR_BUDGET_EXHAUSTED_MESSAGE),
                base::DiagnosticCategory::lexer,
                base::DiagnosticCode::lexer_error_budget,
            });
            this->lexical_error_budget_reported_ = true;
        }
        return;
    }
    ++this->lexical_error_count_;
    this->diagnostics_.push(base::Diagnostic {
        base::Severity::error,
        this->range(begin, end),
        std::string(message),
        base::DiagnosticCategory::lexer,
        base::DiagnosticCode::lexer_invalid_token,
    });
}

} // namespace aurex::lex
