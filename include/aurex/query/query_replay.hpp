#pragma once

#include <aurex/query/query_graph.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

namespace aurex::query {

class QueryContext;

enum class QueryReplaySafetyMode : base::u8 {
    immutable_snapshot,
};

struct QueryReplayNode {
    QueryNodeId id;
    QueryKey key;
    QueryRecord record;
    std::vector<QueryKey> dependencies;
};

struct QueryReplaySnapshot {
    QueryReplaySafetyMode safety_mode = QueryReplaySafetyMode::immutable_snapshot;
    std::vector<QueryReplayNode> nodes;
    std::vector<QueryDependencyEdge> edges;
};

[[nodiscard]] std::optional<QueryReplaySnapshot> make_query_replay_snapshot(const QueryContext& context);

class QueryReplayIndex final {
public:
    [[nodiscard]] static std::optional<QueryReplayIndex> build(QueryReplaySnapshot snapshot);
    [[nodiscard]] static std::optional<QueryReplayIndex> build(const QueryContext& context);

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] base::usize size() const noexcept;
    [[nodiscard]] const QueryReplaySnapshot& snapshot() const noexcept;
    [[nodiscard]] const QueryReplayNode* find(QueryKey key) const;
    [[nodiscard]] const QueryReplayNode* find(QueryNodeId id) const;
    [[nodiscard]] const QueryRecord* record_for(QueryKey key) const;
    [[nodiscard]] std::vector<QueryKey> dependencies_for(QueryKey key) const;
    [[nodiscard]] std::vector<QueryKey> dependents_of(QueryKey key) const;
    [[nodiscard]] bool has_dependency(QueryKey dependent, QueryKey dependency) const;

private:
    explicit QueryReplayIndex(QueryReplaySnapshot snapshot);

    [[nodiscard]] bool rebuild_index();
    [[nodiscard]] bool validate_edges() const;
    [[nodiscard]] bool validate_node_dependencies(const QueryReplayNode& node) const;

    QueryReplaySnapshot snapshot_;
    std::unordered_map<QueryKey, base::usize, QueryKeyHash> index_by_key_;
    std::unordered_map<QueryNodeId, base::usize, QueryNodeIdHash> index_by_node_id_;
    std::unordered_map<QueryKey, std::vector<QueryKey>, QueryKeyHash> dependents_by_dependency_;
};

} // namespace aurex::query
