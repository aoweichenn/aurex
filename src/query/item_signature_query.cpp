#include <aurex/query/item_signature_query.hpp>

#include <utility>

namespace aurex::query {

std::optional<QueryKey> item_signature_query_key(const DefKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::item_signature, stable_key_fingerprint(key));
}

bool is_valid(const ItemSignatureProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.signature);
}

bool is_valid(const ItemSignatureProviderOutput& output) noexcept
{
    if (!is_valid(output.record) || !is_valid(output.result) || output.record.key.kind != QueryKind::item_signature
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

std::optional<ItemSignatureProviderOutput> provide_item_signature_query(const ItemSignatureProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    const QueryResultFingerprint result = query_result_fingerprint(input.signature);
    std::optional<QueryRecord> record = item_signature_query_record(input.key, result);
    // Valid provider input satisfies the typed record builder preconditions.
    return ItemSignatureProviderOutput{
        std::move(*record),
        result,
        {},
    };
}

} // namespace aurex::query
