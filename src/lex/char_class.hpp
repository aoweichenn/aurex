#pragma once

namespace aurex::lex {

inline constexpr char ascii_uppercase_first = 'A';
inline constexpr char ascii_uppercase_last = 'Z';
inline constexpr char ascii_lowercase_first = 'a';
inline constexpr char ascii_lowercase_last = 'z';
inline constexpr char ascii_hex_uppercase_last = 'F';
inline constexpr char ascii_hex_lowercase_last = 'f';
inline constexpr char ascii_digit_first = '0';
inline constexpr char ascii_digit_last = '9';
inline constexpr char ascii_binary_digit_last = '1';
inline constexpr char identifier_joiner = '_';
inline constexpr char ascii_space = ' ';
inline constexpr char ascii_horizontal_tab = '\t';
inline constexpr char ascii_carriage_return = '\r';
inline constexpr char ascii_line_feed = '\n';

[[nodiscard]] inline bool is_ascii_range(const char c, const char first, const char last) noexcept {
    return c >= first && c <= last;
}

[[nodiscard]] inline bool is_ident_start(const char c) noexcept {
    return is_ascii_range(c, ascii_uppercase_first, ascii_uppercase_last) ||
           is_ascii_range(c, ascii_lowercase_first, ascii_lowercase_last) ||
           c == identifier_joiner;
}

[[nodiscard]] inline bool is_decimal_digit(const char c) noexcept {
    return is_ascii_range(c, ascii_digit_first, ascii_digit_last);
}

[[nodiscard]] inline bool is_ident_continue(const char c) noexcept {
    return is_ident_start(c) || is_decimal_digit(c);
}

[[nodiscard]] inline bool is_hex_digit(const char c) noexcept {
    return is_decimal_digit(c) ||
           is_ascii_range(c, ascii_uppercase_first, ascii_hex_uppercase_last) ||
           is_ascii_range(c, ascii_lowercase_first, ascii_hex_lowercase_last);
}

[[nodiscard]] inline bool is_binary_digit(const char c) noexcept {
    return is_ascii_range(c, ascii_digit_first, ascii_binary_digit_last);
}

[[nodiscard]] inline bool is_trivia_space(const char c) noexcept {
    return c == ascii_space ||
           c == ascii_horizontal_tab ||
           c == ascii_carriage_return ||
           c == ascii_line_feed;
}

} // namespace aurex::lex
