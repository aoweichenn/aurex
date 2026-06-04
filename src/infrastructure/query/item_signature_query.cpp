#include <aurex/infrastructure/query/item_signature_query.hpp>
#include <aurex/infrastructure/query/module_exports_query.hpp>

#include <string_view>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_ITEM_SIGNATURE_AUTHORITY_MARKER = "query.item_signature.authority.v1";
constexpr base::u8 QUERY_ITEM_SIGNATURE_MAX_VISIBILITY_RANK = 2;

[[nodiscard]] bool is_valid_item_signature_kind(const DefKind kind) noexcept
{
    switch (kind) {
        case DefKind::function:
        case DefKind::method:
        case DefKind::const_:
        case DefKind::global:
        case DefKind::type_alias:
        case DefKind::struct_:
        case DefKind::enum_:
        case DefKind::enum_case:
        case DefKind::struct_field:
        case DefKind::trait_:
        case DefKind::trait_method:
        case DefKind::synthetic:
            return true;
        case DefKind::invalid:
        case DefKind::value:
        case DefKind::generic_template:
        case DefKind::associated_type:
        case DefKind::associated_const:
            return false;
    }
    return false;
}

} // namespace

std::optional<QueryKey> item_signature_query_key(const DefKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::item_signature, stable_key_fingerprint(key));
}

bool is_valid(const ItemSignatureAuthority& authority) noexcept
{
    return is_valid(authority.signature) && is_valid(authority.module_part)
        && is_valid_item_signature_kind(authority.kind)
        && authority.visibility_rank <= QUERY_ITEM_SIGNATURE_MAX_VISIBILITY_RANK;
}

bool is_valid(const ItemSignatureProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.authority) && input.authority.name_space == input.key.name_space
        && input.authority.kind == input.key.kind && input.authority.module_part.module == input.key.module;
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

QueryResultFingerprint item_signature_result_fingerprint(const ItemSignatureAuthority& authority) noexcept
{
    if (!is_valid(authority)) {
        return {};
    }
    StableHashBuilder builder;
    builder.mix_string(QUERY_ITEM_SIGNATURE_AUTHORITY_MARKER);
    builder.mix_fingerprint(stable_key_fingerprint(authority.signature));
    builder.mix_fingerprint(stable_key_fingerprint(authority.module_part));
    builder.mix_u8(static_cast<base::u8>(authority.name_space));
    builder.mix_u8(static_cast<base::u8>(authority.kind));
    builder.mix_u8(authority.visibility_rank);
    builder.mix_u64(authority.value_component_count);
    builder.mix_u64(authority.generic_param_count);
    builder.mix_bool(authority.has_return_type);
    builder.mix_bool(authority.has_receiver_type);
    builder.mix_bool(authority.is_unsafe);
    builder.mix_bool(authority.is_variadic);
    builder.mix_bool(authority.has_definition);
    return query_result_fingerprint(builder.finish());
}

std::optional<ItemSignatureProviderOutput> provide_item_signature_query(const ItemSignatureProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    const QueryResultFingerprint result = item_signature_result_fingerprint(input.authority);
    std::optional<QueryRecord> record = item_signature_query_record(input.key, result);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> module_exports_key = module_exports_query_key(input.key.module)) {
        dependencies.push_back(*module_exports_key);
    }
    // Valid provider input satisfies the typed record builder preconditions.
    return ItemSignatureProviderOutput{
        std::move(*record),
        result,
        std::move(dependencies),
    };
}

} // namespace aurex::query
