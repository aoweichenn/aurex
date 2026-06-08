#include <aurex/infrastructure/query/dyn_abi_facts.hpp>

#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_DYN_ABI_FACTS_FINGERPRINT_MARKER = "query.dyn_abi_facts.v1";
constexpr base::u8 QUERY_DYN_ABI_INVALID_POLICY_VALUE = 255U;
constexpr base::u8 QUERY_DYN_METADATA_INVALID_POLICY_VALUE = 255U;
constexpr base::u8 QUERY_DYN_BORROW_INVALID_KIND_VALUE = 255U;
constexpr base::u32 QUERY_DYN_ABI_INVALID_SLOT = std::numeric_limits<base::u32>::max();

[[nodiscard]] bool is_valid_slot_index(const base::u32 slot, const base::u32 method_slot_count) noexcept
{
    return slot < method_slot_count;
}

[[nodiscard]] base::u8 stable_dyn_abi_policy_value(const DynAbiPolicy policy) noexcept
{
    return is_valid(policy) ? static_cast<base::u8>(policy) : QUERY_DYN_ABI_INVALID_POLICY_VALUE;
}

[[nodiscard]] base::u8 stable_dyn_metadata_policy_value(const DynMetadataPolicy policy) noexcept
{
    return is_valid(policy) ? static_cast<base::u8>(policy) : QUERY_DYN_METADATA_INVALID_POLICY_VALUE;
}

[[nodiscard]] base::u8 stable_dyn_borrow_kind_value(const DynBorrowKind kind) noexcept
{
    return is_valid(kind) ? static_cast<base::u8>(kind) : QUERY_DYN_BORROW_INVALID_KIND_VALUE;
}

void mix_dyn_object(StableHashBuilder& builder, const DynObjectAbiDescriptor& descriptor) noexcept
{
    builder.mix_u64(descriptor.object_type.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(descriptor.object_type));
    builder.mix_u8(stable_dyn_abi_policy_value(descriptor.abi_policy));
    builder.mix_string(descriptor.object_type_name);
    builder.mix_string(descriptor.principal_trait_name);
}

void mix_dyn_vtable_slot(StableHashBuilder& builder,
    const DynVTableSlotAbiDescriptor& descriptor) noexcept
{
    builder.mix_u32(descriptor.slot);
    builder.mix_u32(descriptor.requirement_ordinal);
    builder.mix_string(descriptor.method_name);
    builder.mix_string(descriptor.function_symbol);
    builder.mix_string(descriptor.function_type_name);
    builder.mix_string(descriptor.receiver_type_name);
    builder.mix_string(descriptor.return_type_name);
}

void mix_dyn_vtable(StableHashBuilder& builder, const DynVTableAbiDescriptor& descriptor) noexcept
{
    builder.mix_u64(descriptor.layout.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(descriptor.layout));
    builder.mix_u8(stable_dyn_abi_policy_value(descriptor.abi_policy));
    builder.mix_u8(stable_dyn_metadata_policy_value(descriptor.metadata_policy));
    builder.mix_string(descriptor.symbol);
    builder.mix_string(descriptor.concrete_type_name);
    builder.mix_string(descriptor.object_type_name);
    builder.mix_u64(descriptor.slots.size());
    for (const DynVTableSlotAbiDescriptor& slot : descriptor.slots) {
        mix_dyn_vtable_slot(builder, slot);
    }
}

void mix_dyn_coercion(StableHashBuilder& builder, const DynCoercionAbiDescriptor& descriptor) noexcept
{
    builder.mix_u64(descriptor.coercion.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(descriptor.coercion));
    builder.mix_u64(descriptor.layout.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(descriptor.layout));
    builder.mix_u8(stable_dyn_borrow_kind_value(descriptor.borrow_kind));
    builder.mix_string(descriptor.source_reference_type_name);
    builder.mix_string(descriptor.target_reference_type_name);
    builder.mix_string(descriptor.source_type_name);
    builder.mix_string(descriptor.object_type_name);
}

void mix_dyn_upcast(StableHashBuilder& builder, const DynUpcastAbiDescriptor& descriptor) noexcept
{
    builder.mix_u64(descriptor.upcast.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(descriptor.upcast));
    builder.mix_u64(descriptor.source_object.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(descriptor.source_object));
    builder.mix_u64(descriptor.target_object.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(descriptor.target_object));
    builder.mix_fingerprint(descriptor.edge_path);
    builder.mix_u8(stable_dyn_borrow_kind_value(descriptor.borrow_kind));
    builder.mix_u8(stable_dyn_abi_policy_value(descriptor.abi_policy));
    builder.mix_u8(stable_dyn_metadata_policy_value(descriptor.metadata_policy));
    builder.mix_string(descriptor.source_reference_type_name);
    builder.mix_string(descriptor.target_reference_type_name);
    builder.mix_string(descriptor.source_object_type_name);
    builder.mix_string(descriptor.target_object_type_name);
}

void mix_dyn_dispatch(StableHashBuilder& builder, const DynDispatchAbiDescriptor& descriptor) noexcept
{
    builder.mix_u64(descriptor.layout.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(descriptor.layout));
    builder.mix_u64(descriptor.object_type.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(descriptor.object_type));
    builder.mix_u32(descriptor.slot);
    builder.mix_string(descriptor.method_name);
    builder.mix_string(descriptor.function_symbol);
    builder.mix_string(descriptor.function_type_name);
    builder.mix_string(descriptor.object_type_name);
}

[[nodiscard]] bool dyn_vtable_slots_are_valid(const DynVTableAbiDescriptor& descriptor) noexcept
{
    base::u32 previous_slot = 0;
    bool has_previous = false;
    for (const DynVTableSlotAbiDescriptor& slot : descriptor.slots) {
        if (!is_valid(slot) || !is_valid_slot_index(slot.slot, descriptor.layout.method_slot_count)) {
            return false;
        }
        if (has_previous && slot.slot <= previous_slot) {
            return false;
        }
        previous_slot = slot.slot;
        has_previous = true;
    }
    return descriptor.slots.size() == descriptor.layout.method_slot_count;
}

void count_borrow_kind(DynAbiFactsSummary& summary, const DynBorrowKind kind) noexcept
{
    switch (kind) {
        case DynBorrowKind::shared:
            ++summary.shared_borrow_count;
            break;
        case DynBorrowKind::mut:
            ++summary.mut_borrow_count;
            break;
    }
}

} // namespace

std::string_view dyn_abi_policy_name(const DynAbiPolicy policy) noexcept
{
    switch (policy) {
        case DynAbiPolicy::borrowed_view_v1:
            return "borrowed_view_v1";
    }
    return "invalid";
}

std::string_view dyn_metadata_policy_name(const DynMetadataPolicy policy) noexcept
{
    switch (policy) {
        case DynMetadataPolicy::borrowed_methods_only_v1:
            return "borrowed_methods_only_v1";
        case DynMetadataPolicy::supertrait_vptr_metadata_v1:
            return "supertrait_vptr_metadata_v1";
    }
    return "invalid";
}

std::string_view dyn_borrow_kind_name(const DynBorrowKind kind) noexcept
{
    switch (kind) {
        case DynBorrowKind::shared:
            return "shared";
        case DynBorrowKind::mut:
            return "mut";
    }
    return "invalid";
}

bool is_valid(const DynAbiPolicy policy) noexcept
{
    return policy == DynAbiPolicy::borrowed_view_v1;
}

bool is_valid(const DynMetadataPolicy policy) noexcept
{
    return policy == DynMetadataPolicy::borrowed_methods_only_v1
        || policy == DynMetadataPolicy::supertrait_vptr_metadata_v1;
}

bool is_valid(const DynBorrowKind kind) noexcept
{
    return kind == DynBorrowKind::shared || kind == DynBorrowKind::mut;
}

bool is_valid(const DynObjectAbiDescriptor& descriptor) noexcept
{
    return is_valid(descriptor.object_type) && is_valid(descriptor.abi_policy)
        && descriptor.object_type.abi_policy == TraitObjectAbiPolicyKey::borrowed_view_v1;
}

bool is_valid(const DynVTableSlotAbiDescriptor& descriptor) noexcept
{
    return !descriptor.method_name.empty() && !descriptor.function_type_name.empty()
        && !descriptor.receiver_type_name.empty() && !descriptor.return_type_name.empty();
}

bool is_valid(const DynVTableAbiDescriptor& descriptor) noexcept
{
    return is_valid(descriptor.layout) && is_valid(descriptor.abi_policy) && is_valid(descriptor.metadata_policy)
        && descriptor.layout.abi_policy == TraitObjectAbiPolicyKey::borrowed_view_v1
        && descriptor.metadata_policy == dyn_metadata_policy_from_key(descriptor.layout.metadata_policy)
        && descriptor.layout.method_slot_count == descriptor.slots.size() && dyn_vtable_slots_are_valid(descriptor);
}

bool is_valid(const DynCoercionAbiDescriptor& descriptor) noexcept
{
    return is_valid(descriptor.coercion) && is_valid(descriptor.layout) && is_valid(descriptor.borrow_kind)
        && descriptor.coercion.vtable_layout == descriptor.layout
        && descriptor.coercion.borrow_kind == dyn_borrow_kind_to_key(descriptor.borrow_kind);
}

bool is_valid(const DynUpcastAbiDescriptor& descriptor) noexcept
{
    return is_valid(descriptor.upcast) && is_valid(descriptor.source_object) && is_valid(descriptor.target_object)
        && is_valid(descriptor.borrow_kind) && is_valid(descriptor.abi_policy) && is_valid(descriptor.metadata_policy)
        && descriptor.upcast.source_object_type == descriptor.source_object
        && descriptor.upcast.target_object_type == descriptor.target_object
        && descriptor.upcast.supertrait_edge_path == descriptor.edge_path
        && descriptor.upcast.borrow_kind == dyn_borrow_kind_to_key(descriptor.borrow_kind)
        && descriptor.abi_policy == DynAbiPolicy::borrowed_view_v1
        && descriptor.metadata_policy == DynMetadataPolicy::supertrait_vptr_metadata_v1;
}

bool is_valid(const DynDispatchAbiDescriptor& descriptor) noexcept
{
    if (is_valid(descriptor.layout)) {
        return is_valid_slot_index(descriptor.slot, descriptor.layout.method_slot_count);
    }
    if (is_valid(descriptor.object_type)) {
        return descriptor.slot != QUERY_DYN_ABI_INVALID_SLOT && !descriptor.method_name.empty();
    }
    return false;
}

bool is_valid(const FunctionDynAbiFacts& facts) noexcept
{
    return std::all_of(facts.objects.begin(), facts.objects.end(), [](const DynObjectAbiDescriptor& descriptor) {
        return is_valid(descriptor);
    }) && std::all_of(facts.vtables.begin(), facts.vtables.end(), [](const DynVTableAbiDescriptor& descriptor) {
        return is_valid(descriptor);
    }) && std::all_of(facts.coercions.begin(), facts.coercions.end(), [](const DynCoercionAbiDescriptor& descriptor) {
        return is_valid(descriptor);
    }) && std::all_of(facts.upcasts.begin(), facts.upcasts.end(), [](const DynUpcastAbiDescriptor& descriptor) {
        return is_valid(descriptor);
    }) && std::all_of(facts.dispatches.begin(), facts.dispatches.end(),
             [](const DynDispatchAbiDescriptor& descriptor) {
                 return is_valid(descriptor);
             });
}

DynAbiPolicy dyn_abi_policy_from_key(const TraitObjectAbiPolicyKey policy) noexcept
{
    switch (policy) {
        case TraitObjectAbiPolicyKey::borrowed_view_v1:
            return DynAbiPolicy::borrowed_view_v1;
    }
    return DynAbiPolicy::borrowed_view_v1;
}

DynMetadataPolicy dyn_metadata_policy_from_key(const TraitObjectMetadataPolicyKey policy) noexcept
{
    switch (policy) {
        case TraitObjectMetadataPolicyKey::borrowed_methods_only_v1:
            return DynMetadataPolicy::borrowed_methods_only_v1;
        case TraitObjectMetadataPolicyKey::supertrait_vptr_metadata_v1:
            return DynMetadataPolicy::supertrait_vptr_metadata_v1;
    }
    return DynMetadataPolicy::borrowed_methods_only_v1;
}

DynBorrowKind dyn_borrow_kind_from_key(const TraitObjectBorrowKindKey kind) noexcept
{
    switch (kind) {
        case TraitObjectBorrowKindKey::shared:
            return DynBorrowKind::shared;
        case TraitObjectBorrowKindKey::mut:
            return DynBorrowKind::mut;
    }
    return DynBorrowKind::shared;
}

TraitObjectBorrowKindKey dyn_borrow_kind_to_key(const DynBorrowKind kind) noexcept
{
    switch (kind) {
        case DynBorrowKind::shared:
            return TraitObjectBorrowKindKey::shared;
        case DynBorrowKind::mut:
            return TraitObjectBorrowKindKey::mut;
    }
    return TraitObjectBorrowKindKey::shared;
}

void record_dyn_object_abi_descriptor(FunctionDynAbiFacts& facts, DynObjectAbiDescriptor descriptor)
{
    facts.objects.push_back(std::move(descriptor));
    facts.summary.object_count = facts.objects.size();
}

void record_dyn_vtable_abi_descriptor(FunctionDynAbiFacts& facts, DynVTableAbiDescriptor descriptor)
{
    facts.summary.slot_count += descriptor.slots.size();
    facts.vtables.push_back(std::move(descriptor));
    facts.summary.vtable_count = facts.vtables.size();
}

void record_dyn_coercion_abi_descriptor(FunctionDynAbiFacts& facts, DynCoercionAbiDescriptor descriptor)
{
    count_borrow_kind(facts.summary, descriptor.borrow_kind);
    facts.coercions.push_back(std::move(descriptor));
    facts.summary.coercion_count = facts.coercions.size();
}

void record_dyn_upcast_abi_descriptor(FunctionDynAbiFacts& facts, DynUpcastAbiDescriptor descriptor)
{
    count_borrow_kind(facts.summary, descriptor.borrow_kind);
    facts.upcasts.push_back(std::move(descriptor));
    facts.summary.upcast_count = facts.upcasts.size();
}

void record_dyn_dispatch_abi_descriptor(FunctionDynAbiFacts& facts, DynDispatchAbiDescriptor descriptor)
{
    facts.dispatches.push_back(std::move(descriptor));
    facts.summary.dispatch_count = facts.dispatches.size();
}

std::optional<const DynVTableAbiDescriptor*> dyn_vtable_descriptor_for_layout(
    const FunctionDynAbiFacts& facts, const VTableLayoutKey& layout) noexcept
{
    if (!is_valid(layout)) {
        return std::nullopt;
    }
    for (const DynVTableAbiDescriptor& descriptor : facts.vtables) {
        if (descriptor.layout == layout) {
            return &descriptor;
        }
    }
    return std::nullopt;
}

std::optional<const DynVTableSlotAbiDescriptor*> dyn_vtable_slot_descriptor(
    const DynVTableAbiDescriptor& vtable, const base::u32 slot) noexcept
{
    if (!is_valid_slot_index(slot, vtable.layout.method_slot_count)) {
        return std::nullopt;
    }
    for (const DynVTableSlotAbiDescriptor& descriptor : vtable.slots) {
        if (descriptor.slot == slot) {
            return &descriptor;
        }
    }
    return std::nullopt;
}

DynMetadataPolicy function_dyn_abi_metadata_policy(const FunctionDynAbiFacts& facts) noexcept
{
    if (!facts.upcasts.empty()) {
        return DynMetadataPolicy::supertrait_vptr_metadata_v1;
    }
    for (const DynVTableAbiDescriptor& descriptor : facts.vtables) {
        if (descriptor.metadata_policy == DynMetadataPolicy::supertrait_vptr_metadata_v1) {
            return DynMetadataPolicy::supertrait_vptr_metadata_v1;
        }
    }
    return DynMetadataPolicy::borrowed_methods_only_v1;
}

StableFingerprint128 function_dyn_abi_facts_fingerprint(const FunctionDynAbiFacts& facts) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_DYN_ABI_FACTS_FINGERPRINT_MARKER);
    builder.mix_string(facts.symbol);
    builder.mix_u64(facts.objects.size());
    builder.mix_u64(facts.vtables.size());
    builder.mix_u64(facts.coercions.size());
    builder.mix_u64(facts.upcasts.size());
    builder.mix_u64(facts.dispatches.size());
    builder.mix_u64(facts.summary.object_count);
    builder.mix_u64(facts.summary.vtable_count);
    builder.mix_u64(facts.summary.slot_count);
    builder.mix_u64(facts.summary.coercion_count);
    builder.mix_u64(facts.summary.upcast_count);
    builder.mix_u64(facts.summary.dispatch_count);
    builder.mix_u64(facts.summary.shared_borrow_count);
    builder.mix_u64(facts.summary.mut_borrow_count);
    for (const DynObjectAbiDescriptor& descriptor : facts.objects) {
        mix_dyn_object(builder, descriptor);
    }
    for (const DynVTableAbiDescriptor& descriptor : facts.vtables) {
        mix_dyn_vtable(builder, descriptor);
    }
    for (const DynCoercionAbiDescriptor& descriptor : facts.coercions) {
        mix_dyn_coercion(builder, descriptor);
    }
    for (const DynUpcastAbiDescriptor& descriptor : facts.upcasts) {
        mix_dyn_upcast(builder, descriptor);
    }
    for (const DynDispatchAbiDescriptor& descriptor : facts.dispatches) {
        mix_dyn_dispatch(builder, descriptor);
    }
    return builder.finish();
}

std::string summarize_function_dyn_abi_facts(const FunctionDynAbiFacts& facts)
{
    std::ostringstream label;
    label << "dyn_abi_facts objects=" << facts.objects.size()
          << " vtables=" << facts.vtables.size()
          << " slots=" << facts.summary.slot_count
          << " coercions=" << facts.coercions.size()
          << " upcasts=" << facts.upcasts.size()
          << " dispatches=" << facts.dispatches.size()
          << " abi=" << dyn_abi_policy_name(DynAbiPolicy::borrowed_view_v1)
          << " metadata=" << dyn_metadata_policy_name(function_dyn_abi_metadata_policy(facts));
    if (!facts.dispatches.empty()) {
        label << " first_dispatch=vtable_slot slot=" << facts.dispatches.front().slot;
    } else if (!facts.upcasts.empty()) {
        const DynUpcastAbiDescriptor& upcast = facts.upcasts.front();
        label << " first_upcast="
              << (upcast.source_reference_type_name.empty() ? "<unknown>" : upcast.source_reference_type_name)
              << "->"
              << (upcast.target_reference_type_name.empty() ? "<unknown>" : upcast.target_reference_type_name)
              << " borrow=" << dyn_borrow_kind_name(upcast.borrow_kind)
              << " metadata=" << dyn_metadata_policy_name(upcast.metadata_policy);
    } else if (!facts.vtables.empty() && !facts.vtables.front().slots.empty()) {
        label << " first_slot=" << facts.vtables.front().slots.front().slot;
    }
    label << " fingerprint=" << debug_string(function_dyn_abi_facts_fingerprint(facts));
    return label.str();
}

std::string dump_function_dyn_abi_facts(const FunctionDynAbiFacts& facts)
{
    std::ostringstream stream;
    stream << "dyn_abi_facts function=" << (facts.symbol.empty() ? "<anonymous>" : facts.symbol)
           << " objects=" << facts.objects.size()
           << " vtables=" << facts.vtables.size()
           << " slots=" << facts.summary.slot_count
           << " coercions=" << facts.coercions.size()
           << " upcasts=" << facts.upcasts.size()
           << " dispatches=" << facts.dispatches.size()
           << " fingerprint=" << debug_string(function_dyn_abi_facts_fingerprint(facts)) << '\n';
    for (base::usize index = 0; index < facts.objects.size(); ++index) {
        const DynObjectAbiDescriptor& object = facts.objects[index];
        stream << "  object #" << index
               << " type=" << (object.object_type_name.empty() ? "<anonymous>" : object.object_type_name)
               << " trait=" << (object.principal_trait_name.empty() ? "<unknown>" : object.principal_trait_name)
               << " abi=" << dyn_abi_policy_name(object.abi_policy)
               << " key=" << debug_string(stable_key_fingerprint(object.object_type)) << '\n';
    }
    for (base::usize index = 0; index < facts.vtables.size(); ++index) {
        const DynVTableAbiDescriptor& vtable = facts.vtables[index];
        stream << "  vtable #" << index
               << " symbol=" << (vtable.symbol.empty() ? "<anonymous>" : vtable.symbol)
               << " concrete=" << (vtable.concrete_type_name.empty() ? "<unknown>" : vtable.concrete_type_name)
               << " object=" << (vtable.object_type_name.empty() ? "<unknown>" : vtable.object_type_name)
               << " abi=" << dyn_abi_policy_name(vtable.abi_policy)
               << " metadata=" << dyn_metadata_policy_name(vtable.metadata_policy)
               << " key=" << debug_string(stable_key_fingerprint(vtable.layout)) << '\n';
        for (const DynVTableSlotAbiDescriptor& slot : vtable.slots) {
            stream << "    dyn_vtable_slot slot=" << slot.slot
                   << " requirement=" << slot.requirement_ordinal
                   << " method=" << (slot.method_name.empty() ? "<anonymous>" : slot.method_name)
                   << " fn=" << (slot.function_symbol.empty() ? "<missing>" : slot.function_symbol)
                   << " function_type=" << (slot.function_type_name.empty() ? "<unknown>" : slot.function_type_name)
                   << " receiver=" << (slot.receiver_type_name.empty() ? "<unknown>" : slot.receiver_type_name)
                   << " return=" << (slot.return_type_name.empty() ? "<unknown>" : slot.return_type_name) << '\n';
        }
    }
    for (base::usize index = 0; index < facts.coercions.size(); ++index) {
        const DynCoercionAbiDescriptor& coercion = facts.coercions[index];
        stream << "  dyn_coercion #" << index
               << " " << (coercion.source_reference_type_name.empty() ? "<unknown>"
                                                                       : coercion.source_reference_type_name)
               << " -> "
               << (coercion.target_reference_type_name.empty() ? "<unknown>"
                                                               : coercion.target_reference_type_name)
               << " source=" << (coercion.source_type_name.empty() ? "<unknown>" : coercion.source_type_name)
               << " object=" << (coercion.object_type_name.empty() ? "<unknown>" : coercion.object_type_name)
               << " borrow=" << dyn_borrow_kind_name(coercion.borrow_kind)
               << " key=" << debug_string(stable_key_fingerprint(coercion.coercion)) << '\n';
    }
    for (base::usize index = 0; index < facts.upcasts.size(); ++index) {
        const DynUpcastAbiDescriptor& upcast = facts.upcasts[index];
        stream << "  dyn_upcast #" << index
               << " " << (upcast.source_reference_type_name.empty() ? "<unknown>"
                                                                     : upcast.source_reference_type_name)
               << " -> "
               << (upcast.target_reference_type_name.empty() ? "<unknown>"
                                                             : upcast.target_reference_type_name)
               << " source_object=" << (upcast.source_object_type_name.empty() ? "<unknown>"
                                                                               : upcast.source_object_type_name)
               << " target_object=" << (upcast.target_object_type_name.empty() ? "<unknown>"
                                                                               : upcast.target_object_type_name)
               << " borrow=" << dyn_borrow_kind_name(upcast.borrow_kind)
               << " abi=" << dyn_abi_policy_name(upcast.abi_policy)
               << " metadata=" << dyn_metadata_policy_name(upcast.metadata_policy)
               << " key=" << debug_string(stable_key_fingerprint(upcast.upcast)) << '\n';
    }
    for (base::usize index = 0; index < facts.dispatches.size(); ++index) {
        const DynDispatchAbiDescriptor& dispatch = facts.dispatches[index];
        stream << "  dyn_dispatch #" << index
               << " dispatch=vtable_slot"
               << " slot=" << dispatch.slot
               << " method=" << (dispatch.method_name.empty() ? "<anonymous>" : dispatch.method_name)
               << " fn=" << (dispatch.function_symbol.empty() ? "<missing>" : dispatch.function_symbol)
               << " function_type=" << (dispatch.function_type_name.empty() ? "<unknown>"
                                                                             : dispatch.function_type_name)
               << " object=" << (dispatch.object_type_name.empty() ? "<unknown>" : dispatch.object_type_name);
        if (is_valid(dispatch.layout)) {
            stream << " layout=" << debug_string(stable_key_fingerprint(dispatch.layout));
        } else {
            stream << " object_key=" << debug_string(stable_key_fingerprint(dispatch.object_type));
        }
        stream << '\n';
    }
    return stream.str();
}

} // namespace aurex::query
