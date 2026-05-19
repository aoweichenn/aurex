#include <aurex/query/function_body_syntax_query.hpp>

#include <utility>

namespace aurex::query {

std::optional<QueryKey> function_body_syntax_query_key(const BodyKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::function_body_syntax, stable_key_fingerprint(key));
}

bool is_valid(const FunctionBodySyntaxProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.syntax);
}

bool is_valid(const FunctionBodySyntaxProviderOutput& output) noexcept
{
    if (!is_valid(output.record) || !is_valid(output.result)
        || output.record.key.kind != QueryKind::function_body_syntax || output.record.result != output.result) {
        return false;
    }
    for (const QueryKey dependency : output.dependencies) {
        if (!is_valid(dependency)) {
            return false;
        }
    }
    return true;
}

std::optional<FunctionBodySyntaxProviderOutput> provide_function_body_syntax_query(
    const FunctionBodySyntaxProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = function_body_syntax_query_record(input.key, input.syntax);
    return FunctionBodySyntaxProviderOutput{
        std::move(*record),
        input.syntax,
        {},
    };
}

} // namespace aurex::query
