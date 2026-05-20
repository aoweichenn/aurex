#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

enum class QuerySourceStageMode : base::u8 {
    semantic,
    lossless_tooling,
};

struct QuerySourceStageKeys {
    FileKey file;
    LexConfigKey lex_config;
    ParserConfigKey parser_config;
    LexFileKey lex_file;
    ParseFileKey parse_file;
};

struct FileContentProviderInput {
    FileKey key;
    QueryResultFingerprint content;
};

struct FileContentProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

struct LexFileProviderInput {
    LexFileKey key;
    QueryResultFingerprint tokens;
};

struct LexFileProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

struct ParseFileProviderInput {
    ParseFileKey key;
    QueryResultFingerprint syntax;
};

struct ParseFileProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> file_content_query_key(FileKey key) noexcept;
[[nodiscard]] std::optional<QueryKey> lex_file_query_key(LexFileKey key) noexcept;
[[nodiscard]] std::optional<QueryKey> parse_file_query_key(ParseFileKey key) noexcept;
[[nodiscard]] std::optional<QuerySourceStageKeys> query_source_stage_keys(
    FileKey file, QuerySourceStageMode mode = QuerySourceStageMode::semantic) noexcept;
[[nodiscard]] bool is_valid(const FileContentProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const FileContentProviderOutput& output) noexcept;
[[nodiscard]] bool is_valid(const LexFileProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const LexFileProviderOutput& output) noexcept;
[[nodiscard]] bool is_valid(const ParseFileProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const ParseFileProviderOutput& output) noexcept;
[[nodiscard]] std::optional<FileContentProviderOutput> provide_file_content_query(
    const FileContentProviderInput& input);
[[nodiscard]] std::optional<LexFileProviderOutput> provide_lex_file_query(const LexFileProviderInput& input);
[[nodiscard]] std::optional<ParseFileProviderOutput> provide_parse_file_query(const ParseFileProviderInput& input);

} // namespace aurex::query
