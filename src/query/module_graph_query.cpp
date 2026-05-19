#include <aurex/query/module_graph_query.hpp>

#include <utility>

namespace aurex::query {

std::optional<QueryKey> module_graph_query_key(const ModuleKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::module_graph, stable_key_fingerprint(key));
}

bool is_valid(const ModuleGraphProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.graph);
}

bool is_valid(const ModuleGraphProviderOutput& output) noexcept
{
    if (!is_valid(output.record) || !is_valid(output.result) || output.record.key.kind != QueryKind::module_graph
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

std::optional<ModuleGraphProviderOutput> provide_module_graph_query(const ModuleGraphProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = module_graph_query_record(input.key, input.graph);
    return ModuleGraphProviderOutput{
        std::move(*record),
        input.graph,
        {},
    };
}

} // namespace aurex::query
