#pragma once

#include <aurex/driver/profile.hpp>

#include <chrono>
#include <string>
#include <string_view>

#include "types.hpp"

namespace aurex::driver::incremental_cache_detail {

[[nodiscard]] std::string query_record_diff_summary_detail(const query::QueryReuseSummary& summary);
[[nodiscard]] std::string query_reuse_plan_summary_detail(const query::QueryReusePlan& plan);
[[nodiscard]] std::string query_pruning_summary_detail(const QueryPruningGateResult& result);
[[nodiscard]] std::string query_provider_evaluation_summary_detail(const QueryProviderEvaluationStats& stats);
[[nodiscard]] std::string project_input_summary_detail(bool reusable, std::string_view changed_inputs);
[[nodiscard]] std::string source_stage_reuse_summary_detail(const SourceStageReuseSummary& summary);

void record_query_record_diff_summary(CompilationProfiler* profiler, const query::QueryReuseSummary& summary,
    std::chrono::steady_clock::duration elapsed);
void record_query_reuse_plan_summary(
    CompilationProfiler* profiler, const query::QueryReusePlan& plan, std::chrono::steady_clock::duration elapsed);
void record_query_pruning_summary(
    CompilationProfiler* profiler, const QueryPruningGateResult& result, std::chrono::steady_clock::duration elapsed);
void record_query_provider_evaluation_summary(CompilationProfiler* profiler, const QueryProviderEvaluationStats& stats,
    std::chrono::steady_clock::duration elapsed);
void record_source_stage_reuse_summary(
    CompilationProfiler* profiler, const SourceStageReuseSummary& summary, std::chrono::steady_clock::duration elapsed);
void record_project_input_summary(CompilationProfiler* profiler, bool reusable, std::string_view changed_inputs,
    std::chrono::steady_clock::duration elapsed);

} // namespace aurex::driver::incremental_cache_detail
