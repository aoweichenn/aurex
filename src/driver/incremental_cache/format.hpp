#pragma once

#include <aurex/base/integer.hpp>
#include <aurex/query/query_context.hpp>
#include <aurex/query/query_result.hpp>
#include <aurex/sema/identifier.hpp>

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::driver::incremental_cache_format {

constexpr std::string_view INCREMENTAL_CACHE_MAGIC = "aurex-incremental-cache-v1";
constexpr base::u64 INCREMENTAL_CACHE_SCHEMA_VERSION = 1;
constexpr std::string_view INCREMENTAL_CACHE_MODE_SEMANTIC_OK = "semantic-ok";
constexpr char INCREMENTAL_CACHE_SEPARATOR = '\t';
constexpr char INCREMENTAL_CACHE_HEX_DIGIT_PREFIX = '0';
constexpr char INCREMENTAL_CACHE_HEX_ALPHA_PREFIX = 'a';
constexpr int INCREMENTAL_CACHE_DECIMAL_BASE = 10;
constexpr int INCREMENTAL_CACHE_HEX_LETTER_OFFSET = 10;
constexpr int INCREMENTAL_CACHE_HEX_BYTE_SHIFT = 4;
constexpr int INCREMENTAL_CACHE_INVALID_HEX_NIBBLE = -1;
constexpr unsigned char INCREMENTAL_CACHE_HEX_LOW_MASK = 0x0fU;
constexpr base::usize INCREMENTAL_CACHE_HEX_BYTE_WIDTH = 2;

constexpr base::usize INCREMENTAL_CACHE_KIND_FIELD = 0;
constexpr base::usize INCREMENTAL_CACHE_FIRST_VALUE_FIELD = 1;
constexpr base::usize INCREMENTAL_CACHE_HEADER_FIELD_COUNT = 2;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_LEGACY_FIELD_COUNT = 6;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_FIELD_COUNT = 10;
constexpr base::usize INCREMENTAL_CACHE_MODULE_LEGACY_FIELD_COUNT = 3;
constexpr base::usize INCREMENTAL_CACHE_MODULE_FIELD_COUNT = 5;
constexpr base::usize INCREMENTAL_CACHE_DEF_FIELD_COUNT = 12;
constexpr base::usize INCREMENTAL_CACHE_QUERY_FIELD_COUNT = 12;
constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_FIELD_COUNT = 13;

constexpr base::usize INCREMENTAL_CACHE_SOURCE_SIZE_FIELD = 1;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_PRIMARY_FIELD = 2;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_SECONDARY_FIELD = 3;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_BYTES_FIELD = 4;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_PATH_FIELD = 5;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_PACKAGE_GLOBAL_FIELD = 6;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_PACKAGE_PRIMARY_FIELD = 7;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_PACKAGE_SECONDARY_FIELD = 8;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_PACKAGE_BYTES_FIELD = 9;

constexpr base::usize INCREMENTAL_CACHE_MODULE_NAME_FIELD = 1;
constexpr base::usize INCREMENTAL_CACHE_MODULE_PATH_FIELD = 2;
constexpr base::usize INCREMENTAL_CACHE_MODULE_SOURCE_ROOT_FIELD = 3;
constexpr base::usize INCREMENTAL_CACHE_MODULE_SOURCE_RELATIVE_FIELD = 4;

constexpr base::usize INCREMENTAL_CACHE_DEF_CATEGORY_FIELD = 1;
constexpr base::usize INCREMENTAL_CACHE_DEF_STABLE_KIND_FIELD = 2;
constexpr base::usize INCREMENTAL_CACHE_DEF_STABLE_GLOBAL_FIELD = 3;
constexpr base::usize INCREMENTAL_CACHE_DEF_STABLE_PRIMARY_FIELD = 4;
constexpr base::usize INCREMENTAL_CACHE_DEF_STABLE_SECONDARY_FIELD = 5;
constexpr base::usize INCREMENTAL_CACHE_DEF_STABLE_BYTES_FIELD = 6;
constexpr base::usize INCREMENTAL_CACHE_DEF_INCREMENTAL_GLOBAL_FIELD = 7;
constexpr base::usize INCREMENTAL_CACHE_DEF_INCREMENTAL_PRIMARY_FIELD = 8;
constexpr base::usize INCREMENTAL_CACHE_DEF_INCREMENTAL_SECONDARY_FIELD = 9;
constexpr base::usize INCREMENTAL_CACHE_DEF_INCREMENTAL_BYTES_FIELD = 10;
constexpr base::usize INCREMENTAL_CACHE_DEF_NAME_FIELD = 11;

constexpr base::usize INCREMENTAL_CACHE_QUERY_KIND_FIELD = 1;
constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEMA_FIELD = 2;
constexpr base::usize INCREMENTAL_CACHE_QUERY_GLOBAL_FIELD = 3;
constexpr base::usize INCREMENTAL_CACHE_QUERY_PAYLOAD_PRIMARY_FIELD = 4;
constexpr base::usize INCREMENTAL_CACHE_QUERY_PAYLOAD_SECONDARY_FIELD = 5;
constexpr base::usize INCREMENTAL_CACHE_QUERY_PAYLOAD_BYTES_FIELD = 6;
constexpr base::usize INCREMENTAL_CACHE_QUERY_RESULT_GLOBAL_FIELD = 7;
constexpr base::usize INCREMENTAL_CACHE_QUERY_RESULT_PRIMARY_FIELD = 8;
constexpr base::usize INCREMENTAL_CACHE_QUERY_RESULT_SECONDARY_FIELD = 9;
constexpr base::usize INCREMENTAL_CACHE_QUERY_RESULT_BYTES_FIELD = 10;
constexpr base::usize INCREMENTAL_CACHE_QUERY_STABLE_KEY_FIELD = 11;

constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_KIND_FIELD = 1;
constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_SCHEMA_FIELD = 2;
constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_GLOBAL_FIELD = 3;
constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_PAYLOAD_PRIMARY_FIELD = 4;
constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_PAYLOAD_SECONDARY_FIELD = 5;
constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_PAYLOAD_BYTES_FIELD = 6;
constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_KIND_FIELD = 7;
constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_SCHEMA_FIELD = 8;
constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_GLOBAL_FIELD = 9;
constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_PAYLOAD_PRIMARY_FIELD = 10;
constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_PAYLOAD_SECONDARY_FIELD = 11;
constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_PAYLOAD_BYTES_FIELD = 12;

constexpr std::string_view INCREMENTAL_CACHE_FIELD_SCHEMA = "schema";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_COMPILER = "compiler";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_MODE = "mode";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_ROOT = "root";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_PACKAGE = "package";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_IMPORT_PATHS = "import_paths";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_IMPORT_PATH = "import_path";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_SOURCES = "sources";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_SOURCE = "source";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_MODULES = "modules";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_MODULE_SOURCE_ROOT_TOPOLOGIES = "module_source_root_topologies";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_MODULE = "module";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_DEFINITIONS = "definitions";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_DEF = "def";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_QUERIES = "queries";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_QUERY = "query";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_QUERY_EDGES = "query_edges";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_QUERY_EDGE = "query_edge";

constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_FUNCTION = "function";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_GENERIC_FUNCTION_INSTANCE = "generic_function_instance";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_GENERIC_TEMPLATE = "generic_template";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_STRUCT = "struct";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_ENUM_CASE = "enum_case";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_TYPE_ALIAS = "type_alias";

constexpr std::string_view INCREMENTAL_CACHE_FILE_CONTENT_RESULT_MARKER = "file-content:v1";
constexpr std::string_view INCREMENTAL_CACHE_LEX_FILE_RESULT_MARKER = "lex-file:v1";
constexpr std::string_view INCREMENTAL_CACHE_LEX_FILE_ERROR_MARKER = "lex-error";
constexpr std::string_view INCREMENTAL_CACHE_PARSE_FILE_RESULT_MARKER = "parse-file:v1";
constexpr std::string_view INCREMENTAL_CACHE_PARSE_FILE_ERROR_MARKER = "parse-error";
constexpr std::string_view INCREMENTAL_CACHE_MODULE_GRAPH_RESULT_MARKER = "module-graph:v1";
constexpr std::string_view INCREMENTAL_CACHE_MODULE_EXPORTS_RESULT_MARKER = "module-exports:v1";
constexpr std::string_view INCREMENTAL_CACHE_MODULE_PACKAGE_EXPORTS_RESULT_MARKER = "module-package-exports:v1";
constexpr std::string_view INCREMENTAL_CACHE_ITEM_LIST_RESULT_MARKER = "item-list:v1";
constexpr std::string_view INCREMENTAL_CACHE_FUNCTION_BODY_SYNTAX_RESULT_MARKER = "function-body-syntax:v1";
constexpr std::string_view INCREMENTAL_CACHE_TYPE_CHECK_BODY_RESULT_MARKER = "type-check-body:v1";
constexpr std::string_view INCREMENTAL_CACHE_GENERIC_INSTANCE_BODY_RESULT_MARKER = "generic-instance-body:v1";
constexpr std::string_view INCREMENTAL_CACHE_LOWER_FUNCTION_IR_RESULT_MARKER = "lower-function-ir:v1";
constexpr std::string_view INCREMENTAL_CACHE_DIAGNOSTICS_RESULT_MARKER = "diagnostics:v1";
constexpr char INCREMENTAL_CACHE_MODULE_NAME_SEPARATOR = '.';

struct SourceFingerprintRecord {
    std::filesystem::path path;
    base::usize size = 0;
    sema::StableFingerprint128 fingerprint;
    query::PackageKey package;
};

struct DefinitionRecord {
    std::string category;
    std::string name;
    sema::StableDefId stable_id;
    sema::IncrementalKey incremental_key;
};

struct ParsedQueryRecord {
    query::QueryRecord record;
};

struct ParsedCache {
    base::u64 schema = 0;
    std::string compiler_version;
    std::string mode;
    std::filesystem::path root_path;
    std::optional<std::string> package_identity;
    std::vector<std::filesystem::path> import_paths;
    std::vector<SourceFingerprintRecord> sources;
    std::vector<ParsedQueryRecord> queries;
    std::vector<query::QueryDependencyEdge> query_edges;
    base::usize module_count = 0;
    base::usize module_source_root_topology_count = 0;
    base::usize definition_count = 0;
    std::optional<base::usize> expected_import_paths;
    std::optional<base::usize> expected_sources;
    std::optional<base::usize> expected_modules;
    std::optional<base::usize> expected_module_source_root_topologies;
    std::optional<base::usize> expected_definitions;
    std::optional<base::usize> expected_queries;
    std::optional<base::usize> expected_query_edges;
};

enum class ParsedCacheReadStatus : base::u8 {
    loaded,
    missing,
    malformed,
};

struct ParsedCacheReadResult {
    std::optional<ParsedCache> cache;
    ParsedCacheReadStatus status = ParsedCacheReadStatus::missing;
};

enum class ParsedCacheValidationStatus : base::u8 {
    valid,
    malformed_cache,
    malformed_query_graph,
    malformed_query_identity,
};

struct QueryKindCacheName {
    query::QueryKind kind = query::QueryKind::invalid;
    std::string_view name;
};

constexpr auto INCREMENTAL_CACHE_QUERY_KIND_NAMES = std::to_array<QueryKindCacheName>({
    {query::QueryKind::file_content, "file_content"},
    {query::QueryKind::lex_file, "lex_file"},
    {query::QueryKind::parse_file, "parse_file"},
    {query::QueryKind::module_graph, "module_graph"},
    {query::QueryKind::module_exports, "module_exports"},
    {query::QueryKind::module_package_exports, "module_package_exports"},
    {query::QueryKind::item_list, "item_list"},
    {query::QueryKind::item_signature, "item_signature"},
    {query::QueryKind::function_body_syntax, "function_body_syntax"},
    {query::QueryKind::type_check_body, "type_check_body"},
    {query::QueryKind::generic_template_signature, "generic_template_signature"},
    {query::QueryKind::generic_instance_signature, "generic_instance_signature"},
    {query::QueryKind::generic_instance_body, "generic_instance_body"},
    {query::QueryKind::diagnostics, "diagnostics"},
    {query::QueryKind::lower_function_ir, "lower_function_ir"},
});

} // namespace aurex::driver::incremental_cache_format
