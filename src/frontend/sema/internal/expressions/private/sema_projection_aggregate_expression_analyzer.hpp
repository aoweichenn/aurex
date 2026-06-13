#pragma once

#include <frontend/sema/internal/core/private/sema_core.hpp>

#include <optional>
#include <string_view>

namespace aurex::sema {

class SemanticAnalyzerCore::ProjectionAggregateExpressionAnalyzer final {
public:
    explicit ProjectionAggregateExpressionAnalyzer(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] TypeHandle analyze_field_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_module_member_expr(
        syntax::ExprId expr_id, syntax::ModuleId module, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_index_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_slice_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_array_literal_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_tuple_literal_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_struct_literal_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);

private:
    [[nodiscard]] std::optional<syntax::ExprKind> builtin_projection_kind_for_field(
        TypeHandle object_type, std::string_view field_name) const noexcept;
    [[nodiscard]] TypeHandle analyze_builtin_projection_field_expr(
        syntax::ExprId expr_id, const ExprView& expr, syntax::ExprKind kind);

    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
