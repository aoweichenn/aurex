#include <aurex/query/generic_instance_key.hpp>
#include <aurex/query/generic_instance_signature_query.hpp>
#include <aurex/query/item_signature_query.hpp>
#include <aurex/query/query_context.hpp>
#include <aurex/query/query_result.hpp>
#include <aurex/query/query_reuse.hpp>
#include <aurex/query/stable_identity.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u32 QUERY_TEST_DISAMBIGUATOR = 1;
constexpr base::u32 QUERY_TEST_GENERIC_PARAM_INDEX = 0;
constexpr base::u32 QUERY_TEST_STABLE_ORDINAL = 0;
constexpr base::u64 QUERY_TEST_ARRAY_COUNT = 4;
constexpr base::u32 QUERY_TEST_WRITER_VALUE = 7;
constexpr base::u64 QUERY_TEST_LEGACY_EMPTY_MODULE_PATH_PRIMARY = 0x6ebe4a07bced0d95ULL;
constexpr base::u64 QUERY_TEST_LEGACY_EMPTY_MODULE_PATH_SECONDARY = 0xbdb2a36668c900d3ULL;
constexpr base::u64 QUERY_TEST_LEGACY_EMPTY_MODULE_GLOBAL_ID = 0x5ca885dc64cb6403ULL;
constexpr base::u64 QUERY_TEST_LEGACY_DOTTED_MODULE_PATH_PRIMARY = 0x6bfecbda1ca3380fULL;
constexpr base::u64 QUERY_TEST_LEGACY_DOTTED_MODULE_PATH_SECONDARY = 0x10201c99140e1aadULL;
constexpr base::u64 QUERY_TEST_LEGACY_DOTTED_MODULE_GLOBAL_ID = 0x26c54d5e4db5e233ULL;
constexpr base::u64 QUERY_TEST_LEGACY_FUNCTION_GLOBAL_ID = 0x0fac6bd8b25eb8adULL;
constexpr base::u64 QUERY_TEST_LEGACY_VALUE_GLOBAL_ID = 0x0fac6bd8b256d8f7ULL;
constexpr base::u64 QUERY_TEST_LEGACY_FIELD_GLOBAL_ID = 0x1f522697ad2bd81cULL;
constexpr base::u64 QUERY_TEST_LEGACY_INCREMENTAL_GLOBAL_ID = 0x55dd4e219a7521c0ULL;
constexpr base::u32 QUERY_TEST_LEGACY_EMPTY_MODULE_PATH_BYTES = 8;
constexpr base::u32 QUERY_TEST_LEGACY_DOTTED_MODULE_PATH_BYTES = 28;
constexpr std::string_view QUERY_TEST_PROVIDER_SIGNATURE = "signature:i32";
constexpr std::string_view QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE = "signature:mismatched-provider-output";

[[nodiscard]] query::PackageKey test_package()
{
    const std::array<std::string_view, 2> parts{"workspace", "root"};
    return query::package_key(parts);
}

[[nodiscard]] query::ModuleKey test_module(const query::PackageKey package)
{
    const std::array<std::string_view, 2> path{"regex", "vm"};
    return query::module_key(package, path);
}

[[nodiscard]] query::DefKey test_template_def(const query::ModuleKey module)
{
    const std::array<std::string_view, 1> path{"Vec"};
    return query::def_key(module, query::DefNamespace::type, query::DefKind::generic_template, path);
}

[[nodiscard]] query::DefKey test_function_def(const query::ModuleKey module)
{
    const std::array<std::string_view, 1> path{"compute"};
    return query::def_key(module, query::DefNamespace::value, query::DefKind::function, path);
}

struct QueryContextItemSignatureSubject {
    query::DefKey def;
    query::ItemSignatureProviderInput input;
};

struct QueryContextGenericInstanceSignatureSubject {
    query::GenericInstanceKey key;
    query::IncrementalKey signature;
};

[[nodiscard]] QueryContextItemSignatureSubject test_item_signature_subject(
    const std::string_view def_name, const std::string_view signature)
{
    const std::array<std::string_view, 2> stable_module_path{"regex", "vm"};
    const query::StableModuleId stable_module = query::stable_module_id(stable_module_path);
    const query::StableDefId stable_def =
        query::stable_definition_id(stable_module, query::StableSymbolKind::function, def_name);
    const query::DefKey def =
        query::def_key_from_stable_id(stable_def, query::DefNamespace::value, query::DefKind::function);
    return QueryContextItemSignatureSubject{
        def,
        query::ItemSignatureProviderInput{
            def,
            query::stable_incremental_key(stable_def, signature),
        },
    };
}

[[nodiscard]] QueryContextGenericInstanceSignatureSubject test_generic_instance_signature_subject(
    const std::string_view template_name, const query::BuiltinTypeKey type_arg, const std::string_view signature)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const std::array<std::string_view, 1> template_path{template_name};
    const query::DefKey template_def =
        query::def_key(module, query::DefNamespace::value, query::DefKind::generic_template, template_path);
    const std::array<query::CanonicalTypeKey, 1> type_args{query::canonical_builtin(type_arg)};
    const query::GenericInstanceKey key = query::generic_instance_key(template_def, type_args,
        std::span<const query::StableFingerprint128>{}, query::param_env_key(std::span<const std::string_view>{}));

    const std::array<std::string_view, 2> stable_module_path{"regex", "vm"};
    const query::StableModuleId stable_module = query::stable_module_id(stable_module_path);
    const query::StableDefId stable_def =
        query::stable_definition_id(stable_module, query::StableSymbolKind::function, template_name);
    return QueryContextGenericInstanceSignatureSubject{
        key,
        query::stable_incremental_key(stable_def, signature),
    };
}

[[nodiscard]] query::GenericInstanceSignatureProviderInput generic_instance_provider_input(
    const QueryContextGenericInstanceSignatureSubject& subject) noexcept
{
    return query::GenericInstanceSignatureProviderInput{
        &subject.key,
        subject.signature,
    };
}

[[nodiscard]] bool query_test_key_less(const query::QueryKey lhs, const query::QueryKey rhs) noexcept
{
    return std::tie(
               lhs.kind, lhs.schema, lhs.global_id, lhs.payload.primary, lhs.payload.secondary, lhs.payload.byte_count)
        < std::tie(
            rhs.kind, rhs.schema, rhs.global_id, rhs.payload.primary, rhs.payload.secondary, rhs.payload.byte_count);
}

void sort_query_test_keys(std::vector<query::QueryKey>& keys)
{
    std::sort(keys.begin(), keys.end(), query_test_key_less);
}

[[nodiscard]] bool contains_query_dependency_edge(
    const std::vector<query::QueryDependencyEdge>& edges, const query::QueryDependencyEdge& edge)
{
    return std::find(edges.begin(), edges.end(), edge) != edges.end();
}

} // namespace

TEST(QueryUnit, StableHashAndWriterSerializationAreDeterministic)
{
    const query::StableFingerprint128 alpha = query::stable_fingerprint("alpha");
    const query::StableFingerprint128 repeated_alpha = query::stable_fingerprint("alpha");
    const query::StableFingerprint128 beta = query::stable_fingerprint("beta");
    EXPECT_EQ(alpha, repeated_alpha);
    EXPECT_NE(alpha, beta);
    EXPECT_GT(alpha.byte_count, 0U);

    const std::array<std::string_view, 2> split_a{"a", "bc"};
    const std::array<std::string_view, 2> split_b{"ab", "c"};
    EXPECT_NE(query::stable_fingerprint(split_a), query::stable_fingerprint(split_b));

    query::StableKeyWriter writer;
    writer.write_string("payload");
    writer.write_u32(QUERY_TEST_WRITER_VALUE);

    query::StableKeyWriter repeated_writer;
    repeated_writer.write_string("payload");
    repeated_writer.write_u32(QUERY_TEST_WRITER_VALUE);

    EXPECT_EQ(writer.storage(), repeated_writer.storage());
    EXPECT_EQ(writer.fingerprint(), repeated_writer.fingerprint());
    EXPECT_FALSE(writer.storage().empty());
}

TEST(QueryUnit, StableHashWriterCoversPrimitiveAndFingerprintFields)
{
    const query::StableFingerprint128 alpha = query::stable_fingerprint("alpha");
    query::StableHashBuilder builder;
    builder.mix_u8(1);
    builder.mix_u16(2);
    builder.mix_u32(3);
    builder.mix_u64(4);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_string("payload");
    builder.mix_fingerprint(alpha);
    const query::StableFingerprint128 fingerprint = builder.finish();

    query::StableKeyWriter writer;
    writer.write_u8(1);
    writer.write_u16(2);
    writer.write_u32(3);
    writer.write_u64(4);
    writer.write_bool(true);
    writer.write_string("payload");
    writer.write_fingerprint(alpha);

    EXPECT_FALSE(writer.bytes().empty());
    EXPECT_FALSE(query::debug_string(fingerprint).empty());
    EXPECT_NE(query::stable_hash_value(fingerprint), 0U);
    EXPECT_EQ(query::StableFingerprintHash{}(fingerprint), query::stable_hash_value(fingerprint));
}

TEST(QueryUnit, StableSemanticKeysSeparateFilesModulesDefinitionsAndBodies)
{
    const query::PackageKey package = test_package();
    ASSERT_TRUE(query::is_valid(package));

    const query::FileKey source_file = query::file_key(package, "/workspace/root/regex/vm.ax");
    const query::FileKey virtual_file =
        query::file_key(package, "/workspace/root/regex/vm.ax", query::SourceRole::virtual_buffer, "buffer:1");
    EXPECT_TRUE(query::is_valid(source_file));
    EXPECT_NE(source_file, virtual_file);
    EXPECT_NE(query::stable_key_fingerprint(source_file), query::stable_key_fingerprint(virtual_file));

    const query::ModuleKey module = test_module(package);
    const std::array<std::string_view, 2> other_path{"regex_vm", "root"};
    const query::ModuleKey other_module = query::module_key(package, other_path);
    EXPECT_TRUE(query::is_valid(module));
    EXPECT_NE(module, other_module);

    const query::DefKey function_def = test_function_def(module);
    const std::array<std::string_view, 1> duplicate_path{"compute"};
    const query::DefKey overloaded_function_def = query::def_key(
        module, query::DefNamespace::value, query::DefKind::function, duplicate_path, QUERY_TEST_DISAMBIGUATOR);
    EXPECT_TRUE(query::is_valid(function_def));
    EXPECT_NE(function_def, overloaded_function_def);
    EXPECT_EQ(query::stable_serialize(function_def), query::stable_serialize(function_def));

    const query::BodyKey function_body =
        query::body_key(function_def, query::BodySlotKind::function_body, QUERY_TEST_STABLE_ORDINAL);
    const query::BodyKey default_arg_body =
        query::body_key(function_def, query::BodySlotKind::default_argument, QUERY_TEST_STABLE_ORDINAL);
    EXPECT_TRUE(query::is_valid(function_body));
    EXPECT_NE(function_body, default_arg_body);

    const query::QueryKey signature_query =
        query::query_key(query::QueryKind::item_signature, query::stable_key_fingerprint(function_def));
    EXPECT_TRUE(query::is_valid(signature_query));
    EXPECT_NE(query::debug_string(signature_query).find("QueryKey"), std::string::npos);
}

TEST(QueryUnit, QueryKeysSerializeFingerprintHashAndDebugEveryPublicKeyShape)
{
    const query::PackageKey package = test_package();
    const query::FileKey source_file = query::file_key(package, "/workspace/root/regex/vm.ax");
    const query::ModuleKey module = test_module(package);
    const query::ModulePartKey module_part =
        query::module_part_key(module, source_file, query::ModulePartKind::fragment, "part", QUERY_TEST_STABLE_ORDINAL);
    const query::DefKey function_def = test_function_def(module);
    const query::MemberKey member =
        query::member_key(function_def, query::MemberKind::struct_field, "field", QUERY_TEST_STABLE_ORDINAL);
    const query::BodyKey body =
        query::body_key(function_def, query::BodySlotKind::const_initializer, QUERY_TEST_STABLE_ORDINAL);
    const query::GenericParamKey generic_param =
        query::generic_param_key(function_def, QUERY_TEST_GENERIC_PARAM_INDEX, query::GenericParamKind::resource);
    const query::QueryKey diagnostics_query =
        query::query_key(query::QueryKind::diagnostics, query::stable_key_fingerprint(member));

    EXPECT_TRUE(query::is_valid(package));
    EXPECT_TRUE(query::is_valid(source_file));
    EXPECT_TRUE(query::is_valid(module));
    EXPECT_TRUE(query::is_valid(function_def));
    EXPECT_TRUE(query::is_valid(member));
    EXPECT_TRUE(query::is_valid(body));
    EXPECT_TRUE(query::is_valid(generic_param));
    EXPECT_TRUE(query::is_valid(diagnostics_query));
    EXPECT_FALSE(query::is_valid(query::PackageKey{}));
    EXPECT_FALSE(query::is_valid(query::FileKey{}));
    EXPECT_FALSE(query::is_valid(query::ModuleKey{}));
    EXPECT_FALSE(query::is_valid(query::DefKey{}));
    EXPECT_FALSE(query::is_valid(query::MemberKey{}));
    EXPECT_FALSE(query::is_valid(query::BodyKey{}));
    EXPECT_FALSE(query::is_valid(query::GenericParamKey{}));
    EXPECT_FALSE(query::is_valid(query::QueryKey{}));
    query::FileKey zero_global_file = source_file;
    zero_global_file.global_id = 0;
    query::ModuleKey zero_global_module = module;
    zero_global_module.global_id = 0;
    query::DefKey invalid_kind_def = function_def;
    invalid_kind_def.kind = query::DefKind::invalid;
    query::DefKey zero_global_def = function_def;
    zero_global_def.global_id = 0;
    query::MemberKey invalid_kind_member = member;
    invalid_kind_member.kind = query::MemberKind::invalid;
    query::MemberKey zero_global_member = member;
    zero_global_member.global_id = 0;
    query::BodyKey zero_global_body = body;
    zero_global_body.global_id = 0;
    query::GenericParamKey zero_global_generic_param = generic_param;
    zero_global_generic_param.global_id = 0;
    query::QueryKey zero_global_query = diagnostics_query;
    zero_global_query.global_id = 0;
    EXPECT_FALSE(query::is_valid(zero_global_file));
    EXPECT_FALSE(query::is_valid(zero_global_module));
    EXPECT_FALSE(query::is_valid(invalid_kind_def));
    EXPECT_FALSE(query::is_valid(zero_global_def));
    EXPECT_FALSE(query::is_valid(invalid_kind_member));
    EXPECT_FALSE(query::is_valid(zero_global_member));
    EXPECT_FALSE(query::is_valid(zero_global_body));
    EXPECT_FALSE(query::is_valid(zero_global_generic_param));
    EXPECT_FALSE(query::is_valid(zero_global_query));

    EXPECT_FALSE(query::stable_serialize(package).empty());
    EXPECT_FALSE(query::stable_serialize(source_file).empty());
    EXPECT_FALSE(query::stable_serialize(module).empty());
    EXPECT_FALSE(query::stable_serialize(module_part).empty());
    EXPECT_FALSE(query::stable_serialize(function_def).empty());
    EXPECT_FALSE(query::stable_serialize(member).empty());
    EXPECT_FALSE(query::stable_serialize(body).empty());
    EXPECT_FALSE(query::stable_serialize(generic_param).empty());
    EXPECT_FALSE(query::stable_serialize(diagnostics_query).empty());

    EXPECT_GT(query::stable_key_fingerprint(package).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(source_file).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(module).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(module_part).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(function_def).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(member).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(body).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(generic_param).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(diagnostics_query).byte_count, 0U);

    EXPECT_NE(query::debug_string(package).find("PackageKey"), std::string::npos);
    EXPECT_NE(query::debug_string(source_file).find("FileKey"), std::string::npos);
    EXPECT_NE(query::debug_string(module).find("ModuleKey"), std::string::npos);
    EXPECT_NE(query::debug_string(module_part).find("ModulePartKey"), std::string::npos);
    EXPECT_NE(query::debug_string(function_def).find("DefKey"), std::string::npos);
    EXPECT_NE(query::debug_string(member).find("MemberKey"), std::string::npos);
    EXPECT_NE(query::debug_string(body).find("BodyKey"), std::string::npos);
    EXPECT_NE(query::debug_string(generic_param).find("GenericParamKey"), std::string::npos);
    EXPECT_NE(query::debug_string(diagnostics_query).find("QueryKey"), std::string::npos);

    EXPECT_NE(query::PackageKeyHash{}(package), 0U);
    EXPECT_NE(query::FileKeyHash{}(source_file), 0U);
    EXPECT_NE(query::ModuleKeyHash{}(module), 0U);
    EXPECT_NE(query::DefKeyHash{}(function_def), 0U);
    EXPECT_NE(query::QueryKeyHash{}(diagnostics_query), 0U);
}

TEST(QueryUnit, LegacyStableIdentityLivesInQueryLayer)
{
    const query::StableModuleId empty_module = query::stable_module_id(std::span<const std::string_view>{});
    const std::array<std::string_view, 2> dotted_path{"a", "b_c"};
    const std::array<std::string_view, 2> underscore_path{"a_b", "c"};
    const query::StableModuleId dotted_module = query::stable_module_id(dotted_path);
    const query::StableModuleId repeated_dotted_module = query::stable_module_id(dotted_path);
    const query::StableModuleId underscore_module = query::stable_module_id(underscore_path);

    EXPECT_EQ(empty_module.path.primary, QUERY_TEST_LEGACY_EMPTY_MODULE_PATH_PRIMARY);
    EXPECT_EQ(empty_module.path.secondary, QUERY_TEST_LEGACY_EMPTY_MODULE_PATH_SECONDARY);
    EXPECT_EQ(empty_module.path.byte_count, QUERY_TEST_LEGACY_EMPTY_MODULE_PATH_BYTES);
    EXPECT_EQ(empty_module.global_id, QUERY_TEST_LEGACY_EMPTY_MODULE_GLOBAL_ID);
    EXPECT_TRUE(query::is_valid(dotted_module));
    EXPECT_EQ(query::stable_identity_fingerprint(dotted_path), dotted_module.path);
    EXPECT_EQ(dotted_module.path.primary, QUERY_TEST_LEGACY_DOTTED_MODULE_PATH_PRIMARY);
    EXPECT_EQ(dotted_module.path.secondary, QUERY_TEST_LEGACY_DOTTED_MODULE_PATH_SECONDARY);
    EXPECT_EQ(dotted_module.path.byte_count, QUERY_TEST_LEGACY_DOTTED_MODULE_PATH_BYTES);
    EXPECT_EQ(dotted_module.global_id, QUERY_TEST_LEGACY_DOTTED_MODULE_GLOBAL_ID);
    EXPECT_EQ(dotted_module, repeated_dotted_module);
    EXPECT_NE(dotted_module, underscore_module);

    const query::StableDefId function_id =
        query::stable_definition_id(dotted_module, query::StableSymbolKind::function, "compute");
    const query::StableDefId value_id =
        query::stable_definition_id(dotted_module, query::StableSymbolKind::value, "compute");
    EXPECT_TRUE(query::is_valid(function_id));
    EXPECT_NE(function_id, value_id);
    EXPECT_EQ(function_id.global_id, QUERY_TEST_LEGACY_FUNCTION_GLOBAL_ID);
    EXPECT_EQ(value_id.global_id, QUERY_TEST_LEGACY_VALUE_GLOBAL_ID);

    const query::StableMemberKey field_key =
        query::stable_member_key(function_id, query::StableSymbolKind::struct_field, "x");
    EXPECT_TRUE(query::is_valid(field_key));
    EXPECT_EQ(field_key.global_id, QUERY_TEST_LEGACY_FIELD_GLOBAL_ID);

    const query::IncrementalKey incremental_key = query::stable_incremental_key(function_id, "signature:i32");
    EXPECT_TRUE(query::is_valid(incremental_key));
    EXPECT_EQ(incremental_key.global_id, QUERY_TEST_LEGACY_INCREMENTAL_GLOBAL_ID);
    EXPECT_FALSE(query::stable_serialize(dotted_module).empty());
    EXPECT_FALSE(query::stable_serialize(function_id).empty());
    EXPECT_FALSE(query::stable_serialize(field_key).empty());
    EXPECT_FALSE(query::stable_serialize(incremental_key).empty());
    EXPECT_GT(query::stable_key_fingerprint(dotted_module).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(function_id).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(field_key).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(incremental_key).byte_count, 0U);
    EXPECT_NE(query::debug_string(dotted_module).find("StableModuleId"), std::string::npos);
    EXPECT_NE(query::debug_string(function_id).find("StableDefId"), std::string::npos);
    EXPECT_NE(query::debug_string(field_key).find("StableMemberKey"), std::string::npos);
    EXPECT_NE(query::debug_string(incremental_key).find("IncrementalKey"), std::string::npos);
    EXPECT_FALSE(query::is_valid(query::StableModuleId{}));
    EXPECT_FALSE(query::is_valid(query::StableDefId{}));
    EXPECT_FALSE(query::is_valid(query::StableMemberKey{}));
    EXPECT_FALSE(query::is_valid(query::IncrementalKey{}));
}

TEST(QueryUnit, QueryRecordsBindTypedKeysToResultFingerprints)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::DefKey function_def = test_function_def(module);
    const std::array<std::string_view, 2> stable_module_path{"regex", "vm"};
    const query::StableModuleId stable_module = query::stable_module_id(stable_module_path);
    const query::StableDefId legacy_function_id =
        query::stable_definition_id(stable_module, query::StableSymbolKind::function, "compute");
    const query::IncrementalKey incremental_key = query::stable_incremental_key(legacy_function_id, "signature:i32");
    const query::QueryResultFingerprint result = query::query_result_fingerprint(incremental_key);

    ASSERT_TRUE(query::is_valid(result));
    EXPECT_EQ(result.fingerprint, incremental_key.fingerprint);
    EXPECT_EQ(result.global_id, incremental_key.global_id);

    const query::ItemSignatureQueryInput item_input{
        function_def,
        result,
    };
    EXPECT_TRUE(query::is_valid(item_input));
    const std::optional<query::QueryRecord> item_record = query::item_signature_query_record(item_input);
    ASSERT_TRUE(item_record.has_value());
    EXPECT_TRUE(query::is_valid(*item_record));
    EXPECT_EQ(item_record->key.kind, query::QueryKind::item_signature);
    EXPECT_EQ(item_record->key.payload, query::stable_key_fingerprint(function_def));
    EXPECT_EQ(item_record->stable_key_bytes, query::stable_serialize(function_def));
    EXPECT_EQ(item_record->result, result);
    EXPECT_EQ(query::query_record_change_status(nullptr, *item_record), query::QueryRecordChangeStatus::missing);
    EXPECT_EQ(
        query::query_record_change_status(&*item_record, *item_record), query::QueryRecordChangeStatus::unchanged);

    const query::DefKey vector_template = test_template_def(module);
    const query::CanonicalTypeKey i32 = query::canonical_builtin(query::BuiltinTypeKey::i32);
    const std::array<query::CanonicalTypeKey, 1> type_args{i32};
    const query::GenericInstanceKey instance = query::generic_instance_key(vector_template, type_args,
        std::span<const query::StableFingerprint128>{}, query::param_env_key(std::span<const std::string_view>{}));
    const query::GenericInstanceSignatureQueryInput generic_input{
        instance,
        result,
    };
    EXPECT_TRUE(query::is_valid(generic_input));
    const std::optional<query::QueryRecord> instance_record =
        query::generic_instance_signature_query_record(generic_input);
    ASSERT_TRUE(instance_record.has_value());
    EXPECT_TRUE(query::is_valid(*instance_record));
    EXPECT_EQ(instance_record->key.kind, query::QueryKind::generic_instance_signature);
    EXPECT_EQ(instance_record->key.payload, query::stable_key_fingerprint(instance));
    EXPECT_EQ(instance_record->stable_key_bytes, query::stable_serialize(instance));

    query::QueryRecord changed_item_record = *item_record;
    changed_item_record.result =
        query::query_result_fingerprint(query::stable_incremental_key(legacy_function_id, "signature:i64"));
    EXPECT_EQ(
        query::query_record_change_status(&*item_record, changed_item_record), query::QueryRecordChangeStatus::changed);

    query::QueryRecord mismatched_identity_record = *instance_record;
    mismatched_identity_record.result = item_record->result;
    EXPECT_EQ(query::query_record_change_status(&*item_record, mismatched_identity_record),
        query::QueryRecordChangeStatus::malformed);

    EXPECT_FALSE(query::is_valid(query::QueryResultFingerprint{}));
    EXPECT_FALSE(query::is_valid(query::QueryRecord{}));
    EXPECT_FALSE(query::is_valid(query::query_result_fingerprint(query::IncrementalKey{})));
    EXPECT_FALSE(query::query_record(query::QueryKind::invalid, query::stable_key_fingerprint(function_def),
        query::stable_serialize(function_def), result)
            .has_value());
    EXPECT_FALSE(
        query::query_record(query::QueryKind::item_signature, query::stable_key_fingerprint(function_def), "", result)
            .has_value());
    EXPECT_FALSE(query::is_valid(query::ItemSignatureQueryInput{}));
    EXPECT_FALSE(
        query::item_signature_query_record(query::ItemSignatureQueryInput{query::DefKey{}, result}).has_value());
    EXPECT_FALSE(query::item_signature_query_record(
        query::ItemSignatureQueryInput{function_def, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::GenericInstanceSignatureQueryInput{}));
    EXPECT_FALSE(query::generic_instance_signature_query_record(
        query::GenericInstanceSignatureQueryInput{query::GenericInstanceKey{}, result})
            .has_value());
    EXPECT_FALSE(query::generic_instance_signature_query_record(
        query::GenericInstanceSignatureQueryInput{instance, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::item_signature_query_record(query::DefKey{}, result).has_value());
    EXPECT_FALSE(query::item_signature_query_record(function_def, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::generic_instance_signature_query_record(query::GenericInstanceKey{}, result).has_value());
    EXPECT_EQ(query::query_record_change_status(&*item_record, query::QueryRecord{}),
        query::QueryRecordChangeStatus::malformed);
    const query::QueryRecord invalid_cached_record;
    EXPECT_EQ(query::query_record_change_status(&invalid_cached_record, *item_record),
        query::QueryRecordChangeStatus::malformed);
}

TEST(QueryUnit, ItemSignatureProviderBuildsRecordFromStableDefinition)
{
    const std::array<std::string_view, 2> stable_module_path{"regex", "vm"};
    const query::StableModuleId stable_module = query::stable_module_id(stable_module_path);
    const query::StableDefId stable_function =
        query::stable_definition_id(stable_module, query::StableSymbolKind::function, "compute");
    const query::DefKey function_def =
        query::def_key_from_stable_id(stable_function, query::DefNamespace::value, query::DefKind::function);
    const query::IncrementalKey signature =
        query::stable_incremental_key(stable_function, QUERY_TEST_PROVIDER_SIGNATURE);

    ASSERT_TRUE(query::is_valid(function_def));
    EXPECT_EQ(function_def.module.global_id, stable_module.global_id);
    EXPECT_EQ(function_def.module.path, stable_module.path);
    EXPECT_EQ(function_def.module.path_component_count, stable_module.part_count);
    EXPECT_EQ(function_def.path, stable_function.name);
    EXPECT_EQ(function_def.disambiguator, stable_function.disambiguator);
    EXPECT_EQ(function_def.global_id, stable_function.global_id);

    const query::ItemSignatureProviderInput input{
        function_def,
        signature,
    };
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::ItemSignatureProviderOutput> output = query::provide_item_signature_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    EXPECT_TRUE(output->dependencies.empty());
    EXPECT_EQ(output->record.key.kind, query::QueryKind::item_signature);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(function_def));
    EXPECT_EQ(output->result, query::query_result_fingerprint(signature));
    EXPECT_EQ(output->record.result, output->result);

    EXPECT_FALSE(query::is_valid(query::ItemSignatureProviderInput{}));
    EXPECT_FALSE(
        query::provide_item_signature_query(query::ItemSignatureProviderInput{query::DefKey{}, signature}).has_value());
    EXPECT_FALSE(
        query::provide_item_signature_query(query::ItemSignatureProviderInput{function_def, query::IncrementalKey{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::ItemSignatureProviderOutput{}));

    query::ItemSignatureProviderOutput invalid_dependency_output = *output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));

    query::ItemSignatureProviderOutput mismatched_result_output = *output;
    mismatched_result_output.result = query::query_result_fingerprint(
        query::stable_incremental_key(stable_function, QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE));
    EXPECT_FALSE(query::is_valid(mismatched_result_output));
}

TEST(QueryUnit, GenericInstanceSignatureProviderBuildsRecordFromStableInstance)
{
    const QueryContextGenericInstanceSignatureSubject subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::QueryKey> expected_key = query::generic_instance_signature_query_key(subject.key);
    ASSERT_TRUE(expected_key.has_value());

    const query::GenericInstanceSignatureProviderInput input = generic_instance_provider_input(subject);
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::GenericInstanceSignatureProviderOutput> output =
        query::provide_generic_instance_signature_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    EXPECT_TRUE(output->dependencies.empty());
    EXPECT_EQ(output->record.key, *expected_key);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::generic_instance_signature);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(subject.key));
    EXPECT_EQ(output->result, query::query_result_fingerprint(subject.signature));
    EXPECT_EQ(output->record.result, output->result);

    const query::GenericInstanceKey invalid_key;
    EXPECT_FALSE(query::generic_instance_signature_query_key(query::GenericInstanceKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::GenericInstanceSignatureProviderInput{}));
    EXPECT_FALSE(query::provide_generic_instance_signature_query(
        query::GenericInstanceSignatureProviderInput{&invalid_key, subject.signature})
            .has_value());
    EXPECT_FALSE(query::provide_generic_instance_signature_query(
        query::GenericInstanceSignatureProviderInput{&subject.key, query::IncrementalKey{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::GenericInstanceSignatureProviderOutput{}));

    query::GenericInstanceSignatureProviderOutput invalid_dependency_output = *output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));

    query::GenericInstanceSignatureProviderOutput mismatched_result_output = *output;
    mismatched_result_output.result = query::query_result_fingerprint(
        query::stable_incremental_key(subject.signature.definition, QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE));
    EXPECT_FALSE(query::is_valid(mismatched_result_output));
}

TEST(QueryUnit, QueryContextCachesItemSignatureAndEmitsCompletedRecords)
{
    const QueryContextItemSignatureSubject subject =
        test_item_signature_subject("compute", QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::QueryKey> expected_key = query::item_signature_query_key(subject.def);
    ASSERT_TRUE(expected_key.has_value());

    base::usize provider_calls = 0;
    query::QueryContext context([&provider_calls](const query::ItemSignatureProviderInput& provider_input) {
        ++provider_calls;
        return query::provide_item_signature_query(provider_input);
    });

    const query::QueryEvaluationResult first = context.evaluate_item_signature(subject.input);
    ASSERT_EQ(first.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(first.node, nullptr);
    EXPECT_EQ(first.node->status, query::QueryNodeStatus::done);
    EXPECT_EQ(first.node->key, *expected_key);
    EXPECT_EQ(first.node->record.key, *expected_key);
    EXPECT_EQ(provider_calls, 1U);
    EXPECT_EQ(context.find(*expected_key), first.node);
    EXPECT_EQ(context.find(query::QueryKey{}), nullptr);

    const std::vector<query::QueryRecord> records = context.completed_records();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records.front().key, first.node->record.key);
    EXPECT_EQ(records.front().result, first.node->record.result);
    EXPECT_EQ(records.front().stable_key_bytes, first.node->record.stable_key_bytes);

    const query::QueryEvaluationResult second = context.evaluate_item_signature(subject.input);
    EXPECT_EQ(second.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(second.node, first.node);
    EXPECT_EQ(provider_calls, 1U);

    context.set_item_signature_provider({});
    const query::QueryEvaluationResult cached_after_reset = context.evaluate_item_signature(subject.input);
    EXPECT_EQ(cached_after_reset.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(provider_calls, 1U);
}

TEST(QueryUnit, QueryContextCachesGenericInstanceSignatureAndEmitsMixedRecords)
{
    const QueryContextItemSignatureSubject item_subject =
        test_item_signature_subject("compute", QUERY_TEST_PROVIDER_SIGNATURE);
    const QueryContextGenericInstanceSignatureSubject generic_subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::QueryKey> generic_key = query::generic_instance_signature_query_key(generic_subject.key);
    ASSERT_TRUE(generic_key.has_value());

    base::usize generic_provider_calls = 0;
    query::QueryContext context(query::ItemSignatureProvider{query::provide_item_signature_query},
        [&generic_provider_calls](const query::GenericInstanceSignatureProviderInput& provider_input) {
            ++generic_provider_calls;
            return query::provide_generic_instance_signature_query(provider_input);
        });

    const query::QueryEvaluationResult generic_first =
        context.evaluate_generic_instance_signature(generic_instance_provider_input(generic_subject));
    ASSERT_EQ(generic_first.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(generic_first.node, nullptr);
    EXPECT_EQ(generic_first.node->status, query::QueryNodeStatus::done);
    EXPECT_EQ(generic_first.node->key, *generic_key);
    EXPECT_EQ(generic_provider_calls, 1U);
    EXPECT_EQ(context.find(*generic_key), generic_first.node);

    const query::QueryEvaluationResult generic_second =
        context.evaluate_generic_instance_signature(generic_instance_provider_input(generic_subject));
    EXPECT_EQ(generic_second.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(generic_second.node, generic_first.node);
    EXPECT_EQ(generic_provider_calls, 1U);

    const query::QueryEvaluationResult item_result = context.evaluate_item_signature(item_subject.input);
    ASSERT_EQ(item_result.status, query::QueryEvaluationStatus::computed);

    const std::vector<query::QueryRecord> records = context.completed_records();
    ASSERT_EQ(records.size(), 2U);
    EXPECT_TRUE(std::is_sorted(
        records.begin(), records.end(), [](const query::QueryRecord& lhs, const query::QueryRecord& rhs) {
            if (lhs.key.kind != rhs.key.kind) {
                return static_cast<base::u8>(lhs.key.kind) < static_cast<base::u8>(rhs.key.kind);
            }
            if (lhs.key.global_id != rhs.key.global_id) {
                return lhs.key.global_id < rhs.key.global_id;
            }
            if (lhs.result.global_id != rhs.result.global_id) {
                return lhs.result.global_id < rhs.result.global_id;
            }
            return lhs.stable_key_bytes < rhs.stable_key_bytes;
        }));
    EXPECT_EQ(records.front().key.kind, query::QueryKind::item_signature);
    EXPECT_EQ(records.back().key.kind, query::QueryKind::generic_instance_signature);

    context.set_generic_instance_signature_provider({});
    const query::QueryEvaluationResult cached_after_reset =
        context.evaluate_generic_instance_signature(generic_instance_provider_input(generic_subject));
    EXPECT_EQ(cached_after_reset.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(generic_provider_calls, 1U);
}

TEST(QueryUnit, QueryContextBuildsDeterministicDependencyEdgeTable)
{
    const QueryContextItemSignatureSubject item_subject =
        test_item_signature_subject("compute", QUERY_TEST_PROVIDER_SIGNATURE);
    const QueryContextGenericInstanceSignatureSubject generic_subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::QueryKey> item_key = query::item_signature_query_key(item_subject.def);
    const std::optional<query::QueryKey> generic_key = query::generic_instance_signature_query_key(generic_subject.key);
    ASSERT_TRUE(item_key.has_value());
    ASSERT_TRUE(generic_key.has_value());

    const query::QueryKey shared_dependency =
        query::query_key(query::QueryKind::module_exports, query::stable_key_fingerprint(item_subject.def.module));
    const query::QueryKey item_only_dependency =
        query::query_key(query::QueryKind::diagnostics, query::stable_key_fingerprint(item_subject.def));

    query::QueryContext context(
        [shared_dependency, item_only_dependency](const query::ItemSignatureProviderInput& provider_input) {
            std::optional<query::ItemSignatureProviderOutput> output =
                query::provide_item_signature_query(provider_input);
            if (output) {
                output->dependencies = {
                    item_only_dependency,
                    shared_dependency,
                    shared_dependency,
                };
            }
            return output;
        },
        [shared_dependency](const query::GenericInstanceSignatureProviderInput& provider_input) {
            std::optional<query::GenericInstanceSignatureProviderOutput> output =
                query::provide_generic_instance_signature_query(provider_input);
            if (output) {
                output->dependencies.push_back(shared_dependency);
            }
            return output;
        });

    const query::QueryEvaluationResult item_result = context.evaluate_item_signature(item_subject.input);
    ASSERT_EQ(item_result.status, query::QueryEvaluationStatus::computed);
    const query::QueryEvaluationResult generic_result =
        context.evaluate_generic_instance_signature(generic_instance_provider_input(generic_subject));
    ASSERT_EQ(generic_result.status, query::QueryEvaluationStatus::computed);

    std::vector<query::QueryKey> expected_item_dependencies{
        shared_dependency,
        item_only_dependency,
    };
    sort_query_test_keys(expected_item_dependencies);
    EXPECT_EQ(context.dependencies_for(*item_key), expected_item_dependencies);
    ASSERT_NE(item_result.node, nullptr);
    EXPECT_EQ(item_result.node->dependencies, expected_item_dependencies);

    const std::vector<query::QueryKey> expected_generic_dependencies{
        shared_dependency,
    };
    EXPECT_EQ(context.dependencies_for(*generic_key), expected_generic_dependencies);
    ASSERT_NE(generic_result.node, nullptr);
    EXPECT_EQ(generic_result.node->dependencies, expected_generic_dependencies);

    std::vector<query::QueryKey> expected_shared_dependents{
        *item_key,
        *generic_key,
    };
    sort_query_test_keys(expected_shared_dependents);
    EXPECT_EQ(context.dependents_of(shared_dependency), expected_shared_dependents);
    EXPECT_EQ(context.dependents_of(item_only_dependency), std::vector<query::QueryKey>{*item_key});
    EXPECT_TRUE(context.dependents_of(query::QueryKey{}).empty());

    EXPECT_TRUE(context.has_dependency(*item_key, shared_dependency));
    EXPECT_TRUE(context.has_dependency(*item_key, item_only_dependency));
    EXPECT_TRUE(context.has_dependency(*generic_key, shared_dependency));
    EXPECT_FALSE(context.has_dependency(*generic_key, item_only_dependency));
    EXPECT_FALSE(context.has_dependency(query::QueryKey{}, shared_dependency));
    EXPECT_EQ(context.dependency_edge_count(), 3U);

    const std::vector<query::QueryDependencyEdge> edges = context.dependency_edges();
    ASSERT_EQ(edges.size(), 3U);
    EXPECT_TRUE(std::is_sorted(
        edges.begin(), edges.end(), [](const query::QueryDependencyEdge& lhs, const query::QueryDependencyEdge& rhs) {
            if (lhs.dependent != rhs.dependent) {
                return query_test_key_less(lhs.dependent, rhs.dependent);
            }
            return query_test_key_less(lhs.dependency, rhs.dependency);
        }));
    EXPECT_TRUE(contains_query_dependency_edge(edges, query::QueryDependencyEdge{*item_key, shared_dependency}));
    EXPECT_TRUE(contains_query_dependency_edge(edges, query::QueryDependencyEdge{*item_key, item_only_dependency}));
    EXPECT_TRUE(contains_query_dependency_edge(edges, query::QueryDependencyEdge{*generic_key, shared_dependency}));
    EXPECT_TRUE(context.dependencies_for(query::QueryKey{}).empty());
}

TEST(QueryUnit, QueryContextSeedsAndInvalidatesCompletedRecordsForCacheReplay)
{
    const QueryContextItemSignatureSubject subject =
        test_item_signature_subject("compute", QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::QueryKey> expected_key = query::item_signature_query_key(subject.def);
    ASSERT_TRUE(expected_key.has_value());

    const std::optional<query::ItemSignatureProviderOutput> output = query::provide_item_signature_query(subject.input);
    ASSERT_TRUE(output.has_value());
    const query::QueryKey dependency =
        query::query_key(query::QueryKind::module_exports, query::stable_key_fingerprint(subject.def.module));

    base::usize provider_calls = 0;
    query::QueryContext context([&provider_calls](const query::ItemSignatureProviderInput& provider_input) {
        ++provider_calls;
        return query::provide_item_signature_query(provider_input);
    });

    EXPECT_FALSE(context.seed_completed_record(query::QueryRecord{}));
    EXPECT_FALSE(context.seed_completed_record(output->record, {query::QueryKey{}}));
    EXPECT_TRUE(context.seed_completed_record(output->record, {dependency, dependency}));
    EXPECT_FALSE(context.seed_completed_record(output->record));
    EXPECT_EQ(context.dependency_edge_count(), 1U);
    EXPECT_EQ(context.dependencies_for(*expected_key), std::vector<query::QueryKey>{dependency});
    EXPECT_TRUE(context.has_dependency(*expected_key, dependency));

    const query::QueryEvaluationResult cached_result = context.evaluate_item_signature(subject.input);
    EXPECT_EQ(cached_result.status, query::QueryEvaluationStatus::cached);
    ASSERT_NE(cached_result.node, nullptr);
    EXPECT_EQ(cached_result.node->record.key, output->record.key);
    EXPECT_EQ(cached_result.node->record.result, output->record.result);
    EXPECT_EQ(cached_result.node->record.stable_key_bytes, output->record.stable_key_bytes);
    EXPECT_EQ(provider_calls, 0U);

    EXPECT_TRUE(context.invalidate(*expected_key));
    EXPECT_FALSE(context.invalidate(*expected_key));
    EXPECT_FALSE(context.invalidate(query::QueryKey{}));
    EXPECT_EQ(context.dependency_edge_count(), 0U);
    EXPECT_TRUE(context.dependencies_for(*expected_key).empty());
    EXPECT_TRUE(context.dependents_of(dependency).empty());
    EXPECT_FALSE(context.has_dependency(*expected_key, dependency));

    const query::QueryEvaluationResult recomputed_result = context.evaluate_item_signature(subject.input);
    EXPECT_EQ(recomputed_result.status, query::QueryEvaluationStatus::computed);
    EXPECT_EQ(provider_calls, 1U);
}

TEST(QueryUnit, QueryReuseDecisionClassifiesCachedCurrentAndMalformedRecords)
{
    const QueryContextItemSignatureSubject subject =
        test_item_signature_subject("compute", QUERY_TEST_PROVIDER_SIGNATURE);
    const QueryContextItemSignatureSubject missing_subject =
        test_item_signature_subject("missing", QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::ItemSignatureProviderOutput> cached_output =
        query::provide_item_signature_query(subject.input);
    const std::optional<query::ItemSignatureProviderOutput> missing_output =
        query::provide_item_signature_query(missing_subject.input);
    ASSERT_TRUE(cached_output.has_value());
    ASSERT_TRUE(missing_output.has_value());

    query::QueryContext cached_context;
    ASSERT_TRUE(cached_context.seed_completed_record(cached_output->record));

    query::QueryRecord changed_record = cached_output->record;
    changed_record.result = query::query_result_fingerprint(
        query::stable_incremental_key(subject.input.signature.definition, QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE));
    ASSERT_TRUE(query::is_valid(changed_record));
    ASSERT_NE(changed_record.result, cached_output->record.result);

    const std::vector<query::QueryRecord> current_records{
        cached_output->record,
        changed_record,
        missing_output->record,
        query::QueryRecord{},
    };
    const std::vector<query::QueryReuseDecision> decisions = query::decide_query_reuse(cached_context, current_records);
    ASSERT_EQ(decisions.size(), current_records.size());

    EXPECT_EQ(decisions[0].key, cached_output->record.key);
    EXPECT_EQ(decisions[0].stable_key_bytes, cached_output->record.stable_key_bytes);
    EXPECT_EQ(decisions[0].change_status, query::QueryRecordChangeStatus::unchanged);
    EXPECT_EQ(decisions[0].disposition, query::QueryReuseDisposition::reuse);
    EXPECT_TRUE(query::can_reuse(decisions[0].change_status));

    EXPECT_EQ(decisions[1].key, changed_record.key);
    EXPECT_EQ(decisions[1].stable_key_bytes, changed_record.stable_key_bytes);
    EXPECT_EQ(decisions[1].change_status, query::QueryRecordChangeStatus::changed);
    EXPECT_EQ(decisions[1].disposition, query::QueryReuseDisposition::recompute);
    EXPECT_FALSE(query::can_reuse(decisions[1].change_status));

    EXPECT_EQ(decisions[2].key, missing_output->record.key);
    EXPECT_EQ(decisions[2].stable_key_bytes, missing_output->record.stable_key_bytes);
    EXPECT_EQ(decisions[2].change_status, query::QueryRecordChangeStatus::missing);
    EXPECT_EQ(decisions[2].disposition, query::QueryReuseDisposition::recompute);

    EXPECT_EQ(decisions[3].change_status, query::QueryRecordChangeStatus::malformed);
    EXPECT_EQ(decisions[3].disposition, query::QueryReuseDisposition::recompute);

    const query::QueryReuseSummary summary = query::summarize_query_reuse(decisions);
    EXPECT_EQ(summary.total, 4U);
    EXPECT_EQ(summary.unchanged, 1U);
    EXPECT_EQ(summary.changed, 1U);
    EXPECT_EQ(summary.missing, 1U);
    EXPECT_EQ(summary.malformed, 1U);
    EXPECT_EQ(summary.reusable, 1U);
    EXPECT_EQ(summary.recompute, 3U);

    const std::vector<query::QueryReuseDecision> missing_decisions =
        query::mark_all_queries_missing(std::span<const query::QueryRecord>{current_records.data(), 2U});
    ASSERT_EQ(missing_decisions.size(), 2U);
    EXPECT_EQ(missing_decisions[0].change_status, query::QueryRecordChangeStatus::missing);
    EXPECT_EQ(missing_decisions[0].disposition, query::QueryReuseDisposition::recompute);
    EXPECT_EQ(missing_decisions[1].change_status, query::QueryRecordChangeStatus::missing);
    EXPECT_EQ(missing_decisions[1].disposition, query::QueryReuseDisposition::recompute);
}

TEST(QueryUnit, QueryContextOrdersCompletedItemSignatureRecordsByQueryKey)
{
    const QueryContextItemSignatureSubject first_subject =
        test_item_signature_subject("compute", QUERY_TEST_PROVIDER_SIGNATURE);
    const QueryContextItemSignatureSubject second_subject =
        test_item_signature_subject("other", QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::QueryKey> first_key = query::item_signature_query_key(first_subject.def);
    const std::optional<query::QueryKey> second_key = query::item_signature_query_key(second_subject.def);
    ASSERT_TRUE(first_key.has_value());
    ASSERT_TRUE(second_key.has_value());
    ASSERT_NE(*first_key, *second_key);

    query::QueryContext context;
    EXPECT_TRUE(context.completed_records().empty());

    const query::QueryEvaluationResult second_result = context.evaluate_item_signature(second_subject.input);
    ASSERT_EQ(second_result.status, query::QueryEvaluationStatus::computed);
    const query::QueryEvaluationResult first_result = context.evaluate_item_signature(first_subject.input);
    ASSERT_EQ(first_result.status, query::QueryEvaluationStatus::computed);

    const std::vector<query::QueryRecord> records = context.completed_records();
    ASSERT_EQ(records.size(), 2U);
    EXPECT_TRUE(std::is_sorted(
        records.begin(), records.end(), [](const query::QueryRecord& lhs, const query::QueryRecord& rhs) {
            return lhs.key.global_id < rhs.key.global_id;
        }));
    const query::QueryKey expected_front = first_key->global_id < second_key->global_id ? *first_key : *second_key;
    const query::QueryKey expected_back = first_key->global_id < second_key->global_id ? *second_key : *first_key;
    EXPECT_EQ(records.front().key, expected_front);
    EXPECT_EQ(records.back().key, expected_back);
}

TEST(QueryUnit, QueryContextTracksGenericInstanceDependenciesFailuresAndCycles)
{
    const QueryContextGenericInstanceSignatureSubject subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::QueryKey> expected_key = query::generic_instance_signature_query_key(subject.key);
    ASSERT_TRUE(expected_key.has_value());
    const query::QueryKey dependency =
        query::query_key(query::QueryKind::generic_template_signature, query::stable_key_fingerprint(subject.key));

    query::QueryContext dependency_context;
    dependency_context.set_generic_instance_signature_provider(
        [dependency](const query::GenericInstanceSignatureProviderInput& provider_input) {
            std::optional<query::GenericInstanceSignatureProviderOutput> output =
                query::provide_generic_instance_signature_query(provider_input);
            if (output) {
                output->dependencies.push_back(dependency);
            }
            return output;
        });
    const query::QueryEvaluationResult dependency_result =
        dependency_context.evaluate_generic_instance_signature(generic_instance_provider_input(subject));
    ASSERT_EQ(dependency_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(dependency_result.node, nullptr);
    ASSERT_EQ(dependency_result.node->dependencies.size(), 1U);
    EXPECT_EQ(dependency_result.node->dependencies.front(), dependency);

    const query::QueryEvaluationResult null_key_result = dependency_context.evaluate_generic_instance_signature(
        query::GenericInstanceSignatureProviderInput{nullptr, subject.signature});
    EXPECT_EQ(null_key_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(null_key_result.node, nullptr);

    const query::GenericInstanceKey invalid_key;
    const query::QueryEvaluationResult invalid_key_result = dependency_context.evaluate_generic_instance_signature(
        query::GenericInstanceSignatureProviderInput{&invalid_key, subject.signature});
    EXPECT_EQ(invalid_key_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(invalid_key_result.node, nullptr);

    query::QueryContext failing_context;
    failing_context.set_generic_instance_signature_provider(
        [](const query::GenericInstanceSignatureProviderInput&)
            -> std::optional<query::GenericInstanceSignatureProviderOutput> {
            return std::nullopt;
        });
    const query::QueryEvaluationResult failed_result =
        failing_context.evaluate_generic_instance_signature(generic_instance_provider_input(subject));
    ASSERT_EQ(failed_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(failed_result.node, nullptr);
    EXPECT_EQ(failed_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(failing_context.completed_records().empty());

    failing_context.set_generic_instance_signature_provider({});
    const query::QueryEvaluationResult retry_result =
        failing_context.evaluate_generic_instance_signature(generic_instance_provider_input(subject));
    ASSERT_EQ(retry_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(retry_result.node, nullptr);
    EXPECT_EQ(retry_result.node->status, query::QueryNodeStatus::done);

    const QueryContextGenericInstanceSignatureSubject other_subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i64, QUERY_TEST_PROVIDER_SIGNATURE);
    query::QueryContext wrong_key_context;
    wrong_key_context.set_generic_instance_signature_provider(
        [&other_subject](const query::GenericInstanceSignatureProviderInput&) {
            return query::provide_generic_instance_signature_query(generic_instance_provider_input(other_subject));
        });
    const query::QueryEvaluationResult wrong_key_result =
        wrong_key_context.evaluate_generic_instance_signature(generic_instance_provider_input(subject));
    ASSERT_EQ(wrong_key_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(wrong_key_result.node, nullptr);
    EXPECT_EQ(wrong_key_result.node->key, *expected_key);
    EXPECT_EQ(wrong_key_result.node->status, query::QueryNodeStatus::failed);

    query::QueryContext invalid_output_context;
    invalid_output_context.set_generic_instance_signature_provider(
        [](const query::GenericInstanceSignatureProviderInput& provider_input) {
            std::optional<query::GenericInstanceSignatureProviderOutput> output =
                query::provide_generic_instance_signature_query(provider_input);
            if (output) {
                output->dependencies.push_back(query::QueryKey{});
            }
            return output;
        });
    const query::QueryEvaluationResult invalid_output_result =
        invalid_output_context.evaluate_generic_instance_signature(generic_instance_provider_input(subject));
    ASSERT_EQ(invalid_output_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(invalid_output_result.node, nullptr);
    EXPECT_EQ(invalid_output_result.node->key, *expected_key);
    EXPECT_EQ(invalid_output_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(invalid_output_context.completed_records().empty());

    query::QueryContext cyclic_context;
    query::QueryEvaluationResult nested_result;
    cyclic_context.set_generic_instance_signature_provider(
        [&cyclic_context, &nested_result](const query::GenericInstanceSignatureProviderInput& provider_input) {
            nested_result = cyclic_context.evaluate_generic_instance_signature(provider_input);
            return query::provide_generic_instance_signature_query(provider_input);
        });
    const query::QueryEvaluationResult cyclic_result =
        cyclic_context.evaluate_generic_instance_signature(generic_instance_provider_input(subject));
    EXPECT_EQ(nested_result.status, query::QueryEvaluationStatus::cycle);
    ASSERT_EQ(cyclic_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(cyclic_result.node, nullptr);
    EXPECT_EQ(cyclic_result.node->status, query::QueryNodeStatus::done);
}

TEST(QueryUnit, QueryContextTracksDependenciesFailuresAndCycles)
{
    const QueryContextItemSignatureSubject subject =
        test_item_signature_subject("compute", QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::QueryKey> expected_key = query::item_signature_query_key(subject.def);
    ASSERT_TRUE(expected_key.has_value());
    const query::QueryKey dependency =
        query::query_key(query::QueryKind::module_exports, query::stable_key_fingerprint(subject.def.module));

    query::QueryContext dependency_context([dependency](const query::ItemSignatureProviderInput& provider_input) {
        std::optional<query::ItemSignatureProviderOutput> output = query::provide_item_signature_query(provider_input);
        if (output) {
            output->dependencies.push_back(dependency);
        }
        return output;
    });
    const query::QueryEvaluationResult dependency_result = dependency_context.evaluate_item_signature(subject.input);
    ASSERT_EQ(dependency_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(dependency_result.node, nullptr);
    ASSERT_EQ(dependency_result.node->dependencies.size(), 1U);
    EXPECT_EQ(dependency_result.node->dependencies.front(), dependency);

    const query::QueryEvaluationResult invalid_key_result = dependency_context.evaluate_item_signature(
        query::ItemSignatureProviderInput{query::DefKey{}, subject.input.signature});
    EXPECT_EQ(invalid_key_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(invalid_key_result.node, nullptr);

    query::QueryContext failing_context(
        [](const query::ItemSignatureProviderInput&) -> std::optional<query::ItemSignatureProviderOutput> {
            return std::nullopt;
        });
    const query::QueryEvaluationResult failed_result = failing_context.evaluate_item_signature(subject.input);
    ASSERT_EQ(failed_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(failed_result.node, nullptr);
    EXPECT_EQ(failed_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(failing_context.completed_records().empty());

    failing_context.set_item_signature_provider({});
    const query::QueryEvaluationResult retry_result = failing_context.evaluate_item_signature(subject.input);
    ASSERT_EQ(retry_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(retry_result.node, nullptr);
    EXPECT_EQ(retry_result.node->status, query::QueryNodeStatus::done);

    const QueryContextItemSignatureSubject other_subject =
        test_item_signature_subject("other", QUERY_TEST_PROVIDER_SIGNATURE);
    query::QueryContext wrong_key_context([&other_subject](const query::ItemSignatureProviderInput&) {
        return query::provide_item_signature_query(other_subject.input);
    });
    const query::QueryEvaluationResult wrong_key_result = wrong_key_context.evaluate_item_signature(subject.input);
    ASSERT_EQ(wrong_key_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(wrong_key_result.node, nullptr);
    EXPECT_EQ(wrong_key_result.node->key, *expected_key);
    EXPECT_EQ(wrong_key_result.node->status, query::QueryNodeStatus::failed);

    query::QueryContext invalid_output_context([](const query::ItemSignatureProviderInput& provider_input) {
        std::optional<query::ItemSignatureProviderOutput> output = query::provide_item_signature_query(provider_input);
        if (output) {
            output->dependencies.push_back(query::QueryKey{});
        }
        return output;
    });
    const query::QueryEvaluationResult invalid_output_result =
        invalid_output_context.evaluate_item_signature(subject.input);
    ASSERT_EQ(invalid_output_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(invalid_output_result.node, nullptr);
    EXPECT_EQ(invalid_output_result.node->key, *expected_key);
    EXPECT_EQ(invalid_output_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(invalid_output_context.completed_records().empty());

    query::QueryContext cyclic_context;
    query::QueryEvaluationResult nested_result;
    cyclic_context.set_item_signature_provider(
        [&cyclic_context, &nested_result](const query::ItemSignatureProviderInput& provider_input) {
            nested_result = cyclic_context.evaluate_item_signature(provider_input);
            return query::provide_item_signature_query(provider_input);
        });
    const query::QueryEvaluationResult cyclic_result = cyclic_context.evaluate_item_signature(subject.input);
    EXPECT_EQ(nested_result.status, query::QueryEvaluationStatus::cycle);
    ASSERT_EQ(cyclic_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(cyclic_result.node, nullptr);
    EXPECT_EQ(cyclic_result.node->status, query::QueryNodeStatus::done);
}

TEST(QueryUnit, CanonicalTypeKeyIsStructuralAndHandleFree)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::DefKey vector_template = test_template_def(module);
    const query::DefKey function_def = test_function_def(module);
    const query::GenericParamKey type_param =
        query::generic_param_key(function_def, QUERY_TEST_GENERIC_PARAM_INDEX, query::GenericParamKind::type);

    const query::CanonicalTypeKey i32 = query::canonical_builtin(query::BuiltinTypeKey::i32);
    const query::CanonicalTypeKey i64 = query::canonical_builtin(query::BuiltinTypeKey::i64);
    const query::CanonicalTypeKey type_t = query::canonical_generic_param(type_param);
    const std::array<query::CanonicalTypeKey, 1> vector_args{type_t};
    const query::CanonicalTypeKey vector_t = query::canonical_nominal(vector_template, vector_args);
    const query::CanonicalTypeKey repeated_vector_t = query::canonical_nominal(vector_template, vector_args);
    EXPECT_EQ(vector_t, repeated_vector_t);
    EXPECT_EQ(query::stable_key_fingerprint(vector_t), query::stable_key_fingerprint(repeated_vector_t));

    const query::CanonicalTypeKey ptr_i32 = query::canonical_pointer(query::PointerMutabilityKey::const_, i32);
    const query::CanonicalTypeKey array_i32 = query::canonical_array(QUERY_TEST_ARRAY_COUNT, i32);
    EXPECT_NE(i32, i64);
    EXPECT_NE(ptr_i32, array_i32);

    const std::array<query::CanonicalTypeKey, 2> params{ptr_i32, vector_t};
    const query::CanonicalTypeKey function_type =
        query::canonical_function(query::FunctionCallConvKey::aurex, false, false, params, i32);
    EXPECT_TRUE(query::is_valid(function_type));
    EXPECT_NE(query::debug_string(function_type).find("function"), std::string::npos);
}

TEST(QueryUnit, CanonicalTypeKeyCoversConstProjectionTraitAndUtilityPaths)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::DefKey function_def = test_function_def(module);
    const query::DefKey vector_template = test_template_def(module);
    const query::MemberKey associated_type =
        query::member_key(function_def, query::MemberKind::associated_type, "Item", QUERY_TEST_STABLE_ORDINAL);
    const query::GenericParamKey type_param =
        query::generic_param_key(function_def, QUERY_TEST_GENERIC_PARAM_INDEX, query::GenericParamKind::type);
    const query::CanonicalTypeKey u8 = query::canonical_builtin(query::BuiltinTypeKey::u8);
    const query::CanonicalTypeKey i32 = query::canonical_builtin(query::BuiltinTypeKey::i32);
    const query::CanonicalTypeKey pointer = query::canonical_pointer(query::PointerMutabilityKey::const_, i32);
    const query::CanonicalTypeKey reference = query::canonical_reference(query::PointerMutabilityKey::mut, i32);
    const query::CanonicalTypeKey array = query::canonical_array(QUERY_TEST_ARRAY_COUNT, i32);
    const query::CanonicalTypeKey slice = query::canonical_slice(query::PointerMutabilityKey::const_, i32);
    const std::array<query::CanonicalTypeKey, 2> tuple_elements{u8, i32};
    const query::CanonicalTypeKey tuple = query::canonical_tuple(tuple_elements);
    const query::CanonicalTypeKey generic_param = query::canonical_generic_param(type_param);
    const std::array<query::CanonicalTypeKey, 1> nominal_args{generic_param};
    const query::CanonicalTypeKey nominal = query::canonical_nominal(vector_template, nominal_args);
    const query::CanonicalTypeKey const_arg = query::canonical_const_arg(query::stable_fingerprint("4"));
    const query::CanonicalTypeKey projection = query::canonical_associated_type_projection(i32, associated_type);
    query::CanonicalTypeKey trait_object;
    trait_object.kind = query::CanonicalTypeKind::trait_object;
    query::CanonicalTypeKey invalid_type;
    query::CanonicalTypeKey unknown_kind_type;
    unknown_kind_type.kind = static_cast<query::CanonicalTypeKind>(255);

    EXPECT_TRUE(query::is_valid(const_arg));
    EXPECT_TRUE(query::is_valid(projection));
    EXPECT_TRUE(query::is_valid(trait_object));
    EXPECT_FALSE(query::is_valid(invalid_type));
    EXPECT_FALSE(query::stable_serialize(const_arg).empty());
    EXPECT_FALSE(query::stable_serialize(projection).empty());
    EXPECT_FALSE(query::stable_serialize(trait_object).empty());
    EXPECT_FALSE(query::stable_serialize(invalid_type).empty());
    EXPECT_NE(query::debug_string(i32).find("builtin"), std::string::npos);
    EXPECT_NE(query::debug_string(pointer).find("pointer"), std::string::npos);
    EXPECT_NE(query::debug_string(reference).find("reference"), std::string::npos);
    EXPECT_NE(query::debug_string(array).find("array"), std::string::npos);
    EXPECT_NE(query::debug_string(slice).find("slice"), std::string::npos);
    EXPECT_NE(query::debug_string(tuple).find("tuple"), std::string::npos);
    EXPECT_NE(query::debug_string(nominal).find("nominal"), std::string::npos);
    EXPECT_NE(query::debug_string(generic_param).find("generic_param"), std::string::npos);
    EXPECT_NE(query::debug_string(const_arg).find("const_arg"), std::string::npos);
    EXPECT_NE(query::debug_string(projection).find("associated_type_projection"), std::string::npos);
    EXPECT_NE(query::debug_string(trait_object).find("trait_object"), std::string::npos);
    EXPECT_NE(query::debug_string(invalid_type).find("invalid"), std::string::npos);
    EXPECT_NE(query::debug_string(unknown_kind_type).find("invalid"), std::string::npos);
    EXPECT_NE(query::CanonicalTypeKeyHash{}(projection), 0U);
}

TEST(QueryUnit, CanonicalTypeKeyEqualityRejectsEveryShallowFieldAndNestedChild)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::DefKey vector_template = test_template_def(module);
    const query::DefKey function_def = test_function_def(module);
    const query::DefKey other_template = test_function_def(module);
    const query::MemberKey associated_type =
        query::member_key(function_def, query::MemberKind::associated_type, "Item", QUERY_TEST_STABLE_ORDINAL);
    const query::MemberKey other_associated_type =
        query::member_key(function_def, query::MemberKind::associated_type, "Other", QUERY_TEST_STABLE_ORDINAL + 1);
    const query::GenericParamKey type_param =
        query::generic_param_key(function_def, QUERY_TEST_GENERIC_PARAM_INDEX, query::GenericParamKind::type);
    const query::GenericParamKey const_param =
        query::generic_param_key(function_def, QUERY_TEST_GENERIC_PARAM_INDEX + 1, query::GenericParamKind::const_);

    const query::CanonicalTypeKey i32 = query::canonical_builtin(query::BuiltinTypeKey::i32);
    const query::CanonicalTypeKey i64 = query::canonical_builtin(query::BuiltinTypeKey::i64);
    EXPECT_NE(i32, i64);
    EXPECT_NE(query::canonical_pointer(query::PointerMutabilityKey::const_, i32),
        query::canonical_pointer(query::PointerMutabilityKey::mut, i32));
    EXPECT_NE(
        query::canonical_array(QUERY_TEST_ARRAY_COUNT, i32), query::canonical_array(QUERY_TEST_ARRAY_COUNT + 1, i32));
    EXPECT_NE(query::canonical_function(
                  query::FunctionCallConvKey::aurex, false, false, std::span<const query::CanonicalTypeKey>{}, i32),
        query::canonical_function(
            query::FunctionCallConvKey::c, false, false, std::span<const query::CanonicalTypeKey>{}, i32));
    EXPECT_NE(query::canonical_function(
                  query::FunctionCallConvKey::aurex, false, false, std::span<const query::CanonicalTypeKey>{}, i32),
        query::canonical_function(
            query::FunctionCallConvKey::aurex, true, false, std::span<const query::CanonicalTypeKey>{}, i32));
    EXPECT_NE(query::canonical_function(
                  query::FunctionCallConvKey::aurex, false, false, std::span<const query::CanonicalTypeKey>{}, i32),
        query::canonical_function(
            query::FunctionCallConvKey::aurex, false, true, std::span<const query::CanonicalTypeKey>{}, i32));
    const std::array<query::CanonicalTypeKey, 1> params{i32};
    EXPECT_NE(query::canonical_function(
                  query::FunctionCallConvKey::aurex, false, false, std::span<const query::CanonicalTypeKey>{}, i32),
        query::canonical_function(query::FunctionCallConvKey::aurex, false, false, params, i32));
    EXPECT_NE(query::canonical_nominal(vector_template, std::span<const query::CanonicalTypeKey>{}),
        query::canonical_nominal(other_template, std::span<const query::CanonicalTypeKey>{}));
    EXPECT_NE(query::canonical_generic_param(type_param), query::canonical_generic_param(const_param));
    EXPECT_NE(query::canonical_const_arg(query::stable_fingerprint("N=4")),
        query::canonical_const_arg(query::stable_fingerprint("N=5")));
    EXPECT_NE(query::canonical_associated_type_projection(i32, associated_type),
        query::canonical_associated_type_projection(i32, other_associated_type));
    EXPECT_NE(query::canonical_pointer(query::PointerMutabilityKey::const_, i32),
        query::canonical_pointer(query::PointerMutabilityKey::const_, i64));
}

TEST(QueryUnit, GenericInstanceKeyUsesCanonicalArgumentsAndParamEnvironment)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::DefKey vector_template = test_template_def(module);

    const query::CanonicalTypeKey i32 = query::canonical_builtin(query::BuiltinTypeKey::i32);
    const query::CanonicalTypeKey str = query::canonical_builtin(query::BuiltinTypeKey::str);
    const std::array<query::CanonicalTypeKey, 1> i32_args{i32};
    const std::array<query::CanonicalTypeKey, 1> str_args{str};
    const std::array<std::string_view, 2> predicates{"T: Eq", "T: Hash"};
    const std::array<std::string_view, 1> weaker_predicates{"T: Eq"};
    const query::ParamEnvKey param_env = query::param_env_key(predicates);
    const query::ParamEnvKey weaker_param_env = query::param_env_key(weaker_predicates);
    const std::array<query::StableFingerprint128, 1> const_args{query::stable_fingerprint("N=4")};

    const query::GenericInstanceKey vec_i32 = query::generic_instance_key(
        vector_template, i32_args, std::span<const query::StableFingerprint128>{}, param_env);
    const query::GenericInstanceKey repeated_vec_i32 = query::generic_instance_key(
        vector_template, i32_args, std::span<const query::StableFingerprint128>{}, param_env);
    const query::GenericInstanceKey vec_str = query::generic_instance_key(
        vector_template, str_args, std::span<const query::StableFingerprint128>{}, param_env);
    const query::GenericInstanceKey vec_i32_weaker_env = query::generic_instance_key(
        vector_template, i32_args, std::span<const query::StableFingerprint128>{}, weaker_param_env);
    const query::GenericInstanceKey vec_i32_const =
        query::generic_instance_key(vector_template, i32_args, const_args, param_env);
    query::GenericInstanceKey changed_type_arg = repeated_vec_i32;
    changed_type_arg.type_args.front() = str;
    query::GenericInstanceKey missing_type_arg = repeated_vec_i32;
    missing_type_arg.type_args.clear();

    EXPECT_TRUE(query::is_valid(vec_i32));
    EXPECT_TRUE(query::is_valid(param_env));
    EXPECT_EQ(vec_i32, repeated_vec_i32);
    EXPECT_EQ(query::stable_serialize(vec_i32), query::stable_serialize(repeated_vec_i32));
    EXPECT_NE(vec_i32, vec_str);
    EXPECT_NE(vec_i32, vec_i32_weaker_env);
    EXPECT_NE(vec_i32, vec_i32_const);
    EXPECT_NE(vec_i32, changed_type_arg);
    EXPECT_NE(vec_i32, missing_type_arg);
    EXPECT_FALSE(query::is_valid(query::ParamEnvKey{}));
    EXPECT_FALSE(query::is_valid(query::GenericInstanceKey{}));
    EXPECT_FALSE(query::stable_serialize(param_env).empty());
    EXPECT_GT(query::stable_key_fingerprint(param_env).byte_count, 0U);
    EXPECT_NE(query::debug_string(vec_i32).find("GenericInstanceKey"), std::string::npos);
    EXPECT_NE(query::debug_string(param_env).find("ParamEnvKey"), std::string::npos);
    EXPECT_NE(query::ParamEnvKeyHash{}(param_env), 0U);
    EXPECT_NE(query::GenericInstanceKeyHash{}(vec_i32_const), 0U);
}

} // namespace aurex::test
