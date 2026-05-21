#include "io.hpp"
#include "profile.hpp"
#include "query.hpp"
#include "reuse.hpp"
#include "source_stage.hpp"
#include "subjects.hpp"

#include <aurex/base/config.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace aurex::driver::incremental_cache_detail {
namespace cache_format = incremental_cache_format;
using namespace cache_format;

base::Result<bool> try_reuse_incremental_check_cache_impl(
    const CompilerInvocation& invocation, CompilationProfiler* const profiler)
{
    if (invocation.incremental_cache_path.empty() || invocation.emit_kind != EmitKind::check) {
        return base::Result<bool>::ok(false);
    }

    const std::optional<ParsedCache> cache = read_incremental_cache(invocation.incremental_cache_path);
    if (!cache) {
        return base::Result<bool>::ok(false);
    }
    if (!parsed_cache_header_matches(*cache, invocation)) {
        return base::Result<bool>::ok(false);
    }
    if (cache_sources_match(*cache)) {
        return base::Result<bool>::ok(true);
    }
    if (!invocation.query_pruning_enabled) {
        return base::Result<bool>::ok(false);
    }

    const bool collect_all_statuses = profiler != nullptr && profiler->enabled();
    const auto source_stage_reuse_started = std::chrono::steady_clock::now();
    const SourceStageReuseSummary source_stage_reuse =
        source_stage_reuse_summary_for_cache(*cache, collect_all_statuses);
    record_source_stage_reuse_summary(
        profiler, source_stage_reuse, std::chrono::steady_clock::now() - source_stage_reuse_started);
    return base::Result<bool>::ok(source_stage_reuse.reusable);
}

base::Result<void> write_incremental_cache_impl(const CompilerInvocation& invocation, const base::SourceManager& sources,
    const std::span<const ModuleRecord> modules, const syntax::AstModule& ast, const sema::CheckedModule& checked,
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
    const QuerySubjectCollection query_subjects = collect_query_subjects(module_records, checked, sources, &ast);
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
    if (!query_collection_records_and_dependency_edges_are_valid(query_collection)) {
        return base::Result<void>::fail(
            {base::ErrorCode::internal_error, std::string(INCREMENTAL_CACHE_QUERY_GRAPH_INVALID)});
    }

    const std::filesystem::path temporary_path = temporary_cache_path(cache_path);
    {
        std::ofstream out(temporary_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            return base::Result<void>::fail(
                {base::ErrorCode::io_error, std::string(INCREMENTAL_CACHE_WRITE_OPEN_FAILED)});
        }

        out << INCREMENTAL_CACHE_MAGIC << '\n';
        write_header_field(out, INCREMENTAL_CACHE_FIELD_SCHEMA, std::to_string(INCREMENTAL_CACHE_SCHEMA_VERSION));
        write_encoded_header_field(out, {INCREMENTAL_CACHE_FIELD_COMPILER, base::config::AUREX_VERSION_STRING});
        write_encoded_header_field(out, {INCREMENTAL_CACHE_FIELD_MODE, INCREMENTAL_CACHE_MODE_SEMANTIC_OK});
        write_encoded_header_field(out, {INCREMENTAL_CACHE_FIELD_ROOT, canonical_or_absolute(invocation.input_path).string()});
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

} // namespace aurex::driver::incremental_cache_detail
