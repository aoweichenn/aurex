#include <aurex/infrastructure/query/trait_object_key.hpp>

#include <algorithm>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr base::u64 QUERY_TRAIT_OBJECT_TYPE_KEY_MARKER = 0x51544f4254593031ULL;
constexpr base::u64 QUERY_VTABLE_LAYOUT_KEY_MARKER = 0x515654424c303131ULL;
constexpr base::u64 QUERY_TRAIT_OBJECT_COERCION_KEY_MARKER = 0x5154434f45524331ULL;

[[nodiscard]] base::u64 global_id_from_fingerprint(
    const base::u64 marker, const StableFingerprint128 fingerprint) noexcept
{
    base::u64 global_id = stable_mix(marker, fingerprint.primary);
    global_id = stable_mix(global_id, fingerprint.secondary);
    global_id = stable_mix(global_id, fingerprint.byte_count);
    return global_id == 0 ? marker : global_id;
}

[[nodiscard]] bool is_nonempty_fingerprint(const StableFingerprint128 fingerprint) noexcept
{
    return fingerprint.byte_count != 0;
}

[[nodiscard]] bool is_trait_definition_key(const DefKey key) noexcept
{
    return is_valid(key) && key.name_space == DefNamespace::trait_ && key.kind == DefKind::trait_;
}

[[nodiscard]] bool is_associated_type_member_key(const MemberKey key) noexcept
{
    return is_valid(key) && key.kind == MemberKind::associated_type;
}

[[nodiscard]] bool is_valid_abi_policy(const TraitObjectAbiPolicyKey policy) noexcept
{
    return policy == TraitObjectAbiPolicyKey::borrowed_view_v1;
}

[[nodiscard]] bool is_valid_metadata_policy(const TraitObjectMetadataPolicyKey policy) noexcept
{
    return policy == TraitObjectMetadataPolicyKey::borrowed_methods_only_v1;
}

[[nodiscard]] bool is_valid_borrow_kind(const TraitObjectBorrowKindKey borrow_kind) noexcept
{
    return borrow_kind == TraitObjectBorrowKindKey::shared || borrow_kind == TraitObjectBorrowKindKey::mut;
}

[[nodiscard]] bool associated_type_equality_less(
    const TraitObjectAssociatedTypeEqualityKey& lhs, const TraitObjectAssociatedTypeEqualityKey& rhs) noexcept
{
    return lhs.associated_type.global_id < rhs.associated_type.global_id;
}

void normalize_associated_equalities(std::vector<TraitObjectAssociatedTypeEqualityKey>& equalities)
{
    std::sort(equalities.begin(), equalities.end(), associated_type_equality_less);
}

[[nodiscard]] bool associated_equalities_are_valid_and_unique(
    const std::vector<TraitObjectAssociatedTypeEqualityKey>& equalities) noexcept
{
    base::u64 previous_member_global_id = 0;
    for (const TraitObjectAssociatedTypeEqualityKey& equality : equalities) {
        if (!is_valid(equality) || equality.associated_type.global_id == previous_member_global_id) {
            return false;
        }
        previous_member_global_id = equality.associated_type.global_id;
    }
    return true;
}

[[nodiscard]] bool canonical_type_keys_are_valid(const std::vector<CanonicalTypeKey>& keys) noexcept
{
    return std::all_of(keys.begin(), keys.end(), [](const CanonicalTypeKey& key) {
        return is_valid(key);
    });
}

void append_trait_object_type_payload(StableKeyWriter& writer, const TraitObjectTypeKey& key)
{
    writer.write_u32(key.schema);
    writer.write_u8(static_cast<base::u8>(key.abi_policy));
    append_stable_key(writer, key.principal_trait);
    writer.write_fingerprint(key.object_origin);
    writer.write_fingerprint(key.object_callability_schema);
    writer.write_u64(static_cast<base::u64>(key.trait_args.size()));
    for (const CanonicalTypeKey& trait_arg : key.trait_args) {
        append_stable_key(writer, trait_arg);
    }
    writer.write_u64(static_cast<base::u64>(key.associated_equalities.size()));
    for (const TraitObjectAssociatedTypeEqualityKey& equality : key.associated_equalities) {
        append_stable_key(writer, equality.associated_type);
        append_stable_key(writer, equality.value_type);
    }
}

void append_vtable_layout_payload(StableKeyWriter& writer, const VTableLayoutKey& key)
{
    writer.write_u32(key.schema);
    writer.write_u8(static_cast<base::u8>(key.abi_policy));
    writer.write_u8(static_cast<base::u8>(key.metadata_policy));
    append_stable_key(writer, key.concrete_type);
    append_stable_key(writer, key.object_type);
    writer.write_fingerprint(key.slot_schema);
    writer.write_fingerprint(key.impl_evidence);
    writer.write_u32(key.method_slot_count);
}

void append_trait_object_coercion_payload(StableKeyWriter& writer, const TraitObjectCoercionKey& key)
{
    writer.write_u32(key.schema);
    writer.write_u8(static_cast<base::u8>(key.borrow_kind));
    append_stable_key(writer, key.source_type);
    writer.write_fingerprint(key.source_origin);
    append_stable_key(writer, key.target_object_type);
    append_stable_key(writer, key.vtable_layout);
}

template <typename Key>
[[nodiscard]] StableFingerprint128 payload_fingerprint(void (*append_payload)(StableKeyWriter&, const Key&),
    const Key& key)
{
    StableKeyWriter writer;
    append_payload(writer, key);
    return writer.fingerprint();
}

} // namespace

bool operator==(const TraitObjectTypeKey& lhs, const TraitObjectTypeKey& rhs) noexcept
{
    return lhs.principal_trait == rhs.principal_trait && lhs.trait_args == rhs.trait_args
        && lhs.associated_equalities == rhs.associated_equalities && lhs.object_origin == rhs.object_origin
        && lhs.object_callability_schema == rhs.object_callability_schema && lhs.abi_policy == rhs.abi_policy
        && lhs.schema == rhs.schema && lhs.global_id == rhs.global_id;
}

bool operator!=(const TraitObjectTypeKey& lhs, const TraitObjectTypeKey& rhs) noexcept
{
    return !(lhs == rhs);
}

bool operator==(const VTableLayoutKey& lhs, const VTableLayoutKey& rhs) noexcept
{
    return lhs.concrete_type == rhs.concrete_type && lhs.object_type == rhs.object_type
        && lhs.slot_schema == rhs.slot_schema && lhs.impl_evidence == rhs.impl_evidence
        && lhs.method_slot_count == rhs.method_slot_count && lhs.metadata_policy == rhs.metadata_policy
        && lhs.abi_policy == rhs.abi_policy && lhs.schema == rhs.schema && lhs.global_id == rhs.global_id;
}

bool operator!=(const VTableLayoutKey& lhs, const VTableLayoutKey& rhs) noexcept
{
    return !(lhs == rhs);
}

bool operator==(const TraitObjectCoercionKey& lhs, const TraitObjectCoercionKey& rhs) noexcept
{
    return lhs.source_type == rhs.source_type && lhs.source_origin == rhs.source_origin
        && lhs.target_object_type == rhs.target_object_type && lhs.vtable_layout == rhs.vtable_layout
        && lhs.borrow_kind == rhs.borrow_kind && lhs.schema == rhs.schema && lhs.global_id == rhs.global_id;
}

bool operator!=(const TraitObjectCoercionKey& lhs, const TraitObjectCoercionKey& rhs) noexcept
{
    return !(lhs == rhs);
}

bool is_valid(const TraitObjectAssociatedTypeEqualityKey& key) noexcept
{
    return is_associated_type_member_key(key.associated_type) && is_valid(key.value_type);
}

bool is_valid(const TraitObjectTypeKey& key) noexcept
{
    return key.schema == QUERY_TRAIT_OBJECT_TYPE_KEY_SCHEMA_VERSION && is_valid_abi_policy(key.abi_policy)
        && is_trait_definition_key(key.principal_trait) && canonical_type_keys_are_valid(key.trait_args)
        && associated_equalities_are_valid_and_unique(key.associated_equalities)
        && is_nonempty_fingerprint(key.object_origin) && is_nonempty_fingerprint(key.object_callability_schema)
        && key.global_id != 0;
}

bool is_valid(const VTableLayoutKey& key) noexcept
{
    return key.schema == QUERY_VTABLE_LAYOUT_KEY_SCHEMA_VERSION && is_valid_abi_policy(key.abi_policy)
        && is_valid_metadata_policy(key.metadata_policy) && is_valid(key.concrete_type) && is_valid(key.object_type)
        && is_nonempty_fingerprint(key.slot_schema) && is_nonempty_fingerprint(key.impl_evidence)
        && key.object_type.object_callability_schema == key.slot_schema && key.object_type.abi_policy == key.abi_policy
        && key.global_id != 0;
}

bool is_valid(const TraitObjectCoercionKey& key) noexcept
{
    return key.schema == QUERY_TRAIT_OBJECT_COERCION_KEY_SCHEMA_VERSION && is_valid_borrow_kind(key.borrow_kind)
        && is_valid(key.source_type) && is_nonempty_fingerprint(key.source_origin)
        && is_valid(key.target_object_type) && is_valid(key.vtable_layout)
        && key.target_object_type.object_origin == key.source_origin
        && key.vtable_layout.object_type == key.target_object_type
        && key.vtable_layout.concrete_type == key.source_type && key.global_id != 0;
}

TraitObjectTypeKey trait_object_type_key(const DefKey principal_trait,
    const std::span<const CanonicalTypeKey> trait_args,
    const std::span<const TraitObjectAssociatedTypeEqualityKey> associated_equalities,
    const StableFingerprint128 object_origin,
    const StableFingerprint128 object_callability_schema,
    const TraitObjectAbiPolicyKey abi_policy)
{
    TraitObjectTypeKey key;
    key.principal_trait = principal_trait;
    key.trait_args.assign(trait_args.begin(), trait_args.end());
    key.associated_equalities.assign(associated_equalities.begin(), associated_equalities.end());
    normalize_associated_equalities(key.associated_equalities);
    key.object_origin = object_origin;
    key.object_callability_schema = object_callability_schema;
    key.abi_policy = abi_policy;
    key.schema = QUERY_TRAIT_OBJECT_TYPE_KEY_SCHEMA_VERSION;
    if (key.schema == QUERY_TRAIT_OBJECT_TYPE_KEY_SCHEMA_VERSION && is_valid_abi_policy(key.abi_policy)
        && is_trait_definition_key(key.principal_trait) && canonical_type_keys_are_valid(key.trait_args)
        && associated_equalities_are_valid_and_unique(key.associated_equalities)
        && is_nonempty_fingerprint(key.object_origin) && is_nonempty_fingerprint(key.object_callability_schema)) {
        key.global_id =
            global_id_from_fingerprint(QUERY_TRAIT_OBJECT_TYPE_KEY_MARKER,
                payload_fingerprint<TraitObjectTypeKey>(append_trait_object_type_payload, key));
    }
    return key;
}

VTableLayoutKey vtable_layout_key(CanonicalTypeKey concrete_type,
    TraitObjectTypeKey object_type,
    const StableFingerprint128 slot_schema,
    const StableFingerprint128 impl_evidence,
    const base::u32 method_slot_count,
    const TraitObjectMetadataPolicyKey metadata_policy,
    const TraitObjectAbiPolicyKey abi_policy)
{
    VTableLayoutKey key;
    key.concrete_type = std::move(concrete_type);
    key.object_type = std::move(object_type);
    key.slot_schema = slot_schema;
    key.impl_evidence = impl_evidence;
    key.method_slot_count = method_slot_count;
    key.metadata_policy = metadata_policy;
    key.abi_policy = abi_policy;
    key.schema = QUERY_VTABLE_LAYOUT_KEY_SCHEMA_VERSION;
    if (key.schema == QUERY_VTABLE_LAYOUT_KEY_SCHEMA_VERSION && is_valid_abi_policy(key.abi_policy)
        && is_valid_metadata_policy(key.metadata_policy) && is_valid(key.concrete_type) && is_valid(key.object_type)
        && is_nonempty_fingerprint(key.slot_schema) && is_nonempty_fingerprint(key.impl_evidence)
        && key.object_type.object_callability_schema == key.slot_schema && key.object_type.abi_policy == key.abi_policy) {
        key.global_id =
            global_id_from_fingerprint(QUERY_VTABLE_LAYOUT_KEY_MARKER,
                payload_fingerprint<VTableLayoutKey>(append_vtable_layout_payload, key));
    }
    return key;
}

TraitObjectCoercionKey trait_object_coercion_key(CanonicalTypeKey source_type,
    const StableFingerprint128 source_origin,
    TraitObjectTypeKey target_object_type,
    VTableLayoutKey vtable_layout,
    const TraitObjectBorrowKindKey borrow_kind)
{
    TraitObjectCoercionKey key;
    key.source_type = std::move(source_type);
    key.source_origin = source_origin;
    key.target_object_type = std::move(target_object_type);
    key.vtable_layout = std::move(vtable_layout);
    key.borrow_kind = borrow_kind;
    key.schema = QUERY_TRAIT_OBJECT_COERCION_KEY_SCHEMA_VERSION;
    if (key.schema == QUERY_TRAIT_OBJECT_COERCION_KEY_SCHEMA_VERSION && is_valid_borrow_kind(key.borrow_kind)
        && is_valid(key.source_type) && is_nonempty_fingerprint(key.source_origin)
        && is_valid(key.target_object_type) && is_valid(key.vtable_layout)
        && key.target_object_type.object_origin == key.source_origin
        && key.vtable_layout.object_type == key.target_object_type
        && key.vtable_layout.concrete_type == key.source_type) {
        key.global_id =
            global_id_from_fingerprint(QUERY_TRAIT_OBJECT_COERCION_KEY_MARKER,
                payload_fingerprint<TraitObjectCoercionKey>(append_trait_object_coercion_payload, key));
    }
    return key;
}

void append_stable_key(StableKeyWriter& writer, const TraitObjectTypeKey& key)
{
    writer.write_u64(QUERY_TRAIT_OBJECT_TYPE_KEY_MARKER);
    append_trait_object_type_payload(writer, key);
    writer.write_u64(key.global_id);
}

void append_stable_key(StableKeyWriter& writer, const VTableLayoutKey& key)
{
    writer.write_u64(QUERY_VTABLE_LAYOUT_KEY_MARKER);
    append_vtable_layout_payload(writer, key);
    writer.write_u64(key.global_id);
}

void append_stable_key(StableKeyWriter& writer, const TraitObjectCoercionKey& key)
{
    writer.write_u64(QUERY_TRAIT_OBJECT_COERCION_KEY_MARKER);
    append_trait_object_coercion_payload(writer, key);
    writer.write_u64(key.global_id);
}

std::string stable_serialize(const TraitObjectTypeKey& key)
{
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.storage();
}

std::string stable_serialize(const VTableLayoutKey& key)
{
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.storage();
}

std::string stable_serialize(const TraitObjectCoercionKey& key)
{
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.storage();
}

StableFingerprint128 stable_key_fingerprint(const TraitObjectTypeKey& key)
{
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.fingerprint();
}

StableFingerprint128 stable_key_fingerprint(const VTableLayoutKey& key)
{
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.fingerprint();
}

StableFingerprint128 stable_key_fingerprint(const TraitObjectCoercionKey& key)
{
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.fingerprint();
}

std::string debug_string(const TraitObjectTypeKey& key)
{
    std::ostringstream out;
    out << "TraitObjectTypeKey{global=" << key.global_id
        << ",fingerprint=" << debug_string(stable_key_fingerprint(key))
        << ",associated_equalities=" << key.associated_equalities.size() << '}';
    return out.str();
}

std::string debug_string(const VTableLayoutKey& key)
{
    std::ostringstream out;
    out << "VTableLayoutKey{global=" << key.global_id
        << ",fingerprint=" << debug_string(stable_key_fingerprint(key))
        << ",method_slots=" << key.method_slot_count << '}';
    return out.str();
}

std::string debug_string(const TraitObjectCoercionKey& key)
{
    std::ostringstream out;
    out << "TraitObjectCoercionKey{global=" << key.global_id
        << ",fingerprint=" << debug_string(stable_key_fingerprint(key)) << '}';
    return out.str();
}

std::size_t TraitObjectTypeKeyHash::operator()(const TraitObjectTypeKey& key) const
{
    return stable_hash_value(stable_key_fingerprint(key));
}

std::size_t VTableLayoutKeyHash::operator()(const VTableLayoutKey& key) const
{
    return stable_hash_value(stable_key_fingerprint(key));
}

std::size_t TraitObjectCoercionKeyHash::operator()(const TraitObjectCoercionKey& key) const
{
    return stable_hash_value(stable_key_fingerprint(key));
}

} // namespace aurex::query
