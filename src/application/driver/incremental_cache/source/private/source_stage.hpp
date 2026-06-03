#pragma once

#include <application/driver/incremental_cache/core/private/types.hpp>

namespace aurex::driver::incremental_cache_detail {

[[nodiscard]] query::QueryContext seed_query_context_from_cache(const ParsedCache& cache);
[[nodiscard]] bool cache_sources_match(const ParsedCache& cache);
[[nodiscard]] SourceStageReuseSummary source_stage_reuse_summary_for_cache(
    const ParsedCache& cache, bool collect_all_statuses);

} // namespace aurex::driver::incremental_cache_detail
