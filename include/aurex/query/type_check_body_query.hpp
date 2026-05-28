#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct TypeCheckBodyAuthority {
    QueryResultFingerprint checked_body;
    QueryResultFingerprint body_syntax_result;
    QueryResultFingerprint signature_result;
    base::u32 expr_side_table_count = 0;
    base::u32 pattern_side_table_count = 0;
    base::u32 type_side_table_count = 0;
    base::u32 stmt_side_table_count = 0;
    base::u32 coercion_count = 0;
    bool retained_side_tables = false;
    bool has_diagnostics = false;
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
