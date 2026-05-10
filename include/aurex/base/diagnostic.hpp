#pragma once

#include <aurex/base/source.hpp>

#include <span>
#include <string>
#include <vector>

namespace aurex::base {

enum class Severity {
    note,
    warning,
    error,
    fatal,
};

struct Diagnostic {
    Severity severity = Severity::error;
    SourceRange range {};
    std::string message;
};

class DiagnosticSink {
public:
    void push(Diagnostic diagnostic);

    [[nodiscard]] bool has_error() const noexcept;
    [[nodiscard]] std::span<const Diagnostic> diagnostics() const noexcept;

private:
    std::vector<Diagnostic> diagnostics_;
    bool has_error_ = false;
};

[[nodiscard]] std::string_view severity_name(Severity severity) noexcept;

} // namespace aurex::base
