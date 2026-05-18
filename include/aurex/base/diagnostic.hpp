#pragma once

#include <aurex/base/source.hpp>

#include <span>
#include <string>
#include <vector>

namespace aurex::base {

enum class Severity {
    note,
    help,
    warning,
    error,
    fatal,
};

enum class DiagnosticCategory {
    general,
    lexer,
    parser,
    semantic,
    type,
    name_resolution,
    visibility,
    pattern,
    safety,
    unsupported,
    capability,
    module,
    internal,
};

enum class DiagnosticCode {
    none,
    lexer_invalid_token,
    lexer_error_budget,
    parser_syntax,
    parser_note,
    semantic_error,
    semantic_type_mismatch,
    semantic_lookup,
    semantic_duplicate,
    semantic_visibility,
    semantic_unsupported,
    semantic_unsafe_required,
    semantic_capability,
    semantic_pattern,
    semantic_pattern_exhaustiveness,
    semantic_pattern_unreachable,
    module_error,
    internal_contract,
};

struct Diagnostic {
    Severity severity = Severity::error;
    SourceRange range{};
    std::string message;
    DiagnosticCategory category = DiagnosticCategory::general;
    DiagnosticCode code = DiagnosticCode::none;
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
[[nodiscard]] std::string_view diagnostic_category_name(DiagnosticCategory category) noexcept;
[[nodiscard]] std::string_view diagnostic_code_name(DiagnosticCode code) noexcept;

} // namespace aurex::base
