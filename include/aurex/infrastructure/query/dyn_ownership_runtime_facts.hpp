#pragma once

#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

enum class DynOwnershipRuntimeBoundaryKind : base::u8 {
    owned_dyn_container = 1,
    erased_drop_glue,
    allocator_boundary,
    cleanup_dropck_boundary,
};

enum class DynOwnershipRuntimeStage : base::u8 {
    preparation = 1,
    future_standard_library_boundary,
    future_runtime_lowering_boundary,
};

enum class DynOwnershipRuntimePolicy : base::u8 {
    owning_dyn_container_v1 = 1,
    owning_dyn_metadata_v1,
    dynamic_drop_metadata_v1,
    allocator_placement_policy_v1,
    allocator_metadata_v1,
    cleanup_dropck_boundary_v1,
};

struct DynOwnedContainerBoundaryFact {
    std::string fact_name;
    DynOwnershipRuntimePolicy abi_policy = DynOwnershipRuntimePolicy::owning_dyn_container_v1;
    DynOwnershipRuntimePolicy metadata_policy = DynOwnershipRuntimePolicy::owning_dyn_metadata_v1;
    DynOwnershipRuntimeStage stage = DynOwnershipRuntimeStage::future_standard_library_boundary;
    bool standard_library_blocked = true;
    bool runtime_lowering_blocked = true;
    bool box_surface_blocked = true;
    bool user_value_surface_blocked = true;
    std::string container_layout_fact;
    std::string move_boundary_fact;
    std::string drop_obligation_fact;
    std::string tooling_boundary_fact;
};

struct DynErasedDropGlueBoundaryFact {
    std::string fact_name;
    DynOwnershipRuntimePolicy metadata_policy = DynOwnershipRuntimePolicy::dynamic_drop_metadata_v1;
    DynOwnershipRuntimeStage stage = DynOwnershipRuntimeStage::future_runtime_lowering_boundary;
    bool runtime_lowering_blocked = true;
    bool dynamic_drop_dispatch_blocked = true;
    bool borrowed_vtable_destructor_free = true;
    bool erased_receiver_recorded = true;
    std::string drop_glue_identity_fact;
    std::string dynamic_drop_slot_layout_fact;
    std::string erased_receiver_fact;
    std::string tooling_boundary_fact;
};

struct DynAllocatorBoundaryFact {
    std::string fact_name;
    DynOwnershipRuntimePolicy abi_policy = DynOwnershipRuntimePolicy::allocator_placement_policy_v1;
    DynOwnershipRuntimePolicy metadata_policy = DynOwnershipRuntimePolicy::allocator_metadata_v1;
    DynOwnershipRuntimeStage stage = DynOwnershipRuntimeStage::future_standard_library_boundary;
    bool standard_library_blocked = true;
    bool allocator_api_blocked = true;
    bool runtime_lowering_blocked = true;
    std::string allocator_identity_fact;
    std::string placement_policy_fact;
    std::string deallocation_policy_fact;
    std::string tooling_boundary_fact;
};

struct DynCleanupDropckBoundaryFact {
    std::string fact_name;
    DynOwnershipRuntimePolicy policy = DynOwnershipRuntimePolicy::cleanup_dropck_boundary_v1;
    DynOwnershipRuntimeStage stage = DynOwnershipRuntimeStage::preparation;
    bool cleanup_bridge_recorded = true;
    bool dropck_bridge_recorded = true;
    bool runtime_lowering_blocked = true;
    bool dynamic_drop_dispatch_blocked = true;
    std::string cleanup_runtime_boundary_fact;
    std::string dropck_erased_receiver_fact;
    std::string resource_cleanup_fact;
    std::string tooling_boundary_fact;
};

struct DynOwnershipRuntimeSummary {
    base::u64 boundary_count = 0;
    base::u64 owned_container_boundary_count = 0;
    base::u64 erased_drop_glue_boundary_count = 0;
    base::u64 allocator_boundary_count = 0;
    base::u64 cleanup_dropck_boundary_count = 0;
    base::u64 standard_library_blocked_count = 0;
    base::u64 runtime_lowering_blocked_count = 0;
    base::u64 box_surface_blocked_count = 0;
    base::u64 allocator_api_blocked_count = 0;
    base::u64 dynamic_drop_dispatch_blocked_count = 0;
    base::u64 borrowed_vtable_destructor_free_count = 0;
    base::u64 cleanup_dropck_bridge_count = 0;
};

struct DynOwnershipRuntimeFacts {
    std::string subject;
    std::vector<DynOwnedContainerBoundaryFact> owned_containers;
    std::vector<DynErasedDropGlueBoundaryFact> erased_drop_glue;
    std::vector<DynAllocatorBoundaryFact> allocator_boundaries;
    std::vector<DynCleanupDropckBoundaryFact> cleanup_dropck_boundaries;
    DynOwnershipRuntimeSummary summary;
    StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view dyn_ownership_runtime_boundary_kind_name(
    DynOwnershipRuntimeBoundaryKind kind) noexcept;
[[nodiscard]] std::string_view dyn_ownership_runtime_stage_name(
    DynOwnershipRuntimeStage stage) noexcept;
[[nodiscard]] std::string_view dyn_ownership_runtime_policy_name(
    DynOwnershipRuntimePolicy policy) noexcept;

[[nodiscard]] bool is_valid(DynOwnershipRuntimeBoundaryKind kind) noexcept;
[[nodiscard]] bool is_valid(DynOwnershipRuntimeStage stage) noexcept;
[[nodiscard]] bool is_valid(DynOwnershipRuntimePolicy policy) noexcept;
[[nodiscard]] bool is_valid(const DynOwnedContainerBoundaryFact& fact) noexcept;
[[nodiscard]] bool is_valid(const DynErasedDropGlueBoundaryFact& fact) noexcept;
[[nodiscard]] bool is_valid(const DynAllocatorBoundaryFact& fact) noexcept;
[[nodiscard]] bool is_valid(const DynCleanupDropckBoundaryFact& fact) noexcept;
[[nodiscard]] bool is_valid(const DynOwnershipRuntimeSummary& summary,
    const DynOwnershipRuntimeFacts& facts) noexcept;
[[nodiscard]] bool is_valid(const DynOwnershipRuntimeFacts& facts) noexcept;
[[nodiscard]] bool is_valid_m17_dyn_ownership_runtime_preparation_baseline(
    const DynOwnershipRuntimeFacts& facts) noexcept;

void record_dyn_owned_container_boundary_fact(
    DynOwnershipRuntimeFacts& facts, DynOwnedContainerBoundaryFact fact);
void record_dyn_erased_drop_glue_boundary_fact(
    DynOwnershipRuntimeFacts& facts, DynErasedDropGlueBoundaryFact fact);
void record_dyn_allocator_boundary_fact(
    DynOwnershipRuntimeFacts& facts, DynAllocatorBoundaryFact fact);
void record_dyn_cleanup_dropck_boundary_fact(
    DynOwnershipRuntimeFacts& facts, DynCleanupDropckBoundaryFact fact);

[[nodiscard]] DynOwnershipRuntimeSummary summarize_dyn_ownership_runtime_counts(
    const DynOwnershipRuntimeFacts& facts) noexcept;
[[nodiscard]] StableFingerprint128 dyn_ownership_runtime_facts_fingerprint(
    const DynOwnershipRuntimeFacts& facts) noexcept;
[[nodiscard]] std::string summarize_dyn_ownership_runtime_facts(
    const DynOwnershipRuntimeFacts& facts);
[[nodiscard]] std::string dump_dyn_ownership_runtime_facts(
    const DynOwnershipRuntimeFacts& facts);
[[nodiscard]] DynOwnershipRuntimeFacts m17_dyn_ownership_runtime_preparation_baseline();

} // namespace aurex::query
