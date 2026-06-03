#pragma once

#include <aurex/application/driver/invocation.hpp>
#include <aurex/infrastructure/base/result.hpp>
#include <aurex/infrastructure/base/source.hpp>

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <application/driver/incremental_cache/core/private/types.hpp>

namespace aurex::driver::incremental_cache_detail {

[[nodiscard]] std::filesystem::path canonical_or_absolute(const std::filesystem::path& path);
[[nodiscard]] sema::StableFingerprint128 fingerprint_text(std::string_view text) noexcept;
[[nodiscard]] std::optional<std::string> read_file_for_fingerprint(const std::filesystem::path& path);
[[nodiscard]] std::optional<SourceFingerprintRecord> fingerprint_file(const std::filesystem::path& path);
[[nodiscard]] bool same_fingerprint(const SourceFingerprintRecord& lhs, const SourceFingerprintRecord& rhs) noexcept;

[[nodiscard]] std::vector<std::filesystem::path> normalized_import_paths(const CompilerInvocation& invocation);
[[nodiscard]] std::vector<std::string> normalized_import_package_identities(const CompilerInvocation& invocation);
[[nodiscard]] std::vector<SourceFingerprintRecord> collect_source_fingerprints(
    const base::SourceManager& sources, std::span<const ModuleRecord> modules);
[[nodiscard]] std::vector<ModuleRecord> sorted_modules(std::span<const ModuleRecord> modules);
[[nodiscard]] std::string_view stable_symbol_kind_name(sema::StableSymbolKind kind) noexcept;
[[nodiscard]] std::optional<query::QueryKind> parse_query_kind_name(std::string_view name) noexcept;
[[nodiscard]] std::string_view query_kind_name(query::QueryKind kind) noexcept;

struct EncodedHeaderField {
    std::string_view name;
    std::string_view value;
};

void write_hex_field(std::ostream& out, std::string_view value);
void write_header_field(std::ostream& out, std::string_view name, std::string_view encoded_value);
void write_encoded_header_field(std::ostream& out, EncodedHeaderField field);
void write_source_record(std::ostream& out, const SourceFingerprintRecord& record);
void write_module_record(std::ostream& out, const ModuleRecord& record);
void write_definition_record(std::ostream& out, const DefinitionRecord& record);
void write_query_record(std::ostream& out, const query::QueryRecord& record);
void write_query_key_fields(std::ostream& out, query::QueryKey key);
void write_query_dependency_edge_record(std::ostream& out, const query::QueryDependencyEdge& edge);

[[nodiscard]] std::filesystem::path temporary_cache_path(const std::filesystem::path& cache_path);
[[nodiscard]] base::Result<void> publish_cache_file(
    const std::filesystem::path& temporary_path, const std::filesystem::path& cache_path);

[[nodiscard]] ParsedCacheReadResult read_incremental_cache_with_status(const std::filesystem::path& cache_path);
[[nodiscard]] std::optional<ParsedCache> read_incremental_cache(const std::filesystem::path& cache_path);
[[nodiscard]] ParsedCacheValidationStatus parsed_cache_validation_status(const ParsedCache& cache);
[[nodiscard]] bool parsed_cache_counts_match(const ParsedCache& cache);
[[nodiscard]] bool parsed_cache_header_matches(const ParsedCache& cache, const CompilerInvocation& invocation);
[[nodiscard]] std::string parsed_cache_project_input_changes(
    const ParsedCache& cache, const CompilerInvocation& invocation);

} // namespace aurex::driver::incremental_cache_detail
