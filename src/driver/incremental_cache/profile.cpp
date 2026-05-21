#include "profile.hpp"

#include "query_stats.hpp"

#include <sstream>

namespace aurex::driver::incremental_cache_detail {

[[nodiscard]] std::string query_record_diff_summary_detail(const query::QueryReuseSummary& summary)
{
    std::ostringstream detail;
    detail << INCREMENTAL_CACHE_PROFILE_TOTAL << summary.total << INCREMENTAL_CACHE_PROFILE_MISSING << summary.missing
           << INCREMENTAL_CACHE_PROFILE_UNCHANGED << summary.unchanged << INCREMENTAL_CACHE_PROFILE_CHANGED
           << summary.changed << INCREMENTAL_CACHE_PROFILE_MALFORMED << summary.malformed;
    return detail.str();
}

[[nodiscard]] std::string query_reuse_plan_summary_detail(const query::QueryReusePlan& plan)
{
    std::ostringstream detail;
    detail << INCREMENTAL_CACHE_PROFILE_REUSABLE << plan.reusable.size() << INCREMENTAL_CACHE_PROFILE_RECOMPUTE_ROOTS
           << plan.recompute_roots.size() << INCREMENTAL_CACHE_PROFILE_PROPAGATED_RECOMPUTE
           << plan.propagated_recompute.size() << INCREMENTAL_CACHE_PROFILE_RECOMPUTE << plan.recompute.size();
    return detail.str();
}

[[nodiscard]] std::string query_pruning_summary_detail(const QueryPruningGateResult& result)
{
    std::ostringstream detail;
    detail << INCREMENTAL_CACHE_PROFILE_PRUNING_ENABLED << (result.enabled ? 1 : 0)
           << INCREMENTAL_CACHE_PROFILE_PRUNING_APPLIED << (result.applied ? 1 : 0)
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED << total_query_execution_count(result.reused)
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED << total_query_execution_count(result.recomputed)
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_FILE_CONTENTS << result.reused.file_contents
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_LEX_FILES << result.reused.lex_files
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_PARSE_FILES << result.reused.parse_files
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_MODULE_GRAPHS << result.reused.module_graphs
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_MODULE_EXPORTS << result.reused.module_exports
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_ITEM_LISTS << result.reused.item_lists
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_ITEM_SIGNATURES << result.reused.item_signatures
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_FUNCTION_BODY_SYNTAXES << result.reused.function_body_syntaxes
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_TYPE_CHECK_BODIES << result.reused.type_check_bodies
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_GENERIC_TEMPLATE_SIGNATURES
           << result.reused.generic_template_signatures
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_GENERIC_INSTANCE_SIGNATURES
           << result.reused.generic_instance_signatures
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_GENERIC_INSTANCE_BODIES << result.reused.generic_instance_bodies
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_LOWER_FUNCTION_IRS << result.reused.lower_function_irs
           << INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_DIAGNOSTICS << result.reused.diagnostics
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_FILE_CONTENTS << result.recomputed.file_contents
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_LEX_FILES << result.recomputed.lex_files
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_PARSE_FILES << result.recomputed.parse_files
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_MODULE_GRAPHS << result.recomputed.module_graphs
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_MODULE_EXPORTS << result.recomputed.module_exports
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_ITEM_LISTS << result.recomputed.item_lists
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_ITEM_SIGNATURES << result.recomputed.item_signatures
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_FUNCTION_BODY_SYNTAXES
           << result.recomputed.function_body_syntaxes << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_TYPE_CHECK_BODIES
           << result.recomputed.type_check_bodies
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_GENERIC_TEMPLATE_SIGNATURES
           << result.recomputed.generic_template_signatures
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_GENERIC_INSTANCE_SIGNATURES
           << result.recomputed.generic_instance_signatures
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_GENERIC_INSTANCE_BODIES
           << result.recomputed.generic_instance_bodies
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_LOWER_FUNCTION_IRS << result.recomputed.lower_function_irs
           << INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_DIAGNOSTICS << result.recomputed.diagnostics
           << INCREMENTAL_CACHE_PROFILE_PRUNING_FALLBACK << result.fallback;
    return detail.str();
}

[[nodiscard]] std::string query_provider_evaluation_summary_detail(const QueryProviderEvaluationStats& stats)
{
    std::ostringstream detail;
    detail << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_MODE
           << (stats.pruned ? INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_MODE_PRUNED
                            : INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_MODE_FULL)
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED << total_query_execution_count(stats.seeded)
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED << total_query_execution_count(stats.evaluated)
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_FILE_CONTENTS << stats.seeded.file_contents
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_LEX_FILES << stats.seeded.lex_files
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_PARSE_FILES << stats.seeded.parse_files
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_MODULE_GRAPHS << stats.seeded.module_graphs
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_MODULE_EXPORTS << stats.seeded.module_exports
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_ITEM_LISTS << stats.seeded.item_lists
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_ITEM_SIGNATURES << stats.seeded.item_signatures
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_FUNCTION_BODY_SYNTAXES
           << stats.seeded.function_body_syntaxes << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_TYPE_CHECK_BODIES
           << stats.seeded.type_check_bodies
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_GENERIC_TEMPLATE_SIGNATURES
           << stats.seeded.generic_template_signatures
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_GENERIC_INSTANCE_SIGNATURES
           << stats.seeded.generic_instance_signatures
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_GENERIC_INSTANCE_BODIES
           << stats.seeded.generic_instance_bodies << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_LOWER_FUNCTION_IRS
           << stats.seeded.lower_function_irs << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_DIAGNOSTICS
           << stats.seeded.diagnostics << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_FILE_CONTENTS
           << stats.evaluated.file_contents << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_LEX_FILES
           << stats.evaluated.lex_files << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_PARSE_FILES
           << stats.evaluated.parse_files << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_MODULE_GRAPHS
           << stats.evaluated.module_graphs << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_MODULE_EXPORTS
           << stats.evaluated.module_exports << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_ITEM_LISTS
           << stats.evaluated.item_lists << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_ITEM_SIGNATURES
           << stats.evaluated.item_signatures
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_FUNCTION_BODY_SYNTAXES
           << stats.evaluated.function_body_syntaxes
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_TYPE_CHECK_BODIES << stats.evaluated.type_check_bodies
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_GENERIC_TEMPLATE_SIGNATURES
           << stats.evaluated.generic_template_signatures
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_GENERIC_INSTANCE_SIGNATURES
           << stats.evaluated.generic_instance_signatures
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_GENERIC_INSTANCE_BODIES
           << stats.evaluated.generic_instance_bodies
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_LOWER_FUNCTION_IRS << stats.evaluated.lower_function_irs
           << INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_DIAGNOSTICS << stats.evaluated.diagnostics;
    return detail.str();
}

[[nodiscard]] std::string source_stage_reuse_summary_detail(const SourceStageReuseSummary& summary)
{
    std::ostringstream detail;
    detail << INCREMENTAL_CACHE_PROFILE_RESULT
           << (summary.reusable ? INCREMENTAL_CACHE_PROFILE_REUSE : INCREMENTAL_CACHE_PROFILE_REJECT)
           << INCREMENTAL_CACHE_PROFILE_REASON << summary.reason << INCREMENTAL_CACHE_PROFILE_SOURCES << summary.sources
           << INCREMENTAL_CACHE_PROFILE_QUERIES << summary.queries << INCREMENTAL_CACHE_PROFILE_UNCHANGED
           << summary.unchanged << INCREMENTAL_CACHE_PROFILE_MISSING << summary.missing
           << INCREMENTAL_CACHE_PROFILE_CHANGED << summary.changed << INCREMENTAL_CACHE_PROFILE_MALFORMED
           << summary.malformed << INCREMENTAL_CACHE_PROFILE_SOURCE_FAILURES << summary.source_failures;
    return detail.str();
}

void record_query_record_diff_summary(CompilationProfiler* const profiler, const query::QueryReuseSummary& summary,
    const std::chrono::steady_clock::duration elapsed)
{
    if (profiler == nullptr || !profiler->enabled()) {
        return;
    }
    profiler->record(INCREMENTAL_CACHE_PROFILE_QUERY_DIFF, query_record_diff_summary_detail(summary), elapsed);
}

void record_query_reuse_plan_summary(CompilationProfiler* const profiler, const query::QueryReusePlan& plan,
    const std::chrono::steady_clock::duration elapsed)
{
    if (profiler == nullptr || !profiler->enabled()) {
        return;
    }
    profiler->record(INCREMENTAL_CACHE_PROFILE_QUERY_PLAN, query_reuse_plan_summary_detail(plan), elapsed);
}

void record_query_pruning_summary(CompilationProfiler* const profiler, const QueryPruningGateResult& result,
    const std::chrono::steady_clock::duration elapsed)
{
    if (!result.enabled || profiler == nullptr || !profiler->enabled()) {
        return;
    }
    profiler->record(INCREMENTAL_CACHE_PROFILE_QUERY_PRUNING, query_pruning_summary_detail(result), elapsed);
}

void record_query_provider_evaluation_summary(CompilationProfiler* const profiler,
    const QueryProviderEvaluationStats& stats, const std::chrono::steady_clock::duration elapsed)
{
    if (!stats.pruned || profiler == nullptr || !profiler->enabled()) {
        return;
    }
    profiler->record(
        INCREMENTAL_CACHE_PROFILE_QUERY_PROVIDER_EVAL, query_provider_evaluation_summary_detail(stats), elapsed);
}

void record_source_stage_reuse_summary(CompilationProfiler* const profiler, const SourceStageReuseSummary& summary,
    const std::chrono::steady_clock::duration elapsed)
{
    if (profiler == nullptr || !profiler->enabled()) {
        return;
    }
    profiler->record(INCREMENTAL_CACHE_PROFILE_SOURCE_STAGE_REUSE, source_stage_reuse_summary_detail(summary), elapsed);
}

} // namespace aurex::driver::incremental_cache_detail
