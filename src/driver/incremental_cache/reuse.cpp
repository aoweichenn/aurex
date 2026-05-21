#include "io.hpp"
#include "query_stats.hpp"
#include "reuse.hpp"
#include "schedule.hpp"
#include "source_stage.hpp"
#include "subjects.hpp"

#include <aurex/query/query_edge_verifier.hpp>

#include <utility>
#include <vector>

namespace aurex::driver::incremental_cache_detail {
namespace cache_format = incremental_cache_format;
using namespace cache_format;

[[nodiscard]] std::string_view parsed_cache_validation_fallback(const ParsedCacheValidationStatus status) noexcept
{
    switch (status) {
        case ParsedCacheValidationStatus::valid:
            return INCREMENTAL_CACHE_PRUNING_FALLBACK_NONE;
        case ParsedCacheValidationStatus::malformed_cache:
            return INCREMENTAL_CACHE_PRUNING_FALLBACK_MALFORMED_CACHE;
        case ParsedCacheValidationStatus::malformed_query_graph:
            return INCREMENTAL_CACHE_PRUNING_FALLBACK_MALFORMED_QUERY_GRAPH;
        case ParsedCacheValidationStatus::malformed_query_identity:
            return INCREMENTAL_CACHE_PRUNING_FALLBACK_MALFORMED_QUERY_IDENTITY;
    }
}

[[nodiscard]] QueryReuseEvaluation query_reuse_evaluation_fallback(
    const std::span<const query::QueryRecord> current_records, const std::string_view fallback)
{
    return QueryReuseEvaluation{
        query::mark_all_queries_recompute(current_records),
        query::QueryContext{},
        false,
        fallback,
    };
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
        INCREMENTAL_CACHE_PRUNING_FALLBACK_NONE,
    };
}

[[nodiscard]] QueryReuseEvaluation build_existing_query_reuse_evaluation(
    const std::filesystem::path& cache_path, const std::span<const query::QueryRecord> current_records)
{
    const ParsedCacheReadResult read = read_incremental_cache_with_status(cache_path);
    if (!read.cache) {
        const std::string_view fallback = read.status == ParsedCacheReadStatus::missing
            ? INCREMENTAL_CACHE_PRUNING_FALLBACK_NO_CACHE
            : INCREMENTAL_CACHE_PRUNING_FALLBACK_MALFORMED_CACHE;
        return query_reuse_evaluation_fallback(current_records, fallback);
    }

    const ParsedCacheValidationStatus validation = parsed_cache_validation_status(*read.cache);
    if (validation != ParsedCacheValidationStatus::valid) {
        return query_reuse_evaluation_fallback(current_records, parsed_cache_validation_fallback(validation));
    }
    return build_query_reuse_evaluation_against_cache(*read.cache, current_records);
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
    if (!invocation.query_pruning_enabled) {
        return query_pruning_fallback(false, current_records, INCREMENTAL_CACHE_PRUNING_FALLBACK_DISABLED);
    }
    if (!evaluation.cache_loaded) {
        return query_pruning_fallback(true, current_records, evaluation.fallback);
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

[[nodiscard]] bool query_record_key_exists(
    const std::vector<query::QueryRecord>& records, const query::QueryKey key) noexcept
{
    for (const query::QueryRecord& record : records) {
        if (record.key == key) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] const query::QueryRecord* query_record_for_key(
    const std::vector<query::QueryRecord>& records, const query::QueryKey key) noexcept
{
    for (const query::QueryRecord& record : records) {
        if (record.key == key) {
            return &record;
        }
    }
    return nullptr;
}

[[nodiscard]] bool query_dependencies_exist_in_records(
    const std::vector<query::QueryRecord>& records, const std::vector<query::QueryKey>& dependencies) noexcept
{
    for (const query::QueryKey dependency : dependencies) {
        if (!query_record_key_exists(records, dependency)) {
            return false;
        }
    }
    return true;
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
        std::vector<query::QueryKey> dependencies = evaluation.cached_context.dependencies_for(cached->key);
        if (!query_dependencies_exist_in_records(collection.records, dependencies)) {
            return false;
        }
        if (!context.seed_completed_record(*cached, std::move(dependencies))) {
            return false;
        }
        increment_query_kind_count(stats.seeded, subject.kind);
    }
    return true;
}

[[nodiscard]] std::vector<query::QueryRecord> completed_records_in_subject_order(
    const query::QueryContext& context, const QuerySubjectCollection& collection)
{
    std::vector<query::QueryRecord> records;
    records.reserve(collection.subjects.size());
    for (const QuerySubject& subject : collection.subjects) {
        const query::QueryNode* const node = context.find(subject.record.key);
        if (node != nullptr && node->status == query::QueryNodeStatus::done) {
            records.push_back(node->record);
        }
    }
    return records;
}

[[nodiscard]] bool query_collection_records_and_dependency_edges_are_valid(const QueryCollection& collection)
{
    for (const query::QueryRecord& record : collection.records) {
        if (!query::query_record_stable_identity_is_valid(record)) {
            return false;
        }
    }
    for (const query::QueryDependencyEdge& edge : collection.dependency_edges) {
        const query::QueryRecord* const dependent = query_record_for_key(collection.records, edge.dependent);
        const query::QueryRecord* const dependency = query_record_for_key(collection.records, edge.dependency);
        if (dependent == nullptr || dependency == nullptr || !query_dependency_edge_schedule_is_valid(edge)
            || !query::query_dependency_edge_records_are_valid(*dependent, *dependency)) {
            return false;
        }
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
            completed_records_in_subject_order(context, collection),
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
            completed_records_in_subject_order(context, collection),
            context.dependency_edges(),
        },
        stats,
    };
}


} // namespace aurex::driver::incremental_cache_detail
