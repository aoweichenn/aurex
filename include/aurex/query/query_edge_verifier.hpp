#pragma once

#include <aurex/query/query_graph.hpp>
#include <aurex/query/query_result.hpp>

namespace aurex::query {

enum class QueryDependencyEdgeValidationStatus : base::u8 {
    valid,
    invalid_record,
    invalid_kind,
    invalid_identity,
};

[[nodiscard]] QueryDependencyEdgeValidationStatus validate_query_dependency_edge_records(
    const QueryRecord& dependent, const QueryRecord& dependency);
[[nodiscard]] bool query_dependency_edge_records_are_valid(const QueryRecord& dependent, const QueryRecord& dependency);

} // namespace aurex::query
