#include <aurex/lex/lexer.hpp>
#include <aurex/syntax/lossless.hpp>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "../io.hpp"
#include "detail.hpp"

namespace aurex::driver::incremental_cache_detail {
namespace cache_format = incremental_cache_format;
using namespace cache_format;

namespace {

[[nodiscard]] query::PackageKey cache_package_key(const query::PackageKey package) noexcept
{
    if (query::is_valid(package)) {
        return package;
    }
    return query::package_key(std::span<const std::string_view>{});
}

[[nodiscard]] query::FileKey source_file_key(const base::SourceFile& file, const query::PackageKey package)
{
    const std::filesystem::path canonical_path = canonical_or_absolute(std::filesystem::path(file.path()));
    return query::file_key(cache_package_key(package), canonical_path.string());
}

[[nodiscard]] query::QueryResultFingerprint file_content_result_fingerprint(
    const query::FileKey key, const std::string_view text)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_FILE_CONTENT_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_string(text);
    return query::query_result_fingerprint(builder.finish());
}

void mix_token_stream_result(query::StableHashBuilder& builder, const std::span<const syntax::Token> tokens)
{
    builder.mix_u64(static_cast<base::u64>(tokens.size()));
    for (base::usize index = 0; index < tokens.size(); ++index) {
        const syntax::Token& token = tokens[index];
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_u64(static_cast<base::u64>(token.kind));
        builder.mix_string(token.text());
    }
}

[[nodiscard]] query::QueryResultFingerprint lex_file_result_fingerprint(
    const query::LexFileKey key, const base::SourceFile& file)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_LEX_FILE_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_bool(key.config.retain_trivia);

    base::DiagnosticSink diagnostics;
    lex::LexerOptions lexer_options;
    lexer_options.emit_trivia_tokens = key.config.retain_trivia;
    lex::Lexer lexer(file.id(), file.text(), diagnostics, lexer_options);
    base::Result<lex::TokenBuffer> tokenize_result = lexer.tokenize();
    if (!tokenize_result) {
        builder.mix_string(INCREMENTAL_CACHE_LEX_FILE_ERROR_MARKER);
        builder.mix_string(file.text());
        return query::query_result_fingerprint(builder.finish());
    }

    mix_token_stream_result(builder, tokenize_result.value().span());
    return query::query_result_fingerprint(builder.finish());
}

void mix_lossless_syntax_tree_result(query::StableHashBuilder& builder, const syntax::LosslessSyntaxTree& tree)
{
    builder.mix_u64(static_cast<base::u64>(tree.node_count()));
    builder.mix_u64(static_cast<base::u64>(tree.element_count()));
    builder.mix_u64(static_cast<base::u64>(tree.token_count()));
    for (base::usize index = 0; index < tree.nodes().size(); ++index) {
        const syntax::LosslessNode& node = tree.nodes()[index];
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_u64(static_cast<base::u64>(node.kind));
        builder.mix_u64(static_cast<base::u64>(node.range.begin));
        builder.mix_u64(static_cast<base::u64>(node.range.end));
        builder.mix_u64(static_cast<base::u64>(node.parent.value));
        builder.mix_u64(static_cast<base::u64>(node.first_child));
        builder.mix_u64(static_cast<base::u64>(node.child_count));
        builder.mix_u64(static_cast<base::u64>(node.first_token));
        builder.mix_u64(static_cast<base::u64>(node.token_count));
    }
    for (base::usize index = 0; index < tree.elements().size(); ++index) {
        const syntax::LosslessElement& element = tree.elements()[index];
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_u64(static_cast<base::u64>(element.kind));
        builder.mix_u64(static_cast<base::u64>(element.index));
    }
}

void mix_lossless_parse_result(
    query::StableHashBuilder& builder, const query::ParseFileKey key, const base::SourceFile& file)
{
    base::DiagnosticSink diagnostics;
    lex::LexerOptions lexer_options;
    lexer_options.emit_trivia_tokens = key.config.lex_config.retain_trivia;
    lex::Lexer lexer(file.id(), file.text(), diagnostics, lexer_options);
    base::Result<lex::TokenBuffer> tokenize_result = lexer.tokenize();
    if (!tokenize_result) {
        builder.mix_string(INCREMENTAL_CACHE_PARSE_FILE_ERROR_MARKER);
        builder.mix_string(file.text());
        return;
    }

    const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokenize_result.value().span());
    builder.mix_bool(tree.is_structurally_valid());
    mix_lossless_syntax_tree_result(builder, tree);
}

[[nodiscard]] query::QueryResultFingerprint parse_file_result_fingerprint(
    const query::ParseFileKey key, const query::QueryResultFingerprint lex_result, const base::SourceFile& file)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_PARSE_FILE_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(lex_result.global_id);
    builder.mix_fingerprint(lex_result.fingerprint);
    builder.mix_bool(key.config.build_lossless_tree);
    if (key.config.build_lossless_tree) {
        mix_lossless_parse_result(builder, key, file);
    }
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] std::optional<SourceStageQueryRecords> source_stage_query_records_for_text(
    const std::filesystem::path& path, std::string text, const query::PackageKey package)
{
    base::SourceManager sources;
    const base::SourceId source_id = sources.add_source(path.string(), std::move(text));
    const base::SourceFile& file = sources.get(source_id);
    const query::FileKey file_key = source_file_key(file, package);
    const query::LexConfigKey lex_config = query::lex_config_key();
    const query::ParserConfigKey parser_config = query::parser_config_key(lex_config);
    const query::LexFileKey lex_key = query::lex_file_key(file_key, lex_config);
    const query::ParseFileKey parse_key = query::parse_file_key(file_key, parser_config);
    if (!query::is_valid(file_key) || !query::is_valid(lex_key) || !query::is_valid(parse_key)) {
        return std::nullopt;
    }

    const query::QueryResultFingerprint lex_result = lex_file_result_fingerprint(lex_key, file);
    const query::QueryResultFingerprint parse_result = parse_file_result_fingerprint(parse_key, lex_result, file);
    std::optional<query::QueryRecord> lex_record = query::lex_file_query_record(lex_key, lex_result);
    std::optional<query::QueryRecord> parse_record = query::parse_file_query_record(parse_key, parse_result);
    if (!lex_record || !parse_record) {
        return std::nullopt;
    }

    return SourceStageQueryRecords{
        std::move(*lex_record),
        std::move(*parse_record),
    };
}

[[nodiscard]] std::unordered_map<std::string, query::PackageKey> source_package_index(
    const std::span<const ModuleRecord> modules)
{
    std::unordered_map<std::string, query::PackageKey> packages;
    for (const ModuleRecord& module : modules) {
        const query::PackageKey package = cache_package_key(module.package);
        packages[canonical_or_absolute(module.path).string()] = package;
        for (const ModulePartRecord& part : module.parts) {
            packages[canonical_or_absolute(part.path).string()] = package;
        }
    }
    return packages;
}

[[nodiscard]] query::PackageKey source_package_for_file(
    const std::unordered_map<std::string, query::PackageKey>& packages, const base::SourceFile& file)
{
    const std::filesystem::path canonical_path = canonical_or_absolute(std::filesystem::path(file.path()));
    if (const auto found = packages.find(canonical_path.string()); found != packages.end()) {
        return cache_package_key(found->second);
    }
    return cache_package_key({});
}

void push_source_file_query_subjects(
    QuerySubjectCollection& collection, const base::SourceFile& file, const query::PackageKey package)
{
    const query::FileKey file_key = source_file_key(file, package);
    const query::LexConfigKey lex_config = query::lex_config_key();
    const query::ParserConfigKey parser_config = query::parser_config_key(lex_config);
    const query::LexFileKey lex_key = query::lex_file_key(file_key, lex_config);
    const query::ParseFileKey parse_key = query::parse_file_key(file_key, parser_config);
    if (!query::is_valid(file_key) || !query::is_valid(lex_key) || !query::is_valid(parse_key)) {
        return;
    }

    const query::QueryResultFingerprint content_result = file_content_result_fingerprint(file_key, file.text());
    const query::QueryResultFingerprint lex_result = lex_file_result_fingerprint(lex_key, file);
    const query::QueryResultFingerprint parse_result = parse_file_result_fingerprint(parse_key, lex_result, file);
    if (!query::is_valid(content_result) || !query::is_valid(lex_result) || !query::is_valid(parse_result)) {
        return;
    }

    collection.file_contents.push_back(FileContentQuerySubject{
        file_key,
        content_result,
    });
    collection.lex_files.push_back(LexFileQuerySubject{
        lex_key,
        lex_result,
    });
    collection.parse_files.push_back(ParseFileQuerySubject{
        parse_key,
        parse_result,
    });
}

} // namespace

[[nodiscard]] std::optional<SourceStageQueryRecords> source_stage_query_records_for_file(
    const std::filesystem::path& path, const query::PackageKey package)
{
    std::optional<std::string> text = read_file_for_fingerprint(path);
    if (!text) {
        return std::nullopt;
    }
    return source_stage_query_records_for_text(path, std::move(*text), package);
}

void collect_source_file_query_subjects(
    QuerySubjectCollection& collection, const base::SourceManager& sources, const std::span<const ModuleRecord> modules)
{
    const std::span<const base::SourceFile> files = sources.files();
    const std::unordered_map<std::string, query::PackageKey> source_packages = source_package_index(modules);
    collection.file_contents.reserve(files.size());
    collection.lex_files.reserve(files.size());
    collection.parse_files.reserve(files.size());
    for (const base::SourceFile& file : files) {
        push_source_file_query_subjects(collection, file, source_package_for_file(source_packages, file));
    }
}

} // namespace aurex::driver::incremental_cache_detail
