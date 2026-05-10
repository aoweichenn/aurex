#include <aurex/sema/sema.hpp>

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
    checked_.expr_types.assign(module_.exprs.size(), invalid_type_handle);
    checked_.expr_c_names.assign(module_.exprs.size(), {});
    checked_.pattern_c_names.assign(module_.patterns.size(), {});
    checked_.pattern_case_sets.assign(module_.patterns.size(), {});
    checked_.syntax_type_handles.assign(module_.types.size(), invalid_type_handle);
    checked_.stmt_local_types.assign(module_.stmts.size(), invalid_type_handle);
    checked_.item_c_names.assign(module_.items.size(), {});
    register_type_names();
    resolve_type_alias_decls();
    analyze_struct_properties();
    register_value_names();
    validate_function_prototypes();

    for (const syntax::ItemNode& item : module_.items) {
        if (is_function_item(item) &&
            item.generic_params.empty() &&
            !item.is_extern_c &&
            !item.is_prototype &&
            syntax::is_valid(item.body)) {
            analyze_function_body(item);
        }
    }

    analyze_entry_points();
    analyze_const_decls();
    validate_type_layouts();
    validate_abi_symbols();

    if (diagnostics_.has_error()) {
        return base::Result<CheckedModule>::fail({base::ErrorCode::sema_error, "semantic analysis failed"});
    }
    return base::Result<CheckedModule>::ok(std::move(checked_));
}

} // namespace aurex::sema
