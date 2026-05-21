#pragma once

#include "types.hpp"

#include <span>

namespace aurex::driver::incremental_cache_detail {

[[nodiscard]] base::usize total_query_execution_count(const QueryKindExecutionCounts& counts) noexcept;
void increment_query_kind_count(QueryKindExecutionCounts& counts, query::QueryKind kind) noexcept;
void increment_query_kind_count(QueryKindExecutionCounts& counts, QuerySubjectKind kind) noexcept;
[[nodiscard]] QueryKindExecutionCounts query_record_counts_by_kind(
    std::span<const query::QueryRecord> records) noexcept;

} // namespace aurex::driver::incremental_cache_detail
