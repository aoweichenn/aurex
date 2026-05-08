#pragma once

#include "aurex/base/integer.hpp"

#include <string_view>

namespace aurex::lex {

inline constexpr base::usize single_byte_lexeme_width = 1;

inline constexpr char lexeme_dot = '.';
inline constexpr char lexeme_plus = '+';
inline constexpr char lexeme_minus = '-';
inline constexpr char lexeme_double_quote = '"';
inline constexpr char lexeme_single_quote = '\'';
inline constexpr char lexeme_escape = '\\';
inline constexpr char lexeme_line_feed = '\n';

inline constexpr char digit_separator = '_';
inline constexpr char float_exponent_lower = 'e';
inline constexpr char float_exponent_upper = 'E';

inline constexpr std::string_view c_string_prefix = "c\"";
inline constexpr std::string_view byte_literal_prefix = "b'";
inline constexpr std::string_view hex_integer_prefix_lower = "0x";
inline constexpr std::string_view hex_integer_prefix_upper = "0X";
inline constexpr std::string_view binary_integer_prefix_lower = "0b";
inline constexpr std::string_view binary_integer_prefix_upper = "0B";
inline constexpr std::string_view line_comment_prefix = "//";
inline constexpr std::string_view block_comment_prefix = "/*";
inline constexpr std::string_view block_comment_suffix = "*/";

inline constexpr std::string_view lexing_failed_message = "lexing failed";
inline constexpr std::string_view invalid_character_message = "invalid character";
inline constexpr std::string_view unterminated_block_comment_message = "unterminated block comment";
inline constexpr std::string_view malformed_digit_separator_message = "digit separator must be between digits";
inline constexpr std::string_view unterminated_string_message = "unterminated string literal";
inline constexpr std::string_view unterminated_c_string_message = "unterminated c string literal";
inline constexpr std::string_view unterminated_byte_message = "unterminated byte literal";
inline constexpr std::string_view oversized_byte_message = "byte literal must contain one byte";
inline constexpr std::string_view invalid_binary_digit_message = "invalid digit in binary literal";
inline constexpr std::string_view invalid_hexadecimal_digit_message = "invalid digit in hexadecimal literal";

} // namespace aurex::lex
