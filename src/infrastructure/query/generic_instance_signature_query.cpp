#include <aurex/infrastructure/query/generic_instance_signature_query.hpp>
#include <aurex/infrastructure/query/generic_template_signature_query.hpp>

#include <cstddef>
#include <string_view>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_GENERIC_INSTANCE_SIGNATURE_AUTHORITY_MARKER =
    "query.generic_instance_signature.authority.v1";
constexpr base::u8 QUERY_GENERIC_INSTANCE_SIGNATURE_MAX_VISIBILITY_RANK = 2;

[[nodiscard]] bool is_valid_generic_instance_signature_kind(const GenericInstanceSignatureKind kind) noexcept
{
    switch (kind) {
        case GenericInstanceSignatureKind::function:
        case GenericInstanceSignatureKind::method:
        case GenericInstanceSignatureKind::struct_:
        case GenericInstanceSignatureKind::enum_:
        case GenericInstanceSignatureKind::type_alias:
            return true;
        case GenericInstanceSignatureKind::invalid:
            return false;
    }
    return false;
}

[[nodiscard]] bool size_matches_u32(const base::u32 expected, const std::size_t actual) noexcept
{
    return static_cast<std::size_t>(expected) == actual;
}

} // namespace

std::optional<QueryKey> generic_instance_signature_query_key(const GenericInstanceKey& key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::generic_instance_signature, stable_key_fingerprint(key));
}

bool is_valid(const GenericInstanceSignatureAuthority& authority) noexcept
{
    return is_valid(authority.signature) && is_valid_generic_instance_signature_kind(authority.kind)
        && authority.visibility_rank <= QUERY_GENERIC_INSTANCE_SIGNATURE_MAX_VISIBILITY_RANK;
}

bool is_valid(const GenericInstanceSignatureProviderInput& input) noexcept
{
    return input.key != nullptr && is_valid(*input.key) && is_valid(input.authority)
        && size_matches_u32(input.authority.type_arg_count, input.key->type_args.size())
        && size_matches_u32(input.authority.const_arg_count, input.key->const_args.size())
        && input.authority.param_env_predicate_count == input.key->param_env.predicate_count;
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

QueryResultFingerprint generic_instance_signature_result_fingerprint(
    const GenericInstanceSignatureAuthority& authority) noexcept
{
    if (!is_valid(authority)) {
        return {};
    }
    StableHashBuilder builder;
    builder.mix_string(QUERY_GENERIC_INSTANCE_SIGNATURE_AUTHORITY_MARKER);
    builder.mix_fingerprint(stable_key_fingerprint(authority.signature));
    builder.mix_u8(static_cast<base::u8>(authority.kind));
    builder.mix_u8(authority.visibility_rank);
    builder.mix_u32(authority.type_arg_count);
    builder.mix_u32(authority.const_arg_count);
    builder.mix_u32(authority.param_env_predicate_count);
    builder.mix_u32(authority.value_param_count);
    builder.mix_u32(authority.generic_param_count);
    builder.mix_bool(authority.has_return_type);
    builder.mix_bool(authority.has_receiver_type);
    builder.mix_bool(authority.is_unsafe);
    builder.mix_bool(authority.is_variadic);
    builder.mix_bool(authority.has_definition);
    return query_result_fingerprint(builder.finish());
}

std::optional<GenericInstanceSignatureProviderOutput> provide_generic_instance_signature_query(
    const GenericInstanceSignatureProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    const QueryResultFingerprint result = generic_instance_signature_result_fingerprint(input.authority);
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
