#pragma once

#include <aurex/infrastructure/query/dyn_ownership_runtime_boundary_gate.hpp>
#include <aurex/infrastructure/query/dyn_ownership_runtime_ir_verifier_facts.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

enum class OwnedDynRuntimeAdmissionCapability : base::u8 {
    owned_object_layout = 1,
    erased_drop_identity,
    allocator_identity,
    runtime_lowering_abi,
    box_dyn_surface,
    borrowed_dyn_abi_separation,
};

enum class OwnedDynRuntimeAdmissionStage : base::u8 {
    admission_design_gate = 1,
    ir_shape_prerequisite,
    runtime_identity_prerequisite,
    standard_library_api_prerequisite,
    blocked_future_implementation,
};

enum class OwnedDynRuntimeAdmissionPolicy : base::u8 {
    owned_handle_metadata_v1 = 1,
    erased_drop_identity_v1,
    allocator_identity_v1,
    runtime_lowering_abi_v1,
    box_dyn_surface_v1,
    borrowed_dyn_remains_destructor_free_v1,
};

struct OwnedDynRuntimeAdmissionFact {
    std::string fact_name;
    OwnedDynRuntimeAdmissionCapability capability =
        OwnedDynRuntimeAdmissionCapability::owned_object_layout;
    OwnedDynRuntimeAdmissionStage stage =
        OwnedDynRuntimeAdmissionStage::admission_design_gate;
    OwnedDynRuntimeAdmissionPolicy policy =
        OwnedDynRuntimeAdmissionPolicy::owned_handle_metadata_v1;
    bool references_m17_runtime_facts = true;
    bool references_m18_boundary_gate = true;
    bool references_m19_ir_verifier_facts = true;
    bool standard_library_api_blocked = true;
    bool box_dyn_surface_blocked = true;
    bool owning_dyn_user_value_blocked = true;
    bool allocator_api_blocked = true;
    bool runtime_lowering_blocked = true;
    bool dynamic_drop_runtime_blocked = true;
    bool borrowed_dyn_abi_unchanged = true;
    bool owned_layout_required = false;
    bool erased_drop_identity_required = false;
    bool allocator_identity_required = false;
    bool runtime_abi_required = false;
    bool backend_helper_required = false;
    bool executable_surface_implemented = false;
    std::string admission_fact;
    std::string required_input_fact;
    std::string next_stage_fact;
    std::string blocked_surface_fact;
};

struct OwnedDynRuntimeAdmissionSummary {
    base::u64 fact_count = 0;
    base::u64 owned_object_layout_count = 0;
    base::u64 erased_drop_identity_count = 0;
    base::u64 allocator_identity_count = 0;
    base::u64 runtime_lowering_abi_count = 0;
    base::u64 box_dyn_surface_count = 0;
    base::u64 borrowed_dyn_abi_separation_count = 0;
    base::u64 m17_reference_count = 0;
    base::u64 m18_reference_count = 0;
    base::u64 m19_reference_count = 0;
    base::u64 standard_library_api_blocked_count = 0;
    base::u64 box_dyn_surface_blocked_count = 0;
    base::u64 owning_dyn_user_value_blocked_count = 0;
    base::u64 allocator_api_blocked_count = 0;
    base::u64 runtime_lowering_blocked_count = 0;
    base::u64 dynamic_drop_runtime_blocked_count = 0;
    base::u64 borrowed_dyn_abi_unchanged_count = 0;
    base::u64 owned_layout_required_count = 0;
    base::u64 erased_drop_identity_required_count = 0;
    base::u64 allocator_identity_required_count = 0;
    base::u64 runtime_abi_required_count = 0;
    base::u64 backend_helper_required_count = 0;
    base::u64 executable_surface_implemented_count = 0;
};

struct OwnedDynRuntimeAdmissionGate {
    std::string subject;
    DynOwnershipRuntimeFacts runtime_facts;
    StableFingerprint128 runtime_facts_fingerprint;
    DynOwnershipRuntimeBoundaryGate boundary_gate;
    StableFingerprint128 boundary_gate_fingerprint;
    FunctionDynOwnershipRuntimeIrVerifierFacts ir_verifier_facts;
    StableFingerprint128 ir_verifier_facts_fingerprint;
    std::vector<OwnedDynRuntimeAdmissionFact> admissions;
    OwnedDynRuntimeAdmissionSummary summary;
    StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view owned_dyn_runtime_admission_capability_name(
    OwnedDynRuntimeAdmissionCapability capability) noexcept;
[[nodiscard]] std::string_view owned_dyn_runtime_admission_stage_name(
    OwnedDynRuntimeAdmissionStage stage) noexcept;
[[nodiscard]] std::string_view owned_dyn_runtime_admission_policy_name(
    OwnedDynRuntimeAdmissionPolicy policy) noexcept;

[[nodiscard]] bool is_valid(OwnedDynRuntimeAdmissionCapability capability) noexcept;
[[nodiscard]] bool is_valid(OwnedDynRuntimeAdmissionStage stage) noexcept;
[[nodiscard]] bool is_valid(OwnedDynRuntimeAdmissionPolicy policy) noexcept;
[[nodiscard]] bool is_valid(const OwnedDynRuntimeAdmissionFact& fact) noexcept;
[[nodiscard]] bool is_valid(
    const OwnedDynRuntimeAdmissionSummary& summary,
    const OwnedDynRuntimeAdmissionGate& gate) noexcept;
[[nodiscard]] bool is_valid(const OwnedDynRuntimeAdmissionGate& gate) noexcept;
[[nodiscard]] bool is_valid_m20_owned_dyn_runtime_admission_gate(
    const OwnedDynRuntimeAdmissionGate& gate) noexcept;

void record_owned_dyn_runtime_admission_fact(
    OwnedDynRuntimeAdmissionGate& gate,
    OwnedDynRuntimeAdmissionFact fact);

[[nodiscard]] OwnedDynRuntimeAdmissionSummary summarize_owned_dyn_runtime_admission_gate_counts(
    const OwnedDynRuntimeAdmissionGate& gate) noexcept;
[[nodiscard]] StableFingerprint128 owned_dyn_runtime_admission_gate_fingerprint(
    const OwnedDynRuntimeAdmissionGate& gate) noexcept;
[[nodiscard]] std::string summarize_owned_dyn_runtime_admission_gate(
    const OwnedDynRuntimeAdmissionGate& gate);
[[nodiscard]] std::string dump_owned_dyn_runtime_admission_gate(
    const OwnedDynRuntimeAdmissionGate& gate);
[[nodiscard]] OwnedDynRuntimeAdmissionGate m20_owned_dyn_runtime_admission_gate_baseline();

} // namespace aurex::query
