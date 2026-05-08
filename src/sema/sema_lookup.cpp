#include "aurex/sema/sema.hpp"

#include "aurex/syntax/module.hpp"

namespace aurex::sema {

namespace {

[[nodiscard]] std::string enum_case_lookup_key(
    const TypeHandle enum_type,
    const std::string_view case_name
) {
    std::string key = std::to_string(enum_type.value);
    key.reserve(key.size() + 1 + case_name.size());
    key.push_back(':');
    key += case_name;
    return key;
}

} // namespace

syntax::ModuleId SemanticAnalyzer::item_module(const syntax::ItemNode& item) const noexcept {
    const auto* const begin = module_.items.data();
    const auto* const end = begin + module_.items.size();
    if (&item < begin || &item >= end) {
        return syntax::invalid_module_id;
    }
    const base::usize index = static_cast<base::usize>(&item - begin);
    if (index >= module_.item_modules.size()) {
        return syntax::invalid_module_id;
    }
    return module_.item_modules[index];
}

syntax::ModuleId SemanticAnalyzer::resolve_import_alias(
    const std::string_view alias,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (!syntax::is_valid(current_module_) || current_module_.value >= module_.modules.size()) {
        if (report_unknown) {
            report(range, "unknown import alias: " + std::string(alias));
        }
        return syntax::invalid_module_id;
    }
    syntax::ModuleId resolved = syntax::invalid_module_id;
    for (const syntax::ResolvedImport& import : module_.modules[current_module_.value].imports) {
        if (import.alias != alias) {
            continue;
        }
        if (syntax::is_valid(resolved)) {
            if (report_unknown) {
                report(range, "ambiguous import alias: " + std::string(alias));
            }
            return syntax::invalid_module_id;
        }
        resolved = import.module;
    }
    if (!syntax::is_valid(resolved) && report_unknown) {
        report(range, "unknown import alias: " + std::string(alias));
    }
    return resolved;
}

const std::vector<syntax::ModuleId>& SemanticAnalyzer::visible_modules(const syntax::ModuleId module) const {
    static const std::vector<syntax::ModuleId> empty;
    if (!syntax::is_valid(module)) {
        return empty;
    }
    if (const auto found = visible_modules_cache_.find(module.value); found != visible_modules_cache_.end()) {
        return found->second;
    }
    std::vector<syntax::ModuleId> result;
    result.push_back(module);
    if (module.value >= module_.modules.size()) {
        auto inserted = visible_modules_cache_.emplace(module.value, std::move(result));
        return inserted.first->second;
    }
    std::unordered_set<base::u32> seen;
    seen.insert(module.value);
    for (const syntax::ResolvedImport& import : module_.modules[module.value].imports) {
        if (!syntax::is_valid(import.module)) {
            continue;
        }
        if (seen.insert(import.module.value).second) {
            result.push_back(import.module);
        }
        append_public_reexports(import.module, result, seen);
    }
    auto inserted = visible_modules_cache_.emplace(module.value, std::move(result));
    return inserted.first->second;
}

void SemanticAnalyzer::append_public_reexports(
    const syntax::ModuleId module,
    std::vector<syntax::ModuleId>& result,
    std::unordered_set<base::u32>& seen
) const {
    if (!syntax::is_valid(module) || module.value >= module_.modules.size()) {
        return;
    }
    for (const syntax::ResolvedImport& import : module_.modules[module.value].imports) {
        if (import.visibility != syntax::Visibility::public_ || !syntax::is_valid(import.module)) {
            continue;
        }
        if (seen.insert(import.module.value).second) {
            result.push_back(import.module);
            append_public_reexports(import.module, result, seen);
        }
    }
}

std::string SemanticAnalyzer::module_name(const syntax::ModuleId module) const {
    if (!syntax::is_valid(module) || module.value >= module_.modules.size()) {
        return "<unknown>";
    }
    return syntax::module_path_to_string(module_.modules[module.value].path);
}

std::string SemanticAnalyzer::qualified_name(const syntax::ModuleId module, const std::string_view name) const {
    const std::string module_text = module_name(module);
    if (module_text.empty() || module_text == "<unknown>") {
        return std::string(name);
    }
    return module_text + "." + std::string(name);
}

std::string SemanticAnalyzer::c_symbol_name(const syntax::ModuleId module, const std::string_view name) const {
    if (!syntax::is_valid(module) || module.value >= module_.modules.size()) {
        return std::string(name);
    }
    return syntax::mangle_c_symbol(module_.modules[module.value].path, name);
}

std::string SemanticAnalyzer::module_key(const syntax::ModuleId module, const std::string_view name) const {
    std::string key = std::to_string(module.value);
    key.reserve(key.size() + 1 + name.size());
    key.push_back(':');
    key += name;
    return key;
}

std::string SemanticAnalyzer::function_key(const syntax::ItemNode& function) const {
    const syntax::ModuleId module = item_module(function);
    if (!syntax::is_valid(function.impl_type)) {
        return module_key(module, function.name);
    }
    const TypeHandle owner_type =
        function.impl_type.value < checked_.syntax_type_handles.size()
            ? checked_.syntax_type_handles[function.impl_type.value]
            : invalid_type_handle;
    return method_key(module, owner_type, function.name);
}

std::string SemanticAnalyzer::method_key(
    const syntax::ModuleId module,
    const TypeHandle owner_type,
    const std::string_view name
) const {
    std::string method_name = "#";
    method_name += std::to_string(owner_type.value);
    method_name.push_back('.');
    method_name += name;
    return module_key(module, method_name);
}

std::string SemanticAnalyzer::method_c_symbol_name(
    const TypeHandle owner_type,
    const std::string_view name
) const {
    return checked_.types.c_name(owner_type) + "_" + std::string(name);
}

bool SemanticAnalyzer::can_access(const syntax::ModuleId owner, const syntax::Visibility visibility) const noexcept {
    return owner.value == current_module_.value || visibility == syntax::Visibility::public_;
}

bool SemanticAnalyzer::method_receiver_matches(
    const FunctionSignature& signature,
    const TypeHandle receiver_type,
    const syntax::ExprId receiver
) {
    if (!signature.has_self_param || signature.param_types.empty()) {
        return false;
    }
    const TypeHandle self_type = signature.param_types.front();
    if (checked_.types.same(self_type, receiver_type)) {
        if (checked_.types.contains_array(self_type)) {
            report(module_.exprs[receiver.value].range, "non-copyable array storage cannot be passed by value");
            return false;
        }
        consume_ownership_transfer(receiver, self_type, "method receiver");
        return true;
    }
    if (!checked_.types.is_pointer(self_type)) {
        return false;
    }
    const TypeHandle pointee = checked_.types.get(self_type).pointee;
    if (checked_.types.is_pointer(receiver_type)) {
        const TypeInfo& self_info = checked_.types.get(self_type);
        const TypeInfo& receiver_info = checked_.types.get(receiver_type);
        if (!checked_.types.same(self_info.pointee, receiver_info.pointee)) {
            return false;
        }
        if (self_info.pointer_mutability == PointerMutability::mut &&
            receiver_info.pointer_mutability != PointerMutability::mut) {
            report(module_.exprs[receiver.value].range, "mutable method receiver requires mutable pointer");
            return false;
        }
        return true;
    }
    if (!checked_.types.same(pointee, receiver_type)) {
        return false;
    }
    const PointerMutability self_mutability = checked_.types.get(self_type).pointer_mutability;
    if (self_mutability == PointerMutability::mut) {
        if (!is_place_expr(receiver)) {
            report(module_.exprs[receiver.value].range, "method receiver must be a place expression");
            return false;
        }
        if (!is_writable_place(receiver)) {
            report(module_.exprs[receiver.value].range, "mutable method receiver requires writable storage");
            return false;
        }
    }
    return true;
}

const FunctionSignature* SemanticAnalyzer::find_method_in_visible_modules(
    const TypeHandle owner_type,
    const std::string_view name,
    const base::SourceRange range,
    const bool require_self,
    const bool report_unknown
) {
    const FunctionSignature* imported_result = nullptr;
    const FunctionSignature* inaccessible_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        const auto found = checked_.functions.find(method_key(module, owner_type, name));
        if (found == checked_.functions.end()) {
            continue;
        }
        const FunctionSignature& signature = found->second;
        if (!signature.is_method || (require_self && !signature.has_self_param)) {
            continue;
        }
        if (!can_access(module, signature.visibility)) {
            if (inaccessible_result == nullptr) {
                inaccessible_result = &signature;
            }
            continue;
        }
        if (imported_result != nullptr) {
            report(range, "ambiguous method '" + checked_.types.display_name(owner_type) + "." + std::string(name) +
                "' from modules " + module_name(result_module) + " and " + module_name(module));
            return nullptr;
        }
        imported_result = &signature;
        result_module = module;
    }
    if (imported_result == nullptr && report_unknown) {
        if (inaccessible_result != nullptr) {
            report(range, "method is private: " + checked_.types.display_name(owner_type) + "." + std::string(name));
            return nullptr;
        }
        report(range, "unknown method: " + checked_.types.display_name(owner_type) + "." + std::string(name));
    }
    return imported_result;
}

TypeHandle SemanticAnalyzer::find_type_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool opaque_allowed_as_pointee,
    const bool report_unknown
) {
    if (const auto found = named_types_.find(module_key(current_module_, name)); found != named_types_.end()) {
        return found->second;
    }
    if (const auto found = checked_.type_aliases.find(module_key(current_module_, name)); found != checked_.type_aliases.end()) {
        return resolve_type_alias(found->second, opaque_allowed_as_pointee);
    }

    TypeHandle imported_result = invalid_type_handle;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        if (module.value == current_module_.value) {
            continue;
        }
        const auto found = named_types_.find(module_key(module, name));
        TypeHandle candidate = invalid_type_handle;
        if (found != named_types_.end()) {
            const auto visibility = type_visibilities_.find(module_key(module, name));
            if (visibility != type_visibilities_.end() && !can_access(module, visibility->second)) {
                continue;
            }
            candidate = found->second;
        } else {
            const auto alias_found = checked_.type_aliases.find(module_key(module, name));
            if (alias_found == checked_.type_aliases.end()) {
                continue;
            }
            if (!can_access(module, alias_found->second.visibility)) {
                continue;
            }
            candidate = resolve_type_alias(alias_found->second, opaque_allowed_as_pointee);
        }
        if (is_valid(imported_result)) {
            report(range, "ambiguous type name '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
            return invalid_type_handle;
        }
        imported_result = candidate;
        result_module = module;
    }
    if (!is_valid(imported_result) && report_unknown) {
        report(range, "unknown type: " + std::string(name));
    }
    return imported_result;
}

TypeHandle SemanticAnalyzer::find_type_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool opaque_allowed_as_pointee,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            report(range, "unknown type: " + std::string(name));
        }
        return invalid_type_handle;
    }

    const std::string key = module_key(module, name);
    if (const auto found = named_types_.find(key); found != named_types_.end()) {
        const auto visibility = type_visibilities_.find(key);
        if (visibility != type_visibilities_.end() && !can_access(module, visibility->second)) {
            if (report_unknown) {
                report(range, "type is private: " + module_name(module) + "." + std::string(name));
            }
            return invalid_type_handle;
        }
        return found->second;
    }
    if (const auto alias_found = checked_.type_aliases.find(key); alias_found != checked_.type_aliases.end()) {
        if (!can_access(module, alias_found->second.visibility)) {
            if (report_unknown) {
                report(range, "type is private: " + module_name(module) + "." + std::string(name));
            }
            return invalid_type_handle;
        }
        return resolve_type_alias(alias_found->second, opaque_allowed_as_pointee);
    }

    if (report_unknown) {
        report(range, "unknown type in module " + module_name(module) + ": " + std::string(name));
    }
    return invalid_type_handle;
}

const FunctionSignature* SemanticAnalyzer::find_function_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (const auto found = checked_.functions.find(module_key(current_module_, name)); found != checked_.functions.end()) {
        return &found->second;
    }

    const FunctionSignature* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        if (module.value == current_module_.value) {
            continue;
        }
        const auto found = checked_.functions.find(module_key(module, name));
        if (found == checked_.functions.end()) {
            continue;
        }
        if (!can_access(module, found->second.visibility)) {
            continue;
        }
        if (imported_result != nullptr) {
            report(range, "ambiguous function name '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
            return nullptr;
        }
        imported_result = &found->second;
        result_module = module;
    }
    if (imported_result == nullptr && report_unknown) {
        report(range, "unknown function: " + std::string(name));
    }
    return imported_result;
}

const FunctionSignature* SemanticAnalyzer::find_function_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    const auto found = checked_.functions.find(module_key(module, name));
    if (found == checked_.functions.end()) {
        if (report_unknown) {
            report(range, "unknown function in module " + module_name(module) + ": " + std::string(name));
        }
        return nullptr;
    }
    if (!can_access(module, found->second.visibility)) {
        if (report_unknown) {
            report(range, "function is private: " + module_name(module) + "." + std::string(name));
        }
        return nullptr;
    }
    return &found->second;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_case_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (const auto found = checked_.enum_cases.find(module_key(current_module_, name)); found != checked_.enum_cases.end()) {
        return &found->second;
    }

    const EnumCaseInfo* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        if (module.value == current_module_.value) {
            continue;
        }
        const auto found = checked_.enum_cases.find(module_key(module, name));
        if (found == checked_.enum_cases.end()) {
            continue;
        }
        if (!can_access(module, found->second.visibility)) {
            continue;
        }
        if (imported_result != nullptr) {
            report(range, "ambiguous enum case '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
            return nullptr;
        }
        imported_result = &found->second;
        result_module = module;
    }
    if (imported_result == nullptr && report_unknown) {
        report(range, "unknown enum case: " + std::string(name));
    }
    return imported_result;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_case_by_type_and_case(
    const TypeHandle enum_type,
    const std::string_view case_name
) const {
    if (is_valid(enum_type)) {
        if (const auto found = enum_cases_by_type_and_case_.find(enum_case_lookup_key(enum_type, case_name));
            found != enum_cases_by_type_and_case_.end()) {
            return found->second;
        }
    }

    for (const auto& entry : checked_.enum_cases) {
        const EnumCaseInfo& candidate = entry.second;
        if (checked_.types.same(candidate.type, enum_type) && candidate.case_name == case_name) {
            return &candidate;
        }
    }
    return nullptr;
}

const std::vector<const EnumCaseInfo*>* SemanticAnalyzer::find_enum_cases_by_type(
    const TypeHandle enum_type
) const noexcept {
    if (!is_valid(enum_type)) {
        return nullptr;
    }
    const auto found = enum_cases_by_type_.find(enum_type.value);
    return found == enum_cases_by_type_.end() ? nullptr : &found->second;
}

void SemanticAnalyzer::index_enum_case(const EnumCaseInfo& info) {
    if (!is_valid(info.type)) {
        return;
    }
    enum_cases_by_type_and_case_.emplace(enum_case_lookup_key(info.type, info.case_name), &info);
    enum_cases_by_type_[info.type.value].push_back(&info);
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_case_by_scoped_name(
    const std::string_view enum_name,
    const std::string_view case_name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (!report_unknown &&
        named_types_.find(module_key(current_module_, enum_name)) == named_types_.end() &&
        checked_.type_aliases.find(module_key(current_module_, enum_name)) == checked_.type_aliases.end()) {
        bool imported_type = false;
        {
            for (syntax::ModuleId module : visible_modules(current_module_)) {
                if (module.value == current_module_.value) {
                    continue;
                }
                const auto named = named_types_.find(module_key(module, enum_name));
                const auto alias = checked_.type_aliases.find(module_key(module, enum_name));
                bool accessible_named = false;
                if (named != named_types_.end()) {
                    const auto visibility = type_visibilities_.find(module_key(module, enum_name));
                    accessible_named = visibility == type_visibilities_.end() || can_access(module, visibility->second);
                }
                const bool accessible_alias = alias != checked_.type_aliases.end() && can_access(module, alias->second.visibility);
                if (accessible_named || accessible_alias) {
                    imported_type = true;
                    break;
                }
            }
        }
        if (!imported_type) {
            return nullptr;
        }
    }
    const TypeHandle enum_type = find_type_in_visible_modules(enum_name, range, false);
    if (!is_valid(enum_type) || checked_.types.get(enum_type).kind != TypeKind::enum_) {
        if (is_valid(enum_type) && report_unknown) {
            report(range, "enum case scope must name an enum type");
        }
        return nullptr;
    }
    if (const EnumCaseInfo* result = find_enum_case_by_type_and_case(enum_type, case_name); result != nullptr) {
        return result;
    }
    if (report_unknown) {
        report(range, "unknown enum case: " + std::string(enum_name) + "." + std::string(case_name));
    }
    return nullptr;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_constructor(const syntax::ExprId callee_id, const bool report_unknown) {
    if (!syntax::is_valid(callee_id) || callee_id.value >= module_.exprs.size()) {
        return nullptr;
    }
    const syntax::ExprNode& callee = module_.exprs[callee_id.value];
    if (callee.kind == syntax::ExprKind::name) {
        if (!callee.scope_name.empty()) {
            return nullptr;
        }
        return find_enum_case_in_visible_modules(callee.text, callee.range, report_unknown);
    }
    if (callee.kind != syntax::ExprKind::field ||
        !syntax::is_valid(callee.object) ||
        callee.object.value >= module_.exprs.size() ||
        module_.exprs[callee.object.value].kind != syntax::ExprKind::name) {
        return nullptr;
    }
    const syntax::ExprNode& enum_name = module_.exprs[callee.object.value];
    const GenericEnumTemplateInfo* generic_template = nullptr;
    if (enum_name.scope_name.empty()) {
        generic_template = find_generic_enum_template_in_visible_modules(enum_name.text, callee.range, false);
    } else {
        const syntax::ModuleId scope_module = resolve_import_alias(enum_name.scope_name, enum_name.scope_range, false);
        if (syntax::is_valid(scope_module)) {
            generic_template = find_generic_enum_template_in_module(scope_module, enum_name.text, callee.range, false);
        }
    }
    if (generic_template != nullptr) {
        return nullptr;
    }
    if (!enum_name.scope_name.empty() || !enum_name.type_args.empty()) {
        const TypeHandle enum_type = resolve_associated_type_owner(enum_name, report_unknown);
        if (!is_valid(enum_type)) {
            return nullptr;
        }
        if (checked_.types.get(enum_type).kind != TypeKind::enum_) {
            if (report_unknown) {
                report(callee.range, "enum case scope must name an enum type");
            }
            return nullptr;
        }
        if (const EnumCaseInfo* result = find_enum_case_by_type_and_case(enum_type, callee.field_name); result != nullptr) {
            return result;
        }
        if (report_unknown) {
            report(callee.range, "unknown enum case: " + std::string(enum_name.text) + "." + std::string(callee.field_name));
        }
        return nullptr;
    }
    return find_enum_case_by_scoped_name(enum_name.text, callee.field_name, callee.range, report_unknown);
}

const Symbol* SemanticAnalyzer::find_symbol(const std::string_view name, const base::SourceRange range) {
    if (const Symbol* local = symbols_.find(name); local != nullptr) {
        return local;
    }

    if (const auto found = global_values_.find(module_key(current_module_, name)); found != global_values_.end()) {
        return &found->second;
    }

    const Symbol* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        if (module.value == current_module_.value) {
            continue;
        }
        const auto found = global_values_.find(module_key(module, name));
        if (found == global_values_.end()) {
            continue;
        }
        if (!can_access(module, found->second.visibility)) {
            continue;
        }
        if (imported_result != nullptr) {
            report(range, "ambiguous name '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
            return nullptr;
        }
        imported_result = &found->second;
        result_module = module;
    }
    if (imported_result == nullptr) {
        report(range, "unknown name: " + std::string(name));
    }
    return imported_result;
}

const Symbol* SemanticAnalyzer::find_symbol_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    const auto found = global_values_.find(module_key(module, name));
    if (found == global_values_.end()) {
        if (report_unknown) {
            report(range, "unknown name in module " + module_name(module) + ": " + std::string(name));
        }
        return nullptr;
    }
    if (!can_access(module, found->second.visibility)) {
        if (report_unknown) {
            report(range, "name is private: " + module_name(module) + "." + std::string(name));
        }
        return nullptr;
    }
    return &found->second;
}

} // namespace aurex::sema
