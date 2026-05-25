#include <aurex/query/item_list_query.hpp>
#include <aurex/query/module_exports_query.hpp>

#include <algorithm>
#include <utility>

namespace aurex::query {

std::optional<QueryKey> module_exports_query_key(const ModuleKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::module_exports, stable_key_fingerprint(key));
}

bool is_valid(const ModuleExportsProviderInput& input) noexcept
{
    if (!is_valid(input.key) || !is_valid(input.exports)) {
        return false;
    }
    for (const ModuleKey& module : input.reexport_dependencies) {
        if (!is_valid(module)) {
            return false;
        }
    }
    return true;
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

std::optional<ModuleExportsProviderOutput> provide_module_exports_query(const ModuleExportsProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = module_exports_query_record(input.key, input.exports);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> item_list_key = item_list_query_key(input.key)) {
        dependencies.push_back(*item_list_key);
    }
    for (const ModuleKey& module : input.reexport_dependencies) {
        if (module == input.key) {
            continue;
        }
        if (const std::optional<QueryKey> reexport_key = module_exports_query_key(module)) {
            dependencies.push_back(*reexport_key);
        }
    }
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
    return ModuleExportsProviderOutput{
        std::move(*record),
        input.exports,
        std::move(dependencies),
    };
}

} // namespace aurex::query
