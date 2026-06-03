#include <aurex/infrastructure/query/diagnostics_query.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <string_view>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_DIAGNOSTICS_RESULT_MARKER = "query-diagnostics:v1";

void mix_source_range(StableHashBuilder& builder, const base::SourceRange& range) noexcept
{
    builder.mix_u64(range.source.value);
    builder.mix_u64(range.begin);
    builder.mix_u64(range.end);
}

[[nodiscard]] std::vector<base::DiagnosticLabel> diagnostic_labels_for_event(const base::Diagnostic& diagnostic)
{
    if (!diagnostic.labels.empty()) {
        return diagnostic.labels;
    }
    return {base::primary_diagnostic_label(diagnostic.range, {})};
}

void mix_diagnostic_child(StableHashBuilder& builder, const base::DiagnosticChild& child) noexcept
{
    builder.mix_u64(static_cast<base::u64>(child.severity));
    builder.mix_u64(static_cast<base::u64>(child.category));
    builder.mix_u64(static_cast<base::u64>(child.code));
    mix_source_range(builder, child.range);
}

void mix_diagnostic_label(StableHashBuilder& builder, const base::DiagnosticLabel& label) noexcept
{
    builder.mix_u64(static_cast<base::u64>(label.style));
    mix_source_range(builder, label.range);
}

} // namespace

std::optional<QueryKey> diagnostics_query_key(const QueryKey producer) noexcept
{
    if (!is_valid(producer) || producer.kind == QueryKind::diagnostics) {
        return std::nullopt;
    }
    return query_key(QueryKind::diagnostics, stable_key_fingerprint(producer));
}

DiagnosticsEventStream diagnostic_events_from_sink(const std::span<const base::Diagnostic> diagnostics)
{
    DiagnosticsEventStream stream;
    stream.events.reserve(diagnostics.size());
    for (base::usize index = 0; index < diagnostics.size(); ++index) {
        const base::Diagnostic& diagnostic = diagnostics[index];
        stream.events.push_back(QueryDiagnosticEvent{
            diagnostic.severity,
            diagnostic.category,
            diagnostic.code,
            diagnostic.range,
            diagnostic.message,
            diagnostic_labels_for_event(diagnostic),
            diagnostic.children,
            static_cast<base::u32>(index),
        });
    }
    return stream;
}

QueryResultFingerprint diagnostics_result_fingerprint(
    const std::span<const QueryDiagnosticEvent> events, const std::span<const std::string_view> context)
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_DIAGNOSTICS_RESULT_MARKER);
    builder.mix_u64(context.size());
    for (base::usize index = 0; index < context.size(); ++index) {
        builder.mix_u64(index);
        builder.mix_string(context[index]);
    }
    builder.mix_u64(events.size());
    for (const QueryDiagnosticEvent& event : events) {
        builder.mix_u64(event.ordinal);
        builder.mix_u64(static_cast<base::u64>(event.severity));
        builder.mix_u64(static_cast<base::u64>(event.category));
        builder.mix_u64(static_cast<base::u64>(event.code));
        mix_source_range(builder, event.range);
        builder.mix_u64(event.labels.size());
        for (const base::DiagnosticLabel& label : event.labels) {
            mix_diagnostic_label(builder, label);
        }
        builder.mix_u64(event.children.size());
        for (const base::DiagnosticChild& child : event.children) {
            mix_diagnostic_child(builder, child);
        }
    }
    return query_result_fingerprint(builder.finish());
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
    for (base::usize index = 0; index < output.stream.events.size(); ++index) {
        const QueryDiagnosticEvent& event = output.stream.events[index];
        if (event.ordinal != index || event.range.begin > event.range.end) {
            return false;
        }
        for (const base::DiagnosticLabel& label : event.labels) {
            if (label.range.begin > label.range.end) {
                return false;
            }
        }
        for (const base::DiagnosticChild& child : event.children) {
            if (child.range.begin > child.range.end) {
                return false;
            }
        }
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
    DiagnosticsEventStream stream;
    stream.events.assign(input.events.begin(), input.events.end());
    return DiagnosticsProviderOutput{
        std::move(*record),
        input.diagnostics,
        std::move(stream),
        {input.producer},
    };
}

} // namespace aurex::query
