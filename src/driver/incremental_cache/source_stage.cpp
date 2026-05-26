#include "source_stage.hpp"

#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "io.hpp"
#include "subjects.hpp"

namespace aurex::driver::incremental_cache_detail {
namespace cache_format = incremental_cache_format;
using namespace cache_format;

namespace {

[[nodiscard]] query::PackageKey cache_source_package(const SourceFingerprintRecord& source) noexcept
{
    if (query::is_valid(source.package)) {
        return source.package;
    }
    return query::package_key(std::span<const std::string_view>{});
}

} // namespace

[[nodiscard]] query::QueryRecordChangeStatus cached_query_record_status(
    const query::QueryContext& cached_context, const query::QueryRecord& current)
{
    const query::QueryNode* const cached_node = cached_context.find(current.key);
    return query::query_record_change_status(cached_node == nullptr ? nullptr : &cached_node->record, current);
}

void reject_source_stage_reuse(SourceStageReuseSummary& summary, const std::string_view reason) noexcept
{
    if (summary.reusable) {
        summary.reusable = false;
        summary.reason = reason;
    }
}

void add_source_stage_query_status(SourceStageReuseSummary& summary, const query::QueryRecordChangeStatus status)
{
    ++summary.queries;
    switch (status) {
        case query::QueryRecordChangeStatus::missing:
            ++summary.missing;
            reject_source_stage_reuse(summary, INCREMENTAL_CACHE_PROFILE_REASON_MISSING_QUERY);
            break;
        case query::QueryRecordChangeStatus::unchanged:
            ++summary.unchanged;
            break;
        case query::QueryRecordChangeStatus::changed:
            ++summary.changed;
            reject_source_stage_reuse(summary, INCREMENTAL_CACHE_PROFILE_REASON_CHANGED_QUERY);
            break;
        case query::QueryRecordChangeStatus::malformed:
            ++summary.malformed;
            reject_source_stage_reuse(summary, INCREMENTAL_CACHE_PROFILE_REASON_MALFORMED_QUERY);
            break;
    }
}

void update_source_stage_reuse_summary_for_source(SourceStageReuseSummary& summary,
    const query::QueryContext& cached_context, const SourceFingerprintRecord& cached_source)
{
    ++summary.sources;
    const std::optional<SourceStageQueryRecords> current =
        source_stage_query_records_for_file(cached_source.path, cache_source_package(cached_source));
    if (!current) {
        ++summary.source_failures;
        reject_source_stage_reuse(summary, INCREMENTAL_CACHE_PROFILE_REASON_SOURCE_FAILURE);
        return;
    }
    add_source_stage_query_status(summary, cached_query_record_status(cached_context, current->lex_file));
    add_source_stage_query_status(summary, cached_query_record_status(cached_context, current->parse_file));
}

[[nodiscard]] bool cache_has_root_source(const ParsedCache& cache)
{
    for (const SourceFingerprintRecord& cached_source : cache.sources) {
        if (cached_source.path == cache.root_path) {
            return true;
        }
    }
    return false;
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

[[nodiscard]] bool cache_sources_match(const ParsedCache& cache)
{
    if (!cache_has_root_source(cache)) {
        return false;
    }
    for (const SourceFingerprintRecord& cached_source : cache.sources) {
        std::optional<SourceFingerprintRecord> current = fingerprint_file(cached_source.path);
        if (!current || !same_fingerprint(cached_source, *current)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] SourceStageReuseSummary source_stage_reuse_summary_for_cache(
    const ParsedCache& cache, const bool collect_all_statuses)
{
    SourceStageReuseSummary summary;
    if (!cache_has_root_source(cache)) {
        reject_source_stage_reuse(summary, INCREMENTAL_CACHE_PROFILE_REASON_MISSING_ROOT_SOURCE);
        return summary;
    }
    const query::QueryContext cached_context = seed_query_context_from_cache(cache);
    for (const SourceFingerprintRecord& cached_source : cache.sources) {
        update_source_stage_reuse_summary_for_source(summary, cached_context, cached_source);
        if (!summary.reusable && !collect_all_statuses) {
            break;
        }
    }
    return summary;
}

} // namespace aurex::driver::incremental_cache_detail
