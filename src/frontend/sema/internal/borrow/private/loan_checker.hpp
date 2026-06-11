#pragma once

#include <aurex/frontend/sema/checked_module.hpp>

#include <string>
#include <string_view>
#include <vector>

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::BodyLoanChecker final {
public:
    explicit BodyLoanChecker(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] bool may_need_local_loan_check(const syntax::ItemNode& function) const;
    void record_empty(const FunctionLookupKey& key, BodyLoanDiagnosticMode mode);
    void check(const syntax::ItemNode& function, const FunctionLookupKey& key, BodyLoanDiagnosticMode mode);

private:
    [[nodiscard]] bool statement_may_need_local_loan_check(syntax::StmtId stmt) const;
    [[nodiscard]] bool statement_may_bind_reference_loan_shallow(syntax::StmtId stmt) const;
    [[nodiscard]] bool expr_may_need_local_loan_check(syntax::ExprId expr) const;
    void push_checked_precheck_expression_children(syntax::ExprId expr, std::vector<syntax::ExprId>& pending_exprs,
        std::vector<syntax::StmtId>& pending_stmts) const;
    [[nodiscard]] bool type_contains_reference(TypeHandle type) const;

    SemanticAnalyzerCore& core_;
};

[[nodiscard]] std::string_view body_loan_kind_name(BodyLoanKind kind) noexcept;
[[nodiscard]] std::string_view body_loan_origin_kind_name(BodyLoanOriginKind kind) noexcept;
[[nodiscard]] std::string_view body_loan_diagnostic_mode_name(BodyLoanDiagnosticMode mode) noexcept;
[[nodiscard]] std::string_view body_loan_conflict_kind_name(BodyLoanConflictKind kind) noexcept;
[[nodiscard]] std::string dump_body_loan_check_result(const BodyLoanCheckResult& result);

} // namespace aurex::sema
