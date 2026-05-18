#include <aurex/query/generic_instance_key.hpp>
#include <aurex/query/query_result.hpp>
#include <aurex/query/stable_identity.hpp>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>

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
