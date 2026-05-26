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

struct ModulePackageExportsProviderInput {
    ModuleKey key;
    QueryResultFingerprint exports;
    std::vector<ModuleKey> public_surface_dependencies{};
    std::vector<ModuleKey> package_surface_dependencies{};
};

struct ModuleExportsProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

struct ModulePackageExportsProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> module_exports_query_key(ModuleKey key) noexcept;
[[nodiscard]] std::optional<QueryKey> module_package_exports_query_key(ModuleKey key) noexcept;
[[nodiscard]] bool is_valid(const ModuleExportsProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const ModulePackageExportsProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const ModuleExportsProviderOutput& output) noexcept;
[[nodiscard]] bool is_valid(const ModulePackageExportsProviderOutput& output) noexcept;
[[nodiscard]] std::optional<ModuleExportsProviderOutput> provide_module_exports_query(
    const ModuleExportsProviderInput& input);
[[nodiscard]] std::optional<ModulePackageExportsProviderOutput> provide_module_package_exports_query(
    const ModulePackageExportsProviderInput& input);

} // namespace aurex::query
