#include <parse/parser_angle_lookahead.hpp>

namespace aurex::parse {
namespace {

using syntax::TokenKind;

constexpr int kGreaterGreaterTokenArity = 2;
constexpr int kSingleGreaterTokenArity = 1;
constexpr int kInitialAngleListDepth = 1;
constexpr int kClosedAngleListDepth = 0;
constexpr base::usize kNextTokenOffset = 1;

[[nodiscard]] bool angle_list_follower_matches(
    const TokenKind kind,
    const AngleListFollower follower
) noexcept {
    switch (follower) {
    case AngleListFollower::type_scope:
        return kind == TokenKind::dot || kind == TokenKind::l_paren;
    case AngleListFollower::struct_literal:
        return kind == TokenKind::l_brace;
    }
    return false;
}

[[nodiscard]] bool angle_list_scan_should_stop(
    const TokenKind kind,
    const AngleListFollower follower
) noexcept {
    switch (kind) {
    case TokenKind::semicolon:
    case TokenKind::r_brace:
    case TokenKind::fat_arrow:
        return true;
    case TokenKind::l_brace:
        return follower == AngleListFollower::type_scope;
    default:
        return false;
    }
}

} // namespace

bool next_angle_list_has_follower(
    const std::span<const syntax::Token> tokens,
    const base::usize less_position,
    const AngleListFollower follower
) noexcept {
    if (less_position >= tokens.size() || tokens[less_position].kind != TokenKind::less) {
        return false;
    }

    base::usize index = less_position + kNextTokenOffset;
    int depth = kInitialAngleListDepth;
    while (index < tokens.size()) {
        const TokenKind kind = tokens[index].kind;
        if (kind == TokenKind::less) {
            ++depth;
        } else if (kind == TokenKind::greater || kind == TokenKind::greater_greater) {
            const int close_count = kind == TokenKind::greater_greater
                ? kGreaterGreaterTokenArity
                : kSingleGreaterTokenArity;
            for (int i = 0; i < close_count; ++i) {
                --depth;
                if (depth == kClosedAngleListDepth) {
                    const base::usize after = index + kNextTokenOffset;
                    return after < tokens.size() && angle_list_follower_matches(tokens[after].kind, follower);
                }
            }
        } else if (angle_list_scan_should_stop(kind, follower)) {
            return false;
        }
        ++index;
    }
    return false;
}

} // namespace aurex::parse
