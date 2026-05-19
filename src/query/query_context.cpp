#include <aurex/query/query_context.hpp>

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

[[nodiscard]] bool query_record_key_less(const QueryRecord& lhs, const QueryRecord& rhs) noexcept
{
    return std::tie(lhs.key.kind, lhs.key.global_id, lhs.result.global_id, lhs.stable_key_bytes)
        < std::tie(rhs.key.kind, rhs.key.global_id, rhs.result.global_id, rhs.stable_key_bytes);
}

[[nodiscard]] bool query_dependency_edge_less(const QueryDependencyEdge& lhs, const QueryDependencyEdge& rhs) noexcept
{
    if (lhs.dependent != rhs.dependent) {
        return query_key_less(lhs.dependent, rhs.dependent);
    }
    return query_key_less(lhs.dependency, rhs.dependency);
}

[[nodiscard]] bool has_invalid_dependency(const std::vector<QueryKey>& dependencies) noexcept
{
    for (const QueryKey dependency : dependencies) {
        if (!is_valid(dependency)) {
            return true;
        }
    }
    return false;
}

void normalize_dependencies(std::vector<QueryKey>& dependencies)
{
    std::sort(dependencies.begin(), dependencies.end(), query_key_less);
    dependencies.erase(std::unique(dependencies.begin(), dependencies.end()), dependencies.end());
}

} // namespace

QueryContext::QueryContext()
    : QueryContext(ItemSignatureProvider{provide_item_signature_query},
          GenericInstanceSignatureProvider{provide_generic_instance_signature_query})
{
}

QueryContext::QueryContext(ItemSignatureProvider item_signature_provider)
    : QueryContext(std::move(item_signature_provider),
          GenericInstanceSignatureProvider{provide_generic_instance_signature_query})
{
}

QueryContext::QueryContext(
    ItemSignatureProvider item_signature_provider, GenericInstanceSignatureProvider generic_instance_signature_provider)
{
    this->set_item_signature_provider(std::move(item_signature_provider));
    this->set_generic_instance_signature_provider(std::move(generic_instance_signature_provider));
}

void QueryContext::set_item_signature_provider(ItemSignatureProvider provider)
{
    this->item_signature_provider_ =
        provider ? std::move(provider) : ItemSignatureProvider{provide_item_signature_query};
}

void QueryContext::set_generic_instance_signature_provider(GenericInstanceSignatureProvider provider)
{
    this->generic_instance_signature_provider_ =
        provider ? std::move(provider) : GenericInstanceSignatureProvider{provide_generic_instance_signature_query};
}

QueryEvaluationResult QueryContext::evaluate_item_signature(const ItemSignatureProviderInput& input)
{
    const std::optional<QueryKey> expected_key = item_signature_query_key(input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<ItemSignatureProviderOutput> output = this->item_signature_provider_(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
    return QueryEvaluationResult{
        QueryEvaluationStatus::computed,
        &node,
    };
}

QueryEvaluationResult QueryContext::evaluate_generic_instance_signature(
    const GenericInstanceSignatureProviderInput& input)
{
    const std::optional<QueryKey> expected_key =
        input.key == nullptr ? std::nullopt : generic_instance_signature_query_key(*input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<GenericInstanceSignatureProviderOutput> output = this->generic_instance_signature_provider_(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
    return QueryEvaluationResult{
        QueryEvaluationStatus::computed,
        &node,
    };
}

bool QueryContext::seed_completed_record(QueryRecord record, std::vector<QueryKey> dependencies)
{
    if (!is_valid(record) || has_invalid_dependency(dependencies)) {
        return false;
    }

    QueryNode& node = this->node_for(record.key);
    if (node.status == QueryNodeStatus::done || node.status == QueryNodeStatus::in_progress) {
        return false;
    }

    const QueryResultFingerprint result = record.result;
    this->complete_query(node, std::move(record), result, std::move(dependencies));
    return true;
}

bool QueryContext::invalidate(const QueryKey key)
{
    const auto found = this->nodes_.find(key);
    if (found == this->nodes_.end() || found->second.status != QueryNodeStatus::done) {
        return false;
    }

    QueryNode& node = found->second;
    this->remove_dependency_edges(node);
    node.status = QueryNodeStatus::failed;
    node.record = {};
    node.result = {};
    node.dependencies.clear();
    return true;
}

const QueryNode* QueryContext::find(const QueryKey key) const
{
    const auto found = this->nodes_.find(key);
    return found == this->nodes_.end() ? nullptr : &found->second;
}

std::vector<QueryKey> QueryContext::dependencies_for(const QueryKey key) const
{
    const QueryNode* const node = this->find(key);
    if (node == nullptr || node->status != QueryNodeStatus::done) {
        return {};
    }
    return node->dependencies;
}

std::vector<QueryKey> QueryContext::dependents_of(const QueryKey key) const
{
    const auto found = this->dependents_by_dependency_.find(key);
    if (found == this->dependents_by_dependency_.end()) {
        return {};
    }
    return found->second;
}

std::vector<QueryDependencyEdge> QueryContext::dependency_edges() const
{
    std::vector<QueryDependencyEdge> edges;
    edges.reserve(this->dependency_edge_count_);
    for (const auto& entry : this->nodes_) {
        const QueryNode& node = entry.second;
        if (node.status != QueryNodeStatus::done) {
            continue;
        }
        for (const QueryKey dependency : node.dependencies) {
            edges.push_back(QueryDependencyEdge{
                node.key,
                dependency,
            });
        }
    }
    std::sort(edges.begin(), edges.end(), query_dependency_edge_less);
    return edges;
}

bool QueryContext::has_dependency(const QueryKey dependent, const QueryKey dependency) const
{
    const auto found = this->dependents_by_dependency_.find(dependency);
    if (found == this->dependents_by_dependency_.end()) {
        return false;
    }
    const std::vector<QueryKey>& dependents = found->second;
    return std::binary_search(dependents.begin(), dependents.end(), dependent, query_key_less);
}

base::usize QueryContext::dependency_edge_count() const noexcept
{
    return this->dependency_edge_count_;
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

QueryContext::QueryEvaluationStart QueryContext::start_query(const QueryKey key)
{
    QueryNode& node = this->node_for(key);
    if (node.status == QueryNodeStatus::done) {
        return QueryEvaluationStart{
            &node,
            QueryEvaluationResult{
                QueryEvaluationStatus::cached,
                &node,
            },
        };
    }
    if (node.status == QueryNodeStatus::in_progress) {
        return QueryEvaluationStart{
            &node,
            QueryEvaluationResult{
                QueryEvaluationStatus::cycle,
                &node,
            },
        };
    }

    node.status = QueryNodeStatus::in_progress;
    this->remove_dependency_edges(node);
    node.record = {};
    node.result = {};
    node.dependencies.clear();
    return QueryEvaluationStart{
        &node,
        std::nullopt,
    };
}

void QueryContext::complete_query(
    QueryNode& node, QueryRecord record, const QueryResultFingerprint result, std::vector<QueryKey> dependencies)
{
    this->remove_dependency_edges(node);
    normalize_dependencies(dependencies);
    node.record = std::move(record);
    node.result = result;
    node.dependencies = std::move(dependencies);
    node.status = QueryNodeStatus::done;
    this->add_dependency_edges(node);
}

void QueryContext::remove_dependency_edges(QueryNode& node)
{
    for (const QueryKey dependency : node.dependencies) {
        const auto found = this->dependents_by_dependency_.find(dependency);
        if (found == this->dependents_by_dependency_.end()) {
            continue;
        }
        std::vector<QueryKey>& dependents = found->second;
        const base::usize removed = std::erase(dependents, node.key);
        this->dependency_edge_count_ -= removed;
        if (dependents.empty()) {
            this->dependents_by_dependency_.erase(found);
        }
    }
}

void QueryContext::add_dependency_edges(const QueryNode& node)
{
    for (const QueryKey dependency : node.dependencies) {
        std::vector<QueryKey>& dependents = this->dependents_by_dependency_[dependency];
        if (std::binary_search(dependents.begin(), dependents.end(), node.key, query_key_less)) {
            continue;
        }
        dependents.push_back(node.key);
        std::sort(dependents.begin(), dependents.end(), query_key_less);
        ++this->dependency_edge_count_;
    }
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
    this->remove_dependency_edges(node);
    node.record = {};
    node.result = {};
    node.dependencies.clear();
    return QueryEvaluationResult{
        QueryEvaluationStatus::failed,
        &node,
    };
}

} // namespace aurex::query
