#pragma once

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

namespace aurex::test {

inline void expect_contains(const std::string_view text, const std::string_view needle) {
    EXPECT_NE(text.find(needle), std::string_view::npos) << "missing: " << needle;
}

inline void expect_contains_all(
    const std::string_view text,
    const std::vector<std::string_view>& needles
) {
    for (const std::string_view needle : needles) {
        expect_contains(text, needle);
    }
}

inline void expect_not_contains(const std::string_view text, const std::string_view needle) {
    EXPECT_EQ(text.find(needle), std::string_view::npos) << "unexpected: " << needle;
}

} // namespace aurex::test
