#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct TypeCheckBodyProviderInput {
    BodyKey key;
    QueryResultFingerprint checked_body;
};

struct TypeCheckBodyProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> type_check_body_query_key(BodyKey key) noexcept;
[[nodiscard]] bool is_valid(const TypeCheckBodyProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const TypeCheckBodyProviderOutput& output) noexcept;
[[nodiscard]] std::optional<TypeCheckBodyProviderOutput> provide_type_check_body_query(
    const TypeCheckBodyProviderInput& input);

} // namespace aurex::query
