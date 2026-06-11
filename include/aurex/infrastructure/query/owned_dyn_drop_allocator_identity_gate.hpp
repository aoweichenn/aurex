#pragma once

#include <aurex/infrastructure/query/owned_dyn_ir_shape_prototype_gate.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

enum class OwnedDynDropAllocatorIdentityFactKind : base::u8 {
    erased_drop_identity = 1,
    allocator_identity,
    cleanup_dropck_bridge,
    owned_handle_identity_binding,
    runtime_lowering_blocker,
};

enum class OwnedDynDropAllocatorIdentityStage : base::u8 {
    identity_prerequisite = 1,
    verifier_identity_guard,
    blocked_future_runtime,
};

enum class OwnedDynDropAllocatorIdentityPolicy : base::u8 {
    compiler_owned_erased_drop_identity_v1 = 1,
    compiler_owned_allocator_identity_v1,
    cleanup_dropck_static_bridge_v1,
    owned_handle_identity_binding_v1,
    runtime_lowering_not_implemented_v1,
};

struct OwnedDynDropAllocatorIdentityFact {
    std::string fact_name;
    OwnedDynDropAllocatorIdentityFactKind kind = OwnedDynDropAllocatorIdentityFactKind::erased_drop_identity;
    OwnedDynDropAllocatorIdentityStage stage = OwnedDynDropAllocatorIdentityStage::identity_prerequisite;
    OwnedDynDropAllocatorIdentityPolicy policy =
        OwnedDynDropAllocatorIdentityPolicy::compiler_owned_erased_drop_identity_v1;
    bool references_m20b_ir_shape_gate = true;
    bool compiler_owned_identity = true;
    bool drop_identity_visible = false;
    bool allocator_identity_visible = false;
    bool cleanup_dropck_bridge_visible = false;
    bool owned_handle_binding_visible = false;
    bool borrowed_dyn_abi_unchanged = true;
    bool standard_library_api_blocked = true;
    bool box_dyn_surface_blocked = true;
    bool owning_dyn_user_value_blocked = true;
    bool allocator_api_blocked = true;
    bool runtime_lowering_blocked = true;
    bool dynamic_drop_runtime_blocked = true;
    bool backend_helper_blocked = true;
    bool executable_runtime_implemented = false;
    TraitObjectTypeKey object_type;
    StableFingerprint128 drop_identity_key;
    StableFingerprint128 allocator_identity_key;
    StableFingerprint128 prototype_identity_set_key;
    base::u64 layout_prototype_count = 1;
    std::string subject_symbol;
    std::string m20b_gate_fact;
    std::string verifier_guard_fact;
    std::string blocked_surface_fact;
};

struct OwnedDynDropAllocatorIdentitySummary {
    base::u64 fact_count = 0;
    base::u64 erased_drop_identity_count = 0;
    base::u64 allocator_identity_count = 0;
    base::u64 cleanup_dropck_bridge_count = 0;
    base::u64 owned_handle_identity_binding_count = 0;
    base::u64 runtime_lowering_blocker_count = 0;
    base::u64 m20b_reference_count = 0;
    base::u64 compiler_owned_identity_count = 0;
    base::u64 drop_identity_visible_count = 0;
    base::u64 allocator_identity_visible_count = 0;
    base::u64 cleanup_dropck_bridge_visible_count = 0;
    base::u64 owned_handle_binding_visible_count = 0;
    base::u64 borrowed_dyn_abi_unchanged_count = 0;
    base::u64 standard_library_api_blocked_count = 0;
    base::u64 box_dyn_surface_blocked_count = 0;
    base::u64 owning_dyn_user_value_blocked_count = 0;
    base::u64 allocator_api_blocked_count = 0;
    base::u64 runtime_lowering_blocked_count = 0;
    base::u64 dynamic_drop_runtime_blocked_count = 0;
    base::u64 backend_helper_blocked_count = 0;
    base::u64 executable_runtime_implemented_count = 0;
    base::u64 observed_layout_prototype_total = 0;
};

struct OwnedDynDropAllocatorIdentityGate {
    std::string subject;
    OwnedDynIrShapePrototypeGate ir_shape_gate;
    StableFingerprint128 ir_shape_gate_fingerprint;
    std::vector<OwnedDynDropAllocatorIdentityFact> facts;
    OwnedDynDropAllocatorIdentitySummary summary;
    StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view owned_dyn_drop_allocator_identity_fact_kind_name(
    OwnedDynDropAllocatorIdentityFactKind kind) noexcept;
[[nodiscard]] std::string_view owned_dyn_drop_allocator_identity_stage_name(
    OwnedDynDropAllocatorIdentityStage stage) noexcept;
[[nodiscard]] std::string_view owned_dyn_drop_allocator_identity_policy_name(
    OwnedDynDropAllocatorIdentityPolicy policy) noexcept;

[[nodiscard]] bool is_valid(OwnedDynDropAllocatorIdentityFactKind kind) noexcept;
[[nodiscard]] bool is_valid(OwnedDynDropAllocatorIdentityStage stage) noexcept;
[[nodiscard]] bool is_valid(OwnedDynDropAllocatorIdentityPolicy policy) noexcept;
[[nodiscard]] bool is_valid(const OwnedDynDropAllocatorIdentityFact& fact) noexcept;
[[nodiscard]] bool is_valid(
    const OwnedDynDropAllocatorIdentitySummary& summary, const OwnedDynDropAllocatorIdentityGate& gate) noexcept;
[[nodiscard]] bool is_valid(const OwnedDynDropAllocatorIdentityGate& gate) noexcept;
[[nodiscard]] bool is_valid_m20c_owned_dyn_drop_allocator_identity_gate(
    const OwnedDynDropAllocatorIdentityGate& gate) noexcept;

void record_owned_dyn_drop_allocator_identity_fact(
    OwnedDynDropAllocatorIdentityGate& gate, OwnedDynDropAllocatorIdentityFact fact);

[[nodiscard]] OwnedDynDropAllocatorIdentitySummary summarize_owned_dyn_drop_allocator_identity_gate_counts(
    const OwnedDynDropAllocatorIdentityGate& gate) noexcept;
[[nodiscard]] StableFingerprint128 owned_dyn_drop_allocator_identity_gate_fingerprint(
    const OwnedDynDropAllocatorIdentityGate& gate) noexcept;
[[nodiscard]] std::string summarize_owned_dyn_drop_allocator_identity_gate(
    const OwnedDynDropAllocatorIdentityGate& gate);
[[nodiscard]] std::string dump_owned_dyn_drop_allocator_identity_gate(const OwnedDynDropAllocatorIdentityGate& gate);
[[nodiscard]] OwnedDynDropAllocatorIdentityGate m20c_owned_dyn_drop_allocator_identity_gate_baseline();

} // namespace aurex::query
