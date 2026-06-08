#pragma once

#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>
#include <aurex/infrastructure/query/trait_object_key.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

enum class DynAbiPolicy : base::u8 {
    borrowed_view_v1 = 1,
};

enum class DynMetadataPolicy : base::u8 {
    borrowed_methods_only_v1 = 1,
    supertrait_vptr_metadata_v1 = 2,
};

enum class DynBorrowKind : base::u8 {
    shared = 0,
    mut,
};

struct DynObjectAbiDescriptor {
    TraitObjectTypeKey object_type;
    DynAbiPolicy abi_policy = DynAbiPolicy::borrowed_view_v1;
    std::string object_type_name;
    std::string principal_trait_name;
};

struct DynVTableSlotAbiDescriptor {
    base::u32 slot = 0;
    base::u32 requirement_ordinal = 0;
    std::string method_name;
    std::string function_symbol;
    std::string function_type_name;
    std::string receiver_type_name;
    std::string return_type_name;
};

struct DynVTableAbiDescriptor {
    VTableLayoutKey layout;
    DynAbiPolicy abi_policy = DynAbiPolicy::borrowed_view_v1;
    DynMetadataPolicy metadata_policy = DynMetadataPolicy::borrowed_methods_only_v1;
    std::string symbol;
    std::string concrete_type_name;
    std::string object_type_name;
    std::vector<DynVTableSlotAbiDescriptor> slots;
};

struct DynCoercionAbiDescriptor {
    TraitObjectCoercionKey coercion;
    VTableLayoutKey layout;
    DynBorrowKind borrow_kind = DynBorrowKind::shared;
    std::string source_reference_type_name;
    std::string target_reference_type_name;
    std::string source_type_name;
    std::string object_type_name;
};

struct DynUpcastAbiDescriptor {
    TraitObjectUpcastCoercionKey upcast;
    TraitObjectTypeKey source_object;
    TraitObjectTypeKey target_object;
    StableFingerprint128 edge_path;
    DynBorrowKind borrow_kind = DynBorrowKind::shared;
    DynAbiPolicy abi_policy = DynAbiPolicy::borrowed_view_v1;
    DynMetadataPolicy metadata_policy = DynMetadataPolicy::supertrait_vptr_metadata_v1;
    std::string source_reference_type_name;
    std::string target_reference_type_name;
    std::string source_object_type_name;
    std::string target_object_type_name;
};

struct DynDispatchAbiDescriptor {
    VTableLayoutKey layout;
    base::u32 slot = 0;
    std::string method_name;
    std::string function_symbol;
    std::string function_type_name;
    TraitObjectTypeKey object_type;
    std::string object_type_name;
};

struct DynAbiFactsSummary {
    base::u64 object_count = 0;
    base::u64 vtable_count = 0;
    base::u64 slot_count = 0;
    base::u64 coercion_count = 0;
    base::u64 upcast_count = 0;
    base::u64 dispatch_count = 0;
    base::u64 shared_borrow_count = 0;
    base::u64 mut_borrow_count = 0;
};

struct FunctionDynAbiFacts {
    std::string symbol;
    std::vector<DynObjectAbiDescriptor> objects;
    std::vector<DynVTableAbiDescriptor> vtables;
    std::vector<DynCoercionAbiDescriptor> coercions;
    std::vector<DynUpcastAbiDescriptor> upcasts;
    std::vector<DynDispatchAbiDescriptor> dispatches;
    DynAbiFactsSummary summary;
    StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view dyn_abi_policy_name(DynAbiPolicy policy) noexcept;
[[nodiscard]] std::string_view dyn_metadata_policy_name(DynMetadataPolicy policy) noexcept;
[[nodiscard]] std::string_view dyn_borrow_kind_name(DynBorrowKind kind) noexcept;
[[nodiscard]] bool is_valid(DynAbiPolicy policy) noexcept;
[[nodiscard]] bool is_valid(DynMetadataPolicy policy) noexcept;
[[nodiscard]] bool is_valid(DynBorrowKind kind) noexcept;
[[nodiscard]] bool is_valid(const DynObjectAbiDescriptor& descriptor) noexcept;
[[nodiscard]] bool is_valid(const DynVTableSlotAbiDescriptor& descriptor) noexcept;
[[nodiscard]] bool is_valid(const DynVTableAbiDescriptor& descriptor) noexcept;
[[nodiscard]] bool is_valid(const DynCoercionAbiDescriptor& descriptor) noexcept;
[[nodiscard]] bool is_valid(const DynUpcastAbiDescriptor& descriptor) noexcept;
[[nodiscard]] bool is_valid(const DynDispatchAbiDescriptor& descriptor) noexcept;
[[nodiscard]] bool is_valid(const FunctionDynAbiFacts& facts) noexcept;

[[nodiscard]] DynAbiPolicy dyn_abi_policy_from_key(TraitObjectAbiPolicyKey policy) noexcept;
[[nodiscard]] DynMetadataPolicy dyn_metadata_policy_from_key(TraitObjectMetadataPolicyKey policy) noexcept;
[[nodiscard]] DynBorrowKind dyn_borrow_kind_from_key(TraitObjectBorrowKindKey kind) noexcept;
[[nodiscard]] TraitObjectBorrowKindKey dyn_borrow_kind_to_key(DynBorrowKind kind) noexcept;

void record_dyn_object_abi_descriptor(FunctionDynAbiFacts& facts, DynObjectAbiDescriptor descriptor);
void record_dyn_vtable_abi_descriptor(FunctionDynAbiFacts& facts, DynVTableAbiDescriptor descriptor);
void record_dyn_coercion_abi_descriptor(FunctionDynAbiFacts& facts, DynCoercionAbiDescriptor descriptor);
void record_dyn_upcast_abi_descriptor(FunctionDynAbiFacts& facts, DynUpcastAbiDescriptor descriptor);
void record_dyn_dispatch_abi_descriptor(FunctionDynAbiFacts& facts, DynDispatchAbiDescriptor descriptor);

[[nodiscard]] std::optional<const DynVTableAbiDescriptor*> dyn_vtable_descriptor_for_layout(
    const FunctionDynAbiFacts& facts, const VTableLayoutKey& layout) noexcept;
[[nodiscard]] std::optional<const DynVTableSlotAbiDescriptor*> dyn_vtable_slot_descriptor(
    const DynVTableAbiDescriptor& vtable, base::u32 slot) noexcept;

[[nodiscard]] StableFingerprint128 function_dyn_abi_facts_fingerprint(
    const FunctionDynAbiFacts& facts) noexcept;
[[nodiscard]] std::string summarize_function_dyn_abi_facts(const FunctionDynAbiFacts& facts);
[[nodiscard]] std::string dump_function_dyn_abi_facts(const FunctionDynAbiFacts& facts);

} // namespace aurex::query
