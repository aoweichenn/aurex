#pragma once

#include <cstddef>
#include <string_view>

namespace aurex::base::config {

inline constexpr std::string_view version_string = "M0V0.1.8";
inline constexpr std::size_t source_preview_radius = 32;
inline constexpr std::size_t initial_token_capacity = 4096;
inline constexpr std::size_t initial_ast_node_capacity = 4096;
inline constexpr std::size_t max_include_depth = 128;

} // namespace aurex::base::config
