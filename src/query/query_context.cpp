#include <aurex/query/query_context.hpp>

#include <algorithm>
#include <tuple>
#include <utility>

namespace aurex::query {
namespace {

[[nodiscard]] bool query_record_key_less(const QueryRecord& lhs, const QueryRecord& rhs) noexcept
{
    return std::tie(lhs.key.kind, lhs.key.global_id, lhs.result.global_id, lhs.stable_key_bytes)
        < std::tie(rhs.key.kind, rhs.key.global_id, rhs.result.global_id, rhs.stable_key_bytes);
}

} // namespace

QueryContext::QueryContext() : QueryContext(ItemSignatureProvider{provide_item_signature_query})
{
}

QueryContext::QueryContext(ItemSignatureProvider item_signature_provider)
{
    this->set_item_signature_provider(std::move(item_signature_provider));
}

void QueryContext::set_item_signature_provider(ItemSignatureProvider provider)
{
    this->item_signature_provider_ =
        provider ? std::move(provider) : ItemSignatureProvider{provide_item_signature_query};
}

QueryEvaluationResult QueryContext::evaluate_item_signature(const ItemSignatureProviderInput& input)
{
    const std::optional<QueryKey> expected_key = item_signature_query_key(input.key);
    if (!expected_key) {
        return {};
    }

    QueryNode& node = this->node_for(*expected_key);
    if (node.status == QueryNodeStatus::done) {
        return QueryEvaluationResult{
            QueryEvaluationStatus::cached,
            &node,
        };
    }
    if (node.status == QueryNodeStatus::in_progress) {
        return QueryEvaluationResult{
            QueryEvaluationStatus::cycle,
            &node,
        };
    }

    node.status = QueryNodeStatus::in_progress;
    node.record = {};
    node.result = {};
    node.dependencies.clear();

    std::optional<ItemSignatureProviderOutput> output = this->item_signature_provider_(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    node.record = std::move(output->record);
    node.result = output->result;
    node.dependencies = std::move(output->dependencies);
    node.status = QueryNodeStatus::done;
    return QueryEvaluationResult{
        QueryEvaluationStatus::computed,
        &node,
    };
}

const QueryNode* QueryContext::find(const QueryKey key) const
{
    const auto found = this->nodes_.find(key);
    return found == this->nodes_.end() ? nullptr : &found->second;
}

std::vector<QueryRecord> QueryContext::completed_records() const
{
    std::vector<QueryRecord> records;
    records.reserve(this->nodes_.size());
    for (const auto& entry : this->nodes_) {
        const QueryNode& node = entry.second;
        if (node.status == QueryNodeStatus::done) {
            records.push_back(node.record);
        }
    }
    std::sort(records.begin(), records.end(), query_record_key_less);
    return records;
}

QueryNode& QueryContext::node_for(const QueryKey key)
{
    const auto inserted = this->nodes_.try_emplace(key,
        QueryNode{
            key,
            QueryNodeStatus::failed,
            {},
            {},
            {},
        });
    return inserted.first->second;
}

QueryEvaluationResult QueryContext::fail_query(QueryNode& node)
{
    node.status = QueryNodeStatus::failed;
    node.record = {};
    node.result = {};
    node.dependencies.clear();
    return QueryEvaluationResult{
        QueryEvaluationStatus::failed,
        &node,
    };
}

} // namespace aurex::query
