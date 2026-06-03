#include <aurex/frontend/sema/sema_messages.hpp>

#include <frontend/sema/internal/expressions/private/sema_builtin_expression_analyzer.hpp>

namespace aurex::sema {

namespace {

[[nodiscard]] base::SourceRange expr_range_or(
    const syntax::AstModule& module, const syntax::ExprId expr, const base::SourceRange& fallback) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size() ? module.exprs.range(expr.value) : fallback;
}

[[nodiscard]] bool is_const_u8_pointer(const TypeTable& types, const TypeHandle type) noexcept
{
    if (!types.is_pointer(type)) {
        return false;
    }
    const TypeInfo& pointer = types.get(type);
    return pointer.pointer_mutability == PointerMutability::const_
        && types.same(pointer.pointee, types.builtin(BuiltinType::u8));
}

[[nodiscard]] bool is_u8_slice(const TypeTable& types, const TypeHandle type) noexcept
{
    return types.is_slice(type) && types.same(types.get(type).slice_element, types.builtin(BuiltinType::u8));
}

[[nodiscard]] bool is_pointer_or_reference(const TypeTable& types, const TypeHandle type) noexcept
{
    return types.is_pointer(type) || types.is_reference(type);
}

} // namespace

SemanticAnalyzerCore::BuiltinExpressionAnalyzer::BuiltinExpressionAnalyzer(SemanticAnalyzerCore& core) noexcept
    : core_(core)
{
}

TypeHandle SemanticAnalyzerCore::BuiltinExpressionAnalyzer::analyze_cast_expr(
    const syntax::ExprId expr_id, const ExprView& expr)
{
    if (expr.kind == syntax::ExprKind::pcast) {
        this->core_.require_unsafe_context(expr.range, SEMA_UNSAFE_PTRCAST);
    } else if (expr.kind == syntax::ExprKind::bcast) {
        this->core_.require_unsafe_context(expr.range, SEMA_UNSAFE_BITCAST);
    }
    const TypeHandle target = this->core_.resolve_type(expr.cast_type);
    const TypeHandle source = expr.kind == syntax::ExprKind::pcast ? this->core_.analyze_expr(expr.cast_expr, target)
                                                                   : this->core_.analyze_expr(expr.cast_expr);
    if (!this->core_.is_valid_cast(expr.kind, target, source)) {
        this->core_.report_general(expr.range, std::string(SEMA_INVALID_CONVERSION));
    }
    return this->core_.record_expr_type(expr_id, target);
}

TypeHandle SemanticAnalyzerCore::BuiltinExpressionAnalyzer::analyze_size_or_align_expr(
    const syntax::ExprId expr_id, const ExprView& expr)
{
    const TypeHandle queried = this->core_.resolve_type(expr.cast_type);
    if (is_valid(queried) && this->core_.state_.checked.types.get(queried).kind == TypeKind::generic_param) {
        if (!this->core_.generic_param_has_capability(queried, CapabilityKind::sized)) {
            this->core_.report_capability(expr.range, std::string(SEMA_GENERIC_SIZEOF_ALIGNOF));
        }
    } else if (is_valid(queried) && this->core_.state_.checked.types.get(queried).kind == TypeKind::opaque_struct) {
        this->core_.report_general(expr.range, std::string(SEMA_OPAQUE_SIZEOF_ALIGNOF));
    } else if (is_valid(queried) && !this->core_.is_valid_storage_type(queried)) {
        this->core_.report_general(expr.range, std::string(SEMA_SIZEOF_ALIGNOF_STORAGE));
    }
    return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::usize));
}

TypeHandle SemanticAnalyzerCore::BuiltinExpressionAnalyzer::analyze_ptr_addr_expr(
    const syntax::ExprId expr_id, const ExprView& expr)
{
    const TypeHandle value = this->core_.analyze_expr(expr.cast_expr);
    if (!is_pointer_or_reference(this->core_.state_.checked.types, value)) {
        this->core_.report_general(expr.range, std::string(SEMA_PTRADDR_POINTER));
    }
    return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::usize));
}

TypeHandle SemanticAnalyzerCore::BuiltinExpressionAnalyzer::analyze_paddr_expr(
    const syntax::ExprId expr_id, const ExprView& expr)
{
    this->core_.require_unsafe_context(expr.range, SEMA_UNSAFE_PTRAT);
    const TypeHandle target = this->core_.resolve_type(expr.cast_type);
    const TypeHandle address =
        this->core_.analyze_expr(expr.cast_expr, this->core_.state_.checked.types.builtin(BuiltinType::usize));
    if (!this->core_.state_.checked.types.is_pointer(target)) {
        this->core_.report_general(expr.range, std::string(SEMA_PTRAT_POINTER));
    }
    if (!this->core_.state_.checked.types.is_integer(address)) {
        this->core_.report_general(
            expr_range_or(this->core_.ctx_.module, expr.cast_expr, expr.range), std::string(SEMA_PTRAT_INTEGER));
    }
    return this->core_.record_expr_type(expr_id, target);
}

TypeHandle SemanticAnalyzerCore::BuiltinExpressionAnalyzer::analyze_str_projection_expr(
    const syntax::ExprId expr_id, const ExprView& expr)
{
    const TypeHandle value = this->core_.analyze_expr(expr.cast_expr);
    if (!this->core_.state_.checked.types.is_str(value)) {
        this->core_.report_general(expr.range,
            expr.kind == syntax::ExprKind::str_data ? std::string(SEMA_STRPTR_STR) : std::string(SEMA_STRBLEN_STR));
    }
    if (expr.kind == syntax::ExprKind::str_data) {
        return this->core_.record_expr_type(expr_id,
            this->core_.state_.checked.types.pointer(
                PointerMutability::const_, this->core_.state_.checked.types.builtin(BuiltinType::u8)));
    }
    return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::usize));
}

TypeHandle SemanticAnalyzerCore::BuiltinExpressionAnalyzer::analyze_slice_projection_expr(
    const syntax::ExprId expr_id, const ExprView& expr)
{
    const TypeHandle value = this->core_.analyze_expr(expr.cast_expr);
    if (!this->core_.state_.checked.types.is_slice(value)) {
        this->core_.report_general(expr.range,
            expr.kind == syntax::ExprKind::slice_data ? std::string(SEMA_SLICEPTR_SLICE)
                                                      : std::string(SEMA_SLICELEN_SLICE));
        if (expr.kind == syntax::ExprKind::slice_data) {
            return this->core_.record_expr_type(expr_id,
                this->core_.state_.checked.types.pointer(
                    PointerMutability::const_, this->core_.state_.checked.types.builtin(BuiltinType::u8)));
        }
        return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::usize));
    }
    if (expr.kind == syntax::ExprKind::slice_data) {
        const TypeInfo& slice = this->core_.state_.checked.types.get(value);
        return this->core_.record_expr_type(
            expr_id, this->core_.state_.checked.types.pointer(slice.slice_mutability, slice.slice_element));
    }
    return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::usize));
}

TypeHandle SemanticAnalyzerCore::BuiltinExpressionAnalyzer::analyze_str_utf8_slice_expr(
    const syntax::ExprId expr_id, const ExprView& expr)
{
    const TypeHandle bytes = this->core_.analyze_expr(expr.cast_expr);
    if (!is_u8_slice(this->core_.state_.checked.types, bytes)) {
        this->core_.report_general(expr.range, std::string(SEMA_STR_UTF8_SLICE));
    }
    if (expr.kind == syntax::ExprKind::str_is_valid_utf8) {
        return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::bool_));
    }
    return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::str));
}

TypeHandle SemanticAnalyzerCore::BuiltinExpressionAnalyzer::analyze_str_from_bytes_unchecked_expr(
    const syntax::ExprId expr_id, const ExprView& expr)
{
    this->core_.require_unsafe_context(expr.range, SEMA_UNSAFE_STRRAW);
    if (expr.args.size() != 2) {
        this->core_.report_general(expr.range, std::string(SEMA_STRRAW_ARITY));
        return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::str));
    }
    const TypeHandle data = this->core_.analyze_expr(expr.args[0]);
    const TypeHandle len =
        this->core_.analyze_expr(expr.args[1], this->core_.state_.checked.types.builtin(BuiltinType::usize));
    if (!is_const_u8_pointer(this->core_.state_.checked.types, data)) {
        this->core_.report_general(
            expr_range_or(this->core_.ctx_.module, expr.args[0], expr.range), std::string(SEMA_STRRAW_DATA_POINTER));
    }
    if (!this->core_.state_.checked.types.is_integer(len)) {
        this->core_.report_general(
            expr_range_or(this->core_.ctx_.module, expr.args[1], expr.range), std::string(SEMA_STRRAW_LENGTH_INTEGER));
    }
    return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::str));
}

SemanticAnalyzerCore::BuiltinExpressionAnalyzer SemanticAnalyzerCore::builtin_expression_analyzer() noexcept
{
    return BuiltinExpressionAnalyzer(*this);
}

TypeHandle SemanticAnalyzerCore::analyze_cast_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    return this->builtin_expression_analyzer().analyze_cast_expr(expr_id, expr);
}

TypeHandle SemanticAnalyzerCore::analyze_size_or_align_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    return this->builtin_expression_analyzer().analyze_size_or_align_expr(expr_id, expr);
}

TypeHandle SemanticAnalyzerCore::analyze_ptr_addr_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    return this->builtin_expression_analyzer().analyze_ptr_addr_expr(expr_id, expr);
}

TypeHandle SemanticAnalyzerCore::analyze_paddr_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    return this->builtin_expression_analyzer().analyze_paddr_expr(expr_id, expr);
}

TypeHandle SemanticAnalyzerCore::analyze_slice_projection_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    return this->builtin_expression_analyzer().analyze_slice_projection_expr(expr_id, expr);
}

TypeHandle SemanticAnalyzerCore::analyze_str_projection_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    return this->builtin_expression_analyzer().analyze_str_projection_expr(expr_id, expr);
}

TypeHandle SemanticAnalyzerCore::analyze_str_utf8_slice_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    return this->builtin_expression_analyzer().analyze_str_utf8_slice_expr(expr_id, expr);
}

TypeHandle SemanticAnalyzerCore::analyze_str_from_bytes_unchecked_expr(
    const syntax::ExprId expr_id, const ExprView& expr)
{
    return this->builtin_expression_analyzer().analyze_str_from_bytes_unchecked_expr(expr_id, expr);
}

} // namespace aurex::sema
