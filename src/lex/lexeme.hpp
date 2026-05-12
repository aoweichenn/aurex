#pragma once

#include <aurex/base/integer.hpp>

#include <string_view>

namespace aurex::lex {

inline constexpr base::usize LEXEME_SINGLE_BYTE_WIDTH = 1;

inline constexpr char LEXEME_NUL = '\0';
inline constexpr char LEXEME_DOT = '.';
inline constexpr char LEXEME_COLON = ':';
inline constexpr char LEXEME_PLUS = '+';
inline constexpr char LEXEME_MINUS = '-';
inline constexpr char LEXEME_STAR = '*';
inline constexpr char LEXEME_SLASH = '/';
inline constexpr char LEXEME_PERCENT = '%';
inline constexpr char LEXEME_AMP = '&';
inline constexpr char LEXEME_PIPE = '|';
inline constexpr char LEXEME_CARET = '^';
inline constexpr char LEXEME_TILDE = '~';
inline constexpr char LEXEME_BANG = '!';
inline constexpr char LEXEME_EQUAL = '=';
inline constexpr char LEXEME_LESS = '<';
inline constexpr char LEXEME_GREATER = '>';
inline constexpr char LEXEME_AT = '@';
inline constexpr char LEXEME_QUESTION = '?';
inline constexpr char LEXEME_L_PAREN = '(';
inline constexpr char LEXEME_R_PAREN = ')';
inline constexpr char LEXEME_L_BRACE = '{';
inline constexpr char LEXEME_R_BRACE = '}';
inline constexpr char LEXEME_L_BRACKET = '[';
inline constexpr char LEXEME_R_BRACKET = ']';
inline constexpr char LEXEME_COMMA = ',';
inline constexpr char LEXEME_SEMICOLON = ';';
inline constexpr char LEXEME_DOUBLE_QUOTE = '"';
inline constexpr char LEXEME_SINGLE_QUOTE = '\'';
inline constexpr char LEXEME_ESCAPE = '\\';
inline constexpr char LEXEME_LINE_FEED = '\n';

inline constexpr char LEXEME_DIGIT_SEPARATOR = '_';
inline constexpr char LEXEME_FLOAT_EXPONENT_LOWER = 'e';
inline constexpr char LEXEME_FLOAT_EXPONENT_UPPER = 'E';

inline constexpr std::string_view LEXEME_C_STRING_PREFIX = "c\"";
inline constexpr std::string_view LEXEME_BYTE_LITERAL_PREFIX = "b'";
inline constexpr std::string_view LEXEME_HEX_INTEGER_PREFIX_LOWER = "0x";
inline constexpr std::string_view LEXEME_HEX_INTEGER_PREFIX_UPPER = "0X";
inline constexpr std::string_view LEXEME_BINARY_INTEGER_PREFIX_LOWER = "0b";
inline constexpr std::string_view LEXEME_BINARY_INTEGER_PREFIX_UPPER = "0B";
inline constexpr std::string_view LEXEME_LINE_COMMENT_PREFIX = "//";
inline constexpr std::string_view LEXEME_BLOCK_COMMENT_PREFIX = "/*";
inline constexpr std::string_view LEXEME_BLOCK_COMMENT_SUFFIX = "*/";
inline constexpr std::string_view LEXEME_BYTE_LITERAL_RECOVERY_CHARS = "'\n";

inline constexpr std::string_view LEXEME_LEXING_FAILED_MESSAGE = "lexing failed";
inline constexpr std::string_view LEXEME_INVALID_CHARACTER_MESSAGE = "invalid character";
inline constexpr std::string_view LEXEME_UNTERMINATED_BLOCK_COMMENT_MESSAGE = "unterminated block comment";
inline constexpr std::string_view LEXEME_MALFORMED_DIGIT_SEPARATOR_MESSAGE = "digit separator must be between digits";
inline constexpr std::string_view LEXEME_UNTERMINATED_STRING_MESSAGE = "unterminated string literal";
inline constexpr std::string_view LEXEME_UNTERMINATED_C_STRING_MESSAGE = "unterminated c string literal";
inline constexpr std::string_view LEXEME_UNTERMINATED_BYTE_MESSAGE = "unterminated byte literal";
inline constexpr std::string_view LEXEME_OVERSIZED_BYTE_MESSAGE = "byte literal must contain one byte";
inline constexpr std::string_view LEXEME_INVALID_BINARY_DIGIT_MESSAGE = "invalid digit in binary literal";
inline constexpr std::string_view LEXEME_INVALID_HEXADECIMAL_DIGIT_MESSAGE = "invalid digit in hexadecimal literal";
inline constexpr std::string_view LEXEME_INTEGER_LITERAL_KIND = "integer";
inline constexpr std::string_view LEXEME_FLOAT_LITERAL_KIND = "float";
inline constexpr std::string_view LEXEME_FLOAT_EXPONENT_LITERAL_KIND = "float exponent";
inline constexpr std::string_view LEXEME_LITERAL_NO_DIGITS_SUFFIX = " literal has no digits";

} // namespace aurex::lex
