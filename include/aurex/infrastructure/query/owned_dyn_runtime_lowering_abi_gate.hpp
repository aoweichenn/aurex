#pragma once

#include <aurex/infrastructure/query/owned_dyn_drop_allocator_identity_gate.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

enum class OwnedDynRuntimeLoweringAbiFactKind : base::u8 {
    runtime_abi_descriptor = 1,
    lowering_transition_guard,
    backend_helper_prerequisite,
    drop_allocator_runtime_bridge,
    dynamic_drop_runtime_blocker,
};

enum class OwnedDynRuntimeLoweringAbiStage : base::u8 {
    abi_design_closure = 1,
    verifier_lowering_guard,
    blocked_future_runtime,
};

enum class OwnedDynRuntimeLoweringAbiPolicy : base::u8 {
    compiler_owned_runtime_abi_descriptor_v1 = 1,
    blocked_to_admitted_transition_check_v1,
    backend_helper_identity_prerequisite_v1,
    drop_allocator_runtime_bridge_v1,
    dynamic_drop_runtime_not_implemented_v1,
};

struct OwnedDynRuntimeLoweringAbiFact {
    std::string fact_name;
    OwnedDynRuntimeLoweringAbiFactKind kind =
        OwnedDynRuntimeLoweringAbiFactKind::runtime_abi_descriptor;
    OwnedDynRuntimeLoweringAbiStage stage =
        OwnedDynRuntimeLoweringAbiStage::abi_design_closure;
    OwnedDynRuntimeLoweringAbiPolicy policy =
        OwnedDynRuntimeLoweringAbiPolicy::compiler_owned_runtime_abi_descriptor_v1;
    bool references_m20c_drop_allocator_identity_gate = true;
    bool compiler_owned_runtime_abi_descriptor = true;
    bool runtime_abi_descriptor_visible = false;
    bool lowering_transition_guard_visible = false;
    bool backend_helper_prerequisite_visible = false;
    bool drop_allocator_runtime_bridge_visible = false;
    bool dynamic_drop_runtime_blocker_visible = false;
    bool borrowed_dyn_abi_unchanged = true;
    bool standard_library_api_blocked = true;
    bool box_dyn_surface_blocked = true;
    bool owning_dyn_user_value_blocked = true;
    bool allocator_api_blocked = true;
    bool runtime_lowering_blocked = true;
    bool dynamic_drop_runtime_blocked = true;
    bool backend_helper_blocked = true;
    bool backend_helper_callable = false;
    bool executable_runtime_implemented = false;
    TraitObjectTypeKey object_type;
    StableFingerprint128 drop_identity_key;
    StableFingerprint128 allocator_identity_key;
    StableFingerprint128 prototype_identity_set_key;
    StableFingerprint128 runtime_abi_descriptor_key;
    StableFingerprint128 backend_helper_identity_key;
    base::u64 layout_prototype_count = 1;
    std::string subject_symbol;
    std::string m20c_gate_fact;
    std::string verifier_guard_fact;
    std::string blocked_surface_fact;
};

struct OwnedDynRuntimeLoweringAbiSummary {
    base::u64 fact_count = 0;
    base::u64 runtime_abi_descriptor_count = 0;
    base::u64 lowering_transition_guard_count = 0;
    base::u64 backend_helper_prerequisite_count = 0;
    base::u64 drop_allocator_runtime_bridge_count = 0;
    base::u64 dynamic_drop_runtime_blocker_count = 0;
    base::u64 m20c_reference_count = 0;
    base::u64 compiler_owned_runtime_abi_descriptor_count = 0;
    base::u64 runtime_abi_descriptor_visible_count = 0;
    base::u64 lowering_transition_guard_visible_count = 0;
    base::u64 backend_helper_prerequisite_visible_count = 0;
    base::u64 drop_allocator_runtime_bridge_visible_count = 0;
    base::u64 dynamic_drop_runtime_blocker_visible_count = 0;
    base::u64 borrowed_dyn_abi_unchanged_count = 0;
    base::u64 standard_library_api_blocked_count = 0;
    base::u64 box_dyn_surface_blocked_count = 0;
    base::u64 owning_dyn_user_value_blocked_count = 0;
    base::u64 allocator_api_blocked_count = 0;
    base::u64 runtime_lowering_blocked_count = 0;
    base::u64 dynamic_drop_runtime_blocked_count = 0;
    base::u64 backend_helper_blocked_count = 0;
    base::u64 backend_helper_callable_count = 0;
    base::u64 executable_runtime_implemented_count = 0;
    base::u64 observed_layout_prototype_total = 0;
};

struct OwnedDynRuntimeLoweringAbiGate {
    std::string subject;
    OwnedDynDropAllocatorIdentityGate drop_allocator_identity_gate;
    StableFingerprint128 drop_allocator_identity_gate_fingerprint;
    std::vector<OwnedDynRuntimeLoweringAbiFact> facts;
    OwnedDynRuntimeLoweringAbiSummary summary;
    StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view owned_dyn_runtime_lowering_abi_fact_kind_name(
    OwnedDynRuntimeLoweringAbiFactKind kind) noexcept;
[[nodiscard]] std::string_view owned_dyn_runtime_lowering_abi_stage_name(
    OwnedDynRuntimeLoweringAbiStage stage) noexcept;
[[nodiscard]] std::string_view owned_dyn_runtime_lowering_abi_policy_name(
    OwnedDynRuntimeLoweringAbiPolicy policy) noexcept;

[[nodiscard]] bool is_valid(OwnedDynRuntimeLoweringAbiFactKind kind) noexcept;
[[nodiscard]] bool is_valid(OwnedDynRuntimeLoweringAbiStage stage) noexcept;
[[nodiscard]] bool is_valid(OwnedDynRuntimeLoweringAbiPolicy policy) noexcept;
[[nodiscard]] bool is_valid(const OwnedDynRuntimeLoweringAbiFact& fact) noexcept;
[[nodiscard]] bool is_valid(
    const OwnedDynRuntimeLoweringAbiSummary& summary,
    const OwnedDynRuntimeLoweringAbiGate& gate) noexcept;
[[nodiscard]] bool is_valid(const OwnedDynRuntimeLoweringAbiGate& gate) noexcept;
[[nodiscard]] bool is_valid_m20d_owned_dyn_runtime_lowering_abi_gate(
    const OwnedDynRuntimeLoweringAbiGate& gate) noexcept;

void record_owned_dyn_runtime_lowering_abi_fact(
    OwnedDynRuntimeLoweringAbiGate& gate,
    OwnedDynRuntimeLoweringAbiFact fact);

[[nodiscard]] OwnedDynRuntimeLoweringAbiSummary summarize_owned_dyn_runtime_lowering_abi_gate_counts(
    const OwnedDynRuntimeLoweringAbiGate& gate) noexcept;
[[nodiscard]] StableFingerprint128 owned_dyn_runtime_lowering_abi_gate_fingerprint(
    const OwnedDynRuntimeLoweringAbiGate& gate) noexcept;
[[nodiscard]] std::string summarize_owned_dyn_runtime_lowering_abi_gate(
    const OwnedDynRuntimeLoweringAbiGate& gate);
[[nodiscard]] std::string dump_owned_dyn_runtime_lowering_abi_gate(
    const OwnedDynRuntimeLoweringAbiGate& gate);
[[nodiscard]] OwnedDynRuntimeLoweringAbiGate m20d_owned_dyn_runtime_lowering_abi_gate_baseline();

} // namespace aurex::query
