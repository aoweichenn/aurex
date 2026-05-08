#pragma once

#include "aurex/base/integer.hpp"

#include <string_view>

namespace aurex::lex {

inline constexpr char eof_sentinel = '\0';
inline constexpr base::usize current_character_lookahead = 0;
inline constexpr base::usize next_character_lookahead = 1;
inline constexpr base::usize single_byte_lexeme_width = 1;

inline constexpr char digit_separator = '_';

inline constexpr std::string_view c_string_prefix = "c\"";
inline constexpr std::string_view byte_literal_prefix = "b'";
inline constexpr std::string_view hex_integer_prefix_lower = "0x";
inline constexpr std::string_view hex_integer_prefix_upper = "0X";
inline constexpr std::string_view binary_integer_prefix_lower = "0b";
inline constexpr std::string_view binary_integer_prefix_upper = "0B";
inline constexpr std::string_view ellipsis_tail_after_dot = "..";
inline constexpr std::string_view line_comment_prefix = "//";
inline constexpr std::string_view block_comment_prefix = "/*";
inline constexpr std::string_view block_comment_suffix = "*/";

template <base::usize Size>
[[nodiscard]] consteval base::usize token_text_length(const char (&)[Size]) noexcept {
    return Size - single_byte_lexeme_width;
}

} // namespace aurex::lex
