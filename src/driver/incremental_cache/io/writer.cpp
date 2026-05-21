#include "../io.hpp"

#include <array>
#include <chrono>
#include <ostream>
#include <string>

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

} // namespace

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

} // namespace aurex::driver::incremental_cache_detail
