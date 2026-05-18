#include <aurex/query/generic_instance_key.hpp>
#include <aurex/query/stable_identity.hpp>

#include <array>
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

[[nodiscard]] query::PackageKey test_package() {
    const std::array<std::string_view, 2> parts {"workspace", "root"};
    return query::package_key(parts);
}

[[nodiscard]] query::ModuleKey test_module(const query::PackageKey package) {
    const std::array<std::string_view, 2> path {"regex", "vm"};
    return query::module_key(package, path);
}

[[nodiscard]] query::DefKey test_template_def(const query::ModuleKey module) {
    const std::array<std::string_view, 1> path {"Vec"};
    return query::def_key(
        module,
        query::DefNamespace::type,
        query::DefKind::generic_template,
        path);
}

[[nodiscard]] query::DefKey test_function_def(const query::ModuleKey module) {
    const std::array<std::string_view, 1> path {"compute"};
    return query::def_key(
        module,
        query::DefNamespace::value,
        query::DefKind::function,
        path);
}

} // namespace

TEST(QueryUnit, StableHashAndWriterSerializationAreDeterministic) {
    const query::StableFingerprint128 alpha = query::stable_fingerprint("alpha");
    const query::StableFingerprint128 repeated_alpha = query::stable_fingerprint("alpha");
    const query::StableFingerprint128 beta = query::stable_fingerprint("beta");
    EXPECT_EQ(alpha, repeated_alpha);
    EXPECT_NE(alpha, beta);
    EXPECT_GT(alpha.byte_count, 0U);

    const std::array<std::string_view, 2> split_a {"a", "bc"};
    const std::array<std::string_view, 2> split_b {"ab", "c"};
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

TEST(QueryUnit, StableSemanticKeysSeparateFilesModulesDefinitionsAndBodies) {
    const query::PackageKey package = test_package();
    ASSERT_TRUE(query::is_valid(package));

    const query::FileKey source_file = query::file_key(package, "/workspace/root/regex/vm.ax");
    const query::FileKey virtual_file = query::file_key(
        package,
        "/workspace/root/regex/vm.ax",
        query::SourceRole::virtual_buffer,
        "buffer:1");
    EXPECT_TRUE(query::is_valid(source_file));
    EXPECT_NE(source_file, virtual_file);
    EXPECT_NE(query::stable_key_fingerprint(source_file), query::stable_key_fingerprint(virtual_file));

    const query::ModuleKey module = test_module(package);
    const std::array<std::string_view, 2> other_path {"regex_vm", "root"};
    const query::ModuleKey other_module = query::module_key(package, other_path);
    EXPECT_TRUE(query::is_valid(module));
    EXPECT_NE(module, other_module);

    const query::DefKey function_def = test_function_def(module);
    const std::array<std::string_view, 1> duplicate_path {"compute"};
    const query::DefKey overloaded_function_def = query::def_key(
        module,
        query::DefNamespace::value,
        query::DefKind::function,
        duplicate_path,
        QUERY_TEST_DISAMBIGUATOR);
    EXPECT_TRUE(query::is_valid(function_def));
    EXPECT_NE(function_def, overloaded_function_def);
    EXPECT_EQ(query::stable_serialize(function_def), query::stable_serialize(function_def));

    const query::BodyKey function_body = query::body_key(
        function_def,
        query::BodySlotKind::function_body,
        QUERY_TEST_STABLE_ORDINAL);
    const query::BodyKey default_arg_body = query::body_key(
        function_def,
        query::BodySlotKind::default_argument,
        QUERY_TEST_STABLE_ORDINAL);
    EXPECT_TRUE(query::is_valid(function_body));
    EXPECT_NE(function_body, default_arg_body);

    const query::QueryKey signature_query = query::query_key(
        query::QueryKind::item_signature,
        query::stable_key_fingerprint(function_def));
    EXPECT_TRUE(query::is_valid(signature_query));
    EXPECT_NE(query::debug_string(signature_query).find("QueryKey"), std::string::npos);
}

TEST(QueryUnit, LegacyStableIdentityLivesInQueryLayer) {
    const query::StableModuleId empty_module = query::stable_module_id(std::span<const std::string_view> {});
    const std::array<std::string_view, 2> dotted_path {"a", "b_c"};
    const std::array<std::string_view, 2> underscore_path {"a_b", "c"};
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

    const query::StableDefId function_id = query::stable_definition_id(
        dotted_module,
        query::StableSymbolKind::function,
        "compute");
    const query::StableDefId value_id = query::stable_definition_id(
        dotted_module,
        query::StableSymbolKind::value,
        "compute");
    EXPECT_TRUE(query::is_valid(function_id));
    EXPECT_NE(function_id, value_id);
    EXPECT_EQ(function_id.global_id, QUERY_TEST_LEGACY_FUNCTION_GLOBAL_ID);
    EXPECT_EQ(value_id.global_id, QUERY_TEST_LEGACY_VALUE_GLOBAL_ID);

    const query::StableMemberKey field_key = query::stable_member_key(
        function_id,
        query::StableSymbolKind::struct_field,
        "x");
    EXPECT_TRUE(query::is_valid(field_key));
    EXPECT_EQ(field_key.global_id, QUERY_TEST_LEGACY_FIELD_GLOBAL_ID);

    const query::IncrementalKey incremental_key = query::stable_incremental_key(function_id, "signature:i32");
    EXPECT_TRUE(query::is_valid(incremental_key));
    EXPECT_EQ(incremental_key.global_id, QUERY_TEST_LEGACY_INCREMENTAL_GLOBAL_ID);
    EXPECT_FALSE(query::stable_serialize(incremental_key).empty());
    EXPECT_NE(query::debug_string(incremental_key).find("IncrementalKey"), std::string::npos);
}

TEST(QueryUnit, CanonicalTypeKeyIsStructuralAndHandleFree) {
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::DefKey vector_template = test_template_def(module);
    const query::DefKey function_def = test_function_def(module);
    const query::GenericParamKey type_param = query::generic_param_key(
        function_def,
        QUERY_TEST_GENERIC_PARAM_INDEX,
        query::GenericParamKind::type);

    const query::CanonicalTypeKey i32 = query::canonical_builtin(query::BuiltinTypeKey::i32);
    const query::CanonicalTypeKey i64 = query::canonical_builtin(query::BuiltinTypeKey::i64);
    const query::CanonicalTypeKey type_t = query::canonical_generic_param(type_param);
    const std::array<query::CanonicalTypeKey, 1> vector_args {type_t};
    const query::CanonicalTypeKey vector_t = query::canonical_nominal(vector_template, vector_args);
    const query::CanonicalTypeKey repeated_vector_t = query::canonical_nominal(vector_template, vector_args);
    EXPECT_EQ(vector_t, repeated_vector_t);
    EXPECT_EQ(query::stable_key_fingerprint(vector_t), query::stable_key_fingerprint(repeated_vector_t));

    const query::CanonicalTypeKey ptr_i32 = query::canonical_pointer(
        query::PointerMutabilityKey::const_,
        i32);
    const query::CanonicalTypeKey array_i32 = query::canonical_array(QUERY_TEST_ARRAY_COUNT, i32);
    EXPECT_NE(i32, i64);
    EXPECT_NE(ptr_i32, array_i32);

    const std::array<query::CanonicalTypeKey, 2> params {ptr_i32, vector_t};
    const query::CanonicalTypeKey function_type = query::canonical_function(
        query::FunctionCallConvKey::aurex,
        false,
        false,
        params,
        i32);
    EXPECT_TRUE(query::is_valid(function_type));
    EXPECT_NE(query::debug_string(function_type).find("function"), std::string::npos);
}

TEST(QueryUnit, GenericInstanceKeyUsesCanonicalArgumentsAndParamEnvironment) {
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::DefKey vector_template = test_template_def(module);

    const query::CanonicalTypeKey i32 = query::canonical_builtin(query::BuiltinTypeKey::i32);
    const query::CanonicalTypeKey str = query::canonical_builtin(query::BuiltinTypeKey::str);
    const std::array<query::CanonicalTypeKey, 1> i32_args {i32};
    const std::array<query::CanonicalTypeKey, 1> str_args {str};
    const std::array<std::string_view, 2> predicates {"T: Eq", "T: Hash"};
    const std::array<std::string_view, 1> weaker_predicates {"T: Eq"};
    const query::ParamEnvKey param_env = query::param_env_key(predicates);
    const query::ParamEnvKey weaker_param_env = query::param_env_key(weaker_predicates);

    const query::GenericInstanceKey vec_i32 = query::generic_instance_key(
        vector_template,
        i32_args,
        std::span<const query::StableFingerprint128> {},
        param_env);
    const query::GenericInstanceKey repeated_vec_i32 = query::generic_instance_key(
        vector_template,
        i32_args,
        std::span<const query::StableFingerprint128> {},
        param_env);
    const query::GenericInstanceKey vec_str = query::generic_instance_key(
        vector_template,
        str_args,
        std::span<const query::StableFingerprint128> {},
        param_env);
    const query::GenericInstanceKey vec_i32_weaker_env = query::generic_instance_key(
        vector_template,
        i32_args,
        std::span<const query::StableFingerprint128> {},
        weaker_param_env);

    EXPECT_TRUE(query::is_valid(vec_i32));
    EXPECT_EQ(vec_i32, repeated_vec_i32);
    EXPECT_EQ(query::stable_serialize(vec_i32), query::stable_serialize(repeated_vec_i32));
    EXPECT_NE(vec_i32, vec_str);
    EXPECT_NE(vec_i32, vec_i32_weaker_env);
    EXPECT_NE(query::debug_string(vec_i32).find("GenericInstanceKey"), std::string::npos);
}

} // namespace aurex::test
