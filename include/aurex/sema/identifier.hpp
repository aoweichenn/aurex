#pragma once

#include <aurex/base/integer.hpp>
#include <aurex/syntax/identifier.hpp>

#include <cstddef>
#include <limits>
#include <ostream>
#include <span>
#include <string_view>

namespace aurex::sema {

inline constexpr base::u32 SEMA_LOOKUP_INVALID_KEY_PART = std::numeric_limits<base::u32>::max();

using syntax::IdentId;
using syntax::IdentifierInterner;
using syntax::INVALID_IDENT_ID;
using syntax::StableHash64;
using syntax::is_valid;

struct GenericParamIdentity {
    base::u64 value = 0;

    [[nodiscard]] friend constexpr bool operator==(
        GenericParamIdentity lhs,
        GenericParamIdentity rhs
    ) noexcept = default;
};

inline constexpr GenericParamIdentity INVALID_GENERIC_PARAM_IDENTITY {};

[[nodiscard]] inline constexpr bool is_valid(const GenericParamIdentity identity) noexcept {
    return identity.value != 0;
}

[[nodiscard]] inline GenericParamIdentity generic_param_identity_from_text(
    const std::string_view text
) noexcept {
    const StableHash64 hash = syntax::stable_hash_text(text);
    return GenericParamIdentity {hash.value == 0 ? 1 : hash.value};
}

struct InternedText {
    IdentId id = INVALID_IDENT_ID;
    const IdentifierInterner* interner = nullptr;

    InternedText() = default;

    constexpr InternedText(const IdentId text_id, const IdentifierInterner* const owner) noexcept
        : id(text_id),
          interner(owner) {}

    [[nodiscard]] bool empty() const noexcept {
        return this->view().empty();
    }

    [[nodiscard]] base::usize size() const noexcept {
        return this->view().size();
    }

    [[nodiscard]] const char* data() const noexcept {
        return this->view().data();
    }

    [[nodiscard]] std::string_view view() const noexcept {
        return this->interner == nullptr ? std::string_view {} : this->interner->text(this->id);
    }

    [[nodiscard]] operator std::string_view() const noexcept {
        return this->view();
    }

    [[nodiscard]] friend bool operator==(const InternedText lhs, const InternedText rhs) noexcept {
        if (lhs.interner == rhs.interner) {
            return lhs.id == rhs.id;
        }
        return lhs.view() == rhs.view();
    }

    [[nodiscard]] friend bool operator!=(const InternedText lhs, const InternedText rhs) noexcept {
        return !(lhs == rhs);
    }

    [[nodiscard]] friend bool operator==(const InternedText lhs, const std::string_view rhs) noexcept {
        return lhs.view() == rhs;
    }

    [[nodiscard]] friend bool operator==(const std::string_view lhs, const InternedText rhs) noexcept {
        return lhs == rhs.view();
    }

    [[nodiscard]] friend bool operator==(const InternedText lhs, const char* const rhs) noexcept {
        return lhs.view() == (rhs == nullptr ? std::string_view {} : std::string_view {rhs});
    }

    [[nodiscard]] friend bool operator==(const char* const lhs, const InternedText rhs) noexcept {
        return (lhs == nullptr ? std::string_view {} : std::string_view {lhs}) == rhs.view();
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
    return InternedText {id, &interner};
}

inline void rebind_interned_text(InternedText& text, const IdentifierInterner& interner) noexcept {
    if (is_valid(text.id)) {
        text.interner = &interner;
    }
}

inline void rebind_interned_text(
    InternedText& text,
    const IdentifierInterner* const from,
    const IdentifierInterner& to
) noexcept {
    if (is_valid(text.id) && text.interner == from) {
        text.interner = &to;
    }
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

struct GenericParamIdentityHash {
    [[nodiscard]] std::size_t operator()(GenericParamIdentity identity) const noexcept;
};

struct StableFingerprint128 {
    base::u64 primary = 0;
    base::u64 secondary = 0;
    base::u32 byte_count = 0;

    [[nodiscard]] friend constexpr bool operator==(
        StableFingerprint128 lhs,
        StableFingerprint128 rhs
    ) noexcept = default;
};

struct StableModuleId {
    StableFingerprint128 path;
    base::u32 part_count = 0;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(
        StableModuleId lhs,
        StableModuleId rhs
    ) noexcept = default;
};

enum class StableSymbolKind : base::u8 {
    invalid = 0,
    type,
    function,
    method,
    value,
    enum_case,
    struct_field,
    generic_template,
    synthetic,
};

struct StableDefId {
    StableModuleId module;
    StableFingerprint128 name;
    base::u64 global_id = 0;
    base::u32 disambiguator = 0;
    StableSymbolKind kind = StableSymbolKind::invalid;

    [[nodiscard]] friend constexpr bool operator==(
        StableDefId lhs,
        StableDefId rhs
    ) noexcept = default;
};

struct StableMemberKey {
    StableDefId owner;
    StableFingerprint128 member_name;
    base::u64 global_id = 0;
    base::u32 disambiguator = 0;
    StableSymbolKind kind = StableSymbolKind::invalid;

    [[nodiscard]] friend constexpr bool operator==(
        StableMemberKey lhs,
        StableMemberKey rhs
    ) noexcept = default;
};

struct IncrementalKey {
    StableDefId definition;
    StableFingerprint128 fingerprint;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(
        IncrementalKey lhs,
        IncrementalKey rhs
    ) noexcept = default;
};

[[nodiscard]] StableFingerprint128 stable_fingerprint(std::string_view text) noexcept;
[[nodiscard]] StableFingerprint128 stable_fingerprint(std::span<const std::string_view> parts) noexcept;
[[nodiscard]] StableModuleId stable_module_id(std::span<const std::string_view> module_path) noexcept;
[[nodiscard]] StableDefId stable_definition_id(
    const StableModuleId& module,
    StableSymbolKind kind,
    std::string_view name,
    base::u32 disambiguator = 0
) noexcept;
[[nodiscard]] StableMemberKey stable_member_key(
    const StableDefId& owner,
    StableSymbolKind kind,
    std::string_view member_name,
    base::u32 disambiguator = 0
) noexcept;
[[nodiscard]] IncrementalKey stable_incremental_key(
    const StableDefId& definition,
    std::string_view semantic_fingerprint
) noexcept;

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
