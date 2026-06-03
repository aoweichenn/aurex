#include <aurex/infrastructure/query/function_body_syntax_query.hpp>

#include <string_view>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_FUNCTION_BODY_SYNTAX_AUTHORITY_MARKER = "query.function_body_syntax.authority.v1";

} // namespace

std::optional<QueryKey> function_body_syntax_query_key(const BodyKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::function_body_syntax, stable_key_fingerprint(key));
}

bool is_valid(const FunctionBodySyntaxAuthority& authority) noexcept
{
    return is_valid(authority.syntax) && is_valid(authority.owner) && is_valid(authority.module_part)
        && authority.module_part.module == authority.owner.module && authority.range_begin <= authority.range_end;
}

bool is_valid(const FunctionBodySyntaxProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.authority) && input.authority.owner == input.key.owner
        && input.authority.slot == input.key.slot && input.authority.ordinal == input.key.ordinal;
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

QueryResultFingerprint function_body_syntax_result_fingerprint(const FunctionBodySyntaxAuthority& authority) noexcept
{
    if (!is_valid(authority)) {
        return {};
    }
    StableHashBuilder builder;
    builder.mix_string(QUERY_FUNCTION_BODY_SYNTAX_AUTHORITY_MARKER);
    builder.mix_u64(authority.syntax.global_id);
    builder.mix_fingerprint(authority.syntax.fingerprint);
    builder.mix_fingerprint(stable_key_fingerprint(authority.owner));
    builder.mix_fingerprint(stable_key_fingerprint(authority.module_part));
    builder.mix_u8(static_cast<base::u8>(authority.slot));
    builder.mix_u32(authority.ordinal);
    return query_result_fingerprint(builder.finish());
}

std::optional<FunctionBodySyntaxProviderOutput> provide_function_body_syntax_query(
    const FunctionBodySyntaxProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    const QueryResultFingerprint result = function_body_syntax_result_fingerprint(input.authority);
    std::optional<QueryRecord> record = function_body_syntax_query_record(input.key, result);
    return FunctionBodySyntaxProviderOutput{
        std::move(*record),
        result,
        {},
    };
}

} // namespace aurex::query
