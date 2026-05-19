#pragma once

#include <aurex/query/query_context.hpp>

#include <span>
#include <string>
#include <vector>

namespace aurex::query {

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

[[nodiscard]] QueryReuseDisposition query_reuse_disposition(QueryRecordChangeStatus status) noexcept;
[[nodiscard]] bool can_reuse(QueryRecordChangeStatus status) noexcept;
[[nodiscard]] std::vector<QueryReuseDecision> decide_query_reuse(
    const QueryContext& cached_context, std::span<const QueryRecord> current_records);
[[nodiscard]] std::vector<QueryReuseDecision> mark_all_queries_missing(std::span<const QueryRecord> current_records);
[[nodiscard]] QueryReuseSummary summarize_query_reuse(std::span<const QueryReuseDecision> decisions) noexcept;

} // namespace aurex::query
