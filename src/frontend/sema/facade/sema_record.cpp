#include <utility>

#include <frontend/sema/internal/core/private/sema_core.hpp>
#include <frontend/sema/internal/core/private/sema_side_tables.hpp>
#include <frontend/sema/internal/diagnostics/private/sema_diagnostics.hpp>

namespace aurex::sema {

SemanticSideTableStore SemanticAnalyzerCore::side_table_store() noexcept
{
    return SemanticSideTableStore(*this);
}

SemanticSideTableReader SemanticAnalyzerCore::side_table_reader() const noexcept
{
    return SemanticSideTableReader(*this);
}

void SemanticAnalyzerCore::record_stmt_local_type(const syntax::StmtId stmt, const TypeHandle type)
{
    this->side_table_store().record_stmt_local_type(stmt, type);
}

void SemanticAnalyzerCore::record_expr_c_name(const syntax::ExprId expr, const std::string_view c_name)
{
    this->side_table_store().record_expr_c_name(expr, c_name);
}

void SemanticAnalyzerCore::record_pattern_c_name(const syntax::PatternId pattern, const std::string_view c_name)
{
    this->side_table_store().record_pattern_c_name(pattern, c_name);
}

void SemanticAnalyzerCore::record_pattern_case_name(const syntax::PatternId pattern, const std::string_view c_name)
{
    this->side_table_store().record_pattern_case_name(pattern, c_name);
}

void SemanticAnalyzerCore::merge_pattern_case_names(
    const syntax::PatternId pattern, const syntax::PatternId alternative)
{
    this->side_table_store().merge_pattern_case_names(pattern, alternative);
}

void SemanticAnalyzerCore::record_syntax_type_handle(const syntax::TypeId type, const TypeHandle resolved)
{
    this->side_table_store().record_syntax_type_handle(type, resolved);
}

TypeHandle SemanticAnalyzerCore::record_expr_type(const syntax::ExprId expr, const TypeHandle type)
{
    return this->side_table_store().record_expr_type(expr, type);
}

TypeHandle SemanticAnalyzerCore::record_expr_intrinsic_type(const syntax::ExprId expr, const TypeHandle type)
{
    return this->side_table_store().record_expr_intrinsic_type(expr, type);
}

TypeHandle SemanticAnalyzerCore::record_expr_types(
    const syntax::ExprId expr, const TypeHandle intrinsic_type, const TypeHandle final_type)
{
    return this->side_table_store().record_expr_types(expr, intrinsic_type, final_type);
}

void SemanticAnalyzerCore::record_expr_expected_type(const syntax::ExprId expr, const TypeHandle expected_type)
{
    this->side_table_store().record_expr_expected_type(expr, expected_type);
}

void SemanticAnalyzerCore::record_expr_owned_use_mode(const syntax::ExprId expr, const OwnedUseMode mode)
{
    this->side_table_store().record_expr_owned_use_mode(expr, mode);
}

void SemanticAnalyzerCore::record_coercion(
    const syntax::ExprId expr, const TypeHandle from_type, const TypeHandle to_type, const CoercionKind kind)
{
    this->side_table_store().record_coercion(expr, from_type, to_type, kind);
}

TypeHandle SemanticAnalyzerCore::cached_expr_intrinsic_type(const syntax::ExprId expr) const noexcept
{
    return this->side_table_reader().cached_expr_intrinsic_type(expr);
}

TypeHandle SemanticAnalyzerCore::cached_expr_type(const syntax::ExprId expr) const noexcept
{
    return this->side_table_reader().cached_expr_type(expr);
}

TypeHandle SemanticAnalyzerCore::cached_expr_expected_type(const syntax::ExprId expr) const noexcept
{
    return this->side_table_reader().cached_expr_expected_type(expr);
}

OwnedUseMode SemanticAnalyzerCore::cached_expr_owned_use_mode(const syntax::ExprId expr) const noexcept
{
    return this->side_table_reader().cached_expr_owned_use_mode(expr);
}

TypeHandle SemanticAnalyzerCore::cached_expr_type_for_expected(
    const syntax::ExprId expr, const TypeHandle expected_type) const noexcept
{
    return this->side_table_reader().cached_expr_type_for_expected(expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::cached_syntax_type(const syntax::TypeId type) const noexcept
{
    return this->side_table_reader().cached_syntax_type(type);
}

TypeHandle SemanticAnalyzerCore::cached_stmt_local_type(const syntax::StmtId stmt) const noexcept
{
    return this->side_table_reader().cached_stmt_local_type(stmt);
}

std::string_view SemanticAnalyzerCore::cached_expr_c_name(const syntax::ExprId expr) const noexcept
{
    return this->side_table_reader().cached_expr_c_name(expr);
}

std::string_view SemanticAnalyzerCore::cached_pattern_c_name(const syntax::PatternId pattern) const noexcept
{
    return this->side_table_reader().cached_pattern_c_name(pattern);
}

SemaTypeTable& SemanticAnalyzerCore::active_expr_types() noexcept
{
    return this->side_table_store().active_expr_types();
}

SemaTypeTable& SemanticAnalyzerCore::active_expr_intrinsic_types() noexcept
{
    return this->side_table_store().active_expr_intrinsic_types();
}

SemaTypeTable& SemanticAnalyzerCore::active_expr_expected_types() noexcept
{
    return this->side_table_store().active_expr_expected_types();
}

SemaOwnedUseModeTable& SemanticAnalyzerCore::active_expr_owned_use_modes() noexcept
{
    return this->side_table_store().active_expr_owned_use_modes();
}

SemaIdentTable& SemanticAnalyzerCore::active_expr_c_name_ids() noexcept
{
    return this->side_table_store().active_expr_c_name_ids();
}

SemaIdentTable& SemanticAnalyzerCore::active_pattern_c_name_ids() noexcept
{
    return this->side_table_store().active_pattern_c_name_ids();
}

PatternCaseNameTable& SemanticAnalyzerCore::active_pattern_case_name_ids() noexcept
{
    return this->side_table_store().active_pattern_case_name_ids();
}

SemaTypeTable& SemanticAnalyzerCore::active_syntax_type_handles() noexcept
{
    return this->side_table_store().active_syntax_type_handles();
}

SemaTypeTable& SemanticAnalyzerCore::active_stmt_local_types() noexcept
{
    return this->side_table_store().active_stmt_local_types();
}

SemanticDiagnosticReporter SemanticAnalyzerCore::diagnostic_reporter() const noexcept
{
    return SemanticDiagnosticReporter(this->ctx_.diagnostics, this->state_.checked.types);
}

void SemanticAnalyzerCore::report(
    const base::SourceRange& range, const SemanticDiagnosticKind kind, std::string message) const
{
    this->diagnostic_reporter().report(range, kind, std::move(message));
}

void SemanticAnalyzerCore::report_general(const base::SourceRange& range, std::string message) const
{
    this->diagnostic_reporter().report_general(range, std::move(message));
}

void SemanticAnalyzerCore::report_type(const base::SourceRange& range, std::string message) const
{
    this->diagnostic_reporter().report_type(range, std::move(message));
}

void SemanticAnalyzerCore::report_lookup(const base::SourceRange& range, std::string message) const
{
    this->diagnostic_reporter().report_lookup(range, std::move(message));
}

void SemanticAnalyzerCore::report_duplicate(const base::SourceRange& range, std::string message) const
{
    this->diagnostic_reporter().report_duplicate(range, std::move(message));
}

void SemanticAnalyzerCore::report_visibility(const base::SourceRange& range, std::string message) const
{
    this->diagnostic_reporter().report_visibility(range, std::move(message));
}

void SemanticAnalyzerCore::report_unsupported(const base::SourceRange& range, std::string message) const
{
    this->diagnostic_reporter().report_unsupported(range, std::move(message));
}

void SemanticAnalyzerCore::report_unsafe_required(const base::SourceRange& range, std::string message) const
{
    this->diagnostic_reporter().report_unsafe_required(range, std::move(message));
}

void SemanticAnalyzerCore::report_capability(const base::SourceRange& range, std::string message) const
{
    this->diagnostic_reporter().report_capability(range, std::move(message));
}

void SemanticAnalyzerCore::report_pattern(const base::SourceRange& range, std::string message) const
{
    this->diagnostic_reporter().report_pattern(range, std::move(message));
}

void SemanticAnalyzerCore::report_pattern_exhaustiveness(const base::SourceRange& range, std::string message) const
{
    this->diagnostic_reporter().report_pattern_exhaustiveness(range, std::move(message));
}

void SemanticAnalyzerCore::report_pattern_unreachable(const base::SourceRange& range, std::string message) const
{
    this->diagnostic_reporter().report_pattern_unreachable(range, std::move(message));
}

void SemanticAnalyzerCore::report_internal_contract(const base::SourceRange& range, std::string message) const
{
    this->diagnostic_reporter().report_internal_contract(range, std::move(message));
}

void SemanticAnalyzerCore::report(const base::SourceRange& range, std::string message,
    const base::DiagnosticCategory category, const base::DiagnosticCode code) const
{
    this->diagnostic_reporter().report(range, std::move(message), category, code);
}

void SemanticAnalyzerCore::report_note(
    const base::SourceRange& range, const SemanticDiagnosticKind kind, std::string message) const
{
    this->diagnostic_reporter().report_note(range, kind, std::move(message));
}

void SemanticAnalyzerCore::report_help(
    const base::SourceRange& range, const SemanticDiagnosticKind kind, std::string message) const
{
    this->diagnostic_reporter().report_help(range, kind, std::move(message));
}

void SemanticAnalyzerCore::report_type_mismatch(
    const base::SourceRange& range, std::string message, const TypeHandle expected, const TypeHandle actual) const
{
    this->diagnostic_reporter().report_type_mismatch(range, std::move(message), expected, actual);
}

void SemanticAnalyzerCore::report_lookup_suggestion(
    const base::SourceRange& range, const std::string_view suggestion) const
{
    this->diagnostic_reporter().report_lookup_suggestion(range, suggestion);
}

} // namespace aurex::sema
