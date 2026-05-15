#include <aurex/sema/sema.hpp>

#include <aurex/sema/sema_messages.hpp>
#include <aurex/syntax/module.hpp>

#include <array>

namespace aurex::sema {

namespace {

constexpr std::string_view SEMA_LOOKUP_UNKNOWN_MODULE_NAME = "<unknown>";

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

[[nodiscard]] bool module_path_matches_parts(
    const syntax::ModulePath& path,
    const std::vector<std::string_view>& parts
) noexcept {
    if (path.parts.size() != parts.size()) {
        return false;
    }
    for (base::usize i = 0; i < parts.size(); ++i) {
        if (path.parts[i] != parts[i]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool module_path_matches_prefix(
    const syntax::ModulePath& path,
    const std::vector<std::string_view>& parts,
    const base::usize prefix_size
) noexcept {
    if (path.parts.size() != prefix_size || parts.size() < prefix_size) {
        return false;
    }
    for (base::usize i = 0; i < prefix_size; ++i) {
        if (path.parts[i] != parts[i]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string module_path_parts_name(const std::vector<std::string_view>& parts) {
    std::string name;
    for (base::usize i = 0; i < parts.size(); ++i) {
        if (i != 0) {
            name.push_back('.');
        }
        name += parts[i];
    }
    return name;
}

} // namespace

syntax::ModuleId SemanticAnalyzer::item_module(const syntax::ItemNode& item) const noexcept {
    const auto* const begin = this->module_.items.data();
    const auto* const end = begin + this->module_.items.size();
    if (&item < begin || &item >= end) {
        return syntax::INVALID_MODULE_ID;
    }
    const base::usize index = static_cast<base::usize>(&item - begin);
    if (index >= this->module_.item_modules.size()) {
        return syntax::INVALID_MODULE_ID;
    }
    return this->module_.item_modules[index];
}

syntax::ModuleId SemanticAnalyzer::resolve_import_alias(
    const std::string_view alias,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (!syntax::is_valid(current_module_) || current_module_.value >= module_.modules.size()) {
        if (report_unknown) {
            report(range, sema_unknown_import_alias_message(alias));
        }
        return syntax::INVALID_MODULE_ID;
    }
    syntax::ModuleId resolved = syntax::INVALID_MODULE_ID;
    for (const syntax::ResolvedImport& import : module_.modules[current_module_.value].imports) {
        if (import.alias != alias) {
            continue;
        }
        if (syntax::is_valid(resolved)) {
            if (report_unknown) {
                report(range, sema_ambiguous_import_alias_message(alias));
            }
            return syntax::INVALID_MODULE_ID;
        }
        resolved = import.module;
    }
    if (!syntax::is_valid(resolved) && report_unknown) {
        report(range, sema_unknown_import_alias_message(alias));
    }
    return resolved;
}

const std::vector<syntax::ModuleId>& SemanticAnalyzer::visible_modules(const syntax::ModuleId module) const {
    static const std::vector<syntax::ModuleId> empty;
    if (!syntax::is_valid(module)) {
        return empty;
    }
    if (const auto found = this->visible_modules_cache_.find(module.value);
        found != this->visible_modules_cache_.end()) {
        return found->second;
    }
    std::vector<syntax::ModuleId> result;
    result.push_back(module);
    if (module.value >= module_.modules.size()) {
        auto inserted = this->visible_modules_cache_.emplace(module.value, std::move(result));
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
    auto inserted = this->visible_modules_cache_.emplace(module.value, std::move(result));
    return inserted.first->second;
}

bool SemanticAnalyzer::module_alias_visible(const std::string_view name) const {
    if (!syntax::is_valid(this->current_module_) ||
        this->current_module_.value >= this->module_.modules.size()) {
        return false;
    }
    for (const syntax::ResolvedImport& import : this->module_.modules[this->current_module_.value].imports) {
        if (import.alias == name) {
            return true;
        }
    }
    return false;
}

bool SemanticAnalyzer::visible_root_module_name_exists(const std::string_view name) const {
    if (name.empty()) {
        return false;
    }
    for (const syntax::ModuleId module : this->visible_modules(this->current_module_)) {
        if (!syntax::is_valid(module) || module.value >= this->module_.modules.size()) {
            continue;
        }
        const syntax::ModulePath& path = this->module_.modules[module.value].path;
        if (!path.parts.empty() && path.parts.front() == name) {
            return true;
        }
    }
    return false;
}

std::vector<std::string_view> SemanticAnalyzer::type_scope_parts(const syntax::TypeNode& type) const {
    if (!type.scope_parts.empty()) {
        return type.scope_parts;
    }
    if (!type.scope_name.empty()) {
        return {type.scope_name};
    }
    return {};
}

syntax::ModuleId SemanticAnalyzer::find_visible_module_path(const std::vector<std::string_view>& parts) const {
    if (parts.empty()) {
        return syntax::INVALID_MODULE_ID;
    }
    for (const syntax::ModuleId module : this->visible_modules(this->current_module_)) {
        if (!syntax::is_valid(module) || module.value >= this->module_.modules.size()) {
            continue;
        }
        if (module_path_matches_parts(this->module_.modules[module.value].path, parts)) {
            return module;
        }
    }
    return syntax::INVALID_MODULE_ID;
}

bool SemanticAnalyzer::visible_module_path_prefix_exists(
    const std::vector<std::string_view>& parts
) const {
    if (parts.size() < 2) {
        return false;
    }
    for (base::usize prefix_size = 1; prefix_size < parts.size(); ++prefix_size) {
        for (const syntax::ModuleId module : this->visible_modules(this->current_module_)) {
            if (!syntax::is_valid(module) || module.value >= this->module_.modules.size()) {
                continue;
            }
            if (module_path_matches_prefix(this->module_.modules[module.value].path, parts, prefix_size)) {
                return true;
            }
        }
    }
    return false;
}

syntax::ModuleId SemanticAnalyzer::resolve_type_scope(
    const syntax::TypeNode& type,
    const bool report_unknown
) {
    const std::vector<std::string_view> parts = this->type_scope_parts(type);
    if (parts.empty()) {
        return syntax::INVALID_MODULE_ID;
    }
    if (parts.size() == 1) {
        return this->resolve_import_alias(parts.front(), type.scope_range, report_unknown);
    }

    const syntax::ModuleId module = this->find_visible_module_path(parts);
    if (!syntax::is_valid(module) && report_unknown) {
        this->report(type.scope_range, sema_unknown_module_path_message(module_path_parts_name(parts)));
    }
    return module;
}

const std::vector<syntax::ModuleId>& SemanticAnalyzer::module_export_modules(const syntax::ModuleId module) const {
    static const std::vector<syntax::ModuleId> empty;
    if (!syntax::is_valid(module)) {
        return empty;
    }
    if (const auto found = this->module_export_modules_cache_.find(module.value);
        found != this->module_export_modules_cache_.end()) {
        return found->second;
    }
    std::vector<syntax::ModuleId> result;
    result.push_back(module);
    if (module.value >= this->module_.modules.size()) {
        auto inserted = this->module_export_modules_cache_.emplace(module.value, std::move(result));
        return inserted.first->second;
    }
    std::unordered_set<base::u32> seen;
    seen.insert(module.value);
    this->append_public_reexports(module, result, seen);
    auto inserted = this->module_export_modules_cache_.emplace(module.value, std::move(result));
    return inserted.first->second;
}

void SemanticAnalyzer::append_public_reexports(
    const syntax::ModuleId module,
    std::vector<syntax::ModuleId>& result,
    std::unordered_set<base::u32>& seen
) const {
    std::vector<syntax::ModuleId> pending;
    pending.push_back(module);
    while (!pending.empty()) {
        const syntax::ModuleId current = pending.back();
        pending.pop_back();
        if (!syntax::is_valid(current) || current.value >= this->module_.modules.size()) {
            continue;
        }
        for (const syntax::ResolvedImport& import : this->module_.modules[current.value].imports) {
            if (import.visibility != syntax::Visibility::public_ || !syntax::is_valid(import.module)) {
                continue;
            }
            if (seen.insert(import.module.value).second) {
                result.push_back(import.module);
                pending.push_back(import.module);
            }
        }
    }
}

std::string SemanticAnalyzer::module_name(const syntax::ModuleId module) const {
    if (!syntax::is_valid(module) || module.value >= module_.modules.size()) {
        return std::string(SEMA_LOOKUP_UNKNOWN_MODULE_NAME);
    }
    return syntax::module_path_to_string(module_.modules[module.value].path);
}

std::string SemanticAnalyzer::qualified_name(const syntax::ModuleId module, const std::string_view name) const {
    const std::string module_text = module_name(module);
    if (module_text.empty() || module_text == SEMA_LOOKUP_UNKNOWN_MODULE_NAME) {
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
    const syntax::ModuleId module = this->item_module(function);
    if (this->has_generic_params(function)) {
        return this->module_key(module, function.name);
    }
    if (!syntax::is_valid(function.impl_type)) {
        return this->module_key(module, function.name);
    }
    const TypeHandle owner_type =
        function.impl_type.value < this->checked_.syntax_type_handles.size()
            ? this->checked_.syntax_type_handles[function.impl_type.value]
            : INVALID_TYPE_HANDLE;
    return this->method_key(module, owner_type, function.name);
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

bool SemanticAnalyzer::top_level_value_name_exists(
    const syntax::ModuleId module,
    const std::string_view name
) const {
    if (const auto found = this->global_values_.find(this->module_key(module, name));
        found != this->global_values_.end()) {
        return found->second.kind == SymbolKind::function || found->second.kind == SymbolKind::const_;
    }
    return false;
}

bool SemanticAnalyzer::module_type_or_value_name_exists(
    const syntax::ModuleId module,
    const std::string_view name
) const {
    const std::string key = this->module_key(module, name);
    return this->named_types_.contains(key) ||
           this->checked_.type_aliases.contains(key) ||
           this->find_any_generic_type_template_in_module(module, name) != nullptr ||
           this->top_level_value_name_exists(module, name) ||
           this->generic_function_templates_.contains(key);
}

bool SemanticAnalyzer::current_generic_param_exists(const std::string_view name) const {
    return this->current_generic_context_ != nullptr &&
           this->current_generic_context_->params.contains(std::string(name));
}

bool SemanticAnalyzer::visible_type_name_exists(const std::string_view name) const {
    if (this->current_generic_param_exists(name)) {
        return true;
    }
    const auto type_visible_in_module = [&](const syntax::ModuleId module) {
        if (!syntax::is_valid(module)) {
            return false;
        }
        const std::string key = this->module_key(module, name);
        if (this->named_types_.contains(key)) {
            const auto visibility = this->type_visibilities_.find(key);
            return module.value == this->current_module_.value ||
                   visibility == this->type_visibilities_.end() ||
                   this->can_access(module, visibility->second);
        }
        if (const auto alias = this->checked_.type_aliases.find(key); alias != this->checked_.type_aliases.end()) {
            return this->can_access(module, alias->second.visibility);
        }
        const GenericTemplateInfo* const generic = this->find_any_generic_type_template_in_module(module, name);
        return generic != nullptr && this->can_access(module, generic->visibility);
    };
    if (type_visible_in_module(this->current_module_)) {
        return true;
    }
    if (!syntax::is_valid(this->current_module_) || this->current_module_.value >= this->module_.modules.size()) {
        return false;
    }
    for (const syntax::ResolvedImport& import : this->module_.modules[this->current_module_.value].imports) {
        if (type_visible_in_module(import.module)) {
            return true;
        }
    }
    return false;
}

bool SemanticAnalyzer::can_define_local_name(
    const std::string_view name,
    const base::SourceRange range
) {
    if (name.empty()) {
        return true;
    }
    if (this->module_alias_visible(name)) {
        this->report(range, sema_local_shadows_import_alias_message(name));
        return false;
    }
    if (this->visible_root_module_name_exists(name)) {
        this->report(range, sema_local_shadows_root_module_message(name));
        return false;
    }
    if (this->current_generic_param_exists(name)) {
        this->report(range, sema_local_shadows_generic_type_parameter_message(name));
        return false;
    }
    if (this->visible_type_name_exists(name)) {
        this->report(range, sema_local_shadows_type_name_message(name));
        return false;
    }
    return true;
}

bool SemanticAnalyzer::type_member_name_exists(
    const TypeHandle owner_type,
    const std::string_view name
) const {
    if (!is_valid(owner_type)) {
        return false;
    }
    if (const auto found = this->enum_cases_by_type_.find(owner_type.value);
        found != this->enum_cases_by_type_.end()) {
        for (const EnumCaseInfo* enum_case : found->second) {
            if (enum_case != nullptr && enum_case->case_name == name) {
                return true;
            }
        }
    }
    for (const auto& entry : this->checked_.functions) {
        const FunctionSignature& signature = entry.second;
        if (signature.is_method &&
            this->checked_.types.same(signature.method_owner_type, owner_type) &&
            signature.name == name) {
            return true;
        }
    }
    for (const auto& entry : this->generic_method_templates_) {
        const GenericTemplateInfo& info = entry.second;
        if (info.name == name &&
            is_valid(info.impl_type_pattern) &&
            this->checked_.types.same(info.impl_type_pattern, owner_type)) {
            return true;
        }
    }
    return false;
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

syntax::ModuleId SemanticAnalyzer::owner_module(const TypeHandle owner_type) const noexcept {
    if (!is_valid(owner_type)) {
        return syntax::INVALID_MODULE_ID;
    }
    if (const StructInfo* info = find_struct(owner_type); info != nullptr) {
        return info->module;
    }
    if (const auto found = enum_cases_by_type_.find(owner_type.value);
        found != enum_cases_by_type_.end() &&
        !found->second.empty()) {
        return found->second.front()->module;
    }
    return syntax::INVALID_MODULE_ID;
}

const FunctionSignature* SemanticAnalyzer::find_method_in_owner_module(
    const TypeHandle owner_type,
    const std::string_view name,
    const bool require_self
) const {
    const syntax::ModuleId owner = owner_module(owner_type);
    if (!syntax::is_valid(owner)) {
        return nullptr;
    }
    const auto found = checked_.functions.find(method_key(owner, owner_type, name));
    if (found == checked_.functions.end()) {
        return nullptr;
    }
    const FunctionSignature& signature = found->second;
    if (!signature.is_method || (require_self && !signature.has_self_param)) {
        return nullptr;
    }
    if (!can_access(owner, signature.visibility)) {
        return nullptr;
    }
    return &signature;
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
    if (this->checked_.types.same(self_type, receiver_type)) {
        if (this->checked_.types.contains_array(self_type)) {
            this->report(this->module_.exprs[receiver.value].range, std::string(SEMA_ARGUMENT_ARRAY_UNSUPPORTED));
            return false;
        }
        return true;
    }
    if (!this->checked_.types.is_pointer(self_type) && !this->checked_.types.is_reference(self_type)) {
        return false;
    }
    const TypeHandle pointee = this->checked_.types.get(self_type).pointee;
    if (this->checked_.types.is_pointer(receiver_type) || this->checked_.types.is_reference(receiver_type)) {
        const TypeInfo& self_info = this->checked_.types.get(self_type);
        const TypeInfo& receiver_info = this->checked_.types.get(receiver_type);
        if (self_info.kind != receiver_info.kind) {
            return false;
        }
        if (!this->checked_.types.same(self_info.pointee, receiver_info.pointee)) {
            return false;
        }
        if (self_info.pointer_mutability == PointerMutability::mut &&
            receiver_info.pointer_mutability != PointerMutability::mut) {
            this->report(this->module_.exprs[receiver.value].range, std::string(SEMA_MUTABLE_METHOD_RECEIVER_POINTER));
            return false;
        }
        return true;
    }
    if (!this->checked_.types.same(pointee, receiver_type)) {
        return false;
    }
    const PointerMutability self_mutability = this->checked_.types.get(self_type).pointer_mutability;
    if (self_mutability == PointerMutability::mut) {
        if (!this->is_place_expr(receiver)) {
            this->report(this->module_.exprs[receiver.value].range, std::string(SEMA_METHOD_RECEIVER_PLACE));
            return false;
        }
        if (!this->is_writable_place(receiver)) {
            this->report(this->module_.exprs[receiver.value].range, std::string(SEMA_MUTABLE_METHOD_RECEIVER_WRITABLE));
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
    const FunctionSignature* result = nullptr;
    const FunctionSignature* inaccessible_result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    const std::array<syntax::ModuleId, 2> modules {this->current_module_, this->owner_module(owner_type)};
    std::unordered_set<base::u32> seen_modules;
    for (const syntax::ModuleId module : modules) {
        if (!syntax::is_valid(module) || !seen_modules.insert(module.value).second) {
            continue;
        }
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
        if (result != nullptr) {
            report(
                range,
                sema_ambiguous_method_message(
                    checked_.types.display_name(owner_type),
                    name,
                    module_name(result_module),
                    module_name(module)
                )
            );
            return nullptr;
        }
        result = &signature;
        result_module = module;
    }
    if (result == nullptr && report_unknown) {
        if (inaccessible_result != nullptr) {
            report(range, sema_private_method_message(checked_.types.display_name(owner_type), name));
            return nullptr;
        }
        report(range, sema_unknown_method_message(checked_.types.display_name(owner_type), name));
    }
    return result;
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

    if (report_unknown) {
        report(range, sema_unknown_type_message(name));
    }
    return INVALID_TYPE_HANDLE;
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
            report(range, sema_unknown_type_message(name));
        }
        return INVALID_TYPE_HANDLE;
    }

    TypeHandle result = INVALID_TYPE_HANDLE;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const std::string key = module_key(candidate_module, name);
        TypeHandle candidate = INVALID_TYPE_HANDLE;
        if (const auto found = named_types_.find(key); found != named_types_.end()) {
            const auto visibility = type_visibilities_.find(key);
            if (visibility != type_visibilities_.end() && !can_access(candidate_module, visibility->second)) {
                if (candidate_module.value == module.value && report_unknown) {
                    report(range, sema_private_type_message(module_name(candidate_module), name));
                    return INVALID_TYPE_HANDLE;
                }
                continue;
            }
            candidate = found->second;
        } else if (const auto alias_found = checked_.type_aliases.find(key); alias_found != checked_.type_aliases.end()) {
            if (!can_access(candidate_module, alias_found->second.visibility)) {
                if (candidate_module.value == module.value && report_unknown) {
                    report(range, sema_private_type_message(module_name(candidate_module), name));
                    return INVALID_TYPE_HANDLE;
                }
                continue;
            }
            candidate = resolve_type_alias(alias_found->second, opaque_allowed_as_pointee);
        }
        if (!is_valid(candidate)) {
            continue;
        }
        if (is_valid(result)) {
            if (report_unknown) {
                report(range, sema_ambiguous_type_name_message(name, module_name(result_module), module_name(candidate_module)));
            }
            return INVALID_TYPE_HANDLE;
        }
        result = candidate;
        result_module = candidate_module;
    }
    if (is_valid(result)) {
        return result;
    }

    if (report_unknown) {
        report(range, sema_unknown_type_in_module_message(module_name(module), name));
    }
    return INVALID_TYPE_HANDLE;
}

const FunctionSignature* SemanticAnalyzer::find_function_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (const auto found = checked_.functions.find(module_key(current_module_, name)); found != checked_.functions.end()) {
        return &found->second;
    }

    if (report_unknown) {
        report(range, sema_unknown_function_message(name));
    }
    return nullptr;
}

const FunctionSignature* SemanticAnalyzer::find_function_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            report(range, sema_unknown_function_in_module_message(module_name(module), name));
        }
        return nullptr;
    }

    const FunctionSignature* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const auto found = checked_.functions.find(module_key(candidate_module, name));
        if (found == checked_.functions.end()) {
            continue;
        }
        if (!can_access(candidate_module, found->second.visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                report(range, sema_private_function_message(module_name(candidate_module), name));
                return nullptr;
            }
            continue;
        }
        if (result != nullptr) {
            if (report_unknown) {
                report(range, sema_ambiguous_function_name_message(name, module_name(result_module), module_name(candidate_module)));
            }
            return nullptr;
        }
        result = &found->second;
        result_module = candidate_module;
    }
    if (result == nullptr && report_unknown) {
        report(range, sema_unknown_function_in_module_message(module_name(module), name));
    }
    return result;
}

const Symbol* SemanticAnalyzer::find_symbol_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            report(range, sema_unknown_name_in_module_message(module_name(module), name));
        }
        return nullptr;
    }

    const Symbol* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const auto found = global_values_.find(module_key(candidate_module, name));
        if (found == global_values_.end()) {
            continue;
        }
        if (!can_access(candidate_module, found->second.visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                report(range, sema_private_name_message(module_name(candidate_module), name));
                return nullptr;
            }
            continue;
        }
        if (result != nullptr) {
            if (report_unknown) {
                report(range, sema_ambiguous_name_message(name, module_name(result_module), module_name(candidate_module)));
            }
            return nullptr;
        }
        result = &found->second;
        result_module = candidate_module;
    }
    if (result == nullptr && report_unknown) {
        report(range, sema_unknown_name_in_module_message(module_name(module), name));
    }
    return result;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_case_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (const auto found = checked_.enum_cases.find(module_key(current_module_, name)); found != checked_.enum_cases.end()) {
        return &found->second;
    }

    if (report_unknown) {
        report(range, sema_unknown_enum_case_message(name));
    }
    return nullptr;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_case_by_type_and_case(
    const TypeHandle enum_type,
    const std::string_view case_name
) const {
    if (!is_valid(enum_type)) {
        return nullptr;
    }
    const auto found = this->enum_cases_by_type_and_case_.find(enum_case_lookup_key(enum_type, case_name));
    if (found == this->enum_cases_by_type_and_case_.end() ||
        found->second == nullptr ||
        !this->checked_.types.same(found->second->type, enum_type) ||
        found->second->case_name != case_name) {
        return nullptr;
    }
    return found->second;
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
        return nullptr;
    }
    const TypeHandle enum_type = find_type_in_visible_modules(enum_name, range, false);
    if (!is_valid(enum_type) || checked_.types.get(enum_type).kind != TypeKind::enum_) {
        if (is_valid(enum_type) && report_unknown) {
            report(range, std::string(SEMA_ENUM_CASE_SCOPE_TYPE));
        }
        return nullptr;
    }
    if (const EnumCaseInfo* result = find_enum_case_by_type_and_case(enum_type, case_name); result != nullptr) {
        return result;
    }
    if (report_unknown) {
        report(range, sema_unknown_scoped_enum_case_message(enum_name, case_name));
    }
    return nullptr;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_case_by_pattern_type(
    const syntax::TypeId enum_type_id,
    const std::string_view case_name,
    const base::SourceRange range
) {
    const TypeHandle enum_type = this->resolve_type(enum_type_id);
    if (!is_valid(enum_type)) {
        return nullptr;
    }
    if (checked_.types.get(enum_type).kind != TypeKind::enum_) {
        report(range, std::string(SEMA_ENUM_CASE_SCOPE_TYPE));
        return nullptr;
    }
    if (const EnumCaseInfo* result = find_enum_case_by_type_and_case(enum_type, case_name); result != nullptr) {
        return result;
    }
    report(range, sema_unknown_scoped_enum_case_message(checked_.types.display_name(enum_type), case_name));
    return nullptr;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_constructor(const syntax::ExprId callee_id, const bool report_unknown) {
    if (!syntax::is_valid(callee_id) || callee_id.value >= module_.exprs.size()) {
        return nullptr;
    }
    const syntax::ExprNode& callee = module_.exprs[callee_id.value];
    if (callee.kind != syntax::ExprKind::field ||
        !syntax::is_valid(callee.object) ||
        callee.object.value >= module_.exprs.size()) {
        return nullptr;
    }
    const TypeHandle enum_type = this->resolve_type_selector(callee.object, report_unknown);
    if (!is_valid(enum_type)) {
        return nullptr;
    }
    if (checked_.types.get(enum_type).kind != TypeKind::enum_) {
        if (report_unknown) {
            report(callee.range, std::string(SEMA_ENUM_CASE_SCOPE_TYPE));
        }
        return nullptr;
    }
    if (const EnumCaseInfo* result = find_enum_case_by_type_and_case(enum_type, callee.field_name); result != nullptr) {
        return result;
    }
    if (report_unknown) {
        report(callee.range, sema_unknown_scoped_enum_case_message(checked_.types.display_name(enum_type), callee.field_name));
    }
    return nullptr;
}

const Symbol* SemanticAnalyzer::find_symbol(const std::string_view name, const base::SourceRange range) {
    if (const Symbol* local = symbols_.find(name); local != nullptr) {
        return local;
    }

    if (const auto found = global_values_.find(module_key(current_module_, name)); found != global_values_.end()) {
        return &found->second;
    }

    report(range, sema_unknown_name_message(name));
    return nullptr;
}

} // namespace aurex::sema
