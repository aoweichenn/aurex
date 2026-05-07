#include "aurex/base/string_literal.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace aurex::base {

namespace {

[[nodiscard]] bool is_hex_digit(const char c) noexcept {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

[[nodiscard]] u32 hex_value(const char c) noexcept {
    if (c >= '0' && c <= '9') {
        return static_cast<u32>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<u32>(c - 'a' + 10);
    }
    return static_cast<u32>(c - 'A' + 10);
}

void add_error(StringLiteralDecode& result, const usize begin, const usize end, std::string message) {
    result.errors.push_back(StringLiteralError {begin, std::max(begin + 1, end), std::move(message)});
}

void append_utf8(std::string& out, const u32 value) {
    if (value <= 0x7FU) {
        out.push_back(static_cast<char>(value));
        return;
    }
    if (value <= 0x7FFU) {
        out.push_back(static_cast<char>(0xC0U | (value >> 6U)));
        out.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        return;
    }
    if (value <= 0xFFFFU) {
        out.push_back(static_cast<char>(0xE0U | (value >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        return;
    }
    out.push_back(static_cast<char>(0xF0U | (value >> 18U)));
    out.push_back(static_cast<char>(0x80U | ((value >> 12U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
}

[[nodiscard]] bool is_continuation(const unsigned char byte) noexcept {
    return (byte & 0xC0U) == 0x80U;
}

} // namespace

bool is_unicode_scalar(const u32 value) noexcept {
    return value <= 0x10FFFFU && !(value >= 0xD800U && value <= 0xDFFFU);
}

bool is_valid_utf8(const std::string_view text) noexcept {
    usize i = 0;
    while (i < text.size()) {
        const auto first = static_cast<unsigned char>(text[i]);
        if (first <= 0x7FU) {
            ++i;
            continue;
        }

        u32 value = 0;
        usize width = 0;
        u32 min_value = 0;
        if ((first & 0xE0U) == 0xC0U) {
            value = first & 0x1FU;
            width = 2;
            min_value = 0x80U;
        } else if ((first & 0xF0U) == 0xE0U) {
            value = first & 0x0FU;
            width = 3;
            min_value = 0x800U;
        } else if ((first & 0xF8U) == 0xF0U) {
            value = first & 0x07U;
            width = 4;
            min_value = 0x10000U;
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
            value = (value << 6U) | static_cast<u32>(next & 0x3FU);
        }
        if (value < min_value || !is_unicode_scalar(value)) {
            return false;
        }
        i += width;
    }
    return true;
}

StringLiteralDecode decode_string_literal(const std::string_view literal, const StringLiteralKind kind) {
    StringLiteralDecode result;
    const bool is_c_string = kind == StringLiteralKind::c_string;

    usize content_begin = 0;
    if (is_c_string && literal.size() >= 2 && literal[0] == 'c' && literal[1] == '"') {
        content_begin = 2;
    } else if (!literal.empty() && literal.front() == '"') {
        content_begin = 1;
    }

    usize content_end = literal.size();
    if (content_end > content_begin && literal[content_end - 1] == '"') {
        --content_end;
    }

    result.decoded.reserve(content_end - content_begin);
    usize i = content_begin;
    while (i < content_end) {
        const char c = literal[i];
        if (c != '\\') {
            if (is_c_string && c == '\0') {
                add_error(result, i, i + 1, "c string literal cannot contain interior NUL");
            }
            result.decoded.push_back(c);
            ++i;
            continue;
        }

        const usize escape_begin = i;
        ++i;
        if (i >= content_end) {
            add_error(result, escape_begin, content_end, "unterminated escape sequence");
            break;
        }

        const char escaped = literal[i];
        ++i;
        switch (escaped) {
        case '0':
            if (is_c_string) {
                add_error(result, escape_begin, i, "c string literal cannot contain interior NUL");
            }
            result.decoded.push_back('\0');
            break;
        case 'n':
            result.decoded.push_back('\n');
            break;
        case 'r':
            result.decoded.push_back('\r');
            break;
        case 't':
            result.decoded.push_back('\t');
            break;
        case '\\':
            result.decoded.push_back('\\');
            break;
        case '"':
            result.decoded.push_back('"');
            break;
        case 'u': {
            if (i >= content_end || literal[i] != '{') {
                add_error(result, escape_begin, i, "unicode escape must use \\u{...}");
                result.decoded.push_back('u');
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
                if (value > ((0x10FFFFU - digit) / 16U)) {
                    overflow = true;
                } else {
                    value = (value * 16U) + digit;
                }
                ++i;
            }
            if (i >= content_end || literal[i] != '}') {
                add_error(result, escape_begin, content_end, "unterminated unicode escape");
                break;
            }
            ++i;
            if (digit_begin == i - 1) {
                add_error(result, escape_begin, i, "unicode escape has no digits");
                break;
            }
            if (invalid_digit) {
                add_error(result, escape_begin, i, "unicode escape contains non-hex digit");
                break;
            }
            if (overflow || !is_unicode_scalar(value)) {
                add_error(result, escape_begin, i, "unicode escape is not a valid Unicode scalar value");
                break;
            }
            if (is_c_string && value == 0) {
                add_error(result, escape_begin, i, "c string literal cannot contain interior NUL");
            }
            append_utf8(result.decoded, value);
            break;
        }
        default:
            add_error(result, escape_begin, i, "invalid escape sequence");
            result.decoded.push_back(escaped);
            break;
        }
    }

    if (kind == StringLiteralKind::string && !is_valid_utf8(result.decoded)) {
        add_error(result, content_begin, content_end, "string literal must contain valid UTF-8");
    }

    return result;
}

} // namespace aurex::base
