#include <aurex/infrastructure/query/dyn_ownership_runtime_boundary_gate.hpp>
#include <aurex/infrastructure/query/project_graph_query.hpp>

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_GATE_FINGERPRINT_MARKER =
    "query.dyn_ownership_runtime_boundary_gate.v1";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_GATE_M18_SUBJECT =
    "M18 Dyn Ownership Runtime Boundary Hardening";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_QUERY_CACHE_FACT =
    "dyn_ownership_runtime_query_cache_projection_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_TOOLING_FACT =
    "dyn_ownership_runtime_tooling_projection_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_REUSE_FACT =
    "dyn_ownership_runtime_reuse_boundary_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_FACT =
    "dyn_ownership_runtime_ir_verifier_planning_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_BORROWED_ABI_GUARD_FACT =
    "dyn_ownership_runtime_borrowed_abi_guard_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_LOWERING_GATE_FACT =
    "dyn_ownership_runtime_lowering_design_gate_fact";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_REQUIRED_M17_FACT =
    "requires_m17_dyn_ownership_runtime_facts";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_STDLIB_BLOCKER_FACT =
    "standard_library_runtime_not_in_m18";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_RUNTIME_BLOCKER_FACT =
    "runtime_lowering_not_in_m18";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_BOX_BLOCKER_FACT =
    "box_dyn_trait_not_in_m18";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_OWNING_VALUE_BLOCKER_FACT =
    "owning_dyn_user_value_not_in_m18";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_ALLOCATOR_BLOCKER_FACT =
    "allocator_api_not_in_m18";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_DYNAMIC_DROP_BLOCKER_FACT =
    "dynamic_drop_runtime_not_in_m18";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_IR_OWNED_OBJECT_FACT =
    "future_ir_owned_dyn_object_placeholder_required";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_IR_DROP_IDENTITY_FACT =
    "future_ir_erased_drop_identity_required";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_IR_ALLOCATOR_FACT =
    "future_ir_allocator_identity_required";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_VERIFIER_GUARD_FACT =
    "future_verifier_rejects_borrowed_vtable_destructor";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_LOWERING_BLOCKER_FACT =
    "future_lowering_blocked_until_stdlib_runtime_stage";
constexpr base::u8 QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_INVALID_ENUM_VALUE = 255U;
constexpr base::u64 QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_M18_CHECKPOINT_COUNT = 6U;
constexpr base::u64 QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_M18_DESIGN_GATE_COUNT = 1U;

[[nodiscard]] base::u8 stable_checkpoint_kind_value(
    const DynOwnershipRuntimeBoundaryCheckpointKind kind) noexcept
{
    return is_valid(kind) ? static_cast<base::u8>(kind)
                          : QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_checkpoint_stage_value(
    const DynOwnershipRuntimeBoundaryCheckpointStage stage) noexcept
{
    return is_valid(stage) ? static_cast<base::u8>(stage)
                           : QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_checkpoint_policy_value(
    const DynOwnershipRuntimeBoundaryCheckpointPolicy policy) noexcept
{
    return is_valid(policy) ? static_cast<base::u8>(policy)
                            : QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_INVALID_ENUM_VALUE;
}

[[nodiscard]] bool nonempty(const std::string_view value) noexcept
{
    return !value.empty();
}

[[nodiscard]] bool checkpoint_policy_matches_kind(
    const DynOwnershipRuntimeBoundaryCheckpointFact& fact) noexcept
{
    switch (fact.kind) {
        case DynOwnershipRuntimeBoundaryCheckpointKind::query_cache_projection:
            return fact.stage == DynOwnershipRuntimeBoundaryCheckpointStage::hardened_query_boundary
                && fact.policy
                    == DynOwnershipRuntimeBoundaryCheckpointPolicy::stable_fingerprint_projection_v1
                && fact.query_cache_boundary_recorded;
        case DynOwnershipRuntimeBoundaryCheckpointKind::tooling_projection:
            return fact.stage == DynOwnershipRuntimeBoundaryCheckpointStage::tooling_boundary
                && fact.policy == DynOwnershipRuntimeBoundaryCheckpointPolicy::semantic_fact_projection_v1
                && fact.tooling_projection_recorded;
        case DynOwnershipRuntimeBoundaryCheckpointKind::reuse_boundary:
            return fact.stage == DynOwnershipRuntimeBoundaryCheckpointStage::hardened_query_boundary
                && fact.policy == DynOwnershipRuntimeBoundaryCheckpointPolicy::body_local_reuse_boundary_v1
                && fact.reuse_boundary_recorded;
        case DynOwnershipRuntimeBoundaryCheckpointKind::ir_verifier_planning:
            return fact.stage == DynOwnershipRuntimeBoundaryCheckpointStage::design_gate
                && fact.policy == DynOwnershipRuntimeBoundaryCheckpointPolicy::ir_verifier_prerequisite_v1
                && fact.ir_verifier_prerequisite_recorded;
        case DynOwnershipRuntimeBoundaryCheckpointKind::borrowed_abi_guard:
            return fact.stage == DynOwnershipRuntimeBoundaryCheckpointStage::hardened_query_boundary
                && fact.policy
                    == DynOwnershipRuntimeBoundaryCheckpointPolicy::borrowed_vtable_destructor_free_v1
                && fact.borrowed_metadata_destructor_free;
        case DynOwnershipRuntimeBoundaryCheckpointKind::runtime_lowering_gate:
            return fact.stage == DynOwnershipRuntimeBoundaryCheckpointStage::blocked_future_runtime
                && fact.policy
                    == DynOwnershipRuntimeBoundaryCheckpointPolicy::runtime_lowering_not_implemented_v1
                && fact.runtime_lowering_blocked;
    }
    return false;
}

[[nodiscard]] bool checkpoint_keeps_m18_blockers(
    const DynOwnershipRuntimeBoundaryCheckpointFact& fact) noexcept
{
    return fact.references_m17_facts
        && fact.standard_library_blocked
        && fact.runtime_lowering_blocked
        && fact.box_surface_blocked
        && fact.owning_dyn_user_value_blocked
        && fact.allocator_api_blocked
        && fact.dynamic_drop_dispatch_blocked
        && fact.borrowed_metadata_destructor_free;
}

[[nodiscard]] bool checkpoint_has_named_payload(
    const DynOwnershipRuntimeBoundaryCheckpointFact& fact) noexcept
{
    return nonempty(fact.fact_name)
        && nonempty(fact.boundary_fact)
        && nonempty(fact.required_input_fact)
        && nonempty(fact.blocked_surface_fact)
        && nonempty(fact.next_stage_fact);
}

[[nodiscard]] bool lowering_gate_keeps_m18_blockers(
    const DynOwnershipRuntimeLoweringDesignGateFact& fact) noexcept
{
    return fact.ir_owned_object_placeholder_required
        && fact.ir_erased_drop_identity_required
        && fact.ir_allocator_identity_required
        && fact.verifier_rejects_borrowed_vtable_destructor
        && fact.verifier_rejects_missing_erased_receiver
        && fact.verifier_rejects_runtime_lowering_without_stdlib
        && !fact.lowering_runtime_implemented
        && !fact.dynamic_drop_runtime_implemented
        && !fact.standard_library_implemented;
}

[[nodiscard]] bool lowering_gate_has_named_payload(
    const DynOwnershipRuntimeLoweringDesignGateFact& fact) noexcept
{
    return nonempty(fact.fact_name)
        && nonempty(fact.ir_owned_object_fact)
        && nonempty(fact.ir_erased_drop_fact)
        && nonempty(fact.ir_allocator_fact)
        && nonempty(fact.verifier_guard_fact)
        && nonempty(fact.lowering_blocker_fact);
}

[[nodiscard]] bool summary_equals(
    const DynOwnershipRuntimeBoundarySummary& lhs,
    const DynOwnershipRuntimeBoundarySummary& rhs) noexcept
{
    return lhs.checkpoint_count == rhs.checkpoint_count
        && lhs.query_cache_checkpoint_count == rhs.query_cache_checkpoint_count
        && lhs.tooling_checkpoint_count == rhs.tooling_checkpoint_count
        && lhs.reuse_checkpoint_count == rhs.reuse_checkpoint_count
        && lhs.ir_verifier_checkpoint_count == rhs.ir_verifier_checkpoint_count
        && lhs.borrowed_abi_guard_count == rhs.borrowed_abi_guard_count
        && lhs.runtime_lowering_gate_count == rhs.runtime_lowering_gate_count
        && lhs.m17_reference_count == rhs.m17_reference_count
        && lhs.standard_library_blocked_count == rhs.standard_library_blocked_count
        && lhs.runtime_lowering_blocked_count == rhs.runtime_lowering_blocked_count
        && lhs.box_surface_blocked_count == rhs.box_surface_blocked_count
        && lhs.owning_dyn_user_value_blocked_count == rhs.owning_dyn_user_value_blocked_count
        && lhs.allocator_api_blocked_count == rhs.allocator_api_blocked_count
        && lhs.dynamic_drop_dispatch_blocked_count == rhs.dynamic_drop_dispatch_blocked_count
        && lhs.borrowed_metadata_destructor_free_count == rhs.borrowed_metadata_destructor_free_count
        && lhs.lowering_design_gate_count == rhs.lowering_design_gate_count
        && lhs.lowering_runtime_implemented_count == rhs.lowering_runtime_implemented_count;
}

void mix_boundary_summary(StableHashBuilder& builder, const DynOwnershipRuntimeBoundarySummary& summary) noexcept
{
    builder.mix_u64(summary.checkpoint_count);
    builder.mix_u64(summary.query_cache_checkpoint_count);
    builder.mix_u64(summary.tooling_checkpoint_count);
    builder.mix_u64(summary.reuse_checkpoint_count);
    builder.mix_u64(summary.ir_verifier_checkpoint_count);
    builder.mix_u64(summary.borrowed_abi_guard_count);
    builder.mix_u64(summary.runtime_lowering_gate_count);
    builder.mix_u64(summary.m17_reference_count);
    builder.mix_u64(summary.standard_library_blocked_count);
    builder.mix_u64(summary.runtime_lowering_blocked_count);
    builder.mix_u64(summary.box_surface_blocked_count);
    builder.mix_u64(summary.owning_dyn_user_value_blocked_count);
    builder.mix_u64(summary.allocator_api_blocked_count);
    builder.mix_u64(summary.dynamic_drop_dispatch_blocked_count);
    builder.mix_u64(summary.borrowed_metadata_destructor_free_count);
    builder.mix_u64(summary.lowering_design_gate_count);
    builder.mix_u64(summary.lowering_runtime_implemented_count);
}

void mix_checkpoint(
    StableHashBuilder& builder, const DynOwnershipRuntimeBoundaryCheckpointFact& fact) noexcept
{
    builder.mix_string(fact.fact_name);
    builder.mix_u8(stable_checkpoint_kind_value(fact.kind));
    builder.mix_u8(stable_checkpoint_stage_value(fact.stage));
    builder.mix_u8(stable_checkpoint_policy_value(fact.policy));
    builder.mix_bool(fact.references_m17_facts);
    builder.mix_bool(fact.query_cache_boundary_recorded);
    builder.mix_bool(fact.tooling_projection_recorded);
    builder.mix_bool(fact.reuse_boundary_recorded);
    builder.mix_bool(fact.ir_verifier_prerequisite_recorded);
    builder.mix_bool(fact.borrowed_metadata_destructor_free);
    builder.mix_bool(fact.standard_library_blocked);
    builder.mix_bool(fact.runtime_lowering_blocked);
    builder.mix_bool(fact.box_surface_blocked);
    builder.mix_bool(fact.owning_dyn_user_value_blocked);
    builder.mix_bool(fact.allocator_api_blocked);
    builder.mix_bool(fact.dynamic_drop_dispatch_blocked);
    builder.mix_string(fact.boundary_fact);
    builder.mix_string(fact.required_input_fact);
    builder.mix_string(fact.blocked_surface_fact);
    builder.mix_string(fact.next_stage_fact);
}

void mix_lowering_gate(
    StableHashBuilder& builder, const DynOwnershipRuntimeLoweringDesignGateFact& fact) noexcept
{
    builder.mix_string(fact.fact_name);
    builder.mix_bool(fact.ir_owned_object_placeholder_required);
    builder.mix_bool(fact.ir_erased_drop_identity_required);
    builder.mix_bool(fact.ir_allocator_identity_required);
    builder.mix_bool(fact.verifier_rejects_borrowed_vtable_destructor);
    builder.mix_bool(fact.verifier_rejects_missing_erased_receiver);
    builder.mix_bool(fact.verifier_rejects_runtime_lowering_without_stdlib);
    builder.mix_bool(fact.lowering_runtime_implemented);
    builder.mix_bool(fact.dynamic_drop_runtime_implemented);
    builder.mix_bool(fact.standard_library_implemented);
    builder.mix_string(fact.ir_owned_object_fact);
    builder.mix_string(fact.ir_erased_drop_fact);
    builder.mix_string(fact.ir_allocator_fact);
    builder.mix_string(fact.verifier_guard_fact);
    builder.mix_string(fact.lowering_blocker_fact);
}

[[nodiscard]] const char* fallback_name(const std::string& value, const char* fallback) noexcept
{
    return value.empty() ? fallback : value.c_str();
}

[[nodiscard]] DynOwnershipRuntimeBoundaryCheckpointFact checkpoint_fact(
    std::string_view fact_name,
    const DynOwnershipRuntimeBoundaryCheckpointKind kind,
    const DynOwnershipRuntimeBoundaryCheckpointStage stage,
    const DynOwnershipRuntimeBoundaryCheckpointPolicy policy,
    std::string_view boundary_fact,
    std::string_view blocked_surface_fact,
    std::string_view next_stage_fact)
{
    DynOwnershipRuntimeBoundaryCheckpointFact fact;
    fact.fact_name = std::string(fact_name);
    fact.kind = kind;
    fact.stage = stage;
    fact.policy = policy;
    fact.references_m17_facts = true;
    fact.query_cache_boundary_recorded = kind == DynOwnershipRuntimeBoundaryCheckpointKind::query_cache_projection;
    fact.tooling_projection_recorded = kind == DynOwnershipRuntimeBoundaryCheckpointKind::tooling_projection;
    fact.reuse_boundary_recorded = kind == DynOwnershipRuntimeBoundaryCheckpointKind::reuse_boundary;
    fact.ir_verifier_prerequisite_recorded = kind == DynOwnershipRuntimeBoundaryCheckpointKind::ir_verifier_planning;
    fact.borrowed_metadata_destructor_free = true;
    fact.standard_library_blocked = true;
    fact.runtime_lowering_blocked = true;
    fact.box_surface_blocked = true;
    fact.owning_dyn_user_value_blocked = true;
    fact.allocator_api_blocked = true;
    fact.dynamic_drop_dispatch_blocked = true;
    fact.boundary_fact = std::string(boundary_fact);
    fact.required_input_fact = std::string(QUERY_DYN_OWNERSHIP_RUNTIME_REQUIRED_M17_FACT);
    fact.blocked_surface_fact = std::string(blocked_surface_fact);
    fact.next_stage_fact = std::string(next_stage_fact);
    return fact;
}

[[nodiscard]] DynOwnershipRuntimeLoweringDesignGateFact m18_lowering_design_gate_fact()
{
    return DynOwnershipRuntimeLoweringDesignGateFact{
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_LOWERING_GATE_FACT),
        true,
        true,
        true,
        true,
        true,
        true,
        false,
        false,
        false,
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_IR_OWNED_OBJECT_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_IR_DROP_IDENTITY_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_IR_ALLOCATOR_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_VERIFIER_GUARD_FACT),
        std::string(QUERY_DYN_OWNERSHIP_RUNTIME_LOWERING_BLOCKER_FACT),
    };
}

[[nodiscard]] std::vector<DynOwnershipRuntimeBoundaryCheckpointFact> m18_checkpoint_facts()
{
    return {
        checkpoint_fact(QUERY_DYN_OWNERSHIP_RUNTIME_QUERY_CACHE_FACT,
            DynOwnershipRuntimeBoundaryCheckpointKind::query_cache_projection,
            DynOwnershipRuntimeBoundaryCheckpointStage::hardened_query_boundary,
            DynOwnershipRuntimeBoundaryCheckpointPolicy::stable_fingerprint_projection_v1,
            "project_query_recorded_for_dyn_ownership_runtime_boundary_gate",
            QUERY_DYN_OWNERSHIP_RUNTIME_RUNTIME_BLOCKER_FACT,
            "m19_runtime_ir_verifier_preparation"),
        checkpoint_fact(QUERY_DYN_OWNERSHIP_RUNTIME_TOOLING_FACT,
            DynOwnershipRuntimeBoundaryCheckpointKind::tooling_projection,
            DynOwnershipRuntimeBoundaryCheckpointStage::tooling_boundary,
            DynOwnershipRuntimeBoundaryCheckpointPolicy::semantic_fact_projection_v1,
            "ide_semantic_fact_dyn_ownership_runtime_boundary_gate",
            QUERY_DYN_OWNERSHIP_RUNTIME_STDLIB_BLOCKER_FACT,
            "m19_tooling_verifier_projection"),
        checkpoint_fact(QUERY_DYN_OWNERSHIP_RUNTIME_REUSE_FACT,
            DynOwnershipRuntimeBoundaryCheckpointKind::reuse_boundary,
            DynOwnershipRuntimeBoundaryCheckpointStage::hardened_query_boundary,
            DynOwnershipRuntimeBoundaryCheckpointPolicy::body_local_reuse_boundary_v1,
            "workspace_reuse_keeps_runtime_boundary_project_scoped",
            QUERY_DYN_OWNERSHIP_RUNTIME_BOX_BLOCKER_FACT,
            "m19_reuse_runtime_ir_gate"),
        checkpoint_fact(QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_FACT,
            DynOwnershipRuntimeBoundaryCheckpointKind::ir_verifier_planning,
            DynOwnershipRuntimeBoundaryCheckpointStage::design_gate,
            DynOwnershipRuntimeBoundaryCheckpointPolicy::ir_verifier_prerequisite_v1,
            "future_ir_verifier_prerequisites_recorded",
            QUERY_DYN_OWNERSHIP_RUNTIME_OWNING_VALUE_BLOCKER_FACT,
            "m19_ir_verifier_preparation"),
        checkpoint_fact(QUERY_DYN_OWNERSHIP_RUNTIME_BORROWED_ABI_GUARD_FACT,
            DynOwnershipRuntimeBoundaryCheckpointKind::borrowed_abi_guard,
            DynOwnershipRuntimeBoundaryCheckpointStage::hardened_query_boundary,
            DynOwnershipRuntimeBoundaryCheckpointPolicy::borrowed_vtable_destructor_free_v1,
            "borrowed_vtable_remains_destructor_free",
            QUERY_DYN_OWNERSHIP_RUNTIME_DYNAMIC_DROP_BLOCKER_FACT,
            "m19_borrowed_abi_verifier_guard"),
        checkpoint_fact(QUERY_DYN_OWNERSHIP_RUNTIME_LOWERING_GATE_FACT,
            DynOwnershipRuntimeBoundaryCheckpointKind::runtime_lowering_gate,
            DynOwnershipRuntimeBoundaryCheckpointStage::blocked_future_runtime,
            DynOwnershipRuntimeBoundaryCheckpointPolicy::runtime_lowering_not_implemented_v1,
            "runtime_lowering_design_gate_remains_blocked",
            QUERY_DYN_OWNERSHIP_RUNTIME_ALLOCATOR_BLOCKER_FACT,
            "future_standard_library_runtime_stage"),
    };
}

[[nodiscard]] bool m18_checkpoint_kinds_are_complete(const DynOwnershipRuntimeBoundaryGate& gate) noexcept
{
    std::array<bool, QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_M18_CHECKPOINT_COUNT> seen{};
    for (const DynOwnershipRuntimeBoundaryCheckpointFact& fact : gate.checkpoints) {
        if (!is_valid(fact.kind)) {
            return false;
        }
        const auto index = static_cast<base::usize>(fact.kind) - 1U;
        if (index >= seen.size() || seen[index]) {
            return false;
        }
        seen[index] = true;
    }
    return std::all_of(seen.begin(), seen.end(), [](const bool present) {
        return present;
    });
}

[[nodiscard]] bool boundary_gate_shape_is_valid(const DynOwnershipRuntimeBoundaryGate& gate) noexcept
{
    return !gate.subject.empty()
        && is_valid_m17_dyn_ownership_runtime_preparation_baseline(gate.runtime_facts)
        && gate.runtime_facts_fingerprint == gate.runtime_facts.fingerprint
        && gate.runtime_facts_fingerprint == dyn_ownership_runtime_facts_fingerprint(gate.runtime_facts)
        && !gate.checkpoints.empty()
        && !gate.lowering_design_gates.empty();
}

} // namespace

std::string_view dyn_ownership_runtime_boundary_checkpoint_kind_name(
    const DynOwnershipRuntimeBoundaryCheckpointKind kind) noexcept
{
    switch (kind) {
        case DynOwnershipRuntimeBoundaryCheckpointKind::query_cache_projection:
            return "query_cache_projection";
        case DynOwnershipRuntimeBoundaryCheckpointKind::tooling_projection:
            return "tooling_projection";
        case DynOwnershipRuntimeBoundaryCheckpointKind::reuse_boundary:
            return "reuse_boundary";
        case DynOwnershipRuntimeBoundaryCheckpointKind::ir_verifier_planning:
            return "ir_verifier_planning";
        case DynOwnershipRuntimeBoundaryCheckpointKind::borrowed_abi_guard:
            return "borrowed_abi_guard";
        case DynOwnershipRuntimeBoundaryCheckpointKind::runtime_lowering_gate:
            return "runtime_lowering_gate";
    }
    return "invalid";
}

std::string_view dyn_ownership_runtime_boundary_checkpoint_stage_name(
    const DynOwnershipRuntimeBoundaryCheckpointStage stage) noexcept
{
    switch (stage) {
        case DynOwnershipRuntimeBoundaryCheckpointStage::hardened_query_boundary:
            return "hardened_query_boundary";
        case DynOwnershipRuntimeBoundaryCheckpointStage::tooling_boundary:
            return "tooling_boundary";
        case DynOwnershipRuntimeBoundaryCheckpointStage::design_gate:
            return "design_gate";
        case DynOwnershipRuntimeBoundaryCheckpointStage::blocked_future_runtime:
            return "blocked_future_runtime";
    }
    return "invalid";
}

std::string_view dyn_ownership_runtime_boundary_checkpoint_policy_name(
    const DynOwnershipRuntimeBoundaryCheckpointPolicy policy) noexcept
{
    switch (policy) {
        case DynOwnershipRuntimeBoundaryCheckpointPolicy::stable_fingerprint_projection_v1:
            return "stable_fingerprint_projection_v1";
        case DynOwnershipRuntimeBoundaryCheckpointPolicy::semantic_fact_projection_v1:
            return "semantic_fact_projection_v1";
        case DynOwnershipRuntimeBoundaryCheckpointPolicy::body_local_reuse_boundary_v1:
            return "body_local_reuse_boundary_v1";
        case DynOwnershipRuntimeBoundaryCheckpointPolicy::ir_verifier_prerequisite_v1:
            return "ir_verifier_prerequisite_v1";
        case DynOwnershipRuntimeBoundaryCheckpointPolicy::borrowed_vtable_destructor_free_v1:
            return "borrowed_vtable_destructor_free_v1";
        case DynOwnershipRuntimeBoundaryCheckpointPolicy::runtime_lowering_not_implemented_v1:
            return "runtime_lowering_not_implemented_v1";
    }
    return "invalid";
}

bool is_valid(const DynOwnershipRuntimeBoundaryCheckpointKind kind) noexcept
{
    return kind == DynOwnershipRuntimeBoundaryCheckpointKind::query_cache_projection
        || kind == DynOwnershipRuntimeBoundaryCheckpointKind::tooling_projection
        || kind == DynOwnershipRuntimeBoundaryCheckpointKind::reuse_boundary
        || kind == DynOwnershipRuntimeBoundaryCheckpointKind::ir_verifier_planning
        || kind == DynOwnershipRuntimeBoundaryCheckpointKind::borrowed_abi_guard
        || kind == DynOwnershipRuntimeBoundaryCheckpointKind::runtime_lowering_gate;
}

bool is_valid(const DynOwnershipRuntimeBoundaryCheckpointStage stage) noexcept
{
    return stage == DynOwnershipRuntimeBoundaryCheckpointStage::hardened_query_boundary
        || stage == DynOwnershipRuntimeBoundaryCheckpointStage::tooling_boundary
        || stage == DynOwnershipRuntimeBoundaryCheckpointStage::design_gate
        || stage == DynOwnershipRuntimeBoundaryCheckpointStage::blocked_future_runtime;
}

bool is_valid(const DynOwnershipRuntimeBoundaryCheckpointPolicy policy) noexcept
{
    return policy == DynOwnershipRuntimeBoundaryCheckpointPolicy::stable_fingerprint_projection_v1
        || policy == DynOwnershipRuntimeBoundaryCheckpointPolicy::semantic_fact_projection_v1
        || policy == DynOwnershipRuntimeBoundaryCheckpointPolicy::body_local_reuse_boundary_v1
        || policy == DynOwnershipRuntimeBoundaryCheckpointPolicy::ir_verifier_prerequisite_v1
        || policy == DynOwnershipRuntimeBoundaryCheckpointPolicy::borrowed_vtable_destructor_free_v1
        || policy == DynOwnershipRuntimeBoundaryCheckpointPolicy::runtime_lowering_not_implemented_v1;
}

bool is_valid(const DynOwnershipRuntimeBoundaryCheckpointFact& fact) noexcept
{
    return is_valid(fact.kind)
        && is_valid(fact.stage)
        && is_valid(fact.policy)
        && checkpoint_policy_matches_kind(fact)
        && checkpoint_keeps_m18_blockers(fact)
        && checkpoint_has_named_payload(fact);
}

bool is_valid(const DynOwnershipRuntimeLoweringDesignGateFact& fact) noexcept
{
    return lowering_gate_keeps_m18_blockers(fact) && lowering_gate_has_named_payload(fact);
}

bool is_valid(
    const DynOwnershipRuntimeBoundarySummary& summary,
    const DynOwnershipRuntimeBoundaryGate& gate) noexcept
{
    return summary_equals(summary, summarize_dyn_ownership_runtime_boundary_gate_counts(gate));
}

bool is_valid(const DynOwnershipRuntimeBoundaryGate& gate) noexcept
{
    return std::all_of(gate.checkpoints.begin(), gate.checkpoints.end(),
               [](const DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
                   return is_valid(fact);
               })
        && std::all_of(gate.lowering_design_gates.begin(), gate.lowering_design_gates.end(),
            [](const DynOwnershipRuntimeLoweringDesignGateFact& fact) {
                return is_valid(fact);
            })
        && boundary_gate_shape_is_valid(gate)
        && is_valid(gate.summary, gate)
        && (gate.fingerprint == StableFingerprint128{}
            || gate.fingerprint == dyn_ownership_runtime_boundary_gate_fingerprint(gate));
}

bool is_valid_m18_dyn_ownership_runtime_boundary_gate(
    const DynOwnershipRuntimeBoundaryGate& gate) noexcept
{
    return is_valid(gate)
        && std::string_view(gate.subject) == QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_GATE_M18_SUBJECT
        && gate.summary.checkpoint_count == QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_M18_CHECKPOINT_COUNT
        && gate.summary.query_cache_checkpoint_count == 1U
        && gate.summary.tooling_checkpoint_count == 1U
        && gate.summary.reuse_checkpoint_count == 1U
        && gate.summary.ir_verifier_checkpoint_count == 1U
        && gate.summary.borrowed_abi_guard_count == 1U
        && gate.summary.runtime_lowering_gate_count == 1U
        && gate.summary.lowering_design_gate_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_M18_DESIGN_GATE_COUNT
        && gate.summary.lowering_runtime_implemented_count == 0U
        && m18_checkpoint_kinds_are_complete(gate);
}

bool is_valid(const DynOwnershipRuntimeBoundaryGateProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.gate);
}

bool is_valid(const DynOwnershipRuntimeBoundaryGateProviderOutput& output) noexcept
{
    if (!is_valid(output.record) || !is_valid(output.result)
        || output.record.key.kind != QueryKind::dyn_ownership_runtime_boundary_gate
        || output.record.result != output.result || !is_valid(output.gate)) {
        return false;
    }
    if (output.dependencies.size() != 1U || output.dependencies.front().kind != QueryKind::project_graph
        || !is_valid(output.dependencies.front())) {
        return false;
    }
    return output.dependencies.front().payload == output.record.key.payload;
}

void record_dyn_ownership_runtime_boundary_checkpoint(
    DynOwnershipRuntimeBoundaryGate& gate, DynOwnershipRuntimeBoundaryCheckpointFact fact)
{
    gate.checkpoints.push_back(std::move(fact));
    gate.summary = summarize_dyn_ownership_runtime_boundary_gate_counts(gate);
}

void record_dyn_ownership_runtime_lowering_design_gate(
    DynOwnershipRuntimeBoundaryGate& gate, DynOwnershipRuntimeLoweringDesignGateFact fact)
{
    gate.lowering_design_gates.push_back(std::move(fact));
    gate.summary = summarize_dyn_ownership_runtime_boundary_gate_counts(gate);
}

DynOwnershipRuntimeBoundarySummary summarize_dyn_ownership_runtime_boundary_gate_counts(
    const DynOwnershipRuntimeBoundaryGate& gate) noexcept
{
    DynOwnershipRuntimeBoundarySummary summary;
    summary.checkpoint_count = static_cast<base::u64>(gate.checkpoints.size());
    summary.lowering_design_gate_count = static_cast<base::u64>(gate.lowering_design_gates.size());

    for (const DynOwnershipRuntimeBoundaryCheckpointFact& fact : gate.checkpoints) {
        switch (fact.kind) {
            case DynOwnershipRuntimeBoundaryCheckpointKind::query_cache_projection:
                ++summary.query_cache_checkpoint_count;
                break;
            case DynOwnershipRuntimeBoundaryCheckpointKind::tooling_projection:
                ++summary.tooling_checkpoint_count;
                break;
            case DynOwnershipRuntimeBoundaryCheckpointKind::reuse_boundary:
                ++summary.reuse_checkpoint_count;
                break;
            case DynOwnershipRuntimeBoundaryCheckpointKind::ir_verifier_planning:
                ++summary.ir_verifier_checkpoint_count;
                break;
            case DynOwnershipRuntimeBoundaryCheckpointKind::borrowed_abi_guard:
                ++summary.borrowed_abi_guard_count;
                break;
            case DynOwnershipRuntimeBoundaryCheckpointKind::runtime_lowering_gate:
                ++summary.runtime_lowering_gate_count;
                break;
        }
        if (fact.references_m17_facts) {
            ++summary.m17_reference_count;
        }
        if (fact.standard_library_blocked) {
            ++summary.standard_library_blocked_count;
        }
        if (fact.runtime_lowering_blocked) {
            ++summary.runtime_lowering_blocked_count;
        }
        if (fact.box_surface_blocked) {
            ++summary.box_surface_blocked_count;
        }
        if (fact.owning_dyn_user_value_blocked) {
            ++summary.owning_dyn_user_value_blocked_count;
        }
        if (fact.allocator_api_blocked) {
            ++summary.allocator_api_blocked_count;
        }
        if (fact.dynamic_drop_dispatch_blocked) {
            ++summary.dynamic_drop_dispatch_blocked_count;
        }
        if (fact.borrowed_metadata_destructor_free) {
            ++summary.borrowed_metadata_destructor_free_count;
        }
    }

    for (const DynOwnershipRuntimeLoweringDesignGateFact& fact : gate.lowering_design_gates) {
        if (fact.lowering_runtime_implemented || fact.dynamic_drop_runtime_implemented
            || fact.standard_library_implemented) {
            ++summary.lowering_runtime_implemented_count;
        }
    }
    return summary;
}

StableFingerprint128 dyn_ownership_runtime_boundary_gate_fingerprint(
    const DynOwnershipRuntimeBoundaryGate& gate) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_GATE_FINGERPRINT_MARKER);
    builder.mix_string(gate.subject);
    builder.mix_fingerprint(gate.runtime_facts_fingerprint);
    builder.mix_fingerprint(dyn_ownership_runtime_facts_fingerprint(gate.runtime_facts));
    mix_boundary_summary(builder, gate.summary);
    builder.mix_u64(static_cast<base::u64>(gate.checkpoints.size()));
    for (const DynOwnershipRuntimeBoundaryCheckpointFact& fact : gate.checkpoints) {
        mix_checkpoint(builder, fact);
    }
    builder.mix_u64(static_cast<base::u64>(gate.lowering_design_gates.size()));
    for (const DynOwnershipRuntimeLoweringDesignGateFact& fact : gate.lowering_design_gates) {
        mix_lowering_gate(builder, fact);
    }
    return builder.finish();
}

QueryResultFingerprint dyn_ownership_runtime_boundary_gate_result_fingerprint(
    const DynOwnershipRuntimeBoundaryGate& gate) noexcept
{
    if (!is_valid(gate)) {
        return {};
    }
    return query_result_fingerprint(dyn_ownership_runtime_boundary_gate_fingerprint(gate));
}

std::string summarize_dyn_ownership_runtime_boundary_gate(
    const DynOwnershipRuntimeBoundaryGate& gate)
{
    std::ostringstream label;
    label << "dyn_ownership_runtime_boundary_gate subject="
          << (gate.subject.empty() ? "<anonymous>" : gate.subject)
          << " checkpoints=" << gate.summary.checkpoint_count
          << " query_cache=" << gate.summary.query_cache_checkpoint_count
          << " tooling=" << gate.summary.tooling_checkpoint_count
          << " reuse=" << gate.summary.reuse_checkpoint_count
          << " ir_verifier=" << gate.summary.ir_verifier_checkpoint_count
          << " borrowed_abi_guard=" << gate.summary.borrowed_abi_guard_count
          << " runtime_lowering_gate=" << gate.summary.runtime_lowering_gate_count
          << " m17_references=" << gate.summary.m17_reference_count
          << " standard_library_blocked=" << gate.summary.standard_library_blocked_count
          << " runtime_lowering_blocked=" << gate.summary.runtime_lowering_blocked_count
          << " box_surface_blocked=" << gate.summary.box_surface_blocked_count
          << " owning_dyn_user_value_blocked=" << gate.summary.owning_dyn_user_value_blocked_count
          << " allocator_api_blocked=" << gate.summary.allocator_api_blocked_count
          << " dynamic_drop_dispatch_blocked=" << gate.summary.dynamic_drop_dispatch_blocked_count
          << " borrowed_metadata_destructor_free="
          << gate.summary.borrowed_metadata_destructor_free_count
          << " lowering_design_gates=" << gate.summary.lowering_design_gate_count
          << " lowering_runtime_implemented=" << gate.summary.lowering_runtime_implemented_count;
    if (!gate.checkpoints.empty()) {
        label << " first_checkpoint="
              << fallback_name(gate.checkpoints.front().fact_name, "<unknown>")
              << " policy="
              << dyn_ownership_runtime_boundary_checkpoint_policy_name(gate.checkpoints.front().policy);
    }
    label << " runtime_facts=" << debug_string(gate.runtime_facts_fingerprint)
          << " fingerprint=" << debug_string(dyn_ownership_runtime_boundary_gate_fingerprint(gate));
    return label.str();
}

std::string dump_dyn_ownership_runtime_boundary_gate(
    const DynOwnershipRuntimeBoundaryGate& gate)
{
    std::ostringstream stream;
    stream << "dyn_ownership_runtime_boundary_gate subject="
           << (gate.subject.empty() ? "<anonymous>" : gate.subject)
           << " checkpoints=" << gate.summary.checkpoint_count
           << " lowering_design_gates=" << gate.summary.lowering_design_gate_count
           << " runtime_facts=" << debug_string(gate.runtime_facts_fingerprint)
           << " fingerprint=" << debug_string(dyn_ownership_runtime_boundary_gate_fingerprint(gate))
           << '\n';
    for (base::usize index = 0; index < gate.checkpoints.size(); ++index) {
        const DynOwnershipRuntimeBoundaryCheckpointFact& fact = gate.checkpoints[index];
        stream << "  checkpoint #" << index
               << " name=" << fallback_name(fact.fact_name, "<anonymous>")
               << " kind=" << dyn_ownership_runtime_boundary_checkpoint_kind_name(fact.kind)
               << " stage=" << dyn_ownership_runtime_boundary_checkpoint_stage_name(fact.stage)
               << " policy=" << dyn_ownership_runtime_boundary_checkpoint_policy_name(fact.policy)
               << " m17=" << (fact.references_m17_facts ? "yes" : "no")
               << " query_cache=" << (fact.query_cache_boundary_recorded ? "yes" : "no")
               << " tooling=" << (fact.tooling_projection_recorded ? "yes" : "no")
               << " reuse=" << (fact.reuse_boundary_recorded ? "yes" : "no")
               << " ir_verifier=" << (fact.ir_verifier_prerequisite_recorded ? "yes" : "no")
               << " borrowed_metadata_destructor_free="
               << (fact.borrowed_metadata_destructor_free ? "yes" : "no")
               << " standard_library_blocked=" << (fact.standard_library_blocked ? "yes" : "no")
               << " runtime_lowering_blocked=" << (fact.runtime_lowering_blocked ? "yes" : "no")
               << " box_surface_blocked=" << (fact.box_surface_blocked ? "yes" : "no")
               << " owning_dyn_user_value_blocked="
               << (fact.owning_dyn_user_value_blocked ? "yes" : "no")
               << " allocator_api_blocked=" << (fact.allocator_api_blocked ? "yes" : "no")
               << " dynamic_drop_dispatch_blocked="
               << (fact.dynamic_drop_dispatch_blocked ? "yes" : "no")
               << " boundary=" << fallback_name(fact.boundary_fact, "<unknown>")
               << " required_input=" << fallback_name(fact.required_input_fact, "<unknown>")
               << " blocked_surface=" << fallback_name(fact.blocked_surface_fact, "<unknown>")
               << " next=" << fallback_name(fact.next_stage_fact, "<unknown>")
               << '\n';
    }
    for (base::usize index = 0; index < gate.lowering_design_gates.size(); ++index) {
        const DynOwnershipRuntimeLoweringDesignGateFact& fact = gate.lowering_design_gates[index];
        stream << "  lowering_design_gate #" << index
               << " name=" << fallback_name(fact.fact_name, "<anonymous>")
               << " owned_object_required="
               << (fact.ir_owned_object_placeholder_required ? "yes" : "no")
               << " erased_drop_identity_required="
               << (fact.ir_erased_drop_identity_required ? "yes" : "no")
               << " allocator_identity_required="
               << (fact.ir_allocator_identity_required ? "yes" : "no")
               << " verifier_rejects_borrowed_vtable_destructor="
               << (fact.verifier_rejects_borrowed_vtable_destructor ? "yes" : "no")
               << " verifier_rejects_missing_erased_receiver="
               << (fact.verifier_rejects_missing_erased_receiver ? "yes" : "no")
               << " verifier_rejects_runtime_lowering_without_stdlib="
               << (fact.verifier_rejects_runtime_lowering_without_stdlib ? "yes" : "no")
               << " lowering_runtime_implemented="
               << (fact.lowering_runtime_implemented ? "yes" : "no")
               << " dynamic_drop_runtime_implemented="
               << (fact.dynamic_drop_runtime_implemented ? "yes" : "no")
               << " standard_library_implemented="
               << (fact.standard_library_implemented ? "yes" : "no")
               << " ir_owned_object=" << fallback_name(fact.ir_owned_object_fact, "<unknown>")
               << " ir_erased_drop=" << fallback_name(fact.ir_erased_drop_fact, "<unknown>")
               << " ir_allocator=" << fallback_name(fact.ir_allocator_fact, "<unknown>")
               << " verifier_guard=" << fallback_name(fact.verifier_guard_fact, "<unknown>")
               << " lowering_blocker=" << fallback_name(fact.lowering_blocker_fact, "<unknown>")
               << '\n';
    }
    return stream.str();
}

DynOwnershipRuntimeBoundaryGate m18_dyn_ownership_runtime_boundary_gate_baseline()
{
    DynOwnershipRuntimeBoundaryGate gate;
    gate.subject = std::string(QUERY_DYN_OWNERSHIP_RUNTIME_BOUNDARY_GATE_M18_SUBJECT);
    gate.runtime_facts = m17_dyn_ownership_runtime_preparation_baseline();
    gate.runtime_facts_fingerprint = gate.runtime_facts.fingerprint;
    for (DynOwnershipRuntimeBoundaryCheckpointFact& fact : m18_checkpoint_facts()) {
        record_dyn_ownership_runtime_boundary_checkpoint(gate, std::move(fact));
    }
    record_dyn_ownership_runtime_lowering_design_gate(gate, m18_lowering_design_gate_fact());
    gate.fingerprint = dyn_ownership_runtime_boundary_gate_fingerprint(gate);
    return gate;
}

std::optional<QueryKey> dyn_ownership_runtime_boundary_gate_query_key(
    const ProjectKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::dyn_ownership_runtime_boundary_gate, stable_key_fingerprint(key));
}

std::optional<DynOwnershipRuntimeBoundaryGateProviderOutput> provide_dyn_ownership_runtime_boundary_gate_query(
    const DynOwnershipRuntimeBoundaryGateProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    const QueryResultFingerprint result = dyn_ownership_runtime_boundary_gate_result_fingerprint(input.gate);
    std::optional<QueryRecord> record = dyn_ownership_runtime_boundary_gate_query_record(input.key, result);
    if (!record) {
        return std::nullopt;
    }
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> project_graph = project_graph_query_key(input.key)) {
        dependencies.push_back(*project_graph);
    }
    return DynOwnershipRuntimeBoundaryGateProviderOutput{
        std::move(*record),
        result,
        std::move(dependencies),
        input.gate,
    };
}

} // namespace aurex::query
