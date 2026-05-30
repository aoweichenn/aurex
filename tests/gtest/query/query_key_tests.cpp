#include <aurex/query/diagnostics_query.hpp>
#include <aurex/query/function_body_syntax_query.hpp>
#include <aurex/query/generic_instance_body_query.hpp>
#include <aurex/query/generic_instance_key.hpp>
#include <aurex/query/generic_instance_signature_query.hpp>
#include <aurex/query/generic_template_signature_query.hpp>
#include <aurex/query/item_list_query.hpp>
#include <aurex/query/item_signature_query.hpp>
#include <aurex/query/lower_function_ir_query.hpp>
#include <aurex/query/module_exports_query.hpp>
#include <aurex/query/module_graph_query.hpp>
#include <aurex/query/module_part_query.hpp>
#include <aurex/query/project_graph_query.hpp>
#include <aurex/query/query_context.hpp>
#include <aurex/query/query_edge_verifier.hpp>
#include <aurex/query/query_executor.hpp>
#include <aurex/query/query_interner.hpp>
#include <aurex/query/query_provider_set.hpp>
#include <aurex/query/query_replay.hpp>
#include <aurex/query/query_result.hpp>
#include <aurex/query/query_reuse.hpp>
#include <aurex/query/source_file_query.hpp>
#include <aurex/query/stable_identity.hpp>
#include <aurex/query/stable_key_decoder.hpp>
#include <aurex/query/type_check_body_query.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u32 QUERY_TEST_DISAMBIGUATOR = 1;
constexpr base::u32 QUERY_TEST_GENERIC_PARAM_INDEX = 0;
constexpr base::u32 QUERY_TEST_STABLE_ORDINAL = 0;
constexpr base::u8 QUERY_TEST_PUBLIC_VISIBILITY_RANK = 2;
constexpr base::u32 QUERY_TEST_VALUE_COMPONENT_COUNT = 1;
constexpr base::u32 QUERY_TEST_GENERIC_PARAM_COUNT = 0;
constexpr base::u64 QUERY_TEST_BODY_RANGE_BEGIN = 1;
constexpr base::u64 QUERY_TEST_BODY_RANGE_END = 42;
constexpr base::u64 QUERY_TEST_ARRAY_COUNT = 4;
constexpr base::u32 QUERY_TEST_WRITER_VALUE = 7;
constexpr base::usize QUERY_TEST_STABLE_U8_WIDTH = 1;
constexpr base::usize QUERY_TEST_STABLE_U64_WIDTH = 8;
constexpr base::u8 QUERY_TEST_TRAILING_STABLE_BYTE = 0x7f;
constexpr base::u8 QUERY_TEST_STABLE_BYTE_FLIP_MASK = 0xff;
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
constexpr base::u32 QUERY_TEST_UNKNOWN_NODE_ID = 999;
constexpr base::usize QUERY_TEST_LIMITED_INTERNER_CAPACITY = 1;
constexpr std::string_view QUERY_TEST_PROVIDER_SIGNATURE = "signature:i32";
constexpr std::string_view QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE = "signature:mismatched-provider-output";
constexpr std::string_view QUERY_TEST_MODULE_EXPORTS_SIGNATURE = "exports:v1";
constexpr std::string_view QUERY_TEST_CHANGED_MODULE_EXPORTS_SIGNATURE = "exports:v2";
constexpr std::string_view QUERY_TEST_REEXPORTED_MODULE_NAME = "reexported";
constexpr std::string_view QUERY_TEST_FILE_CONTENT = "file-content:module regex.vm";
constexpr std::string_view QUERY_TEST_LEX_FILE = "lex-file:tokens";
constexpr std::string_view QUERY_TEST_PARSE_FILE = "parse-file:ast";
constexpr std::string_view QUERY_TEST_MODULE_GRAPH = "module-graph:regex.vm";
constexpr std::string_view QUERY_TEST_PROJECT_GRAPH = "project-graph:workspace";
constexpr std::string_view QUERY_TEST_MODULE_PART = "module-part:regex.vm.primary";
constexpr std::string_view QUERY_TEST_BODY_SYNTAX = "body-syntax:return value";
constexpr std::string_view QUERY_TEST_ITEM_LIST = "item-list:compute";
constexpr std::string_view QUERY_TEST_GENERIC_TEMPLATE_SIGNATURE = "generic-template-signature:T";
constexpr std::string_view QUERY_TEST_TYPE_CHECK_BODY = "type-check-body:return i32";
constexpr std::string_view QUERY_TEST_GENERIC_INSTANCE_BODY = "generic-instance-body:return T";
constexpr std::string_view QUERY_TEST_LOWER_FUNCTION_IR = "lower-function-ir:return i32";
constexpr std::string_view QUERY_TEST_DIAGNOSTICS = "diagnostics:empty";

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

[[nodiscard]] query::ProjectKey test_project_key()
{
    return query::project_key(query::stable_fingerprint(QUERY_TEST_PROJECT_GRAPH));
}

[[nodiscard]] query::ModuleKey test_reexported_module(const query::PackageKey package)
{
    const std::array<std::string_view, 1> path{QUERY_TEST_REEXPORTED_MODULE_NAME};
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

[[nodiscard]] query::ModulePartKey test_primary_module_part(const query::ModuleKey module)
{
    const query::FileKey file = query::file_key(module.package, "/workspace/root/regex/vm.ax");
    return query::module_part_key(module, file, query::ModulePartKind::primary, "<primary>");
}

[[nodiscard]] query::GenericTemplateSignatureAuthority test_template_signature_authority(
    const query::DefKey template_def, const query::IncrementalKey signature, const base::u32 param_count = 1,
    const base::u32 constraint_count = 0)
{
    return query::GenericTemplateSignatureAuthority{
        signature,
        test_primary_module_part(template_def.module),
        template_def.name_space,
        QUERY_TEST_PUBLIC_VISIBILITY_RANK,
        param_count,
        constraint_count,
    };
}

[[nodiscard]] query::ItemSignatureAuthority test_item_signature_authority(
    const query::DefKey def, const query::IncrementalKey signature)
{
    return query::ItemSignatureAuthority{
        signature,
        test_primary_module_part(def.module),
        def.name_space,
        def.kind,
        QUERY_TEST_PUBLIC_VISIBILITY_RANK,
        QUERY_TEST_VALUE_COMPONENT_COUNT,
        QUERY_TEST_GENERIC_PARAM_COUNT,
        true,
        def.kind == query::DefKind::method || def.kind == query::DefKind::trait_method,
        false,
        false,
        true,
    };
}

[[nodiscard]] query::GenericInstanceSignatureAuthority test_generic_instance_signature_authority(
    const query::GenericInstanceKey& key, const query::IncrementalKey signature,
    const query::GenericInstanceSignatureKind kind = query::GenericInstanceSignatureKind::function)
{
    return query::GenericInstanceSignatureAuthority{
        signature,
        kind,
        QUERY_TEST_PUBLIC_VISIBILITY_RANK,
        static_cast<base::u32>(key.type_args.size()),
        static_cast<base::u32>(key.const_args.size()),
        key.param_env.predicate_count,
        1,
        static_cast<base::u32>(key.type_args.size()),
        true,
        false,
        false,
        false,
        true,
    };
}

struct QueryContextItemSignatureSubject {
    query::DefKey def;
    query::ItemSignatureProviderInput input;
};

struct QueryContextGenericInstanceSignatureSubject {
    query::GenericInstanceKey key;
    query::IncrementalKey signature;
    query::GenericInstanceSignatureAuthority authority;
};

struct QueryContextSourceSubject {
    query::FileKey file;
    query::LexFileKey lex_file;
    query::ParseFileKey parse_file;
    query::QueryResultFingerprint content;
    query::QueryResultFingerprint tokens;
    query::QueryResultFingerprint syntax;
};

struct QueryContextBodySubject {
    query::BodyKey body;
    query::FunctionBodySyntaxAuthority syntax_authority;
    query::TypeCheckBodyAuthority type_check_authority;
    query::QueryResultFingerprint lowered_ir;
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
            test_item_signature_authority(def, query::stable_incremental_key(stable_def, signature)),
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
    const query::IncrementalKey incremental_signature = query::stable_incremental_key(stable_def, signature);
    return QueryContextGenericInstanceSignatureSubject{
        key,
        incremental_signature,
        test_generic_instance_signature_authority(key, incremental_signature),
    };
}

[[nodiscard]] query::GenericInstanceSignatureProviderInput generic_instance_provider_input(
    const QueryContextGenericInstanceSignatureSubject& subject) noexcept
{
    return query::GenericInstanceSignatureProviderInput{
        &subject.key,
        subject.authority,
    };
}

[[nodiscard]] query::GenericInstanceBodyProviderInput generic_instance_body_provider_input(
    const QueryContextGenericInstanceSignatureSubject& subject, const query::QueryResultFingerprint result) noexcept
{
    return query::GenericInstanceBodyProviderInput{
        &subject.key,
        query::GenericInstanceBodyAuthority{
            result,
            query::generic_instance_signature_result_fingerprint(subject.authority),
        },
    };
}

[[nodiscard]] QueryContextBodySubject test_body_subject(const std::string_view function_name)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const std::array<std::string_view, 1> path{function_name};
    const query::DefKey function_def =
        query::def_key(module, query::DefNamespace::value, query::DefKind::function, path);
    const query::BodyKey body = query::body_key(function_def, query::BodySlotKind::function_body);
    const query::FunctionBodySyntaxAuthority syntax_authority{
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_BODY_SYNTAX)),
        function_def,
        test_primary_module_part(module),
        QUERY_TEST_BODY_RANGE_BEGIN,
        QUERY_TEST_BODY_RANGE_END,
        body.slot,
        body.ordinal,
    };
    const query::QueryResultFingerprint syntax_result =
        query::function_body_syntax_result_fingerprint(syntax_authority);
    const std::array<std::string_view, 2> stable_module_path{"regex", "vm"};
    const query::StableDefId stable_function = query::stable_definition_id(
        query::stable_module_id(stable_module_path), query::StableSymbolKind::function, function_name);
    const query::ItemSignatureAuthority signature_authority = test_item_signature_authority(
        function_def, query::stable_incremental_key(stable_function, QUERY_TEST_PROVIDER_SIGNATURE));
    const query::TypeCheckBodyAuthority type_check_authority{
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_TYPE_CHECK_BODY)),
        syntax_result,
        query::item_signature_result_fingerprint(signature_authority),
    };
    return QueryContextBodySubject{
        body,
        syntax_authority,
        type_check_authority,
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_LOWER_FUNCTION_IR)),
    };
}

[[nodiscard]] QueryContextSourceSubject test_source_subject()
{
    const query::PackageKey package = test_package();
    const query::FileKey file = query::file_key(package, "/workspace/root/regex/vm.ax");
    const query::LexConfigKey lex_config = query::lex_config_key();
    const query::ParserConfigKey parser_config = query::parser_config_key(lex_config);
    return QueryContextSourceSubject{
        file,
        query::lex_file_key(file, lex_config),
        query::parse_file_key(file, parser_config),
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_FILE_CONTENT)),
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_LEX_FILE)),
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_PARSE_FILE)),
    };
}

[[nodiscard]] QueryContextBodySubject test_body_subject()
{
    return test_body_subject("compute");
}

[[nodiscard]] query::QueryResultFingerprint test_query_result(const std::string_view payload)
{
    return query::query_result_fingerprint(query::stable_fingerprint(payload));
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

[[nodiscard]] std::string query_test_with_trailing_stable_byte(std::string bytes)
{
    bytes.push_back(static_cast<char>(QUERY_TEST_TRAILING_STABLE_BYTE));
    return bytes;
}

[[nodiscard]] std::string query_test_without_last_stable_byte(std::string bytes)
{
    if (!bytes.empty()) {
        bytes.pop_back();
    }
    return bytes;
}

[[nodiscard]] std::string query_test_with_flipped_first_stable_byte(std::string bytes)
{
    if (!bytes.empty()) {
        const auto first_byte = static_cast<base::u8>(static_cast<unsigned char>(bytes.front()));
        bytes.front() = static_cast<char>(first_byte ^ QUERY_TEST_STABLE_BYTE_FLIP_MASK);
    }
    return bytes;
}

void query_test_flip_stable_byte_at(std::string& bytes, const base::usize offset)
{
    if (offset < bytes.size()) {
        const auto byte = static_cast<base::u8>(static_cast<unsigned char>(bytes[offset]));
        bytes[offset] = static_cast<char>(byte ^ QUERY_TEST_STABLE_BYTE_FLIP_MASK);
    }
}

[[nodiscard]] std::string query_test_stable_fingerprint_bytes(const query::StableFingerprint128 fingerprint)
{
    query::StableKeyWriter writer;
    writer.write_fingerprint(fingerprint);
    return writer.storage();
}

[[nodiscard]] query::GenericInstanceKey query_test_generic_instance_with_type_arg(
    const query::DefKey template_def, const query::CanonicalTypeKey& type_arg)
{
    const std::array<query::CanonicalTypeKey, 1> type_args{type_arg};
    return query::generic_instance_key(template_def, type_args, std::span<const query::StableFingerprint128>{},
        query::param_env_key(std::span<const std::string_view>{}));
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

    const query::ProjectKey project = test_project_key();
    EXPECT_TRUE(query::is_valid(project));

    const query::FileKey source_file = query::file_key(package, "/workspace/root/regex/vm.ax");
    const query::FileKey virtual_file =
        query::file_key(package, "/workspace/root/regex/vm.ax", query::SourceRole::virtual_buffer, "buffer:1");
    EXPECT_TRUE(query::is_valid(source_file));
    EXPECT_NE(source_file, virtual_file);
    EXPECT_NE(query::stable_key_fingerprint(source_file), query::stable_key_fingerprint(virtual_file));

    const query::LexConfigKey lex_config = query::lex_config_key();
    const query::LexConfigKey lossless_lex_config = query::lex_config_key(true);
    const query::ParserConfigKey parser_config = query::parser_config_key(lex_config);
    const query::ParserConfigKey lossless_parser_config = query::parser_config_key(lossless_lex_config, true, true);
    const query::LexFileKey lex_file = query::lex_file_key(source_file, lex_config);
    const query::LexFileKey lossless_lex_file = query::lex_file_key(source_file, lossless_lex_config);
    const query::ParseFileKey parse_file = query::parse_file_key(source_file, parser_config);
    const query::ParseFileKey lossless_parse_file = query::parse_file_key(source_file, lossless_parser_config);
    EXPECT_TRUE(query::is_valid(lex_config));
    EXPECT_TRUE(query::is_valid(parser_config));
    EXPECT_TRUE(query::is_valid(lex_file));
    EXPECT_TRUE(query::is_valid(parse_file));
    EXPECT_NE(lex_config, lossless_lex_config);
    EXPECT_NE(parser_config, lossless_parser_config);
    EXPECT_NE(lex_file, lossless_lex_file);
    EXPECT_NE(parse_file, lossless_parse_file);

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

TEST(QueryUnit, QuerySourceStageKeysSeparateSemanticAndLosslessToolingModes)
{
    const QueryContextSourceSubject subject = test_source_subject();
    const std::optional<query::QuerySourceStageKeys> semantic_keys = query::query_source_stage_keys(subject.file);
    const std::optional<query::QuerySourceStageKeys> lossless_keys =
        query::query_source_stage_keys(subject.file, query::QuerySourceStageMode::lossless_tooling);
    ASSERT_TRUE(semantic_keys.has_value());
    ASSERT_TRUE(lossless_keys.has_value());

    EXPECT_FALSE(semantic_keys->lex_config.retain_trivia);
    EXPECT_FALSE(semantic_keys->parser_config.build_lossless_tree);
    EXPECT_TRUE(lossless_keys->lex_config.retain_trivia);
    EXPECT_TRUE(lossless_keys->parser_config.build_lossless_tree);
    EXPECT_EQ(semantic_keys->parser_config.lex_config, semantic_keys->lex_config);
    EXPECT_EQ(lossless_keys->parser_config.lex_config, lossless_keys->lex_config);
    EXPECT_EQ(semantic_keys->file, subject.file);
    EXPECT_EQ(lossless_keys->file, subject.file);
    EXPECT_NE(semantic_keys->lex_file, lossless_keys->lex_file);
    EXPECT_NE(semantic_keys->parse_file, lossless_keys->parse_file);
    EXPECT_EQ(semantic_keys->lex_file.file, subject.file);
    EXPECT_EQ(lossless_keys->parse_file.file, subject.file);
    EXPECT_FALSE(query::query_source_stage_keys(query::FileKey{}).has_value());

    const query::FileKey local_buffer = query::file_key(
        test_package(), "/workspace/root/regex/vm.ax", query::SourceRole::virtual_buffer, "ide-buffer:1");
    const std::optional<query::QuerySourceStageKeys> local_lossless_keys =
        query::query_source_stage_keys(local_buffer, query::QuerySourceStageMode::lossless_tooling);
    ASSERT_TRUE(local_lossless_keys.has_value());
    EXPECT_NE(local_lossless_keys->file, subject.file);
    EXPECT_EQ(local_lossless_keys->lex_file.file, local_buffer);
    EXPECT_EQ(local_lossless_keys->parse_file.file, local_buffer);
    EXPECT_TRUE(local_lossless_keys->lex_config.retain_trivia);

    const query::QueryResultFingerprint lossless_tokens = test_query_result("lex-file:tokens-with-trivia");
    const query::QueryResultFingerprint lossless_syntax = test_query_result("parse-file:lossless-cst");
    const std::optional<query::LexFileProviderOutput> lex_output =
        query::provide_lex_file_query(query::LexFileProviderInput{lossless_keys->lex_file, lossless_tokens});
    ASSERT_TRUE(lex_output.has_value());
    EXPECT_EQ(lex_output->record.result, lossless_tokens);
    ASSERT_EQ(lex_output->dependencies.size(), 1U);
    const std::optional<query::QueryKey> content_query = query::file_content_query_key(subject.file);
    ASSERT_TRUE(content_query.has_value());
    EXPECT_EQ(lex_output->dependencies.front(), *content_query);

    const std::optional<query::ParseFileProviderOutput> parse_output =
        query::provide_parse_file_query(query::ParseFileProviderInput{lossless_keys->parse_file, lossless_syntax});
    ASSERT_TRUE(parse_output.has_value());
    EXPECT_EQ(parse_output->record.result, lossless_syntax);
    ASSERT_EQ(parse_output->dependencies.size(), 1U);
    const std::optional<query::QueryKey> lossless_lex_query = query::lex_file_query_key(lossless_keys->lex_file);
    const std::optional<query::QueryKey> semantic_lex_query = query::lex_file_query_key(semantic_keys->lex_file);
    ASSERT_TRUE(lossless_lex_query.has_value());
    ASSERT_TRUE(semantic_lex_query.has_value());
    EXPECT_EQ(parse_output->dependencies.front(), *lossless_lex_query);
    EXPECT_NE(parse_output->dependencies.front(), *semantic_lex_query);
}

TEST(QueryUnit, QueryKeysSerializeFingerprintHashAndDebugEveryPublicKeyShape)
{
    const query::ProjectKey project = test_project_key();
    const query::PackageKey package = test_package();
    const query::FileKey source_file = query::file_key(package, "/workspace/root/regex/vm.ax");
    const query::LexConfigKey lex_config = query::lex_config_key();
    const query::ParserConfigKey parser_config = query::parser_config_key(lex_config);
    const query::LexFileKey lex_file = query::lex_file_key(source_file, lex_config);
    const query::ParseFileKey parse_file = query::parse_file_key(source_file, parser_config);
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
    EXPECT_TRUE(query::is_valid(lex_config));
    EXPECT_TRUE(query::is_valid(parser_config));
    EXPECT_TRUE(query::is_valid(lex_file));
    EXPECT_TRUE(query::is_valid(parse_file));
    EXPECT_TRUE(query::is_valid(module));
    EXPECT_TRUE(query::is_valid(module_part));
    EXPECT_TRUE(query::is_valid(function_def));
    EXPECT_TRUE(query::is_valid(member));
    EXPECT_TRUE(query::is_valid(body));
    EXPECT_TRUE(query::is_valid(generic_param));
    EXPECT_TRUE(query::is_valid(diagnostics_query));
    EXPECT_FALSE(query::is_valid(query::ProjectKey{}));
    EXPECT_FALSE(query::is_valid(query::PackageKey{}));
    EXPECT_FALSE(query::is_valid(query::FileKey{}));
    EXPECT_FALSE(query::is_valid(query::LexConfigKey{}));
    EXPECT_FALSE(query::is_valid(query::ParserConfigKey{}));
    EXPECT_FALSE(query::is_valid(query::LexFileKey{}));
    EXPECT_FALSE(query::is_valid(query::ParseFileKey{}));
    EXPECT_FALSE(query::is_valid(query::ModuleKey{}));
    EXPECT_FALSE(query::is_valid(query::ModulePartKey{}));
    EXPECT_FALSE(query::is_valid(query::DefKey{}));
    EXPECT_FALSE(query::is_valid(query::MemberKey{}));
    EXPECT_FALSE(query::is_valid(query::BodyKey{}));
    EXPECT_FALSE(query::is_valid(query::GenericParamKey{}));
    EXPECT_FALSE(query::is_valid(query::QueryKey{}));
    query::FileKey zero_global_file = source_file;
    zero_global_file.global_id = 0;
    query::LexConfigKey zero_global_lex_config = lex_config;
    zero_global_lex_config.global_id = 0;
    query::LexConfigKey wrong_schema_lex_config = lex_config;
    wrong_schema_lex_config.schema += 1;
    query::ParserConfigKey zero_global_parser_config = parser_config;
    zero_global_parser_config.global_id = 0;
    query::ParserConfigKey wrong_schema_parser_config = parser_config;
    wrong_schema_parser_config.schema += 1;
    query::ParserConfigKey invalid_lex_parser_config = parser_config;
    invalid_lex_parser_config.lex_config = {};
    query::LexFileKey zero_global_lex_file = lex_file;
    zero_global_lex_file.global_id = 0;
    query::ParseFileKey zero_global_parse_file = parse_file;
    zero_global_parse_file.global_id = 0;
    query::ModuleKey zero_global_module = module;
    zero_global_module.global_id = 0;
    query::ModulePartKey zero_global_module_part = module_part;
    zero_global_module_part.global_id = 0;
    query::ModulePartKey invalid_module_part_module = module_part;
    invalid_module_part_module.module = {};
    query::ModulePartKey invalid_module_part_file = module_part;
    invalid_module_part_file.file = {};
    const std::array<std::string_view, 1> other_package_parts{"other-package"};
    query::ModulePartKey cross_package_module_part = module_part;
    cross_package_module_part.file =
        query::file_key(query::package_key(other_package_parts), "/workspace/root/regex/vm.ax");
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
    query::ProjectKey zero_global_project = project;
    zero_global_project.global_id = 0;
    EXPECT_FALSE(query::is_valid(zero_global_project));
    EXPECT_FALSE(query::is_valid(zero_global_file));
    EXPECT_FALSE(query::is_valid(zero_global_lex_config));
    EXPECT_FALSE(query::is_valid(wrong_schema_lex_config));
    EXPECT_FALSE(query::is_valid(zero_global_parser_config));
    EXPECT_FALSE(query::is_valid(wrong_schema_parser_config));
    EXPECT_FALSE(query::is_valid(invalid_lex_parser_config));
    EXPECT_FALSE(query::is_valid(zero_global_lex_file));
    EXPECT_FALSE(query::is_valid(zero_global_parse_file));
    EXPECT_FALSE(query::is_valid(zero_global_module));
    EXPECT_FALSE(query::is_valid(zero_global_module_part));
    EXPECT_FALSE(query::is_valid(invalid_module_part_module));
    EXPECT_FALSE(query::is_valid(invalid_module_part_file));
    EXPECT_FALSE(query::is_valid(cross_package_module_part));
    EXPECT_FALSE(query::is_valid(invalid_kind_def));
    EXPECT_FALSE(query::is_valid(zero_global_def));
    EXPECT_FALSE(query::is_valid(invalid_kind_member));
    EXPECT_FALSE(query::is_valid(zero_global_member));
    EXPECT_FALSE(query::is_valid(zero_global_body));
    EXPECT_FALSE(query::is_valid(zero_global_generic_param));
    EXPECT_FALSE(query::is_valid(zero_global_query));

    EXPECT_FALSE(query::stable_serialize(package).empty());
    EXPECT_FALSE(query::stable_serialize(project).empty());
    EXPECT_FALSE(query::stable_serialize(source_file).empty());
    EXPECT_FALSE(query::stable_serialize(lex_config).empty());
    EXPECT_FALSE(query::stable_serialize(parser_config).empty());
    EXPECT_FALSE(query::stable_serialize(lex_file).empty());
    EXPECT_FALSE(query::stable_serialize(parse_file).empty());
    EXPECT_FALSE(query::stable_serialize(module).empty());
    EXPECT_FALSE(query::stable_serialize(module_part).empty());
    EXPECT_FALSE(query::stable_serialize(function_def).empty());
    EXPECT_FALSE(query::stable_serialize(member).empty());
    EXPECT_FALSE(query::stable_serialize(body).empty());
    EXPECT_FALSE(query::stable_serialize(generic_param).empty());
    EXPECT_FALSE(query::stable_serialize(diagnostics_query).empty());

    EXPECT_GT(query::stable_key_fingerprint(package).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(project).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(source_file).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(lex_config).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(parser_config).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(lex_file).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(parse_file).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(module).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(module_part).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(function_def).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(member).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(body).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(generic_param).byte_count, 0U);
    EXPECT_GT(query::stable_key_fingerprint(diagnostics_query).byte_count, 0U);

    EXPECT_NE(query::debug_string(package).find("PackageKey"), std::string::npos);
    EXPECT_NE(query::debug_string(project).find("ProjectKey"), std::string::npos);
    EXPECT_NE(query::debug_string(source_file).find("FileKey"), std::string::npos);
    EXPECT_NE(query::debug_string(lex_config).find("LexConfigKey"), std::string::npos);
    EXPECT_NE(query::debug_string(parser_config).find("ParserConfigKey"), std::string::npos);
    EXPECT_NE(query::debug_string(lex_file).find("LexFileKey"), std::string::npos);
    EXPECT_NE(query::debug_string(parse_file).find("ParseFileKey"), std::string::npos);
    EXPECT_NE(query::debug_string(module).find("ModuleKey"), std::string::npos);
    EXPECT_NE(query::debug_string(module_part).find("ModulePartKey"), std::string::npos);
    EXPECT_NE(query::debug_string(function_def).find("DefKey"), std::string::npos);
    EXPECT_NE(query::debug_string(member).find("MemberKey"), std::string::npos);
    EXPECT_NE(query::debug_string(body).find("BodyKey"), std::string::npos);
    EXPECT_NE(query::debug_string(generic_param).find("GenericParamKey"), std::string::npos);
    EXPECT_NE(query::debug_string(diagnostics_query).find("QueryKey"), std::string::npos);

    EXPECT_NE(query::PackageKeyHash{}(package), 0U);
    EXPECT_NE(query::ProjectKeyHash{}(project), 0U);
    EXPECT_NE(query::FileKeyHash{}(source_file), 0U);
    EXPECT_NE(query::LexFileKeyHash{}(lex_file), 0U);
    EXPECT_NE(query::ParseFileKeyHash{}(parse_file), 0U);
    EXPECT_NE(query::ModuleKeyHash{}(module), 0U);
    EXPECT_NE(query::ModulePartKeyHash{}(module_part), 0U);
    EXPECT_NE(query::DefKeyHash{}(function_def), 0U);
    EXPECT_NE(query::QueryKeyHash{}(diagnostics_query), 0U);
}

TEST(QueryUnit, StableKeyDecoderProjectsSourceIdentitySlices)
{
    const QueryContextSourceSubject source_subject = test_source_subject();
    const query::ModuleKey module = test_module(source_subject.file.package);
    const query::ModulePartKey module_part =
        query::module_part_key(module, source_subject.file, query::ModulePartKind::primary, "<primary>");
    const std::string file_bytes = query::stable_serialize(source_subject.file);
    const std::string module_bytes = query::stable_serialize(module);
    const std::string module_part_bytes = query::stable_serialize(module_part);
    const std::string lex_config_bytes = query::stable_serialize(source_subject.lex_file.config);
    const std::string lex_file_bytes = query::stable_serialize(source_subject.lex_file);
    const std::string parse_file_bytes = query::stable_serialize(source_subject.parse_file);

    EXPECT_TRUE(query::stable_key_has_file_key_layout(file_bytes));
    EXPECT_FALSE(query::stable_key_has_file_key_layout(lex_file_bytes));
    EXPECT_TRUE(query::stable_key_has_module_part_key_layout(module_part_bytes));

    const std::optional<query::DecodedLexFileKeyIdentity> lex_identity =
        query::decode_lex_file_key_identity(lex_file_bytes);
    ASSERT_TRUE(lex_identity.has_value());
    EXPECT_EQ(lex_identity->file, file_bytes);
    EXPECT_EQ(lex_identity->lex_config, lex_config_bytes);

    const std::optional<query::DecodedParseFileKeyIdentity> parse_identity =
        query::decode_parse_file_key_identity(parse_file_bytes);
    ASSERT_TRUE(parse_identity.has_value());
    EXPECT_EQ(parse_identity->file, file_bytes);
    EXPECT_EQ(parse_identity->lex_config, lex_config_bytes);

    const std::optional<query::DecodedModulePartKeyIdentity> module_part_identity =
        query::decode_module_part_key_identity(module_part_bytes);
    ASSERT_TRUE(module_part_identity.has_value());
    EXPECT_EQ(module_part_identity->module, module_bytes);
    EXPECT_EQ(module_part_identity->file, file_bytes);
}

TEST(QueryUnit, StableKeyDecoderProjectsDefinitionBodyGenericAndQueryIdentitySlices)
{
    const query::ProjectKey project = test_project_key();
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::DefKey function_def = test_function_def(module);
    const query::DefKey template_def = test_template_def(module);
    const query::MemberKey associated_member =
        query::member_key(function_def, query::MemberKind::associated_type, "Item", QUERY_TEST_STABLE_ORDINAL);
    const query::GenericParamKey generic_param =
        query::generic_param_key(template_def, QUERY_TEST_GENERIC_PARAM_INDEX, query::GenericParamKind::type);
    const query::BodyKey body = query::body_key(function_def, query::BodySlotKind::function_body);
    const query::CanonicalTypeKey i32 = query::canonical_builtin(query::BuiltinTypeKey::i32);
    const query::CanonicalTypeKey pointer = query::canonical_pointer(query::PointerMutabilityKey::const_, i32);
    const query::CanonicalTypeKey reference = query::canonical_reference(query::PointerMutabilityKey::mut, i32);
    const query::CanonicalTypeKey array = query::canonical_array(QUERY_TEST_ARRAY_COUNT, i32);
    const query::CanonicalTypeKey slice = query::canonical_slice(query::PointerMutabilityKey::const_, i32);
    const std::array<query::CanonicalTypeKey, 2> tuple_elements{pointer, reference};
    const query::CanonicalTypeKey tuple = query::canonical_tuple(tuple_elements);
    const std::array<query::CanonicalTypeKey, 2> function_params{pointer, reference};
    const query::CanonicalTypeKey function_type =
        query::canonical_function(query::FunctionCallConvKey::c, true, true, function_params, i32);
    const std::array<query::CanonicalTypeKey, 1> nominal_args{query::canonical_generic_param(generic_param)};
    const query::CanonicalTypeKey nominal = query::canonical_nominal(template_def, nominal_args);
    const query::CanonicalTypeKey const_arg = query::canonical_const_arg(query::stable_fingerprint("N=4"));
    const query::CanonicalTypeKey projection = query::canonical_associated_type_projection(i32, associated_member);
    query::CanonicalTypeKey trait_object;
    trait_object.kind = query::CanonicalTypeKind::trait_object;
    const std::vector<query::CanonicalTypeKey> type_args{
        i32,
        pointer,
        reference,
        array,
        slice,
        tuple,
        function_type,
        nominal,
        query::canonical_generic_param(generic_param),
        const_arg,
        projection,
        trait_object,
    };
    const std::array<query::StableFingerprint128, 1> const_args{query::stable_fingerprint("const:N=4")};
    const std::array<std::string_view, 1> predicates{"T: Copy"};
    const query::GenericInstanceKey instance = query::generic_instance_key(template_def,
        std::span<const query::CanonicalTypeKey>{type_args.data(), type_args.size()}, const_args,
        query::param_env_key(predicates));
    const query::QueryKey diagnostics_key =
        query::query_key(query::QueryKind::diagnostics, query::stable_key_fingerprint(function_def));
    const std::string project_bytes = query::stable_serialize(project);
    const std::string module_bytes = query::stable_serialize(module);
    const std::string function_def_bytes = query::stable_serialize(function_def);
    const std::string template_def_bytes = query::stable_serialize(template_def);
    const std::string body_bytes = query::stable_serialize(body);
    const std::string instance_bytes = query::stable_serialize(instance);
    const std::string diagnostics_key_bytes = query::stable_serialize(diagnostics_key);

    EXPECT_TRUE(query::stable_key_has_project_key_layout(project_bytes));
    EXPECT_TRUE(query::stable_key_has_module_key_layout(module_bytes));
    EXPECT_TRUE(query::stable_key_has_body_key_layout(body_bytes));
    EXPECT_TRUE(query::stable_key_has_generic_instance_key_layout(instance_bytes));
    EXPECT_TRUE(query::stable_key_has_query_key_layout(diagnostics_key_bytes));

    const std::optional<query::DecodedDefKeyIdentity> def_identity = query::decode_def_key_identity(function_def_bytes);
    ASSERT_TRUE(def_identity.has_value());
    EXPECT_EQ(def_identity->module, module_bytes);

    const std::optional<query::DecodedBodyKeyIdentity> body_identity = query::decode_body_key_identity(body_bytes);
    ASSERT_TRUE(body_identity.has_value());
    EXPECT_EQ(body_identity->owner, function_def_bytes);

    const std::optional<query::DecodedGenericInstanceKeyIdentity> instance_identity =
        query::decode_generic_instance_key_identity(instance_bytes);
    ASSERT_TRUE(instance_identity.has_value());
    EXPECT_EQ(instance_identity->template_def, template_def_bytes);
}

TEST(QueryUnit, StableKeyDecoderMatchesStableKeyLayoutToQueryKind)
{
    const QueryContextSourceSubject source_subject = test_source_subject();
    const query::ProjectKey project = test_project_key();
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::ModulePartKey module_part =
        query::module_part_key(module, source_subject.file, query::ModulePartKind::primary, "<primary>");
    const query::DefKey function_def = test_function_def(module);
    const query::DefKey template_def = test_template_def(module);
    const query::BodyKey body = query::body_key(function_def, query::BodySlotKind::function_body);
    const std::array<query::CanonicalTypeKey, 1> type_args{query::canonical_builtin(query::BuiltinTypeKey::i32)};
    const query::GenericInstanceKey instance = query::generic_instance_key(template_def, type_args,
        std::span<const query::StableFingerprint128>{}, query::param_env_key(std::span<const std::string_view>{}));
    const query::QueryKey producer =
        query::query_key(query::QueryKind::item_signature, query::stable_key_fingerprint(function_def));

    const std::string project_bytes = query::stable_serialize(project);
    const std::string file_bytes = query::stable_serialize(source_subject.file);
    const std::string lex_file_bytes = query::stable_serialize(source_subject.lex_file);
    const std::string parse_file_bytes = query::stable_serialize(source_subject.parse_file);
    const std::string module_bytes = query::stable_serialize(module);
    const std::string module_part_bytes = query::stable_serialize(module_part);
    const std::string def_bytes = query::stable_serialize(function_def);
    const std::string body_bytes = query::stable_serialize(body);
    const std::string instance_bytes = query::stable_serialize(instance);
    const std::string producer_bytes = query::stable_serialize(producer);

    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::project_graph, project_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::file_content, file_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::lex_file, lex_file_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::parse_file, parse_file_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::module_part, module_part_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::module_graph, module_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::module_exports, module_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::module_package_exports, module_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::item_list, module_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::item_signature, def_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::generic_template_signature, def_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::function_body_syntax, body_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::type_check_body, body_bytes));
    EXPECT_TRUE(
        query::stable_key_layout_matches_query_kind(query::QueryKind::generic_instance_signature, instance_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::generic_instance_body, instance_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::lower_function_ir, body_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::lower_function_ir, instance_bytes));
    EXPECT_TRUE(query::stable_key_layout_matches_query_kind(query::QueryKind::diagnostics, producer_bytes));

    EXPECT_FALSE(query::stable_key_layout_matches_query_kind(query::QueryKind::file_content, lex_file_bytes));
    EXPECT_FALSE(query::stable_key_layout_matches_query_kind(query::QueryKind::project_graph, module_bytes));
    EXPECT_FALSE(query::stable_key_layout_matches_query_kind(query::QueryKind::module_part, module_bytes));
    EXPECT_FALSE(query::stable_key_layout_matches_query_kind(query::QueryKind::item_signature, module_bytes));
    EXPECT_FALSE(query::stable_key_layout_matches_query_kind(query::QueryKind::lower_function_ir, module_bytes));
    EXPECT_FALSE(query::stable_key_layout_matches_query_kind(query::QueryKind::diagnostics, def_bytes));
    EXPECT_FALSE(query::stable_key_layout_matches_query_kind(query::QueryKind::invalid, file_bytes));
}

TEST(QueryUnit, StableKeyDecoderRejectsMalformedStableKeys)
{
    const QueryContextSourceSubject source_subject = test_source_subject();
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::ModulePartKey module_part =
        query::module_part_key(module, source_subject.file, query::ModulePartKind::primary, "<primary>");
    const query::DefKey function_def = test_function_def(module);
    const query::DefKey template_def = test_template_def(module);
    const query::BodyKey body = query::body_key(function_def, query::BodySlotKind::function_body);
    const std::array<query::CanonicalTypeKey, 1> type_args{query::canonical_builtin(query::BuiltinTypeKey::i32)};
    const query::GenericInstanceKey instance = query::generic_instance_key(template_def, type_args,
        std::span<const query::StableFingerprint128>{}, query::param_env_key(std::span<const std::string_view>{}));
    const std::string lex_file_bytes = query::stable_serialize(source_subject.lex_file);
    const std::string parse_file_bytes = query::stable_serialize(source_subject.parse_file);
    const std::string module_part_bytes = query::stable_serialize(module_part);
    const std::string function_def_bytes = query::stable_serialize(function_def);
    const std::string body_bytes = query::stable_serialize(body);
    const std::string instance_bytes = query::stable_serialize(instance);

    EXPECT_FALSE(
        query::decode_lex_file_key_identity(query_test_with_flipped_first_stable_byte(lex_file_bytes)).has_value());
    std::string lex_file_with_bad_file = lex_file_bytes;
    query_test_flip_stable_byte_at(lex_file_with_bad_file, QUERY_TEST_STABLE_U64_WIDTH);
    EXPECT_FALSE(query::decode_lex_file_key_identity(lex_file_with_bad_file).has_value());

    std::string lex_file_with_bad_config = lex_file_bytes;
    const std::string lex_config_bytes = query::stable_serialize(source_subject.lex_file.config);
    const base::usize lex_config_offset = lex_file_with_bad_config.find(lex_config_bytes);
    ASSERT_NE(lex_config_offset, std::string::npos);
    query_test_flip_stable_byte_at(lex_file_with_bad_config, lex_config_offset);
    EXPECT_FALSE(query::decode_lex_file_key_identity(lex_file_with_bad_config).has_value());

    EXPECT_FALSE(
        query::decode_parse_file_key_identity(query_test_without_last_stable_byte(parse_file_bytes)).has_value());
    std::string parse_file_with_bad_parser_config = parse_file_bytes;
    const std::string parser_config_bytes = query::stable_serialize(source_subject.parse_file.config);
    const base::usize parser_config_offset = parse_file_with_bad_parser_config.find(parser_config_bytes);
    ASSERT_NE(parser_config_offset, std::string::npos);
    query_test_flip_stable_byte_at(parse_file_with_bad_parser_config, parser_config_offset);
    EXPECT_FALSE(query::decode_parse_file_key_identity(parse_file_with_bad_parser_config).has_value());

    std::string parse_file_with_truncated_parser_schema = parse_file_bytes;
    const base::usize parser_schema_offset =
        parser_config_offset + QUERY_TEST_STABLE_U64_WIDTH + lex_config_bytes.size();
    parse_file_with_truncated_parser_schema.resize(parser_schema_offset + QUERY_TEST_STABLE_U8_WIDTH);
    EXPECT_FALSE(query::decode_parse_file_key_identity(parse_file_with_truncated_parser_schema).has_value());

    std::string module_part_with_bad_file = module_part_bytes;
    const std::string file_bytes = query::stable_serialize(source_subject.file);
    const base::usize module_part_file_offset = module_part_with_bad_file.find(file_bytes);
    ASSERT_NE(module_part_file_offset, std::string::npos);
    query_test_flip_stable_byte_at(module_part_with_bad_file, module_part_file_offset);
    EXPECT_FALSE(query::decode_module_part_key_identity(module_part_with_bad_file).has_value());
    EXPECT_FALSE(query::stable_key_has_module_part_key_layout(query_test_with_trailing_stable_byte(module_part_bytes)));

    EXPECT_FALSE(query::decode_def_key_identity(query_test_with_trailing_stable_byte(function_def_bytes)).has_value());
    std::string def_with_bad_module = function_def_bytes;
    const std::string module_bytes = query::stable_serialize(function_def.module);
    const base::usize module_offset = def_with_bad_module.find(module_bytes);
    ASSERT_NE(module_offset, std::string::npos);
    query_test_flip_stable_byte_at(def_with_bad_module, module_offset);
    EXPECT_FALSE(query::decode_def_key_identity(def_with_bad_module).has_value());

    query::BodyKey zero_global_body = body;
    zero_global_body.global_id = 0;
    EXPECT_FALSE(query::decode_body_key_identity(query::stable_serialize(zero_global_body)).has_value());
    EXPECT_FALSE(query::stable_key_has_body_key_layout(query::stable_serialize(zero_global_body)));
    std::string body_with_bad_owner = body_bytes;
    const base::usize body_owner_offset = body_with_bad_owner.find(function_def_bytes);
    ASSERT_NE(body_owner_offset, std::string::npos);
    query_test_flip_stable_byte_at(body_with_bad_owner, body_owner_offset);
    EXPECT_FALSE(query::decode_body_key_identity(body_with_bad_owner).has_value());

    query::GenericInstanceKey invalid_canonical_instance = instance;
    invalid_canonical_instance.type_args.front().kind = query::CanonicalTypeKind::invalid;
    EXPECT_FALSE(
        query::decode_generic_instance_key_identity(query::stable_serialize(invalid_canonical_instance)).has_value());
    EXPECT_FALSE(
        query::decode_generic_instance_key_identity(query_test_with_trailing_stable_byte(instance_bytes)).has_value());

    query::QueryKey invalid_query =
        query::query_key(query::QueryKind::diagnostics, query::stable_key_fingerprint(function_def));
    invalid_query.kind = query::QueryKind::invalid;
    EXPECT_FALSE(query::stable_key_has_query_key_layout(query::stable_serialize(invalid_query)));
}

TEST(QueryUnit, StableKeyDecoderRejectsMalformedCanonicalTypeShapes)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::DefKey template_def = test_template_def(module);
    const query::CanonicalTypeKey i32 = query::canonical_builtin(query::BuiltinTypeKey::i32);
    const auto expect_rejected_type_arg = [&template_def](const query::CanonicalTypeKey& type_arg) {
        const query::GenericInstanceKey instance = query_test_generic_instance_with_type_arg(template_def, type_arg);
        const std::string bytes = query::stable_serialize(instance);
        EXPECT_FALSE(query::decode_generic_instance_key_identity(bytes).has_value());
        EXPECT_FALSE(query::stable_key_has_generic_instance_key_layout(bytes));
    };

    query::CanonicalTypeKey invalid_builtin;
    invalid_builtin.kind = query::CanonicalTypeKind::builtin;
    invalid_builtin.builtin = static_cast<query::BuiltinTypeKey>(QUERY_TEST_STABLE_BYTE_FLIP_MASK);
    expect_rejected_type_arg(invalid_builtin);

    query::CanonicalTypeKey invalid_pointer_mutability;
    invalid_pointer_mutability.kind = query::CanonicalTypeKind::pointer;
    invalid_pointer_mutability.mutability = static_cast<query::PointerMutabilityKey>(QUERY_TEST_STABLE_BYTE_FLIP_MASK);
    invalid_pointer_mutability.children.push_back(i32);
    expect_rejected_type_arg(invalid_pointer_mutability);

    query::CanonicalTypeKey invalid_pointer_child_count;
    invalid_pointer_child_count.kind = query::CanonicalTypeKind::pointer;
    invalid_pointer_child_count.mutability = query::PointerMutabilityKey::const_;
    expect_rejected_type_arg(invalid_pointer_child_count);

    query::CanonicalTypeKey invalid_function_call_conv;
    invalid_function_call_conv.kind = query::CanonicalTypeKind::function;
    invalid_function_call_conv.function_call_conv =
        static_cast<query::FunctionCallConvKey>(QUERY_TEST_STABLE_BYTE_FLIP_MASK);
    invalid_function_call_conv.children.push_back(i32);
    expect_rejected_type_arg(invalid_function_call_conv);

    query::CanonicalTypeKey invalid_nominal_def;
    invalid_nominal_def.kind = query::CanonicalTypeKind::nominal;
    expect_rejected_type_arg(invalid_nominal_def);

    query::CanonicalTypeKey invalid_generic_param;
    invalid_generic_param.kind = query::CanonicalTypeKind::generic_param;
    expect_rejected_type_arg(invalid_generic_param);

    query::CanonicalTypeKey invalid_associated_member;
    invalid_associated_member.kind = query::CanonicalTypeKind::associated_type_projection;
    invalid_associated_member.children.push_back(i32);
    expect_rejected_type_arg(invalid_associated_member);

    const query::CanonicalTypeKey valid_array = query::canonical_array(QUERY_TEST_ARRAY_COUNT, i32);
    const query::GenericInstanceKey array_instance =
        query_test_generic_instance_with_type_arg(template_def, valid_array);
    std::string truncated_array_instance = query::stable_serialize(array_instance);
    const std::string array_type_bytes = query::stable_serialize(valid_array);
    const base::usize array_type_offset = truncated_array_instance.find(array_type_bytes);
    ASSERT_NE(array_type_offset, std::string::npos);
    truncated_array_instance.resize(
        array_type_offset + QUERY_TEST_STABLE_U64_WIDTH + QUERY_TEST_STABLE_U8_WIDTH + QUERY_TEST_STABLE_U8_WIDTH);
    EXPECT_FALSE(query::decode_generic_instance_key_identity(truncated_array_instance).has_value());

    const query::CanonicalTypeKey valid_const_arg = query::canonical_const_arg(query::stable_fingerprint("N=8"));
    const query::GenericInstanceKey const_type_instance =
        query_test_generic_instance_with_type_arg(template_def, valid_const_arg);
    std::string truncated_const_type_instance = query::stable_serialize(const_type_instance);
    const std::string const_type_bytes = query::stable_serialize(valid_const_arg);
    const base::usize const_type_offset = truncated_const_type_instance.find(const_type_bytes);
    ASSERT_NE(const_type_offset, std::string::npos);
    truncated_const_type_instance.resize(
        const_type_offset + QUERY_TEST_STABLE_U64_WIDTH + QUERY_TEST_STABLE_U8_WIDTH + QUERY_TEST_STABLE_U8_WIDTH);
    EXPECT_FALSE(query::decode_generic_instance_key_identity(truncated_const_type_instance).has_value());

    const query::GenericInstanceKey builtin_instance = query_test_generic_instance_with_type_arg(template_def, i32);
    std::string truncated_builtin_kind_instance = query::stable_serialize(builtin_instance);
    const std::string builtin_type_bytes = query::stable_serialize(i32);
    const base::usize builtin_type_offset = truncated_builtin_kind_instance.find(builtin_type_bytes);
    ASSERT_NE(builtin_type_offset, std::string::npos);
    truncated_builtin_kind_instance.resize(builtin_type_offset + QUERY_TEST_STABLE_U64_WIDTH);
    EXPECT_FALSE(query::decode_generic_instance_key_identity(truncated_builtin_kind_instance).has_value());

    const query::StableFingerprint128 const_arg_fingerprint = query::stable_fingerprint("const-count-overflow");
    const std::array<query::StableFingerprint128, 1> const_args{const_arg_fingerprint};
    const query::GenericInstanceKey const_instance =
        query::generic_instance_key(template_def, std::span<const query::CanonicalTypeKey>{}, const_args,
            query::param_env_key(std::span<const std::string_view>{}));
    std::string truncated_const_instance = query::stable_serialize(const_instance);
    const std::string const_arg_bytes = query_test_stable_fingerprint_bytes(const_arg_fingerprint);
    const base::usize const_arg_offset = truncated_const_instance.find(const_arg_bytes);
    ASSERT_NE(const_arg_offset, std::string::npos);
    truncated_const_instance.resize(const_arg_offset + QUERY_TEST_STABLE_U8_WIDTH);
    EXPECT_FALSE(query::decode_generic_instance_key_identity(truncated_const_instance).has_value());

    const query::GenericInstanceKey no_arg_instance =
        query::generic_instance_key(template_def, std::span<const query::CanonicalTypeKey>{},
            std::span<const query::StableFingerprint128>{}, query::param_env_key(std::span<const std::string_view>{}));
    std::string oversized_type_count_instance = query::stable_serialize(no_arg_instance);
    const std::string template_def_bytes = query::stable_serialize(template_def);
    const base::usize type_count_offset = QUERY_TEST_STABLE_U64_WIDTH + template_def_bytes.size();
    ASSERT_LE(type_count_offset + QUERY_TEST_STABLE_U64_WIDTH, oversized_type_count_instance.size());
    for (base::usize index = 0; index < QUERY_TEST_STABLE_U64_WIDTH; ++index) {
        oversized_type_count_instance[type_count_offset + index] = static_cast<char>(QUERY_TEST_STABLE_BYTE_FLIP_MASK);
    }
    EXPECT_FALSE(query::decode_generic_instance_key_identity(oversized_type_count_instance).has_value());
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

    const query::StableFingerprint128 exports_fingerprint =
        query::stable_fingerprint(QUERY_TEST_MODULE_EXPORTS_SIGNATURE);
    const query::QueryResultFingerprint exports_result = query::query_result_fingerprint(exports_fingerprint);
    ASSERT_TRUE(query::is_valid(exports_result));
    EXPECT_EQ(exports_result.fingerprint, exports_fingerprint);
    EXPECT_NE(exports_result.global_id, 0U);

    const QueryContextSourceSubject source_subject = test_source_subject();
    const std::optional<query::QueryRecord> file_content_record =
        query::file_content_query_record(source_subject.file, source_subject.content);
    ASSERT_TRUE(file_content_record.has_value());
    EXPECT_TRUE(query::is_valid(*file_content_record));
    EXPECT_EQ(file_content_record->key.kind, query::QueryKind::file_content);
    EXPECT_EQ(file_content_record->key.payload, query::stable_key_fingerprint(source_subject.file));
    EXPECT_EQ(file_content_record->stable_key_bytes, query::stable_serialize(source_subject.file));
    EXPECT_EQ(file_content_record->result, source_subject.content);

    const query::LexFileQueryInput lex_file_input{
        source_subject.lex_file,
        source_subject.tokens,
    };
    EXPECT_TRUE(query::is_valid(lex_file_input));
    const std::optional<query::QueryRecord> lex_file_record = query::lex_file_query_record(lex_file_input);
    ASSERT_TRUE(lex_file_record.has_value());
    EXPECT_TRUE(query::is_valid(*lex_file_record));
    EXPECT_EQ(lex_file_record->key.kind, query::QueryKind::lex_file);
    EXPECT_EQ(lex_file_record->key.payload, query::stable_key_fingerprint(source_subject.lex_file));
    EXPECT_EQ(lex_file_record->stable_key_bytes, query::stable_serialize(source_subject.lex_file));
    EXPECT_EQ(lex_file_record->result, source_subject.tokens);

    const query::ParseFileQueryInput parse_file_input{
        source_subject.parse_file,
        source_subject.syntax,
    };
    EXPECT_TRUE(query::is_valid(parse_file_input));
    const std::optional<query::QueryRecord> parse_file_record = query::parse_file_query_record(parse_file_input);
    ASSERT_TRUE(parse_file_record.has_value());
    EXPECT_TRUE(query::is_valid(*parse_file_record));
    EXPECT_EQ(parse_file_record->key.kind, query::QueryKind::parse_file);
    EXPECT_EQ(parse_file_record->key.payload, query::stable_key_fingerprint(source_subject.parse_file));
    EXPECT_EQ(parse_file_record->stable_key_bytes, query::stable_serialize(source_subject.parse_file));
    EXPECT_EQ(parse_file_record->result, source_subject.syntax);

    const query::ModulePartKey module_part =
        query::module_part_key(module, source_subject.file, query::ModulePartKind::primary, "<primary>");
    const query::QueryResultFingerprint module_part_result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_MODULE_PART));
    const query::ModulePartQueryInput module_part_input{
        module_part,
        module_part_result,
    };
    EXPECT_TRUE(query::is_valid(module_part_input));
    const std::optional<query::QueryRecord> module_part_record = query::module_part_query_record(module_part_input);
    ASSERT_TRUE(module_part_record.has_value());
    EXPECT_TRUE(query::is_valid(*module_part_record));
    EXPECT_EQ(module_part_record->key.kind, query::QueryKind::module_part);
    EXPECT_EQ(module_part_record->key.payload, query::stable_key_fingerprint(module_part));
    EXPECT_EQ(module_part_record->stable_key_bytes, query::stable_serialize(module_part));
    EXPECT_EQ(module_part_record->result, module_part_result);

    const query::QueryResultFingerprint module_graph_result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_MODULE_GRAPH));
    const query::ModuleGraphQueryInput module_graph_input{
        module,
        module_graph_result,
    };
    EXPECT_TRUE(query::is_valid(module_graph_input));
    const std::optional<query::QueryRecord> module_graph_record = query::module_graph_query_record(module_graph_input);
    ASSERT_TRUE(module_graph_record.has_value());
    EXPECT_TRUE(query::is_valid(*module_graph_record));
    EXPECT_EQ(module_graph_record->key.kind, query::QueryKind::module_graph);
    EXPECT_EQ(module_graph_record->key.payload, query::stable_key_fingerprint(module));
    EXPECT_EQ(module_graph_record->stable_key_bytes, query::stable_serialize(module));
    EXPECT_EQ(module_graph_record->result, module_graph_result);

    const query::ModuleExportsQueryInput exports_input{
        module,
        exports_result,
    };
    EXPECT_TRUE(query::is_valid(exports_input));
    const std::optional<query::QueryRecord> exports_record = query::module_exports_query_record(exports_input);
    ASSERT_TRUE(exports_record.has_value());
    EXPECT_TRUE(query::is_valid(*exports_record));
    EXPECT_EQ(exports_record->key.kind, query::QueryKind::module_exports);
    EXPECT_EQ(exports_record->key.payload, query::stable_key_fingerprint(module));
    EXPECT_EQ(exports_record->stable_key_bytes, query::stable_serialize(module));
    EXPECT_EQ(exports_record->result, exports_result);

    const query::ModulePackageExportsQueryInput package_exports_input{
        module,
        exports_result,
    };
    EXPECT_TRUE(query::is_valid(package_exports_input));
    const std::optional<query::QueryRecord> package_exports_record =
        query::module_package_exports_query_record(package_exports_input);
    ASSERT_TRUE(package_exports_record.has_value());
    EXPECT_TRUE(query::is_valid(*package_exports_record));
    EXPECT_EQ(package_exports_record->key.kind, query::QueryKind::module_package_exports);
    EXPECT_EQ(package_exports_record->key.payload, query::stable_key_fingerprint(module));
    EXPECT_EQ(package_exports_record->stable_key_bytes, query::stable_serialize(module));
    EXPECT_EQ(package_exports_record->result, exports_result);

    const query::QueryResultFingerprint item_list_result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_ITEM_LIST));
    const query::ItemListQueryInput item_list_input{
        module,
        item_list_result,
    };
    EXPECT_TRUE(query::is_valid(item_list_input));
    const std::optional<query::QueryRecord> item_list_record = query::item_list_query_record(item_list_input);
    ASSERT_TRUE(item_list_record.has_value());
    EXPECT_TRUE(query::is_valid(*item_list_record));
    EXPECT_EQ(item_list_record->key.kind, query::QueryKind::item_list);
    EXPECT_EQ(item_list_record->key.payload, query::stable_key_fingerprint(module));
    EXPECT_EQ(item_list_record->stable_key_bytes, query::stable_serialize(module));
    EXPECT_EQ(item_list_record->result, item_list_result);

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

    const query::DefKey generic_template_def = test_template_def(module);
    const query::IncrementalKey generic_template_signature =
        query::stable_incremental_key(legacy_function_id, QUERY_TEST_GENERIC_TEMPLATE_SIGNATURE);
    const query::GenericTemplateSignatureQueryInput template_input{
        generic_template_def,
        query::query_result_fingerprint(generic_template_signature),
    };
    EXPECT_TRUE(query::is_valid(template_input));
    const std::optional<query::QueryRecord> template_record =
        query::generic_template_signature_query_record(template_input);
    ASSERT_TRUE(template_record.has_value());
    EXPECT_TRUE(query::is_valid(*template_record));
    EXPECT_EQ(template_record->key.kind, query::QueryKind::generic_template_signature);
    EXPECT_EQ(template_record->key.payload, query::stable_key_fingerprint(generic_template_def));
    EXPECT_EQ(template_record->stable_key_bytes, query::stable_serialize(generic_template_def));

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

    const query::QueryResultFingerprint generic_body_result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_GENERIC_INSTANCE_BODY));
    const query::GenericInstanceBodyQueryInput generic_body_input{
        instance,
        generic_body_result,
    };
    EXPECT_TRUE(query::is_valid(generic_body_input));
    const std::optional<query::QueryRecord> generic_body_record =
        query::generic_instance_body_query_record(generic_body_input);
    ASSERT_TRUE(generic_body_record.has_value());
    EXPECT_TRUE(query::is_valid(*generic_body_record));
    EXPECT_EQ(generic_body_record->key.kind, query::QueryKind::generic_instance_body);
    EXPECT_EQ(generic_body_record->key.payload, query::stable_key_fingerprint(instance));
    EXPECT_EQ(generic_body_record->stable_key_bytes, query::stable_serialize(instance));

    const query::BodyKey function_body =
        query::body_key(function_def, query::BodySlotKind::function_body, QUERY_TEST_STABLE_ORDINAL);
    const query::QueryResultFingerprint lower_ir_result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_LOWER_FUNCTION_IR));
    const query::LowerFunctionIRQueryInput lower_body_input{
        function_body,
        lower_ir_result,
    };
    EXPECT_TRUE(query::is_valid(lower_body_input));
    const std::optional<query::QueryRecord> lower_body_record = query::lower_function_ir_query_record(lower_body_input);
    ASSERT_TRUE(lower_body_record.has_value());
    EXPECT_TRUE(query::is_valid(*lower_body_record));
    EXPECT_EQ(lower_body_record->key.kind, query::QueryKind::lower_function_ir);
    EXPECT_EQ(lower_body_record->key.payload, query::stable_key_fingerprint(function_body));
    EXPECT_EQ(lower_body_record->stable_key_bytes, query::stable_serialize(function_body));

    const query::LowerGenericInstanceIRQueryInput lower_generic_input{
        instance,
        lower_ir_result,
    };
    EXPECT_TRUE(query::is_valid(lower_generic_input));
    const std::optional<query::QueryRecord> lower_generic_record =
        query::lower_generic_instance_ir_query_record(lower_generic_input);
    ASSERT_TRUE(lower_generic_record.has_value());
    EXPECT_TRUE(query::is_valid(*lower_generic_record));
    EXPECT_EQ(lower_generic_record->key.kind, query::QueryKind::lower_function_ir);
    EXPECT_EQ(lower_generic_record->key.payload, query::stable_key_fingerprint(instance));
    EXPECT_EQ(lower_generic_record->stable_key_bytes, query::stable_serialize(instance));

    const query::QueryResultFingerprint diagnostics_result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_DIAGNOSTICS));
    const query::DiagnosticsQueryInput diagnostics_input{
        item_record->key,
        diagnostics_result,
    };
    EXPECT_TRUE(query::is_valid(diagnostics_input));
    const std::optional<query::QueryRecord> diagnostics_record = query::diagnostics_query_record(diagnostics_input);
    ASSERT_TRUE(diagnostics_record.has_value());
    EXPECT_TRUE(query::is_valid(*diagnostics_record));
    EXPECT_EQ(diagnostics_record->key.kind, query::QueryKind::diagnostics);
    EXPECT_EQ(diagnostics_record->key.payload, query::stable_key_fingerprint(item_record->key));
    EXPECT_EQ(diagnostics_record->stable_key_bytes, query::stable_serialize(item_record->key));

    const std::array<const query::QueryRecord*, 14> records_with_stable_identity{
        &*file_content_record,
        &*lex_file_record,
        &*parse_file_record,
        &*module_part_record,
        &*module_graph_record,
        &*exports_record,
        &*item_list_record,
        &*item_record,
        &*template_record,
        &*instance_record,
        &*generic_body_record,
        &*lower_body_record,
        &*lower_generic_record,
        &*diagnostics_record,
    };
    for (const query::QueryRecord* const record : records_with_stable_identity) {
        EXPECT_TRUE(query::query_record_stable_identity_is_valid(*record));
    }

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
    EXPECT_FALSE(query::is_valid(query::query_result_fingerprint(query::StableFingerprint128{})));
    EXPECT_FALSE(query::is_valid(query::query_result_fingerprint(query::IncrementalKey{})));
    EXPECT_FALSE(query::query_record(query::QueryKind::invalid, query::stable_key_fingerprint(function_def),
        query::stable_serialize(function_def), result)
            .has_value());
    EXPECT_FALSE(
        query::query_record(query::QueryKind::item_signature, query::stable_key_fingerprint(function_def), "", result)
            .has_value());
    query::QueryRecord wrong_shape_record = *item_record;
    wrong_shape_record.key = query::query_key(query::QueryKind::item_signature, query::stable_key_fingerprint(module));
    wrong_shape_record.stable_key_bytes = query::stable_serialize(module);
    EXPECT_TRUE(query::is_valid(wrong_shape_record));
    EXPECT_FALSE(query::query_record_stable_identity_is_valid(wrong_shape_record));
    EXPECT_EQ(
        query::query_record_change_status(nullptr, wrong_shape_record), query::QueryRecordChangeStatus::malformed);

    query::QueryRecord wrong_payload_record = *item_record;
    wrong_payload_record.key =
        query::query_key(query::QueryKind::item_signature, query::stable_key_fingerprint(module));
    EXPECT_TRUE(query::is_valid(wrong_payload_record));
    EXPECT_FALSE(query::query_record_stable_identity_is_valid(wrong_payload_record));
    EXPECT_FALSE(query::is_valid(query::FileContentQueryInput{}));
    EXPECT_FALSE(
        query::file_content_query_record(query::FileContentQueryInput{query::FileKey{}, source_subject.content})
            .has_value());
    EXPECT_FALSE(query::file_content_query_record(
        query::FileContentQueryInput{source_subject.file, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::LexFileQueryInput{}));
    EXPECT_FALSE(
        query::lex_file_query_record(query::LexFileQueryInput{query::LexFileKey{}, source_subject.tokens}).has_value());
    EXPECT_FALSE(
        query::lex_file_query_record(query::LexFileQueryInput{source_subject.lex_file, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::ParseFileQueryInput{}));
    EXPECT_FALSE(
        query::parse_file_query_record(query::ParseFileQueryInput{query::ParseFileKey{}, source_subject.syntax})
            .has_value());
    EXPECT_FALSE(query::parse_file_query_record(
        query::ParseFileQueryInput{source_subject.parse_file, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::ModuleGraphQueryInput{}));
    EXPECT_FALSE(query::is_valid(query::ModulePartQueryInput{}));
    EXPECT_FALSE(
        query::module_part_query_record(query::ModulePartQueryInput{query::ModulePartKey{}, module_part_result})
            .has_value());
    EXPECT_FALSE(
        query::module_part_query_record(query::ModulePartQueryInput{module_part, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::module_part_query_record(query::ModulePartKey{}, module_part_result).has_value());
    EXPECT_FALSE(query::module_part_query_record(module_part, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::module_graph_query_record(query::ModuleGraphQueryInput{query::ModuleKey{}, module_graph_result})
            .has_value());
    EXPECT_FALSE(query::module_graph_query_record(query::ModuleGraphQueryInput{module, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::module_graph_query_record(query::ModuleKey{}, module_graph_result).has_value());
    EXPECT_FALSE(query::module_graph_query_record(module, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::is_valid(query::ModuleExportsQueryInput{}));
    EXPECT_FALSE(
        query::module_exports_query_record(query::ModuleExportsQueryInput{query::ModuleKey{}, result}).has_value());
    EXPECT_FALSE(
        query::module_exports_query_record(query::ModuleExportsQueryInput{module, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::module_exports_query_record(query::ModuleKey{}, result).has_value());
    EXPECT_FALSE(query::module_exports_query_record(module, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::is_valid(query::ModulePackageExportsQueryInput{}));
    EXPECT_FALSE(
        query::module_package_exports_query_record(query::ModulePackageExportsQueryInput{query::ModuleKey{}, result})
            .has_value());
    EXPECT_FALSE(query::module_package_exports_query_record(
        query::ModulePackageExportsQueryInput{module, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::module_package_exports_query_record(query::ModuleKey{}, result).has_value());
    EXPECT_FALSE(query::module_package_exports_query_record(module, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::is_valid(query::ItemListQueryInput{}));
    EXPECT_FALSE(
        query::item_list_query_record(query::ItemListQueryInput{query::ModuleKey{}, item_list_result}).has_value());
    EXPECT_FALSE(
        query::item_list_query_record(query::ItemListQueryInput{module, query::QueryResultFingerprint{}}).has_value());
    EXPECT_FALSE(query::item_list_query_record(query::ModuleKey{}, item_list_result).has_value());
    EXPECT_FALSE(query::item_list_query_record(module, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::is_valid(query::ItemSignatureQueryInput{}));
    EXPECT_FALSE(
        query::item_signature_query_record(query::ItemSignatureQueryInput{query::DefKey{}, result}).has_value());
    EXPECT_FALSE(query::item_signature_query_record(
        query::ItemSignatureQueryInput{function_def, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::GenericTemplateSignatureQueryInput{}));
    EXPECT_FALSE(query::generic_template_signature_query_record(
        query::GenericTemplateSignatureQueryInput{query::DefKey{}, template_input.result})
            .has_value());
    EXPECT_FALSE(query::generic_template_signature_query_record(
        query::GenericTemplateSignatureQueryInput{generic_template_def, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::generic_template_signature_query_record(query::DefKey{}, template_input.result).has_value());
    EXPECT_FALSE(query::generic_template_signature_query_record(generic_template_def, query::QueryResultFingerprint{})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::GenericInstanceSignatureQueryInput{}));
    EXPECT_FALSE(query::generic_instance_signature_query_record(
        query::GenericInstanceSignatureQueryInput{query::GenericInstanceKey{}, result})
            .has_value());
    EXPECT_FALSE(query::generic_instance_signature_query_record(
        query::GenericInstanceSignatureQueryInput{instance, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::GenericInstanceBodyQueryInput{}));
    EXPECT_FALSE(query::generic_instance_body_query_record(
        query::GenericInstanceBodyQueryInput{query::GenericInstanceKey{}, generic_body_result})
            .has_value());
    EXPECT_FALSE(query::generic_instance_body_query_record(
        query::GenericInstanceBodyQueryInput{instance, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::LowerFunctionIRQueryInput{}));
    EXPECT_FALSE(
        query::lower_function_ir_query_record(query::LowerFunctionIRQueryInput{query::BodyKey{}, lower_ir_result})
            .has_value());
    EXPECT_FALSE(query::lower_function_ir_query_record(
        query::LowerFunctionIRQueryInput{function_body, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::LowerGenericInstanceIRQueryInput{}));
    EXPECT_FALSE(query::lower_generic_instance_ir_query_record(
        query::LowerGenericInstanceIRQueryInput{query::GenericInstanceKey{}, lower_ir_result})
            .has_value());
    EXPECT_FALSE(query::lower_generic_instance_ir_query_record(
        query::LowerGenericInstanceIRQueryInput{instance, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::DiagnosticsQueryInput{}));
    EXPECT_FALSE(query::diagnostics_query_record(query::DiagnosticsQueryInput{query::QueryKey{}, diagnostics_result})
            .has_value());
    EXPECT_FALSE(
        query::diagnostics_query_record(query::DiagnosticsQueryInput{item_record->key, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(
        query::diagnostics_query_record(query::DiagnosticsQueryInput{diagnostics_record->key, diagnostics_result})
            .has_value());
    EXPECT_FALSE(query::item_signature_query_record(query::DefKey{}, result).has_value());
    EXPECT_FALSE(query::item_signature_query_record(function_def, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::generic_instance_signature_query_record(query::GenericInstanceKey{}, result).has_value());
    EXPECT_FALSE(
        query::generic_instance_body_query_record(query::GenericInstanceKey{}, generic_body_result).has_value());
    EXPECT_FALSE(query::generic_instance_body_query_record(instance, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::lower_function_ir_query_record(query::BodyKey{}, lower_ir_result).has_value());
    EXPECT_FALSE(query::lower_function_ir_query_record(function_body, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(
        query::lower_generic_instance_ir_query_record(query::GenericInstanceKey{}, lower_ir_result).has_value());
    EXPECT_FALSE(query::lower_generic_instance_ir_query_record(instance, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::file_content_query_record(query::FileKey{}, source_subject.content).has_value());
    EXPECT_FALSE(query::file_content_query_record(source_subject.file, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::lex_file_query_record(query::LexFileKey{}, source_subject.tokens).has_value());
    EXPECT_FALSE(query::lex_file_query_record(source_subject.lex_file, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::parse_file_query_record(query::ParseFileKey{}, source_subject.syntax).has_value());
    EXPECT_FALSE(
        query::parse_file_query_record(source_subject.parse_file, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::module_graph_query_record(query::ModuleKey{}, module_graph_result).has_value());
    EXPECT_FALSE(query::module_graph_query_record(module, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::item_list_query_record(query::ModuleKey{}, item_list_result).has_value());
    EXPECT_FALSE(query::item_list_query_record(module, query::QueryResultFingerprint{}).has_value());
    EXPECT_FALSE(query::diagnostics_query_record(query::QueryKey{}, diagnostics_result).has_value());
    EXPECT_FALSE(query::diagnostics_query_record(item_record->key, query::QueryResultFingerprint{}).has_value());
    EXPECT_EQ(query::query_record_change_status(&*item_record, query::QueryRecord{}),
        query::QueryRecordChangeStatus::malformed);
    const query::QueryRecord invalid_cached_record;
    EXPECT_EQ(query::query_record_change_status(&invalid_cached_record, *item_record),
        query::QueryRecordChangeStatus::malformed);
}

TEST(QueryUnit, QueryInternerAssignsNodeIdsAndBindsStableIdentities)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::DefKey function_def = test_function_def(module);
    const query::QueryResultFingerprint result = test_query_result(QUERY_TEST_PROVIDER_SIGNATURE);
    const query::QueryResultFingerprint module_graph_result = test_query_result(QUERY_TEST_MODULE_GRAPH);
    const std::optional<query::QueryRecord> item_record = query::item_signature_query_record(function_def, result);
    const std::optional<query::QueryRecord> module_graph_record =
        query::module_graph_query_record(module, module_graph_result);
    ASSERT_TRUE(item_record.has_value());
    ASSERT_TRUE(module_graph_record.has_value());

    query::QueryInterner interner;
    EXPECT_EQ(interner.size(), 0U);
    EXPECT_EQ(interner.stable_identity_count(), 0U);
    EXPECT_FALSE(query::is_valid(query::QueryNodeId{}));
    EXPECT_FALSE(interner.intern_key(query::QueryKey{}).has_value());
    EXPECT_FALSE(interner.find(query::QueryKey{}).has_value());
    EXPECT_EQ(interner.find(query::QueryNodeId{}), nullptr);
    EXPECT_FALSE(interner.stable_key_bytes(query::QueryNodeId{}).has_value());

    const std::optional<query::QueryNodeId> item_id = interner.intern_key(item_record->key);
    ASSERT_TRUE(item_id.has_value());
    EXPECT_TRUE(query::is_valid(*item_id));
    EXPECT_EQ(item_id->value, query::QUERY_NODE_ID_FIRST_VALUE);
    EXPECT_NE(query::QueryNodeIdHash{}(*item_id), 0U);
    EXPECT_EQ(interner.size(), 1U);
    EXPECT_EQ(interner.stable_identity_count(), 0U);
    EXPECT_EQ(interner.intern_key(item_record->key), item_id);
    EXPECT_EQ(interner.find(item_record->key), item_id);

    const query::QueryInternedIdentity* const unbound_identity = interner.find(*item_id);
    ASSERT_NE(unbound_identity, nullptr);
    EXPECT_EQ(unbound_identity->id, *item_id);
    EXPECT_EQ(unbound_identity->key, item_record->key);
    EXPECT_FALSE(unbound_identity->stable_identity_bound);
    EXPECT_FALSE(interner.stable_key_bytes(*item_id).has_value());

    const std::optional<query::QueryNodeId> bound_item_id = interner.intern_record(*item_record);
    ASSERT_TRUE(bound_item_id.has_value());
    EXPECT_EQ(bound_item_id, item_id);
    EXPECT_EQ(interner.size(), 1U);
    EXPECT_EQ(interner.stable_identity_count(), 1U);
    const std::optional<std::string_view> item_stable_key = interner.stable_key_bytes(*item_id);
    ASSERT_TRUE(item_stable_key.has_value());
    EXPECT_EQ(*item_stable_key, item_record->stable_key_bytes);
    EXPECT_TRUE(interner.bind_record(*item_id, *item_record));
    EXPECT_EQ(interner.stable_identity_count(), 1U);

    const std::optional<query::QueryNodeId> module_id = interner.intern_record(*module_graph_record);
    ASSERT_TRUE(module_id.has_value());
    EXPECT_NE(*module_id, *item_id);
    EXPECT_EQ(interner.size(), 2U);
    EXPECT_EQ(interner.stable_identity_count(), 2U);
    EXPECT_FALSE(interner.bind_record(query::QueryNodeId{}, *item_record));
    EXPECT_FALSE(interner.bind_record(query::QueryNodeId{QUERY_TEST_UNKNOWN_NODE_ID}, *item_record));
    EXPECT_FALSE(interner.bind_record(*item_id, *module_graph_record));

    query::QueryRecord wrong_payload_record = *item_record;
    wrong_payload_record.key =
        query::query_key(query::QueryKind::item_signature, query::stable_key_fingerprint(module));
    EXPECT_FALSE(interner.bind_record(*item_id, wrong_payload_record));
    EXPECT_FALSE(interner.intern_record(wrong_payload_record).has_value());
    EXPECT_EQ(interner.find(query::QueryNodeId{QUERY_TEST_UNKNOWN_NODE_ID}), nullptr);
    EXPECT_FALSE(interner.stable_key_bytes(query::QueryNodeId{QUERY_TEST_UNKNOWN_NODE_ID}).has_value());

    query::QueryInterner limited_interner{QUERY_TEST_LIMITED_INTERNER_CAPACITY};
    EXPECT_TRUE(limited_interner.intern_record(*item_record).has_value());
    EXPECT_FALSE(limited_interner.intern_key(module_graph_record->key).has_value());
    EXPECT_FALSE(limited_interner.intern_record(*module_graph_record).has_value());
    EXPECT_EQ(limited_interner.size(), QUERY_TEST_LIMITED_INTERNER_CAPACITY);
    EXPECT_EQ(limited_interner.stable_identity_count(), QUERY_TEST_LIMITED_INTERNER_CAPACITY);
}

TEST(QueryUnit, QueryExecutorEvaluatesOwnedRequestsOnDemand)
{
    const QueryContextSourceSubject source_subject = test_source_subject();
    const query::ProjectKey project = test_project_key();
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::ModulePartKey module_part =
        query::module_part_key(module, source_subject.file, query::ModulePartKind::primary, "<primary>");
    const query::DefKey template_def = test_template_def(module);
    const std::array<std::string_view, 2> template_module_path{"regex", "vm"};
    const query::IncrementalKey template_signature =
        query::stable_incremental_key(query::stable_definition_id(query::stable_module_id(template_module_path),
                                          query::StableSymbolKind::function, "ExecutorTemplate"),
            QUERY_TEST_GENERIC_TEMPLATE_SIGNATURE);
    const QueryContextItemSignatureSubject item_subject =
        test_item_signature_subject("executor_compute", QUERY_TEST_PROVIDER_SIGNATURE);
    const QueryContextGenericInstanceSignatureSubject generic_subject = test_generic_instance_signature_subject(
        "ExecutorVec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const query::GenericTemplateSignatureAuthority template_authority =
        test_template_signature_authority(template_def, template_signature);
    const query::GenericInstanceBodyProviderInput generic_body_input =
        generic_instance_body_provider_input(generic_subject, test_query_result(QUERY_TEST_GENERIC_INSTANCE_BODY));
    const QueryContextBodySubject body_subject = test_body_subject("executor_body");
    const std::optional<query::QueryKey> item_query = query::item_signature_query_key(item_subject.def);
    ASSERT_TRUE(item_query.has_value());

    query::QueryContext context;
    query::QueryExecutor executor{context};
    std::vector<query::QueryRequest> requests{
        query::QueryRequest{query::ProjectGraphProviderInput{project, test_query_result(QUERY_TEST_PROJECT_GRAPH)}},
        query::QueryRequest{query::FileContentProviderInput{source_subject.file, source_subject.content}},
        query::QueryRequest{query::LexFileProviderInput{source_subject.lex_file, source_subject.tokens}},
        query::QueryRequest{query::ParseFileProviderInput{source_subject.parse_file, source_subject.syntax}},
        query::QueryRequest{query::ModuleGraphProviderInput{module, test_query_result(QUERY_TEST_MODULE_GRAPH), {}}},
        query::QueryRequest{query::ModulePartProviderInput{module_part, test_query_result(QUERY_TEST_MODULE_PART)}},
        query::QueryRequest{query::ItemListProviderInput{module, test_query_result(QUERY_TEST_ITEM_LIST)}},
        query::QueryRequest{
            query::ModuleExportsProviderInput{module, test_query_result(QUERY_TEST_MODULE_EXPORTS_SIGNATURE)}},
        query::QueryRequest{item_subject.input},
        query::QueryRequest{query::GenericTemplateSignatureProviderInput{template_def, template_authority}},
        query::QueryRequest{
            query::GenericInstanceSignatureQueryRequest{generic_subject.key, generic_subject.authority}},
        query::QueryRequest{query::FunctionBodySyntaxProviderInput{body_subject.body, body_subject.syntax_authority}},
        query::QueryRequest{query::TypeCheckBodyProviderInput{body_subject.body, body_subject.type_check_authority}},
        query::QueryRequest{query::GenericInstanceBodyQueryRequest{generic_subject.key, generic_body_input.authority}},
        query::QueryRequest{query::LowerFunctionIRProviderInput{body_subject.body, body_subject.lowered_ir}},
        query::QueryRequest{query::LowerGenericInstanceIRQueryRequest{generic_subject.key, body_subject.lowered_ir}},
        query::QueryRequest{query::DiagnosticsProviderInput{*item_query, test_query_result(QUERY_TEST_DIAGNOSTICS)}},
    };

    for (const query::QueryRequest& request : requests) {
        EXPECT_TRUE(query::query_request_key(request).has_value());
    }
    const query::QueryBatchExecutionResult computed = executor.evaluate_all(requests);
    EXPECT_EQ(computed.results.size(), requests.size());
    EXPECT_EQ(computed.summary.total, requests.size());
    EXPECT_EQ(computed.summary.computed, requests.size());
    EXPECT_EQ(computed.summary.cached, 0U);
    EXPECT_EQ(computed.summary.failed, 0U);
    EXPECT_EQ(computed.summary.cycles, 0U);
    EXPECT_EQ(context.completed_records().size(), requests.size());

    const query::QueryBatchExecutionResult cached = executor.evaluate_all(requests);
    EXPECT_EQ(cached.summary.total, requests.size());
    EXPECT_EQ(cached.summary.computed, 0U);
    EXPECT_EQ(cached.summary.cached, requests.size());
    EXPECT_EQ(cached.summary.failed, 0U);
    EXPECT_EQ(cached.summary.cycles, 0U);

    const query::QueryRequest invalid_request;
    EXPECT_FALSE(query::query_request_key(invalid_request).has_value());
    const query::QueryEvaluationResult invalid_result = executor.evaluate(invalid_request);
    EXPECT_EQ(invalid_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(invalid_result.node, nullptr);
    const std::array<query::QueryRequest, 1> invalid_requests{invalid_request};
    const query::QueryBatchExecutionResult invalid_batch = executor.evaluate_all(invalid_requests);
    EXPECT_EQ(invalid_batch.summary.total, 1U);
    EXPECT_EQ(invalid_batch.summary.failed, 1U);
}

TEST(QueryUnit, QueryExecutorDoesNotEvaluateUnrequestedDependencies)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    base::usize module_graph_calls = 0;
    base::usize item_list_calls = 0;
    query::QueryContext context;
    context.set_module_graph_provider([&module_graph_calls](const query::ModuleGraphProviderInput& input) {
        ++module_graph_calls;
        return query::provide_module_graph_query(input);
    });
    context.set_item_list_provider([&item_list_calls](const query::ItemListProviderInput& input) {
        ++item_list_calls;
        return query::provide_item_list_query(input);
    });

    query::QueryExecutor executor{context};
    const query::QueryRequest item_list_request{
        query::ItemListProviderInput{module, test_query_result(QUERY_TEST_ITEM_LIST)}};
    const query::QueryEvaluationResult result = executor.evaluate(item_list_request);
    EXPECT_EQ(result.status, query::QueryEvaluationStatus::computed);
    EXPECT_EQ(item_list_calls, 1U);
    EXPECT_EQ(module_graph_calls, 0U);

    const std::optional<query::QueryKey> graph_key = query::module_graph_query_key(module);
    ASSERT_TRUE(graph_key.has_value());
    EXPECT_EQ(context.find(*graph_key), nullptr);
    EXPECT_TRUE(context.node_id_for(*graph_key).has_value());
}

TEST(QueryUnit, QueryReplayIndexCapturesClosedImmutableGraph)
{
    const QueryContextSourceSubject subject = test_source_subject();
    query::QueryContext context;
    query::QueryExecutor executor{context};
    const std::array<query::QueryRequest, 3> requests{
        query::QueryRequest{query::FileContentProviderInput{subject.file, subject.content}},
        query::QueryRequest{query::LexFileProviderInput{subject.lex_file, subject.tokens}},
        query::QueryRequest{query::ParseFileProviderInput{subject.parse_file, subject.syntax}},
    };
    const query::QueryBatchExecutionResult result = executor.evaluate_all(requests);
    EXPECT_EQ(result.summary.computed, requests.size());

    const std::optional<query::QueryKey> content_key = query::file_content_query_key(subject.file);
    const std::optional<query::QueryKey> lex_key = query::lex_file_query_key(subject.lex_file);
    const std::optional<query::QueryKey> parse_key = query::parse_file_query_key(subject.parse_file);
    ASSERT_TRUE(content_key.has_value());
    ASSERT_TRUE(lex_key.has_value());
    ASSERT_TRUE(parse_key.has_value());

    const std::optional<query::QueryReplaySnapshot> snapshot = query::make_query_replay_snapshot(context);
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->safety_mode, query::QueryReplaySafetyMode::immutable_snapshot);
    EXPECT_EQ(snapshot->nodes.size(), requests.size());
    EXPECT_EQ(snapshot->edges.size(), 2U);

    const std::optional<query::QueryReplayIndex> index = query::QueryReplayIndex::build(context);
    ASSERT_TRUE(index.has_value());
    EXPECT_FALSE(index->empty());
    EXPECT_EQ(index->size(), requests.size());
    EXPECT_EQ(index->snapshot().safety_mode, query::QueryReplaySafetyMode::immutable_snapshot);
    EXPECT_EQ(index->snapshot().nodes.size(), requests.size());
    EXPECT_NE(index->find(*content_key), nullptr);
    EXPECT_NE(index->find(*lex_key), nullptr);
    const query::QueryReplayNode* const parse_node = index->find(*parse_key);
    ASSERT_NE(parse_node, nullptr);
    ASSERT_TRUE(query::is_valid(parse_node->id));
    EXPECT_EQ(index->find(parse_node->id), parse_node);
    ASSERT_NE(index->record_for(*parse_key), nullptr);
    EXPECT_EQ(index->record_for(*parse_key)->key, *parse_key);
    EXPECT_EQ(index->dependencies_for(*parse_key), std::vector<query::QueryKey>{*lex_key});
    EXPECT_EQ(index->dependents_of(*content_key), std::vector<query::QueryKey>{*lex_key});
    EXPECT_TRUE(index->has_dependency(*parse_key, *lex_key));
    EXPECT_TRUE(index->has_dependency(*lex_key, *content_key));
    EXPECT_FALSE(index->has_dependency(*content_key, *lex_key));
    EXPECT_FALSE(index->has_dependency(*parse_key, query::QueryKey{}));
    EXPECT_EQ(index->find(query::QueryKey{}), nullptr);
    EXPECT_EQ(index->find(query::QueryNodeId{}), nullptr);
    EXPECT_EQ(index->record_for(query::QueryKey{}), nullptr);
    EXPECT_TRUE(index->dependencies_for(query::QueryKey{}).empty());
    EXPECT_TRUE(index->dependents_of(query::QueryKey{}).empty());

    const std::optional<query::QueryReplayIndex> empty_index =
        query::QueryReplayIndex::build(query::QueryReplaySnapshot{});
    ASSERT_TRUE(empty_index.has_value());
    EXPECT_TRUE(empty_index->empty());
}

TEST(QueryUnit, QueryReplayIndexRejectsMalformedSnapshots)
{
    const QueryContextSourceSubject subject = test_source_subject();
    query::QueryContext context;
    query::QueryExecutor executor{context};
    const std::array<query::QueryRequest, 3> requests{
        query::QueryRequest{query::FileContentProviderInput{subject.file, subject.content}},
        query::QueryRequest{query::LexFileProviderInput{subject.lex_file, subject.tokens}},
        query::QueryRequest{query::ParseFileProviderInput{subject.parse_file, subject.syntax}},
    };
    ASSERT_EQ(executor.evaluate_all(requests).summary.computed, requests.size());
    const std::optional<query::QueryReplaySnapshot> snapshot = query::make_query_replay_snapshot(context);
    ASSERT_TRUE(snapshot.has_value());

    query::QueryReplaySnapshot duplicate_node = *snapshot;
    duplicate_node.nodes.push_back(duplicate_node.nodes.front());
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(duplicate_node)).has_value());

    query::QueryReplaySnapshot malformed_record = *snapshot;
    malformed_record.nodes.front().record = query::QueryRecord{};
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(malformed_record)).has_value());

    query::QueryReplaySnapshot duplicate_edge = *snapshot;
    duplicate_edge.edges.push_back(duplicate_edge.edges.front());
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(duplicate_edge)).has_value());

    query::QueryReplaySnapshot missing_edge = *snapshot;
    missing_edge.edges.clear();
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(missing_edge)).has_value());

    query::QueryReplaySnapshot missing_dependency_row = *snapshot;
    missing_dependency_row.nodes.erase(missing_dependency_row.nodes.begin());
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(missing_dependency_row)).has_value());
}

TEST(QueryUnit, QueryGraphDependencyKindRulesCoverEveryQueryKind)
{
    constexpr query::StableFingerprint128 QUERY_TEST_GRAPH_PAYLOAD{1, 2, 3};
    constexpr query::StableFingerprint128 QUERY_TEST_GRAPH_OTHER_PAYLOAD{4, 5, 6};
    const auto make_key = [QUERY_TEST_GRAPH_PAYLOAD](const query::QueryKind kind) {
        return query::query_key(kind, QUERY_TEST_GRAPH_PAYLOAD);
    };
    const auto make_other_key = [QUERY_TEST_GRAPH_OTHER_PAYLOAD](const query::QueryKind kind) {
        return query::query_key(kind, QUERY_TEST_GRAPH_OTHER_PAYLOAD);
    };
    const auto edge_is_expected = [&](const query::QueryKind dependent, const query::QueryKind dependency) {
        return query::query_dependency_edge_kind_is_expected(query::QueryDependencyEdge{
            make_key(dependent),
            make_key(dependency),
        });
    };

    const std::array invalid_dependents{
        query::QueryKind::project_graph,
        query::QueryKind::file_content,
        query::QueryKind::function_body_syntax,
    };
    for (const query::QueryKind dependent : invalid_dependents) {
        EXPECT_FALSE(edge_is_expected(dependent, query::QueryKind::lex_file));
    }
    EXPECT_FALSE(edge_is_expected(query::QueryKind::invalid, query::QueryKind::file_content));

    const std::array accepted_edges{
        query::QueryDependencyEdge{make_key(query::QueryKind::lex_file), make_key(query::QueryKind::file_content)},
        query::QueryDependencyEdge{make_key(query::QueryKind::parse_file), make_key(query::QueryKind::lex_file)},
        query::QueryDependencyEdge{make_key(query::QueryKind::module_part), make_key(query::QueryKind::parse_file)},
        query::QueryDependencyEdge{
            make_key(query::QueryKind::module_graph),
            make_key(query::QueryKind::project_graph),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::module_graph),
            make_key(query::QueryKind::module_part),
        },
        query::QueryDependencyEdge{make_key(query::QueryKind::item_list), make_key(query::QueryKind::module_graph)},
        query::QueryDependencyEdge{make_key(query::QueryKind::module_exports), make_key(query::QueryKind::item_list)},
        query::QueryDependencyEdge{
            make_key(query::QueryKind::module_exports),
            make_other_key(query::QueryKind::module_exports),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::module_package_exports),
            make_key(query::QueryKind::item_list),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::module_package_exports),
            make_key(query::QueryKind::module_exports),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::module_package_exports),
            make_other_key(query::QueryKind::module_package_exports),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::item_signature),
            make_key(query::QueryKind::module_exports),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::generic_template_signature),
            make_key(query::QueryKind::item_list),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::generic_instance_signature),
            make_key(query::QueryKind::generic_template_signature),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::type_check_body),
            make_key(query::QueryKind::function_body_syntax),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::type_check_body),
            make_key(query::QueryKind::item_signature),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::generic_instance_body),
            make_key(query::QueryKind::generic_instance_signature),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::lower_function_ir),
            make_key(query::QueryKind::type_check_body),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::lower_function_ir),
            make_key(query::QueryKind::generic_instance_body),
        },
        query::QueryDependencyEdge{make_key(query::QueryKind::diagnostics), make_key(query::QueryKind::parse_file)},
    };
    for (const query::QueryDependencyEdge& edge : accepted_edges) {
        EXPECT_TRUE(query::query_dependency_edge_kind_is_expected(edge));
        EXPECT_FALSE(query::query_dependency_edge_kind_is_expected(query::QueryDependencyEdge{
            edge.dependent,
            edge.dependent,
        }));
    }

    const std::array rejected_edges{
        query::QueryDependencyEdge{make_key(query::QueryKind::lex_file), make_key(query::QueryKind::parse_file)},
        query::QueryDependencyEdge{make_key(query::QueryKind::parse_file), make_key(query::QueryKind::file_content)},
        query::QueryDependencyEdge{make_key(query::QueryKind::module_part), make_key(query::QueryKind::lex_file)},
        query::QueryDependencyEdge{make_key(query::QueryKind::module_graph), make_key(query::QueryKind::lex_file)},
        query::QueryDependencyEdge{make_key(query::QueryKind::item_list), make_key(query::QueryKind::module_exports)},
        query::QueryDependencyEdge{
            make_key(query::QueryKind::module_exports),
            make_key(query::QueryKind::module_graph),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::module_package_exports),
            make_key(query::QueryKind::module_graph),
        },
        query::QueryDependencyEdge{make_key(query::QueryKind::item_signature), make_key(query::QueryKind::item_list)},
        query::QueryDependencyEdge{
            make_key(query::QueryKind::generic_template_signature),
            make_key(query::QueryKind::module_exports),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::generic_instance_signature),
            make_key(query::QueryKind::item_signature),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::type_check_body),
            make_key(query::QueryKind::generic_instance_body),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::generic_instance_body),
            make_key(query::QueryKind::generic_template_signature),
        },
        query::QueryDependencyEdge{
            make_key(query::QueryKind::lower_function_ir),
            make_key(query::QueryKind::function_body_syntax),
        },
        query::QueryDependencyEdge{make_key(query::QueryKind::diagnostics), make_key(query::QueryKind::diagnostics)},
        query::QueryDependencyEdge{make_key(query::QueryKind::diagnostics), query::QueryKey{}},
        query::QueryDependencyEdge{query::QueryKey{}, make_key(query::QueryKind::file_content)},
    };
    for (const query::QueryDependencyEdge& edge : rejected_edges) {
        EXPECT_FALSE(query::query_dependency_edge_kind_is_expected(edge));
    }
}

TEST(QueryUnit, QueryEdgeVerifierAcceptsExpectedStableIdentityShapes)
{
    const QueryContextSourceSubject source_subject = test_source_subject();
    const std::optional<query::QueryRecord> file_content_record =
        query::file_content_query_record(source_subject.file, source_subject.content);
    const std::optional<query::QueryRecord> lex_file_record =
        query::lex_file_query_record(source_subject.lex_file, source_subject.tokens);
    const std::optional<query::QueryRecord> parse_file_record =
        query::parse_file_query_record(source_subject.parse_file, source_subject.syntax);
    const query::ModuleKey source_module = test_module(source_subject.file.package);
    const query::ModulePartKey module_part =
        query::module_part_key(source_module, source_subject.file, query::ModulePartKind::primary, "<primary>");
    const std::optional<query::QueryRecord> module_part_record =
        query::module_part_query_record(module_part, test_query_result(QUERY_TEST_MODULE_PART));
    ASSERT_TRUE(file_content_record.has_value());
    ASSERT_TRUE(lex_file_record.has_value());
    ASSERT_TRUE(parse_file_record.has_value());
    ASSERT_TRUE(module_part_record.has_value());
    EXPECT_EQ(query::validate_query_dependency_edge_records(*lex_file_record, *file_content_record),
        query::QueryDependencyEdgeValidationStatus::valid);
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*parse_file_record, *lex_file_record));
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*module_part_record, *parse_file_record));

    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::ModuleKey reexported_module = test_reexported_module(package);
    const query::ProjectKey project = test_project_key();
    const query::DefKey function_def = test_function_def(module);
    const query::DefKey template_def = test_template_def(module);
    const query::BodyKey function_body = query::body_key(function_def, query::BodySlotKind::function_body);
    const std::optional<query::QueryRecord> module_graph_record =
        query::module_graph_query_record(module, test_query_result(QUERY_TEST_MODULE_GRAPH));
    const std::optional<query::QueryRecord> project_graph_record =
        query::project_graph_query_record(project, test_query_result(QUERY_TEST_PROJECT_GRAPH));
    const std::optional<query::QueryRecord> item_list_record =
        query::item_list_query_record(module, test_query_result(QUERY_TEST_ITEM_LIST));
    const std::optional<query::QueryRecord> exports_record =
        query::module_exports_query_record(module, test_query_result(QUERY_TEST_MODULE_EXPORTS_SIGNATURE));
    const std::optional<query::QueryRecord> reexported_exports_record =
        query::module_exports_query_record(reexported_module, test_query_result(QUERY_TEST_MODULE_EXPORTS_SIGNATURE));
    const std::optional<query::QueryRecord> item_record =
        query::item_signature_query_record(function_def, test_query_result(QUERY_TEST_PROVIDER_SIGNATURE));
    const std::optional<query::QueryRecord> template_record = query::generic_template_signature_query_record(
        template_def, test_query_result(QUERY_TEST_GENERIC_TEMPLATE_SIGNATURE));
    const std::optional<query::QueryRecord> body_syntax_record =
        query::function_body_syntax_query_record(function_body, test_query_result(QUERY_TEST_BODY_SYNTAX));
    const std::optional<query::QueryRecord> type_check_record =
        query::type_check_body_query_record(function_body, test_query_result(QUERY_TEST_TYPE_CHECK_BODY));
    const std::optional<query::QueryRecord> lower_body_record =
        query::lower_function_ir_query_record(function_body, test_query_result(QUERY_TEST_LOWER_FUNCTION_IR));
    ASSERT_TRUE(module_graph_record.has_value());
    ASSERT_TRUE(project_graph_record.has_value());
    ASSERT_TRUE(item_list_record.has_value());
    ASSERT_TRUE(exports_record.has_value());
    ASSERT_TRUE(reexported_exports_record.has_value());
    ASSERT_TRUE(item_record.has_value());
    ASSERT_TRUE(template_record.has_value());
    ASSERT_TRUE(body_syntax_record.has_value());
    ASSERT_TRUE(type_check_record.has_value());
    ASSERT_TRUE(lower_body_record.has_value());

    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*module_graph_record, *project_graph_record));
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*module_graph_record, *module_part_record));
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*item_list_record, *module_graph_record));
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*exports_record, *item_list_record));
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*exports_record, *reexported_exports_record));
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*item_record, *exports_record));
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*template_record, *item_list_record));
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*type_check_record, *body_syntax_record));
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*type_check_record, *item_record));
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*lower_body_record, *type_check_record));

    const QueryContextGenericInstanceSignatureSubject generic_subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::QueryRecord> generic_template_record = query::generic_template_signature_query_record(
        generic_subject.key.template_def, test_query_result(QUERY_TEST_GENERIC_TEMPLATE_SIGNATURE));
    const std::optional<query::QueryRecord> generic_signature_record = query::generic_instance_signature_query_record(
        generic_subject.key, test_query_result(QUERY_TEST_PROVIDER_SIGNATURE));
    const std::optional<query::QueryRecord> generic_body_record = query::generic_instance_body_query_record(
        generic_subject.key, test_query_result(QUERY_TEST_GENERIC_INSTANCE_BODY));
    const std::optional<query::QueryRecord> lower_generic_record = query::lower_generic_instance_ir_query_record(
        generic_subject.key, test_query_result(QUERY_TEST_LOWER_FUNCTION_IR));
    ASSERT_TRUE(generic_template_record.has_value());
    ASSERT_TRUE(generic_signature_record.has_value());
    ASSERT_TRUE(generic_body_record.has_value());
    ASSERT_TRUE(lower_generic_record.has_value());
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*generic_signature_record, *generic_template_record));
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*generic_body_record, *generic_signature_record));
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*lower_generic_record, *generic_body_record));

    const std::optional<query::QueryRecord> diagnostics_record =
        query::diagnostics_query_record(item_record->key, test_query_result(QUERY_TEST_DIAGNOSTICS));
    ASSERT_TRUE(diagnostics_record.has_value());
    EXPECT_TRUE(query::query_dependency_edge_records_are_valid(*diagnostics_record, *item_record));
}

TEST(QueryUnit, QueryEdgeVerifierRejectsMalformedKindsAndStableIdentities)
{
    const QueryContextSourceSubject source_subject = test_source_subject();
    const std::optional<query::QueryRecord> file_content_record =
        query::file_content_query_record(source_subject.file, source_subject.content);
    const std::optional<query::QueryRecord> lex_file_record =
        query::lex_file_query_record(source_subject.lex_file, source_subject.tokens);
    const std::optional<query::QueryRecord> parse_file_record =
        query::parse_file_query_record(source_subject.parse_file, source_subject.syntax);
    ASSERT_TRUE(file_content_record.has_value());
    ASSERT_TRUE(lex_file_record.has_value());
    ASSERT_TRUE(parse_file_record.has_value());

    query::QueryRecord invalid_record = *file_content_record;
    invalid_record.stable_key_bytes.clear();
    EXPECT_EQ(query::validate_query_dependency_edge_records(*lex_file_record, invalid_record),
        query::QueryDependencyEdgeValidationStatus::invalid_record);

    const query::QueryResultFingerprint result = test_query_result("edge-verifier-short-record");
    const std::optional<query::QueryRecord> short_file_record = query::query_record(
        query::QueryKind::file_content, query::stable_fingerprint("short-file"), "short-file", result);
    const std::optional<query::QueryRecord> short_lex_record =
        query::query_record(query::QueryKind::lex_file, query::stable_fingerprint("short-lex"), "short-lex", result);
    ASSERT_TRUE(short_file_record.has_value());
    ASSERT_TRUE(short_lex_record.has_value());
    EXPECT_EQ(query::validate_query_dependency_edge_records(*short_lex_record, *short_file_record),
        query::QueryDependencyEdgeValidationStatus::invalid_identity);

    const std::optional<query::QueryRecord> malformed_module_graph_record = query::query_record(
        query::QueryKind::module_graph, query::stable_fingerprint("malformed-module"), "malformed-module", result);
    const std::optional<query::QueryRecord> malformed_item_list_record = query::query_record(
        query::QueryKind::item_list, query::stable_fingerprint("malformed-module"), "malformed-module", result);
    const std::optional<query::QueryRecord> malformed_exports_record = query::query_record(
        query::QueryKind::module_exports, query::stable_fingerprint("malformed-module"), "malformed-module", result);
    const std::optional<query::QueryRecord> malformed_other_exports_record =
        query::query_record(query::QueryKind::module_exports, query::stable_fingerprint("malformed-other-module"),
            "malformed-other-module", result);
    ASSERT_TRUE(malformed_module_graph_record.has_value());
    ASSERT_TRUE(malformed_item_list_record.has_value());
    ASSERT_TRUE(malformed_exports_record.has_value());
    ASSERT_TRUE(malformed_other_exports_record.has_value());
    EXPECT_EQ(
        query::validate_query_dependency_edge_records(*malformed_item_list_record, *malformed_module_graph_record),
        query::QueryDependencyEdgeValidationStatus::invalid_identity);
    EXPECT_EQ(query::validate_query_dependency_edge_records(*malformed_exports_record, *malformed_exports_record),
        query::QueryDependencyEdgeValidationStatus::invalid_kind);
    EXPECT_EQ(query::validate_query_dependency_edge_records(*malformed_exports_record, *malformed_other_exports_record),
        query::QueryDependencyEdgeValidationStatus::invalid_identity);

    const query::LexFileKey wrong_lex_file = query::lex_file_key(source_subject.file, query::lex_config_key(true));
    const std::optional<query::QueryRecord> wrong_lex_record =
        query::lex_file_query_record(wrong_lex_file, source_subject.tokens);
    ASSERT_TRUE(wrong_lex_record.has_value());
    EXPECT_EQ(query::validate_query_dependency_edge_records(*parse_file_record, *wrong_lex_record),
        query::QueryDependencyEdgeValidationStatus::invalid_identity);

    const query::ModuleKey source_module = test_module(source_subject.file.package);
    const query::ModulePartKey module_part =
        query::module_part_key(source_module, source_subject.file, query::ModulePartKind::primary, "<primary>");
    const query::FileKey wrong_source_file = query::file_key(source_subject.file.package, "/workspace/root/other.ax");
    const query::ParseFileKey wrong_parse_file = query::parse_file_key(wrong_source_file, query::parser_config_key());
    const std::optional<query::QueryRecord> module_part_record =
        query::module_part_query_record(module_part, test_query_result(QUERY_TEST_MODULE_PART));
    const std::optional<query::QueryRecord> wrong_parse_record =
        query::parse_file_query_record(wrong_parse_file, source_subject.syntax);
    ASSERT_TRUE(module_part_record.has_value());
    ASSERT_TRUE(wrong_parse_record.has_value());
    EXPECT_EQ(query::validate_query_dependency_edge_records(*module_part_record, *wrong_parse_record),
        query::QueryDependencyEdgeValidationStatus::invalid_identity);

    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::DefKey function_def = test_function_def(module);
    const std::optional<query::QueryRecord> item_list_record =
        query::item_list_query_record(module, test_query_result(QUERY_TEST_ITEM_LIST));
    const std::optional<query::QueryRecord> exports_record =
        query::module_exports_query_record(module, test_query_result(QUERY_TEST_MODULE_EXPORTS_SIGNATURE));
    const std::optional<query::QueryRecord> item_record =
        query::item_signature_query_record(function_def, test_query_result(QUERY_TEST_PROVIDER_SIGNATURE));
    ASSERT_TRUE(item_list_record.has_value());
    ASSERT_TRUE(exports_record.has_value());
    ASSERT_TRUE(item_record.has_value());
    EXPECT_EQ(query::validate_query_dependency_edge_records(*item_record, *item_list_record),
        query::QueryDependencyEdgeValidationStatus::invalid_kind);

    const std::array<std::string_view, 1> wrong_module_path{"wrong_module"};
    const query::ModuleKey wrong_module = query::module_key(package, wrong_module_path);
    const std::optional<query::QueryRecord> wrong_exports_record =
        query::module_exports_query_record(wrong_module, test_query_result(QUERY_TEST_MODULE_EXPORTS_SIGNATURE));
    ASSERT_TRUE(wrong_exports_record.has_value());
    EXPECT_EQ(query::validate_query_dependency_edge_records(*item_record, *wrong_exports_record),
        query::QueryDependencyEdgeValidationStatus::invalid_identity);

    const std::optional<query::QueryRecord> diagnostics_record =
        query::diagnostics_query_record(item_record->key, test_query_result(QUERY_TEST_DIAGNOSTICS));
    ASSERT_TRUE(diagnostics_record.has_value());
    EXPECT_EQ(query::validate_query_dependency_edge_records(*diagnostics_record, *exports_record),
        query::QueryDependencyEdgeValidationStatus::invalid_identity);
}

TEST(QueryUnit, ModuleExportsProviderBuildsRecordFromStableModule)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::QueryResultFingerprint exports =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_MODULE_EXPORTS_SIGNATURE));
    const std::optional<query::QueryKey> expected_key = query::module_exports_query_key(module);
    ASSERT_TRUE(expected_key.has_value());

    const query::ModuleExportsProviderInput input{
        module,
        exports,
    };
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::ModuleExportsProviderOutput> output = query::provide_module_exports_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    const std::optional<query::QueryKey> item_list_key = query::item_list_query_key(module);
    ASSERT_TRUE(item_list_key.has_value());
    EXPECT_EQ(output->dependencies, std::vector<query::QueryKey>{*item_list_key});
    EXPECT_EQ(output->record.key, *expected_key);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::module_exports);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(module));
    EXPECT_EQ(output->result, exports);
    EXPECT_EQ(output->record.result, output->result);

    EXPECT_FALSE(query::module_exports_query_key(query::ModuleKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::ModuleExportsProviderInput{}));
    EXPECT_FALSE(query::provide_module_exports_query(query::ModuleExportsProviderInput{query::ModuleKey{}, exports})
            .has_value());
    EXPECT_FALSE(
        query::provide_module_exports_query(query::ModuleExportsProviderInput{module, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::ModuleExportsProviderOutput{}));

    query::ModuleExportsProviderOutput invalid_dependency_output = *output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));

    query::ModuleExportsProviderOutput mismatched_result_output = *output;
    mismatched_result_output.result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_CHANGED_MODULE_EXPORTS_SIGNATURE));
    EXPECT_FALSE(query::is_valid(mismatched_result_output));
}

TEST(QueryUnit, ModuleExportsProviderAddsReexportModuleDependencies)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::ModuleKey reexported_module = test_reexported_module(package);
    const query::QueryResultFingerprint exports =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_MODULE_EXPORTS_SIGNATURE));
    const query::ModuleExportsProviderInput input{
        module,
        exports,
        {
            reexported_module,
            reexported_module,
            module,
        },
    };

    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::ModuleExportsProviderOutput> output = query::provide_module_exports_query(input);
    ASSERT_TRUE(output.has_value());
    const std::optional<query::QueryKey> item_list_key = query::item_list_query_key(module);
    const std::optional<query::QueryKey> reexported_exports_key = query::module_exports_query_key(reexported_module);
    ASSERT_TRUE(item_list_key.has_value());
    ASSERT_TRUE(reexported_exports_key.has_value());
    std::vector<query::QueryKey> expected_dependencies{
        *item_list_key,
        *reexported_exports_key,
    };
    sort_query_test_keys(expected_dependencies);
    EXPECT_EQ(output->dependencies, expected_dependencies);

    query::ModuleExportsProviderInput invalid_dependency_input = input;
    invalid_dependency_input.reexport_dependencies.push_back(query::ModuleKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_input));
    EXPECT_FALSE(query::provide_module_exports_query(invalid_dependency_input).has_value());
}

TEST(QueryUnit, ModulePackageExportsProviderAddsPublicAndPackageDependencies)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::ModuleKey public_reexported_module = test_reexported_module(package);
    const std::array<std::string_view, 2> package_reexported_parts{"pkg", "internal"};
    const query::ModuleKey package_reexported_module = query::module_key(package, package_reexported_parts);
    const query::QueryResultFingerprint exports =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_MODULE_EXPORTS_SIGNATURE));
    const query::ModulePackageExportsProviderInput input{
        module,
        exports,
        {
            public_reexported_module,
            public_reexported_module,
            module,
        },
        {
            package_reexported_module,
            package_reexported_module,
            module,
        },
    };

    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::ModulePackageExportsProviderOutput> output =
        query::provide_module_package_exports_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    const std::optional<query::QueryKey> item_list_key = query::item_list_query_key(module);
    const std::optional<query::QueryKey> public_exports_key = query::module_exports_query_key(public_reexported_module);
    const std::optional<query::QueryKey> package_exports_key =
        query::module_package_exports_query_key(package_reexported_module);
    ASSERT_TRUE(item_list_key.has_value());
    ASSERT_TRUE(public_exports_key.has_value());
    ASSERT_TRUE(package_exports_key.has_value());
    std::vector<query::QueryKey> expected_dependencies{
        *item_list_key,
        *public_exports_key,
        *package_exports_key,
    };
    sort_query_test_keys(expected_dependencies);
    EXPECT_EQ(output->dependencies, expected_dependencies);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::module_package_exports);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(module));
    EXPECT_EQ(output->result, exports);

    EXPECT_FALSE(query::module_package_exports_query_key(query::ModuleKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::ModulePackageExportsProviderInput{}));
    EXPECT_FALSE(query::provide_module_package_exports_query(
        query::ModulePackageExportsProviderInput{query::ModuleKey{}, exports})
            .has_value());
    EXPECT_FALSE(query::provide_module_package_exports_query(
        query::ModulePackageExportsProviderInput{module, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::ModulePackageExportsProviderOutput{}));

    query::ModulePackageExportsProviderInput invalid_public_dependency = input;
    invalid_public_dependency.public_surface_dependencies.push_back(query::ModuleKey{});
    EXPECT_FALSE(query::is_valid(invalid_public_dependency));
    EXPECT_FALSE(query::provide_module_package_exports_query(invalid_public_dependency).has_value());

    query::ModulePackageExportsProviderInput invalid_package_dependency = input;
    invalid_package_dependency.package_surface_dependencies.push_back(query::ModuleKey{});
    EXPECT_FALSE(query::is_valid(invalid_package_dependency));
    EXPECT_FALSE(query::provide_module_package_exports_query(invalid_package_dependency).has_value());

    query::ModulePackageExportsProviderOutput invalid_dependency_output = *output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));
}

TEST(QueryUnit, ModulePartProviderBuildsRecordAndParseDependency)
{
    const QueryContextSourceSubject subject = test_source_subject();
    const query::ModuleKey module = test_module(subject.file.package);
    const query::ModulePartKey module_part =
        query::module_part_key(module, subject.file, query::ModulePartKind::primary, "<primary>");
    const query::QueryResultFingerprint part = test_query_result(QUERY_TEST_MODULE_PART);
    const std::optional<query::QueryKey> expected_key = query::module_part_query_key(module_part);
    const std::optional<query::QueryKey> expected_parse_key = query::parse_file_query_key(subject.parse_file);
    ASSERT_TRUE(expected_key.has_value());
    ASSERT_TRUE(expected_parse_key.has_value());

    const query::ModulePartProviderInput input{
        module_part,
        part,
    };
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::ModulePartProviderOutput> output = query::provide_module_part_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    EXPECT_EQ(output->dependencies, std::vector<query::QueryKey>{*expected_parse_key});
    EXPECT_EQ(output->record.key, *expected_key);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::module_part);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(module_part));
    EXPECT_EQ(output->result, part);
    EXPECT_EQ(output->record.result, output->result);

    query::QueryContext context;
    const query::QueryEvaluationResult result = context.evaluate_module_part(input);
    EXPECT_EQ(result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(result.node, nullptr);
    EXPECT_TRUE(context.has_dependency(*expected_key, *expected_parse_key));

    EXPECT_FALSE(query::module_part_query_key(query::ModulePartKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::ModulePartProviderInput{}));
    EXPECT_FALSE(
        query::provide_module_part_query(query::ModulePartProviderInput{query::ModulePartKey{}, part}).has_value());
    EXPECT_FALSE(
        query::provide_module_part_query(query::ModulePartProviderInput{module_part, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::ModulePartProviderOutput{}));

    query::ModulePartProviderOutput invalid_dependency_output = *output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));
}

TEST(QueryUnit, ProjectGraphProviderBuildsRecordWithoutIncidentalDependencies)
{
    const query::ProjectKey project = test_project_key();
    const query::QueryResultFingerprint graph = test_query_result(QUERY_TEST_PROJECT_GRAPH);
    const std::optional<query::QueryKey> expected_key = query::project_graph_query_key(project);
    ASSERT_TRUE(expected_key.has_value());

    const query::ProjectGraphProviderInput input{
        project,
        graph,
    };
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::ProjectGraphProviderOutput> output = query::provide_project_graph_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    EXPECT_EQ(output->record.key, *expected_key);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::project_graph);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(project));
    EXPECT_EQ(output->result, graph);
    EXPECT_EQ(output->record.result, output->result);
    EXPECT_TRUE(output->dependencies.empty());

    EXPECT_FALSE(query::project_graph_query_key(query::ProjectKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::ProjectGraphProviderInput{}));
    EXPECT_FALSE(
        query::provide_project_graph_query(query::ProjectGraphProviderInput{query::ProjectKey{}, graph}).has_value());
    EXPECT_FALSE(query::provide_project_graph_query(query::ProjectGraphProviderInput{
                                                        project,
                                                        query::QueryResultFingerprint{},
                                                    })
            .has_value());

    query::ProjectGraphProviderOutput invalid_dependency_output = *output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));

    query::ProjectGraphProviderOutput mismatched_result_output = *output;
    mismatched_result_output.result = test_query_result(QUERY_TEST_CHANGED_MODULE_EXPORTS_SIGNATURE);
    EXPECT_FALSE(query::is_valid(mismatched_result_output));
}

TEST(QueryUnit, QueryContextProjectGraphOverrideFailureKeepsFailedNodeOutOfEdges)
{
    const query::ProjectKey project = test_project_key();
    const query::QueryResultFingerprint graph = test_query_result(QUERY_TEST_PROJECT_GRAPH);
    const std::optional<query::QueryKey> project_graph_key = query::project_graph_query_key(project);
    ASSERT_TRUE(project_graph_key.has_value());

    query::QueryContext context;
    context.set_project_graph_provider(
        [](const query::ProjectGraphProviderInput&) -> std::optional<query::ProjectGraphProviderOutput> {
            return std::nullopt;
        });
    context.set_module_part_provider([](const query::ModulePartProviderInput& input) {
        return query::provide_module_part_query(input);
    });
    context.set_module_package_exports_provider([](const query::ModulePackageExportsProviderInput& input) {
        return query::provide_module_package_exports_query(input);
    });

    const query::QueryEvaluationResult result = context.evaluate_project_graph(query::ProjectGraphProviderInput{
        project,
        graph,
    });
    EXPECT_EQ(result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(result.node, nullptr);
    EXPECT_TRUE(context.dependency_edges().empty());

    const std::optional<query::QueryNodeId> node_id = context.node_id_for(*project_graph_key);
    ASSERT_TRUE(node_id.has_value());
    EXPECT_EQ(context.find(*node_id), result.node);
}

TEST(QueryUnit, ModuleGraphAndItemListProvidersBuildRecordsAndDependencies)
{
    const query::ModuleKey module = test_module(test_package());
    const QueryContextSourceSubject source_subject = test_source_subject();
    const query::ModulePartKey module_part =
        query::module_part_key(module, source_subject.file, query::ModulePartKind::primary, "<primary>");
    const query::ProjectKey project = test_project_key();
    const query::QueryResultFingerprint graph =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_MODULE_GRAPH));
    const query::QueryResultFingerprint items =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_ITEM_LIST));
    const std::optional<query::QueryKey> project_graph_key = query::project_graph_query_key(project);
    const std::optional<query::QueryKey> module_part_key = query::module_part_query_key(module_part);
    const std::optional<query::QueryKey> graph_key = query::module_graph_query_key(module);
    const std::optional<query::QueryKey> item_list_key = query::item_list_query_key(module);
    ASSERT_TRUE(project_graph_key.has_value());
    ASSERT_TRUE(module_part_key.has_value());
    ASSERT_TRUE(graph_key.has_value());
    ASSERT_TRUE(item_list_key.has_value());
    const std::vector<query::QueryKey> graph_dependencies{*project_graph_key, *module_part_key};

    const query::ModuleGraphProviderInput graph_input{
        module,
        graph,
        graph_dependencies,
    };
    ASSERT_TRUE(query::is_valid(graph_input));
    const std::optional<query::ModuleGraphProviderOutput> graph_output = query::provide_module_graph_query(graph_input);
    ASSERT_TRUE(graph_output.has_value());
    EXPECT_TRUE(query::is_valid(*graph_output));
    EXPECT_EQ(graph_output->dependencies, graph_dependencies);
    EXPECT_EQ(graph_output->record.key, *graph_key);
    EXPECT_EQ(graph_output->record.key.kind, query::QueryKind::module_graph);
    EXPECT_EQ(graph_output->record.stable_key_bytes, query::stable_serialize(module));
    EXPECT_EQ(graph_output->result, graph);
    EXPECT_EQ(graph_output->record.result, graph_output->result);

    const query::ItemListProviderInput item_input{
        module,
        items,
    };
    ASSERT_TRUE(query::is_valid(item_input));
    const std::optional<query::ItemListProviderOutput> item_output = query::provide_item_list_query(item_input);
    ASSERT_TRUE(item_output.has_value());
    EXPECT_TRUE(query::is_valid(*item_output));
    EXPECT_EQ(item_output->dependencies, std::vector<query::QueryKey>{*graph_key});
    EXPECT_EQ(item_output->record.key, *item_list_key);
    EXPECT_EQ(item_output->record.key.kind, query::QueryKind::item_list);
    EXPECT_EQ(item_output->record.stable_key_bytes, query::stable_serialize(module));
    EXPECT_EQ(item_output->result, items);
    EXPECT_EQ(item_output->record.result, item_output->result);

    EXPECT_FALSE(query::module_graph_query_key(query::ModuleKey{}).has_value());
    EXPECT_FALSE(query::item_list_query_key(query::ModuleKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::ModuleGraphProviderInput{}));
    EXPECT_FALSE(query::is_valid(query::ItemListProviderInput{}));
    EXPECT_FALSE(
        query::provide_module_graph_query(query::ModuleGraphProviderInput{query::ModuleKey{}, graph, {}}).has_value());
    EXPECT_FALSE(
        query::provide_module_graph_query(query::ModuleGraphProviderInput{module, query::QueryResultFingerprint{}, {}})
            .has_value());
    EXPECT_FALSE(query::provide_module_graph_query(query::ModuleGraphProviderInput{module, graph, {query::QueryKey{}}})
            .has_value());
    EXPECT_FALSE(query::provide_item_list_query(query::ItemListProviderInput{query::ModuleKey{}, items}).has_value());
    EXPECT_FALSE(query::provide_item_list_query(query::ItemListProviderInput{module, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::ModuleGraphProviderOutput{}));
    EXPECT_FALSE(query::is_valid(query::ItemListProviderOutput{}));

    query::ModuleGraphProviderOutput invalid_graph_dependency_output = *graph_output;
    invalid_graph_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_graph_dependency_output));

    query::ItemListProviderOutput invalid_dependency_output = *item_output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));

    query::ModuleGraphProviderOutput mismatched_result_output = *graph_output;
    mismatched_result_output.result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE));
    EXPECT_FALSE(query::is_valid(mismatched_result_output));
}

TEST(QueryUnit, SourceFileProvidersBuildRecordsAndPipelineDependencies)
{
    const QueryContextSourceSubject subject = test_source_subject();
    const std::optional<query::QueryKey> expected_content_key = query::file_content_query_key(subject.file);
    const std::optional<query::QueryKey> expected_lex_key = query::lex_file_query_key(subject.lex_file);
    const std::optional<query::QueryKey> expected_parse_key = query::parse_file_query_key(subject.parse_file);
    ASSERT_TRUE(expected_content_key.has_value());
    ASSERT_TRUE(expected_lex_key.has_value());
    ASSERT_TRUE(expected_parse_key.has_value());

    const query::FileContentProviderInput content_input{
        subject.file,
        subject.content,
    };
    ASSERT_TRUE(query::is_valid(content_input));
    const std::optional<query::FileContentProviderOutput> content_output =
        query::provide_file_content_query(content_input);
    ASSERT_TRUE(content_output.has_value());
    EXPECT_TRUE(query::is_valid(*content_output));
    EXPECT_TRUE(content_output->dependencies.empty());
    EXPECT_EQ(content_output->record.key, *expected_content_key);
    EXPECT_EQ(content_output->record.stable_key_bytes, query::stable_serialize(subject.file));
    EXPECT_EQ(content_output->result, subject.content);
    EXPECT_EQ(content_output->record.result, content_output->result);

    const query::LexFileProviderInput lex_input{
        subject.lex_file,
        subject.tokens,
    };
    ASSERT_TRUE(query::is_valid(lex_input));
    const std::optional<query::LexFileProviderOutput> lex_output = query::provide_lex_file_query(lex_input);
    ASSERT_TRUE(lex_output.has_value());
    EXPECT_TRUE(query::is_valid(*lex_output));
    EXPECT_EQ(lex_output->dependencies, std::vector<query::QueryKey>{*expected_content_key});
    EXPECT_EQ(lex_output->record.key, *expected_lex_key);
    EXPECT_EQ(lex_output->record.stable_key_bytes, query::stable_serialize(subject.lex_file));
    EXPECT_EQ(lex_output->result, subject.tokens);
    EXPECT_EQ(lex_output->record.result, lex_output->result);

    const query::ParseFileProviderInput parse_input{
        subject.parse_file,
        subject.syntax,
    };
    ASSERT_TRUE(query::is_valid(parse_input));
    const std::optional<query::ParseFileProviderOutput> parse_output = query::provide_parse_file_query(parse_input);
    ASSERT_TRUE(parse_output.has_value());
    EXPECT_TRUE(query::is_valid(*parse_output));
    EXPECT_EQ(parse_output->dependencies, std::vector<query::QueryKey>{*expected_lex_key});
    EXPECT_EQ(parse_output->record.key, *expected_parse_key);
    EXPECT_EQ(parse_output->record.stable_key_bytes, query::stable_serialize(subject.parse_file));
    EXPECT_EQ(parse_output->result, subject.syntax);
    EXPECT_EQ(parse_output->record.result, parse_output->result);

    EXPECT_FALSE(query::file_content_query_key(query::FileKey{}).has_value());
    EXPECT_FALSE(query::lex_file_query_key(query::LexFileKey{}).has_value());
    EXPECT_FALSE(query::parse_file_query_key(query::ParseFileKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::FileContentProviderInput{}));
    EXPECT_FALSE(query::provide_file_content_query(query::FileContentProviderInput{query::FileKey{}, subject.content})
            .has_value());
    EXPECT_FALSE(query::provide_file_content_query(
        query::FileContentProviderInput{subject.file, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::LexFileProviderInput{}));
    EXPECT_FALSE(
        query::provide_lex_file_query(query::LexFileProviderInput{query::LexFileKey{}, subject.tokens}).has_value());
    EXPECT_FALSE(
        query::provide_lex_file_query(query::LexFileProviderInput{subject.lex_file, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::ParseFileProviderInput{}));
    EXPECT_FALSE(query::provide_parse_file_query(query::ParseFileProviderInput{query::ParseFileKey{}, subject.syntax})
            .has_value());
    EXPECT_FALSE(query::provide_parse_file_query(
        query::ParseFileProviderInput{subject.parse_file, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::FileContentProviderOutput{}));
    EXPECT_FALSE(query::is_valid(query::LexFileProviderOutput{}));
    EXPECT_FALSE(query::is_valid(query::ParseFileProviderOutput{}));

    query::LexFileProviderOutput invalid_dependency_output = *lex_output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));

    query::ParseFileProviderOutput mismatched_result_output = *parse_output;
    mismatched_result_output.result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE));
    EXPECT_FALSE(query::is_valid(mismatched_result_output));
}

TEST(QueryUnit, QueryProviderSetOwnsProviderStrategiesAndDefaultFallbacks)
{
    const QueryContextSourceSubject subject = test_source_subject();
    base::usize content_provider_calls = 0;
    query::QueryProviderSet providers;
    providers.set_file_content_provider(
        [&content_provider_calls](const query::FileContentProviderInput& provider_input) {
            ++content_provider_calls;
            return query::provide_file_content_query(provider_input);
        });

    const query::FileContentProviderInput content_input{
        subject.file,
        subject.content,
    };
    const std::optional<query::FileContentProviderOutput> custom_content =
        providers.provide_file_content(content_input);
    ASSERT_TRUE(custom_content.has_value());
    EXPECT_EQ(custom_content->result, subject.content);
    EXPECT_EQ(content_provider_calls, 1U);

    providers.set_file_content_provider({});
    const std::optional<query::FileContentProviderOutput> fallback_content =
        providers.provide_file_content(content_input);
    ASSERT_TRUE(fallback_content.has_value());
    EXPECT_EQ(fallback_content->record.key, custom_content->record.key);
    EXPECT_EQ(fallback_content->result, custom_content->result);
    EXPECT_EQ(content_provider_calls, 1U);

    query::QueryProviderSet item_providers{
        [](const query::ItemSignatureProviderInput&) -> std::optional<query::ItemSignatureProviderOutput> {
            return std::nullopt;
        },
    };
    const QueryContextItemSignatureSubject item_subject =
        test_item_signature_subject("provider_set_item", QUERY_TEST_PROVIDER_SIGNATURE);
    EXPECT_FALSE(item_providers.provide_item_signature(item_subject.input).has_value());
    item_providers.set_item_signature_provider({});
    const std::optional<query::ItemSignatureProviderOutput> fallback_item =
        item_providers.provide_item_signature(item_subject.input);
    ASSERT_TRUE(fallback_item.has_value());
    EXPECT_TRUE(query::is_valid(*fallback_item));
}

TEST(QueryUnit, QueryProviderOverridesAggregateProviderWiring)
{
    const QueryContextSourceSubject subject = test_source_subject();
    base::usize content_provider_calls = 0;
    base::usize lex_provider_calls = 0;
    query::QueryProviderOverrides overrides;
    overrides.file_content = [&content_provider_calls](const query::FileContentProviderInput& provider_input) {
        ++content_provider_calls;
        return query::provide_file_content_query(provider_input);
    };
    overrides.lex_file = [&lex_provider_calls](
                             const query::LexFileProviderInput&) -> std::optional<query::LexFileProviderOutput> {
        ++lex_provider_calls;
        return std::nullopt;
    };
    query::QueryProviderSet providers{std::move(overrides)};

    const std::optional<query::FileContentProviderOutput> content_output =
        providers.provide_file_content(query::FileContentProviderInput{
            subject.file,
            subject.content,
        });
    ASSERT_TRUE(content_output.has_value());
    EXPECT_EQ(content_output->result, subject.content);
    EXPECT_EQ(content_provider_calls, 1U);

    EXPECT_FALSE(providers.provide_lex_file(query::LexFileProviderInput{subject.lex_file, subject.tokens}).has_value());
    EXPECT_EQ(lex_provider_calls, 1U);

    const std::optional<query::ParseFileProviderOutput> fallback_parse =
        providers.provide_parse_file(query::ParseFileProviderInput{
            subject.parse_file,
            subject.syntax,
        });
    ASSERT_TRUE(fallback_parse.has_value());
    EXPECT_TRUE(query::is_valid(*fallback_parse));
}

TEST(QueryUnit, QueryContextAcceptsProviderOverridesAggregate)
{
    const QueryContextSourceSubject subject = test_source_subject();
    base::usize content_provider_calls = 0;
    query::QueryProviderOverrides overrides;
    overrides.file_content = [&content_provider_calls](const query::FileContentProviderInput& provider_input) {
        ++content_provider_calls;
        return query::provide_file_content_query(provider_input);
    };
    query::QueryContext context{std::move(overrides)};

    const query::QueryEvaluationResult content_result =
        context.evaluate_file_content(query::FileContentProviderInput{subject.file, subject.content});
    ASSERT_EQ(content_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(content_result.node, nullptr);
    EXPECT_EQ(content_provider_calls, 1U);

    const query::QueryEvaluationResult cached_content =
        context.evaluate_file_content(query::FileContentProviderInput{subject.file, subject.content});
    EXPECT_EQ(cached_content.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(content_provider_calls, 1U);

    const query::QueryEvaluationResult default_parse =
        context.evaluate_parse_file(query::ParseFileProviderInput{subject.parse_file, subject.syntax});
    EXPECT_EQ(default_parse.status, query::QueryEvaluationStatus::computed);
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
        test_item_signature_authority(function_def, signature),
    };
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::ItemSignatureProviderOutput> output = query::provide_item_signature_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    const std::optional<query::QueryKey> module_exports_key = query::module_exports_query_key(function_def.module);
    ASSERT_TRUE(module_exports_key.has_value());
    EXPECT_EQ(output->dependencies, std::vector<query::QueryKey>{*module_exports_key});
    EXPECT_EQ(output->record.key.kind, query::QueryKind::item_signature);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(function_def));
    EXPECT_EQ(output->result, query::item_signature_result_fingerprint(input.authority));
    EXPECT_EQ(output->record.result, output->result);

    const query::StableDefId stable_trait =
        query::stable_definition_id(stable_module, query::StableSymbolKind::type, "Reader");
    const query::DefKey trait_def =
        query::def_key_from_stable_id(stable_trait, query::DefNamespace::trait_, query::DefKind::trait_);
    const query::StableDefId stable_trait_method =
        query::stable_definition_id(stable_module, query::StableSymbolKind::method, "Reader.read");
    const query::DefKey trait_method_def =
        query::def_key_from_stable_id(stable_trait_method, query::DefNamespace::member, query::DefKind::trait_method);
    const query::StableDefId stable_impl =
        query::stable_definition_id(stable_module, query::StableSymbolKind::synthetic, "Reader_for_File");
    const query::DefKey impl_def =
        query::def_key_from_stable_id(stable_impl, query::DefNamespace::impl_, query::DefKind::synthetic);
    const std::array<std::pair<query::DefKey, query::StableDefId>, 3> wp2_defs{
        std::pair{trait_def, stable_trait},
        std::pair{trait_method_def, stable_trait_method},
        std::pair{impl_def, stable_impl},
    };
    for (const auto& [def, stable_def] : wp2_defs) {
        const query::IncrementalKey wp2_signature =
            query::stable_incremental_key(stable_def, QUERY_TEST_PROVIDER_SIGNATURE);
        const query::ItemSignatureProviderInput wp2_input{
            def,
            test_item_signature_authority(def, wp2_signature),
        };
        ASSERT_TRUE(query::is_valid(wp2_input));
        const std::optional<query::ItemSignatureProviderOutput> wp2_output =
            query::provide_item_signature_query(wp2_input);
        ASSERT_TRUE(wp2_output.has_value());
        EXPECT_TRUE(query::is_valid(*wp2_output));
        EXPECT_EQ(wp2_output->record.key.kind, query::QueryKind::item_signature);
        EXPECT_EQ(wp2_output->record.stable_key_bytes, query::stable_serialize(def));
    }

    query::ItemSignatureAuthority invalid_authority = input.authority;
    invalid_authority.signature = {};
    EXPECT_FALSE(query::is_valid(query::ItemSignatureProviderInput{}));
    EXPECT_FALSE(
        query::provide_item_signature_query(query::ItemSignatureProviderInput{query::DefKey{}, input.authority})
            .has_value());
    EXPECT_FALSE(query::provide_item_signature_query(query::ItemSignatureProviderInput{function_def, invalid_authority})
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

TEST(QueryUnit, StableIdAdaptersCanBindPackageAwareQueryKeys)
{
    const std::array<std::string_view, 2> stable_module_path{"regex", "vm"};
    const query::StableModuleId stable_module = query::stable_module_id(stable_module_path);
    const query::StableDefId stable_function =
        query::stable_definition_id(stable_module, query::StableSymbolKind::function, "compute");
    const query::PackageKey package = test_package();
    const query::ModuleKey legacy_module = query::module_key_from_stable_id(stable_module);
    const query::ModuleKey packaged_module = query::module_key_from_stable_id(package, stable_module);
    const query::DefKey legacy_function =
        query::def_key_from_stable_id(stable_function, query::DefNamespace::value, query::DefKind::function);
    const query::DefKey packaged_function =
        query::def_key_from_stable_id(package, stable_function, query::DefNamespace::value, query::DefKind::function);

    ASSERT_TRUE(query::is_valid(packaged_module));
    ASSERT_TRUE(query::is_valid(packaged_function));
    EXPECT_EQ(packaged_module.package, package);
    EXPECT_EQ(packaged_function.module, packaged_module);
    EXPECT_EQ(packaged_function.path, stable_function.name);
    EXPECT_NE(packaged_module, legacy_module);
    EXPECT_NE(packaged_function, legacy_function);
}

TEST(QueryUnit, GenericTemplateSignatureProviderBuildsRecordFromStableTemplate)
{
    const query::PackageKey package = test_package();
    const query::ModuleKey module = test_module(package);
    const query::DefKey template_def = test_template_def(module);
    const std::array<std::string_view, 2> stable_module_path{"regex", "vm"};
    const query::StableModuleId stable_module = query::stable_module_id(stable_module_path);
    const query::StableDefId stable_template =
        query::stable_definition_id(stable_module, query::StableSymbolKind::generic_template, "Vec");
    const query::IncrementalKey signature =
        query::stable_incremental_key(stable_template, QUERY_TEST_GENERIC_TEMPLATE_SIGNATURE);
    const query::GenericTemplateSignatureAuthority authority =
        test_template_signature_authority(template_def, signature);
    const std::optional<query::QueryKey> expected_key = query::generic_template_signature_query_key(template_def);
    ASSERT_TRUE(expected_key.has_value());

    const query::GenericTemplateSignatureProviderInput input{
        template_def,
        authority,
    };
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::GenericTemplateSignatureProviderOutput> output =
        query::provide_generic_template_signature_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    const std::optional<query::QueryKey> item_list_key = query::item_list_query_key(template_def.module);
    ASSERT_TRUE(item_list_key.has_value());
    EXPECT_EQ(output->dependencies, std::vector<query::QueryKey>{*item_list_key});
    EXPECT_EQ(output->record.key, *expected_key);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::generic_template_signature);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(template_def));
    EXPECT_EQ(output->result, query::generic_template_signature_result_fingerprint(authority));
    EXPECT_EQ(output->record.result, output->result);

    query::GenericTemplateSignatureAuthority different_constraint_authority = authority;
    ++different_constraint_authority.constraint_count;
    EXPECT_NE(query::generic_template_signature_result_fingerprint(different_constraint_authority), output->result);

    const query::DefKey function_def = test_function_def(module);
    query::GenericTemplateSignatureAuthority invalid_authority = authority;
    invalid_authority.signature = {};
    query::GenericTemplateSignatureAuthority invalid_part_authority = authority;
    invalid_part_authority.module_part = {};
    EXPECT_FALSE(query::generic_template_signature_query_key(query::DefKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::GenericTemplateSignatureProviderInput{}));
    EXPECT_FALSE(query::provide_generic_template_signature_query(
        query::GenericTemplateSignatureProviderInput{query::DefKey{}, authority})
            .has_value());
    EXPECT_FALSE(query::provide_generic_template_signature_query(
        query::GenericTemplateSignatureProviderInput{function_def, authority})
            .has_value());
    EXPECT_FALSE(query::provide_generic_template_signature_query(
        query::GenericTemplateSignatureProviderInput{template_def, invalid_authority})
            .has_value());
    EXPECT_FALSE(query::provide_generic_template_signature_query(
        query::GenericTemplateSignatureProviderInput{template_def, invalid_part_authority})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::GenericTemplateSignatureProviderOutput{}));

    query::GenericTemplateSignatureProviderOutput invalid_dependency_output = *output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));

    query::GenericTemplateSignatureProviderOutput mismatched_result_output = *output;
    mismatched_result_output.result = query::query_result_fingerprint(
        query::stable_incremental_key(stable_template, QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE));
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
    const std::optional<query::QueryKey> template_signature_key =
        query::generic_template_signature_query_key(subject.key.template_def);
    ASSERT_TRUE(template_signature_key.has_value());
    EXPECT_EQ(output->dependencies, std::vector<query::QueryKey>{*template_signature_key});
    EXPECT_EQ(output->record.key, *expected_key);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::generic_instance_signature);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(subject.key));
    EXPECT_EQ(output->result, query::generic_instance_signature_result_fingerprint(subject.authority));
    EXPECT_EQ(output->record.result, output->result);

    query::GenericInstanceSignatureAuthority method_authority = subject.authority;
    method_authority.kind = query::GenericInstanceSignatureKind::method;
    method_authority.has_receiver_type = true;
    EXPECT_NE(query::generic_instance_signature_result_fingerprint(method_authority), output->result);

    const query::GenericInstanceKey invalid_key;
    query::GenericInstanceSignatureAuthority invalid_authority = subject.authority;
    invalid_authority.signature = {};
    query::GenericInstanceSignatureAuthority mismatched_arg_count_authority = subject.authority;
    ++mismatched_arg_count_authority.type_arg_count;
    EXPECT_FALSE(query::generic_instance_signature_query_key(query::GenericInstanceKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::GenericInstanceSignatureProviderInput{}));
    EXPECT_FALSE(query::provide_generic_instance_signature_query(
        query::GenericInstanceSignatureProviderInput{&invalid_key, subject.authority})
            .has_value());
    EXPECT_FALSE(query::provide_generic_instance_signature_query(
        query::GenericInstanceSignatureProviderInput{&subject.key, invalid_authority})
            .has_value());
    EXPECT_FALSE(query::provide_generic_instance_signature_query(
        query::GenericInstanceSignatureProviderInput{&subject.key, mismatched_arg_count_authority})
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

TEST(QueryUnit, GenericInstanceBodyProviderBuildsRecordAndSignatureDependency)
{
    const QueryContextGenericInstanceSignatureSubject subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const query::QueryResultFingerprint body =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_GENERIC_INSTANCE_BODY));
    const std::optional<query::QueryKey> expected_key = query::generic_instance_body_query_key(subject.key);
    ASSERT_TRUE(expected_key.has_value());

    const query::GenericInstanceBodyProviderInput input = generic_instance_body_provider_input(subject, body);
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::GenericInstanceBodyProviderOutput> output =
        query::provide_generic_instance_body_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    const std::optional<query::QueryKey> signature_key = query::generic_instance_signature_query_key(subject.key);
    ASSERT_TRUE(signature_key.has_value());
    EXPECT_EQ(output->dependencies, std::vector<query::QueryKey>{*signature_key});
    EXPECT_EQ(output->record.key, *expected_key);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::generic_instance_body);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(subject.key));
    EXPECT_EQ(output->result, query::generic_instance_body_result_fingerprint(input.authority));
    EXPECT_EQ(output->record.result, output->result);

    query::GenericInstanceBodyAuthority side_table_authority = input.authority;
    ++side_table_authority.expr_side_table_count;
    EXPECT_NE(query::generic_instance_body_result_fingerprint(side_table_authority), output->result);

    const query::GenericInstanceKey invalid_key;
    query::GenericInstanceBodyAuthority invalid_authority = input.authority;
    invalid_authority.checked_body = {};
    query::GenericInstanceBodyAuthority invalid_signature_authority = input.authority;
    invalid_signature_authority.signature_result = {};
    EXPECT_FALSE(query::generic_instance_body_query_key(query::GenericInstanceKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::GenericInstanceBodyProviderInput{}));
    EXPECT_FALSE(query::provide_generic_instance_body_query(
        query::GenericInstanceBodyProviderInput{&invalid_key, input.authority})
            .has_value());
    EXPECT_FALSE(query::provide_generic_instance_body_query(
        query::GenericInstanceBodyProviderInput{&subject.key, invalid_authority})
            .has_value());
    EXPECT_FALSE(query::provide_generic_instance_body_query(
        query::GenericInstanceBodyProviderInput{&subject.key, invalid_signature_authority})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::GenericInstanceBodyProviderOutput{}));

    query::GenericInstanceBodyProviderOutput invalid_dependency_output = *output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));

    query::GenericInstanceBodyProviderOutput mismatched_result_output = *output;
    mismatched_result_output.result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE));
    EXPECT_FALSE(query::is_valid(mismatched_result_output));
}

TEST(QueryUnit, FunctionBodySyntaxProviderBuildsRecordFromStableBody)
{
    const QueryContextBodySubject subject = test_body_subject();
    const std::optional<query::QueryKey> expected_key = query::function_body_syntax_query_key(subject.body);
    ASSERT_TRUE(expected_key.has_value());

    const query::FunctionBodySyntaxProviderInput input{
        subject.body,
        subject.syntax_authority,
    };
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::FunctionBodySyntaxProviderOutput> output =
        query::provide_function_body_syntax_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    EXPECT_TRUE(output->dependencies.empty());
    EXPECT_EQ(output->record.key, *expected_key);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::function_body_syntax);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(subject.body));
    EXPECT_EQ(output->result, query::function_body_syntax_result_fingerprint(subject.syntax_authority));
    EXPECT_EQ(output->record.result, output->result);

    query::FunctionBodySyntaxAuthority shifted_range_authority = subject.syntax_authority;
    shifted_range_authority.range_begin += 128;
    shifted_range_authority.range_end += 128;
    EXPECT_TRUE(query::is_valid(shifted_range_authority));
    EXPECT_EQ(query::function_body_syntax_result_fingerprint(shifted_range_authority), output->result);

    query::FunctionBodySyntaxAuthority invalid_syntax_authority = subject.syntax_authority;
    invalid_syntax_authority.syntax = {};
    query::FunctionBodySyntaxAuthority invalid_range_authority = subject.syntax_authority;
    invalid_range_authority.range_begin = invalid_range_authority.range_end + 1;
    EXPECT_FALSE(query::is_valid(invalid_range_authority));
    EXPECT_FALSE(query::is_valid(query::function_body_syntax_result_fingerprint(invalid_range_authority)));
    EXPECT_FALSE(query::function_body_syntax_query_key(query::BodyKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::FunctionBodySyntaxProviderInput{}));
    EXPECT_FALSE(query::provide_function_body_syntax_query(
        query::FunctionBodySyntaxProviderInput{query::BodyKey{}, subject.syntax_authority})
            .has_value());
    EXPECT_FALSE(query::provide_function_body_syntax_query(
        query::FunctionBodySyntaxProviderInput{subject.body, invalid_syntax_authority})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::FunctionBodySyntaxProviderOutput{}));

    query::FunctionBodySyntaxProviderOutput invalid_dependency_output = *output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));

    query::FunctionBodySyntaxProviderOutput mismatched_result_output = *output;
    mismatched_result_output.result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE));
    EXPECT_FALSE(query::is_valid(mismatched_result_output));
}

TEST(QueryUnit, TypeCheckBodyProviderBuildsRecordAndBodyDependencies)
{
    const QueryContextBodySubject subject = test_body_subject();
    const std::optional<query::QueryKey> expected_key = query::type_check_body_query_key(subject.body);
    ASSERT_TRUE(expected_key.has_value());

    const query::TypeCheckBodyProviderInput input{
        subject.body,
        subject.type_check_authority,
    };
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::TypeCheckBodyProviderOutput> output = query::provide_type_check_body_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    const std::optional<query::QueryKey> body_syntax_key = query::function_body_syntax_query_key(subject.body);
    const std::optional<query::QueryKey> item_signature_key = query::item_signature_query_key(subject.body.owner);
    ASSERT_TRUE(body_syntax_key.has_value());
    ASSERT_TRUE(item_signature_key.has_value());
    EXPECT_EQ(output->dependencies, (std::vector<query::QueryKey>{*body_syntax_key, *item_signature_key}));
    EXPECT_EQ(output->record.key, *expected_key);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::type_check_body);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(subject.body));
    EXPECT_EQ(output->result, query::type_check_body_result_fingerprint(subject.type_check_authority));
    EXPECT_EQ(output->record.result, output->result);

    query::TypeCheckBodyAuthority invalid_type_authority = subject.type_check_authority;
    invalid_type_authority.checked_body = {};
    EXPECT_FALSE(query::type_check_body_query_key(query::BodyKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::TypeCheckBodyProviderInput{}));
    EXPECT_FALSE(query::provide_type_check_body_query(
        query::TypeCheckBodyProviderInput{query::BodyKey{}, subject.type_check_authority})
            .has_value());
    EXPECT_FALSE(
        query::provide_type_check_body_query(query::TypeCheckBodyProviderInput{subject.body, invalid_type_authority})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::TypeCheckBodyProviderOutput{}));

    query::TypeCheckBodyProviderOutput invalid_dependency_output = *output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));

    query::TypeCheckBodyProviderOutput mismatched_result_output = *output;
    mismatched_result_output.result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE));
    EXPECT_FALSE(query::is_valid(mismatched_result_output));
}

TEST(QueryUnit, LowerFunctionIRProviderBuildsRecordAndTypeCheckDependency)
{
    const QueryContextBodySubject subject = test_body_subject();
    const std::optional<query::QueryKey> expected_key = query::lower_function_ir_query_key(subject.body);
    ASSERT_TRUE(expected_key.has_value());

    const query::LowerFunctionIRProviderInput input{
        subject.body,
        subject.lowered_ir,
    };
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::LowerFunctionIRProviderOutput> output = query::provide_lower_function_ir_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    const std::optional<query::QueryKey> type_check_key = query::type_check_body_query_key(subject.body);
    ASSERT_TRUE(type_check_key.has_value());
    EXPECT_EQ(output->dependencies, std::vector<query::QueryKey>{*type_check_key});
    EXPECT_EQ(output->record.key, *expected_key);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::lower_function_ir);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(subject.body));
    EXPECT_EQ(output->result, subject.lowered_ir);
    EXPECT_EQ(output->record.result, output->result);

    EXPECT_FALSE(query::lower_function_ir_query_key(query::BodyKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::LowerFunctionIRProviderInput{}));
    EXPECT_FALSE(query::provide_lower_function_ir_query(
        query::LowerFunctionIRProviderInput{query::BodyKey{}, subject.lowered_ir})
            .has_value());
    EXPECT_FALSE(query::provide_lower_function_ir_query(
        query::LowerFunctionIRProviderInput{subject.body, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::LowerFunctionIRProviderOutput{}));

    query::LowerFunctionIRProviderOutput invalid_dependency_output = *output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));

    query::LowerFunctionIRProviderOutput mismatched_result_output = *output;
    mismatched_result_output.result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE));
    EXPECT_FALSE(query::is_valid(mismatched_result_output));
}

TEST(QueryUnit, LowerGenericInstanceIRProviderBuildsRecordAndGenericBodyDependency)
{
    const QueryContextGenericInstanceSignatureSubject subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const query::QueryResultFingerprint lowered_ir =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_LOWER_FUNCTION_IR));
    const std::optional<query::QueryKey> expected_key = query::lower_generic_instance_ir_query_key(subject.key);
    ASSERT_TRUE(expected_key.has_value());

    const query::LowerGenericInstanceIRProviderInput input{
        &subject.key,
        lowered_ir,
    };
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::LowerGenericInstanceIRProviderOutput> output =
        query::provide_lower_generic_instance_ir_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    const std::optional<query::QueryKey> generic_body_key = query::generic_instance_body_query_key(subject.key);
    ASSERT_TRUE(generic_body_key.has_value());
    EXPECT_EQ(output->dependencies, std::vector<query::QueryKey>{*generic_body_key});
    EXPECT_EQ(output->record.key, *expected_key);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::lower_function_ir);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(subject.key));
    EXPECT_EQ(output->result, lowered_ir);
    EXPECT_EQ(output->record.result, output->result);

    const query::GenericInstanceKey invalid_key;
    EXPECT_FALSE(query::lower_generic_instance_ir_query_key(query::GenericInstanceKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::LowerGenericInstanceIRProviderInput{}));
    EXPECT_FALSE(query::provide_lower_generic_instance_ir_query(
        query::LowerGenericInstanceIRProviderInput{&invalid_key, lowered_ir})
            .has_value());
    EXPECT_FALSE(query::provide_lower_generic_instance_ir_query(
        query::LowerGenericInstanceIRProviderInput{&subject.key, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(query::is_valid(query::LowerGenericInstanceIRProviderOutput{}));

    query::LowerGenericInstanceIRProviderOutput invalid_dependency_output = *output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));

    query::LowerGenericInstanceIRProviderOutput mismatched_result_output = *output;
    mismatched_result_output.result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE));
    EXPECT_FALSE(query::is_valid(mismatched_result_output));
}

TEST(QueryUnit, DiagnosticsProviderBuildsRecordAndProducerDependency)
{
    const QueryContextBodySubject subject = test_body_subject();
    const std::optional<query::QueryKey> producer = query::type_check_body_query_key(subject.body);
    ASSERT_TRUE(producer.has_value());
    base::DiagnosticSink sink;
    sink.push(base::Diagnostic{
        base::Severity::warning,
        base::SourceRange{base::SourceId{1}, 4U, 9U},
        "first wording",
        base::DiagnosticCategory::type,
        base::DiagnosticCode::semantic_type_mismatch,
    });
    sink.push(base::Diagnostic{
        base::Severity::help,
        base::SourceRange{base::SourceId{1}, 10U, 10U},
        "second wording",
        base::DiagnosticCategory::semantic,
        base::DiagnosticCode::semantic_error,
    });
    const query::DiagnosticsEventStream events = query::diagnostic_events_from_sink(sink.diagnostics());
    ASSERT_EQ(events.events.size(), 2U);
    EXPECT_EQ(events.events[0].ordinal, 0U);
    EXPECT_EQ(events.events[1].ordinal, 1U);
    EXPECT_EQ(events.events[0].message, "first wording");
    EXPECT_EQ(events.events[1].category, base::DiagnosticCategory::semantic);
    ASSERT_EQ(events.events[0].labels.size(), 1U);
    EXPECT_EQ(events.events[0].labels[0].style, base::DiagnosticLabelStyle::primary);
    EXPECT_EQ(events.events[0].labels[0].range.begin, 4U);
    const query::QueryResultFingerprint diagnostics = query::diagnostics_result_fingerprint(events.events);
    const std::optional<query::QueryKey> expected_key = query::diagnostics_query_key(*producer);
    ASSERT_TRUE(expected_key.has_value());

    const query::DiagnosticsProviderInput input{
        *producer,
        diagnostics,
        events.events,
    };
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::DiagnosticsProviderOutput> output = query::provide_diagnostics_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    EXPECT_EQ(output->dependencies, std::vector<query::QueryKey>{*producer});
    EXPECT_EQ(output->record.key, *expected_key);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::diagnostics);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(*producer));
    EXPECT_EQ(output->result, diagnostics);
    EXPECT_EQ(output->record.result, output->result);
    ASSERT_EQ(output->stream.events.size(), events.events.size());
    EXPECT_EQ(output->stream.events[0].message, events.events[0].message);
    EXPECT_EQ(output->stream.events[1].range.begin, events.events[1].range.begin);

    query::DiagnosticsEventStream reworded = events;
    reworded.events[0].message = "changed wording";
    EXPECT_EQ(query::diagnostics_result_fingerprint(reworded.events), diagnostics);
    reworded.events[0].range.begin += 1U;
    EXPECT_NE(query::diagnostics_result_fingerprint(reworded.events), diagnostics);

    query::DiagnosticsEventStream relabeled = events;
    relabeled.events[0].labels.push_back(
        base::secondary_diagnostic_label(base::SourceRange{base::SourceId{1}, 20U, 25U}, "related span"));
    EXPECT_NE(query::diagnostics_result_fingerprint(relabeled.events), diagnostics);

    query::DiagnosticsEventStream with_child = events;
    with_child.events[0].children.push_back(
        base::diagnostic_help(base::SourceRange{base::SourceId{1}, 26U, 31U}, "try this"));
    EXPECT_NE(query::diagnostics_result_fingerprint(with_child.events), diagnostics);

    EXPECT_FALSE(query::diagnostics_query_key(query::QueryKey{}).has_value());
    EXPECT_FALSE(query::diagnostics_query_key(*expected_key).has_value());
    EXPECT_FALSE(query::is_valid(query::DiagnosticsProviderInput{}));
    EXPECT_FALSE(
        query::provide_diagnostics_query(query::DiagnosticsProviderInput{query::QueryKey{}, diagnostics}).has_value());
    EXPECT_FALSE(
        query::provide_diagnostics_query(query::DiagnosticsProviderInput{*producer, query::QueryResultFingerprint{}})
            .has_value());
    EXPECT_FALSE(
        query::provide_diagnostics_query(query::DiagnosticsProviderInput{*expected_key, diagnostics}).has_value());
    EXPECT_FALSE(query::is_valid(query::DiagnosticsProviderOutput{}));

    query::DiagnosticsProviderOutput invalid_dependency_output = *output;
    invalid_dependency_output.dependencies.push_back(query::QueryKey{});
    EXPECT_FALSE(query::is_valid(invalid_dependency_output));

    query::DiagnosticsProviderOutput invalid_event_order_output = *output;
    invalid_event_order_output.stream.events[1].ordinal = 0U;
    EXPECT_FALSE(query::is_valid(invalid_event_order_output));

    query::DiagnosticsProviderOutput invalid_label_range_output = *output;
    invalid_label_range_output.stream.events[0].labels[0].range.begin = 99U;
    invalid_label_range_output.stream.events[0].labels[0].range.end = 1U;
    EXPECT_FALSE(query::is_valid(invalid_label_range_output));

    query::DiagnosticsProviderOutput invalid_child_range_output = *output;
    invalid_child_range_output.stream.events[0].children.push_back(
        base::diagnostic_note(base::SourceRange{base::SourceId{1}, 40U, 20U}, "bad child"));
    EXPECT_FALSE(query::is_valid(invalid_child_range_output));

    query::DiagnosticsProviderOutput mismatched_result_output = *output;
    mismatched_result_output.result =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE));
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

TEST(QueryUnit, QueryContextTracksRevisionAndReuseStateForComputedCachedAndInvalidatedNodes)
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

    EXPECT_EQ(context.current_revision(), query::QUERY_REVISION_INITIAL);
    EXPECT_EQ(context.advance_revision(), query::QUERY_REVISION_INITIAL + 1U);

    const query::QueryRevision first_revision = context.current_revision();
    const query::QueryEvaluationResult first = context.evaluate_item_signature(subject.input);
    ASSERT_EQ(first.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(first.node, nullptr);
    EXPECT_EQ(first.node->verified_revision, first_revision);
    EXPECT_EQ(first.node->changed_revision, first_revision);
    EXPECT_EQ(first.node->reuse_state, query::QueryReuseState::red);
    EXPECT_EQ(provider_calls, 1U);

    const query::QueryRevision computed_changed_revision = first.node->changed_revision;
    EXPECT_EQ(context.advance_revision(), first_revision + 1U);

    const query::QueryEvaluationResult cached = context.evaluate_item_signature(subject.input);
    ASSERT_EQ(cached.status, query::QueryEvaluationStatus::cached);
    ASSERT_NE(cached.node, nullptr);
    EXPECT_EQ(cached.node, first.node);
    EXPECT_EQ(cached.node->verified_revision, context.current_revision());
    EXPECT_EQ(cached.node->changed_revision, computed_changed_revision);
    EXPECT_EQ(cached.node->reuse_state, query::QueryReuseState::green);
    EXPECT_EQ(provider_calls, 1U);

    ASSERT_TRUE(context.invalidate(*expected_key));
    const query::QueryNode* const invalidated = context.find(*expected_key);
    ASSERT_NE(invalidated, nullptr);
    EXPECT_EQ(invalidated->status, query::QueryNodeStatus::failed);
    EXPECT_EQ(invalidated->verified_revision, context.current_revision());
    EXPECT_EQ(invalidated->changed_revision, context.current_revision());
    EXPECT_EQ(invalidated->reuse_state, query::QueryReuseState::red);

    const query::QueryEvaluationResult recomputed = context.evaluate_item_signature(subject.input);
    ASSERT_EQ(recomputed.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(recomputed.node, nullptr);
    EXPECT_EQ(recomputed.node, first.node);
    EXPECT_EQ(recomputed.node->verified_revision, context.current_revision());
    EXPECT_EQ(recomputed.node->changed_revision, context.current_revision());
    EXPECT_EQ(recomputed.node->reuse_state, query::QueryReuseState::red);
    EXPECT_EQ(provider_calls, 2U);
}

TEST(QueryUnit, QueryContextCachesModuleExportsAndEmitsCompletedRecords)
{
    const query::ModuleKey module = test_module(test_package());
    const query::QueryResultFingerprint exports =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_MODULE_EXPORTS_SIGNATURE));
    const query::ModuleExportsProviderInput input{
        module,
        exports,
    };
    const std::optional<query::QueryKey> expected_key = query::module_exports_query_key(module);
    ASSERT_TRUE(expected_key.has_value());

    base::usize provider_calls = 0;
    query::QueryContext context(
        query::ModuleExportsProvider{[&provider_calls](const query::ModuleExportsProviderInput& provider_input) {
            ++provider_calls;
            return query::provide_module_exports_query(provider_input);
        }},
        query::ItemSignatureProvider{query::provide_item_signature_query},
        query::GenericInstanceSignatureProvider{query::provide_generic_instance_signature_query});

    const query::QueryEvaluationResult first = context.evaluate_module_exports(input);
    ASSERT_EQ(first.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(first.node, nullptr);
    EXPECT_EQ(first.node->status, query::QueryNodeStatus::done);
    EXPECT_EQ(first.node->key, *expected_key);
    EXPECT_EQ(first.node->record.key, *expected_key);
    EXPECT_EQ(first.node->record.result, exports);
    EXPECT_EQ(provider_calls, 1U);
    EXPECT_EQ(context.find(*expected_key), first.node);

    const std::vector<query::QueryRecord> records = context.completed_records();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records.front().key, first.node->record.key);
    EXPECT_EQ(records.front().result, first.node->record.result);
    EXPECT_EQ(records.front().stable_key_bytes, first.node->record.stable_key_bytes);

    const query::QueryEvaluationResult second = context.evaluate_module_exports(input);
    EXPECT_EQ(second.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(second.node, first.node);
    EXPECT_EQ(provider_calls, 1U);

    context.set_module_exports_provider({});
    const query::QueryEvaluationResult cached_after_reset = context.evaluate_module_exports(input);
    EXPECT_EQ(cached_after_reset.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(provider_calls, 1U);
}

TEST(QueryUnit, QueryContextCachesModuleGraphItemListAndGenericTemplateSignature)
{
    const query::ModuleKey module = test_module(test_package());
    const query::DefKey template_def = test_template_def(module);
    const query::QueryResultFingerprint graph =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_MODULE_GRAPH));
    const query::QueryResultFingerprint items =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_ITEM_LIST));
    const std::array<std::string_view, 2> stable_module_path{"regex", "vm"};
    const query::StableModuleId stable_module = query::stable_module_id(stable_module_path);
    const query::StableDefId stable_template =
        query::stable_definition_id(stable_module, query::StableSymbolKind::generic_template, "Vec");
    const query::IncrementalKey template_signature =
        query::stable_incremental_key(stable_template, QUERY_TEST_GENERIC_TEMPLATE_SIGNATURE);
    const query::GenericTemplateSignatureAuthority template_authority =
        test_template_signature_authority(template_def, template_signature);
    const std::optional<query::QueryKey> module_graph_key = query::module_graph_query_key(module);
    const std::optional<query::QueryKey> item_list_key = query::item_list_query_key(module);
    const std::optional<query::QueryKey> template_key = query::generic_template_signature_query_key(template_def);
    ASSERT_TRUE(module_graph_key.has_value());
    ASSERT_TRUE(item_list_key.has_value());
    ASSERT_TRUE(template_key.has_value());

    base::usize module_graph_provider_calls = 0;
    base::usize item_list_provider_calls = 0;
    base::usize template_provider_calls = 0;
    query::QueryContext context;
    context.set_module_graph_provider(
        [&module_graph_provider_calls](const query::ModuleGraphProviderInput& provider_input) {
            ++module_graph_provider_calls;
            return query::provide_module_graph_query(provider_input);
        });
    context.set_item_list_provider([&item_list_provider_calls](const query::ItemListProviderInput& provider_input) {
        ++item_list_provider_calls;
        return query::provide_item_list_query(provider_input);
    });
    context.set_generic_template_signature_provider(
        [&template_provider_calls](const query::GenericTemplateSignatureProviderInput& provider_input) {
            ++template_provider_calls;
            return query::provide_generic_template_signature_query(provider_input);
        });

    const query::QueryEvaluationResult graph_result =
        context.evaluate_module_graph(query::ModuleGraphProviderInput{module, graph, {}});
    ASSERT_EQ(graph_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(graph_result.node, nullptr);
    EXPECT_EQ(graph_result.node->key, *module_graph_key);
    EXPECT_EQ(module_graph_provider_calls, 1U);

    const query::QueryEvaluationResult item_result =
        context.evaluate_item_list(query::ItemListProviderInput{module, items});
    ASSERT_EQ(item_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(item_result.node, nullptr);
    EXPECT_EQ(item_result.node->key, *item_list_key);
    EXPECT_EQ(item_list_provider_calls, 1U);
    EXPECT_EQ(context.dependencies_for(*item_list_key), std::vector<query::QueryKey>{*module_graph_key});

    const query::QueryEvaluationResult template_result = context.evaluate_generic_template_signature(
        query::GenericTemplateSignatureProviderInput{template_def, template_authority});
    ASSERT_EQ(template_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(template_result.node, nullptr);
    EXPECT_EQ(template_result.node->key, *template_key);
    EXPECT_EQ(template_provider_calls, 1U);
    EXPECT_EQ(context.dependencies_for(*template_key), std::vector<query::QueryKey>{*item_list_key});
    EXPECT_EQ(context.dependency_edge_count(), 2U);

    EXPECT_EQ(context.evaluate_module_graph(query::ModuleGraphProviderInput{module, graph, {}}).status,
        query::QueryEvaluationStatus::cached);
    EXPECT_EQ(context.evaluate_item_list(query::ItemListProviderInput{module, items}).status,
        query::QueryEvaluationStatus::cached);
    EXPECT_EQ(context
                  .evaluate_generic_template_signature(
                      query::GenericTemplateSignatureProviderInput{template_def, template_authority})
                  .status,
        query::QueryEvaluationStatus::cached);
    EXPECT_EQ(module_graph_provider_calls, 1U);
    EXPECT_EQ(item_list_provider_calls, 1U);
    EXPECT_EQ(template_provider_calls, 1U);

    context.set_module_graph_provider({});
    context.set_item_list_provider({});
    context.set_generic_template_signature_provider({});
    ASSERT_TRUE(context.invalidate(*module_graph_key));
    ASSERT_TRUE(context.invalidate(*item_list_key));
    ASSERT_TRUE(context.invalidate(*template_key));
    EXPECT_EQ(context.evaluate_module_graph(query::ModuleGraphProviderInput{module, graph, {}}).status,
        query::QueryEvaluationStatus::computed);
    EXPECT_EQ(context.evaluate_item_list(query::ItemListProviderInput{module, items}).status,
        query::QueryEvaluationStatus::computed);
    EXPECT_EQ(context
                  .evaluate_generic_template_signature(
                      query::GenericTemplateSignatureProviderInput{template_def, template_authority})
                  .status,
        query::QueryEvaluationStatus::computed);
    EXPECT_EQ(module_graph_provider_calls, 1U);
    EXPECT_EQ(item_list_provider_calls, 1U);
    EXPECT_EQ(template_provider_calls, 1U);
}

TEST(QueryUnit, QueryContextCachesSourcePipelineQueriesAndRecordsDependencies)
{
    const QueryContextSourceSubject subject = test_source_subject();
    const std::optional<query::QueryKey> content_key = query::file_content_query_key(subject.file);
    const std::optional<query::QueryKey> lex_key = query::lex_file_query_key(subject.lex_file);
    const std::optional<query::QueryKey> parse_key = query::parse_file_query_key(subject.parse_file);
    ASSERT_TRUE(content_key.has_value());
    ASSERT_TRUE(lex_key.has_value());
    ASSERT_TRUE(parse_key.has_value());

    base::usize content_provider_calls = 0;
    base::usize lex_provider_calls = 0;
    base::usize parse_provider_calls = 0;
    query::QueryContext context;
    context.set_file_content_provider([&content_provider_calls](const query::FileContentProviderInput& provider_input) {
        ++content_provider_calls;
        return query::provide_file_content_query(provider_input);
    });
    context.set_lex_file_provider([&lex_provider_calls](const query::LexFileProviderInput& provider_input) {
        ++lex_provider_calls;
        return query::provide_lex_file_query(provider_input);
    });
    context.set_parse_file_provider([&parse_provider_calls](const query::ParseFileProviderInput& provider_input) {
        ++parse_provider_calls;
        return query::provide_parse_file_query(provider_input);
    });

    const query::QueryEvaluationResult content_result =
        context.evaluate_file_content(query::FileContentProviderInput{subject.file, subject.content});
    ASSERT_EQ(content_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(content_result.node, nullptr);
    EXPECT_EQ(content_result.node->key, *content_key);
    EXPECT_EQ(content_provider_calls, 1U);

    const query::QueryEvaluationResult lex_result =
        context.evaluate_lex_file(query::LexFileProviderInput{subject.lex_file, subject.tokens});
    ASSERT_EQ(lex_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(lex_result.node, nullptr);
    EXPECT_EQ(lex_result.node->key, *lex_key);
    EXPECT_EQ(lex_provider_calls, 1U);
    EXPECT_TRUE(context.has_dependency(*lex_key, *content_key));

    const query::QueryEvaluationResult parse_result =
        context.evaluate_parse_file(query::ParseFileProviderInput{subject.parse_file, subject.syntax});
    ASSERT_EQ(parse_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(parse_result.node, nullptr);
    EXPECT_EQ(parse_result.node->key, *parse_key);
    EXPECT_EQ(parse_provider_calls, 1U);
    EXPECT_TRUE(context.has_dependency(*parse_key, *lex_key));
    EXPECT_EQ(context.dependency_edge_count(), 2U);

    const std::vector<query::QueryRecord> records = context.completed_records();
    ASSERT_EQ(records.size(), 3U);
    EXPECT_EQ(records[0].key.kind, query::QueryKind::file_content);
    EXPECT_EQ(records[1].key.kind, query::QueryKind::lex_file);
    EXPECT_EQ(records[2].key.kind, query::QueryKind::parse_file);

    const query::QueryEvaluationResult cached_parse =
        context.evaluate_parse_file(query::ParseFileProviderInput{subject.parse_file, subject.syntax});
    EXPECT_EQ(cached_parse.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(cached_parse.node, parse_result.node);
    EXPECT_EQ(parse_provider_calls, 1U);

    context.set_parse_file_provider({});
    const query::QueryEvaluationResult cached_after_reset =
        context.evaluate_parse_file(query::ParseFileProviderInput{subject.parse_file, subject.syntax});
    EXPECT_EQ(cached_after_reset.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(parse_provider_calls, 1U);
}

TEST(QueryUnit, QueryContextTracksSourcePipelineFailuresAndProviderFallbacks)
{
    const QueryContextSourceSubject subject = test_source_subject();
    const QueryContextSourceSubject other_subject = [] {
        QueryContextSourceSubject other = test_source_subject();
        other.file = query::file_key(test_package(), "/workspace/root/regex/other.ax");
        other.lex_file = query::lex_file_key(other.file, other.lex_file.config);
        other.parse_file = query::parse_file_key(other.file, other.parse_file.config);
        return other;
    }();

    query::QueryContext invalid_context;
    EXPECT_EQ(invalid_context.evaluate_file_content(query::FileContentProviderInput{}).status,
        query::QueryEvaluationStatus::failed);
    EXPECT_EQ(
        invalid_context.evaluate_lex_file(query::LexFileProviderInput{}).status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(invalid_context.evaluate_parse_file(query::ParseFileProviderInput{}).status,
        query::QueryEvaluationStatus::failed);

    query::QueryContext failing_context;
    failing_context.set_file_content_provider([](const query::FileContentProviderInput&) {
        return std::optional<query::FileContentProviderOutput>{};
    });
    const query::QueryEvaluationResult failed_content =
        failing_context.evaluate_file_content(query::FileContentProviderInput{subject.file, subject.content});
    EXPECT_EQ(failed_content.status, query::QueryEvaluationStatus::failed);
    failing_context.set_file_content_provider({});
    const query::QueryEvaluationResult retried_content =
        failing_context.evaluate_file_content(query::FileContentProviderInput{subject.file, subject.content});
    EXPECT_EQ(retried_content.status, query::QueryEvaluationStatus::computed);

    query::QueryContext wrong_key_context;
    wrong_key_context.set_lex_file_provider([&other_subject, &subject](const query::LexFileProviderInput&) {
        return query::provide_lex_file_query(query::LexFileProviderInput{other_subject.lex_file, subject.tokens});
    });
    const query::QueryEvaluationResult wrong_key_result =
        wrong_key_context.evaluate_lex_file(query::LexFileProviderInput{subject.lex_file, subject.tokens});
    EXPECT_EQ(wrong_key_result.status, query::QueryEvaluationStatus::failed);

    query::QueryContext invalid_output_context;
    invalid_output_context.set_parse_file_provider([](const query::ParseFileProviderInput& provider_input) {
        std::optional<query::ParseFileProviderOutput> output = query::provide_parse_file_query(provider_input);
        if (output) {
            output->dependencies.push_back(query::QueryKey{});
        }
        return output;
    });
    const query::QueryEvaluationResult invalid_output_result =
        invalid_output_context.evaluate_parse_file(query::ParseFileProviderInput{subject.parse_file, subject.syntax});
    EXPECT_EQ(invalid_output_result.status, query::QueryEvaluationStatus::failed);
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

TEST(QueryUnit, QueryContextCachesBodyQueriesAndRecordsTypeCheckDependencies)
{
    const QueryContextBodySubject subject = test_body_subject();
    const std::optional<query::QueryKey> body_syntax_key = query::function_body_syntax_query_key(subject.body);
    const std::optional<query::QueryKey> type_check_key = query::type_check_body_query_key(subject.body);
    const std::optional<query::QueryKey> item_signature_key = query::item_signature_query_key(subject.body.owner);
    ASSERT_TRUE(body_syntax_key.has_value());
    ASSERT_TRUE(type_check_key.has_value());
    ASSERT_TRUE(item_signature_key.has_value());

    base::usize body_provider_calls = 0;
    base::usize type_provider_calls = 0;
    query::QueryContext context;
    context.set_function_body_syntax_provider(
        [&body_provider_calls](const query::FunctionBodySyntaxProviderInput& provider_input) {
            ++body_provider_calls;
            return query::provide_function_body_syntax_query(provider_input);
        });
    context.set_type_check_body_provider(
        [&type_provider_calls](const query::TypeCheckBodyProviderInput& provider_input) {
            ++type_provider_calls;
            return query::provide_type_check_body_query(provider_input);
        });

    const query::QueryEvaluationResult body_result = context.evaluate_function_body_syntax(
        query::FunctionBodySyntaxProviderInput{subject.body, subject.syntax_authority});
    ASSERT_EQ(body_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(body_result.node, nullptr);
    EXPECT_EQ(body_result.node->key, *body_syntax_key);
    EXPECT_EQ(body_provider_calls, 1U);

    const query::QueryEvaluationResult type_result =
        context.evaluate_type_check_body(query::TypeCheckBodyProviderInput{subject.body, subject.type_check_authority});
    ASSERT_EQ(type_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(type_result.node, nullptr);
    EXPECT_EQ(type_result.node->key, *type_check_key);
    EXPECT_EQ(type_provider_calls, 1U);

    std::vector<query::QueryKey> expected_type_dependencies{
        *body_syntax_key,
        *item_signature_key,
    };
    sort_query_test_keys(expected_type_dependencies);
    EXPECT_EQ(context.dependencies_for(*type_check_key), expected_type_dependencies);
    EXPECT_TRUE(context.has_dependency(*type_check_key, *body_syntax_key));
    EXPECT_TRUE(context.has_dependency(*type_check_key, *item_signature_key));
    EXPECT_EQ(context.dependency_edge_count(), 2U);

    const query::QueryEvaluationResult cached_body = context.evaluate_function_body_syntax(
        query::FunctionBodySyntaxProviderInput{subject.body, subject.syntax_authority});
    EXPECT_EQ(cached_body.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(body_provider_calls, 1U);

    const query::QueryEvaluationResult cached_type =
        context.evaluate_type_check_body(query::TypeCheckBodyProviderInput{subject.body, subject.type_check_authority});
    EXPECT_EQ(cached_type.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(type_provider_calls, 1U);

    context.set_function_body_syntax_provider({});
    ASSERT_TRUE(context.invalidate(*body_syntax_key));
    const query::QueryEvaluationResult default_body = context.evaluate_function_body_syntax(
        query::FunctionBodySyntaxProviderInput{subject.body, subject.syntax_authority});
    ASSERT_EQ(default_body.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(default_body.node, nullptr);
    EXPECT_EQ(default_body.node->key, *body_syntax_key);
    EXPECT_EQ(body_provider_calls, 1U);

    context.set_type_check_body_provider({});
    ASSERT_TRUE(context.invalidate(*type_check_key));
    const query::QueryEvaluationResult default_type =
        context.evaluate_type_check_body(query::TypeCheckBodyProviderInput{subject.body, subject.type_check_authority});
    ASSERT_EQ(default_type.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(default_type.node, nullptr);
    EXPECT_EQ(default_type.node->key, *type_check_key);
    EXPECT_EQ(type_provider_calls, 1U);
}

TEST(QueryUnit, QueryContextTracksBodyFailuresAndProviderFallbacks)
{
    const QueryContextBodySubject subject = test_body_subject();
    const QueryContextBodySubject other_subject = test_body_subject("other");
    const std::optional<query::QueryKey> body_syntax_key = query::function_body_syntax_query_key(subject.body);
    const std::optional<query::QueryKey> type_check_key = query::type_check_body_query_key(subject.body);
    ASSERT_TRUE(body_syntax_key.has_value());
    ASSERT_TRUE(type_check_key.has_value());

    query::QueryContext invalid_input_context;
    const query::QueryEvaluationResult invalid_body_key_result = invalid_input_context.evaluate_function_body_syntax(
        query::FunctionBodySyntaxProviderInput{query::BodyKey{}, subject.syntax_authority});
    EXPECT_EQ(invalid_body_key_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(invalid_body_key_result.node, nullptr);
    const query::QueryEvaluationResult invalid_type_key_result = invalid_input_context.evaluate_type_check_body(
        query::TypeCheckBodyProviderInput{query::BodyKey{}, subject.type_check_authority});
    EXPECT_EQ(invalid_type_key_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(invalid_type_key_result.node, nullptr);

    query::QueryContext failing_context;
    failing_context.set_function_body_syntax_provider(
        [](const query::FunctionBodySyntaxProviderInput&) -> std::optional<query::FunctionBodySyntaxProviderOutput> {
            return std::nullopt;
        });
    failing_context.set_type_check_body_provider(
        [](const query::TypeCheckBodyProviderInput&) -> std::optional<query::TypeCheckBodyProviderOutput> {
            return std::nullopt;
        });
    const query::QueryEvaluationResult failed_body_result = failing_context.evaluate_function_body_syntax(
        query::FunctionBodySyntaxProviderInput{subject.body, subject.syntax_authority});
    ASSERT_EQ(failed_body_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(failed_body_result.node, nullptr);
    EXPECT_EQ(failed_body_result.node->key, *body_syntax_key);
    EXPECT_EQ(failed_body_result.node->status, query::QueryNodeStatus::failed);
    const query::QueryEvaluationResult failed_type_result = failing_context.evaluate_type_check_body(
        query::TypeCheckBodyProviderInput{subject.body, subject.type_check_authority});
    ASSERT_EQ(failed_type_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(failed_type_result.node, nullptr);
    EXPECT_EQ(failed_type_result.node->key, *type_check_key);
    EXPECT_EQ(failed_type_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(failing_context.completed_records().empty());

    failing_context.set_function_body_syntax_provider({});
    const query::QueryEvaluationResult retry_body_result = failing_context.evaluate_function_body_syntax(
        query::FunctionBodySyntaxProviderInput{subject.body, subject.syntax_authority});
    ASSERT_EQ(retry_body_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(retry_body_result.node, nullptr);
    EXPECT_EQ(retry_body_result.node->status, query::QueryNodeStatus::done);
    failing_context.set_type_check_body_provider({});
    const query::QueryEvaluationResult retry_type_result = failing_context.evaluate_type_check_body(
        query::TypeCheckBodyProviderInput{subject.body, subject.type_check_authority});
    ASSERT_EQ(retry_type_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(retry_type_result.node, nullptr);
    EXPECT_EQ(retry_type_result.node->status, query::QueryNodeStatus::done);

    query::QueryContext wrong_key_context;
    wrong_key_context.set_function_body_syntax_provider(
        [&other_subject](const query::FunctionBodySyntaxProviderInput&) {
            return query::provide_function_body_syntax_query(
                query::FunctionBodySyntaxProviderInput{other_subject.body, other_subject.syntax_authority});
        });
    wrong_key_context.set_type_check_body_provider([&other_subject](const query::TypeCheckBodyProviderInput&) {
        return query::provide_type_check_body_query(
            query::TypeCheckBodyProviderInput{other_subject.body, other_subject.type_check_authority});
    });
    const query::QueryEvaluationResult wrong_body_key_result = wrong_key_context.evaluate_function_body_syntax(
        query::FunctionBodySyntaxProviderInput{subject.body, subject.syntax_authority});
    ASSERT_EQ(wrong_body_key_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(wrong_body_key_result.node, nullptr);
    EXPECT_EQ(wrong_body_key_result.node->key, *body_syntax_key);
    const query::QueryEvaluationResult wrong_type_key_result = wrong_key_context.evaluate_type_check_body(
        query::TypeCheckBodyProviderInput{subject.body, subject.type_check_authority});
    ASSERT_EQ(wrong_type_key_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(wrong_type_key_result.node, nullptr);
    EXPECT_EQ(wrong_type_key_result.node->key, *type_check_key);

    query::QueryContext invalid_output_context;
    invalid_output_context.set_function_body_syntax_provider(
        [](const query::FunctionBodySyntaxProviderInput& provider_input) {
            std::optional<query::FunctionBodySyntaxProviderOutput> output =
                query::provide_function_body_syntax_query(provider_input);
            if (output) {
                output->dependencies.push_back(query::QueryKey{});
            }
            return output;
        });
    invalid_output_context.set_type_check_body_provider([](const query::TypeCheckBodyProviderInput& provider_input) {
        std::optional<query::TypeCheckBodyProviderOutput> output = query::provide_type_check_body_query(provider_input);
        if (output) {
            output->dependencies.push_back(query::QueryKey{});
        }
        return output;
    });
    const query::QueryEvaluationResult invalid_body_output_result =
        invalid_output_context.evaluate_function_body_syntax(
            query::FunctionBodySyntaxProviderInput{subject.body, subject.syntax_authority});
    ASSERT_EQ(invalid_body_output_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(invalid_body_output_result.node, nullptr);
    EXPECT_EQ(invalid_body_output_result.node->status, query::QueryNodeStatus::failed);
    const query::QueryEvaluationResult invalid_type_output_result = invalid_output_context.evaluate_type_check_body(
        query::TypeCheckBodyProviderInput{subject.body, subject.type_check_authority});
    ASSERT_EQ(invalid_type_output_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(invalid_type_output_result.node, nullptr);
    EXPECT_EQ(invalid_type_output_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(invalid_output_context.completed_records().empty());
}

TEST(QueryUnit, QueryContextCachesLowerFunctionIRAndRecordsTypeCheckDependency)
{
    const QueryContextBodySubject subject = test_body_subject();
    const std::optional<query::QueryKey> lower_key = query::lower_function_ir_query_key(subject.body);
    const std::optional<query::QueryKey> type_check_key = query::type_check_body_query_key(subject.body);
    ASSERT_TRUE(lower_key.has_value());
    ASSERT_TRUE(type_check_key.has_value());

    base::usize lower_provider_calls = 0;
    query::QueryContext context;
    context.set_lower_function_ir_provider(
        [&lower_provider_calls](const query::LowerFunctionIRProviderInput& provider_input) {
            ++lower_provider_calls;
            return query::provide_lower_function_ir_query(provider_input);
        });

    const query::LowerFunctionIRProviderInput input{
        subject.body,
        subject.lowered_ir,
    };
    const query::QueryEvaluationResult first = context.evaluate_lower_function_ir(input);
    ASSERT_EQ(first.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(first.node, nullptr);
    EXPECT_EQ(first.node->key, *lower_key);
    EXPECT_EQ(lower_provider_calls, 1U);
    EXPECT_EQ(context.dependencies_for(*lower_key), std::vector<query::QueryKey>{*type_check_key});

    const query::QueryEvaluationResult cached = context.evaluate_lower_function_ir(input);
    EXPECT_EQ(cached.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(lower_provider_calls, 1U);

    context.set_lower_function_ir_provider({});
    ASSERT_TRUE(context.invalidate(*lower_key));
    const query::QueryEvaluationResult default_lower = context.evaluate_lower_function_ir(input);
    ASSERT_EQ(default_lower.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(default_lower.node, nullptr);
    EXPECT_EQ(default_lower.node->key, *lower_key);
    EXPECT_EQ(lower_provider_calls, 1U);
}

TEST(QueryUnit, QueryContextTracksLowerFunctionIRFailuresAndProviderFallbacks)
{
    const QueryContextBodySubject subject = test_body_subject();
    const QueryContextBodySubject other_subject = test_body_subject("other");
    const std::optional<query::QueryKey> lower_key = query::lower_function_ir_query_key(subject.body);
    ASSERT_TRUE(lower_key.has_value());

    query::QueryContext invalid_input_context;
    const query::QueryEvaluationResult invalid_key_result = invalid_input_context.evaluate_lower_function_ir(
        query::LowerFunctionIRProviderInput{query::BodyKey{}, subject.lowered_ir});
    EXPECT_EQ(invalid_key_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(invalid_key_result.node, nullptr);

    query::QueryContext failing_context;
    failing_context.set_lower_function_ir_provider(
        [](const query::LowerFunctionIRProviderInput&) -> std::optional<query::LowerFunctionIRProviderOutput> {
            return std::nullopt;
        });
    const query::LowerFunctionIRProviderInput input{
        subject.body,
        subject.lowered_ir,
    };
    const query::QueryEvaluationResult failed_result = failing_context.evaluate_lower_function_ir(input);
    ASSERT_EQ(failed_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(failed_result.node, nullptr);
    EXPECT_EQ(failed_result.node->key, *lower_key);
    EXPECT_EQ(failed_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(failing_context.completed_records().empty());

    failing_context.set_lower_function_ir_provider({});
    const query::QueryEvaluationResult retry_result = failing_context.evaluate_lower_function_ir(input);
    ASSERT_EQ(retry_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(retry_result.node, nullptr);
    EXPECT_EQ(retry_result.node->status, query::QueryNodeStatus::done);

    query::QueryContext wrong_key_context;
    wrong_key_context.set_lower_function_ir_provider([&other_subject](const query::LowerFunctionIRProviderInput&) {
        return query::provide_lower_function_ir_query(
            query::LowerFunctionIRProviderInput{other_subject.body, other_subject.lowered_ir});
    });
    const query::QueryEvaluationResult wrong_key_result = wrong_key_context.evaluate_lower_function_ir(input);
    ASSERT_EQ(wrong_key_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(wrong_key_result.node, nullptr);
    EXPECT_EQ(wrong_key_result.node->key, *lower_key);

    query::QueryContext invalid_output_context;
    invalid_output_context.set_lower_function_ir_provider(
        [](const query::LowerFunctionIRProviderInput& provider_input) {
            std::optional<query::LowerFunctionIRProviderOutput> output =
                query::provide_lower_function_ir_query(provider_input);
            if (output) {
                output->dependencies.push_back(query::QueryKey{});
            }
            return output;
        });
    const query::QueryEvaluationResult invalid_output_result = invalid_output_context.evaluate_lower_function_ir(input);
    ASSERT_EQ(invalid_output_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(invalid_output_result.node, nullptr);
    EXPECT_EQ(invalid_output_result.node->key, *lower_key);
    EXPECT_EQ(invalid_output_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(invalid_output_context.completed_records().empty());
}

TEST(QueryUnit, QueryContextCachesGenericBodyAndDiagnosticsDependencies)
{
    const QueryContextGenericInstanceSignatureSubject generic_subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const query::QueryResultFingerprint generic_body =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_GENERIC_INSTANCE_BODY));
    const std::optional<query::QueryKey> signature_key =
        query::generic_instance_signature_query_key(generic_subject.key);
    const std::optional<query::QueryKey> body_key = query::generic_instance_body_query_key(generic_subject.key);
    ASSERT_TRUE(signature_key.has_value());
    ASSERT_TRUE(body_key.has_value());

    base::usize generic_body_provider_calls = 0;
    base::usize diagnostics_provider_calls = 0;
    query::QueryContext context;
    context.set_generic_instance_body_provider(
        [&generic_body_provider_calls](const query::GenericInstanceBodyProviderInput& provider_input) {
            ++generic_body_provider_calls;
            return query::provide_generic_instance_body_query(provider_input);
        });
    context.set_diagnostics_provider(
        [&diagnostics_provider_calls](const query::DiagnosticsProviderInput& provider_input) {
            ++diagnostics_provider_calls;
            return query::provide_diagnostics_query(provider_input);
        });

    const query::QueryEvaluationResult body_result =
        context.evaluate_generic_instance_body(generic_instance_body_provider_input(generic_subject, generic_body));
    ASSERT_EQ(body_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(body_result.node, nullptr);
    EXPECT_EQ(body_result.node->key, *body_key);
    EXPECT_EQ(generic_body_provider_calls, 1U);
    EXPECT_EQ(context.dependencies_for(*body_key), std::vector<query::QueryKey>{*signature_key});

    const query::QueryResultFingerprint diagnostics =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_DIAGNOSTICS));
    const std::optional<query::QueryKey> diagnostics_key = query::diagnostics_query_key(*body_key);
    ASSERT_TRUE(diagnostics_key.has_value());
    const query::DiagnosticsProviderInput diagnostics_input{
        *body_key,
        diagnostics,
    };
    const query::QueryEvaluationResult diagnostics_result = context.evaluate_diagnostics(diagnostics_input);
    ASSERT_EQ(diagnostics_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(diagnostics_result.node, nullptr);
    EXPECT_EQ(diagnostics_result.node->key, *diagnostics_key);
    EXPECT_EQ(diagnostics_provider_calls, 1U);
    EXPECT_EQ(context.dependencies_for(*diagnostics_key), std::vector<query::QueryKey>{*body_key});
    EXPECT_TRUE(context.has_dependency(*diagnostics_key, *body_key));

    const query::QueryEvaluationResult cached_body =
        context.evaluate_generic_instance_body(generic_instance_body_provider_input(generic_subject, generic_body));
    EXPECT_EQ(cached_body.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(generic_body_provider_calls, 1U);

    const query::QueryEvaluationResult cached_diagnostics = context.evaluate_diagnostics(diagnostics_input);
    EXPECT_EQ(cached_diagnostics.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(diagnostics_provider_calls, 1U);

    context.set_generic_instance_body_provider({});
    ASSERT_TRUE(context.invalidate(*body_key));
    const query::QueryEvaluationResult default_body =
        context.evaluate_generic_instance_body(generic_instance_body_provider_input(generic_subject, generic_body));
    ASSERT_EQ(default_body.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(default_body.node, nullptr);
    EXPECT_EQ(default_body.node->key, *body_key);
    EXPECT_EQ(generic_body_provider_calls, 1U);

    context.set_diagnostics_provider({});
    ASSERT_TRUE(context.invalidate(*diagnostics_key));
    const query::QueryEvaluationResult default_diagnostics = context.evaluate_diagnostics(diagnostics_input);
    ASSERT_EQ(default_diagnostics.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(default_diagnostics.node, nullptr);
    EXPECT_EQ(default_diagnostics.node->key, *diagnostics_key);
    EXPECT_EQ(diagnostics_provider_calls, 1U);
}

TEST(QueryUnit, QueryContextTracksGenericBodyFailuresAndProviderFallbacks)
{
    const QueryContextGenericInstanceSignatureSubject subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const QueryContextGenericInstanceSignatureSubject other_subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i64, QUERY_TEST_PROVIDER_SIGNATURE);
    const query::QueryResultFingerprint generic_body =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_GENERIC_INSTANCE_BODY));
    const query::GenericInstanceBodyProviderInput generic_body_input =
        generic_instance_body_provider_input(subject, generic_body);
    const std::optional<query::QueryKey> expected_key = query::generic_instance_body_query_key(subject.key);
    ASSERT_TRUE(expected_key.has_value());

    query::QueryContext invalid_input_context;
    const query::QueryEvaluationResult null_key_result = invalid_input_context.evaluate_generic_instance_body(
        query::GenericInstanceBodyProviderInput{nullptr, generic_body_input.authority});
    EXPECT_EQ(null_key_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(null_key_result.node, nullptr);
    const query::GenericInstanceKey invalid_key;
    const query::QueryEvaluationResult invalid_key_result = invalid_input_context.evaluate_generic_instance_body(
        query::GenericInstanceBodyProviderInput{&invalid_key, generic_body_input.authority});
    EXPECT_EQ(invalid_key_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(invalid_key_result.node, nullptr);

    query::QueryContext failing_context;
    failing_context.set_generic_instance_body_provider(
        [](const query::GenericInstanceBodyProviderInput&) -> std::optional<query::GenericInstanceBodyProviderOutput> {
            return std::nullopt;
        });
    const query::QueryEvaluationResult failed_result =
        failing_context.evaluate_generic_instance_body(generic_instance_body_provider_input(subject, generic_body));
    ASSERT_EQ(failed_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(failed_result.node, nullptr);
    EXPECT_EQ(failed_result.node->key, *expected_key);
    EXPECT_EQ(failed_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(failing_context.completed_records().empty());

    failing_context.set_generic_instance_body_provider({});
    const query::QueryEvaluationResult retry_result =
        failing_context.evaluate_generic_instance_body(generic_instance_body_provider_input(subject, generic_body));
    ASSERT_EQ(retry_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(retry_result.node, nullptr);
    EXPECT_EQ(retry_result.node->status, query::QueryNodeStatus::done);

    query::QueryContext wrong_key_context;
    wrong_key_context.set_generic_instance_body_provider(
        [&other_subject, generic_body](const query::GenericInstanceBodyProviderInput&) {
            return query::provide_generic_instance_body_query(
                generic_instance_body_provider_input(other_subject, generic_body));
        });
    const query::QueryEvaluationResult wrong_key_result =
        wrong_key_context.evaluate_generic_instance_body(generic_instance_body_provider_input(subject, generic_body));
    ASSERT_EQ(wrong_key_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(wrong_key_result.node, nullptr);
    EXPECT_EQ(wrong_key_result.node->key, *expected_key);

    query::QueryContext invalid_output_context;
    invalid_output_context.set_generic_instance_body_provider(
        [](const query::GenericInstanceBodyProviderInput& provider_input) {
            std::optional<query::GenericInstanceBodyProviderOutput> output =
                query::provide_generic_instance_body_query(provider_input);
            if (output) {
                output->dependencies.push_back(query::QueryKey{});
            }
            return output;
        });
    const query::QueryEvaluationResult invalid_output_result = invalid_output_context.evaluate_generic_instance_body(
        generic_instance_body_provider_input(subject, generic_body));
    ASSERT_EQ(invalid_output_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(invalid_output_result.node, nullptr);
    EXPECT_EQ(invalid_output_result.node->key, *expected_key);
    EXPECT_EQ(invalid_output_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(invalid_output_context.completed_records().empty());
}

TEST(QueryUnit, QueryContextCachesLowerGenericInstanceIRAndRecordsGenericBodyDependency)
{
    const QueryContextGenericInstanceSignatureSubject subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const query::QueryResultFingerprint lowered_ir =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_LOWER_FUNCTION_IR));
    const std::optional<query::QueryKey> lower_key = query::lower_generic_instance_ir_query_key(subject.key);
    const std::optional<query::QueryKey> generic_body_key = query::generic_instance_body_query_key(subject.key);
    ASSERT_TRUE(lower_key.has_value());
    ASSERT_TRUE(generic_body_key.has_value());

    base::usize lower_provider_calls = 0;
    query::QueryContext context;
    context.set_lower_generic_instance_ir_provider(
        [&lower_provider_calls](const query::LowerGenericInstanceIRProviderInput& provider_input) {
            ++lower_provider_calls;
            return query::provide_lower_generic_instance_ir_query(provider_input);
        });

    const query::LowerGenericInstanceIRProviderInput input{
        &subject.key,
        lowered_ir,
    };
    const query::QueryEvaluationResult first = context.evaluate_lower_generic_instance_ir(input);
    ASSERT_EQ(first.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(first.node, nullptr);
    EXPECT_EQ(first.node->key, *lower_key);
    EXPECT_EQ(lower_provider_calls, 1U);
    EXPECT_EQ(context.dependencies_for(*lower_key), std::vector<query::QueryKey>{*generic_body_key});

    const query::QueryEvaluationResult cached = context.evaluate_lower_generic_instance_ir(input);
    EXPECT_EQ(cached.status, query::QueryEvaluationStatus::cached);
    EXPECT_EQ(lower_provider_calls, 1U);

    context.set_lower_generic_instance_ir_provider({});
    ASSERT_TRUE(context.invalidate(*lower_key));
    const query::QueryEvaluationResult default_lower = context.evaluate_lower_generic_instance_ir(input);
    ASSERT_EQ(default_lower.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(default_lower.node, nullptr);
    EXPECT_EQ(default_lower.node->key, *lower_key);
    EXPECT_EQ(lower_provider_calls, 1U);
}

TEST(QueryUnit, QueryContextTracksLowerGenericInstanceIRFailuresAndProviderFallbacks)
{
    const QueryContextGenericInstanceSignatureSubject subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const QueryContextGenericInstanceSignatureSubject other_subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i64, QUERY_TEST_PROVIDER_SIGNATURE);
    const query::QueryResultFingerprint lowered_ir =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_LOWER_FUNCTION_IR));
    const std::optional<query::QueryKey> lower_key = query::lower_generic_instance_ir_query_key(subject.key);
    ASSERT_TRUE(lower_key.has_value());

    query::QueryContext invalid_input_context;
    const query::QueryEvaluationResult null_key_result = invalid_input_context.evaluate_lower_generic_instance_ir(
        query::LowerGenericInstanceIRProviderInput{nullptr, lowered_ir});
    EXPECT_EQ(null_key_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(null_key_result.node, nullptr);
    const query::GenericInstanceKey invalid_key;
    const query::QueryEvaluationResult invalid_key_result = invalid_input_context.evaluate_lower_generic_instance_ir(
        query::LowerGenericInstanceIRProviderInput{&invalid_key, lowered_ir});
    EXPECT_EQ(invalid_key_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(invalid_key_result.node, nullptr);

    query::QueryContext failing_context;
    failing_context.set_lower_generic_instance_ir_provider(
        [](const query::LowerGenericInstanceIRProviderInput&)
            -> std::optional<query::LowerGenericInstanceIRProviderOutput> {
            return std::nullopt;
        });
    const query::LowerGenericInstanceIRProviderInput input{
        &subject.key,
        lowered_ir,
    };
    const query::QueryEvaluationResult failed_result = failing_context.evaluate_lower_generic_instance_ir(input);
    ASSERT_EQ(failed_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(failed_result.node, nullptr);
    EXPECT_EQ(failed_result.node->key, *lower_key);
    EXPECT_EQ(failed_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(failing_context.completed_records().empty());

    failing_context.set_lower_generic_instance_ir_provider({});
    const query::QueryEvaluationResult retry_result = failing_context.evaluate_lower_generic_instance_ir(input);
    ASSERT_EQ(retry_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(retry_result.node, nullptr);
    EXPECT_EQ(retry_result.node->status, query::QueryNodeStatus::done);

    query::QueryContext wrong_key_context;
    wrong_key_context.set_lower_generic_instance_ir_provider(
        [&other_subject, lowered_ir](const query::LowerGenericInstanceIRProviderInput&) {
            return query::provide_lower_generic_instance_ir_query(query::LowerGenericInstanceIRProviderInput{
                &other_subject.key,
                lowered_ir,
            });
        });
    const query::QueryEvaluationResult wrong_key_result = wrong_key_context.evaluate_lower_generic_instance_ir(input);
    ASSERT_EQ(wrong_key_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(wrong_key_result.node, nullptr);
    EXPECT_EQ(wrong_key_result.node->key, *lower_key);

    query::QueryContext invalid_output_context;
    invalid_output_context.set_lower_generic_instance_ir_provider(
        [](const query::LowerGenericInstanceIRProviderInput& provider_input) {
            std::optional<query::LowerGenericInstanceIRProviderOutput> output =
                query::provide_lower_generic_instance_ir_query(provider_input);
            if (output) {
                output->dependencies.push_back(query::QueryKey{});
            }
            return output;
        });
    const query::QueryEvaluationResult invalid_output_result =
        invalid_output_context.evaluate_lower_generic_instance_ir(input);
    ASSERT_EQ(invalid_output_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(invalid_output_result.node, nullptr);
    EXPECT_EQ(invalid_output_result.node->key, *lower_key);
    EXPECT_EQ(invalid_output_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(invalid_output_context.completed_records().empty());
}

TEST(QueryUnit, QueryContextTracksDiagnosticsFailuresAndProviderFallbacks)
{
    const QueryContextGenericInstanceSignatureSubject subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const QueryContextGenericInstanceSignatureSubject other_subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i64, QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::QueryKey> producer = query::generic_instance_body_query_key(subject.key);
    const std::optional<query::QueryKey> other_producer = query::generic_instance_body_query_key(other_subject.key);
    ASSERT_TRUE(producer.has_value());
    ASSERT_TRUE(other_producer.has_value());
    const query::QueryKey other_producer_key = *other_producer;
    const query::QueryResultFingerprint diagnostics =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_DIAGNOSTICS));
    const std::optional<query::QueryKey> expected_key = query::diagnostics_query_key(*producer);
    ASSERT_TRUE(expected_key.has_value());

    query::QueryContext invalid_input_context;
    const query::QueryEvaluationResult invalid_producer_result =
        invalid_input_context.evaluate_diagnostics(query::DiagnosticsProviderInput{query::QueryKey{}, diagnostics});
    EXPECT_EQ(invalid_producer_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(invalid_producer_result.node, nullptr);
    const query::QueryEvaluationResult diagnostics_producer_result =
        invalid_input_context.evaluate_diagnostics(query::DiagnosticsProviderInput{*expected_key, diagnostics});
    EXPECT_EQ(diagnostics_producer_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(diagnostics_producer_result.node, nullptr);

    query::QueryContext failing_context;
    failing_context.set_diagnostics_provider(
        [](const query::DiagnosticsProviderInput&) -> std::optional<query::DiagnosticsProviderOutput> {
            return std::nullopt;
        });
    const query::DiagnosticsProviderInput diagnostics_input{
        *producer,
        diagnostics,
    };
    const query::QueryEvaluationResult failed_result = failing_context.evaluate_diagnostics(diagnostics_input);
    ASSERT_EQ(failed_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(failed_result.node, nullptr);
    EXPECT_EQ(failed_result.node->key, *expected_key);
    EXPECT_EQ(failed_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(failing_context.completed_records().empty());

    failing_context.set_diagnostics_provider({});
    const query::QueryEvaluationResult retry_result = failing_context.evaluate_diagnostics(diagnostics_input);
    ASSERT_EQ(retry_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(retry_result.node, nullptr);
    EXPECT_EQ(retry_result.node->status, query::QueryNodeStatus::done);

    query::QueryContext wrong_key_context;
    wrong_key_context.set_diagnostics_provider(
        [other_producer_key, diagnostics](const query::DiagnosticsProviderInput&) {
            return query::provide_diagnostics_query(query::DiagnosticsProviderInput{
                other_producer_key,
                diagnostics,
            });
        });
    const query::QueryEvaluationResult wrong_key_result = wrong_key_context.evaluate_diagnostics(diagnostics_input);
    ASSERT_EQ(wrong_key_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(wrong_key_result.node, nullptr);
    EXPECT_EQ(wrong_key_result.node->key, *expected_key);

    query::QueryContext invalid_output_context;
    invalid_output_context.set_diagnostics_provider([](const query::DiagnosticsProviderInput& provider_input) {
        std::optional<query::DiagnosticsProviderOutput> output = query::provide_diagnostics_query(provider_input);
        if (output) {
            output->dependencies.push_back(query::QueryKey{});
        }
        return output;
    });
    const query::QueryEvaluationResult invalid_output_result =
        invalid_output_context.evaluate_diagnostics(diagnostics_input);
    ASSERT_EQ(invalid_output_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(invalid_output_result.node, nullptr);
    EXPECT_EQ(invalid_output_result.node->key, *expected_key);
    EXPECT_EQ(invalid_output_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(invalid_output_context.completed_records().empty());
}

TEST(QueryUnit, QueryContextBuildsDeterministicDependencyEdgeTable)
{
    const QueryContextBodySubject first_subject = test_body_subject("compute");
    const QueryContextBodySubject second_subject = test_body_subject("other");
    const QueryContextGenericInstanceSignatureSubject generic_subject =
        test_generic_instance_signature_subject("Vec", query::BuiltinTypeKey::i32, QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::QueryKey> first_key = query::lower_function_ir_query_key(first_subject.body);
    const std::optional<query::QueryKey> second_key = query::lower_function_ir_query_key(second_subject.body);
    const std::optional<query::QueryKey> shared_dependency = query::type_check_body_query_key(first_subject.body);
    const std::optional<query::QueryKey> first_only_dependency =
        query::generic_instance_body_query_key(generic_subject.key);
    ASSERT_TRUE(first_key.has_value());
    ASSERT_TRUE(second_key.has_value());
    ASSERT_TRUE(shared_dependency.has_value());
    ASSERT_TRUE(first_only_dependency.has_value());

    query::QueryContext context;
    context.set_lower_function_ir_provider(
        [&first_subject, shared_dependency = *shared_dependency, first_only_dependency = *first_only_dependency](
            const query::LowerFunctionIRProviderInput& provider_input) {
            std::optional<query::LowerFunctionIRProviderOutput> output =
                query::provide_lower_function_ir_query(provider_input);
            if (output && provider_input.key == first_subject.body) {
                output->dependencies = {
                    first_only_dependency,
                    shared_dependency,
                    shared_dependency,
                };
            } else if (output) {
                output->dependencies = {
                    shared_dependency,
                };
            }
            return output;
        });

    const query::QueryEvaluationResult first_result = context.evaluate_lower_function_ir(
        query::LowerFunctionIRProviderInput{first_subject.body, first_subject.lowered_ir});
    ASSERT_EQ(first_result.status, query::QueryEvaluationStatus::computed);
    const query::QueryEvaluationResult second_result = context.evaluate_lower_function_ir(
        query::LowerFunctionIRProviderInput{second_subject.body, second_subject.lowered_ir});
    ASSERT_EQ(second_result.status, query::QueryEvaluationStatus::computed);

    std::vector<query::QueryKey> expected_first_dependencies{
        *first_only_dependency,
        *shared_dependency,
    };
    sort_query_test_keys(expected_first_dependencies);
    EXPECT_EQ(context.dependencies_for(*first_key), expected_first_dependencies);
    ASSERT_NE(first_result.node, nullptr);
    EXPECT_EQ(first_result.node->dependencies, expected_first_dependencies);

    const std::vector<query::QueryKey> expected_second_dependencies{
        *shared_dependency,
    };
    EXPECT_EQ(context.dependencies_for(*second_key), expected_second_dependencies);
    ASSERT_NE(second_result.node, nullptr);
    EXPECT_EQ(second_result.node->dependencies, expected_second_dependencies);

    std::vector<query::QueryKey> expected_shared_dependents{
        *first_key,
        *second_key,
    };
    sort_query_test_keys(expected_shared_dependents);
    EXPECT_EQ(context.dependents_of(*shared_dependency), expected_shared_dependents);
    EXPECT_EQ(context.dependents_of(*first_only_dependency), std::vector<query::QueryKey>{*first_key});
    EXPECT_TRUE(context.dependents_of(query::QueryKey{}).empty());

    EXPECT_TRUE(context.has_dependency(*first_key, *shared_dependency));
    EXPECT_TRUE(context.has_dependency(*first_key, *first_only_dependency));
    EXPECT_TRUE(context.has_dependency(*second_key, *shared_dependency));
    EXPECT_FALSE(context.has_dependency(*second_key, *first_only_dependency));
    EXPECT_FALSE(context.has_dependency(query::QueryKey{}, *shared_dependency));
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
    EXPECT_TRUE(contains_query_dependency_edge(edges, query::QueryDependencyEdge{*first_key, *shared_dependency}));
    EXPECT_TRUE(contains_query_dependency_edge(edges, query::QueryDependencyEdge{*first_key, *first_only_dependency}));
    EXPECT_TRUE(contains_query_dependency_edge(edges, query::QueryDependencyEdge{*second_key, *shared_dependency}));
    EXPECT_TRUE(context.dependencies_for(query::QueryKey{}).empty());
}

TEST(QueryUnit, QueryContextTracksModuleExportsFailuresAndCycles)
{
    const query::ModuleKey module = test_module(test_package());
    const query::QueryResultFingerprint exports =
        query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_MODULE_EXPORTS_SIGNATURE));
    const query::ModuleExportsProviderInput input{
        module,
        exports,
    };
    const std::optional<query::QueryKey> expected_key = query::module_exports_query_key(module);
    const std::optional<query::QueryKey> item_list_dependency = query::item_list_query_key(module);
    ASSERT_TRUE(expected_key.has_value());
    ASSERT_TRUE(item_list_dependency.has_value());

    query::QueryContext dependency_context;
    dependency_context.set_module_exports_provider([dependency = *item_list_dependency](
                                                       const query::ModuleExportsProviderInput& provider_input) {
        std::optional<query::ModuleExportsProviderOutput> output = query::provide_module_exports_query(provider_input);
        if (output) {
            output->dependencies.push_back(dependency);
        }
        return output;
    });
    const query::QueryEvaluationResult dependency_result = dependency_context.evaluate_module_exports(input);
    ASSERT_EQ(dependency_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(dependency_result.node, nullptr);
    std::vector<query::QueryKey> expected_dependencies{
        *item_list_dependency,
    };
    sort_query_test_keys(expected_dependencies);
    EXPECT_EQ(dependency_result.node->dependencies, expected_dependencies);

    const query::QueryEvaluationResult invalid_key_result =
        dependency_context.evaluate_module_exports(query::ModuleExportsProviderInput{query::ModuleKey{}, exports});
    EXPECT_EQ(invalid_key_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(invalid_key_result.node, nullptr);

    query::QueryContext failing_context;
    failing_context.set_module_exports_provider(
        [](const query::ModuleExportsProviderInput&) -> std::optional<query::ModuleExportsProviderOutput> {
            return std::nullopt;
        });
    const query::QueryEvaluationResult failed_result = failing_context.evaluate_module_exports(input);
    ASSERT_EQ(failed_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(failed_result.node, nullptr);
    EXPECT_EQ(failed_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(failing_context.completed_records().empty());

    failing_context.set_module_exports_provider({});
    const query::QueryEvaluationResult retry_result = failing_context.evaluate_module_exports(input);
    ASSERT_EQ(retry_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(retry_result.node, nullptr);
    EXPECT_EQ(retry_result.node->status, query::QueryNodeStatus::done);

    const std::array<std::string_view, 1> other_module_path{"other"};
    const query::ModuleKey other_module = query::module_key(test_package(), other_module_path);
    query::QueryContext wrong_key_context;
    wrong_key_context.set_module_exports_provider([other_module, exports](const query::ModuleExportsProviderInput&) {
        return query::provide_module_exports_query(query::ModuleExportsProviderInput{
            other_module,
            exports,
        });
    });
    const query::QueryEvaluationResult wrong_key_result = wrong_key_context.evaluate_module_exports(input);
    ASSERT_EQ(wrong_key_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(wrong_key_result.node, nullptr);
    EXPECT_EQ(wrong_key_result.node->key, *expected_key);
    EXPECT_EQ(wrong_key_result.node->status, query::QueryNodeStatus::failed);

    query::QueryContext invalid_output_context;
    invalid_output_context.set_module_exports_provider([](const query::ModuleExportsProviderInput& provider_input) {
        std::optional<query::ModuleExportsProviderOutput> output = query::provide_module_exports_query(provider_input);
        if (output) {
            output->dependencies.push_back(query::QueryKey{});
        }
        return output;
    });
    const query::QueryEvaluationResult invalid_output_result = invalid_output_context.evaluate_module_exports(input);
    ASSERT_EQ(invalid_output_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(invalid_output_result.node, nullptr);
    EXPECT_EQ(invalid_output_result.node->key, *expected_key);
    EXPECT_EQ(invalid_output_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(invalid_output_context.completed_records().empty());

    query::QueryContext cyclic_context;
    query::QueryEvaluationResult nested_result;
    cyclic_context.set_module_exports_provider(
        [&cyclic_context, &nested_result](const query::ModuleExportsProviderInput& provider_input) {
            nested_result = cyclic_context.evaluate_module_exports(provider_input);
            return query::provide_module_exports_query(provider_input);
        });
    const query::QueryEvaluationResult cyclic_result = cyclic_context.evaluate_module_exports(input);
    EXPECT_EQ(nested_result.status, query::QueryEvaluationStatus::cycle);
    ASSERT_EQ(cyclic_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(cyclic_result.node, nullptr);
    EXPECT_EQ(cyclic_result.node->status, query::QueryNodeStatus::done);
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
    EXPECT_EQ(context.interned_query_count(), 0U);
    EXPECT_EQ(context.bound_stable_identity_count(), 0U);

    EXPECT_FALSE(context.seed_completed_record(query::QueryRecord{}));
    EXPECT_FALSE(context.seed_completed_record(output->record, {query::QueryKey{}}));
    const query::QueryKey wrong_kind_dependency =
        query::query_key(query::QueryKind::item_list, query::stable_key_fingerprint(subject.def.module));
    EXPECT_FALSE(context.seed_completed_record(output->record, {wrong_kind_dependency}));
    query::QueryRecord wrong_shape_record = output->record;
    wrong_shape_record.key =
        query::query_key(query::QueryKind::item_signature, query::stable_key_fingerprint(subject.def.module));
    wrong_shape_record.stable_key_bytes = query::stable_serialize(subject.def.module);
    EXPECT_FALSE(context.seed_completed_record(wrong_shape_record, {dependency}));
    EXPECT_EQ(context.find(wrong_shape_record.key), nullptr);
    EXPECT_EQ(context.interned_query_count(), 0U);
    EXPECT_EQ(context.bound_stable_identity_count(), 0U);
    EXPECT_TRUE(context.seed_completed_record(output->record, {dependency, dependency}));
    EXPECT_FALSE(context.seed_completed_record(output->record));
    EXPECT_EQ(context.interned_query_count(), 2U);
    EXPECT_EQ(context.bound_stable_identity_count(), 1U);
    const std::optional<query::QueryNodeId> seeded_node_id = context.node_id_for(*expected_key);
    ASSERT_TRUE(seeded_node_id.has_value());
    EXPECT_TRUE(query::is_valid(*seeded_node_id));
    EXPECT_EQ(context.find(*seeded_node_id), context.find(*expected_key));
    EXPECT_TRUE(context.node_id_for(dependency).has_value());
    EXPECT_EQ(context.dependency_edge_count(), 1U);
    EXPECT_EQ(context.dependencies_for(*expected_key), std::vector<query::QueryKey>{dependency});
    EXPECT_TRUE(context.has_dependency(*expected_key, dependency));
    const query::QueryNode* const seeded_node = context.find(*expected_key);
    ASSERT_NE(seeded_node, nullptr);
    EXPECT_EQ(seeded_node->verified_revision, context.current_revision());
    EXPECT_EQ(seeded_node->changed_revision, query::QUERY_REVISION_INVALID);
    EXPECT_EQ(seeded_node->reuse_state, query::QueryReuseState::green);

    EXPECT_EQ(context.advance_revision(), query::QUERY_REVISION_INITIAL + 1U);
    const query::QueryEvaluationResult cached_result = context.evaluate_item_signature(subject.input);
    EXPECT_EQ(cached_result.status, query::QueryEvaluationStatus::cached);
    ASSERT_NE(cached_result.node, nullptr);
    EXPECT_EQ(cached_result.node->id, *seeded_node_id);
    EXPECT_EQ(cached_result.node->record.key, output->record.key);
    EXPECT_EQ(cached_result.node->record.result, output->record.result);
    EXPECT_EQ(cached_result.node->record.stable_key_bytes, output->record.stable_key_bytes);
    EXPECT_EQ(cached_result.node->verified_revision, context.current_revision());
    EXPECT_EQ(cached_result.node->changed_revision, query::QUERY_REVISION_INVALID);
    EXPECT_EQ(cached_result.node->reuse_state, query::QueryReuseState::green);
    EXPECT_EQ(provider_calls, 0U);

    EXPECT_TRUE(context.invalidate(*expected_key));
    EXPECT_FALSE(context.invalidate(*expected_key));
    EXPECT_FALSE(context.invalidate(query::QueryKey{}));
    EXPECT_EQ(context.dependency_edge_count(), 0U);
    EXPECT_TRUE(context.dependencies_for(*expected_key).empty());
    EXPECT_TRUE(context.dependents_of(dependency).empty());
    EXPECT_FALSE(context.has_dependency(*expected_key, dependency));
    EXPECT_EQ(context.interned_query_count(), 2U);
    EXPECT_EQ(context.bound_stable_identity_count(), 1U);
    const query::QueryNode* const invalidated_node = context.find(*expected_key);
    ASSERT_NE(invalidated_node, nullptr);
    EXPECT_EQ(invalidated_node->verified_revision, context.current_revision());
    EXPECT_EQ(invalidated_node->changed_revision, context.current_revision());
    EXPECT_EQ(invalidated_node->reuse_state, query::QueryReuseState::red);

    const query::QueryEvaluationResult recomputed_result = context.evaluate_item_signature(subject.input);
    EXPECT_EQ(recomputed_result.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(recomputed_result.node, nullptr);
    EXPECT_EQ(recomputed_result.node->verified_revision, context.current_revision());
    EXPECT_EQ(recomputed_result.node->changed_revision, context.current_revision());
    EXPECT_EQ(recomputed_result.node->reuse_state, query::QueryReuseState::red);
    EXPECT_EQ(provider_calls, 1U);
    EXPECT_EQ(context.node_id_for(*expected_key), seeded_node_id);
    EXPECT_EQ(context.interned_query_count(), 2U);
    EXPECT_EQ(context.bound_stable_identity_count(), 1U);
}

TEST(QueryUnit, QueryContextRejectsProviderRecordsWithMalformedStableIdentity)
{
    const QueryContextItemSignatureSubject subject =
        test_item_signature_subject("compute", QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::QueryKey> expected_key = query::item_signature_query_key(subject.def);
    ASSERT_TRUE(expected_key.has_value());

    query::QueryContext context([](const query::ItemSignatureProviderInput& provider_input) {
        std::optional<query::ItemSignatureProviderOutput> output = query::provide_item_signature_query(provider_input);
        if (output) {
            output->record.stable_key_bytes = query::stable_serialize(provider_input.key.module);
        }
        return output;
    });

    const query::QueryEvaluationResult result = context.evaluate_item_signature(subject.input);
    ASSERT_EQ(result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(result.node, nullptr);
    EXPECT_EQ(result.node->key, *expected_key);
    EXPECT_EQ(result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(context.completed_records().empty());
    EXPECT_EQ(context.dependency_edge_count(), 0U);
    EXPECT_EQ(context.interned_query_count(), 1U);
    EXPECT_EQ(context.bound_stable_identity_count(), 0U);
    EXPECT_TRUE(context.node_id_for(*expected_key).has_value());
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
    changed_record.result = query::query_result_fingerprint(query::stable_incremental_key(
        subject.input.authority.signature.definition, QUERY_TEST_PROVIDER_MISMATCHED_SIGNATURE));
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

TEST(QueryUnit, QueryReusePlanCutsPropagationAtUnchangedDependents)
{
    const QueryContextItemSignatureSubject subject =
        test_item_signature_subject("compute", QUERY_TEST_PROVIDER_SIGNATURE);
    const std::optional<query::ItemSignatureProviderOutput> item_output =
        query::provide_item_signature_query(subject.input);
    ASSERT_TRUE(item_output.has_value());

    const query::QueryResultFingerprint cached_exports_result =
        query::query_result_fingerprint(query::stable_incremental_key(
            subject.input.authority.signature.definition, QUERY_TEST_MODULE_EXPORTS_SIGNATURE));
    const query::QueryResultFingerprint changed_exports_result =
        query::query_result_fingerprint(query::stable_incremental_key(
            subject.input.authority.signature.definition, QUERY_TEST_CHANGED_MODULE_EXPORTS_SIGNATURE));
    const std::optional<query::QueryRecord> cached_exports_record =
        query::query_record(query::QueryKind::module_exports, query::stable_key_fingerprint(subject.def.module),
            query::stable_serialize(subject.def.module), cached_exports_result);
    const std::optional<query::QueryRecord> changed_exports_record =
        query::query_record(query::QueryKind::module_exports, query::stable_key_fingerprint(subject.def.module),
            query::stable_serialize(subject.def.module), changed_exports_result);
    ASSERT_TRUE(cached_exports_record.has_value());
    ASSERT_TRUE(changed_exports_record.has_value());
    ASSERT_NE(cached_exports_record->result, changed_exports_record->result);

    query::QueryContext cached_context;
    ASSERT_TRUE(cached_context.seed_completed_record(*cached_exports_record));
    ASSERT_TRUE(cached_context.seed_completed_record(item_output->record, {cached_exports_record->key}));

    const std::vector<query::QueryRecord> current_records{
        *changed_exports_record,
        item_output->record,
        query::QueryRecord{},
    };
    const query::QueryReusePlan plan = query::build_query_reuse_plan(cached_context, current_records);
    ASSERT_EQ(plan.decisions.size(), current_records.size());
    EXPECT_EQ(plan.decisions[0].change_status, query::QueryRecordChangeStatus::changed);
    EXPECT_EQ(plan.decisions[1].change_status, query::QueryRecordChangeStatus::unchanged);
    EXPECT_EQ(plan.decisions[2].change_status, query::QueryRecordChangeStatus::malformed);

    EXPECT_EQ(plan.summary.total, 3U);
    EXPECT_EQ(plan.summary.changed, 1U);
    EXPECT_EQ(plan.summary.unchanged, 1U);
    EXPECT_EQ(plan.summary.malformed, 1U);
    EXPECT_EQ(plan.summary.reusable, 1U);
    EXPECT_EQ(plan.summary.recompute, 2U);

    EXPECT_EQ(plan.reusable, std::vector<query::QueryKey>{item_output->record.key});
    EXPECT_EQ(plan.recompute_roots, std::vector<query::QueryKey>{changed_exports_record->key});
    EXPECT_TRUE(plan.propagated_recompute.empty());

    std::vector<query::QueryKey> expected_recompute{
        changed_exports_record->key,
    };
    sort_query_test_keys(expected_recompute);
    EXPECT_EQ(plan.recompute, expected_recompute);

    const query::QueryReusePlan missing_plan =
        query::mark_all_queries_recompute(std::span<const query::QueryRecord>{current_records.data(), 2U});
    ASSERT_EQ(missing_plan.decisions.size(), 2U);
    EXPECT_EQ(missing_plan.summary.missing, 2U);
    EXPECT_EQ(missing_plan.summary.recompute, 2U);
    EXPECT_EQ(missing_plan.recompute_roots.size(), 2U);
    EXPECT_TRUE(missing_plan.reusable.empty());
    EXPECT_TRUE(missing_plan.propagated_recompute.empty());
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
                output->dependencies = {
                    dependency,
                };
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
        query::GenericInstanceSignatureProviderInput{nullptr, subject.authority});
    EXPECT_EQ(null_key_result.status, query::QueryEvaluationStatus::failed);
    EXPECT_EQ(null_key_result.node, nullptr);

    const query::GenericInstanceKey invalid_key;
    const query::QueryEvaluationResult invalid_key_result = dependency_context.evaluate_generic_instance_signature(
        query::GenericInstanceSignatureProviderInput{&invalid_key, subject.authority});
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
        query::ItemSignatureProviderInput{query::DefKey{}, subject.input.authority});
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

    const query::QueryKey unexpected_kind_dependency =
        query::query_key(query::QueryKind::item_list, query::stable_key_fingerprint(subject.def.module));
    query::QueryContext unexpected_kind_context([unexpected_kind_dependency](
                                                    const query::ItemSignatureProviderInput& provider_input) {
        std::optional<query::ItemSignatureProviderOutput> output = query::provide_item_signature_query(provider_input);
        if (output) {
            output->dependencies = {
                unexpected_kind_dependency,
            };
        }
        return output;
    });
    const query::QueryEvaluationResult unexpected_kind_result =
        unexpected_kind_context.evaluate_item_signature(subject.input);
    ASSERT_EQ(unexpected_kind_result.status, query::QueryEvaluationStatus::failed);
    ASSERT_NE(unexpected_kind_result.node, nullptr);
    EXPECT_EQ(unexpected_kind_result.node->key, *expected_key);
    EXPECT_EQ(unexpected_kind_result.node->status, query::QueryNodeStatus::failed);
    EXPECT_TRUE(unexpected_kind_context.completed_records().empty());

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
