#include <aurex/query/query_reuse.hpp>

#include <algorithm>
#include <tuple>
#include <utility>

namespace aurex::query {
namespace {

[[nodiscard]] bool query_key_less(const QueryKey lhs, const QueryKey rhs) noexcept
{
    return std::tie(
               lhs.kind, lhs.schema, lhs.global_id, lhs.payload.primary, lhs.payload.secondary, lhs.payload.byte_count)
        < std::tie(
            rhs.kind, rhs.schema, rhs.global_id, rhs.payload.primary, rhs.payload.secondary, rhs.payload.byte_count);
}

[[nodiscard]] bool contains_query_key(const std::vector<QueryKey>& keys, const QueryKey key)
{
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

void push_unique_query_key(std::vector<QueryKey>& keys, const QueryKey key)
{
    if (!is_valid(key) || contains_query_key(keys, key)) {
        return;
    }
    keys.push_back(key);
}

void remove_query_key(std::vector<QueryKey>& keys, const QueryKey key)
{
    std::erase(keys, key);
}

void sort_query_keys(std::vector<QueryKey>& keys)
{
    std::sort(keys.begin(), keys.end(), query_key_less);
}

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

void propagate_recompute_dependents(const QueryContext& cached_context, QueryReusePlan& plan)
{
    std::vector<QueryKey> worklist = plan.recompute_roots;
    for (base::usize index = 0; index < worklist.size(); ++index) {
        const std::vector<QueryKey> dependents = cached_context.dependents_of(worklist[index]);
        for (const QueryKey dependent : dependents) {
            if (contains_query_key(plan.recompute, dependent)) {
                continue;
            }
            if (contains_query_key(plan.reusable, dependent)) {
                continue;
            }
            push_unique_query_key(plan.propagated_recompute, dependent);
            push_unique_query_key(plan.recompute, dependent);
            worklist.push_back(dependent);
        }
    }
}

void finalize_query_reuse_plan(QueryReusePlan& plan)
{
    sort_query_keys(plan.reusable);
    sort_query_keys(plan.recompute_roots);
    sort_query_keys(plan.propagated_recompute);
    sort_query_keys(plan.recompute);
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

QueryReusePlan build_query_reuse_plan(const QueryContext& cached_context, std::vector<QueryReuseDecision> decisions)
{
    QueryReusePlan plan;
    plan.decisions = std::move(decisions);
    plan.summary = summarize_query_reuse(plan.decisions);

    for (const QueryReuseDecision& decision : plan.decisions) {
        switch (decision.disposition) {
            case QueryReuseDisposition::reuse:
                if (!contains_query_key(plan.recompute, decision.key)) {
                    push_unique_query_key(plan.reusable, decision.key);
                }
                break;
            case QueryReuseDisposition::recompute:
                remove_query_key(plan.reusable, decision.key);
                push_unique_query_key(plan.recompute_roots, decision.key);
                push_unique_query_key(plan.recompute, decision.key);
                break;
        }
    }

    propagate_recompute_dependents(cached_context, plan);
    finalize_query_reuse_plan(plan);
    return plan;
}

QueryReusePlan build_query_reuse_plan(
    const QueryContext& cached_context, const std::span<const QueryRecord> current_records)
{
    return build_query_reuse_plan(cached_context, decide_query_reuse(cached_context, current_records));
}

QueryReusePlan mark_all_queries_recompute(const std::span<const QueryRecord> current_records)
{
    QueryContext empty_cache;
    return build_query_reuse_plan(empty_cache, mark_all_queries_missing(current_records));
}

} // namespace aurex::query
