#include <aurex/sema/sema.hpp>

#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <utility>

namespace aurex::sema {

namespace {

[[nodiscard]] base::usize enum_case_count(const syntax::AstModule& module) noexcept {
    base::usize count = 0;
    for (base::usize i = 0; i < module.items.size(); ++i) {
        if (module.items.kind(i) != syntax::ItemKind::enum_decl) {
            continue;
        }
        const syntax::ItemNode item = module.items[i];
        count += item.enum_cases.size();
    }
    return count;
}

} // namespace

SemanticAnalyzer::SemanticAnalyzer(
    syntax::AstModule& module,
    base::DiagnosticSink& diagnostics,
    const SemanticOptions options
) noexcept
    : module_(module), diagnostics_(diagnostics), options_(options) {}

SemanticAnalyzer::SemanticAnalyzer(
    syntax::AstModule&& module,
    base::DiagnosticSink& diagnostics,
    const SemanticOptions options
) noexcept
    : owned_module_(std::move(module)), module_(*this->owned_module_), diagnostics_(diagnostics), options_(options) {}

base::Result<CheckedModule> SemanticAnalyzer::analyze() {
    this->checked_.normalized_ast.original_expr_count = this->module_.exprs.size();
    this->checked_.normalized_ast.original_type_count = this->module_.types.size();
    if (!this->module_.identifiers_ready()) {
        this->module_.intern_identifiers();
    }
    this->normalize_parser_only_module_contract();
    this->module_.finalize_identifiers();
    if (!this->validate_ast_contract()) {
        return base::Result<CheckedModule>::fail({base::ErrorCode::sema_error, std::string(SEMA_ANALYSIS_FAILED)});
    }

    this->checked_.expr_types.assign(this->module_.exprs.size(), INVALID_TYPE_HANDLE);
    this->checked_.expr_expected_types.assign(this->module_.exprs.size(), INVALID_TYPE_HANDLE);
    this->checked_.expr_c_name_ids.assign(this->module_.exprs.size(), INVALID_IDENT_ID);
    this->checked_.pattern_c_name_ids.assign(this->module_.patterns.size(), INVALID_IDENT_ID);
    this->checked_.syntax_type_handles.assign(this->module_.types.size(), INVALID_TYPE_HANDLE);
    this->checked_.stmt_local_types.assign(this->module_.stmts.size(), INVALID_TYPE_HANDLE);
    this->checked_.item_c_name_ids.assign(this->module_.items.size(), INVALID_IDENT_ID);
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
    const base::usize expected_identifier_count =
        this->module_.identifiers.size() +
        this->module_.items.size() +
        enum_cases +
        this->module_.modules.size();
    this->module_.identifiers.reserve(expected_identifier_count);
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

    for (base::u32 index = 0; index < this->module_.items.size(); ++index) {
        if (this->module_.items.kind(index) != syntax::ItemKind::fn_decl) {
            continue;
        }
        const syntax::ItemNode item = this->module_.items[index];
        if (!this->has_generic_params(item) &&
            !item.is_extern_c &&
            !item.is_prototype &&
            syntax::is_valid(item.body)) {
            this->analyze_function_body(item, syntax::ItemId {index});
        }
    }

    this->analyze_entry_points();
    this->analyze_const_decls();
    this->validate_type_layouts();
    this->validate_abi_symbols();

    if (this->diagnostics_.has_error()) {
        return base::Result<CheckedModule>::fail({base::ErrorCode::sema_error, std::string(SEMA_ANALYSIS_FAILED)});
    }
    this->checked_.normalized_ast.final_expr_count = this->module_.exprs.size();
    this->checked_.normalized_ast.final_type_count = this->module_.types.size();
    return base::Result<CheckedModule>::ok(std::move(this->checked_));
}

void SemanticAnalyzer::normalize_parser_only_module_contract() {
    if (!this->module_.modules.empty()) {
        this->checked_.normalized_ast.parser_only_module_contract_added = false;
        return;
    }
    syntax::ModuleInfo root;
    root.path = this->module_.module_path;
    this->module_.modules.push_back(std::move(root));
    this->module_.item_modules.assign(this->module_.items.size(), syntax::ModuleId {0});
    this->checked_.normalized_ast.parser_only_module_contract_added = true;
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
            this->report(this->module_.items.range(i), std::string(SEMA_AST_ITEM_MODULE_INVALID));
            valid = false;
        }
    }
    return valid;
}

} // namespace aurex::sema
