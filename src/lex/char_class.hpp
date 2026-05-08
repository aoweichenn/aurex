#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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

inline constexpr std::size_t byte_char_class_count = 256;
inline constexpr std::uint8_t no_char_class = 0;
inline constexpr std::uint8_t char_class_identifier_start = static_cast<std::uint8_t>(1U << 0U);
inline constexpr std::uint8_t char_class_decimal_digit = static_cast<std::uint8_t>(1U << 1U);
inline constexpr std::uint8_t char_class_binary_digit = static_cast<std::uint8_t>(1U << 2U);
inline constexpr std::uint8_t char_class_hex_digit = static_cast<std::uint8_t>(1U << 3U);
inline constexpr std::uint8_t char_class_trivia_space = static_cast<std::uint8_t>(1U << 4U);
inline constexpr std::uint8_t char_class_identifier_continue =
    static_cast<std::uint8_t>(char_class_identifier_start | char_class_decimal_digit);

[[nodiscard]] constexpr std::size_t char_class_index(const char c) noexcept {
    return static_cast<unsigned char>(c);
}

constexpr void mark_char_class(
    std::array<std::uint8_t, byte_char_class_count>& table,
    const char c,
    const std::uint8_t flags
) noexcept {
    const std::size_t index = char_class_index(c);
    table[index] = static_cast<std::uint8_t>(table[index] | flags);
}

constexpr void mark_char_class_range(
    std::array<std::uint8_t, byte_char_class_count>& table,
    const char first,
    const char last,
    const std::uint8_t flags
) noexcept {
    const std::size_t first_index = char_class_index(first);
    const std::size_t last_index = char_class_index(last);
    for (std::size_t index = first_index; index <= last_index; ++index) {
        table[index] = static_cast<std::uint8_t>(table[index] | flags);
    }
}

[[nodiscard]] consteval std::array<std::uint8_t, byte_char_class_count> build_byte_char_classes() noexcept {
    std::array<std::uint8_t, byte_char_class_count> table {};

    mark_char_class_range(table, ascii_uppercase_first, ascii_uppercase_last, char_class_identifier_start);
    mark_char_class_range(table, ascii_lowercase_first, ascii_lowercase_last, char_class_identifier_start);
    mark_char_class(table, identifier_joiner, char_class_identifier_start);

    mark_char_class_range(table, ascii_digit_first, ascii_digit_last, char_class_decimal_digit);
    mark_char_class_range(table, ascii_digit_first, ascii_binary_digit_last, char_class_binary_digit);
    mark_char_class_range(table, ascii_digit_first, ascii_digit_last, char_class_hex_digit);
    mark_char_class_range(table, ascii_uppercase_first, ascii_hex_uppercase_last, char_class_hex_digit);
    mark_char_class_range(table, ascii_lowercase_first, ascii_hex_lowercase_last, char_class_hex_digit);

    mark_char_class(table, ascii_space, char_class_trivia_space);
    mark_char_class(table, ascii_horizontal_tab, char_class_trivia_space);
    mark_char_class(table, ascii_carriage_return, char_class_trivia_space);
    mark_char_class(table, ascii_line_feed, char_class_trivia_space);

    return table;
}

inline constexpr std::array byte_char_classes = build_byte_char_classes();

[[nodiscard]] inline bool has_char_class(const char c, const std::uint8_t flags) noexcept {
    return (byte_char_classes[char_class_index(c)] & flags) != no_char_class;
}

[[nodiscard]] inline bool is_ident_start(const char c) noexcept {
    return has_char_class(c, char_class_identifier_start);
}

[[nodiscard]] inline bool is_decimal_digit(const char c) noexcept {
    return has_char_class(c, char_class_decimal_digit);
}

[[nodiscard]] inline bool is_ident_continue(const char c) noexcept {
    return has_char_class(c, char_class_identifier_continue);
}

[[nodiscard]] inline bool is_hex_digit(const char c) noexcept {
    return has_char_class(c, char_class_hex_digit);
}

[[nodiscard]] inline bool is_binary_digit(const char c) noexcept {
    return has_char_class(c, char_class_binary_digit);
}

[[nodiscard]] inline bool is_trivia_space(const char c) noexcept {
    return has_char_class(c, char_class_trivia_space);
}

} // namespace aurex::lex
