#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/query/query_result.hpp>

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace aurex::query {

struct QueryDiagnosticEvent {
    base::Severity severity = base::Severity::error;
    base::DiagnosticCategory category = base::DiagnosticCategory::general;
    base::DiagnosticCode code = base::DiagnosticCode::none;
    base::SourceRange range{};
    std::string message;
    std::vector<base::DiagnosticLabel> labels;
    std::vector<base::DiagnosticChild> children;
    base::u32 ordinal = 0;
};

struct DiagnosticsEventStream {
    std::vector<QueryDiagnosticEvent> events;
};

struct DiagnosticsProviderInput {
    QueryKey producer;
    QueryResultFingerprint diagnostics;
    std::span<const QueryDiagnosticEvent> events{};
};

struct DiagnosticsProviderOutput {
    QueryRecord record;
    QueryResultFingerprint result;
    DiagnosticsEventStream stream;
    std::vector<QueryKey> dependencies;
};

[[nodiscard]] std::optional<QueryKey> diagnostics_query_key(QueryKey producer) noexcept;
[[nodiscard]] DiagnosticsEventStream diagnostic_events_from_sink(std::span<const base::Diagnostic> diagnostics);
[[nodiscard]] QueryResultFingerprint diagnostics_result_fingerprint(
    std::span<const QueryDiagnosticEvent> events, std::span<const std::string_view> context = {});
[[nodiscard]] bool is_valid(const DiagnosticsProviderInput& input) noexcept;
[[nodiscard]] bool is_valid(const DiagnosticsProviderOutput& output) noexcept;
[[nodiscard]] std::optional<DiagnosticsProviderOutput> provide_diagnostics_query(const DiagnosticsProviderInput& input);

} // namespace aurex::query
