#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct GenericInstanceBodyProviderInput {
    const GenericInstanceKey* key = nullptr;
    QueryResultFingerprint checked_body;
};

struct GenericInstanceBodyProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> generic_instance_body_query_key(const GenericInstanceKey& key) noexcept;
[[nodiscard]] bool is_valid(const GenericInstanceBodyProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const GenericInstanceBodyProviderOutput& output) noexcept;
[[nodiscard]] std::optional<GenericInstanceBodyProviderOutput> provide_generic_instance_body_query(
    const GenericInstanceBodyProviderInput& input);

} // namespace aurex::query
