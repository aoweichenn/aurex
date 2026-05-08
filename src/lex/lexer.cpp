#include "aurex/lex/lexer.hpp"

#include "aurex/base/config.hpp"
#include "aurex/base/string_literal.hpp"

#include <array>
#include <string>

namespace aurex::lex {

namespace {

[[nodiscard]] bool is_ident_start(const char c) noexcept {
    const auto value = static_cast<unsigned char>(c);
    return std::isalpha(value) != 0 || c == '_';
}

[[nodiscard]] bool is_ident_continue(const char c) noexcept {
    const auto value = static_cast<unsigned char>(c);
    return std::isalnum(value) != 0 || c == '_';
}

[[nodiscard]] bool is_decimal_digit(const char c) noexcept {
    return c >= '0' && c <= '9';
}

[[nodiscard]] bool is_hex_digit(const char c) noexcept {
    const auto value = static_cast<unsigned char>(c);
    return std::isxdigit(value) != 0;
}

[[nodiscard]] bool is_binary_digit(const char c) noexcept {
    return c == '0' || c == '1';
}

[[nodiscard]] syntax::TokenKind keyword_kind(const std::string_view text) noexcept {
    using syntax::TokenKind;
    struct Entry {
        std::string_view text;
        TokenKind kind;
    };
    static constexpr auto entries = std::to_array<Entry>({
        Entry {"module", TokenKind::kw_module},
        Entry {"import", TokenKind::kw_import},
        Entry {"as", TokenKind::kw_as},
        Entry {"pub", TokenKind::kw_pub},
        Entry {"priv", TokenKind::kw_priv},
        Entry {"extern", TokenKind::kw_extern},
        Entry {"export", TokenKind::kw_export},
        Entry {"c", TokenKind::kw_c},
        Entry {"fn", TokenKind::kw_fn},
        Entry {"struct", TokenKind::kw_struct},
        Entry {"opaque", TokenKind::kw_opaque},
        Entry {"enum", TokenKind::kw_enum},
        Entry {"const", TokenKind::kw_const},
        Entry {"type", TokenKind::kw_type},
        Entry {"impl", TokenKind::kw_impl},
        Entry {"match", TokenKind::kw_match},
        Entry {"let", TokenKind::kw_let},
        Entry {"var", TokenKind::kw_var},
        Entry {"if", TokenKind::kw_if},
        Entry {"else", TokenKind::kw_else},
        Entry {"for", TokenKind::kw_for},
        Entry {"while", TokenKind::kw_while},
        Entry {"break", TokenKind::kw_break},
        Entry {"continue", TokenKind::kw_continue},
        Entry {"defer", TokenKind::kw_defer},
        Entry {"return", TokenKind::kw_return},
        Entry {"noncopy", TokenKind::kw_noncopy},
        Entry {"move", TokenKind::kw_move},
        Entry {"true", TokenKind::kw_true},
        Entry {"false", TokenKind::kw_false},
        Entry {"null", TokenKind::kw_null},
        Entry {"void", TokenKind::kw_void},
        Entry {"bool", TokenKind::kw_bool},
        Entry {"i8", TokenKind::kw_i8},
        Entry {"u8", TokenKind::kw_u8},
        Entry {"i16", TokenKind::kw_i16},
        Entry {"u16", TokenKind::kw_u16},
        Entry {"i32", TokenKind::kw_i32},
        Entry {"u32", TokenKind::kw_u32},
        Entry {"i64", TokenKind::kw_i64},
        Entry {"u64", TokenKind::kw_u64},
        Entry {"isize", TokenKind::kw_isize},
        Entry {"usize", TokenKind::kw_usize},
        Entry {"f32", TokenKind::kw_f32},
        Entry {"f64", TokenKind::kw_f64},
        Entry {"str", TokenKind::kw_str},
        Entry {"mut", TokenKind::kw_mut},
        Entry {"cast", TokenKind::kw_cast},
        Entry {"ptr_cast", TokenKind::kw_ptr_cast},
        Entry {"bit_cast", TokenKind::kw_bit_cast},
        Entry {"size_of", TokenKind::kw_size_of},
        Entry {"align_of", TokenKind::kw_align_of},
        Entry {"ptr_addr", TokenKind::kw_ptr_addr},
        Entry {"ptr_from_addr", TokenKind::kw_ptr_from_addr},
        Entry {"str_data", TokenKind::kw_str_data},
        Entry {"str_byte_len", TokenKind::kw_str_byte_len},
        Entry {"str_from_bytes_unchecked", TokenKind::kw_str_from_bytes_unchecked},
    });

    for (const Entry& entry : entries) {
        if (entry.text == text) {
            return entry.kind;
        }
    }
    return TokenKind::identifier;
}

} // namespace

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

char Lexer::peek() const noexcept {
    if (is_at_end()) {
        return '\0';
    }
    return source_text_[offset_];
}

char Lexer::peek_next() const noexcept {
    const base::usize next = offset_ + 1;
    if (next >= source_text_.size()) {
        return '\0';
    }
    return source_text_[next];
}

char Lexer::advance() noexcept {
    if (is_at_end()) {
        return '\0';
    }
    const char c = source_text_[offset_];
    ++offset_;
    return c;
}

bool Lexer::match(const char expected) noexcept {
    if (peek() != expected) {
        return false;
    }
    ++offset_;
    return true;
}

void Lexer::skip_trivia() {
    bool consumed = true;
    while (consumed && !is_at_end()) {
        consumed = false;
        while (!is_at_end()) {
            const char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
                consumed = true;
                continue;
            }
            break;
        }

        if (peek() == '/' && peek_next() == '/') {
            scan_line_comment();
            consumed = true;
        } else if (peek() == '/' && peek_next() == '*') {
            scan_block_comment();
            consumed = true;
        }
    }
}

void Lexer::scan_token() {
    const base::usize begin = offset_;

    if (peek() == 'c' && peek_next() == '"') {
        scan_c_string();
        return;
    }
    if (peek() == 'b' && peek_next() == '\'') {
        scan_byte();
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
        scan_string();
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
        if (peek() == '.' && peek_next() == '.') {
            advance();
            advance();
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

void Lexer::scan_number() {
    const base::usize begin = offset_;
    bool is_float = false;

    const auto scan_digits = [this](const auto digit_predicate, const std::string_view literal_kind) {
        bool saw_digit = false;
        bool previous_was_digit = false;
        bool previous_was_separator = false;
        while (digit_predicate(peek()) || peek() == '_') {
            const base::usize separator_begin = offset_;
            const char c = advance();
            if (c == '_') {
                if (!previous_was_digit) {
                    report(separator_begin, separator_begin + 1, "digit separator must be between digits");
                }
                previous_was_digit = false;
                previous_was_separator = true;
                continue;
            }
            saw_digit = true;
            previous_was_digit = true;
            previous_was_separator = false;
        }
        if (previous_was_separator && offset_ > 0) {
            report(offset_ - 1, offset_, "digit separator must be between digits");
        }
        if (!saw_digit) {
            report(offset_, offset_, std::string(literal_kind) + " literal has no digits");
        }
        return saw_digit;
    };

    if (peek() == '0' && (peek_next() == 'x' || peek_next() == 'X')) {
        advance();
        advance();
        static_cast<void>(scan_digits(is_hex_digit, "integer"));
    } else if (peek() == '0' && (peek_next() == 'b' || peek_next() == 'B')) {
        advance();
        advance();
        static_cast<void>(scan_digits(is_binary_digit, "integer"));
    } else {
        static_cast<void>(scan_digits(is_decimal_digit, "integer"));
        if (peek() == '.' && is_decimal_digit(peek_next())) {
            is_float = true;
            advance();
            static_cast<void>(scan_digits(is_decimal_digit, "float"));
        }
        if (peek() == 'e' || peek() == 'E') {
            const base::usize next = offset_ + 1;
            const char next_char = next < source_text_.size() ? source_text_[next] : '\0';
            if (is_decimal_digit(next_char) ||
                next_char == '+' ||
                next_char == '-') {
                is_float = true;
                advance();
                if (peek() == '+' || peek() == '-') {
                    advance();
                }
                static_cast<void>(scan_digits(is_decimal_digit, "float exponent"));
            }
        }
    }

    add_token(is_float ? syntax::TokenKind::float_literal : syntax::TokenKind::integer_literal, begin, offset_);
}

void Lexer::scan_string() {
    const base::usize begin = offset_ - 1;
    bool escaped = false;
    while (!is_at_end()) {
        const char c = advance();
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            const base::StringLiteralDecode decoded = base::decode_string_literal(
                source_text_.substr(begin, offset_ - begin),
                base::StringLiteralKind::string
            );
            for (const base::StringLiteralError& error : decoded.errors) {
                report(begin + error.begin, begin + error.end, error.message);
            }
            if (decoded.ok()) {
                add_token(syntax::TokenKind::string_literal, begin, offset_);
            } else if (options_.emit_invalid_tokens) {
                add_token(syntax::TokenKind::invalid, begin, offset_);
            }
            return;
        }
        if (c == '\n') {
            report(begin, offset_, "unterminated string literal");
            add_token(syntax::TokenKind::invalid, begin, offset_);
            return;
        }
    }
    report(begin, offset_, "unterminated string literal");
    add_token(syntax::TokenKind::invalid, begin, offset_);
}

void Lexer::scan_c_string() {
    const base::usize begin = offset_;
    advance();
    advance();
    bool escaped = false;
    while (!is_at_end()) {
        const char c = advance();
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            const base::StringLiteralDecode decoded = base::decode_string_literal(
                source_text_.substr(begin, offset_ - begin),
                base::StringLiteralKind::c_string
            );
            for (const base::StringLiteralError& error : decoded.errors) {
                report(begin + error.begin, begin + error.end, error.message);
            }
            if (decoded.ok()) {
                add_token(syntax::TokenKind::c_string_literal, begin, offset_);
            } else if (options_.emit_invalid_tokens) {
                add_token(syntax::TokenKind::invalid, begin, offset_);
            }
            return;
        }
        if (c == '\n') {
            report(begin, offset_, "unterminated c string literal");
            add_token(syntax::TokenKind::invalid, begin, offset_);
            return;
        }
    }
    report(begin, offset_, "unterminated c string literal");
    add_token(syntax::TokenKind::invalid, begin, offset_);
}

void Lexer::scan_byte() {
    const base::usize begin = offset_;
    advance();
    advance();

    if (is_at_end() || peek() == '\n') {
        report(begin, offset_, "unterminated byte literal");
        add_token(syntax::TokenKind::invalid, begin, offset_);
        return;
    }

    if (peek() == '\\') {
        advance();
        if (!is_at_end()) {
            advance();
        }
    } else {
        advance();
    }

    if (!match('\'')) {
        while (!is_at_end() && peek() != '\'' && peek() != '\n') {
            advance();
        }
        if (match('\'')) {
            report(begin, offset_, "byte literal must contain one byte");
        } else {
            report(begin, offset_, "unterminated byte literal");
        }
        add_token(syntax::TokenKind::invalid, begin, offset_);
        return;
    }

    add_token(syntax::TokenKind::byte_literal, begin, offset_);
}

void Lexer::scan_line_comment() {
    while (!is_at_end() && peek() != '\n') {
        advance();
    }
}

void Lexer::scan_block_comment() {
    const base::usize begin = offset_;
    advance();
    advance();
    while (!is_at_end()) {
        if (peek() == '*' && peek_next() == '/') {
            advance();
            advance();
            return;
        }
        advance();
    }
    report(begin, offset_, "unterminated block comment");
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
