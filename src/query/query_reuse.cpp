#include <aurex/query/query_reuse.hpp>

namespace aurex::query {
namespace {

[[nodiscard]] const QueryRecord* cached_record_for(const QueryContext& cached_context, const QueryRecord& current)
{
    const QueryNode* const node = cached_context.find(current.key);
    if (node == nullptr || node->record.stable_key_bytes != current.stable_key_bytes) {
        return nullptr;
    }
    return &node->record;
}

[[nodiscard]] QueryReuseDecision query_reuse_decision(const QueryRecord* const cached, const QueryRecord& current)
{
    const QueryRecordChangeStatus change_status = query_record_change_status(cached, current);
    return QueryReuseDecision{
        current.key,
        current.stable_key_bytes,
        change_status,
        query_reuse_disposition(change_status),
    };
}

} // namespace

QueryReuseDisposition query_reuse_disposition(const QueryRecordChangeStatus status) noexcept
{
    return status == QueryRecordChangeStatus::unchanged ? QueryReuseDisposition::reuse
                                                        : QueryReuseDisposition::recompute;
}

bool can_reuse(const QueryRecordChangeStatus status) noexcept
{
    return query_reuse_disposition(status) == QueryReuseDisposition::reuse;
}

std::vector<QueryReuseDecision> decide_query_reuse(
    const QueryContext& cached_context, const std::span<const QueryRecord> current_records)
{
    std::vector<QueryReuseDecision> decisions;
    decisions.reserve(current_records.size());
    for (const QueryRecord& current : current_records) {
        decisions.push_back(query_reuse_decision(cached_record_for(cached_context, current), current));
    }
    return decisions;
}

std::vector<QueryReuseDecision> mark_all_queries_missing(const std::span<const QueryRecord> current_records)
{
    std::vector<QueryReuseDecision> decisions;
    decisions.reserve(current_records.size());
    for (const QueryRecord& current : current_records) {
        decisions.push_back(query_reuse_decision(nullptr, current));
    }
    return decisions;
}

QueryReuseSummary summarize_query_reuse(const std::span<const QueryReuseDecision> decisions) noexcept
{
    QueryReuseSummary summary;
    summary.total = decisions.size();
    for (const QueryReuseDecision& decision : decisions) {
        switch (decision.change_status) {
            case QueryRecordChangeStatus::missing:
                summary.missing += 1;
                break;
            case QueryRecordChangeStatus::unchanged:
                summary.unchanged += 1;
                break;
            case QueryRecordChangeStatus::changed:
                summary.changed += 1;
                break;
            case QueryRecordChangeStatus::malformed:
                summary.malformed += 1;
                break;
        }

        switch (decision.disposition) {
            case QueryReuseDisposition::reuse:
                summary.reusable += 1;
                break;
            case QueryReuseDisposition::recompute:
                summary.recompute += 1;
                break;
        }
    }
    return summary;
}

} // namespace aurex::query
