#pragma once

#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>
#include <aurex/infrastructure/query/trait_object_key.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

enum class DynOwnershipRuntimeIrVerifierFactKind : base::u8 {
    borrowed_vtable_destructor_free = 1,
    static_cleanup_only,
    erased_drop_identity_required,
    allocator_identity_required,
    owned_dyn_object_placeholder_blocked,
    runtime_lowering_blocked_without_stdlib,
};

enum class DynOwnershipRuntimeIrVerifierStage : base::u8 {
    ir_verifier_preparation = 1,
    verifier_negative_matrix,
    blocked_future_runtime,
};

enum class DynOwnershipRuntimeIrVerifierPolicy : base::u8 {
    borrowed_vtable_methods_only_v1 = 1,
    static_cleanup_marker_only_v1,
    erased_drop_identity_prerequisite_v1,
    allocator_identity_prerequisite_v1,
    owned_dyn_object_placeholder_not_lowered_v1,
    runtime_lowering_not_implemented_v1,
};

struct DynOwnershipRuntimeIrVerifierFact {
    std::string fact_name;
    DynOwnershipRuntimeIrVerifierFactKind kind =
        DynOwnershipRuntimeIrVerifierFactKind::borrowed_vtable_destructor_free;
    DynOwnershipRuntimeIrVerifierStage stage =
        DynOwnershipRuntimeIrVerifierStage::ir_verifier_preparation;
    DynOwnershipRuntimeIrVerifierPolicy policy =
        DynOwnershipRuntimeIrVerifierPolicy::borrowed_vtable_methods_only_v1;
    bool verifier_visible = true;
    bool borrowed_vtable_destructor_free = true;
    bool static_cleanup_only = true;
    bool erased_drop_identity_required = false;
    bool allocator_identity_required = false;
    bool owned_dyn_object_placeholder_blocked = true;
    bool runtime_lowering_blocked = true;
    bool dynamic_drop_runtime_blocked = true;
    bool standard_library_blocked = true;
    bool lowering_runtime_implemented = false;
    TraitObjectTypeKey object_type;
    VTableLayoutKey vtable_layout;
    base::u32 observed_value_id = 0;
    base::u32 static_cleanup_marker_count = 0;
    base::u32 blocked_runtime_marker_count = 0;
    std::string subject_symbol;
    std::string boundary_fact;
    std::string verifier_guard_fact;
    std::string blocked_surface_fact;
};

struct DynOwnershipRuntimeIrVerifierSummary {
    base::u64 fact_count = 0;
    base::u64 borrowed_vtable_count = 0;
    base::u64 destructor_free_vtable_count = 0;
    base::u64 static_cleanup_only_count = 0;
    base::u64 erased_drop_identity_required_count = 0;
    base::u64 allocator_identity_required_count = 0;
    base::u64 owned_dyn_object_placeholder_blocked_count = 0;
    base::u64 runtime_lowering_blocked_count = 0;
    base::u64 dynamic_drop_runtime_blocked_count = 0;
    base::u64 standard_library_blocked_count = 0;
    base::u64 lowering_runtime_implemented_count = 0;
    base::u64 cleanup_marker_count = 0;
    base::u64 blocked_runtime_marker_count = 0;
};

struct FunctionDynOwnershipRuntimeIrVerifierFacts {
    std::string symbol;
    std::vector<DynOwnershipRuntimeIrVerifierFact> facts;
    DynOwnershipRuntimeIrVerifierSummary summary;
    StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view dyn_ownership_runtime_ir_verifier_fact_kind_name(
    DynOwnershipRuntimeIrVerifierFactKind kind) noexcept;
[[nodiscard]] std::string_view dyn_ownership_runtime_ir_verifier_stage_name(
    DynOwnershipRuntimeIrVerifierStage stage) noexcept;
[[nodiscard]] std::string_view dyn_ownership_runtime_ir_verifier_policy_name(
    DynOwnershipRuntimeIrVerifierPolicy policy) noexcept;

[[nodiscard]] bool is_valid(DynOwnershipRuntimeIrVerifierFactKind kind) noexcept;
[[nodiscard]] bool is_valid(DynOwnershipRuntimeIrVerifierStage stage) noexcept;
[[nodiscard]] bool is_valid(DynOwnershipRuntimeIrVerifierPolicy policy) noexcept;
[[nodiscard]] bool is_valid(const DynOwnershipRuntimeIrVerifierFact& fact) noexcept;
[[nodiscard]] bool is_valid(
    const DynOwnershipRuntimeIrVerifierSummary& summary,
    const FunctionDynOwnershipRuntimeIrVerifierFacts& facts) noexcept;
[[nodiscard]] bool is_valid(const FunctionDynOwnershipRuntimeIrVerifierFacts& facts) noexcept;
[[nodiscard]] bool is_valid_m19_dyn_ownership_runtime_ir_verifier_baseline(
    const FunctionDynOwnershipRuntimeIrVerifierFacts& facts) noexcept;

void record_dyn_ownership_runtime_ir_verifier_fact(
    FunctionDynOwnershipRuntimeIrVerifierFacts& facts,
    DynOwnershipRuntimeIrVerifierFact fact);

[[nodiscard]] DynOwnershipRuntimeIrVerifierSummary
summarize_dyn_ownership_runtime_ir_verifier_counts(
    const FunctionDynOwnershipRuntimeIrVerifierFacts& facts) noexcept;
[[nodiscard]] StableFingerprint128 dyn_ownership_runtime_ir_verifier_facts_fingerprint(
    const FunctionDynOwnershipRuntimeIrVerifierFacts& facts) noexcept;
[[nodiscard]] std::string summarize_dyn_ownership_runtime_ir_verifier_facts(
    const FunctionDynOwnershipRuntimeIrVerifierFacts& facts);
[[nodiscard]] std::string dump_dyn_ownership_runtime_ir_verifier_facts(
    const FunctionDynOwnershipRuntimeIrVerifierFacts& facts);
[[nodiscard]] FunctionDynOwnershipRuntimeIrVerifierFacts
m19_dyn_ownership_runtime_ir_verifier_baseline();

} // namespace aurex::query
