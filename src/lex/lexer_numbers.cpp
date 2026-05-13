#include <aurex/lex/lexer.hpp>

#include <lex/char_class.hpp>
#include <lex/lexeme.hpp>

#include <string>

namespace aurex::lex {

namespace {

enum class NumberScanState {
    DECIMAL_INTEGER,
    RADIX_INTEGER,
    FRACTION,
    EXPONENT,
};

[[nodiscard]] bool is_float_state(const NumberScanState state) noexcept {
    return state == NumberScanState::FRACTION || state == NumberScanState::EXPONENT;
}

[[nodiscard]] bool can_start_integer_suffix(const char c) noexcept {
    return c == 'i' || c == 'u';
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
    while (width < remaining.size() && remaining[width] == LEXEME_DIGIT_SEPARATOR) {
        const base::usize separator_begin = begin + width;
        ++width;
        if (!previous_was_digit) {
            result.had_error = true;
            this->report(
                separator_begin,
                separator_begin + LEXEME_SINGLE_BYTE_WIDTH,
                LEXEME_MALFORMED_DIGIT_SEPARATOR_MESSAGE
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
            previous_separator_begin + LEXEME_SINGLE_BYTE_WIDTH,
            LEXEME_MALFORMED_DIGIT_SEPARATOR_MESSAGE
        );
    }
    if (!result.saw_digit) {
        result.had_error = true;
        this->report(
            begin + width,
            begin + width,
            std::string(literal_kind) + std::string(LEXEME_LITERAL_NO_DIGITS_SUFFIX)
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
    if (first != LEXEME_DIGIT_SEPARATOR && !is_ident_continue(first)) {
        return false;
    }

    bool had_error = false;
    bool reported = false;
    const base::usize begin = this->cursor_.offset();
    const std::string_view remaining = this->cursor_.remaining_text();
    base::usize width = 0;
    while (width < remaining.size() &&
           (remaining[width] == LEXEME_DIGIT_SEPARATOR || is_ident_continue(remaining[width]))) {
        const char c = remaining[width];
        if (c != LEXEME_DIGIT_SEPARATOR && !is_valid_digit(c) && !reported) {
            this->report(begin + width, begin + width + LEXEME_SINGLE_BYTE_WIDTH, message);
            reported = true;
            had_error = true;
        }
        ++width;
    }
    this->advance_bytes(width);
    return had_error;
}

bool Lexer::scan_fraction_part(bool& had_error) {
    if (this->peek() != LEXEME_DOT) {
        return false;
    }
    if (!is_decimal_digit(this->peek_next())) {
        const char next = this->peek_next();
        if (next == LEXEME_DOT || is_ident_continue(next)) {
            return false;
        }
        this->advance();
        return true;
    }
    this->advance();
    const DigitScanResult digits = this->scan_digits_matching(
        [](const char c) noexcept { return is_decimal_digit(c); },
        LEXEME_FLOAT_LITERAL_KIND
    );
    had_error = had_error || digits.had_error;
    return true;
}

void Lexer::scan_numeric_suffix() {
    if (!is_ident_start(this->peek())) {
        return;
    }
    const std::string_view remaining = this->cursor_.remaining_text();
    base::usize width = 0;
    while (width < remaining.size() && is_ident_continue(remaining[width])) {
        ++width;
    }
    this->advance_bytes(width);
}

bool Lexer::scan_integer_suffix() {
    if (!can_start_integer_suffix(this->peek())) {
        return false;
    }
    this->scan_numeric_suffix();
    return true;
}

void Lexer::scan_leading_dot_float(const base::usize begin) {
    bool had_error = false;
    this->advance();
    const DigitScanResult digits = this->scan_digits_matching(
        [](const char c) noexcept { return is_decimal_digit(c); },
        LEXEME_FLOAT_LITERAL_KIND
    );
    had_error = digits.had_error;
    static_cast<void>(this->scan_exponent_part(had_error));
    this->scan_numeric_suffix();
    if (had_error) {
        this->finish_invalid_token(begin);
        return;
    }
    this->add_nonempty_token(syntax::TokenKind::float_literal, begin, this->cursor_.offset());
}

bool Lexer::scan_exponent_part(bool& had_error) {
    const char exponent_marker = this->peek();
    if (exponent_marker != LEXEME_FLOAT_EXPONENT_LOWER && exponent_marker != LEXEME_FLOAT_EXPONENT_UPPER) {
        return false;
    }

    const char next_char = this->peek_next();
    if (!is_decimal_digit(next_char) && next_char != LEXEME_PLUS && next_char != LEXEME_MINUS) {
        if (next_char != LEXEME_DIGIT_SEPARATOR && is_ident_continue(next_char)) {
            return false;
        }
        this->advance();
        const DigitScanResult digits = this->scan_digits_matching(
            [](const char c) noexcept { return is_decimal_digit(c); },
            LEXEME_FLOAT_EXPONENT_LITERAL_KIND
        );
        had_error = had_error || digits.had_error;
        return true;
    }

    this->advance();
    if (this->peek() == LEXEME_PLUS || this->peek() == LEXEME_MINUS) {
        this->advance();
    }
    const DigitScanResult digits = this->scan_digits_matching(
        [](const char c) noexcept { return is_decimal_digit(c); },
        LEXEME_FLOAT_EXPONENT_LITERAL_KIND
    );
    had_error = had_error || digits.had_error;
    return true;
}

void Lexer::scan_number() {
    const base::usize begin = this->cursor_.offset();
    const char first = this->peek();
    const char second = this->peek_next();
    NumberScanState state = NumberScanState::DECIMAL_INTEGER;
    bool had_error = false;

    if (first == LEXEME_HEX_INTEGER_PREFIX_LOWER.front() &&
        (second == LEXEME_HEX_INTEGER_PREFIX_LOWER.back() || second == LEXEME_HEX_INTEGER_PREFIX_UPPER.back())) {
        state = NumberScanState::RADIX_INTEGER;
        this->advance_bytes(LEXEME_HEX_INTEGER_PREFIX_LOWER.size());
        const DigitScanResult digits = this->scan_digits_matching(
            [](const char c) noexcept { return is_hex_digit(c); },
            LEXEME_INTEGER_LITERAL_KIND
        );
        had_error = digits.had_error;
        if (!this->scan_integer_suffix()) {
            had_error = this->scan_invalid_radix_tail_matching(
                [](const char c) noexcept { return is_hex_digit(c); },
                LEXEME_INVALID_HEXADECIMAL_DIGIT_MESSAGE
            ) || had_error;
        }
    } else if (first == LEXEME_BINARY_INTEGER_PREFIX_LOWER.front() &&
               (second == LEXEME_BINARY_INTEGER_PREFIX_LOWER.back() || second == LEXEME_BINARY_INTEGER_PREFIX_UPPER.back())) {
        state = NumberScanState::RADIX_INTEGER;
        this->advance_bytes(LEXEME_BINARY_INTEGER_PREFIX_LOWER.size());
        const DigitScanResult digits = this->scan_digits_matching(
            [](const char c) noexcept { return is_binary_digit(c); },
            LEXEME_INTEGER_LITERAL_KIND
        );
        had_error = digits.had_error;
        if (!this->scan_integer_suffix()) {
            had_error = this->scan_invalid_radix_tail_matching(
                [](const char c) noexcept { return is_binary_digit(c); },
                LEXEME_INVALID_BINARY_DIGIT_MESSAGE
            ) || had_error;
        }
    } else {
        const DigitScanResult digits = this->scan_digits_matching(
            [](const char c) noexcept { return is_decimal_digit(c); },
            LEXEME_INTEGER_LITERAL_KIND
        );
        had_error = digits.had_error;
        if (this->scan_fraction_part(had_error)) {
            state = NumberScanState::FRACTION;
        }
        if (this->scan_exponent_part(had_error)) {
            state = NumberScanState::EXPONENT;
        }
        this->scan_numeric_suffix();
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
