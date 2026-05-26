#pragma once

#include <aurex/driver/invocation.hpp>

#include <filesystem>
#include <span>

#include "types.hpp"

namespace aurex::driver::incremental_cache_detail {

[[nodiscard]] QueryReuseEvaluation build_existing_query_reuse_evaluation(
    const std::filesystem::path& cache_path, std::span<const query::QueryRecord> current_records);
[[nodiscard]] QueryPruningGateResult apply_query_pruning_gate(const CompilerInvocation& invocation,
    const QueryReuseEvaluation& evaluation, std::span<const query::QueryRecord> current_records);
[[nodiscard]] QueryCollectionResult collect_queries_from_subjects(const QuerySubjectCollection& collection);
[[nodiscard]] QueryCollectionResult collect_queries_from_pruned_subjects(
    const QuerySubjectCollection& collection, const QueryReuseEvaluation& evaluation);
[[nodiscard]] bool query_collection_records_and_dependency_edges_are_valid(const QueryCollection& collection);

} // namespace aurex::driver::incremental_cache_detail
