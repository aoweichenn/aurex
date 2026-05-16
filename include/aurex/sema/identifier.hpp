#pragma once

#include <aurex/base/integer.hpp>
#include <aurex/syntax/identifier.hpp>

#include <cstddef>
#include <limits>

namespace aurex::sema {

inline constexpr base::u32 SEMA_LOOKUP_INVALID_KEY_PART = std::numeric_limits<base::u32>::max();

using syntax::IdentId;
using syntax::IdentifierInterner;
using syntax::INVALID_IDENT_ID;
using syntax::is_valid;

struct ModuleLookupKey {
    base::u32 module = SEMA_LOOKUP_INVALID_KEY_PART;
    IdentId name = INVALID_IDENT_ID;

    [[nodiscard]] friend constexpr bool operator==(
        ModuleLookupKey lhs,
        ModuleLookupKey rhs
    ) noexcept = default;
};

[[nodiscard]] inline constexpr bool is_valid(const ModuleLookupKey key) noexcept {
    return key.module != SEMA_LOOKUP_INVALID_KEY_PART && is_valid(key.name);
}

struct ModuleLookupKeyHash {
    [[nodiscard]] std::size_t operator()(ModuleLookupKey key) const noexcept;
};

struct IdentIdHash {
    [[nodiscard]] std::size_t operator()(IdentId id) const noexcept;
};

struct MethodLookupKey {
    base::u32 module = SEMA_LOOKUP_INVALID_KEY_PART;
    base::u32 owner_type = SEMA_LOOKUP_INVALID_KEY_PART;
    IdentId name = INVALID_IDENT_ID;

    [[nodiscard]] friend constexpr bool operator==(
        MethodLookupKey lhs,
        MethodLookupKey rhs
    ) noexcept = default;
};

[[nodiscard]] inline constexpr bool is_valid(const MethodLookupKey key) noexcept {
    return key.module != SEMA_LOOKUP_INVALID_KEY_PART &&
           key.owner_type != SEMA_LOOKUP_INVALID_KEY_PART &&
           is_valid(key.name);
}

struct MethodLookupKeyHash {
    [[nodiscard]] std::size_t operator()(MethodLookupKey key) const noexcept;
};

struct FunctionLookupKey {
    base::u32 module = SEMA_LOOKUP_INVALID_KEY_PART;
    base::u32 owner_type = SEMA_LOOKUP_INVALID_KEY_PART;
    IdentId name = INVALID_IDENT_ID;

    [[nodiscard]] friend constexpr bool operator==(
        FunctionLookupKey lhs,
        FunctionLookupKey rhs
    ) noexcept = default;
};

[[nodiscard]] inline constexpr bool is_valid(const FunctionLookupKey key) noexcept {
    return key.module != SEMA_LOOKUP_INVALID_KEY_PART && is_valid(key.name);
}

struct FunctionLookupKeyHash {
    [[nodiscard]] std::size_t operator()(FunctionLookupKey key) const noexcept;
};

struct EnumCaseLookupKey {
    base::u32 enum_type = SEMA_LOOKUP_INVALID_KEY_PART;
    IdentId case_name = INVALID_IDENT_ID;

    [[nodiscard]] friend constexpr bool operator==(
        EnumCaseLookupKey lhs,
        EnumCaseLookupKey rhs
    ) noexcept = default;
};

struct EnumCaseLookupKeyHash {
    [[nodiscard]] std::size_t operator()(EnumCaseLookupKey key) const noexcept;
};

} // namespace aurex::sema
