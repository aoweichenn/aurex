#include <aurex/infrastructure/query/dyn_ownership_runtime_facts.hpp>

#include <algorithm>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_FINGERPRINT_MARKER =
    "query.dyn_ownership_runtime_facts.v1";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_M17_SUBJECT =
    "M17 Dyn Ownership Runtime Preparation";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_OWNED_FACT =
    "owned_dyn_container_layout_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_OWNED_MOVE_FACT =
    "owned_dyn_move_boundary_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_OWNED_DROP_FACT =
    "owned_dyn_drop_obligation_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_OWNED_TOOLING_FACT =
    "owned_dyn_tooling_boundary_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_ERASED_DROP_FACT =
    "erased_drop_glue_identity_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_DROP_SLOT_FACT =
    "dynamic_drop_slot_layout_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_ERASED_RECEIVER_FACT =
    "dropck_erased_receiver_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_DROP_TOOLING_FACT =
    "dynamic_drop_tooling_boundary_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_ALLOCATOR_FACT =
    "allocator_identity_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_PLACEMENT_FACT =
    "allocator_placement_policy_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_DEALLOCATION_FACT =
    "owned_dyn_deallocation_policy_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_ALLOCATOR_TOOLING_FACT =
    "allocator_tooling_boundary_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_CLEANUP_FACT =
    "cleanup_runtime_boundary_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_CLEANUP_RESOURCE_FACT =
    "resource_cleanup_boundary_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_CLEANUP_TOOLING_FACT =
    "cleanup_dropck_tooling_boundary_fact";
constexpr base::u8 QUERY_DYN_OWNERSHIP_RUNTIME_INVALID_KIND_VALUE = 255U;
constexpr base::u8 QUERY_DYN_OWNERSHIP_RUNTIME_INVALID_STAGE_VALUE = 255U;
constexpr base::u8 QUERY_DYN_OWNERSHIP_RUNTIME_INVALID_POLICY_VALUE = 255U;
constexpr base::u64 QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_BOUNDARY_COUNT = 4U;
constexpr base::u64 QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_SINGLE_BOUNDARY_COUNT = 1U;
constexpr base::u64 QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_STDLIB_BLOCKER_COUNT = 2U;
constexpr base::u64 QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_RUNTIME_BLOCKER_COUNT = 4U;
constexpr base::u64 QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_DYNAMIC_DROP_BLOCKER_COUNT = 2U;
constexpr DynOwnershipRuntimeStage QUERY_DYN_OWNERSHIP_RUNTIME_OWNED_CONTAINER_STAGE =
    DynOwnershipRuntimeStage::future_standard_library_boundary;
constexpr DynOwnershipRuntimeStage QUERY_DYN_OWNERSHIP_RUNTIME_ERASED_DROP_STAGE =
    DynOwnershipRuntimeStage::future_runtime_lowering_boundary;
constexpr DynOwnershipRuntimeStage QUERY_DYN_OWNERSHIP_RUNTIME_ALLOCATOR_STAGE =
    DynOwnershipRuntimeStage::future_standard_library_boundary;
constexpr DynOwnershipRuntimeStage QUERY_DYN_OWNERSHIP_RUNTIME_CLEANUP_DROPCK_STAGE =
    DynOwnershipRuntimeStage::preparation;

[[nodiscard]] base::u8 stable_boundary_kind_value(
    const DynOwnershipRuntimeBoundaryKind kind) noexcept
{
    return is_valid(kind) ? static_cast<base::u8>(kind)
                          : QUERY_DYN_OWNERSHIP_RUNTIME_INVALID_KIND_VALUE;
}

[[nodiscard]] base::u8 stable_stage_value(const DynOwnershipRuntimeStage stage) noexcept
{
    return is_valid(stage) ? static_cast<base::u8>(stage)
                           : QUERY_DYN_OWNERSHIP_RUNTIME_INVALID_STAGE_VALUE;
}

[[nodiscard]] base::u8 stable_policy_value(const DynOwnershipRuntimePolicy policy) noexcept
{
    return is_valid(policy) ? static_cast<base::u8>(policy)
                            : QUERY_DYN_OWNERSHIP_RUNTIME_INVALID_POLICY_VALUE;
}

[[nodiscard]] bool nonempty(std::string_view value) noexcept
{
    return !value.empty();
}

[[nodiscard]] bool owned_container_policy_is_exact(
    const DynOwnedContainerBoundaryFact& fact) noexcept
{
    return fact.abi_policy == DynOwnershipRuntimePolicy::owning_dyn_container_v1
        && fact.metadata_policy == DynOwnershipRuntimePolicy::owning_dyn_metadata_v1;
}

[[nodiscard]] bool owned_container_stage_is_exact(
    const DynOwnedContainerBoundaryFact& fact) noexcept
{
    return fact.stage == QUERY_DYN_OWNERSHIP_RUNTIME_OWNED_CONTAINER_STAGE;
}

[[nodiscard]] bool erased_drop_policy_is_exact(
    const DynErasedDropGlueBoundaryFact& fact) noexcept
{
    return fact.metadata_policy == DynOwnershipRuntimePolicy::dynamic_drop_metadata_v1;
}

[[nodiscard]] bool erased_drop_stage_is_exact(
    const DynErasedDropGlueBoundaryFact& fact) noexcept
{
    return fact.stage == QUERY_DYN_OWNERSHIP_RUNTIME_ERASED_DROP_STAGE;
}

[[nodiscard]] bool allocator_policy_is_exact(const DynAllocatorBoundaryFact& fact) noexcept
{
    return fact.abi_policy == DynOwnershipRuntimePolicy::allocator_placement_policy_v1
        && fact.metadata_policy == DynOwnershipRuntimePolicy::allocator_metadata_v1;
}

[[nodiscard]] bool allocator_stage_is_exact(const DynAllocatorBoundaryFact& fact) noexcept
{
    return fact.stage == QUERY_DYN_OWNERSHIP_RUNTIME_ALLOCATOR_STAGE;
}

[[nodiscard]] bool cleanup_policy_is_exact(const DynCleanupDropckBoundaryFact& fact) noexcept
{
    return fact.policy == DynOwnershipRuntimePolicy::cleanup_dropck_boundary_v1;
}

[[nodiscard]] bool cleanup_stage_is_exact(const DynCleanupDropckBoundaryFact& fact) noexcept
{
    return fact.stage == QUERY_DYN_OWNERSHIP_RUNTIME_CLEANUP_DROPCK_STAGE;
}

[[nodiscard]] bool summary_equals(
    const DynOwnershipRuntimeSummary& lhs,
    const DynOwnershipRuntimeSummary& rhs) noexcept
{
    return lhs.boundary_count == rhs.boundary_count
        && lhs.owned_container_boundary_count == rhs.owned_container_boundary_count
        && lhs.erased_drop_glue_boundary_count == rhs.erased_drop_glue_boundary_count
        && lhs.allocator_boundary_count == rhs.allocator_boundary_count
        && lhs.cleanup_dropck_boundary_count == rhs.cleanup_dropck_boundary_count
        && lhs.standard_library_blocked_count == rhs.standard_library_blocked_count
        && lhs.runtime_lowering_blocked_count == rhs.runtime_lowering_blocked_count
        && lhs.box_surface_blocked_count == rhs.box_surface_blocked_count
        && lhs.allocator_api_blocked_count == rhs.allocator_api_blocked_count
        && lhs.dynamic_drop_dispatch_blocked_count == rhs.dynamic_drop_dispatch_blocked_count
        && lhs.borrowed_vtable_destructor_free_count == rhs.borrowed_vtable_destructor_free_count
        && lhs.cleanup_dropck_bridge_count == rhs.cleanup_dropck_bridge_count;
}

[[nodiscard]] bool has_required_fact_shape(const DynOwnershipRuntimeFacts& facts) noexcept
{
    return !facts.subject.empty()
        && facts.summary.boundary_count > 0U
        && facts.summary.boundary_count
            == facts.summary.owned_container_boundary_count
                + facts.summary.erased_drop_glue_boundary_count
                + facts.summary.allocator_boundary_count
                + facts.summary.cleanup_dropck_boundary_count;
}

void mix_summary(StableHashBuilder& builder, const DynOwnershipRuntimeSummary& summary) noexcept
{
    builder.mix_u64(summary.boundary_count);
    builder.mix_u64(summary.owned_container_boundary_count);
    builder.mix_u64(summary.erased_drop_glue_boundary_count);
    builder.mix_u64(summary.allocator_boundary_count);
    builder.mix_u64(summary.cleanup_dropck_boundary_count);
    builder.mix_u64(summary.standard_library_blocked_count);
    builder.mix_u64(summary.runtime_lowering_blocked_count);
    builder.mix_u64(summary.box_surface_blocked_count);
    builder.mix_u64(summary.allocator_api_blocked_count);
    builder.mix_u64(summary.dynamic_drop_dispatch_blocked_count);
    builder.mix_u64(summary.borrowed_vtable_destructor_free_count);
    builder.mix_u64(summary.cleanup_dropck_bridge_count);
}

void mix_owned_container_fact(
    StableHashBuilder& builder, const DynOwnedContainerBoundaryFact& fact) noexcept
{
    builder.mix_u8(stable_boundary_kind_value(DynOwnershipRuntimeBoundaryKind::owned_dyn_container));
    builder.mix_string(fact.fact_name);
    builder.mix_u8(stable_policy_value(fact.abi_policy));
    builder.mix_u8(stable_policy_value(fact.metadata_policy));
    builder.mix_u8(stable_stage_value(fact.stage));
    builder.mix_bool(fact.standard_library_blocked);
    builder.mix_bool(fact.runtime_lowering_blocked);
    builder.mix_bool(fact.box_surface_blocked);
    builder.mix_bool(fact.user_value_surface_blocked);
    builder.mix_string(fact.container_layout_fact);
    builder.mix_string(fact.move_boundary_fact);
    builder.mix_string(fact.drop_obligation_fact);
    builder.mix_string(fact.tooling_boundary_fact);
}

void mix_erased_drop_glue_fact(
    StableHashBuilder& builder, const DynErasedDropGlueBoundaryFact& fact) noexcept
{
    builder.mix_u8(stable_boundary_kind_value(DynOwnershipRuntimeBoundaryKind::erased_drop_glue));
    builder.mix_string(fact.fact_name);
    builder.mix_u8(stable_policy_value(fact.metadata_policy));
    builder.mix_u8(stable_stage_value(fact.stage));
    builder.mix_bool(fact.runtime_lowering_blocked);
    builder.mix_bool(fact.dynamic_drop_dispatch_blocked);
    builder.mix_bool(fact.borrowed_vtable_destructor_free);
    builder.mix_bool(fact.erased_receiver_recorded);
    builder.mix_string(fact.drop_glue_identity_fact);
    builder.mix_string(fact.dynamic_drop_slot_layout_fact);
    builder.mix_string(fact.erased_receiver_fact);
    builder.mix_string(fact.tooling_boundary_fact);
}

void mix_allocator_fact(StableHashBuilder& builder, const DynAllocatorBoundaryFact& fact) noexcept
{
    builder.mix_u8(stable_boundary_kind_value(DynOwnershipRuntimeBoundaryKind::allocator_boundary));
    builder.mix_string(fact.fact_name);
    builder.mix_u8(stable_policy_value(fact.abi_policy));
    builder.mix_u8(stable_policy_value(fact.metadata_policy));
    builder.mix_u8(stable_stage_value(fact.stage));
    builder.mix_bool(fact.standard_library_blocked);
    builder.mix_bool(fact.allocator_api_blocked);
    builder.mix_bool(fact.runtime_lowering_blocked);
    builder.mix_string(fact.allocator_identity_fact);
    builder.mix_string(fact.placement_policy_fact);
    builder.mix_string(fact.deallocation_policy_fact);
    builder.mix_string(fact.tooling_boundary_fact);
}

void mix_cleanup_dropck_fact(
    StableHashBuilder& builder, const DynCleanupDropckBoundaryFact& fact) noexcept
{
    builder.mix_u8(stable_boundary_kind_value(DynOwnershipRuntimeBoundaryKind::cleanup_dropck_boundary));
    builder.mix_string(fact.fact_name);
    builder.mix_u8(stable_policy_value(fact.policy));
    builder.mix_u8(stable_stage_value(fact.stage));
    builder.mix_bool(fact.cleanup_bridge_recorded);
    builder.mix_bool(fact.dropck_bridge_recorded);
    builder.mix_bool(fact.runtime_lowering_blocked);
    builder.mix_bool(fact.dynamic_drop_dispatch_blocked);
    builder.mix_string(fact.cleanup_runtime_boundary_fact);
    builder.mix_string(fact.dropck_erased_receiver_fact);
    builder.mix_string(fact.resource_cleanup_fact);
    builder.mix_string(fact.tooling_boundary_fact);
}

[[nodiscard]] const char* fallback_name(const std::string& value, const char* fallback) noexcept
{
    return value.empty() ? fallback : value.c_str();
}

[[nodiscard]] DynOwnedContainerBoundaryFact m17_owned_container_fact()
{
    return DynOwnedContainerBoundaryFact{
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_OWNED_FACT),
        DynOwnershipRuntimePolicy::owning_dyn_container_v1,
        DynOwnershipRuntimePolicy::owning_dyn_metadata_v1,
        QUERY_DYN_OWNERSHIP_RUNTIME_OWNED_CONTAINER_STAGE,
        true,
        true,
        true,
        true,
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_OWNED_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_OWNED_MOVE_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_OWNED_DROP_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_OWNED_TOOLING_FACT),
    };
}

[[nodiscard]] DynErasedDropGlueBoundaryFact m17_erased_drop_glue_fact()
{
    return DynErasedDropGlueBoundaryFact{
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_ERASED_DROP_FACT),
        DynOwnershipRuntimePolicy::dynamic_drop_metadata_v1,
        QUERY_DYN_OWNERSHIP_RUNTIME_ERASED_DROP_STAGE,
        true,
        true,
        true,
        true,
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_ERASED_DROP_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_DROP_SLOT_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_ERASED_RECEIVER_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_DROP_TOOLING_FACT),
    };
}

[[nodiscard]] DynAllocatorBoundaryFact m17_allocator_fact()
{
    return DynAllocatorBoundaryFact{
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_ALLOCATOR_FACT),
        DynOwnershipRuntimePolicy::allocator_placement_policy_v1,
        DynOwnershipRuntimePolicy::allocator_metadata_v1,
        QUERY_DYN_OWNERSHIP_RUNTIME_ALLOCATOR_STAGE,
        true,
        true,
        true,
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_ALLOCATOR_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_PLACEMENT_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_DEALLOCATION_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_ALLOCATOR_TOOLING_FACT),
    };
}

[[nodiscard]] DynCleanupDropckBoundaryFact m17_cleanup_dropck_fact()
{
    return DynCleanupDropckBoundaryFact{
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_CLEANUP_FACT),
        DynOwnershipRuntimePolicy::cleanup_dropck_boundary_v1,
        QUERY_DYN_OWNERSHIP_RUNTIME_CLEANUP_DROPCK_STAGE,
        true,
        true,
        true,
        true,
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_CLEANUP_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_ERASED_RECEIVER_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_CLEANUP_RESOURCE_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_CLEANUP_TOOLING_FACT),
    };
}

} // namespace

std::string_view dyn_ownership_runtime_boundary_kind_name(
    const DynOwnershipRuntimeBoundaryKind kind) noexcept
{
    switch (kind) {
        case DynOwnershipRuntimeBoundaryKind::owned_dyn_container:
            return "owned_dyn_container";
        case DynOwnershipRuntimeBoundaryKind::erased_drop_glue:
            return "erased_drop_glue";
        case DynOwnershipRuntimeBoundaryKind::allocator_boundary:
            return "allocator_boundary";
        case DynOwnershipRuntimeBoundaryKind::cleanup_dropck_boundary:
            return "cleanup_dropck_boundary";
    }
    return "invalid";
}

std::string_view dyn_ownership_runtime_stage_name(
    const DynOwnershipRuntimeStage stage) noexcept
{
    switch (stage) {
        case DynOwnershipRuntimeStage::preparation:
            return "preparation";
        case DynOwnershipRuntimeStage::future_standard_library_boundary:
            return "future_standard_library_boundary";
        case DynOwnershipRuntimeStage::future_runtime_lowering_boundary:
            return "future_runtime_lowering_boundary";
    }
    return "invalid";
}

std::string_view dyn_ownership_runtime_policy_name(
    const DynOwnershipRuntimePolicy policy) noexcept
{
    switch (policy) {
        case DynOwnershipRuntimePolicy::owning_dyn_container_v1:
            return "owning_dyn_container_v1";
        case DynOwnershipRuntimePolicy::owning_dyn_metadata_v1:
            return "owning_dyn_metadata_v1";
        case DynOwnershipRuntimePolicy::dynamic_drop_metadata_v1:
            return "dynamic_drop_metadata_v1";
        case DynOwnershipRuntimePolicy::allocator_placement_policy_v1:
            return "allocator_placement_policy_v1";
        case DynOwnershipRuntimePolicy::allocator_metadata_v1:
            return "allocator_metadata_v1";
        case DynOwnershipRuntimePolicy::cleanup_dropck_boundary_v1:
            return "cleanup_dropck_boundary_v1";
    }
    return "invalid";
}

bool is_valid(const DynOwnershipRuntimeBoundaryKind kind) noexcept
{
    return kind == DynOwnershipRuntimeBoundaryKind::owned_dyn_container
        || kind == DynOwnershipRuntimeBoundaryKind::erased_drop_glue
        || kind == DynOwnershipRuntimeBoundaryKind::allocator_boundary
        || kind == DynOwnershipRuntimeBoundaryKind::cleanup_dropck_boundary;
}

bool is_valid(const DynOwnershipRuntimeStage stage) noexcept
{
    return stage == DynOwnershipRuntimeStage::preparation
        || stage == DynOwnershipRuntimeStage::future_standard_library_boundary
        || stage == DynOwnershipRuntimeStage::future_runtime_lowering_boundary;
}

bool is_valid(const DynOwnershipRuntimePolicy policy) noexcept
{
    return policy == DynOwnershipRuntimePolicy::owning_dyn_container_v1
        || policy == DynOwnershipRuntimePolicy::owning_dyn_metadata_v1
        || policy == DynOwnershipRuntimePolicy::dynamic_drop_metadata_v1
        || policy == DynOwnershipRuntimePolicy::allocator_placement_policy_v1
        || policy == DynOwnershipRuntimePolicy::allocator_metadata_v1
        || policy == DynOwnershipRuntimePolicy::cleanup_dropck_boundary_v1;
}

bool is_valid(const DynOwnedContainerBoundaryFact& fact) noexcept
{
    return nonempty(fact.fact_name)
        && owned_container_policy_is_exact(fact)
        && owned_container_stage_is_exact(fact)
        && fact.standard_library_blocked
        && fact.runtime_lowering_blocked
        && fact.box_surface_blocked
        && fact.user_value_surface_blocked
        && nonempty(fact.container_layout_fact)
        && nonempty(fact.move_boundary_fact)
        && nonempty(fact.drop_obligation_fact)
        && nonempty(fact.tooling_boundary_fact);
}

bool is_valid(const DynErasedDropGlueBoundaryFact& fact) noexcept
{
    return nonempty(fact.fact_name)
        && erased_drop_policy_is_exact(fact)
        && erased_drop_stage_is_exact(fact)
        && fact.runtime_lowering_blocked
        && fact.dynamic_drop_dispatch_blocked
        && fact.borrowed_vtable_destructor_free
        && fact.erased_receiver_recorded
        && nonempty(fact.drop_glue_identity_fact)
        && nonempty(fact.dynamic_drop_slot_layout_fact)
        && nonempty(fact.erased_receiver_fact)
        && nonempty(fact.tooling_boundary_fact);
}

bool is_valid(const DynAllocatorBoundaryFact& fact) noexcept
{
    return nonempty(fact.fact_name)
        && allocator_policy_is_exact(fact)
        && allocator_stage_is_exact(fact)
        && fact.standard_library_blocked
        && fact.allocator_api_blocked
        && fact.runtime_lowering_blocked
        && nonempty(fact.allocator_identity_fact)
        && nonempty(fact.placement_policy_fact)
        && nonempty(fact.deallocation_policy_fact)
        && nonempty(fact.tooling_boundary_fact);
}

bool is_valid(const DynCleanupDropckBoundaryFact& fact) noexcept
{
    return nonempty(fact.fact_name)
        && cleanup_policy_is_exact(fact)
        && cleanup_stage_is_exact(fact)
        && fact.cleanup_bridge_recorded
        && fact.dropck_bridge_recorded
        && fact.runtime_lowering_blocked
        && fact.dynamic_drop_dispatch_blocked
        && nonempty(fact.cleanup_runtime_boundary_fact)
        && nonempty(fact.dropck_erased_receiver_fact)
        && nonempty(fact.resource_cleanup_fact)
        && nonempty(fact.tooling_boundary_fact);
}

bool is_valid(
    const DynOwnershipRuntimeSummary& summary,
    const DynOwnershipRuntimeFacts& facts) noexcept
{
    return summary_equals(summary, summarize_dyn_ownership_runtime_counts(facts));
}

bool is_valid(const DynOwnershipRuntimeFacts& facts) noexcept
{
    return std::all_of(facts.owned_containers.begin(), facts.owned_containers.end(),
               [](const DynOwnedContainerBoundaryFact& fact) {
                   return is_valid(fact);
               })
        && std::all_of(facts.erased_drop_glue.begin(), facts.erased_drop_glue.end(),
            [](const DynErasedDropGlueBoundaryFact& fact) {
                return is_valid(fact);
            })
        && std::all_of(facts.allocator_boundaries.begin(), facts.allocator_boundaries.end(),
            [](const DynAllocatorBoundaryFact& fact) {
                return is_valid(fact);
            })
        && std::all_of(facts.cleanup_dropck_boundaries.begin(), facts.cleanup_dropck_boundaries.end(),
            [](const DynCleanupDropckBoundaryFact& fact) {
                return is_valid(fact);
            })
        && is_valid(facts.summary, facts)
        && has_required_fact_shape(facts)
        && (facts.fingerprint == StableFingerprint128{}
            || facts.fingerprint == dyn_ownership_runtime_facts_fingerprint(facts));
}

bool is_valid_m17_dyn_ownership_runtime_preparation_baseline(
    const DynOwnershipRuntimeFacts& facts) noexcept
{
    return is_valid(facts)
        && std::string_view(facts.subject) == QUERY_DYN_OWNERSHIP_RUNTIME_M17_SUBJECT
        && facts.summary.boundary_count == QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_BOUNDARY_COUNT
        && facts.summary.owned_container_boundary_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_SINGLE_BOUNDARY_COUNT
        && facts.summary.erased_drop_glue_boundary_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_SINGLE_BOUNDARY_COUNT
        && facts.summary.allocator_boundary_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_SINGLE_BOUNDARY_COUNT
        && facts.summary.cleanup_dropck_boundary_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_SINGLE_BOUNDARY_COUNT
        && facts.summary.standard_library_blocked_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_STDLIB_BLOCKER_COUNT
        && facts.summary.runtime_lowering_blocked_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_RUNTIME_BLOCKER_COUNT
        && facts.summary.box_surface_blocked_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_SINGLE_BOUNDARY_COUNT
        && facts.summary.allocator_api_blocked_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_SINGLE_BOUNDARY_COUNT
        && facts.summary.dynamic_drop_dispatch_blocked_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_DYNAMIC_DROP_BLOCKER_COUNT
        && facts.summary.borrowed_vtable_destructor_free_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_SINGLE_BOUNDARY_COUNT
        && facts.summary.cleanup_dropck_bridge_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_M17_REQUIRED_SINGLE_BOUNDARY_COUNT;
}

void record_dyn_owned_container_boundary_fact(
    DynOwnershipRuntimeFacts& facts, DynOwnedContainerBoundaryFact fact)
{
    facts.owned_containers.push_back(std::move(fact));
    facts.summary = summarize_dyn_ownership_runtime_counts(facts);
}

void record_dyn_erased_drop_glue_boundary_fact(
    DynOwnershipRuntimeFacts& facts, DynErasedDropGlueBoundaryFact fact)
{
    facts.erased_drop_glue.push_back(std::move(fact));
    facts.summary = summarize_dyn_ownership_runtime_counts(facts);
}

void record_dyn_allocator_boundary_fact(DynOwnershipRuntimeFacts& facts, DynAllocatorBoundaryFact fact)
{
    facts.allocator_boundaries.push_back(std::move(fact));
    facts.summary = summarize_dyn_ownership_runtime_counts(facts);
}

void record_dyn_cleanup_dropck_boundary_fact(
    DynOwnershipRuntimeFacts& facts, DynCleanupDropckBoundaryFact fact)
{
    facts.cleanup_dropck_boundaries.push_back(std::move(fact));
    facts.summary = summarize_dyn_ownership_runtime_counts(facts);
}

DynOwnershipRuntimeSummary summarize_dyn_ownership_runtime_counts(
    const DynOwnershipRuntimeFacts& facts) noexcept
{
    DynOwnershipRuntimeSummary summary;
    summary.owned_container_boundary_count = static_cast<base::u64>(facts.owned_containers.size());
    summary.erased_drop_glue_boundary_count = static_cast<base::u64>(facts.erased_drop_glue.size());
    summary.allocator_boundary_count = static_cast<base::u64>(facts.allocator_boundaries.size());
    summary.cleanup_dropck_boundary_count =
        static_cast<base::u64>(facts.cleanup_dropck_boundaries.size());
    summary.boundary_count = summary.owned_container_boundary_count
        + summary.erased_drop_glue_boundary_count
        + summary.allocator_boundary_count
        + summary.cleanup_dropck_boundary_count;

    for (const DynOwnedContainerBoundaryFact& fact : facts.owned_containers) {
        if (fact.standard_library_blocked) {
            ++summary.standard_library_blocked_count;
        }
        if (fact.runtime_lowering_blocked) {
            ++summary.runtime_lowering_blocked_count;
        }
        if (fact.box_surface_blocked) {
            ++summary.box_surface_blocked_count;
        }
    }
    for (const DynErasedDropGlueBoundaryFact& fact : facts.erased_drop_glue) {
        if (fact.runtime_lowering_blocked) {
            ++summary.runtime_lowering_blocked_count;
        }
        if (fact.dynamic_drop_dispatch_blocked) {
            ++summary.dynamic_drop_dispatch_blocked_count;
        }
        if (fact.borrowed_vtable_destructor_free) {
            ++summary.borrowed_vtable_destructor_free_count;
        }
    }
    for (const DynAllocatorBoundaryFact& fact : facts.allocator_boundaries) {
        if (fact.standard_library_blocked) {
            ++summary.standard_library_blocked_count;
        }
        if (fact.allocator_api_blocked) {
            ++summary.allocator_api_blocked_count;
        }
        if (fact.runtime_lowering_blocked) {
            ++summary.runtime_lowering_blocked_count;
        }
    }
    for (const DynCleanupDropckBoundaryFact& fact : facts.cleanup_dropck_boundaries) {
        if (fact.runtime_lowering_blocked) {
            ++summary.runtime_lowering_blocked_count;
        }
        if (fact.dynamic_drop_dispatch_blocked) {
            ++summary.dynamic_drop_dispatch_blocked_count;
        }
        if (fact.cleanup_bridge_recorded && fact.dropck_bridge_recorded) {
            ++summary.cleanup_dropck_bridge_count;
        }
    }
    return summary;
}

StableFingerprint128 dyn_ownership_runtime_facts_fingerprint(
    const DynOwnershipRuntimeFacts& facts) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_DYN_OWNERSHIP_RUNTIME_FINGERPRINT_MARKER);
    builder.mix_string(facts.subject);
    builder.mix_u64(static_cast<base::u64>(facts.owned_containers.size()));
    builder.mix_u64(static_cast<base::u64>(facts.erased_drop_glue.size()));
    builder.mix_u64(static_cast<base::u64>(facts.allocator_boundaries.size()));
    builder.mix_u64(static_cast<base::u64>(facts.cleanup_dropck_boundaries.size()));
    mix_summary(builder, facts.summary);
    for (const DynOwnedContainerBoundaryFact& fact : facts.owned_containers) {
        mix_owned_container_fact(builder, fact);
    }
    for (const DynErasedDropGlueBoundaryFact& fact : facts.erased_drop_glue) {
        mix_erased_drop_glue_fact(builder, fact);
    }
    for (const DynAllocatorBoundaryFact& fact : facts.allocator_boundaries) {
        mix_allocator_fact(builder, fact);
    }
    for (const DynCleanupDropckBoundaryFact& fact : facts.cleanup_dropck_boundaries) {
        mix_cleanup_dropck_fact(builder, fact);
    }
    return builder.finish();
}

std::string summarize_dyn_ownership_runtime_facts(const DynOwnershipRuntimeFacts& facts)
{
    std::ostringstream label;
    label << "dyn_ownership_runtime_facts subject="
          << (facts.subject.empty() ? "<anonymous>" : facts.subject)
          << " boundaries=" << facts.summary.boundary_count
          << " owned_containers=" << facts.summary.owned_container_boundary_count
          << " erased_drop_glue=" << facts.summary.erased_drop_glue_boundary_count
          << " allocator_boundaries=" << facts.summary.allocator_boundary_count
          << " cleanup_dropck_boundaries=" << facts.summary.cleanup_dropck_boundary_count
          << " standard_library_blocked=" << facts.summary.standard_library_blocked_count
          << " runtime_lowering_blocked=" << facts.summary.runtime_lowering_blocked_count
          << " box_surface_blocked=" << facts.summary.box_surface_blocked_count
          << " allocator_api_blocked=" << facts.summary.allocator_api_blocked_count
          << " dynamic_drop_dispatch_blocked="
          << facts.summary.dynamic_drop_dispatch_blocked_count
          << " borrowed_vtable_destructor_free="
          << facts.summary.borrowed_vtable_destructor_free_count
          << " cleanup_dropck_bridges=" << facts.summary.cleanup_dropck_bridge_count;
    if (!facts.owned_containers.empty()) {
        label << " first_owned_container="
              << fallback_name(facts.owned_containers.front().container_layout_fact, "<unknown>")
              << " policy="
              << dyn_ownership_runtime_policy_name(facts.owned_containers.front().abi_policy)
              << " metadata="
              << dyn_ownership_runtime_policy_name(facts.owned_containers.front().metadata_policy);
    }
    if (!facts.erased_drop_glue.empty()) {
        label << " first_erased_drop="
              << fallback_name(facts.erased_drop_glue.front().drop_glue_identity_fact, "<unknown>")
              << " borrowed_vtable_destructor_free="
              << (facts.erased_drop_glue.front().borrowed_vtable_destructor_free ? "yes" : "no");
    }
    label << " fingerprint=" << debug_string(dyn_ownership_runtime_facts_fingerprint(facts));
    return label.str();
}

std::string dump_dyn_ownership_runtime_facts(const DynOwnershipRuntimeFacts& facts)
{
    std::ostringstream stream;
    stream << "dyn_ownership_runtime_facts subject="
           << (facts.subject.empty() ? "<anonymous>" : facts.subject)
           << " boundaries=" << facts.summary.boundary_count
           << " standard_library_blocked=" << facts.summary.standard_library_blocked_count
           << " runtime_lowering_blocked=" << facts.summary.runtime_lowering_blocked_count
           << " fingerprint=" << debug_string(dyn_ownership_runtime_facts_fingerprint(facts))
           << '\n';

    for (base::usize index = 0; index < facts.owned_containers.size(); ++index) {
        const DynOwnedContainerBoundaryFact& fact = facts.owned_containers[index];
        stream << "  owned_dyn_container_boundary_fact #" << index
               << " name=" << fallback_name(fact.fact_name, "<anonymous>")
               << " abi_policy=" << dyn_ownership_runtime_policy_name(fact.abi_policy)
               << " metadata_policy=" << dyn_ownership_runtime_policy_name(fact.metadata_policy)
               << " stage=" << dyn_ownership_runtime_stage_name(fact.stage)
               << " standard_library_blocked=" << (fact.standard_library_blocked ? "yes" : "no")
               << " runtime_lowering_blocked=" << (fact.runtime_lowering_blocked ? "yes" : "no")
               << " box_surface_blocked=" << (fact.box_surface_blocked ? "yes" : "no")
               << " user_value_surface_blocked="
               << (fact.user_value_surface_blocked ? "yes" : "no")
               << " layout=" << fallback_name(fact.container_layout_fact, "<unknown>")
               << " move=" << fallback_name(fact.move_boundary_fact, "<unknown>")
               << " drop=" << fallback_name(fact.drop_obligation_fact, "<unknown>")
               << " tooling=" << fallback_name(fact.tooling_boundary_fact, "<unknown>")
               << '\n';
    }
    for (base::usize index = 0; index < facts.erased_drop_glue.size(); ++index) {
        const DynErasedDropGlueBoundaryFact& fact = facts.erased_drop_glue[index];
        stream << "  erased_drop_glue_boundary_fact #" << index
               << " name=" << fallback_name(fact.fact_name, "<anonymous>")
               << " metadata_policy=" << dyn_ownership_runtime_policy_name(fact.metadata_policy)
               << " stage=" << dyn_ownership_runtime_stage_name(fact.stage)
               << " runtime_lowering_blocked=" << (fact.runtime_lowering_blocked ? "yes" : "no")
               << " dynamic_drop_dispatch_blocked="
               << (fact.dynamic_drop_dispatch_blocked ? "yes" : "no")
               << " borrowed_vtable_destructor_free="
               << (fact.borrowed_vtable_destructor_free ? "yes" : "no")
               << " receiver_recorded=" << (fact.erased_receiver_recorded ? "yes" : "no")
               << " identity=" << fallback_name(fact.drop_glue_identity_fact, "<unknown>")
               << " drop_slot=" << fallback_name(fact.dynamic_drop_slot_layout_fact, "<unknown>")
               << " receiver=" << fallback_name(fact.erased_receiver_fact, "<unknown>")
               << " tooling=" << fallback_name(fact.tooling_boundary_fact, "<unknown>")
               << '\n';
    }
    for (base::usize index = 0; index < facts.allocator_boundaries.size(); ++index) {
        const DynAllocatorBoundaryFact& fact = facts.allocator_boundaries[index];
        stream << "  allocator_boundary_fact #" << index
               << " name=" << fallback_name(fact.fact_name, "<anonymous>")
               << " abi_policy=" << dyn_ownership_runtime_policy_name(fact.abi_policy)
               << " metadata_policy=" << dyn_ownership_runtime_policy_name(fact.metadata_policy)
               << " stage=" << dyn_ownership_runtime_stage_name(fact.stage)
               << " standard_library_blocked=" << (fact.standard_library_blocked ? "yes" : "no")
               << " allocator_api_blocked=" << (fact.allocator_api_blocked ? "yes" : "no")
               << " runtime_lowering_blocked=" << (fact.runtime_lowering_blocked ? "yes" : "no")
               << " identity=" << fallback_name(fact.allocator_identity_fact, "<unknown>")
               << " placement=" << fallback_name(fact.placement_policy_fact, "<unknown>")
               << " deallocation=" << fallback_name(fact.deallocation_policy_fact, "<unknown>")
               << " tooling=" << fallback_name(fact.tooling_boundary_fact, "<unknown>")
               << '\n';
    }
    for (base::usize index = 0; index < facts.cleanup_dropck_boundaries.size(); ++index) {
        const DynCleanupDropckBoundaryFact& fact = facts.cleanup_dropck_boundaries[index];
        stream << "  cleanup_dropck_boundary_fact #" << index
               << " name=" << fallback_name(fact.fact_name, "<anonymous>")
               << " policy=" << dyn_ownership_runtime_policy_name(fact.policy)
               << " stage=" << dyn_ownership_runtime_stage_name(fact.stage)
               << " cleanup_bridge=" << (fact.cleanup_bridge_recorded ? "yes" : "no")
               << " dropck_bridge=" << (fact.dropck_bridge_recorded ? "yes" : "no")
               << " runtime_lowering_blocked=" << (fact.runtime_lowering_blocked ? "yes" : "no")
               << " dynamic_drop_dispatch_blocked="
               << (fact.dynamic_drop_dispatch_blocked ? "yes" : "no")
               << " cleanup=" << fallback_name(fact.cleanup_runtime_boundary_fact, "<unknown>")
               << " dropck=" << fallback_name(fact.dropck_erased_receiver_fact, "<unknown>")
               << " resource=" << fallback_name(fact.resource_cleanup_fact, "<unknown>")
               << " tooling=" << fallback_name(fact.tooling_boundary_fact, "<unknown>")
               << '\n';
    }
    return stream.str();
}

DynOwnershipRuntimeFacts m17_dyn_ownership_runtime_preparation_baseline()
{
    DynOwnershipRuntimeFacts facts;
    facts.subject = std::string(QUERY_DYN_OWNERSHIP_RUNTIME_M17_SUBJECT);
    record_dyn_owned_container_boundary_fact(facts, m17_owned_container_fact());
    record_dyn_erased_drop_glue_boundary_fact(facts, m17_erased_drop_glue_fact());
    record_dyn_allocator_boundary_fact(facts, m17_allocator_fact());
    record_dyn_cleanup_dropck_boundary_fact(facts, m17_cleanup_dropck_fact());
    facts.fingerprint = dyn_ownership_runtime_facts_fingerprint(facts);
    return facts;
}

} // namespace aurex::query
