#include <aurex/sema/sema_messages.hpp>

#include <utility>

#include <sema/internal/sema_core.hpp>

namespace aurex::sema {

namespace {

template <typename T>
void ensure_side_table_slot(SemaVector<T>& table, const base::usize index)
{
    if (index >= table.size()) {
        table.resize(index + 1);
    }
}

template <>
void ensure_side_table_slot<TypeHandle>(SemaVector<TypeHandle>& table, const base::usize index)
{
    if (index >= table.size()) {
        table.resize(index + 1, INVALID_TYPE_HANDLE);
    }
}

template <typename T>
[[nodiscard]] bool record_local_dense_slot(SemaVector<T>& table, const base::usize index, const T value)
{
    if (index == SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX || index >= table.size()) {
        return false;
    }
    table[index] = value;
    return true;
}

template <typename T>
[[nodiscard]] T cached_local_dense_slot(const SemaVector<T>& table, const base::usize index, const T fallback) noexcept
{
    return index != SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX && index < table.size() ? table[index] : fallback;
}

void record_sparse_fallback(
    GenericSideTables& side_tables, const GenericSparseFallbackKind kind, const base::usize local_index) noexcept
{
    if (side_tables.local_dense && local_index == SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX) {
        side_tables.record_sparse_fallback(kind);
    }
}

} // namespace

void SemanticAnalyzerCore::record_stmt_local_type(const syntax::StmtId stmt, const TypeHandle type)
{
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->sparse) {
        if (syntax::is_valid(stmt)) {
            const base::usize local_index = this->state_.flow.current_side_tables.side_tables->local_stmt_index(stmt);
            if (record_local_dense_slot(
                    this->state_.flow.current_side_tables.side_tables->stmt_local_types, local_index, type)) {
                return;
            }
            record_sparse_fallback(*this->state_.flow.current_side_tables.side_tables,
                GenericSparseFallbackKind::stmt_local_type, local_index);
            this->state_.flow.current_side_tables.side_tables->sparse_stmt_local_types[stmt.value] = type;
        }
        return;
    }
    SemaTypeTable& stmt_local_types = this->active_stmt_local_types();
    if (syntax::is_valid(stmt)) {
        ensure_side_table_slot(stmt_local_types, stmt.value);
        stmt_local_types[stmt.value] = type;
    }
}

void SemanticAnalyzerCore::record_expr_c_name(const syntax::ExprId expr, const std::string_view c_name)
{
    if (!syntax::is_valid(expr) || c_name.empty()) {
        return;
    }
    const IdentId c_name_id = this->state_.checked.intern_c_name(c_name);
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->sparse) {
        const base::usize local_index = this->state_.flow.current_side_tables.side_tables->local_expr_index(expr);
        if (record_local_dense_slot(
                this->state_.flow.current_side_tables.side_tables->expr_c_name_ids, local_index, c_name_id)) {
            return;
        }
        record_sparse_fallback(
            *this->state_.flow.current_side_tables.side_tables, GenericSparseFallbackKind::expr_c_name, local_index);
        this->state_.flow.current_side_tables.side_tables->sparse_expr_c_name_ids[expr.value] = c_name_id;
        return;
    }
    SemaIdentTable& expr_c_name_ids = this->active_expr_c_name_ids();
    ensure_side_table_slot(expr_c_name_ids, expr.value);
    expr_c_name_ids[expr.value] = c_name_id;
}

void SemanticAnalyzerCore::record_pattern_c_name(const syntax::PatternId pattern, const std::string_view c_name)
{
    if (!syntax::is_valid(pattern) || c_name.empty()) {
        return;
    }
    const IdentId c_name_id = this->state_.checked.intern_c_name(c_name);
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->sparse) {
        const base::usize local_index = this->state_.flow.current_side_tables.side_tables->local_pattern_index(pattern);
        if (record_local_dense_slot(
                this->state_.flow.current_side_tables.side_tables->pattern_c_name_ids, local_index, c_name_id)) {
            return;
        }
        record_sparse_fallback(
            *this->state_.flow.current_side_tables.side_tables, GenericSparseFallbackKind::pattern_c_name, local_index);
        this->state_.flow.current_side_tables.side_tables->sparse_pattern_c_name_ids[pattern.value] = c_name_id;
        return;
    }
    SemaIdentTable& pattern_c_name_ids = this->active_pattern_c_name_ids();
    ensure_side_table_slot(pattern_c_name_ids, pattern.value);
    pattern_c_name_ids[pattern.value] = c_name_id;
}

void SemanticAnalyzerCore::record_pattern_case_name(const syntax::PatternId pattern, const std::string_view c_name)
{
    if (!syntax::is_valid(pattern) || c_name.empty()) {
        return;
    }
    const IdentId c_name_id = this->state_.checked.intern_c_name(c_name);
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->local_dense) {
        if (const base::usize local_index =
                this->state_.flow.current_side_tables.side_tables->local_pattern_index(pattern);
            local_index != SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX) {
            this->active_pattern_case_name_ids().insert(static_cast<base::u32>(local_index), c_name_id);
            return;
        }
        this->state_.flow.current_side_tables.side_tables->record_sparse_fallback(
            GenericSparseFallbackKind::pattern_case_name);
    }
    this->active_pattern_case_name_ids().insert(pattern.value, c_name_id);
}

void SemanticAnalyzerCore::merge_pattern_case_names(
    const syntax::PatternId pattern, const syntax::PatternId alternative)
{
    if (!syntax::is_valid(pattern) || !syntax::is_valid(alternative)) {
        return;
    }
    PatternCaseNameTable& pattern_case_name_ids = this->active_pattern_case_name_ids();
    base::u32 target_index = pattern.value;
    base::u32 alternative_index = alternative.value;
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->local_dense) {
        const base::usize target_local =
            this->state_.flow.current_side_tables.side_tables->local_pattern_index(pattern);
        const base::usize alternative_local =
            this->state_.flow.current_side_tables.side_tables->local_pattern_index(alternative);
        if (target_local != SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX) {
            target_index = static_cast<base::u32>(target_local);
        } else {
            this->state_.flow.current_side_tables.side_tables->record_sparse_fallback(
                GenericSparseFallbackKind::pattern_case_name);
        }
        if (alternative_local != SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX) {
            alternative_index = static_cast<base::u32>(alternative_local);
        } else {
            this->state_.flow.current_side_tables.side_tables->record_sparse_fallback(
                GenericSparseFallbackKind::pattern_case_name);
        }
    }
    const auto found = pattern_case_name_ids.find(alternative_index);
    if (found != pattern_case_name_ids.end()) {
        pattern_case_name_ids.merge(target_index, found->second);
    }
}

void SemanticAnalyzerCore::record_syntax_type_handle(const syntax::TypeId type, const TypeHandle resolved)
{
    if (!this->state_.flow.current_side_tables.cache_syntax_types) {
        return;
    }
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->sparse) {
        if (syntax::is_valid(type)) {
            const base::usize local_index = this->state_.flow.current_side_tables.side_tables->local_type_index(type);
            if (record_local_dense_slot(
                    this->state_.flow.current_side_tables.side_tables->syntax_type_handles, local_index, resolved)) {
                return;
            }
            record_sparse_fallback(*this->state_.flow.current_side_tables.side_tables,
                GenericSparseFallbackKind::syntax_type, local_index);
            this->state_.flow.current_side_tables.side_tables->sparse_syntax_type_handles[type.value] = resolved;
        }
        return;
    }
    SemaTypeTable& syntax_type_handles = this->active_syntax_type_handles();
    if (syntax::is_valid(type)) {
        ensure_side_table_slot(syntax_type_handles, type.value);
        syntax_type_handles[type.value] = resolved;
    }
}

TypeHandle SemanticAnalyzerCore::record_expr_type(const syntax::ExprId expr, const TypeHandle type)
{
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->sparse) {
        if (syntax::is_valid(expr)) {
            const base::usize local_index = this->state_.flow.current_side_tables.side_tables->local_expr_index(expr);
            if (record_local_dense_slot(
                    this->state_.flow.current_side_tables.side_tables->expr_types, local_index, type)) {
                return type;
            }
            record_sparse_fallback(
                *this->state_.flow.current_side_tables.side_tables, GenericSparseFallbackKind::expr_type, local_index);
            this->state_.flow.current_side_tables.side_tables->sparse_expr_types[expr.value] = type;
        }
        return type;
    }
    SemaTypeTable& expr_types = this->active_expr_types();
    if (syntax::is_valid(expr)) {
        ensure_side_table_slot(expr_types, expr.value);
        expr_types[expr.value] = type;
    }
    return type;
}

TypeHandle SemanticAnalyzerCore::record_expr_intrinsic_type(const syntax::ExprId expr, const TypeHandle type)
{
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->sparse) {
        if (syntax::is_valid(expr)) {
            const base::usize local_index = this->state_.flow.current_side_tables.side_tables->local_expr_index(expr);
            if (record_local_dense_slot(
                    this->state_.flow.current_side_tables.side_tables->expr_intrinsic_types, local_index, type)) {
                return type;
            }
            record_sparse_fallback(*this->state_.flow.current_side_tables.side_tables,
                GenericSparseFallbackKind::expr_intrinsic_type, local_index);
            this->state_.flow.current_side_tables.side_tables->sparse_expr_intrinsic_types[expr.value] = type;
        }
        return type;
    }
    SemaTypeTable& expr_intrinsic_types = this->active_expr_intrinsic_types();
    if (syntax::is_valid(expr)) {
        ensure_side_table_slot(expr_intrinsic_types, expr.value);
        expr_intrinsic_types[expr.value] = type;
    }
    return type;
}

TypeHandle SemanticAnalyzerCore::record_expr_types(
    const syntax::ExprId expr, const TypeHandle intrinsic_type, const TypeHandle final_type)
{
    static_cast<void>(this->record_expr_intrinsic_type(expr, intrinsic_type));
    return this->record_expr_type(expr, final_type);
}

void SemanticAnalyzerCore::record_expr_expected_type(const syntax::ExprId expr, const TypeHandle expected_type)
{
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->sparse) {
        if (syntax::is_valid(expr)) {
            const base::usize local_index = this->state_.flow.current_side_tables.side_tables->local_expr_index(expr);
            if (record_local_dense_slot(this->state_.flow.current_side_tables.side_tables->expr_expected_types,
                    local_index, expected_type)) {
                return;
            }
            record_sparse_fallback(*this->state_.flow.current_side_tables.side_tables,
                GenericSparseFallbackKind::expr_expected_type, local_index);
            this->state_.flow.current_side_tables.side_tables->sparse_expr_expected_types[expr.value] = expected_type;
        }
        return;
    }
    SemaTypeTable& expr_expected_types = this->active_expr_expected_types();
    if (syntax::is_valid(expr)) {
        ensure_side_table_slot(expr_expected_types, expr.value);
        expr_expected_types[expr.value] = expected_type;
    }
}

void SemanticAnalyzerCore::record_coercion(
    const syntax::ExprId expr, const TypeHandle from_type, const TypeHandle to_type, const CoercionKind kind)
{
    const bool null_to_pointer = kind == CoercionKind::null_to_pointer && !is_valid(from_type) && is_valid(to_type)
        && this->state_.checked.types.is_pointer(to_type);
    if (!syntax::is_valid(expr) || !is_valid(to_type) || (!is_valid(from_type) && !null_to_pointer)
        || (is_valid(from_type) && this->state_.checked.types.same(from_type, to_type))) {
        return;
    }
    this->state_.checked.coercions.push_back(CoercionRecord{
        expr,
        from_type,
        to_type,
        kind,
    });
}

TypeHandle SemanticAnalyzerCore::cached_expr_intrinsic_type(const syntax::ExprId expr) const noexcept
{
    if (!syntax::is_valid(expr)) {
        return INVALID_TYPE_HANDLE;
    }
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->sparse) {
        const base::usize local_index = this->state_.flow.current_side_tables.side_tables->local_expr_index(expr);
        if (local_index != SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX
            && local_index < this->state_.flow.current_side_tables.side_tables->expr_intrinsic_types.size()) {
            return this->state_.flow.current_side_tables.side_tables->expr_intrinsic_types[local_index];
        }
        const auto found =
            this->state_.flow.current_side_tables.side_tables->sparse_expr_intrinsic_types.find(expr.value);
        return found == this->state_.flow.current_side_tables.side_tables->sparse_expr_intrinsic_types.end()
            ? INVALID_TYPE_HANDLE
            : found->second;
    }
    const SemaTypeTable& expr_intrinsic_types = this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.expr_intrinsic_types
        : this->state_.flow.current_side_tables.side_tables->expr_intrinsic_types;
    return expr.value < expr_intrinsic_types.size() ? expr_intrinsic_types[expr.value] : INVALID_TYPE_HANDLE;
}

TypeHandle SemanticAnalyzerCore::cached_expr_type(const syntax::ExprId expr) const noexcept
{
    if (!syntax::is_valid(expr)) {
        return INVALID_TYPE_HANDLE;
    }
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->sparse) {
        const base::usize local_index = this->state_.flow.current_side_tables.side_tables->local_expr_index(expr);
        if (local_index != SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX
            && local_index < this->state_.flow.current_side_tables.side_tables->expr_types.size()) {
            return this->state_.flow.current_side_tables.side_tables->expr_types[local_index];
        }
        const auto found = this->state_.flow.current_side_tables.side_tables->sparse_expr_types.find(expr.value);
        return found == this->state_.flow.current_side_tables.side_tables->sparse_expr_types.end() ? INVALID_TYPE_HANDLE
                                                                                                   : found->second;
    }
    const SemaTypeTable& expr_types = this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.expr_types
        : this->state_.flow.current_side_tables.side_tables->expr_types;
    return expr.value < expr_types.size() ? expr_types[expr.value] : INVALID_TYPE_HANDLE;
}

TypeHandle SemanticAnalyzerCore::cached_expr_expected_type(const syntax::ExprId expr) const noexcept
{
    if (!syntax::is_valid(expr)) {
        return INVALID_TYPE_HANDLE;
    }
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->sparse) {
        const base::usize local_index = this->state_.flow.current_side_tables.side_tables->local_expr_index(expr);
        if (local_index != SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX
            && local_index < this->state_.flow.current_side_tables.side_tables->expr_expected_types.size()) {
            return this->state_.flow.current_side_tables.side_tables->expr_expected_types[local_index];
        }
        const auto found =
            this->state_.flow.current_side_tables.side_tables->sparse_expr_expected_types.find(expr.value);
        return found == this->state_.flow.current_side_tables.side_tables->sparse_expr_expected_types.end()
            ? INVALID_TYPE_HANDLE
            : found->second;
    }
    const SemaTypeTable& expr_expected_types = this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.expr_expected_types
        : this->state_.flow.current_side_tables.side_tables->expr_expected_types;
    return expr.value < expr_expected_types.size() ? expr_expected_types[expr.value] : INVALID_TYPE_HANDLE;
}

TypeHandle SemanticAnalyzerCore::cached_expr_type_for_expected(
    const syntax::ExprId expr, const TypeHandle expected_type) const noexcept
{
    const TypeHandle cached_type = this->cached_expr_type(expr);
    if (!is_valid(cached_type)) {
        return INVALID_TYPE_HANDLE;
    }
    const TypeHandle cached_expected = this->cached_expr_expected_type(expr);
    return this->state_.checked.types.same(cached_expected, expected_type) ? cached_type : INVALID_TYPE_HANDLE;
}

TypeHandle SemanticAnalyzerCore::cached_syntax_type(const syntax::TypeId type) const noexcept
{
    if (!syntax::is_valid(type) || !this->state_.flow.current_side_tables.cache_syntax_types) {
        return INVALID_TYPE_HANDLE;
    }
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->sparse) {
        const base::usize local_index = this->state_.flow.current_side_tables.side_tables->local_type_index(type);
        if (local_index != SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX
            && local_index < this->state_.flow.current_side_tables.side_tables->syntax_type_handles.size()) {
            return this->state_.flow.current_side_tables.side_tables->syntax_type_handles[local_index];
        }
        const auto found =
            this->state_.flow.current_side_tables.side_tables->sparse_syntax_type_handles.find(type.value);
        return found == this->state_.flow.current_side_tables.side_tables->sparse_syntax_type_handles.end()
            ? INVALID_TYPE_HANDLE
            : found->second;
    }
    const SemaTypeTable& syntax_type_handles = this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.syntax_type_handles
        : this->state_.flow.current_side_tables.side_tables->syntax_type_handles;
    return type.value < syntax_type_handles.size() ? syntax_type_handles[type.value] : INVALID_TYPE_HANDLE;
}

std::string_view SemanticAnalyzerCore::cached_expr_c_name(const syntax::ExprId expr) const noexcept
{
    if (!syntax::is_valid(expr)) {
        return {};
    }
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->sparse) {
        const IdentId local =
            cached_local_dense_slot(this->state_.flow.current_side_tables.side_tables->expr_c_name_ids,
                this->state_.flow.current_side_tables.side_tables->local_expr_index(expr), INVALID_IDENT_ID);
        if (is_valid(local)) {
            return this->state_.checked.c_name_text(local);
        }
        const auto found = this->state_.flow.current_side_tables.side_tables->sparse_expr_c_name_ids.find(expr.value);
        return found == this->state_.flow.current_side_tables.side_tables->sparse_expr_c_name_ids.end()
            ? std::string_view{}
            : this->state_.checked.c_name_text(found->second);
    }
    const SemaIdentTable& expr_c_name_ids = this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.expr_c_name_ids
        : this->state_.flow.current_side_tables.side_tables->expr_c_name_ids;
    return expr.value < expr_c_name_ids.size() ? this->state_.checked.c_name_text(expr_c_name_ids[expr.value])
                                               : std::string_view{};
}

std::string_view SemanticAnalyzerCore::cached_pattern_c_name(const syntax::PatternId pattern) const noexcept
{
    if (!syntax::is_valid(pattern)) {
        return {};
    }
    if (this->state_.flow.current_side_tables.side_tables != nullptr
        && this->state_.flow.current_side_tables.side_tables->sparse) {
        const IdentId local =
            cached_local_dense_slot(this->state_.flow.current_side_tables.side_tables->pattern_c_name_ids,
                this->state_.flow.current_side_tables.side_tables->local_pattern_index(pattern), INVALID_IDENT_ID);
        if (is_valid(local)) {
            return this->state_.checked.c_name_text(local);
        }
        const auto found =
            this->state_.flow.current_side_tables.side_tables->sparse_pattern_c_name_ids.find(pattern.value);
        return found == this->state_.flow.current_side_tables.side_tables->sparse_pattern_c_name_ids.end()
            ? std::string_view{}
            : this->state_.checked.c_name_text(found->second);
    }
    const SemaIdentTable& pattern_c_name_ids = this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.pattern_c_name_ids
        : this->state_.flow.current_side_tables.side_tables->pattern_c_name_ids;
    return pattern.value < pattern_c_name_ids.size()
        ? this->state_.checked.c_name_text(pattern_c_name_ids[pattern.value])
        : std::string_view{};
}

SemaTypeTable& SemanticAnalyzerCore::active_expr_types() noexcept
{
    return this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.expr_types
        : this->state_.flow.current_side_tables.side_tables->expr_types;
}

SemaTypeTable& SemanticAnalyzerCore::active_expr_intrinsic_types() noexcept
{
    return this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.expr_intrinsic_types
        : this->state_.flow.current_side_tables.side_tables->expr_intrinsic_types;
}

SemaTypeTable& SemanticAnalyzerCore::active_expr_expected_types() noexcept
{
    return this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.expr_expected_types
        : this->state_.flow.current_side_tables.side_tables->expr_expected_types;
}

SemaIdentTable& SemanticAnalyzerCore::active_expr_c_name_ids() noexcept
{
    return this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.expr_c_name_ids
        : this->state_.flow.current_side_tables.side_tables->expr_c_name_ids;
}

SemaIdentTable& SemanticAnalyzerCore::active_pattern_c_name_ids() noexcept
{
    return this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.pattern_c_name_ids
        : this->state_.flow.current_side_tables.side_tables->pattern_c_name_ids;
}

PatternCaseNameTable& SemanticAnalyzerCore::active_pattern_case_name_ids() noexcept
{
    return this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.pattern_case_name_ids
        : this->state_.flow.current_side_tables.side_tables->pattern_case_name_ids;
}

SemaTypeTable& SemanticAnalyzerCore::active_syntax_type_handles() noexcept
{
    return this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.syntax_type_handles
        : this->state_.flow.current_side_tables.side_tables->syntax_type_handles;
}

SemaTypeTable& SemanticAnalyzerCore::active_stmt_local_types() noexcept
{
    return this->state_.flow.current_side_tables.side_tables == nullptr
        ? this->state_.checked.stmt_local_types
        : this->state_.flow.current_side_tables.side_tables->stmt_local_types;
}

void SemanticAnalyzerCore::report(
    const base::SourceRange& range, const SemanticDiagnosticKind kind, std::string message) const
{
    const SemanticDiagnosticMetadata metadata = semantic_diagnostic_metadata(kind);
    this->report(range, std::move(message), metadata.category, metadata.code);
}

void SemanticAnalyzerCore::report_general(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::general, std::move(message));
}

void SemanticAnalyzerCore::report_type(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::type_mismatch, std::move(message));
}

void SemanticAnalyzerCore::report_lookup(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::lookup, std::move(message));
}

void SemanticAnalyzerCore::report_duplicate(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::duplicate, std::move(message));
}

void SemanticAnalyzerCore::report_visibility(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::visibility, std::move(message));
}

void SemanticAnalyzerCore::report_unsupported(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::unsupported, std::move(message));
}

void SemanticAnalyzerCore::report_unsafe_required(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::unsafe_required, std::move(message));
}

void SemanticAnalyzerCore::report_capability(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::capability, std::move(message));
}

void SemanticAnalyzerCore::report_pattern(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::pattern, std::move(message));
}

void SemanticAnalyzerCore::report_pattern_exhaustiveness(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::pattern_exhaustiveness, std::move(message));
}

void SemanticAnalyzerCore::report_pattern_unreachable(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::pattern_unreachable, std::move(message));
}

void SemanticAnalyzerCore::report_internal_contract(const base::SourceRange& range, std::string message) const
{
    this->report(range, SemanticDiagnosticKind::internal_contract, std::move(message));
}

void SemanticAnalyzerCore::report(const base::SourceRange& range, std::string message,
    const base::DiagnosticCategory category, const base::DiagnosticCode code) const
{
    this->ctx_.diagnostics.push(base::Diagnostic{
        base::Severity::error,
        range,
        std::move(message),
        category,
        code,
    });
}

void SemanticAnalyzerCore::report_note(
    const base::SourceRange& range, const SemanticDiagnosticKind kind, std::string message) const
{
    const SemanticDiagnosticMetadata metadata = semantic_secondary_diagnostic_metadata(kind);
    this->ctx_.diagnostics.push(base::Diagnostic{
        base::Severity::note,
        range,
        std::move(message),
        metadata.category,
        metadata.code,
    });
}

void SemanticAnalyzerCore::report_help(
    const base::SourceRange& range, const SemanticDiagnosticKind kind, std::string message) const
{
    const SemanticDiagnosticMetadata metadata = semantic_secondary_diagnostic_metadata(kind);
    this->ctx_.diagnostics.push(base::Diagnostic{
        base::Severity::help,
        range,
        std::move(message),
        metadata.category,
        metadata.code,
    });
}

void SemanticAnalyzerCore::report_type_mismatch(
    const base::SourceRange& range, std::string message, const TypeHandle expected, const TypeHandle actual) const
{
    this->report(range, SemanticDiagnosticKind::type_mismatch, std::move(message));
    if (is_valid(expected)) {
        this->report_note(range, SemanticDiagnosticKind::type_mismatch,
            sema_expected_type_note_message(this->state_.checked.types.display_name(expected)));
    }
    if (is_valid(actual)) {
        this->report_note(range, SemanticDiagnosticKind::type_mismatch,
            sema_actual_type_note_message(this->state_.checked.types.display_name(actual)));
    }
}

void SemanticAnalyzerCore::report_lookup_suggestion(
    const base::SourceRange& range, const std::string_view suggestion) const
{
    if (!suggestion.empty()) {
        this->report_help(range, SemanticDiagnosticKind::lookup, sema_did_you_mean_message(suggestion));
    }
}

} // namespace aurex::sema
