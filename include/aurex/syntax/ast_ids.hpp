#pragma once

#include <aurex/base/integer.hpp>

#include <limits>

namespace aurex::syntax {

struct TypeId {
    base::u32 value = invalid_value;
    static constexpr base::u32 invalid_value = std::numeric_limits<base::u32>::max();
};

struct ExprId {
    base::u32 value = invalid_value;
    static constexpr base::u32 invalid_value = std::numeric_limits<base::u32>::max();
};

struct PatternId {
    base::u32 value = invalid_value;
    static constexpr base::u32 invalid_value = std::numeric_limits<base::u32>::max();
};

struct StmtId {
    base::u32 value = invalid_value;
    static constexpr base::u32 invalid_value = std::numeric_limits<base::u32>::max();
};

struct ItemId {
    base::u32 value = invalid_value;
    static constexpr base::u32 invalid_value = std::numeric_limits<base::u32>::max();
};

struct ModuleId {
    base::u32 value = invalid_value;
    static constexpr base::u32 invalid_value = std::numeric_limits<base::u32>::max();
};

inline constexpr TypeId invalid_type_id {TypeId::invalid_value};
inline constexpr ExprId invalid_expr_id {ExprId::invalid_value};
inline constexpr PatternId invalid_pattern_id {PatternId::invalid_value};
inline constexpr StmtId invalid_stmt_id {StmtId::invalid_value};
inline constexpr ItemId invalid_item_id {ItemId::invalid_value};
inline constexpr ModuleId invalid_module_id {ModuleId::invalid_value};

[[nodiscard]] inline constexpr bool is_valid(const TypeId id) noexcept {
    return id.value != TypeId::invalid_value;
}

[[nodiscard]] inline constexpr bool is_valid(const ExprId id) noexcept {
    return id.value != ExprId::invalid_value;
}

[[nodiscard]] inline constexpr bool is_valid(const PatternId id) noexcept {
    return id.value != PatternId::invalid_value;
}

[[nodiscard]] inline constexpr bool is_valid(const StmtId id) noexcept {
    return id.value != StmtId::invalid_value;
}

[[nodiscard]] inline constexpr bool is_valid(const ItemId id) noexcept {
    return id.value != ItemId::invalid_value;
}

[[nodiscard]] inline constexpr bool is_valid(const ModuleId id) noexcept {
    return id.value != ModuleId::invalid_value;
}

} // namespace aurex::syntax
