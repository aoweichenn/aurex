#include <aurex/sema/sema.hpp>

#include <utility>

namespace aurex::sema {

void SemanticAnalyzer::record_stmt_local_type(const syntax::StmtId stmt, const TypeHandle type) noexcept {
    std::vector<TypeHandle>& stmt_local_types = this->active_stmt_local_types();
    if (syntax::is_valid(stmt) && stmt.value < stmt_local_types.size()) {
        stmt_local_types[stmt.value] = type;
    }
}

void SemanticAnalyzer::record_expr_c_name(const syntax::ExprId expr, const std::string_view c_name) {
    if (!syntax::is_valid(expr) || c_name.empty()) {
        return;
    }
    std::vector<std::string>& expr_c_names = this->active_expr_c_names();
    if (expr.value < expr_c_names.size()) {
        expr_c_names[expr.value] = std::string(c_name);
    }
}

void SemanticAnalyzer::record_pattern_c_name(const syntax::PatternId pattern, const std::string_view c_name) {
    if (!syntax::is_valid(pattern) || c_name.empty()) {
        return;
    }
    std::vector<std::string>& pattern_c_names = this->active_pattern_c_names();
    if (pattern.value < pattern_c_names.size()) {
        pattern_c_names[pattern.value] = std::string(c_name);
    }
}

void SemanticAnalyzer::record_pattern_case_name(const syntax::PatternId pattern, const std::string_view c_name) {
    if (!syntax::is_valid(pattern) || c_name.empty()) {
        return;
    }
    std::vector<std::unordered_set<std::string>>& pattern_case_sets = this->active_pattern_case_sets();
    if (pattern.value < pattern_case_sets.size()) {
        pattern_case_sets[pattern.value].insert(std::string(c_name));
    }
}

void SemanticAnalyzer::merge_pattern_case_names(const syntax::PatternId pattern, const syntax::PatternId alternative) {
    if (!syntax::is_valid(pattern) || !syntax::is_valid(alternative)) {
        return;
    }
    std::vector<std::unordered_set<std::string>>& pattern_case_sets = this->active_pattern_case_sets();
    if (pattern.value < pattern_case_sets.size() &&
        alternative.value < pattern_case_sets.size()) {
        pattern_case_sets[pattern.value].insert(
            pattern_case_sets[alternative.value].begin(),
            pattern_case_sets[alternative.value].end()
        );
    }
}

void SemanticAnalyzer::record_syntax_type_handle(const syntax::TypeId type, const TypeHandle resolved) noexcept {
    if (!this->current_side_tables_.cache_syntax_types) {
        return;
    }
    std::vector<TypeHandle>& syntax_type_handles = this->active_syntax_type_handles();
    if (syntax::is_valid(type) && type.value < syntax_type_handles.size()) {
        syntax_type_handles[type.value] = resolved;
    }
}

TypeHandle SemanticAnalyzer::record_expr_type(const syntax::ExprId expr, const TypeHandle type) noexcept {
    std::vector<TypeHandle>& expr_types = this->active_expr_types();
    if (syntax::is_valid(expr) && expr.value < expr_types.size()) {
        expr_types[expr.value] = type;
    }
    return type;
}

std::vector<TypeHandle>& SemanticAnalyzer::active_expr_types() noexcept {
    return this->current_side_tables_.side_tables == nullptr
        ? this->checked_.expr_types
        : this->current_side_tables_.side_tables->expr_types;
}

std::vector<std::string>& SemanticAnalyzer::active_expr_c_names() noexcept {
    return this->current_side_tables_.side_tables == nullptr
        ? this->checked_.expr_c_names
        : this->current_side_tables_.side_tables->expr_c_names;
}

std::vector<std::string>& SemanticAnalyzer::active_pattern_c_names() noexcept {
    return this->current_side_tables_.side_tables == nullptr
        ? this->checked_.pattern_c_names
        : this->current_side_tables_.side_tables->pattern_c_names;
}

std::vector<std::unordered_set<std::string>>& SemanticAnalyzer::active_pattern_case_sets() noexcept {
    return this->current_side_tables_.side_tables == nullptr
        ? this->checked_.pattern_case_sets
        : this->current_side_tables_.side_tables->pattern_case_sets;
}

std::vector<TypeHandle>& SemanticAnalyzer::active_syntax_type_handles() noexcept {
    return this->current_side_tables_.side_tables == nullptr
        ? this->checked_.syntax_type_handles
        : this->current_side_tables_.side_tables->syntax_type_handles;
}

std::vector<TypeHandle>& SemanticAnalyzer::active_stmt_local_types() noexcept {
    return this->current_side_tables_.side_tables == nullptr
        ? this->checked_.stmt_local_types
        : this->current_side_tables_.side_tables->stmt_local_types;
}

void SemanticAnalyzer::report(base::SourceRange range, std::string message) {
    diagnostics_.push(base::Diagnostic {
        base::Severity::error,
        range,
        std::move(message),
    });
}

} // namespace aurex::sema
