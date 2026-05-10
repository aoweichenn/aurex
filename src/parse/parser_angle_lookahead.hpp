#pragma once

#include <aurex/base/config.hpp>
#include <aurex/syntax/token.hpp>

#include <span>

namespace aurex::parse {

enum class AngleListFollower {
    type_scope,
    struct_literal,
};

[[nodiscard]] bool next_angle_list_has_follower(
    std::span<const syntax::Token> tokens,
    base::usize less_position,
    AngleListFollower follower
) noexcept;

} // namespace aurex::parse
