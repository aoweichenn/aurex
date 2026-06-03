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
    StableFingerprint128 body_loan_fingerprint;
    base::u32 expr_side_table_count = 0;
    base::u32 pattern_side_table_count = 0;
    base::u32 type_side_table_count = 0;
    base::u32 stmt_side_table_count = 0;
    base::u32 coercion_count = 0;
    base::u32 borrow_summary_origin_count = 0;
    base::u32 borrow_summary_dependency_count = 0;
    base::u32 borrow_contract_selector_count = 0;
    base::u32 lifetime_region_count = 0;
    base::u32 lifetime_outlives_constraint_count = 0;
    base::u32 lifetime_type_outlives_constraint_count = 0;
    base::u32 lifetime_return_region_count = 0;
    base::u32 lifetime_violation_count = 0;
    base::u32 body_loan_count = 0;
    base::u32 body_reborrow_count = 0;
    base::u32 body_two_phase_borrow_count = 0;
    base::u32 body_loan_conflict_count = 0;
    bool retained_side_tables = false;
    bool has_diagnostics = false;
    bool has_borrow_summary = false;
    bool has_borrow_contract = false;
    bool has_lifetime_facts = false;
    bool has_body_loan_check = false;
    bool borrow_summary_has_unknown_return_origin = false;
    bool borrow_summary_has_local_return_escape = false;
    bool borrow_contract_unknown_return_allowed = false;
    bool borrow_contract_has_local_return_escape = false;
    bool borrow_contract_has_mismatch = false;
    bool lifetime_has_emitted_diagnostics = false;
    bool lifetime_has_unknown_origin = false;
    bool lifetime_has_ambiguous_elision = false;
    bool lifetime_has_return_origin_mismatch = false;
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
