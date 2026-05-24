#include <aurex/sema/sema_messages.hpp>

#include <utility>

#include <sema/internal/sema_diagnostics.hpp>

namespace aurex::sema {

SemanticDiagnosticReporter::SemanticDiagnosticReporter(
    base::DiagnosticSink& diagnostics, const TypeTable& types) noexcept
    : diagnostics_(diagnostics), types_(types)
{
}

void SemanticDiagnosticReporter::report(
    const base::SourceRange& range, const SemanticDiagnosticKind kind, std::string message) const
{
    const SemanticDiagnosticMetadata metadata = semantic_diagnostic_metadata(kind);
    this->report(range, std::move(message), metadata.category, metadata.code);
}

void SemanticDiagnosticReporter::report_general(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::general, std::move(message));
}

void SemanticDiagnosticReporter::report_type(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::type_mismatch, std::move(message));
}

void SemanticDiagnosticReporter::report_lookup(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::lookup, std::move(message));
}

void SemanticDiagnosticReporter::report_duplicate(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::duplicate, std::move(message));
}

void SemanticDiagnosticReporter::report_visibility(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::visibility, std::move(message));
}

void SemanticDiagnosticReporter::report_unsupported(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::unsupported, std::move(message));
}

void SemanticDiagnosticReporter::report_unsafe_required(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::unsafe_required, std::move(message));
}

void SemanticDiagnosticReporter::report_capability(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::capability, std::move(message));
}

void SemanticDiagnosticReporter::report_pattern(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::pattern, std::move(message));
}

void SemanticDiagnosticReporter::report_pattern_exhaustiveness(
    const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::pattern_exhaustiveness, std::move(message));
}

void SemanticDiagnosticReporter::report_pattern_unreachable(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::pattern_unreachable, std::move(message));
}

void SemanticDiagnosticReporter::report_internal_contract(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::internal_contract, std::move(message));
}

void SemanticDiagnosticReporter::report(const base::SourceRange& range, std::string message,
    const base::DiagnosticCategory category, const base::DiagnosticCode code) const
{
    this->diagnostics_.push(base::Diagnostic{
        base::Severity::error,
        range,
        std::move(message),
        category,
        code,
    });
}

void SemanticDiagnosticReporter::report_note(
    const base::SourceRange& range, const SemanticDiagnosticKind kind, std::string message) const
{
    const SemanticDiagnosticMetadata metadata = semantic_secondary_diagnostic_metadata(kind);
    this->diagnostics_.push(base::Diagnostic{
        base::Severity::note,
        range,
        std::move(message),
        metadata.category,
        metadata.code,
    });
}

void SemanticDiagnosticReporter::report_help(
    const base::SourceRange& range, const SemanticDiagnosticKind kind, std::string message) const
{
    const SemanticDiagnosticMetadata metadata = semantic_secondary_diagnostic_metadata(kind);
    this->diagnostics_.push(base::Diagnostic{
        base::Severity::help,
        range,
        std::move(message),
        metadata.category,
        metadata.code,
    });
}

void SemanticDiagnosticReporter::report_type_mismatch(
    const base::SourceRange& range, std::string message, const TypeHandle expected, const TypeHandle actual) const
{
    this->report(range, SemanticDiagnosticKind::type_mismatch, std::move(message));
    if (is_valid(expected)) {
        this->report_note(range, SemanticDiagnosticKind::type_mismatch,
            sema_expected_type_note_message(this->types_.display_name(expected)));
    }
    if (is_valid(actual)) {
        this->report_note(range, SemanticDiagnosticKind::type_mismatch,
            sema_actual_type_note_message(this->types_.display_name(actual)));
    }
}

void SemanticDiagnosticReporter::report_lookup_suggestion(
    const base::SourceRange& range, const std::string_view suggestion) const
{
    if (!suggestion.empty()) {
        this->report_help(range, SemanticDiagnosticKind::lookup, sema_did_you_mean_message(suggestion));
    }
}

} // namespace aurex::sema
