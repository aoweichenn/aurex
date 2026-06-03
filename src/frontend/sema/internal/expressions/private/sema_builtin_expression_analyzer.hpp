#pragma once

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::BuiltinExpressionAnalyzer final {
public:
    explicit BuiltinExpressionAnalyzer(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] TypeHandle analyze_cast_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_size_or_align_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_ptr_addr_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_paddr_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_slice_projection_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_str_projection_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_str_utf8_slice_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_str_from_bytes_unchecked_expr(syntax::ExprId expr_id, const ExprView& expr);

private:
    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
