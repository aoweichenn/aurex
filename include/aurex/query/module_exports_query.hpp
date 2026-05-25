#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct ModuleExportsProviderInput {
    ModuleKey key;
    QueryResultFingerprint exports;
    std::vector<ModuleKey> reexport_dependencies{};
};

struct ModuleExportsProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> module_exports_query_key(ModuleKey key) noexcept;
[[nodiscard]] bool is_valid(const ModuleExportsProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const ModuleExportsProviderOutput& output) noexcept;
[[nodiscard]] std::optional<ModuleExportsProviderOutput> provide_module_exports_query(
    const ModuleExportsProviderInput& input);

} // namespace aurex::query
