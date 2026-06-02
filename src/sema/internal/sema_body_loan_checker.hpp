#pragma once

#include <aurex/sema/checked_module.hpp>

#include <string>
#include <string_view>

#include <sema/internal/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::BodyLoanChecker final {
public:
    explicit BodyLoanChecker(SemanticAnalyzerCore& core) noexcept;

    void check(const syntax::ItemNode& function, const FunctionLookupKey& key, BodyLoanDiagnosticMode mode);

private:
    SemanticAnalyzerCore& core_;
};

[[nodiscard]] std::string_view body_loan_kind_name(BodyLoanKind kind) noexcept;
[[nodiscard]] std::string_view body_loan_origin_kind_name(BodyLoanOriginKind kind) noexcept;
[[nodiscard]] std::string_view body_loan_diagnostic_mode_name(BodyLoanDiagnosticMode mode) noexcept;
[[nodiscard]] std::string_view body_loan_conflict_kind_name(BodyLoanConflictKind kind) noexcept;
[[nodiscard]] std::string dump_body_loan_check_result(const BodyLoanCheckResult& result);

} // namespace aurex::sema
