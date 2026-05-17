#include <backend/llvm/llvm_backend_internal.hpp>

#include <aurex/base/string_literal.hpp>

#include <charconv>
#include <string_view>
#include <utility>

namespace aurex::backend {

namespace {

constexpr int LLVM_BACKEND_UTIL_DECIMAL_BASE = 10;
constexpr int LLVM_BACKEND_UTIL_HEX_BASE = 16;
constexpr int LLVM_BACKEND_UTIL_BINARY_BASE = 2;
constexpr std::size_t LLVM_BACKEND_UTIL_RADIX_PREFIX_LENGTH = 2U;
constexpr char LLVM_BACKEND_UTIL_ZERO = '0';
constexpr char LLVM_BACKEND_UTIL_HEX_LOWER = 'x';
constexpr char LLVM_BACKEND_UTIL_HEX_UPPER = 'X';
constexpr char LLVM_BACKEND_UTIL_BINARY_LOWER = 'b';
constexpr char LLVM_BACKEND_UTIL_BINARY_UPPER = 'B';
constexpr char LLVM_BACKEND_UTIL_DIGIT_SEPARATOR = '_';
constexpr char LLVM_BACKEND_UTIL_DECIMAL_FIRST = '0';
constexpr char LLVM_BACKEND_UTIL_DECIMAL_LAST = '9';
constexpr char LLVM_BACKEND_UTIL_HEX_LOWER_FIRST = 'a';
constexpr char LLVM_BACKEND_UTIL_HEX_LOWER_LAST = 'f';
constexpr char LLVM_BACKEND_UTIL_HEX_UPPER_FIRST = 'A';
constexpr char LLVM_BACKEND_UTIL_HEX_UPPER_LAST = 'F';
constexpr std::uint64_t LLVM_BACKEND_UTIL_HEX_DIGIT_OFFSET = 10U;
constexpr char LLVM_BACKEND_UTIL_FLOAT_DOT = '.';
constexpr char LLVM_BACKEND_UTIL_FLOAT_EXPONENT_LOWER = 'e';
constexpr char LLVM_BACKEND_UTIL_FLOAT_EXPONENT_UPPER = 'E';
constexpr char LLVM_BACKEND_UTIL_FLOAT_PLUS = '+';
constexpr char LLVM_BACKEND_UTIL_FLOAT_MINUS = '-';
constexpr std::string_view LLVM_BACKEND_UTIL_INTEGER_SUFFIX_I8 = "i8";
constexpr std::string_view LLVM_BACKEND_UTIL_INTEGER_SUFFIX_I16 = "i16";
constexpr std::string_view LLVM_BACKEND_UTIL_INTEGER_SUFFIX_I32 = "i32";
constexpr std::string_view LLVM_BACKEND_UTIL_INTEGER_SUFFIX_I64 = "i64";
constexpr std::string_view LLVM_BACKEND_UTIL_INTEGER_SUFFIX_ISIZE = "isize";
constexpr std::string_view LLVM_BACKEND_UTIL_INTEGER_SUFFIX_U8 = "u8";
constexpr std::string_view LLVM_BACKEND_UTIL_INTEGER_SUFFIX_U16 = "u16";
constexpr std::string_view LLVM_BACKEND_UTIL_INTEGER_SUFFIX_U32 = "u32";
constexpr std::string_view LLVM_BACKEND_UTIL_INTEGER_SUFFIX_U64 = "u64";
constexpr std::string_view LLVM_BACKEND_UTIL_INTEGER_SUFFIX_USIZE = "usize";
constexpr std::string_view LLVM_BACKEND_UTIL_FLOAT_SUFFIX_F32 = "f32";
constexpr std::string_view LLVM_BACKEND_UTIL_FLOAT_SUFFIX_F64 = "f64";

struct NumericParts {
    std::string_view digits;
    std::string_view suffix;
};

[[nodiscard]] bool integer_suffix_is_valid(const std::string_view suffix) noexcept {
    return suffix.empty() ||
           suffix == LLVM_BACKEND_UTIL_INTEGER_SUFFIX_I8 ||
           suffix == LLVM_BACKEND_UTIL_INTEGER_SUFFIX_I16 ||
           suffix == LLVM_BACKEND_UTIL_INTEGER_SUFFIX_I32 ||
           suffix == LLVM_BACKEND_UTIL_INTEGER_SUFFIX_I64 ||
           suffix == LLVM_BACKEND_UTIL_INTEGER_SUFFIX_ISIZE ||
           suffix == LLVM_BACKEND_UTIL_INTEGER_SUFFIX_U8 ||
           suffix == LLVM_BACKEND_UTIL_INTEGER_SUFFIX_U16 ||
           suffix == LLVM_BACKEND_UTIL_INTEGER_SUFFIX_U32 ||
           suffix == LLVM_BACKEND_UTIL_INTEGER_SUFFIX_U64 ||
           suffix == LLVM_BACKEND_UTIL_INTEGER_SUFFIX_USIZE;
}

[[nodiscard]] bool float_suffix_is_valid(const std::string_view suffix) noexcept {
    return suffix.empty() ||
           suffix == LLVM_BACKEND_UTIL_FLOAT_SUFFIX_F32 ||
           suffix == LLVM_BACKEND_UTIL_FLOAT_SUFFIX_F64;
}

[[nodiscard]] bool integer_digit_value(const char c, const int base, std::uint64_t& value) noexcept {
    if (c >= LLVM_BACKEND_UTIL_DECIMAL_FIRST && c <= LLVM_BACKEND_UTIL_DECIMAL_LAST) {
        value = static_cast<std::uint64_t>(c - LLVM_BACKEND_UTIL_DECIMAL_FIRST);
        return value < static_cast<std::uint64_t>(base);
    }
    if (c >= LLVM_BACKEND_UTIL_HEX_LOWER_FIRST && c <= LLVM_BACKEND_UTIL_HEX_LOWER_LAST) {
        value = static_cast<std::uint64_t>(LLVM_BACKEND_UTIL_HEX_DIGIT_OFFSET +
            static_cast<std::uint64_t>(c - LLVM_BACKEND_UTIL_HEX_LOWER_FIRST));
        return value < static_cast<std::uint64_t>(base);
    }
    if (c >= LLVM_BACKEND_UTIL_HEX_UPPER_FIRST && c <= LLVM_BACKEND_UTIL_HEX_UPPER_LAST) {
        value = static_cast<std::uint64_t>(LLVM_BACKEND_UTIL_HEX_DIGIT_OFFSET +
            static_cast<std::uint64_t>(c - LLVM_BACKEND_UTIL_HEX_UPPER_FIRST));
        return value < static_cast<std::uint64_t>(base);
    }
    return false;
}

[[nodiscard]] NumericParts split_integer_literal(const std::string_view text, int& base) noexcept {
    base = LLVM_BACKEND_UTIL_DECIMAL_BASE;
    std::size_t index = 0;
    if (text.size() > LLVM_BACKEND_UTIL_RADIX_PREFIX_LENGTH && text[0] == LLVM_BACKEND_UTIL_ZERO) {
        if (text[1] == LLVM_BACKEND_UTIL_HEX_LOWER || text[1] == LLVM_BACKEND_UTIL_HEX_UPPER) {
            base = LLVM_BACKEND_UTIL_HEX_BASE;
            index = LLVM_BACKEND_UTIL_RADIX_PREFIX_LENGTH;
        } else if (text[1] == LLVM_BACKEND_UTIL_BINARY_LOWER || text[1] == LLVM_BACKEND_UTIL_BINARY_UPPER) {
            base = LLVM_BACKEND_UTIL_BINARY_BASE;
            index = LLVM_BACKEND_UTIL_RADIX_PREFIX_LENGTH;
        }
    }
    while (index < text.size()) {
        if (text[index] == LLVM_BACKEND_UTIL_DIGIT_SEPARATOR) {
            ++index;
            continue;
        }
        std::uint64_t digit = 0;
        if (!integer_digit_value(text[index], base, digit)) {
            break;
        }
        ++index;
    }
    return NumericParts {text.substr(0, index), text.substr(index)};
}

[[nodiscard]] bool is_decimal_digit(const char c) noexcept {
    return c >= LLVM_BACKEND_UTIL_DECIMAL_FIRST && c <= LLVM_BACKEND_UTIL_DECIMAL_LAST;
}

[[nodiscard]] NumericParts split_float_literal(const std::string_view text) noexcept {
    std::size_t index = 0;
    const auto consume_digits = [&]() noexcept {
        while (index < text.size() &&
               (is_decimal_digit(text[index]) || text[index] == LLVM_BACKEND_UTIL_DIGIT_SEPARATOR)) {
            ++index;
        }
    };
    if (index < text.size() && text[index] == LLVM_BACKEND_UTIL_FLOAT_DOT) {
        ++index;
    }
    consume_digits();
    if (index < text.size() && text[index] == LLVM_BACKEND_UTIL_FLOAT_DOT) {
        ++index;
        consume_digits();
    }
    if (index < text.size() &&
        (text[index] == LLVM_BACKEND_UTIL_FLOAT_EXPONENT_LOWER ||
         text[index] == LLVM_BACKEND_UTIL_FLOAT_EXPONENT_UPPER)) {
        ++index;
        if (index < text.size() &&
            (text[index] == LLVM_BACKEND_UTIL_FLOAT_PLUS || text[index] == LLVM_BACKEND_UTIL_FLOAT_MINUS)) {
            ++index;
        }
        consume_digits();
    }
    return NumericParts {text.substr(0, index), text.substr(index)};
}

} // namespace

bool parse_u64(const std::string_view text, std::uint64_t& out) noexcept {
    int base = LLVM_BACKEND_UTIL_DECIMAL_BASE;
    const NumericParts parts = split_integer_literal(text, base);
    if (!integer_suffix_is_valid(parts.suffix)) {
        return false;
    }
    std::string digits;
    digits.reserve(parts.digits.size());
    for (std::size_t i = base == LLVM_BACKEND_UTIL_DECIMAL_BASE ? 0U : LLVM_BACKEND_UTIL_RADIX_PREFIX_LENGTH;
         i < parts.digits.size();
         ++i) {
        if (parts.digits[i] != LLVM_BACKEND_UTIL_DIGIT_SEPARATOR) {
            digits.push_back(parts.digits[i]);
        }
    }
    if (digits.empty()) {
        return false;
    }
    const char* begin = digits.data();
    const char* end = digits.data() + digits.size();
    const auto result = std::from_chars(begin, end, out, base);
    return result.ec == std::errc {} && result.ptr == end;
}

bool parse_f64(const std::string_view text, double& out) noexcept {
    const NumericParts parts = split_float_literal(text);
    if (!float_suffix_is_valid(parts.suffix)) {
        return false;
    }
    std::string digits;
    digits.reserve(parts.digits.size());
    for (const char c : parts.digits) {
        if (c != LLVM_BACKEND_UTIL_DIGIT_SEPARATOR) {
            digits.push_back(c);
        }
    }
    const char* begin = digits.data();
    const char* end = digits.data() + digits.size();
    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc {} && result.ptr == end;
}

std::string decode_string_literal(const std::string_view literal, const bool has_c_prefix) {
    base::StringLiteralDecode decoded = base::decode_string_literal(
        literal,
        has_c_prefix ? base::StringLiteralKind::c_string : base::StringLiteralKind::string
    );
    return std::move(decoded.decoded);
}

std::string decode_raw_string_literal(const std::string_view literal) {
    base::StringLiteralDecode decoded =
        base::decode_string_literal(literal, base::StringLiteralKind::raw_string);
    return std::move(decoded.decoded);
}

std::uint64_t parse_byte_literal(const std::string_view literal) {
    const base::ByteLiteralDecode decoded = base::decode_byte_literal(literal);
    return decoded.value;
}

std::uint64_t parse_char_literal(const std::string_view literal) {
    const base::CharLiteralDecode decoded = base::decode_char_literal(literal);
    return decoded.value;
}

} // namespace aurex::backend
