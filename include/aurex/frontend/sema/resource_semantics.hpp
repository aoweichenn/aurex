#pragma once

#include <aurex/frontend/sema/type.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aurex::sema {

struct CheckedModule;
struct StructInfo;

enum class ResourceCopyKind {
    copy,
    move_only,
};

enum class ResourceDiscardKind {
    discard,
    must_consume,
};

enum class ResourceCleanupKind {
    trivial,
    needs_drop,
};

enum class ResourceOwnershipKind {
    owned_value,
    borrowed_view,
    raw_pointer,
    shared_managed,
};

struct ResourceSemanticsSummary {
    ResourceCopyKind copy = ResourceCopyKind::move_only;
    ResourceDiscardKind discard = ResourceDiscardKind::discard;
    ResourceCleanupKind cleanup = ResourceCleanupKind::needs_drop;
    ResourceOwnershipKind ownership = ResourceOwnershipKind::owned_value;

    [[nodiscard]] friend constexpr bool operator==(
        ResourceSemanticsSummary lhs, ResourceSemanticsSummary rhs) noexcept = default;
};

[[nodiscard]] constexpr bool resource_is_copy(const ResourceSemanticsSummary summary) noexcept
{
    return summary.copy == ResourceCopyKind::copy;
}

[[nodiscard]] constexpr bool resource_needs_drop(const ResourceSemanticsSummary summary) noexcept
{
    return summary.cleanup == ResourceCleanupKind::needs_drop;
}

[[nodiscard]] std::string_view resource_copy_kind_name(ResourceCopyKind kind) noexcept;
[[nodiscard]] std::string_view resource_discard_kind_name(ResourceDiscardKind kind) noexcept;
[[nodiscard]] std::string_view resource_cleanup_kind_name(ResourceCleanupKind kind) noexcept;
[[nodiscard]] std::string_view resource_ownership_kind_name(ResourceOwnershipKind kind) noexcept;
[[nodiscard]] std::string resource_semantics_debug_string(ResourceSemanticsSummary summary);
[[nodiscard]] query::StableFingerprint128 resource_semantics_fingerprint(ResourceSemanticsSummary summary) noexcept;

class ResourceSemanticsClassifier final {
public:
    using GenericCopyPredicate = std::function<bool(TypeHandle)>;
    using StructuralComponentProvider = std::function<std::optional<std::vector<TypeHandle>>(TypeHandle)>;

    explicit ResourceSemanticsClassifier(const CheckedModule& checked, GenericCopyPredicate generic_copy_predicate = {},
        StructuralComponentProvider structural_component_provider = {});

    [[nodiscard]] ResourceSemanticsSummary classify(TypeHandle type) const;

private:
    void build_indexes();
    [[nodiscard]] const StructInfo* indexed_struct_info(TypeHandle type) const;
    [[nodiscard]] std::span<const TypeHandle> indexed_enum_payload_types(TypeHandle type) const;
    [[nodiscard]] std::optional<std::vector<TypeHandle>> structural_components(TypeHandle type) const;

    const CheckedModule& checked_;
    GenericCopyPredicate generic_copy_predicate_;
    StructuralComponentProvider structural_component_provider_;
    std::unordered_map<base::u32, const StructInfo*> struct_infos_by_type_;
    std::unordered_map<base::u32, std::vector<TypeHandle>> enum_payload_types_by_type_;
};

} // namespace aurex::sema
