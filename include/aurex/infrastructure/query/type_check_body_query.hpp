#pragma once

#include <aurex/infrastructure/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct TypeCheckBodyAuthority {
    QueryResultFingerprint checked_body;
    QueryResultFingerprint body_syntax_result;
    QueryResultFingerprint signature_result;
    StableFingerprint128 borrow_summary_fingerprint;
    StableFingerprint128 borrow_contract_fingerprint;
    StableFingerprint128 lifetime_fingerprint;
    StableFingerprint128 dropck_fingerprint;
    StableFingerprint128 place_state_fingerprint;
    StableFingerprint128 body_loan_fingerprint;
    StableFingerprint128 destructor_fingerprint;
    base::u64 expr_side_table_count = 0;
    base::u64 pattern_side_table_count = 0;
    base::u64 type_side_table_count = 0;
    base::u64 stmt_side_table_count = 0;
    base::u64 coercion_count = 0;
    base::u64 borrow_summary_origin_count = 0;
    base::u64 borrow_summary_dependency_count = 0;
    base::u64 borrow_summary_storage_escape_count = 0;
    base::u64 borrow_contract_selector_count = 0;
    base::u64 lifetime_region_count = 0;
    base::u64 lifetime_outlives_constraint_count = 0;
    base::u64 lifetime_type_outlives_constraint_count = 0;
    base::u64 lifetime_live_range_count = 0;
    base::u64 lifetime_return_region_count = 0;
    base::u64 lifetime_violation_count = 0;
    base::u64 type_lifetime_info_count = 0;
    base::u64 generic_lifetime_predicate_count = 0;
    base::u64 dropck_fact_count = 0;
    base::u64 dropck_action_count = 0;
    base::u64 dropck_required_outlives_count = 0;
    base::u64 dropck_violation_count = 0;
    base::u64 place_state_place_count = 0;
    base::u64 place_state_event_count = 0;
    base::u64 place_state_partial_projection_count = 0;
    base::u64 place_state_drop_place_count = 0;
    base::u64 place_state_move_candidate_count = 0;
    base::u64 place_state_borrow_event_count = 0;
    base::u64 place_state_partial_move_count = 0;
    base::u64 place_state_skipped_drop_count = 0;
    base::u64 place_state_violation_count = 0;
    base::u64 place_state_emitted_diagnostic_count = 0;
    base::u64 body_loan_count = 0;
    base::u64 body_reborrow_count = 0;
    base::u64 body_two_phase_borrow_count = 0;
    base::u64 body_loan_conflict_count = 0;
    base::u64 destructor_count = 0;
    bool retained_side_tables = false;
    bool has_diagnostics = false;
    bool has_borrow_summary = false;
    bool has_borrow_contract = false;
    bool has_lifetime_facts = false;
    bool has_dropck_facts = false;
    bool has_place_state_facts = false;
    bool has_body_loan_check = false;
    bool has_destructor_facts = false;
    bool borrow_summary_has_unknown_return_origin = false;
    bool borrow_summary_has_local_return_escape = false;
    bool borrow_summary_has_storage_escape = false;
    bool borrow_contract_unknown_return_allowed = false;
    bool borrow_contract_has_local_return_escape = false;
    bool borrow_contract_has_mismatch = false;
    bool lifetime_has_emitted_diagnostics = false;
    bool lifetime_has_unknown_origin = false;
    bool lifetime_has_ambiguous_elision = false;
    bool lifetime_has_return_origin_mismatch = false;
    bool lifetime_has_local_escape = false;
    bool lifetime_has_unknown_escape = false;
    bool dropck_graph_missing = false;
    bool dropck_has_emitted_diagnostics = false;
    bool dropck_has_generic_type_outlives = false;
    bool dropck_has_borrowed_drop = false;
    bool dropck_has_borrowed_field_dangling = false;
    bool dropck_has_destructor_escape = false;
    bool dropck_has_drop_glue_missing = false;
    bool place_state_graph_missing = false;
    bool place_state_has_partial_projection = false;
    bool place_state_has_drop_action = false;
    bool place_state_has_move_candidate = false;
    bool place_state_has_borrow = false;
    bool place_state_has_partial_move = false;
    bool place_state_has_skipped_drop = false;
    bool place_state_has_violation = false;
    bool place_state_has_emitted_diagnostics = false;
    bool body_loan_graph_missing = false;
    bool body_loan_has_emitted_diagnostics = false;
    bool body_two_phase_has_emitted_diagnostics = false;
};

struct TypeCheckBodyProviderInput {
    BodyKey key;
    TypeCheckBodyAuthority authority;
};

struct TypeCheckBodyProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> type_check_body_query_key(BodyKey key) noexcept;
[[nodiscard]] bool is_valid(const TypeCheckBodyAuthority& authority) noexcept;
[[nodiscard]] bool is_valid(const TypeCheckBodyProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const TypeCheckBodyProviderOutput& output) noexcept;
[[nodiscard]] QueryResultFingerprint type_check_body_result_fingerprint(
    const TypeCheckBodyAuthority& authority) noexcept;
[[nodiscard]] std::optional<TypeCheckBodyProviderOutput> provide_type_check_body_query(
    const TypeCheckBodyProviderInput& input);

} // namespace aurex::query
