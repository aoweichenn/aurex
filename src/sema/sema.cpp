#include <aurex/sema/sema.hpp>

#include <aurex/sema/sema_messages.hpp>

#include <utility>

namespace aurex::sema {

namespace {

[[nodiscard]] bool is_function_item(const syntax::ItemNode& item) noexcept {
    return item.kind == syntax::ItemKind::fn_decl;
}

} // namespace

SemanticAnalyzer::SemanticAnalyzer(const syntax::AstModule& module, base::DiagnosticSink& diagnostics) noexcept
    : module_(module), diagnostics_(diagnostics) {}

base::Result<CheckedModule> SemanticAnalyzer::analyze() {
    this->checked_.expr_types.assign(this->module_.exprs.size(), INVALID_TYPE_HANDLE);
    this->checked_.expr_c_names.assign(this->module_.exprs.size(), {});
    this->checked_.pattern_c_names.assign(this->module_.patterns.size(), {});
    this->checked_.pattern_case_sets.assign(this->module_.patterns.size(), {});
    this->checked_.syntax_type_handles.assign(this->module_.types.size(), INVALID_TYPE_HANDLE);
    this->checked_.stmt_local_types.assign(this->module_.stmts.size(), INVALID_TYPE_HANDLE);
    this->checked_.item_c_names.assign(this->module_.items.size(), {});
    this->register_type_names();
    this->resolve_type_alias_decls();
    this->analyze_struct_properties();
    this->register_value_names();
    this->validate_function_prototypes();

    for (const auto& entry : this->generic_function_templates_) {
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
    return base::Result<CheckedModule>::ok(std::move(this->checked_));
}

} // namespace aurex::sema
