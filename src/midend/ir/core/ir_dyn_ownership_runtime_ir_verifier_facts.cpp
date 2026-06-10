#include <aurex/midend/ir/ir_dyn_ownership_runtime_ir_verifier_facts.hpp>

#include <aurex/midend/ir/ir_value_closure.hpp>

#include <algorithm>
#include <utility>

namespace aurex::ir {
namespace {

constexpr base::u32 IR_DYN_OWNERSHIP_RUNTIME_NO_VALUE = ValueId::INVALID_VALUE;
constexpr base::usize IR_DYN_OWNERSHIP_RUNTIME_FUTURE_FACT_COUNT = 4U;

[[nodiscard]] std::string_view safe_function_symbol(const Module& module, const Function& function) noexcept
{
    return module.has_text(function.symbol) ? module.text(function.symbol) : std::string_view{};
}

[[nodiscard]] const TraitObjectVTableLayout* find_layout(
    const Module& module,
    const query::VTableLayoutKey& layout_key) noexcept
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

[[nodiscard]] bool is_static_or_marker_cleanup_policy(const CleanupAbiPolicy policy) noexcept
{
    switch (policy) {
        case CleanupAbiPolicy::structural_static:
        case CleanupAbiPolicy::generic_marker_only:
        case CleanupAbiPolicy::associated_projection_marker_only:
        case CleanupAbiPolicy::opaque_marker_only:
        case CleanupAbiPolicy::unknown_marker_only:
        case CleanupAbiPolicy::static_custom_destructor:
            return true;
        case CleanupAbiPolicy::none:
        case CleanupAbiPolicy::dynamic_erased_drop_blocked:
            return false;
    }
    return false;
}

[[nodiscard]] query::DynOwnershipRuntimeIrVerifierFact make_base_fact(
    const std::string_view symbol,
    const query::DynOwnershipRuntimeIrVerifierFactKind kind,
    const query::DynOwnershipRuntimeIrVerifierStage stage,
    const query::DynOwnershipRuntimeIrVerifierPolicy policy,
    const std::string_view fact_name,
    const std::string_view boundary_fact,
    const std::string_view verifier_guard_fact,
    const std::string_view blocked_surface_fact)
{
    query::DynOwnershipRuntimeIrVerifierFact fact;
    fact.fact_name = std::string(fact_name);
    fact.kind = kind;
    fact.stage = stage;
    fact.policy = policy;
    fact.verifier_visible = true;
    fact.borrowed_vtable_destructor_free =
        kind == query::DynOwnershipRuntimeIrVerifierFactKind::borrowed_vtable_destructor_free;
    fact.static_cleanup_only =
        kind == query::DynOwnershipRuntimeIrVerifierFactKind::static_cleanup_only;
    fact.erased_drop_identity_required =
        kind == query::DynOwnershipRuntimeIrVerifierFactKind::erased_drop_identity_required;
    fact.allocator_identity_required =
        kind == query::DynOwnershipRuntimeIrVerifierFactKind::allocator_identity_required;
    fact.owned_dyn_object_placeholder_blocked = true;
    fact.runtime_lowering_blocked = true;
    fact.dynamic_drop_runtime_blocked = true;
    fact.standard_library_blocked = true;
    fact.lowering_runtime_implemented = false;
    fact.observed_value_id = IR_DYN_OWNERSHIP_RUNTIME_NO_VALUE;
    fact.subject_symbol = std::string(symbol);
    fact.boundary_fact = std::string(boundary_fact);
    fact.verifier_guard_fact = std::string(verifier_guard_fact);
    fact.blocked_surface_fact = std::string(blocked_surface_fact);
    return fact;
}

[[nodiscard]] query::DynOwnershipRuntimeIrVerifierFact make_borrowed_vtable_fact(
    const std::string_view symbol,
    const TraitObjectVTableLayout& layout)
{
    query::DynOwnershipRuntimeIrVerifierFact fact =
        make_base_fact(symbol,
            query::DynOwnershipRuntimeIrVerifierFactKind::borrowed_vtable_destructor_free,
            query::DynOwnershipRuntimeIrVerifierStage::ir_verifier_preparation,
            query::DynOwnershipRuntimeIrVerifierPolicy::borrowed_vtable_methods_only_v1,
            "borrowed_vtable_destructor_free_ir_guard",
            "borrowed_vtable_layout_has_no_destructor_slot",
            "verifier_rejects_borrowed_vtable_destructor_slot",
            "dynamic_drop_runtime_not_in_m19");
    fact.borrowed_vtable_destructor_free = layout.destructor_slot_blocked;
    fact.object_type = layout.layout_key.object_type;
    fact.vtable_layout = layout.layout_key;
    return fact;
}

[[nodiscard]] query::DynOwnershipRuntimeIrVerifierFact make_static_cleanup_fact(
    const std::string_view symbol,
    const ValueId id,
    const Value& value)
{
    query::DynOwnershipRuntimeIrVerifierFact fact =
        make_base_fact(symbol,
            query::DynOwnershipRuntimeIrVerifierFactKind::static_cleanup_only,
            query::DynOwnershipRuntimeIrVerifierStage::verifier_negative_matrix,
            query::DynOwnershipRuntimeIrVerifierPolicy::static_cleanup_marker_only_v1,
            "static_cleanup_marker_only_ir_guard",
            "drop_markers_remain_static_or_marker_only",
            "verifier_rejects_dynamic_erased_drop_policy",
            "dynamic_drop_dispatch_not_in_m19");
    fact.static_cleanup_only = is_static_or_marker_cleanup_policy(value.cleanup_policy);
    fact.observed_value_id = id.value;
    fact.static_cleanup_marker_count = fact.static_cleanup_only ? 1U : 0U;
    fact.blocked_runtime_marker_count =
        value.cleanup_policy == CleanupAbiPolicy::dynamic_erased_drop_blocked ? 1U : 0U;
    return fact;
}

[[nodiscard]] query::DynOwnershipRuntimeIrVerifierFact make_future_fact(
    const std::string_view symbol,
    const query::DynOwnershipRuntimeIrVerifierFactKind kind,
    const query::DynOwnershipRuntimeIrVerifierPolicy policy,
    const std::string_view fact_name,
    const std::string_view boundary_fact,
    const std::string_view verifier_guard_fact,
    const std::string_view blocked_surface_fact)
{
    return make_base_fact(symbol,
        kind,
        query::DynOwnershipRuntimeIrVerifierStage::blocked_future_runtime,
        policy,
        fact_name,
        boundary_fact,
        verifier_guard_fact,
        blocked_surface_fact);
}

void push_unique_borrowed_vtable_fact(
    query::FunctionDynOwnershipRuntimeIrVerifierFacts& facts,
    query::DynOwnershipRuntimeIrVerifierFact fact)
{
    const auto found = std::ranges::find_if(facts.facts,
        [&fact](const query::DynOwnershipRuntimeIrVerifierFact& existing) {
            return existing.kind == query::DynOwnershipRuntimeIrVerifierFactKind::borrowed_vtable_destructor_free
                && existing.vtable_layout == fact.vtable_layout;
        });
    if (found == facts.facts.end()) {
        query::record_dyn_ownership_runtime_ir_verifier_fact(facts, std::move(fact));
    }
}

void include_layout_by_key(
    query::FunctionDynOwnershipRuntimeIrVerifierFacts& facts,
    const Module& module,
    const std::string_view symbol,
    const query::VTableLayoutKey& layout_key)
{
    const TraitObjectVTableLayout* const layout = find_layout(module, layout_key);
    if (layout != nullptr) {
        push_unique_borrowed_vtable_fact(facts, make_borrowed_vtable_fact(symbol, *layout));
    }
}

void include_value_facts(
    query::FunctionDynOwnershipRuntimeIrVerifierFacts& facts,
    const Module& module,
    const std::string_view symbol,
    const ValueId id)
{
    const Value& value = module.values[id.value];
    if (value.kind == ValueKind::drop || value.kind == ValueKind::drop_if) {
        query::record_dyn_ownership_runtime_ir_verifier_fact(
            facts, make_static_cleanup_fact(symbol, id, value));
        return;
    }
    if (value.kind == ValueKind::trait_object_pack
        || value.kind == ValueKind::trait_object_data
        || value.kind == ValueKind::trait_object_vtable
        || value.kind == ValueKind::vtable_slot) {
        include_layout_by_key(facts, module, symbol, value.vtable_layout);
        return;
    }
    if (value.kind == ValueKind::trait_object_upcast) {
        include_layout_by_key(facts, module, symbol, value.vtable_layout);
        include_layout_by_key(facts, module, symbol, value.target_vtable_layout);
        return;
    }
    if (value.kind == ValueKind::trait_object_composition_project) {
        include_layout_by_key(facts, module, symbol, value.target_vtable_layout);
    }
}

void include_future_blocker_facts(
    query::FunctionDynOwnershipRuntimeIrVerifierFacts& facts,
    const std::string_view symbol)
{
    query::record_dyn_ownership_runtime_ir_verifier_fact(facts,
        make_future_fact(symbol,
            query::DynOwnershipRuntimeIrVerifierFactKind::erased_drop_identity_required,
            query::DynOwnershipRuntimeIrVerifierPolicy::erased_drop_identity_prerequisite_v1,
            "future_erased_drop_identity_required_by_verifier",
            "future_erased_drop_identity_required",
            "verifier_requires_identity_before_runtime_lowering",
            "dynamic_drop_runtime_not_in_m19"));
    query::record_dyn_ownership_runtime_ir_verifier_fact(facts,
        make_future_fact(symbol,
            query::DynOwnershipRuntimeIrVerifierFactKind::allocator_identity_required,
            query::DynOwnershipRuntimeIrVerifierPolicy::allocator_identity_prerequisite_v1,
            "future_allocator_identity_required_by_verifier",
            "future_allocator_identity_required",
            "verifier_requires_allocator_identity_before_owning_dyn",
            "allocator_api_not_in_m19"));
    query::record_dyn_ownership_runtime_ir_verifier_fact(facts,
        make_future_fact(symbol,
            query::DynOwnershipRuntimeIrVerifierFactKind::owned_dyn_object_placeholder_blocked,
            query::DynOwnershipRuntimeIrVerifierPolicy::owned_dyn_object_placeholder_not_lowered_v1,
            "future_owned_dyn_object_placeholder_blocked_in_ir",
            "future_owned_dyn_object_placeholder_required",
            "verifier_keeps_owned_dyn_user_values_blocked",
            "owning_dyn_user_value_not_in_m19"));
    query::record_dyn_ownership_runtime_ir_verifier_fact(facts,
        make_future_fact(symbol,
            query::DynOwnershipRuntimeIrVerifierFactKind::runtime_lowering_blocked_without_stdlib,
            query::DynOwnershipRuntimeIrVerifierPolicy::runtime_lowering_not_implemented_v1,
            "runtime_lowering_blocked_until_stdlib_runtime_stage",
            "runtime_lowering_requires_stdlib_runtime_stage",
            "verifier_rejects_runtime_lowering_without_stdlib",
            "runtime_abi_lowering_not_in_m19"));
}

} // namespace

query::FunctionDynOwnershipRuntimeIrVerifierFacts
function_dyn_ownership_runtime_ir_verifier_facts(const Module& module, const Function& function)
{
    query::FunctionDynOwnershipRuntimeIrVerifierFacts facts;
    facts.symbol = std::string(safe_function_symbol(module, function));
    const std::vector<ValueId> values = collect_function_value_closure(module, function);
    facts.facts.reserve(values.size() + IR_DYN_OWNERSHIP_RUNTIME_FUTURE_FACT_COUNT);
    for (const ValueId id : values) {
        include_value_facts(facts, module, facts.symbol, id);
    }
    include_future_blocker_facts(facts, facts.symbol);
    facts.summary = query::summarize_dyn_ownership_runtime_ir_verifier_counts(facts);
    facts.fingerprint = query::dyn_ownership_runtime_ir_verifier_facts_fingerprint(facts);
    return facts;
}

std::vector<query::FunctionDynOwnershipRuntimeIrVerifierFacts>
function_dyn_ownership_runtime_ir_verifier_facts(const Module& module)
{
    std::vector<query::FunctionDynOwnershipRuntimeIrVerifierFacts> facts;
    facts.reserve(module.functions.size());
    for (const Function& function : module.functions) {
        facts.push_back(function_dyn_ownership_runtime_ir_verifier_facts(module, function));
    }
    return facts;
}

std::optional<query::FunctionDynOwnershipRuntimeIrVerifierFacts>
function_dyn_ownership_runtime_ir_verifier_facts_by_symbol(
    const Module& module,
    const std::string_view symbol)
{
    for (const Function& function : module.functions) {
        if (safe_function_symbol(module, function) == symbol) {
            return function_dyn_ownership_runtime_ir_verifier_facts(module, function);
        }
    }
    return std::nullopt;
}

} // namespace aurex::ir
