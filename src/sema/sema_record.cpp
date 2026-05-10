#include <aurex/sema/sema.hpp>

#include <utility>

namespace aurex::sema {

void SemanticAnalyzer::record_stmt_local_type(const syntax::StmtId stmt, const TypeHandle type) noexcept {
    if (current_generic_stmt_local_types_ == nullptr &&
        syntax::is_valid(stmt) &&
        stmt.value < checked_.stmt_local_types.size()) {
        checked_.stmt_local_types[stmt.value] = type;
    }
    if (current_generic_stmt_local_types_ != nullptr && syntax::is_valid(stmt)) {
        (*current_generic_stmt_local_types_)[stmt.value] = type;
    }
}

void SemanticAnalyzer::record_expr_c_name(const syntax::ExprId expr, const std::string_view c_name) {
    if (!syntax::is_valid(expr) || c_name.empty()) {
        return;
    }
    if (current_generic_expr_c_names_ == nullptr && expr.value < checked_.expr_c_names.size()) {
        checked_.expr_c_names[expr.value] = std::string(c_name);
    }
    if (current_generic_expr_c_names_ != nullptr) {
        (*current_generic_expr_c_names_)[expr.value] = std::string(c_name);
    }
}

void SemanticAnalyzer::record_pattern_c_name(const syntax::PatternId pattern, const std::string_view c_name) {
    if (!syntax::is_valid(pattern) || c_name.empty()) {
        return;
    }
    if (current_generic_pattern_c_names_ == nullptr && pattern.value < checked_.pattern_c_names.size()) {
        checked_.pattern_c_names[pattern.value] = std::string(c_name);
    }
    if (current_generic_pattern_c_names_ != nullptr) {
        (*current_generic_pattern_c_names_)[pattern.value] = std::string(c_name);
    }
}

void SemanticAnalyzer::record_pattern_case_name(const syntax::PatternId pattern, const std::string_view c_name) {
    if (!syntax::is_valid(pattern) || c_name.empty()) {
        return;
    }
    if (current_generic_pattern_case_sets_ == nullptr && pattern.value < checked_.pattern_case_sets.size()) {
        checked_.pattern_case_sets[pattern.value].insert(std::string(c_name));
    }
    if (current_generic_pattern_case_sets_ != nullptr) {
        (*current_generic_pattern_case_sets_)[pattern.value].insert(std::string(c_name));
    }
}

void SemanticAnalyzer::merge_pattern_case_names(const syntax::PatternId pattern, const syntax::PatternId alternative) {
    if (!syntax::is_valid(pattern) || !syntax::is_valid(alternative)) {
        return;
    }
    if (current_generic_pattern_case_sets_ == nullptr &&
        pattern.value < checked_.pattern_case_sets.size() &&
        alternative.value < checked_.pattern_case_sets.size()) {
        checked_.pattern_case_sets[pattern.value].insert(
            checked_.pattern_case_sets[alternative.value].begin(),
            checked_.pattern_case_sets[alternative.value].end()
        );
    }
    if (current_generic_pattern_case_sets_ != nullptr) {
        auto& dst = (*current_generic_pattern_case_sets_)[pattern.value];
        if (const auto found = current_generic_pattern_case_sets_->find(alternative.value);
            found != current_generic_pattern_case_sets_->end()) {
            dst.insert(found->second.begin(), found->second.end());
        } else if (alternative.value < checked_.pattern_case_sets.size()) {
            dst.insert(
                checked_.pattern_case_sets[alternative.value].begin(),
                checked_.pattern_case_sets[alternative.value].end()
            );
        }
    }
}

void SemanticAnalyzer::record_syntax_type_handle(const syntax::TypeId type, const TypeHandle resolved) noexcept {
    if (current_generic_syntax_type_handles_ == nullptr &&
        syntax::is_valid(type) &&
        type.value < checked_.syntax_type_handles.size()) {
        checked_.syntax_type_handles[type.value] = resolved;
    }
    if (current_generic_syntax_type_handles_ != nullptr && syntax::is_valid(type)) {
        (*current_generic_syntax_type_handles_)[type.value] = resolved;
    }
}

TypeHandle SemanticAnalyzer::record_expr_type(const syntax::ExprId expr, const TypeHandle type) noexcept {
    if (current_generic_expr_types_ == nullptr &&
        syntax::is_valid(expr) &&
        expr.value < checked_.expr_types.size()) {
        checked_.expr_types[expr.value] = type;
    }
    if (current_generic_expr_types_ != nullptr && syntax::is_valid(expr)) {
        (*current_generic_expr_types_)[expr.value] = type;
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
