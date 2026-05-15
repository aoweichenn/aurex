#include <aurex/sema/sema.hpp>

#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <utility>

namespace aurex::sema {

namespace {

[[nodiscard]] bool is_function_item(const syntax::ItemNode& item) noexcept {
    return item.kind == syntax::ItemKind::fn_decl;
}

[[nodiscard]] base::usize enum_case_count(const syntax::AstModule& module) noexcept {
    base::usize count = 0;
    for (const syntax::ItemNode& item : module.items) {
        if (item.kind == syntax::ItemKind::enum_decl) {
            count += item.enum_cases.size();
        }
    }
    return count;
}

} // namespace

SemanticAnalyzer::SemanticAnalyzer(const syntax::AstModule& module, base::DiagnosticSink& diagnostics) noexcept
    : module_(module), diagnostics_(diagnostics) {
    this->module_.exprs.reserve(this->module_.exprs.size() + base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
    this->module_.types.reserve(this->module_.types.size() + base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
}

SemanticAnalyzer::SemanticAnalyzer(syntax::AstModule&& module, base::DiagnosticSink& diagnostics) noexcept
    : module_(std::move(module)), diagnostics_(diagnostics) {
    this->module_.exprs.reserve(this->module_.exprs.size() + base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
    this->module_.types.reserve(this->module_.types.size() + base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
}

base::Result<CheckedModule> SemanticAnalyzer::analyze() {
    this->normalize_parser_only_module_contract();
    if (!this->validate_ast_contract()) {
        return base::Result<CheckedModule>::fail({base::ErrorCode::sema_error, std::string(SEMA_ANALYSIS_FAILED)});
    }

    this->checked_.expr_types.assign(this->module_.exprs.size(), INVALID_TYPE_HANDLE);
    this->checked_.expr_expected_types.assign(this->module_.exprs.size(), INVALID_TYPE_HANDLE);
    this->checked_.expr_c_names.assign(this->module_.exprs.size(), {});
    this->checked_.pattern_c_names.assign(this->module_.patterns.size(), {});
    this->checked_.pattern_case_sets.assign(this->module_.patterns.size(), {});
    this->checked_.syntax_type_handles.assign(this->module_.types.size(), INVALID_TYPE_HANDLE);
    this->checked_.stmt_local_types.assign(this->module_.stmts.size(), INVALID_TYPE_HANDLE);
    this->checked_.item_c_names.assign(this->module_.items.size(), {});
    const base::usize enum_cases = enum_case_count(this->module_);
    this->checked_.functions.reserve(this->module_.items.size());
    this->checked_.structs.reserve(this->module_.items.size());
    this->checked_.enum_cases.reserve(enum_cases);
    this->checked_.type_aliases.reserve(this->module_.items.size());
    this->named_types_.reserve(this->module_.items.size());
    this->type_visibilities_.reserve(this->module_.items.size());
    this->generic_struct_templates_.reserve(this->module_.items.size());
    this->generic_enum_templates_.reserve(this->module_.items.size());
    this->generic_type_alias_templates_.reserve(this->module_.items.size());
    this->generic_function_templates_.reserve(this->module_.items.size());
    this->generic_method_templates_.reserve(this->module_.items.size());
    this->generic_struct_instances_.reserve(this->module_.items.size());
    this->generic_enum_instances_.reserve(this->module_.items.size());
    this->resolved_generic_type_aliases_.reserve(this->module_.items.size());
    this->generic_function_instances_.reserve(this->module_.items.size());
    this->generic_placeholder_functions_.reserve(this->module_.items.size());
    this->resolved_type_aliases_.reserve(this->module_.items.size());
    this->global_values_.reserve(this->module_.items.size());
    this->function_definition_items_.reserve(this->module_.items.size());
    this->function_body_states_.reserve(this->module_.items.size());
    this->struct_infos_by_type_.reserve(this->module_.items.size());
    this->identifiers_.reserve(this->module_.items.size() + enum_cases + this->module_.modules.size());
    this->named_types_by_name_.reserve(this->module_.items.size());
    this->type_aliases_by_name_.reserve(this->module_.items.size());
    this->generic_struct_templates_by_name_.reserve(this->module_.items.size());
    this->generic_enum_templates_by_name_.reserve(this->module_.items.size());
    this->generic_type_alias_templates_by_name_.reserve(this->module_.items.size());
    this->generic_function_templates_by_name_.reserve(this->module_.items.size());
    this->generic_method_templates_by_name_.reserve(this->module_.items.size());
    this->functions_by_name_.reserve(this->module_.items.size());
    this->methods_by_name_.reserve(this->module_.items.size());
    this->global_values_by_name_.reserve(this->module_.items.size());
    this->method_global_values_by_name_.reserve(this->module_.items.size());
    this->enum_cases_by_module_name_.reserve(enum_cases);
    this->enum_cases_by_type_.reserve(enum_cases);
    this->enum_cases_by_type_and_case_.reserve(enum_cases);
    this->visible_modules_cache_.reserve(this->module_.modules.size());
    this->module_export_modules_cache_.reserve(this->module_.modules.size());
    this->register_type_names();
    this->resolve_type_alias_decls();
    this->analyze_struct_properties();
    this->register_value_names();
    this->validate_module_namespace_conflicts();
    this->validate_function_prototypes();

    for (const auto& entry : this->generic_function_templates_) {
        this->analyze_generic_function_definition(entry.second);
    }
    for (const auto& entry : this->generic_method_templates_) {
        this->analyze_generic_function_definition(entry.second);
    }

    for (const syntax::ItemNode& item : this->module_.items) {
        if (is_function_item(item) &&
            !this->has_generic_params(item) &&
            !item.is_extern_c &&
            !item.is_prototype &&
            syntax::is_valid(item.body)) {
            this->analyze_function_body(item);
        }
    }

    this->analyze_entry_points();
    this->analyze_const_decls();
    this->validate_type_layouts();
    this->validate_abi_symbols();

    if (this->diagnostics_.has_error()) {
        return base::Result<CheckedModule>::fail({base::ErrorCode::sema_error, std::string(SEMA_ANALYSIS_FAILED)});
    }
    this->checked_.normalized_ast.emplace(std::move(this->module_));
    return base::Result<CheckedModule>::ok(std::move(this->checked_));
}

void SemanticAnalyzer::normalize_parser_only_module_contract() {
    if (!this->module_.modules.empty()) {
        return;
    }
    syntax::ModuleInfo root;
    root.path = this->module_.module_path;
    this->module_.modules.push_back(std::move(root));
    this->module_.item_modules.assign(this->module_.items.size(), syntax::ModuleId {0});
}

bool SemanticAnalyzer::validate_ast_contract() {
    bool valid = true;
    if (this->module_.item_modules.size() != this->module_.items.size()) {
        this->report({}, std::string(SEMA_AST_ITEM_MODULE_CONTRACT));
        valid = false;
    }
    const base::usize count = std::min(this->module_.item_modules.size(), this->module_.items.size());
    for (base::usize i = 0; i < count; ++i) {
        const syntax::ModuleId owner = this->module_.item_modules[i];
        if (!syntax::is_valid(owner) || owner.value >= this->module_.modules.size()) {
            this->report(this->module_.items[i].range, std::string(SEMA_AST_ITEM_MODULE_INVALID));
            valid = false;
        }
    }
    return valid;
}

} // namespace aurex::sema
