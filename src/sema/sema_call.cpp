#include <aurex/sema/sema.hpp>

#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <string>
#include <string_view>

namespace aurex::sema {

namespace {

constexpr base::usize SEMA_RECEIVER_ARGUMENT_COUNT = 1;
constexpr std::string_view SEMA_FUNCTION_VALUE_CALL_NAME = "<function>";

[[nodiscard]] std::string module_selector_path_name(const std::vector<std::string_view>& parts) {
    std::string name;
    for (base::usize i = 0; i < parts.size(); ++i) {
        if (i != 0) {
            name.push_back('.');
        }
        name += parts[i];
    }
    return name;
}

[[nodiscard]] FunctionCallConv signature_call_conv(const FunctionSignature& signature) noexcept {
    return signature.is_extern_c || signature.is_export_c ? FunctionCallConv::c : FunctionCallConv::aurex;
}

[[nodiscard]] base::SourceRange call_expr_range_or(
    const syntax::AstModule& module,
    const syntax::ExprId expr,
    const base::SourceRange fallback
) noexcept {
    return syntax::is_valid(expr) && expr.value < module.exprs.size()
        ? module.exprs.range(expr.value)
        : fallback;
}

} // namespace

TypeHandle SemanticAnalyzer::function_type_from_signature(const FunctionSignature& signature) {
    return this->checked_.types.function(
        signature_call_conv(signature),
        signature.is_unsafe,
        signature.is_variadic,
        signature.param_types,
        signature.return_type
    );
}

TypeHandle SemanticAnalyzer::function_type_from_symbol(const Symbol& symbol, const base::SourceRange range) {
    const FunctionSignature* signature = syntax::is_valid(symbol.module)
        ? this->find_function_in_module(symbol.module, symbol.name_id, symbol.name, range, false)
        : nullptr;
    if (signature == nullptr) {
        return symbol.type;
    }
    this->ensure_function_return_known(*signature, range);
    return this->function_type_from_signature(*signature);
}

bool SemanticAnalyzer::in_unsafe_context() const noexcept {
    return this->unsafe_context_depth_ > 0;
}

void SemanticAnalyzer::require_unsafe_context(
    const base::SourceRange range,
    const std::string_view operation
) {
    if (!this->in_unsafe_context()) {
        this->report(range, std::string(operation));
    }
}

void SemanticAnalyzer::validate_unsafe_call(
    const FunctionSignature& signature,
    const base::SourceRange range
) {
    if (signature.is_unsafe && !this->in_unsafe_context()) {
        this->report(range, sema_unsafe_function_call_message(signature.name));
    }
}

void SemanticAnalyzer::validate_unsafe_function_value_call(
    const TypeHandle callee_type,
    const base::SourceRange range
) {
    if (!this->checked_.types.is_function(callee_type)) {
        return;
    }
    const TypeInfo& function = this->checked_.types.get(callee_type);
    if (function.function_is_unsafe && !this->in_unsafe_context()) {
        this->report(range, sema_unsafe_function_call_message(SEMA_FUNCTION_VALUE_CALL_NAME));
    }
}

TypeHandle SemanticAnalyzer::resolve_associated_type_owner(
    const syntax::ExprId object,
    const bool report_unknown
) {
    return this->resolve_type_selector(object, report_unknown);
}

TypeHandle SemanticAnalyzer::resolve_associated_generic_type_owner(
    const syntax::ExprId apply,
    const bool report_unknown
) {
    return this->resolve_type_selector(apply, report_unknown);
}

SemanticAnalyzer::ModuleSelectorPath SemanticAnalyzer::expr_selector_path(
    const syntax::ExprId expr_id
) const {
    ModuleSelectorPath path;
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->module_.exprs.size()) {
        return path;
    }

    path.range = this->module_.exprs.range(expr_id.value);
    syntax::ExprId current = expr_id;
    while (syntax::is_valid(current) && current.value < this->module_.exprs.size()) {
        const SemanticAnalyzer::ExprView expr = this->expr_view(current);
        if (expr.kind == syntax::ExprKind::field) {
            if (expr.field_name.empty()) {
                return {};
            }
            path.parts.push_back(expr.field_name);
            path.part_ids.push_back(expr.field_name_id);
            current = expr.object;
            continue;
        }
        if (expr.kind == syntax::ExprKind::name && expr.scope_name.empty()) {
            path.parts.push_back(expr.text);
            path.part_ids.push_back(expr.text_id);
            std::reverse(path.parts.begin(), path.parts.end());
            std::reverse(path.part_ids.begin(), path.part_ids.end());
            return path;
        }
        return {};
    }
    return {};
}

SemanticAnalyzer::ModuleSelector SemanticAnalyzer::resolve_module_selector(
    const syntax::ExprId expr_id,
    const bool report_unknown
) {
    const ModuleSelectorPath path = this->expr_selector_path(expr_id);
    if (path.parts.empty()) {
        return {};
    }

    if (path.parts.size() == 1) {
        const std::string_view name = path.parts.front();
        const IdentId name_id = path.part_ids.front();
        const syntax::ModuleId alias_module = this->resolve_import_alias(name, path.range, false);
        if (syntax::is_valid(alias_module)) {
            return ModuleSelector {alias_module, false};
        }
        const syntax::ModuleId path_module = this->find_visible_module_path(path.parts);
        if (syntax::is_valid(path_module)) {
            return ModuleSelector {path_module, false};
        }
        const bool failed_selector = !this->selector_base_has_non_module_meaning(name_id, name);
        if (failed_selector && report_unknown) {
            if (this->visible_root_module_name_exists(name)) {
                this->report(path.range, sema_unknown_module_path_message(std::string(name)));
            } else {
                static_cast<void>(this->resolve_import_alias(name, path.range, true));
            }
        }
        return ModuleSelector {syntax::INVALID_MODULE_ID, failed_selector};
    }

    const syntax::ModuleId path_module = this->find_visible_module_path(path.parts);
    if (syntax::is_valid(path_module)) {
        return ModuleSelector {path_module, false};
    }

    const std::string_view root = path.parts.front();
    const IdentId root_id = path.part_ids.front();
    const bool failed_selector =
        !this->selector_base_has_non_module_meaning(root_id, root) &&
        !this->module_alias_visible(root) &&
        !this->visible_module_path_prefix_exists(path.parts);
    if (failed_selector && report_unknown) {
        if (this->visible_root_module_name_exists(root)) {
            this->report(path.range, sema_unknown_module_path_message(module_selector_path_name(path.parts)));
        } else {
            static_cast<void>(this->resolve_import_alias(root, path.range, true));
        }
    }
    return ModuleSelector {syntax::INVALID_MODULE_ID, failed_selector};
}

SemanticAnalyzer::NamedTypeSelector SemanticAnalyzer::resolve_named_type_selector(
    const syntax::ExprId expr_id,
    const bool report_unknown
) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->module_.exprs.size()) {
        return {};
    }
    const SemanticAnalyzer::ExprView expr = this->expr_view(expr_id);
    if (expr.kind == syntax::ExprKind::generic_apply) {
        NamedTypeSelector selector = this->resolve_named_type_selector(expr.callee, report_unknown);
        if (selector.name.empty()) {
            return {};
        }
        selector.range = expr.range;
        selector.type_args.assign(expr.type_args.begin(), expr.type_args.end());
        return selector;
    }
    if (expr.kind == syntax::ExprKind::name) {
        if (!expr.scope_name.empty()) {
            const syntax::ModuleId module = this->resolve_import_alias(expr.scope_name, expr.scope_range, report_unknown);
            if (!syntax::is_valid(module)) {
                return {};
            }
            NamedTypeSelector selector;
            selector.module = module;
            selector.name = expr.text;
            selector.name_id = expr.text_id;
            selector.range = expr.range;
            selector.qualified = true;
            return selector;
        }
        if (syntax::is_valid(this->resolve_import_alias(expr.text, expr.range, false))) {
            return {};
        }
        NamedTypeSelector selector;
        selector.name = expr.text;
        selector.name_id = expr.text_id;
        selector.range = expr.range;
        return selector;
    }
    if (expr.kind != syntax::ExprKind::field ||
        !syntax::is_valid(expr.object) ||
        expr.object.value >= this->module_.exprs.size()) {
        return {};
    }
    const ModuleSelector module = this->resolve_module_selector(expr.object, false);
    if (!syntax::is_valid(module.module)) {
        return {};
    }
    NamedTypeSelector selector;
    selector.module = module.module;
    selector.name = expr.field_name;
    selector.name_id = expr.field_name_id;
    selector.range = expr.range;
    selector.qualified = true;
    return selector;
}

TypeHandle SemanticAnalyzer::resolve_type_selector(
    const syntax::ExprId expr_id,
    const bool report_unknown
) {
    const NamedTypeSelector selector = this->resolve_named_type_selector(expr_id, report_unknown);
    if (selector.name.empty()) {
        return INVALID_TYPE_HANDLE;
    }
    return this->resolve_named_type_selector_type(selector, false, report_unknown);
}

TypeHandle SemanticAnalyzer::resolve_named_type_selector_type(
    const NamedTypeSelector& selector,
    const bool opaque_allowed_as_pointee,
    const bool report_unknown
) {
    if (selector.name.empty()) {
        return INVALID_TYPE_HANDLE;
    }
    if (!selector.qualified && selector.type_args.empty() && this->current_generic_context_ != nullptr) {
        if (const auto found = this->current_generic_context_->params.find(selector.name_id);
            found != this->current_generic_context_->params.end()) {
            return found->second;
        }
    }
    if (!selector.type_args.empty()) {
        return this->resolve_generic_type_selector(
            selector,
            syntax::INVALID_TYPE_ID,
            opaque_allowed_as_pointee,
            report_unknown
        );
    }
    const bool reported_missing_generic_args =
        !selector.qualified &&
        report_unknown &&
        this->report_generic_type_requires_args_if_visible(selector.name_id, selector.name, selector.range);
    const TypeHandle resolved = selector.qualified
        ? this->find_type_in_module(
              selector.module,
              selector.name_id,
              selector.name,
              selector.range,
              opaque_allowed_as_pointee,
              report_unknown
          )
        : this->find_type_in_visible_modules(
              selector.name_id,
              selector.name,
              selector.range,
              opaque_allowed_as_pointee,
              report_unknown && !reported_missing_generic_args
          );
    if (is_valid(resolved) &&
        this->checked_.types.get(resolved).kind == TypeKind::opaque_struct &&
        !opaque_allowed_as_pointee) {
        this->report(selector.range, std::string(SEMA_OPAQUE_POINTER_ONLY));
    }
    return resolved;
}

bool SemanticAnalyzer::selector_base_has_non_module_meaning(
    const IdentId name_id,
    const std::string_view name
) const {
    if (this->symbols_.find(name_id) != nullptr) {
        return true;
    }
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->current_module_, name_id);
    if (is_valid(lookup_key)) {
        if (this->global_values_by_name_.contains(lookup_key) ||
            this->named_types_by_name_.contains(lookup_key) ||
            this->type_aliases_by_name_.contains(lookup_key)) {
            return true;
        }
    }
    if (this->find_any_generic_type_template_in_module(this->current_module_, name_id, name) != nullptr) {
        return true;
    }
    return false;
}

TypeHandle SemanticAnalyzer::resolve_generic_type_selector(
    const NamedTypeSelector& selector,
    const syntax::TypeId use_type_id,
    const bool opaque_allowed_as_pointee,
    const bool report_unknown
) {
    if (selector.name.empty()) {
        return INVALID_TYPE_HANDLE;
    }
    std::vector<TypeHandle> args;
    args.reserve(selector.type_args.size());
    for (const syntax::TypeId arg : selector.type_args) {
        args.push_back(this->resolve_type(arg, false));
    }

    syntax::TypeNode use_type;
    use_type.kind = syntax::TypeKind::named;
    use_type.name = selector.name;
    use_type.range = selector.range;
    use_type.type_args = selector.type_args;

    const GenericTemplateInfo* generic_struct = selector.qualified
        ? this->find_generic_struct_in_module(selector.module, selector.name_id, selector.name, selector.range, false)
        : this->find_generic_struct_in_visible_modules(selector.name_id, selector.name, selector.range, false);
    if (generic_struct != nullptr) {
        return this->instantiate_generic_struct(*generic_struct, use_type, use_type_id, args);
    }
    const GenericTemplateInfo* generic_enum = selector.qualified
        ? this->find_generic_enum_in_module(selector.module, selector.name_id, selector.name, selector.range, false)
        : this->find_generic_enum_in_visible_modules(selector.name_id, selector.name, selector.range, false);
    if (generic_enum != nullptr) {
        return this->instantiate_generic_enum(*generic_enum, use_type, use_type_id, args);
    }
    const GenericTemplateInfo* generic_alias = selector.qualified
        ? this->find_generic_type_alias_in_module(selector.module, selector.name_id, selector.name, selector.range, false)
        : this->find_generic_type_alias_in_visible_modules(selector.name_id, selector.name, selector.range, false);
    if (generic_alias != nullptr) {
        return this->instantiate_generic_type_alias(*generic_alias, use_type, use_type_id, args, opaque_allowed_as_pointee);
    }

    const TypeHandle concrete = selector.qualified
        ? this->find_type_in_module(
              selector.module,
              selector.name_id,
              selector.name,
              selector.range,
              opaque_allowed_as_pointee,
              false
          )
        : this->find_type_in_visible_modules(
              selector.name_id,
              selector.name,
              selector.range,
              opaque_allowed_as_pointee,
              false
          );
    if (is_valid(concrete)) {
        if (report_unknown) {
            this->report(selector.range, sema_type_not_generic_message(selector.name));
        }
        return INVALID_TYPE_HANDLE;
    }
    if (report_unknown) {
        if (selector.qualified) {
            this->report_generic_type_template_in_module(selector.module, selector.name_id, selector.name, selector.range);
        } else {
            static_cast<void>(this->find_generic_struct_in_visible_modules(selector.name_id, selector.name, selector.range, true));
        }
    }
    return INVALID_TYPE_HANDLE;
}

const FunctionSignature* SemanticAnalyzer::find_function_selector(
    const syntax::ExprId callee,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (syntax::is_valid(callee) && callee.value < this->module_.exprs.size()) {
        const SemanticAnalyzer::ExprView expr = this->expr_view(callee);
        if (expr.kind == syntax::ExprKind::name && !expr.scope_name.empty()) {
            const syntax::ModuleId module = this->resolve_import_alias(expr.scope_name, expr.scope_range, report_unknown);
            return syntax::is_valid(module)
                ? this->find_function_in_module(module, name_id, name, range, report_unknown)
                : nullptr;
        }
        if (expr.kind == syntax::ExprKind::field) {
            const ModuleSelector module = this->resolve_module_selector(expr.object, false);
            if (syntax::is_valid(module.module)) {
                return this->find_function_in_module(module.module, name_id, name, range, report_unknown);
            }
        }
    }
    return this->find_function_in_visible_modules(name_id, name, range, report_unknown);
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_function_selector(
    const NamedTypeSelector& selector,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (selector.name.empty()) {
        return nullptr;
    }
    return selector.qualified
        ? this->find_generic_function_in_module(selector.module, selector.name_id, selector.name, range, report_unknown)
        : this->find_generic_function_in_visible_modules(selector.name_id, selector.name, range, report_unknown);
}

TypeHandle SemanticAnalyzer::analyze_call_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const TypeHandle expected_type
) {
    if (!syntax::is_valid(expr.callee) ||
        expr.callee.value >= this->module_.exprs.size()) {
        this->report(expr.range, std::string(SEMA_CALLEE_FUNCTION_NAME));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const SemanticAnalyzer::ExprView callee = this->expr_view(expr.callee);
    if (callee.kind == syntax::ExprKind::generic_apply) {
        const NamedTypeSelector selector = this->resolve_named_type_selector(expr.callee, false);
        if (selector.name.empty()) {
            if (syntax::is_valid(callee.callee) && callee.callee.value < this->module_.exprs.size()) {
                const SemanticAnalyzer::ExprView generic_callee = this->expr_view(callee.callee);
                if (generic_callee.kind == syntax::ExprKind::field) {
                    const ModuleSelector module = this->resolve_module_selector(generic_callee.object, false);
                    if (module.failed_as_module_selector) {
                        static_cast<void>(this->resolve_module_selector(generic_callee.object, true));
                        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
                    }
                }
            }
            this->report(callee.range, std::string(SEMA_EXPLICIT_GENERIC_CALL_SYNTAX));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        return this->analyze_explicit_generic_function_call_expr(
            expr_id,
            expr,
            callee,
            selector.name
        );
    }
    if (callee.kind == syntax::ExprKind::name && callee.scope_name.empty()) {
        if (const Symbol* local = this->symbols_.find(callee.text_id); local != nullptr) {
            return this->analyze_function_value_call_expr(expr_id, expr, callee.text);
        }
    }
    if (callee.kind != syntax::ExprKind::name && callee.kind != syntax::ExprKind::field) {
        return this->analyze_function_value_call_expr(expr_id, expr, SEMA_FUNCTION_VALUE_CALL_NAME);
    }
    const std::string name = callee.kind == syntax::ExprKind::field
        ? std::string(callee.field_name)
        : std::string(callee.text);

    if (const EnumCaseInfo* enum_case = this->find_enum_constructor(expr.callee, false); enum_case != nullptr) {
        return this->analyze_enum_constructor_call(expr_id, expr, *enum_case);
    }
    if (callee.kind == syntax::ExprKind::field) {
        const ModuleSelector module = this->resolve_module_selector(callee.object, false);
        if (syntax::is_valid(module.module)) {
            return this->analyze_function_call_expr(expr_id, expr, callee, name, expected_type);
        }
        if (module.failed_as_module_selector) {
            static_cast<void>(this->resolve_module_selector(callee.object, true));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        return this->analyze_field_call_expr(expr_id, expr, callee, name, expected_type);
    }
    return this->analyze_function_call_expr(expr_id, expr, callee, name, expected_type);
}

TypeHandle SemanticAnalyzer::analyze_explicit_generic_function_call_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const SemanticAnalyzer::ExprView&,
    const std::string_view name
) {
    const NamedTypeSelector selector = this->resolve_named_type_selector(expr.callee, true);
    if (selector.name.empty()) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const base::SourceRange callee_range = selector.range;

    const GenericTemplateInfo* generic = this->find_generic_function_selector(selector, callee_range, false);
    if (generic == nullptr) {
        NamedTypeSelector generic_lookup = selector;
        generic_lookup.type_args.clear();
        const FunctionSignature* signature = selector.qualified
            ? this->find_function_in_module(selector.module, selector.name_id, name, callee_range, false)
            : this->find_function_in_visible_modules(selector.name_id, name, callee_range, false);
        if (signature != nullptr) {
            this->report(callee_range, sema_function_not_generic_message(name));
        } else if (generic_lookup.qualified) {
            static_cast<void>(this->find_generic_function_selector(generic_lookup, callee_range, true));
        } else if (selector.qualified && syntax::is_valid(generic_lookup.module)) {
            this->report(callee_range, sema_unknown_function_in_module_message(this->module_name(generic_lookup.module), name));
        } else {
            static_cast<void>(this->find_generic_function_selector(generic_lookup, callee_range, true));
        }
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    std::vector<TypeHandle> args;
    args.reserve(selector.type_args.size());
    for (const syntax::TypeId arg : selector.type_args) {
        args.push_back(this->resolve_type(arg));
    }
    FunctionSignature* signature = this->instantiate_generic_function(*generic, args, callee_range);
    if (signature == nullptr) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    this->validate_unsafe_call(*signature, callee_range);
    this->record_expr_c_name(expr.callee, signature->c_name);
    this->validate_call_arguments(expr, name, signature->param_types, 0, signature->is_variadic);
    return this->record_expr_type(expr_id, signature->return_type);
}

TypeHandle SemanticAnalyzer::analyze_enum_constructor_call(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const EnumCaseInfo& enum_case
) {
    const std::string case_display_name = enum_case_display_name(this->checked_.types, enum_case);
    if (enum_case.payload_types.empty()) {
        this->report(expr.range, sema_enum_payload_constructor_case_message(case_display_name));
    }
    if (expr.args.size() != enum_case.payload_types.size()) {
        if (enum_case.payload_types.size() == 1) {
            this->report(expr.range, sema_enum_payload_constructor_arity_message(case_display_name));
        } else {
            this->report(expr.range, sema_enum_payload_constructor_argument_count_message(
                case_display_name,
                enum_case.payload_types.size()
            ));
        }
    }
    const base::usize checked_arg_count = std::min(expr.args.size(), enum_case.payload_types.size());
    for (base::usize i = 0; i < checked_arg_count; ++i) {
        const TypeHandle expected = enum_case.payload_types[i];
        const TypeHandle actual = this->analyze_expr(expr.args[i], expected);
        if (!this->can_assign(expected, actual, expr.args[i])) {
            this->report(
                call_expr_range_or(this->module_, expr.args[i], expr.range),
                std::string(SEMA_ENUM_PAYLOAD_ARGUMENT_TYPE_MISMATCH)
            );
        }
        if (this->checked_.types.contains_array(expected)) {
            this->report(
                call_expr_range_or(this->module_, expr.args[i], expr.range),
                std::string(SEMA_ENUM_PAYLOAD_ARRAY_ARGUMENT_UNSUPPORTED)
            );
        }
    }
    this->record_expr_c_name(expr.callee, enum_case.c_name);
    return this->record_expr_type(expr_id, enum_case.type);
}

TypeHandle SemanticAnalyzer::analyze_field_call_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const SemanticAnalyzer::ExprView& callee,
    const std::string_view name,
    const TypeHandle
) {
    const FunctionSignature* signature = nullptr;
    TypeHandle receiver_type = INVALID_TYPE_HANDLE;
    bool has_receiver = false;
    bool receiver_valid = true;

    if (syntax::is_valid(callee.object) && callee.object.value < this->module_.exprs.size()) {
        const TypeHandle associated_owner = this->resolve_type_selector(callee.object, false);
        if (is_valid(associated_owner)) {
        signature = this->find_method_in_visible_modules(
            associated_owner,
            callee.field_name_id,
            callee.field_name,
            callee.range,
            false,
            false
        );
        if (signature == nullptr) {
            signature = this->find_generic_method_in_visible_modules(
                associated_owner,
                callee.field_name_id,
                callee.field_name,
                callee.range,
                false,
                    false
                );
        }
        if (signature == nullptr) {
            signature = this->find_method_in_visible_modules(
                associated_owner,
                callee.field_name_id,
                callee.field_name,
                callee.range,
                false
            );
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
            if (signature->has_self_param) {
                this->report(
                    callee.range,
                    sema_method_requires_receiver_message(this->checked_.types.display_name(associated_owner), name)
                );
                receiver_valid = false;
            }
        }
    }
    if (signature == nullptr) {
        has_receiver = true;
        receiver_type = this->analyze_expr(callee.object);
        TypeHandle owner_type = receiver_type;
        if (this->checked_.types.is_pointer(owner_type) || this->checked_.types.is_reference(owner_type)) {
            owner_type = this->checked_.types.get(owner_type).pointee;
        }
        signature = this->find_method_in_visible_modules(
            owner_type,
            callee.field_name_id,
            callee.field_name,
            callee.range,
            true,
            false
        );
        if (signature == nullptr) {
            signature = this->find_generic_method_in_visible_modules(
                owner_type,
                callee.field_name_id,
                callee.field_name,
                callee.range,
                true,
                false
            );
        }
        if (signature == nullptr) {
            const TypeHandle callee_type = this->analyze_expr(expr.callee);
            if (this->checked_.types.is_function(callee_type)) {
                return this->analyze_function_value_call_expr(expr_id, expr, name);
            }
            signature = this->find_method_in_visible_modules(
                owner_type,
                callee.field_name_id,
                callee.field_name,
                callee.range,
                true
            );
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        receiver_valid = this->method_receiver_matches(*signature, receiver_type, callee.object);
    }
    this->ensure_function_return_known(*signature, callee.range);
    this->validate_unsafe_call(*signature, callee.range);
    this->record_expr_c_name(expr.callee, signature->c_name);
    const base::usize receiver_count = has_receiver ? SEMA_RECEIVER_ARGUMENT_COUNT : 0;
    if (!receiver_valid || signature->param_types.size() < receiver_count) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    this->validate_call_arguments(expr, name, signature->param_types, receiver_count, signature->is_variadic);
    return this->record_expr_type(expr_id, signature->return_type);
}

TypeHandle SemanticAnalyzer::analyze_function_value_call_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const std::string_view name
) {
    const TypeHandle callee_type = this->analyze_expr(expr.callee);
    if (!this->checked_.types.is_function(callee_type)) {
        this->report(
            expr.range,
            std::string(SEMA_CALLEE_FUNCTION_NAME)
        );
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const TypeInfo& function = this->checked_.types.get(callee_type);
    this->validate_unsafe_function_value_call(callee_type, expr.range);
    this->validate_call_arguments(
        expr,
        name.empty() ? SEMA_FUNCTION_VALUE_CALL_NAME : name,
        function.function_params,
        0,
        function.function_is_variadic
    );
    return this->record_expr_type(expr_id, function.function_return);
}

TypeHandle SemanticAnalyzer::analyze_function_call_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const SemanticAnalyzer::ExprView& callee,
    const std::string_view name,
    const TypeHandle
) {
    const base::SourceRange callee_range = callee.range;
    const FunctionSignature* signature = nullptr;
    const NamedTypeSelector selector = this->resolve_named_type_selector(expr.callee, false);
    if (const GenericTemplateInfo* generic = this->find_generic_function_selector(selector, callee_range, false);
        generic != nullptr) {
        std::vector<TypeHandle> args;
        if (!this->infer_generic_arguments(*generic, expr, args)) {
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        signature = this->instantiate_generic_function(*generic, args, callee_range);
        if (signature == nullptr) {
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        this->validate_unsafe_call(*signature, callee_range);
        this->record_expr_c_name(expr.callee, signature->c_name);
        this->validate_call_arguments(expr, name, signature->param_types, 0, signature->is_variadic);
        return this->record_expr_type(expr_id, signature->return_type);
    }
    signature = this->find_function_selector(
        expr.callee,
        callee.kind == syntax::ExprKind::field ? callee.field_name_id : callee.text_id,
        name,
        callee_range,
        true
    );
    if (signature == nullptr) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    this->ensure_function_return_known(*signature, call_expr_range_or(this->module_, expr.callee, callee_range));
    this->validate_unsafe_call(*signature, callee_range);
    this->record_expr_c_name(expr.callee, signature->c_name);
    this->validate_call_arguments(expr, name, signature->param_types, 0, signature->is_variadic);
    return this->record_expr_type(expr_id, signature->return_type);
}

void SemanticAnalyzer::validate_call_arguments(
    const SemanticAnalyzer::ExprView& expr,
    const std::string_view name,
    const std::span<const TypeHandle> param_types,
    const base::usize receiver_count,
    const bool is_variadic
) {
    const base::usize expected_count =
        param_types.size() >= receiver_count
            ? param_types.size() - receiver_count
            : 0;
    if (is_variadic ? expr.args.size() < expected_count : expected_count != expr.args.size()) {
        this->report(expr.range, sema_argument_count_message(name));
    }
    const base::usize count = expr.args.size() < expected_count ? expr.args.size() : expected_count;
    for (base::usize i = 0; i < count; ++i) {
        const TypeHandle expected = param_types[i + receiver_count];
        const TypeHandle actual = this->analyze_expr(expr.args[i], expected);
        if (!this->can_assign(expected, actual, expr.args[i])) {
            this->report(call_expr_range_or(this->module_, expr.args[i], expr.range), sema_argument_type_message(name));
        }
        if (this->checked_.types.contains_array(expected)) {
            this->report(call_expr_range_or(this->module_, expr.args[i], expr.range), std::string(SEMA_ARGUMENT_ARRAY_UNSUPPORTED));
        }
    }
    if (is_variadic) {
        for (base::usize i = count; i < expr.args.size(); ++i) {
            const TypeHandle actual = this->analyze_expr(expr.args[i]);
            if (!is_valid(actual)) {
                this->report(
                    call_expr_range_or(this->module_, expr.args[i], expr.range),
                    sema_variadic_argument_type_infer_message(name)
                );
            } else if (this->is_array_containing_value_type(actual)) {
                this->report(call_expr_range_or(this->module_, expr.args[i], expr.range), std::string(SEMA_ARGUMENT_ARRAY_UNSUPPORTED));
            }
        }
    }
}

} // namespace aurex::sema
