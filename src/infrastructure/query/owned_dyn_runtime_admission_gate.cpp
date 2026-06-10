#include <aurex/infrastructure/query/owned_dyn_runtime_admission_gate.hpp>

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_FINGERPRINT_MARKER =
    "query.owned_dyn_runtime_admission_gate.v1";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_M20_SUBJECT =
    "M20a Owned Dyn Runtime Admission Design Gate";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_LAYOUT_FACT =
    "owned_dyn_object_layout_admission_fact";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_DROP_IDENTITY_FACT =
    "erased_drop_identity_admission_fact";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_ALLOCATOR_FACT =
    "allocator_identity_admission_fact";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_RUNTIME_ABI_FACT =
    "runtime_lowering_abi_admission_fact";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_BOX_SURFACE_FACT =
    "box_dyn_surface_admission_fact";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_BORROWED_ABI_FACT =
    "borrowed_dyn_abi_separation_fact";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_M17_INPUT =
    "requires_m17_dyn_ownership_runtime_facts";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_M18_INPUT =
    "requires_m18_dyn_ownership_runtime_boundary_gate";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_M19_INPUT =
    "requires_m19_dyn_ownership_runtime_ir_verifier_facts";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_M20B_IR_SHAPE =
    "m20b_owned_dyn_ir_shape_prerequisite";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_M20C_IDENTITIES =
    "m20c_drop_allocator_identity_prerequisite";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_M20D_RUNTIME =
    "m20d_runtime_lowering_abi_design_closure";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_M21_STDLIB =
    "m21_standard_library_box_dyn_surface";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_BORROWED_GUARD =
    "borrowed_dyn_vtable_remains_destructor_free";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_NO_STDLIB =
    "standard_library_api_not_in_m20a";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_NO_BOX =
    "box_dyn_trait_not_in_m20a";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_NO_ALLOCATOR =
    "allocator_api_not_in_m20a";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_NO_RUNTIME =
    "runtime_abi_lowering_not_in_m20a";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_NO_DYNAMIC_DROP =
    "dynamic_drop_runtime_not_in_m20a";
constexpr std::string_view QUERY_OWNED_DYN_RUNTIME_ADMISSION_NO_BACKEND =
    "backend_runtime_helper_not_in_m20a";
constexpr base::u8 QUERY_OWNED_DYN_RUNTIME_ADMISSION_INVALID_ENUM_VALUE = 255U;
constexpr base::u64 QUERY_OWNED_DYN_RUNTIME_ADMISSION_FACT_COUNT = 6U;

[[nodiscard]] base::u8 stable_capability_value(
    const OwnedDynRuntimeAdmissionCapability capability) noexcept
{
    return is_valid(capability) ? static_cast<base::u8>(capability)
                                : QUERY_OWNED_DYN_RUNTIME_ADMISSION_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_stage_value(
    const OwnedDynRuntimeAdmissionStage stage) noexcept
{
    return is_valid(stage) ? static_cast<base::u8>(stage)
                           : QUERY_OWNED_DYN_RUNTIME_ADMISSION_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_policy_value(
    const OwnedDynRuntimeAdmissionPolicy policy) noexcept
{
    return is_valid(policy) ? static_cast<base::u8>(policy)
                            : QUERY_OWNED_DYN_RUNTIME_ADMISSION_INVALID_ENUM_VALUE;
}

[[nodiscard]] bool nonempty(const std::string_view value) noexcept
{
    return !value.empty();
}

[[nodiscard]] bool fact_payload_is_named(const OwnedDynRuntimeAdmissionFact& fact) noexcept
{
    return nonempty(fact.fact_name)
        && nonempty(fact.admission_fact)
        && nonempty(fact.required_input_fact)
        && nonempty(fact.next_stage_fact)
        && nonempty(fact.blocked_surface_fact);
}

[[nodiscard]] bool baseline_blockers_are_intact(
    const OwnedDynRuntimeAdmissionFact& fact) noexcept
{
    return fact.references_m17_runtime_facts
        && fact.references_m18_boundary_gate
        && fact.references_m19_ir_verifier_facts
        && fact.standard_library_api_blocked
        && fact.box_dyn_surface_blocked
        && fact.owning_dyn_user_value_blocked
        && fact.allocator_api_blocked
        && fact.runtime_lowering_blocked
        && fact.dynamic_drop_runtime_blocked
        && fact.borrowed_dyn_abi_unchanged
        && !fact.executable_surface_implemented;
}

[[nodiscard]] bool capability_policy_stage_match(
    const OwnedDynRuntimeAdmissionFact& fact) noexcept
{
    switch (fact.capability) {
        case OwnedDynRuntimeAdmissionCapability::owned_object_layout:
            return fact.stage == OwnedDynRuntimeAdmissionStage::ir_shape_prerequisite
                && fact.policy == OwnedDynRuntimeAdmissionPolicy::owned_handle_metadata_v1
                && fact.owned_layout_required
                && !fact.erased_drop_identity_required
                && !fact.allocator_identity_required
                && !fact.runtime_abi_required
                && !fact.backend_helper_required;
        case OwnedDynRuntimeAdmissionCapability::erased_drop_identity:
            return fact.stage == OwnedDynRuntimeAdmissionStage::runtime_identity_prerequisite
                && fact.policy == OwnedDynRuntimeAdmissionPolicy::erased_drop_identity_v1
                && fact.owned_layout_required
                && fact.erased_drop_identity_required
                && !fact.allocator_identity_required
                && !fact.runtime_abi_required
                && !fact.backend_helper_required;
        case OwnedDynRuntimeAdmissionCapability::allocator_identity:
            return fact.stage == OwnedDynRuntimeAdmissionStage::runtime_identity_prerequisite
                && fact.policy == OwnedDynRuntimeAdmissionPolicy::allocator_identity_v1
                && fact.owned_layout_required
                && !fact.erased_drop_identity_required
                && fact.allocator_identity_required
                && !fact.runtime_abi_required
                && !fact.backend_helper_required;
        case OwnedDynRuntimeAdmissionCapability::runtime_lowering_abi:
            return fact.stage == OwnedDynRuntimeAdmissionStage::blocked_future_implementation
                && fact.policy == OwnedDynRuntimeAdmissionPolicy::runtime_lowering_abi_v1
                && fact.owned_layout_required
                && fact.erased_drop_identity_required
                && fact.allocator_identity_required
                && fact.runtime_abi_required
                && fact.backend_helper_required;
        case OwnedDynRuntimeAdmissionCapability::box_dyn_surface:
            return fact.stage == OwnedDynRuntimeAdmissionStage::standard_library_api_prerequisite
                && fact.policy == OwnedDynRuntimeAdmissionPolicy::box_dyn_surface_v1
                && fact.owned_layout_required
                && fact.erased_drop_identity_required
                && fact.allocator_identity_required
                && !fact.runtime_abi_required
                && !fact.backend_helper_required;
        case OwnedDynRuntimeAdmissionCapability::borrowed_dyn_abi_separation:
            return fact.stage == OwnedDynRuntimeAdmissionStage::admission_design_gate
                && fact.policy
                    == OwnedDynRuntimeAdmissionPolicy::borrowed_dyn_remains_destructor_free_v1
                && !fact.owned_layout_required
                && !fact.erased_drop_identity_required
                && !fact.allocator_identity_required
                && !fact.runtime_abi_required
                && !fact.backend_helper_required;
    }
    return false;
}

[[nodiscard]] bool summary_equals(
    const OwnedDynRuntimeAdmissionSummary& lhs,
    const OwnedDynRuntimeAdmissionSummary& rhs) noexcept
{
    return lhs.fact_count == rhs.fact_count
        && lhs.owned_object_layout_count == rhs.owned_object_layout_count
        && lhs.erased_drop_identity_count == rhs.erased_drop_identity_count
        && lhs.allocator_identity_count == rhs.allocator_identity_count
        && lhs.runtime_lowering_abi_count == rhs.runtime_lowering_abi_count
        && lhs.box_dyn_surface_count == rhs.box_dyn_surface_count
        && lhs.borrowed_dyn_abi_separation_count == rhs.borrowed_dyn_abi_separation_count
        && lhs.m17_reference_count == rhs.m17_reference_count
        && lhs.m18_reference_count == rhs.m18_reference_count
        && lhs.m19_reference_count == rhs.m19_reference_count
        && lhs.standard_library_api_blocked_count == rhs.standard_library_api_blocked_count
        && lhs.box_dyn_surface_blocked_count == rhs.box_dyn_surface_blocked_count
        && lhs.owning_dyn_user_value_blocked_count == rhs.owning_dyn_user_value_blocked_count
        && lhs.allocator_api_blocked_count == rhs.allocator_api_blocked_count
        && lhs.runtime_lowering_blocked_count == rhs.runtime_lowering_blocked_count
        && lhs.dynamic_drop_runtime_blocked_count == rhs.dynamic_drop_runtime_blocked_count
        && lhs.borrowed_dyn_abi_unchanged_count == rhs.borrowed_dyn_abi_unchanged_count
        && lhs.owned_layout_required_count == rhs.owned_layout_required_count
        && lhs.erased_drop_identity_required_count == rhs.erased_drop_identity_required_count
        && lhs.allocator_identity_required_count == rhs.allocator_identity_required_count
        && lhs.runtime_abi_required_count == rhs.runtime_abi_required_count
        && lhs.backend_helper_required_count == rhs.backend_helper_required_count
        && lhs.executable_surface_implemented_count == rhs.executable_surface_implemented_count;
}

void mix_summary(
    StableHashBuilder& builder,
    const OwnedDynRuntimeAdmissionSummary& summary) noexcept
{
    builder.mix_u64(summary.fact_count);
    builder.mix_u64(summary.owned_object_layout_count);
    builder.mix_u64(summary.erased_drop_identity_count);
    builder.mix_u64(summary.allocator_identity_count);
    builder.mix_u64(summary.runtime_lowering_abi_count);
    builder.mix_u64(summary.box_dyn_surface_count);
    builder.mix_u64(summary.borrowed_dyn_abi_separation_count);
    builder.mix_u64(summary.m17_reference_count);
    builder.mix_u64(summary.m18_reference_count);
    builder.mix_u64(summary.m19_reference_count);
    builder.mix_u64(summary.standard_library_api_blocked_count);
    builder.mix_u64(summary.box_dyn_surface_blocked_count);
    builder.mix_u64(summary.owning_dyn_user_value_blocked_count);
    builder.mix_u64(summary.allocator_api_blocked_count);
    builder.mix_u64(summary.runtime_lowering_blocked_count);
    builder.mix_u64(summary.dynamic_drop_runtime_blocked_count);
    builder.mix_u64(summary.borrowed_dyn_abi_unchanged_count);
    builder.mix_u64(summary.owned_layout_required_count);
    builder.mix_u64(summary.erased_drop_identity_required_count);
    builder.mix_u64(summary.allocator_identity_required_count);
    builder.mix_u64(summary.runtime_abi_required_count);
    builder.mix_u64(summary.backend_helper_required_count);
    builder.mix_u64(summary.executable_surface_implemented_count);
}

void mix_fact(StableHashBuilder& builder, const OwnedDynRuntimeAdmissionFact& fact) noexcept
{
    builder.mix_string(fact.fact_name);
    builder.mix_u8(stable_capability_value(fact.capability));
    builder.mix_u8(stable_stage_value(fact.stage));
    builder.mix_u8(stable_policy_value(fact.policy));
    builder.mix_bool(fact.references_m17_runtime_facts);
    builder.mix_bool(fact.references_m18_boundary_gate);
    builder.mix_bool(fact.references_m19_ir_verifier_facts);
    builder.mix_bool(fact.standard_library_api_blocked);
    builder.mix_bool(fact.box_dyn_surface_blocked);
    builder.mix_bool(fact.owning_dyn_user_value_blocked);
    builder.mix_bool(fact.allocator_api_blocked);
    builder.mix_bool(fact.runtime_lowering_blocked);
    builder.mix_bool(fact.dynamic_drop_runtime_blocked);
    builder.mix_bool(fact.borrowed_dyn_abi_unchanged);
    builder.mix_bool(fact.owned_layout_required);
    builder.mix_bool(fact.erased_drop_identity_required);
    builder.mix_bool(fact.allocator_identity_required);
    builder.mix_bool(fact.runtime_abi_required);
    builder.mix_bool(fact.backend_helper_required);
    builder.mix_bool(fact.executable_surface_implemented);
    builder.mix_string(fact.admission_fact);
    builder.mix_string(fact.required_input_fact);
    builder.mix_string(fact.next_stage_fact);
    builder.mix_string(fact.blocked_surface_fact);
}

[[nodiscard]] OwnedDynRuntimeAdmissionFact admission_fact(
    const OwnedDynRuntimeAdmissionCapability capability,
    const OwnedDynRuntimeAdmissionStage stage,
    const OwnedDynRuntimeAdmissionPolicy policy,
    const std::string_view fact_name,
    const std::string_view admission,
    const std::string_view required_input,
    const std::string_view next_stage,
    const std::string_view blocked_surface,
    const bool owned_layout_required,
    const bool erased_drop_identity_required,
    const bool allocator_identity_required,
    const bool runtime_abi_required,
    const bool backend_helper_required)
{
    OwnedDynRuntimeAdmissionFact fact;
    fact.fact_name = std::string(fact_name);
    fact.capability = capability;
    fact.stage = stage;
    fact.policy = policy;
    fact.owned_layout_required = owned_layout_required;
    fact.erased_drop_identity_required = erased_drop_identity_required;
    fact.allocator_identity_required = allocator_identity_required;
    fact.runtime_abi_required = runtime_abi_required;
    fact.backend_helper_required = backend_helper_required;
    fact.admission_fact = std::string(admission);
    fact.required_input_fact = std::string(required_input);
    fact.next_stage_fact = std::string(next_stage);
    fact.blocked_surface_fact = std::string(blocked_surface);
    return fact;
}

[[nodiscard]] bool m20_fact_kinds_are_complete(
    const OwnedDynRuntimeAdmissionGate& gate) noexcept
{
    std::array<bool, QUERY_OWNED_DYN_RUNTIME_ADMISSION_FACT_COUNT> seen{};
    for (const OwnedDynRuntimeAdmissionFact& fact : gate.admissions) {
        if (!is_valid(fact.capability)) {
            return false;
        }
        const auto index = static_cast<base::usize>(fact.capability) - 1U;
        if (index >= seen.size() || seen[index]) {
            return false;
        }
        seen[index] = true;
    }
    return std::all_of(seen.begin(), seen.end(), [](const bool present) {
        return present;
    });
}

[[nodiscard]] const char* fallback_name(const std::string& value, const char* fallback) noexcept
{
    return value.empty() ? fallback : value.c_str();
}

} // namespace

std::string_view owned_dyn_runtime_admission_capability_name(
    const OwnedDynRuntimeAdmissionCapability capability) noexcept
{
    switch (capability) {
        case OwnedDynRuntimeAdmissionCapability::owned_object_layout:
            return "owned_object_layout";
        case OwnedDynRuntimeAdmissionCapability::erased_drop_identity:
            return "erased_drop_identity";
        case OwnedDynRuntimeAdmissionCapability::allocator_identity:
            return "allocator_identity";
        case OwnedDynRuntimeAdmissionCapability::runtime_lowering_abi:
            return "runtime_lowering_abi";
        case OwnedDynRuntimeAdmissionCapability::box_dyn_surface:
            return "box_dyn_surface";
        case OwnedDynRuntimeAdmissionCapability::borrowed_dyn_abi_separation:
            return "borrowed_dyn_abi_separation";
    }
    return "invalid";
}

std::string_view owned_dyn_runtime_admission_stage_name(
    const OwnedDynRuntimeAdmissionStage stage) noexcept
{
    switch (stage) {
        case OwnedDynRuntimeAdmissionStage::admission_design_gate:
            return "admission_design_gate";
        case OwnedDynRuntimeAdmissionStage::ir_shape_prerequisite:
            return "ir_shape_prerequisite";
        case OwnedDynRuntimeAdmissionStage::runtime_identity_prerequisite:
            return "runtime_identity_prerequisite";
        case OwnedDynRuntimeAdmissionStage::standard_library_api_prerequisite:
            return "standard_library_api_prerequisite";
        case OwnedDynRuntimeAdmissionStage::blocked_future_implementation:
            return "blocked_future_implementation";
    }
    return "invalid";
}

std::string_view owned_dyn_runtime_admission_policy_name(
    const OwnedDynRuntimeAdmissionPolicy policy) noexcept
{
    switch (policy) {
        case OwnedDynRuntimeAdmissionPolicy::owned_handle_metadata_v1:
            return "owned_handle_metadata_v1";
        case OwnedDynRuntimeAdmissionPolicy::erased_drop_identity_v1:
            return "erased_drop_identity_v1";
        case OwnedDynRuntimeAdmissionPolicy::allocator_identity_v1:
            return "allocator_identity_v1";
        case OwnedDynRuntimeAdmissionPolicy::runtime_lowering_abi_v1:
            return "runtime_lowering_abi_v1";
        case OwnedDynRuntimeAdmissionPolicy::box_dyn_surface_v1:
            return "box_dyn_surface_v1";
        case OwnedDynRuntimeAdmissionPolicy::borrowed_dyn_remains_destructor_free_v1:
            return "borrowed_dyn_remains_destructor_free_v1";
    }
    return "invalid";
}

bool is_valid(const OwnedDynRuntimeAdmissionCapability capability) noexcept
{
    return capability == OwnedDynRuntimeAdmissionCapability::owned_object_layout
        || capability == OwnedDynRuntimeAdmissionCapability::erased_drop_identity
        || capability == OwnedDynRuntimeAdmissionCapability::allocator_identity
        || capability == OwnedDynRuntimeAdmissionCapability::runtime_lowering_abi
        || capability == OwnedDynRuntimeAdmissionCapability::box_dyn_surface
        || capability == OwnedDynRuntimeAdmissionCapability::borrowed_dyn_abi_separation;
}

bool is_valid(const OwnedDynRuntimeAdmissionStage stage) noexcept
{
    return stage == OwnedDynRuntimeAdmissionStage::admission_design_gate
        || stage == OwnedDynRuntimeAdmissionStage::ir_shape_prerequisite
        || stage == OwnedDynRuntimeAdmissionStage::runtime_identity_prerequisite
        || stage == OwnedDynRuntimeAdmissionStage::standard_library_api_prerequisite
        || stage == OwnedDynRuntimeAdmissionStage::blocked_future_implementation;
}

bool is_valid(const OwnedDynRuntimeAdmissionPolicy policy) noexcept
{
    return policy == OwnedDynRuntimeAdmissionPolicy::owned_handle_metadata_v1
        || policy == OwnedDynRuntimeAdmissionPolicy::erased_drop_identity_v1
        || policy == OwnedDynRuntimeAdmissionPolicy::allocator_identity_v1
        || policy == OwnedDynRuntimeAdmissionPolicy::runtime_lowering_abi_v1
        || policy == OwnedDynRuntimeAdmissionPolicy::box_dyn_surface_v1
        || policy == OwnedDynRuntimeAdmissionPolicy::borrowed_dyn_remains_destructor_free_v1;
}

bool is_valid(const OwnedDynRuntimeAdmissionFact& fact) noexcept
{
    return is_valid(fact.capability)
        && is_valid(fact.stage)
        && is_valid(fact.policy)
        && fact_payload_is_named(fact)
        && baseline_blockers_are_intact(fact)
        && capability_policy_stage_match(fact);
}

bool is_valid(
    const OwnedDynRuntimeAdmissionSummary& summary,
    const OwnedDynRuntimeAdmissionGate& gate) noexcept
{
    return summary_equals(summary, summarize_owned_dyn_runtime_admission_gate_counts(gate));
}

bool is_valid(const OwnedDynRuntimeAdmissionGate& gate) noexcept
{
    return !gate.subject.empty()
        && is_valid_m17_dyn_ownership_runtime_preparation_baseline(gate.runtime_facts)
        && gate.runtime_facts_fingerprint == gate.runtime_facts.fingerprint
        && gate.runtime_facts_fingerprint
            == dyn_ownership_runtime_facts_fingerprint(gate.runtime_facts)
        && is_valid_m18_dyn_ownership_runtime_boundary_gate(gate.boundary_gate)
        && gate.boundary_gate_fingerprint == gate.boundary_gate.fingerprint
        && gate.boundary_gate_fingerprint
            == dyn_ownership_runtime_boundary_gate_fingerprint(gate.boundary_gate)
        && is_valid_m19_dyn_ownership_runtime_ir_verifier_baseline(gate.ir_verifier_facts)
        && gate.ir_verifier_facts_fingerprint == gate.ir_verifier_facts.fingerprint
        && gate.ir_verifier_facts_fingerprint
            == dyn_ownership_runtime_ir_verifier_facts_fingerprint(gate.ir_verifier_facts)
        && !gate.admissions.empty()
        && std::all_of(gate.admissions.begin(),
            gate.admissions.end(),
            [](const OwnedDynRuntimeAdmissionFact& fact) {
                return is_valid(fact);
            })
        && is_valid(gate.summary, gate)
        && (gate.fingerprint == StableFingerprint128{}
            || gate.fingerprint == owned_dyn_runtime_admission_gate_fingerprint(gate));
}

bool is_valid_m20_owned_dyn_runtime_admission_gate(
    const OwnedDynRuntimeAdmissionGate& gate) noexcept
{
    return is_valid(gate)
        && std::string_view(gate.subject) == QUERY_OWNED_DYN_RUNTIME_ADMISSION_M20_SUBJECT
        && gate.summary.fact_count == QUERY_OWNED_DYN_RUNTIME_ADMISSION_FACT_COUNT
        && gate.summary.owned_object_layout_count == 1U
        && gate.summary.erased_drop_identity_count == 1U
        && gate.summary.allocator_identity_count == 1U
        && gate.summary.runtime_lowering_abi_count == 1U
        && gate.summary.box_dyn_surface_count == 1U
        && gate.summary.borrowed_dyn_abi_separation_count == 1U
        && gate.summary.m17_reference_count == QUERY_OWNED_DYN_RUNTIME_ADMISSION_FACT_COUNT
        && gate.summary.m18_reference_count == QUERY_OWNED_DYN_RUNTIME_ADMISSION_FACT_COUNT
        && gate.summary.m19_reference_count == QUERY_OWNED_DYN_RUNTIME_ADMISSION_FACT_COUNT
        && gate.summary.standard_library_api_blocked_count
            == QUERY_OWNED_DYN_RUNTIME_ADMISSION_FACT_COUNT
        && gate.summary.box_dyn_surface_blocked_count
            == QUERY_OWNED_DYN_RUNTIME_ADMISSION_FACT_COUNT
        && gate.summary.owning_dyn_user_value_blocked_count
            == QUERY_OWNED_DYN_RUNTIME_ADMISSION_FACT_COUNT
        && gate.summary.allocator_api_blocked_count
            == QUERY_OWNED_DYN_RUNTIME_ADMISSION_FACT_COUNT
        && gate.summary.runtime_lowering_blocked_count
            == QUERY_OWNED_DYN_RUNTIME_ADMISSION_FACT_COUNT
        && gate.summary.dynamic_drop_runtime_blocked_count
            == QUERY_OWNED_DYN_RUNTIME_ADMISSION_FACT_COUNT
        && gate.summary.borrowed_dyn_abi_unchanged_count
            == QUERY_OWNED_DYN_RUNTIME_ADMISSION_FACT_COUNT
        && gate.summary.executable_surface_implemented_count == 0U
        && m20_fact_kinds_are_complete(gate);
}

void record_owned_dyn_runtime_admission_fact(
    OwnedDynRuntimeAdmissionGate& gate,
    OwnedDynRuntimeAdmissionFact fact)
{
    gate.admissions.push_back(std::move(fact));
    gate.summary = summarize_owned_dyn_runtime_admission_gate_counts(gate);
}

OwnedDynRuntimeAdmissionSummary summarize_owned_dyn_runtime_admission_gate_counts(
    const OwnedDynRuntimeAdmissionGate& gate) noexcept
{
    OwnedDynRuntimeAdmissionSummary summary;
    summary.fact_count = static_cast<base::u64>(gate.admissions.size());
    for (const OwnedDynRuntimeAdmissionFact& fact : gate.admissions) {
        switch (fact.capability) {
            case OwnedDynRuntimeAdmissionCapability::owned_object_layout:
                ++summary.owned_object_layout_count;
                break;
            case OwnedDynRuntimeAdmissionCapability::erased_drop_identity:
                ++summary.erased_drop_identity_count;
                break;
            case OwnedDynRuntimeAdmissionCapability::allocator_identity:
                ++summary.allocator_identity_count;
                break;
            case OwnedDynRuntimeAdmissionCapability::runtime_lowering_abi:
                ++summary.runtime_lowering_abi_count;
                break;
            case OwnedDynRuntimeAdmissionCapability::box_dyn_surface:
                ++summary.box_dyn_surface_count;
                break;
            case OwnedDynRuntimeAdmissionCapability::borrowed_dyn_abi_separation:
                ++summary.borrowed_dyn_abi_separation_count;
                break;
        }
        if (fact.references_m17_runtime_facts) {
            ++summary.m17_reference_count;
        }
        if (fact.references_m18_boundary_gate) {
            ++summary.m18_reference_count;
        }
        if (fact.references_m19_ir_verifier_facts) {
            ++summary.m19_reference_count;
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
        if (fact.borrowed_dyn_abi_unchanged) {
            ++summary.borrowed_dyn_abi_unchanged_count;
        }
        if (fact.owned_layout_required) {
            ++summary.owned_layout_required_count;
        }
        if (fact.erased_drop_identity_required) {
            ++summary.erased_drop_identity_required_count;
        }
        if (fact.allocator_identity_required) {
            ++summary.allocator_identity_required_count;
        }
        if (fact.runtime_abi_required) {
            ++summary.runtime_abi_required_count;
        }
        if (fact.backend_helper_required) {
            ++summary.backend_helper_required_count;
        }
        if (fact.executable_surface_implemented) {
            ++summary.executable_surface_implemented_count;
        }
    }
    return summary;
}

StableFingerprint128 owned_dyn_runtime_admission_gate_fingerprint(
    const OwnedDynRuntimeAdmissionGate& gate) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_OWNED_DYN_RUNTIME_ADMISSION_FINGERPRINT_MARKER);
    builder.mix_string(gate.subject);
    builder.mix_fingerprint(gate.runtime_facts_fingerprint);
    builder.mix_fingerprint(gate.boundary_gate_fingerprint);
    builder.mix_fingerprint(gate.ir_verifier_facts_fingerprint);
    mix_summary(builder, gate.summary);
    builder.mix_u64(static_cast<base::u64>(gate.admissions.size()));
    for (const OwnedDynRuntimeAdmissionFact& fact : gate.admissions) {
        mix_fact(builder, fact);
    }
    return builder.finish();
}

std::string summarize_owned_dyn_runtime_admission_gate(
    const OwnedDynRuntimeAdmissionGate& gate)
{
    std::ostringstream label;
    label << "owned_dyn_runtime_admission_gate subject="
          << (gate.subject.empty() ? "<anonymous>" : gate.subject)
          << " facts=" << gate.summary.fact_count
          << " owned_layout=" << gate.summary.owned_object_layout_count
          << " erased_drop_identity=" << gate.summary.erased_drop_identity_count
          << " allocator_identity=" << gate.summary.allocator_identity_count
          << " runtime_lowering_abi=" << gate.summary.runtime_lowering_abi_count
          << " box_dyn_surface=" << gate.summary.box_dyn_surface_count
          << " borrowed_dyn_abi_separation="
          << gate.summary.borrowed_dyn_abi_separation_count
          << " m17_refs=" << gate.summary.m17_reference_count
          << " m18_refs=" << gate.summary.m18_reference_count
          << " m19_refs=" << gate.summary.m19_reference_count
          << " standard_library_api_blocked="
          << gate.summary.standard_library_api_blocked_count
          << " box_dyn_surface_blocked=" << gate.summary.box_dyn_surface_blocked_count
          << " runtime_lowering_blocked=" << gate.summary.runtime_lowering_blocked_count
          << " dynamic_drop_runtime_blocked="
          << gate.summary.dynamic_drop_runtime_blocked_count
          << " executable_surface_implemented="
          << gate.summary.executable_surface_implemented_count;
    if (!gate.admissions.empty()) {
        label << " first_fact=" << gate.admissions.front().fact_name
              << " policy="
              << owned_dyn_runtime_admission_policy_name(gate.admissions.front().policy);
    }
    label << " fingerprint=" << debug_string(owned_dyn_runtime_admission_gate_fingerprint(gate));
    return label.str();
}

std::string dump_owned_dyn_runtime_admission_gate(
    const OwnedDynRuntimeAdmissionGate& gate)
{
    std::ostringstream stream;
    stream << "owned_dyn_runtime_admission_gate subject="
           << (gate.subject.empty() ? "<anonymous>" : gate.subject)
           << " facts=" << gate.summary.fact_count
           << " runtime_facts=" << debug_string(gate.runtime_facts_fingerprint)
           << " boundary_gate=" << debug_string(gate.boundary_gate_fingerprint)
           << " ir_verifier_facts=" << debug_string(gate.ir_verifier_facts_fingerprint)
           << " fingerprint=" << debug_string(owned_dyn_runtime_admission_gate_fingerprint(gate))
           << '\n';
    for (base::usize index = 0; index < gate.admissions.size(); ++index) {
        const OwnedDynRuntimeAdmissionFact& fact = gate.admissions[index];
        stream << "  admission #" << index
               << " name=" << fact.fact_name
               << " capability=" << owned_dyn_runtime_admission_capability_name(fact.capability)
               << " stage=" << owned_dyn_runtime_admission_stage_name(fact.stage)
               << " policy=" << owned_dyn_runtime_admission_policy_name(fact.policy)
               << " m17=" << (fact.references_m17_runtime_facts ? "yes" : "no")
               << " m18=" << (fact.references_m18_boundary_gate ? "yes" : "no")
               << " m19=" << (fact.references_m19_ir_verifier_facts ? "yes" : "no")
               << " stdlib_blocked=" << (fact.standard_library_api_blocked ? "yes" : "no")
               << " box_blocked=" << (fact.box_dyn_surface_blocked ? "yes" : "no")
               << " owning_value_blocked="
               << (fact.owning_dyn_user_value_blocked ? "yes" : "no")
               << " allocator_api_blocked=" << (fact.allocator_api_blocked ? "yes" : "no")
               << " runtime_blocked=" << (fact.runtime_lowering_blocked ? "yes" : "no")
               << " dynamic_drop_blocked="
               << (fact.dynamic_drop_runtime_blocked ? "yes" : "no")
               << " borrowed_abi_unchanged="
               << (fact.borrowed_dyn_abi_unchanged ? "yes" : "no")
               << " owned_layout_required=" << (fact.owned_layout_required ? "yes" : "no")
               << " erased_drop_identity_required="
               << (fact.erased_drop_identity_required ? "yes" : "no")
               << " allocator_identity_required="
               << (fact.allocator_identity_required ? "yes" : "no")
               << " runtime_abi_required=" << (fact.runtime_abi_required ? "yes" : "no")
               << " backend_helper_required="
               << (fact.backend_helper_required ? "yes" : "no")
               << " executable_surface_implemented="
               << (fact.executable_surface_implemented ? "yes" : "no")
               << " admission=" << fallback_name(fact.admission_fact, "<unknown>")
               << " required_input=" << fallback_name(fact.required_input_fact, "<unknown>")
               << " next_stage=" << fallback_name(fact.next_stage_fact, "<unknown>")
               << " blocked_surface=" << fallback_name(fact.blocked_surface_fact, "<unknown>")
               << '\n';
    }
    return stream.str();
}

OwnedDynRuntimeAdmissionGate m20_owned_dyn_runtime_admission_gate_baseline()
{
    OwnedDynRuntimeAdmissionGate gate;
    gate.subject = std::string(QUERY_OWNED_DYN_RUNTIME_ADMISSION_M20_SUBJECT);
    gate.runtime_facts = m17_dyn_ownership_runtime_preparation_baseline();
    gate.runtime_facts_fingerprint = gate.runtime_facts.fingerprint;
    gate.boundary_gate = m18_dyn_ownership_runtime_boundary_gate_baseline();
    gate.boundary_gate_fingerprint = gate.boundary_gate.fingerprint;
    gate.ir_verifier_facts = m19_dyn_ownership_runtime_ir_verifier_baseline();
    gate.ir_verifier_facts_fingerprint = gate.ir_verifier_facts.fingerprint;

    record_owned_dyn_runtime_admission_fact(gate,
        admission_fact(OwnedDynRuntimeAdmissionCapability::owned_object_layout,
            OwnedDynRuntimeAdmissionStage::ir_shape_prerequisite,
            OwnedDynRuntimeAdmissionPolicy::owned_handle_metadata_v1,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_LAYOUT_FACT,
            "owned_dyn_layout_requires_separate_owner_metadata",
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_M19_INPUT,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_M20B_IR_SHAPE,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_NO_BOX,
            true,
            false,
            false,
            false,
            false));
    record_owned_dyn_runtime_admission_fact(gate,
        admission_fact(OwnedDynRuntimeAdmissionCapability::erased_drop_identity,
            OwnedDynRuntimeAdmissionStage::runtime_identity_prerequisite,
            OwnedDynRuntimeAdmissionPolicy::erased_drop_identity_v1,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_DROP_IDENTITY_FACT,
            "erased_drop_identity_must_precede_dynamic_drop_runtime",
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_M17_INPUT,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_M20C_IDENTITIES,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_NO_DYNAMIC_DROP,
            true,
            true,
            false,
            false,
            false));
    record_owned_dyn_runtime_admission_fact(gate,
        admission_fact(OwnedDynRuntimeAdmissionCapability::allocator_identity,
            OwnedDynRuntimeAdmissionStage::runtime_identity_prerequisite,
            OwnedDynRuntimeAdmissionPolicy::allocator_identity_v1,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_ALLOCATOR_FACT,
            "allocator_identity_must_precede_deallocation_runtime",
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_M17_INPUT,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_M20C_IDENTITIES,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_NO_ALLOCATOR,
            true,
            false,
            true,
            false,
            false));
    record_owned_dyn_runtime_admission_fact(gate,
        admission_fact(OwnedDynRuntimeAdmissionCapability::runtime_lowering_abi,
            OwnedDynRuntimeAdmissionStage::blocked_future_implementation,
            OwnedDynRuntimeAdmissionPolicy::runtime_lowering_abi_v1,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_RUNTIME_ABI_FACT,
            "runtime_lowering_requires_owned_layout_drop_and_allocator_identity",
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_M18_INPUT,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_M20D_RUNTIME,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_NO_RUNTIME,
            true,
            true,
            true,
            true,
            true));
    record_owned_dyn_runtime_admission_fact(gate,
        admission_fact(OwnedDynRuntimeAdmissionCapability::box_dyn_surface,
            OwnedDynRuntimeAdmissionStage::standard_library_api_prerequisite,
            OwnedDynRuntimeAdmissionPolicy::box_dyn_surface_v1,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_BOX_SURFACE_FACT,
            "box_dyn_surface_requires_stdlib_after_owned_runtime_identity",
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_M18_INPUT,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_M21_STDLIB,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_NO_STDLIB,
            true,
            true,
            true,
            false,
            false));
    record_owned_dyn_runtime_admission_fact(gate,
        admission_fact(OwnedDynRuntimeAdmissionCapability::borrowed_dyn_abi_separation,
            OwnedDynRuntimeAdmissionStage::admission_design_gate,
            OwnedDynRuntimeAdmissionPolicy::borrowed_dyn_remains_destructor_free_v1,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_BORROWED_ABI_FACT,
            "borrowed_dyn_vtable_must_not_gain_owning_drop_slots",
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_M19_INPUT,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_BORROWED_GUARD,
            QUERY_OWNED_DYN_RUNTIME_ADMISSION_NO_BACKEND,
            false,
            false,
            false,
            false,
            false));

    gate.fingerprint = owned_dyn_runtime_admission_gate_fingerprint(gate);
    return gate;
}

} // namespace aurex::query
