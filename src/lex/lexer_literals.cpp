#include "aurex/lex/lexer.hpp"

#include "aurex/base/string_literal.hpp"
#include "char_class.hpp"

#include <string>

namespace aurex::lex {

bool Lexer::scan_digits(const DigitSet digit_set, const std::string_view literal_kind) {
    bool saw_digit = false;
    bool previous_was_digit = false;
    bool previous_was_separator = false;
    while (peek() == '_' ||
           (digit_set == DigitSet::decimal && is_decimal_digit(peek())) ||
           (digit_set == DigitSet::hexadecimal && is_hex_digit(peek())) ||
           (digit_set == DigitSet::binary && is_binary_digit(peek()))) {
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
}

bool Lexer::scan_fraction_part() {
    if (peek() != '.' || !is_decimal_digit(peek_next())) {
        return false;
    }
    advance();
    static_cast<void>(scan_digits(DigitSet::decimal, "float"));
    return true;
}

bool Lexer::scan_exponent_part() {
    if (peek() != 'e' && peek() != 'E') {
        return false;
    }

    const base::usize next = offset_ + 1;
    const char next_char = next < source_text_.size() ? source_text_[next] : '\0';
    if (!is_decimal_digit(next_char) && next_char != '+' && next_char != '-') {
        return false;
    }

    advance();
    if (peek() == '+' || peek() == '-') {
        advance();
    }
    static_cast<void>(scan_digits(DigitSet::decimal, "float exponent"));
    return true;
}

void Lexer::scan_number() {
    const base::usize begin = offset_;
    bool is_float = false;

    if (peek() == '0' && (peek_next() == 'x' || peek_next() == 'X')) {
        advance();
        advance();
        static_cast<void>(scan_digits(DigitSet::hexadecimal, "integer"));
    } else if (peek() == '0' && (peek_next() == 'b' || peek_next() == 'B')) {
        advance();
        advance();
        static_cast<void>(scan_digits(DigitSet::binary, "integer"));
    } else {
        static_cast<void>(scan_digits(DigitSet::decimal, "integer"));
        is_float = scan_fraction_part();
        is_float = scan_exponent_part() || is_float;
    }

    add_token(is_float ? syntax::TokenKind::float_literal : syntax::TokenKind::integer_literal, begin, offset_);
}

void Lexer::scan_string() {
    const base::usize begin = offset_ - 1;
    scan_string_body(
        begin,
        syntax::TokenKind::string_literal,
        base::StringLiteralKind::string,
        "unterminated string literal"
    );
}

void Lexer::scan_c_string() {
    const base::usize begin = offset_;
    advance();
    advance();
    scan_string_body(
        begin,
        syntax::TokenKind::c_string_literal,
        base::StringLiteralKind::c_string,
        "unterminated c string literal"
    );
}

void Lexer::scan_string_body(
    const base::usize begin,
    const syntax::TokenKind token_kind,
    const base::StringLiteralKind literal_kind,
    const std::string_view unterminated_message
) {
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
                literal_kind
            );
            for (const base::StringLiteralError& error : decoded.errors) {
                report(begin + error.begin, begin + error.end, error.message);
            }
            if (decoded.ok()) {
                add_token(token_kind, begin, offset_);
            } else if (options_.emit_invalid_tokens) {
                add_token(syntax::TokenKind::invalid, begin, offset_);
            }
            return;
        }
        if (c == '\n') {
            report(begin, offset_, std::string(unterminated_message));
            add_token(syntax::TokenKind::invalid, begin, offset_);
            return;
        }
    }
    report(begin, offset_, std::string(unterminated_message));
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

} // namespace aurex::lex
