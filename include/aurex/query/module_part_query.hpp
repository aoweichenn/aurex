#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct ModulePartProviderInput {
    ModulePartKey key;
    QueryResultFingerprint part;
};

struct ModulePartProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> module_part_query_key(ModulePartKey key) noexcept;
[[nodiscard]] bool is_valid(const ModulePartProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const ModulePartProviderOutput& output) noexcept;
[[nodiscard]] std::optional<ModulePartProviderOutput> provide_module_part_query(
    const ModulePartProviderInput& input);

} // namespace aurex::query
