#include <aurex/query/function_body_syntax_query.hpp>
#include <aurex/query/item_signature_query.hpp>
#include <aurex/query/type_check_body_query.hpp>

#include <utility>

namespace aurex::query {

std::optional<QueryKey> type_check_body_query_key(const BodyKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::type_check_body, stable_key_fingerprint(key));
}

bool is_valid(const TypeCheckBodyProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.checked_body);
}

bool is_valid(const TypeCheckBodyProviderOutput& output) noexcept
{
    if (!is_valid(output.record) || !is_valid(output.result) || output.record.key.kind != QueryKind::type_check_body
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

std::optional<TypeCheckBodyProviderOutput> provide_type_check_body_query(const TypeCheckBodyProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = type_check_body_query_record(input.key, input.checked_body);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> function_body_key = function_body_syntax_query_key(input.key)) {
        dependencies.push_back(*function_body_key);
    }
    if (const std::optional<QueryKey> item_signature_key = item_signature_query_key(input.key.owner)) {
        dependencies.push_back(*item_signature_key);
    }
    return TypeCheckBodyProviderOutput{
        std::move(*record),
        input.checked_body,
        std::move(dependencies),
    };
}

} // namespace aurex::query
