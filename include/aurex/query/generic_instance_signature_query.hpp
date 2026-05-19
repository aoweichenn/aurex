#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct GenericInstanceSignatureProviderInput {
    const GenericInstanceKey* key = nullptr;
    IncrementalKey signature;
};

struct GenericInstanceSignatureProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> generic_instance_signature_query_key(const GenericInstanceKey& key) noexcept;
[[nodiscard]] bool is_valid(const GenericInstanceSignatureProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const GenericInstanceSignatureProviderOutput& output) noexcept;
[[nodiscard]] std::optional<GenericInstanceSignatureProviderOutput> provide_generic_instance_signature_query(
    const GenericInstanceSignatureProviderInput& input);

} // namespace aurex::query
