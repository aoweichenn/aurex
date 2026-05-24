#pragma once

#include <aurex/base/result.hpp>
#include <aurex/sema/checked_module.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore;

class SemanticAnalysisPipeline final {
public:
    explicit SemanticAnalysisPipeline(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] base::Result<CheckedModule> run();

private:
    [[nodiscard]] bool prepare_analysis_session();
    void reserve_analysis_storage();
    void run_declaration_phases();
    void run_function_body_phases();
    void run_validation_phases();
    [[nodiscard]] base::Result<CheckedModule> finish_analysis();
    void normalize_parser_only_module_contract();
    [[nodiscard]] bool validate_ast_contract() const;

    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
