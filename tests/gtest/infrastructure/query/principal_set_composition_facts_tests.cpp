#include <aurex/infrastructure/query/principal_set_composition_facts.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u8 QUERY_TEST_INVALID_ENUM_VALUE = 0xffU;
constexpr base::u32 QUERY_TEST_METHOD_SLOT_ZERO = 0U;
constexpr base::u32 QUERY_TEST_METHOD_SLOT_ONE = 1U;
constexpr base::u32 QUERY_TEST_ASSOCIATED_ORDINAL = 0U;
constexpr base::u32 QUERY_TEST_VTABLE_METHOD_SLOT_COUNT = 2U;

struct PrincipalSetFixture {
    query::CanonicalTypeKey concrete_type;
    query::TraitObjectTypeKey draw_object;
    query::TraitObjectTypeKey debug_object;
    query::TraitObjectTypeKey display_object;
    query::TraitObjectTypeKey super_object;
    query::VTableLayoutKey draw_layout;
    query::VTableLayoutKey debug_layout;
    query::VTableLayoutKey display_layout;
    query::VTableLayoutKey super_layout;
    query::MemberKey item_member;
};

[[nodiscard]] query::PackageKey test_package()
{
    const std::array<std::string_view, 2> parts{"workspace", "root"};
    return query::package_key(parts);
}

[[nodiscard]] query::ModuleKey test_module()
{
    const std::array<std::string_view, 2> path{"dyn", "principal_set"};
    return query::module_key(test_package(), path);
}

[[nodiscard]] query::DefKey test_trait_def(const std::string_view name)
{
    const std::array<std::string_view, 1> path{name};
    return query::def_key(test_module(), query::DefNamespace::trait_, query::DefKind::trait_, path);
}

[[nodiscard]] query::DefKey test_struct_def(const std::string_view name)
{
    const std::array<std::string_view, 1> path{name};
    return query::def_key(test_module(), query::DefNamespace::type, query::DefKind::struct_, path);
}

[[nodiscard]] query::TraitObjectTypeKey test_object_key(
    const std::string_view trait_name,
    const query::StableFingerprint128 origin,
    const std::string_view schema_name)
{
    const query::DefKey trait = test_trait_def(trait_name);
    const query::StableFingerprint128 schema =
        query::stable_fingerprint(std::string("principal-set-slot-schema:") + std::string(schema_name));
    return query::trait_object_type_key(
        trait,
        std::span<const query::CanonicalTypeKey>{},
        std::span<const query::TraitObjectAssociatedTypeEqualityKey>{},
        origin,
        schema);
}

[[nodiscard]] query::VTableLayoutKey test_layout_key(
    const query::CanonicalTypeKey& concrete_type,
    const query::TraitObjectTypeKey& object,
    const std::string_view impl_name)
{
    return query::vtable_layout_key(
        concrete_type,
        object,
        object.object_callability_schema,
        query::stable_fingerprint(std::string("impl:") + std::string(impl_name)),
        QUERY_TEST_VTABLE_METHOD_SLOT_COUNT,
        query::TraitObjectMetadataPolicyKey::borrowed_methods_only_v1);
}

[[nodiscard]] PrincipalSetFixture make_fixture()
{
    PrincipalSetFixture fixture;
    const query::StableFingerprint128 origin = query::stable_fingerprint("origin:Canvas");
    fixture.concrete_type = query::canonical_nominal(test_struct_def("Canvas"), {});
    fixture.draw_object = test_object_key("Draw", origin, "Draw");
    fixture.debug_object = test_object_key("Debug", origin, "Debug");
    fixture.display_object = test_object_key("Display", origin, "Display");
    fixture.super_object = test_object_key("Renderable", origin, "Renderable");
    fixture.draw_layout = test_layout_key(fixture.concrete_type, fixture.draw_object, "Canvas:Draw");
    fixture.debug_layout = test_layout_key(fixture.concrete_type, fixture.debug_object, "Canvas:Debug");
    fixture.display_layout = test_layout_key(fixture.concrete_type, fixture.display_object, "Canvas:Display");
    fixture.super_layout = test_layout_key(fixture.concrete_type, fixture.super_object, "Canvas:Renderable");
    fixture.item_member = query::member_key(
        fixture.draw_object.principal_trait,
        query::MemberKind::associated_type,
        "Item",
        QUERY_TEST_ASSOCIATED_ORDINAL);
    return fixture;
}

[[nodiscard]] query::PrincipalSetPrincipalDescriptor principal_descriptor(
    const query::TraitObjectTypeKey& object,
    const std::string_view principal_name)
{
    return query::PrincipalSetPrincipalDescriptor{
        object,
        std::string(principal_name),
        std::string("dyn ") + std::string(principal_name),
    };
}

[[nodiscard]] bool trait_object_key_less(
    const query::TraitObjectTypeKey& lhs,
    const query::TraitObjectTypeKey& rhs) noexcept
{
    if (lhs.principal_trait.global_id != rhs.principal_trait.global_id) {
        return lhs.principal_trait.global_id < rhs.principal_trait.global_id;
    }
    return lhs.global_id < rhs.global_id;
}

[[nodiscard]] query::PrincipalSetIdentityFact test_identity_fact(const PrincipalSetFixture& fixture)
{
    const std::array<query::PrincipalSetPrincipalDescriptor, 2> principals{
        principal_descriptor(fixture.draw_object, "Draw"),
        principal_descriptor(fixture.debug_object, "Debug"),
    };
    return query::principal_set_identity_fact(principals);
}

[[nodiscard]] query::CompositionWitnessDescriptor witness_descriptor(
    const query::TraitObjectTypeKey& object,
    const query::VTableLayoutKey& layout,
    const std::string_view principal_name)
{
    return query::CompositionWitnessDescriptor{
        object,
        layout,
        query::stable_fingerprint(std::string("witness:Canvas:") + std::string(principal_name)),
        std::string(principal_name),
        "Canvas",
    };
}

[[nodiscard]] query::CompositionWitnessSetFact test_witness_set(
    const PrincipalSetFixture& fixture,
    const query::PrincipalSetIdentityFact& identity)
{
    query::CompositionWitnessSetFact fact{
        identity.principal_set_identity,
        query::PrincipalSetMetadataPolicy::principal_set_metadata_v1,
        {
            witness_descriptor(fixture.draw_object, fixture.draw_layout, "Draw"),
            witness_descriptor(fixture.debug_object, fixture.debug_layout, "Debug"),
        },
    };
    std::sort(fact.witnesses.begin(), fact.witnesses.end(),
        [](const query::CompositionWitnessDescriptor& lhs,
            const query::CompositionWitnessDescriptor& rhs) {
            return trait_object_key_less(lhs.principal_object, rhs.principal_object);
        });
    return fact;
}

[[nodiscard]] query::PrincipalMethodNamespaceFact test_method_namespace(
    const PrincipalSetFixture& fixture,
    const query::PrincipalSetIdentityFact& identity)
{
    return query::PrincipalMethodNamespaceFact{
        identity.principal_set_identity,
        {
            query::PrincipalMethodNamespaceEntry{
                fixture.draw_object,
                "Draw",
                "draw",
                QUERY_TEST_METHOD_SLOT_ZERO,
                query::PrincipalMethodNamespaceStatus::unique_principal_method,
            },
            query::PrincipalMethodNamespaceEntry{
                fixture.debug_object,
                "Debug",
                "fmt",
                QUERY_TEST_METHOD_SLOT_ONE,
                query::PrincipalMethodNamespaceStatus::unique_principal_method,
            },
        },
    };
}

[[nodiscard]] query::AssociatedEqualityMergeFact associated_merge(
    const PrincipalSetFixture& fixture,
    const query::PrincipalSetIdentityFact& identity,
    const query::PrincipalAssociatedEqualityMergeStatus status)
{
    query::AssociatedEqualityMergeFact fact{
        identity.principal_set_identity,
        fixture.item_member,
        status == query::PrincipalAssociatedEqualityMergeStatus::unconstrained
            ? query::CanonicalTypeKey{}
            : query::canonical_builtin(query::BuiltinTypeKey::i32),
        status,
        {
            fixture.draw_object,
            fixture.debug_object,
        },
        "Item",
        status == query::PrincipalAssociatedEqualityMergeStatus::unconstrained ? "" : "i32",
    };
    std::sort(fact.contributing_principals.begin(), fact.contributing_principals.end(),
        [](const query::TraitObjectTypeKey& lhs, const query::TraitObjectTypeKey& rhs) {
            return trait_object_key_less(lhs, rhs);
        });
    return fact;
}

[[nodiscard]] query::CompositionProjectionFact projection_fact(
    const PrincipalSetFixture& fixture,
    const query::PrincipalSetIdentityFact& identity,
    const query::PrincipalSetProjectionKind kind,
    const query::TraitObjectTypeKey& target,
    const query::DynBorrowKind borrow_kind)
{
    return query::CompositionProjectionFact{
        identity.principal_set_identity,
        kind,
        fixture.concrete_type,
        fixture.draw_object,
        target,
        query::stable_fingerprint(std::string("projection:")
            + std::string(query::principal_set_projection_kind_name(kind))),
        borrow_kind,
        true,
        true,
        kind == query::PrincipalSetProjectionKind::concrete_to_composition ? "&Canvas" : "&dyn Draw + Debug",
        target == fixture.super_object ? "&dyn Renderable" : "&dyn Draw",
    };
}

[[nodiscard]] query::PrincipalSetCompositionFacts full_facts()
{
    const PrincipalSetFixture fixture = make_fixture();
    const query::PrincipalSetIdentityFact identity = test_identity_fact(fixture);
    query::PrincipalSetCompositionFacts facts;
    facts.subject = "Canvas dyn composition";
    query::record_principal_set_identity_fact(facts, identity);
    query::record_composition_witness_set_fact(facts, test_witness_set(fixture, identity));
    query::record_principal_method_namespace_fact(facts, test_method_namespace(fixture, identity));
    query::record_associated_equality_merge_fact(
        facts,
        associated_merge(fixture, identity, query::PrincipalAssociatedEqualityMergeStatus::satisfied));
    query::record_associated_equality_merge_fact(
        facts,
        associated_merge(fixture, identity, query::PrincipalAssociatedEqualityMergeStatus::conflict));
    query::record_composition_projection_fact(facts,
        projection_fact(
            fixture,
            identity,
            query::PrincipalSetProjectionKind::concrete_to_composition,
            fixture.draw_object,
            query::DynBorrowKind::shared));
    query::record_composition_projection_fact(facts,
        projection_fact(
            fixture,
            identity,
            query::PrincipalSetProjectionKind::composition_to_supertrait,
            fixture.super_object,
            query::DynBorrowKind::mut));
    facts.fingerprint = query::principal_set_composition_facts_fingerprint(facts);
    return facts;
}

} // namespace

TEST(QueryUnit, PrincipalSetCompositionFactsExposeEnumNamesAndInvalidFallbacks)
{
    EXPECT_EQ(query::principal_set_metadata_policy_name(
                  query::PrincipalSetMetadataPolicy::principal_set_metadata_v1),
        "principal_set_metadata_v1");
    EXPECT_EQ(query::principal_method_namespace_status_name(
                  query::PrincipalMethodNamespaceStatus::unique_principal_method),
        "unique_principal_method");
    EXPECT_EQ(query::principal_method_namespace_status_name(
                  query::PrincipalMethodNamespaceStatus::ambiguous_requires_principal),
        "ambiguous_requires_principal");
    EXPECT_EQ(query::principal_associated_equality_merge_status_name(
                  query::PrincipalAssociatedEqualityMergeStatus::satisfied),
        "satisfied");
    EXPECT_EQ(query::principal_associated_equality_merge_status_name(
                  query::PrincipalAssociatedEqualityMergeStatus::conflict),
        "conflict");
    EXPECT_EQ(query::principal_associated_equality_merge_status_name(
                  query::PrincipalAssociatedEqualityMergeStatus::unconstrained),
        "unconstrained");
    EXPECT_EQ(query::principal_set_projection_kind_name(
                  query::PrincipalSetProjectionKind::concrete_to_composition),
        "concrete_to_composition");
    EXPECT_EQ(query::principal_set_projection_kind_name(
                  query::PrincipalSetProjectionKind::composition_to_principal),
        "composition_to_principal");
    EXPECT_EQ(query::principal_set_projection_kind_name(
                  query::PrincipalSetProjectionKind::composition_to_supertrait),
        "composition_to_supertrait");

    EXPECT_EQ(query::principal_set_metadata_policy_name(
                  static_cast<query::PrincipalSetMetadataPolicy>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::principal_method_namespace_status_name(
                  static_cast<query::PrincipalMethodNamespaceStatus>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::principal_associated_equality_merge_status_name(
                  static_cast<query::PrincipalAssociatedEqualityMergeStatus>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::principal_set_projection_kind_name(
                  static_cast<query::PrincipalSetProjectionKind>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");

    EXPECT_FALSE(query::is_valid(
        static_cast<query::PrincipalSetMetadataPolicy>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::PrincipalMethodNamespaceStatus>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::PrincipalAssociatedEqualityMergeStatus>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::PrincipalSetProjectionKind>(QUERY_TEST_INVALID_ENUM_VALUE)));
}

TEST(QueryUnit, PrincipalSetCompositionIdentityCanonicalizesPrincipalSet)
{
    const PrincipalSetFixture fixture = make_fixture();
    const std::array<query::PrincipalSetPrincipalDescriptor, 2> source_order{
        principal_descriptor(fixture.draw_object, "Draw"),
        principal_descriptor(fixture.debug_object, "Debug"),
    };
    const std::array<query::PrincipalSetPrincipalDescriptor, 2> reversed_order{
        principal_descriptor(fixture.debug_object, "Debug"),
        principal_descriptor(fixture.draw_object, "Draw"),
    };
    const query::PrincipalSetIdentityFact identity = query::principal_set_identity_fact(source_order);
    const query::PrincipalSetIdentityFact reversed = query::principal_set_identity_fact(reversed_order);

    ASSERT_TRUE(query::is_valid(identity));
    ASSERT_TRUE(query::is_valid(reversed));
    ASSERT_EQ(identity.principals.size(), reversed.principals.size());
    EXPECT_EQ(identity.principal_set_identity, reversed.principal_set_identity);
    EXPECT_EQ(identity.principals.front().object_type.global_id, reversed.principals.front().object_type.global_id);
    EXPECT_EQ(identity.principals.back().object_type.global_id, reversed.principals.back().object_type.global_id);
    EXPECT_TRUE(std::is_sorted(identity.principals.begin(), identity.principals.end(),
        [](const query::PrincipalSetPrincipalDescriptor& lhs,
            const query::PrincipalSetPrincipalDescriptor& rhs) {
            return trait_object_key_less(lhs.object_type, rhs.object_type);
        }));

    std::array<query::PrincipalSetPrincipalDescriptor, 2> renamed = source_order;
    renamed[0].principal_name = "Sketch";
    renamed[0].object_type_name = "dyn Sketch";
    const query::PrincipalSetIdentityFact renamed_identity = query::principal_set_identity_fact(renamed);
    EXPECT_TRUE(query::is_valid(renamed_identity));
    EXPECT_EQ(identity.principal_set_identity, renamed_identity.principal_set_identity);
}

TEST(QueryUnit, PrincipalSetCompositionFactsValidationRejectsBoundaryDrift)
{
    const PrincipalSetFixture fixture = make_fixture();
    const query::PrincipalSetIdentityFact identity = test_identity_fact(fixture);
    ASSERT_TRUE(query::is_valid(identity));

    const std::array<query::PrincipalSetPrincipalDescriptor, 1> single{
        principal_descriptor(fixture.draw_object, "Draw"),
    };
    EXPECT_FALSE(query::is_valid(query::principal_set_identity_fact(single)));

    const std::array<query::PrincipalSetPrincipalDescriptor, 2> duplicate{
        principal_descriptor(fixture.draw_object, "Draw"),
        principal_descriptor(fixture.draw_object, "Draw"),
    };
    EXPECT_FALSE(query::is_valid(query::principal_set_identity_fact(duplicate)));

    const query::TraitObjectTypeKey other_origin_object =
        test_object_key("OtherOrigin", query::stable_fingerprint("origin:Other"), "OtherOrigin");
    const std::array<query::PrincipalSetPrincipalDescriptor, 2> mixed_origin{
        principal_descriptor(fixture.draw_object, "Draw"),
        principal_descriptor(other_origin_object, "OtherOrigin"),
    };
    const query::PrincipalSetIdentityFact mixed_origin_identity =
        query::principal_set_identity_fact(mixed_origin);
    EXPECT_TRUE(query::is_valid(mixed_origin_identity));
    EXPECT_NE(mixed_origin_identity.object_origin, fixture.draw_object.object_origin);

    query::CompositionWitnessSetFact witness_set = test_witness_set(fixture, identity);
    EXPECT_TRUE(query::is_valid(witness_set));
    witness_set.metadata_policy =
        static_cast<query::PrincipalSetMetadataPolicy>(QUERY_TEST_INVALID_ENUM_VALUE);
    EXPECT_FALSE(query::is_valid(witness_set));

    query::CompositionWitnessDescriptor wrong_layout =
        witness_descriptor(fixture.draw_object, fixture.debug_layout, "Draw");
    EXPECT_FALSE(query::is_valid(wrong_layout));

    query::AssociatedEqualityMergeFact unconstrained =
        associated_merge(fixture, identity, query::PrincipalAssociatedEqualityMergeStatus::unconstrained);
    EXPECT_TRUE(query::is_valid(unconstrained));
    unconstrained.merged_type = query::canonical_builtin(query::BuiltinTypeKey::i32);
    EXPECT_FALSE(query::is_valid(unconstrained));

    query::AssociatedEqualityMergeFact bad_member =
        associated_merge(fixture, identity, query::PrincipalAssociatedEqualityMergeStatus::satisfied);
    bad_member.associated_type.kind = query::MemberKind::trait_method;
    EXPECT_FALSE(query::is_valid(bad_member));

    query::CompositionProjectionFact projection = projection_fact(
        fixture,
        identity,
        query::PrincipalSetProjectionKind::composition_to_supertrait,
        fixture.super_object,
        query::DynBorrowKind::shared);
    EXPECT_TRUE(query::is_valid(projection));
    projection.data_pointer_preserved = false;
    EXPECT_FALSE(query::is_valid(projection));
    projection.data_pointer_preserved = true;
    projection.origin_preserved = false;
    EXPECT_FALSE(query::is_valid(projection));
    projection.origin_preserved = true;
    projection.kind = static_cast<query::PrincipalSetProjectionKind>(QUERY_TEST_INVALID_ENUM_VALUE);
    EXPECT_FALSE(query::is_valid(projection));
}

TEST(QueryUnit, PrincipalSetCompositionFactsRejectFlattenedNamespace)
{
    const PrincipalSetFixture fixture = make_fixture();
    const query::PrincipalSetIdentityFact identity = test_identity_fact(fixture);
    query::PrincipalMethodNamespaceFact flattened{
        identity.principal_set_identity,
        {
            query::PrincipalMethodNamespaceEntry{
                fixture.draw_object,
                "Draw",
                "format",
                QUERY_TEST_METHOD_SLOT_ZERO,
                query::PrincipalMethodNamespaceStatus::unique_principal_method,
            },
            query::PrincipalMethodNamespaceEntry{
                fixture.debug_object,
                "Debug",
                "format",
                QUERY_TEST_METHOD_SLOT_ONE,
                query::PrincipalMethodNamespaceStatus::unique_principal_method,
            },
        },
    };
    EXPECT_FALSE(query::is_valid(flattened));

    for (query::PrincipalMethodNamespaceEntry& method : flattened.methods) {
        method.status = query::PrincipalMethodNamespaceStatus::ambiguous_requires_principal;
    }
    EXPECT_TRUE(query::is_valid(flattened));

    query::PrincipalMethodNamespaceFact duplicate_in_principal = flattened;
    duplicate_in_principal.methods.back().principal_object = fixture.draw_object;
    duplicate_in_principal.methods.back().principal_name = "Draw";
    EXPECT_FALSE(query::is_valid(duplicate_in_principal));
}

TEST(QueryUnit, PrincipalSetCompositionFactsSummaryDumpAndFingerprintAreStable)
{
    const query::PrincipalSetCompositionFacts facts = full_facts();
    ASSERT_TRUE(query::is_valid(facts));
    EXPECT_EQ(facts.summary.principal_set_count, 1U);
    EXPECT_EQ(facts.summary.principal_count, 2U);
    EXPECT_EQ(facts.summary.witness_set_count, 1U);
    EXPECT_EQ(facts.summary.witness_count, 2U);
    EXPECT_EQ(facts.summary.method_namespace_count, 1U);
    EXPECT_EQ(facts.summary.method_count, 2U);
    EXPECT_EQ(facts.summary.associated_equality_merge_count, 2U);
    EXPECT_EQ(facts.summary.associated_equality_conflict_count, 1U);
    EXPECT_EQ(facts.summary.projection_count, 2U);
    EXPECT_EQ(facts.summary.supertrait_projection_count, 1U);
    EXPECT_EQ(facts.summary.shared_borrow_projection_count, 1U);
    EXPECT_EQ(facts.summary.mut_borrow_projection_count, 1U);
    EXPECT_EQ(facts.fingerprint, query::principal_set_composition_facts_fingerprint(facts));

    const std::string summary = query::summarize_principal_set_composition_facts(facts);
    EXPECT_NE(summary.find("principal_set_composition_facts subject=Canvas dyn composition"),
        std::string::npos) << summary;
    EXPECT_NE(summary.find("principal_sets=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("principals=2"), std::string::npos) << summary;
    EXPECT_NE(summary.find("conflicts=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("metadata=principal_set_metadata_v1"), std::string::npos) << summary;

    const std::string dump = query::dump_principal_set_composition_facts(facts);
    EXPECT_NE(dump.find("principal_set_identity_fact"), std::string::npos) << dump;
    EXPECT_NE(dump.find("composition_witness_set_fact"), std::string::npos) << dump;
    EXPECT_NE(dump.find("principal_method_namespace_fact"), std::string::npos) << dump;
    EXPECT_NE(dump.find("associated_equality_merge_fact"), std::string::npos) << dump;
    EXPECT_NE(dump.find("composition_projection_fact"), std::string::npos) << dump;
    EXPECT_NE(dump.find("principal=Draw"), std::string::npos) << dump;
    EXPECT_NE(dump.find("principal=Debug"), std::string::npos) << dump;
    EXPECT_NE(dump.find("status=conflict"), std::string::npos) << dump;
    EXPECT_NE(dump.find("kind=composition_to_supertrait"), std::string::npos) << dump;
    EXPECT_NE(dump.find("borrow=mut"), std::string::npos) << dump;

    query::PrincipalSetCompositionFacts changed_slot = facts;
    changed_slot.method_namespaces.front().methods.front().slot = QUERY_TEST_METHOD_SLOT_ONE;
    changed_slot.fingerprint = query::principal_set_composition_facts_fingerprint(changed_slot);
    EXPECT_TRUE(query::is_valid(changed_slot));
    EXPECT_NE(changed_slot.fingerprint, facts.fingerprint);

    query::PrincipalSetCompositionFacts changed_projection = facts;
    changed_projection.projections.back().projection_path =
        query::stable_fingerprint("projection:composition_to_supertrait:other");
    changed_projection.fingerprint = query::principal_set_composition_facts_fingerprint(changed_projection);
    EXPECT_TRUE(query::is_valid(changed_projection));
    EXPECT_NE(changed_projection.fingerprint, facts.fingerprint);
}

TEST(QueryUnit, PrincipalSetCompositionFactsRejectUnknownIdentityOrStaleSummary)
{
    query::PrincipalSetCompositionFacts facts = full_facts();
    ASSERT_TRUE(query::is_valid(facts));

    query::PrincipalSetCompositionFacts unknown_identity = facts;
    unknown_identity.projections.front().principal_set_identity = query::stable_fingerprint("unknown identity");
    unknown_identity.fingerprint = query::principal_set_composition_facts_fingerprint(unknown_identity);
    EXPECT_FALSE(query::is_valid(unknown_identity));

    query::PrincipalSetCompositionFacts stale_summary = facts;
    ++stale_summary.summary.method_count;
    stale_summary.fingerprint = query::principal_set_composition_facts_fingerprint(stale_summary);
    EXPECT_FALSE(query::is_valid(stale_summary));

    query::PrincipalSetCompositionFacts stale_fingerprint = facts;
    stale_fingerprint.subject = "changed without fingerprint refresh";
    EXPECT_FALSE(query::is_valid(stale_fingerprint));
}

TEST(QueryUnit, PrincipalSetCompositionFactsRejectCrossIdentityPayloadDrift)
{
    const PrincipalSetFixture fixture = make_fixture();
    query::PrincipalSetCompositionFacts facts = full_facts();
    ASSERT_TRUE(query::is_valid(facts));

    query::PrincipalSetCompositionFacts wrong_witness = facts;
    wrong_witness.witness_sets.front().witnesses.front() =
        witness_descriptor(fixture.display_object, fixture.display_layout, "Display");
    std::sort(wrong_witness.witness_sets.front().witnesses.begin(),
        wrong_witness.witness_sets.front().witnesses.end(),
        [](const query::CompositionWitnessDescriptor& lhs,
            const query::CompositionWitnessDescriptor& rhs) {
            return trait_object_key_less(lhs.principal_object, rhs.principal_object);
        });
    wrong_witness.fingerprint = query::principal_set_composition_facts_fingerprint(wrong_witness);
    EXPECT_FALSE(query::is_valid(wrong_witness));

    query::PrincipalSetCompositionFacts wrong_method = facts;
    wrong_method.method_namespaces.front().methods.front().principal_object = fixture.display_object;
    wrong_method.method_namespaces.front().methods.front().principal_name = "Display";
    wrong_method.fingerprint = query::principal_set_composition_facts_fingerprint(wrong_method);
    EXPECT_FALSE(query::is_valid(wrong_method));

    query::PrincipalSetCompositionFacts wrong_associated = facts;
    wrong_associated.associated_equality_merges.front().contributing_principals.front() =
        fixture.display_object;
    std::sort(wrong_associated.associated_equality_merges.front().contributing_principals.begin(),
        wrong_associated.associated_equality_merges.front().contributing_principals.end(),
        [](const query::TraitObjectTypeKey& lhs, const query::TraitObjectTypeKey& rhs) {
            return trait_object_key_less(lhs, rhs);
        });
    wrong_associated.fingerprint = query::principal_set_composition_facts_fingerprint(wrong_associated);
    EXPECT_FALSE(query::is_valid(wrong_associated));

    query::PrincipalSetCompositionFacts wrong_projection_source = facts;
    wrong_projection_source.projections.front().source_principal = fixture.display_object;
    wrong_projection_source.projections.front().target_object = fixture.display_object;
    wrong_projection_source.fingerprint =
        query::principal_set_composition_facts_fingerprint(wrong_projection_source);
    EXPECT_FALSE(query::is_valid(wrong_projection_source));

    query::PrincipalSetCompositionFacts wrong_projection_target = facts;
    wrong_projection_target.projections.front().target_object = fixture.display_object;
    wrong_projection_target.fingerprint =
        query::principal_set_composition_facts_fingerprint(wrong_projection_target);
    EXPECT_FALSE(query::is_valid(wrong_projection_target));
}

} // namespace aurex::test
