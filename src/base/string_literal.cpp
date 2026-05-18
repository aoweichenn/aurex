#include <aurex/base/string_literal.hpp>
#include <aurex/base/string_literal_messages.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace aurex::base {

namespace {

constexpr char STRING_LITERAL_ASCII_ZERO = '0';
constexpr char STRING_LITERAL_ASCII_NINE = '9';
constexpr char STRING_LITERAL_ASCII_LOWER_A = 'a';
constexpr char STRING_LITERAL_ASCII_LOWER_F = 'f';
constexpr char STRING_LITERAL_ASCII_UPPER_A = 'A';
constexpr char STRING_LITERAL_ASCII_UPPER_F = 'F';
constexpr char STRING_LITERAL_C_PREFIX = 'c';
constexpr char STRING_LITERAL_RAW_PREFIX = 'r';
constexpr char STRING_LITERAL_BYTE_PREFIX = 'b';
constexpr char STRING_LITERAL_ESCAPE_NULL = '0';
constexpr char STRING_LITERAL_ESCAPE_NEWLINE = 'n';
constexpr char STRING_LITERAL_ESCAPE_CARRIAGE_RETURN = 'r';
constexpr char STRING_LITERAL_ESCAPE_TAB = 't';
constexpr char STRING_LITERAL_ESCAPE_BACKSLASH = '\\';
constexpr char STRING_LITERAL_ESCAPE_DOUBLE_QUOTE = '"';
constexpr char STRING_LITERAL_ESCAPE_SINGLE_QUOTE = '\'';
constexpr char STRING_LITERAL_ESCAPE_UNICODE = 'u';
constexpr char STRING_LITERAL_ESCAPE_LEFT_BRACE = '{';
constexpr char STRING_LITERAL_ESCAPE_RIGHT_BRACE = '}';
constexpr char STRING_LITERAL_NULL_CHAR = '\0';
constexpr char STRING_LITERAL_NEWLINE_CHAR = '\n';
constexpr char STRING_LITERAL_CARRIAGE_RETURN_CHAR = '\r';
constexpr char STRING_LITERAL_TAB_CHAR = '\t';
constexpr unsigned char STRING_LITERAL_ASCII_SINGLE_BYTE_MAX = 0x7FU;
constexpr usize STRING_LITERAL_C_STRING_PREFIX_LENGTH = 2U;
constexpr usize STRING_LITERAL_RAW_STRING_PREFIX_LENGTH = 2U;
constexpr usize STRING_LITERAL_BYTE_STRING_PREFIX_LENGTH = 2U;
constexpr usize STRING_LITERAL_BYTE_LITERAL_PREFIX_LENGTH = 2U;
constexpr u32 STRING_LITERAL_HEX_DIGIT_BASE = 16U;
constexpr u32 STRING_LITERAL_HEX_DIGIT_OFFSET = 10U;
constexpr u32 STRING_LITERAL_UTF8_CONTINUATION_SHIFT = 6U;
constexpr u32 STRING_LITERAL_UTF8_TWO_BYTE_SHIFT = 6U;
constexpr u32 STRING_LITERAL_UTF8_THREE_BYTE_FIRST_SHIFT = 12U;
constexpr u32 STRING_LITERAL_UTF8_THREE_BYTE_SECOND_SHIFT = 6U;
constexpr u32 STRING_LITERAL_UTF8_FOUR_BYTE_FIRST_SHIFT = 18U;
constexpr u32 STRING_LITERAL_UTF8_FOUR_BYTE_SECOND_SHIFT = 12U;
constexpr u32 STRING_LITERAL_UTF8_FOUR_BYTE_THIRD_SHIFT = 6U;

constexpr u32 STRING_LITERAL_UTF8_SINGLE_BYTE_MAX = 0x7FU;
constexpr u32 STRING_LITERAL_UTF8_TWO_BYTE_MAX = 0x7FFU;
constexpr u32 STRING_LITERAL_UTF8_TWO_BYTE_PREFIX_MASK = 0xE0U;
constexpr u32 STRING_LITERAL_UTF8_TWO_BYTE_PREFIX_VALUE = 0xC0U;
constexpr u32 STRING_LITERAL_UTF8_TWO_BYTE_PREFIX_PAYLOAD_MASK = 0x1FU;
constexpr u32 STRING_LITERAL_UTF8_THREE_BYTE_PREFIX_MASK = 0xF0U;
constexpr u32 STRING_LITERAL_UTF8_THREE_BYTE_PREFIX_VALUE = 0xE0U;
constexpr u32 STRING_LITERAL_UTF8_THREE_BYTE_PREFIX_PAYLOAD_MASK = 0x0FU;
constexpr u32 STRING_LITERAL_UTF8_FOUR_BYTE_PREFIX_MASK = 0xF8U;
constexpr u32 STRING_LITERAL_UTF8_FOUR_BYTE_PREFIX_VALUE = 0xF0U;
constexpr u32 STRING_LITERAL_UTF8_FOUR_BYTE_PREFIX_PAYLOAD_MASK = 0x07U;
constexpr u32 STRING_LITERAL_UTF8_CONTINUATION_MASK = 0xC0U;
constexpr u32 STRING_LITERAL_UTF8_CONTINUATION_VALUE = 0x80U;
constexpr u32 STRING_LITERAL_UTF8_CONTINUATION_PAYLOAD_MASK = 0x3FU;
constexpr u32 STRING_LITERAL_UTF8_TWO_BYTE_MIN = 0x80U;
constexpr u32 STRING_LITERAL_UTF8_THREE_BYTE_MIN = 0x800U;
constexpr u32 STRING_LITERAL_UTF8_FOUR_BYTE_MIN = 0x10000U;
constexpr u32 STRING_LITERAL_UTF8_THREE_BYTE_MAX = 0xFFFFU;
constexpr u32 STRING_LITERAL_UNICODE_MAX = 0x10FFFFU;
constexpr u32 STRING_LITERAL_UNICODE_SURROGATE_BEGIN = 0xD800U;
constexpr u32 STRING_LITERAL_UNICODE_SURROGATE_END = 0xDFFFU;
constexpr u32 STRING_LITERAL_UNICODE_NULL_VALUE = 0U;

[[nodiscard]] bool is_hex_digit(const char c) noexcept
{
    return (c >= STRING_LITERAL_ASCII_ZERO && c <= STRING_LITERAL_ASCII_NINE)
        || (c >= STRING_LITERAL_ASCII_LOWER_A && c <= STRING_LITERAL_ASCII_LOWER_F)
        || (c >= STRING_LITERAL_ASCII_UPPER_A && c <= STRING_LITERAL_ASCII_UPPER_F);
}

[[nodiscard]] u32 hex_value(const char c) noexcept
{
    if (c >= STRING_LITERAL_ASCII_ZERO && c <= STRING_LITERAL_ASCII_NINE) {
        return static_cast<u32>(c - STRING_LITERAL_ASCII_ZERO);
    }
    if (c >= STRING_LITERAL_ASCII_LOWER_A && c <= STRING_LITERAL_ASCII_LOWER_F) {
        return static_cast<u32>(c - STRING_LITERAL_ASCII_LOWER_A + STRING_LITERAL_HEX_DIGIT_OFFSET);
    }
    return static_cast<u32>(c - STRING_LITERAL_ASCII_UPPER_A + STRING_LITERAL_HEX_DIGIT_OFFSET);
}

template <typename DecodeResult>
void add_error(DecodeResult& result, const usize begin, const usize end, std::string message)
{
    result.errors.push_back(StringLiteralError{begin, std::max(begin + 1, end), std::move(message)});
}

void append_utf8(std::string& out, const u32 value)
{
    if (value <= STRING_LITERAL_UTF8_SINGLE_BYTE_MAX) {
        out.push_back(static_cast<char>(value));
        return;
    }
    if (value <= STRING_LITERAL_UTF8_TWO_BYTE_MAX) {
        out.push_back(static_cast<char>(
            STRING_LITERAL_UTF8_TWO_BYTE_PREFIX_VALUE | (value >> STRING_LITERAL_UTF8_TWO_BYTE_SHIFT)));
        out.push_back(static_cast<char>(
            STRING_LITERAL_UTF8_CONTINUATION_VALUE | (value & STRING_LITERAL_UTF8_CONTINUATION_PAYLOAD_MASK)));
        return;
    }
    if (value <= STRING_LITERAL_UTF8_THREE_BYTE_MAX) {
        out.push_back(static_cast<char>(STRING_LITERAL_UTF8_THREE_BYTE_PREFIX_VALUE
            | ((value >> STRING_LITERAL_UTF8_THREE_BYTE_FIRST_SHIFT)
                & STRING_LITERAL_UTF8_THREE_BYTE_PREFIX_PAYLOAD_MASK)));
        out.push_back(static_cast<char>(STRING_LITERAL_UTF8_CONTINUATION_VALUE
            | ((value >> STRING_LITERAL_UTF8_THREE_BYTE_SECOND_SHIFT)
                & STRING_LITERAL_UTF8_CONTINUATION_PAYLOAD_MASK)));
        out.push_back(static_cast<char>(
            STRING_LITERAL_UTF8_CONTINUATION_VALUE | (value & STRING_LITERAL_UTF8_CONTINUATION_PAYLOAD_MASK)));
        return;
    }
    out.push_back(static_cast<char>(STRING_LITERAL_UTF8_FOUR_BYTE_PREFIX_VALUE
        | ((value >> STRING_LITERAL_UTF8_FOUR_BYTE_FIRST_SHIFT) & STRING_LITERAL_UTF8_FOUR_BYTE_PREFIX_PAYLOAD_MASK)));
    out.push_back(static_cast<char>(STRING_LITERAL_UTF8_CONTINUATION_VALUE
        | ((value >> STRING_LITERAL_UTF8_FOUR_BYTE_SECOND_SHIFT) & STRING_LITERAL_UTF8_CONTINUATION_PAYLOAD_MASK)));
    out.push_back(static_cast<char>(STRING_LITERAL_UTF8_CONTINUATION_VALUE
        | ((value >> STRING_LITERAL_UTF8_FOUR_BYTE_THIRD_SHIFT) & STRING_LITERAL_UTF8_CONTINUATION_PAYLOAD_MASK)));
    out.push_back(static_cast<char>(
        STRING_LITERAL_UTF8_CONTINUATION_VALUE | (value & STRING_LITERAL_UTF8_CONTINUATION_PAYLOAD_MASK)));
}

[[nodiscard]] bool is_continuation(const unsigned char byte) noexcept
{
    return (byte & STRING_LITERAL_UTF8_CONTINUATION_MASK) == STRING_LITERAL_UTF8_CONTINUATION_VALUE;
}

[[nodiscard]] bool decode_utf8_scalar_at(const std::string_view text, usize& index, u32& value) noexcept
{
    if (index >= text.size()) {
        return false;
    }

    const auto first = static_cast<unsigned char>(text[index]);
    if (first <= STRING_LITERAL_UTF8_SINGLE_BYTE_MAX) {
        value = first;
        ++index;
        return true;
    }

    usize width = 0;
    u32 min_value = 0;
    if ((first & STRING_LITERAL_UTF8_TWO_BYTE_PREFIX_MASK) == STRING_LITERAL_UTF8_TWO_BYTE_PREFIX_VALUE) {
        value = first & STRING_LITERAL_UTF8_TWO_BYTE_PREFIX_PAYLOAD_MASK;
        width = 2;
        min_value = STRING_LITERAL_UTF8_TWO_BYTE_MIN;
    } else if ((first & STRING_LITERAL_UTF8_THREE_BYTE_PREFIX_MASK) == STRING_LITERAL_UTF8_THREE_BYTE_PREFIX_VALUE) {
        value = first & STRING_LITERAL_UTF8_THREE_BYTE_PREFIX_PAYLOAD_MASK;
        width = 3;
        min_value = STRING_LITERAL_UTF8_THREE_BYTE_MIN;
    } else if ((first & STRING_LITERAL_UTF8_FOUR_BYTE_PREFIX_MASK) == STRING_LITERAL_UTF8_FOUR_BYTE_PREFIX_VALUE) {
        value = first & STRING_LITERAL_UTF8_FOUR_BYTE_PREFIX_PAYLOAD_MASK;
        width = 4;
        min_value = STRING_LITERAL_UTF8_FOUR_BYTE_MIN;
    } else {
        return false;
    }

    if (index + width > text.size()) {
        return false;
    }
    for (usize offset = 1; offset < width; ++offset) {
        const auto next = static_cast<unsigned char>(text[index + offset]);
        if (!is_continuation(next)) {
            return false;
        }
        value = (value << STRING_LITERAL_UTF8_CONTINUATION_SHIFT)
            | static_cast<u32>(next & STRING_LITERAL_UTF8_CONTINUATION_PAYLOAD_MASK);
    }
    if (value < min_value || !is_unicode_scalar(value)) {
        return false;
    }
    index += width;
    return true;
}

[[nodiscard]] bool is_non_ascii_byte(const char c) noexcept
{
    return static_cast<unsigned char>(c) > STRING_LITERAL_ASCII_SINGLE_BYTE_MAX;
}

[[nodiscard]] usize quoted_content_begin(const std::string_view literal, const StringLiteralKind kind) noexcept
{
    if (kind == StringLiteralKind::c_string && literal.size() >= STRING_LITERAL_C_STRING_PREFIX_LENGTH
        && literal[0] == STRING_LITERAL_C_PREFIX && literal[1] == STRING_LITERAL_ESCAPE_DOUBLE_QUOTE) {
        return STRING_LITERAL_C_STRING_PREFIX_LENGTH;
    }
    if (kind == StringLiteralKind::raw_string && literal.size() >= STRING_LITERAL_RAW_STRING_PREFIX_LENGTH
        && literal[0] == STRING_LITERAL_RAW_PREFIX && literal[1] == STRING_LITERAL_ESCAPE_DOUBLE_QUOTE) {
        return STRING_LITERAL_RAW_STRING_PREFIX_LENGTH;
    }
    if (kind == StringLiteralKind::byte_string && literal.size() >= STRING_LITERAL_BYTE_STRING_PREFIX_LENGTH
        && literal[0] == STRING_LITERAL_BYTE_PREFIX && literal[1] == STRING_LITERAL_ESCAPE_DOUBLE_QUOTE) {
        return STRING_LITERAL_BYTE_STRING_PREFIX_LENGTH;
    }
    if (!literal.empty() && literal.front() == STRING_LITERAL_ESCAPE_DOUBLE_QUOTE) {
        return 1;
    }
    return 0;
}

[[nodiscard]] usize quoted_content_end(const std::string_view literal, const usize content_begin) noexcept
{
    usize content_end = literal.size();
    if (content_end > content_begin && literal[content_end - 1] == STRING_LITERAL_ESCAPE_DOUBLE_QUOTE) {
        --content_end;
    }
    return content_end;
}

[[nodiscard]] bool decode_simple_escaped_byte(const char escaped, u8& value) noexcept
{
    switch (escaped) {
        case STRING_LITERAL_ESCAPE_NULL:
            value = 0;
            return true;
        case STRING_LITERAL_ESCAPE_NEWLINE:
            value = static_cast<u8>(STRING_LITERAL_NEWLINE_CHAR);
            return true;
        case STRING_LITERAL_ESCAPE_CARRIAGE_RETURN:
            value = static_cast<u8>(STRING_LITERAL_CARRIAGE_RETURN_CHAR);
            return true;
        case STRING_LITERAL_ESCAPE_TAB:
            value = static_cast<u8>(STRING_LITERAL_TAB_CHAR);
            return true;
        case STRING_LITERAL_ESCAPE_BACKSLASH:
            value = static_cast<u8>(STRING_LITERAL_ESCAPE_BACKSLASH);
            return true;
        case STRING_LITERAL_ESCAPE_SINGLE_QUOTE:
            value = static_cast<u8>(STRING_LITERAL_ESCAPE_SINGLE_QUOTE);
            return true;
        default:
            return false;
    }
}

[[nodiscard]] bool decode_simple_escaped_scalar(const char escaped, u32& value) noexcept
{
    u8 byte = 0;
    if (decode_simple_escaped_byte(escaped, byte)) {
        value = byte;
        return true;
    }
    if (escaped == STRING_LITERAL_ESCAPE_DOUBLE_QUOTE) {
        value = static_cast<u32>(STRING_LITERAL_ESCAPE_DOUBLE_QUOTE);
        return true;
    }
    return false;
}

} // namespace

bool is_unicode_scalar(const u32 value) noexcept
{
    return value <= STRING_LITERAL_UNICODE_MAX
        && !(value >= STRING_LITERAL_UNICODE_SURROGATE_BEGIN && value <= STRING_LITERAL_UNICODE_SURROGATE_END);
}

bool is_valid_utf8(const std::string_view text) noexcept
{
    usize i = 0;
    while (i < text.size()) {
        const auto first = static_cast<unsigned char>(text[i]);
        if (first <= STRING_LITERAL_UTF8_SINGLE_BYTE_MAX) {
            ++i;
            continue;
        }

        u32 value = 0;
        usize width = 0;
        u32 min_value = 0;
        if ((first & STRING_LITERAL_UTF8_TWO_BYTE_PREFIX_MASK) == STRING_LITERAL_UTF8_TWO_BYTE_PREFIX_VALUE) {
            value = first & STRING_LITERAL_UTF8_TWO_BYTE_PREFIX_PAYLOAD_MASK;
            width = 2;
            min_value = STRING_LITERAL_UTF8_TWO_BYTE_MIN;
        } else if ((first & STRING_LITERAL_UTF8_THREE_BYTE_PREFIX_MASK)
            == STRING_LITERAL_UTF8_THREE_BYTE_PREFIX_VALUE) {
            value = first & STRING_LITERAL_UTF8_THREE_BYTE_PREFIX_PAYLOAD_MASK;
            width = 3;
            min_value = STRING_LITERAL_UTF8_THREE_BYTE_MIN;
        } else if ((first & STRING_LITERAL_UTF8_FOUR_BYTE_PREFIX_MASK) == STRING_LITERAL_UTF8_FOUR_BYTE_PREFIX_VALUE) {
            value = first & STRING_LITERAL_UTF8_FOUR_BYTE_PREFIX_PAYLOAD_MASK;
            width = 4;
            min_value = STRING_LITERAL_UTF8_FOUR_BYTE_MIN;
        } else {
            return false;
        }

        if (i + width > text.size()) {
            return false;
        }
        for (usize j = 1; j < width; ++j) {
            const auto next = static_cast<unsigned char>(text[i + j]);
            if (!is_continuation(next)) {
                return false;
            }
            value = (value << STRING_LITERAL_UTF8_CONTINUATION_SHIFT)
                | static_cast<u32>(next & STRING_LITERAL_UTF8_CONTINUATION_PAYLOAD_MASK);
        }
        if (value < min_value || !is_unicode_scalar(value)) {
            return false;
        }
        i += width;
    }
    return true;
}

StringLiteralDecode decode_string_literal(const std::string_view literal, const StringLiteralKind kind)
{
    StringLiteralDecode result;
    const bool is_c_string = kind == StringLiteralKind::c_string;
    const bool is_raw_string = kind == StringLiteralKind::raw_string;
    const bool is_byte_string = kind == StringLiteralKind::byte_string;

    const usize content_begin = quoted_content_begin(literal, kind);
    const usize content_end = quoted_content_end(literal, content_begin);

    result.decoded.reserve(content_end - content_begin);
    if (is_raw_string) {
        result.decoded.assign(literal.substr(content_begin, content_end - content_begin));
        if (!is_valid_utf8(result.decoded)) {
            add_error(result, content_begin, content_end, std::string(STRING_LITERAL_INVALID_UTF8));
        }
        return result;
    }

    usize i = content_begin;
    while (i < content_end) {
        const char c = literal[i];
        if (c != STRING_LITERAL_ESCAPE_BACKSLASH) {
            if (is_c_string && c == STRING_LITERAL_NULL_CHAR) {
                add_error(result, i, i + 1, std::string(STRING_LITERAL_C_STRING_INTERIOR_NUL));
            }
            if (is_byte_string && is_non_ascii_byte(c)) {
                add_error(result, i, i + 1, std::string(STRING_LITERAL_BYTE_STRING_NON_ASCII));
            }
            result.decoded.push_back(c);
            ++i;
            continue;
        }

        const usize escape_begin = i;
        ++i;
        if (i >= content_end) {
            add_error(result, escape_begin, content_end, std::string(STRING_LITERAL_UNTERMINATED_ESCAPE));
            break;
        }

        const char escaped = literal[i];
        ++i;
        switch (escaped) {
            case STRING_LITERAL_ESCAPE_NULL:
                if (is_c_string) {
                    add_error(result, escape_begin, i, std::string(STRING_LITERAL_C_STRING_INTERIOR_NUL));
                }
                result.decoded.push_back(STRING_LITERAL_NULL_CHAR);
                break;
            case STRING_LITERAL_ESCAPE_NEWLINE:
                result.decoded.push_back(STRING_LITERAL_NEWLINE_CHAR);
                break;
            case STRING_LITERAL_ESCAPE_CARRIAGE_RETURN:
                result.decoded.push_back(STRING_LITERAL_CARRIAGE_RETURN_CHAR);
                break;
            case STRING_LITERAL_ESCAPE_TAB:
                result.decoded.push_back(STRING_LITERAL_TAB_CHAR);
                break;
            case STRING_LITERAL_ESCAPE_BACKSLASH:
                result.decoded.push_back(STRING_LITERAL_ESCAPE_BACKSLASH);
                break;
            case STRING_LITERAL_ESCAPE_DOUBLE_QUOTE:
                result.decoded.push_back(STRING_LITERAL_ESCAPE_DOUBLE_QUOTE);
                break;
            case STRING_LITERAL_ESCAPE_UNICODE: {
                if (is_byte_string) {
                    add_error(result, escape_begin, i, std::string(STRING_LITERAL_INVALID_ESCAPE));
                    result.decoded.push_back(STRING_LITERAL_ESCAPE_UNICODE);
                    break;
                }
                if (i >= content_end || literal[i] != STRING_LITERAL_ESCAPE_LEFT_BRACE) {
                    add_error(result, escape_begin, i, std::string(STRING_LITERAL_UNICODE_BRACES));
                    result.decoded.push_back(STRING_LITERAL_ESCAPE_UNICODE);
                    break;
                }
                ++i;
                const usize digit_begin = i;
                u32 value = 0;
                bool overflow = false;
                bool invalid_digit = false;
                while (i < content_end && literal[i] != '}') {
                    if (!is_hex_digit(literal[i])) {
                        invalid_digit = true;
                        ++i;
                        continue;
                    }
                    const u32 digit = hex_value(literal[i]);
                    if (value > ((STRING_LITERAL_UNICODE_MAX - digit) / STRING_LITERAL_HEX_DIGIT_BASE)) {
                        overflow = true;
                    } else {
                        value = (value * STRING_LITERAL_HEX_DIGIT_BASE) + digit;
                    }
                    ++i;
                }
                if (i >= content_end || literal[i] != STRING_LITERAL_ESCAPE_RIGHT_BRACE) {
                    add_error(
                        result, escape_begin, content_end, std::string(STRING_LITERAL_UNTERMINATED_UNICODE_ESCAPE));
                    break;
                }
                ++i;
                if (digit_begin == i - 1) {
                    add_error(result, escape_begin, i, std::string(STRING_LITERAL_UNICODE_ESCAPE_NO_DIGITS));
                    break;
                }
                if (invalid_digit) {
                    add_error(result, escape_begin, i, std::string(STRING_LITERAL_UNICODE_ESCAPE_NON_HEX));
                    break;
                }
                if (overflow || !is_unicode_scalar(value)) {
                    add_error(result, escape_begin, i, std::string(STRING_LITERAL_UNICODE_ESCAPE_INVALID_SCALAR));
                    break;
                }
                if (is_c_string && value == STRING_LITERAL_UNICODE_NULL_VALUE) {
                    add_error(result, escape_begin, i, std::string(STRING_LITERAL_C_STRING_INTERIOR_NUL));
                }
                append_utf8(result.decoded, value);
                break;
            }
            default:
                add_error(result, escape_begin, i, std::string(STRING_LITERAL_INVALID_ESCAPE));
                result.decoded.push_back(escaped);
                break;
        }
    }

    if (kind == StringLiteralKind::string && !is_valid_utf8(result.decoded)) {
        add_error(result, content_begin, content_end, std::string(STRING_LITERAL_INVALID_UTF8));
    }

    return result;
}

ByteLiteralDecode decode_byte_literal(const std::string_view literal)
{
    ByteLiteralDecode result;
    usize content_begin = 0;
    if (literal.size() >= STRING_LITERAL_BYTE_LITERAL_PREFIX_LENGTH && literal[0] == STRING_LITERAL_BYTE_PREFIX
        && literal[1] == STRING_LITERAL_ESCAPE_SINGLE_QUOTE) {
        content_begin = STRING_LITERAL_BYTE_LITERAL_PREFIX_LENGTH;
    } else if (!literal.empty() && literal.front() == STRING_LITERAL_ESCAPE_SINGLE_QUOTE) {
        content_begin = 1;
    }

    usize content_end = literal.size();
    if (content_end > content_begin && literal[content_end - 1] == STRING_LITERAL_ESCAPE_SINGLE_QUOTE) {
        --content_end;
    }

    const std::string_view content = literal.substr(content_begin, content_end - content_begin);
    if (content.empty()) {
        add_error(result, content_begin, content_end, std::string(STRING_LITERAL_BYTE_LITERAL_ONE_BYTE));
        return result;
    }

    if (content.front() == STRING_LITERAL_ESCAPE_BACKSLASH) {
        if (content.size() < 2) {
            add_error(result, content_begin, content_end, std::string(STRING_LITERAL_UNTERMINATED_ESCAPE));
            return result;
        }
        u8 value = 0;
        if (!decode_simple_escaped_byte(content[1], value)) {
            add_error(result, content_begin, content_begin + std::min<usize>(content.size(), 2U),
                std::string(STRING_LITERAL_INVALID_ESCAPE));
            return result;
        }
        if (content.size() != 2) {
            add_error(result, content_begin, content_end, std::string(STRING_LITERAL_BYTE_LITERAL_ONE_BYTE));
            return result;
        }
        result.value = value;
        return result;
    }

    if (content.size() != 1 || is_non_ascii_byte(content.front())) {
        add_error(result, content_begin, content_end, std::string(STRING_LITERAL_BYTE_LITERAL_ONE_BYTE));
        return result;
    }
    result.value = static_cast<u8>(content.front());
    return result;
}

CharLiteralDecode decode_char_literal(const std::string_view literal)
{
    CharLiteralDecode result;
    usize content_begin = 0;
    if (!literal.empty() && literal.front() == STRING_LITERAL_ESCAPE_SINGLE_QUOTE) {
        content_begin = 1;
    }

    usize content_end = literal.size();
    if (content_end > content_begin && literal[content_end - 1] == STRING_LITERAL_ESCAPE_SINGLE_QUOTE) {
        --content_end;
    }

    const std::string_view content = literal.substr(content_begin, content_end - content_begin);
    if (content.empty()) {
        add_error(result, content_begin, content_end, std::string(STRING_LITERAL_CHAR_LITERAL_ONE_SCALAR));
        return result;
    }

    std::string decoded;
    if (content.front() == STRING_LITERAL_ESCAPE_BACKSLASH) {
        if (content.size() < 2) {
            add_error(result, content_begin, content_end, std::string(STRING_LITERAL_UNTERMINATED_ESCAPE));
            return result;
        }
        u32 scalar = 0;
        if (decode_simple_escaped_scalar(content[1], scalar)) {
            if (content.size() != 2) {
                add_error(result, content_begin, content_end, std::string(STRING_LITERAL_CHAR_LITERAL_ONE_SCALAR));
                return result;
            }
            result.value = scalar;
            return result;
        }

        std::string fake;
        fake.reserve(content.size() + 2U);
        fake.push_back(STRING_LITERAL_ESCAPE_DOUBLE_QUOTE);
        fake.append(content);
        fake.push_back(STRING_LITERAL_ESCAPE_DOUBLE_QUOTE);
        StringLiteralDecode string_decoded = decode_string_literal(fake, StringLiteralKind::string);
        for (StringLiteralError error : string_decoded.errors) {
            result.errors.push_back(std::move(error));
        }
        decoded = std::move(string_decoded.decoded);
    } else {
        decoded.assign(content);
    }

    usize index = 0;
    u32 scalar = 0;
    if (!decode_utf8_scalar_at(decoded, index, scalar)) {
        add_error(result, content_begin, content_end, std::string(STRING_LITERAL_CHAR_LITERAL_INVALID_UTF8));
        return result;
    }
    if (index != decoded.size()) {
        add_error(result, content_begin, content_end, std::string(STRING_LITERAL_CHAR_LITERAL_ONE_SCALAR));
        return result;
    }
    result.value = scalar;
    return result;
}

} // namespace aurex::base
