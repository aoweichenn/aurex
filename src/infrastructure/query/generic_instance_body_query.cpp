#include <aurex/infrastructure/query/generic_instance_body_query.hpp>
#include <aurex/infrastructure/query/generic_instance_signature_query.hpp>

#include <string_view>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_GENERIC_INSTANCE_BODY_AUTHORITY_MARKER = "query.generic_instance_body.authority.v1";

} // namespace

std::optional<QueryKey> generic_instance_body_query_key(const GenericInstanceKey& key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::generic_instance_body, stable_key_fingerprint(key));
}

bool is_valid(const GenericInstanceBodyAuthority& authority) noexcept
{
    return is_valid(authority.checked_body) && is_valid(authority.signature_result);
}

bool is_valid(const GenericInstanceBodyProviderInput& input) noexcept
{
    return input.key != nullptr && is_valid(*input.key) && is_valid(input.authority);
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

QueryResultFingerprint generic_instance_body_result_fingerprint(const GenericInstanceBodyAuthority& authority) noexcept
{
    if (!is_valid(authority)) {
        return {};
    }
    StableHashBuilder builder;
    builder.mix_string(QUERY_GENERIC_INSTANCE_BODY_AUTHORITY_MARKER);
    builder.mix_u64(authority.checked_body.global_id);
    builder.mix_fingerprint(authority.checked_body.fingerprint);
    builder.mix_u64(authority.signature_result.global_id);
    builder.mix_fingerprint(authority.signature_result.fingerprint);
    builder.mix_u32(authority.expr_side_table_count);
    builder.mix_u32(authority.pattern_side_table_count);
    builder.mix_u32(authority.type_side_table_count);
    builder.mix_u32(authority.stmt_side_table_count);
    builder.mix_u32(authority.sparse_fallback_count);
    builder.mix_bool(authority.retained_side_tables);
    builder.mix_bool(authority.local_dense_side_tables);
    builder.mix_bool(authority.sparse_side_tables);
    return query_result_fingerprint(builder.finish());
}

std::optional<GenericInstanceBodyProviderOutput> provide_generic_instance_body_query(
    const GenericInstanceBodyProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    const QueryResultFingerprint result = generic_instance_body_result_fingerprint(input.authority);
    std::optional<QueryRecord> record = generic_instance_body_query_record(*input.key, result);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> signature_key = generic_instance_signature_query_key(*input.key)) {
        dependencies.push_back(*signature_key);
    }
    return GenericInstanceBodyProviderOutput{
        std::move(*record),
        result,
        std::move(dependencies),
    };
}

} // namespace aurex::query
