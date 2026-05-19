#include <aurex/query/generic_instance_signature_query.hpp>
#include <aurex/query/generic_template_signature_query.hpp>

#include <utility>

namespace aurex::query {

std::optional<QueryKey> generic_instance_signature_query_key(const GenericInstanceKey& key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::generic_instance_signature, stable_key_fingerprint(key));
}

bool is_valid(const GenericInstanceSignatureProviderInput& input) noexcept
{
    return input.key != nullptr && is_valid(*input.key) && is_valid(input.signature);
}

bool is_valid(const GenericInstanceSignatureProviderOutput& output) noexcept
{
    if (!is_valid(output.record) || !is_valid(output.result)
        || output.record.key.kind != QueryKind::generic_instance_signature || output.record.result != output.result) {
        return false;
    }
    for (const QueryKey dependency : output.dependencies) {
        if (!is_valid(dependency)) {
            return false;
        }
    }
    return true;
}

std::optional<GenericInstanceSignatureProviderOutput> provide_generic_instance_signature_query(
    const GenericInstanceSignatureProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    const QueryResultFingerprint result = query_result_fingerprint(input.signature);
    std::optional<QueryRecord> record = generic_instance_signature_query_record(*input.key, result);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> template_signature_key =
            generic_template_signature_query_key(input.key->template_def)) {
        dependencies.push_back(*template_signature_key);
    }
    // Valid provider input satisfies the typed record builder preconditions.
    return GenericInstanceSignatureProviderOutput{
        std::move(*record),
        result,
        std::move(dependencies),
    };
}

} // namespace aurex::query
