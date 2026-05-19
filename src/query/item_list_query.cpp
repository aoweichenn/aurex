#include <aurex/query/item_list_query.hpp>
#include <aurex/query/module_graph_query.hpp>

#include <utility>

namespace aurex::query {

std::optional<QueryKey> item_list_query_key(const ModuleKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::item_list, stable_key_fingerprint(key));
}

bool is_valid(const ItemListProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.items);
}

bool is_valid(const ItemListProviderOutput& output) noexcept
{
    if (!is_valid(output.record) || !is_valid(output.result) || output.record.key.kind != QueryKind::item_list
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

std::optional<ItemListProviderOutput> provide_item_list_query(const ItemListProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = item_list_query_record(input.key, input.items);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> module_graph_key = module_graph_query_key(input.key)) {
        dependencies.push_back(*module_graph_key);
    }
    return ItemListProviderOutput{
        std::move(*record),
        input.items,
        std::move(dependencies),
    };
}

} // namespace aurex::query
