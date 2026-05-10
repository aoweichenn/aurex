#include <parse/parser_angle_lookahead.hpp>

namespace aurex::parse {
namespace {

using syntax::TokenKind;

constexpr int PARSER_ANGLE_GREATER_GREATER_TOKEN_ARITY = 2;
constexpr int PARSER_ANGLE_SINGLE_GREATER_TOKEN_ARITY = 1;
constexpr int PARSER_ANGLE_INITIAL_LIST_DEPTH = 1;
constexpr int PARSER_ANGLE_CLOSED_LIST_DEPTH = 0;
constexpr base::usize PARSER_ANGLE_NEXT_TOKEN_OFFSET = 1;

[[nodiscard]] bool angle_list_follower_matches(
    const TokenKind kind,
    const AngleListFollower follower
) noexcept {
    switch (follower) {
    case AngleListFollower::TYPE_SCOPE:
        return kind == TokenKind::dot || kind == TokenKind::l_paren;
    case AngleListFollower::STRUCT_LITERAL:
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
        return follower == AngleListFollower::TYPE_SCOPE;
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

    base::usize index = less_position + PARSER_ANGLE_NEXT_TOKEN_OFFSET;
    int depth = PARSER_ANGLE_INITIAL_LIST_DEPTH;
    while (index < tokens.size()) {
        const TokenKind kind = tokens[index].kind;
        if (kind == TokenKind::less) {
            ++depth;
        } else if (kind == TokenKind::greater || kind == TokenKind::greater_greater) {
            const int close_count = kind == TokenKind::greater_greater
                ? PARSER_ANGLE_GREATER_GREATER_TOKEN_ARITY
                : PARSER_ANGLE_SINGLE_GREATER_TOKEN_ARITY;
            for (int i = 0; i < close_count; ++i) {
                --depth;
                if (depth == PARSER_ANGLE_CLOSED_LIST_DEPTH) {
                    const base::usize after = index + PARSER_ANGLE_NEXT_TOKEN_OFFSET;
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
