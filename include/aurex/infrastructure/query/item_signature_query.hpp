#pragma once

#include <aurex/infrastructure/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct ItemSignatureAuthority {
    IncrementalKey signature;
    ModulePartKey module_part;
    DefNamespace name_space = DefNamespace::value;
    DefKind kind = DefKind::invalid;
    base::u8 visibility_rank = 0;
    base::u32 value_component_count = 0;
    base::u32 generic_param_count = 0;
    bool has_return_type = false;
    bool has_receiver_type = false;
    bool is_unsafe = false;
    bool is_variadic = false;
    bool has_definition = false;
};

struct ItemSignatureProviderInput {
    DefKey key;
    ItemSignatureAuthority authority;
};

struct ItemSignatureProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> item_signature_query_key(DefKey key) noexcept;
[[nodiscard]] bool is_valid(const ItemSignatureAuthority& authority) noexcept;
[[nodiscard]] bool is_valid(const ItemSignatureProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const ItemSignatureProviderOutput& output) noexcept;
[[nodiscard]] QueryResultFingerprint item_signature_result_fingerprint(
    const ItemSignatureAuthority& authority) noexcept;
[[nodiscard]] std::optional<ItemSignatureProviderOutput> provide_item_signature_query(
    const ItemSignatureProviderInput& input);

} // namespace aurex::query
