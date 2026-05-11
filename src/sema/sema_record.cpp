#include <aurex/sema/sema.hpp>

#include <utility>

namespace aurex::sema {

void SemanticAnalyzer::record_stmt_local_type(const syntax::StmtId stmt, const TypeHandle type) noexcept {
    if (syntax::is_valid(stmt) && stmt.value < checked_.stmt_local_types.size()) {
        checked_.stmt_local_types[stmt.value] = type;
    }
}

void SemanticAnalyzer::record_expr_c_name(const syntax::ExprId expr, const std::string_view c_name) {
    if (!syntax::is_valid(expr) || c_name.empty()) {
        return;
    }
    if (expr.value < checked_.expr_c_names.size()) {
        checked_.expr_c_names[expr.value] = std::string(c_name);
    }
}

void SemanticAnalyzer::record_pattern_c_name(const syntax::PatternId pattern, const std::string_view c_name) {
    if (!syntax::is_valid(pattern) || c_name.empty()) {
        return;
    }
    if (pattern.value < checked_.pattern_c_names.size()) {
        checked_.pattern_c_names[pattern.value] = std::string(c_name);
    }
}

void SemanticAnalyzer::record_pattern_case_name(const syntax::PatternId pattern, const std::string_view c_name) {
    if (!syntax::is_valid(pattern) || c_name.empty()) {
        return;
    }
    if (pattern.value < checked_.pattern_case_sets.size()) {
        checked_.pattern_case_sets[pattern.value].insert(std::string(c_name));
    }
}

void SemanticAnalyzer::merge_pattern_case_names(const syntax::PatternId pattern, const syntax::PatternId alternative) {
    if (!syntax::is_valid(pattern) || !syntax::is_valid(alternative)) {
        return;
    }
    if (pattern.value < checked_.pattern_case_sets.size() &&
        alternative.value < checked_.pattern_case_sets.size()) {
        checked_.pattern_case_sets[pattern.value].insert(
            checked_.pattern_case_sets[alternative.value].begin(),
            checked_.pattern_case_sets[alternative.value].end()
        );
    }
}

void SemanticAnalyzer::record_syntax_type_handle(const syntax::TypeId type, const TypeHandle resolved) noexcept {
    if (syntax::is_valid(type) && type.value < checked_.syntax_type_handles.size()) {
        checked_.syntax_type_handles[type.value] = resolved;
    }
}

TypeHandle SemanticAnalyzer::record_expr_type(const syntax::ExprId expr, const TypeHandle type) noexcept {
    if (syntax::is_valid(expr) && expr.value < checked_.expr_types.size()) {
        checked_.expr_types[expr.value] = type;
    }
    return type;
}

void SemanticAnalyzer::report(base::SourceRange range, std::string message) {
    diagnostics_.push(base::Diagnostic {
        base::Severity::error,
        range,
        std::move(message),
    });
}

} // namespace aurex::sema
