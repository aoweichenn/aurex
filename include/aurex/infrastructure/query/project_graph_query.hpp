#pragma once

#include <aurex/infrastructure/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct ProjectGraphProviderInput {
    ProjectKey key;
    QueryResultFingerprint graph;
};

struct ProjectGraphProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> project_graph_query_key(ProjectKey key) noexcept;
[[nodiscard]] bool is_valid(const ProjectGraphProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const ProjectGraphProviderOutput& output) noexcept;
[[nodiscard]] std::optional<ProjectGraphProviderOutput> provide_project_graph_query(
    const ProjectGraphProviderInput& input);

} // namespace aurex::query
