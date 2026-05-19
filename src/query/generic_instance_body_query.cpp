#include <aurex/query/generic_instance_body_query.hpp>
#include <aurex/query/generic_instance_signature_query.hpp>

#include <utility>

namespace aurex::query {

std::optional<QueryKey> generic_instance_body_query_key(const GenericInstanceKey& key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::generic_instance_body, stable_key_fingerprint(key));
}

bool is_valid(const GenericInstanceBodyProviderInput& input) noexcept
{
    return input.key != nullptr && is_valid(*input.key) && is_valid(input.checked_body);
}

bool is_valid(const GenericInstanceBodyProviderOutput& output) noexcept
{
    if (!is_valid(output.record) || !is_valid(output.result)
        || output.record.key.kind != QueryKind::generic_instance_body || output.record.result != output.result) {
        return false;
    }
    for (const QueryKey dependency : output.dependencies) {
        if (!is_valid(dependency)) {
            return false;
        }
    }
    return true;
}

std::optional<GenericInstanceBodyProviderOutput> provide_generic_instance_body_query(
    const GenericInstanceBodyProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = generic_instance_body_query_record(*input.key, input.checked_body);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> signature_key = generic_instance_signature_query_key(*input.key)) {
        dependencies.push_back(*signature_key);
    }
    return GenericInstanceBodyProviderOutput{
        std::move(*record),
        input.checked_body,
        std::move(dependencies),
    };
}

} // namespace aurex::query
