#include "aurex/backend/llvm_backend.hpp"
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
    EXPECT_EQ(backend::decode_string_literal("\\q", false), "q");

    EXPECT_EQ(backend::parse_byte_literal("b'\\\\'"), static_cast<std::uint64_t>('\\'));
    EXPECT_EQ(backend::parse_byte_literal("b'\\''"), static_cast<std::uint64_t>('\''));
    EXPECT_EQ(backend::parse_byte_literal("b'\\x'"), static_cast<std::uint64_t>('x'));
    EXPECT_EQ(backend::parse_byte_literal("b''"), 0U);
}

} // namespace aurex::test
