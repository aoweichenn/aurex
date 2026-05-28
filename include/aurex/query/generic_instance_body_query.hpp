#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct GenericInstanceBodyAuthority {
    QueryResultFingerprint checked_body;
    QueryResultFingerprint signature_result;
    base::u32 expr_side_table_count = 0;
    base::u32 pattern_side_table_count = 0;
    base::u32 type_side_table_count = 0;
    base::u32 stmt_side_table_count = 0;
    base::u32 sparse_fallback_count = 0;
    bool retained_side_tables = false;
    bool local_dense_side_tables = false;
    bool sparse_side_tables = false;
};

struct GenericInstanceBodyProviderInput {
    const GenericInstanceKey* key = nullptr;
    GenericInstanceBodyAuthority authority;
};

struct GenericInstanceBodyProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> generic_instance_body_query_key(const GenericInstanceKey& key) noexcept;
[[nodiscard]] bool is_valid(const GenericInstanceBodyAuthority& authority) noexcept;
[[nodiscard]] bool is_valid(const GenericInstanceBodyProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const GenericInstanceBodyProviderOutput& output) noexcept;
[[nodiscard]] QueryResultFingerprint generic_instance_body_result_fingerprint(
    const GenericInstanceBodyAuthority& authority) noexcept;
[[nodiscard]] std::optional<GenericInstanceBodyProviderOutput> provide_generic_instance_body_query(
    const GenericInstanceBodyProviderInput& input);

} // namespace aurex::query
