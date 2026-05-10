#pragma once

#include <aurex/base/integer.hpp>

#include <string_view>

namespace aurex::base {

struct LineColumn {
    usize line = 1;
    usize column = 1;
};

[[nodiscard]] LineColumn line_column(std::string_view text, usize offset) noexcept;

} // namespace aurex::base
