#pragma once

#include <aurex/frontend/sema/checked_module.hpp>

#include <string_view>

namespace aurex::sema {

class SemanticAnalyzerCore;

class SemanticSideTableReader final {
public:
    explicit SemanticSideTableReader(const SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] TypeHandle cached_expr_intrinsic_type(syntax::ExprId expr) const noexcept;
    [[nodiscard]] TypeHandle cached_expr_type(syntax::ExprId expr) const noexcept;
    [[nodiscard]] TypeHandle cached_expr_expected_type(syntax::ExprId expr) const noexcept;
    [[nodiscard]] OwnedUseMode cached_expr_owned_use_mode(syntax::ExprId expr) const noexcept;
    [[nodiscard]] TypeHandle cached_expr_type_for_expected(
        syntax::ExprId expr, TypeHandle expected_type) const noexcept;
    [[nodiscard]] TypeHandle cached_syntax_type(syntax::TypeId type) const noexcept;
    [[nodiscard]] TypeHandle cached_stmt_local_type(syntax::StmtId stmt) const noexcept;
    [[nodiscard]] const RangeValuePlan* cached_range_value_plan(syntax::ExprId expr) const noexcept;
    [[nodiscard]] const ForInIterationPlan* cached_for_in_iteration_plan(syntax::StmtId stmt) const noexcept;
    [[nodiscard]] std::string_view cached_expr_c_name(syntax::ExprId expr) const noexcept;
    [[nodiscard]] std::string_view cached_pattern_c_name(syntax::PatternId pattern) const noexcept;

private:
    [[nodiscard]] const GenericSideTables* current_side_tables() const noexcept;

    const SemanticAnalyzerCore& core_;
};

class SemanticSideTableStore final {
public:
    explicit SemanticSideTableStore(SemanticAnalyzerCore& core) noexcept;

    void record_stmt_local_type(syntax::StmtId stmt, TypeHandle type);
    void record_range_value_plan(syntax::ExprId expr, const RangeValuePlan& plan);
    void record_for_in_iteration_plan(syntax::StmtId stmt, const ForInIterationPlan& plan);
    void record_expr_c_name(syntax::ExprId expr, std::string_view c_name);
    void record_pattern_c_name(syntax::PatternId pattern, std::string_view c_name);
    void record_pattern_case_name(syntax::PatternId pattern, std::string_view c_name);
    void merge_pattern_case_names(syntax::PatternId pattern, syntax::PatternId alternative);
    void record_syntax_type_handle(syntax::TypeId type, TypeHandle resolved);
    [[nodiscard]] TypeHandle record_expr_intrinsic_type(syntax::ExprId expr, TypeHandle type);
    [[nodiscard]] TypeHandle record_expr_types(syntax::ExprId expr, TypeHandle intrinsic_type, TypeHandle final_type);
    [[nodiscard]] TypeHandle record_expr_type(syntax::ExprId expr, TypeHandle type);
    void record_expr_expected_type(syntax::ExprId expr, TypeHandle expected_type);
    void record_expr_owned_use_mode(syntax::ExprId expr, OwnedUseMode mode);
    void record_coercion(syntax::ExprId expr, TypeHandle from_type, TypeHandle to_type, CoercionKind kind);

    [[nodiscard]] SemaTypeTable& active_expr_intrinsic_types() noexcept;
    [[nodiscard]] SemaTypeTable& active_expr_types() noexcept;
    [[nodiscard]] SemaTypeTable& active_expr_expected_types() noexcept;
    [[nodiscard]] SemaOwnedUseModeTable& active_expr_owned_use_modes() noexcept;
    [[nodiscard]] SemaIdentTable& active_expr_c_name_ids() noexcept;
    [[nodiscard]] SemaIdentTable& active_pattern_c_name_ids() noexcept;
    [[nodiscard]] PatternCaseNameTable& active_pattern_case_name_ids() noexcept;
    [[nodiscard]] SemaTypeTable& active_syntax_type_handles() noexcept;
    [[nodiscard]] SemaTypeTable& active_stmt_local_types() noexcept;

private:
    [[nodiscard]] GenericSideTables* current_side_tables() noexcept;

    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
