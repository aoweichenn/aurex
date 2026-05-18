#include <aurex/backend/llvm_backend.hpp>
#include <aurex/base/string_literal.hpp>

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/support/ir_test_helpers.hpp>

namespace aurex::backend {
[[nodiscard]] bool parse_u64(std::string_view text, std::uint64_t& out) noexcept;
[[nodiscard]] bool parse_f64(std::string_view text, double& out) noexcept;
[[nodiscard]] std::string decode_string_literal(std::string_view literal, bool has_c_prefix);
[[nodiscard]] std::string decode_raw_string_literal(std::string_view literal);
[[nodiscard]] std::uint64_t parse_byte_literal(std::string_view literal);
[[nodiscard]] std::uint64_t parse_char_literal(std::string_view literal);
} // namespace aurex::backend

namespace aurex::test {
namespace {

using base::ErrorCode;
using namespace irtest;

} // namespace

TEST(CoreUnit, LlvmBackendUtilityHelpersCoverLiteralVariants)
{
    std::uint64_t parsed = 0;
    EXPECT_TRUE(backend::parse_u64("0x2a", parsed));
    EXPECT_EQ(parsed, 42U);
    EXPECT_TRUE(backend::parse_u64("0b1010", parsed));
    EXPECT_EQ(parsed, 10U);
    EXPECT_TRUE(backend::parse_u64("42usize", parsed));
    EXPECT_EQ(parsed, 42U);
    EXPECT_TRUE(backend::parse_u64("0xffu8", parsed));
    EXPECT_EQ(parsed, 255U);
    EXPECT_FALSE(backend::parse_u64("1f32", parsed));
    EXPECT_FALSE(backend::parse_u64("", parsed));
    EXPECT_FALSE(backend::parse_u64("0x", parsed));
    EXPECT_FALSE(backend::parse_u64("not-a-number", parsed));

    double parsed_float = 0.0;
    EXPECT_TRUE(backend::parse_f64(".5f32", parsed_float));
    EXPECT_DOUBLE_EQ(parsed_float, 0.5);
    EXPECT_TRUE(backend::parse_f64("1.", parsed_float));
    EXPECT_DOUBLE_EQ(parsed_float, 1.0);
    EXPECT_FALSE(backend::parse_f64("1.0u8", parsed_float));

    const std::string decoded = backend::decode_string_literal("\"\\0\\r\\t\\\\\\\"x\"", false);
    ASSERT_EQ(decoded.size(), 6U);
    EXPECT_EQ(decoded[0], '\0');
    EXPECT_EQ(decoded[1], '\r');
    EXPECT_EQ(decoded[2], '\t');
    EXPECT_EQ(decoded[3], '\\');
    EXPECT_EQ(decoded[4], '"');
    EXPECT_EQ(decoded[5], 'x');
    EXPECT_EQ(backend::decode_string_literal("c\"raw\"", true), "raw");
    EXPECT_EQ(backend::decode_raw_string_literal("r\"C:\\tmp\\a\""), "C:\\tmp\\a");
    const std::string omega = backend::decode_string_literal("\"\\u{03A9}\"", false);
    ASSERT_EQ(omega.size(), 2U);
    EXPECT_EQ(static_cast<unsigned char>(omega[0]), 0xCEU);
    EXPECT_EQ(static_cast<unsigned char>(omega[1]), 0xA9U);

    const base::StringLiteralDecode bad_escape =
        base::decode_string_literal("\"\\q\"", base::StringLiteralKind::string);
    ASSERT_FALSE(bad_escape.ok());
    ASSERT_FALSE(bad_escape.errors.empty());
    expect_contains(bad_escape.errors.front().message, "invalid escape sequence");

    const base::StringLiteralDecode surrogate =
        base::decode_string_literal("\"\\u{D800}\"", base::StringLiteralKind::string);
    ASSERT_FALSE(surrogate.ok());
    ASSERT_FALSE(surrogate.errors.empty());
    expect_contains(surrogate.errors.front().message, "not a valid Unicode scalar");

    std::string invalid_utf8 = "\"";
    invalid_utf8.push_back(static_cast<char>(0xC3));
    invalid_utf8.push_back('(');
    invalid_utf8.push_back('"');
    const base::StringLiteralDecode bad_utf8 =
        base::decode_string_literal(invalid_utf8, base::StringLiteralKind::string);
    ASSERT_FALSE(bad_utf8.ok());
    expect_contains(bad_utf8.errors.back().message, "valid UTF-8");

    const base::StringLiteralDecode c_nul =
        base::decode_string_literal("c\"a\\0b\"", base::StringLiteralKind::c_string);
    ASSERT_FALSE(c_nul.ok());
    expect_contains(c_nul.errors.front().message, "interior NUL");

    EXPECT_EQ(backend::parse_byte_literal("b'\\\\'"), static_cast<std::uint64_t>('\\'));
    EXPECT_EQ(backend::parse_byte_literal("b'\\''"), static_cast<std::uint64_t>('\''));
    EXPECT_EQ(backend::parse_byte_literal("b'\\0'"), 0U);
    EXPECT_EQ(backend::parse_byte_literal("b'\\r'"), static_cast<std::uint64_t>('\r'));
    EXPECT_EQ(backend::parse_byte_literal("b'\\t'"), static_cast<std::uint64_t>('\t'));
    EXPECT_EQ(backend::parse_byte_literal("b'\\x'"), 0U);
    EXPECT_EQ(backend::parse_byte_literal("b''"), 0U);
    EXPECT_EQ(backend::parse_char_literal("'λ'"), 0x03BBU);
    EXPECT_EQ(backend::parse_char_literal("'\\u{1F600}'"), 0x1F600U);
}

TEST(CoreUnit, BaseStringLiteralHelpersCoverUtf8AndEscapes)
{
    constexpr base::u32 BASE_STRING_LITERAL_UNICODE_NULL_TEST_VALUE = 0U;
    constexpr base::u32 BASE_STRING_LITERAL_UNICODE_MAX_TEST_VALUE = 0x10FFFFU;
    constexpr base::u32 BASE_STRING_LITERAL_UNICODE_SURROGATE_TEST_VALUE = 0xD800U;
    constexpr base::u32 BASE_STRING_LITERAL_UNICODE_ABOVE_MAX_TEST_VALUE = 0x110000U;
    constexpr base::usize BASE_STRING_LITERAL_UTF8_TWO_BYTE_WIDTH = 2;
    constexpr base::usize BASE_STRING_LITERAL_UTF8_THREE_BYTE_WIDTH = 3;
    constexpr base::usize BASE_STRING_LITERAL_UTF8_FOUR_BYTE_WIDTH = 4;
    constexpr base::usize BASE_STRING_LITERAL_UTF8_SURROGATE_WIDTH = 3;
    constexpr unsigned char BASE_STRING_LITERAL_INVALID_UTF8_LEAD_BYTE = 0xC3U;
    constexpr unsigned char BASE_STRING_LITERAL_INVALID_UTF8_STANDALONE_CONTINUATION = 0x80U;
    constexpr unsigned char BASE_STRING_LITERAL_OVERLONG_UTF8_LEAD_BYTE = 0xC0U;
    constexpr unsigned char BASE_STRING_LITERAL_OVERLONG_UTF8_TRAIL_BYTE = 0x80U;
    constexpr unsigned char BASE_STRING_LITERAL_SURROGATE_UTF8_BYTE_0 = 0xEDU;
    constexpr unsigned char BASE_STRING_LITERAL_SURROGATE_UTF8_BYTE_1 = 0xA0U;
    constexpr unsigned char BASE_STRING_LITERAL_SURROGATE_UTF8_BYTE_2 = 0x80U;

    const std::string BASE_STRING_LITERAL_UTF8_TWO_BYTE{"\xC2\xA9", BASE_STRING_LITERAL_UTF8_TWO_BYTE_WIDTH};
    const std::string BASE_STRING_LITERAL_UTF8_THREE_BYTE{"\xE2\x82\xAC", BASE_STRING_LITERAL_UTF8_THREE_BYTE_WIDTH};
    const std::string BASE_STRING_LITERAL_UTF8_FOUR_BYTE{"\xF0\x9F\x98\x80", BASE_STRING_LITERAL_UTF8_FOUR_BYTE_WIDTH};
    const std::string BASE_STRING_LITERAL_UTF8_SURROGATE{
        static_cast<char>(BASE_STRING_LITERAL_SURROGATE_UTF8_BYTE_0),
        static_cast<char>(BASE_STRING_LITERAL_SURROGATE_UTF8_BYTE_1),
        static_cast<char>(BASE_STRING_LITERAL_SURROGATE_UTF8_BYTE_2),
    };
    ASSERT_EQ(BASE_STRING_LITERAL_UTF8_SURROGATE.size(), BASE_STRING_LITERAL_UTF8_SURROGATE_WIDTH);

    EXPECT_TRUE(base::is_unicode_scalar(BASE_STRING_LITERAL_UNICODE_NULL_TEST_VALUE));
    EXPECT_TRUE(base::is_unicode_scalar(BASE_STRING_LITERAL_UNICODE_MAX_TEST_VALUE));
    EXPECT_FALSE(base::is_unicode_scalar(BASE_STRING_LITERAL_UNICODE_SURROGATE_TEST_VALUE));
    EXPECT_FALSE(base::is_unicode_scalar(BASE_STRING_LITERAL_UNICODE_ABOVE_MAX_TEST_VALUE));

    EXPECT_TRUE(base::is_valid_utf8("plain"));
    EXPECT_TRUE(base::is_valid_utf8(BASE_STRING_LITERAL_UTF8_TWO_BYTE));
    EXPECT_TRUE(base::is_valid_utf8(BASE_STRING_LITERAL_UTF8_THREE_BYTE));
    EXPECT_TRUE(base::is_valid_utf8(BASE_STRING_LITERAL_UTF8_FOUR_BYTE));

    std::string invalid_utf8;
    invalid_utf8.push_back(static_cast<char>(BASE_STRING_LITERAL_INVALID_UTF8_LEAD_BYTE));
    invalid_utf8.push_back('(');
    EXPECT_FALSE(base::is_valid_utf8(invalid_utf8));

    std::string invalid_lead_utf8;
    invalid_lead_utf8.push_back(static_cast<char>(BASE_STRING_LITERAL_INVALID_UTF8_STANDALONE_CONTINUATION));
    EXPECT_FALSE(base::is_valid_utf8(invalid_lead_utf8));

    std::string truncated_utf8;
    truncated_utf8.push_back(static_cast<char>(BASE_STRING_LITERAL_INVALID_UTF8_LEAD_BYTE));
    EXPECT_FALSE(base::is_valid_utf8(truncated_utf8));

    std::string overlong_utf8;
    overlong_utf8.push_back(static_cast<char>(BASE_STRING_LITERAL_OVERLONG_UTF8_LEAD_BYTE));
    overlong_utf8.push_back(static_cast<char>(BASE_STRING_LITERAL_OVERLONG_UTF8_TRAIL_BYTE));
    EXPECT_FALSE(base::is_valid_utf8(overlong_utf8));
    EXPECT_FALSE(base::is_valid_utf8(BASE_STRING_LITERAL_UTF8_SURROGATE));

    const base::StringLiteralDecode plain = base::decode_string_literal("\"plain\"", base::StringLiteralKind::string);
    ASSERT_TRUE(plain.ok());
    EXPECT_EQ(plain.decoded, "plain");

    const base::StringLiteralDecode unquoted_plain =
        base::decode_string_literal("plain", base::StringLiteralKind::string);
    ASSERT_TRUE(unquoted_plain.ok());
    EXPECT_EQ(unquoted_plain.decoded, "plain");

    const base::StringLiteralDecode escapes =
        base::decode_string_literal("\"\\0\\n\\r\\t\\\\\\\"\"", base::StringLiteralKind::string);
    ASSERT_TRUE(escapes.ok());
    ASSERT_EQ(escapes.decoded.size(), 6U);
    EXPECT_EQ(escapes.decoded[0], '\0');
    EXPECT_EQ(escapes.decoded[1], '\n');
    EXPECT_EQ(escapes.decoded[2], '\r');
    EXPECT_EQ(escapes.decoded[3], '\t');
    EXPECT_EQ(escapes.decoded[4], '\\');
    EXPECT_EQ(escapes.decoded[5], '"');

    const base::StringLiteralDecode unicode_two =
        base::decode_string_literal("\"\\u{00A9}\"", base::StringLiteralKind::string);
    ASSERT_TRUE(unicode_two.ok());
    EXPECT_EQ(unicode_two.decoded, BASE_STRING_LITERAL_UTF8_TWO_BYTE);

    const base::StringLiteralDecode unicode_three =
        base::decode_string_literal("\"\\u{20AC}\"", base::StringLiteralKind::string);
    ASSERT_TRUE(unicode_three.ok());
    EXPECT_EQ(unicode_three.decoded, BASE_STRING_LITERAL_UTF8_THREE_BYTE);

    const base::StringLiteralDecode unicode_four =
        base::decode_string_literal("\"\\u{1F600}\"", base::StringLiteralKind::string);
    ASSERT_TRUE(unicode_four.ok());
    EXPECT_EQ(unicode_four.decoded, BASE_STRING_LITERAL_UTF8_FOUR_BYTE);

    const base::StringLiteralDecode c_string =
        base::decode_string_literal("c\"abc\"", base::StringLiteralKind::c_string);
    ASSERT_TRUE(c_string.ok());
    EXPECT_EQ(c_string.decoded, "abc");

    const base::StringLiteralDecode raw_string =
        base::decode_string_literal("r\"C:\\tmp\\a\nnext\"", base::StringLiteralKind::raw_string);
    ASSERT_TRUE(raw_string.ok());
    EXPECT_EQ(raw_string.decoded, "C:\\tmp\\a\nnext");

    std::string raw_invalid_utf8 = "r\"";
    raw_invalid_utf8.push_back(static_cast<char>(BASE_STRING_LITERAL_INVALID_UTF8_LEAD_BYTE));
    raw_invalid_utf8.push_back('(');
    raw_invalid_utf8.push_back('"');
    const base::StringLiteralDecode raw_utf8_error =
        base::decode_string_literal(raw_invalid_utf8, base::StringLiteralKind::raw_string);
    ASSERT_FALSE(raw_utf8_error.ok());
    expect_contains(raw_utf8_error.errors.front().message, "valid UTF-8");

    const base::StringLiteralDecode byte_string =
        base::decode_string_literal("b\"a\\n\\0\"", base::StringLiteralKind::byte_string);
    ASSERT_TRUE(byte_string.ok());
    ASSERT_EQ(byte_string.decoded.size(), 3U);
    EXPECT_EQ(byte_string.decoded[0], 'a');
    EXPECT_EQ(byte_string.decoded[1], '\n');
    EXPECT_EQ(byte_string.decoded[2], '\0');

    const base::ByteLiteralDecode byte_literal = base::decode_byte_literal("b'\\n'");
    ASSERT_TRUE(byte_literal.ok());
    EXPECT_EQ(byte_literal.value, static_cast<base::u8>('\n'));

    const base::ByteLiteralDecode plain_byte_literal = base::decode_byte_literal("'a'");
    ASSERT_TRUE(plain_byte_literal.ok());
    EXPECT_EQ(plain_byte_literal.value, static_cast<base::u8>('a'));

    const base::ByteLiteralDecode unterminated_byte_escape = base::decode_byte_literal("b'\\'");
    ASSERT_FALSE(unterminated_byte_escape.ok());
    expect_contains(unterminated_byte_escape.errors.front().message, "unterminated escape sequence");

    const base::ByteLiteralDecode overlong_byte_escape = base::decode_byte_literal("b'\\nq'");
    ASSERT_FALSE(overlong_byte_escape.ok());
    expect_contains(overlong_byte_escape.errors.front().message, "one byte");

    const base::CharLiteralDecode char_literal = base::decode_char_literal("'\\u{03BB}'");
    ASSERT_TRUE(char_literal.ok());
    EXPECT_EQ(char_literal.value, 0x03BBU);

    const base::CharLiteralDecode char_simple_escape = base::decode_char_literal("'\\n'");
    ASSERT_TRUE(char_simple_escape.ok());
    EXPECT_EQ(char_simple_escape.value, static_cast<base::u32>('\n'));

    const base::CharLiteralDecode char_quote_escape = base::decode_char_literal("'\\\"'");
    ASSERT_TRUE(char_quote_escape.ok());
    EXPECT_EQ(char_quote_escape.value, static_cast<base::u32>('"'));

    const base::CharLiteralDecode char_three_byte_scalar = base::decode_char_literal("'€'");
    ASSERT_TRUE(char_three_byte_scalar.ok());
    EXPECT_EQ(char_three_byte_scalar.value, 0x20ACU);

    const base::StringLiteralDecode invalid_escape =
        base::decode_string_literal("\"\\q\"", base::StringLiteralKind::string);
    ASSERT_FALSE(invalid_escape.ok());
    ASSERT_FALSE(invalid_escape.errors.empty());
    expect_contains(invalid_escape.errors.front().message, "invalid escape sequence");

    const base::StringLiteralDecode missing_digits =
        base::decode_string_literal("\"\\u{}\"", base::StringLiteralKind::string);
    ASSERT_FALSE(missing_digits.ok());
    ASSERT_FALSE(missing_digits.errors.empty());
    expect_contains(missing_digits.errors.front().message, "has no digits");

    const base::StringLiteralDecode non_hex_digit =
        base::decode_string_literal("\"\\u{12G}\"", base::StringLiteralKind::string);
    ASSERT_FALSE(non_hex_digit.ok());
    ASSERT_FALSE(non_hex_digit.errors.empty());
    expect_contains(non_hex_digit.errors.front().message, "non-hex digit");

    const base::StringLiteralDecode unterminated_unicode =
        base::decode_string_literal("\"\\u{12", base::StringLiteralKind::string);
    ASSERT_FALSE(unterminated_unicode.ok());
    ASSERT_FALSE(unterminated_unicode.errors.empty());
    expect_contains(unterminated_unicode.errors.front().message, "unterminated unicode escape");

    const base::StringLiteralDecode overflow_unicode =
        base::decode_string_literal("\"\\u{110000}\"", base::StringLiteralKind::string);
    ASSERT_FALSE(overflow_unicode.ok());
    ASSERT_FALSE(overflow_unicode.errors.empty());
    expect_contains(overflow_unicode.errors.front().message, "not a valid Unicode scalar");

    const base::StringLiteralDecode c_nul =
        base::decode_string_literal("c\"a\\0b\"", base::StringLiteralKind::c_string);
    ASSERT_FALSE(c_nul.ok());
    ASSERT_FALSE(c_nul.errors.empty());
    expect_contains(c_nul.errors.front().message, "interior NUL");

    const base::StringLiteralDecode byte_unicode =
        base::decode_string_literal("b\"\\u{41}\"", base::StringLiteralKind::byte_string);
    ASSERT_FALSE(byte_unicode.ok());
    expect_contains(byte_unicode.errors.front().message, "invalid escape sequence");

    const base::CharLiteralDecode char_multi = base::decode_char_literal("'ab'");
    ASSERT_FALSE(char_multi.ok());
    expect_contains(char_multi.errors.front().message, "one Unicode scalar");

    const base::CharLiteralDecode char_empty = base::decode_char_literal("''");
    ASSERT_FALSE(char_empty.ok());
    expect_contains(char_empty.errors.front().message, "one Unicode scalar");

    const base::CharLiteralDecode char_unterminated_escape = base::decode_char_literal("'\\'");
    ASSERT_FALSE(char_unterminated_escape.ok());
    expect_contains(char_unterminated_escape.errors.front().message, "unterminated escape sequence");

    const base::CharLiteralDecode char_overlong_escape = base::decode_char_literal("'\\nq'");
    ASSERT_FALSE(char_overlong_escape.ok());
    expect_contains(char_overlong_escape.errors.front().message, "one Unicode scalar");

    std::string truncated_escape = "\"abc";
    truncated_escape.push_back('\\');
    const base::StringLiteralDecode unterminated_escape =
        base::decode_string_literal(truncated_escape, base::StringLiteralKind::string);
    ASSERT_FALSE(unterminated_escape.ok());
    ASSERT_FALSE(unterminated_escape.errors.empty());
    expect_contains(unterminated_escape.errors.front().message, "unterminated escape sequence");
}

TEST(CoreUnit, BaseStringLiteralHelpersCoverUnicodeEscapeEdges)
{
    constexpr unsigned char BASE_STRING_LITERAL_UTF8_SINGLE_BYTE_MAX_TEST_VALUE = 0x7FU;
    constexpr base::usize BASE_STRING_LITERAL_SINGLE_BYTE_WIDTH = 1;

    const std::string single_byte_expected(
        BASE_STRING_LITERAL_SINGLE_BYTE_WIDTH, static_cast<char>(BASE_STRING_LITERAL_UTF8_SINGLE_BYTE_MAX_TEST_VALUE));
    const base::StringLiteralDecode lowercase_hex =
        base::decode_string_literal("\"\\u{7f}\"", base::StringLiteralKind::string);
    ASSERT_TRUE(lowercase_hex.ok());
    EXPECT_EQ(lowercase_hex.decoded, single_byte_expected);

    const base::StringLiteralDecode missing_brace =
        base::decode_string_literal("\"\\u1\"", base::StringLiteralKind::string);
    ASSERT_FALSE(missing_brace.ok());
    ASSERT_FALSE(missing_brace.errors.empty());
    EXPECT_EQ(missing_brace.decoded, "u1");
    expect_contains(missing_brace.errors.front().message, "unicode escape must use");

    const base::StringLiteralDecode c_unicode_nul =
        base::decode_string_literal("c\"\\u{0}\"", base::StringLiteralKind::c_string);
    ASSERT_FALSE(c_unicode_nul.ok());
    ASSERT_FALSE(c_unicode_nul.errors.empty());
    ASSERT_EQ(c_unicode_nul.decoded.size(), BASE_STRING_LITERAL_SINGLE_BYTE_WIDTH);
    EXPECT_EQ(c_unicode_nul.decoded.front(), '\0');
    expect_contains(c_unicode_nul.errors.front().message, "interior NUL");
}

} // namespace aurex::test
