#pragma once

#include <aurex/infrastructure/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

enum class GenericInstanceSignatureKind : base::u8 {
    invalid = 0,
    function,
    method,
    struct_,
    enum_,
    type_alias,
};

struct GenericInstanceSignatureAuthority {
    IncrementalKey signature;
    GenericInstanceSignatureKind kind = GenericInstanceSignatureKind::invalid;
    base::u8 visibility_rank = 0;
    base::u32 type_arg_count = 0;
    base::u32 const_arg_count = 0;
    base::u32 param_env_predicate_count = 0;
    base::u32 value_param_count = 0;
    base::u32 generic_param_count = 0;
    bool has_return_type = false;
    bool has_receiver_type = false;
    bool is_unsafe = false;
    bool is_variadic = false;
    bool has_definition = false;
};

struct GenericInstanceSignatureProviderInput {
    const GenericInstanceKey* key = nullptr;
    GenericInstanceSignatureAuthority authority;
};

struct GenericInstanceSignatureProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> generic_instance_signature_query_key(const GenericInstanceKey& key) noexcept;
[[nodiscard]] bool is_valid(const GenericInstanceSignatureAuthority& authority) noexcept;
[[nodiscard]] bool is_valid(const GenericInstanceSignatureProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const GenericInstanceSignatureProviderOutput& output) noexcept;
[[nodiscard]] QueryResultFingerprint generic_instance_signature_result_fingerprint(
    const GenericInstanceSignatureAuthority& authority) noexcept;
[[nodiscard]] std::optional<GenericInstanceSignatureProviderOutput> provide_generic_instance_signature_query(
    const GenericInstanceSignatureProviderInput& input);

} // namespace aurex::query
