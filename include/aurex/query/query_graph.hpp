#pragma once

#include <aurex/query/query_interner.hpp>

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
    QueryNodeId id;
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

struct QueryDependencyEdge {
    QueryKey dependent;
    QueryKey dependency;

    [[nodiscard]] friend constexpr bool operator==(QueryDependencyEdge lhs, QueryDependencyEdge rhs) noexcept = default;
};

[[nodiscard]] bool query_dependency_edge_kind_is_expected(QueryDependencyEdge edge) noexcept;

} // namespace aurex::query
