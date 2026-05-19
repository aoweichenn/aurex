#include <aurex/query/diagnostics_query.hpp>

#include <utility>

namespace aurex::query {

std::optional<QueryKey> diagnostics_query_key(const QueryKey producer) noexcept
{
    if (!is_valid(producer) || producer.kind == QueryKind::diagnostics) {
        return std::nullopt;
    }
    return query_key(QueryKind::diagnostics, stable_key_fingerprint(producer));
}

bool is_valid(const DiagnosticsProviderInput& input) noexcept
{
    return is_valid(input.producer) && input.producer.kind != QueryKind::diagnostics && is_valid(input.diagnostics);
}

bool is_valid(const DiagnosticsProviderOutput& output) noexcept
{
    if (!is_valid(output.record) || !is_valid(output.result) || output.record.key.kind != QueryKind::diagnostics
        || output.record.result != output.result) {
        return false;
    }
    for (const QueryKey dependency : output.dependencies) {
        if (!is_valid(dependency)) {
            return false;
        }
    }
    return true;
}

std::optional<DiagnosticsProviderOutput> provide_diagnostics_query(const DiagnosticsProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = diagnostics_query_record(input.producer, input.diagnostics);
    return DiagnosticsProviderOutput{
        std::move(*record),
        input.diagnostics,
        {input.producer},
    };
}

} // namespace aurex::query
