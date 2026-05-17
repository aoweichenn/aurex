#include <aurex/base/diagnostic.hpp>

#include <utility>

namespace aurex::base {

namespace {

[[nodiscard]] bool is_error_severity(const Severity severity) noexcept {
    return severity == Severity::error || severity == Severity::fatal;
}

} // namespace

void DiagnosticSink::push(Diagnostic diagnostic) {
    this->has_error_ = this->has_error_ || is_error_severity(diagnostic.severity);
    this->diagnostics_.push_back(std::move(diagnostic));
}

bool DiagnosticSink::has_error() const noexcept {
    return this->has_error_;
}

std::span<const Diagnostic> DiagnosticSink::diagnostics() const noexcept {
    return this->diagnostics_;
}

std::string_view severity_name(const Severity severity) noexcept {
    switch (severity) {
    case Severity::note:
        return "note";
    case Severity::help:
        return "help";
    case Severity::warning:
        return "warning";
    case Severity::error:
        return "error";
    case Severity::fatal:
        return "fatal";
    }
    return "unknown";
}

std::string_view diagnostic_category_name(const DiagnosticCategory category) noexcept {
    switch (category) {
    case DiagnosticCategory::general:
        return "general";
    case DiagnosticCategory::lexer:
        return "lexer";
    case DiagnosticCategory::parser:
        return "parser";
    case DiagnosticCategory::semantic:
        return "semantic";
    case DiagnosticCategory::type:
        return "type";
    case DiagnosticCategory::name_resolution:
        return "name_resolution";
    case DiagnosticCategory::visibility:
        return "visibility";
    case DiagnosticCategory::pattern:
        return "pattern";
    case DiagnosticCategory::safety:
        return "safety";
    case DiagnosticCategory::unsupported:
        return "unsupported";
    case DiagnosticCategory::capability:
        return "capability";
    case DiagnosticCategory::module:
        return "module";
    case DiagnosticCategory::internal:
        return "internal";
    }
    return "unknown";
}

std::string_view diagnostic_code_name(const DiagnosticCode code) noexcept {
    switch (code) {
    case DiagnosticCode::none:
        return "none";
    case DiagnosticCode::lexer_invalid_token:
        return "LEX0001";
    case DiagnosticCode::lexer_error_budget:
        return "LEX0002";
    case DiagnosticCode::parser_syntax:
        return "PAR0001";
    case DiagnosticCode::parser_note:
        return "PAR0002";
    case DiagnosticCode::semantic_error:
        return "SEM0001";
    case DiagnosticCode::semantic_type_mismatch:
        return "SEM0100";
    case DiagnosticCode::semantic_lookup:
        return "SEM0200";
    case DiagnosticCode::semantic_duplicate:
        return "SEM0201";
    case DiagnosticCode::semantic_visibility:
        return "SEM0202";
    case DiagnosticCode::semantic_unsupported:
        return "SEM0300";
    case DiagnosticCode::semantic_unsafe_required:
        return "SEM0400";
    case DiagnosticCode::semantic_capability:
        return "SEM0450";
    case DiagnosticCode::semantic_pattern:
        return "SEM0500";
    case DiagnosticCode::semantic_pattern_exhaustiveness:
        return "SEM0501";
    case DiagnosticCode::semantic_pattern_unreachable:
        return "SEM0502";
    case DiagnosticCode::module_error:
        return "MOD0001";
    case DiagnosticCode::internal_contract:
        return "INT0001";
    }
    return "unknown";
}

} // namespace aurex::base
