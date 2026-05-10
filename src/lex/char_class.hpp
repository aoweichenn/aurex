#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace aurex::lex {

inline constexpr char LEX_ASCII_UPPERCASE_FIRST = 'A';
inline constexpr char LEX_ASCII_UPPERCASE_LAST = 'Z';
inline constexpr char LEX_ASCII_LOWERCASE_FIRST = 'a';
inline constexpr char LEX_ASCII_LOWERCASE_LAST = 'z';
inline constexpr char LEX_ASCII_HEX_UPPERCASE_LAST = 'F';
inline constexpr char LEX_ASCII_HEX_LOWERCASE_LAST = 'f';
inline constexpr char LEX_ASCII_DIGIT_FIRST = '0';
inline constexpr char LEX_ASCII_DIGIT_LAST = '9';
inline constexpr char LEX_ASCII_BINARY_DIGIT_LAST = '1';
inline constexpr char LEX_IDENTIFIER_JOINER = '_';
inline constexpr char LEX_ASCII_SPACE = ' ';
inline constexpr char LEX_ASCII_HORIZONTAL_TAB = '\t';
inline constexpr char LEX_ASCII_CARRIAGE_RETURN = '\r';
inline constexpr char LEX_ASCII_LINE_FEED = '\n';

inline constexpr std::size_t LEX_BYTE_CHAR_CLASS_COUNT = 256;
inline constexpr std::uint8_t LEX_NO_CHAR_CLASS = 0;
inline constexpr std::uint8_t LEX_CHAR_CLASS_IDENTIFIER_START = static_cast<std::uint8_t>(1U << 0U);
inline constexpr std::uint8_t LEX_CHAR_CLASS_DECIMAL_DIGIT = static_cast<std::uint8_t>(1U << 1U);
inline constexpr std::uint8_t LEX_CHAR_CLASS_BINARY_DIGIT = static_cast<std::uint8_t>(1U << 2U);
inline constexpr std::uint8_t LEX_CHAR_CLASS_HEX_DIGIT = static_cast<std::uint8_t>(1U << 3U);
inline constexpr std::uint8_t LEX_CHAR_CLASS_TRIVIA_SPACE = static_cast<std::uint8_t>(1U << 4U);
inline constexpr std::uint8_t LEX_CHAR_CLASS_IDENTIFIER_CONTINUE =
    static_cast<std::uint8_t>(LEX_CHAR_CLASS_IDENTIFIER_START | LEX_CHAR_CLASS_DECIMAL_DIGIT);

[[nodiscard]] constexpr std::size_t char_class_index(const char c) noexcept {
    return static_cast<unsigned char>(c);
}

constexpr void mark_char_class(
    std::array<std::uint8_t, LEX_BYTE_CHAR_CLASS_COUNT>& table,
    const char c,
    const std::uint8_t flags
) noexcept {
    const std::size_t index = char_class_index(c);
    table[index] = static_cast<std::uint8_t>(table[index] | flags);
}

constexpr void mark_char_class_range(
    std::array<std::uint8_t, LEX_BYTE_CHAR_CLASS_COUNT>& table,
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

[[nodiscard]] consteval std::array<std::uint8_t, LEX_BYTE_CHAR_CLASS_COUNT> build_byte_char_classes() noexcept {
    std::array<std::uint8_t, LEX_BYTE_CHAR_CLASS_COUNT> table {};

    mark_char_class_range(table, LEX_ASCII_UPPERCASE_FIRST, LEX_ASCII_UPPERCASE_LAST, LEX_CHAR_CLASS_IDENTIFIER_START);
    mark_char_class_range(table, LEX_ASCII_LOWERCASE_FIRST, LEX_ASCII_LOWERCASE_LAST, LEX_CHAR_CLASS_IDENTIFIER_START);
    mark_char_class(table, LEX_IDENTIFIER_JOINER, LEX_CHAR_CLASS_IDENTIFIER_START);

    mark_char_class_range(table, LEX_ASCII_DIGIT_FIRST, LEX_ASCII_DIGIT_LAST, LEX_CHAR_CLASS_DECIMAL_DIGIT);
    mark_char_class_range(table, LEX_ASCII_DIGIT_FIRST, LEX_ASCII_BINARY_DIGIT_LAST, LEX_CHAR_CLASS_BINARY_DIGIT);
    mark_char_class_range(table, LEX_ASCII_DIGIT_FIRST, LEX_ASCII_DIGIT_LAST, LEX_CHAR_CLASS_HEX_DIGIT);
    mark_char_class_range(table, LEX_ASCII_UPPERCASE_FIRST, LEX_ASCII_HEX_UPPERCASE_LAST, LEX_CHAR_CLASS_HEX_DIGIT);
    mark_char_class_range(table, LEX_ASCII_LOWERCASE_FIRST, LEX_ASCII_HEX_LOWERCASE_LAST, LEX_CHAR_CLASS_HEX_DIGIT);

    mark_char_class(table, LEX_ASCII_SPACE, LEX_CHAR_CLASS_TRIVIA_SPACE);
    mark_char_class(table, LEX_ASCII_HORIZONTAL_TAB, LEX_CHAR_CLASS_TRIVIA_SPACE);
    mark_char_class(table, LEX_ASCII_CARRIAGE_RETURN, LEX_CHAR_CLASS_TRIVIA_SPACE);
    mark_char_class(table, LEX_ASCII_LINE_FEED, LEX_CHAR_CLASS_TRIVIA_SPACE);

    return table;
}

inline constexpr std::array LEX_BYTE_CHAR_CLASSES = build_byte_char_classes();

[[nodiscard]] inline std::uint8_t char_classes(const char c) noexcept {
    return LEX_BYTE_CHAR_CLASSES[char_class_index(c)];
}

[[nodiscard]] inline bool has_char_class_flags(
    const std::uint8_t classes,
    const std::uint8_t flags
) noexcept {
    return (classes & flags) != LEX_NO_CHAR_CLASS;
}

[[nodiscard]] inline bool has_char_class(const char c, const std::uint8_t flags) noexcept {
    return has_char_class_flags(char_classes(c), flags);
}

[[nodiscard]] inline bool is_ident_start(const char c) noexcept {
    return has_char_class(c, LEX_CHAR_CLASS_IDENTIFIER_START);
}

[[nodiscard]] inline bool is_decimal_digit(const char c) noexcept {
    return has_char_class(c, LEX_CHAR_CLASS_DECIMAL_DIGIT);
}

[[nodiscard]] inline bool is_ident_continue(const char c) noexcept {
    return has_char_class(c, LEX_CHAR_CLASS_IDENTIFIER_CONTINUE);
}

[[nodiscard]] inline bool is_hex_digit(const char c) noexcept {
    return has_char_class(c, LEX_CHAR_CLASS_HEX_DIGIT);
}

[[nodiscard]] inline bool is_binary_digit(const char c) noexcept {
    return has_char_class(c, LEX_CHAR_CLASS_BINARY_DIGIT);
}

[[nodiscard]] inline bool is_trivia_space(const char c) noexcept {
    return has_char_class(c, LEX_CHAR_CLASS_TRIVIA_SPACE);
}

} // namespace aurex::lex
