#include <aurex/sema/sema.hpp>

#include <utility>

namespace aurex::sema {

namespace {

template <typename T>
void ensure_side_table_slot(
    std::vector<T>& table,
    const base::usize index
) {
    if (index >= table.size()) {
        table.resize(index + 1);
    }
}

template <>
void ensure_side_table_slot<TypeHandle>(
    std::vector<TypeHandle>& table,
    const base::usize index
) {
    if (index >= table.size()) {
        table.resize(index + 1, INVALID_TYPE_HANDLE);
    }
}

} // namespace

void SemanticAnalyzer::record_stmt_local_type(const syntax::StmtId stmt, const TypeHandle type) {
    if (this->current_side_tables_.side_tables != nullptr && this->current_side_tables_.side_tables->sparse) {
        if (syntax::is_valid(stmt)) {
            this->current_side_tables_.side_tables->sparse_stmt_local_types[stmt.value] = type;
        }
        return;
    }
    std::vector<TypeHandle>& stmt_local_types = this->active_stmt_local_types();
    if (syntax::is_valid(stmt)) {
        ensure_side_table_slot(stmt_local_types, stmt.value);
        stmt_local_types[stmt.value] = type;
    }
}

void SemanticAnalyzer::record_expr_c_name(const syntax::ExprId expr, const std::string_view c_name) {
    if (!syntax::is_valid(expr) || c_name.empty()) {
        return;
    }
    if (this->current_side_tables_.side_tables != nullptr && this->current_side_tables_.side_tables->sparse) {
        this->current_side_tables_.side_tables->sparse_expr_c_names[expr.value] = std::string(c_name);
        return;
    }
    std::vector<std::string>& expr_c_names = this->active_expr_c_names();
    ensure_side_table_slot(expr_c_names, expr.value);
    expr_c_names[expr.value] = std::string(c_name);
}

void SemanticAnalyzer::record_pattern_c_name(const syntax::PatternId pattern, const std::string_view c_name) {
    if (!syntax::is_valid(pattern) || c_name.empty()) {
        return;
    }
    if (this->current_side_tables_.side_tables != nullptr && this->current_side_tables_.side_tables->sparse) {
        this->current_side_tables_.side_tables->sparse_pattern_c_names[pattern.value] = std::string(c_name);
        return;
    }
    std::vector<std::string>& pattern_c_names = this->active_pattern_c_names();
    ensure_side_table_slot(pattern_c_names, pattern.value);
    pattern_c_names[pattern.value] = std::string(c_name);
}

void SemanticAnalyzer::record_pattern_case_name(const syntax::PatternId pattern, const std::string_view c_name) {
    if (!syntax::is_valid(pattern) || c_name.empty()) {
        return;
    }
    if (this->current_side_tables_.side_tables != nullptr && this->current_side_tables_.side_tables->sparse) {
        this->current_side_tables_.side_tables->sparse_pattern_case_sets[pattern.value].insert(std::string(c_name));
        return;
    }
    std::vector<std::unordered_set<std::string>>& pattern_case_sets = this->active_pattern_case_sets();
    ensure_side_table_slot(pattern_case_sets, pattern.value);
    pattern_case_sets[pattern.value].insert(std::string(c_name));
}

void SemanticAnalyzer::merge_pattern_case_names(const syntax::PatternId pattern, const syntax::PatternId alternative) {
    if (!syntax::is_valid(pattern) || !syntax::is_valid(alternative)) {
        return;
    }
    if (this->current_side_tables_.side_tables != nullptr && this->current_side_tables_.side_tables->sparse) {
        const auto found = this->current_side_tables_.side_tables->sparse_pattern_case_sets.find(alternative.value);
        if (found != this->current_side_tables_.side_tables->sparse_pattern_case_sets.end()) {
            std::unordered_set<std::string>& target =
                this->current_side_tables_.side_tables->sparse_pattern_case_sets[pattern.value];
            target.insert(found->second.begin(), found->second.end());
        }
        return;
    }
    std::vector<std::unordered_set<std::string>>& pattern_case_sets = this->active_pattern_case_sets();
    if (alternative.value < pattern_case_sets.size()) {
        ensure_side_table_slot(pattern_case_sets, pattern.value);
        pattern_case_sets[pattern.value].insert(
            pattern_case_sets[alternative.value].begin(),
            pattern_case_sets[alternative.value].end()
        );
    }
}

void SemanticAnalyzer::record_syntax_type_handle(const syntax::TypeId type, const TypeHandle resolved) {
    if (!this->current_side_tables_.cache_syntax_types) {
        return;
    }
    if (this->current_side_tables_.side_tables != nullptr && this->current_side_tables_.side_tables->sparse) {
        if (syntax::is_valid(type)) {
            this->current_side_tables_.side_tables->sparse_syntax_type_handles[type.value] = resolved;
        }
        return;
    }
    std::vector<TypeHandle>& syntax_type_handles = this->active_syntax_type_handles();
    if (syntax::is_valid(type)) {
        ensure_side_table_slot(syntax_type_handles, type.value);
        syntax_type_handles[type.value] = resolved;
    }
}

TypeHandle SemanticAnalyzer::record_expr_type(const syntax::ExprId expr, const TypeHandle type) {
    if (this->current_side_tables_.side_tables != nullptr && this->current_side_tables_.side_tables->sparse) {
        if (syntax::is_valid(expr)) {
            this->current_side_tables_.side_tables->sparse_expr_types[expr.value] = type;
        }
        return type;
    }
    std::vector<TypeHandle>& expr_types = this->active_expr_types();
    if (syntax::is_valid(expr)) {
        ensure_side_table_slot(expr_types, expr.value);
        expr_types[expr.value] = type;
    }
    return type;
}

TypeHandle SemanticAnalyzer::cached_expr_type(const syntax::ExprId expr) const noexcept {
    if (!syntax::is_valid(expr)) {
        return INVALID_TYPE_HANDLE;
    }
    if (this->current_side_tables_.side_tables != nullptr && this->current_side_tables_.side_tables->sparse) {
        const auto found = this->current_side_tables_.side_tables->sparse_expr_types.find(expr.value);
        return found == this->current_side_tables_.side_tables->sparse_expr_types.end()
            ? INVALID_TYPE_HANDLE
            : found->second;
    }
    const std::vector<TypeHandle>& expr_types = this->current_side_tables_.side_tables == nullptr
        ? this->checked_.expr_types
        : this->current_side_tables_.side_tables->expr_types;
    return expr.value < expr_types.size() ? expr_types[expr.value] : INVALID_TYPE_HANDLE;
}

TypeHandle SemanticAnalyzer::cached_syntax_type(const syntax::TypeId type) const noexcept {
    if (!syntax::is_valid(type) || !this->current_side_tables_.cache_syntax_types) {
        return INVALID_TYPE_HANDLE;
    }
    if (this->current_side_tables_.side_tables != nullptr && this->current_side_tables_.side_tables->sparse) {
        const auto found = this->current_side_tables_.side_tables->sparse_syntax_type_handles.find(type.value);
        return found == this->current_side_tables_.side_tables->sparse_syntax_type_handles.end()
            ? INVALID_TYPE_HANDLE
            : found->second;
    }
    const std::vector<TypeHandle>& syntax_type_handles = this->current_side_tables_.side_tables == nullptr
        ? this->checked_.syntax_type_handles
        : this->current_side_tables_.side_tables->syntax_type_handles;
    return type.value < syntax_type_handles.size() ? syntax_type_handles[type.value] : INVALID_TYPE_HANDLE;
}

std::string_view SemanticAnalyzer::cached_pattern_c_name(const syntax::PatternId pattern) const noexcept {
    if (!syntax::is_valid(pattern)) {
        return {};
    }
    if (this->current_side_tables_.side_tables != nullptr && this->current_side_tables_.side_tables->sparse) {
        const auto found = this->current_side_tables_.side_tables->sparse_pattern_c_names.find(pattern.value);
        return found == this->current_side_tables_.side_tables->sparse_pattern_c_names.end()
            ? std::string_view {}
            : std::string_view {found->second};
    }
    const std::vector<std::string>& pattern_c_names = this->current_side_tables_.side_tables == nullptr
        ? this->checked_.pattern_c_names
        : this->current_side_tables_.side_tables->pattern_c_names;
    return pattern.value < pattern_c_names.size() ? std::string_view {pattern_c_names[pattern.value]} : std::string_view {};
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
