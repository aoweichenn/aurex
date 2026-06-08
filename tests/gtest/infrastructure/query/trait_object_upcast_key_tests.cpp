#include <aurex/infrastructure/query/canonical_type_key.hpp>
#include <aurex/infrastructure/query/dyn_abi_facts.hpp>
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
    const std::string dump = query::dump_function_dyn_abi_facts(facts);
    EXPECT_NE(dump.find("dyn_upcast #0 &dyn Child -> &dyn Parent"), std::string::npos) << dump;
    EXPECT_NE(dump.find("metadata=supertrait_vptr_metadata_v1"), std::string::npos) << dump;

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

} // namespace aurex::test
