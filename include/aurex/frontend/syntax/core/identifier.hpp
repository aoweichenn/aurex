#pragma once

#include <aurex/infrastructure/base/bump_allocator.hpp>
#include <aurex/infrastructure/base/integer.hpp>

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <unordered_map>

namespace aurex::syntax {

inline constexpr base::u32 SYNTAX_INVALID_IDENT_VALUE = std::numeric_limits<base::u32>::max();

struct IdentId {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = SYNTAX_INVALID_IDENT_VALUE;

    [[nodiscard]] friend constexpr bool operator==(IdentId lhs, IdentId rhs) noexcept = default;
};

inline constexpr IdentId INVALID_IDENT_ID{IdentId::INVALID_VALUE};

struct StableHash64 {
    base::u64 value = 0;

    [[nodiscard]] friend constexpr bool operator==(StableHash64 lhs, StableHash64 rhs) noexcept = default;
};

[[nodiscard]] inline constexpr bool is_valid(const IdentId id) noexcept
{
    return id.value != IdentId::INVALID_VALUE;
}

struct IdentifierTextHash {
    [[nodiscard]] std::size_t operator()(std::string_view value) const noexcept;
};

[[nodiscard]] StableHash64 stable_hash_text(std::string_view text) noexcept;

class IdentifierInterner final {
public:
    IdentifierInterner();
    IdentifierInterner(const IdentifierInterner& other);
    IdentifierInterner& operator=(const IdentifierInterner& other);
    IdentifierInterner(IdentifierInterner&& other) noexcept;
    IdentifierInterner& operator=(IdentifierInterner&& other) noexcept;
    ~IdentifierInterner() = default;

    void reserve(base::usize expected_identifiers);

    [[nodiscard]] IdentId intern(std::string_view text);
    [[nodiscard]] IdentId find(std::string_view text) const noexcept;
    [[nodiscard]] std::string_view text(IdentId id) const noexcept;
    [[nodiscard]] StableHash64 stable_hash(IdentId id) const noexcept;
    [[nodiscard]] base::usize size() const noexcept;
    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;

private:
    using TextVector = base::BumpVector<std::string_view>;
    using IdMapEntry = std::pair<const std::string_view, IdentId>;
    using IdMap = std::unordered_map<std::string_view, IdentId, IdentifierTextHash, std::equal_to<>,
        base::BumpAllocatorAdapter<IdMapEntry>>;

    void ensure_storage();
    void swap(IdentifierInterner& other) noexcept;

    std::unique_ptr<base::BumpAllocator> arena_;
    TextVector texts_;
    IdMap ids_;
};

} // namespace aurex::syntax
