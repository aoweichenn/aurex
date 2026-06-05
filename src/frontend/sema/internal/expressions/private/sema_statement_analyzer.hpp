#pragma once

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::StatementAnalyzer final {
public:
    explicit StatementAnalyzer(SemanticAnalyzerCore& core) noexcept;

    void analyze_function_body(const syntax::ItemNode& function, const syntax::ItemId function_id);
    void analyze_function_body_with_signature(const syntax::ItemNode& function, const FunctionLookupKey& key,
        const FunctionSignature& signature, FunctionBodyState& state);
    void analyze_block(
        const syntax::StmtId block, const TypeHandle expected_return, ReturnTypeInference* const return_inference);
    void analyze_block_statements(
        const syntax::StmtId block, const TypeHandle expected_return, ReturnTypeInference* const return_inference);
    TypeHandle analyze_assignment_target(const syntax::ExprId expr_id);
    void analyze_stmt(
        const syntax::StmtId stmt_id, const TypeHandle expected_return, ReturnTypeInference* const return_inference);
    void analyze_statement_tree(const syntax::StmtId root, const TypeHandle expected_return,
        ReturnTypeInference* const return_inference, const StatementAnalysisRootKind root_kind);
    void analyze_statement_action(const StatementAnalysisAction& action, std::vector<StatementAnalysisAction>& stack,
        const TypeHandle expected_return, ReturnTypeInference* const return_inference);
    void analyze_statement_block(const syntax::StmtId block, std::vector<StatementAnalysisAction>& stack) const;
    void analyze_pattern_scoped_block(const syntax::PatternId pattern, const TypeHandle pattern_type,
        const syntax::StmtId block, std::vector<StatementAnalysisAction>& stack);
    void analyze_for_condition(const syntax::StmtId stmt_id);
    TypeHandle analyze_for_range_bounds(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt);
    void define_for_range_local(const syntax::StmtNode& stmt, const TypeHandle type);
    void define_local_pattern(
        const syntax::PatternId pattern_id, const TypeHandle type, const bool is_mutable, const bool allow_refutable);
    void analyze_statement_node(const syntax::StmtId stmt_id, std::vector<StatementAnalysisAction>& stack,
        const TypeHandle expected_return, ReturnTypeInference* const return_inference);
    bool block_guarantees_return(const syntax::StmtId block_id) const;
    bool stmt_guarantees_return(const syntax::StmtId stmt_id) const;
    bool block_may_fallthrough(const syntax::StmtId block_id) const;
    bool stmt_may_fallthrough(const syntax::StmtId stmt_id) const;
    void record_inferred_return(const syntax::StmtId stmt_id, const TypeHandle actual, ReturnTypeInference& inference);
    void finalize_inferred_return(
        const syntax::ItemNode& function, const FunctionLookupKey& key, ReturnTypeInference& inference);
    void resolve_pending_null_returns(ReturnTypeInference& inference);
    void report_return_inference_diagnostic(const syntax::StmtId stmt_id, const std::string_view message) const;
    void validate_function_return_type(const syntax::ItemNode& function, const TypeHandle return_type) const;
    void ensure_function_return_known(const FunctionSignature& signature, const base::SourceRange& use_range);

private:
    [[nodiscard]] bool resource_assignment_requires_unsupported_diagnostic(
        syntax::ExprId lhs, TypeHandle lhs_type) const;
    [[nodiscard]] bool is_owned_local_field_assignment(syntax::ExprId lhs) const;

    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
