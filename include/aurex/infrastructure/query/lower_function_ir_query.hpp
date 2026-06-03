#pragma once

#include <aurex/infrastructure/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct LowerFunctionIRProviderInput {
    BodyKey key;
    QueryResultFingerprint ir;
};

struct LowerFunctionIRProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

struct LowerGenericInstanceIRProviderInput {
    const GenericInstanceKey* key = nullptr;
    QueryResultFingerprint ir;
};

struct LowerGenericInstanceIRProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> lower_function_ir_query_key(BodyKey key) noexcept;
[[nodiscard]] std::optional<QueryKey> lower_generic_instance_ir_query_key(const GenericInstanceKey& key) noexcept;
[[nodiscard]] bool is_valid(const LowerFunctionIRProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const LowerFunctionIRProviderOutput& output) noexcept;
[[nodiscard]] bool is_valid(const LowerGenericInstanceIRProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const LowerGenericInstanceIRProviderOutput& output) noexcept;
[[nodiscard]] std::optional<LowerFunctionIRProviderOutput> provide_lower_function_ir_query(
    const LowerFunctionIRProviderInput& input);
[[nodiscard]] std::optional<LowerGenericInstanceIRProviderOutput> provide_lower_generic_instance_ir_query(
    const LowerGenericInstanceIRProviderInput& input);

} // namespace aurex::query
