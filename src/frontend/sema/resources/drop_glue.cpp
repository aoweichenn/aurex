#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/drop_glue.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace aurex::sema {
namespace {

constexpr base::u64 SEMA_DROP_GLUE_PLAN_FINGERPRINT_MARKER = 0x53444c474c554531ULL;
constexpr base::usize SEMA_DROP_GLUE_ACTION_STACK_RESERVE = 16;

enum class DropGlueActionKind : base::u8 {
    enter,
    exit,
    emit_step,
};

struct DropGlueAction {
    DropGlueActionKind kind = DropGlueActionKind::enter;
    TypeHandle type = INVALID_TYPE_HANDLE;
    DropGlueStep step;
};

[[nodiscard]] base::Result<DropGluePlan> drop_glue_error(const std::string_view message)
{
    return base::Result<DropGluePlan>::fail({
        base::ErrorCode::internal_error,
        std::string(message),
    });
}

[[nodiscard]] const StructInfo* drop_glue_struct_info(const CheckedModule& checked, const TypeHandle type)
{
    for (const auto& entry : checked.structs) {
        if (entry.second.type.value == type.value) {
            return &entry.second;
        }
    }
    return nullptr;
}

[[nodiscard]] std::vector<TypeHandle> drop_glue_enum_payload_types(const CheckedModule& checked, const TypeHandle type)
{
    std::vector<TypeHandle> payloads;
    for (const auto& entry : checked.enum_cases) {
        const EnumCaseInfo& enum_case = entry.second;
        if (enum_case.type.value == type.value) {
            payloads.insert(payloads.end(), enum_case.payload_types.begin(), enum_case.payload_types.end());
        }
    }
    return payloads;
}

[[nodiscard]] bool drop_glue_type_needs_drop(const ResourceSemanticsClassifier& classifier, const TypeHandle type)
{
    return resource_needs_drop(classifier.classify(type));
}

[[nodiscard]] DropGlueStep make_drop_glue_step(const DropGlueStepKind kind, const TypeHandle owner_type,
    const TypeHandle value_type, const base::u32 ordinal, const ResourceSemanticsClassifier& classifier)
{
    return DropGlueStep{
        kind,
        owner_type,
        value_type,
        ordinal,
        classifier.classify(value_type),
    };
}

void push_drop_glue_step_actions(std::vector<DropGlueAction>& actions, const DropGlueStep& step)
{
    actions.push_back(DropGlueAction{DropGlueActionKind::emit_step, INVALID_TYPE_HANDLE, step});
    actions.push_back(DropGlueAction{DropGlueActionKind::enter, step.value_type, {}});
}

void append_drop_glue_struct_actions(std::vector<DropGlueAction>& actions, const CheckedModule& checked,
    const TypeHandle type, const ResourceSemanticsClassifier& classifier)
{
    const StructInfo* const info = drop_glue_struct_info(checked, type);
    if (info == nullptr) {
        actions.push_back(DropGlueAction{
            DropGlueActionKind::emit_step,
            INVALID_TYPE_HANDLE,
            make_drop_glue_step(DropGlueStepKind::opaque_value, type, type, 0, classifier),
        });
        return;
    }
    for (base::usize index = 0; index < info->fields.size(); ++index) {
        const base::usize field_index = info->fields.size() - index - 1U;
        const TypeHandle field_type = info->fields[field_index].type;
        if (drop_glue_type_needs_drop(classifier, field_type)) {
            push_drop_glue_step_actions(actions,
                make_drop_glue_step(
                    DropGlueStepKind::struct_field, type, field_type, static_cast<base::u32>(field_index), classifier));
        }
    }
}

void append_drop_glue_tuple_actions(std::vector<DropGlueAction>& actions, const TypeInfo& info, const TypeHandle type,
    const ResourceSemanticsClassifier& classifier)
{
    for (base::usize index = 0; index < info.tuple_elements.size(); ++index) {
        const base::usize element_index = info.tuple_elements.size() - index - 1U;
        const TypeHandle element = info.tuple_elements[element_index];
        if (drop_glue_type_needs_drop(classifier, element)) {
            push_drop_glue_step_actions(actions,
                make_drop_glue_step(
                    DropGlueStepKind::tuple_element, type, element, static_cast<base::u32>(element_index), classifier));
        }
    }
}

void append_drop_glue_array_actions(std::vector<DropGlueAction>& actions, const TypeInfo& info, const TypeHandle type,
    const ResourceSemanticsClassifier& classifier)
{
    if (drop_glue_type_needs_drop(classifier, info.array_element)) {
        push_drop_glue_step_actions(
            actions, make_drop_glue_step(DropGlueStepKind::array_element, type, info.array_element, 0, classifier));
    }
}

void append_drop_glue_enum_actions(std::vector<DropGlueAction>& actions, const CheckedModule& checked,
    const TypeHandle type, const ResourceSemanticsClassifier& classifier)
{
    const std::vector<TypeHandle> payloads = drop_glue_enum_payload_types(checked, type);
    for (base::usize index = 0; index < payloads.size(); ++index) {
        const TypeHandle payload = payloads[index];
        if (drop_glue_type_needs_drop(classifier, payload)) {
            push_drop_glue_step_actions(actions,
                make_drop_glue_step(
                    DropGlueStepKind::enum_payload, type, payload, static_cast<base::u32>(index), classifier));
        }
    }
}

void append_drop_glue_actions_for_type(std::vector<DropGlueAction>& stack, const CheckedModule& checked,
    const TypeHandle type, const TypeInfo& info, const ResourceSemanticsClassifier& classifier)
{
    std::vector<DropGlueAction> actions;
    switch (info.kind) {
        case TypeKind::struct_:
            append_drop_glue_struct_actions(actions, checked, type, classifier);
            break;
        case TypeKind::tuple:
            append_drop_glue_tuple_actions(actions, info, type, classifier);
            break;
        case TypeKind::array:
            append_drop_glue_array_actions(actions, info, type, classifier);
            break;
        case TypeKind::enum_:
            append_drop_glue_enum_actions(actions, checked, type, classifier);
            break;
        case TypeKind::generic_param:
        case TypeKind::associated_projection:
            actions.push_back(DropGlueAction{
                DropGlueActionKind::emit_step,
                INVALID_TYPE_HANDLE,
                make_drop_glue_step(DropGlueStepKind::generic_value, type, type, 0, classifier),
            });
            break;
        case TypeKind::opaque_struct:
            actions.push_back(DropGlueAction{
                DropGlueActionKind::emit_step,
                INVALID_TYPE_HANDLE,
                make_drop_glue_step(DropGlueStepKind::opaque_value, type, type, 0, classifier),
            });
            break;
        case TypeKind::builtin:
        case TypeKind::pointer:
        case TypeKind::reference:
        case TypeKind::slice:
        case TypeKind::function:
            break;
    }
    for (base::usize index = actions.size(); index > 0; --index) {
        stack.push_back(actions[index - 1U]);
    }
}

[[nodiscard]] query::StableFingerprint128 drop_glue_plan_fingerprint(const DropGluePlan& plan)
{
    query::StableHashBuilder builder;
    builder.mix_u64(SEMA_DROP_GLUE_PLAN_FINGERPRINT_MARKER);
    builder.mix_u32(plan.root_type.value);
    builder.mix_fingerprint(resource_semantics_fingerprint(plan.root_resource));
    builder.mix_u64(plan.steps.size());
    for (const DropGlueStep& step : plan.steps) {
        builder.mix_u8(static_cast<base::u8>(step.kind));
        builder.mix_u32(step.owner_type.value);
        builder.mix_u32(step.value_type.value);
        builder.mix_u32(step.ordinal);
        builder.mix_fingerprint(resource_semantics_fingerprint(step.resource));
    }
    return builder.finish();
}

} // namespace

std::string_view drop_glue_step_kind_name(const DropGlueStepKind kind) noexcept
{
    switch (kind) {
        case DropGlueStepKind::custom_destructor:
            return "custom_destructor";
        case DropGlueStepKind::struct_field:
            return "struct_field";
        case DropGlueStepKind::tuple_element:
            return "tuple_element";
        case DropGlueStepKind::array_element:
            return "array_element";
        case DropGlueStepKind::enum_payload:
            return "enum_payload";
        case DropGlueStepKind::generic_value:
            return "generic_value";
        case DropGlueStepKind::opaque_value:
            return "opaque_value";
    }
    return "invalid";
}

bool drop_glue_plan_needs_drop(const DropGluePlan& plan) noexcept
{
    return resource_needs_drop(plan.root_resource);
}

base::Result<DropGluePlan> build_drop_glue_plan(const CheckedModule& checked, const TypeHandle root)
{
    if (!is_valid(root)) {
        return drop_glue_error("invalid drop-glue root type");
    }
    if (root.value >= checked.types.size()) {
        return drop_glue_error("unknown drop-glue root type");
    }

    const ResourceSemanticsClassifier classifier(checked);
    DropGluePlan plan;
    plan.root_type = root;
    plan.root_resource = classifier.classify(root);
    if (!resource_needs_drop(plan.root_resource)) {
        plan.fingerprint = drop_glue_plan_fingerprint(plan);
        return base::Result<DropGluePlan>::ok(std::move(plan));
    }

    std::unordered_set<base::u32> active;
    std::vector<DropGlueAction> stack;
    stack.reserve(SEMA_DROP_GLUE_ACTION_STACK_RESERVE);
    stack.push_back(DropGlueAction{DropGlueActionKind::enter, root, {}});
    while (!stack.empty()) {
        const DropGlueAction action = stack.back();
        stack.pop_back();
        if (action.kind == DropGlueActionKind::emit_step) {
            plan.steps.push_back(action.step);
            continue;
        }
        if (action.kind == DropGlueActionKind::exit) {
            active.erase(action.type.value);
            continue;
        }
        if (!is_valid(action.type) || action.type.value >= checked.types.size()
            || !drop_glue_type_needs_drop(classifier, action.type)) {
            continue;
        }
        if (active.contains(action.type.value)) {
            plan.steps.push_back(
                make_drop_glue_step(DropGlueStepKind::generic_value, action.type, action.type, 0, classifier));
            continue;
        }
        const TypeInfo& info = checked.types.get(action.type);
        active.insert(action.type.value);
        stack.push_back(DropGlueAction{DropGlueActionKind::exit, action.type, {}});
        append_drop_glue_actions_for_type(stack, checked, action.type, info, classifier);
    }

    plan.fingerprint = drop_glue_plan_fingerprint(plan);
    return base::Result<DropGluePlan>::ok(std::move(plan));
}

} // namespace aurex::sema
