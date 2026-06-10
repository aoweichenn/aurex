#pragma once

#include <aurex/infrastructure/query/owned_dyn_runtime_admission_gate.hpp>
#include <aurex/infrastructure/query/trait_object_key.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

inline constexpr base::u32 QUERY_OWNED_DYN_IR_SHAPE_HANDLE_FIELD_COUNT = 2U;

enum class OwnedDynIrShapePrototypeFactKind : base::u8 {
    owned_handle_metadata = 1,
    erased_payload_pointer,
    vtable_pointer_metadata,
    drop_identity_placeholder,
    allocator_identity_placeholder,
    runtime_lowering_blocker,
};

enum class OwnedDynIrShapePrototypeStage : base::u8 {
    ir_shape_prototype = 1,
    verifier_shape_guard,
    blocked_future_runtime,
};

enum class OwnedDynIrShapePrototypePolicy : base::u8 {
    owned_handle_two_field_v1 = 1,
    erased_payload_pointer_v1,
    borrowed_vtable_pointer_unchanged_v1,
    drop_identity_not_lowered_v1,
    allocator_identity_not_lowered_v1,
    runtime_lowering_not_implemented_v1,
};

struct OwnedDynIrShapePrototypeFact {
    std::string fact_name;
    OwnedDynIrShapePrototypeFactKind kind =
        OwnedDynIrShapePrototypeFactKind::owned_handle_metadata;
    OwnedDynIrShapePrototypeStage stage =
        OwnedDynIrShapePrototypeStage::ir_shape_prototype;
    OwnedDynIrShapePrototypePolicy policy =
        OwnedDynIrShapePrototypePolicy::owned_handle_two_field_v1;
    bool references_m20a_admission_gate = true;
    bool compiler_owned_ir_shape = true;
    bool owned_layout_prototype_visible = true;
    bool handle_metadata_visible = false;
    bool erased_payload_pointer_visible = false;
    bool vtable_pointer_visible = false;
    bool drop_identity_placeholder_visible = false;
    bool allocator_identity_placeholder_visible = false;
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
    base::u64 layout_prototype_count = 1;
    base::u32 handle_field_count = QUERY_OWNED_DYN_IR_SHAPE_HANDLE_FIELD_COUNT;
    std::string subject_symbol;
    std::string m20a_gate_fact;
    std::string verifier_guard_fact;
    std::string blocked_surface_fact;
};

struct OwnedDynIrShapePrototypeSummary {
    base::u64 fact_count = 0;
    base::u64 owned_handle_metadata_count = 0;
    base::u64 erased_payload_pointer_count = 0;
    base::u64 vtable_pointer_metadata_count = 0;
    base::u64 drop_identity_placeholder_count = 0;
    base::u64 allocator_identity_placeholder_count = 0;
    base::u64 runtime_lowering_blocker_count = 0;
    base::u64 m20a_reference_count = 0;
    base::u64 compiler_owned_ir_shape_count = 0;
    base::u64 owned_layout_prototype_visible_count = 0;
    base::u64 handle_metadata_visible_count = 0;
    base::u64 erased_payload_pointer_visible_count = 0;
    base::u64 vtable_pointer_visible_count = 0;
    base::u64 drop_identity_placeholder_visible_count = 0;
    base::u64 allocator_identity_placeholder_visible_count = 0;
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

struct OwnedDynIrShapePrototypeGate {
    std::string subject;
    OwnedDynRuntimeAdmissionGate admission_gate;
    StableFingerprint128 admission_gate_fingerprint;
    std::vector<OwnedDynIrShapePrototypeFact> facts;
    OwnedDynIrShapePrototypeSummary summary;
    StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view owned_dyn_ir_shape_prototype_fact_kind_name(
    OwnedDynIrShapePrototypeFactKind kind) noexcept;
[[nodiscard]] std::string_view owned_dyn_ir_shape_prototype_stage_name(
    OwnedDynIrShapePrototypeStage stage) noexcept;
[[nodiscard]] std::string_view owned_dyn_ir_shape_prototype_policy_name(
    OwnedDynIrShapePrototypePolicy policy) noexcept;

[[nodiscard]] bool is_valid(OwnedDynIrShapePrototypeFactKind kind) noexcept;
[[nodiscard]] bool is_valid(OwnedDynIrShapePrototypeStage stage) noexcept;
[[nodiscard]] bool is_valid(OwnedDynIrShapePrototypePolicy policy) noexcept;
[[nodiscard]] bool is_valid(const OwnedDynIrShapePrototypeFact& fact) noexcept;
[[nodiscard]] bool is_valid(
    const OwnedDynIrShapePrototypeSummary& summary,
    const OwnedDynIrShapePrototypeGate& gate) noexcept;
[[nodiscard]] bool is_valid(const OwnedDynIrShapePrototypeGate& gate) noexcept;
[[nodiscard]] bool is_valid_m20b_owned_dyn_ir_shape_prototype_gate(
    const OwnedDynIrShapePrototypeGate& gate) noexcept;

void record_owned_dyn_ir_shape_prototype_fact(
    OwnedDynIrShapePrototypeGate& gate,
    OwnedDynIrShapePrototypeFact fact);

[[nodiscard]] OwnedDynIrShapePrototypeSummary summarize_owned_dyn_ir_shape_prototype_gate_counts(
    const OwnedDynIrShapePrototypeGate& gate) noexcept;
[[nodiscard]] StableFingerprint128 owned_dyn_ir_shape_prototype_gate_fingerprint(
    const OwnedDynIrShapePrototypeGate& gate) noexcept;
[[nodiscard]] std::string summarize_owned_dyn_ir_shape_prototype_gate(
    const OwnedDynIrShapePrototypeGate& gate);
[[nodiscard]] std::string dump_owned_dyn_ir_shape_prototype_gate(
    const OwnedDynIrShapePrototypeGate& gate);
[[nodiscard]] OwnedDynIrShapePrototypeGate m20b_owned_dyn_ir_shape_prototype_gate_baseline();

} // namespace aurex::query
