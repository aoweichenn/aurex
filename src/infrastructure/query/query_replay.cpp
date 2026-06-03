#include <aurex/infrastructure/query/query_context.hpp>
#include <aurex/infrastructure/query/query_edge_verifier.hpp>
#include <aurex/infrastructure/query/query_replay.hpp>

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

[[nodiscard]] bool query_dependency_edge_less(const QueryDependencyEdge& lhs, const QueryDependencyEdge& rhs) noexcept
{
    if (lhs.dependent != rhs.dependent) {
        return query_key_less(lhs.dependent, rhs.dependent);
    }
    return query_key_less(lhs.dependency, rhs.dependency);
}

[[nodiscard]] bool contains_query_key(const std::vector<QueryKey>& keys, const QueryKey key) noexcept
{
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

} // namespace

std::optional<QueryReplaySnapshot> make_query_replay_snapshot(const QueryContext& context)
{
    QueryReplaySnapshot snapshot;
    const std::vector<QueryRecord> records = context.completed_records();
    snapshot.nodes.reserve(records.size());
    for (const QueryRecord& record : records) {
        if (!query_record_stable_identity_is_valid(record)) {
            return std::nullopt;
        }
        const QueryNode* const node = context.find(record.key);
        if (node == nullptr || node->status != QueryNodeStatus::done || !is_valid(node->id)) {
            return std::nullopt;
        }
        snapshot.nodes.push_back(QueryReplayNode{
            node->id,
            node->key,
            node->record,
            node->dependencies,
        });
    }
    snapshot.edges = context.dependency_edges();
    return snapshot;
}

std::optional<QueryReplayIndex> QueryReplayIndex::build(QueryReplaySnapshot snapshot)
{
    QueryReplayIndex index{std::move(snapshot)};
    if (!index.rebuild_index()) {
        return std::nullopt;
    }
    return index;
}

std::optional<QueryReplayIndex> QueryReplayIndex::build(const QueryContext& context)
{
    std::optional<QueryReplaySnapshot> snapshot = make_query_replay_snapshot(context);
    if (!snapshot.has_value()) {
        return std::nullopt;
    }
    return QueryReplayIndex::build(std::move(*snapshot));
}

bool QueryReplayIndex::empty() const noexcept
{
    return this->snapshot_.nodes.empty();
}

base::usize QueryReplayIndex::size() const noexcept
{
    return this->snapshot_.nodes.size();
}

const QueryReplaySnapshot& QueryReplayIndex::snapshot() const noexcept
{
    return this->snapshot_;
}

const QueryReplayNode* QueryReplayIndex::find(const QueryKey key) const
{
    const auto found = this->index_by_key_.find(key);
    if (found == this->index_by_key_.end()) {
        return nullptr;
    }
    return &this->snapshot_.nodes[found->second];
}

const QueryReplayNode* QueryReplayIndex::find(const QueryNodeId id) const
{
    const auto found = this->index_by_node_id_.find(id);
    if (found == this->index_by_node_id_.end()) {
        return nullptr;
    }
    return &this->snapshot_.nodes[found->second];
}

const QueryRecord* QueryReplayIndex::record_for(const QueryKey key) const
{
    const QueryReplayNode* const node = this->find(key);
    return node == nullptr ? nullptr : &node->record;
}

std::vector<QueryKey> QueryReplayIndex::dependencies_for(const QueryKey key) const
{
    const QueryReplayNode* const node = this->find(key);
    return node == nullptr ? std::vector<QueryKey>{} : node->dependencies;
}

std::vector<QueryKey> QueryReplayIndex::dependents_of(const QueryKey key) const
{
    const auto found = this->dependents_by_dependency_.find(key);
    if (found == this->dependents_by_dependency_.end()) {
        return {};
    }
    return found->second;
}

bool QueryReplayIndex::has_dependency(const QueryKey dependent, const QueryKey dependency) const
{
    const auto found = this->dependents_by_dependency_.find(dependency);
    if (found == this->dependents_by_dependency_.end()) {
        return false;
    }
    const std::vector<QueryKey>& dependents = found->second;
    return std::binary_search(dependents.begin(), dependents.end(), dependent, query_key_less);
}

QueryReplayIndex::QueryReplayIndex(QueryReplaySnapshot snapshot) : snapshot_(std::move(snapshot))
{
}

bool QueryReplayIndex::rebuild_index()
{
    if (this->snapshot_.safety_mode != QueryReplaySafetyMode::immutable_snapshot) {
        return false;
    }

    this->index_by_key_.clear();
    this->index_by_node_id_.clear();
    this->dependents_by_dependency_.clear();
    for (base::usize index = 0; index < this->snapshot_.nodes.size(); ++index) {
        const QueryReplayNode& node = this->snapshot_.nodes[index];
        if (!is_valid(node.id) || !is_valid(node.key) || node.record.key != node.key
            || !query_record_stable_identity_is_valid(node.record)) {
            return false;
        }
        if (!this->index_by_key_.emplace(node.key, index).second
            || !this->index_by_node_id_.emplace(node.id, index).second) {
            return false;
        }
    }
    std::sort(this->snapshot_.edges.begin(), this->snapshot_.edges.end(), query_dependency_edge_less);
    if (std::adjacent_find(this->snapshot_.edges.begin(), this->snapshot_.edges.end()) != this->snapshot_.edges.end()) {
        return false;
    }
    if (!this->validate_edges()) {
        return false;
    }
    for (const QueryReplayNode& node : this->snapshot_.nodes) {
        if (!this->validate_node_dependencies(node)) {
            return false;
        }
    }
    for (const QueryDependencyEdge& edge : this->snapshot_.edges) {
        this->dependents_by_dependency_[edge.dependency].push_back(edge.dependent);
    }
    for (auto& entry : this->dependents_by_dependency_) {
        std::vector<QueryKey>& dependents = entry.second;
        std::sort(dependents.begin(), dependents.end(), query_key_less);
    }
    return true;
}

bool QueryReplayIndex::validate_edges() const
{
    for (const QueryDependencyEdge& edge : this->snapshot_.edges) {
        const QueryReplayNode* const dependent = this->find(edge.dependent);
        const QueryReplayNode* const dependency = this->find(edge.dependency);
        if (dependent == nullptr || dependency == nullptr || !query_dependency_edge_kind_is_expected(edge)
            || !contains_query_key(dependent->dependencies, edge.dependency)
            || !query_dependency_edge_records_are_valid(dependent->record, dependency->record)) {
            return false;
        }
    }
    return true;
}

bool QueryReplayIndex::validate_node_dependencies(const QueryReplayNode& node) const
{
    std::vector<QueryKey> sorted_dependencies = node.dependencies;
    std::sort(sorted_dependencies.begin(), sorted_dependencies.end(), query_key_less);
    if (std::adjacent_find(sorted_dependencies.begin(), sorted_dependencies.end()) != sorted_dependencies.end()) {
        return false;
    }

    for (const QueryKey dependency : node.dependencies) {
        const QueryDependencyEdge edge{
            node.key,
            dependency,
        };
        if (this->find(dependency) == nullptr || !query_dependency_edge_kind_is_expected(edge)
            || !std::binary_search(
                this->snapshot_.edges.begin(), this->snapshot_.edges.end(), edge, query_dependency_edge_less)) {
            return false;
        }
    }
    return true;
}

} // namespace aurex::query
