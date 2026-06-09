#include <aurex/infrastructure/query/canonical_type_key.hpp>
#include <aurex/infrastructure/query/dyn_abi_facts.hpp>
#include <aurex/infrastructure/query/lower_function_ir_query.hpp>
#include <aurex/infrastructure/query/stable_identity.hpp>
#include <aurex/infrastructure/query/trait_object_key.hpp>

#include <array>
#include <span>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u8 QUERY_TEST_INVALID_ENUM_VALUE = 0xffU;

[[nodiscard]] query::PackageKey test_package()
{
    const std::array<std::string_view, 2> parts{"workspace", "root"};
    return query::package_key(parts);
}

[[nodiscard]] query::ModuleKey test_module()
{
    const std::array<std::string_view, 2> path{"dyn", "upcast"};
    return query::module_key(test_package(), path);
}

[[nodiscard]] query::DefKey test_trait_def(const std::string_view name)
{
    const std::array<std::string_view, 1> path{name};
    return query::def_key(test_module(), query::DefNamespace::trait_, query::DefKind::trait_, path);
}

[[nodiscard]] query::TraitObjectTypeKey test_object_key(
    const std::string_view trait_name, const std::string_view schema_name)
{
    const query::DefKey trait = test_trait_def(trait_name);
    const query::StableFingerprint128 origin =
        query::stable_fingerprint(std::string("origin:") + std::string(trait_name));
    const query::StableFingerprint128 schema =
        query::stable_fingerprint(std::string("slot-schema:") + std::string(schema_name));
    return query::trait_object_type_key(
        trait,
        std::span<const query::CanonicalTypeKey>{},
        std::span<const query::TraitObjectAssociatedTypeEqualityKey>{},
        origin,
        schema);
}

[[nodiscard]] query::TraitObjectTypeKey test_generic_object_key(
    const std::string_view trait_name,
    const query::CanonicalTypeKey& arg,
    const std::string_view schema_name)
{
    const std::array<query::CanonicalTypeKey, 1> args{arg};
    const query::DefKey trait = test_trait_def(trait_name);
    std::string origin_payload = "origin:";
    origin_payload.append(trait_name);
    origin_payload.push_back(':');
    origin_payload.append(schema_name);
    std::string schema_payload = "slot-schema:";
    schema_payload.append(schema_name);
    const query::StableFingerprint128 origin = query::stable_fingerprint(origin_payload);
    const query::StableFingerprint128 schema = query::stable_fingerprint(schema_payload);
    return query::trait_object_type_key(
        trait,
        args,
        std::span<const query::TraitObjectAssociatedTypeEqualityKey>{},
        origin,
        schema);
}

} // namespace

TEST(QueryUnit, TraitObjectUpcastCoercionKeySerializesFingerprintsAndHashes)
{
    const query::TraitObjectTypeKey child = test_object_key("Child", "Child");
    const query::TraitObjectTypeKey parent = test_object_key("Parent", "Parent");
    const query::StableFingerprint128 edge_path = query::stable_fingerprint("Child->Parent");
    const query::TraitObjectUpcastCoercionKey upcast = query::trait_object_upcast_coercion_key(
        child, child.object_origin, parent, edge_path, query::TraitObjectBorrowKindKey::shared);

    ASSERT_TRUE(query::is_valid(child));
    ASSERT_TRUE(query::is_valid(parent));
    ASSERT_TRUE(query::is_valid(upcast));
    EXPECT_EQ(upcast.source_object_type, child);
    EXPECT_EQ(upcast.target_object_type, parent);
    EXPECT_EQ(upcast.source_origin, child.object_origin);
    EXPECT_EQ(upcast.supertrait_edge_path, edge_path);
    EXPECT_GT(upcast.global_id, 0U);
    EXPECT_FALSE(query::stable_serialize(upcast).empty());
    EXPECT_EQ(query::stable_key_fingerprint(upcast), query::stable_key_fingerprint(upcast));
    EXPECT_NE(query::TraitObjectUpcastCoercionKeyHash{}(upcast), 0U);
    EXPECT_NE(query::debug_string(upcast).find("TraitObjectUpcastCoercionKey"), std::string::npos);

    const query::TraitObjectUpcastCoercionKey repeated = query::trait_object_upcast_coercion_key(
        child, child.object_origin, parent, edge_path, query::TraitObjectBorrowKindKey::shared);
    EXPECT_EQ(upcast, repeated);
    EXPECT_EQ(query::stable_serialize(upcast), query::stable_serialize(repeated));

    const query::TraitObjectUpcastCoercionKey mut_upcast = query::trait_object_upcast_coercion_key(
        child, child.object_origin, parent, edge_path, query::TraitObjectBorrowKindKey::mut);
    EXPECT_TRUE(query::is_valid(mut_upcast));
    EXPECT_NE(upcast, mut_upcast);
    EXPECT_NE(query::stable_key_fingerprint(upcast), query::stable_key_fingerprint(mut_upcast));

    const query::TraitObjectUpcastCoercionKey other_path = query::trait_object_upcast_coercion_key(
        child, child.object_origin, parent, query::stable_fingerprint("Child->Mid->Parent"),
        query::TraitObjectBorrowKindKey::shared);
    EXPECT_TRUE(query::is_valid(other_path));
    EXPECT_NE(upcast, other_path);
}

TEST(QueryUnit, TraitObjectUpcastCoercionKeyRejectsMalformedInputs)
{
    const query::TraitObjectTypeKey child = test_object_key("Child", "Child");
    const query::TraitObjectTypeKey parent = test_object_key("Parent", "Parent");
    const query::StableFingerprint128 edge_path = query::stable_fingerprint("Child->Parent");

    EXPECT_FALSE(query::is_valid(query::TraitObjectUpcastCoercionKey{}));
    EXPECT_FALSE(query::is_valid(query::trait_object_upcast_coercion_key(
        {}, child.object_origin, parent, edge_path, query::TraitObjectBorrowKindKey::shared)));
    EXPECT_FALSE(query::is_valid(query::trait_object_upcast_coercion_key(
        child, {}, parent, edge_path, query::TraitObjectBorrowKindKey::shared)));
    EXPECT_FALSE(query::is_valid(query::trait_object_upcast_coercion_key(
        child, child.object_origin, {}, edge_path, query::TraitObjectBorrowKindKey::shared)));
    EXPECT_FALSE(query::is_valid(query::trait_object_upcast_coercion_key(
        child, child.object_origin, parent, {}, query::TraitObjectBorrowKindKey::shared)));
    EXPECT_FALSE(query::is_valid(query::trait_object_upcast_coercion_key(
        child, child.object_origin, parent, edge_path,
        static_cast<query::TraitObjectBorrowKindKey>(QUERY_TEST_INVALID_ENUM_VALUE))));
    EXPECT_FALSE(query::is_valid(query::trait_object_upcast_coercion_key(
        child, child.object_origin, child, edge_path, query::TraitObjectBorrowKindKey::shared)));

    query::TraitObjectTypeKey invalid_abi_parent = parent;
    invalid_abi_parent.abi_policy = static_cast<query::TraitObjectAbiPolicyKey>(QUERY_TEST_INVALID_ENUM_VALUE);
    EXPECT_FALSE(query::is_valid(query::trait_object_upcast_coercion_key(
        child, child.object_origin, invalid_abi_parent, edge_path, query::TraitObjectBorrowKindKey::shared)));

    query::TraitObjectUpcastCoercionKey zero_global = query::trait_object_upcast_coercion_key(
        child, child.object_origin, parent, edge_path, query::TraitObjectBorrowKindKey::shared);
    ASSERT_TRUE(query::is_valid(zero_global));
    zero_global.global_id = 0U;
    EXPECT_FALSE(query::is_valid(zero_global));

    query::TraitObjectUpcastCoercionKey wrong_schema = query::trait_object_upcast_coercion_key(
        child, child.object_origin, parent, edge_path, query::TraitObjectBorrowKindKey::shared);
    ASSERT_TRUE(query::is_valid(wrong_schema));
    ++wrong_schema.schema;
    EXPECT_FALSE(query::is_valid(wrong_schema));
}

TEST(QueryUnit, TraitObjectUpcastCoercionKeyIncludesGenericObjectIdentity)
{
    const query::CanonicalTypeKey i32 = query::canonical_builtin(query::BuiltinTypeKey::i32);
    const query::CanonicalTypeKey bool_type = query::canonical_builtin(query::BuiltinTypeKey::bool_);
    const query::TraitObjectTypeKey child_i32 = test_generic_object_key("Child", i32, "Child-i32");
    const query::TraitObjectTypeKey parent_i32 = test_generic_object_key("Parent", i32, "Parent-i32");
    const query::TraitObjectTypeKey parent_bool = test_generic_object_key("Parent", bool_type, "Parent-bool");
    const query::StableFingerprint128 edge_path = query::stable_fingerprint("Child[T]->Parent[T]");

    const query::TraitObjectUpcastCoercionKey to_i32 = query::trait_object_upcast_coercion_key(
        child_i32, child_i32.object_origin, parent_i32, edge_path, query::TraitObjectBorrowKindKey::shared);
    const query::TraitObjectUpcastCoercionKey to_bool = query::trait_object_upcast_coercion_key(
        child_i32, child_i32.object_origin, parent_bool, edge_path, query::TraitObjectBorrowKindKey::shared);

    ASSERT_TRUE(query::is_valid(to_i32));
    ASSERT_TRUE(query::is_valid(to_bool));
    EXPECT_NE(to_i32, to_bool);
    EXPECT_NE(query::stable_key_fingerprint(to_i32), query::stable_key_fingerprint(to_bool));
}

TEST(QueryUnit, FunctionDynAbiSummaryDerivesSupertraitMetadataFromVTableOnlyFacts)
{
    const query::TraitObjectTypeKey child = test_object_key("Child", "Child");
    const query::CanonicalTypeKey concrete = query::canonical_nominal(test_trait_def("File"), {});
    const query::VTableLayoutKey layout = query::vtable_layout_key(concrete,
        child,
        child.object_callability_schema,
        query::stable_fingerprint("impl File: Child"),
        0U,
        query::TraitObjectMetadataPolicyKey::supertrait_vptr_metadata_v1);

    query::FunctionDynAbiFacts facts;
    query::record_dyn_vtable_abi_descriptor(facts,
        query::DynVTableAbiDescriptor{
            layout,
            query::DynAbiPolicy::borrowed_view_v1,
            query::DynMetadataPolicy::supertrait_vptr_metadata_v1,
            "_Aurex_vtable_File_Child",
            "File",
            "dyn Child",
            {},
        });

    ASSERT_TRUE(query::is_valid(facts));
    EXPECT_TRUE(facts.upcasts.empty());
    EXPECT_EQ(query::function_dyn_abi_metadata_policy(facts),
        query::DynMetadataPolicy::supertrait_vptr_metadata_v1);
    const std::string summary = query::summarize_function_dyn_abi_facts(facts);
    EXPECT_NE(summary.find("vtables=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("upcasts=0"), std::string::npos) << summary;
    EXPECT_NE(summary.find("metadata=supertrait_vptr_metadata_v1"), std::string::npos) << summary;
}

TEST(QueryUnit, DynUpcastAbiDescriptorValidatesMetadataBorrowAndDump)
{
    const query::TraitObjectTypeKey child = test_object_key("Child", "Child");
    const query::TraitObjectTypeKey parent = test_object_key("Parent", "Parent");
    const query::StableFingerprint128 edge_path = query::stable_fingerprint("Child->Parent");
    const query::TraitObjectUpcastCoercionKey upcast = query::trait_object_upcast_coercion_key(
        child, child.object_origin, parent, edge_path, query::TraitObjectBorrowKindKey::shared);

    query::DynUpcastAbiDescriptor descriptor{
        upcast,
        child,
        parent,
        edge_path,
        query::DynBorrowKind::shared,
        query::DynAbiPolicy::borrowed_view_v1,
        query::DynMetadataPolicy::supertrait_vptr_metadata_v1,
        "&dyn Child",
        "&dyn Parent",
        "dyn Child",
        "dyn Parent",
    };
    EXPECT_TRUE(query::is_valid(descriptor));
    EXPECT_EQ(query::dyn_metadata_policy_name(query::DynMetadataPolicy::supertrait_vptr_metadata_v1),
        "supertrait_vptr_metadata_v1");
    EXPECT_EQ(query::dyn_metadata_policy_from_key(
                  query::TraitObjectMetadataPolicyKey::supertrait_vptr_metadata_v1),
        query::DynMetadataPolicy::supertrait_vptr_metadata_v1);

    query::FunctionDynAbiFacts facts;
    facts.symbol = "upcast_test";
    query::record_dyn_upcast_abi_descriptor(facts, descriptor);
    facts.fingerprint = query::function_dyn_abi_facts_fingerprint(facts);
    EXPECT_TRUE(query::is_valid(facts));
    EXPECT_EQ(facts.summary.upcast_count, 1U);
    EXPECT_EQ(facts.summary.shared_borrow_count, 1U);
    EXPECT_EQ(facts.summary.mut_borrow_count, 0U);
    EXPECT_EQ(facts.fingerprint, query::function_dyn_abi_facts_fingerprint(facts));
    const std::string summary = query::summarize_function_dyn_abi_facts(facts);
    EXPECT_NE(summary.find("upcasts=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("metadata=supertrait_vptr_metadata_v1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("first_upcast=&dyn Child->&dyn Parent"), std::string::npos) << summary;
    EXPECT_NE(summary.find("borrow=shared"), std::string::npos) << summary;
    const std::string dump = query::dump_function_dyn_abi_facts(facts);
    EXPECT_NE(dump.find("dyn_upcast #0 &dyn Child -> &dyn Parent"), std::string::npos) << dump;
    EXPECT_NE(dump.find("metadata=supertrait_vptr_metadata_v1"), std::string::npos) << dump;

    query::FunctionDynAbiFacts changed_edge_facts = facts;
    changed_edge_facts.upcasts.front().edge_path = query::stable_fingerprint("Child->OtherParent");
    EXPECT_NE(query::function_dyn_abi_facts_fingerprint(changed_edge_facts), facts.fingerprint);

    query::FunctionDynAbiFacts changed_borrow_facts = facts;
    query::DynUpcastAbiDescriptor mut_descriptor = descriptor;
    mut_descriptor.upcast = query::trait_object_upcast_coercion_key(
        child, child.object_origin, parent, edge_path, query::TraitObjectBorrowKindKey::mut);
    mut_descriptor.borrow_kind = query::DynBorrowKind::mut;
    changed_borrow_facts.upcasts.front() = mut_descriptor;
    changed_borrow_facts.summary.shared_borrow_count = 0U;
    changed_borrow_facts.summary.mut_borrow_count = 1U;
    EXPECT_TRUE(query::is_valid(changed_borrow_facts));
    EXPECT_NE(query::function_dyn_abi_facts_fingerprint(changed_borrow_facts), facts.fingerprint);

    const query::QueryResultFingerprint lowered_ir =
        query::query_result_fingerprint(query::stable_fingerprint("dyn-upcast-lower-ir"));
    EXPECT_NE(query::lower_function_ir_result_fingerprint(
                  lowered_ir, query::FunctionCleanupMarkerFacts{}, changed_borrow_facts),
        query::lower_function_ir_result_fingerprint(lowered_ir, query::FunctionCleanupMarkerFacts{}, facts));

    query::DynUpcastAbiDescriptor wrong_edge = descriptor;
    wrong_edge.edge_path = query::stable_fingerprint("other-edge");
    EXPECT_FALSE(query::is_valid(wrong_edge));

    query::DynUpcastAbiDescriptor wrong_borrow = descriptor;
    wrong_borrow.borrow_kind = query::DynBorrowKind::mut;
    EXPECT_FALSE(query::is_valid(wrong_borrow));

    query::DynUpcastAbiDescriptor wrong_metadata = descriptor;
    wrong_metadata.metadata_policy = query::DynMetadataPolicy::borrowed_methods_only_v1;
    EXPECT_FALSE(query::is_valid(wrong_metadata));
}

TEST(QueryUnit, DynCompositionAbiDescriptorsValidateFingerprintAndDump)
{
    const query::TraitObjectTypeKey draw = test_object_key("Draw", "Draw");
    const query::TraitObjectTypeKey debug = test_object_key("Debug", "Debug");
    const query::CanonicalTypeKey concrete = query::canonical_nominal(test_trait_def("File"), {});
    const query::StableFingerprint128 principal_set_identity =
        query::stable_fingerprint("dyn composition Draw+Debug");
    const query::VTableLayoutKey draw_layout = query::vtable_layout_key(concrete,
        draw,
        draw.object_callability_schema,
        query::stable_fingerprint("impl File: Draw"),
        1U);
    const query::VTableLayoutKey debug_layout = query::vtable_layout_key(concrete,
        debug,
        debug.object_callability_schema,
        query::stable_fingerprint("impl File: Debug"),
        1U);

    query::DynPrincipalSetMetadataAbiDescriptor metadata;
    metadata.principal_set_identity = principal_set_identity;
    metadata.symbol = "__aurex_principal_set_metadata_File_Draw_Debug";
    metadata.concrete_type_name = "File";
    metadata.composition_object_type_name = "dyn (Debug + Draw)";
    metadata.witnesses.push_back(query::DynPrincipalSetWitnessAbiDescriptor{
        0U,
        debug,
        debug_layout,
        "dyn Debug",
        "__aurex_vtable_File_Debug",
    });
    metadata.witnesses.push_back(query::DynPrincipalSetWitnessAbiDescriptor{
        1U,
        draw,
        draw_layout,
        "dyn Draw",
        "__aurex_vtable_File_Draw",
    });

    query::DynCompositionProjectionAbiDescriptor projection;
    projection.principal_set_identity = principal_set_identity;
    projection.principal_object = draw;
    projection.target_vtable_layout = draw_layout;
    projection.principal_index = 1U;
    projection.borrow_kind = query::DynBorrowKind::shared;
    projection.source_reference_type_name = "&dyn (Debug + Draw)";
    projection.target_reference_type_name = "&dyn Draw";
    projection.source_object_type_name = "dyn (Debug + Draw)";
    projection.target_object_type_name = "dyn Draw";

    ASSERT_TRUE(query::is_valid(metadata));
    ASSERT_TRUE(query::is_valid(projection));
    EXPECT_EQ(query::dyn_metadata_policy_name(query::DynMetadataPolicy::principal_set_metadata_v1),
        "principal_set_metadata_v1");

    query::FunctionDynAbiFacts facts;
    facts.symbol = "composition_test";
    query::record_dyn_principal_set_metadata_abi_descriptor(facts, metadata);
    query::record_dyn_composition_projection_abi_descriptor(facts, projection);
    facts.fingerprint = query::function_dyn_abi_facts_fingerprint(facts);

    ASSERT_TRUE(query::is_valid(facts));
    EXPECT_EQ(facts.summary.principal_set_metadata_count, 1U);
    EXPECT_EQ(facts.summary.principal_set_witness_count, 2U);
    EXPECT_EQ(facts.summary.composition_projection_count, 1U);
    EXPECT_EQ(facts.summary.shared_borrow_count, 1U);
    EXPECT_EQ(facts.summary.mut_borrow_count, 0U);
    EXPECT_EQ(query::function_dyn_abi_metadata_policy(facts),
        query::DynMetadataPolicy::principal_set_metadata_v1);
    EXPECT_EQ(facts.fingerprint, query::function_dyn_abi_facts_fingerprint(facts));

    const std::string summary = query::summarize_function_dyn_abi_facts(facts);
    EXPECT_NE(summary.find("principal_sets=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("composition_projections=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("metadata=principal_set_metadata_v1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("first_composition_projection=&dyn (Debug + Draw)->&dyn Draw"),
        std::string::npos) << summary;
    EXPECT_NE(summary.find("principal_index=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("borrow=shared"), std::string::npos) << summary;

    const std::string dump = query::dump_function_dyn_abi_facts(facts);
    EXPECT_NE(dump.find("dyn_principal_set_metadata #0"), std::string::npos) << dump;
    EXPECT_NE(dump.find("dyn_principal_set_witness index=0 object=dyn Debug"), std::string::npos)
        << dump;
    EXPECT_NE(dump.find("dyn_principal_set_witness index=1 object=dyn Draw"), std::string::npos)
        << dump;
    EXPECT_NE(dump.find("dyn_composition_projection #0 &dyn (Debug + Draw) -> &dyn Draw"),
        std::string::npos) << dump;
    EXPECT_NE(dump.find("metadata=principal_set_metadata_v1"), std::string::npos) << dump;

    query::FunctionDynAbiFacts changed_witness_facts = facts;
    changed_witness_facts.principal_sets.front().witnesses.front().vtable_symbol =
        "__aurex_vtable_File_Debug_v2";
    EXPECT_TRUE(query::is_valid(changed_witness_facts));
    EXPECT_NE(query::function_dyn_abi_facts_fingerprint(changed_witness_facts), facts.fingerprint);

    query::FunctionDynAbiFacts changed_borrow_facts = facts;
    changed_borrow_facts.composition_projections.front().borrow_kind = query::DynBorrowKind::mut;
    changed_borrow_facts.summary.shared_borrow_count = 0U;
    changed_borrow_facts.summary.mut_borrow_count = 1U;
    EXPECT_TRUE(query::is_valid(changed_borrow_facts));
    EXPECT_NE(query::function_dyn_abi_facts_fingerprint(changed_borrow_facts), facts.fingerprint);

    const query::QueryResultFingerprint lowered_ir =
        query::query_result_fingerprint(query::stable_fingerprint("dyn-composition-lower-ir"));
    EXPECT_NE(query::lower_function_ir_result_fingerprint(
                  lowered_ir, query::FunctionCleanupMarkerFacts{}, changed_borrow_facts),
        query::lower_function_ir_result_fingerprint(lowered_ir, query::FunctionCleanupMarkerFacts{}, facts));

    query::DynPrincipalSetMetadataAbiDescriptor unsorted = metadata;
    std::swap(unsorted.witnesses[0], unsorted.witnesses[1]);
    EXPECT_FALSE(query::is_valid(unsorted));

    query::DynPrincipalSetMetadataAbiDescriptor too_small = metadata;
    too_small.witnesses.pop_back();
    EXPECT_FALSE(query::is_valid(too_small));

    query::DynPrincipalSetMetadataAbiDescriptor sparse = metadata;
    sparse.witnesses.back().principal_index = 2U;
    EXPECT_FALSE(query::is_valid(sparse));

    query::DynPrincipalSetMetadataAbiDescriptor wrong_metadata_policy = metadata;
    wrong_metadata_policy.metadata_policy = query::DynMetadataPolicy::borrowed_methods_only_v1;
    EXPECT_FALSE(query::is_valid(wrong_metadata_policy));

    query::DynCompositionProjectionAbiDescriptor wrong_layout = projection;
    wrong_layout.target_vtable_layout = debug_layout;
    EXPECT_FALSE(query::is_valid(wrong_layout));
}

} // namespace aurex::test
