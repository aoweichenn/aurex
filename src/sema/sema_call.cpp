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
        if (object.type_args.empty()) {
            return find_type_in_visible_modules(object.text, object.range, false, report_unknown);
        }
        if (const GenericStructTemplateInfo* struct_template =
                find_generic_struct_template_in_visible_modules(object.text, object.range, report_unknown);
            struct_template != nullptr) {
            return instantiate_generic_struct_from_syntax(*struct_template, object.type_args, object.range, false);
        }
        if (const GenericEnumTemplateInfo* enum_template =
                find_generic_enum_template_in_visible_modules(object.text, object.range, report_unknown);
            enum_template != nullptr) {
            return instantiate_generic_enum_from_syntax(*enum_template, object.type_args, object.range, false);
        }
        return INVALID_TYPE_HANDLE;
    }

    const syntax::ModuleId scope_module = resolve_import_alias(
        object.scope_name,
        object.scope_range,
        report_unknown
    );
    if (!syntax::is_valid(scope_module)) {
        return INVALID_TYPE_HANDLE;
    }
    if (object.type_args.empty()) {
        return find_type_in_module(scope_module, object.text, object.range, false, report_unknown);
    }
    if (const GenericStructTemplateInfo* struct_template =
            find_generic_struct_template_in_module(scope_module, object.text, object.range, report_unknown);
        struct_template != nullptr) {
        return instantiate_generic_struct_from_syntax(*struct_template, object.type_args, object.range, false);
    }
    if (const GenericEnumTemplateInfo* enum_template =
            find_generic_enum_template_in_module(scope_module, object.text, object.range, report_unknown);
        enum_template != nullptr) {
        return instantiate_generic_enum_from_syntax(*enum_template, object.type_args, object.range, false);
    }
    return INVALID_TYPE_HANDLE;
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
    if (this->is_generic_enum_constructor_call(callee)) {
        return this->analyze_generic_enum_constructor_call(expr_id, expr, expected_type);
    }
    if (callee.kind == syntax::ExprKind::field) {
        return this->analyze_field_call_expr(expr_id, expr, callee, name, expected_type);
    }
    return this->analyze_function_call_expr(expr_id, expr, callee, name, expected_type);
}

bool SemanticAnalyzer::is_generic_enum_constructor_call(const syntax::ExprNode& callee) {
    if (callee.kind != syntax::ExprKind::field ||
        !syntax::is_valid(callee.object) ||
        callee.object.value >= this->module_.exprs.size() ||
        this->module_.exprs[callee.object.value].kind != syntax::ExprKind::name) {
        return false;
    }

    const syntax::ExprNode& enum_name = this->module_.exprs[callee.object.value];
    const GenericEnumTemplateInfo* enum_template = nullptr;
    if (enum_name.scope_name.empty()) {
        enum_template = this->find_generic_enum_template_in_visible_modules(enum_name.text, callee.range, false);
    } else {
        const syntax::ModuleId scope_module = this->resolve_import_alias(enum_name.scope_name, enum_name.scope_range, false);
        if (syntax::is_valid(scope_module)) {
            enum_template = this->find_generic_enum_template_in_module(scope_module, enum_name.text, callee.range, false);
        }
    }
    if (enum_template == nullptr ||
        !syntax::is_valid(enum_template->item) ||
        enum_template->item.value >= this->module_.items.size()) {
        return false;
    }

    const syntax::ItemNode& enum_item = this->module_.items[enum_template->item.value];
    for (const syntax::EnumCaseDecl& enum_case : enum_item.enum_cases) {
        if (enum_case.name == callee.field_name) {
            return true;
        }
    }
    return false;
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

TypeHandle SemanticAnalyzer::analyze_generic_enum_constructor_call(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    std::vector<TypeHandle> arg_types;
    arg_types.reserve(expr.args.size());
    for (const syntax::ExprId arg : expr.args) {
        arg_types.push_back(this->analyze_expr(arg));
    }
    const EnumCaseInfo* enum_case = this->instantiate_generic_enum_constructor(expr.callee, arg_types, expected_type, true);
    if (enum_case == nullptr) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (!is_valid(enum_case->payload_type)) {
        this->report(expr.range, "enum case constructor requires a payload case: " + enum_case->name);
    }
    if (expr.args.size() != SEMA_ENUM_PAYLOAD_ARGUMENT_COUNT) {
        this->report(expr.range, "enum payload constructor requires exactly one argument: " + enum_case->name);
    }
    if (!arg_types.empty()) {
        if (!this->can_assign(enum_case->payload_type, arg_types.front(), expr.args.front())) {
            this->report(this->module_.exprs[expr.args.front().value].range, "enum payload constructor argument type mismatch");
        }
        if (this->checked_.types.contains_array(enum_case->payload_type)) {
            this->report(this->module_.exprs[expr.args.front().value].range, "array-containing type cannot be used as enum payload");
        }
    }
    this->record_expr_c_name(expr.callee, enum_case->c_name);
    return this->record_expr_type(expr_id, enum_case->type);
}

TypeHandle SemanticAnalyzer::analyze_field_call_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const syntax::ExprNode& callee,
    const std::string_view name,
    const TypeHandle expected_type
) {
    const FunctionSignature* signature = nullptr;
    FunctionSignature generic_signature;
    const auto use_generic_instance = [&](const GenericFunctionInstanceInfo& instance) {
        generic_signature.name = std::string(name);
        generic_signature.c_name = instance.c_name;
        generic_signature.module = instance.module;
        generic_signature.method_owner_type = instance.method_owner_type;
        generic_signature.return_type = instance.return_type;
        generic_signature.param_types = instance.param_types;
        generic_signature.range = callee.range;
        generic_signature.has_definition = true;
        generic_signature.visibility = instance.visibility;
        generic_signature.is_method = instance.is_method;
        generic_signature.has_self_param = instance.has_self_param;
        signature = &generic_signature;
    };
    TypeHandle receiver_type = INVALID_TYPE_HANDLE;
    bool has_receiver = false;
    bool receiver_valid = true;
    std::vector<TypeHandle> call_arg_types;
    bool call_arg_types_ready = false;
    const auto collect_call_arg_types = [&]() -> const std::vector<TypeHandle>& {
        if (!call_arg_types_ready) {
            call_arg_types.reserve(expr.args.size());
            for (const syntax::ExprId arg : expr.args) {
                call_arg_types.push_back(this->analyze_expr(arg));
            }
            call_arg_types_ready = true;
        }
        return call_arg_types;
    };

    if (syntax::is_valid(callee.object) &&
        callee.object.value < this->module_.exprs.size() &&
        this->module_.exprs[callee.object.value].kind == syntax::ExprKind::name) {
        const syntax::ExprNode& object = this->module_.exprs[callee.object.value];
        const TypeHandle associated_owner = this->resolve_associated_type_owner(object, false);
        if (is_valid(associated_owner)) {
            signature = this->find_method_in_visible_modules(associated_owner, callee.field_name, callee.range, false, false);
            if (signature != nullptr && !callee.type_args.empty()) {
                this->report(callee.range, "type arguments require a generic method: " + this->checked_.types.display_name(associated_owner) + "." + std::string(name));
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
            if (signature == nullptr) {
                const std::vector<TypeHandle>& method_arg_types = collect_call_arg_types();
                if (const GenericFunctionInstanceInfo* instance =
                        this->find_generic_method_in_visible_modules(
                            associated_owner,
                            callee.field_name,
                            callee.range,
                            false,
                            &method_arg_types,
                            expected_type,
                            &callee.type_args,
                            false
                        );
                    instance != nullptr) {
                    use_generic_instance(*instance);
                }
            }
            if (signature == nullptr) {
                const std::vector<TypeHandle>& method_arg_types = collect_call_arg_types();
                const auto diagnostics_before_generic_lookup = this->diagnostics_.diagnostics().size();
                if (const GenericFunctionInstanceInfo* instance =
                        this->find_generic_method_in_visible_modules(
                            associated_owner,
                            callee.field_name,
                            callee.range,
                            false,
                            &method_arg_types,
                            expected_type,
                            &callee.type_args,
                            true
                        );
                    instance != nullptr) {
                    use_generic_instance(*instance);
                }
                if (signature == nullptr &&
                    this->diagnostics_.diagnostics().size() != diagnostics_before_generic_lookup) {
                    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
                }
            }
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
        if (signature != nullptr && !callee.type_args.empty()) {
            this->report(callee.range, "type arguments require a generic method: " + this->checked_.types.display_name(owner_type) + "." + std::string(name));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        if (signature == nullptr) {
            std::vector<TypeHandle> method_arg_types;
            method_arg_types.reserve(expr.args.size() + SEMA_RECEIVER_ARGUMENT_COUNT);
            method_arg_types.push_back(receiver_type);
            const std::vector<TypeHandle>& plain_arg_types = collect_call_arg_types();
            method_arg_types.insert(method_arg_types.end(), plain_arg_types.begin(), plain_arg_types.end());
            if (const GenericFunctionInstanceInfo* instance =
                    this->find_generic_method_in_visible_modules(
                        owner_type,
                        callee.field_name,
                        callee.range,
                        true,
                        &method_arg_types,
                        expected_type,
                        &callee.type_args,
                        false
                    );
                instance != nullptr) {
                use_generic_instance(*instance);
            }
        }
        if (signature == nullptr) {
            std::vector<TypeHandle> method_arg_types;
            method_arg_types.reserve(expr.args.size() + SEMA_RECEIVER_ARGUMENT_COUNT);
            method_arg_types.push_back(receiver_type);
            const std::vector<TypeHandle>& plain_arg_types = collect_call_arg_types();
            method_arg_types.insert(method_arg_types.end(), plain_arg_types.begin(), plain_arg_types.end());
            const auto diagnostics_before_generic_lookup = this->diagnostics_.diagnostics().size();
            if (const GenericFunctionInstanceInfo* instance =
                    this->find_generic_method_in_visible_modules(
                        owner_type,
                        callee.field_name,
                        callee.range,
                        true,
                        &method_arg_types,
                        expected_type,
                        &callee.type_args,
                        true
                    );
                instance != nullptr) {
                use_generic_instance(*instance);
            }
            if (signature == nullptr &&
                this->diagnostics_.diagnostics().size() != diagnostics_before_generic_lookup) {
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
        }
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
    const TypeHandle expected_type
) {
    const base::SourceRange callee_range = callee.range;
    const FunctionSignature* signature = nullptr;
    const GenericFunctionTemplateInfo* generic_function = nullptr;
    const bool qualified_callee = callee.kind == syntax::ExprKind::name && !callee.scope_name.empty();
    syntax::ModuleId callee_module = syntax::INVALID_MODULE_ID;
    if (qualified_callee) {
        callee_module = this->resolve_import_alias(callee.scope_name, callee.scope_range);
        if (!syntax::is_valid(callee_module)) {
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
    }
    if (callee.type_args.empty()) {
        if (qualified_callee) {
            signature = this->find_function_in_module(callee_module, name, callee_range, false);
            if (signature == nullptr) {
                generic_function = this->find_generic_function_template_in_module(callee_module, name, callee_range, false);
                if (generic_function == nullptr) {
                    signature = this->find_function_in_module(callee_module, name, callee_range, true);
                }
            }
        } else {
            signature = this->find_function_in_visible_modules(name, callee_range, false);
        }
        if (signature == nullptr && callee.kind == syntax::ExprKind::name && !qualified_callee) {
            const auto diagnostics_before_generic_lookup = this->diagnostics_.diagnostics().size();
            generic_function = this->find_generic_function_template_in_visible_modules(name, callee_range, false);
            const bool generic_lookup_reported =
                this->diagnostics_.diagnostics().size() != diagnostics_before_generic_lookup;
            if (generic_function == nullptr && !generic_lookup_reported) {
                signature = this->find_function_in_visible_modules(name, callee_range, true);
            }
        }
    } else if (callee.kind == syntax::ExprKind::name) {
        generic_function = qualified_callee
            ? this->find_generic_function_template_in_module(callee_module, name, callee_range, false)
            : this->find_generic_function_template_in_visible_modules(name, callee_range, false);
    }
    if (signature == nullptr && callee.kind == syntax::ExprKind::name && (!callee.type_args.empty() || generic_function != nullptr)) {
        if (generic_function == nullptr) {
            if (qualified_callee) {
                if (this->find_function_in_module(callee_module, name, callee_range, false) != nullptr) {
                    this->report(callee_range, "type arguments require a generic function: " + std::string(callee.scope_name) + "::" + std::string(name));
                } else {
                    static_cast<void>(this->find_generic_function_template_in_module(callee_module, name, callee_range, true));
                }
            } else {
                if (this->find_function_in_visible_modules(name, callee_range, false) != nullptr) {
                    this->report(callee_range, "type arguments require a generic function: " + std::string(name));
                } else {
                    static_cast<void>(this->find_generic_function_template_in_visible_modules(name, callee_range, true));
                }
            }
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }

        const GenericFunctionInstanceInfo* instance = nullptr;
        if (!callee.type_args.empty()) {
            instance = this->instantiate_generic_function_from_syntax(*generic_function, callee.type_args, callee_range);
            if (instance == nullptr) {
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
        } else {
            std::vector<TypeHandle> arg_types;
            arg_types.reserve(expr.args.size());
            for (const syntax::ExprId arg : expr.args) {
                arg_types.push_back(this->analyze_expr(arg));
            }
            std::vector<TypeHandle> inferred(generic_function->params.size(), INVALID_TYPE_HANDLE);
            static_cast<void>(this->infer_generic_function_args(*generic_function, arg_types, expected_type, inferred, callee_range));
            for (const TypeHandle arg : inferred) {
                if (!is_valid(arg)) {
                    this->report(callee_range, "generic function requires explicit type arguments: " + generic_function->name);
                    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
                }
            }
            instance = this->instantiate_generic_function(*generic_function, inferred, callee_range);
            if (instance == nullptr) {
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
        }

        this->record_expr_c_name(expr.callee, instance->c_name);
        this->validate_call_arguments(expr, name, instance->param_types, 0, false);
        return this->record_expr_type(expr_id, instance->return_type);
    }
    if (signature == nullptr) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (!callee.type_args.empty()) {
        this->report(callee_range, "type arguments require a generic function: " + std::string(name));
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
