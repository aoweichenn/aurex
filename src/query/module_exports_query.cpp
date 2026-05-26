#include <aurex/query/item_list_query.hpp>
#include <aurex/query/module_exports_query.hpp>

#include <algorithm>
#include <utility>

namespace aurex::query {
namespace {

[[nodiscard]] bool module_keys_are_valid(const std::vector<ModuleKey>& modules) noexcept
{
    for (const ModuleKey& module : modules) {
        if (!is_valid(module)) {
            return false;
        }
    }
    return true;
}

void push_query_key_if_valid(std::vector<QueryKey>& dependencies, const std::optional<QueryKey> key)
{
    if (key) {
        dependencies.push_back(*key);
    }
}

void sort_unique_query_keys(std::vector<QueryKey>& dependencies)
{
    std::sort(dependencies.begin(), dependencies.end(), [](const QueryKey lhs, const QueryKey rhs) {
        if (lhs.kind != rhs.kind) {
            return lhs.kind < rhs.kind;
        }
        if (lhs.global_id != rhs.global_id) {
            return lhs.global_id < rhs.global_id;
        }
        if (lhs.payload.primary != rhs.payload.primary) {
            return lhs.payload.primary < rhs.payload.primary;
        }
        if (lhs.payload.secondary != rhs.payload.secondary) {
            return lhs.payload.secondary < rhs.payload.secondary;
        }
        return lhs.payload.byte_count < rhs.payload.byte_count;
    });
    dependencies.erase(std::unique(dependencies.begin(), dependencies.end()), dependencies.end());
}

} // namespace

std::optional<QueryKey> module_exports_query_key(const ModuleKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::module_exports, stable_key_fingerprint(key));
}

std::optional<QueryKey> module_package_exports_query_key(const ModuleKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::module_package_exports, stable_key_fingerprint(key));
}

bool is_valid(const ModuleExportsProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.exports) && module_keys_are_valid(input.reexport_dependencies);
}

bool is_valid(const ModulePackageExportsProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.exports) && module_keys_are_valid(input.public_surface_dependencies)
        && module_keys_are_valid(input.package_surface_dependencies);
}

bool is_valid(const ModuleExportsProviderOutput& output) noexcept
{
    if (!is_valid(output.record) || !is_valid(output.result) || output.record.key.kind != QueryKind::module_exports
        || output.record.result != output.result) {
        return false;
    }
    for (const QueryKey dependency : output.dependencies) {
        if (!is_valid(dependency)) {
            return false;
        }
    }
    return true;
}

bool is_valid(const ModulePackageExportsProviderOutput& output) noexcept
{
    if (!is_valid(output.record) || !is_valid(output.result)
        || output.record.key.kind != QueryKind::module_package_exports || output.record.result != output.result) {
        return false;
    }
    for (const QueryKey dependency : output.dependencies) {
        if (!is_valid(dependency)) {
            return false;
        }
    }
    return true;
}

std::optional<ModuleExportsProviderOutput> provide_module_exports_query(const ModuleExportsProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = module_exports_query_record(input.key, input.exports);
    std::vector<QueryKey> dependencies;
    push_query_key_if_valid(dependencies, item_list_query_key(input.key));
    for (const ModuleKey& module : input.reexport_dependencies) {
        if (module == input.key) {
            continue;
        }
        push_query_key_if_valid(dependencies, module_exports_query_key(module));
    }
    sort_unique_query_keys(dependencies);
    return ModuleExportsProviderOutput{
        std::move(*record),
        input.exports,
        std::move(dependencies),
    };
}

std::optional<ModulePackageExportsProviderOutput> provide_module_package_exports_query(
    const ModulePackageExportsProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = module_package_exports_query_record(input.key, input.exports);
    std::vector<QueryKey> dependencies;
    push_query_key_if_valid(dependencies, item_list_query_key(input.key));
    for (const ModuleKey& module : input.public_surface_dependencies) {
        if (module == input.key) {
            continue;
        }
        push_query_key_if_valid(dependencies, module_exports_query_key(module));
    }
    for (const ModuleKey& module : input.package_surface_dependencies) {
        if (module == input.key) {
            continue;
        }
        push_query_key_if_valid(dependencies, module_package_exports_query_key(module));
    }
    sort_unique_query_keys(dependencies);
    return ModulePackageExportsProviderOutput{
        std::move(*record),
        input.exports,
        std::move(dependencies),
    };
}

} // namespace aurex::query
