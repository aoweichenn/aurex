#include <aurex/query/module_exports_query.hpp>

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
    return is_valid(input.key) && is_valid(input.exports);
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
    return ModuleExportsProviderOutput{
        std::move(*record),
        input.exports,
        {},
    };
}

} // namespace aurex::query
