#pragma once

#include <string_view>

namespace aurex::base {

inline constexpr std::string_view STRING_LITERAL_C_STRING_INTERIOR_NUL =
    "c string literal cannot contain interior NUL";

inline constexpr std::string_view STRING_LITERAL_UNTERMINATED_ESCAPE =
    "unterminated escape sequence";

inline constexpr std::string_view STRING_LITERAL_UNICODE_BRACES =
    "unicode escape must use \\u{...}";

inline constexpr std::string_view STRING_LITERAL_UNTERMINATED_UNICODE_ESCAPE =
    "unterminated unicode escape";

inline constexpr std::string_view STRING_LITERAL_UNICODE_ESCAPE_NO_DIGITS =
    "unicode escape has no digits";

inline constexpr std::string_view STRING_LITERAL_UNICODE_ESCAPE_NON_HEX =
    "unicode escape contains non-hex digit";

inline constexpr std::string_view STRING_LITERAL_UNICODE_ESCAPE_INVALID_SCALAR =
    "unicode escape is not a valid Unicode scalar value";

inline constexpr std::string_view STRING_LITERAL_INVALID_ESCAPE =
    "invalid escape sequence";

inline constexpr std::string_view STRING_LITERAL_INVALID_UTF8 =
    "string literal must contain valid UTF-8";

inline constexpr std::string_view STRING_LITERAL_BYTE_STRING_NON_ASCII =
    "byte string literal can only contain ASCII bytes";

inline constexpr std::string_view STRING_LITERAL_BYTE_LITERAL_ONE_BYTE =
    "byte literal must contain one byte";

inline constexpr std::string_view STRING_LITERAL_CHAR_LITERAL_ONE_SCALAR =
    "char literal must contain one Unicode scalar value";

inline constexpr std::string_view STRING_LITERAL_CHAR_LITERAL_INVALID_UTF8 =
    "char literal must contain valid UTF-8";

} // namespace aurex::base
