#include <aurex/midend/ir/ir_cleanup_marker_facts.hpp>

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace aurex::ir {
namespace {

constexpr base::u32 IR_CLEANUP_MARKER_INVALID_VALUE_ID = ValueId::INVALID_VALUE;
constexpr base::u32 IR_CLEANUP_MARKER_INVALID_TYPE_ID = sema::TypeHandle::INVALID_VALUE;

[[nodiscard]] std::string_view safe_function_symbol(const Module& module, const Function& function) noexcept
{
    return module.has_text(function.symbol) ? module.text(function.symbol) : std::string_view{};
}

void push_value_id(std::vector<ValueId>& pending, const ValueId id)
{
    if (is_valid(id)) {
        pending.push_back(id);
    }
}

void push_value_references(std::vector<ValueId>& pending, const Value& value)
{
    push_value_id(pending, value.lhs);
    push_value_id(pending, value.rhs);
    push_value_id(pending, value.object);
    push_value_id(pending, value.index);
    for (const ValueId arg : value.args) {
        push_value_id(pending, arg);
    }
    for (const FieldValue& field : value.fields) {
        push_value_id(pending, field.value);
    }
    for (const PhiInput& incoming : value.incoming) {
        push_value_id(pending, incoming.value);
    }
    for (const ValueId element : value.elements) {
        push_value_id(pending, element);
    }
}

void push_terminator_values(std::vector<ValueId>& pending, const Terminator& terminator)
{
    push_value_id(pending, terminator.condition);
    push_value_id(pending, terminator.value);
}

[[nodiscard]] std::vector<ValueId> collect_function_value_closure(const Module& module, const Function& function)
{
    std::vector<ValueId> pending;
    pending.reserve(function.param_values.size() + function.blocks.size());
    for (const ValueId param : function.param_values) {
        push_value_id(pending, param);
    }
    for (const BasicBlock& block : function.blocks) {
        for (const ValueId value_id : block.values) {
            push_value_id(pending, value_id);
        }
        push_terminator_values(pending, block.terminator);
    }

    std::unordered_set<base::u32> seen;
    seen.reserve(pending.size());
    std::vector<ValueId> values;
    while (!pending.empty()) {
        const ValueId id = pending.back();
        pending.pop_back();
        if (!is_valid(id) || id.value >= module.values.size() || !seen.insert(id.value).second) {
            continue;
        }
        values.push_back(id);
        const Value& value = module.values[id.value];
        push_value_references(pending, value);
        if (is_valid(value.constant) && value.constant.value < module.constants.size()) {
            push_value_id(pending, module.constants[value.constant.value].initializer);
        }
    }

    std::sort(values.begin(), values.end(), [](const ValueId lhs, const ValueId rhs) noexcept {
        return lhs.value < rhs.value;
    });
    return values;
}

[[nodiscard]] std::string cleanup_marker_target_type_name(const Module& module, const sema::TypeHandle type)
{
    return sema::is_valid(type) && type.value < module.types.size() ? module.types.display_name(type) : std::string{};
}

[[nodiscard]] query::CleanupMarkerFact make_cleanup_marker_fact(
    const Module& module, const ValueId id, const Value& value)
{
    return query::CleanupMarkerFact{
        value.kind == ValueKind::drop_if ? query::CleanupMarkerKind::drop_if : query::CleanupMarkerKind::drop,
        query_cleanup_marker_policy(value.cleanup_policy),
        id.value,
        is_valid(value.object) ? value.object.value : IR_CLEANUP_MARKER_INVALID_VALUE_ID,
        is_valid(value.lhs) ? value.lhs.value : IR_CLEANUP_MARKER_INVALID_VALUE_ID,
        sema::is_valid(value.target_type) ? value.target_type.value : IR_CLEANUP_MARKER_INVALID_TYPE_ID,
        cleanup_marker_target_type_name(module, value.target_type),
    };
}

} // namespace

query::CleanupMarkerPolicy query_cleanup_marker_policy(const CleanupAbiPolicy policy) noexcept
{
    switch (policy) {
        case CleanupAbiPolicy::none:
            return query::CleanupMarkerPolicy::none;
        case CleanupAbiPolicy::structural_static:
            return query::CleanupMarkerPolicy::structural_static;
        case CleanupAbiPolicy::generic_marker_only:
            return query::CleanupMarkerPolicy::generic_marker_only;
        case CleanupAbiPolicy::associated_projection_marker_only:
            return query::CleanupMarkerPolicy::associated_projection_marker_only;
        case CleanupAbiPolicy::opaque_marker_only:
            return query::CleanupMarkerPolicy::opaque_marker_only;
        case CleanupAbiPolicy::unknown_marker_only:
            return query::CleanupMarkerPolicy::unknown_marker_only;
        case CleanupAbiPolicy::static_custom_destructor:
            return query::CleanupMarkerPolicy::static_custom_destructor;
    }
    return query::CleanupMarkerPolicy::none;
}

query::FunctionCleanupMarkerFacts function_cleanup_marker_facts(const Module& module, const Function& function)
{
    query::FunctionCleanupMarkerFacts facts;
    facts.symbol = std::string(safe_function_symbol(module, function));
    const std::vector<ValueId> values = collect_function_value_closure(module, function);
    facts.markers.reserve(values.size());
    for (const ValueId id : values) {
        const Value& value = module.values[id.value];
        if (value.kind != ValueKind::drop && value.kind != ValueKind::drop_if) {
            continue;
        }
        query::record_cleanup_marker_fact(facts, make_cleanup_marker_fact(module, id, value));
    }
    facts.fingerprint = query::function_cleanup_marker_facts_fingerprint(facts);
    return facts;
}

std::vector<query::FunctionCleanupMarkerFacts> function_cleanup_marker_facts(const Module& module)
{
    std::vector<query::FunctionCleanupMarkerFacts> facts;
    facts.reserve(module.functions.size());
    for (const Function& function : module.functions) {
        facts.push_back(function_cleanup_marker_facts(module, function));
    }
    return facts;
}

std::optional<query::FunctionCleanupMarkerFacts> function_cleanup_marker_facts_by_symbol(
    const Module& module, const std::string_view symbol)
{
    for (const Function& function : module.functions) {
        if (safe_function_symbol(module, function) == symbol) {
            return function_cleanup_marker_facts(module, function);
        }
    }
    return std::nullopt;
}

} // namespace aurex::ir
