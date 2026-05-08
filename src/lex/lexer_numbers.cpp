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

template <typename IsDigit>
Lexer::DigitScanResult Lexer::scan_digits_matching(
    const IsDigit is_digit,
    const std::string_view literal_kind
) {
    DigitScanResult result;
    bool previous_was_digit = false;
    bool previous_was_separator = false;
    const base::usize begin = this->cursor_.offset();
    const std::string_view remaining = this->cursor_.remaining_text();
    base::usize width = 0;
    base::usize previous_separator_begin = begin;

    const auto scan_digit_run = [&]() noexcept {
        const base::usize run_begin = width;
        while (width < remaining.size() && is_digit(remaining[width])) {
            ++width;
        }
        if (width == run_begin) {
            return;
        }
        result.saw_digit = true;
        previous_was_digit = true;
        previous_was_separator = false;
    };

    scan_digit_run();
    while (width < remaining.size() && remaining[width] == digit_separator) {
        const base::usize separator_begin = begin + width;
        ++width;
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
        scan_digit_run();
    }
    this->advance_bytes(width);
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
            begin + width,
            begin + width,
            std::string(literal_kind) + " literal has no digits"
        );
    }
    return result;
}

template <typename IsValidDigit>
bool Lexer::scan_invalid_radix_tail_matching(
    const IsValidDigit is_valid_digit,
    const std::string_view message
) {
    const char first = this->peek();
    if (first != digit_separator && !is_ident_continue(first)) {
        return false;
    }

    bool had_error = false;
    bool reported = false;
    const base::usize begin = this->cursor_.offset();
    const std::string_view remaining = this->cursor_.remaining_text();
    base::usize width = 0;
    while (width < remaining.size() &&
           (remaining[width] == digit_separator || is_ident_continue(remaining[width]))) {
        const char c = remaining[width];
        if (c != digit_separator && !is_valid_digit(c) && !reported) {
            this->report(begin + width, begin + width + single_byte_lexeme_width, message);
            reported = true;
            had_error = true;
        }
        ++width;
    }
    this->advance_bytes(width);
    return had_error;
}

bool Lexer::scan_fraction_part(bool& had_error) {
    if (this->peek() != lexeme_dot || !is_decimal_digit(this->peek_next())) {
        return false;
    }
    this->advance();
    const DigitScanResult digits = this->scan_digits_matching(
        [](const char c) noexcept { return is_decimal_digit(c); },
        "float"
    );
    had_error = had_error || digits.had_error;
    return true;
}

bool Lexer::scan_exponent_part(bool& had_error) {
    const char exponent_marker = this->peek();
    if (exponent_marker != float_exponent_lower && exponent_marker != float_exponent_upper) {
        return false;
    }

    const char next_char = this->peek_next();
    if (!is_decimal_digit(next_char) && next_char != lexeme_plus && next_char != lexeme_minus) {
        if (next_char != digit_separator && is_ident_continue(next_char)) {
            return false;
        }
        this->advance();
        const DigitScanResult digits = this->scan_digits_matching(
            [](const char c) noexcept { return is_decimal_digit(c); },
            "float exponent"
        );
        had_error = had_error || digits.had_error;
        return true;
    }

    this->advance();
    if (this->peek() == lexeme_plus || this->peek() == lexeme_minus) {
        this->advance();
    }
    const DigitScanResult digits = this->scan_digits_matching(
        [](const char c) noexcept { return is_decimal_digit(c); },
        "float exponent"
    );
    had_error = had_error || digits.had_error;
    return true;
}

void Lexer::scan_number() {
    const base::usize begin = this->cursor_.offset();
    const char first = this->peek();
    const char second = this->peek_next();
    NumberScanState state = NumberScanState::decimal_integer;
    bool had_error = false;

    if (first == hex_integer_prefix_lower.front() &&
        (second == hex_integer_prefix_lower.back() || second == hex_integer_prefix_upper.back())) {
        state = NumberScanState::radix_integer;
        this->advance_bytes(hex_integer_prefix_lower.size());
        const DigitScanResult digits = this->scan_digits_matching(
            [](const char c) noexcept { return is_hex_digit(c); },
            "integer"
        );
        had_error = digits.had_error;
        had_error = this->scan_invalid_radix_tail_matching(
            [](const char c) noexcept { return is_hex_digit(c); },
            invalid_hexadecimal_digit_message
        ) || had_error;
    } else if (first == binary_integer_prefix_lower.front() &&
               (second == binary_integer_prefix_lower.back() || second == binary_integer_prefix_upper.back())) {
        state = NumberScanState::radix_integer;
        this->advance_bytes(binary_integer_prefix_lower.size());
        const DigitScanResult digits = this->scan_digits_matching(
            [](const char c) noexcept { return is_binary_digit(c); },
            "integer"
        );
        had_error = digits.had_error;
        had_error = this->scan_invalid_radix_tail_matching(
            [](const char c) noexcept { return is_binary_digit(c); },
            invalid_binary_digit_message
        ) || had_error;
    } else {
        const DigitScanResult digits = this->scan_digits_matching(
            [](const char c) noexcept { return is_decimal_digit(c); },
            "integer"
        );
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
    this->add_nonempty_token(
        is_float_state(state) ? syntax::TokenKind::float_literal : syntax::TokenKind::integer_literal,
        begin,
        this->cursor_.offset()
    );
}

} // namespace aurex::lex
