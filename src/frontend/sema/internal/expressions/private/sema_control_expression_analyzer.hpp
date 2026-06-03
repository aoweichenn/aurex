#pragma once

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::ControlExpressionAnalyzer final {
public:
    explicit ControlExpressionAnalyzer(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] TypeHandle analyze_try_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_if_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_block_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_unsafe_block_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);

private:
    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
