#pragma once

#include <aurex/sema/checked_module.hpp>

#include <string>
#include <string_view>

#include <sema/internal/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::BodyLoanChecker final {
public:
    explicit BodyLoanChecker(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] bool may_need_local_loan_check(const syntax::ItemNode& function) const;
    void record_empty(const FunctionLookupKey& key, BodyLoanDiagnosticMode mode);
    void check(const syntax::ItemNode& function, const FunctionLookupKey& key, BodyLoanDiagnosticMode mode);

private:
    [[nodiscard]] bool statement_may_bind_reference_loan(syntax::StmtId stmt) const;
    [[nodiscard]] bool statement_may_bind_reference_loan_shallow(syntax::StmtId stmt) const;
    [[nodiscard]] bool statement_may_have_two_phase_receiver(syntax::StmtId stmt) const;
    [[nodiscard]] bool expr_may_have_two_phase_receiver(syntax::ExprId expr) const;
    [[nodiscard]] bool expr_may_contain_reference_loan_statement(syntax::ExprId expr) const;
    [[nodiscard]] bool type_contains_reference(TypeHandle type) const;

    SemanticAnalyzerCore& core_;
};

[[nodiscard]] std::string_view body_loan_kind_name(BodyLoanKind kind) noexcept;
[[nodiscard]] std::string_view body_loan_origin_kind_name(BodyLoanOriginKind kind) noexcept;
[[nodiscard]] std::string_view body_loan_diagnostic_mode_name(BodyLoanDiagnosticMode mode) noexcept;
[[nodiscard]] std::string_view body_loan_conflict_kind_name(BodyLoanConflictKind kind) noexcept;
[[nodiscard]] std::string dump_body_loan_check_result(const BodyLoanCheckResult& result);

} // namespace aurex::sema
