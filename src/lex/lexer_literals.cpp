#include "aurex/lex/lexer.hpp"

#include "aurex/base/string_literal.hpp"
#include "char_class.hpp"
#include "lexeme.hpp"

#include <string>

namespace aurex::lex {

namespace {

enum class NumberScanState {
    decimal_integer,
    radix_integer,
    fraction,
    exponent,
};

[[nodiscard]] bool is_float_state(const NumberScanState state) noexcept {
    return state == NumberScanState::fraction || state == NumberScanState::exponent;
}

} // namespace

Lexer::DigitScanResult Lexer::scan_digits(const DigitSet digit_set, const std::string_view literal_kind) {
    DigitScanResult result;
    bool previous_was_digit = false;
    bool previous_was_separator = false;
    base::usize previous_separator_begin = cursor_.offset();
    while (peek() == digit_separator ||
           (digit_set == DigitSet::decimal && is_decimal_digit(peek())) ||
           (digit_set == DigitSet::hexadecimal && is_hex_digit(peek())) ||
           (digit_set == DigitSet::binary && is_binary_digit(peek()))) {
        const base::usize separator_begin = cursor_.offset();
        const char c = advance();
        if (c == digit_separator) {
            if (!previous_was_digit) {
                result.had_error = true;
                report(
                    separator_begin,
                    separator_begin + single_byte_lexeme_width,
                    malformed_digit_separator_message
                );
            }
            previous_separator_begin = separator_begin;
            previous_was_digit = false;
            previous_was_separator = true;
            continue;
        }
        result.saw_digit = true;
        previous_was_digit = true;
        previous_was_separator = false;
    }
    if (previous_was_separator) {
        result.had_error = true;
        report(
            previous_separator_begin,
            previous_separator_begin + single_byte_lexeme_width,
            malformed_digit_separator_message
        );
    }
    if (!result.saw_digit) {
        result.had_error = true;
        report(cursor_.offset(), cursor_.offset(), std::string(literal_kind) + " literal has no digits");
    }
    return result;
}

bool Lexer::scan_invalid_radix_tail(const DigitSet digit_set, const std::string_view message) {
    bool had_error = false;
    bool reported = false;
    while (peek() == digit_separator || is_ident_continue(peek())) {
        const char c = peek();
        const bool valid_digit =
            (digit_set == DigitSet::hexadecimal && is_hex_digit(c)) ||
            (digit_set == DigitSet::binary && is_binary_digit(c));
        if (c != digit_separator && !valid_digit && !reported) {
            report(cursor_.offset(), cursor_.offset() + single_byte_lexeme_width, message);
            reported = true;
            had_error = true;
        }
        advance();
    }
    return had_error;
}

bool Lexer::scan_fraction_part(bool& had_error) {
    if (peek() != lexeme_dot || !is_decimal_digit(peek_next())) {
        return false;
    }
    advance();
    const DigitScanResult digits = scan_digits(DigitSet::decimal, "float");
    had_error = had_error || digits.had_error;
    return true;
}

bool Lexer::scan_exponent_part(bool& had_error) {
    if (peek() != float_exponent_lower && peek() != float_exponent_upper) {
        return false;
    }

    const char next_char = peek_next();
    if (!is_decimal_digit(next_char) && next_char != lexeme_plus && next_char != lexeme_minus) {
        if (next_char != digit_separator && is_ident_continue(next_char)) {
            return false;
        }
        advance();
        const DigitScanResult digits = scan_digits(DigitSet::decimal, "float exponent");
        had_error = had_error || digits.had_error;
        return true;
    }

    advance();
    if (peek() == lexeme_plus || peek() == lexeme_minus) {
        advance();
    }
    const DigitScanResult digits = scan_digits(DigitSet::decimal, "float exponent");
    had_error = had_error || digits.had_error;
    return true;
}

void Lexer::scan_number() {
    const base::usize begin = cursor_.offset();
    NumberScanState state = NumberScanState::decimal_integer;
    bool had_error = false;

    if (starts_with(hex_integer_prefix_lower) || starts_with(hex_integer_prefix_upper)) {
        state = NumberScanState::radix_integer;
        advance_bytes(hex_integer_prefix_lower.size());
        const DigitScanResult digits = scan_digits(DigitSet::hexadecimal, "integer");
        had_error = digits.had_error;
        had_error = scan_invalid_radix_tail(
            DigitSet::hexadecimal,
            invalid_hexadecimal_digit_message
        ) || had_error;
    } else if (starts_with(binary_integer_prefix_lower) || starts_with(binary_integer_prefix_upper)) {
        state = NumberScanState::radix_integer;
        advance_bytes(binary_integer_prefix_lower.size());
        const DigitScanResult digits = scan_digits(DigitSet::binary, "integer");
        had_error = digits.had_error;
        had_error = scan_invalid_radix_tail(
            DigitSet::binary,
            invalid_binary_digit_message
        ) || had_error;
    } else {
        const DigitScanResult digits = scan_digits(DigitSet::decimal, "integer");
        had_error = digits.had_error;
        if (scan_fraction_part(had_error)) {
            state = NumberScanState::fraction;
        }
        if (scan_exponent_part(had_error)) {
            state = NumberScanState::exponent;
        }
    }

    if (had_error) {
        if (options_.emit_invalid_tokens) {
            finish_token(syntax::TokenKind::invalid, begin);
        }
        return;
    }
    finish_token(
        is_float_state(state) ? syntax::TokenKind::float_literal : syntax::TokenKind::integer_literal,
        begin
    );
}

void Lexer::scan_string(const base::usize begin) {
    scan_string_body(
        begin,
        syntax::TokenKind::string_literal,
        base::StringLiteralKind::string,
        unterminated_string_message
    );
}

void Lexer::scan_c_string(const base::usize begin) {
    advance_bytes(c_string_prefix.size());
    scan_string_body(
        begin,
        syntax::TokenKind::c_string_literal,
        base::StringLiteralKind::c_string,
        unterminated_c_string_message
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
        if (c == lexeme_escape) {
            escaped = true;
            continue;
        }
        if (c == lexeme_double_quote) {
            const base::StringLiteralDecode decoded = base::decode_string_literal(
                cursor_.slice(begin, cursor_.offset()),
                literal_kind
            );
            for (const base::StringLiteralError& error : decoded.errors) {
                report(begin + error.begin, begin + error.end, error.message);
            }
            if (decoded.ok()) {
                finish_token(token_kind, begin);
            } else if (options_.emit_invalid_tokens) {
                finish_token(syntax::TokenKind::invalid, begin);
            }
            return;
        }
        if (c == lexeme_line_feed) {
            report(begin, cursor_.offset(), std::string(unterminated_message));
            finish_token(syntax::TokenKind::invalid, begin);
            return;
        }
    }
    report(begin, cursor_.offset(), std::string(unterminated_message));
    finish_token(syntax::TokenKind::invalid, begin);
}

void Lexer::scan_byte(const base::usize begin) {
    advance_bytes(byte_literal_prefix.size());

    if (is_at_end() || peek() == lexeme_line_feed) {
        report(begin, cursor_.offset(), unterminated_byte_message);
        finish_token(syntax::TokenKind::invalid, begin);
        return;
    }

    if (peek() == lexeme_escape) {
        advance();
        if (!is_at_end()) {
            advance();
        }
    } else {
        advance();
    }

    if (!match(lexeme_single_quote)) {
        while (!is_at_end() && peek() != lexeme_single_quote && peek() != lexeme_line_feed) {
            advance();
        }
        if (match(lexeme_single_quote)) {
            report(begin, cursor_.offset(), oversized_byte_message);
        } else {
            report(begin, cursor_.offset(), unterminated_byte_message);
        }
        finish_token(syntax::TokenKind::invalid, begin);
        return;
    }

    finish_token(syntax::TokenKind::byte_literal, begin);
}

} // namespace aurex::lex
