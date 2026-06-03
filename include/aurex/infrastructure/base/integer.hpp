#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace aurex::base {

using i8 = std::int8_t;
using u8 = std::uint8_t;
using i16 = std::int16_t;
using u16 = std::uint16_t;
using i32 = std::int32_t;
using u32 = std::uint32_t;
using i64 = std::int64_t;
using u64 = std::uint64_t;
using isize = std::ptrdiff_t;
using usize = std::size_t;

inline constexpr usize BASE_U32_MAX_AS_USIZE = static_cast<usize>(std::numeric_limits<u32>::max());

[[nodiscard]] constexpr bool fits_u32(const usize value) noexcept
{
    return value <= BASE_U32_MAX_AS_USIZE;
}

[[nodiscard]] inline u32 checked_u32(const usize value, const std::string_view context)
{
    if (!fits_u32(value)) {
        throw std::length_error(std::string(context) + " exceeds u32 capacity");
    }
    return static_cast<u32>(value);
}

[[nodiscard]] constexpr bool checked_add_overflows(const usize lhs, const usize rhs) noexcept
{
    return lhs > std::numeric_limits<usize>::max() - rhs;
}

[[nodiscard]] inline usize checked_add_usize(const usize lhs, const usize rhs, const std::string_view context)
{
    if (checked_add_overflows(lhs, rhs)) {
        throw std::length_error(std::string(context) + " size addition overflows");
    }
    return lhs + rhs;
}

[[nodiscard]] inline usize checked_mul_usize(const usize lhs, const usize rhs, const std::string_view context)
{
    if (lhs != 0 && rhs > std::numeric_limits<usize>::max() / lhs) {
        throw std::length_error(std::string(context) + " size multiplication overflows");
    }
    return lhs * rhs;
}

} // namespace aurex::base
