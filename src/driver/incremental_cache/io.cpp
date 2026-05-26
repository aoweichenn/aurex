#include "io.hpp"

#include <aurex/base/config.hpp>
#include <aurex/driver/package_identity.hpp>
#include <aurex/query/query_edge_verifier.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "schedule.hpp"

namespace aurex::driver::incremental_cache_detail {
namespace cache_format = incremental_cache_format;
using namespace cache_format;

namespace {

void append_hex_string(std::ostream& out, const std::string_view value)
{
    static constexpr char DIGITS[] = "0123456789abcdef";
    for (const unsigned char byte : value) {
        out << DIGITS[byte >> INCREMENTAL_CACHE_HEX_BYTE_SHIFT] << DIGITS[byte & INCREMENTAL_CACHE_HEX_LOW_MASK];
    }
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
    if (kind == INCREMENTAL_CACHE_FIELD_PACKAGE) {
        std::optional<std::string> package_identity = decode_hex_string(value);
        if (!package_identity || cache.package_identity.has_value()) {
            return false;
        }
        cache.package_identity = std::move(*package_identity);
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
    if ((fields.size() != INCREMENTAL_CACHE_SOURCE_LEGACY_FIELD_COUNT
            && fields.size() != INCREMENTAL_CACHE_SOURCE_FIELD_COUNT)
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
    if (fields.size() == INCREMENTAL_CACHE_SOURCE_FIELD_COUNT) {
        base::u64 package_global = 0;
        base::u64 package_primary = 0;
        base::u64 package_secondary = 0;
        base::u32 package_bytes = 0;
        if (!parse_u64(fields[INCREMENTAL_CACHE_SOURCE_PACKAGE_GLOBAL_FIELD], package_global)
            || !parse_u64(fields[INCREMENTAL_CACHE_SOURCE_PACKAGE_PRIMARY_FIELD], package_primary)
            || !parse_u64(fields[INCREMENTAL_CACHE_SOURCE_PACKAGE_SECONDARY_FIELD], package_secondary)
            || !parse_u32(fields[INCREMENTAL_CACHE_SOURCE_PACKAGE_BYTES_FIELD], package_bytes) || package_global == 0) {
            return false;
        }
        record.package = query::PackageKey{
            query::StableFingerprint128{
                package_primary,
                package_secondary,
                package_bytes,
            },
            package_global,
        };
    }
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

[[nodiscard]] ParsedCacheValidationStatus parsed_cache_query_records_validation_status(const ParsedCache& cache);
[[nodiscard]] ParsedCacheValidationStatus parsed_cache_query_edges_validation_status(const ParsedCache& cache);

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

struct QueryKeyFieldLayout {
    base::usize kind = 0;
    base::usize schema = 0;
    base::usize global = 0;
    base::usize payload_primary = 0;
    base::usize payload_secondary = 0;
    base::usize payload_bytes = 0;
};

[[nodiscard]] std::optional<query::QueryKey> parse_query_key_fields(
    const std::vector<std::string_view>& fields, const QueryKeyFieldLayout& layout)
{
    const std::optional<query::QueryKind> query_kind = parse_query_kind_name(fields[layout.kind]);
    if (!query_kind.has_value()) {
        return std::nullopt;
    }

    base::u32 schema = 0;
    base::u64 query_global = 0;
    base::u64 payload_primary = 0;
    base::u64 payload_secondary = 0;
    base::u32 payload_bytes = 0;
    if (!parse_u32(fields[layout.schema], schema) || schema != query::QUERY_KEY_SCHEMA_VERSION
        || !parse_u64(fields[layout.global], query_global) || query_global == 0
        || !parse_u64(fields[layout.payload_primary], payload_primary)
        || !parse_u64(fields[layout.payload_secondary], payload_secondary)
        || !parse_u32(fields[layout.payload_bytes], payload_bytes)) {
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

[[nodiscard]] const query::QueryRecord* parsed_query_record_for_key(
    const ParsedCache& cache, const query::QueryKey key) noexcept
{
    for (const ParsedQueryRecord& record : cache.queries) {
        if (record.record.key == key) {
            return &record.record;
        }
    }
    return nullptr;
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

    static constexpr QueryKeyFieldLayout DEPENDENT_LAYOUT{
        INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_KIND_FIELD,
        INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_SCHEMA_FIELD,
        INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_GLOBAL_FIELD,
        INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_PAYLOAD_PRIMARY_FIELD,
        INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_PAYLOAD_SECONDARY_FIELD,
        INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENT_PAYLOAD_BYTES_FIELD,
    };
    static constexpr QueryKeyFieldLayout DEPENDENCY_LAYOUT{
        INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_KIND_FIELD,
        INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_SCHEMA_FIELD,
        INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_GLOBAL_FIELD,
        INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_PAYLOAD_PRIMARY_FIELD,
        INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_PAYLOAD_SECONDARY_FIELD,
        INCREMENTAL_CACHE_QUERY_EDGE_DEPENDENCY_PAYLOAD_BYTES_FIELD,
    };
    const std::optional<query::QueryKey> dependent = parse_query_key_fields(fields, DEPENDENT_LAYOUT);
    const std::optional<query::QueryKey> dependency = parse_query_key_fields(fields, DEPENDENCY_LAYOUT);
    if (!dependent || !dependency) {
        return false;
    }

    return push_parsed_query_dependency_edge(cache,
        query::QueryDependencyEdge{
            *dependent,
            *dependency,
        });
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

[[nodiscard]] ParsedCacheValidationStatus parsed_cache_query_records_validation_status(const ParsedCache& cache)
{
    for (const ParsedQueryRecord& record : cache.queries) {
        if (!query::query_record_stable_identity_is_valid(record.record)) {
            return ParsedCacheValidationStatus::malformed_query_identity;
        }
    }
    return ParsedCacheValidationStatus::valid;
}

[[nodiscard]] ParsedCacheValidationStatus parsed_cache_query_edges_validation_status(const ParsedCache& cache)
{
    for (const query::QueryDependencyEdge& edge : cache.query_edges) {
        const query::QueryRecord* const dependent = parsed_query_record_for_key(cache, edge.dependent);
        const query::QueryRecord* const dependency = parsed_query_record_for_key(cache, edge.dependency);
        if (dependent == nullptr || dependency == nullptr || !query_dependency_edge_schedule_is_valid(edge)) {
            return ParsedCacheValidationStatus::malformed_query_graph;
        }

        const query::QueryDependencyEdgeValidationStatus status =
            query::validate_query_dependency_edge_records(*dependent, *dependency);
        if (status == query::QueryDependencyEdgeValidationStatus::invalid_identity) {
            return ParsedCacheValidationStatus::malformed_query_identity;
        }
        if (status != query::QueryDependencyEdgeValidationStatus::valid) {
            return ParsedCacheValidationStatus::malformed_query_graph;
        }
    }
    return ParsedCacheValidationStatus::valid;
}

} // namespace

[[nodiscard]] std::optional<query::QueryKind> parse_query_kind_name(const std::string_view name) noexcept
{
    for (const QueryKindCacheName& entry : INCREMENTAL_CACHE_QUERY_KIND_NAMES) {
        if (entry.name == name) {
            return entry.kind;
        }
    }
    return std::nullopt;
}

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

[[nodiscard]] bool same_fingerprint(const SourceFingerprintRecord& lhs, const SourceFingerprintRecord& rhs) noexcept
{
    return lhs.size == rhs.size && lhs.fingerprint.primary == rhs.fingerprint.primary
        && lhs.fingerprint.secondary == rhs.fingerprint.secondary
        && lhs.fingerprint.byte_count == rhs.fingerprint.byte_count;
}

[[nodiscard]] query::PackageKey default_cache_package_key() noexcept
{
    return query::package_key(std::span<const std::string_view>{});
}

[[nodiscard]] query::PackageKey source_package_or_default(const query::PackageKey package) noexcept
{
    return query::is_valid(package) ? package : default_cache_package_key();
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
        default_cache_package_key(),
    };
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

[[nodiscard]] std::unordered_map<std::string, query::PackageKey> source_package_index(
    const std::span<const ModuleRecord> modules)
{
    std::unordered_map<std::string, query::PackageKey> packages;
    for (const ModuleRecord& module : modules) {
        const query::PackageKey package = source_package_or_default(module.package);
        packages[canonical_or_absolute(module.path).string()] = package;
        for (const ModulePartRecord& part : module.parts) {
            packages[canonical_or_absolute(part.path).string()] = package;
        }
    }
    return packages;
}

[[nodiscard]] query::PackageKey source_package_for_path(
    const std::unordered_map<std::string, query::PackageKey>& packages, const std::filesystem::path& path)
{
    const auto found = packages.find(canonical_or_absolute(path).string());
    if (found != packages.end()) {
        return source_package_or_default(found->second);
    }
    return default_cache_package_key();
}

[[nodiscard]] std::vector<SourceFingerprintRecord> collect_source_fingerprints(
    const base::SourceManager& sources, const std::span<const ModuleRecord> modules)
{
    const std::unordered_map<std::string, query::PackageKey> source_packages = source_package_index(modules);
    std::vector<SourceFingerprintRecord> records;
    records.reserve(sources.files().size());
    for (const base::SourceFile& file : sources.files()) {
        const std::filesystem::path path(file.path());
        records.push_back(SourceFingerprintRecord{
            canonical_or_absolute(path),
            file.text().size(),
            fingerprint_text(file.text()),
            source_package_for_path(source_packages, path),
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
        ModuleRecord normalized = module;
        normalized.path = canonical_or_absolute(module.path);
        for (ModulePartRecord& part : normalized.parts) {
            part.path = canonical_or_absolute(part.path);
        }
        sorted.push_back(std::move(normalized));
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

void write_hex_field(std::ostream& out, const std::string_view value)
{
    append_hex_string(out, value);
}

void write_header_field(std::ostream& out, const std::string_view name, const std::string_view encoded_value)
{
    out << name << INCREMENTAL_CACHE_SEPARATOR << encoded_value << '\n';
}

void write_encoded_header_field(std::ostream& out, const EncodedHeaderField field)
{
    out << field.name << INCREMENTAL_CACHE_SEPARATOR;
    write_hex_field(out, field.value);
    out << '\n';
}

void write_source_record(std::ostream& out, const SourceFingerprintRecord& record)
{
    const query::PackageKey package = source_package_or_default(record.package);
    out << INCREMENTAL_CACHE_FIELD_SOURCE << INCREMENTAL_CACHE_SEPARATOR << record.size << INCREMENTAL_CACHE_SEPARATOR
        << record.fingerprint.primary << INCREMENTAL_CACHE_SEPARATOR << record.fingerprint.secondary
        << INCREMENTAL_CACHE_SEPARATOR << record.fingerprint.byte_count << INCREMENTAL_CACHE_SEPARATOR;
    write_hex_field(out, record.path.string());
    out << INCREMENTAL_CACHE_SEPARATOR << package.global_id << INCREMENTAL_CACHE_SEPARATOR << package.identity.primary
        << INCREMENTAL_CACHE_SEPARATOR << package.identity.secondary << INCREMENTAL_CACHE_SEPARATOR
        << package.identity.byte_count << '\n';
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

[[nodiscard]] ParsedCacheValidationStatus parsed_cache_validation_status(const ParsedCache& cache)
{
    const bool queries_match =
        cache.expected_queries.has_value() ? cache.queries.size() == *cache.expected_queries : cache.queries.empty();
    const bool query_edges_match = cache.expected_query_edges.has_value()
        ? cache.query_edges.size() == *cache.expected_query_edges
        : cache.query_edges.empty();
    if (!cache.expected_import_paths || !cache.expected_sources || !cache.expected_modules
        || !cache.expected_definitions || cache.import_paths.size() != *cache.expected_import_paths
        || cache.sources.size() != *cache.expected_sources || cache.module_count != *cache.expected_modules
        || cache.definition_count != *cache.expected_definitions || !queries_match || !query_edges_match) {
        return ParsedCacheValidationStatus::malformed_cache;
    }

    const ParsedCacheValidationStatus query_records_validation = parsed_cache_query_records_validation_status(cache);
    if (query_records_validation != ParsedCacheValidationStatus::valid) {
        return query_records_validation;
    }
    return parsed_cache_query_edges_validation_status(cache);
}

[[nodiscard]] bool parsed_cache_counts_match(const ParsedCache& cache)
{
    return parsed_cache_validation_status(cache) == ParsedCacheValidationStatus::valid;
}

[[nodiscard]] bool parsed_cache_header_matches(const ParsedCache& cache, const CompilerInvocation& invocation)
{
    if (cache.schema != INCREMENTAL_CACHE_SCHEMA_VERSION || cache.compiler_version != base::config::AUREX_VERSION_STRING
        || cache.mode != INCREMENTAL_CACHE_MODE_SEMANTIC_OK
        || cache.root_path != canonical_or_absolute(invocation.input_path)
        || cache.package_identity.value_or(std::string{}) != package_identity_for_invocation(invocation)
        || !parsed_cache_counts_match(cache)) {
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

[[nodiscard]] ParsedCacheReadResult read_incremental_cache_with_status(const std::filesystem::path& cache_path)
{
    std::error_code exists_error;
    if (!std::filesystem::exists(cache_path, exists_error)) {
        return ParsedCacheReadResult{
            std::nullopt,
            exists_error ? ParsedCacheReadStatus::malformed : ParsedCacheReadStatus::missing,
        };
    }

    std::ifstream input(cache_path, std::ios::binary);
    if (!input) {
        return ParsedCacheReadResult{
            std::nullopt,
            ParsedCacheReadStatus::malformed,
        };
    }

    std::string line;
    if (!std::getline(input, line) || line != INCREMENTAL_CACHE_MAGIC) {
        return ParsedCacheReadResult{
            std::nullopt,
            ParsedCacheReadStatus::malformed,
        };
    }

    ParsedCache cache;
    while (std::getline(input, line)) {
        if (!parse_cache_line(cache, line)) {
            return ParsedCacheReadResult{
                std::nullopt,
                ParsedCacheReadStatus::malformed,
            };
        }
    }
    if (input.bad()) {
        return ParsedCacheReadResult{
            std::nullopt,
            ParsedCacheReadStatus::malformed,
        };
    }
    return ParsedCacheReadResult{
        std::move(cache),
        ParsedCacheReadStatus::loaded,
    };
}

[[nodiscard]] std::optional<ParsedCache> read_incremental_cache(const std::filesystem::path& cache_path)
{
    ParsedCacheReadResult result = read_incremental_cache_with_status(cache_path);
    return std::move(result.cache);
}

} // namespace aurex::driver::incremental_cache_detail
