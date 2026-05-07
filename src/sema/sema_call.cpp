#include "aurex/sema/sema.hpp"

namespace aurex::sema {

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
        return invalid_type_handle;
    }

    const syntax::ModuleId scope_module = resolve_import_alias(
        object.scope_name,
        object.scope_range,
        report_unknown
    );
    if (!syntax::is_valid(scope_module)) {
        return invalid_type_handle;
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
    return invalid_type_handle;
}

TypeHandle SemanticAnalyzer::analyze_call_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    if (!syntax::is_valid(expr.callee) ||
        (module_.exprs[expr.callee.value].kind != syntax::ExprKind::name &&
         module_.exprs[expr.callee.value].kind != syntax::ExprKind::field)) {
        report(expr.range, "callee must be a function name");
        return record_expr_type(expr_id, invalid_type_handle);
    }
    const syntax::ExprNode& callee = module_.exprs[expr.callee.value];
    const std::string name = callee.kind == syntax::ExprKind::field
        ? std::string(callee.field_name)
        : std::string(callee.text);
    const base::SourceRange callee_range = callee.range;
    bool generic_enum_constructor = false;
    if (callee.kind == syntax::ExprKind::field &&
        syntax::is_valid(callee.object) &&
        callee.object.value < module_.exprs.size() &&
        module_.exprs[callee.object.value].kind == syntax::ExprKind::name) {
        const syntax::ExprNode& enum_name = module_.exprs[callee.object.value];
        const GenericEnumTemplateInfo* enum_template = nullptr;
        if (enum_name.scope_name.empty()) {
            enum_template = find_generic_enum_template_in_visible_modules(enum_name.text, callee.range, false);
        } else {
            const syntax::ModuleId scope_module = resolve_import_alias(enum_name.scope_name, enum_name.scope_range, false);
            if (syntax::is_valid(scope_module)) {
                enum_template = find_generic_enum_template_in_module(scope_module, enum_name.text, callee.range, false);
            }
        }
        if (enum_template != nullptr &&
            syntax::is_valid(enum_template->item) &&
            enum_template->item.value < module_.items.size()) {
            const syntax::ItemNode& enum_item = module_.items[enum_template->item.value];
            for (const syntax::EnumCaseDecl& enum_case : enum_item.enum_cases) {
                if (enum_case.name == callee.field_name) {
                    generic_enum_constructor = true;
                    break;
                }
            }
        }
    }
    if (const EnumCaseInfo* enum_case = find_enum_constructor(expr.callee, false); enum_case != nullptr) {
        if (!is_valid(enum_case->payload_type)) {
            report(expr.range, "enum case constructor requires a payload case: " + enum_case->name);
        }
        if (expr.args.size() != 1) {
            report(expr.range, "enum payload constructor requires exactly one argument: " + enum_case->name);
        }
        TypeHandle actual = invalid_type_handle;
        if (!expr.args.empty()) {
            actual = analyze_expr(expr.args.front(), enum_case->payload_type);
            if (!can_assign(enum_case->payload_type, actual, expr.args.front())) {
                report(module_.exprs[expr.args.front().value].range, "enum payload constructor argument type mismatch");
            }
            if (is_copy_forbidden_value(enum_case->payload_type)) {
                report(module_.exprs[expr.args.front().value].range, "non-copyable array storage cannot be used as enum payload");
            }
        }
        record_expr_c_name(expr.callee, enum_case->c_name);
        return record_expr_type(expr_id, enum_case->type);
    }
    if (generic_enum_constructor) {
        std::vector<TypeHandle> arg_types;
        arg_types.reserve(expr.args.size());
        for (syntax::ExprId arg : expr.args) {
            arg_types.push_back(analyze_expr(arg));
        }
        const EnumCaseInfo* enum_case = instantiate_generic_enum_constructor(expr.callee, arg_types, expected_type, true);
        if (enum_case == nullptr) {
            return record_expr_type(expr_id, invalid_type_handle);
        }
        if (!is_valid(enum_case->payload_type)) {
            report(expr.range, "enum case constructor requires a payload case: " + enum_case->name);
        }
        if (expr.args.size() != 1) {
            report(expr.range, "enum payload constructor requires exactly one argument: " + enum_case->name);
        }
        if (!arg_types.empty()) {
            if (!can_assign(enum_case->payload_type, arg_types.front(), expr.args.front())) {
                report(module_.exprs[expr.args.front().value].range, "enum payload constructor argument type mismatch");
            }
            if (is_copy_forbidden_value(enum_case->payload_type)) {
                report(module_.exprs[expr.args.front().value].range, "non-copyable array storage cannot be used as enum payload");
            }
        }
        record_expr_c_name(expr.callee, enum_case->c_name);
        return record_expr_type(expr_id, enum_case->type);
    }
    if (callee.kind == syntax::ExprKind::field) {
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
        TypeHandle receiver_type = invalid_type_handle;
        bool has_receiver = false;
        bool receiver_valid = true;
        std::vector<TypeHandle> call_arg_types;
        bool call_arg_types_ready = false;
        const auto collect_call_arg_types = [&]() -> const std::vector<TypeHandle>& {
            if (!call_arg_types_ready) {
                call_arg_types.reserve(expr.args.size());
                for (syntax::ExprId arg : expr.args) {
                    call_arg_types.push_back(analyze_expr(arg));
                }
                call_arg_types_ready = true;
            }
            return call_arg_types;
        };
        if (syntax::is_valid(callee.object) &&
            callee.object.value < module_.exprs.size() &&
            module_.exprs[callee.object.value].kind == syntax::ExprKind::name) {
            const syntax::ExprNode& object = module_.exprs[callee.object.value];
            const TypeHandle associated_owner = resolve_associated_type_owner(object, false);
            if (is_valid(associated_owner)) {
                signature = find_method_in_visible_modules(associated_owner, callee.field_name, callee.range, false, false);
                if (signature != nullptr && !callee.type_args.empty()) {
                    report(callee.range, "type arguments require a generic method: " + checked_.types.display_name(associated_owner) + "." + name);
                    return record_expr_type(expr_id, invalid_type_handle);
                }
                if (signature == nullptr) {
                    const std::vector<TypeHandle>& method_arg_types = collect_call_arg_types();
                    if (const GenericFunctionInstanceInfo* instance =
                            find_generic_method_in_visible_modules(
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
                    const auto diagnostics_before_generic_lookup = diagnostics_.diagnostics().size();
                    if (const GenericFunctionInstanceInfo* instance =
                            find_generic_method_in_visible_modules(
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
                        diagnostics_.diagnostics().size() != diagnostics_before_generic_lookup) {
                        return record_expr_type(expr_id, invalid_type_handle);
                    }
                }
                if (signature == nullptr) {
                    signature = find_method_in_visible_modules(associated_owner, callee.field_name, callee.range, false);
                    return record_expr_type(expr_id, invalid_type_handle);
                }
                if (signature->has_self_param) {
                    report(callee.range, "method requires a receiver: " + checked_.types.display_name(associated_owner) + "." + name);
                    receiver_valid = false;
                }
            }
        }
        if (signature == nullptr) {
            has_receiver = true;
            receiver_type = analyze_expr(callee.object);
            TypeHandle owner_type = receiver_type;
            if (checked_.types.is_pointer(owner_type)) {
                owner_type = checked_.types.get(owner_type).pointee;
            }
            signature = find_method_in_visible_modules(owner_type, callee.field_name, callee.range, true, false);
            if (signature != nullptr && !callee.type_args.empty()) {
                report(callee.range, "type arguments require a generic method: " + checked_.types.display_name(owner_type) + "." + name);
                return record_expr_type(expr_id, invalid_type_handle);
            }
            if (signature == nullptr) {
                std::vector<TypeHandle> method_arg_types;
                method_arg_types.reserve(expr.args.size() + 1);
                method_arg_types.push_back(receiver_type);
                const std::vector<TypeHandle>& plain_arg_types = collect_call_arg_types();
                method_arg_types.insert(method_arg_types.end(), plain_arg_types.begin(), plain_arg_types.end());
                if (const GenericFunctionInstanceInfo* instance =
                        find_generic_method_in_visible_modules(
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
                method_arg_types.reserve(expr.args.size() + 1);
                method_arg_types.push_back(receiver_type);
                const std::vector<TypeHandle>& plain_arg_types = collect_call_arg_types();
                method_arg_types.insert(method_arg_types.end(), plain_arg_types.begin(), plain_arg_types.end());
                const auto diagnostics_before_generic_lookup = diagnostics_.diagnostics().size();
                if (const GenericFunctionInstanceInfo* instance =
                        find_generic_method_in_visible_modules(
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
                    diagnostics_.diagnostics().size() != diagnostics_before_generic_lookup) {
                    return record_expr_type(expr_id, invalid_type_handle);
                }
            }
            if (signature == nullptr) {
                signature = find_method_in_visible_modules(owner_type, callee.field_name, callee.range, true);
                return record_expr_type(expr_id, invalid_type_handle);
            }
            receiver_valid = method_receiver_matches(*signature, receiver_type, callee.object);
        }
        ensure_function_return_known(*signature, callee.range);
        record_expr_c_name(expr.callee, signature->c_name);
        const base::usize receiver_count = has_receiver ? 1 : 0;
        const base::usize expected_count =
            signature->param_types.size() >= receiver_count
                ? signature->param_types.size() - receiver_count
                : 0;
        if (!receiver_valid || signature->param_types.size() < receiver_count) {
            return record_expr_type(expr_id, invalid_type_handle);
        }
        if (signature->is_variadic ? expr.args.size() < expected_count : expected_count != expr.args.size()) {
            report(expr.range, "argument count mismatch in call to " + name);
        }
        const base::usize count = expr.args.size() < expected_count ? expr.args.size() : expected_count;
        for (base::usize i = 0; i < count; ++i) {
            const TypeHandle expected = signature->param_types[i + receiver_count];
            const TypeHandle actual = analyze_expr(expr.args[i], expected);
            if (!can_assign(expected, actual, expr.args[i])) {
                report(module_.exprs[expr.args[i].value].range, "argument type mismatch in call to " + name);
            }
            if (is_copy_forbidden_value(expected)) {
                report(module_.exprs[expr.args[i].value].range, "non-copyable array storage cannot be passed by value");
            }
        }
        if (signature->is_variadic) {
            for (base::usize i = count; i < expr.args.size(); ++i) {
                const TypeHandle actual = analyze_expr(expr.args[i]);
                if (!is_valid(actual)) {
                    report(module_.exprs[expr.args[i].value].range, "variadic argument type cannot be inferred in call to " + name);
                } else if (is_copy_forbidden_value(actual)) {
                    report(module_.exprs[expr.args[i].value].range, "non-copyable array storage cannot be passed by value");
                }
            }
        }
        return record_expr_type(expr_id, signature->return_type);
    }
    const FunctionSignature* signature = nullptr;
    const GenericFunctionTemplateInfo* generic_function = nullptr;
    const bool qualified_callee = callee.kind == syntax::ExprKind::name && !callee.scope_name.empty();
    syntax::ModuleId callee_module = syntax::invalid_module_id;
    if (qualified_callee) {
        callee_module = resolve_import_alias(callee.scope_name, callee.scope_range);
        if (!syntax::is_valid(callee_module)) {
            return record_expr_type(expr_id, invalid_type_handle);
        }
    }
    if (callee.type_args.empty()) {
        if (qualified_callee) {
            signature = find_function_in_module(callee_module, name, callee_range, false);
            if (signature == nullptr) {
                generic_function = find_generic_function_template_in_module(callee_module, name, callee_range, false);
                if (generic_function == nullptr) {
                    signature = find_function_in_module(callee_module, name, callee_range, true);
                }
            }
        } else {
            signature = find_function_in_visible_modules(name, callee_range, false);
        }
        if (signature == nullptr && callee.kind == syntax::ExprKind::name && !qualified_callee) {
            const auto diagnostics_before_generic_lookup = diagnostics_.diagnostics().size();
            generic_function = find_generic_function_template_in_visible_modules(name, callee_range, false);
            const bool generic_lookup_reported =
                diagnostics_.diagnostics().size() != diagnostics_before_generic_lookup;
            if (generic_function == nullptr && !generic_lookup_reported) {
                signature = find_function_in_visible_modules(name, callee_range, true);
            }
        }
    } else if (callee.kind == syntax::ExprKind::name) {
        generic_function = qualified_callee
            ? find_generic_function_template_in_module(callee_module, name, callee_range, false)
            : find_generic_function_template_in_visible_modules(name, callee_range, false);
    }
    if (signature == nullptr && callee.kind == syntax::ExprKind::name && (!callee.type_args.empty() || generic_function != nullptr)) {
        if (generic_function == nullptr) {
            if (qualified_callee) {
                if (find_function_in_module(callee_module, name, callee_range, false) != nullptr) {
                    report(callee_range, "type arguments require a generic function: " + std::string(callee.scope_name) + "::" + name);
                } else {
                    static_cast<void>(find_generic_function_template_in_module(callee_module, name, callee_range, true));
                }
            } else {
                if (find_function_in_visible_modules(name, callee_range, false) != nullptr) {
                    report(callee_range, "type arguments require a generic function: " + name);
                } else {
                    static_cast<void>(find_generic_function_template_in_visible_modules(name, callee_range, true));
                }
            }
            return record_expr_type(expr_id, invalid_type_handle);
        }

        const GenericFunctionInstanceInfo* instance = nullptr;
        if (!callee.type_args.empty()) {
            instance = instantiate_generic_function_from_syntax(*generic_function, callee.type_args, callee_range);
            if (instance == nullptr) {
                return record_expr_type(expr_id, invalid_type_handle);
            }
        } else {
            std::vector<TypeHandle> arg_types;
            arg_types.reserve(expr.args.size());
            for (syntax::ExprId arg : expr.args) {
                arg_types.push_back(analyze_expr(arg));
            }
            std::vector<TypeHandle> inferred(generic_function->params.size(), invalid_type_handle);
            static_cast<void>(infer_generic_function_args(*generic_function, arg_types, expected_type, inferred, callee_range));
            for (TypeHandle arg : inferred) {
                if (!is_valid(arg)) {
                    report(callee_range, "generic function requires explicit type arguments: " + generic_function->name);
                    return record_expr_type(expr_id, invalid_type_handle);
                }
            }
            instance = instantiate_generic_function(*generic_function, inferred, callee_range);
            if (instance == nullptr) {
                return record_expr_type(expr_id, invalid_type_handle);
            }
        }

        record_expr_c_name(expr.callee, instance->c_name);
        if (instance->param_types.size() != expr.args.size()) {
            report(expr.range, "argument count mismatch in call to " + name);
        }
        const base::usize count = expr.args.size() < instance->param_types.size() ? expr.args.size() : instance->param_types.size();
        for (base::usize i = 0; i < count; ++i) {
            const TypeHandle actual = analyze_expr(expr.args[i], instance->param_types[i]);
            if (!can_assign(instance->param_types[i], actual, expr.args[i])) {
                report(module_.exprs[expr.args[i].value].range, "argument type mismatch in call to " + name);
            }
            if (is_copy_forbidden_value(instance->param_types[i])) {
                report(module_.exprs[expr.args[i].value].range, "non-copyable array storage cannot be passed by value");
            }
        }
        return record_expr_type(expr_id, instance->return_type);
    }
    if (signature == nullptr) {
        return record_expr_type(expr_id, invalid_type_handle);
    }
    if (!callee.type_args.empty()) {
        report(callee_range, "type arguments require a generic function: " + name);
        return record_expr_type(expr_id, invalid_type_handle);
    }
    ensure_function_return_known(*signature, module_.exprs[expr.callee.value].range);
    record_expr_c_name(expr.callee, signature->c_name);
    if (signature->is_variadic ? expr.args.size() < signature->param_types.size() : signature->param_types.size() != expr.args.size()) {
        report(expr.range, "argument count mismatch in call to " + name);
    }
    const base::usize count = expr.args.size() < signature->param_types.size() ? expr.args.size() : signature->param_types.size();
    for (base::usize i = 0; i < count; ++i) {
        const TypeHandle actual = analyze_expr(expr.args[i], signature->param_types[i]);
        if (!can_assign(signature->param_types[i], actual, expr.args[i])) {
            report(module_.exprs[expr.args[i].value].range, "argument type mismatch in call to " + name);
        }
        if (is_copy_forbidden_value(signature->param_types[i])) {
            report(module_.exprs[expr.args[i].value].range, "non-copyable array storage cannot be passed by value");
        }
    }
    if (signature->is_variadic) {
        for (base::usize i = count; i < expr.args.size(); ++i) {
            const TypeHandle actual = analyze_expr(expr.args[i]);
            if (!is_valid(actual)) {
                report(module_.exprs[expr.args[i].value].range, "variadic argument type cannot be inferred in call to " + name);
            } else if (is_copy_forbidden_value(actual)) {
                report(module_.exprs[expr.args[i].value].range, "non-copyable array storage cannot be passed by value");
            }
        }
    }
    return record_expr_type(expr_id, signature->return_type);
}

} // namespace aurex::sema
