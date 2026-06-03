#include <aurex/infrastructure/query/function_body_syntax_query.hpp>
#include <aurex/infrastructure/query/item_signature_query.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <string_view>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_TYPE_CHECK_BODY_AUTHORITY_MARKER = "query.type_check_body.authority.v1";

} // namespace

std::optional<QueryKey> type_check_body_query_key(const BodyKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::type_check_body, stable_key_fingerprint(key));
}

bool is_valid(const TypeCheckBodyAuthority& authority) noexcept
{
    return is_valid(authority.checked_body) && is_valid(authority.body_syntax_result)
        && is_valid(authority.signature_result);
}

bool is_valid(const TypeCheckBodyProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.authority);
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

QueryResultFingerprint type_check_body_result_fingerprint(const TypeCheckBodyAuthority& authority) noexcept
{
    if (!is_valid(authority)) {
        return {};
    }
    StableHashBuilder builder;
    builder.mix_string(QUERY_TYPE_CHECK_BODY_AUTHORITY_MARKER);
    builder.mix_u64(authority.checked_body.global_id);
    builder.mix_fingerprint(authority.checked_body.fingerprint);
    builder.mix_u64(authority.body_syntax_result.global_id);
    builder.mix_fingerprint(authority.body_syntax_result.fingerprint);
    builder.mix_u64(authority.signature_result.global_id);
    builder.mix_fingerprint(authority.signature_result.fingerprint);
    builder.mix_fingerprint(authority.borrow_summary_fingerprint);
    builder.mix_fingerprint(authority.borrow_contract_fingerprint);
    builder.mix_fingerprint(authority.lifetime_fingerprint);
    builder.mix_fingerprint(authority.body_loan_fingerprint);
    builder.mix_u32(authority.expr_side_table_count);
    builder.mix_u32(authority.pattern_side_table_count);
    builder.mix_u32(authority.type_side_table_count);
    builder.mix_u32(authority.stmt_side_table_count);
    builder.mix_u32(authority.coercion_count);
    builder.mix_u32(authority.borrow_summary_origin_count);
    builder.mix_u32(authority.borrow_summary_dependency_count);
    builder.mix_u32(authority.borrow_contract_selector_count);
    builder.mix_u32(authority.lifetime_region_count);
    builder.mix_u32(authority.lifetime_outlives_constraint_count);
    builder.mix_u32(authority.lifetime_type_outlives_constraint_count);
    builder.mix_u32(authority.lifetime_live_range_count);
    builder.mix_u32(authority.lifetime_return_region_count);
    builder.mix_u32(authority.lifetime_violation_count);
    builder.mix_u32(authority.type_lifetime_info_count);
    builder.mix_u32(authority.generic_lifetime_predicate_count);
    builder.mix_u32(authority.body_loan_count);
    builder.mix_u32(authority.body_reborrow_count);
    builder.mix_u32(authority.body_two_phase_borrow_count);
    builder.mix_u32(authority.body_loan_conflict_count);
    builder.mix_bool(authority.retained_side_tables);
    builder.mix_bool(authority.has_diagnostics);
    builder.mix_bool(authority.has_borrow_summary);
    builder.mix_bool(authority.has_borrow_contract);
    builder.mix_bool(authority.has_lifetime_facts);
    builder.mix_bool(authority.has_body_loan_check);
    builder.mix_bool(authority.borrow_summary_has_unknown_return_origin);
    builder.mix_bool(authority.borrow_summary_has_local_return_escape);
    builder.mix_bool(authority.borrow_contract_unknown_return_allowed);
    builder.mix_bool(authority.borrow_contract_has_local_return_escape);
    builder.mix_bool(authority.borrow_contract_has_mismatch);
    builder.mix_bool(authority.lifetime_has_emitted_diagnostics);
    builder.mix_bool(authority.lifetime_has_unknown_origin);
    builder.mix_bool(authority.lifetime_has_ambiguous_elision);
    builder.mix_bool(authority.lifetime_has_return_origin_mismatch);
    builder.mix_bool(authority.lifetime_has_local_escape);
    builder.mix_bool(authority.lifetime_has_unknown_escape);
    builder.mix_bool(authority.body_loan_graph_missing);
    builder.mix_bool(authority.body_loan_has_emitted_diagnostics);
    builder.mix_bool(authority.body_two_phase_has_emitted_diagnostics);
    return query_result_fingerprint(builder.finish());
}

std::optional<TypeCheckBodyProviderOutput> provide_type_check_body_query(const TypeCheckBodyProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    const QueryResultFingerprint result = type_check_body_result_fingerprint(input.authority);
    std::optional<QueryRecord> record = type_check_body_query_record(input.key, result);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> function_body_key = function_body_syntax_query_key(input.key)) {
        dependencies.push_back(*function_body_key);
    }
    if (const std::optional<QueryKey> item_signature_key = item_signature_query_key(input.key.owner)) {
        dependencies.push_back(*item_signature_key);
    }
    return TypeCheckBodyProviderOutput{
        std::move(*record),
        result,
        std::move(dependencies),
    };
}

} // namespace aurex::query
