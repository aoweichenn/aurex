#include <aurex/query/generic_template_signature_query.hpp>
#include <aurex/query/item_list_query.hpp>

#include <utility>

namespace aurex::query {

std::optional<QueryKey> generic_template_signature_query_key(const DefKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::generic_template_signature, stable_key_fingerprint(key));
}

bool is_valid(const GenericTemplateSignatureProviderInput& input) noexcept
{
    return is_valid(input.key) && input.key.kind == DefKind::generic_template && is_valid(input.signature);
}

bool is_valid(const GenericTemplateSignatureProviderOutput& output) noexcept
{
    if (!is_valid(output.record) || !is_valid(output.result)
        || output.record.key.kind != QueryKind::generic_template_signature || output.record.result != output.result) {
        return false;
    }
    for (const QueryKey dependency : output.dependencies) {
        if (!is_valid(dependency)) {
            return false;
        }
    }
    return true;
}

std::optional<GenericTemplateSignatureProviderOutput> provide_generic_template_signature_query(
    const GenericTemplateSignatureProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    const QueryResultFingerprint result = query_result_fingerprint(input.signature);
    std::optional<QueryRecord> record = generic_template_signature_query_record(input.key, result);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> item_list_key = item_list_query_key(input.key.module)) {
        dependencies.push_back(*item_list_key);
    }
    return GenericTemplateSignatureProviderOutput{
        std::move(*record),
        result,
        std::move(dependencies),
    };
}

} // namespace aurex::query
