#pragma once

#include <aurex/query/query_result.hpp>

#include <optional>
#include <vector>

namespace aurex::query {

struct DiagnosticsProviderInput {
    QueryKey producer;
    QueryResultFingerprint diagnostics;
};

struct DiagnosticsProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> diagnostics_query_key(QueryKey producer) noexcept;
[[nodiscard]] bool is_valid(const DiagnosticsProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const DiagnosticsProviderOutput& output) noexcept;
[[nodiscard]] std::optional<DiagnosticsProviderOutput> provide_diagnostics_query(
    const DiagnosticsProviderInput& input);

} // namespace aurex::query
