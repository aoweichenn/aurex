#include <aurex/query/source_file_query.hpp>

#include <utility>

namespace aurex::query {
namespace {

[[nodiscard]] bool is_valid_source_file_output(
    const QueryRecord& record, const QueryResultFingerprint result, const QueryKind kind) noexcept
{
    return is_valid(record) && is_valid(result) && record.key.kind == kind && record.result == result;
}

[[nodiscard]] bool dependencies_are_valid(const std::vector<QueryKey>& dependencies) noexcept
{
    for (const QueryKey dependency : dependencies) {
        if (!is_valid(dependency)) {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<QueryKey> file_content_query_key(const FileKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::file_content, stable_key_fingerprint(key));
}

std::optional<QueryKey> lex_file_query_key(const LexFileKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::lex_file, stable_key_fingerprint(key));
}

std::optional<QueryKey> parse_file_query_key(const ParseFileKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::parse_file, stable_key_fingerprint(key));
}

std::optional<QuerySourceStageKeys> query_source_stage_keys(
    const FileKey file, const QuerySourceStageMode mode) noexcept
{
    if (!is_valid(file)) {
        return std::nullopt;
    }

    const bool retain_trivia = mode == QuerySourceStageMode::lossless_tooling;
    const bool build_lossless_tree = mode == QuerySourceStageMode::lossless_tooling;
    const LexConfigKey lex_config = lex_config_key(retain_trivia);
    const ParserConfigKey parser_config =
        parser_config_key(lex_config, QUERY_PARSER_CONFIG_DEFAULT_ENABLE_RECOVERY, build_lossless_tree);
    return QuerySourceStageKeys{
        file,
        lex_config,
        parser_config,
        lex_file_key(file, lex_config),
        parse_file_key(file, parser_config),
    };
}

bool is_valid(const FileContentProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.content);
}

bool is_valid(const FileContentProviderOutput& output) noexcept
{
    return is_valid_source_file_output(output.record, output.result, QueryKind::file_content)
        && dependencies_are_valid(output.dependencies);
}

bool is_valid(const LexFileProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.tokens);
}

bool is_valid(const LexFileProviderOutput& output) noexcept
{
    return is_valid_source_file_output(output.record, output.result, QueryKind::lex_file)
        && dependencies_are_valid(output.dependencies);
}

bool is_valid(const ParseFileProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.syntax);
}

bool is_valid(const ParseFileProviderOutput& output) noexcept
{
    return is_valid_source_file_output(output.record, output.result, QueryKind::parse_file)
        && dependencies_are_valid(output.dependencies);
}

std::optional<FileContentProviderOutput> provide_file_content_query(const FileContentProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = file_content_query_record(input.key, input.content);
    return FileContentProviderOutput{
        std::move(*record),
        input.content,
        {},
    };
}

std::optional<LexFileProviderOutput> provide_lex_file_query(const LexFileProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = lex_file_query_record(input.key, input.tokens);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> content_key = file_content_query_key(input.key.file)) {
        dependencies.push_back(*content_key);
    }
    return LexFileProviderOutput{
        std::move(*record),
        input.tokens,
        std::move(dependencies),
    };
}

std::optional<ParseFileProviderOutput> provide_parse_file_query(const ParseFileProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = parse_file_query_record(input.key, input.syntax);
    std::vector<QueryKey> dependencies;
    const LexFileKey lex_key = lex_file_key(input.key.file, input.key.config.lex_config);
    if (const std::optional<QueryKey> tokens_key = lex_file_query_key(lex_key)) {
        dependencies.push_back(*tokens_key);
    }
    return ParseFileProviderOutput{
        std::move(*record),
        input.syntax,
        std::move(dependencies),
    };
}

} // namespace aurex::query
