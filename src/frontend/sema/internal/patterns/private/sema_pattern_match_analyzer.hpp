#pragma once

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::PatternMatchAnalyzer final {
public:
    explicit PatternMatchAnalyzer(SemanticAnalyzerCore& core) noexcept;

    bool pattern_is_irrefutable(const syntax::PatternId pattern_id, const TypeHandle matched) const;
    bool analyze_pattern(
        const syntax::PatternId pattern_id, const TypeHandle matched, std::vector<PatternBinding>& bindings);
    void define_pattern_bindings(const std::vector<PatternBinding>& bindings, const bool is_mutable);
    TypeHandle analyze_match_expr(
        const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type);
    void analyze_single_value_pattern(const syntax::PatternId pattern_id, const TypeHandle matched, bool& covered_true,
        bool& covered_false, bool& saw_wildcard) const;

private:
    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
