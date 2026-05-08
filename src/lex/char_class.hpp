#pragma once

namespace aurex::lex {

[[nodiscard]] inline bool is_ident_start(const char c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

[[nodiscard]] inline bool is_ident_continue(const char c) noexcept {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

[[nodiscard]] inline bool is_decimal_digit(const char c) noexcept {
    return c >= '0' && c <= '9';
}

[[nodiscard]] inline bool is_hex_digit(const char c) noexcept {
    return is_decimal_digit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

[[nodiscard]] inline bool is_binary_digit(const char c) noexcept {
    return c == '0' || c == '1';
}

[[nodiscard]] inline bool is_trivia_space(const char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

} // namespace aurex::lex
