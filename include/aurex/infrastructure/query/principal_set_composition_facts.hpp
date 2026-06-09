#pragma once

#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/canonical_type_key.hpp>
#include <aurex/infrastructure/query/dyn_abi_facts.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>
#include <aurex/infrastructure/query/trait_object_key.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

enum class PrincipalSetMetadataPolicy : base::u8 {
    principal_set_metadata_v1 = 1,
};

enum class PrincipalMethodNamespaceStatus : base::u8 {
    unique_principal_method = 1,
    ambiguous_requires_principal,
};

enum class PrincipalAssociatedEqualityMergeStatus : base::u8 {
    satisfied = 1,
    conflict,
    unconstrained,
};

enum class PrincipalSetProjectionKind : base::u8 {
    concrete_to_composition = 1,
    composition_to_principal,
    composition_to_supertrait,
};

enum class BorrowedDynViewPathUse : base::u8 {
    explicit_projection = 1,
    expected_type_projection,
    method_dispatch,
};

struct PrincipalSetPrincipalDescriptor {
    TraitObjectTypeKey object_type;
    std::string principal_name;
    std::string object_type_name;
};

struct PrincipalSetIdentityFact {
    StableFingerprint128 principal_set_identity;
    StableFingerprint128 object_origin;
    PrincipalSetMetadataPolicy metadata_policy = PrincipalSetMetadataPolicy::principal_set_metadata_v1;
    std::vector<PrincipalSetPrincipalDescriptor> principals;
};

struct CompositionWitnessDescriptor {
    TraitObjectTypeKey principal_object;
    VTableLayoutKey vtable_layout;
    StableFingerprint128 witness_fingerprint;
    std::string principal_name;
    std::string concrete_type_name;
};

struct CompositionWitnessSetFact {
    StableFingerprint128 principal_set_identity;
    PrincipalSetMetadataPolicy metadata_policy = PrincipalSetMetadataPolicy::principal_set_metadata_v1;
    std::vector<CompositionWitnessDescriptor> witnesses;
};

struct PrincipalMethodNamespaceEntry {
    TraitObjectTypeKey principal_object;
    std::string principal_name;
    std::string method_name;
    base::u32 slot = 0;
    PrincipalMethodNamespaceStatus status = PrincipalMethodNamespaceStatus::unique_principal_method;
};

struct PrincipalMethodNamespaceFact {
    StableFingerprint128 principal_set_identity;
    std::vector<PrincipalMethodNamespaceEntry> methods;
};

struct AssociatedEqualityMergeFact {
    StableFingerprint128 principal_set_identity;
    MemberKey associated_type;
    CanonicalTypeKey merged_type;
    PrincipalAssociatedEqualityMergeStatus status =
        PrincipalAssociatedEqualityMergeStatus::satisfied;
    std::vector<TraitObjectTypeKey> contributing_principals;
    std::string associated_type_name;
    std::string merged_type_name;
};

struct CompositionProjectionFact {
    StableFingerprint128 principal_set_identity;
    PrincipalSetProjectionKind kind = PrincipalSetProjectionKind::concrete_to_composition;
    CanonicalTypeKey concrete_type;
    TraitObjectTypeKey source_principal;
    TraitObjectTypeKey target_object;
    StableFingerprint128 projection_path;
    DynBorrowKind borrow_kind = DynBorrowKind::shared;
    bool data_pointer_preserved = true;
    bool origin_preserved = true;
    std::string source_view_name;
    std::string target_view_name;
};

struct BorrowedDynViewPathFact {
    StableFingerprint128 principal_set_identity;
    TraitObjectTypeKey source_principal;
    TraitObjectTypeKey target_object;
    StableFingerprint128 projection_path;
    StableFingerprint128 supertrait_edge_path;
    DynBorrowKind borrow_kind = DynBorrowKind::shared;
    BorrowedDynViewPathUse use = BorrowedDynViewPathUse::explicit_projection;
    bool composition_project_step = true;
    bool supertrait_upcast_step = true;
    bool vtable_dispatch_step = false;
    std::string method_name;
    std::string source_view_name;
    std::string projected_view_name;
    std::string target_view_name;
};

struct PrincipalSetCompositionSummary {
    base::u64 principal_set_count = 0;
    base::u64 principal_count = 0;
    base::u64 witness_set_count = 0;
    base::u64 witness_count = 0;
    base::u64 method_namespace_count = 0;
    base::u64 method_count = 0;
    base::u64 associated_equality_merge_count = 0;
    base::u64 associated_equality_conflict_count = 0;
    base::u64 projection_count = 0;
    base::u64 supertrait_projection_count = 0;
    base::u64 borrowed_view_path_count = 0;
    base::u64 borrowed_view_path_dispatch_count = 0;
    base::u64 borrowed_view_path_expected_projection_count = 0;
    base::u64 shared_borrow_projection_count = 0;
    base::u64 mut_borrow_projection_count = 0;
};

struct PrincipalSetCompositionFacts {
    std::string subject;
    std::vector<PrincipalSetIdentityFact> identity_facts;
    std::vector<CompositionWitnessSetFact> witness_sets;
    std::vector<PrincipalMethodNamespaceFact> method_namespaces;
    std::vector<AssociatedEqualityMergeFact> associated_equality_merges;
    std::vector<CompositionProjectionFact> projections;
    std::vector<BorrowedDynViewPathFact> borrowed_view_paths;
    PrincipalSetCompositionSummary summary;
    StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view principal_set_metadata_policy_name(
    PrincipalSetMetadataPolicy policy) noexcept;
[[nodiscard]] std::string_view principal_method_namespace_status_name(
    PrincipalMethodNamespaceStatus status) noexcept;
[[nodiscard]] std::string_view principal_associated_equality_merge_status_name(
    PrincipalAssociatedEqualityMergeStatus status) noexcept;
[[nodiscard]] std::string_view principal_set_projection_kind_name(
    PrincipalSetProjectionKind kind) noexcept;
[[nodiscard]] std::string_view borrowed_dyn_view_path_use_name(
    BorrowedDynViewPathUse use) noexcept;

[[nodiscard]] bool is_valid(PrincipalSetMetadataPolicy policy) noexcept;
[[nodiscard]] bool is_valid(PrincipalMethodNamespaceStatus status) noexcept;
[[nodiscard]] bool is_valid(PrincipalAssociatedEqualityMergeStatus status) noexcept;
[[nodiscard]] bool is_valid(PrincipalSetProjectionKind kind) noexcept;
[[nodiscard]] bool is_valid(BorrowedDynViewPathUse use) noexcept;
[[nodiscard]] bool is_valid(const PrincipalSetPrincipalDescriptor& descriptor) noexcept;
[[nodiscard]] bool is_valid(const PrincipalSetIdentityFact& fact) noexcept;
[[nodiscard]] bool is_valid(const CompositionWitnessDescriptor& descriptor) noexcept;
[[nodiscard]] bool is_valid(const CompositionWitnessSetFact& fact) noexcept;
[[nodiscard]] bool is_valid(const PrincipalMethodNamespaceEntry& entry) noexcept;
[[nodiscard]] bool is_valid(const PrincipalMethodNamespaceFact& fact) noexcept;
[[nodiscard]] bool is_valid(const AssociatedEqualityMergeFact& fact) noexcept;
[[nodiscard]] bool is_valid(const CompositionProjectionFact& fact) noexcept;
[[nodiscard]] bool is_valid(const BorrowedDynViewPathFact& fact) noexcept;
[[nodiscard]] bool is_valid(const PrincipalSetCompositionSummary& summary,
    const PrincipalSetCompositionFacts& facts) noexcept;
[[nodiscard]] bool is_valid(const PrincipalSetCompositionFacts& facts) noexcept;

[[nodiscard]] PrincipalSetIdentityFact principal_set_identity_fact(
    std::span<const PrincipalSetPrincipalDescriptor> principals);

void record_principal_set_identity_fact(
    PrincipalSetCompositionFacts& facts, PrincipalSetIdentityFact fact);
void record_composition_witness_set_fact(
    PrincipalSetCompositionFacts& facts, CompositionWitnessSetFact fact);
void record_principal_method_namespace_fact(
    PrincipalSetCompositionFacts& facts, PrincipalMethodNamespaceFact fact);
void record_associated_equality_merge_fact(
    PrincipalSetCompositionFacts& facts, AssociatedEqualityMergeFact fact);
void record_composition_projection_fact(
    PrincipalSetCompositionFacts& facts, CompositionProjectionFact fact);
void record_borrowed_dyn_view_path_fact(
    PrincipalSetCompositionFacts& facts, BorrowedDynViewPathFact fact);

[[nodiscard]] PrincipalSetCompositionSummary summarize_principal_set_composition_counts(
    const PrincipalSetCompositionFacts& facts) noexcept;
[[nodiscard]] StableFingerprint128 principal_set_composition_facts_fingerprint(
    const PrincipalSetCompositionFacts& facts) noexcept;
[[nodiscard]] std::string summarize_principal_set_composition_facts(
    const PrincipalSetCompositionFacts& facts);
[[nodiscard]] std::string dump_principal_set_composition_facts(
    const PrincipalSetCompositionFacts& facts);

} // namespace aurex::query
