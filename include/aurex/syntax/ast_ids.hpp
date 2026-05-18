#pragma once

#include <aurex/base/integer.hpp>

#include <limits>

namespace aurex::syntax {

struct TypeId {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = std::numeric_limits<base::u32>::max();
};

struct ExprId {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = std::numeric_limits<base::u32>::max();
};

struct PatternId {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = std::numeric_limits<base::u32>::max();
};

struct StmtId {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = std::numeric_limits<base::u32>::max();
};

struct ItemId {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = std::numeric_limits<base::u32>::max();
};

struct ModuleId {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = std::numeric_limits<base::u32>::max();
};

inline constexpr TypeId INVALID_TYPE_ID{TypeId::INVALID_VALUE};
inline constexpr ExprId INVALID_EXPR_ID{ExprId::INVALID_VALUE};
inline constexpr PatternId INVALID_PATTERN_ID{PatternId::INVALID_VALUE};
inline constexpr StmtId INVALID_STMT_ID{StmtId::INVALID_VALUE};
inline constexpr ItemId INVALID_ITEM_ID{ItemId::INVALID_VALUE};
inline constexpr ModuleId INVALID_MODULE_ID{ModuleId::INVALID_VALUE};

[[nodiscard]] inline constexpr bool is_valid(const TypeId id) noexcept
{
    return id.value != TypeId::INVALID_VALUE;
}

[[nodiscard]] inline constexpr bool is_valid(const ExprId id) noexcept
{
    return id.value != ExprId::INVALID_VALUE;
}

[[nodiscard]] inline constexpr bool is_valid(const PatternId id) noexcept
{
    return id.value != PatternId::INVALID_VALUE;
}

[[nodiscard]] inline constexpr bool is_valid(const StmtId id) noexcept
{
    return id.value != StmtId::INVALID_VALUE;
}

[[nodiscard]] inline constexpr bool is_valid(const ItemId id) noexcept
{
    return id.value != ItemId::INVALID_VALUE;
}

[[nodiscard]] inline constexpr bool is_valid(const ModuleId id) noexcept
{
    return id.value != ModuleId::INVALID_VALUE;
}

} // namespace aurex::syntax
