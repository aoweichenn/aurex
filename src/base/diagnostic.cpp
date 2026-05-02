#include "aurex/base/diagnostic.hpp"

#include <utility>

namespace aurex::base {

void DiagnosticSink::push(Diagnostic diagnostic) {
    diagnostics_.push_back(std::move(diagnostic));
}

bool DiagnosticSink::has_error() const noexcept {
    for (const Diagnostic& diagnostic : diagnostics_) {
        if (diagnostic.severity == Severity::error || diagnostic.severity == Severity::fatal) {
            return true;
        }
    }
    return false;
}

std::span<const Diagnostic> DiagnosticSink::diagnostics() const noexcept {
    return diagnostics_;
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
