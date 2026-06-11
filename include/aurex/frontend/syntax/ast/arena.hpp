#pragma once

#include <aurex/infrastructure/base/bump_allocator.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace aurex::syntax {

template <typename T>
using AstArenaVector = base::BumpVector<T>;

template <typename T>
[[nodiscard]] AstArenaVector<T> make_ast_arena_vector(base::BumpAllocator& arena)
{
    return AstArenaVector<T>(base::BumpAllocatorAdapter<T>{arena});
}

template <typename T>
[[nodiscard]] bool ast_arena_vector_uses_arena(const AstArenaVector<T>& values, base::BumpAllocator& arena) noexcept
{
    return values.get_allocator() == base::BumpAllocatorAdapter<T>{arena};
}

template <typename T, typename Allocator>
[[nodiscard]] AstArenaVector<T> copy_ast_arena_vector(
    base::BumpAllocator& arena, const std::vector<T, Allocator>& values)
{
    AstArenaVector<T> copy = make_ast_arena_vector<T>(arena);
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

template <typename T>
[[nodiscard]] AstArenaVector<T> move_or_copy_ast_arena_vector(base::BumpAllocator& arena, AstArenaVector<T>&& values)
{
    if (ast_arena_vector_uses_arena(values, arena)) {
        return std::move(values);
    }
    return copy_ast_arena_vector(arena, values);
}

template <typename T, typename Allocator>
[[nodiscard]] std::vector<T> copy_std_vector(const std::vector<T, Allocator>& values)
{
    std::vector<T> copy;
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

template <typename T, typename Allocator>
[[nodiscard]] AstArenaVector<T> copy_detached_ast_vector(const std::vector<T, Allocator>& values)
{
    AstArenaVector<T> copy{base::BumpAllocatorAdapter<T>::heap_backed()};
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

inline constexpr base::usize SYNTAX_AST_RESERVE_EXPR_TOKEN_DIVISOR = 8;
inline constexpr base::usize SYNTAX_AST_RESERVE_STMT_TOKEN_DIVISOR = 16;
inline constexpr base::usize SYNTAX_AST_RESERVE_TYPE_TOKEN_DIVISOR = 16;
inline constexpr base::usize SYNTAX_AST_RESERVE_PATTERN_TOKEN_DIVISOR = 32;
inline constexpr base::usize SYNTAX_AST_RESERVE_ITEM_TOKEN_DIVISOR = 64;
inline constexpr base::usize SYNTAX_AST_RESERVE_IDENTIFIER_TOKEN_DIVISOR = 8;
inline constexpr base::usize SYNTAX_AST_RESERVE_EXPRS_PER_STATEMENT = 6;
inline constexpr base::usize SYNTAX_AST_RESERVE_TYPES_PER_TYPE_SITE = 2;
inline constexpr base::usize SYNTAX_AST_RESERVE_TYPES_PER_ITEM = 4;
inline constexpr base::usize SYNTAX_AST_RESERVE_PATTERNS_PER_PATTERN_SITE = 2;
inline constexpr base::usize SYNTAX_AST_RESERVE_EXPR_NAME_DIVISOR = 2;
inline constexpr base::usize SYNTAX_AST_RESERVE_PRIMARY_PAYLOAD_DIVISOR = 3;
inline constexpr base::usize SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR = 8;
inline constexpr base::usize SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR = 32;
inline constexpr base::usize SYNTAX_AST_ARENA_ALLOCATION_PADDING_BYTES = alignof(std::max_align_t);

[[nodiscard]] constexpr base::usize ast_reserve_fraction(const base::usize size, const base::usize divisor) noexcept
{
    return size == 0 ? 0 : ((size - 1) / divisor) + 1;
}

[[nodiscard]] constexpr base::usize ast_reserve_at_least(
    const base::usize minimum, const base::usize estimated) noexcept
{
    return estimated < minimum ? minimum : estimated;
}

[[nodiscard]] constexpr base::usize ast_reserve_larger(const base::usize lhs, const base::usize rhs) noexcept
{
    return lhs < rhs ? rhs : lhs;
}

struct AstReserveEstimate {
    struct Exprs {
        base::usize headers = 0;
        base::usize literals = 0;
        base::usize names = 0;
        base::usize generic_applies = 0;
        base::usize unaries = 0;
        base::usize tries = 0;
        base::usize binaries = 0;
        base::usize calls = 0;
        base::usize lambdas = 0;
        base::usize ifs = 0;
        base::usize blocks = 0;
        base::usize matches = 0;
        base::usize arrays = 0;
        base::usize tuples = 0;
        base::usize fields = 0;
        base::usize indexes = 0;
        base::usize slices = 0;
        base::usize struct_literals = 0;
        base::usize casts = 0;
    };

    base::usize tokens = 0;
    base::usize statements = 0;
    base::usize items = 0;
    base::usize type_sites = 0;
    base::usize pattern_sites = 0;
    base::usize identifier_tokens = 0;
    Exprs exprs;
};

[[nodiscard]] constexpr AstReserveEstimate::Exprs ast_expr_reserve_for_node_capacity(const base::usize size) noexcept
{
    return AstReserveEstimate::Exprs{
        size,
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_PRIMARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_EXPR_NAME_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_PRIMARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
    };
}

} // namespace aurex::syntax
