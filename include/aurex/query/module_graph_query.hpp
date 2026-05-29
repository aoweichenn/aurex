#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct ModuleGraphProviderInput {
    ModuleKey key;
    QueryResultFingerprint graph;
    std::vector<QueryKey> dependencies;
};

struct ModuleGraphProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> module_graph_query_key(ModuleKey key) noexcept;
[[nodiscard]] bool is_valid(const ModuleGraphProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const ModuleGraphProviderOutput& output) noexcept;
[[nodiscard]] std::optional<ModuleGraphProviderOutput> provide_module_graph_query(
    const ModuleGraphProviderInput& input);

} // namespace aurex::query
