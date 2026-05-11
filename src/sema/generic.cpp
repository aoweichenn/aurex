#include <aurex/sema/sema.hpp>

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace aurex::sema {

namespace {

constexpr std::string_view SEMA_GENERIC_INSTANCE_SEPARATOR = "$";
constexpr std::string_view SEMA_GENERIC_ARG_LIST_OPEN = "[";
constexpr std::string_view SEMA_GENERIC_ARG_LIST_CLOSE = "]";
constexpr std::string_view SEMA_GENERIC_ARG_LIST_SEPARATOR = ",";
constexpr char SEMA_GENERIC_MANGLE_FALLBACK_CHAR = '_';

[[nodiscard]] GenericSideTables make_generic_side_tables(const syntax::AstModule& module) {
    GenericSideTables side_tables;
    side_tables.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    side_tables.expr_c_names.assign(module.exprs.size(), {});
    side_tables.pattern_c_names.assign(module.patterns.size(), {});
    side_tables.pattern_case_sets.assign(module.patterns.size(), {});
    side_tables.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    side_tables.stmt_local_types.assign(module.stmts.size(), INVALID_TYPE_HANDLE);
    return side_tables;
}

[[nodiscard]] std::string mangle_generic_fragment(std::string text) {
    for (char& c : text) {
        const bool alnum = (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9');
        if (!alnum) {
            c = SEMA_GENERIC_MANGLE_FALLBACK_CHAR;
        }
    }
    return text;
}

} // namespace

bool SemanticAnalyzer::has_generic_params(const syntax::ItemNode& item) const noexcept {
    return !item.generic_params.empty();
}

void SemanticAnalyzer::validate_generic_parameter_list(const syntax::ItemNode& item) {
    std::unordered_set<std::string> seen;
    for (const syntax::GenericParamDecl& param : item.generic_params) {
        if (!seen.insert(std::string(param.name)).second) {
            this->report(param.range, "duplicate generic parameter `" + std::string(param.name) + "`");
        }
    }
}

void SemanticAnalyzer::register_generic_template(
    const syntax::ItemNode& item,
    const syntax::ItemId item_id
) {
    this->validate_generic_parameter_list(item);
    const syntax::ModuleId owner = this->item_module(item);
    GenericTemplateInfo info;
    info.item = item_id;
    info.module = owner;
    info.name = std::string(item.name);
    info.key = this->module_key(owner, item.name);
    info.visibility = item.visibility;
    info.params.reserve(item.generic_params.size());
    for (const syntax::GenericParamDecl& param : item.generic_params) {
        info.params.push_back(std::string(param.name));
    }

    if (item.kind == syntax::ItemKind::struct_decl) {
        if (this->named_types_.contains(info.key) ||
            this->checked_.type_aliases.contains(info.key) ||
            this->generic_struct_templates_.contains(info.key)) {
            this->report(item.range, "duplicate type definition in module " + this->module_name(owner) + ": " + std::string(item.name));
            return;
        }
        this->generic_struct_templates_.emplace(info.key, std::move(info));
        return;
    }

    if (item.is_extern_c || item.is_export_c || item.is_prototype) {
        this->report(item.range, "generic functions cannot use C ABI or prototypes in M2");
    }
    if (syntax::is_valid(item.impl_type)) {
        this->report(item.range, "generic methods are not supported in M2");
    }
    if (this->checked_.functions.contains(info.key) || this->generic_function_templates_.contains(info.key)) {
        this->report(item.range, "duplicate function definition in module " + this->module_name(owner) + ": " + std::string(item.name));
        return;
    }
    this->generic_function_templates_.emplace(info.key, std::move(info));
}

std::string SemanticAnalyzer::generic_instance_suffix(const std::vector<TypeHandle>& args) const {
    std::string suffix;
    suffix += SEMA_GENERIC_ARG_LIST_OPEN;
    for (base::usize i = 0; i < args.size(); ++i) {
        if (i != 0) {
            suffix += SEMA_GENERIC_ARG_LIST_SEPARATOR;
        }
        suffix += this->checked_.types.display_name(args[i]);
    }
    suffix += SEMA_GENERIC_ARG_LIST_CLOSE;
    return suffix;
}

std::string SemanticAnalyzer::generic_struct_instance_key(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    return info.key + std::string(SEMA_GENERIC_INSTANCE_SEPARATOR) + this->generic_instance_suffix(args);
}

std::string SemanticAnalyzer::generic_function_instance_key(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    return info.key + std::string(SEMA_GENERIC_INSTANCE_SEPARATOR) + this->generic_instance_suffix(args);
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_struct_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (const auto found = this->generic_struct_templates_.find(this->module_key(this->current_module_, name));
        found != this->generic_struct_templates_.end()) {
        return &found->second;
    }

    const GenericTemplateInfo* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (syntax::ModuleId module : this->visible_modules(this->current_module_)) {
        if (module.value == this->current_module_.value) {
            continue;
        }
        const auto found = this->generic_struct_templates_.find(this->module_key(module, name));
        if (found == this->generic_struct_templates_.end()) {
            continue;
        }
        if (!this->can_access(module, found->second.visibility)) {
            continue;
        }
        if (imported_result != nullptr) {
            this->report(range, "ambiguous generic type name '" + std::string(name) + "' from modules " +
                this->module_name(result_module) + " and " + this->module_name(module));
            return nullptr;
        }
        imported_result = &found->second;
        result_module = module;
    }
    if (imported_result == nullptr && report_unknown) {
        this->report(range, "unknown generic type: " + std::string(name));
    }
    return imported_result;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_struct_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->report(range, "unknown generic type: " + std::string(name));
        }
        return nullptr;
    }
    const auto found = this->generic_struct_templates_.find(this->module_key(module, name));
    if (found == this->generic_struct_templates_.end()) {
        if (report_unknown) {
            this->report(range, "unknown generic type in module " + this->module_name(module) + ": " + std::string(name));
        }
        return nullptr;
    }
    if (!this->can_access(module, found->second.visibility)) {
        if (report_unknown) {
            this->report(range, "generic type is private: " + this->module_name(module) + "." + std::string(name));
        }
        return nullptr;
    }
    return &found->second;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_function_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (const auto found = this->generic_function_templates_.find(this->module_key(this->current_module_, name));
        found != this->generic_function_templates_.end()) {
        return &found->second;
    }

    const GenericTemplateInfo* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (syntax::ModuleId module : this->visible_modules(this->current_module_)) {
        if (module.value == this->current_module_.value) {
            continue;
        }
        const auto found = this->generic_function_templates_.find(this->module_key(module, name));
        if (found == this->generic_function_templates_.end()) {
            continue;
        }
        if (!this->can_access(module, found->second.visibility)) {
            continue;
        }
        if (imported_result != nullptr) {
            this->report(range, "ambiguous generic function name '" + std::string(name) + "' from modules " +
                this->module_name(result_module) + " and " + this->module_name(module));
            return nullptr;
        }
        imported_result = &found->second;
        result_module = module;
    }
    if (imported_result == nullptr && report_unknown) {
        this->report(range, "unknown generic function: " + std::string(name));
    }
    return imported_result;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_function_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->report(range, "unknown generic function: " + std::string(name));
        }
        return nullptr;
    }
    const auto found = this->generic_function_templates_.find(this->module_key(module, name));
    if (found == this->generic_function_templates_.end()) {
        if (report_unknown) {
            this->report(range, "unknown generic function in module " + this->module_name(module) + ": " + std::string(name));
        }
        return nullptr;
    }
    if (!this->can_access(module, found->second.visibility)) {
        if (report_unknown) {
            this->report(range, "generic function is private: " + this->module_name(module) + "." + std::string(name));
        }
        return nullptr;
    }
    return &found->second;
}

TypeHandle SemanticAnalyzer::instantiate_generic_struct(
    const GenericTemplateInfo& info,
    const syntax::TypeNode& use_type,
    const syntax::TypeId,
    const std::vector<TypeHandle>& args
) {
    if (args.size() != info.params.size()) {
        this->report(use_type.range, "generic type argument count mismatch for " + info.name);
        return INVALID_TYPE_HANDLE;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return INVALID_TYPE_HANDLE;
        }
    }

    const std::string instance_key = this->generic_struct_instance_key(info, args);
    if (const auto found = this->generic_struct_instances_.find(instance_key); found != this->generic_struct_instances_.end()) {
        return found->second;
    }

    const syntax::ItemNode& item = this->module_.items[info.item.value];
    std::unordered_map<std::string, TypeHandle> substitution;
    for (base::usize i = 0; i < info.params.size(); ++i) {
        substitution.emplace(info.params[i], args[i]);
    }
    const std::string qualified = this->qualified_name(info.module, item.name) + this->generic_instance_suffix(args);
    const std::string c_name = this->c_symbol_name(
        info.module,
        std::string(item.name) + std::string(SEMA_GENERIC_INSTANCE_SEPARATOR) +
            mangle_generic_fragment(this->generic_instance_suffix(args))
    );

    const TypeHandle handle = this->checked_.types.named_struct(qualified, c_name, false);
    this->checked_.types.set_generic_instance(handle, info.key, args);
    this->generic_struct_instances_[instance_key] = handle;
    this->type_visibilities_[instance_key] = info.visibility;

    StructInfo struct_info;
    struct_info.name = std::string(item.name) + this->generic_instance_suffix(args);
    struct_info.c_name = c_name;
    struct_info.module = info.module;
    struct_info.type = handle;
    struct_info.visibility = info.visibility;
    struct_info.is_generic_placeholder = std::any_of(
        args.begin(),
        args.end(),
        [&](const TypeHandle arg) {
            return is_valid(arg) && this->checked_.types.get(arg).kind == TypeKind::generic_param;
        }
    );

    const syntax::ModuleId previous_module = this->current_module_;
    GenericContext generic_context;
    generic_context.params = substitution;
    GenericContext* const previous_body_generic_context = this->current_generic_context_;
    const GenericSideTableScope previous_side_tables = this->current_side_tables_;
    this->current_module_ = info.module;
    this->current_generic_context_ = &generic_context;
    this->current_side_tables_.cache_syntax_types = false;

    bool contains_array = false;
    std::unordered_set<std::string> seen_fields;
    for (const syntax::FieldDecl& field : item.fields) {
        if (!seen_fields.insert(std::string(field.name)).second) {
            this->report(field.range, "duplicate struct field: " + std::string(field.name));
            continue;
        }
        const TypeHandle field_type = this->resolve_type(field.type);
        if (!this->is_valid_storage_type(field_type)) {
            this->report(field.range, "field type is not valid storage");
        }
        if (this->checked_.types.contains_array(field_type)) {
            contains_array = true;
        }
        struct_info.fields.push_back(StructFieldInfo {
            std::string(field.name),
            {},
            syntax::INVALID_MODULE_ID,
            field_type,
            field.range,
            field.visibility,
        });
    }

    this->current_module_ = previous_module;
    this->current_generic_context_ = previous_body_generic_context;
    this->current_side_tables_ = previous_side_tables;

    this->checked_.types.set_record_contains_array(handle, contains_array);
    auto inserted = this->checked_.structs.emplace(instance_key, std::move(struct_info));
    if (inserted.second) {
        this->struct_infos_by_type_[handle.value] = &inserted.first->second;
    }
    return handle;
}

bool SemanticAnalyzer::unify_generic_type(
    const TypeHandle pattern,
    const TypeHandle actual,
    std::unordered_map<std::string, TypeHandle>& inferred
) const {
    if (!is_valid(pattern) || !is_valid(actual)) {
        return false;
    }

    std::vector<std::pair<TypeHandle, TypeHandle>> pending;
    pending.emplace_back(pattern, actual);
    while (!pending.empty()) {
        const auto [current_pattern, current_actual] = pending.back();
        pending.pop_back();
        if (!is_valid(current_pattern) || !is_valid(current_actual)) {
            return false;
        }
        const TypeInfo& pattern_info = this->checked_.types.get(current_pattern);
        if (pattern_info.kind == TypeKind::generic_param) {
            const auto found = inferred.find(pattern_info.name);
            if (found == inferred.end()) {
                inferred.emplace(pattern_info.name, current_actual);
                continue;
            }
            if (!this->checked_.types.same(found->second, current_actual)) {
                return false;
            }
            continue;
        }
        const TypeInfo& actual_info = this->checked_.types.get(current_actual);
        if (pattern_info.kind != actual_info.kind) {
            return false;
        }
        switch (pattern_info.kind) {
        case TypeKind::builtin:
        case TypeKind::enum_:
        case TypeKind::opaque_struct:
            if (!this->checked_.types.same(current_pattern, current_actual)) {
                return false;
            }
            break;
        case TypeKind::pointer:
            if (pattern_info.pointer_mutability != actual_info.pointer_mutability) {
                return false;
            }
            pending.emplace_back(pattern_info.pointee, actual_info.pointee);
            break;
        case TypeKind::array:
            if (pattern_info.array_count != actual_info.array_count) {
                return false;
            }
            pending.emplace_back(pattern_info.array_element, actual_info.array_element);
            break;
        case TypeKind::struct_:
            if (pattern_info.generic_origin_key.empty() ||
                pattern_info.generic_origin_key != actual_info.generic_origin_key ||
                pattern_info.generic_args.size() != actual_info.generic_args.size()) {
                if (!this->checked_.types.same(current_pattern, current_actual)) {
                    return false;
                }
                break;
            }
            for (base::usize i = 0; i < pattern_info.generic_args.size(); ++i) {
                pending.emplace_back(pattern_info.generic_args[i], actual_info.generic_args[i]);
            }
            break;
        case TypeKind::generic_param:
            return false;
        }
    }
    return true;
}

bool SemanticAnalyzer::infer_generic_arguments(
    const GenericTemplateInfo& info,
    const syntax::ExprNode& call,
    std::vector<TypeHandle>& args
) {
    const syntax::ItemNode& function = this->module_.items[info.item.value];
    if (call.args.size() != function.params.size()) {
        this->report(call.range, "argument count mismatch in call to " + info.name);
        return false;
    }

    GenericContext generic_context;
    for (const std::string& param : info.params) {
        generic_context.params.emplace(param, this->checked_.types.generic_param(param));
    }

    const syntax::ModuleId previous_module = this->current_module_;
    GenericContext* const previous_body_generic_context = this->current_generic_context_;
    const GenericSideTableScope previous_side_tables = this->current_side_tables_;
    this->current_module_ = info.module;
    this->current_generic_context_ = &generic_context;
    this->current_side_tables_.cache_syntax_types = false;

    std::vector<TypeHandle> pattern_param_types;
    pattern_param_types.reserve(function.params.size());
    for (const syntax::ParamDecl& param : function.params) {
        pattern_param_types.push_back(this->resolve_type(param.type));
    }

    this->current_module_ = previous_module;
    this->current_generic_context_ = previous_body_generic_context;
    this->current_side_tables_ = previous_side_tables;

    std::unordered_map<std::string, TypeHandle> inferred;
    for (base::usize i = 0; i < call.args.size(); ++i) {
        const TypeHandle actual = this->analyze_expr(call.args[i]);
        if (!this->unify_generic_type(pattern_param_types[i], actual, inferred)) {
            this->report(this->module_.exprs[call.args[i].value].range, "cannot infer generic type argument for call to " + info.name);
            return false;
        }
    }

    args.clear();
    args.reserve(info.params.size());
    for (const std::string& param : info.params) {
        const auto found = inferred.find(param);
        if (found == inferred.end() || !is_valid(found->second)) {
            this->report(call.range, "cannot infer generic type argument `" + param + "` for call to " + info.name);
            return false;
        }
        args.push_back(found->second);
    }
    return true;
}

FunctionSignature* SemanticAnalyzer::instantiate_generic_placeholder_function(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const base::SourceRange use_range
) {
    if (args.size() != info.params.size()) {
        this->report(use_range, "generic function argument count mismatch for " + info.name);
        return nullptr;
    }
    const syntax::ItemNode& function = this->module_.items[info.item.value];

    std::unordered_map<std::string, TypeHandle> substitution;
    for (base::usize i = 0; i < info.params.size(); ++i) {
        if (!is_valid(args[i])) {
            return nullptr;
        }
        substitution.emplace(info.params[i], args[i]);
    }

    GenericContext generic_context;
    generic_context.params = std::move(substitution);
    const syntax::ModuleId previous_module = this->current_module_;
    GenericContext* const previous_generic_context = this->current_generic_context_;
    const GenericSideTableScope previous_side_tables = this->current_side_tables_;
    this->current_module_ = info.module;
    this->current_generic_context_ = &generic_context;
    this->current_side_tables_.cache_syntax_types = false;

    FunctionSignature signature;
    signature.name = info.name + this->generic_instance_suffix(args);
    signature.c_name = signature.name;
    signature.module = info.module;
    signature.return_type = syntax::is_valid(function.return_type)
        ? this->resolve_type(function.return_type)
        : INVALID_TYPE_HANDLE;
    signature.range = function.range;
    signature.has_definition = true;
    signature.visibility = info.visibility;
    signature.definition_item = info.item;
    for (const syntax::ParamDecl& param : function.params) {
        signature.param_types.push_back(this->resolve_type(param.type));
    }

    this->current_module_ = previous_module;
    this->current_generic_context_ = previous_generic_context;
    this->current_side_tables_ = previous_side_tables;

    const std::string key = this->generic_function_instance_key(info, args);
    auto inserted = this->generic_placeholder_functions_.emplace(key, std::move(signature));
    return &inserted.first->second;
}

bool SemanticAnalyzer::type_contains_generic_param(const TypeHandle type) const {
    if (!is_valid(type)) {
        return false;
    }
    std::vector<TypeHandle> pending;
    pending.push_back(type);
    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!is_valid(current)) {
            continue;
        }
        const TypeInfo& info = this->checked_.types.get(current);
        switch (info.kind) {
        case TypeKind::generic_param:
            return true;
        case TypeKind::pointer:
            pending.push_back(info.pointee);
            break;
        case TypeKind::array:
            pending.push_back(info.array_element);
            break;
        case TypeKind::struct_:
            for (const TypeHandle arg : info.generic_args) {
                pending.push_back(arg);
            }
            break;
        case TypeKind::builtin:
        case TypeKind::enum_:
        case TypeKind::opaque_struct:
            break;
        }
    }
    return false;
}

FunctionSignature* SemanticAnalyzer::instantiate_generic_function(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const base::SourceRange use_range
) {
    if (args.size() != info.params.size()) {
        this->report(use_range, "generic function argument count mismatch for " + info.name);
        return nullptr;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return nullptr;
        }
    }
    if (std::any_of(args.begin(), args.end(), [&](const TypeHandle arg) {
            return this->type_contains_generic_param(arg);
        })) {
        return this->instantiate_generic_placeholder_function(info, args, use_range);
    }
    const std::string key = this->generic_function_instance_key(info, args);
    if (const auto found = this->generic_function_instances_.find(key);
        found != this->generic_function_instances_.end()) {
        return &this->checked_.generic_function_instances[found->second].signature;
    }
    const syntax::ItemNode& function = this->module_.items[info.item.value];

    std::unordered_map<std::string, TypeHandle> substitution;
    for (base::usize i = 0; i < info.params.size(); ++i) {
        substitution.emplace(info.params[i], args[i]);
    }

    GenericContext generic_context;
    generic_context.params = substitution;
    const syntax::ModuleId previous_module = this->current_module_;
    GenericContext* const previous_generic_context = this->current_generic_context_;
    const GenericSideTableScope previous_side_tables = this->current_side_tables_;
    this->current_module_ = info.module;
    this->current_generic_context_ = &generic_context;
    this->current_side_tables_.cache_syntax_types = false;

    FunctionSignature signature;
    signature.name = info.name + this->generic_instance_suffix(args);
    signature.c_name = this->c_symbol_name(
        info.module,
        info.name + std::string(SEMA_GENERIC_INSTANCE_SEPARATOR) +
            mangle_generic_fragment(this->generic_instance_suffix(args))
    );
    signature.module = info.module;
    signature.return_type = syntax::is_valid(function.return_type)
        ? this->resolve_type(function.return_type)
        : INVALID_TYPE_HANDLE;
    signature.range = function.range;
    signature.has_definition = true;
    signature.visibility = info.visibility;
    signature.definition_item = info.item;
    for (const syntax::ParamDecl& param : function.params) {
        signature.param_types.push_back(this->resolve_type(param.type));
    }
    this->current_module_ = previous_module;
    this->current_generic_context_ = previous_generic_context;
    this->current_side_tables_ = previous_side_tables;

    if (syntax::is_valid(function.return_type) && is_valid(signature.return_type)) {
        this->validate_function_return_type(function, signature.return_type);
    }

    GenericFunctionInstanceInfo instance;
    instance.key = key;
    instance.item = info.item;
    instance.signature = std::move(signature);
    instance.side_tables = make_generic_side_tables(this->module_);
    const base::usize instance_index = this->checked_.generic_function_instances.size();
    this->checked_.generic_function_instances.push_back(std::move(instance));
    this->generic_function_instances_[key] = instance_index;

    FunctionSignature checked_signature = this->checked_.generic_function_instances[instance_index].signature;
    auto function_inserted = this->checked_.functions.emplace(key, checked_signature);
    if (!function_inserted.second) {
        function_inserted.first->second = checked_signature;
    }
    this->function_body_states_[key] = FunctionBodyState::not_started;
    GenericContext body_context;
    body_context.params = substitution;
    GenericContext* const previous_body_generic_context = this->current_generic_context_;
    const GenericSideTableScope previous_body_side_tables = this->current_side_tables_;
    this->current_generic_context_ = &body_context;
    this->current_side_tables_.side_tables = &this->checked_.generic_function_instances[instance_index].side_tables;
    this->current_side_tables_.cache_syntax_types = false;
    this->analyze_function_body_with_signature(
        function,
        key,
        this->checked_.generic_function_instances[instance_index].signature,
        this->function_body_states_[key]
    );
    this->current_generic_context_ = previous_body_generic_context;
    this->current_side_tables_ = previous_body_side_tables;
    this->checked_.generic_function_instances[instance_index].signature = this->checked_.functions.at(key);
    return &this->checked_.generic_function_instances[instance_index].signature;
}

void SemanticAnalyzer::analyze_generic_function_definition(const GenericTemplateInfo& info) {
    const syntax::ItemNode& function = this->module_.items[info.item.value];
    GenericContext generic_context;
    for (const std::string& param : info.params) {
        generic_context.params.emplace(param, this->checked_.types.generic_param(param));
    }

    const syntax::ModuleId previous_module = this->current_module_;
    GenericContext* const previous_generic_context = this->current_generic_context_;
    const GenericSideTableScope previous_side_tables = this->current_side_tables_;
    this->current_module_ = info.module;
    this->current_generic_context_ = &generic_context;
    this->current_side_tables_.cache_syntax_types = false;

    FunctionSignature signature;
    signature.name = info.name;
    signature.c_name = info.name;
    signature.module = info.module;
    signature.return_type = syntax::is_valid(function.return_type)
        ? this->resolve_type(function.return_type)
        : INVALID_TYPE_HANDLE;
    signature.range = function.range;
    signature.has_definition = true;
    signature.visibility = info.visibility;
    for (const syntax::ParamDecl& param : function.params) {
        signature.param_types.push_back(this->resolve_type(param.type));
    }
    this->generic_placeholder_functions_[info.key] = signature;
    this->current_module_ = previous_module;
    this->current_generic_context_ = previous_generic_context;
    this->current_side_tables_ = previous_side_tables;

    FunctionBodyState state = FunctionBodyState::not_started;
    this->analyze_generic_function_body(function, info, signature, state);
}

void SemanticAnalyzer::analyze_generic_function_body(
    const syntax::ItemNode& function,
    const GenericTemplateInfo& info,
    const FunctionSignature& signature,
    FunctionBodyState& state
) {
    GenericContext generic_context;
    for (const std::string& param : info.params) {
        generic_context.params.emplace(param, this->checked_.types.generic_param(param));
    }
    GenericContext* const previous_generic_context = this->current_generic_context_;
    GenericSideTables side_tables = make_generic_side_tables(this->module_);
    GenericSideTableScope previous_side_tables = this->current_side_tables_;
    this->current_generic_context_ = &generic_context;
    this->current_side_tables_.side_tables = &side_tables;
    this->current_side_tables_.cache_syntax_types = false;
    this->analyze_function_body_with_signature(function, info.key, signature, state);
    this->current_generic_context_ = previous_generic_context;
    this->current_side_tables_ = previous_side_tables;
}

} // namespace aurex::sema
