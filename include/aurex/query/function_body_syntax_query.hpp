#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct FunctionBodySyntaxAuthority {
    QueryResultFingerprint syntax;
    DefKey owner;
    ModulePartKey module_part;
    base::u64 range_begin = 0;
    base::u64 range_end = 0;
    BodySlotKind slot = BodySlotKind::function_body;
    base::u32 ordinal = 0;
};

struct FunctionBodySyntaxProviderInput {
    BodyKey key;
    FunctionBodySyntaxAuthority authority;
};

struct FunctionBodySyntaxProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> function_body_syntax_query_key(BodyKey key) noexcept;
[[nodiscard]] bool is_valid(const FunctionBodySyntaxAuthority& authority) noexcept;
[[nodiscard]] bool is_valid(const FunctionBodySyntaxProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const FunctionBodySyntaxProviderOutput& output) noexcept;
[[nodiscard]] QueryResultFingerprint function_body_syntax_result_fingerprint(
    const FunctionBodySyntaxAuthority& authority) noexcept;
[[nodiscard]] std::optional<FunctionBodySyntaxProviderOutput> provide_function_body_syntax_query(
    const FunctionBodySyntaxProviderInput& input);

} // namespace aurex::query
