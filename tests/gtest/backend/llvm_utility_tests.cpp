#include "aurex/backend/llvm_backend.hpp"
#include "aurex/base/string_literal.hpp"
#include "gtest/support/ir_test_helpers.hpp"

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace aurex::backend {
[[nodiscard]] bool parse_u64(const std::string& text, std::uint64_t& out) noexcept;
[[nodiscard]] std::string decode_string_literal(const std::string& literal, bool has_c_prefix);
[[nodiscard]] std::uint64_t parse_byte_literal(const std::string& literal);
} // namespace aurex::backend

namespace aurex::test {
namespace {

using base::ErrorCode;
using namespace irtest;

} // namespace

TEST(CoreUnit, LlvmBackendUtilityHelpersCoverLiteralVariants) {
    std::uint64_t parsed = 0;
    EXPECT_TRUE(backend::parse_u64("0x2a", parsed));
    EXPECT_EQ(parsed, 42U);
    EXPECT_TRUE(backend::parse_u64("0b1010", parsed));
    EXPECT_EQ(parsed, 10U);
    EXPECT_FALSE(backend::parse_u64("not-a-number", parsed));

    const std::string decoded = backend::decode_string_literal("\"\\0\\r\\t\\\\\\\"x\"", false);
    ASSERT_EQ(decoded.size(), 6U);
    EXPECT_EQ(decoded[0], '\0');
    EXPECT_EQ(decoded[1], '\r');
    EXPECT_EQ(decoded[2], '\t');
    EXPECT_EQ(decoded[3], '\\');
    EXPECT_EQ(decoded[4], '"');
    EXPECT_EQ(decoded[5], 'x');
    EXPECT_EQ(backend::decode_string_literal("c\"raw\"", true), "raw");
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
    EXPECT_EQ(backend::parse_byte_literal("b'\\x'"), static_cast<std::uint64_t>('x'));
    EXPECT_EQ(backend::parse_byte_literal("b''"), 0U);
}

} // namespace aurex::test
