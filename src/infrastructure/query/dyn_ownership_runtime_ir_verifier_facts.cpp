#include <aurex/infrastructure/query/dyn_ownership_runtime_ir_verifier_facts.hpp>

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_FINGERPRINT_MARKER =
    "query.dyn_ownership_runtime_ir_verifier_facts.v1";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_M19_SYMBOL =
    "M19 Dyn Ownership Runtime IR Verifier Preparation";
constexpr base::u8 QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_INVALID_ENUM_VALUE = 255U;
constexpr base::u64 QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_BASELINE_FACT_COUNT = 6U;
constexpr base::u32 QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_NO_OBSERVED_VALUE = 0U;
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_BORROWED_VTABLE_FACT =
    "borrowed_vtable_destructor_free_ir_guard";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_STATIC_CLEANUP_FACT =
    "static_cleanup_marker_only_ir_guard";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_DROP_IDENTITY_FACT =
    "future_erased_drop_identity_required_by_verifier";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_ALLOCATOR_FACT =
    "future_allocator_identity_required_by_verifier";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_OWNED_OBJECT_FACT =
    "future_owned_dyn_object_placeholder_blocked_in_ir";
constexpr std::string_view QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_RUNTIME_BLOCKER_FACT =
    "runtime_lowering_blocked_until_stdlib_runtime_stage";

[[nodiscard]] base::u8 stable_kind_value(
    const DynOwnershipRuntimeIrVerifierFactKind kind) noexcept
{
    return is_valid(kind) ? static_cast<base::u8>(kind)
                          : QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_stage_value(
    const DynOwnershipRuntimeIrVerifierStage stage) noexcept
{
    return is_valid(stage) ? static_cast<base::u8>(stage)
                           : QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_policy_value(
    const DynOwnershipRuntimeIrVerifierPolicy policy) noexcept
{
    return is_valid(policy) ? static_cast<base::u8>(policy)
                            : QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_INVALID_ENUM_VALUE;
}

[[nodiscard]] bool nonempty(const std::string_view value) noexcept
{
    return !value.empty();
}

[[nodiscard]] bool fact_payload_is_named(
    const DynOwnershipRuntimeIrVerifierFact& fact) noexcept
{
    return nonempty(fact.fact_name)
        && nonempty(fact.subject_symbol)
        && nonempty(fact.boundary_fact)
        && nonempty(fact.verifier_guard_fact)
        && nonempty(fact.blocked_surface_fact);
}

[[nodiscard]] bool blockers_are_intact(const DynOwnershipRuntimeIrVerifierFact& fact) noexcept
{
    return fact.verifier_visible
        && fact.owned_dyn_object_placeholder_blocked
        && fact.runtime_lowering_blocked
        && fact.dynamic_drop_runtime_blocked
        && fact.standard_library_blocked
        && !fact.lowering_runtime_implemented;
}

[[nodiscard]] bool kind_policy_stage_match(
    const DynOwnershipRuntimeIrVerifierFact& fact) noexcept
{
    switch (fact.kind) {
        case DynOwnershipRuntimeIrVerifierFactKind::borrowed_vtable_destructor_free:
            return fact.stage == DynOwnershipRuntimeIrVerifierStage::ir_verifier_preparation
                && fact.policy
                    == DynOwnershipRuntimeIrVerifierPolicy::borrowed_vtable_methods_only_v1
                && fact.borrowed_vtable_destructor_free
                && !fact.erased_drop_identity_required
                && !fact.allocator_identity_required;
        case DynOwnershipRuntimeIrVerifierFactKind::static_cleanup_only:
            return fact.stage == DynOwnershipRuntimeIrVerifierStage::verifier_negative_matrix
                && fact.policy == DynOwnershipRuntimeIrVerifierPolicy::static_cleanup_marker_only_v1
                && fact.static_cleanup_only
                && !fact.erased_drop_identity_required
                && !fact.allocator_identity_required;
        case DynOwnershipRuntimeIrVerifierFactKind::erased_drop_identity_required:
            return fact.stage == DynOwnershipRuntimeIrVerifierStage::blocked_future_runtime
                && fact.policy
                    == DynOwnershipRuntimeIrVerifierPolicy::erased_drop_identity_prerequisite_v1
                && fact.erased_drop_identity_required;
        case DynOwnershipRuntimeIrVerifierFactKind::allocator_identity_required:
            return fact.stage == DynOwnershipRuntimeIrVerifierStage::blocked_future_runtime
                && fact.policy
                    == DynOwnershipRuntimeIrVerifierPolicy::allocator_identity_prerequisite_v1
                && fact.allocator_identity_required;
        case DynOwnershipRuntimeIrVerifierFactKind::owned_dyn_object_placeholder_blocked:
            return fact.stage == DynOwnershipRuntimeIrVerifierStage::blocked_future_runtime
                && fact.policy
                    == DynOwnershipRuntimeIrVerifierPolicy::owned_dyn_object_placeholder_not_lowered_v1
                && fact.owned_dyn_object_placeholder_blocked;
        case DynOwnershipRuntimeIrVerifierFactKind::runtime_lowering_blocked_without_stdlib:
            return fact.stage == DynOwnershipRuntimeIrVerifierStage::blocked_future_runtime
                && fact.policy
                    == DynOwnershipRuntimeIrVerifierPolicy::runtime_lowering_not_implemented_v1
                && fact.runtime_lowering_blocked
                && fact.standard_library_blocked;
    }
    return false;
}

[[nodiscard]] bool summary_equals(
    const DynOwnershipRuntimeIrVerifierSummary& lhs,
    const DynOwnershipRuntimeIrVerifierSummary& rhs) noexcept
{
    return lhs.fact_count == rhs.fact_count
        && lhs.borrowed_vtable_count == rhs.borrowed_vtable_count
        && lhs.destructor_free_vtable_count == rhs.destructor_free_vtable_count
        && lhs.static_cleanup_only_count == rhs.static_cleanup_only_count
        && lhs.erased_drop_identity_required_count == rhs.erased_drop_identity_required_count
        && lhs.allocator_identity_required_count == rhs.allocator_identity_required_count
        && lhs.owned_dyn_object_placeholder_blocked_count
            == rhs.owned_dyn_object_placeholder_blocked_count
        && lhs.runtime_lowering_blocked_count == rhs.runtime_lowering_blocked_count
        && lhs.dynamic_drop_runtime_blocked_count == rhs.dynamic_drop_runtime_blocked_count
        && lhs.standard_library_blocked_count == rhs.standard_library_blocked_count
        && lhs.lowering_runtime_implemented_count == rhs.lowering_runtime_implemented_count
        && lhs.cleanup_marker_count == rhs.cleanup_marker_count
        && lhs.blocked_runtime_marker_count == rhs.blocked_runtime_marker_count;
}

void mix_summary(
    StableHashBuilder& builder,
    const DynOwnershipRuntimeIrVerifierSummary& summary) noexcept
{
    builder.mix_u64(summary.fact_count);
    builder.mix_u64(summary.borrowed_vtable_count);
    builder.mix_u64(summary.destructor_free_vtable_count);
    builder.mix_u64(summary.static_cleanup_only_count);
    builder.mix_u64(summary.erased_drop_identity_required_count);
    builder.mix_u64(summary.allocator_identity_required_count);
    builder.mix_u64(summary.owned_dyn_object_placeholder_blocked_count);
    builder.mix_u64(summary.runtime_lowering_blocked_count);
    builder.mix_u64(summary.dynamic_drop_runtime_blocked_count);
    builder.mix_u64(summary.standard_library_blocked_count);
    builder.mix_u64(summary.lowering_runtime_implemented_count);
    builder.mix_u64(summary.cleanup_marker_count);
    builder.mix_u64(summary.blocked_runtime_marker_count);
}

void mix_fact(StableHashBuilder& builder, const DynOwnershipRuntimeIrVerifierFact& fact) noexcept
{
    builder.mix_string(fact.fact_name);
    builder.mix_u8(stable_kind_value(fact.kind));
    builder.mix_u8(stable_stage_value(fact.stage));
    builder.mix_u8(stable_policy_value(fact.policy));
    builder.mix_bool(fact.verifier_visible);
    builder.mix_bool(fact.borrowed_vtable_destructor_free);
    builder.mix_bool(fact.static_cleanup_only);
    builder.mix_bool(fact.erased_drop_identity_required);
    builder.mix_bool(fact.allocator_identity_required);
    builder.mix_bool(fact.owned_dyn_object_placeholder_blocked);
    builder.mix_bool(fact.runtime_lowering_blocked);
    builder.mix_bool(fact.dynamic_drop_runtime_blocked);
    builder.mix_bool(fact.standard_library_blocked);
    builder.mix_bool(fact.lowering_runtime_implemented);
    builder.mix_u64(fact.object_type.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(fact.object_type));
    builder.mix_u64(fact.vtable_layout.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(fact.vtable_layout));
    builder.mix_u32(fact.observed_value_id);
    builder.mix_u32(fact.static_cleanup_marker_count);
    builder.mix_u32(fact.blocked_runtime_marker_count);
    builder.mix_string(fact.subject_symbol);
    builder.mix_string(fact.boundary_fact);
    builder.mix_string(fact.verifier_guard_fact);
    builder.mix_string(fact.blocked_surface_fact);
}

[[nodiscard]] DynOwnershipRuntimeIrVerifierFact baseline_fact(
    const DynOwnershipRuntimeIrVerifierFactKind kind,
    const DynOwnershipRuntimeIrVerifierStage stage,
    const DynOwnershipRuntimeIrVerifierPolicy policy,
    const std::string_view fact_name,
    const std::string_view boundary_fact,
    const std::string_view verifier_guard_fact,
    const std::string_view blocked_surface_fact)
{
    DynOwnershipRuntimeIrVerifierFact fact;
    fact.fact_name = std::string(fact_name);
    fact.kind = kind;
    fact.stage = stage;
    fact.policy = policy;
    fact.verifier_visible = true;
    fact.borrowed_vtable_destructor_free =
        kind == DynOwnershipRuntimeIrVerifierFactKind::borrowed_vtable_destructor_free;
    fact.static_cleanup_only =
        kind == DynOwnershipRuntimeIrVerifierFactKind::static_cleanup_only;
    fact.erased_drop_identity_required =
        kind == DynOwnershipRuntimeIrVerifierFactKind::erased_drop_identity_required;
    fact.allocator_identity_required =
        kind == DynOwnershipRuntimeIrVerifierFactKind::allocator_identity_required;
    fact.owned_dyn_object_placeholder_blocked = true;
    fact.runtime_lowering_blocked = true;
    fact.dynamic_drop_runtime_blocked = true;
    fact.standard_library_blocked = true;
    fact.lowering_runtime_implemented = false;
    fact.observed_value_id = QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_NO_OBSERVED_VALUE;
    fact.subject_symbol = std::string(QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_M19_SYMBOL);
    fact.boundary_fact = std::string(boundary_fact);
    fact.verifier_guard_fact = std::string(verifier_guard_fact);
    fact.blocked_surface_fact = std::string(blocked_surface_fact);
    return fact;
}

[[nodiscard]] bool m19_fact_kinds_are_complete(
    const FunctionDynOwnershipRuntimeIrVerifierFacts& facts) noexcept
{
    std::array<bool, QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_BASELINE_FACT_COUNT> seen{};
    for (const DynOwnershipRuntimeIrVerifierFact& fact : facts.facts) {
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

} // namespace

std::string_view dyn_ownership_runtime_ir_verifier_fact_kind_name(
    const DynOwnershipRuntimeIrVerifierFactKind kind) noexcept
{
    switch (kind) {
        case DynOwnershipRuntimeIrVerifierFactKind::borrowed_vtable_destructor_free:
            return "borrowed_vtable_destructor_free";
        case DynOwnershipRuntimeIrVerifierFactKind::static_cleanup_only:
            return "static_cleanup_only";
        case DynOwnershipRuntimeIrVerifierFactKind::erased_drop_identity_required:
            return "erased_drop_identity_required";
        case DynOwnershipRuntimeIrVerifierFactKind::allocator_identity_required:
            return "allocator_identity_required";
        case DynOwnershipRuntimeIrVerifierFactKind::owned_dyn_object_placeholder_blocked:
            return "owned_dyn_object_placeholder_blocked";
        case DynOwnershipRuntimeIrVerifierFactKind::runtime_lowering_blocked_without_stdlib:
            return "runtime_lowering_blocked_without_stdlib";
    }
    return "invalid";
}

std::string_view dyn_ownership_runtime_ir_verifier_stage_name(
    const DynOwnershipRuntimeIrVerifierStage stage) noexcept
{
    switch (stage) {
        case DynOwnershipRuntimeIrVerifierStage::ir_verifier_preparation:
            return "ir_verifier_preparation";
        case DynOwnershipRuntimeIrVerifierStage::verifier_negative_matrix:
            return "verifier_negative_matrix";
        case DynOwnershipRuntimeIrVerifierStage::blocked_future_runtime:
            return "blocked_future_runtime";
    }
    return "invalid";
}

std::string_view dyn_ownership_runtime_ir_verifier_policy_name(
    const DynOwnershipRuntimeIrVerifierPolicy policy) noexcept
{
    switch (policy) {
        case DynOwnershipRuntimeIrVerifierPolicy::borrowed_vtable_methods_only_v1:
            return "borrowed_vtable_methods_only_v1";
        case DynOwnershipRuntimeIrVerifierPolicy::static_cleanup_marker_only_v1:
            return "static_cleanup_marker_only_v1";
        case DynOwnershipRuntimeIrVerifierPolicy::erased_drop_identity_prerequisite_v1:
            return "erased_drop_identity_prerequisite_v1";
        case DynOwnershipRuntimeIrVerifierPolicy::allocator_identity_prerequisite_v1:
            return "allocator_identity_prerequisite_v1";
        case DynOwnershipRuntimeIrVerifierPolicy::owned_dyn_object_placeholder_not_lowered_v1:
            return "owned_dyn_object_placeholder_not_lowered_v1";
        case DynOwnershipRuntimeIrVerifierPolicy::runtime_lowering_not_implemented_v1:
            return "runtime_lowering_not_implemented_v1";
    }
    return "invalid";
}

bool is_valid(const DynOwnershipRuntimeIrVerifierFactKind kind) noexcept
{
    return kind == DynOwnershipRuntimeIrVerifierFactKind::borrowed_vtable_destructor_free
        || kind == DynOwnershipRuntimeIrVerifierFactKind::static_cleanup_only
        || kind == DynOwnershipRuntimeIrVerifierFactKind::erased_drop_identity_required
        || kind == DynOwnershipRuntimeIrVerifierFactKind::allocator_identity_required
        || kind == DynOwnershipRuntimeIrVerifierFactKind::owned_dyn_object_placeholder_blocked
        || kind == DynOwnershipRuntimeIrVerifierFactKind::runtime_lowering_blocked_without_stdlib;
}

bool is_valid(const DynOwnershipRuntimeIrVerifierStage stage) noexcept
{
    return stage == DynOwnershipRuntimeIrVerifierStage::ir_verifier_preparation
        || stage == DynOwnershipRuntimeIrVerifierStage::verifier_negative_matrix
        || stage == DynOwnershipRuntimeIrVerifierStage::blocked_future_runtime;
}

bool is_valid(const DynOwnershipRuntimeIrVerifierPolicy policy) noexcept
{
    return policy == DynOwnershipRuntimeIrVerifierPolicy::borrowed_vtable_methods_only_v1
        || policy == DynOwnershipRuntimeIrVerifierPolicy::static_cleanup_marker_only_v1
        || policy == DynOwnershipRuntimeIrVerifierPolicy::erased_drop_identity_prerequisite_v1
        || policy == DynOwnershipRuntimeIrVerifierPolicy::allocator_identity_prerequisite_v1
        || policy == DynOwnershipRuntimeIrVerifierPolicy::owned_dyn_object_placeholder_not_lowered_v1
        || policy == DynOwnershipRuntimeIrVerifierPolicy::runtime_lowering_not_implemented_v1;
}

bool is_valid(const DynOwnershipRuntimeIrVerifierFact& fact) noexcept
{
    return is_valid(fact.kind)
        && is_valid(fact.stage)
        && is_valid(fact.policy)
        && fact_payload_is_named(fact)
        && blockers_are_intact(fact)
        && kind_policy_stage_match(fact);
}

bool is_valid(
    const DynOwnershipRuntimeIrVerifierSummary& summary,
    const FunctionDynOwnershipRuntimeIrVerifierFacts& facts) noexcept
{
    return summary_equals(summary, summarize_dyn_ownership_runtime_ir_verifier_counts(facts));
}

bool is_valid(const FunctionDynOwnershipRuntimeIrVerifierFacts& facts) noexcept
{
    return !facts.symbol.empty()
        && !facts.facts.empty()
        && std::all_of(facts.facts.begin(), facts.facts.end(),
            [](const DynOwnershipRuntimeIrVerifierFact& fact) {
                return is_valid(fact);
            })
        && is_valid(facts.summary, facts)
        && (facts.fingerprint == StableFingerprint128{}
            || facts.fingerprint == dyn_ownership_runtime_ir_verifier_facts_fingerprint(facts));
}

bool is_valid_m19_dyn_ownership_runtime_ir_verifier_baseline(
    const FunctionDynOwnershipRuntimeIrVerifierFacts& facts) noexcept
{
    return is_valid(facts)
        && std::string_view(facts.symbol) == QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_M19_SYMBOL
        && facts.summary.fact_count == QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_BASELINE_FACT_COUNT
        && facts.summary.borrowed_vtable_count == 1U
        && facts.summary.destructor_free_vtable_count == 1U
        && facts.summary.static_cleanup_only_count == 1U
        && facts.summary.erased_drop_identity_required_count == 1U
        && facts.summary.allocator_identity_required_count == 1U
        && facts.summary.owned_dyn_object_placeholder_blocked_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_BASELINE_FACT_COUNT
        && facts.summary.runtime_lowering_blocked_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_BASELINE_FACT_COUNT
        && facts.summary.dynamic_drop_runtime_blocked_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_BASELINE_FACT_COUNT
        && facts.summary.standard_library_blocked_count
            == QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_BASELINE_FACT_COUNT
        && facts.summary.lowering_runtime_implemented_count == 0U
        && m19_fact_kinds_are_complete(facts);
}

void record_dyn_ownership_runtime_ir_verifier_fact(
    FunctionDynOwnershipRuntimeIrVerifierFacts& facts,
    DynOwnershipRuntimeIrVerifierFact fact)
{
    facts.facts.push_back(std::move(fact));
    facts.summary = summarize_dyn_ownership_runtime_ir_verifier_counts(facts);
}

DynOwnershipRuntimeIrVerifierSummary summarize_dyn_ownership_runtime_ir_verifier_counts(
    const FunctionDynOwnershipRuntimeIrVerifierFacts& facts) noexcept
{
    DynOwnershipRuntimeIrVerifierSummary summary;
    summary.fact_count = static_cast<base::u64>(facts.facts.size());
    for (const DynOwnershipRuntimeIrVerifierFact& fact : facts.facts) {
        if (fact.kind == DynOwnershipRuntimeIrVerifierFactKind::borrowed_vtable_destructor_free) {
            ++summary.borrowed_vtable_count;
        }
        if (fact.borrowed_vtable_destructor_free) {
            ++summary.destructor_free_vtable_count;
        }
        if (fact.static_cleanup_only) {
            ++summary.static_cleanup_only_count;
        }
        if (fact.erased_drop_identity_required) {
            ++summary.erased_drop_identity_required_count;
        }
        if (fact.allocator_identity_required) {
            ++summary.allocator_identity_required_count;
        }
        if (fact.owned_dyn_object_placeholder_blocked) {
            ++summary.owned_dyn_object_placeholder_blocked_count;
        }
        if (fact.runtime_lowering_blocked) {
            ++summary.runtime_lowering_blocked_count;
        }
        if (fact.dynamic_drop_runtime_blocked) {
            ++summary.dynamic_drop_runtime_blocked_count;
        }
        if (fact.standard_library_blocked) {
            ++summary.standard_library_blocked_count;
        }
        if (fact.lowering_runtime_implemented) {
            ++summary.lowering_runtime_implemented_count;
        }
        summary.cleanup_marker_count += fact.static_cleanup_marker_count;
        summary.blocked_runtime_marker_count += fact.blocked_runtime_marker_count;
    }
    return summary;
}

StableFingerprint128 dyn_ownership_runtime_ir_verifier_facts_fingerprint(
    const FunctionDynOwnershipRuntimeIrVerifierFacts& facts) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_FINGERPRINT_MARKER);
    builder.mix_string(facts.symbol);
    mix_summary(builder, facts.summary);
    builder.mix_u64(static_cast<base::u64>(facts.facts.size()));
    for (const DynOwnershipRuntimeIrVerifierFact& fact : facts.facts) {
        mix_fact(builder, fact);
    }
    return builder.finish();
}

std::string summarize_dyn_ownership_runtime_ir_verifier_facts(
    const FunctionDynOwnershipRuntimeIrVerifierFacts& facts)
{
    std::ostringstream label;
    label << "dyn_ownership_runtime_ir_verifier_facts function="
          << (facts.symbol.empty() ? "<anonymous>" : facts.symbol)
          << " facts=" << facts.summary.fact_count
          << " borrowed_vtables=" << facts.summary.borrowed_vtable_count
          << " destructor_free_vtables=" << facts.summary.destructor_free_vtable_count
          << " static_cleanup_only=" << facts.summary.static_cleanup_only_count
          << " erased_drop_identity_required="
          << facts.summary.erased_drop_identity_required_count
          << " allocator_identity_required=" << facts.summary.allocator_identity_required_count
          << " owned_dyn_object_placeholder_blocked="
          << facts.summary.owned_dyn_object_placeholder_blocked_count
          << " runtime_lowering_blocked=" << facts.summary.runtime_lowering_blocked_count
          << " dynamic_drop_runtime_blocked="
          << facts.summary.dynamic_drop_runtime_blocked_count
          << " standard_library_blocked=" << facts.summary.standard_library_blocked_count
          << " lowering_runtime_implemented="
          << facts.summary.lowering_runtime_implemented_count
          << " cleanup_markers=" << facts.summary.cleanup_marker_count
          << " blocked_runtime_markers=" << facts.summary.blocked_runtime_marker_count;
    if (!facts.facts.empty()) {
        label << " first_fact=" << facts.facts.front().fact_name
              << " policy="
              << dyn_ownership_runtime_ir_verifier_policy_name(facts.facts.front().policy);
    }
    label << " fingerprint="
          << debug_string(dyn_ownership_runtime_ir_verifier_facts_fingerprint(facts));
    return label.str();
}

std::string dump_dyn_ownership_runtime_ir_verifier_facts(
    const FunctionDynOwnershipRuntimeIrVerifierFacts& facts)
{
    std::ostringstream stream;
    stream << "dyn_ownership_runtime_ir_verifier_facts function="
           << (facts.symbol.empty() ? "<anonymous>" : facts.symbol)
           << " facts=" << facts.summary.fact_count
           << " fingerprint="
           << debug_string(dyn_ownership_runtime_ir_verifier_facts_fingerprint(facts))
           << '\n';
    for (base::usize index = 0; index < facts.facts.size(); ++index) {
        const DynOwnershipRuntimeIrVerifierFact& fact = facts.facts[index];
        stream << "  fact #" << index
               << " name=" << fact.fact_name
               << " kind=" << dyn_ownership_runtime_ir_verifier_fact_kind_name(fact.kind)
               << " stage=" << dyn_ownership_runtime_ir_verifier_stage_name(fact.stage)
               << " policy=" << dyn_ownership_runtime_ir_verifier_policy_name(fact.policy)
               << " verifier_visible=" << (fact.verifier_visible ? "yes" : "no")
               << " borrowed_vtable_destructor_free="
               << (fact.borrowed_vtable_destructor_free ? "yes" : "no")
               << " static_cleanup_only=" << (fact.static_cleanup_only ? "yes" : "no")
               << " erased_drop_identity_required="
               << (fact.erased_drop_identity_required ? "yes" : "no")
               << " allocator_identity_required="
               << (fact.allocator_identity_required ? "yes" : "no")
               << " owned_dyn_object_placeholder_blocked="
               << (fact.owned_dyn_object_placeholder_blocked ? "yes" : "no")
               << " runtime_lowering_blocked="
               << (fact.runtime_lowering_blocked ? "yes" : "no")
               << " dynamic_drop_runtime_blocked="
               << (fact.dynamic_drop_runtime_blocked ? "yes" : "no")
               << " standard_library_blocked="
               << (fact.standard_library_blocked ? "yes" : "no")
               << " lowering_runtime_implemented="
               << (fact.lowering_runtime_implemented ? "yes" : "no")
               << " object=" << fact.object_type.global_id
               << " vtable=" << fact.vtable_layout.global_id
               << " value=v" << fact.observed_value_id
               << " cleanup_markers=" << fact.static_cleanup_marker_count
               << " blocked_runtime_markers=" << fact.blocked_runtime_marker_count
               << " boundary=" << fact.boundary_fact
               << " verifier_guard=" << fact.verifier_guard_fact
               << " blocked_surface=" << fact.blocked_surface_fact
               << '\n';
    }
    return stream.str();
}

FunctionDynOwnershipRuntimeIrVerifierFacts m19_dyn_ownership_runtime_ir_verifier_baseline()
{
    FunctionDynOwnershipRuntimeIrVerifierFacts facts;
    facts.symbol = std::string(QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_M19_SYMBOL);
    record_dyn_ownership_runtime_ir_verifier_fact(facts,
        baseline_fact(DynOwnershipRuntimeIrVerifierFactKind::borrowed_vtable_destructor_free,
            DynOwnershipRuntimeIrVerifierStage::ir_verifier_preparation,
            DynOwnershipRuntimeIrVerifierPolicy::borrowed_vtable_methods_only_v1,
            QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_BORROWED_VTABLE_FACT,
            "borrowed_vtable_layout_has_no_destructor_slot",
            "verifier_rejects_borrowed_vtable_destructor_slot",
            "dynamic_drop_runtime_not_in_m19"));
    record_dyn_ownership_runtime_ir_verifier_fact(facts,
        baseline_fact(DynOwnershipRuntimeIrVerifierFactKind::static_cleanup_only,
            DynOwnershipRuntimeIrVerifierStage::verifier_negative_matrix,
            DynOwnershipRuntimeIrVerifierPolicy::static_cleanup_marker_only_v1,
            QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_STATIC_CLEANUP_FACT,
            "drop_markers_remain_static_or_marker_only",
            "verifier_rejects_dynamic_erased_drop_policy",
            "dynamic_drop_dispatch_not_in_m19"));
    record_dyn_ownership_runtime_ir_verifier_fact(facts,
        baseline_fact(DynOwnershipRuntimeIrVerifierFactKind::erased_drop_identity_required,
            DynOwnershipRuntimeIrVerifierStage::blocked_future_runtime,
            DynOwnershipRuntimeIrVerifierPolicy::erased_drop_identity_prerequisite_v1,
            QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_DROP_IDENTITY_FACT,
            "future_erased_drop_identity_required",
            "verifier_requires_identity_before_runtime_lowering",
            "dynamic_drop_runtime_not_in_m19"));
    record_dyn_ownership_runtime_ir_verifier_fact(facts,
        baseline_fact(DynOwnershipRuntimeIrVerifierFactKind::allocator_identity_required,
            DynOwnershipRuntimeIrVerifierStage::blocked_future_runtime,
            DynOwnershipRuntimeIrVerifierPolicy::allocator_identity_prerequisite_v1,
            QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_ALLOCATOR_FACT,
            "future_allocator_identity_required",
            "verifier_requires_allocator_identity_before_owning_dyn",
            "allocator_api_not_in_m19"));
    record_dyn_ownership_runtime_ir_verifier_fact(facts,
        baseline_fact(DynOwnershipRuntimeIrVerifierFactKind::owned_dyn_object_placeholder_blocked,
            DynOwnershipRuntimeIrVerifierStage::blocked_future_runtime,
            DynOwnershipRuntimeIrVerifierPolicy::owned_dyn_object_placeholder_not_lowered_v1,
            QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_OWNED_OBJECT_FACT,
            "future_owned_dyn_object_placeholder_required",
            "verifier_keeps_owned_dyn_user_values_blocked",
            "owning_dyn_user_value_not_in_m19"));
    record_dyn_ownership_runtime_ir_verifier_fact(facts,
        baseline_fact(DynOwnershipRuntimeIrVerifierFactKind::runtime_lowering_blocked_without_stdlib,
            DynOwnershipRuntimeIrVerifierStage::blocked_future_runtime,
            DynOwnershipRuntimeIrVerifierPolicy::runtime_lowering_not_implemented_v1,
            QUERY_DYN_OWNERSHIP_RUNTIME_IR_VERIFIER_RUNTIME_BLOCKER_FACT,
            "runtime_lowering_requires_stdlib_runtime_stage",
            "verifier_rejects_runtime_lowering_without_stdlib",
            "runtime_abi_lowering_not_in_m19"));
    facts.fingerprint = dyn_ownership_runtime_ir_verifier_facts_fingerprint(facts);
    return facts;
}

} // namespace aurex::query
