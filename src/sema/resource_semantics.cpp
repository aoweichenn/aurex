#include <aurex/sema/checked_module.hpp>
#include <aurex/sema/resource_semantics.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace aurex::sema {
namespace {

constexpr base::u64 SEMA_RESOURCE_FINGERPRINT_MARKER = 0x53454d415245534fULL;
constexpr base::usize SEMA_RESOURCE_CLASSIFIER_INITIAL_STACK_CAPACITY = 16;

[[nodiscard]] constexpr ResourceSemanticsSummary trivial_owned_summary() noexcept
{
    return ResourceSemanticsSummary{
        ResourceCopyKind::copy,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::trivial,
        ResourceOwnershipKind::owned_value,
    };
}

[[nodiscard]] constexpr ResourceSemanticsSummary conservative_owned_summary() noexcept
{
    return ResourceSemanticsSummary{
        ResourceCopyKind::move_only,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::needs_drop,
        ResourceOwnershipKind::owned_value,
    };
}

[[nodiscard]] constexpr ResourceSemanticsSummary borrowed_view_summary() noexcept
{
    return ResourceSemanticsSummary{
        ResourceCopyKind::copy,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::trivial,
        ResourceOwnershipKind::borrowed_view,
    };
}

[[nodiscard]] constexpr ResourceSemanticsSummary raw_pointer_summary() noexcept
{
    return ResourceSemanticsSummary{
        ResourceCopyKind::copy,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::trivial,
        ResourceOwnershipKind::raw_pointer,
    };
}

[[nodiscard]] constexpr ResourceSemanticsSummary join_owned_components(
    const ResourceSemanticsSummary lhs, const ResourceSemanticsSummary rhs) noexcept
{
    return ResourceSemanticsSummary{
        lhs.copy == ResourceCopyKind::copy && rhs.copy == ResourceCopyKind::copy ? ResourceCopyKind::copy
                                                                                 : ResourceCopyKind::move_only,
        lhs.discard == ResourceDiscardKind::discard && rhs.discard == ResourceDiscardKind::discard
            ? ResourceDiscardKind::discard
            : ResourceDiscardKind::must_consume,
        lhs.cleanup == ResourceCleanupKind::trivial && rhs.cleanup == ResourceCleanupKind::trivial
            ? ResourceCleanupKind::trivial
            : ResourceCleanupKind::needs_drop,
        ResourceOwnershipKind::owned_value,
    };
}

enum class ClassificationFrameStage {
    enter,
    finish,
};

struct ClassificationFrame {
    TypeHandle type = INVALID_TYPE_HANDLE;
    ClassificationFrameStage stage = ClassificationFrameStage::enter;
    std::vector<TypeHandle> components;
    base::usize next_component_index = 0;
};

[[nodiscard]] ResourceSemanticsSummary classify_leaf(const TypeInfo& info) noexcept
{
    if (info.kind == TypeKind::builtin) {
        return info.builtin == BuiltinType::str ? borrowed_view_summary() : trivial_owned_summary();
    }
    if (info.kind == TypeKind::pointer) {
        return raw_pointer_summary();
    }
    if (info.kind == TypeKind::reference || info.kind == TypeKind::slice) {
        return borrowed_view_summary();
    }
    if (info.kind == TypeKind::function) {
        return trivial_owned_summary();
    }
    return conservative_owned_summary();
}

[[nodiscard]] bool is_structural_type(const TypeInfo& info) noexcept
{
    return info.kind == TypeKind::array || info.kind == TypeKind::tuple || info.kind == TypeKind::struct_
        || info.kind == TypeKind::enum_;
}

} // namespace

std::string_view resource_copy_kind_name(const ResourceCopyKind kind) noexcept
{
    switch (kind) {
        case ResourceCopyKind::copy:
            return "Copy";
        case ResourceCopyKind::move_only:
            return "MoveOnly";
    }
    return "<invalid>";
}

std::string_view resource_discard_kind_name(const ResourceDiscardKind kind) noexcept
{
    switch (kind) {
        case ResourceDiscardKind::discard:
            return "Discard";
        case ResourceDiscardKind::must_consume:
            return "MustConsume";
    }
    return "<invalid>";
}

std::string_view resource_cleanup_kind_name(const ResourceCleanupKind kind) noexcept
{
    switch (kind) {
        case ResourceCleanupKind::trivial:
            return "Trivial";
        case ResourceCleanupKind::needs_drop:
            return "NeedsDrop";
    }
    return "<invalid>";
}

std::string_view resource_ownership_kind_name(const ResourceOwnershipKind kind) noexcept
{
    switch (kind) {
        case ResourceOwnershipKind::owned_value:
            return "OwnedValue";
        case ResourceOwnershipKind::borrowed_view:
            return "BorrowedView";
        case ResourceOwnershipKind::raw_pointer:
            return "RawPointer";
        case ResourceOwnershipKind::shared_managed:
            return "SharedManaged";
    }
    return "<invalid>";
}

std::string resource_semantics_debug_string(const ResourceSemanticsSummary summary)
{
    std::string result;
    result.reserve(64);
    result += resource_copy_kind_name(summary.copy);
    result += "/";
    result += resource_discard_kind_name(summary.discard);
    result += "/";
    result += resource_cleanup_kind_name(summary.cleanup);
    result += "/";
    result += resource_ownership_kind_name(summary.ownership);
    return result;
}

query::StableFingerprint128 resource_semantics_fingerprint(const ResourceSemanticsSummary summary) noexcept
{
    query::StableHashBuilder hash;
    hash.mix_u64(SEMA_RESOURCE_FINGERPRINT_MARKER);
    hash.mix_u8(static_cast<base::u8>(summary.copy));
    hash.mix_u8(static_cast<base::u8>(summary.discard));
    hash.mix_u8(static_cast<base::u8>(summary.cleanup));
    hash.mix_u8(static_cast<base::u8>(summary.ownership));
    return hash.finish();
}

ResourceSemanticsClassifier::ResourceSemanticsClassifier(const CheckedModule& checked,
    GenericCopyPredicate generic_copy_predicate, StructuralComponentProvider structural_component_provider)
    : checked_(checked), generic_copy_predicate_(std::move(generic_copy_predicate)),
      structural_component_provider_(std::move(structural_component_provider))
{
    this->build_indexes();
}

void ResourceSemanticsClassifier::build_indexes()
{
    this->struct_infos_by_type_.reserve(this->checked_.structs.size());
    for (const auto& entry : this->checked_.structs) {
        if (is_valid(entry.second.type)) {
            this->struct_infos_by_type_.emplace(entry.second.type.value, &entry.second);
        }
    }
    this->enum_payload_types_by_type_.reserve(this->checked_.enum_cases.size());
    for (const auto& entry : this->checked_.enum_cases) {
        const EnumCaseInfo& enum_case = entry.second;
        if (!is_valid(enum_case.type)) {
            continue;
        }
        std::vector<TypeHandle>& payload_types = this->enum_payload_types_by_type_[enum_case.type.value];
        payload_types.insert(payload_types.end(), enum_case.payload_types.begin(), enum_case.payload_types.end());
    }
}

const StructInfo* ResourceSemanticsClassifier::indexed_struct_info(const TypeHandle type) const
{
    if (!is_valid(type)) {
        return nullptr;
    }
    const auto found = this->struct_infos_by_type_.find(type.value);
    return found == this->struct_infos_by_type_.end() ? nullptr : found->second;
}

std::span<const TypeHandle> ResourceSemanticsClassifier::indexed_enum_payload_types(const TypeHandle type) const
{
    if (!is_valid(type)) {
        return {};
    }
    const auto found = this->enum_payload_types_by_type_.find(type.value);
    return found == this->enum_payload_types_by_type_.end() ? std::span<const TypeHandle>{}
                                                            : std::span<const TypeHandle>{found->second};
}

std::optional<std::vector<TypeHandle>> ResourceSemanticsClassifier::structural_components(const TypeHandle type) const
{
    if (this->structural_component_provider_) {
        return this->structural_component_provider_(type);
    }
    std::vector<TypeHandle> components;
    const TypeInfo& info = this->checked_.types.get(type);
    switch (info.kind) {
        case TypeKind::array:
            components.push_back(info.array_element);
            break;
        case TypeKind::tuple:
            components.insert(components.end(), info.tuple_elements.begin(), info.tuple_elements.end());
            break;
        case TypeKind::struct_: {
            const StructInfo* const struct_info = this->indexed_struct_info(type);
            if (struct_info == nullptr) {
                return std::nullopt;
            }
            components.reserve(struct_info->fields.size());
            for (const StructFieldInfo& field : struct_info->fields) {
                components.push_back(field.type);
            }
            break;
        }
        case TypeKind::enum_: {
            if (!is_valid(info.enum_underlying)) {
                return std::nullopt;
            }
            const std::span<const TypeHandle> payload_types = this->indexed_enum_payload_types(type);
            components.insert(components.end(), payload_types.begin(), payload_types.end());
            break;
        }
        default:
            break;
    }
    return components;
}

ResourceSemanticsSummary ResourceSemanticsClassifier::classify(const TypeHandle type) const
{
    if (!is_valid(type) || type.value >= this->checked_.types.size()) {
        return conservative_owned_summary();
    }
    const TypeInfo& root_info = this->checked_.types.get(type);
    if (!is_structural_type(root_info)) {
        ResourceSemanticsSummary summary = classify_leaf(root_info);
        if (root_info.kind == TypeKind::generic_param && this->generic_copy_predicate_
            && this->generic_copy_predicate_(type)) {
            summary.copy = ResourceCopyKind::copy;
        }
        return summary;
    }

    std::unordered_map<base::u32, ResourceSemanticsSummary> completed;
    std::unordered_set<base::u32> active;
    std::vector<ClassificationFrame> stack;
    stack.reserve(SEMA_RESOURCE_CLASSIFIER_INITIAL_STACK_CAPACITY);
    stack.push_back(ClassificationFrame{type, ClassificationFrameStage::enter, {}});

    while (!stack.empty()) {
        ClassificationFrame& frame = stack.back();
        if (completed.contains(frame.type.value)) {
            stack.pop_back();
            continue;
        }
        const TypeInfo& info = this->checked_.types.get(frame.type);
        if (frame.stage == ClassificationFrameStage::enter) {
            if (!is_structural_type(info)) {
                ResourceSemanticsSummary summary = classify_leaf(info);
                if (info.kind == TypeKind::generic_param && this->generic_copy_predicate_
                    && this->generic_copy_predicate_(frame.type)) {
                    summary.copy = ResourceCopyKind::copy;
                }
                completed.emplace(frame.type.value, summary);
                stack.pop_back();
                continue;
            }

            std::optional<std::vector<TypeHandle>> structural_components = this->structural_components(frame.type);
            if (!structural_components.has_value()) {
                completed.emplace(frame.type.value, conservative_owned_summary());
                stack.pop_back();
                continue;
            }
            frame.components = std::move(structural_components.value());
            frame.next_component_index = 0;
            frame.stage = ClassificationFrameStage::finish;
            active.insert(frame.type.value);
            continue;
        }

        while (frame.next_component_index < frame.components.size()) {
            const TypeHandle component = frame.components[frame.next_component_index];
            ++frame.next_component_index;
            if (!is_valid(component) || component.value >= this->checked_.types.size()
                || completed.contains(component.value)) {
                continue;
            }
            if (active.contains(component.value)) {
                completed.emplace(component.value, conservative_owned_summary());
                continue;
            }
            stack.push_back(ClassificationFrame{component, ClassificationFrameStage::enter, {}});
            break;
        }
        if (stack.back().stage == ClassificationFrameStage::enter) {
            continue;
        }

        ResourceSemanticsSummary summary = trivial_owned_summary();
        for (const TypeHandle component : frame.components) {
            const auto found = completed.find(component.value);
            summary =
                join_owned_components(summary, found == completed.end() ? conservative_owned_summary() : found->second);
        }
        completed.emplace(frame.type.value, summary);
        active.erase(frame.type.value);
        stack.pop_back();
    }

    const auto found = completed.find(type.value);
    return found == completed.end() ? conservative_owned_summary() : found->second;
}

} // namespace aurex::sema
