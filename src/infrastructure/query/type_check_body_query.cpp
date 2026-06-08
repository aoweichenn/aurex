#include <aurex/infrastructure/query/function_body_syntax_query.hpp>
#include <aurex/infrastructure/query/item_signature_query.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <string_view>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_TYPE_CHECK_BODY_AUTHORITY_MARKER = "query.type_check_body.authority.v7";

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
    builder.mix_fingerprint(authority.move_rejection_fingerprint);
    builder.mix_fingerprint(authority.lifetime_fingerprint);
    builder.mix_fingerprint(authority.dropck_fingerprint);
    builder.mix_fingerprint(authority.place_state_fingerprint);
    builder.mix_fingerprint(authority.body_loan_fingerprint);
    builder.mix_fingerprint(authority.destructor_fingerprint);
    builder.mix_fingerprint(authority.trait_object_fingerprint);
    builder.mix_fingerprint(authority.principal_set_composition_fingerprint);
    builder.mix_u64(authority.expr_side_table_count);
    builder.mix_u64(authority.pattern_side_table_count);
    builder.mix_u64(authority.type_side_table_count);
    builder.mix_u64(authority.stmt_side_table_count);
    builder.mix_u64(authority.coercion_count);
    builder.mix_u64(authority.trait_object_method_slot_count);
    builder.mix_u64(authority.trait_object_callability_count);
    builder.mix_u64(authority.vtable_layout_count);
    builder.mix_u64(authority.trait_object_coercion_count);
    builder.mix_u64(authority.trait_supertrait_edge_count);
    builder.mix_u64(authority.trait_object_upcast_coercion_count);
    builder.mix_u64(authority.principal_set_composition_count);
    builder.mix_u64(authority.principal_set_composition_principal_count);
    builder.mix_u64(authority.principal_set_composition_projection_count);
    builder.mix_u64(authority.borrow_summary_origin_count);
    builder.mix_u64(authority.borrow_summary_dependency_count);
    builder.mix_u64(authority.borrow_summary_storage_escape_count);
    builder.mix_u64(authority.borrow_contract_selector_count);
    builder.mix_u64(authority.move_rejection_count);
    builder.mix_u64(authority.move_rejection_pattern_payload_count);
    builder.mix_u64(authority.move_rejection_try_payload_count);
    builder.mix_u64(authority.move_rejection_indexed_element_count);
    builder.mix_u64(authority.lifetime_region_count);
    builder.mix_u64(authority.lifetime_outlives_constraint_count);
    builder.mix_u64(authority.lifetime_type_outlives_constraint_count);
    builder.mix_u64(authority.lifetime_live_range_count);
    builder.mix_u64(authority.lifetime_return_region_count);
    builder.mix_u64(authority.lifetime_violation_count);
    builder.mix_u64(authority.type_lifetime_info_count);
    builder.mix_u64(authority.generic_lifetime_predicate_count);
    builder.mix_u64(authority.dropck_fact_count);
    builder.mix_u64(authority.dropck_action_count);
    builder.mix_u64(authority.dropck_required_outlives_count);
    builder.mix_u64(authority.dropck_violation_count);
    builder.mix_u64(authority.place_state_place_count);
    builder.mix_u64(authority.place_state_event_count);
    builder.mix_u64(authority.place_state_partial_projection_count);
    builder.mix_u64(authority.place_state_drop_place_count);
    builder.mix_u64(authority.place_state_move_candidate_count);
    builder.mix_u64(authority.place_state_borrow_event_count);
    builder.mix_u64(authority.place_state_partial_move_count);
    builder.mix_u64(authority.place_state_skipped_drop_count);
    builder.mix_u64(authority.place_state_violation_count);
    builder.mix_u64(authority.place_state_emitted_diagnostic_count);
    builder.mix_u64(authority.body_loan_count);
    builder.mix_u64(authority.body_reborrow_count);
    builder.mix_u64(authority.body_two_phase_borrow_count);
    builder.mix_u64(authority.body_loan_conflict_count);
    builder.mix_u64(authority.destructor_count);
    builder.mix_bool(authority.retained_side_tables);
    builder.mix_bool(authority.has_diagnostics);
    builder.mix_bool(authority.has_borrow_summary);
    builder.mix_bool(authority.has_borrow_contract);
    builder.mix_bool(authority.has_move_rejection_facts);
    builder.mix_bool(authority.has_lifetime_facts);
    builder.mix_bool(authority.has_dropck_facts);
    builder.mix_bool(authority.has_place_state_facts);
    builder.mix_bool(authority.has_body_loan_check);
    builder.mix_bool(authority.has_destructor_facts);
    builder.mix_bool(authority.has_trait_object_facts);
    builder.mix_bool(authority.has_principal_set_composition_facts);
    builder.mix_bool(authority.borrow_summary_has_unknown_return_origin);
    builder.mix_bool(authority.borrow_summary_has_local_return_escape);
    builder.mix_bool(authority.borrow_summary_has_storage_escape);
    builder.mix_bool(authority.borrow_contract_unknown_return_allowed);
    builder.mix_bool(authority.borrow_contract_has_local_return_escape);
    builder.mix_bool(authority.borrow_contract_has_mismatch);
    builder.mix_bool(authority.move_rejection_has_pattern_payload);
    builder.mix_bool(authority.move_rejection_has_try_payload);
    builder.mix_bool(authority.move_rejection_has_indexed_element);
    builder.mix_bool(authority.move_rejection_has_emitted_diagnostics);
    builder.mix_bool(authority.lifetime_has_emitted_diagnostics);
    builder.mix_bool(authority.lifetime_has_unknown_origin);
    builder.mix_bool(authority.lifetime_has_ambiguous_elision);
    builder.mix_bool(authority.lifetime_has_return_origin_mismatch);
    builder.mix_bool(authority.lifetime_has_local_escape);
    builder.mix_bool(authority.lifetime_has_unknown_escape);
    builder.mix_bool(authority.dropck_graph_missing);
    builder.mix_bool(authority.dropck_has_emitted_diagnostics);
    builder.mix_bool(authority.dropck_has_generic_type_outlives);
    builder.mix_bool(authority.dropck_has_borrowed_drop);
    builder.mix_bool(authority.dropck_has_borrowed_field_dangling);
    builder.mix_bool(authority.dropck_has_destructor_escape);
    builder.mix_bool(authority.dropck_has_drop_glue_missing);
    builder.mix_bool(authority.place_state_graph_missing);
    builder.mix_bool(authority.place_state_has_partial_projection);
    builder.mix_bool(authority.place_state_has_drop_action);
    builder.mix_bool(authority.place_state_has_move_candidate);
    builder.mix_bool(authority.place_state_has_borrow);
    builder.mix_bool(authority.place_state_has_partial_move);
    builder.mix_bool(authority.place_state_has_skipped_drop);
    builder.mix_bool(authority.place_state_has_violation);
    builder.mix_bool(authority.place_state_has_emitted_diagnostics);
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
