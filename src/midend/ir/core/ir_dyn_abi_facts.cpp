#include <aurex/midend/ir/ir_dyn_abi_facts.hpp>

#include <aurex/midend/ir/ir_value_closure.hpp>

#include <algorithm>
#include <utility>

namespace aurex::ir {
namespace {

[[nodiscard]] std::string_view safe_function_symbol(const Module& module, const Function& function) noexcept
{
    return module.has_text(function.symbol) ? module.text(function.symbol) : std::string_view{};
}

[[nodiscard]] std::string text_or_empty(const Module& module, const IrTextId id)
{
    return module.has_text(id) ? std::string(module.text(id)) : std::string{};
}

[[nodiscard]] std::string type_name(const Module& module, const sema::TypeHandle type)
{
    return sema::is_valid(type) && type.value < module.types.size() ? module.types.display_name(type) : std::string{};
}

[[nodiscard]] std::string trait_object_principal_name(const Module& module, const sema::TypeHandle type)
{
    if (!sema::is_valid(type) || type.value >= module.types.size()) {
        return {};
    }
    const sema::TypeInfo& info = module.types.get(type);
    return info.kind == sema::TypeKind::trait_object && !info.trait_object_name.empty()
        ? std::string(info.trait_object_name.view())
        : std::string{};
}

[[nodiscard]] const TraitObjectVTableLayout* find_layout(
    const Module& module, const query::VTableLayoutKey& layout_key) noexcept
{
    if (!query::is_valid(layout_key)) {
        return nullptr;
    }
    for (const TraitObjectVTableLayout& layout : module.trait_object_vtables) {
        if (layout.layout_key == layout_key) {
            return &layout;
        }
    }
    return nullptr;
}

[[nodiscard]] const TraitObjectVTableMethodSlot* find_slot(
    const TraitObjectVTableLayout& layout, const base::u32 slot) noexcept
{
    for (const TraitObjectVTableMethodSlot& method_slot : layout.method_slots) {
        if (method_slot.slot == slot) {
            return &method_slot;
        }
    }
    return nullptr;
}

[[nodiscard]] std::string function_symbol_for_slot(
    const Module& module, const TraitObjectVTableMethodSlot& slot)
{
    if (!is_valid(slot.function) || slot.function.value >= module.functions.size()) {
        return {};
    }
    return text_or_empty(module, module.functions[slot.function.value].symbol);
}

[[nodiscard]] query::DynObjectAbiDescriptor make_object_descriptor(
    const Module& module, const TraitObjectVTableLayout& layout)
{
    return query::DynObjectAbiDescriptor{
        layout.layout_key.object_type,
        query::dyn_abi_policy_from_key(layout.layout_key.object_type.abi_policy),
        type_name(module, layout.object_type),
        trait_object_principal_name(module, layout.object_type),
    };
}

[[nodiscard]] query::DynVTableSlotAbiDescriptor make_slot_descriptor(
    const Module& module, const TraitObjectVTableMethodSlot& slot)
{
    return query::DynVTableSlotAbiDescriptor{
        slot.slot,
        slot.slot,
        text_or_empty(module, slot.method_name),
        function_symbol_for_slot(module, slot),
        type_name(module, slot.function_type),
        type_name(module, slot.receiver_type),
        type_name(module, slot.return_type),
    };
}

[[nodiscard]] query::DynVTableAbiDescriptor make_vtable_descriptor(
    const Module& module, const TraitObjectVTableLayout& layout)
{
    query::DynVTableAbiDescriptor descriptor;
    descriptor.layout = layout.layout_key;
    descriptor.abi_policy = query::dyn_abi_policy_from_key(layout.layout_key.abi_policy);
    descriptor.metadata_policy = query::dyn_metadata_policy_from_key(layout.layout_key.metadata_policy);
    descriptor.symbol = text_or_empty(module, layout.symbol);
    descriptor.concrete_type_name = type_name(module, layout.concrete_type);
    descriptor.object_type_name = type_name(module, layout.object_type);
    descriptor.slots.reserve(layout.method_slots.size());
    for (const TraitObjectVTableMethodSlot& slot : layout.method_slots) {
        descriptor.slots.push_back(make_slot_descriptor(module, slot));
    }
    std::ranges::sort(descriptor.slots, [](const query::DynVTableSlotAbiDescriptor& lhs,
                                           const query::DynVTableSlotAbiDescriptor& rhs) noexcept {
        return lhs.slot < rhs.slot;
    });
    return descriptor;
}

[[nodiscard]] query::DynDispatchAbiDescriptor make_dispatch_descriptor(
    const Module& module, const TraitObjectVTableLayout& layout, const Value& value,
    const TraitObjectVTableMethodSlot* const slot)
{
    return query::DynDispatchAbiDescriptor{
        value.vtable_layout,
        value.vtable_slot,
        slot == nullptr ? std::string{} : text_or_empty(module, slot->method_name),
        slot == nullptr ? std::string{} : function_symbol_for_slot(module, *slot),
        slot == nullptr ? std::string{} : type_name(module, slot->function_type),
        layout.layout_key.object_type,
        type_name(module, layout.object_type),
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

void push_unique_dispatch_descriptor(query::FunctionDynAbiFacts& facts, query::DynDispatchAbiDescriptor descriptor)
{
    if (!query::is_valid(descriptor)) {
        return;
    }
    const auto found = std::ranges::find_if(facts.dispatches,
        [&descriptor](const query::DynDispatchAbiDescriptor& dispatch) {
            return dispatch.layout == descriptor.layout && dispatch.slot == descriptor.slot
                && dispatch.method_name == descriptor.method_name;
        });
    if (found == facts.dispatches.end()) {
        query::record_dyn_dispatch_abi_descriptor(facts, std::move(descriptor));
    }
}

void include_layout(query::FunctionDynAbiFacts& facts, const Module& module, const TraitObjectVTableLayout& layout)
{
    push_unique_object_descriptor(facts, make_object_descriptor(module, layout));
    push_unique_vtable_descriptor(facts, make_vtable_descriptor(module, layout));
}

void include_layout_by_key(query::FunctionDynAbiFacts& facts, const Module& module,
    const query::VTableLayoutKey& layout_key)
{
    const TraitObjectVTableLayout* const layout = find_layout(module, layout_key);
    if (layout != nullptr) {
        include_layout(facts, module, *layout);
    }
}

void include_value_dyn_abi(query::FunctionDynAbiFacts& facts, const Module& module, const Value& value)
{
    if (value.kind == ValueKind::trait_object_pack || value.kind == ValueKind::trait_object_data
        || value.kind == ValueKind::trait_object_vtable) {
        include_layout_by_key(facts, module, value.vtable_layout);
        return;
    }
    if (value.kind != ValueKind::vtable_slot) {
        return;
    }
    const TraitObjectVTableLayout* const layout = find_layout(module, value.vtable_layout);
    if (layout == nullptr) {
        return;
    }
    include_layout(facts, module, *layout);
    push_unique_dispatch_descriptor(facts,
        make_dispatch_descriptor(module, *layout, value, find_slot(*layout, value.vtable_slot)));
}

} // namespace

query::FunctionDynAbiFacts function_dyn_abi_facts(const Module& module, const Function& function)
{
    query::FunctionDynAbiFacts facts;
    facts.symbol = std::string(safe_function_symbol(module, function));
    const std::vector<ValueId> values = collect_function_value_closure(module, function);
    facts.objects.reserve(values.size());
    facts.vtables.reserve(values.size());
    facts.dispatches.reserve(values.size());
    for (const ValueId id : values) {
        include_value_dyn_abi(facts, module, module.values[id.value]);
    }
    facts.summary.object_count = facts.objects.size();
    facts.summary.vtable_count = facts.vtables.size();
    facts.summary.dispatch_count = facts.dispatches.size();
    facts.fingerprint = query::function_dyn_abi_facts_fingerprint(facts);
    return facts;
}

std::vector<query::FunctionDynAbiFacts> function_dyn_abi_facts(const Module& module)
{
    std::vector<query::FunctionDynAbiFacts> facts;
    facts.reserve(module.functions.size());
    for (const Function& function : module.functions) {
        facts.push_back(function_dyn_abi_facts(module, function));
    }
    return facts;
}

std::optional<query::FunctionDynAbiFacts> function_dyn_abi_facts_by_symbol(
    const Module& module, const std::string_view symbol)
{
    for (const Function& function : module.functions) {
        if (safe_function_symbol(module, function) == symbol) {
            return function_dyn_abi_facts(module, function);
        }
    }
    return std::nullopt;
}

} // namespace aurex::ir
