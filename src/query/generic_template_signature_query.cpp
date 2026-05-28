#include <aurex/query/generic_template_signature_query.hpp>
#include <aurex/query/item_list_query.hpp>

#include <string_view>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_GENERIC_TEMPLATE_SIGNATURE_AUTHORITY_MARKER =
    "query.generic_template_signature.authority.v1";
constexpr base::u8 QUERY_GENERIC_TEMPLATE_SIGNATURE_MAX_VISIBILITY_RANK = 2;

} // namespace

std::optional<QueryKey> generic_template_signature_query_key(const DefKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::generic_template_signature, stable_key_fingerprint(key));
}

bool is_valid(const GenericTemplateSignatureAuthority& authority) noexcept
{
    return is_valid(authority.signature) && is_valid(authority.module_part)
        && authority.visibility_rank <= QUERY_GENERIC_TEMPLATE_SIGNATURE_MAX_VISIBILITY_RANK;
}

bool is_valid(const GenericTemplateSignatureProviderInput& input) noexcept
{
    return is_valid(input.key) && input.key.kind == DefKind::generic_template && is_valid(input.authority)
        && input.authority.name_space == input.key.name_space && input.authority.module_part.module == input.key.module;
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

QueryResultFingerprint generic_template_signature_result_fingerprint(
    const GenericTemplateSignatureAuthority& authority) noexcept
{
    if (!is_valid(authority)) {
        return {};
    }
    StableHashBuilder builder;
    builder.mix_string(QUERY_GENERIC_TEMPLATE_SIGNATURE_AUTHORITY_MARKER);
    builder.mix_fingerprint(stable_key_fingerprint(authority.signature));
    builder.mix_fingerprint(stable_key_fingerprint(authority.module_part));
    builder.mix_u8(static_cast<base::u8>(authority.name_space));
    builder.mix_u8(authority.visibility_rank);
    builder.mix_u32(authority.param_count);
    builder.mix_u32(authority.constraint_count);
    return query_result_fingerprint(builder.finish());
}

std::optional<GenericTemplateSignatureProviderOutput> provide_generic_template_signature_query(
    const GenericTemplateSignatureProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    const QueryResultFingerprint result = generic_template_signature_result_fingerprint(input.authority);
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
