#pragma once

#include <aurex/base/integer.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::base {

enum class StringLiteralKind {
    string,
    c_string,
    raw_string,
    byte_string,
};

struct StringLiteralError {
    usize begin = 0;
    usize end = 0;
    std::string message;
};

struct StringLiteralDecode {
    std::string decoded;
    std::vector<StringLiteralError> errors;

    [[nodiscard]] bool ok() const noexcept {
        return errors.empty();
    }
};

struct ByteLiteralDecode {
    u8 value = 0;
    std::vector<StringLiteralError> errors;

    [[nodiscard]] bool ok() const noexcept {
        return errors.empty();
    }
};

struct CharLiteralDecode {
    u32 value = 0;
    std::vector<StringLiteralError> errors;

    [[nodiscard]] bool ok() const noexcept {
        return errors.empty();
    }
};

[[nodiscard]] bool is_valid_utf8(std::string_view text) noexcept;
[[nodiscard]] bool is_unicode_scalar(u32 value) noexcept;
[[nodiscard]] StringLiteralDecode decode_string_literal(std::string_view literal, StringLiteralKind kind);
[[nodiscard]] ByteLiteralDecode decode_byte_literal(std::string_view literal);
[[nodiscard]] CharLiteralDecode decode_char_literal(std::string_view literal);

} // namespace aurex::base
