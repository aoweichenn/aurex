#include <aurex/infrastructure/query/owned_dyn_ir_shape_prototype_gate.hpp>

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_FINGERPRINT_MARKER =
    "query.owned_dyn_ir_shape_prototype_gate.v1";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_M20B_SUBJECT =
    "M20b Owned Dyn IR Shape Prototype Gate";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_HANDLE_FACT =
    "owned_dyn_handle_metadata_ir_shape_fact";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_DATA_FACT =
    "owned_dyn_erased_payload_pointer_ir_shape_fact";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_VTABLE_FACT =
    "owned_dyn_vtable_pointer_ir_shape_fact";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_DROP_FACT =
    "owned_dyn_drop_identity_placeholder_ir_shape_fact";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_ALLOCATOR_FACT =
    "owned_dyn_allocator_identity_placeholder_ir_shape_fact";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_RUNTIME_FACT =
    "owned_dyn_runtime_lowering_blocker_ir_shape_fact";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_M20A_GATE =
    "requires_m20a_owned_dyn_runtime_admission_gate";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_VERIFIER_SHAPE =
    "verifier_requires_compiler_owned_two_field_shape";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_BORROWED_ABI =
    "borrowed_dyn_abi_remains_destructor_free";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_NO_BOX =
    "box_dyn_trait_not_in_m20b";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_NO_OWNING_VALUE =
    "owning_dyn_user_value_not_in_m20b";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_NO_ALLOCATOR =
    "allocator_api_not_in_m20b";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_NO_RUNTIME =
    "runtime_abi_lowering_not_in_m20b";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_NO_DYNAMIC_DROP =
    "dynamic_drop_runtime_not_in_m20b";
constexpr std::string_view QUERY_OWNED_DYN_IR_SHAPE_NO_BACKEND =
    "backend_runtime_helper_not_in_m20b";
constexpr base::u8 QUERY_OWNED_DYN_IR_SHAPE_INVALID_ENUM_VALUE = 255U;
constexpr base::u64 QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT = 6U;
constexpr base::u64 QUERY_OWNED_DYN_IR_SHAPE_SINGLE_PROTOTYPE = 1U;

[[nodiscard]] base::u8 stable_kind_value(
    const OwnedDynIrShapePrototypeFactKind kind) noexcept
{
    return is_valid(kind) ? static_cast<base::u8>(kind)
                          : QUERY_OWNED_DYN_IR_SHAPE_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_stage_value(
    const OwnedDynIrShapePrototypeStage stage) noexcept
{
    return is_valid(stage) ? static_cast<base::u8>(stage)
                           : QUERY_OWNED_DYN_IR_SHAPE_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_policy_value(
    const OwnedDynIrShapePrototypePolicy policy) noexcept
{
    return is_valid(policy) ? static_cast<base::u8>(policy)
                            : QUERY_OWNED_DYN_IR_SHAPE_INVALID_ENUM_VALUE;
}

[[nodiscard]] bool nonempty(const std::string_view value) noexcept
{
    return !value.empty();
}

[[nodiscard]] bool fact_payload_is_named(const OwnedDynIrShapePrototypeFact& fact) noexcept
{
    return nonempty(fact.fact_name)
        && nonempty(fact.subject_symbol)
        && nonempty(fact.m20a_gate_fact)
        && nonempty(fact.verifier_guard_fact)
        && nonempty(fact.blocked_surface_fact);
}

[[nodiscard]] bool blockers_are_intact(const OwnedDynIrShapePrototypeFact& fact) noexcept
{
    return fact.references_m20a_admission_gate
        && fact.compiler_owned_ir_shape
        && fact.owned_layout_prototype_visible
        && fact.borrowed_dyn_abi_unchanged
        && fact.standard_library_api_blocked
        && fact.box_dyn_surface_blocked
        && fact.owning_dyn_user_value_blocked
        && fact.allocator_api_blocked
        && fact.runtime_lowering_blocked
        && fact.dynamic_drop_runtime_blocked
        && fact.backend_helper_blocked
        && !fact.executable_runtime_implemented
        && fact.layout_prototype_count > 0U
        && fact.handle_field_count == QUERY_OWNED_DYN_IR_SHAPE_HANDLE_FIELD_COUNT;
}

[[nodiscard]] bool kind_policy_stage_match(const OwnedDynIrShapePrototypeFact& fact) noexcept
{
    switch (fact.kind) {
        case OwnedDynIrShapePrototypeFactKind::owned_handle_metadata:
            return fact.stage == OwnedDynIrShapePrototypeStage::ir_shape_prototype
                && fact.policy == OwnedDynIrShapePrototypePolicy::owned_handle_two_field_v1
                && fact.handle_metadata_visible
                && !fact.erased_payload_pointer_visible
                && !fact.vtable_pointer_visible
                && !fact.drop_identity_placeholder_visible
                && !fact.allocator_identity_placeholder_visible;
        case OwnedDynIrShapePrototypeFactKind::erased_payload_pointer:
            return fact.stage == OwnedDynIrShapePrototypeStage::ir_shape_prototype
                && fact.policy == OwnedDynIrShapePrototypePolicy::erased_payload_pointer_v1
                && fact.erased_payload_pointer_visible
                && !fact.handle_metadata_visible
                && !fact.vtable_pointer_visible
                && !fact.drop_identity_placeholder_visible
                && !fact.allocator_identity_placeholder_visible;
        case OwnedDynIrShapePrototypeFactKind::vtable_pointer_metadata:
            return fact.stage == OwnedDynIrShapePrototypeStage::verifier_shape_guard
                && fact.policy
                    == OwnedDynIrShapePrototypePolicy::borrowed_vtable_pointer_unchanged_v1
                && fact.vtable_pointer_visible
                && !fact.handle_metadata_visible
                && !fact.erased_payload_pointer_visible
                && !fact.drop_identity_placeholder_visible
                && !fact.allocator_identity_placeholder_visible;
        case OwnedDynIrShapePrototypeFactKind::drop_identity_placeholder:
            return fact.stage == OwnedDynIrShapePrototypeStage::blocked_future_runtime
                && fact.policy == OwnedDynIrShapePrototypePolicy::drop_identity_not_lowered_v1
                && fact.drop_identity_placeholder_visible
                && !fact.handle_metadata_visible
                && !fact.erased_payload_pointer_visible
                && !fact.vtable_pointer_visible
                && !fact.allocator_identity_placeholder_visible;
        case OwnedDynIrShapePrototypeFactKind::allocator_identity_placeholder:
            return fact.stage == OwnedDynIrShapePrototypeStage::blocked_future_runtime
                && fact.policy == OwnedDynIrShapePrototypePolicy::allocator_identity_not_lowered_v1
                && fact.allocator_identity_placeholder_visible
                && !fact.handle_metadata_visible
                && !fact.erased_payload_pointer_visible
                && !fact.vtable_pointer_visible
                && !fact.drop_identity_placeholder_visible;
        case OwnedDynIrShapePrototypeFactKind::runtime_lowering_blocker:
            return fact.stage == OwnedDynIrShapePrototypeStage::blocked_future_runtime
                && fact.policy == OwnedDynIrShapePrototypePolicy::runtime_lowering_not_implemented_v1
                && !fact.handle_metadata_visible
                && !fact.erased_payload_pointer_visible
                && !fact.vtable_pointer_visible
                && !fact.drop_identity_placeholder_visible
                && !fact.allocator_identity_placeholder_visible;
    }
    return false;
}

[[nodiscard]] bool summary_equals(
    const OwnedDynIrShapePrototypeSummary& lhs,
    const OwnedDynIrShapePrototypeSummary& rhs) noexcept
{
    return lhs.fact_count == rhs.fact_count
        && lhs.owned_handle_metadata_count == rhs.owned_handle_metadata_count
        && lhs.erased_payload_pointer_count == rhs.erased_payload_pointer_count
        && lhs.vtable_pointer_metadata_count == rhs.vtable_pointer_metadata_count
        && lhs.drop_identity_placeholder_count == rhs.drop_identity_placeholder_count
        && lhs.allocator_identity_placeholder_count == rhs.allocator_identity_placeholder_count
        && lhs.runtime_lowering_blocker_count == rhs.runtime_lowering_blocker_count
        && lhs.m20a_reference_count == rhs.m20a_reference_count
        && lhs.compiler_owned_ir_shape_count == rhs.compiler_owned_ir_shape_count
        && lhs.owned_layout_prototype_visible_count == rhs.owned_layout_prototype_visible_count
        && lhs.handle_metadata_visible_count == rhs.handle_metadata_visible_count
        && lhs.erased_payload_pointer_visible_count == rhs.erased_payload_pointer_visible_count
        && lhs.vtable_pointer_visible_count == rhs.vtable_pointer_visible_count
        && lhs.drop_identity_placeholder_visible_count == rhs.drop_identity_placeholder_visible_count
        && lhs.allocator_identity_placeholder_visible_count
            == rhs.allocator_identity_placeholder_visible_count
        && lhs.borrowed_dyn_abi_unchanged_count == rhs.borrowed_dyn_abi_unchanged_count
        && lhs.standard_library_api_blocked_count == rhs.standard_library_api_blocked_count
        && lhs.box_dyn_surface_blocked_count == rhs.box_dyn_surface_blocked_count
        && lhs.owning_dyn_user_value_blocked_count == rhs.owning_dyn_user_value_blocked_count
        && lhs.allocator_api_blocked_count == rhs.allocator_api_blocked_count
        && lhs.runtime_lowering_blocked_count == rhs.runtime_lowering_blocked_count
        && lhs.dynamic_drop_runtime_blocked_count == rhs.dynamic_drop_runtime_blocked_count
        && lhs.backend_helper_blocked_count == rhs.backend_helper_blocked_count
        && lhs.executable_runtime_implemented_count == rhs.executable_runtime_implemented_count
        && lhs.observed_layout_prototype_total == rhs.observed_layout_prototype_total;
}

void mix_summary(StableHashBuilder& builder, const OwnedDynIrShapePrototypeSummary& summary) noexcept
{
    builder.mix_u64(summary.fact_count);
    builder.mix_u64(summary.owned_handle_metadata_count);
    builder.mix_u64(summary.erased_payload_pointer_count);
    builder.mix_u64(summary.vtable_pointer_metadata_count);
    builder.mix_u64(summary.drop_identity_placeholder_count);
    builder.mix_u64(summary.allocator_identity_placeholder_count);
    builder.mix_u64(summary.runtime_lowering_blocker_count);
    builder.mix_u64(summary.m20a_reference_count);
    builder.mix_u64(summary.compiler_owned_ir_shape_count);
    builder.mix_u64(summary.owned_layout_prototype_visible_count);
    builder.mix_u64(summary.handle_metadata_visible_count);
    builder.mix_u64(summary.erased_payload_pointer_visible_count);
    builder.mix_u64(summary.vtable_pointer_visible_count);
    builder.mix_u64(summary.drop_identity_placeholder_visible_count);
    builder.mix_u64(summary.allocator_identity_placeholder_visible_count);
    builder.mix_u64(summary.borrowed_dyn_abi_unchanged_count);
    builder.mix_u64(summary.standard_library_api_blocked_count);
    builder.mix_u64(summary.box_dyn_surface_blocked_count);
    builder.mix_u64(summary.owning_dyn_user_value_blocked_count);
    builder.mix_u64(summary.allocator_api_blocked_count);
    builder.mix_u64(summary.runtime_lowering_blocked_count);
    builder.mix_u64(summary.dynamic_drop_runtime_blocked_count);
    builder.mix_u64(summary.backend_helper_blocked_count);
    builder.mix_u64(summary.executable_runtime_implemented_count);
    builder.mix_u64(summary.observed_layout_prototype_total);
}

void mix_fact(StableHashBuilder& builder, const OwnedDynIrShapePrototypeFact& fact) noexcept
{
    builder.mix_string(fact.fact_name);
    builder.mix_u8(stable_kind_value(fact.kind));
    builder.mix_u8(stable_stage_value(fact.stage));
    builder.mix_u8(stable_policy_value(fact.policy));
    builder.mix_bool(fact.references_m20a_admission_gate);
    builder.mix_bool(fact.compiler_owned_ir_shape);
    builder.mix_bool(fact.owned_layout_prototype_visible);
    builder.mix_bool(fact.handle_metadata_visible);
    builder.mix_bool(fact.erased_payload_pointer_visible);
    builder.mix_bool(fact.vtable_pointer_visible);
    builder.mix_bool(fact.drop_identity_placeholder_visible);
    builder.mix_bool(fact.allocator_identity_placeholder_visible);
    builder.mix_bool(fact.borrowed_dyn_abi_unchanged);
    builder.mix_bool(fact.standard_library_api_blocked);
    builder.mix_bool(fact.box_dyn_surface_blocked);
    builder.mix_bool(fact.owning_dyn_user_value_blocked);
    builder.mix_bool(fact.allocator_api_blocked);
    builder.mix_bool(fact.runtime_lowering_blocked);
    builder.mix_bool(fact.dynamic_drop_runtime_blocked);
    builder.mix_bool(fact.backend_helper_blocked);
    builder.mix_bool(fact.executable_runtime_implemented);
    builder.mix_u64(fact.object_type.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(fact.object_type));
    builder.mix_u64(fact.layout_prototype_count);
    builder.mix_u32(fact.handle_field_count);
    builder.mix_string(fact.subject_symbol);
    builder.mix_string(fact.m20a_gate_fact);
    builder.mix_string(fact.verifier_guard_fact);
    builder.mix_string(fact.blocked_surface_fact);
}

[[nodiscard]] OwnedDynIrShapePrototypeFact shape_fact(
    const OwnedDynIrShapePrototypeFactKind kind,
    const OwnedDynIrShapePrototypeStage stage,
    const OwnedDynIrShapePrototypePolicy policy,
    const std::string_view fact_name,
    const std::string_view verifier_guard,
    const std::string_view blocked_surface)
{
    OwnedDynIrShapePrototypeFact fact;
    fact.fact_name = std::string(fact_name);
    fact.kind = kind;
    fact.stage = stage;
    fact.policy = policy;
    fact.handle_metadata_visible = kind == OwnedDynIrShapePrototypeFactKind::owned_handle_metadata;
    fact.erased_payload_pointer_visible = kind == OwnedDynIrShapePrototypeFactKind::erased_payload_pointer;
    fact.vtable_pointer_visible = kind == OwnedDynIrShapePrototypeFactKind::vtable_pointer_metadata;
    fact.drop_identity_placeholder_visible =
        kind == OwnedDynIrShapePrototypeFactKind::drop_identity_placeholder;
    fact.allocator_identity_placeholder_visible =
        kind == OwnedDynIrShapePrototypeFactKind::allocator_identity_placeholder;
    fact.subject_symbol = std::string(QUERY_OWNED_DYN_IR_SHAPE_M20B_SUBJECT);
    fact.m20a_gate_fact = std::string(QUERY_OWNED_DYN_IR_SHAPE_M20A_GATE);
    fact.verifier_guard_fact = std::string(verifier_guard);
    fact.blocked_surface_fact = std::string(blocked_surface);
    return fact;
}

[[nodiscard]] bool m20b_fact_kinds_are_complete(
    const OwnedDynIrShapePrototypeGate& gate) noexcept
{
    std::array<bool, QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT> seen{};
    for (const OwnedDynIrShapePrototypeFact& fact : gate.facts) {
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

[[nodiscard]] bool layout_prototype_counts_are_consistent(
    const OwnedDynIrShapePrototypeGate& gate) noexcept
{
    if (gate.facts.empty()) {
        return false;
    }
    const base::u64 expected = gate.facts.front().layout_prototype_count;
    return expected > 0U
        && std::all_of(gate.facts.begin(),
            gate.facts.end(),
            [expected](const OwnedDynIrShapePrototypeFact& fact) {
                return fact.layout_prototype_count == expected;
            });
}

[[nodiscard]] const char* fallback_name(const std::string& value, const char* fallback) noexcept
{
    return value.empty() ? fallback : value.c_str();
}

} // namespace

std::string_view owned_dyn_ir_shape_prototype_fact_kind_name(
    const OwnedDynIrShapePrototypeFactKind kind) noexcept
{
    switch (kind) {
        case OwnedDynIrShapePrototypeFactKind::owned_handle_metadata:
            return "owned_handle_metadata";
        case OwnedDynIrShapePrototypeFactKind::erased_payload_pointer:
            return "erased_payload_pointer";
        case OwnedDynIrShapePrototypeFactKind::vtable_pointer_metadata:
            return "vtable_pointer_metadata";
        case OwnedDynIrShapePrototypeFactKind::drop_identity_placeholder:
            return "drop_identity_placeholder";
        case OwnedDynIrShapePrototypeFactKind::allocator_identity_placeholder:
            return "allocator_identity_placeholder";
        case OwnedDynIrShapePrototypeFactKind::runtime_lowering_blocker:
            return "runtime_lowering_blocker";
    }
    return "invalid";
}

std::string_view owned_dyn_ir_shape_prototype_stage_name(
    const OwnedDynIrShapePrototypeStage stage) noexcept
{
    switch (stage) {
        case OwnedDynIrShapePrototypeStage::ir_shape_prototype:
            return "ir_shape_prototype";
        case OwnedDynIrShapePrototypeStage::verifier_shape_guard:
            return "verifier_shape_guard";
        case OwnedDynIrShapePrototypeStage::blocked_future_runtime:
            return "blocked_future_runtime";
    }
    return "invalid";
}

std::string_view owned_dyn_ir_shape_prototype_policy_name(
    const OwnedDynIrShapePrototypePolicy policy) noexcept
{
    switch (policy) {
        case OwnedDynIrShapePrototypePolicy::owned_handle_two_field_v1:
            return "owned_handle_two_field_v1";
        case OwnedDynIrShapePrototypePolicy::erased_payload_pointer_v1:
            return "erased_payload_pointer_v1";
        case OwnedDynIrShapePrototypePolicy::borrowed_vtable_pointer_unchanged_v1:
            return "borrowed_vtable_pointer_unchanged_v1";
        case OwnedDynIrShapePrototypePolicy::drop_identity_not_lowered_v1:
            return "drop_identity_not_lowered_v1";
        case OwnedDynIrShapePrototypePolicy::allocator_identity_not_lowered_v1:
            return "allocator_identity_not_lowered_v1";
        case OwnedDynIrShapePrototypePolicy::runtime_lowering_not_implemented_v1:
            return "runtime_lowering_not_implemented_v1";
    }
    return "invalid";
}

bool is_valid(const OwnedDynIrShapePrototypeFactKind kind) noexcept
{
    return kind == OwnedDynIrShapePrototypeFactKind::owned_handle_metadata
        || kind == OwnedDynIrShapePrototypeFactKind::erased_payload_pointer
        || kind == OwnedDynIrShapePrototypeFactKind::vtable_pointer_metadata
        || kind == OwnedDynIrShapePrototypeFactKind::drop_identity_placeholder
        || kind == OwnedDynIrShapePrototypeFactKind::allocator_identity_placeholder
        || kind == OwnedDynIrShapePrototypeFactKind::runtime_lowering_blocker;
}

bool is_valid(const OwnedDynIrShapePrototypeStage stage) noexcept
{
    return stage == OwnedDynIrShapePrototypeStage::ir_shape_prototype
        || stage == OwnedDynIrShapePrototypeStage::verifier_shape_guard
        || stage == OwnedDynIrShapePrototypeStage::blocked_future_runtime;
}

bool is_valid(const OwnedDynIrShapePrototypePolicy policy) noexcept
{
    return policy == OwnedDynIrShapePrototypePolicy::owned_handle_two_field_v1
        || policy == OwnedDynIrShapePrototypePolicy::erased_payload_pointer_v1
        || policy == OwnedDynIrShapePrototypePolicy::borrowed_vtable_pointer_unchanged_v1
        || policy == OwnedDynIrShapePrototypePolicy::drop_identity_not_lowered_v1
        || policy == OwnedDynIrShapePrototypePolicy::allocator_identity_not_lowered_v1
        || policy == OwnedDynIrShapePrototypePolicy::runtime_lowering_not_implemented_v1;
}

bool is_valid(const OwnedDynIrShapePrototypeFact& fact) noexcept
{
    return is_valid(fact.kind)
        && is_valid(fact.stage)
        && is_valid(fact.policy)
        && fact_payload_is_named(fact)
        && blockers_are_intact(fact)
        && kind_policy_stage_match(fact);
}

bool is_valid(
    const OwnedDynIrShapePrototypeSummary& summary,
    const OwnedDynIrShapePrototypeGate& gate) noexcept
{
    return summary_equals(summary, summarize_owned_dyn_ir_shape_prototype_gate_counts(gate));
}

bool is_valid(const OwnedDynIrShapePrototypeGate& gate) noexcept
{
    return !gate.subject.empty()
        && is_valid_m20_owned_dyn_runtime_admission_gate(gate.admission_gate)
        && gate.admission_gate_fingerprint == gate.admission_gate.fingerprint
        && gate.admission_gate_fingerprint
            == owned_dyn_runtime_admission_gate_fingerprint(gate.admission_gate)
        && !gate.facts.empty()
        && std::all_of(gate.facts.begin(),
            gate.facts.end(),
            [](const OwnedDynIrShapePrototypeFact& fact) {
                return is_valid(fact);
            })
        && layout_prototype_counts_are_consistent(gate)
        && is_valid(gate.summary, gate)
        && (gate.fingerprint == StableFingerprint128{}
            || gate.fingerprint == owned_dyn_ir_shape_prototype_gate_fingerprint(gate));
}

bool is_valid_m20b_owned_dyn_ir_shape_prototype_gate(
    const OwnedDynIrShapePrototypeGate& gate) noexcept
{
    return is_valid(gate)
        && std::string_view(gate.subject) == QUERY_OWNED_DYN_IR_SHAPE_M20B_SUBJECT
        && gate.summary.fact_count == QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT
        && gate.summary.owned_handle_metadata_count == QUERY_OWNED_DYN_IR_SHAPE_SINGLE_PROTOTYPE
        && gate.summary.erased_payload_pointer_count == QUERY_OWNED_DYN_IR_SHAPE_SINGLE_PROTOTYPE
        && gate.summary.vtable_pointer_metadata_count == QUERY_OWNED_DYN_IR_SHAPE_SINGLE_PROTOTYPE
        && gate.summary.drop_identity_placeholder_count == QUERY_OWNED_DYN_IR_SHAPE_SINGLE_PROTOTYPE
        && gate.summary.allocator_identity_placeholder_count == QUERY_OWNED_DYN_IR_SHAPE_SINGLE_PROTOTYPE
        && gate.summary.runtime_lowering_blocker_count == QUERY_OWNED_DYN_IR_SHAPE_SINGLE_PROTOTYPE
        && gate.summary.m20a_reference_count == QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT
        && gate.summary.compiler_owned_ir_shape_count == QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT
        && gate.summary.owned_layout_prototype_visible_count == QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT
        && gate.summary.handle_metadata_visible_count == QUERY_OWNED_DYN_IR_SHAPE_SINGLE_PROTOTYPE
        && gate.summary.erased_payload_pointer_visible_count == QUERY_OWNED_DYN_IR_SHAPE_SINGLE_PROTOTYPE
        && gate.summary.vtable_pointer_visible_count == QUERY_OWNED_DYN_IR_SHAPE_SINGLE_PROTOTYPE
        && gate.summary.drop_identity_placeholder_visible_count == QUERY_OWNED_DYN_IR_SHAPE_SINGLE_PROTOTYPE
        && gate.summary.allocator_identity_placeholder_visible_count
            == QUERY_OWNED_DYN_IR_SHAPE_SINGLE_PROTOTYPE
        && gate.summary.borrowed_dyn_abi_unchanged_count == QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT
        && gate.summary.standard_library_api_blocked_count == QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT
        && gate.summary.box_dyn_surface_blocked_count == QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT
        && gate.summary.owning_dyn_user_value_blocked_count == QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT
        && gate.summary.allocator_api_blocked_count == QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT
        && gate.summary.runtime_lowering_blocked_count == QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT
        && gate.summary.dynamic_drop_runtime_blocked_count == QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT
        && gate.summary.backend_helper_blocked_count == QUERY_OWNED_DYN_IR_SHAPE_FACT_COUNT
        && gate.summary.executable_runtime_implemented_count == 0U
        && gate.summary.observed_layout_prototype_total > 0U
        && m20b_fact_kinds_are_complete(gate);
}

void record_owned_dyn_ir_shape_prototype_fact(
    OwnedDynIrShapePrototypeGate& gate,
    OwnedDynIrShapePrototypeFact fact)
{
    gate.facts.push_back(std::move(fact));
    gate.summary = summarize_owned_dyn_ir_shape_prototype_gate_counts(gate);
}

OwnedDynIrShapePrototypeSummary summarize_owned_dyn_ir_shape_prototype_gate_counts(
    const OwnedDynIrShapePrototypeGate& gate) noexcept
{
    OwnedDynIrShapePrototypeSummary summary;
    summary.fact_count = static_cast<base::u64>(gate.facts.size());
    for (const OwnedDynIrShapePrototypeFact& fact : gate.facts) {
        switch (fact.kind) {
            case OwnedDynIrShapePrototypeFactKind::owned_handle_metadata:
                ++summary.owned_handle_metadata_count;
                break;
            case OwnedDynIrShapePrototypeFactKind::erased_payload_pointer:
                ++summary.erased_payload_pointer_count;
                break;
            case OwnedDynIrShapePrototypeFactKind::vtable_pointer_metadata:
                ++summary.vtable_pointer_metadata_count;
                break;
            case OwnedDynIrShapePrototypeFactKind::drop_identity_placeholder:
                ++summary.drop_identity_placeholder_count;
                break;
            case OwnedDynIrShapePrototypeFactKind::allocator_identity_placeholder:
                ++summary.allocator_identity_placeholder_count;
                break;
            case OwnedDynIrShapePrototypeFactKind::runtime_lowering_blocker:
                ++summary.runtime_lowering_blocker_count;
                break;
        }
        if (fact.references_m20a_admission_gate) {
            ++summary.m20a_reference_count;
        }
        if (fact.compiler_owned_ir_shape) {
            ++summary.compiler_owned_ir_shape_count;
        }
        if (fact.owned_layout_prototype_visible) {
            ++summary.owned_layout_prototype_visible_count;
        }
        if (fact.handle_metadata_visible) {
            ++summary.handle_metadata_visible_count;
        }
        if (fact.erased_payload_pointer_visible) {
            ++summary.erased_payload_pointer_visible_count;
        }
        if (fact.vtable_pointer_visible) {
            ++summary.vtable_pointer_visible_count;
        }
        if (fact.drop_identity_placeholder_visible) {
            ++summary.drop_identity_placeholder_visible_count;
        }
        if (fact.allocator_identity_placeholder_visible) {
            ++summary.allocator_identity_placeholder_visible_count;
        }
        if (fact.borrowed_dyn_abi_unchanged) {
            ++summary.borrowed_dyn_abi_unchanged_count;
        }
        if (fact.standard_library_api_blocked) {
            ++summary.standard_library_api_blocked_count;
        }
        if (fact.box_dyn_surface_blocked) {
            ++summary.box_dyn_surface_blocked_count;
        }
        if (fact.owning_dyn_user_value_blocked) {
            ++summary.owning_dyn_user_value_blocked_count;
        }
        if (fact.allocator_api_blocked) {
            ++summary.allocator_api_blocked_count;
        }
        if (fact.runtime_lowering_blocked) {
            ++summary.runtime_lowering_blocked_count;
        }
        if (fact.dynamic_drop_runtime_blocked) {
            ++summary.dynamic_drop_runtime_blocked_count;
        }
        if (fact.backend_helper_blocked) {
            ++summary.backend_helper_blocked_count;
        }
        if (fact.executable_runtime_implemented) {
            ++summary.executable_runtime_implemented_count;
        }
        summary.observed_layout_prototype_total =
            std::max(summary.observed_layout_prototype_total, fact.layout_prototype_count);
    }
    return summary;
}

StableFingerprint128 owned_dyn_ir_shape_prototype_gate_fingerprint(
    const OwnedDynIrShapePrototypeGate& gate) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_OWNED_DYN_IR_SHAPE_FINGERPRINT_MARKER);
    builder.mix_string(gate.subject);
    builder.mix_fingerprint(gate.admission_gate_fingerprint);
    mix_summary(builder, gate.summary);
    builder.mix_u64(static_cast<base::u64>(gate.facts.size()));
    for (const OwnedDynIrShapePrototypeFact& fact : gate.facts) {
        mix_fact(builder, fact);
    }
    return builder.finish();
}

std::string summarize_owned_dyn_ir_shape_prototype_gate(
    const OwnedDynIrShapePrototypeGate& gate)
{
    std::ostringstream label;
    label << "owned_dyn_ir_shape_prototype_gate subject="
          << (gate.subject.empty() ? "<anonymous>" : gate.subject)
          << " facts=" << gate.summary.fact_count
          << " owned_handle=" << gate.summary.owned_handle_metadata_count
          << " erased_payload_pointer=" << gate.summary.erased_payload_pointer_count
          << " vtable_pointer=" << gate.summary.vtable_pointer_metadata_count
          << " drop_identity_placeholder="
          << gate.summary.drop_identity_placeholder_count
          << " allocator_identity_placeholder="
          << gate.summary.allocator_identity_placeholder_count
          << " runtime_lowering_blocker=" << gate.summary.runtime_lowering_blocker_count
          << " m20a_refs=" << gate.summary.m20a_reference_count
          << " compiler_owned=" << gate.summary.compiler_owned_ir_shape_count
          << " layout_prototype_visible="
          << gate.summary.owned_layout_prototype_visible_count
          << " borrowed_abi_unchanged="
          << gate.summary.borrowed_dyn_abi_unchanged_count
          << " standard_library_api_blocked="
          << gate.summary.standard_library_api_blocked_count
          << " box_dyn_surface_blocked=" << gate.summary.box_dyn_surface_blocked_count
          << " runtime_lowering_blocked="
          << gate.summary.runtime_lowering_blocked_count
          << " dynamic_drop_runtime_blocked="
          << gate.summary.dynamic_drop_runtime_blocked_count
          << " backend_helper_blocked="
          << gate.summary.backend_helper_blocked_count
          << " executable_runtime_implemented="
          << gate.summary.executable_runtime_implemented_count;
    if (!gate.facts.empty()) {
        label << " first_fact=" << gate.facts.front().fact_name
              << " policy="
              << owned_dyn_ir_shape_prototype_policy_name(gate.facts.front().policy);
    }
    label << " fingerprint=" << debug_string(owned_dyn_ir_shape_prototype_gate_fingerprint(gate));
    return label.str();
}

std::string dump_owned_dyn_ir_shape_prototype_gate(
    const OwnedDynIrShapePrototypeGate& gate)
{
    std::ostringstream stream;
    stream << "owned_dyn_ir_shape_prototype_gate subject="
           << (gate.subject.empty() ? "<anonymous>" : gate.subject)
           << " facts=" << gate.summary.fact_count
           << " admission_gate=" << debug_string(gate.admission_gate_fingerprint)
           << " fingerprint=" << debug_string(owned_dyn_ir_shape_prototype_gate_fingerprint(gate))
           << '\n';
    for (base::usize index = 0; index < gate.facts.size(); ++index) {
        const OwnedDynIrShapePrototypeFact& fact = gate.facts[index];
        stream << "  shape #" << index
               << " name=" << fact.fact_name
               << " kind=" << owned_dyn_ir_shape_prototype_fact_kind_name(fact.kind)
               << " stage=" << owned_dyn_ir_shape_prototype_stage_name(fact.stage)
               << " policy=" << owned_dyn_ir_shape_prototype_policy_name(fact.policy)
               << " m20a=" << (fact.references_m20a_admission_gate ? "yes" : "no")
               << " compiler_owned=" << (fact.compiler_owned_ir_shape ? "yes" : "no")
               << " layout_prototype_visible="
               << (fact.owned_layout_prototype_visible ? "yes" : "no")
               << " handle=" << (fact.handle_metadata_visible ? "yes" : "no")
               << " payload_ptr=" << (fact.erased_payload_pointer_visible ? "yes" : "no")
               << " vtable_ptr=" << (fact.vtable_pointer_visible ? "yes" : "no")
               << " drop_identity_placeholder="
               << (fact.drop_identity_placeholder_visible ? "yes" : "no")
               << " allocator_identity_placeholder="
               << (fact.allocator_identity_placeholder_visible ? "yes" : "no")
               << " borrowed_abi_unchanged="
               << (fact.borrowed_dyn_abi_unchanged ? "yes" : "no")
               << " stdlib_blocked=" << (fact.standard_library_api_blocked ? "yes" : "no")
               << " box_blocked=" << (fact.box_dyn_surface_blocked ? "yes" : "no")
               << " owning_value_blocked="
               << (fact.owning_dyn_user_value_blocked ? "yes" : "no")
               << " allocator_api_blocked=" << (fact.allocator_api_blocked ? "yes" : "no")
               << " runtime_blocked=" << (fact.runtime_lowering_blocked ? "yes" : "no")
               << " dynamic_drop_blocked="
               << (fact.dynamic_drop_runtime_blocked ? "yes" : "no")
               << " backend_helper_blocked="
               << (fact.backend_helper_blocked ? "yes" : "no")
               << " executable_runtime_implemented="
               << (fact.executable_runtime_implemented ? "yes" : "no")
               << " object_key=" << fact.object_type.global_id
               << " prototype_count=" << fact.layout_prototype_count
               << " handle_fields=" << fact.handle_field_count
               << " m20a_gate=" << fallback_name(fact.m20a_gate_fact, "<unknown>")
               << " verifier_guard=" << fallback_name(fact.verifier_guard_fact, "<unknown>")
               << " blocked_surface=" << fallback_name(fact.blocked_surface_fact, "<unknown>")
               << '\n';
    }
    return stream.str();
}

OwnedDynIrShapePrototypeGate m20b_owned_dyn_ir_shape_prototype_gate_baseline()
{
    OwnedDynIrShapePrototypeGate gate;
    gate.subject = std::string(QUERY_OWNED_DYN_IR_SHAPE_M20B_SUBJECT);
    gate.admission_gate = m20_owned_dyn_runtime_admission_gate_baseline();
    gate.admission_gate_fingerprint = gate.admission_gate.fingerprint;

    record_owned_dyn_ir_shape_prototype_fact(gate,
        shape_fact(OwnedDynIrShapePrototypeFactKind::owned_handle_metadata,
            OwnedDynIrShapePrototypeStage::ir_shape_prototype,
            OwnedDynIrShapePrototypePolicy::owned_handle_two_field_v1,
            QUERY_OWNED_DYN_IR_SHAPE_HANDLE_FACT,
            QUERY_OWNED_DYN_IR_SHAPE_VERIFIER_SHAPE,
            QUERY_OWNED_DYN_IR_SHAPE_NO_OWNING_VALUE));
    record_owned_dyn_ir_shape_prototype_fact(gate,
        shape_fact(OwnedDynIrShapePrototypeFactKind::erased_payload_pointer,
            OwnedDynIrShapePrototypeStage::ir_shape_prototype,
            OwnedDynIrShapePrototypePolicy::erased_payload_pointer_v1,
            QUERY_OWNED_DYN_IR_SHAPE_DATA_FACT,
            "verifier_requires_mut_u8_erased_payload_pointer",
            QUERY_OWNED_DYN_IR_SHAPE_NO_BOX));
    record_owned_dyn_ir_shape_prototype_fact(gate,
        shape_fact(OwnedDynIrShapePrototypeFactKind::vtable_pointer_metadata,
            OwnedDynIrShapePrototypeStage::verifier_shape_guard,
            OwnedDynIrShapePrototypePolicy::borrowed_vtable_pointer_unchanged_v1,
            QUERY_OWNED_DYN_IR_SHAPE_VTABLE_FACT,
            QUERY_OWNED_DYN_IR_SHAPE_BORROWED_ABI,
            QUERY_OWNED_DYN_IR_SHAPE_NO_BACKEND));
    record_owned_dyn_ir_shape_prototype_fact(gate,
        shape_fact(OwnedDynIrShapePrototypeFactKind::drop_identity_placeholder,
            OwnedDynIrShapePrototypeStage::blocked_future_runtime,
            OwnedDynIrShapePrototypePolicy::drop_identity_not_lowered_v1,
            QUERY_OWNED_DYN_IR_SHAPE_DROP_FACT,
            "verifier_keeps_erased_drop_identity_as_placeholder",
            QUERY_OWNED_DYN_IR_SHAPE_NO_DYNAMIC_DROP));
    record_owned_dyn_ir_shape_prototype_fact(gate,
        shape_fact(OwnedDynIrShapePrototypeFactKind::allocator_identity_placeholder,
            OwnedDynIrShapePrototypeStage::blocked_future_runtime,
            OwnedDynIrShapePrototypePolicy::allocator_identity_not_lowered_v1,
            QUERY_OWNED_DYN_IR_SHAPE_ALLOCATOR_FACT,
            "verifier_keeps_allocator_identity_as_placeholder",
            QUERY_OWNED_DYN_IR_SHAPE_NO_ALLOCATOR));
    record_owned_dyn_ir_shape_prototype_fact(gate,
        shape_fact(OwnedDynIrShapePrototypeFactKind::runtime_lowering_blocker,
            OwnedDynIrShapePrototypeStage::blocked_future_runtime,
            OwnedDynIrShapePrototypePolicy::runtime_lowering_not_implemented_v1,
            QUERY_OWNED_DYN_IR_SHAPE_RUNTIME_FACT,
            "verifier_keeps_owned_dyn_runtime_lowering_blocked",
            QUERY_OWNED_DYN_IR_SHAPE_NO_RUNTIME));

    gate.fingerprint = owned_dyn_ir_shape_prototype_gate_fingerprint(gate);
    return gate;
}

} // namespace aurex::query
