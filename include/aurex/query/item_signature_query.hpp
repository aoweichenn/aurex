#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct ItemSignatureProviderInput {
    DefKey key;
    IncrementalKey signature;
};

struct ItemSignatureProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> item_signature_query_key(DefKey key) noexcept;
[[nodiscard]] bool is_valid(const ItemSignatureProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const ItemSignatureProviderOutput& output) noexcept;
[[nodiscard]] std::optional<ItemSignatureProviderOutput> provide_item_signature_query(
    const ItemSignatureProviderInput& input);

} // namespace aurex::query
