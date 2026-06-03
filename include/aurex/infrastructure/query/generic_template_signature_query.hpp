#pragma once

#include <aurex/infrastructure/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct GenericTemplateSignatureAuthority {
    IncrementalKey signature;
    ModulePartKey module_part;
    DefNamespace name_space = DefNamespace::value;
    base::u8 visibility_rank = 0;
    base::u32 param_count = 0;
    base::u32 constraint_count = 0;
};

struct GenericTemplateSignatureProviderInput {
    DefKey key;
    GenericTemplateSignatureAuthority authority;
};

struct GenericTemplateSignatureProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> generic_template_signature_query_key(DefKey key) noexcept;
[[nodiscard]] bool is_valid(const GenericTemplateSignatureAuthority& authority) noexcept;
[[nodiscard]] bool is_valid(const GenericTemplateSignatureProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const GenericTemplateSignatureProviderOutput& output) noexcept;
[[nodiscard]] QueryResultFingerprint generic_template_signature_result_fingerprint(
    const GenericTemplateSignatureAuthority& authority) noexcept;
[[nodiscard]] std::optional<GenericTemplateSignatureProviderOutput> provide_generic_template_signature_query(
    const GenericTemplateSignatureProviderInput& input);

} // namespace aurex::query
