#include "io.hpp"
#include "schedule.hpp"
#include "subjects.hpp"

#include <aurex/lex/lexer.hpp>
#include <aurex/syntax/lossless.hpp>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace aurex::driver::incremental_cache_detail {
namespace cache_format = incremental_cache_format;
using namespace cache_format;

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

[[nodiscard]] bool item_list_signature_entry_less(const ItemListSignatureEntry& lhs, const ItemListSignatureEntry& rhs)
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

[[nodiscard]] query::PackageKey cache_package_key() noexcept
{
    return query::package_key(std::span<const std::string_view>{});
}

[[nodiscard]] query::FileKey source_file_key(const base::SourceFile& file)
{
    const std::filesystem::path canonical_path = canonical_or_absolute(std::filesystem::path(file.path()));
    return query::file_key(cache_package_key(), canonical_path.string());
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
    const std::filesystem::path& path, std::string text)
{
    base::SourceManager sources;
    const base::SourceId source_id = sources.add_source(path.string(), std::move(text));
    const base::SourceFile& file = sources.get(source_id);
    const query::FileKey file_key = source_file_key(file);
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

[[nodiscard]] std::optional<SourceStageQueryRecords> source_stage_query_records_for_file(
    const std::filesystem::path& path)
{
    std::optional<std::string> text = read_file_for_fingerprint(path);
    if (!text) {
        return std::nullopt;
    }
    return source_stage_query_records_for_text(path, std::move(*text));
}

[[nodiscard]] query::QueryResultFingerprint module_graph_result_fingerprint(
    const query::ModuleKey key, const std::span<const ModuleRecord> modules)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_MODULE_GRAPH_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(static_cast<base::u64>(modules.size()));
    for (base::usize index = 0; index < modules.size(); ++index) {
        const ModuleRecord& module = modules[index];
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_string(module.name);
        builder.mix_string(module.path.string());
    }
    return query::query_result_fingerprint(builder.finish());
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

void push_item_list_signature_entry(std::vector<ItemListSignatureEntry>& entries, const std::string_view category,
    const std::string_view name, const sema::StableDefId& stable_id, const sema::IncrementalKey& incremental_key,
    const sema::StableModuleId& module)
{
    if (stable_id.module != module || !query::is_valid(stable_id) || !query::is_valid(incremental_key)) {
        return;
    }
    entries.push_back(ItemListSignatureEntry{
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

[[nodiscard]] std::vector<ItemListSignatureEntry> collect_item_list_signature_entries(
    const sema::CheckedModule& checked, const sema::StableModuleId& module)
{
    std::vector<ItemListSignatureEntry> entries;
    entries.reserve(checked.functions.size() + checked.generic_template_signatures.size() + checked.structs.size()
        + checked.enum_cases.size() + checked.type_aliases.size());

    for (const sema::GenericTemplateSignatureInfo& info : checked.generic_template_signatures) {
        push_item_list_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_GENERIC_TEMPLATE, info.name.view(),
            info.stable_id, info.incremental_key, module);
    }
    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        push_item_list_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_FUNCTION, signature.name.view(),
            signature.stable_id, signature.incremental_key, module);
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        push_item_list_signature_entry(
            entries, INCREMENTAL_CACHE_CATEGORY_STRUCT, info.name.view(), info.stable_id, info.incremental_key, module);
    }
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        push_item_list_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_ENUM_CASE, info.name.view(), info.stable_id,
            info.incremental_key, module);
    }
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& info = entry.second;
        push_item_list_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_TYPE_ALIAS, info.name.view(), info.stable_id,
            info.incremental_key, module);
    }

    std::sort(entries.begin(), entries.end(), item_list_signature_entry_less);
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

[[nodiscard]] query::QueryResultFingerprint item_list_result_fingerprint(
    const query::ModuleKey key, const std::vector<ItemListSignatureEntry>& entries)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_ITEM_LIST_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(static_cast<base::u64>(entries.size()));
    for (base::usize index = 0; index < entries.size(); ++index) {
        const ItemListSignatureEntry& entry = entries[index];
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
    return std::string_view{text.data() + range.begin, range.end - range.begin};
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

[[nodiscard]] std::optional<base::SourceRange> function_signature_body_range(
    const sema::FunctionSignature& signature, const syntax::AstModule& ast) noexcept
{
    const syntax::ItemId item = syntax::is_valid(signature.definition_item) ? signature.definition_item
                                                                            : signature.prototype_item;
    if (!syntax::is_valid(item) || item.value >= ast.items.size()) {
        return std::nullopt;
    }
    const syntax::ItemNode* const node = ast.items.ptr(item.value);
    if (node == nullptr || node->kind != syntax::ItemKind::fn_decl || !syntax::is_valid(node->body)
        || node->body.value >= ast.stmts.size()) {
        return std::nullopt;
    }
    return ast.stmts.range(node->body.value);
}

[[nodiscard]] std::optional<std::string_view> function_body_text(
    const base::SourceManager& sources, const sema::FunctionSignature& signature, const syntax::AstModule& ast) noexcept
{
    if (const std::optional<base::SourceRange> body_range = function_signature_body_range(signature, ast)) {
        return source_range_text(sources, *body_range);
    }
    return source_range_text(sources, signature.range);
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

void push_generic_template_signature_query_subject(
    std::vector<GenericTemplateSignatureQuerySubject>& subjects, const sema::GenericTemplateSignatureInfo& info)
{
    subjects.push_back(GenericTemplateSignatureQuerySubject{
        info.stable_id,
        info.incremental_key,
        info.name_space,
    });
}

void push_source_file_query_subjects(QuerySubjectCollection& collection, const base::SourceFile& file)
{
    const query::FileKey file_key = source_file_key(file);
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

void push_module_graph_query_subject(std::vector<ModuleGraphQuerySubject>& subjects, const ModuleRecord& module,
    const std::span<const ModuleRecord> modules)
{
    const sema::StableModuleId stable_module = stable_module_id_from_record(module);
    const query::ModuleKey key = query::module_key_from_stable_id(stable_module);
    if (!query::is_valid(stable_module) || !query::is_valid(key)) {
        return;
    }
    subjects.push_back(ModuleGraphQuerySubject{
        key,
        module_graph_result_fingerprint(key, modules),
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
    const base::SourceManager& sources, const syntax::AstModule* const ast)
{
    if (!signature.has_definition || signature.has_conflict || !query::is_valid(signature.stable_id)
        || !query::is_valid(signature.incremental_key)) {
        return;
    }
    const query::BodyKey key = function_body_key(signature);
    if (!query::is_valid(key)) {
        return;
    }
    const std::optional<std::string_view> body_text =
        ast == nullptr ? source_range_text(sources, signature.range) : function_body_text(sources, signature, *ast);
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

void push_item_list_query_subject(
    std::vector<ItemListQuerySubject>& subjects, const ModuleRecord& module, const sema::CheckedModule& checked)
{
    const sema::StableModuleId stable_module = stable_module_id_from_record(module);
    const query::ModuleKey key = query::module_key_from_stable_id(stable_module);
    if (!query::is_valid(stable_module) || !query::is_valid(key)) {
        return;
    }

    const std::vector<ItemListSignatureEntry> entries = collect_item_list_signature_entries(checked, stable_module);
    subjects.push_back(ItemListQuerySubject{
        key,
        item_list_result_fingerprint(key, entries),
    });
}

void evaluate_file_content_query_subject(query::QueryContext& context, const FileContentQuerySubject& subject)
{
    const query::FileContentProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_file_content(input));
}

void evaluate_lex_file_query_subject(query::QueryContext& context, const LexFileQuerySubject& subject)
{
    const query::LexFileProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_lex_file(input));
}

void evaluate_parse_file_query_subject(query::QueryContext& context, const ParseFileQuerySubject& subject)
{
    const query::ParseFileProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_parse_file(input));
}

void evaluate_module_graph_query_subject(query::QueryContext& context, const ModuleGraphQuerySubject& subject)
{
    const query::ModuleGraphProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_module_graph(input));
}

void evaluate_module_exports_query_subject(query::QueryContext& context, const ModuleExportsQuerySubject& subject)
{
    const query::ModuleExportsProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_module_exports(input));
}

void evaluate_item_list_query_subject(query::QueryContext& context, const ItemListQuerySubject& subject)
{
    const query::ItemListProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_item_list(input));
}

void evaluate_item_signature_query_subject(query::QueryContext& context, const ItemSignatureQuerySubject& subject)
{
    const query::ItemSignatureProviderInput input{
        query::def_key_from_stable_id(subject.stable_id, subject.name_space, subject.kind),
        subject.incremental_key,
    };
    static_cast<void>(context.evaluate_item_signature(input));
}

void evaluate_generic_template_signature_query_subject(
    query::QueryContext& context, const GenericTemplateSignatureQuerySubject& subject)
{
    const query::GenericTemplateSignatureProviderInput input{
        query::def_key_from_stable_id(subject.stable_id, subject.name_space, query::DefKind::generic_template),
        subject.incremental_key,
    };
    static_cast<void>(context.evaluate_generic_template_signature(input));
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

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const FileContentQuerySubject& subject)
{
    return query::file_content_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const LexFileQuerySubject& subject)
{
    return query::lex_file_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ParseFileQuerySubject& subject)
{
    return query::parse_file_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ModuleGraphQuerySubject& subject)
{
    return query::module_graph_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ModuleExportsQuerySubject& subject)
{
    return query::module_exports_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ItemListQuerySubject& subject)
{
    return query::item_list_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ItemSignatureQuerySubject& subject)
{
    const query::DefKey key = query::def_key_from_stable_id(subject.stable_id, subject.name_space, subject.kind);
    return query::item_signature_query_record(key, query::query_result_fingerprint(subject.incremental_key));
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(
    const GenericTemplateSignatureQuerySubject& subject)
{
    const query::DefKey key =
        query::def_key_from_stable_id(subject.stable_id, subject.name_space, query::DefKind::generic_template);
    return query::generic_template_signature_query_record(
        key, query::query_result_fingerprint(subject.incremental_key));
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
    collection.subjects.reserve(collection.file_contents.size() + collection.lex_files.size()
        + collection.parse_files.size() + collection.module_graphs.size() + collection.module_exports.size()
        + collection.item_lists.size() + collection.item_signatures.size() + collection.function_body_syntaxes.size()
        + collection.type_check_bodies.size() + collection.generic_template_signatures.size()
        + collection.generic_instance_signatures.size() + collection.generic_instance_bodies.size()
        + collection.lower_function_irs.size());
    std::unordered_set<query::QueryKey, query::QueryKeyHash> keys;
    keys.reserve(collection.file_contents.size() + collection.lex_files.size() + collection.parse_files.size()
        + collection.module_graphs.size() + collection.module_exports.size() + collection.item_lists.size()
        + collection.item_signatures.size() + collection.function_body_syntaxes.size()
        + collection.type_check_bodies.size() + collection.generic_template_signatures.size()
        + collection.generic_instance_signatures.size() + collection.generic_instance_bodies.size()
        + collection.lower_function_irs.size());

    for (base::usize index = 0; index < collection.file_contents.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::file_content, index,
            query_record_for_subject(collection.file_contents[index]));
    }
    for (base::usize index = 0; index < collection.lex_files.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::lex_file, index,
            query_record_for_subject(collection.lex_files[index]));
    }
    for (base::usize index = 0; index < collection.parse_files.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::parse_file, index,
            query_record_for_subject(collection.parse_files[index]));
    }
    for (base::usize index = 0; index < collection.module_graphs.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::module_graph, index,
            query_record_for_subject(collection.module_graphs[index]));
    }
    for (base::usize index = 0; index < collection.module_exports.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::module_exports, index,
            query_record_for_subject(collection.module_exports[index]));
    }
    for (base::usize index = 0; index < collection.item_lists.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::item_list, index,
            query_record_for_subject(collection.item_lists[index]));
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
    for (base::usize index = 0; index < collection.generic_template_signatures.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::generic_template_signature, index,
            query_record_for_subject(collection.generic_template_signatures[index]));
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

    std::sort(collection.subjects.begin(), collection.subjects.end(), query_subject_schedule_less);

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

[[nodiscard]] std::vector<ModuleGraphQuerySubject> collect_module_graph_query_subjects(
    const std::span<const ModuleRecord> modules)
{
    std::vector<ModuleGraphQuerySubject> subjects;
    subjects.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        push_module_graph_query_subject(subjects, module, modules);
    }
    return subjects;
}

[[nodiscard]] std::vector<ItemListQuerySubject> collect_item_list_query_subjects(
    const std::span<const ModuleRecord> modules, const sema::CheckedModule& checked)
{
    std::vector<ItemListQuerySubject> subjects;
    subjects.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        push_item_list_query_subject(subjects, module, checked);
    }
    return subjects;
}

void collect_source_file_query_subjects(QuerySubjectCollection& collection, const base::SourceManager& sources)
{
    const std::span<const base::SourceFile> files = sources.files();
    collection.file_contents.reserve(files.size());
    collection.lex_files.reserve(files.size());
    collection.parse_files.reserve(files.size());
    for (const base::SourceFile& file : files) {
        push_source_file_query_subjects(collection, file);
    }
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

[[nodiscard]] std::vector<GenericTemplateSignatureQuerySubject> collect_generic_template_signature_query_subjects(
    const sema::CheckedModule& checked)
{
    std::vector<GenericTemplateSignatureQuerySubject> subjects;
    subjects.reserve(checked.generic_template_signatures.size());
    for (const sema::GenericTemplateSignatureInfo& info : checked.generic_template_signatures) {
        push_generic_template_signature_query_subject(subjects, info);
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
    const syntax::AstModule* const ast, std::vector<FunctionBodySyntaxQuerySubject>& syntax_subjects,
    std::vector<TypeCheckBodyQuerySubject>& type_check_subjects)
{
    syntax_subjects.reserve(checked.functions.size());
    type_check_subjects.reserve(checked.functions.size());
    for (const auto& entry : checked.functions) {
        push_function_body_query_subjects(syntax_subjects, type_check_subjects, entry.second, sources, ast);
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
    const std::span<const ModuleRecord> modules, const sema::CheckedModule& checked, const base::SourceManager& sources,
    const syntax::AstModule* const ast)
{
    QuerySubjectCollection collection;
    collect_source_file_query_subjects(collection, sources);
    collection.module_graphs = collect_module_graph_query_subjects(modules);
    collection.item_lists = collect_item_list_query_subjects(modules, checked);
    collection.module_exports = collect_module_exports_query_subjects(modules, checked);
    collection.item_signatures = collect_item_signature_query_subjects(checked);
    collect_function_body_query_subjects(
        checked, sources, ast, collection.function_body_syntaxes, collection.type_check_bodies);
    collection.generic_template_signatures = collect_generic_template_signature_query_subjects(checked);
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
        case QuerySubjectKind::file_content:
            evaluate_file_content_query_subject(context, collection.file_contents[subject.index]);
            return;
        case QuerySubjectKind::lex_file:
            evaluate_lex_file_query_subject(context, collection.lex_files[subject.index]);
            return;
        case QuerySubjectKind::parse_file:
            evaluate_parse_file_query_subject(context, collection.parse_files[subject.index]);
            return;
        case QuerySubjectKind::module_graph:
            evaluate_module_graph_query_subject(context, collection.module_graphs[subject.index]);
            return;
        case QuerySubjectKind::module_exports:
            evaluate_module_exports_query_subject(context, collection.module_exports[subject.index]);
            return;
        case QuerySubjectKind::item_list:
            evaluate_item_list_query_subject(context, collection.item_lists[subject.index]);
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
        case QuerySubjectKind::generic_template_signature:
            evaluate_generic_template_signature_query_subject(
                context, collection.generic_template_signatures[subject.index]);
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

[[nodiscard]] std::vector<DefinitionRecord> collect_definitions(const sema::CheckedModule& checked)
{
    std::vector<DefinitionRecord> records;
    records.reserve(checked.functions.size() + checked.generic_template_signatures.size()
        + checked.generic_function_instances.size() + checked.structs.size() + checked.enum_cases.size()
        + checked.type_aliases.size());

    for (const sema::GenericTemplateSignatureInfo& info : checked.generic_template_signatures) {
        push_definition(records, INCREMENTAL_CACHE_CATEGORY_GENERIC_TEMPLATE, info.name.view(), info.stable_id,
            info.incremental_key);
    }
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


} // namespace aurex::driver::incremental_cache_detail
