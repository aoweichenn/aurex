#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct GenericTemplateSignatureProviderInput {
    DefKey key;
    IncrementalKey signature;
};

struct GenericTemplateSignatureProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> generic_template_signature_query_key(DefKey key) noexcept;
[[nodiscard]] bool is_valid(const GenericTemplateSignatureProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const GenericTemplateSignatureProviderOutput& output) noexcept;
[[nodiscard]] std::optional<GenericTemplateSignatureProviderOutput> provide_generic_template_signature_query(
    const GenericTemplateSignatureProviderInput& input);

} // namespace aurex::query
