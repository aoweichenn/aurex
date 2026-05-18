#pragma once

#include <aurex/base/diagnostic.hpp>

namespace aurex::sema {

enum class SemanticDiagnosticKind {
    general,
    type_mismatch,
    lookup,
    duplicate,
    visibility,
    unsupported,
    unsafe_required,
    capability,
    pattern,
    pattern_exhaustiveness,
    pattern_unreachable,
    internal_contract,
};

struct SemanticDiagnosticMetadata {
    base::DiagnosticCategory category = base::DiagnosticCategory::semantic;
    base::DiagnosticCode code = base::DiagnosticCode::semantic_error;
};

[[nodiscard]] constexpr SemanticDiagnosticMetadata semantic_diagnostic_metadata(
    const SemanticDiagnosticKind kind) noexcept
{
    switch (kind) {
        case SemanticDiagnosticKind::general:
            return SemanticDiagnosticMetadata{
                base::DiagnosticCategory::semantic,
                base::DiagnosticCode::semantic_error,
            };
        case SemanticDiagnosticKind::type_mismatch:
            return SemanticDiagnosticMetadata{
                base::DiagnosticCategory::type,
                base::DiagnosticCode::semantic_type_mismatch,
            };
        case SemanticDiagnosticKind::lookup:
            return SemanticDiagnosticMetadata{
                base::DiagnosticCategory::name_resolution,
                base::DiagnosticCode::semantic_lookup,
            };
        case SemanticDiagnosticKind::duplicate:
            return SemanticDiagnosticMetadata{
                base::DiagnosticCategory::name_resolution,
                base::DiagnosticCode::semantic_duplicate,
            };
        case SemanticDiagnosticKind::visibility:
            return SemanticDiagnosticMetadata{
                base::DiagnosticCategory::visibility,
                base::DiagnosticCode::semantic_visibility,
            };
        case SemanticDiagnosticKind::unsupported:
            return SemanticDiagnosticMetadata{
                base::DiagnosticCategory::unsupported,
                base::DiagnosticCode::semantic_unsupported,
            };
        case SemanticDiagnosticKind::unsafe_required:
            return SemanticDiagnosticMetadata{
                base::DiagnosticCategory::safety,
                base::DiagnosticCode::semantic_unsafe_required,
            };
        case SemanticDiagnosticKind::capability:
            return SemanticDiagnosticMetadata{
                base::DiagnosticCategory::capability,
                base::DiagnosticCode::semantic_capability,
            };
        case SemanticDiagnosticKind::pattern:
            return SemanticDiagnosticMetadata{
                base::DiagnosticCategory::pattern,
                base::DiagnosticCode::semantic_pattern,
            };
        case SemanticDiagnosticKind::pattern_exhaustiveness:
            return SemanticDiagnosticMetadata{
                base::DiagnosticCategory::pattern,
                base::DiagnosticCode::semantic_pattern_exhaustiveness,
            };
        case SemanticDiagnosticKind::pattern_unreachable:
            return SemanticDiagnosticMetadata{
                base::DiagnosticCategory::pattern,
                base::DiagnosticCode::semantic_pattern_unreachable,
            };
        case SemanticDiagnosticKind::internal_contract:
            return SemanticDiagnosticMetadata{
                base::DiagnosticCategory::internal,
                base::DiagnosticCode::internal_contract,
            };
    }
    return SemanticDiagnosticMetadata{};
}

[[nodiscard]] constexpr SemanticDiagnosticMetadata semantic_secondary_diagnostic_metadata(
    const SemanticDiagnosticKind kind) noexcept
{
    SemanticDiagnosticMetadata metadata = semantic_diagnostic_metadata(kind);
    if (metadata.code == base::DiagnosticCode::semantic_error) {
        metadata.code = base::DiagnosticCode::none;
    }
    return metadata;
}

} // namespace aurex::sema
