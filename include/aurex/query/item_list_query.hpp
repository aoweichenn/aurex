#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct ItemListProviderInput {
    ModuleKey key;
    QueryResultFingerprint items;
};

struct ItemListProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> item_list_query_key(ModuleKey key) noexcept;
[[nodiscard]] bool is_valid(const ItemListProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const ItemListProviderOutput& output) noexcept;
[[nodiscard]] std::optional<ItemListProviderOutput> provide_item_list_query(const ItemListProviderInput& input);

} // namespace aurex::query
