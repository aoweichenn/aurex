#pragma once

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::OperatorExpressionAnalyzer final {
public:
    explicit OperatorExpressionAnalyzer(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] TypeHandle analyze_unary_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_binary_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] BinaryExprAnalysis analyze_binary_operands(const ExprView& expr, TypeHandle expected_type);
    void diagnose_binary_operand_mismatch(const ExprView& expr, const BinaryExprAnalysis& analysis) const;
    void diagnose_binary_literal_hazards(const ExprView& expr, TypeHandle lhs) const;
    void diagnose_binary_rhs_literal_hazards(const ExprView& expr, TypeHandle lhs) const;
    void diagnose_signed_binary_literal_overflow(const ExprView& expr, TypeHandle lhs) const;
    [[nodiscard]] TypeHandle record_binary_operator_expr(
        syntax::ExprId expr_id, const ExprView& expr, const BinaryExprAnalysis& analysis);
    [[nodiscard]] TypeHandle record_ordering_binary_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle lhs);
    [[nodiscard]] TypeHandle record_equality_binary_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle lhs, bool null_pointer_comparison);
    [[nodiscard]] TypeHandle record_logical_binary_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle lhs, TypeHandle rhs);
    [[nodiscard]] TypeHandle record_integer_binary_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle result_intrinsic, TypeHandle lhs);
    [[nodiscard]] TypeHandle record_numeric_binary_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle result_intrinsic, TypeHandle lhs);

private:
    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
