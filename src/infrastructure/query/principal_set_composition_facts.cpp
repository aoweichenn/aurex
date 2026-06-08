#include <aurex/infrastructure/query/principal_set_composition_facts.hpp>

#include <algorithm>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_PRINCIPAL_SET_COMPOSITION_FINGERPRINT_MARKER =
    "query.principal_set_composition_facts.v1";
constexpr base::usize QUERY_PRINCIPAL_SET_MIN_PRINCIPAL_COUNT = 2;
constexpr base::u8 QUERY_PRINCIPAL_SET_INVALID_METADATA_POLICY_VALUE = 255U;
constexpr base::u8 QUERY_PRINCIPAL_SET_INVALID_METHOD_STATUS_VALUE = 255U;
constexpr base::u8 QUERY_PRINCIPAL_SET_INVALID_ASSOCIATED_STATUS_VALUE = 255U;
constexpr base::u8 QUERY_PRINCIPAL_SET_INVALID_PROJECTION_KIND_VALUE = 255U;
constexpr base::u8 QUERY_PRINCIPAL_SET_INVALID_BORROW_KIND_VALUE = 255U;

[[nodiscard]] bool is_nonempty_fingerprint(const StableFingerprint128 fingerprint) noexcept
{
    return fingerprint.byte_count != 0;
}

[[nodiscard]] bool is_associated_type_member_key(const MemberKey key) noexcept
{
    return is_valid(key) && key.kind == MemberKind::associated_type;
}

[[nodiscard]] base::u8 stable_metadata_policy_value(const PrincipalSetMetadataPolicy policy) noexcept
{
    return is_valid(policy) ? static_cast<base::u8>(policy)
                            : QUERY_PRINCIPAL_SET_INVALID_METADATA_POLICY_VALUE;
}

[[nodiscard]] base::u8 stable_method_status_value(
    const PrincipalMethodNamespaceStatus status) noexcept
{
    return is_valid(status) ? static_cast<base::u8>(status)
                            : QUERY_PRINCIPAL_SET_INVALID_METHOD_STATUS_VALUE;
}

[[nodiscard]] base::u8 stable_associated_status_value(
    const PrincipalAssociatedEqualityMergeStatus status) noexcept
{
    return is_valid(status) ? static_cast<base::u8>(status)
                            : QUERY_PRINCIPAL_SET_INVALID_ASSOCIATED_STATUS_VALUE;
}

[[nodiscard]] base::u8 stable_projection_kind_value(const PrincipalSetProjectionKind kind) noexcept
{
    return is_valid(kind) ? static_cast<base::u8>(kind)
                          : QUERY_PRINCIPAL_SET_INVALID_PROJECTION_KIND_VALUE;
}

[[nodiscard]] base::u8 stable_borrow_kind_value(const DynBorrowKind kind) noexcept
{
    return is_valid(kind) ? static_cast<base::u8>(kind)
                          : QUERY_PRINCIPAL_SET_INVALID_BORROW_KIND_VALUE;
}

[[nodiscard]] bool principal_descriptor_less(
    const PrincipalSetPrincipalDescriptor& lhs,
    const PrincipalSetPrincipalDescriptor& rhs) noexcept
{
    if (lhs.object_type.principal_trait.global_id != rhs.object_type.principal_trait.global_id) {
        return lhs.object_type.principal_trait.global_id < rhs.object_type.principal_trait.global_id;
    }
    return lhs.object_type.global_id < rhs.object_type.global_id;
}

[[nodiscard]] bool trait_object_key_less(
    const TraitObjectTypeKey& lhs,
    const TraitObjectTypeKey& rhs) noexcept
{
    if (lhs.principal_trait.global_id != rhs.principal_trait.global_id) {
        return lhs.principal_trait.global_id < rhs.principal_trait.global_id;
    }
    return lhs.global_id < rhs.global_id;
}

void normalize_principals(std::vector<PrincipalSetPrincipalDescriptor>& principals)
{
    std::sort(principals.begin(), principals.end(), principal_descriptor_less);
}

[[nodiscard]] bool principal_descriptors_are_canonical_unique(
    const std::vector<PrincipalSetPrincipalDescriptor>& principals) noexcept
{
    if (principals.size() < QUERY_PRINCIPAL_SET_MIN_PRINCIPAL_COUNT) {
        return false;
    }

    base::u64 previous_principal_global_id = 0;
    bool has_previous = false;
    for (base::usize index = 0; index < principals.size(); ++index) {
        const PrincipalSetPrincipalDescriptor& principal = principals[index];
        if (!is_valid(principal)) {
            return false;
        }
        if (has_previous) {
            if (!principal_descriptor_less(principals[index - 1U], principal)
                || principal.object_type.principal_trait.global_id <= previous_principal_global_id) {
                return false;
            }
        }
        previous_principal_global_id = principal.object_type.principal_trait.global_id;
        has_previous = true;
    }
    return true;
}

[[nodiscard]] bool trait_object_keys_are_valid_unique_sorted(
    const std::vector<TraitObjectTypeKey>& principals) noexcept
{
    bool has_previous = false;
    for (base::usize index = 0; index < principals.size(); ++index) {
        const TraitObjectTypeKey& principal = principals[index];
        if (!is_valid(principal)) {
            return false;
        }
        if (has_previous && !trait_object_key_less(principals[index - 1U], principal)) {
            return false;
        }
        has_previous = true;
    }
    return true;
}

[[nodiscard]] bool witnesses_are_valid_unique_sorted(
    const std::vector<CompositionWitnessDescriptor>& witnesses) noexcept
{
    bool has_previous = false;
    for (base::usize index = 0; index < witnesses.size(); ++index) {
        const CompositionWitnessDescriptor& witness = witnesses[index];
        if (!is_valid(witness)) {
            return false;
        }
        if (has_previous && !trait_object_key_less(witnesses[index - 1U].principal_object, witness.principal_object)) {
            return false;
        }
        has_previous = true;
    }
    return true;
}

[[nodiscard]] bool method_entries_have_principal_namespace(
    const std::vector<PrincipalMethodNamespaceEntry>& methods) noexcept
{
    for (const PrincipalMethodNamespaceEntry& method : methods) {
        if (!is_valid(method)) {
            return false;
        }
    }

    for (base::usize left_index = 0; left_index < methods.size(); ++left_index) {
        const PrincipalMethodNamespaceEntry& left = methods[left_index];
        for (base::usize right_index = left_index + 1U; right_index < methods.size(); ++right_index) {
            const PrincipalMethodNamespaceEntry& right = methods[right_index];
            const bool same_name = left.method_name == right.method_name;
            const bool distinct_principal = left.principal_object.global_id != right.principal_object.global_id;
            if (same_name && !distinct_principal) {
                return false;
            }
            if (same_name && distinct_principal
                && (left.status != PrincipalMethodNamespaceStatus::ambiguous_requires_principal
                    || right.status != PrincipalMethodNamespaceStatus::ambiguous_requires_principal)) {
                return false;
            }
        }
    }
    return true;
}

void count_projection_borrow_kind(
    PrincipalSetCompositionSummary& summary, const DynBorrowKind borrow_kind) noexcept
{
    switch (borrow_kind) {
        case DynBorrowKind::shared:
            ++summary.shared_borrow_projection_count;
            break;
        case DynBorrowKind::mut:
            ++summary.mut_borrow_projection_count;
            break;
    }
}

void mix_principal_descriptor(
    StableHashBuilder& builder, const PrincipalSetPrincipalDescriptor& descriptor) noexcept
{
    builder.mix_u64(descriptor.object_type.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(descriptor.object_type));
    builder.mix_string(descriptor.principal_name);
    builder.mix_string(descriptor.object_type_name);
}

void mix_principal_identity_descriptor(
    StableHashBuilder& builder, const PrincipalSetPrincipalDescriptor& descriptor) noexcept
{
    builder.mix_u64(descriptor.object_type.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(descriptor.object_type));
}

void mix_identity_fact(StableHashBuilder& builder, const PrincipalSetIdentityFact& fact) noexcept
{
    builder.mix_fingerprint(fact.principal_set_identity);
    builder.mix_fingerprint(fact.object_origin);
    builder.mix_u8(stable_metadata_policy_value(fact.metadata_policy));
    builder.mix_u64(static_cast<base::u64>(fact.principals.size()));
    for (const PrincipalSetPrincipalDescriptor& principal : fact.principals) {
        mix_principal_descriptor(builder, principal);
    }
}

void mix_witness_descriptor(
    StableHashBuilder& builder, const CompositionWitnessDescriptor& descriptor) noexcept
{
    builder.mix_u64(descriptor.principal_object.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(descriptor.principal_object));
    builder.mix_u64(descriptor.vtable_layout.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(descriptor.vtable_layout));
    builder.mix_fingerprint(descriptor.witness_fingerprint);
    builder.mix_string(descriptor.principal_name);
    builder.mix_string(descriptor.concrete_type_name);
}

void mix_witness_set_fact(StableHashBuilder& builder, const CompositionWitnessSetFact& fact) noexcept
{
    builder.mix_fingerprint(fact.principal_set_identity);
    builder.mix_u8(stable_metadata_policy_value(fact.metadata_policy));
    builder.mix_u64(static_cast<base::u64>(fact.witnesses.size()));
    for (const CompositionWitnessDescriptor& witness : fact.witnesses) {
        mix_witness_descriptor(builder, witness);
    }
}

void mix_method_entry(StableHashBuilder& builder, const PrincipalMethodNamespaceEntry& entry) noexcept
{
    builder.mix_u64(entry.principal_object.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(entry.principal_object));
    builder.mix_string(entry.principal_name);
    builder.mix_string(entry.method_name);
    builder.mix_u32(entry.slot);
    builder.mix_u8(stable_method_status_value(entry.status));
}

void mix_method_namespace_fact(
    StableHashBuilder& builder, const PrincipalMethodNamespaceFact& fact) noexcept
{
    builder.mix_fingerprint(fact.principal_set_identity);
    builder.mix_u64(static_cast<base::u64>(fact.methods.size()));
    for (const PrincipalMethodNamespaceEntry& method : fact.methods) {
        mix_method_entry(builder, method);
    }
}

void mix_associated_equality_merge_fact(
    StableHashBuilder& builder, const AssociatedEqualityMergeFact& fact) noexcept
{
    builder.mix_fingerprint(fact.principal_set_identity);
    builder.mix_u64(fact.associated_type.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(fact.associated_type));
    builder.mix_fingerprint(stable_key_fingerprint(fact.merged_type));
    builder.mix_u8(stable_associated_status_value(fact.status));
    builder.mix_u64(static_cast<base::u64>(fact.contributing_principals.size()));
    for (const TraitObjectTypeKey& principal : fact.contributing_principals) {
        builder.mix_u64(principal.global_id);
        builder.mix_fingerprint(stable_key_fingerprint(principal));
    }
    builder.mix_string(fact.associated_type_name);
    builder.mix_string(fact.merged_type_name);
}

void mix_projection_fact(StableHashBuilder& builder, const CompositionProjectionFact& fact) noexcept
{
    builder.mix_fingerprint(fact.principal_set_identity);
    builder.mix_u8(stable_projection_kind_value(fact.kind));
    builder.mix_fingerprint(stable_key_fingerprint(fact.concrete_type));
    builder.mix_u64(fact.source_principal.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(fact.source_principal));
    builder.mix_u64(fact.target_object.global_id);
    builder.mix_fingerprint(stable_key_fingerprint(fact.target_object));
    builder.mix_fingerprint(fact.projection_path);
    builder.mix_u8(stable_borrow_kind_value(fact.borrow_kind));
    builder.mix_bool(fact.data_pointer_preserved);
    builder.mix_bool(fact.origin_preserved);
    builder.mix_string(fact.source_view_name);
    builder.mix_string(fact.target_view_name);
}

void mix_summary(StableHashBuilder& builder, const PrincipalSetCompositionSummary& summary) noexcept
{
    builder.mix_u64(summary.principal_set_count);
    builder.mix_u64(summary.principal_count);
    builder.mix_u64(summary.witness_set_count);
    builder.mix_u64(summary.witness_count);
    builder.mix_u64(summary.method_namespace_count);
    builder.mix_u64(summary.method_count);
    builder.mix_u64(summary.associated_equality_merge_count);
    builder.mix_u64(summary.associated_equality_conflict_count);
    builder.mix_u64(summary.projection_count);
    builder.mix_u64(summary.supertrait_projection_count);
    builder.mix_u64(summary.shared_borrow_projection_count);
    builder.mix_u64(summary.mut_borrow_projection_count);
}

[[nodiscard]] bool has_identity_fact_for(
    const PrincipalSetCompositionFacts& facts, const StableFingerprint128 identity) noexcept
{
    return std::any_of(facts.identity_facts.begin(), facts.identity_facts.end(),
        [identity](const PrincipalSetIdentityFact& fact) {
            return fact.principal_set_identity == identity;
        });
}

[[nodiscard]] bool all_fact_identities_are_known(const PrincipalSetCompositionFacts& facts) noexcept
{
    for (const CompositionWitnessSetFact& witness_set : facts.witness_sets) {
        if (!has_identity_fact_for(facts, witness_set.principal_set_identity)) {
            return false;
        }
    }
    for (const PrincipalMethodNamespaceFact& method_namespace : facts.method_namespaces) {
        if (!has_identity_fact_for(facts, method_namespace.principal_set_identity)) {
            return false;
        }
    }
    for (const AssociatedEqualityMergeFact& merge : facts.associated_equality_merges) {
        if (!has_identity_fact_for(facts, merge.principal_set_identity)) {
            return false;
        }
    }
    for (const CompositionProjectionFact& projection : facts.projections) {
        if (!has_identity_fact_for(facts, projection.principal_set_identity)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] const PrincipalSetIdentityFact* identity_fact_for(
    const PrincipalSetCompositionFacts& facts, const StableFingerprint128 identity) noexcept
{
    for (const PrincipalSetIdentityFact& fact : facts.identity_facts) {
        if (fact.principal_set_identity == identity) {
            return &fact;
        }
    }
    return nullptr;
}

[[nodiscard]] bool identity_contains_principal(
    const PrincipalSetIdentityFact& identity, const TraitObjectTypeKey& principal) noexcept
{
    return std::any_of(identity.principals.begin(), identity.principals.end(),
        [&principal](const PrincipalSetPrincipalDescriptor& descriptor) {
            return descriptor.object_type == principal;
        });
}

[[nodiscard]] bool witness_set_matches_identity(
    const PrincipalSetCompositionFacts& facts, const CompositionWitnessSetFact& witness_set) noexcept
{
    const PrincipalSetIdentityFact* identity = identity_fact_for(facts, witness_set.principal_set_identity);
    if (identity == nullptr || witness_set.witnesses.size() != identity->principals.size()) {
        return false;
    }
    return std::all_of(witness_set.witnesses.begin(), witness_set.witnesses.end(),
        [identity](const CompositionWitnessDescriptor& witness) {
            return identity_contains_principal(*identity, witness.principal_object);
        });
}

[[nodiscard]] bool method_namespace_matches_identity(
    const PrincipalSetCompositionFacts& facts, const PrincipalMethodNamespaceFact& method_namespace) noexcept
{
    const PrincipalSetIdentityFact* identity = identity_fact_for(facts, method_namespace.principal_set_identity);
    if (identity == nullptr) {
        return false;
    }
    return std::all_of(method_namespace.methods.begin(), method_namespace.methods.end(),
        [identity](const PrincipalMethodNamespaceEntry& method) {
            return identity_contains_principal(*identity, method.principal_object);
        });
}

[[nodiscard]] bool associated_merge_matches_identity(
    const PrincipalSetCompositionFacts& facts, const AssociatedEqualityMergeFact& merge) noexcept
{
    const PrincipalSetIdentityFact* identity = identity_fact_for(facts, merge.principal_set_identity);
    if (identity == nullptr) {
        return false;
    }
    return std::all_of(merge.contributing_principals.begin(), merge.contributing_principals.end(),
        [identity](const TraitObjectTypeKey& principal) {
            return identity_contains_principal(*identity, principal);
        });
}

[[nodiscard]] bool projection_matches_identity(
    const PrincipalSetCompositionFacts& facts, const CompositionProjectionFact& projection) noexcept
{
    const PrincipalSetIdentityFact* identity = identity_fact_for(facts, projection.principal_set_identity);
    if (identity == nullptr || !identity_contains_principal(*identity, projection.source_principal)) {
        return false;
    }
    if (projection.kind == PrincipalSetProjectionKind::composition_to_supertrait) {
        return true;
    }
    return identity_contains_principal(*identity, projection.target_object);
}

[[nodiscard]] bool fact_payloads_match_identities(const PrincipalSetCompositionFacts& facts) noexcept
{
    for (const CompositionWitnessSetFact& witness_set : facts.witness_sets) {
        if (!witness_set_matches_identity(facts, witness_set)) {
            return false;
        }
    }
    for (const PrincipalMethodNamespaceFact& method_namespace : facts.method_namespaces) {
        if (!method_namespace_matches_identity(facts, method_namespace)) {
            return false;
        }
    }
    for (const AssociatedEqualityMergeFact& merge : facts.associated_equality_merges) {
        if (!associated_merge_matches_identity(facts, merge)) {
            return false;
        }
    }
    for (const CompositionProjectionFact& projection : facts.projections) {
        if (!projection_matches_identity(facts, projection)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool fact_identities_are_unique(
    const std::vector<PrincipalSetIdentityFact>& identities) noexcept
{
    for (base::usize left_index = 0; left_index < identities.size(); ++left_index) {
        for (base::usize right_index = left_index + 1U; right_index < identities.size(); ++right_index) {
            if (identities[left_index].principal_set_identity
                == identities[right_index].principal_set_identity) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool summary_equals(
    const PrincipalSetCompositionSummary& lhs,
    const PrincipalSetCompositionSummary& rhs) noexcept
{
    return lhs.principal_set_count == rhs.principal_set_count
        && lhs.principal_count == rhs.principal_count
        && lhs.witness_set_count == rhs.witness_set_count
        && lhs.witness_count == rhs.witness_count
        && lhs.method_namespace_count == rhs.method_namespace_count
        && lhs.method_count == rhs.method_count
        && lhs.associated_equality_merge_count == rhs.associated_equality_merge_count
        && lhs.associated_equality_conflict_count == rhs.associated_equality_conflict_count
        && lhs.projection_count == rhs.projection_count
        && lhs.supertrait_projection_count == rhs.supertrait_projection_count
        && lhs.shared_borrow_projection_count == rhs.shared_borrow_projection_count
        && lhs.mut_borrow_projection_count == rhs.mut_borrow_projection_count;
}

[[nodiscard]] const char* fallback_name(const std::string& value, const char* fallback) noexcept
{
    return value.empty() ? fallback : value.c_str();
}

} // namespace

std::string_view principal_set_metadata_policy_name(const PrincipalSetMetadataPolicy policy) noexcept
{
    switch (policy) {
        case PrincipalSetMetadataPolicy::principal_set_metadata_v1:
            return "principal_set_metadata_v1";
    }
    return "invalid";
}

std::string_view principal_method_namespace_status_name(
    const PrincipalMethodNamespaceStatus status) noexcept
{
    switch (status) {
        case PrincipalMethodNamespaceStatus::unique_principal_method:
            return "unique_principal_method";
        case PrincipalMethodNamespaceStatus::ambiguous_requires_principal:
            return "ambiguous_requires_principal";
    }
    return "invalid";
}

std::string_view principal_associated_equality_merge_status_name(
    const PrincipalAssociatedEqualityMergeStatus status) noexcept
{
    switch (status) {
        case PrincipalAssociatedEqualityMergeStatus::satisfied:
            return "satisfied";
        case PrincipalAssociatedEqualityMergeStatus::conflict:
            return "conflict";
        case PrincipalAssociatedEqualityMergeStatus::unconstrained:
            return "unconstrained";
    }
    return "invalid";
}

std::string_view principal_set_projection_kind_name(const PrincipalSetProjectionKind kind) noexcept
{
    switch (kind) {
        case PrincipalSetProjectionKind::concrete_to_composition:
            return "concrete_to_composition";
        case PrincipalSetProjectionKind::composition_to_principal:
            return "composition_to_principal";
        case PrincipalSetProjectionKind::composition_to_supertrait:
            return "composition_to_supertrait";
    }
    return "invalid";
}

bool is_valid(const PrincipalSetMetadataPolicy policy) noexcept
{
    return policy == PrincipalSetMetadataPolicy::principal_set_metadata_v1;
}

bool is_valid(const PrincipalMethodNamespaceStatus status) noexcept
{
    return status == PrincipalMethodNamespaceStatus::unique_principal_method
        || status == PrincipalMethodNamespaceStatus::ambiguous_requires_principal;
}

bool is_valid(const PrincipalAssociatedEqualityMergeStatus status) noexcept
{
    return status == PrincipalAssociatedEqualityMergeStatus::satisfied
        || status == PrincipalAssociatedEqualityMergeStatus::conflict
        || status == PrincipalAssociatedEqualityMergeStatus::unconstrained;
}

bool is_valid(const PrincipalSetProjectionKind kind) noexcept
{
    return kind == PrincipalSetProjectionKind::concrete_to_composition
        || kind == PrincipalSetProjectionKind::composition_to_principal
        || kind == PrincipalSetProjectionKind::composition_to_supertrait;
}

bool is_valid(const PrincipalSetPrincipalDescriptor& descriptor) noexcept
{
    return is_valid(descriptor.object_type)
        && descriptor.object_type.abi_policy == TraitObjectAbiPolicyKey::borrowed_view_v1;
}

bool is_valid(const PrincipalSetIdentityFact& fact) noexcept
{
    return is_nonempty_fingerprint(fact.principal_set_identity)
        && is_nonempty_fingerprint(fact.object_origin)
        && is_valid(fact.metadata_policy)
        && principal_descriptors_are_canonical_unique(fact.principals);
}

bool is_valid(const CompositionWitnessDescriptor& descriptor) noexcept
{
    return is_valid(descriptor.principal_object) && is_valid(descriptor.vtable_layout)
        && descriptor.vtable_layout.object_type == descriptor.principal_object
        && is_nonempty_fingerprint(descriptor.witness_fingerprint);
}

bool is_valid(const CompositionWitnessSetFact& fact) noexcept
{
    return is_nonempty_fingerprint(fact.principal_set_identity)
        && is_valid(fact.metadata_policy)
        && fact.witnesses.size() >= QUERY_PRINCIPAL_SET_MIN_PRINCIPAL_COUNT
        && witnesses_are_valid_unique_sorted(fact.witnesses);
}

bool is_valid(const PrincipalMethodNamespaceEntry& entry) noexcept
{
    return is_valid(entry.principal_object) && !entry.method_name.empty() && is_valid(entry.status);
}

bool is_valid(const PrincipalMethodNamespaceFact& fact) noexcept
{
    return is_nonempty_fingerprint(fact.principal_set_identity)
        && !fact.methods.empty()
        && method_entries_have_principal_namespace(fact.methods);
}

bool is_valid(const AssociatedEqualityMergeFact& fact) noexcept
{
    const bool merged_type_required =
        fact.status != PrincipalAssociatedEqualityMergeStatus::unconstrained;
    const bool merged_type_valid = merged_type_required ? is_valid(fact.merged_type)
                                                        : fact.merged_type.kind == CanonicalTypeKind::invalid;
    return is_nonempty_fingerprint(fact.principal_set_identity)
        && is_associated_type_member_key(fact.associated_type)
        && is_valid(fact.status)
        && merged_type_valid
        && !fact.contributing_principals.empty()
        && trait_object_keys_are_valid_unique_sorted(fact.contributing_principals);
}

bool is_valid(const CompositionProjectionFact& fact) noexcept
{
    if (!is_nonempty_fingerprint(fact.principal_set_identity) || !is_valid(fact.kind)
        || !is_valid(fact.concrete_type) || !is_valid(fact.source_principal)
        || !is_valid(fact.target_object) || !is_nonempty_fingerprint(fact.projection_path)
        || !is_valid(fact.borrow_kind) || !fact.data_pointer_preserved || !fact.origin_preserved) {
        return false;
    }
    if (fact.kind == PrincipalSetProjectionKind::composition_to_supertrait
        && fact.source_principal.principal_trait == fact.target_object.principal_trait) {
        return false;
    }
    return fact.kind == PrincipalSetProjectionKind::concrete_to_composition
        || fact.source_principal.object_origin == fact.target_object.object_origin;
}

bool is_valid(
    const PrincipalSetCompositionSummary& summary,
    const PrincipalSetCompositionFacts& facts) noexcept
{
    return summary_equals(summary, summarize_principal_set_composition_counts(facts));
}

bool is_valid(const PrincipalSetCompositionFacts& facts) noexcept
{
    return fact_identities_are_unique(facts.identity_facts)
        && std::all_of(facts.identity_facts.begin(), facts.identity_facts.end(),
            [](const PrincipalSetIdentityFact& fact) {
                return is_valid(fact);
            })
        && std::all_of(facts.witness_sets.begin(), facts.witness_sets.end(),
            [](const CompositionWitnessSetFact& fact) {
                return is_valid(fact);
            })
        && std::all_of(facts.method_namespaces.begin(), facts.method_namespaces.end(),
            [](const PrincipalMethodNamespaceFact& fact) {
                return is_valid(fact);
            })
        && std::all_of(facts.associated_equality_merges.begin(), facts.associated_equality_merges.end(),
            [](const AssociatedEqualityMergeFact& fact) {
                return is_valid(fact);
            })
        && std::all_of(facts.projections.begin(), facts.projections.end(),
            [](const CompositionProjectionFact& fact) {
                return is_valid(fact);
            })
        && all_fact_identities_are_known(facts)
        && fact_payloads_match_identities(facts)
        && is_valid(facts.summary, facts)
        && (facts.fingerprint == StableFingerprint128{}
            || facts.fingerprint == principal_set_composition_facts_fingerprint(facts));
}

PrincipalSetIdentityFact principal_set_identity_fact(
    const std::span<const PrincipalSetPrincipalDescriptor> principals)
{
    PrincipalSetIdentityFact fact;
    fact.metadata_policy = PrincipalSetMetadataPolicy::principal_set_metadata_v1;
    fact.principals.assign(principals.begin(), principals.end());
    normalize_principals(fact.principals);
    if (principal_descriptors_are_canonical_unique(fact.principals)) {
        StableHashBuilder builder;
        builder.mix_string("principal_set_identity_fact.v1");
        builder.mix_u8(stable_metadata_policy_value(fact.metadata_policy));
        builder.mix_u64(static_cast<base::u64>(fact.principals.size()));
        for (const PrincipalSetPrincipalDescriptor& principal : fact.principals) {
            mix_principal_identity_descriptor(builder, principal);
        }
        fact.object_origin = builder.finish();
        StableHashBuilder identity_builder;
        identity_builder.mix_string("principal_set_identity_fact.identity.v1");
        identity_builder.mix_fingerprint(fact.object_origin);
        fact.principal_set_identity = identity_builder.finish();
    }
    return fact;
}

void record_principal_set_identity_fact(
    PrincipalSetCompositionFacts& facts, PrincipalSetIdentityFact fact)
{
    facts.identity_facts.push_back(std::move(fact));
    facts.summary = summarize_principal_set_composition_counts(facts);
}

void record_composition_witness_set_fact(
    PrincipalSetCompositionFacts& facts, CompositionWitnessSetFact fact)
{
    facts.witness_sets.push_back(std::move(fact));
    facts.summary = summarize_principal_set_composition_counts(facts);
}

void record_principal_method_namespace_fact(
    PrincipalSetCompositionFacts& facts, PrincipalMethodNamespaceFact fact)
{
    facts.method_namespaces.push_back(std::move(fact));
    facts.summary = summarize_principal_set_composition_counts(facts);
}

void record_associated_equality_merge_fact(
    PrincipalSetCompositionFacts& facts, AssociatedEqualityMergeFact fact)
{
    facts.associated_equality_merges.push_back(std::move(fact));
    facts.summary = summarize_principal_set_composition_counts(facts);
}

void record_composition_projection_fact(
    PrincipalSetCompositionFacts& facts, CompositionProjectionFact fact)
{
    facts.projections.push_back(std::move(fact));
    facts.summary = summarize_principal_set_composition_counts(facts);
}

PrincipalSetCompositionSummary summarize_principal_set_composition_counts(
    const PrincipalSetCompositionFacts& facts) noexcept
{
    PrincipalSetCompositionSummary summary;
    summary.principal_set_count = static_cast<base::u64>(facts.identity_facts.size());
    summary.witness_set_count = static_cast<base::u64>(facts.witness_sets.size());
    summary.method_namespace_count = static_cast<base::u64>(facts.method_namespaces.size());
    summary.associated_equality_merge_count =
        static_cast<base::u64>(facts.associated_equality_merges.size());
    summary.projection_count = static_cast<base::u64>(facts.projections.size());

    for (const PrincipalSetIdentityFact& fact : facts.identity_facts) {
        summary.principal_count += static_cast<base::u64>(fact.principals.size());
    }
    for (const CompositionWitnessSetFact& fact : facts.witness_sets) {
        summary.witness_count += static_cast<base::u64>(fact.witnesses.size());
    }
    for (const PrincipalMethodNamespaceFact& fact : facts.method_namespaces) {
        summary.method_count += static_cast<base::u64>(fact.methods.size());
    }
    for (const AssociatedEqualityMergeFact& fact : facts.associated_equality_merges) {
        if (fact.status == PrincipalAssociatedEqualityMergeStatus::conflict) {
            ++summary.associated_equality_conflict_count;
        }
    }
    for (const CompositionProjectionFact& fact : facts.projections) {
        if (fact.kind == PrincipalSetProjectionKind::composition_to_supertrait) {
            ++summary.supertrait_projection_count;
        }
        count_projection_borrow_kind(summary, fact.borrow_kind);
    }
    return summary;
}

StableFingerprint128 principal_set_composition_facts_fingerprint(
    const PrincipalSetCompositionFacts& facts) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_PRINCIPAL_SET_COMPOSITION_FINGERPRINT_MARKER);
    builder.mix_string(facts.subject);
    builder.mix_u64(static_cast<base::u64>(facts.identity_facts.size()));
    builder.mix_u64(static_cast<base::u64>(facts.witness_sets.size()));
    builder.mix_u64(static_cast<base::u64>(facts.method_namespaces.size()));
    builder.mix_u64(static_cast<base::u64>(facts.associated_equality_merges.size()));
    builder.mix_u64(static_cast<base::u64>(facts.projections.size()));
    mix_summary(builder, facts.summary);
    for (const PrincipalSetIdentityFact& fact : facts.identity_facts) {
        mix_identity_fact(builder, fact);
    }
    for (const CompositionWitnessSetFact& fact : facts.witness_sets) {
        mix_witness_set_fact(builder, fact);
    }
    for (const PrincipalMethodNamespaceFact& fact : facts.method_namespaces) {
        mix_method_namespace_fact(builder, fact);
    }
    for (const AssociatedEqualityMergeFact& fact : facts.associated_equality_merges) {
        mix_associated_equality_merge_fact(builder, fact);
    }
    for (const CompositionProjectionFact& fact : facts.projections) {
        mix_projection_fact(builder, fact);
    }
    return builder.finish();
}

std::string summarize_principal_set_composition_facts(
    const PrincipalSetCompositionFacts& facts)
{
    std::ostringstream label;
    label << "principal_set_composition_facts subject="
          << (facts.subject.empty() ? "<anonymous>" : facts.subject)
          << " principal_sets=" << facts.summary.principal_set_count
          << " principals=" << facts.summary.principal_count
          << " witness_sets=" << facts.summary.witness_set_count
          << " witnesses=" << facts.summary.witness_count
          << " method_namespaces=" << facts.summary.method_namespace_count
          << " methods=" << facts.summary.method_count
          << " associated_equality_merges=" << facts.summary.associated_equality_merge_count
          << " conflicts=" << facts.summary.associated_equality_conflict_count
          << " projections=" << facts.summary.projection_count
          << " supertrait_projections=" << facts.summary.supertrait_projection_count
          << " shared_projection_borrows=" << facts.summary.shared_borrow_projection_count
          << " mut_projection_borrows=" << facts.summary.mut_borrow_projection_count
          << " metadata="
          << principal_set_metadata_policy_name(PrincipalSetMetadataPolicy::principal_set_metadata_v1);
    if (!facts.projections.empty()) {
        label << " first_projection="
              << principal_set_projection_kind_name(facts.projections.front().kind)
              << " borrow=" << dyn_borrow_kind_name(facts.projections.front().borrow_kind);
    }
    label << " fingerprint=" << debug_string(principal_set_composition_facts_fingerprint(facts));
    return label.str();
}

std::string dump_principal_set_composition_facts(
    const PrincipalSetCompositionFacts& facts)
{
    std::ostringstream stream;
    stream << "principal_set_composition_facts subject="
           << (facts.subject.empty() ? "<anonymous>" : facts.subject)
           << " principal_sets=" << facts.summary.principal_set_count
           << " principals=" << facts.summary.principal_count
           << " witness_sets=" << facts.summary.witness_set_count
           << " witnesses=" << facts.summary.witness_count
           << " method_namespaces=" << facts.summary.method_namespace_count
           << " methods=" << facts.summary.method_count
           << " associated_equality_merges=" << facts.summary.associated_equality_merge_count
           << " conflicts=" << facts.summary.associated_equality_conflict_count
           << " projections=" << facts.summary.projection_count
           << " metadata="
           << principal_set_metadata_policy_name(PrincipalSetMetadataPolicy::principal_set_metadata_v1)
           << " fingerprint=" << debug_string(principal_set_composition_facts_fingerprint(facts)) << '\n';

    for (base::usize index = 0; index < facts.identity_facts.size(); ++index) {
        const PrincipalSetIdentityFact& fact = facts.identity_facts[index];
        stream << "  principal_set_identity_fact #" << index
               << " identity=" << debug_string(fact.principal_set_identity)
               << " origin=" << debug_string(fact.object_origin)
               << " metadata=" << principal_set_metadata_policy_name(fact.metadata_policy)
               << " principals=" << fact.principals.size() << '\n';
        for (const PrincipalSetPrincipalDescriptor& principal : fact.principals) {
            stream << "    principal=" << fallback_name(principal.principal_name, "<anonymous>")
                   << " object=" << fallback_name(principal.object_type_name, "<unknown>")
                   << " key=" << debug_string(stable_key_fingerprint(principal.object_type)) << '\n';
        }
    }
    for (base::usize index = 0; index < facts.witness_sets.size(); ++index) {
        const CompositionWitnessSetFact& fact = facts.witness_sets[index];
        stream << "  composition_witness_set_fact #" << index
               << " identity=" << debug_string(fact.principal_set_identity)
               << " metadata=" << principal_set_metadata_policy_name(fact.metadata_policy)
               << " witnesses=" << fact.witnesses.size() << '\n';
        for (const CompositionWitnessDescriptor& witness : fact.witnesses) {
            stream << "    witness principal=" << fallback_name(witness.principal_name, "<anonymous>")
                   << " concrete=" << fallback_name(witness.concrete_type_name, "<unknown>")
                   << " object_key=" << debug_string(stable_key_fingerprint(witness.principal_object))
                   << " layout=" << debug_string(stable_key_fingerprint(witness.vtable_layout))
                   << " evidence=" << debug_string(witness.witness_fingerprint) << '\n';
        }
    }
    for (base::usize index = 0; index < facts.method_namespaces.size(); ++index) {
        const PrincipalMethodNamespaceFact& fact = facts.method_namespaces[index];
        stream << "  principal_method_namespace_fact #" << index
               << " identity=" << debug_string(fact.principal_set_identity)
               << " methods=" << fact.methods.size() << '\n';
        for (const PrincipalMethodNamespaceEntry& method : fact.methods) {
            stream << "    method principal=" << fallback_name(method.principal_name, "<anonymous>")
                   << " name=" << method.method_name
                   << " slot=" << method.slot
                   << " status=" << principal_method_namespace_status_name(method.status)
                   << " object_key=" << debug_string(stable_key_fingerprint(method.principal_object))
                   << '\n';
        }
    }
    for (base::usize index = 0; index < facts.associated_equality_merges.size(); ++index) {
        const AssociatedEqualityMergeFact& fact = facts.associated_equality_merges[index];
        stream << "  associated_equality_merge_fact #" << index
               << " identity=" << debug_string(fact.principal_set_identity)
               << " associated=" << fallback_name(fact.associated_type_name, "<anonymous>")
               << " merged=" << fallback_name(fact.merged_type_name, "<unconstrained>")
               << " status=" << principal_associated_equality_merge_status_name(fact.status)
               << " contributors=" << fact.contributing_principals.size() << '\n';
    }
    for (base::usize index = 0; index < facts.projections.size(); ++index) {
        const CompositionProjectionFact& fact = facts.projections[index];
        stream << "  composition_projection_fact #" << index
               << " identity=" << debug_string(fact.principal_set_identity)
               << " kind=" << principal_set_projection_kind_name(fact.kind)
               << " borrow=" << dyn_borrow_kind_name(fact.borrow_kind)
               << " source=" << fallback_name(fact.source_view_name, "<unknown>")
               << " target=" << fallback_name(fact.target_view_name, "<unknown>")
               << " data_pointer_preserved=" << (fact.data_pointer_preserved ? "yes" : "no")
               << " origin_preserved=" << (fact.origin_preserved ? "yes" : "no")
               << " path=" << debug_string(fact.projection_path) << '\n';
    }
    return stream.str();
}

} // namespace aurex::query
