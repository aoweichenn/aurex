#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/sema/diagnostic_kind.hpp>
#include <aurex/sema/type.hpp>

#include <string>
#include <string_view>

namespace aurex::sema {

class SemanticDiagnosticReporter final {
public:
    SemanticDiagnosticReporter(base::DiagnosticSink& diagnostics, const TypeTable& types) noexcept;

    void report(const base::SourceRange& range, SemanticDiagnosticKind kind, std::string message) const;
    void report_general(const base::SourceRange& range, std::string message) const;
    void report_type(const base::SourceRange& range, std::string message) const;
    void report_lookup(const base::SourceRange& range, std::string message) const;
    void report_duplicate(const base::SourceRange& range, std::string message) const;
    void report_visibility(const base::SourceRange& range, std::string message) const;
    void report_unsupported(const base::SourceRange& range, std::string message) const;
    void report_unsafe_required(const base::SourceRange& range, std::string message) const;
    void report_capability(const base::SourceRange& range, std::string message) const;
    void report_pattern(const base::SourceRange& range, std::string message) const;
    void report_pattern_exhaustiveness(const base::SourceRange& range, std::string message) const;
    void report_pattern_unreachable(const base::SourceRange& range, std::string message) const;
    void report_internal_contract(const base::SourceRange& range, std::string message) const;
    void report(const base::SourceRange& range, std::string message, base::DiagnosticCategory category,
        base::DiagnosticCode code) const;
    void report_note(const base::SourceRange& range, SemanticDiagnosticKind kind, std::string message) const;
    void report_help(const base::SourceRange& range, SemanticDiagnosticKind kind, std::string message) const;
    void report_type_mismatch(
        const base::SourceRange& range, std::string message, TypeHandle expected, TypeHandle actual) const;
    void report_lookup_suggestion(const base::SourceRange& range, std::string_view suggestion) const;

private:
    base::DiagnosticSink& diagnostics_;
    const TypeTable& types_;
};

} // namespace aurex::sema
