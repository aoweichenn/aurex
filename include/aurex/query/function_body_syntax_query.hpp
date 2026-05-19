#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct FunctionBodySyntaxProviderInput {
    BodyKey key;
    QueryResultFingerprint syntax;
};

struct FunctionBodySyntaxProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> function_body_syntax_query_key(BodyKey key) noexcept;
[[nodiscard]] bool is_valid(const FunctionBodySyntaxProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const FunctionBodySyntaxProviderOutput& output) noexcept;
[[nodiscard]] std::optional<FunctionBodySyntaxProviderOutput> provide_function_body_syntax_query(
    const FunctionBodySyntaxProviderInput& input);

} // namespace aurex::query
