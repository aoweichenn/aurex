#include "aurex/lex/lexer.hpp"

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
    const auto is_digit_in_set = [digit_set](const char c) noexcept {
        return (digit_set == DigitSet::decimal && is_decimal_digit(c)) ||
               (digit_set == DigitSet::hexadecimal && is_hex_digit(c)) ||
               (digit_set == DigitSet::binary && is_binary_digit(c));
    };

    DigitScanResult result;
    bool previous_was_digit = false;
    bool previous_was_separator = false;
    base::usize previous_separator_begin = this->cursor_.offset();
    while (true) {
        const char c = this->peek();
        if (c != digit_separator && !is_digit_in_set(c)) {
            break;
        }

        const base::usize separator_begin = this->cursor_.offset();
        this->advance();
        if (c == digit_separator) {
            if (!previous_was_digit) {
                result.had_error = true;
                this->report(
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
        this->report(
            previous_separator_begin,
            previous_separator_begin + single_byte_lexeme_width,
            malformed_digit_separator_message
        );
    }
    if (!result.saw_digit) {
        result.had_error = true;
        this->report(
            this->cursor_.offset(),
            this->cursor_.offset(),
            std::string(literal_kind) + " literal has no digits"
        );
    }
    return result;
}

bool Lexer::scan_invalid_radix_tail(const DigitSet digit_set, const std::string_view message) {
    const auto is_valid_radix_digit = [digit_set](const char c) noexcept {
        return (digit_set == DigitSet::hexadecimal && is_hex_digit(c)) ||
               (digit_set == DigitSet::binary && is_binary_digit(c));
    };

    bool had_error = false;
    bool reported = false;
    while (this->peek() == digit_separator || is_ident_continue(this->peek())) {
        const char c = this->peek();
        if (c != digit_separator && !is_valid_radix_digit(c) && !reported) {
            this->report(this->cursor_.offset(), this->cursor_.offset() + single_byte_lexeme_width, message);
            reported = true;
            had_error = true;
        }
        this->advance();
    }
    return had_error;
}

bool Lexer::scan_fraction_part(bool& had_error) {
    if (this->peek() != lexeme_dot || !is_decimal_digit(this->peek_next())) {
        return false;
    }
    this->advance();
    const DigitScanResult digits = this->scan_digits(DigitSet::decimal, "float");
    had_error = had_error || digits.had_error;
    return true;
}

bool Lexer::scan_exponent_part(bool& had_error) {
    if (this->peek() != float_exponent_lower && this->peek() != float_exponent_upper) {
        return false;
    }

    const char next_char = this->peek_next();
    if (!is_decimal_digit(next_char) && next_char != lexeme_plus && next_char != lexeme_minus) {
        if (next_char != digit_separator && is_ident_continue(next_char)) {
            return false;
        }
        this->advance();
        const DigitScanResult digits = this->scan_digits(DigitSet::decimal, "float exponent");
        had_error = had_error || digits.had_error;
        return true;
    }

    this->advance();
    if (this->peek() == lexeme_plus || this->peek() == lexeme_minus) {
        this->advance();
    }
    const DigitScanResult digits = this->scan_digits(DigitSet::decimal, "float exponent");
    had_error = had_error || digits.had_error;
    return true;
}

void Lexer::scan_number() {
    const base::usize begin = this->cursor_.offset();
    NumberScanState state = NumberScanState::decimal_integer;
    bool had_error = false;

    if (this->starts_with(hex_integer_prefix_lower) || this->starts_with(hex_integer_prefix_upper)) {
        state = NumberScanState::radix_integer;
        this->advance_bytes(hex_integer_prefix_lower.size());
        const DigitScanResult digits = this->scan_digits(DigitSet::hexadecimal, "integer");
        had_error = digits.had_error;
        had_error = this->scan_invalid_radix_tail(
            DigitSet::hexadecimal,
            invalid_hexadecimal_digit_message
        ) || had_error;
    } else if (this->starts_with(binary_integer_prefix_lower) || this->starts_with(binary_integer_prefix_upper)) {
        state = NumberScanState::radix_integer;
        this->advance_bytes(binary_integer_prefix_lower.size());
        const DigitScanResult digits = this->scan_digits(DigitSet::binary, "integer");
        had_error = digits.had_error;
        had_error = this->scan_invalid_radix_tail(
            DigitSet::binary,
            invalid_binary_digit_message
        ) || had_error;
    } else {
        const DigitScanResult digits = this->scan_digits(DigitSet::decimal, "integer");
        had_error = digits.had_error;
        if (this->scan_fraction_part(had_error)) {
            state = NumberScanState::fraction;
        }
        if (this->scan_exponent_part(had_error)) {
            state = NumberScanState::exponent;
        }
    }

    if (had_error) {
        this->finish_invalid_token(begin);
        return;
    }
    this->finish_token(
        is_float_state(state) ? syntax::TokenKind::float_literal : syntax::TokenKind::integer_literal,
        begin
    );
}

} // namespace aurex::lex
