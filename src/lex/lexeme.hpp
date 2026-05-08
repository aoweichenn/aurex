#pragma once

#include "aurex/base/integer.hpp"

#include <string_view>

namespace aurex::lex {

inline constexpr char eof_sentinel = '\0';
inline constexpr base::usize current_character_lookahead = 0;
inline constexpr base::usize next_character_lookahead = 1;
inline constexpr base::usize single_byte_lexeme_width = 1;

inline constexpr char lexeme_l_paren = '(';
inline constexpr char lexeme_r_paren = ')';
inline constexpr char lexeme_l_brace = '{';
inline constexpr char lexeme_r_brace = '}';
inline constexpr char lexeme_l_bracket = '[';
inline constexpr char lexeme_r_bracket = ']';
inline constexpr char lexeme_comma = ',';
inline constexpr char lexeme_dot = '.';
inline constexpr char lexeme_semicolon = ';';
inline constexpr char lexeme_colon = ':';
inline constexpr char lexeme_plus = '+';
inline constexpr char lexeme_minus = '-';
inline constexpr char lexeme_star = '*';
inline constexpr char lexeme_slash = '/';
inline constexpr char lexeme_percent = '%';
inline constexpr char lexeme_amp = '&';
inline constexpr char lexeme_pipe = '|';
inline constexpr char lexeme_caret = '^';
inline constexpr char lexeme_tilde = '~';
inline constexpr char lexeme_bang = '!';
inline constexpr char lexeme_equal = '=';
inline constexpr char lexeme_less = '<';
inline constexpr char lexeme_greater = '>';
inline constexpr char lexeme_at = '@';
inline constexpr char lexeme_question = '?';
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
inline constexpr std::string_view ellipsis_tail_after_dot = "..";
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

template <base::usize Size>
[[nodiscard]] consteval base::usize token_text_length(const char (&)[Size]) noexcept {
    return Size - single_byte_lexeme_width;
}

} // namespace aurex::lex
