#pragma once

#include <sema/internal/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::ExpressionAnalyzer final {
public:
    explicit ExpressionAnalyzer(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr);
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_literal_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_value_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_control_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_operator_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_projection_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_aggregate_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_builtin_expr(syntax::ExprId expr_id, const ExprView& expr);

private:
    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
