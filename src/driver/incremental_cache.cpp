#include <aurex/base/config.hpp>
#include <aurex/driver/incremental_cache.hpp>
#include <aurex/driver/profile.hpp>
#include <aurex/query/query_context.hpp>
#include <aurex/query/query_result.hpp>
#include <aurex/query/query_reuse.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace aurex::driver {
namespace {

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
constexpr base::usize INCREMENTAL_CACHE_SOURCE_FIELD_COUNT = 6;
constexpr base::usize INCREMENTAL_CACHE_MODULE_FIELD_COUNT = 3;
constexpr base::usize INCREMENTAL_CACHE_DEF_FIELD_COUNT = 12;
constexpr base::usize INCREMENTAL_CACHE_QUERY_FIELD_COUNT = 12;
constexpr base::usize INCREMENTAL_CACHE_QUERY_EDGE_FIELD_COUNT = 13;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_SIZE_FIELD = 1;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_PRIMARY_FIELD = 2;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_SECONDARY_FIELD = 3;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_BYTES_FIELD = 4;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_PATH_FIELD = 5;
constexpr base::usize INCREMENTAL_CACHE_MODULE_NAME_FIELD = 1;
constexpr base::usize INCREMENTAL_CACHE_MODULE_PATH_FIELD = 2;
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
constexpr std::string_view INCREMENTAL_CACHE_FIELD_IMPORT_PATHS = "import_paths";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_IMPORT_PATH = "import_path";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_SOURCES = "sources";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_SOURCE = "source";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_MODULES = "modules";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_MODULE = "module";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_DEFINITIONS = "definitions";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_DEF = "def";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_QUERIES = "queries";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_QUERY = "query";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_QUERY_EDGES = "query_edges";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_QUERY_EDGE = "query_edge";

constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_FUNCTION = "function";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_GENERIC_FUNCTION_INSTANCE = "generic_function_instance";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_STRUCT = "struct";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_ENUM_CASE = "enum_case";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_TYPE_ALIAS = "type_alias";

constexpr std::string_view INCREMENTAL_CACHE_WRITE_OPEN_FAILED = "failed to open incremental cache file for writing";
constexpr std::string_view INCREMENTAL_CACHE_WRITE_FAILED = "failed to write incremental cache file";
constexpr std::string_view INCREMENTAL_CACHE_RENAME_FAILED = "failed to publish incremental cache file";
constexpr std::string_view INCREMENTAL_CACHE_DIRECTORY_FAILED = "failed to create incremental cache directory";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_QUERY_DIFF = "incremental_cache.query_diff";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_QUERY_PLAN = "incremental_cache.query_plan";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_QUERY_PRUNING = "incremental_cache.query_pruning";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_QUERY_PROVIDER_EVAL = "incremental_cache.query_provider_eval";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_TOTAL = "total=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_MISSING = ",missing=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_UNCHANGED = ",unchanged=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_CHANGED = ",changed=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_MALFORMED = ",malformed=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_REUSABLE = "reusable=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_RECOMPUTE_ROOTS = ",recompute_roots=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROPAGATED_RECOMPUTE = ",propagated_recompute=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_RECOMPUTE = ",recompute=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_ENABLED = "enabled=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_APPLIED = ",applied=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED = ",reused=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED = ",recomputed=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_MODULE_EXPORTS = ",reused_module_exports=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_ITEM_SIGNATURES = ",reused_item_signatures=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_FUNCTION_BODY_SYNTAXES =
    ",reused_function_body_syntaxes=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_TYPE_CHECK_BODIES = ",reused_type_check_bodies=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_GENERIC_INSTANCE_SIGNATURES =
    ",reused_generic_instance_signatures=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_GENERIC_INSTANCE_BODIES =
    ",reused_generic_instance_bodies=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_LOWER_FUNCTION_IRS = ",reused_lower_function_irs=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_DIAGNOSTICS = ",reused_diagnostics=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_MODULE_EXPORTS = ",recomputed_module_exports=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_ITEM_SIGNATURES =
    ",recomputed_item_signatures=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_FUNCTION_BODY_SYNTAXES =
    ",recomputed_function_body_syntaxes=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_TYPE_CHECK_BODIES =
    ",recomputed_type_check_bodies=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_GENERIC_INSTANCE_SIGNATURES =
    ",recomputed_generic_instance_signatures=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_GENERIC_INSTANCE_BODIES =
    ",recomputed_generic_instance_bodies=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_LOWER_FUNCTION_IRS =
    ",recomputed_lower_function_irs=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_DIAGNOSTICS = ",recomputed_diagnostics=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_FALLBACK = ",fallback=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_MODE = "mode=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_MODE_FULL = "full";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_MODE_PRUNED = "pruned";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED = ",seeded=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED = ",evaluated=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_MODULE_EXPORTS = ",seeded_module_exports=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_ITEM_SIGNATURES = ",seeded_item_signatures=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_FUNCTION_BODY_SYNTAXES =
    ",seeded_function_body_syntaxes=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_TYPE_CHECK_BODIES =
    ",seeded_type_check_bodies=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_GENERIC_INSTANCE_SIGNATURES =
    ",seeded_generic_instance_signatures=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_GENERIC_INSTANCE_BODIES =
    ",seeded_generic_instance_bodies=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_LOWER_FUNCTION_IRS =
    ",seeded_lower_function_irs=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_DIAGNOSTICS = ",seeded_diagnostics=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_MODULE_EXPORTS =
    ",evaluated_module_exports=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_ITEM_SIGNATURES =
    ",evaluated_item_signatures=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_FUNCTION_BODY_SYNTAXES =
    ",evaluated_function_body_syntaxes=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_TYPE_CHECK_BODIES =
    ",evaluated_type_check_bodies=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_GENERIC_INSTANCE_SIGNATURES =
    ",evaluated_generic_instance_signatures=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_GENERIC_INSTANCE_BODIES =
    ",evaluated_generic_instance_bodies=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_LOWER_FUNCTION_IRS =
    ",evaluated_lower_function_irs=";
constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_DIAGNOSTICS = ",evaluated_diagnostics=";
constexpr std::string_view INCREMENTAL_CACHE_PRUNING_FALLBACK_DISABLED = "disabled";
constexpr std::string_view INCREMENTAL_CACHE_PRUNING_FALLBACK_NONE = "none";
constexpr std::string_view INCREMENTAL_CACHE_PRUNING_FALLBACK_NO_CACHE = "no_cache";
constexpr std::string_view INCREMENTAL_CACHE_PRUNING_FALLBACK_INCOMPLETE_PLAN = "incomplete_plan";
constexpr std::string_view INCREMENTAL_CACHE_PRUNING_FALLBACK_MISSING_REUSABLE_RECORD = "missing_reusable_record";
constexpr std::string_view INCREMENTAL_CACHE_MODULE_EXPORTS_RESULT_MARKER = "module-exports:v1";
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

struct QueryCollection {
    std::vector<query::QueryRecord> records;
    std::vector<query::QueryDependencyEdge> dependency_edges;
};

struct QueryKindExecutionCounts {
    base::usize total = 0;
    base::usize module_exports = 0;
    base::usize item_signatures = 0;
    base::usize function_body_syntaxes = 0;
    base::usize type_check_bodies = 0;
    base::usize generic_instance_signatures = 0;
    base::usize generic_instance_bodies = 0;
    base::usize lower_function_irs = 0;
    base::usize diagnostics = 0;
};

struct QueryProviderEvaluationStats {
    bool pruned = false;
    QueryKindExecutionCounts seeded;
    QueryKindExecutionCounts evaluated;
};

struct QueryCollectionResult {
    QueryCollection collection;
    QueryProviderEvaluationStats stats;
};

struct QueryReuseEvaluation {
    query::QueryReusePlan plan;
    query::QueryContext cached_context;
    bool cache_loaded = false;
};

struct QueryPruningGateResult {
    bool enabled = false;
    bool applied = false;
    QueryKindExecutionCounts reused;
    QueryKindExecutionCounts recomputed;
    std::string_view fallback = INCREMENTAL_CACHE_PRUNING_FALLBACK_DISABLED;
};

struct QueryKindCacheName {
    query::QueryKind kind = query::QueryKind::invalid;
    std::string_view name;
};

using QueryDependenciesByDependent =
    std::unordered_map<query::QueryKey, std::vector<query::QueryKey>, query::QueryKeyHash>;

struct ModuleExportsQuerySubject {
    query::ModuleKey key;
    query::QueryResultFingerprint result;
};

struct ModuleExportsSignatureEntry {
    std::string category;
    std::string name;
    sema::StableDefId stable_id;
    sema::IncrementalKey incremental_key;
};

struct ItemSignatureQuerySubject {
    sema::StableDefId stable_id;
    sema::IncrementalKey incremental_key;
    query::DefNamespace name_space = query::DefNamespace::value;
    query::DefKind kind = query::DefKind::invalid;
};

struct GenericInstanceSignatureQuerySubject {
    const query::GenericInstanceKey* key = nullptr;
    sema::IncrementalKey incremental_key;
};

struct GenericInstanceBodyQuerySubject {
    const query::GenericInstanceKey* key = nullptr;
    query::QueryResultFingerprint result;
};

struct FunctionBodySyntaxQuerySubject {
    query::BodyKey key;
    query::QueryResultFingerprint result;
};

struct TypeCheckBodyQuerySubject {
    query::BodyKey key;
    query::QueryResultFingerprint result;
};

enum class LowerFunctionIRSubjectKind : base::u8 {
    body,
    generic_instance,
};

struct LowerFunctionIRQuerySubject {
    LowerFunctionIRSubjectKind kind = LowerFunctionIRSubjectKind::body;
    query::BodyKey body;
    const query::GenericInstanceKey* generic_instance = nullptr;
    query::QueryResultFingerprint result;
};

struct DiagnosticsQuerySubject {
    query::QueryKey producer;
    query::QueryResultFingerprint result;
};

enum class QuerySubjectKind : base::u8 {
    module_exports,
    item_signature,
    function_body_syntax,
    type_check_body,
    generic_instance_signature,
    generic_instance_body,
    lower_function_ir,
    diagnostics,
};

struct QuerySubject {
    QuerySubjectKind kind = QuerySubjectKind::module_exports;
    base::usize index = 0;
    query::QueryRecord record;
};

struct QuerySubjectCollection {
    std::vector<ModuleExportsQuerySubject> module_exports;
    std::vector<ItemSignatureQuerySubject> item_signatures;
    std::vector<FunctionBodySyntaxQuerySubject> function_body_syntaxes;
    std::vector<TypeCheckBodyQuerySubject> type_check_bodies;
    std::vector<GenericInstanceSignatureQuerySubject> generic_instance_signatures;
    std::vector<GenericInstanceBodyQuerySubject> generic_instance_bodies;
    std::vector<LowerFunctionIRQuerySubject> lower_function_irs;
    std::vector<DiagnosticsQuerySubject> diagnostics;
    std::vector<QuerySubject> subjects;
    std::vector<query::QueryRecord> records;
};

struct ParsedCache {
    base::u64 schema = 0;
    std::string compiler_version;
    std::string mode;
    std::filesystem::path root_path;
    std::vector<std::filesystem::path> import_paths;
    std::vector<SourceFingerprintRecord> sources;
    std::vector<ParsedQueryRecord> queries;
    std::vector<query::QueryDependencyEdge> query_edges;
    base::usize module_count = 0;
    base::usize definition_count = 0;
    std::optional<base::usize> expected_import_paths;
    std::optional<base::usize> expected_sources;
    std::optional<base::usize> expected_modules;
    std::optional<base::usize> expected_definitions;
    std::optional<base::usize> expected_queries;
    std::optional<base::usize> expected_query_edges;
};

constexpr auto INCREMENTAL_CACHE_QUERY_KIND_NAMES = std::to_array<QueryKindCacheName>({
    {query::QueryKind::file_content, "file_content"},
    {query::QueryKind::lex_file, "lex_file"},
    {query::QueryKind::parse_file, "parse_file"},
    {query::QueryKind::module_graph, "module_graph"},
    {query::QueryKind::module_exports, "module_exports"},
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

[[nodiscard]] std::filesystem::path canonical_or_absolute(const std::filesystem::path& path)
{
    std::error_code canonical_error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, canonical_error);
    if (!canonical_error) {
        return canonical;
    }

    std::error_code absolute_error;
    std::filesystem::path absolute = std::filesystem::absolute(path, absolute_error);
    return absolute_error ? path : absolute;
}

[[nodiscard]] sema::StableFingerprint128 fingerprint_text(const std::string_view text) noexcept
{
    return sema::stable_fingerprint(text);
}

[[nodiscard]] std::optional<std::string> read_file_for_fingerprint(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    std::string text;
    std::error_code size_error;
    const std::uintmax_t size = std::filesystem::file_size(path, size_error);
    if (!size_error) {
        text.resize(static_cast<std::size_t>(size));
        if (!text.empty()) {
            input.read(text.data(), static_cast<std::streamsize>(text.size()));
            if (!input) {
                return std::nullopt;
            }
        }
        return text;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) {
        return std::nullopt;
    }
    return buffer.str();
}

[[nodiscard]] std::optional<SourceFingerprintRecord> fingerprint_file(const std::filesystem::path& path)
{
    const std::optional<std::string> text = read_file_for_fingerprint(path);
    if (!text) {
        return std::nullopt;
    }
    return SourceFingerprintRecord{
        canonical_or_absolute(path),
        text->size(),
        fingerprint_text(*text),
    };
}

[[nodiscard]] bool same_fingerprint(const SourceFingerprintRecord& lhs, const SourceFingerprintRecord& rhs) noexcept
{
    return lhs.size == rhs.size && lhs.fingerprint.primary == rhs.fingerprint.primary
        && lhs.fingerprint.secondary == rhs.fingerprint.secondary
        && lhs.fingerprint.byte_count == rhs.fingerprint.byte_count;
}

[[nodiscard]] bool parse_u64(const std::string_view text, base::u64& value) noexcept
{
    base::u64 parsed = 0;
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, parsed, INCREMENTAL_CACHE_DECIMAL_BASE);
    if (result.ec != std::errc{} || result.ptr != end) {
        return false;
    }
    value = parsed;
    return true;
}

[[nodiscard]] bool parse_usize(const std::string_view text, base::usize& value) noexcept
{
    base::u64 parsed = 0;
    if (!parse_u64(text, parsed) || parsed > std::numeric_limits<base::usize>::max()) {
        return false;
    }
    value = static_cast<base::usize>(parsed);
    return true;
}

[[nodiscard]] bool parse_u32(const std::string_view text, base::u32& value) noexcept
{
    base::u64 parsed = 0;
    if (!parse_u64(text, parsed) || parsed > std::numeric_limits<base::u32>::max()) {
        return false;
    }
    value = static_cast<base::u32>(parsed);
    return true;
}

[[nodiscard]] int hex_nibble(const char value) noexcept
{
    if (value >= '0' && value <= '9') {
        return value - INCREMENTAL_CACHE_HEX_DIGIT_PREFIX;
    }
    if (value >= 'a' && value <= 'f') {
        return value - INCREMENTAL_CACHE_HEX_ALPHA_PREFIX + INCREMENTAL_CACHE_HEX_LETTER_OFFSET;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + INCREMENTAL_CACHE_HEX_LETTER_OFFSET;
    }
    return INCREMENTAL_CACHE_INVALID_HEX_NIBBLE;
}

void append_hex_string(std::ostream& out, const std::string_view value)
{
    static constexpr char DIGITS[] = "0123456789abcdef";
    for (const unsigned char byte : value) {
        out << DIGITS[byte >> INCREMENTAL_CACHE_HEX_BYTE_SHIFT] << DIGITS[byte & INCREMENTAL_CACHE_HEX_LOW_MASK];
    }
}

[[nodiscard]] std::optional<std::string> decode_hex_string(const std::string_view encoded)
{
    if (encoded.size() % INCREMENTAL_CACHE_HEX_BYTE_WIDTH != 0) {
        return std::nullopt;
    }

    std::string decoded;
    decoded.reserve(encoded.size() / INCREMENTAL_CACHE_HEX_BYTE_WIDTH);
    for (base::usize index = 0; index < encoded.size(); index += INCREMENTAL_CACHE_HEX_BYTE_WIDTH) {
        const int high = hex_nibble(encoded[index]);
        const int low = hex_nibble(encoded[index + 1]);
        if (high == INCREMENTAL_CACHE_INVALID_HEX_NIBBLE || low == INCREMENTAL_CACHE_INVALID_HEX_NIBBLE) {
            return std::nullopt;
        }
        decoded.push_back(static_cast<char>((high << INCREMENTAL_CACHE_HEX_BYTE_SHIFT) | low));
    }
    return decoded;
}

[[nodiscard]] std::optional<std::filesystem::path> decode_path(const std::string_view encoded)
{
    const std::optional<std::string> decoded = decode_hex_string(encoded);
    if (!decoded) {
        return std::nullopt;
    }
    return std::filesystem::path(*decoded);
}

[[nodiscard]] std::vector<std::string_view> split_cache_fields(const std::string_view line)
{
    std::vector<std::string_view> fields;
    base::usize begin = 0;
    for (;;) {
        const base::usize end = line.find(INCREMENTAL_CACHE_SEPARATOR, begin);
        if (end == std::string_view::npos) {
            fields.push_back(line.substr(begin));
            return fields;
        }
        fields.push_back(line.substr(begin, end - begin));
        begin = end + 1;
    }
}

[[nodiscard]] bool assign_count(std::optional<base::usize>& target, const std::string_view value) noexcept
{
    if (target) {
        return false;
    }
    base::usize parsed = 0;
    if (!parse_usize(value, parsed)) {
        return false;
    }
    target = parsed;
    return true;
}

[[nodiscard]] bool parse_header_line(ParsedCache& cache, const std::vector<std::string_view>& fields)
{
    if (fields.size() != INCREMENTAL_CACHE_HEADER_FIELD_COUNT) {
        return false;
    }

    const std::string_view kind = fields[INCREMENTAL_CACHE_KIND_FIELD];
    const std::string_view value = fields[INCREMENTAL_CACHE_FIRST_VALUE_FIELD];
    if (kind == INCREMENTAL_CACHE_FIELD_SCHEMA) {
        return cache.schema == 0 && parse_u64(value, cache.schema);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_COMPILER) {
        std::optional<std::string> compiler = decode_hex_string(value);
        if (!compiler || !cache.compiler_version.empty()) {
            return false;
        }
        cache.compiler_version = std::move(*compiler);
        return true;
    }
    if (kind == INCREMENTAL_CACHE_FIELD_MODE) {
        std::optional<std::string> mode = decode_hex_string(value);
        if (!mode || !cache.mode.empty()) {
            return false;
        }
        cache.mode = std::move(*mode);
        return true;
    }
    if (kind == INCREMENTAL_CACHE_FIELD_ROOT) {
        const std::optional<std::filesystem::path> path = decode_path(value);
        if (!path || !cache.root_path.empty()) {
            return false;
        }
        cache.root_path = *path;
        return true;
    }
    if (kind == INCREMENTAL_CACHE_FIELD_IMPORT_PATHS) {
        return assign_count(cache.expected_import_paths, value);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_SOURCES) {
        return assign_count(cache.expected_sources, value);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_MODULES) {
        return assign_count(cache.expected_modules, value);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_DEFINITIONS) {
        return assign_count(cache.expected_definitions, value);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_QUERIES) {
        return assign_count(cache.expected_queries, value);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_QUERY_EDGES) {
        return assign_count(cache.expected_query_edges, value);
    }
    return false;
}

[[nodiscard]] bool parse_import_path_line(ParsedCache& cache, const std::vector<std::string_view>& fields)
{
    if (fields.size() != INCREMENTAL_CACHE_HEADER_FIELD_COUNT
        || fields[INCREMENTAL_CACHE_KIND_FIELD] != INCREMENTAL_CACHE_FIELD_IMPORT_PATH) {
        return false;
    }
    const std::optional<std::filesystem::path> path = decode_path(fields[INCREMENTAL_CACHE_FIRST_VALUE_FIELD]);
    if (!path) {
        return false;
    }
    cache.import_paths.push_back(*path);
    return true;
}

[[nodiscard]] bool parse_source_line(ParsedCache& cache, const std::vector<std::string_view>& fields)
{
    if (fields.size() != INCREMENTAL_CACHE_SOURCE_FIELD_COUNT
        || fields[INCREMENTAL_CACHE_KIND_FIELD] != INCREMENTAL_CACHE_FIELD_SOURCE) {
        return false;
    }

    SourceFingerprintRecord record;
    base::u64 size = 0;
    if (!parse_u64(fields[INCREMENTAL_CACHE_SOURCE_SIZE_FIELD], size) || size > std::numeric_limits<base::usize>::max()
        || !parse_u64(fields[INCREMENTAL_CACHE_SOURCE_PRIMARY_FIELD], record.fingerprint.primary)
        || !parse_u64(fields[INCREMENTAL_CACHE_SOURCE_SECONDARY_FIELD], record.fingerprint.secondary)
        || !parse_u32(fields[INCREMENTAL_CACHE_SOURCE_BYTES_FIELD], record.fingerprint.byte_count)) {
        return false;
    }
    record.size = static_cast<base::usize>(size);

    const std::optional<std::filesystem::path> path = decode_path(fields[INCREMENTAL_CACHE_SOURCE_PATH_FIELD]);
    if (!path) {
        return false;
    }
    record.path = *path;
    cache.sources.push_back(std::move(record));
    return true;
}

[[nodiscard]] bool parse_module_line(ParsedCache& cache, const std::vector<std::string_view>& fields)
{
    if (fields.size() != INCREMENTAL_CACHE_MODULE_FIELD_COUNT
        || fields[INCREMENTAL_CACHE_KIND_FIELD] != INCREMENTAL_CACHE_FIELD_MODULE
        || !decode_hex_string(fields[INCREMENTAL_CACHE_MODULE_NAME_FIELD])
        || !decode_path(fields[INCREMENTAL_CACHE_MODULE_PATH_FIELD])) {
        return false;
    }
    cache.module_count += 1;
    return true;
}

[[nodiscard]] bool parse_definition_line(ParsedCache& cache, const std::vector<std::string_view>& fields)
{
    if (fields.size() != INCREMENTAL_CACHE_DEF_FIELD_COUNT
        || fields[INCREMENTAL_CACHE_KIND_FIELD] != INCREMENTAL_CACHE_FIELD_DEF
        || fields[INCREMENTAL_CACHE_DEF_CATEGORY_FIELD].empty()
        || fields[INCREMENTAL_CACHE_DEF_STABLE_KIND_FIELD].empty()
        || !decode_hex_string(fields[INCREMENTAL_CACHE_DEF_NAME_FIELD])) {
        return false;
    }

    base::u64 stable_global = 0;
    base::u64 stable_primary = 0;
    base::u64 stable_secondary = 0;
    base::u32 stable_bytes = 0;
    base::u64 incremental_global = 0;
    base::u64 incremental_primary = 0;
    base::u64 incremental_secondary = 0;
    base::u32 incremental_bytes = 0;
    if (!parse_u64(fields[INCREMENTAL_CACHE_DEF_STABLE_GLOBAL_FIELD], stable_global)
        || !parse_u64(fields[INCREMENTAL_CACHE_DEF_STABLE_PRIMARY_FIELD], stable_primary)
        || !parse_u64(fields[INCREMENTAL_CACHE_DEF_STABLE_SECONDARY_FIELD], stable_secondary)
        || !parse_u32(fields[INCREMENTAL_CACHE_DEF_STABLE_BYTES_FIELD], stable_bytes)
        || !parse_u64(fields[INCREMENTAL_CACHE_DEF_INCREMENTAL_GLOBAL_FIELD], incremental_global)
        || !parse_u64(fields[INCREMENTAL_CACHE_DEF_INCREMENTAL_PRIMARY_FIELD], incremental_primary)
        || !parse_u64(fields[INCREMENTAL_CACHE_DEF_INCREMENTAL_SECONDARY_FIELD], incremental_secondary)
        || !parse_u32(fields[INCREMENTAL_CACHE_DEF_INCREMENTAL_BYTES_FIELD], incremental_bytes)) {
        return false;
    }
    cache.definition_count += 1;
    return true;
}

[[nodiscard]] std::optional<query::QueryKind> parse_query_kind_name(const std::string_view name) noexcept
{
    for (const QueryKindCacheName& entry : INCREMENTAL_CACHE_QUERY_KIND_NAMES) {
        if (entry.name == name) {
            return entry.kind;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<query::QueryKey> parse_query_key_fields(const std::vector<std::string_view>& fields,
    const base::usize kind_field, const base::usize schema_field, const base::usize global_field,
    const base::usize payload_primary_field, const base::usize payload_secondary_field,
    const base::usize payload_bytes_field)
{
    const std::optional<query::QueryKind> query_kind = parse_query_kind_name(fields[kind_field]);
    if (!query_kind.has_value()) {
        return std::nullopt;
    }

    base::u32 schema = 0;
    base::u64 query_global = 0;
    base::u64 payload_primary = 0;
    base::u64 payload_secondary = 0;
    base::u32 payload_bytes = 0;
    if (!parse_u32(fields[schema_field], schema) || schema != query::QUERY_KEY_SCHEMA_VERSION
        || !parse_u64(fields[global_field], query_global) || query_global == 0
        || !parse_u64(fields[payload_primary_field], payload_primary)
        || !parse_u64(fields[payload_secondary_field], payload_secondary)
        || !parse_u32(fields[payload_bytes_field], payload_bytes)) {
        return std::nullopt;
    }

    const query::StableFingerprint128 payload{
        payload_primary,
        payload_secondary,
        payload_bytes,
    };
    const query::QueryKey key =
        query::query_key(*query_kind, payload, static_cast<base::u16>(query::QUERY_KEY_SCHEMA_VERSION));
    if (!query::is_valid(key) || key.global_id != query_global) {
        return std::nullopt;
    }
    return key;
}

[[nodiscard]] std::optional<base::usize> parsed_query_record_index(
    const ParsedCache& cache, const query::QueryKind kind, const std::string_view stable_key_bytes)
{
    for (base::usize index = 0; index < cache.queries.size(); ++index) {
        const ParsedQueryRecord& record = cache.queries[index];
        if (record.record.key.kind == kind && record.record.stable_key_bytes == stable_key_bytes) {
            return index;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool push_parsed_query_record(ParsedCache& cache, ParsedQueryRecord record)
{
    if (parsed_query_record_index(cache, record.record.key.kind, record.record.stable_key_bytes)) {
        return false;
    }
    cache.queries.push_back(std::move(record));
    return true;
}

[[nodiscard]] bool parsed_query_record_exists(const ParsedCache& cache, const query::QueryKey key) noexcept
{
    for (const ParsedQueryRecord& record : cache.queries) {
        if (record.record.key == key) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool has_parsed_query_dependency_edge(
    const ParsedCache& cache, const query::QueryDependencyEdge edge) noexcept
{
    return std::find(cache.query_edges.begin(), cache.query_edges.end(), edge) != cache.query_edges.end();
}

[[nodiscard]] bool push_parsed_query_dependency_edge(ParsedCache& cache, const query::QueryDependencyEdge edge)
{
    if (!query::is_valid(edge.dependent) || !query::is_valid(edge.dependency)
        || has_parsed_query_dependency_edge(cache, edge)) {
        return false;
    }
    cache.query_edges.push_back(edge);
    return true;
}

[[nodiscard]] bool parse_query_line(ParsedCache& cache, const std::vector<std::string_view>& fields)
{
    if (fields.size() != INCREMENTAL_CACHE_QUERY_FIELD_COUNT
        || fields[INCREMENTAL_CACHE_KIND_FIELD] != INCREMENTAL_CACHE_FIELD_QUERY) {
        return false;
    }

    const std::optional<query::QueryKind> query_kind =
        parse_query_kind_name(fields[INCREMENTAL_CACHE_QUERY_KIND_FIELD]);
    if (!query_kind.has_value()) {
        return false;
    }

    base::u32 schema = 0;
    base::u64 query_global = 0;
    base::u64 payload_primary = 0;
    base::u64 payload_secondary = 0;
    base::u32 payload_bytes = 0;
    base::u64 result_global = 0;
    base::u64 result_primary = 0;
    base::u64 result_secondary = 0;
    base::u32 result_bytes = 0;
    std::optional<std::string> stable_key_bytes = decode_hex_string(fields[INCREMENTAL_CACHE_QUERY_STABLE_KEY_FIELD]);
    if (!parse_u32(fields[INCREMENTAL_CACHE_QUERY_SCHEMA_FIELD], schema) || schema != query::QUERY_KEY_SCHEMA_VERSION
        || !parse_u64(fields[INCREMENTAL_CACHE_QUERY_GLOBAL_FIELD], query_global)
        || !parse_u64(fields[INCREMENTAL_CACHE_QUERY_PAYLOAD_PRIMARY_FIELD], payload_primary)
        || !parse_u64(fields[INCREMENTAL_CACHE_QUERY_PAYLOAD_SECONDARY_FIELD], payload_secondary)
        || !parse_u32(fields[INCREMENTAL_CACHE_QUERY_PAYLOAD_BYTES_FIELD], payload_bytes)
        || !parse_u64(fields[INCREMENTAL_CACHE_QUERY_RESULT_GLOBAL_FIELD], result_global)
        || !parse_u64(fields[INCREMENTAL_CACHE_QUERY_RESULT_PRIMARY_FIELD], result_primary)
        || !parse_u64(fields[INCREMENTAL_CACHE_QUERY_RESULT_SECONDARY_FIELD], result_secondary)
        || !parse_u32(fields[INCREMENTAL_CACHE_QUERY_RESULT_BYTES_FIELD], result_bytes) || query_global == 0
        || result_global == 0 || !stable_key_bytes || stable_key_bytes->empty()) {
        return false;
    }

    const query::StableFingerprint128 payload{
        payload_primary,
        payload_secondary,
        payload_bytes,
    };
    if (payload != query::stable_fingerprint(*stable_key_bytes)) {
        return false;
    }

    query::QueryKey key = query::query_key(*query_kind, payload, static_cast<base::u16>(schema));
    if (key.global_id != query_global) {
        return false;
    }

    const query::StableFingerprint128 result_fingerprint{
        result_primary,
        result_secondary,
        result_bytes,
    };
    if (result_fingerprint.byte_count == 0) {
        return false;
    }

    ParsedQueryRecord record{
        query::QueryRecord{
            key,
            query::QueryResultFingerprint{
                result_fingerprint,
                result_global,
            },
            std::move(*stable_key_bytes),
        },
    };
    if (!query::is_valid(record.record)) {
        return false;
    }
    return push_parsed_query_record(cache, std::move(record));
}

[[nodiscard]] bool parse_query_edge_line(ParsedCache& cache, const std::vector<std::string_view>& fields)
{
    if (fields.size() != INCREMENTAL_CACHE_QUERY_EDGE_FIELD_COUNT
        || fields[INCREMENTAL_CACHE_KIND_FIELD] != INCREMENTAL_CACHE_FIELD_QUERY_EDGE) {
        return false;
    }

    const std::optional<query::QueryKey> dependent =
        parse_query_key_fields(fields, INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_KIND_FIELD,
            INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_SCHEMA_FIELD, INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_GLOBAL_FIELD,
            INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_PAYLOAD_PRIMARY_FIELD,
            INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_PAYLOAD_SECONDARY_FIELD,
            INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_PAYLOAD_BYTES_FIELD);
    const std::optional<query::QueryKey> dependency =
        parse_query_key_fields(fields, INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_KIND_FIELD,
            INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_SCHEMA_FIELD, INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_GLOBAL_FIELD,
            INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_PAYLOAD_PRIMARY_FIELD,
            INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_PAYLOAD_SECONDARY_FIELD,
            INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_PAYLOAD_BYTES_FIELD);
    if (!dependent || !dependency) {
        return false;
    }

    return push_parsed_query_dependency_edge(cache,
        query::QueryDependencyEdge{
            *dependent,
            *dependency,
        });
}

[[nodiscard]] bool parsed_cache_query_edges_resolve(const ParsedCache& cache) noexcept
{
    for (const query::QueryDependencyEdge& edge : cache.query_edges) {
        if (!parsed_query_record_exists(cache, edge.dependent)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] QueryDependenciesByDependent query_dependencies_by_dependent(const ParsedCache& cache)
{
    QueryDependenciesByDependent dependencies;
    dependencies.reserve(cache.query_edges.size());
    for (const query::QueryDependencyEdge& edge : cache.query_edges) {
        dependencies[edge.dependent].push_back(edge.dependency);
    }
    return dependencies;
}

[[nodiscard]] bool query_key_less(const query::QueryKey lhs, const query::QueryKey rhs) noexcept
{
    return std::tie(
               lhs.kind, lhs.schema, lhs.global_id, lhs.payload.primary, lhs.payload.secondary, lhs.payload.byte_count)
        < std::tie(
            rhs.kind, rhs.schema, rhs.global_id, rhs.payload.primary, rhs.payload.secondary, rhs.payload.byte_count);
}

[[nodiscard]] bool query_record_key_less(const query::QueryRecord& lhs, const query::QueryRecord& rhs) noexcept
{
    return std::tie(lhs.key.kind, lhs.key.global_id, lhs.result.global_id, lhs.stable_key_bytes)
        < std::tie(rhs.key.kind, rhs.key.global_id, rhs.result.global_id, rhs.stable_key_bytes);
}

[[nodiscard]] bool contains_query_key(const std::vector<query::QueryKey>& keys, const query::QueryKey key) noexcept
{
    return std::binary_search(keys.begin(), keys.end(), key, query_key_less);
}

[[nodiscard]] bool parse_cache_line(ParsedCache& cache, const std::string_view line)
{
    if (line.empty()) {
        return true;
    }

    const std::vector<std::string_view> fields = split_cache_fields(line);
    const std::string_view kind = fields[INCREMENTAL_CACHE_KIND_FIELD];
    if (kind == INCREMENTAL_CACHE_FIELD_IMPORT_PATH) {
        return parse_import_path_line(cache, fields);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_SOURCE) {
        return parse_source_line(cache, fields);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_MODULE) {
        return parse_module_line(cache, fields);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_DEF) {
        return parse_definition_line(cache, fields);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_QUERY) {
        return parse_query_line(cache, fields);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_QUERY_EDGE) {
        return parse_query_edge_line(cache, fields);
    }
    return parse_header_line(cache, fields);
}

[[nodiscard]] bool parsed_cache_counts_match(const ParsedCache& cache) noexcept
{
    const bool queries_match =
        cache.expected_queries.has_value() ? cache.queries.size() == *cache.expected_queries : cache.queries.empty();
    const bool query_edges_match = cache.expected_query_edges.has_value()
        ? cache.query_edges.size() == *cache.expected_query_edges
        : cache.query_edges.empty();
    return cache.expected_import_paths && cache.expected_sources && cache.expected_modules && cache.expected_definitions
        && cache.import_paths.size() == *cache.expected_import_paths && cache.sources.size() == *cache.expected_sources
        && cache.module_count == *cache.expected_modules && cache.definition_count == *cache.expected_definitions
        && queries_match && query_edges_match && parsed_cache_query_edges_resolve(cache);
}

[[nodiscard]] bool parsed_cache_header_matches(const ParsedCache& cache, const CompilerInvocation& invocation)
{
    if (cache.schema != INCREMENTAL_CACHE_SCHEMA_VERSION || cache.compiler_version != base::config::AUREX_VERSION_STRING
        || cache.mode != INCREMENTAL_CACHE_MODE_SEMANTIC_OK
        || cache.root_path != canonical_or_absolute(invocation.input_path) || !parsed_cache_counts_match(cache)) {
        return false;
    }

    if (cache.import_paths.size() != invocation.import_paths.size()) {
        return false;
    }
    for (base::usize index = 0; index < cache.import_paths.size(); ++index) {
        if (cache.import_paths[index] != canonical_or_absolute(invocation.import_paths[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<ParsedCache> read_incremental_cache(const std::filesystem::path& cache_path)
{
    std::error_code exists_error;
    if (!std::filesystem::exists(cache_path, exists_error) || exists_error) {
        return std::nullopt;
    }

    std::ifstream input(cache_path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    std::string line;
    if (!std::getline(input, line) || line != INCREMENTAL_CACHE_MAGIC) {
        return std::nullopt;
    }

    ParsedCache cache;
    while (std::getline(input, line)) {
        if (!parse_cache_line(cache, line)) {
            return std::nullopt;
        }
    }
    if (input.bad()) {
        return std::nullopt;
    }
    return cache;
}

[[nodiscard]] query::QueryContext seed_query_context_from_cache(const ParsedCache& cache)
{
    query::QueryContext context;
    QueryDependenciesByDependent dependencies_by_dependent = query_dependencies_by_dependent(cache);
    for (const ParsedQueryRecord& record : cache.queries) {
        std::vector<query::QueryKey> dependencies;
        const auto found = dependencies_by_dependent.find(record.record.key);
        if (found != dependencies_by_dependent.end()) {
            dependencies = std::move(found->second);
        }
        static_cast<void>(context.seed_completed_record(record.record, std::move(dependencies)));
    }
    return context;
}

[[nodiscard]] QueryReuseEvaluation build_query_reuse_evaluation_against_cache(
    const ParsedCache& cache, const std::span<const query::QueryRecord> current_records)
{
    query::QueryContext cached_context = seed_query_context_from_cache(cache);
    query::QueryReusePlan plan = query::build_query_reuse_plan(cached_context, current_records);
    return QueryReuseEvaluation{
        std::move(plan),
        std::move(cached_context),
        true,
    };
}

[[nodiscard]] QueryReuseEvaluation build_existing_query_reuse_evaluation(
    const std::filesystem::path& cache_path, const std::span<const query::QueryRecord> current_records)
{
    const std::optional<ParsedCache> cache = read_incremental_cache(cache_path);
    if (!cache) {
        return QueryReuseEvaluation{
            query::mark_all_queries_recompute(current_records),
            query::QueryContext{},
            false,
        };
    }
    return build_query_reuse_evaluation_against_cache(*cache, current_records);
}

[[nodiscard]] std::string query_record_diff_summary_detail(const query::QueryReuseSummary& summary)
{
    std::ostringstream detail;
    detail << INCREMENTAL_CACHE_PROFILE_TOTAL << summary.total << INCREMENTAL_CACHE_PROFILE_MISSING << summary.missing
           << INCREMENTAL_CACHE_PROFILE_UNCHANGED << summary.unchanged << INCREMENTAL_CACHE_PROFILE_CHANGED
           << summary.changed << INCREMENTAL_CACHE_PROFILE_MALFORMED << summary.malformed;
    return detail.str();
}

[[nodiscard]] std::string query_reuse_plan_summary_detail(const query::QueryReusePlan& plan)
{
    std::ostringstream detail;
    detail << INCREMENTAL_CACHE_PROFILE_REUSABLE << plan.reusable.size() << INCREMENTAL_CACHE_PROFILE_RECOMPUTE_ROOTS
           << plan.recompute_roots.size() << INCREMENTAL_CACHE_PROFILE_PROPAGATED_RECOMPUTE
           << plan.propagated_recompute.size() << INCREMENTAL_CACHE_PROFILE_RECOMPUTE << plan.recompute.size();
    return detail.str();
}

[[nodiscard]] base::usize total_query_execution_count(const QueryKindExecutionCounts& counts) noexcept
{
    return counts.total;
}

void increment_query_kind_count(QueryKindExecutionCounts& counts, const query::QueryKind kind) noexcept
{
    counts.total += 1;
    switch (kind) {
        case query::QueryKind::module_exports:
            counts.module_exports += 1;
            return;
        case query::QueryKind::item_signature:
            counts.item_signatures += 1;
            return;
        case query::QueryKind::function_body_syntax:
            counts.function_body_syntaxes += 1;
            return;
        case query::QueryKind::type_check_body:
            counts.type_check_bodies += 1;
            return;
        case query::QueryKind::generic_instance_signature:
            counts.generic_instance_signatures += 1;
            return;
        case query::QueryKind::generic_instance_body:
            counts.generic_instance_bodies += 1;
            return;
        case query::QueryKind::lower_function_ir:
            counts.lower_function_irs += 1;
            return;
        case query::QueryKind::diagnostics:
            counts.diagnostics += 1;
            return;
        case query::QueryKind::invalid:
        case query::QueryKind::file_content:
        case query::QueryKind::lex_file:
        case query::QueryKind::parse_file:
        case query::QueryKind::module_graph:
        case query::QueryKind::item_list:
        case query::QueryKind::generic_template_signature:
            return;
    }
}

[[nodiscard]] QueryKindExecutionCounts query_record_counts_by_kind(
    const std::span<const query::QueryRecord> records) noexcept
{
    QueryKindExecutionCounts counts;
    for (const query::QueryRecord& record : records) {
        increment_query_kind_count(counts, record.key.kind);
    }
    return counts;
}

[[nodiscard]] std::string query_pruning_summary_detail(const QueryPruningGateResult& result)
{
    std::ostringstream detail;
    detail << INCREMENTAL_CACHE_PROFILE_PRUNING_ENABLED << (result.enabled ? 1 : 0)
           << INCREMENTAL_CACHE_PROFILE_PRUNING_APPLIED << (result.applied ? 1 : 0)
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED << total_query_execution_count(result.reused)
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED << total_query_execution_count(result.recomputed)
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_MODULE_EXPORTS << result.reused.module_exports
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_ITEM_SIGNATURES << result.reused.item_signatures
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_FUNCTION_BODY_SYNTAXES << result.reused.function_body_syntaxes
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_TYPE_CHECK_BODIES << result.reused.type_check_bodies
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_GENERIC_INSTANCE_SIGNATURES
           << result.reused.generic_instance_signatures
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_GENERIC_INSTANCE_BODIES << result.reused.generic_instance_bodies
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_LOWER_FUNCTION_IRS << result.reused.lower_function_irs
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_DIAGNOSTICS << result.reused.diagnostics
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_MODULE_EXPORTS << result.recomputed.module_exports
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_ITEM_SIGNATURES << result.recomputed.item_signatures
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_FUNCTION_BODY_SYNTAXES
           << result.recomputed.function_body_syntaxes << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_TYPE_CHECK_BODIES
           << result.recomputed.type_check_bodies
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_GENERIC_INSTANCE_SIGNATURES
           << result.recomputed.generic_instance_signatures
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_GENERIC_INSTANCE_BODIES
           << result.recomputed.generic_instance_bodies
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_LOWER_FUNCTION_IRS << result.recomputed.lower_function_irs
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_DIAGNOSTICS << result.recomputed.diagnostics
           << INCREMENTAL_CACHE_PROFILE_PRUNING_FALLBACK << result.fallback;
    return detail.str();
}

[[nodiscard]] std::string query_provider_evaluation_summary_detail(const QueryProviderEvaluationStats& stats)
{
    std::ostringstream detail;
    detail << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_MODE
           << (stats.pruned ? INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_MODE_PRUNED
                            : INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_MODE_FULL)
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED << total_query_execution_count(stats.seeded)
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED << total_query_execution_count(stats.evaluated)
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_MODULE_EXPORTS << stats.seeded.module_exports
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_ITEM_SIGNATURES << stats.seeded.item_signatures
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_FUNCTION_BODY_SYNTAXES
           << stats.seeded.function_body_syntaxes << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_TYPE_CHECK_BODIES
           << stats.seeded.type_check_bodies
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_GENERIC_INSTANCE_SIGNATURES
           << stats.seeded.generic_instance_signatures
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_GENERIC_INSTANCE_BODIES
           << stats.seeded.generic_instance_bodies << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_LOWER_FUNCTION_IRS
           << stats.seeded.lower_function_irs << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_DIAGNOSTICS
           << stats.seeded.diagnostics << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_MODULE_EXPORTS
           << stats.evaluated.module_exports << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_ITEM_SIGNATURES
           << stats.evaluated.item_signatures
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_FUNCTION_BODY_SYNTAXES
           << stats.evaluated.function_body_syntaxes
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_TYPE_CHECK_BODIES << stats.evaluated.type_check_bodies
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_GENERIC_INSTANCE_SIGNATURES
           << stats.evaluated.generic_instance_signatures
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_GENERIC_INSTANCE_BODIES
           << stats.evaluated.generic_instance_bodies
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_LOWER_FUNCTION_IRS << stats.evaluated.lower_function_irs
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_DIAGNOSTICS << stats.evaluated.diagnostics;
    return detail.str();
}

void record_query_record_diff_summary(CompilationProfiler* const profiler, const query::QueryReuseSummary& summary,
    const std::chrono::steady_clock::duration elapsed)
{
    if (profiler == nullptr || !profiler->enabled()) {
        return;
    }
    profiler->record(INCREMENTAL_CACHE_PROFILE_QUERY_DIFF, query_record_diff_summary_detail(summary), elapsed);
}

void record_query_reuse_plan_summary(CompilationProfiler* const profiler, const query::QueryReusePlan& plan,
    const std::chrono::steady_clock::duration elapsed)
{
    if (profiler == nullptr || !profiler->enabled()) {
        return;
    }
    profiler->record(INCREMENTAL_CACHE_PROFILE_QUERY_PLAN, query_reuse_plan_summary_detail(plan), elapsed);
}

void record_query_pruning_summary(CompilationProfiler* const profiler, const QueryPruningGateResult& result,
    const std::chrono::steady_clock::duration elapsed)
{
    if (!result.enabled || profiler == nullptr || !profiler->enabled()) {
        return;
    }
    profiler->record(INCREMENTAL_CACHE_PROFILE_QUERY_PRUNING, query_pruning_summary_detail(result), elapsed);
}

void record_query_provider_evaluation_summary(CompilationProfiler* const profiler,
    const QueryProviderEvaluationStats& stats, const std::chrono::steady_clock::duration elapsed)
{
    if (!stats.pruned || profiler == nullptr || !profiler->enabled()) {
        return;
    }
    profiler->record(
        INCREMENTAL_CACHE_PROFILE_QUERY_PROVIDER_EVAL, query_provider_evaluation_summary_detail(stats), elapsed);
}

[[nodiscard]] bool query_reuse_plan_matches_records(
    const query::QueryReusePlan& plan, const std::span<const query::QueryRecord> current_records)
{
    if (plan.decisions.size() != current_records.size()) {
        return false;
    }
    for (base::usize index = 0; index < current_records.size(); ++index) {
        const query::QueryReuseDecision& decision = plan.decisions[index];
        const query::QueryRecord& current = current_records[index];
        if (decision.key != current.key || decision.stable_key_bytes != current.stable_key_bytes) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] const query::QueryRecord* reusable_cached_record(
    const query::QueryContext& cached_context, const query::QueryRecord& current)
{
    const query::QueryNode* const node = cached_context.find(current.key);
    if (node == nullptr || node->status != query::QueryNodeStatus::done || node->record.key != current.key
        || node->record.stable_key_bytes != current.stable_key_bytes || node->record.result != current.result) {
        return nullptr;
    }
    return &node->record;
}

[[nodiscard]] QueryPruningGateResult query_pruning_fallback(
    const bool enabled, const std::span<const query::QueryRecord> current_records, const std::string_view fallback)
{
    return QueryPruningGateResult{
        enabled,
        false,
        QueryKindExecutionCounts{},
        query_record_counts_by_kind(current_records),
        fallback,
    };
}

[[nodiscard]] QueryPruningGateResult apply_query_pruning_gate(const CompilerInvocation& invocation,
    const QueryReuseEvaluation& evaluation, const std::span<const query::QueryRecord> current_records)
{
    if (!invocation.experimental_query_pruning) {
        return query_pruning_fallback(false, current_records, INCREMENTAL_CACHE_PRUNING_FALLBACK_DISABLED);
    }
    if (!evaluation.cache_loaded) {
        return query_pruning_fallback(true, current_records, INCREMENTAL_CACHE_PRUNING_FALLBACK_NO_CACHE);
    }
    if (!query_reuse_plan_matches_records(evaluation.plan, current_records)) {
        return query_pruning_fallback(true, current_records, INCREMENTAL_CACHE_PRUNING_FALLBACK_INCOMPLETE_PLAN);
    }

    QueryPruningGateResult result;
    result.enabled = true;
    result.applied = true;
    result.fallback = INCREMENTAL_CACHE_PRUNING_FALLBACK_NONE;
    for (const query::QueryRecord& current : current_records) {
        if (contains_query_key(evaluation.plan.reusable, current.key)) {
            const query::QueryRecord* const cached = reusable_cached_record(evaluation.cached_context, current);
            if (cached == nullptr) {
                return query_pruning_fallback(
                    true, current_records, INCREMENTAL_CACHE_PRUNING_FALLBACK_MISSING_REUSABLE_RECORD);
            }
            increment_query_kind_count(result.reused, current.key.kind);
            continue;
        }
        increment_query_kind_count(result.recomputed, current.key.kind);
    }
    return result;
}

[[nodiscard]] bool cache_sources_match(const ParsedCache& cache)
{
    bool has_root_source = false;
    for (const SourceFingerprintRecord& cached_source : cache.sources) {
        if (cached_source.path == cache.root_path) {
            has_root_source = true;
        }
        std::optional<SourceFingerprintRecord> current = fingerprint_file(cached_source.path);
        if (!current || !same_fingerprint(cached_source, *current)) {
            return false;
        }
    }
    return has_root_source;
}

[[nodiscard]] std::vector<std::filesystem::path> normalized_import_paths(const CompilerInvocation& invocation)
{
    std::vector<std::filesystem::path> paths;
    paths.reserve(invocation.import_paths.size());
    for (const std::filesystem::path& path : invocation.import_paths) {
        paths.push_back(canonical_or_absolute(path));
    }
    return paths;
}

[[nodiscard]] std::vector<SourceFingerprintRecord> collect_source_fingerprints(const base::SourceManager& sources)
{
    std::vector<SourceFingerprintRecord> records;
    records.reserve(sources.files().size());
    for (const base::SourceFile& file : sources.files()) {
        records.push_back(SourceFingerprintRecord{
            canonical_or_absolute(std::filesystem::path(file.path())),
            file.text().size(),
            fingerprint_text(file.text()),
        });
    }
    std::sort(
        records.begin(), records.end(), [](const SourceFingerprintRecord& lhs, const SourceFingerprintRecord& rhs) {
            return lhs.path.string() < rhs.path.string();
        });
    return records;
}

[[nodiscard]] std::vector<ModuleRecord> sorted_modules(const std::span<const ModuleRecord> modules)
{
    std::vector<ModuleRecord> sorted;
    sorted.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        sorted.push_back(ModuleRecord{module.name, canonical_or_absolute(module.path)});
    }
    std::sort(sorted.begin(), sorted.end(), [](const ModuleRecord& lhs, const ModuleRecord& rhs) {
        if (lhs.name != rhs.name) {
            return lhs.name < rhs.name;
        }
        return lhs.path.string() < rhs.path.string();
    });
    return sorted;
}

[[nodiscard]] std::string_view stable_symbol_kind_name(const sema::StableSymbolKind kind) noexcept
{
    static constexpr auto NAMES = std::to_array<std::string_view>({
        "invalid",
        "type",
        "function",
        "method",
        "value",
        "enum_case",
        "struct_field",
        "generic_template",
        "synthetic",
    });
    const base::usize index = static_cast<base::usize>(kind);
    return index < NAMES.size() ? NAMES[index] : NAMES.front();
}

[[nodiscard]] std::string_view query_kind_name(const query::QueryKind kind) noexcept
{
    for (const QueryKindCacheName& entry : INCREMENTAL_CACHE_QUERY_KIND_NAMES) {
        if (entry.kind == kind) {
            return entry.name;
        }
    }
    return {};
}

[[nodiscard]] std::vector<std::string_view> module_name_parts(const std::string_view module_name)
{
    std::vector<std::string_view> parts;
    base::usize begin = 0;
    while (begin < module_name.size()) {
        const base::usize end = module_name.find(INCREMENTAL_CACHE_MODULE_NAME_SEPARATOR, begin);
        if (end == std::string_view::npos) {
            parts.push_back(module_name.substr(begin));
            break;
        }
        parts.push_back(module_name.substr(begin, end - begin));
        begin = end + 1;
    }
    return parts;
}

[[nodiscard]] sema::StableModuleId stable_module_id_from_record(const ModuleRecord& module)
{
    const std::vector<std::string_view> parts = module_name_parts(module.name);
    return sema::stable_module_id(std::span<const std::string_view>{parts.data(), parts.size()});
}

[[nodiscard]] bool module_exports_signature_entry_less(
    const ModuleExportsSignatureEntry& lhs, const ModuleExportsSignatureEntry& rhs)
{
    return std::tie(lhs.category, lhs.name, lhs.stable_id.global_id, lhs.stable_id.name.primary,
               lhs.stable_id.name.secondary, lhs.stable_id.name.byte_count, lhs.stable_id.disambiguator,
               lhs.stable_id.kind, lhs.incremental_key.global_id, lhs.incremental_key.fingerprint.primary,
               lhs.incremental_key.fingerprint.secondary, lhs.incremental_key.fingerprint.byte_count)
        < std::tie(rhs.category, rhs.name, rhs.stable_id.global_id, rhs.stable_id.name.primary,
            rhs.stable_id.name.secondary, rhs.stable_id.name.byte_count, rhs.stable_id.disambiguator,
            rhs.stable_id.kind, rhs.incremental_key.global_id, rhs.incremental_key.fingerprint.primary,
            rhs.incremental_key.fingerprint.secondary, rhs.incremental_key.fingerprint.byte_count);
}

void push_module_exports_signature_entry(std::vector<ModuleExportsSignatureEntry>& entries,
    const std::string_view category, const std::string_view name, const sema::StableDefId& stable_id,
    const sema::IncrementalKey& incremental_key, const syntax::Visibility visibility,
    const sema::StableModuleId& module)
{
    if (visibility != syntax::Visibility::public_ || stable_id.module != module || !query::is_valid(stable_id)
        || !query::is_valid(incremental_key)) {
        return;
    }
    entries.push_back(ModuleExportsSignatureEntry{
        std::string(category),
        std::string(name),
        stable_id,
        incremental_key,
    });
}

[[nodiscard]] std::vector<ModuleExportsSignatureEntry> collect_module_exports_signature_entries(
    const sema::CheckedModule& checked, const sema::StableModuleId& module)
{
    std::vector<ModuleExportsSignatureEntry> entries;
    entries.reserve(
        checked.functions.size() + checked.structs.size() + checked.enum_cases.size() + checked.type_aliases.size());

    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_FUNCTION, signature.name.view(),
            signature.stable_id, signature.incremental_key, signature.visibility, module);
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_STRUCT, info.name.view(),
            info.stable_id, info.incremental_key, info.visibility, module);
    }
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_ENUM_CASE, info.name.view(),
            info.stable_id, info.incremental_key, info.visibility, module);
    }
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& info = entry.second;
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_TYPE_ALIAS, info.name.view(),
            info.stable_id, info.incremental_key, info.visibility, module);
    }

    std::sort(entries.begin(), entries.end(), module_exports_signature_entry_less);
    return entries;
}

[[nodiscard]] query::QueryResultFingerprint module_exports_result_fingerprint(
    const query::ModuleKey key, const std::vector<ModuleExportsSignatureEntry>& entries)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_MODULE_EXPORTS_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(static_cast<base::u64>(entries.size()));
    for (base::usize index = 0; index < entries.size(); ++index) {
        const ModuleExportsSignatureEntry& entry = entries[index];
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_string(entry.category);
        builder.mix_string(entry.name);
        builder.mix_fingerprint(query::stable_key_fingerprint(entry.stable_id));
        builder.mix_u64(entry.incremental_key.global_id);
        builder.mix_fingerprint(entry.incremental_key.fingerprint);
    }
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::DefKind function_signature_def_kind(const sema::FunctionSignature& signature) noexcept
{
    return signature.is_method ? query::DefKind::method : query::DefKind::function;
}

[[nodiscard]] std::optional<std::string_view> source_range_text(
    const base::SourceManager& sources, const base::SourceRange& range) noexcept
{
    const std::span<const base::SourceFile> files = sources.files();
    if (range.source.value >= files.size()) {
        return std::nullopt;
    }
    const std::string_view text = files[range.source.value].text();
    if (range.begin > range.end || range.end > text.size()) {
        return std::nullopt;
    }
    return text.substr(range.begin, range.end - range.begin);
}

[[nodiscard]] query::BodyKey function_body_key(const sema::FunctionSignature& signature) noexcept
{
    return query::body_key(query::def_key_from_stable_id(
                               signature.stable_id, query::DefNamespace::value, function_signature_def_kind(signature)),
        query::BodySlotKind::function_body);
}

[[nodiscard]] query::QueryResultFingerprint function_body_syntax_result_fingerprint(
    const query::BodyKey key, const std::string_view body_text)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_FUNCTION_BODY_SYNTAX_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_string(body_text);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint type_check_body_result_fingerprint(const query::BodyKey key,
    const query::QueryResultFingerprint body_syntax_result, const sema::IncrementalKey& signature_key)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_TYPE_CHECK_BODY_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(body_syntax_result.global_id);
    builder.mix_fingerprint(body_syntax_result.fingerprint);
    builder.mix_u64(signature_key.global_id);
    builder.mix_fingerprint(signature_key.fingerprint);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint generic_instance_body_result_fingerprint(
    const query::GenericInstanceKey& key, const sema::IncrementalKey& signature_key, const std::string_view body_text)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_GENERIC_INSTANCE_BODY_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(signature_key.global_id);
    builder.mix_fingerprint(signature_key.fingerprint);
    builder.mix_string(body_text);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint lower_function_ir_result_fingerprint(
    const query::BodyKey key, const query::QueryResultFingerprint type_check_result)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_LOWER_FUNCTION_IR_RESULT_MARKER);
    builder.mix_string("body");
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(type_check_result.global_id);
    builder.mix_fingerprint(type_check_result.fingerprint);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint lower_generic_instance_ir_result_fingerprint(
    const query::GenericInstanceKey& key, const query::QueryResultFingerprint generic_body_result)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_LOWER_FUNCTION_IR_RESULT_MARKER);
    builder.mix_string("generic-instance");
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(generic_body_result.global_id);
    builder.mix_fingerprint(generic_body_result.fingerprint);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint diagnostics_result_fingerprint(const query::QueryKey producer)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_DIAGNOSTICS_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(producer));
    return query::query_result_fingerprint(builder.finish());
}

void push_definition(std::vector<DefinitionRecord>& records, const std::string_view category,
    const std::string_view name, const sema::StableDefId& stable_id, const sema::IncrementalKey& incremental_key)
{
    records.push_back(DefinitionRecord{
        std::string(category),
        std::string(name),
        stable_id,
        incremental_key,
    });
}

void push_item_signature_query_subject(std::vector<ItemSignatureQuerySubject>& subjects,
    const sema::StableDefId& stable_id, const sema::IncrementalKey& incremental_key,
    const query::DefNamespace name_space, const query::DefKind kind)
{
    subjects.push_back(ItemSignatureQuerySubject{
        stable_id,
        incremental_key,
        name_space,
        kind,
    });
}

void push_generic_instance_signature_query_subject(std::vector<GenericInstanceSignatureQuerySubject>& subjects,
    const query::GenericInstanceKey& key, const sema::IncrementalKey& incremental_key)
{
    subjects.push_back(GenericInstanceSignatureQuerySubject{
        &key,
        incremental_key,
    });
}

void push_generic_instance_body_query_subject(std::vector<GenericInstanceBodyQuerySubject>& subjects,
    const sema::GenericFunctionInstanceInfo& instance, const base::SourceManager& sources)
{
    if (!query::is_valid(instance.generic_instance_key) || !query::is_valid(instance.signature.incremental_key)
        || !instance.signature.has_definition || instance.signature.has_conflict) {
        return;
    }
    const std::optional<std::string_view> body_text = source_range_text(sources, instance.signature.range);
    if (!body_text) {
        return;
    }
    const query::QueryResultFingerprint result = generic_instance_body_result_fingerprint(
        instance.generic_instance_key, instance.signature.incremental_key, *body_text);
    if (!query::is_valid(result)) {
        return;
    }
    subjects.push_back(GenericInstanceBodyQuerySubject{
        &instance.generic_instance_key,
        result,
    });
}

void push_lower_function_ir_query_subject(
    std::vector<LowerFunctionIRQuerySubject>& subjects, const TypeCheckBodyQuerySubject& type_check_subject)
{
    const query::QueryResultFingerprint result =
        lower_function_ir_result_fingerprint(type_check_subject.key, type_check_subject.result);
    subjects.push_back(LowerFunctionIRQuerySubject{
        LowerFunctionIRSubjectKind::body,
        type_check_subject.key,
        nullptr,
        result,
    });
}

void push_lower_generic_instance_ir_query_subject(
    std::vector<LowerFunctionIRQuerySubject>& subjects, const GenericInstanceBodyQuerySubject& generic_body_subject)
{
    if (generic_body_subject.key == nullptr) {
        return;
    }
    const query::QueryResultFingerprint result =
        lower_generic_instance_ir_result_fingerprint(*generic_body_subject.key, generic_body_subject.result);
    subjects.push_back(LowerFunctionIRQuerySubject{
        LowerFunctionIRSubjectKind::generic_instance,
        {},
        generic_body_subject.key,
        result,
    });
}

void push_function_body_query_subjects(std::vector<FunctionBodySyntaxQuerySubject>& syntax_subjects,
    std::vector<TypeCheckBodyQuerySubject>& type_check_subjects, const sema::FunctionSignature& signature,
    const base::SourceManager& sources)
{
    if (!signature.has_definition || signature.has_conflict || !query::is_valid(signature.stable_id)
        || !query::is_valid(signature.incremental_key)) {
        return;
    }
    const query::BodyKey key = function_body_key(signature);
    if (!query::is_valid(key)) {
        return;
    }
    const std::optional<std::string_view> body_text = source_range_text(sources, signature.range);
    if (!body_text) {
        return;
    }
    const query::QueryResultFingerprint syntax_result = function_body_syntax_result_fingerprint(key, *body_text);
    if (!query::is_valid(syntax_result)) {
        return;
    }
    syntax_subjects.push_back(FunctionBodySyntaxQuerySubject{
        key,
        syntax_result,
    });
    type_check_subjects.push_back(TypeCheckBodyQuerySubject{
        key,
        type_check_body_result_fingerprint(key, syntax_result, signature.incremental_key),
    });
}

void push_module_exports_query_subject(
    std::vector<ModuleExportsQuerySubject>& subjects, const ModuleRecord& module, const sema::CheckedModule& checked)
{
    const sema::StableModuleId stable_module = stable_module_id_from_record(module);
    const query::ModuleKey key = query::module_key_from_stable_id(stable_module);
    if (!query::is_valid(stable_module) || !query::is_valid(key)) {
        return;
    }

    const std::vector<ModuleExportsSignatureEntry> entries =
        collect_module_exports_signature_entries(checked, stable_module);
    subjects.push_back(ModuleExportsQuerySubject{
        key,
        module_exports_result_fingerprint(key, entries),
    });
}

void evaluate_module_exports_query_subject(query::QueryContext& context, const ModuleExportsQuerySubject& subject)
{
    const query::ModuleExportsProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_module_exports(input));
}

void evaluate_item_signature_query_subject(query::QueryContext& context, const ItemSignatureQuerySubject& subject)
{
    const query::ItemSignatureProviderInput input{
        query::def_key_from_stable_id(subject.stable_id, subject.name_space, subject.kind),
        subject.incremental_key,
    };
    static_cast<void>(context.evaluate_item_signature(input));
}

void evaluate_generic_instance_signature_query_subject(
    query::QueryContext& context, const GenericInstanceSignatureQuerySubject& subject)
{
    const query::GenericInstanceSignatureProviderInput input{
        subject.key,
        subject.incremental_key,
    };
    static_cast<void>(context.evaluate_generic_instance_signature(input));
}

void evaluate_generic_instance_body_query_subject(
    query::QueryContext& context, const GenericInstanceBodyQuerySubject& subject)
{
    const query::GenericInstanceBodyProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_generic_instance_body(input));
}

void evaluate_lower_function_ir_query_subject(query::QueryContext& context, const LowerFunctionIRQuerySubject& subject)
{
    switch (subject.kind) {
        case LowerFunctionIRSubjectKind::body: {
            const query::LowerFunctionIRProviderInput input{
                subject.body,
                subject.result,
            };
            static_cast<void>(context.evaluate_lower_function_ir(input));
            return;
        }
        case LowerFunctionIRSubjectKind::generic_instance: {
            const query::LowerGenericInstanceIRProviderInput input{
                subject.generic_instance,
                subject.result,
            };
            static_cast<void>(context.evaluate_lower_generic_instance_ir(input));
            return;
        }
    }
}

void evaluate_function_body_syntax_query_subject(
    query::QueryContext& context, const FunctionBodySyntaxQuerySubject& subject)
{
    const query::FunctionBodySyntaxProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_function_body_syntax(input));
}

void evaluate_type_check_body_query_subject(query::QueryContext& context, const TypeCheckBodyQuerySubject& subject)
{
    const query::TypeCheckBodyProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_type_check_body(input));
}

void evaluate_diagnostics_query_subject(query::QueryContext& context, const DiagnosticsQuerySubject& subject)
{
    const query::DiagnosticsProviderInput input{
        subject.producer,
        subject.result,
    };
    static_cast<void>(context.evaluate_diagnostics(input));
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ModuleExportsQuerySubject& subject)
{
    return query::module_exports_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ItemSignatureQuerySubject& subject)
{
    const query::DefKey key = query::def_key_from_stable_id(subject.stable_id, subject.name_space, subject.kind);
    return query::item_signature_query_record(key, query::query_result_fingerprint(subject.incremental_key));
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(
    const GenericInstanceSignatureQuerySubject& subject)
{
    if (subject.key == nullptr) {
        return std::nullopt;
    }
    return query::generic_instance_signature_query_record(
        *subject.key, query::query_result_fingerprint(subject.incremental_key));
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const GenericInstanceBodyQuerySubject& subject)
{
    if (subject.key == nullptr) {
        return std::nullopt;
    }
    return query::generic_instance_body_query_record(*subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const LowerFunctionIRQuerySubject& subject)
{
    switch (subject.kind) {
        case LowerFunctionIRSubjectKind::body:
            return query::lower_function_ir_query_record(subject.body, subject.result);
        case LowerFunctionIRSubjectKind::generic_instance:
            if (subject.generic_instance == nullptr) {
                return std::nullopt;
            }
            return query::lower_generic_instance_ir_query_record(*subject.generic_instance, subject.result);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const FunctionBodySyntaxQuerySubject& subject)
{
    return query::function_body_syntax_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const TypeCheckBodyQuerySubject& subject)
{
    return query::type_check_body_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const DiagnosticsQuerySubject& subject)
{
    return query::diagnostics_query_record(subject.producer, subject.result);
}

void push_query_subject(std::vector<QuerySubject>& subjects,
    std::unordered_set<query::QueryKey, query::QueryKeyHash>& keys, const QuerySubjectKind kind,
    const base::usize index, std::optional<query::QueryRecord> record)
{
    if (!record) {
        return;
    }
    const auto inserted = keys.insert(record->key);
    if (!inserted.second) {
        return;
    }
    subjects.push_back(QuerySubject{
        kind,
        index,
        std::move(*record),
    });
}

void collect_diagnostics_query_subjects(QuerySubjectCollection& collection)
{
    collection.diagnostics.reserve(collection.subjects.size());
    for (const QuerySubject& subject : collection.subjects) {
        const query::QueryResultFingerprint result = diagnostics_result_fingerprint(subject.record.key);
        if (!query::is_valid(result)) {
            continue;
        }
        collection.diagnostics.push_back(DiagnosticsQuerySubject{
            subject.record.key,
            result,
        });
    }
}

void build_ordered_query_subjects(QuerySubjectCollection& collection)
{
    collection.subjects.reserve(collection.module_exports.size() + collection.item_signatures.size()
        + collection.function_body_syntaxes.size() + collection.type_check_bodies.size()
        + collection.generic_instance_signatures.size() + collection.generic_instance_bodies.size()
        + collection.lower_function_irs.size());
    std::unordered_set<query::QueryKey, query::QueryKeyHash> keys;
    keys.reserve(collection.module_exports.size() + collection.item_signatures.size()
        + collection.function_body_syntaxes.size() + collection.type_check_bodies.size()
        + collection.generic_instance_signatures.size() + collection.generic_instance_bodies.size()
        + collection.lower_function_irs.size());

    for (base::usize index = 0; index < collection.module_exports.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::module_exports, index,
            query_record_for_subject(collection.module_exports[index]));
    }
    for (base::usize index = 0; index < collection.item_signatures.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::item_signature, index,
            query_record_for_subject(collection.item_signatures[index]));
    }
    for (base::usize index = 0; index < collection.function_body_syntaxes.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::function_body_syntax, index,
            query_record_for_subject(collection.function_body_syntaxes[index]));
    }
    for (base::usize index = 0; index < collection.type_check_bodies.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::type_check_body, index,
            query_record_for_subject(collection.type_check_bodies[index]));
    }
    for (base::usize index = 0; index < collection.generic_instance_signatures.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::generic_instance_signature, index,
            query_record_for_subject(collection.generic_instance_signatures[index]));
    }
    for (base::usize index = 0; index < collection.generic_instance_bodies.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::generic_instance_body, index,
            query_record_for_subject(collection.generic_instance_bodies[index]));
    }
    for (base::usize index = 0; index < collection.lower_function_irs.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::lower_function_ir, index,
            query_record_for_subject(collection.lower_function_irs[index]));
    }

    collect_diagnostics_query_subjects(collection);
    collection.subjects.reserve(collection.subjects.size() + collection.diagnostics.size());
    keys.reserve(keys.size() + collection.diagnostics.size());
    for (base::usize index = 0; index < collection.diagnostics.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::diagnostics, index,
            query_record_for_subject(collection.diagnostics[index]));
    }

    std::sort(
        collection.subjects.begin(), collection.subjects.end(), [](const QuerySubject& lhs, const QuerySubject& rhs) {
            return query_record_key_less(lhs.record, rhs.record);
        });

    collection.records.reserve(collection.subjects.size());
    for (const QuerySubject& subject : collection.subjects) {
        collection.records.push_back(subject.record);
    }
}

[[nodiscard]] std::vector<ModuleExportsQuerySubject> collect_module_exports_query_subjects(
    const std::span<const ModuleRecord> modules, const sema::CheckedModule& checked)
{
    std::vector<ModuleExportsQuerySubject> subjects;
    subjects.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        push_module_exports_query_subject(subjects, module, checked);
    }
    return subjects;
}

[[nodiscard]] std::vector<ItemSignatureQuerySubject> collect_item_signature_query_subjects(
    const sema::CheckedModule& checked)
{
    std::vector<ItemSignatureQuerySubject> subjects;
    subjects.reserve(
        checked.functions.size() + checked.structs.size() + checked.enum_cases.size() + checked.type_aliases.size());

    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        push_item_signature_query_subject(subjects, signature.stable_id, signature.incremental_key,
            query::DefNamespace::value, function_signature_def_kind(signature));
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        push_item_signature_query_subject(
            subjects, info.stable_id, info.incremental_key, query::DefNamespace::type, query::DefKind::struct_);
    }
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        push_item_signature_query_subject(
            subjects, info.stable_id, info.incremental_key, query::DefNamespace::value, query::DefKind::enum_case);
    }
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& info = entry.second;
        push_item_signature_query_subject(
            subjects, info.stable_id, info.incremental_key, query::DefNamespace::type, query::DefKind::type_alias);
    }
    return subjects;
}

[[nodiscard]] std::vector<GenericInstanceSignatureQuerySubject> collect_generic_instance_signature_query_subjects(
    const sema::CheckedModule& checked)
{
    std::vector<GenericInstanceSignatureQuerySubject> subjects;
    subjects.reserve(checked.generic_function_instances.size() + checked.structs.size());

    for (const sema::GenericFunctionInstanceInfo& instance : checked.generic_function_instances) {
        push_generic_instance_signature_query_subject(
            subjects, instance.generic_instance_key, instance.signature.incremental_key);
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        push_generic_instance_signature_query_subject(subjects, info.generic_instance_key, info.incremental_key);
    }
    return subjects;
}

[[nodiscard]] std::vector<GenericInstanceBodyQuerySubject> collect_generic_instance_body_query_subjects(
    const sema::CheckedModule& checked, const base::SourceManager& sources)
{
    std::vector<GenericInstanceBodyQuerySubject> subjects;
    subjects.reserve(checked.generic_function_instances.size());
    for (const sema::GenericFunctionInstanceInfo& instance : checked.generic_function_instances) {
        push_generic_instance_body_query_subject(subjects, instance, sources);
    }
    return subjects;
}

void collect_function_body_query_subjects(const sema::CheckedModule& checked, const base::SourceManager& sources,
    std::vector<FunctionBodySyntaxQuerySubject>& syntax_subjects,
    std::vector<TypeCheckBodyQuerySubject>& type_check_subjects)
{
    syntax_subjects.reserve(checked.functions.size());
    type_check_subjects.reserve(checked.functions.size());
    for (const auto& entry : checked.functions) {
        push_function_body_query_subjects(syntax_subjects, type_check_subjects, entry.second, sources);
    }
}

[[nodiscard]] std::vector<LowerFunctionIRQuerySubject> collect_lower_function_ir_query_subjects(
    const std::vector<TypeCheckBodyQuerySubject>& type_check_subjects,
    const std::vector<GenericInstanceBodyQuerySubject>& generic_body_subjects)
{
    std::vector<LowerFunctionIRQuerySubject> subjects;
    subjects.reserve(type_check_subjects.size() + generic_body_subjects.size());
    for (const TypeCheckBodyQuerySubject& type_check_subject : type_check_subjects) {
        push_lower_function_ir_query_subject(subjects, type_check_subject);
    }
    for (const GenericInstanceBodyQuerySubject& generic_body_subject : generic_body_subjects) {
        push_lower_generic_instance_ir_query_subject(subjects, generic_body_subject);
    }
    return subjects;
}

[[nodiscard]] QuerySubjectCollection collect_query_subjects(
    const std::span<const ModuleRecord> modules, const sema::CheckedModule& checked, const base::SourceManager& sources)
{
    QuerySubjectCollection collection;
    collection.module_exports = collect_module_exports_query_subjects(modules, checked);
    collection.item_signatures = collect_item_signature_query_subjects(checked);
    collect_function_body_query_subjects(
        checked, sources, collection.function_body_syntaxes, collection.type_check_bodies);
    collection.generic_instance_signatures = collect_generic_instance_signature_query_subjects(checked);
    collection.generic_instance_bodies = collect_generic_instance_body_query_subjects(checked, sources);
    collection.lower_function_irs =
        collect_lower_function_ir_query_subjects(collection.type_check_bodies, collection.generic_instance_bodies);
    build_ordered_query_subjects(collection);
    return collection;
}

void evaluate_query_subject(
    query::QueryContext& context, const QuerySubjectCollection& collection, const QuerySubject& subject)
{
    switch (subject.kind) {
        case QuerySubjectKind::module_exports:
            evaluate_module_exports_query_subject(context, collection.module_exports[subject.index]);
            return;
        case QuerySubjectKind::item_signature:
            evaluate_item_signature_query_subject(context, collection.item_signatures[subject.index]);
            return;
        case QuerySubjectKind::function_body_syntax:
            evaluate_function_body_syntax_query_subject(context, collection.function_body_syntaxes[subject.index]);
            return;
        case QuerySubjectKind::type_check_body:
            evaluate_type_check_body_query_subject(context, collection.type_check_bodies[subject.index]);
            return;
        case QuerySubjectKind::generic_instance_signature:
            evaluate_generic_instance_signature_query_subject(
                context, collection.generic_instance_signatures[subject.index]);
            return;
        case QuerySubjectKind::generic_instance_body:
            evaluate_generic_instance_body_query_subject(context, collection.generic_instance_bodies[subject.index]);
            return;
        case QuerySubjectKind::lower_function_ir:
            evaluate_lower_function_ir_query_subject(context, collection.lower_function_irs[subject.index]);
            return;
        case QuerySubjectKind::diagnostics:
            evaluate_diagnostics_query_subject(context, collection.diagnostics[subject.index]);
            return;
    }
}

void increment_query_kind_count(QueryKindExecutionCounts& counts, const QuerySubjectKind kind) noexcept
{
    counts.total += 1;
    switch (kind) {
        case QuerySubjectKind::module_exports:
            counts.module_exports += 1;
            return;
        case QuerySubjectKind::item_signature:
            counts.item_signatures += 1;
            return;
        case QuerySubjectKind::function_body_syntax:
            counts.function_body_syntaxes += 1;
            return;
        case QuerySubjectKind::type_check_body:
            counts.type_check_bodies += 1;
            return;
        case QuerySubjectKind::generic_instance_signature:
            counts.generic_instance_signatures += 1;
            return;
        case QuerySubjectKind::generic_instance_body:
            counts.generic_instance_bodies += 1;
            return;
        case QuerySubjectKind::lower_function_ir:
            counts.lower_function_irs += 1;
            return;
        case QuerySubjectKind::diagnostics:
            counts.diagnostics += 1;
            return;
    }
}

void evaluate_query_subjects(
    query::QueryContext& context, const QuerySubjectCollection& collection, QueryProviderEvaluationStats& stats)
{
    for (const QuerySubject& subject : collection.subjects) {
        evaluate_query_subject(context, collection, subject);
        increment_query_kind_count(stats.evaluated, subject.kind);
    }
}

void evaluate_recomputed_query_subjects(query::QueryContext& context, const QuerySubjectCollection& collection,
    const query::QueryReusePlan& plan, QueryProviderEvaluationStats& stats)
{
    for (const QuerySubject& subject : collection.subjects) {
        if (contains_query_key(plan.recompute, subject.record.key)) {
            evaluate_query_subject(context, collection, subject);
            increment_query_kind_count(stats.evaluated, subject.kind);
        }
    }
}

[[nodiscard]] bool seed_reusable_query_subjects(query::QueryContext& context, const QuerySubjectCollection& collection,
    const QueryReuseEvaluation& evaluation, QueryProviderEvaluationStats& stats)
{
    for (const QuerySubject& subject : collection.subjects) {
        if (!contains_query_key(evaluation.plan.reusable, subject.record.key)) {
            continue;
        }
        const query::QueryRecord* const cached = reusable_cached_record(evaluation.cached_context, subject.record);
        if (cached == nullptr) {
            return false;
        }
        if (!context.seed_completed_record(*cached, evaluation.cached_context.dependencies_for(cached->key))) {
            return false;
        }
        increment_query_kind_count(stats.seeded, subject.kind);
    }
    return true;
}

[[nodiscard]] QueryCollectionResult collect_queries_from_subjects(const QuerySubjectCollection& collection)
{
    query::QueryContext context;
    QueryProviderEvaluationStats stats;
    evaluate_query_subjects(context, collection, stats);
    return QueryCollectionResult{
        QueryCollection{
            context.completed_records(),
            context.dependency_edges(),
        },
        stats,
    };
}

[[nodiscard]] QueryCollectionResult collect_queries_from_pruned_subjects(
    const QuerySubjectCollection& collection, const QueryReuseEvaluation& evaluation)
{
    query::QueryContext context;
    QueryProviderEvaluationStats stats;
    stats.pruned = true;
    if (!seed_reusable_query_subjects(context, collection, evaluation, stats)) {
        return collect_queries_from_subjects(collection);
    }
    evaluate_recomputed_query_subjects(context, collection, evaluation.plan, stats);
    return QueryCollectionResult{
        QueryCollection{
            context.completed_records(),
            context.dependency_edges(),
        },
        stats,
    };
}

[[nodiscard]] std::vector<DefinitionRecord> collect_definitions(const sema::CheckedModule& checked)
{
    std::vector<DefinitionRecord> records;
    records.reserve(checked.functions.size() + checked.generic_function_instances.size() + checked.structs.size()
        + checked.enum_cases.size() + checked.type_aliases.size());

    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        push_definition(records, INCREMENTAL_CACHE_CATEGORY_FUNCTION, signature.name.view(), signature.stable_id,
            signature.incremental_key);
    }
    for (const sema::GenericFunctionInstanceInfo& instance : checked.generic_function_instances) {
        push_definition(records, INCREMENTAL_CACHE_CATEGORY_GENERIC_FUNCTION_INSTANCE, instance.signature.name.view(),
            instance.signature.stable_id, instance.signature.incremental_key);
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        push_definition(
            records, INCREMENTAL_CACHE_CATEGORY_STRUCT, info.name.view(), info.stable_id, info.incremental_key);
    }
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        push_definition(
            records, INCREMENTAL_CACHE_CATEGORY_ENUM_CASE, info.name.view(), info.stable_id, info.incremental_key);
    }
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& info = entry.second;
        push_definition(
            records, INCREMENTAL_CACHE_CATEGORY_TYPE_ALIAS, info.name.view(), info.stable_id, info.incremental_key);
    }

    std::sort(records.begin(), records.end(), [](const DefinitionRecord& lhs, const DefinitionRecord& rhs) {
        if (lhs.category != rhs.category) {
            return lhs.category < rhs.category;
        }
        if (lhs.name != rhs.name) {
            return lhs.name < rhs.name;
        }
        if (lhs.stable_id.global_id != rhs.stable_id.global_id) {
            return lhs.stable_id.global_id < rhs.stable_id.global_id;
        }
        return lhs.incremental_key.global_id < rhs.incremental_key.global_id;
    });
    return records;
}

void write_hex_field(std::ostream& out, const std::string_view value)
{
    append_hex_string(out, value);
}

void write_header_field(std::ostream& out, const std::string_view name, const std::string_view encoded_value)
{
    out << name << INCREMENTAL_CACHE_SEPARATOR << encoded_value << '\n';
}

void write_encoded_header_field(std::ostream& out, const std::string_view name, const std::string_view value)
{
    out << name << INCREMENTAL_CACHE_SEPARATOR;
    write_hex_field(out, value);
    out << '\n';
}

void write_source_record(std::ostream& out, const SourceFingerprintRecord& record)
{
    out << INCREMENTAL_CACHE_FIELD_SOURCE << INCREMENTAL_CACHE_SEPARATOR << record.size << INCREMENTAL_CACHE_SEPARATOR
        << record.fingerprint.primary << INCREMENTAL_CACHE_SEPARATOR << record.fingerprint.secondary
        << INCREMENTAL_CACHE_SEPARATOR << record.fingerprint.byte_count << INCREMENTAL_CACHE_SEPARATOR;
    write_hex_field(out, record.path.string());
    out << '\n';
}

void write_module_record(std::ostream& out, const ModuleRecord& record)
{
    out << INCREMENTAL_CACHE_FIELD_MODULE << INCREMENTAL_CACHE_SEPARATOR;
    write_hex_field(out, record.name);
    out << INCREMENTAL_CACHE_SEPARATOR;
    write_hex_field(out, record.path.string());
    out << '\n';
}

void write_definition_record(std::ostream& out, const DefinitionRecord& record)
{
    out << INCREMENTAL_CACHE_FIELD_DEF << INCREMENTAL_CACHE_SEPARATOR << record.category << INCREMENTAL_CACHE_SEPARATOR
        << stable_symbol_kind_name(record.stable_id.kind) << INCREMENTAL_CACHE_SEPARATOR << record.stable_id.global_id
        << INCREMENTAL_CACHE_SEPARATOR << record.stable_id.name.primary << INCREMENTAL_CACHE_SEPARATOR
        << record.stable_id.name.secondary << INCREMENTAL_CACHE_SEPARATOR << record.stable_id.name.byte_count
        << INCREMENTAL_CACHE_SEPARATOR << record.incremental_key.global_id << INCREMENTAL_CACHE_SEPARATOR
        << record.incremental_key.fingerprint.primary << INCREMENTAL_CACHE_SEPARATOR
        << record.incremental_key.fingerprint.secondary << INCREMENTAL_CACHE_SEPARATOR
        << record.incremental_key.fingerprint.byte_count << INCREMENTAL_CACHE_SEPARATOR;
    write_hex_field(out, record.name);
    out << '\n';
}

void write_query_record(std::ostream& out, const query::QueryRecord& record)
{
    out << INCREMENTAL_CACHE_FIELD_QUERY << INCREMENTAL_CACHE_SEPARATOR << query_kind_name(record.key.kind)
        << INCREMENTAL_CACHE_SEPARATOR << record.key.schema << INCREMENTAL_CACHE_SEPARATOR << record.key.global_id
        << INCREMENTAL_CACHE_SEPARATOR << record.key.payload.primary << INCREMENTAL_CACHE_SEPARATOR
        << record.key.payload.secondary << INCREMENTAL_CACHE_SEPARATOR << record.key.payload.byte_count
        << INCREMENTAL_CACHE_SEPARATOR << record.result.global_id << INCREMENTAL_CACHE_SEPARATOR
        << record.result.fingerprint.primary << INCREMENTAL_CACHE_SEPARATOR << record.result.fingerprint.secondary
        << INCREMENTAL_CACHE_SEPARATOR << record.result.fingerprint.byte_count << INCREMENTAL_CACHE_SEPARATOR;
    write_hex_field(out, record.stable_key_bytes);
    out << '\n';
}

void write_query_key_fields(std::ostream& out, const query::QueryKey key)
{
    out << query_kind_name(key.kind) << INCREMENTAL_CACHE_SEPARATOR << key.schema << INCREMENTAL_CACHE_SEPARATOR
        << key.global_id << INCREMENTAL_CACHE_SEPARATOR << key.payload.primary << INCREMENTAL_CACHE_SEPARATOR
        << key.payload.secondary << INCREMENTAL_CACHE_SEPARATOR << key.payload.byte_count;
}

void write_query_dependency_edge_record(std::ostream& out, const query::QueryDependencyEdge& edge)
{
    out << INCREMENTAL_CACHE_FIELD_QUERY_EDGE << INCREMENTAL_CACHE_SEPARATOR;
    write_query_key_fields(out, edge.dependent);
    out << INCREMENTAL_CACHE_SEPARATOR;
    write_query_key_fields(out, edge.dependency);
    out << '\n';
}

[[nodiscard]] std::filesystem::path temporary_cache_path(const std::filesystem::path& cache_path)
{
    std::filesystem::path temporary = cache_path;
    temporary += ".tmp.";
    temporary += std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return temporary;
}

[[nodiscard]] base::Result<void> publish_cache_file(
    const std::filesystem::path& temporary_path, const std::filesystem::path& cache_path)
{
    std::error_code rename_error;
    std::filesystem::rename(temporary_path, cache_path, rename_error);
    if (rename_error) {
        std::error_code remove_error;
        std::filesystem::remove(temporary_path, remove_error);
        return base::Result<void>::fail({base::ErrorCode::io_error, std::string(INCREMENTAL_CACHE_RENAME_FAILED)});
    }
    return base::Result<void>::ok();
}

} // namespace

base::Result<bool> try_reuse_incremental_check_cache(const CompilerInvocation& invocation)
{
    if (invocation.incremental_cache_path.empty() || invocation.emit_kind != EmitKind::check) {
        return base::Result<bool>::ok(false);
    }

    const std::optional<ParsedCache> cache = read_incremental_cache(invocation.incremental_cache_path);
    if (!cache) {
        return base::Result<bool>::ok(false);
    }
    if (!parsed_cache_header_matches(*cache, invocation) || !cache_sources_match(*cache)) {
        return base::Result<bool>::ok(false);
    }
    return base::Result<bool>::ok(true);
}

base::Result<void> write_incremental_cache(const CompilerInvocation& invocation, const base::SourceManager& sources,
    const std::span<const ModuleRecord> modules, const sema::CheckedModule& checked,
    CompilationProfiler* const profiler)
{
    if (invocation.incremental_cache_path.empty()) {
        return base::Result<void>::ok();
    }

    const std::filesystem::path cache_path = invocation.incremental_cache_path;
    const std::filesystem::path parent = cache_path.parent_path();
    if (!parent.empty()) {
        std::error_code directory_error;
        std::filesystem::create_directories(parent, directory_error);
        if (directory_error) {
            return base::Result<void>::fail(
                {base::ErrorCode::io_error, std::string(INCREMENTAL_CACHE_DIRECTORY_FAILED)});
        }
    }

    const std::vector<std::filesystem::path> imports = normalized_import_paths(invocation);
    const std::vector<SourceFingerprintRecord> source_records = collect_source_fingerprints(sources);
    const std::vector<ModuleRecord> module_records = sorted_modules(modules);
    const std::vector<DefinitionRecord> definition_records = collect_definitions(checked);
    const QuerySubjectCollection query_subjects = collect_query_subjects(module_records, checked, sources);
    const auto query_diff_started = std::chrono::steady_clock::now();
    const QueryReuseEvaluation query_reuse_evaluation =
        build_existing_query_reuse_evaluation(cache_path, query_subjects.records);
    const std::chrono::steady_clock::duration query_diff_elapsed =
        std::chrono::steady_clock::now() - query_diff_started;
    record_query_record_diff_summary(profiler, query_reuse_evaluation.plan.summary, query_diff_elapsed);
    record_query_reuse_plan_summary(profiler, query_reuse_evaluation.plan, query_diff_elapsed);

    const auto query_pruning_started = std::chrono::steady_clock::now();
    const QueryPruningGateResult query_pruning =
        apply_query_pruning_gate(invocation, query_reuse_evaluation, query_subjects.records);
    const std::chrono::steady_clock::duration query_pruning_elapsed =
        std::chrono::steady_clock::now() - query_pruning_started;
    record_query_pruning_summary(profiler, query_pruning, query_pruning_elapsed);
    const auto query_provider_eval_started = std::chrono::steady_clock::now();
    const QueryCollectionResult query_collection_result = query_pruning.applied
        ? collect_queries_from_pruned_subjects(query_subjects, query_reuse_evaluation)
        : collect_queries_from_subjects(query_subjects);
    const std::chrono::steady_clock::duration query_provider_eval_elapsed =
        std::chrono::steady_clock::now() - query_provider_eval_started;
    record_query_provider_evaluation_summary(profiler, query_collection_result.stats, query_provider_eval_elapsed);
    const QueryCollection& query_collection = query_collection_result.collection;

    const std::filesystem::path temporary_path = temporary_cache_path(cache_path);
    {
        std::ofstream out(temporary_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            return base::Result<void>::fail(
                {base::ErrorCode::io_error, std::string(INCREMENTAL_CACHE_WRITE_OPEN_FAILED)});
        }

        out << INCREMENTAL_CACHE_MAGIC << '\n';
        write_header_field(out, INCREMENTAL_CACHE_FIELD_SCHEMA, std::to_string(INCREMENTAL_CACHE_SCHEMA_VERSION));
        write_encoded_header_field(out, INCREMENTAL_CACHE_FIELD_COMPILER, base::config::AUREX_VERSION_STRING);
        write_encoded_header_field(out, INCREMENTAL_CACHE_FIELD_MODE, INCREMENTAL_CACHE_MODE_SEMANTIC_OK);
        write_encoded_header_field(
            out, INCREMENTAL_CACHE_FIELD_ROOT, canonical_or_absolute(invocation.input_path).string());
        write_header_field(out, INCREMENTAL_CACHE_FIELD_IMPORT_PATHS, std::to_string(imports.size()));
        for (const std::filesystem::path& import_path : imports) {
            out << INCREMENTAL_CACHE_FIELD_IMPORT_PATH << INCREMENTAL_CACHE_SEPARATOR;
            write_hex_field(out, import_path.string());
            out << '\n';
        }
        write_header_field(out, INCREMENTAL_CACHE_FIELD_SOURCES, std::to_string(source_records.size()));
        for (const SourceFingerprintRecord& record : source_records) {
            write_source_record(out, record);
        }
        write_header_field(out, INCREMENTAL_CACHE_FIELD_MODULES, std::to_string(module_records.size()));
        for (const ModuleRecord& record : module_records) {
            write_module_record(out, record);
        }
        write_header_field(out, INCREMENTAL_CACHE_FIELD_DEFINITIONS, std::to_string(definition_records.size()));
        for (const DefinitionRecord& record : definition_records) {
            write_definition_record(out, record);
        }
        write_header_field(out, INCREMENTAL_CACHE_FIELD_QUERIES, std::to_string(query_collection.records.size()));
        for (const query::QueryRecord& record : query_collection.records) {
            write_query_record(out, record);
        }
        write_header_field(
            out, INCREMENTAL_CACHE_FIELD_QUERY_EDGES, std::to_string(query_collection.dependency_edges.size()));
        for (const query::QueryDependencyEdge& edge : query_collection.dependency_edges) {
            write_query_dependency_edge_record(out, edge);
        }

        if (!out) {
            return base::Result<void>::fail({base::ErrorCode::io_error, std::string(INCREMENTAL_CACHE_WRITE_FAILED)});
        }
    }

    return publish_cache_file(temporary_path, cache_path);
}

} // namespace aurex::driver
