#pragma once

#include <aurex/infrastructure/query/query_interner.hpp>

#include <vector>

namespace aurex::query {

using QueryRevision = base::u64;

inline constexpr QueryRevision QUERY_REVISION_INVALID = 0;
inline constexpr QueryRevision QUERY_REVISION_INITIAL = 1;

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

enum class QueryReuseState : base::u8 {
    unknown,
    green,
    red,
};

struct QueryNode {
    QueryNodeId id;
    QueryKey key;
    QueryNodeStatus status = QueryNodeStatus::failed;
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
    QueryRevision verified_revision = QUERY_REVISION_INVALID;
    QueryRevision changed_revision = QUERY_REVISION_INVALID;
    QueryReuseState reuse_state = QueryReuseState::unknown;
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
