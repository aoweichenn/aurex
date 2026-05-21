#pragma once

#include <aurex/query/query_graph.hpp>

#include <span>
#include <string>
#include <vector>

namespace aurex::query {

class QueryContext;

enum class QueryReuseDisposition : base::u8 {
    reuse,
    recompute,
};

struct QueryReuseDecision {
    QueryKey key;
    std::string stable_key_bytes;
    QueryRecordChangeStatus change_status = QueryRecordChangeStatus::malformed;
    QueryReuseDisposition disposition = QueryReuseDisposition::recompute;
};

struct QueryReuseSummary {
    base::usize total = 0;
    base::usize missing = 0;
    base::usize unchanged = 0;
    base::usize changed = 0;
    base::usize malformed = 0;
    base::usize reusable = 0;
    base::usize recompute = 0;
};

struct QueryReusePlan {
    std::vector<QueryReuseDecision> decisions;
    QueryReuseSummary summary;
    std::vector<QueryKey> reusable;
    std::vector<QueryKey> recompute_roots;
    std::vector<QueryKey> propagated_recompute;
    std::vector<QueryKey> recompute;
};

[[nodiscard]] QueryReuseDisposition query_reuse_disposition(QueryRecordChangeStatus status) noexcept;
[[nodiscard]] bool can_reuse(QueryRecordChangeStatus status) noexcept;
[[nodiscard]] std::vector<QueryReuseDecision> decide_query_reuse(
    const QueryContext& cached_context, std::span<const QueryRecord> current_records);
[[nodiscard]] std::vector<QueryReuseDecision> mark_all_queries_missing(std::span<const QueryRecord> current_records);
[[nodiscard]] QueryReuseSummary summarize_query_reuse(std::span<const QueryReuseDecision> decisions) noexcept;
[[nodiscard]] QueryReusePlan build_query_reuse_plan(
    const QueryContext& cached_context, std::vector<QueryReuseDecision> decisions);
[[nodiscard]] QueryReusePlan build_query_reuse_plan(
    const QueryContext& cached_context, std::span<const QueryRecord> current_records);
[[nodiscard]] QueryReusePlan mark_all_queries_recompute(std::span<const QueryRecord> current_records);

} // namespace aurex::query
