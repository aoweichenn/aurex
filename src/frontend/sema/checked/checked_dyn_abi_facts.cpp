#include <aurex/frontend/sema/checked_dyn_abi_facts.hpp>

#include <algorithm>
#include <utility>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_DYN_ABI_FACTS_MODULE_SYMBOL = "<checked-module>";
[[nodiscard]] std::string type_name(const CheckedModule& checked, const TypeHandle type)
{
    return is_valid(type) ? checked.types.display_name(type) : std::string{};
}

[[nodiscard]] std::string interned_name(const InternedText text)
{
    return text.empty() ? std::string{} : std::string(text.view());
}

[[nodiscard]] query::DynObjectAbiDescriptor make_object_descriptor(
    const CheckedModule& checked, const TraitObjectCallabilityFact& fact)
{
    return query::DynObjectAbiDescriptor{
        fact.object_type_key,
        query::dyn_abi_policy_from_key(fact.object_type_key.abi_policy),
        type_name(checked, fact.object_type),
        interned_name(fact.trait_name),
    };
}

[[nodiscard]] query::DynObjectAbiDescriptor make_object_descriptor(
    const CheckedModule& checked, const VTableLayoutFact& fact)
{
    return query::DynObjectAbiDescriptor{
        fact.layout_key.object_type,
        query::dyn_abi_policy_from_key(fact.layout_key.object_type.abi_policy),
        type_name(checked, fact.object_type),
        {},
    };
}

[[nodiscard]] query::DynObjectAbiDescriptor make_object_descriptor(
    const CheckedModule& checked, const TraitObjectCoercionFact& fact)
{
    return query::DynObjectAbiDescriptor{
        fact.coercion_key.target_object_type,
        query::dyn_abi_policy_from_key(fact.coercion_key.target_object_type.abi_policy),
        type_name(checked, fact.object_type),
        {},
    };
}

[[nodiscard]] query::DynVTableSlotAbiDescriptor make_slot_descriptor(
    const CheckedModule& checked, const VTableMethodSlotFact& slot)
{
    std::string function_type = "fn(";
    for (base::usize index = 0; index < slot.param_types.size(); ++index) {
        if (index != 0) {
            function_type.append(", ");
        }
        function_type.append(type_name(checked, slot.param_types[index]));
    }
    function_type.append(") -> ");
    function_type.append(type_name(checked, slot.return_type));
    return query::DynVTableSlotAbiDescriptor{
        slot.slot,
        slot.requirement_ordinal,
        interned_name(slot.method_name),
        {},
        std::move(function_type),
        type_name(checked, slot.receiver_type),
        type_name(checked, slot.return_type),
    };
}

[[nodiscard]] query::DynVTableAbiDescriptor make_vtable_descriptor(
    const CheckedModule& checked, const VTableLayoutFact& fact)
{
    query::DynVTableAbiDescriptor descriptor;
    descriptor.layout = fact.layout_key;
    descriptor.abi_policy = query::dyn_abi_policy_from_key(fact.layout_key.abi_policy);
    descriptor.metadata_policy = query::dyn_metadata_policy_from_key(fact.layout_key.metadata_policy);
    descriptor.concrete_type_name = type_name(checked, fact.concrete_type);
    descriptor.object_type_name = type_name(checked, fact.object_type);
    descriptor.slots.reserve(fact.method_slots.size());
    for (const VTableMethodSlotFact& slot : fact.method_slots) {
        descriptor.slots.push_back(make_slot_descriptor(checked, slot));
    }
    std::ranges::sort(descriptor.slots, [](const query::DynVTableSlotAbiDescriptor& lhs,
                                           const query::DynVTableSlotAbiDescriptor& rhs) noexcept {
        return lhs.slot < rhs.slot;
    });
    return descriptor;
}

[[nodiscard]] query::DynCoercionAbiDescriptor make_coercion_descriptor(
    const CheckedModule& checked, const TraitObjectCoercionFact& fact)
{
    return query::DynCoercionAbiDescriptor{
        fact.coercion_key,
        fact.vtable_layout,
        query::dyn_borrow_kind_from_key(fact.borrow_kind),
        type_name(checked, fact.source_reference_type),
        type_name(checked, fact.target_reference_type),
        type_name(checked, fact.source_type),
        type_name(checked, fact.object_type),
    };
}

[[nodiscard]] query::DynDispatchAbiDescriptor make_dispatch_descriptor(
    const CheckedModule& checked, const TraitMethodCallBinding& binding)
{
    query::TraitObjectTypeKey object_type;
    std::string object_type_name;
    if (is_valid(binding.self_type) && binding.self_type.value < checked.types.size()) {
        const TypeInfo& info = checked.types.get(binding.self_type);
        if (info.kind == TypeKind::trait_object) {
            object_type = info.trait_object_key;
            object_type_name = type_name(checked, binding.self_type);
        }
    }
    return query::DynDispatchAbiDescriptor{
        binding.vtable_layout,
        binding.vtable_slot,
        interned_name(binding.method_name),
        {},
        {},
        object_type,
        std::move(object_type_name),
    };
}

void push_unique_object_descriptor(query::FunctionDynAbiFacts& facts, query::DynObjectAbiDescriptor descriptor)
{
    if (!query::is_valid(descriptor)) {
        return;
    }
    const auto found = std::ranges::find_if(facts.objects, [&descriptor](const query::DynObjectAbiDescriptor& object) {
        return object.object_type == descriptor.object_type;
    });
    if (found == facts.objects.end()) {
        query::record_dyn_object_abi_descriptor(facts, std::move(descriptor));
        return;
    }
    if (found->principal_trait_name.empty()) {
        found->principal_trait_name = std::move(descriptor.principal_trait_name);
    }
    if (found->object_type_name.empty()) {
        found->object_type_name = std::move(descriptor.object_type_name);
    }
}

void push_unique_vtable_descriptor(query::FunctionDynAbiFacts& facts, query::DynVTableAbiDescriptor descriptor)
{
    if (!query::is_valid(descriptor)) {
        return;
    }
    const auto found = std::ranges::find_if(facts.vtables, [&descriptor](const query::DynVTableAbiDescriptor& vtable) {
        return vtable.layout == descriptor.layout;
    });
    if (found == facts.vtables.end()) {
        query::record_dyn_vtable_abi_descriptor(facts, std::move(descriptor));
    }
}

void push_unique_coercion_descriptor(query::FunctionDynAbiFacts& facts, query::DynCoercionAbiDescriptor descriptor)
{
    if (!query::is_valid(descriptor)) {
        return;
    }
    const auto found = std::ranges::find_if(facts.coercions,
        [&descriptor](const query::DynCoercionAbiDescriptor& coercion) {
            return coercion.coercion == descriptor.coercion;
        });
    if (found == facts.coercions.end()) {
        query::record_dyn_coercion_abi_descriptor(facts, std::move(descriptor));
    }
}

void push_dispatch_descriptor(query::FunctionDynAbiFacts& facts, query::DynDispatchAbiDescriptor descriptor)
{
    if (query::is_valid(descriptor)) {
        query::record_dyn_dispatch_abi_descriptor(facts, std::move(descriptor));
    }
}

} // namespace

query::FunctionDynAbiFacts checked_dyn_abi_facts(const CheckedModule& checked)
{
    query::FunctionDynAbiFacts facts;
    facts.symbol = std::string(SEMA_DYN_ABI_FACTS_MODULE_SYMBOL);
    facts.objects.reserve(checked.trait_object_callability.size() + checked.vtable_layouts.size());
    facts.vtables.reserve(checked.vtable_layouts.size());
    facts.coercions.reserve(checked.trait_object_coercions.size());
    facts.dispatches.reserve(checked.trait_method_calls.size());

    for (const TraitObjectCallabilityFact& fact : checked.trait_object_callability) {
        push_unique_object_descriptor(facts, make_object_descriptor(checked, fact));
    }
    for (const VTableLayoutFact& fact : checked.vtable_layouts) {
        push_unique_object_descriptor(facts, make_object_descriptor(checked, fact));
        push_unique_vtable_descriptor(facts, make_vtable_descriptor(checked, fact));
    }
    for (const TraitObjectCoercionFact& fact : checked.trait_object_coercions) {
        push_unique_object_descriptor(facts, make_object_descriptor(checked, fact));
        push_unique_coercion_descriptor(facts, make_coercion_descriptor(checked, fact));
    }
    for (const TraitMethodCallBinding& binding : checked.trait_method_calls) {
        if (binding.dispatch != TraitMethodDispatchKind::vtable_slot) {
            continue;
        }
        push_dispatch_descriptor(facts, make_dispatch_descriptor(checked, binding));
    }

    std::ranges::sort(facts.objects, [](const query::DynObjectAbiDescriptor& lhs,
                                        const query::DynObjectAbiDescriptor& rhs) noexcept {
        return lhs.object_type.global_id < rhs.object_type.global_id;
    });
    std::ranges::sort(facts.vtables, [](const query::DynVTableAbiDescriptor& lhs,
                                        const query::DynVTableAbiDescriptor& rhs) noexcept {
        return lhs.layout.global_id < rhs.layout.global_id;
    });
    std::ranges::sort(facts.coercions, [](const query::DynCoercionAbiDescriptor& lhs,
                                          const query::DynCoercionAbiDescriptor& rhs) noexcept {
        return lhs.coercion.global_id < rhs.coercion.global_id;
    });
    std::ranges::sort(facts.dispatches, [](const query::DynDispatchAbiDescriptor& lhs,
                                           const query::DynDispatchAbiDescriptor& rhs) noexcept {
        if (lhs.layout.global_id != rhs.layout.global_id) {
            return lhs.layout.global_id < rhs.layout.global_id;
        }
        if (lhs.object_type.global_id != rhs.object_type.global_id) {
            return lhs.object_type.global_id < rhs.object_type.global_id;
        }
        return lhs.slot < rhs.slot;
    });
    facts.summary.object_count = facts.objects.size();
    facts.summary.vtable_count = facts.vtables.size();
    facts.summary.coercion_count = facts.coercions.size();
    facts.summary.dispatch_count = facts.dispatches.size();
    facts.fingerprint = query::function_dyn_abi_facts_fingerprint(facts);
    return facts;
}

} // namespace aurex::sema
