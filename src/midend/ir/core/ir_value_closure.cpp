#include <aurex/midend/ir/ir_value_closure.hpp>

#include <algorithm>
#include <unordered_set>

namespace aurex::ir {
namespace {

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

} // namespace

std::vector<ValueId> collect_function_value_closure(const Module& module, const Function& function)
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

    std::ranges::sort(values, [](const ValueId lhs, const ValueId rhs) noexcept {
        return lhs.value < rhs.value;
    });
    return values;
}

} // namespace aurex::ir
