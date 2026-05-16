#pragma once

#include <aurex/base/integer.hpp>
#include <aurex/syntax/identifier.hpp>

#include <cstddef>
#include <limits>
#include <ostream>
#include <string_view>

namespace aurex::sema {

inline constexpr base::u32 SEMA_LOOKUP_INVALID_KEY_PART = std::numeric_limits<base::u32>::max();

using syntax::IdentId;
using syntax::IdentifierInterner;
using syntax::INVALID_IDENT_ID;
using syntax::is_valid;

struct InternedText {
    IdentId id = INVALID_IDENT_ID;
    std::string_view text {};

    InternedText() = default;

    constexpr InternedText(const IdentId text_id, const std::string_view text_value) noexcept
        : id(text_id),
          text(text_value) {}

    InternedText(const char* const literal) noexcept
        : text(literal == nullptr ? std::string_view {} : std::string_view {literal}) {}

    InternedText& operator=(const char* const literal) noexcept {
        this->id = INVALID_IDENT_ID;
        this->text = literal == nullptr ? std::string_view {} : std::string_view {literal};
        return *this;
    }

    [[nodiscard]] bool empty() const noexcept {
        return this->text.empty();
    }

    [[nodiscard]] base::usize size() const noexcept {
        return this->text.size();
    }

    [[nodiscard]] const char* data() const noexcept {
        return this->text.data();
    }

    [[nodiscard]] std::string_view view() const noexcept {
        return this->text;
    }

    [[nodiscard]] operator std::string_view() const noexcept {
        return this->text;
    }

    [[nodiscard]] friend bool operator==(const InternedText lhs, const InternedText rhs) noexcept {
        return lhs.text == rhs.text;
    }

    [[nodiscard]] friend bool operator!=(const InternedText lhs, const InternedText rhs) noexcept {
        return !(lhs == rhs);
    }

    [[nodiscard]] friend bool operator==(const InternedText lhs, const std::string_view rhs) noexcept {
        return lhs.text == rhs;
    }

    [[nodiscard]] friend bool operator==(const std::string_view lhs, const InternedText rhs) noexcept {
        return lhs == rhs.text;
    }

    [[nodiscard]] friend bool operator==(const InternedText lhs, const char* const rhs) noexcept {
        return lhs.text == (rhs == nullptr ? std::string_view {} : std::string_view {rhs});
    }

    [[nodiscard]] friend bool operator==(const char* const lhs, const InternedText rhs) noexcept {
        return (lhs == nullptr ? std::string_view {} : std::string_view {lhs}) == rhs.text;
    }

    [[nodiscard]] friend bool operator!=(const InternedText lhs, const std::string_view rhs) noexcept {
        return !(lhs == rhs);
    }

    [[nodiscard]] friend bool operator!=(const std::string_view lhs, const InternedText rhs) noexcept {
        return !(lhs == rhs);
    }

    [[nodiscard]] friend bool operator!=(const InternedText lhs, const char* const rhs) noexcept {
        return !(lhs == rhs);
    }

    [[nodiscard]] friend bool operator!=(const char* const lhs, const InternedText rhs) noexcept {
        return !(lhs == rhs);
    }
};

inline std::ostream& operator<<(std::ostream& out, const InternedText text) {
    out << text.view();
    return out;
}

[[nodiscard]] inline InternedText intern_text(IdentifierInterner& interner, const std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const IdentId id = interner.intern(text);
    return InternedText {id, interner.text(id)};
}

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
