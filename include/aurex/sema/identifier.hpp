#pragma once

#include <aurex/base/integer.hpp>

#include <cstddef>
#include <deque>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aurex::sema {

inline constexpr base::u32 SEMA_LOOKUP_INVALID_KEY_PART = std::numeric_limits<base::u32>::max();

struct IdentId {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = SEMA_LOOKUP_INVALID_KEY_PART;

    [[nodiscard]] friend constexpr bool operator==(IdentId lhs, IdentId rhs) noexcept = default;
};

inline constexpr IdentId INVALID_IDENT_ID {IdentId::INVALID_VALUE};

[[nodiscard]] inline constexpr bool is_valid(const IdentId id) noexcept {
    return id.value != IdentId::INVALID_VALUE;
}

struct IdentifierTextHash {
    [[nodiscard]] std::size_t operator()(std::string_view value) const noexcept;
};

class IdentifierInterner final {
public:
    IdentifierInterner() = default;
    IdentifierInterner(const IdentifierInterner&) = delete;
    IdentifierInterner& operator=(const IdentifierInterner&) = delete;
    IdentifierInterner(IdentifierInterner&&) = delete;
    IdentifierInterner& operator=(IdentifierInterner&&) = delete;
    ~IdentifierInterner() = default;

    void reserve(base::usize expected_identifiers);

    [[nodiscard]] IdentId intern(std::string_view text);
    [[nodiscard]] IdentId find(std::string_view text) const noexcept;
    [[nodiscard]] std::string_view text(IdentId id) const noexcept;
    [[nodiscard]] base::usize size() const noexcept;

private:
    std::deque<std::string> storage_;
    std::vector<std::string_view> texts_;
    std::unordered_map<std::string_view, IdentId, IdentifierTextHash, std::equal_to<>> ids_;
};

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
