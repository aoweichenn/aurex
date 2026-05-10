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
    case Severity::warning:
        return "warning";
    case Severity::error:
        return "error";
    case Severity::fatal:
        return "fatal";
    }
    return "unknown";
}

} // namespace aurex::base
