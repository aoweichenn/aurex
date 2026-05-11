#include <aurex/sema/sema.hpp>

namespace aurex::sema {

namespace {

constexpr base::usize SEMA_ENUM_PAYLOAD_ARGUMENT_COUNT = 1;
constexpr base::usize SEMA_RECEIVER_ARGUMENT_COUNT = 1;

} // namespace

TypeHandle SemanticAnalyzer::resolve_associated_type_owner(
    const syntax::ExprNode& object,
    const bool report_unknown
) {
    if (object.scope_name.empty()) {
        return find_type_in_visible_modules(object.text, object.range, false, report_unknown);
    }

    const syntax::ModuleId scope_module = resolve_import_alias(
        object.scope_name,
        object.scope_range,
        report_unknown
    );
    if (!syntax::is_valid(scope_module)) {
        return INVALID_TYPE_HANDLE;
    }
    return find_type_in_module(scope_module, object.text, object.range, false, report_unknown);
}

TypeHandle SemanticAnalyzer::analyze_call_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    if (!syntax::is_valid(expr.callee) ||
        (this->module_.exprs[expr.callee.value].kind != syntax::ExprKind::name &&
         this->module_.exprs[expr.callee.value].kind != syntax::ExprKind::field)) {
        this->report(expr.range, "callee must be a function name");
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const syntax::ExprNode& callee = this->module_.exprs[expr.callee.value];
    const std::string name = callee.kind == syntax::ExprKind::field
        ? std::string(callee.field_name)
        : std::string(callee.text);

    if (const EnumCaseInfo* enum_case = this->find_enum_constructor(expr.callee, false); enum_case != nullptr) {
        return this->analyze_enum_constructor_call(expr_id, expr, *enum_case);
    }
    if (callee.kind == syntax::ExprKind::field) {
        return this->analyze_field_call_expr(expr_id, expr, callee, name, expected_type);
    }
    return this->analyze_function_call_expr(expr_id, expr, callee, name, expected_type);
}

TypeHandle SemanticAnalyzer::analyze_enum_constructor_call(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const EnumCaseInfo& enum_case
) {
    if (!is_valid(enum_case.payload_type)) {
        this->report(expr.range, "enum case constructor requires a payload case: " + enum_case.name);
    }
    if (expr.args.size() != SEMA_ENUM_PAYLOAD_ARGUMENT_COUNT) {
        this->report(expr.range, "enum payload constructor requires exactly one argument: " + enum_case.name);
    }
    TypeHandle actual = INVALID_TYPE_HANDLE;
    if (!expr.args.empty()) {
        actual = this->analyze_expr(expr.args.front(), enum_case.payload_type);
        if (!this->can_assign(enum_case.payload_type, actual, expr.args.front())) {
            this->report(this->module_.exprs[expr.args.front().value].range, "enum payload constructor argument type mismatch");
        }
        if (this->checked_.types.contains_array(enum_case.payload_type)) {
            this->report(this->module_.exprs[expr.args.front().value].range, "array-containing type cannot be used as enum payload");
        }
    }
    this->record_expr_c_name(expr.callee, enum_case.c_name);
    return this->record_expr_type(expr_id, enum_case.type);
}

TypeHandle SemanticAnalyzer::analyze_field_call_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const syntax::ExprNode& callee,
    const std::string_view name,
    const TypeHandle
) {
    const FunctionSignature* signature = nullptr;
    TypeHandle receiver_type = INVALID_TYPE_HANDLE;
    bool has_receiver = false;
    bool receiver_valid = true;

    if (syntax::is_valid(callee.object) &&
        callee.object.value < this->module_.exprs.size() &&
        this->module_.exprs[callee.object.value].kind == syntax::ExprKind::name) {
        const syntax::ExprNode& object = this->module_.exprs[callee.object.value];
        const TypeHandle associated_owner = this->resolve_associated_type_owner(object, false);
        if (is_valid(associated_owner)) {
            signature = this->find_method_in_visible_modules(associated_owner, callee.field_name, callee.range, false, false);
            if (signature == nullptr) {
                signature = this->find_method_in_visible_modules(associated_owner, callee.field_name, callee.range, false);
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
            if (signature->has_self_param) {
                this->report(callee.range, "method requires a receiver: " + this->checked_.types.display_name(associated_owner) + "." + std::string(name));
                receiver_valid = false;
            }
        }
    }
    if (signature == nullptr) {
        has_receiver = true;
        receiver_type = this->analyze_expr(callee.object);
        TypeHandle owner_type = receiver_type;
        if (this->checked_.types.is_pointer(owner_type)) {
            owner_type = this->checked_.types.get(owner_type).pointee;
        }
        signature = this->find_method_in_visible_modules(owner_type, callee.field_name, callee.range, true, false);
        if (signature == nullptr) {
            signature = this->find_method_in_visible_modules(owner_type, callee.field_name, callee.range, true);
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        receiver_valid = this->method_receiver_matches(*signature, receiver_type, callee.object);
    }
    this->ensure_function_return_known(*signature, callee.range);
    this->record_expr_c_name(expr.callee, signature->c_name);
    const base::usize receiver_count = has_receiver ? SEMA_RECEIVER_ARGUMENT_COUNT : 0;
    if (!receiver_valid || signature->param_types.size() < receiver_count) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    this->validate_call_arguments(expr, name, signature->param_types, receiver_count, signature->is_variadic);
    return this->record_expr_type(expr_id, signature->return_type);
}

TypeHandle SemanticAnalyzer::analyze_function_call_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const syntax::ExprNode& callee,
    const std::string_view name,
    const TypeHandle
) {
    const base::SourceRange callee_range = callee.range;
    const FunctionSignature* signature = nullptr;
    const bool qualified_callee = callee.kind == syntax::ExprKind::name && !callee.scope_name.empty();
    syntax::ModuleId callee_module = syntax::INVALID_MODULE_ID;
    if (qualified_callee) {
        callee_module = this->resolve_import_alias(callee.scope_name, callee.scope_range);
        if (!syntax::is_valid(callee_module)) {
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
    }
    if (!callee.type_args.empty()) {
        const GenericTemplateInfo* generic = qualified_callee
            ? this->find_generic_function_in_module(callee_module, name, callee_range, false)
            : this->find_generic_function_in_visible_modules(name, callee_range, false);
        if (generic == nullptr) {
            signature = qualified_callee
                ? this->find_function_in_module(callee_module, name, callee_range, false)
                : this->find_function_in_visible_modules(name, callee_range, false);
            if (signature != nullptr) {
                this->report(callee_range, "non-generic function cannot take type arguments: " + std::string(name));
            } else {
                static_cast<void>(qualified_callee
                    ? this->find_generic_function_in_module(callee_module, name, callee_range, true)
                    : this->find_generic_function_in_visible_modules(name, callee_range, true));
            }
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        std::vector<TypeHandle> args;
        args.reserve(callee.type_args.size());
        for (const syntax::TypeId arg : callee.type_args) {
            args.push_back(this->resolve_type(arg));
        }
        signature = this->instantiate_generic_function(*generic, args, callee_range);
        if (signature == nullptr) {
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        this->record_expr_c_name(expr.callee, signature->c_name);
        this->validate_call_arguments(expr, name, signature->param_types, 0, signature->is_variadic);
        return this->record_expr_type(expr_id, signature->return_type);
    }
    if (const GenericTemplateInfo* generic = qualified_callee
            ? this->find_generic_function_in_module(callee_module, name, callee_range, false)
            : this->find_generic_function_in_visible_modules(name, callee_range, false);
        generic != nullptr) {
        std::vector<TypeHandle> args;
        if (!this->infer_generic_arguments(*generic, expr, args)) {
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        signature = this->instantiate_generic_function(*generic, args, callee_range);
        if (signature == nullptr) {
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        this->record_expr_c_name(expr.callee, signature->c_name);
        this->validate_call_arguments(expr, name, signature->param_types, 0, signature->is_variadic);
        return this->record_expr_type(expr_id, signature->return_type);
    }
    if (qualified_callee) {
        signature = this->find_function_in_module(callee_module, name, callee_range, true);
    } else {
        signature = this->find_function_in_visible_modules(name, callee_range, true);
    }
    if (signature == nullptr) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    this->ensure_function_return_known(*signature, this->module_.exprs[expr.callee.value].range);
    this->record_expr_c_name(expr.callee, signature->c_name);
    this->validate_call_arguments(expr, name, signature->param_types, 0, signature->is_variadic);
    return this->record_expr_type(expr_id, signature->return_type);
}

void SemanticAnalyzer::validate_call_arguments(
    const syntax::ExprNode& expr,
    const std::string_view name,
    const std::vector<TypeHandle>& param_types,
    const base::usize receiver_count,
    const bool is_variadic
) {
    const base::usize expected_count =
        param_types.size() >= receiver_count
            ? param_types.size() - receiver_count
            : 0;
    if (is_variadic ? expr.args.size() < expected_count : expected_count != expr.args.size()) {
        this->report(expr.range, "argument count mismatch in call to " + std::string(name));
    }
    const base::usize count = expr.args.size() < expected_count ? expr.args.size() : expected_count;
    for (base::usize i = 0; i < count; ++i) {
        const TypeHandle expected = param_types[i + receiver_count];
        const TypeHandle actual = this->analyze_expr(expr.args[i], expected);
        if (!this->can_assign(expected, actual, expr.args[i])) {
            this->report(this->module_.exprs[expr.args[i].value].range, "argument type mismatch in call to " + std::string(name));
        }
        if (this->checked_.types.contains_array(expected)) {
            this->report(this->module_.exprs[expr.args[i].value].range, "array-containing type cannot be passed by value");
        }
    }
    if (is_variadic) {
        for (base::usize i = count; i < expr.args.size(); ++i) {
            const TypeHandle actual = this->analyze_expr(expr.args[i]);
            if (!is_valid(actual)) {
                this->report(this->module_.exprs[expr.args[i].value].range, "variadic argument type cannot be inferred in call to " + std::string(name));
            } else if (this->is_array_containing_value_type(actual)) {
                this->report(this->module_.exprs[expr.args[i].value].range, "array-containing type cannot be passed by value");
            }
        }
    }
}

} // namespace aurex::sema
