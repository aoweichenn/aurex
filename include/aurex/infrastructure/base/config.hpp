#pragma once

#include <cstddef>
#include <string_view>

namespace aurex::base::config {

inline constexpr std::string_view AUREX_VERSION_STRING = "0.1.2";
inline constexpr std::size_t AUREX_SOURCE_PREVIEW_RADIUS = 32;
inline constexpr std::size_t AUREX_INITIAL_TOKEN_CAPACITY = 4096;
inline constexpr std::size_t AUREX_INITIAL_AST_NODE_CAPACITY = 4096;
inline constexpr std::size_t AUREX_MAX_INCLUDE_DEPTH = 128;

} // namespace aurex::base::config
