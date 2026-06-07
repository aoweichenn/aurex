#pragma once

#include <aurex/infrastructure/query/canonical_type_key.hpp>

#include <span>
#include <string>
#include <vector>

namespace aurex::query {

inline constexpr base::u32 QUERY_TRAIT_OBJECT_TYPE_KEY_SCHEMA_VERSION = 1;
inline constexpr base::u32 QUERY_VTABLE_LAYOUT_KEY_SCHEMA_VERSION = 1;
inline constexpr base::u32 QUERY_TRAIT_OBJECT_COERCION_KEY_SCHEMA_VERSION = 1;

enum class TraitObjectAbiPolicyKey : base::u8 {
    borrowed_view_v1 = 1,
};

enum class TraitObjectMetadataPolicyKey : base::u8 {
    borrowed_methods_only_v1 = 1,
};

enum class TraitObjectBorrowKindKey : base::u8 {
    shared = 0,
    mut,
};

struct TraitObjectAssociatedTypeEqualityKey {
    MemberKey associated_type;
    CanonicalTypeKey value_type;

    [[nodiscard]] friend bool operator==(
        const TraitObjectAssociatedTypeEqualityKey& lhs,
        const TraitObjectAssociatedTypeEqualityKey& rhs) noexcept = default;
};

struct TraitObjectTypeKey {
    DefKey principal_trait;
    std::vector<CanonicalTypeKey> trait_args;
    std::vector<TraitObjectAssociatedTypeEqualityKey> associated_equalities;
    StableFingerprint128 object_origin;
    StableFingerprint128 object_callability_schema;
    TraitObjectAbiPolicyKey abi_policy = TraitObjectAbiPolicyKey::borrowed_view_v1;
    base::u32 schema = QUERY_TRAIT_OBJECT_TYPE_KEY_SCHEMA_VERSION;
    base::u64 global_id = 0;
};

struct VTableLayoutKey {
    CanonicalTypeKey concrete_type;
    TraitObjectTypeKey object_type;
    StableFingerprint128 slot_schema;
    StableFingerprint128 impl_evidence;
    base::u32 method_slot_count = 0;
    TraitObjectMetadataPolicyKey metadata_policy = TraitObjectMetadataPolicyKey::borrowed_methods_only_v1;
    TraitObjectAbiPolicyKey abi_policy = TraitObjectAbiPolicyKey::borrowed_view_v1;
    base::u32 schema = QUERY_VTABLE_LAYOUT_KEY_SCHEMA_VERSION;
    base::u64 global_id = 0;
};

struct TraitObjectCoercionKey {
    CanonicalTypeKey source_type;
    StableFingerprint128 source_origin;
    TraitObjectTypeKey target_object_type;
    VTableLayoutKey vtable_layout;
    TraitObjectBorrowKindKey borrow_kind = TraitObjectBorrowKindKey::shared;
    base::u32 schema = QUERY_TRAIT_OBJECT_COERCION_KEY_SCHEMA_VERSION;
    base::u64 global_id = 0;
};

[[nodiscard]] bool operator==(
    const TraitObjectTypeKey& lhs, const TraitObjectTypeKey& rhs) noexcept;
[[nodiscard]] bool operator!=(
    const TraitObjectTypeKey& lhs, const TraitObjectTypeKey& rhs) noexcept;
[[nodiscard]] bool operator==(const VTableLayoutKey& lhs, const VTableLayoutKey& rhs) noexcept;
[[nodiscard]] bool operator!=(const VTableLayoutKey& lhs, const VTableLayoutKey& rhs) noexcept;
[[nodiscard]] bool operator==(
    const TraitObjectCoercionKey& lhs, const TraitObjectCoercionKey& rhs) noexcept;
[[nodiscard]] bool operator!=(
    const TraitObjectCoercionKey& lhs, const TraitObjectCoercionKey& rhs) noexcept;

[[nodiscard]] bool is_valid(const TraitObjectAssociatedTypeEqualityKey& key) noexcept;
[[nodiscard]] bool is_valid(const TraitObjectTypeKey& key) noexcept;
[[nodiscard]] bool is_valid(const VTableLayoutKey& key) noexcept;
[[nodiscard]] bool is_valid(const TraitObjectCoercionKey& key) noexcept;

[[nodiscard]] TraitObjectTypeKey trait_object_type_key(DefKey principal_trait,
    std::span<const CanonicalTypeKey> trait_args,
    std::span<const TraitObjectAssociatedTypeEqualityKey> associated_equalities,
    StableFingerprint128 object_origin,
    StableFingerprint128 object_callability_schema,
    TraitObjectAbiPolicyKey abi_policy = TraitObjectAbiPolicyKey::borrowed_view_v1);
[[nodiscard]] VTableLayoutKey vtable_layout_key(CanonicalTypeKey concrete_type,
    TraitObjectTypeKey object_type,
    StableFingerprint128 slot_schema,
    StableFingerprint128 impl_evidence,
    base::u32 method_slot_count,
    TraitObjectMetadataPolicyKey metadata_policy = TraitObjectMetadataPolicyKey::borrowed_methods_only_v1,
    TraitObjectAbiPolicyKey abi_policy = TraitObjectAbiPolicyKey::borrowed_view_v1);
[[nodiscard]] TraitObjectCoercionKey trait_object_coercion_key(CanonicalTypeKey source_type,
    StableFingerprint128 source_origin,
    TraitObjectTypeKey target_object_type,
    VTableLayoutKey vtable_layout,
    TraitObjectBorrowKindKey borrow_kind);

void append_stable_key(StableKeyWriter& writer, const TraitObjectTypeKey& key);
void append_stable_key(StableKeyWriter& writer, const VTableLayoutKey& key);
void append_stable_key(StableKeyWriter& writer, const TraitObjectCoercionKey& key);

[[nodiscard]] std::string stable_serialize(const TraitObjectTypeKey& key);
[[nodiscard]] std::string stable_serialize(const VTableLayoutKey& key);
[[nodiscard]] std::string stable_serialize(const TraitObjectCoercionKey& key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(const TraitObjectTypeKey& key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(const VTableLayoutKey& key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(const TraitObjectCoercionKey& key);
[[nodiscard]] std::string debug_string(const TraitObjectTypeKey& key);
[[nodiscard]] std::string debug_string(const VTableLayoutKey& key);
[[nodiscard]] std::string debug_string(const TraitObjectCoercionKey& key);

struct TraitObjectTypeKeyHash {
    [[nodiscard]] std::size_t operator()(const TraitObjectTypeKey& key) const;
};

struct VTableLayoutKeyHash {
    [[nodiscard]] std::size_t operator()(const VTableLayoutKey& key) const;
};

struct TraitObjectCoercionKeyHash {
    [[nodiscard]] std::size_t operator()(const TraitObjectCoercionKey& key) const;
};

} // namespace aurex::query
