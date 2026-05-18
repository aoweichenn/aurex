#pragma once

#include <aurex/query/item_signature_query.hpp>

#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace aurex::query {

enum class QueryNodeStatus : base::u8 {
    in_progress,
    done,
    failed,
};

enum class QueryEvaluationStatus : base::u8 {
    computed,
    cached,
    failed,
    cycle,
};

struct QueryNode {
    QueryKey key;
    QueryNodeStatus status = QueryNodeStatus::failed;
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

struct QueryEvaluationResult {
    QueryEvaluationStatus status = QueryEvaluationStatus::failed;
    const QueryNode* node = nullptr;
};

using ItemSignatureProvider =
    std::function<std::optional<ItemSignatureProviderOutput>(const ItemSignatureProviderInput&)>;

class QueryContext final {
public:
    QueryContext();
    explicit QueryContext(ItemSignatureProvider item_signature_provider);

    void set_item_signature_provider(ItemSignatureProvider provider);
    [[nodiscard]] QueryEvaluationResult evaluate_item_signature(const ItemSignatureProviderInput& input);
    [[nodiscard]] const QueryNode* find(QueryKey key) const;
    [[nodiscard]] std::vector<QueryRecord> completed_records() const;

private:
    [[nodiscard]] QueryNode& node_for(QueryKey key);
    [[nodiscard]] QueryEvaluationResult fail_query(QueryNode& node);

    std::unordered_map<QueryKey, QueryNode, QueryKeyHash> nodes_;
    ItemSignatureProvider item_signature_provider_;
};

} // namespace aurex::query
