#include <aurex/infrastructure/base/string_literal.hpp>

#include <frontend/sema/internal/expressions/private/sema_builtin_expression_analyzer.hpp>
#include <frontend/sema/internal/expressions/private/sema_expression_analyzer.hpp>

namespace aurex::sema {

namespace {

enum class ExprAnalysisCategory {
    invalid,
    literal,
    value,
    control,
    aggregate,
    projection,
    operator_,
    builtin,
};

[[nodiscard]] bool expr_kind_has_contextual_final_type(const syntax::ExprKind kind) noexcept
{
    switch (kind) {
        case syntax::ExprKind::integer_literal:
        case syntax::ExprKind::float_literal:
        case syntax::ExprKind::null_literal:
        case syntax::ExprKind::unary:
        case syntax::ExprKind::binary:
        case syntax::ExprKind::if_expr:
        case syntax::ExprKind::block_expr:
        case syntax::ExprKind::unsafe_block:
        case syntax::ExprKind::match_expr:
        case syntax::ExprKind::array_literal:
        case syntax::ExprKind::tuple_literal:
        case syntax::ExprKind::slice:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] ExprAnalysisCategory expr_analysis_category(const syntax::ExprKind kind) noexcept
{
    switch (kind) {
        case syntax::ExprKind::integer_literal:
        case syntax::ExprKind::float_literal:
        case syntax::ExprKind::bool_literal:
        case syntax::ExprKind::null_literal:
        case syntax::ExprKind::string_literal:
        case syntax::ExprKind::c_string_literal:
        case syntax::ExprKind::raw_string_literal:
        case syntax::ExprKind::byte_string_literal:
        case syntax::ExprKind::byte_literal:
        case syntax::ExprKind::char_literal:
            return ExprAnalysisCategory::literal;
        case syntax::ExprKind::name:
        case syntax::ExprKind::generic_apply:
        case syntax::ExprKind::call:
            return ExprAnalysisCategory::value;
        case syntax::ExprKind::try_expr:
        case syntax::ExprKind::if_expr:
        case syntax::ExprKind::block_expr:
        case syntax::ExprKind::unsafe_block:
        case syntax::ExprKind::match_expr:
            return ExprAnalysisCategory::control;
        case syntax::ExprKind::array_literal:
        case syntax::ExprKind::tuple_literal:
        case syntax::ExprKind::struct_literal:
            return ExprAnalysisCategory::aggregate;
        case syntax::ExprKind::field:
        case syntax::ExprKind::index:
        case syntax::ExprKind::slice:
            return ExprAnalysisCategory::projection;
        case syntax::ExprKind::unary:
        case syntax::ExprKind::binary:
            return ExprAnalysisCategory::operator_;
        case syntax::ExprKind::cast:
        case syntax::ExprKind::pcast:
        case syntax::ExprKind::bcast:
        case syntax::ExprKind::size_of:
        case syntax::ExprKind::align_of:
        case syntax::ExprKind::ptr_addr:
        case syntax::ExprKind::paddr:
        case syntax::ExprKind::slice_data:
        case syntax::ExprKind::slice_len:
        case syntax::ExprKind::str_data:
        case syntax::ExprKind::str_byte_len:
        case syntax::ExprKind::str_is_valid_utf8:
        case syntax::ExprKind::str_from_utf8_checked:
        case syntax::ExprKind::str_from_bytes_unchecked:
            return ExprAnalysisCategory::builtin;
        case syntax::ExprKind::invalid:
            return ExprAnalysisCategory::invalid;
    }
    return ExprAnalysisCategory::invalid;
}

} // namespace

SemanticAnalyzerCore::ExpressionAnalyzer::ExpressionAnalyzer(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

TypeHandle SemanticAnalyzerCore::ExpressionAnalyzer::analyze_expr(const syntax::ExprId expr_id)
{
    return this->analyze_expr(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzerCore::ExpressionAnalyzer::analyze_expr(
    const syntax::ExprId expr_id, const TypeHandle expected_type)
{
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->core_.ctx_.module.exprs.size()) {
        return INVALID_TYPE_HANDLE;
    }

    const TypeHandle cached_type = this->core_.cached_expr_type_for_expected(expr_id, expected_type);
    if (is_valid(cached_type)) {
        return cached_type;
    }
    const TypeHandle analyzed = this->analyze_expr(expr_id, this->core_.expr_view(expr_id), expected_type);
    if (is_valid(expected_type) && is_valid(analyzed)
        && !this->core_.state_.checked.types.same(expected_type, analyzed)) {
        const base::SourceRange range = this->core_.ctx_.module.exprs.range(expr_id.value);
        if (this->core_.can_borrowed_dyn_trait_coerce(expected_type, analyzed)) {
            const TypeHandle intrinsic =
                is_valid(this->core_.cached_expr_intrinsic_type(expr_id))
                ? this->core_.cached_expr_intrinsic_type(expr_id)
                : analyzed;
            this->core_.record_borrowed_dyn_trait_coercion_if_needed(expr_id, analyzed, expected_type, range);
            this->core_.record_expr_expected_type(expr_id, expected_type);
            return this->core_.record_expr_types(expr_id, intrinsic, expected_type);
        }
        if (this->core_.can_borrowed_dyn_trait_upcast(expected_type, analyzed)) {
            const TypeHandle intrinsic =
                is_valid(this->core_.cached_expr_intrinsic_type(expr_id))
                ? this->core_.cached_expr_intrinsic_type(expr_id)
                : analyzed;
            this->core_.record_borrowed_dyn_trait_upcast_if_needed(expr_id, analyzed, expected_type, range);
            this->core_.record_expr_expected_type(expr_id, expected_type);
            return this->core_.record_expr_types(expr_id, intrinsic, expected_type);
        }
        static_cast<void>(this->report_failed_borrowed_dyn_trait_coercion(expected_type, analyzed, range));
    }
    if (!is_valid(this->core_.cached_expr_intrinsic_type(expr_id))) {
        const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(expr_id.value);
        if (!is_valid(expected_type) || !expr_kind_has_contextual_final_type(kind)) {
            static_cast<void>(this->core_.record_expr_intrinsic_type(expr_id, analyzed));
        }
    }
    this->core_.record_expr_expected_type(expr_id, expected_type);
    return analyzed;
}

bool SemanticAnalyzerCore::ExpressionAnalyzer::report_failed_borrowed_dyn_trait_coercion(
    const TypeHandle expected_type,
    const TypeHandle analyzed_type,
    const base::SourceRange& range)
{
    const TypeTable& types = this->core_.state_.checked.types;
    if (!types.is_reference(expected_type) || !types.is_reference(analyzed_type)) {
        return false;
    }
    const TypeInfo& expected_ref = types.get(expected_type);
    const TypeInfo& analyzed_ref = types.get(analyzed_type);
    if (expected_ref.pointer_mutability == PointerMutability::mut
        && analyzed_ref.pointer_mutability != PointerMutability::mut) {
        return false;
    }
    if (!is_valid(expected_ref.pointee) || expected_ref.pointee.value >= types.size()
        || !is_valid(analyzed_ref.pointee) || analyzed_ref.pointee.value >= types.size()) {
        return false;
    }
    const TypeInfo& object_info = types.get(expected_ref.pointee);
    const TypeInfo& source_info = types.get(analyzed_ref.pointee);
    if (object_info.kind != TypeKind::trait_object) {
        return false;
    }
    if (source_info.kind == TypeKind::trait_object) {
        return this->core_.find_supertrait_edge_path(source_info, object_info) == nullptr;
    }
    return this->core_.find_trait_object_impl(analyzed_ref.pointee, object_info, range, true) == nullptr;
}

TypeHandle SemanticAnalyzerCore::ExpressionAnalyzer::analyze_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    switch (expr_analysis_category(expr.kind)) {
        case ExprAnalysisCategory::literal:
            return this->analyze_literal_expr(expr_id, expr, expected_type);
        case ExprAnalysisCategory::value:
            return this->analyze_value_expr(expr_id, expr, expected_type);
        case ExprAnalysisCategory::control:
            return this->analyze_control_expr(expr_id, expr, expected_type);
        case ExprAnalysisCategory::aggregate:
            return this->analyze_aggregate_expr(expr_id, expr, expected_type);
        case ExprAnalysisCategory::projection:
            return this->analyze_projection_expr(expr_id, expr, expected_type);
        case ExprAnalysisCategory::operator_:
            return this->analyze_operator_expr(expr_id, expr, expected_type);
        case ExprAnalysisCategory::builtin:
            return this->analyze_builtin_expr(expr_id, expr);
        case ExprAnalysisCategory::invalid:
            return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzerCore::ExpressionAnalyzer::analyze_literal_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    switch (expr.kind) {
        case syntax::ExprKind::integer_literal:
            return this->core_.analyze_integer_literal(expr_id, expr.text, expr.range, expected_type);
        case syntax::ExprKind::float_literal:
            return this->core_.analyze_float_literal(expr_id, expr.text, expr.range, expected_type);
        case syntax::ExprKind::bool_literal:
            return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::bool_));
        case syntax::ExprKind::null_literal:
            if (this->core_.state_.checked.types.is_pointer(expected_type)) {
                this->core_.record_coercion(expr_id, INVALID_TYPE_HANDLE, expected_type, CoercionKind::null_to_pointer);
                return this->core_.record_expr_types(expr_id, INVALID_TYPE_HANDLE, expected_type);
            }
            return this->core_.record_expr_types(expr_id, INVALID_TYPE_HANDLE, INVALID_TYPE_HANDLE);
        case syntax::ExprKind::string_literal:
            return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::str));
        case syntax::ExprKind::raw_string_literal:
            return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::str));
        case syntax::ExprKind::c_string_literal:
            return this->core_.record_expr_type(expr_id,
                this->core_.state_.checked.types.pointer(
                    PointerMutability::const_, this->core_.state_.checked.types.builtin(BuiltinType::u8)));
        case syntax::ExprKind::byte_string_literal: {
            const base::StringLiteralDecode decoded =
                base::decode_string_literal(expr.text, base::StringLiteralKind::byte_string);
            for (const base::StringLiteralError& error : decoded.errors) {
                this->core_.report_type(
                    base::SourceRange{
                        expr.range.source,
                        expr.range.begin + error.begin,
                        expr.range.begin + error.end,
                    },
                    error.message);
            }
            return this->core_.record_expr_type(expr_id,
                this->core_.state_.checked.types.array(static_cast<base::u64>(decoded.decoded.size()),
                    this->core_.state_.checked.types.builtin(BuiltinType::u8)));
        }
        case syntax::ExprKind::byte_literal:
            return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::u8));
        case syntax::ExprKind::char_literal:
            return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::char_));
        default:
            return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
}

TypeHandle SemanticAnalyzerCore::ExpressionAnalyzer::analyze_value_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    switch (expr.kind) {
        case syntax::ExprKind::name:
            return this->core_.analyze_name_expr(expr_id, expr);
        case syntax::ExprKind::generic_apply:
            return this->core_.analyze_generic_apply_expr(expr_id, expr);
        case syntax::ExprKind::call:
            return this->core_.analyze_call_expr(expr_id, expr, expected_type);
        default:
            return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
}

TypeHandle SemanticAnalyzerCore::ExpressionAnalyzer::analyze_control_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    switch (expr.kind) {
        case syntax::ExprKind::try_expr:
            return this->core_.analyze_try_expr(expr_id, expr);
        case syntax::ExprKind::if_expr:
            return this->core_.analyze_if_expr(expr_id, expr, expected_type);
        case syntax::ExprKind::block_expr:
            return this->core_.analyze_block_expr(expr_id, expr, expected_type);
        case syntax::ExprKind::unsafe_block:
            return this->core_.analyze_unsafe_block_expr(expr_id, expr, expected_type);
        case syntax::ExprKind::match_expr:
            return this->core_.analyze_match_expr(expr_id, expr, expected_type);
        default:
            return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
}

TypeHandle SemanticAnalyzerCore::ExpressionAnalyzer::analyze_aggregate_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    switch (expr.kind) {
        case syntax::ExprKind::array_literal:
            return this->core_.analyze_array_literal_expr(expr_id, expr, expected_type);
        case syntax::ExprKind::tuple_literal:
            return this->core_.analyze_tuple_literal_expr(expr_id, expr, expected_type);
        case syntax::ExprKind::struct_literal:
            return this->core_.analyze_struct_literal_expr(expr_id, expr, expected_type);
        default:
            return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
}

TypeHandle SemanticAnalyzerCore::ExpressionAnalyzer::analyze_projection_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    switch (expr.kind) {
        case syntax::ExprKind::field:
            return this->core_.analyze_field_expr(expr_id, expr, expected_type);
        case syntax::ExprKind::index:
            return this->core_.analyze_index_expr(expr_id, expr);
        case syntax::ExprKind::slice:
            return this->core_.analyze_slice_expr(expr_id, expr, expected_type);
        default:
            return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
}

TypeHandle SemanticAnalyzerCore::ExpressionAnalyzer::analyze_operator_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    switch (expr.kind) {
        case syntax::ExprKind::unary:
            return this->core_.analyze_unary_expr(expr_id, expr, expected_type);
        case syntax::ExprKind::binary:
            return this->core_.analyze_binary_expr(expr_id, expr, expected_type);
        default:
            return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
}

TypeHandle SemanticAnalyzerCore::ExpressionAnalyzer::analyze_builtin_expr(
    const syntax::ExprId expr_id, const ExprView& expr)
{
    switch (expr.kind) {
        case syntax::ExprKind::cast:
        case syntax::ExprKind::pcast:
        case syntax::ExprKind::bcast:
            return this->core_.analyze_cast_expr(expr_id, expr);
        case syntax::ExprKind::size_of:
        case syntax::ExprKind::align_of:
            return this->core_.analyze_size_or_align_expr(expr_id, expr);
        case syntax::ExprKind::ptr_addr:
            return this->core_.analyze_ptr_addr_expr(expr_id, expr);
        case syntax::ExprKind::paddr:
            return this->core_.analyze_paddr_expr(expr_id, expr);
        case syntax::ExprKind::slice_data:
        case syntax::ExprKind::slice_len:
            return this->core_.analyze_slice_projection_expr(expr_id, expr);
        case syntax::ExprKind::str_data:
        case syntax::ExprKind::str_byte_len:
            return this->core_.analyze_str_projection_expr(expr_id, expr);
        case syntax::ExprKind::str_is_valid_utf8:
        case syntax::ExprKind::str_from_utf8_checked:
            return this->core_.analyze_str_utf8_slice_expr(expr_id, expr);
        case syntax::ExprKind::str_from_bytes_unchecked:
            return this->core_.analyze_str_from_bytes_unchecked_expr(expr_id, expr);
        default:
            return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
}

SemanticAnalyzerCore::ExpressionAnalyzer SemanticAnalyzerCore::expression_analyzer() noexcept
{
    return ExpressionAnalyzer(*this);
}

TypeHandle SemanticAnalyzerCore::analyze_expr(const syntax::ExprId expr_id)
{
    return this->expression_analyzer().analyze_expr(expr_id);
}

TypeHandle SemanticAnalyzerCore::analyze_expr(const syntax::ExprId expr_id, const TypeHandle expected_type)
{
    return this->expression_analyzer().analyze_expr(expr_id, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->expression_analyzer().analyze_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_literal_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->expression_analyzer().analyze_literal_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_value_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->expression_analyzer().analyze_value_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_control_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->expression_analyzer().analyze_control_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_aggregate_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->expression_analyzer().analyze_aggregate_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_projection_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->expression_analyzer().analyze_projection_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_operator_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->expression_analyzer().analyze_operator_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_builtin_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    return this->expression_analyzer().analyze_builtin_expr(expr_id, expr);
}

} // namespace aurex::sema
