#pragma once

#include <aurex/infrastructure/query/dyn_ownership_runtime_facts.hpp>
#include <aurex/infrastructure/query/query_result.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

enum class DynOwnershipRuntimeBoundaryCheckpointKind : base::u8 {
    query_cache_projection = 1,
    tooling_projection,
    reuse_boundary,
    ir_verifier_planning,
    borrowed_abi_guard,
    runtime_lowering_gate,
};

enum class DynOwnershipRuntimeBoundaryCheckpointStage : base::u8 {
    hardened_query_boundary = 1,
    tooling_boundary,
    design_gate,
    blocked_future_runtime,
};

enum class DynOwnershipRuntimeBoundaryCheckpointPolicy : base::u8 {
    stable_fingerprint_projection_v1 = 1,
    semantic_fact_projection_v1,
    body_local_reuse_boundary_v1,
    ir_verifier_prerequisite_v1,
    borrowed_vtable_destructor_free_v1,
    runtime_lowering_not_implemented_v1,
};

struct DynOwnershipRuntimeBoundaryCheckpointFact {
    std::string fact_name;
    DynOwnershipRuntimeBoundaryCheckpointKind kind =
        DynOwnershipRuntimeBoundaryCheckpointKind::query_cache_projection;
    DynOwnershipRuntimeBoundaryCheckpointStage stage =
        DynOwnershipRuntimeBoundaryCheckpointStage::hardened_query_boundary;
    DynOwnershipRuntimeBoundaryCheckpointPolicy policy =
        DynOwnershipRuntimeBoundaryCheckpointPolicy::stable_fingerprint_projection_v1;
    bool references_m17_facts = true;
    bool query_cache_boundary_recorded = false;
    bool tooling_projection_recorded = false;
    bool reuse_boundary_recorded = false;
    bool ir_verifier_prerequisite_recorded = false;
    bool borrowed_metadata_destructor_free = true;
    bool standard_library_blocked = true;
    bool runtime_lowering_blocked = true;
    bool box_surface_blocked = true;
    bool owning_dyn_user_value_blocked = true;
    bool allocator_api_blocked = true;
    bool dynamic_drop_dispatch_blocked = true;
    std::string boundary_fact;
    std::string required_input_fact;
    std::string blocked_surface_fact;
    std::string next_stage_fact;
};

struct DynOwnershipRuntimeLoweringDesignGateFact {
    std::string fact_name;
    bool ir_owned_object_placeholder_required = true;
    bool ir_erased_drop_identity_required = true;
    bool ir_allocator_identity_required = true;
    bool verifier_rejects_borrowed_vtable_destructor = true;
    bool verifier_rejects_missing_erased_receiver = true;
    bool verifier_rejects_runtime_lowering_without_stdlib = true;
    bool lowering_runtime_implemented = false;
    bool dynamic_drop_runtime_implemented = false;
    bool standard_library_implemented = false;
    std::string ir_owned_object_fact;
    std::string ir_erased_drop_fact;
    std::string ir_allocator_fact;
    std::string verifier_guard_fact;
    std::string lowering_blocker_fact;
};

struct DynOwnershipRuntimeBoundarySummary {
    base::u64 checkpoint_count = 0;
    base::u64 query_cache_checkpoint_count = 0;
    base::u64 tooling_checkpoint_count = 0;
    base::u64 reuse_checkpoint_count = 0;
    base::u64 ir_verifier_checkpoint_count = 0;
    base::u64 borrowed_abi_guard_count = 0;
    base::u64 runtime_lowering_gate_count = 0;
    base::u64 m17_reference_count = 0;
    base::u64 standard_library_blocked_count = 0;
    base::u64 runtime_lowering_blocked_count = 0;
    base::u64 box_surface_blocked_count = 0;
    base::u64 owning_dyn_user_value_blocked_count = 0;
    base::u64 allocator_api_blocked_count = 0;
    base::u64 dynamic_drop_dispatch_blocked_count = 0;
    base::u64 borrowed_metadata_destructor_free_count = 0;
    base::u64 lowering_design_gate_count = 0;
    base::u64 lowering_runtime_implemented_count = 0;
};

struct DynOwnershipRuntimeBoundaryGate {
    std::string subject;
    DynOwnershipRuntimeFacts runtime_facts;
    StableFingerprint128 runtime_facts_fingerprint;
    std::vector<DynOwnershipRuntimeBoundaryCheckpointFact> checkpoints;
    std::vector<DynOwnershipRuntimeLoweringDesignGateFact> lowering_design_gates;
    DynOwnershipRuntimeBoundarySummary summary;
    StableFingerprint128 fingerprint;
};

struct DynOwnershipRuntimeBoundaryGateProviderInput {
    ProjectKey key;
    DynOwnershipRuntimeBoundaryGate gate;
};

struct DynOwnershipRuntimeBoundaryGateProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
    DynOwnershipRuntimeBoundaryGate gate;
};

[[nodiscard]] std::string_view dyn_ownership_runtime_boundary_checkpoint_kind_name(
    DynOwnershipRuntimeBoundaryCheckpointKind kind) noexcept;
[[nodiscard]] std::string_view dyn_ownership_runtime_boundary_checkpoint_stage_name(
    DynOwnershipRuntimeBoundaryCheckpointStage stage) noexcept;
[[nodiscard]] std::string_view dyn_ownership_runtime_boundary_checkpoint_policy_name(
    DynOwnershipRuntimeBoundaryCheckpointPolicy policy) noexcept;

[[nodiscard]] bool is_valid(DynOwnershipRuntimeBoundaryCheckpointKind kind) noexcept;
[[nodiscard]] bool is_valid(DynOwnershipRuntimeBoundaryCheckpointStage stage) noexcept;
[[nodiscard]] bool is_valid(DynOwnershipRuntimeBoundaryCheckpointPolicy policy) noexcept;
[[nodiscard]] bool is_valid(const DynOwnershipRuntimeBoundaryCheckpointFact& fact) noexcept;
[[nodiscard]] bool is_valid(const DynOwnershipRuntimeLoweringDesignGateFact& fact) noexcept;
[[nodiscard]] bool is_valid(const DynOwnershipRuntimeBoundarySummary& summary,
    const DynOwnershipRuntimeBoundaryGate& gate) noexcept;
[[nodiscard]] bool is_valid(const DynOwnershipRuntimeBoundaryGate& gate) noexcept;
[[nodiscard]] bool is_valid_m18_dyn_ownership_runtime_boundary_gate(
    const DynOwnershipRuntimeBoundaryGate& gate) noexcept;
[[nodiscard]] bool is_valid(const DynOwnershipRuntimeBoundaryGateProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const DynOwnershipRuntimeBoundaryGateProviderOutput& output) noexcept;

void record_dyn_ownership_runtime_boundary_checkpoint(
    DynOwnershipRuntimeBoundaryGate& gate, DynOwnershipRuntimeBoundaryCheckpointFact fact);
void record_dyn_ownership_runtime_lowering_design_gate(
    DynOwnershipRuntimeBoundaryGate& gate, DynOwnershipRuntimeLoweringDesignGateFact fact);

[[nodiscard]] DynOwnershipRuntimeBoundarySummary summarize_dyn_ownership_runtime_boundary_gate_counts(
    const DynOwnershipRuntimeBoundaryGate& gate) noexcept;
[[nodiscard]] StableFingerprint128 dyn_ownership_runtime_boundary_gate_fingerprint(
    const DynOwnershipRuntimeBoundaryGate& gate) noexcept;
[[nodiscard]] QueryResultFingerprint dyn_ownership_runtime_boundary_gate_result_fingerprint(
    const DynOwnershipRuntimeBoundaryGate& gate) noexcept;
[[nodiscard]] std::string summarize_dyn_ownership_runtime_boundary_gate(
    const DynOwnershipRuntimeBoundaryGate& gate);
[[nodiscard]] std::string dump_dyn_ownership_runtime_boundary_gate(
    const DynOwnershipRuntimeBoundaryGate& gate);
[[nodiscard]] DynOwnershipRuntimeBoundaryGate m18_dyn_ownership_runtime_boundary_gate_baseline();

[[nodiscard]] std::optional<QueryKey> dyn_ownership_runtime_boundary_gate_query_key(
    ProjectKey key) noexcept;
[[nodiscard]] std::optional<DynOwnershipRuntimeBoundaryGateProviderOutput>
provide_dyn_ownership_runtime_boundary_gate_query(
    const DynOwnershipRuntimeBoundaryGateProviderInput& input);

} // namespace aurex::query
