#include <aurex/sema/sema.hpp>

#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace aurex::sema {

namespace {

constexpr std::string_view SEMA_GENERIC_INSTANCE_SEPARATOR = "$";
constexpr std::string_view SEMA_GENERIC_ARG_LIST_OPEN = "[";
constexpr std::string_view SEMA_GENERIC_ARG_LIST_CLOSE = "]";
constexpr std::string_view SEMA_GENERIC_ARG_LIST_SEPARATOR = ",";
constexpr std::string_view SEMA_GENERIC_KEY_ARG_PREFIX = "t";
constexpr std::string_view SEMA_GENERIC_KEY_ARG_SEPARATOR = ".";
constexpr std::string_view SEMA_GENERIC_ABI_SUFFIX_PREFIX = "__aurexg";
constexpr std::string_view SEMA_GENERIC_ABI_ARG_PREFIX = "_t";
constexpr std::string_view SEMA_CAPABILITY_COPY = "Copy";
constexpr std::string_view SEMA_CAPABILITY_DROP = "Drop";

[[nodiscard]] GenericSideTables make_generic_side_tables(const syntax::AstModule& module) {
    static_cast<void>(module);
    GenericSideTables side_tables;
    side_tables.sparse = true;
    return side_tables;
}

[[nodiscard]] std::optional<CapabilityKind> parse_capability_kind(const std::string_view name) noexcept {
    if (name == capability_name(CapabilityKind::sized)) {
        return CapabilityKind::sized;
    }
    if (name == capability_name(CapabilityKind::eq)) {
        return CapabilityKind::eq;
    }
    if (name == capability_name(CapabilityKind::ord)) {
        return CapabilityKind::ord;
    }
    if (name == capability_name(CapabilityKind::hash)) {
        return CapabilityKind::hash;
    }
    return std::nullopt;
}

[[nodiscard]] bool is_resource_capability(const std::string_view name) noexcept {
    return name == SEMA_CAPABILITY_COPY || name == SEMA_CAPABILITY_DROP;
}

} // namespace

std::string_view capability_name(const CapabilityKind capability) noexcept {
    switch (capability) {
    case CapabilityKind::sized:
        return "Sized";
    case CapabilityKind::eq:
        return "Eq";
    case CapabilityKind::ord:
        return "Ord";
    case CapabilityKind::hash:
        return "Hash";
    }
    return "<invalid>";
}

bool SemanticAnalyzer::has_generic_params(const syntax::ItemNode& item) const noexcept {
    return !item.generic_params.empty();
}

bool SemanticAnalyzer::has_generic_constraints(const syntax::ItemNode& item) const noexcept {
    return !item.where_constraints.empty();
}

void SemanticAnalyzer::validate_generic_parameter_list(const syntax::ItemNode& item) {
    std::unordered_map<std::string, base::SourceRange> seen;
    seen.reserve(item.generic_params.size());
    for (const syntax::GenericParamDecl& param : item.generic_params) {
        const std::string name(param.name);
        const auto [first, inserted] = seen.emplace(name, param.range);
        if (!inserted) {
            this->report(param.range, sema_duplicate_generic_parameter_message(param.name));
            this->diagnostics_.push(base::Diagnostic {
                base::Severity::note,
                first->second,
                sema_first_generic_parameter_message(param.name),
            });
        }
    }
}

void SemanticAnalyzer::validate_generic_constraints(
    const syntax::ItemNode& item,
    GenericTemplateInfo& info
) {
    info.constraints.reserve(item.generic_params.size());
    std::unordered_set<std::string> params;
    params.reserve(item.generic_params.size());
    for (const syntax::GenericParamDecl& param : item.generic_params) {
        params.insert(std::string(param.name));
    }

    for (const syntax::GenericConstraintDecl& constraint : item.where_constraints) {
        const std::string param_name(constraint.param_name);
        if (!params.contains(param_name)) {
            this->report(constraint.param_range, sema_unknown_generic_constraint_param_message(constraint.param_name));
            continue;
        }
        std::unordered_set<CapabilityKind, CapabilityKindHash>& capabilities = info.constraints[param_name];
        for (base::usize i = 0; i < constraint.capability_names.size(); ++i) {
            const std::string_view capability_name = constraint.capability_names[i];
            const base::SourceRange capability_range =
                i < constraint.capability_ranges.size() ? constraint.capability_ranges[i] : constraint.range;
            if (is_resource_capability(capability_name)) {
                this->report(capability_range, std::string(SEMA_GENERIC_RESOURCE_CAPABILITY_UNSUPPORTED));
                continue;
            }
            const std::optional<CapabilityKind> capability = parse_capability_kind(capability_name);
            if (!capability.has_value()) {
                this->report(capability_range, sema_unknown_capability_message(capability_name));
                continue;
            }
            if (!capabilities.insert(*capability).second) {
                this->report(capability_range, sema_duplicate_capability_message(constraint.param_name, capability_name));
            }
        }
    }
}

bool SemanticAnalyzer::generic_param_has_capability(
    const std::string_view param,
    const CapabilityKind capability
) const {
    if (this->current_generic_context_ == nullptr) {
        return false;
    }
    const auto identity = this->current_generic_context_->param_identities.find(std::string(param));
    if (identity != this->current_generic_context_->param_identities.end()) {
        const auto by_identity = this->current_generic_context_->constraints_by_identity.find(identity->second);
        if (by_identity != this->current_generic_context_->constraints_by_identity.end()) {
            return by_identity->second.contains(capability);
        }
    }
    const auto found = this->current_generic_context_->constraints.find(std::string(param));
    return found != this->current_generic_context_->constraints.end() &&
           found->second.contains(capability);
}

bool SemanticAnalyzer::generic_param_has_capability(
    const TypeHandle param,
    const CapabilityKind capability
) const {
    if (this->current_generic_context_ == nullptr || !is_valid(param)) {
        return false;
    }
    const TypeInfo& info = this->checked_.types.get(param);
    if (info.kind != TypeKind::generic_param) {
        return false;
    }
    const std::string identity = this->generic_param_identity_key(info);
    if (!identity.empty()) {
        const auto found = this->current_generic_context_->constraints_by_identity.find(identity);
        if (found != this->current_generic_context_->constraints_by_identity.end()) {
            return found->second.contains(capability);
        }
    }
    return this->generic_param_has_capability(info.name, capability);
}

bool SemanticAnalyzer::type_satisfies_capability(
    const TypeHandle type,
    const CapabilityKind capability
) const {
    if (!is_valid(type)) {
        return false;
    }
    const TypeInfo& info = this->checked_.types.get(type);
    if (info.kind == TypeKind::generic_param) {
        return this->generic_param_has_capability(type, capability);
    }
    if (capability == CapabilityKind::sized) {
        return this->is_valid_storage_type(type);
    }
    if (capability == CapabilityKind::eq) {
        return this->type_satisfies_equality_capability(type);
    }
    if (capability == CapabilityKind::ord) {
        return this->type_satisfies_ordering_capability(type);
    }
    if (capability == CapabilityKind::hash) {
        return this->type_supports_hash_capability(type);
    }
    return false;
}

bool SemanticAnalyzer::type_satisfies_equality_capability(const TypeHandle type) const {
    if (!is_valid(type)) {
        return false;
    }
    const TypeInfo& info = this->checked_.types.get(type);
    return this->checked_.types.is_bool(type) ||
           this->checked_.types.is_char(type) ||
           this->checked_.types.is_integer(type) ||
           this->checked_.types.is_pointer(type) ||
           (info.kind == TypeKind::enum_ && !is_valid(info.enum_payload_storage));
}

bool SemanticAnalyzer::type_satisfies_ordering_capability(const TypeHandle type) const {
    return this->checked_.types.is_integer(type);
}

bool SemanticAnalyzer::type_supports_equality_operator(const TypeHandle type) const {
    if (!is_valid(type)) {
        return false;
    }
    const TypeInfo& info = this->checked_.types.get(type);
    return this->checked_.types.is_bool(type) ||
           this->checked_.types.is_char(type) ||
           this->checked_.types.is_integer(type) ||
           this->checked_.types.is_float(type) ||
           this->checked_.types.is_pointer(type) ||
           (info.kind == TypeKind::enum_ && !is_valid(info.enum_payload_storage));
}

bool SemanticAnalyzer::type_supports_ordering_operator(const TypeHandle type) const {
    return this->checked_.types.is_integer(type) || this->checked_.types.is_float(type);
}

bool SemanticAnalyzer::type_supports_hash_capability(const TypeHandle type) const {
    if (!is_valid(type)) {
        return false;
    }
    return this->checked_.types.is_bool(type) ||
           this->checked_.types.is_char(type) ||
           this->checked_.types.is_integer(type) ||
           this->checked_.types.is_pointer(type);
}

bool SemanticAnalyzer::validate_generic_arguments(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const base::SourceRange use_range
) {
    bool ok = true;
    for (base::usize i = 0; i < info.params.size() && i < args.size(); ++i) {
        const auto found = info.constraints.find(info.params[i]);
        if (found == info.constraints.end()) {
            continue;
        }
        for (const CapabilityKind capability : found->second) {
            if (!this->type_satisfies_capability(args[i], capability)) {
                this->report(
                    use_range,
                    sema_generic_capability_not_satisfied_message(
                        this->checked_.types.display_name(args[i]),
                        capability_name(capability)
                    )
                );
                ok = false;
            }
        }
    }
    return ok;
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
    this->validate_generic_constraints(item, info);

    if (item.kind == syntax::ItemKind::struct_decl) {
        if (this->named_types_.contains(info.key) ||
            this->checked_.type_aliases.contains(info.key) ||
            this->generic_struct_templates_.contains(info.key) ||
            this->generic_enum_templates_.contains(info.key) ||
            this->generic_type_alias_templates_.contains(info.key)) {
            this->report(item.range, sema_duplicate_type_definition_message(this->module_name(owner), item.name));
            return;
        }
        auto inserted = this->generic_struct_templates_.emplace(info.key, std::move(info));
        if (inserted.second) {
            this->index_generic_struct_template(inserted.first->second);
        }
        return;
    }

    if (item.kind == syntax::ItemKind::enum_decl) {
        if (this->named_types_.contains(info.key) ||
            this->checked_.type_aliases.contains(info.key) ||
            this->generic_struct_templates_.contains(info.key) ||
            this->generic_enum_templates_.contains(info.key) ||
            this->generic_type_alias_templates_.contains(info.key)) {
            this->report(item.range, sema_duplicate_type_definition_message(this->module_name(owner), item.name));
            return;
        }
        auto inserted = this->generic_enum_templates_.emplace(info.key, std::move(info));
        if (inserted.second) {
            this->index_generic_enum_template(inserted.first->second);
        }
        return;
    }

    if (item.kind == syntax::ItemKind::type_alias) {
        if (this->named_types_.contains(info.key) ||
            this->checked_.type_aliases.contains(info.key) ||
            this->generic_struct_templates_.contains(info.key) ||
            this->generic_enum_templates_.contains(info.key) ||
            this->generic_type_alias_templates_.contains(info.key)) {
            this->report(item.range, sema_duplicate_type_definition_message(this->module_name(owner), item.name));
            return;
        }
        auto inserted = this->generic_type_alias_templates_.emplace(info.key, std::move(info));
        if (inserted.second) {
            this->index_generic_type_alias_template(inserted.first->second);
        }
        return;
    }

    if (item.kind != syntax::ItemKind::fn_decl) {
        if (item.kind != syntax::ItemKind::impl_block) {
            this->report(item.range, std::string(SEMA_GENERIC_PARAMS_UNSUPPORTED_ON_ITEM));
        }
        return;
    }

    if (item.visibility == syntax::Visibility::public_ && !syntax::is_valid(item.return_type)) {
        this->report(item.range, std::string(SEMA_PUBLIC_FUNCTION_RETURN_TYPE_EXPLICIT));
    }
    if (item.is_extern_c || item.is_export_c || item.is_prototype) {
        this->report(item.range, std::string(SEMA_GENERIC_C_ABI_OR_PROTOTYPE_UNSUPPORTED));
    }
    if (syntax::is_valid(item.impl_type)) {
        std::string method_template_name = "#generic_method:";
        method_template_name += std::to_string(item_id.value);
        method_template_name.push_back('.');
        method_template_name += item.name;
        info.key = this->module_key(owner, method_template_name);

        const syntax::ModuleId previous_module = this->current_module_;
        GenericContext generic_context;
        this->populate_generic_placeholder_context(info, generic_context);
        GenericContext* const previous_generic_context = this->current_generic_context_;
        const GenericSideTableScope previous_side_tables = this->current_side_tables_;
        this->current_module_ = owner;
        this->current_generic_context_ = &generic_context;
        this->current_side_tables_.cache_syntax_types = false;
        info.impl_type_pattern = this->resolve_type(item.impl_type);
        this->current_module_ = previous_module;
        this->current_generic_context_ = previous_generic_context;
        this->current_side_tables_ = previous_side_tables;
        if (!is_valid(info.impl_type_pattern)) {
            return;
        }
        const TypeKind impl_type_kind = this->checked_.types.get(info.impl_type_pattern).kind;
        if (impl_type_kind != TypeKind::struct_ &&
            impl_type_kind != TypeKind::enum_ &&
            impl_type_kind != TypeKind::opaque_struct) {
            this->report(item.range, std::string(SEMA_IMPL_TARGET_NAMED_TYPE));
            return;
        }

        std::unordered_set<std::string> owner_params;
        std::vector<TypeHandle> pending;
        pending.push_back(info.impl_type_pattern);
        while (!pending.empty()) {
            const TypeHandle current = pending.back();
            pending.pop_back();
            if (!is_valid(current)) {
                continue;
            }
            const TypeInfo& type_info = this->checked_.types.get(current);
            if (type_info.kind == TypeKind::generic_param) {
                owner_params.insert(this->generic_param_identity_key(type_info));
                continue;
            }
            if (type_info.kind == TypeKind::pointer || type_info.kind == TypeKind::reference) {
                pending.push_back(type_info.pointee);
            } else if (type_info.kind == TypeKind::slice) {
                pending.push_back(type_info.slice_element);
            } else if (type_info.kind == TypeKind::array) {
                pending.push_back(type_info.array_element);
            } else if (type_info.kind == TypeKind::tuple) {
                for (const TypeHandle element : type_info.tuple_elements) {
                    pending.push_back(element);
                }
            } else if (type_info.kind == TypeKind::function) {
                pending.push_back(type_info.function_return);
                for (const TypeHandle param : type_info.function_params) {
                    pending.push_back(param);
                }
            } else if (type_info.kind == TypeKind::struct_ || type_info.kind == TypeKind::enum_) {
                for (const TypeHandle arg : type_info.generic_args) {
                    pending.push_back(arg);
                }
            }
        }
        bool method_local_generic = false;
        for (base::usize i = 0; i < info.params.size(); ++i) {
            if (!owner_params.contains(this->generic_param_identity_key(info, i))) {
                method_local_generic = true;
            }
        }
        if (method_local_generic) {
            this->report(item.range, std::string(SEMA_GENERIC_METHODS_UNSUPPORTED));
            return;
        }
        if (this->type_member_name_exists(info.impl_type_pattern, item.name)) {
            this->report(
                item.range,
                sema_duplicate_type_member_message(
                    this->checked_.types.display_name(info.impl_type_pattern),
                    item.name
                )
            );
            return;
        }
        auto inserted = this->generic_method_templates_.emplace(info.key, std::move(info));
        if (inserted.second) {
            this->index_generic_method_template(inserted.first->second);
        }
        return;
    }
    if (this->checked_.functions.contains(info.key) || this->generic_function_templates_.contains(info.key)) {
        this->report(item.range, sema_duplicate_function_definition_message(this->module_name(owner), item.name));
        return;
    }
    auto inserted = this->generic_function_templates_.emplace(info.key, std::move(info));
    if (inserted.second) {
        this->index_generic_function_template(inserted.first->second);
    }
}

std::string SemanticAnalyzer::generic_param_identity_key(
    const GenericTemplateInfo& info,
    const base::usize index
) const {
    std::string key = info.key;
    key += "#param:";
    key += std::to_string(index);
    if (index < info.params.size()) {
        key.push_back(':');
        key += info.params[index];
    }
    if (syntax::is_valid(info.item) &&
        info.item.value < this->module_.items.size() &&
        index < this->module_.items[info.item.value].generic_params.size()) {
        const base::SourceRange range = this->module_.items[info.item.value].generic_params[index].range;
        key.push_back('@');
        key += std::to_string(range.source.value);
        key.push_back(':');
        key += std::to_string(range.begin);
        key.push_back(':');
        key += std::to_string(range.end);
    }
    return key;
}

std::string SemanticAnalyzer::generic_param_identity_key(const TypeInfo& info) const {
    if (!info.generic_identity_key.empty()) {
        return info.generic_identity_key;
    }
    return info.name;
}

TypeHandle SemanticAnalyzer::generic_param_placeholder(
    const GenericTemplateInfo& info,
    const base::usize index
) {
    if (index >= info.params.size()) {
        return INVALID_TYPE_HANDLE;
    }
    return this->checked_.types.generic_param(this->generic_param_identity_key(info, index), info.params[index]);
}

void SemanticAnalyzer::populate_generic_placeholder_context(
    const GenericTemplateInfo& info,
    GenericContext& context
) {
    context.params.clear();
    context.param_identities.clear();
    context.constraints = info.constraints;
    context.constraints_by_identity.clear();
    context.params.reserve(info.params.size());
    context.param_identities.reserve(info.params.size());
    context.constraints_by_identity.reserve(info.params.size());
    for (base::usize i = 0; i < info.params.size(); ++i) {
        const std::string identity = this->generic_param_identity_key(info, i);
        context.params.emplace(info.params[i], this->generic_param_placeholder(info, i));
        context.param_identities.emplace(info.params[i], identity);
        if (const auto constraints = info.constraints.find(info.params[i]); constraints != info.constraints.end()) {
            context.constraints_by_identity.emplace(identity, constraints->second);
        }
    }
}

void SemanticAnalyzer::populate_generic_concrete_context(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    GenericContext& context
) {
    context.params.clear();
    context.param_identities.clear();
    context.constraints = info.constraints;
    context.constraints_by_identity.clear();
    context.params.reserve(info.params.size());
    context.param_identities.reserve(info.params.size());
    context.constraints_by_identity.reserve(info.params.size());
    for (base::usize i = 0; i < info.params.size() && i < args.size(); ++i) {
        const std::string identity = this->generic_param_identity_key(info, i);
        context.params.emplace(info.params[i], args[i]);
        context.param_identities.emplace(info.params[i], identity);
        const auto constraints = info.constraints.find(info.params[i]);
        if (constraints == info.constraints.end()) {
            continue;
        }
        context.constraints_by_identity.emplace(identity, constraints->second);
        if (is_valid(args[i])) {
            const TypeInfo& arg_info = this->checked_.types.get(args[i]);
            if (arg_info.kind == TypeKind::generic_param) {
                context.constraints_by_identity[this->generic_param_identity_key(arg_info)].insert(
                    constraints->second.begin(),
                    constraints->second.end()
                );
            }
        }
    }
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

std::string SemanticAnalyzer::generic_instance_key_suffix(const std::vector<TypeHandle>& args) const {
    std::string suffix;
    suffix += SEMA_GENERIC_KEY_ARG_PREFIX;
    for (base::usize i = 0; i < args.size(); ++i) {
        if (i != 0) {
            suffix += SEMA_GENERIC_KEY_ARG_SEPARATOR;
        }
        suffix += std::to_string(args[i].value);
    }
    return suffix;
}

std::string SemanticAnalyzer::generic_instance_abi_suffix(const std::vector<TypeHandle>& args) const {
    std::string suffix(SEMA_GENERIC_ABI_SUFFIX_PREFIX);
    for (const TypeHandle arg : args) {
        suffix += SEMA_GENERIC_ABI_ARG_PREFIX;
        suffix += std::to_string(arg.value);
    }
    return suffix;
}

std::string SemanticAnalyzer::generic_struct_instance_key(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    return info.key + std::string(SEMA_GENERIC_INSTANCE_SEPARATOR) + this->generic_instance_key_suffix(args);
}

std::string SemanticAnalyzer::generic_enum_instance_key(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    return info.key + std::string(SEMA_GENERIC_INSTANCE_SEPARATOR) + this->generic_instance_key_suffix(args);
}

std::string SemanticAnalyzer::generic_type_alias_instance_key(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    return info.key + std::string(SEMA_GENERIC_INSTANCE_SEPARATOR) + this->generic_instance_key_suffix(args);
}

std::string SemanticAnalyzer::generic_function_instance_key(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    return info.key + std::string(SEMA_GENERIC_INSTANCE_SEPARATOR) + this->generic_instance_key_suffix(args);
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_struct_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->current_module_, name);
    if (is_valid(lookup_key)) {
        if (const auto found = this->generic_struct_templates_by_name_.find(lookup_key);
            found != this->generic_struct_templates_by_name_.end()) {
            return found->second;
        }
    }
    if (!this->generic_struct_lookup_complete()) {
        if (const auto found = this->generic_struct_templates_.find(this->module_key(this->current_module_, name));
            found != this->generic_struct_templates_.end()) {
            return &found->second;
        }
    }

    if (report_unknown) {
        this->report(range, sema_unknown_generic_type_message(name));
    }
    return nullptr;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_struct_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->report(range, sema_unknown_generic_type_message(name));
        }
        return nullptr;
    }
    const GenericTemplateInfo* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const GenericTemplateInfo* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(candidate_module, name);
        if (is_valid(lookup_key)) {
            if (const auto found = this->generic_struct_templates_by_name_.find(lookup_key);
                found != this->generic_struct_templates_by_name_.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr && !this->generic_struct_lookup_complete()) {
            const auto found = this->generic_struct_templates_.find(this->module_key(candidate_module, name));
            if (found != this->generic_struct_templates_.end()) {
                candidate = &found->second;
            }
        }
        if (candidate == nullptr) {
            continue;
        }
        if (!this->can_access(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->report(range, sema_private_generic_type_message(this->module_name(candidate_module), name));
                return nullptr;
            }
            continue;
        }
        if (result != nullptr) {
            this->report(
                range,
                sema_ambiguous_generic_type_name_message(
                    name,
                    this->module_name(result_module),
                    this->module_name(candidate_module)
                )
            );
            return nullptr;
        }
        result = candidate;
        result_module = candidate_module;
    }
    if (result == nullptr && report_unknown) {
        this->report(range, sema_unknown_generic_type_in_module_message(this->module_name(module), name));
    }
    return result;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_enum_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->current_module_, name);
    if (is_valid(lookup_key)) {
        if (const auto found = this->generic_enum_templates_by_name_.find(lookup_key);
            found != this->generic_enum_templates_by_name_.end()) {
            return found->second;
        }
    }
    if (!this->generic_enum_lookup_complete()) {
        if (const auto found = this->generic_enum_templates_.find(this->module_key(this->current_module_, name));
            found != this->generic_enum_templates_.end()) {
            return &found->second;
        }
    }

    if (report_unknown) {
        this->report(range, sema_unknown_generic_type_message(name));
    }
    return nullptr;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_enum_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->report(range, sema_unknown_generic_type_message(name));
        }
        return nullptr;
    }
    const GenericTemplateInfo* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const GenericTemplateInfo* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(candidate_module, name);
        if (is_valid(lookup_key)) {
            if (const auto found = this->generic_enum_templates_by_name_.find(lookup_key);
                found != this->generic_enum_templates_by_name_.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr && !this->generic_enum_lookup_complete()) {
            const auto found = this->generic_enum_templates_.find(this->module_key(candidate_module, name));
            if (found != this->generic_enum_templates_.end()) {
                candidate = &found->second;
            }
        }
        if (candidate == nullptr) {
            continue;
        }
        if (!this->can_access(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->report(range, sema_private_generic_type_message(this->module_name(candidate_module), name));
                return nullptr;
            }
            continue;
        }
        if (result != nullptr) {
            this->report(
                range,
                sema_ambiguous_generic_type_name_message(
                    name,
                    this->module_name(result_module),
                    this->module_name(candidate_module)
                )
            );
            return nullptr;
        }
        result = candidate;
        result_module = candidate_module;
    }
    if (result == nullptr && report_unknown) {
        this->report(range, sema_unknown_generic_type_in_module_message(this->module_name(module), name));
    }
    return result;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_type_alias_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->current_module_, name);
    if (is_valid(lookup_key)) {
        if (const auto found = this->generic_type_alias_templates_by_name_.find(lookup_key);
            found != this->generic_type_alias_templates_by_name_.end()) {
            return found->second;
        }
    }
    if (!this->generic_type_alias_lookup_complete()) {
        if (const auto found = this->generic_type_alias_templates_.find(this->module_key(this->current_module_, name));
            found != this->generic_type_alias_templates_.end()) {
            return &found->second;
        }
    }

    if (report_unknown) {
        this->report(range, sema_unknown_generic_type_message(name));
    }
    return nullptr;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_type_alias_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->report(range, sema_unknown_generic_type_message(name));
        }
        return nullptr;
    }
    const GenericTemplateInfo* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const GenericTemplateInfo* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(candidate_module, name);
        if (is_valid(lookup_key)) {
            if (const auto found = this->generic_type_alias_templates_by_name_.find(lookup_key);
                found != this->generic_type_alias_templates_by_name_.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr && !this->generic_type_alias_lookup_complete()) {
            const auto found = this->generic_type_alias_templates_.find(this->module_key(candidate_module, name));
            if (found != this->generic_type_alias_templates_.end()) {
                candidate = &found->second;
            }
        }
        if (candidate == nullptr) {
            continue;
        }
        if (!this->can_access(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->report(range, sema_private_generic_type_message(this->module_name(candidate_module), name));
                return nullptr;
            }
            continue;
        }
        if (result != nullptr) {
            this->report(
                range,
                sema_ambiguous_generic_type_name_message(
                    name,
                    this->module_name(result_module),
                    this->module_name(candidate_module)
                )
            );
            return nullptr;
        }
        result = candidate;
        result_module = candidate_module;
    }
    if (result == nullptr && report_unknown) {
        this->report(range, sema_unknown_generic_type_in_module_message(this->module_name(module), name));
    }
    return result;
}

bool SemanticAnalyzer::generic_type_template_exists_in_module(
    const syntax::ModuleId module,
    const std::string_view name
) const {
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        if (const GenericTemplateInfo* const found = this->find_any_generic_type_template_in_module(candidate_module, name);
            found != nullptr && this->can_access(candidate_module, found->visibility)) {
            return true;
        }
    }
    return false;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_any_generic_type_template_in_module(
    const syntax::ModuleId module,
    const std::string_view name
) const {
    if (!syntax::is_valid(module)) {
        return nullptr;
    }
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(module, name);
    if (is_valid(lookup_key)) {
        if (const auto found = this->generic_struct_templates_by_name_.find(lookup_key);
            found != this->generic_struct_templates_by_name_.end()) {
            return found->second;
        }
        if (const auto found = this->generic_enum_templates_by_name_.find(lookup_key);
            found != this->generic_enum_templates_by_name_.end()) {
            return found->second;
        }
        if (const auto found = this->generic_type_alias_templates_by_name_.find(lookup_key);
            found != this->generic_type_alias_templates_by_name_.end()) {
            return found->second;
        }
    }
    if (!this->generic_struct_lookup_complete() ||
        !this->generic_enum_lookup_complete() ||
        !this->generic_type_alias_lookup_complete()) {
        const std::string key = this->module_key(module, name);
        if (const auto found = this->generic_struct_templates_.find(key);
            found != this->generic_struct_templates_.end()) {
            return &found->second;
        }
        if (const auto found = this->generic_enum_templates_.find(key);
            found != this->generic_enum_templates_.end()) {
            return &found->second;
        }
        if (const auto found = this->generic_type_alias_templates_.find(key);
            found != this->generic_type_alias_templates_.end()) {
            return &found->second;
        }
    }
    return nullptr;
}

bool SemanticAnalyzer::report_generic_type_requires_args_if_visible(
    const std::string_view name,
    const base::SourceRange range
) {
    if (const GenericTemplateInfo* const found =
            this->find_any_generic_type_template_in_module(this->current_module_, name);
        found != nullptr && this->can_access(this->current_module_, found->visibility)) {
        this->report(range, sema_generic_type_requires_args_message(name));
        return true;
    }
    return false;
}

void SemanticAnalyzer::report_generic_type_template_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range
) {
    if (!syntax::is_valid(module)) {
        this->report(range, sema_unknown_generic_type_message(name));
        return;
    }

    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(candidate_module, name);
        const bool has_struct_template = is_valid(lookup_key) &&
            this->generic_struct_templates_by_name_.contains(lookup_key);
        const bool has_enum_template = is_valid(lookup_key) &&
            this->generic_enum_templates_by_name_.contains(lookup_key);
        const bool has_alias_template = is_valid(lookup_key) &&
            this->generic_type_alias_templates_by_name_.contains(lookup_key);
        const bool fallback_needed = !this->generic_struct_lookup_complete() ||
            !this->generic_enum_lookup_complete() ||
            !this->generic_type_alias_lookup_complete();
        std::string fallback_key;
        if (fallback_needed) {
            fallback_key = this->module_key(candidate_module, name);
        }
        if (has_struct_template ||
            (fallback_needed && this->generic_struct_templates_.contains(fallback_key))) {
            if (const GenericTemplateInfo* info = this->find_any_generic_type_template_in_module(candidate_module, name);
                info != nullptr && this->can_access(candidate_module, info->visibility)) {
                this->report(range, sema_generic_type_requires_args_message(name));
                return;
            }
            static_cast<void>(this->find_generic_struct_in_module(module, name, range, true));
            return;
        }
        if (has_enum_template ||
            (fallback_needed && this->generic_enum_templates_.contains(fallback_key))) {
            if (const GenericTemplateInfo* info = this->find_any_generic_type_template_in_module(candidate_module, name);
                info != nullptr && this->can_access(candidate_module, info->visibility)) {
                this->report(range, sema_generic_type_requires_args_message(name));
                return;
            }
            static_cast<void>(this->find_generic_enum_in_module(module, name, range, true));
            return;
        }
        if (has_alias_template ||
            (fallback_needed && this->generic_type_alias_templates_.contains(fallback_key))) {
            if (const GenericTemplateInfo* info = this->find_any_generic_type_template_in_module(candidate_module, name);
                info != nullptr && this->can_access(candidate_module, info->visibility)) {
                this->report(range, sema_generic_type_requires_args_message(name));
                return;
            }
            static_cast<void>(this->find_generic_type_alias_in_module(module, name, range, true));
            return;
        }
    }
    this->report(range, sema_unknown_generic_type_in_module_message(this->module_name(module), name));
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_function_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->current_module_, name);
    if (is_valid(lookup_key)) {
        if (const auto found = this->generic_function_templates_by_name_.find(lookup_key);
            found != this->generic_function_templates_by_name_.end()) {
            return found->second;
        }
    }
    if (!this->generic_function_lookup_complete()) {
        if (const auto found = this->generic_function_templates_.find(this->module_key(this->current_module_, name));
            found != this->generic_function_templates_.end()) {
            return &found->second;
        }
    }

    if (report_unknown) {
        this->report(range, sema_unknown_generic_function_message(name));
    }
    return nullptr;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_function_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->report(range, sema_unknown_generic_function_message(name));
        }
        return nullptr;
    }
    const GenericTemplateInfo* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const GenericTemplateInfo* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(candidate_module, name);
        if (is_valid(lookup_key)) {
            if (const auto found = this->generic_function_templates_by_name_.find(lookup_key);
                found != this->generic_function_templates_by_name_.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr && !this->generic_function_lookup_complete()) {
            const auto found = this->generic_function_templates_.find(this->module_key(candidate_module, name));
            if (found != this->generic_function_templates_.end()) {
                candidate = &found->second;
            }
        }
        if (candidate == nullptr) {
            continue;
        }
        if (!this->can_access(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->report(range, sema_private_generic_function_message(this->module_name(candidate_module), name));
                return nullptr;
            }
            continue;
        }
        if (result != nullptr) {
            this->report(
                range,
                sema_ambiguous_generic_function_name_message(
                    name,
                    this->module_name(result_module),
                    this->module_name(candidate_module)
                )
            );
            return nullptr;
        }
        result = candidate;
        result_module = candidate_module;
    }
    if (result == nullptr && report_unknown) {
        this->report(range, sema_unknown_generic_function_in_module_message(this->module_name(module), name));
    }
    return result;
}

TypeHandle SemanticAnalyzer::instantiate_generic_struct(
    const GenericTemplateInfo& info,
    const syntax::TypeNode& use_type,
    const syntax::TypeId,
    const std::vector<TypeHandle>& args
) {
    if (args.size() != info.params.size()) {
        this->report(
            use_type.range,
            sema_generic_argument_count_message("generic type arguments", info.name, args.size(), info.params.size())
        );
        return INVALID_TYPE_HANDLE;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return INVALID_TYPE_HANDLE;
        }
    }
    if (!this->validate_generic_arguments(info, args, use_type.range)) {
        return INVALID_TYPE_HANDLE;
    }

    const std::string instance_key = this->generic_struct_instance_key(info, args);
    if (const auto found = this->generic_struct_instances_.find(instance_key); found != this->generic_struct_instances_.end()) {
        return found->second;
    }

    const syntax::ItemNode& item = this->module_.items[info.item.value];
    const std::string display_suffix = this->generic_instance_suffix(args);
    const std::string abi_suffix = this->generic_instance_abi_suffix(args);
    const std::string qualified = this->qualified_name(info.module, item.name) + display_suffix;
    const std::string c_name = this->c_symbol_name(
        info.module,
        std::string(item.name) + abi_suffix
    );

    const TypeHandle handle = this->checked_.types.named_struct(qualified, c_name, false);
    this->checked_.types.set_generic_instance(handle, info.key, args);
    this->generic_struct_instances_[instance_key] = handle;
    this->type_visibilities_[instance_key] = info.visibility;

    StructInfo struct_info;
    struct_info.name = std::string(item.name) + display_suffix;
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
    this->populate_generic_concrete_context(info, args, generic_context);
    GenericContext* const previous_body_generic_context = this->current_generic_context_;
    const GenericSideTableScope previous_side_tables = this->current_side_tables_;
    this->current_module_ = info.module;
    this->current_generic_context_ = &generic_context;
    this->current_side_tables_.cache_syntax_types = false;

    bool contains_array = false;
    std::unordered_set<std::string> seen_fields;
    for (const syntax::FieldDecl& field : item.fields) {
        if (!seen_fields.insert(std::string(field.name)).second) {
            this->report(field.range, sema_duplicate_struct_field_message(field.name));
            continue;
        }
        const TypeHandle field_type = this->resolve_type(field.type);
        if (!this->is_valid_storage_type(field_type)) {
            this->report(field.range, std::string(SEMA_FIELD_STORAGE));
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

TypeHandle SemanticAnalyzer::instantiate_generic_enum(
    const GenericTemplateInfo& info,
    const syntax::TypeNode& use_type,
    const syntax::TypeId,
    const std::vector<TypeHandle>& args
) {
    if (args.size() != info.params.size()) {
        this->report(
            use_type.range,
            sema_generic_argument_count_message("generic type arguments", info.name, args.size(), info.params.size())
        );
        return INVALID_TYPE_HANDLE;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return INVALID_TYPE_HANDLE;
        }
    }
    if (!this->validate_generic_arguments(info, args, use_type.range)) {
        return INVALID_TYPE_HANDLE;
    }

    const std::string instance_key = this->generic_enum_instance_key(info, args);
    if (const auto found = this->generic_enum_instances_.find(instance_key); found != this->generic_enum_instances_.end()) {
        return found->second;
    }

    const syntax::ItemNode& item = this->module_.items[info.item.value];
    const std::string suffix = this->generic_instance_suffix(args);
    const std::string abi_suffix = this->generic_instance_abi_suffix(args);
    const std::string display_name = std::string(item.name) + suffix;
    const std::string qualified = this->qualified_name(info.module, item.name) + suffix;
    const std::string c_name = this->c_symbol_name(
        info.module,
        std::string(item.name) + abi_suffix
    );

    const TypeHandle handle = this->checked_.types.named_enum(qualified, c_name);
    this->checked_.types.set_generic_instance(handle, info.key, args);
    this->generic_enum_instances_[instance_key] = handle;
    this->type_visibilities_[instance_key] = info.visibility;

    const syntax::ModuleId previous_module = this->current_module_;
    GenericContext generic_context;
    this->populate_generic_concrete_context(info, args, generic_context);
    GenericContext* const previous_generic_context = this->current_generic_context_;
    const GenericSideTableScope previous_side_tables = this->current_side_tables_;
    this->current_module_ = info.module;
    this->current_generic_context_ = &generic_context;
    this->current_side_tables_.cache_syntax_types = false;

    this->register_enum_cases_for_item(
        item,
        info.module,
        handle,
        display_name,
        display_name + "_",
        std::string(item.name) + abi_suffix + "_",
        info.visibility
    );

    this->current_module_ = previous_module;
    this->current_generic_context_ = previous_generic_context;
    this->current_side_tables_ = previous_side_tables;
    return handle;
}

TypeHandle SemanticAnalyzer::instantiate_generic_type_alias(
    const GenericTemplateInfo& info,
    const syntax::TypeNode& use_type,
    const syntax::TypeId,
    const std::vector<TypeHandle>& args,
    const bool opaque_allowed_as_pointee
) {
    if (args.size() != info.params.size()) {
        this->report(
            use_type.range,
            sema_generic_argument_count_message("generic type arguments", info.name, args.size(), info.params.size())
        );
        return INVALID_TYPE_HANDLE;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return INVALID_TYPE_HANDLE;
        }
    }
    if (!this->validate_generic_arguments(info, args, use_type.range)) {
        return INVALID_TYPE_HANDLE;
    }

    const std::string instance_key = this->generic_type_alias_instance_key(info, args);
    if (const auto found = this->resolved_generic_type_aliases_.find(instance_key);
        found != this->resolved_generic_type_aliases_.end()) {
        return found->second;
    }
    if (std::find(this->resolving_type_aliases_.begin(), this->resolving_type_aliases_.end(), instance_key) !=
        this->resolving_type_aliases_.end()) {
        this->report(use_type.range, sema_cyclic_type_alias_message(info.name));
        this->resolved_generic_type_aliases_[instance_key] = INVALID_TYPE_HANDLE;
        return INVALID_TYPE_HANDLE;
    }

    const syntax::ItemNode& item = this->module_.items[info.item.value];
    this->resolving_type_aliases_.push_back(instance_key);
    const syntax::ModuleId previous_module = this->current_module_;
    GenericContext generic_context;
    this->populate_generic_concrete_context(info, args, generic_context);
    GenericContext* const previous_generic_context = this->current_generic_context_;
    const GenericSideTableScope previous_side_tables = this->current_side_tables_;
    this->current_module_ = info.module;
    this->current_generic_context_ = &generic_context;
    this->current_side_tables_.cache_syntax_types = false;

    const TypeHandle resolved = this->resolve_type(item.alias_type, opaque_allowed_as_pointee);

    this->current_module_ = previous_module;
    this->current_generic_context_ = previous_generic_context;
    this->current_side_tables_ = previous_side_tables;
    this->resolving_type_aliases_.pop_back();
    this->resolved_generic_type_aliases_[instance_key] = resolved;
    return resolved;
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
            const std::string identity = this->generic_param_identity_key(pattern_info);
            const auto found = inferred.find(identity);
            if (found == inferred.end()) {
                inferred.emplace(identity, current_actual);
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
        case TypeKind::opaque_struct:
            if (!this->checked_.types.same(current_pattern, current_actual)) {
                return false;
            }
            break;
        case TypeKind::pointer:
            if (pattern_info.pointer_mutability == PointerMutability::mut &&
                actual_info.pointer_mutability != PointerMutability::mut) {
                return false;
            }
            pending.emplace_back(pattern_info.pointee, actual_info.pointee);
            break;
        case TypeKind::reference:
            if (pattern_info.pointer_mutability == PointerMutability::mut &&
                actual_info.pointer_mutability != PointerMutability::mut) {
                return false;
            }
            pending.emplace_back(pattern_info.pointee, actual_info.pointee);
            break;
        case TypeKind::slice:
            if (pattern_info.slice_mutability == PointerMutability::mut &&
                actual_info.slice_mutability != PointerMutability::mut) {
                return false;
            }
            pending.emplace_back(pattern_info.slice_element, actual_info.slice_element);
            break;
        case TypeKind::function:
            if (pattern_info.function_call_conv != actual_info.function_call_conv ||
                pattern_info.function_is_unsafe != actual_info.function_is_unsafe ||
                pattern_info.function_is_variadic != actual_info.function_is_variadic ||
                pattern_info.function_params.size() != actual_info.function_params.size()) {
                return false;
            }
            pending.emplace_back(pattern_info.function_return, actual_info.function_return);
            for (base::usize i = 0; i < pattern_info.function_params.size(); ++i) {
                pending.emplace_back(pattern_info.function_params[i], actual_info.function_params[i]);
            }
            break;
        case TypeKind::tuple:
            if (pattern_info.tuple_elements.size() != actual_info.tuple_elements.size()) {
                return false;
            }
            for (base::usize i = 0; i < pattern_info.tuple_elements.size(); ++i) {
                pending.emplace_back(pattern_info.tuple_elements[i], actual_info.tuple_elements[i]);
            }
            break;
        case TypeKind::array:
            if (pattern_info.array_count != actual_info.array_count) {
                return false;
            }
            pending.emplace_back(pattern_info.array_element, actual_info.array_element);
            break;
        case TypeKind::struct_:
        case TypeKind::enum_:
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
        this->report(call.range, sema_argument_count_message(info.name));
        return false;
    }

    GenericContext generic_context;
    this->populate_generic_placeholder_context(info, generic_context);

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
        const TypeHandle actual = this->analyze_expr(call.args[i], pattern_param_types[i]);
        if (!this->unify_generic_type(pattern_param_types[i], actual, inferred)) {
            this->report(
                this->module_.exprs[call.args[i].value].range,
                sema_generic_call_argument_unify_message(info.name)
            );
            return false;
        }
    }

    args.clear();
    args.reserve(info.params.size());
    for (base::usize i = 0; i < info.params.size(); ++i) {
        const auto found = inferred.find(this->generic_param_identity_key(info, i));
        if (found == inferred.end() || !is_valid(found->second)) {
            this->report(call.range, sema_generic_call_argument_infer_message(info.params[i], info.name));
            return false;
        }
        args.push_back(found->second);
    }
    if (!this->validate_generic_arguments(info, args, call.range)) {
        return false;
    }
    return true;
}

FunctionSignature* SemanticAnalyzer::instantiate_generic_placeholder_function(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const base::SourceRange use_range
) {
    if (args.size() != info.params.size()) {
        this->report(
            use_range,
            sema_generic_argument_count_message("generic function type arguments", info.name, args.size(), info.params.size())
        );
        return nullptr;
    }
    const syntax::ItemNode& function = this->module_.items[info.item.value];

    for (base::usize i = 0; i < info.params.size(); ++i) {
        if (!is_valid(args[i])) {
            return nullptr;
        }
    }

    GenericContext generic_context;
    this->populate_generic_concrete_context(info, args, generic_context);
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
    signature.is_unsafe = function.is_unsafe;
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
        case TypeKind::reference:
            pending.push_back(info.pointee);
            break;
        case TypeKind::slice:
            pending.push_back(info.slice_element);
            break;
        case TypeKind::function:
            pending.push_back(info.function_return);
            for (const TypeHandle param : info.function_params) {
                pending.push_back(param);
            }
            break;
        case TypeKind::tuple:
            for (const TypeHandle element : info.tuple_elements) {
                pending.push_back(element);
            }
            break;
        case TypeKind::array:
            pending.push_back(info.array_element);
            break;
        case TypeKind::struct_:
        case TypeKind::enum_:
            for (const TypeHandle arg : info.generic_args) {
                pending.push_back(arg);
            }
            break;
        case TypeKind::builtin:
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
        this->report(
            use_range,
            sema_generic_argument_count_message("generic function type arguments", info.name, args.size(), info.params.size())
        );
        return nullptr;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return nullptr;
        }
    }
    if (!this->validate_generic_arguments(info, args, use_range)) {
        return nullptr;
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

    GenericContext generic_context;
    this->populate_generic_concrete_context(info, args, generic_context);
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
        info.name + this->generic_instance_abi_suffix(args)
    );
    signature.module = info.module;
    signature.return_type = syntax::is_valid(function.return_type)
        ? this->resolve_type(function.return_type)
        : INVALID_TYPE_HANDLE;
    signature.range = function.range;
    signature.is_unsafe = function.is_unsafe;
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
    } else {
        this->internal_function_lookup_exclusions_ += 1;
    }
    this->function_body_states_[key] = FunctionBodyState::not_started;
    GenericContext body_context;
    this->populate_generic_concrete_context(info, args, body_context);
    GenericContext* const previous_body_generic_context = this->current_generic_context_;
    const GenericSideTableScope previous_body_side_tables = this->current_side_tables_;
    this->current_generic_context_ = &body_context;
    this->current_side_tables_.side_tables = &this->checked_.generic_function_instances[instance_index].side_tables;
    this->current_side_tables_.cache_syntax_types = false;
    this->current_module_ = info.module;
    this->analyze_function_body_with_signature(
        function,
        key,
        this->checked_.generic_function_instances[instance_index].signature,
        this->function_body_states_[key]
    );
    this->current_module_ = previous_module;
    this->current_generic_context_ = previous_body_generic_context;
    this->current_side_tables_ = previous_body_side_tables;
    this->checked_.generic_function_instances[instance_index].signature = this->checked_.functions.at(key);
    return &this->checked_.generic_function_instances[instance_index].signature;
}

FunctionSignature* SemanticAnalyzer::instantiate_generic_method(
    const GenericTemplateInfo& info,
    const TypeHandle owner_type,
    const std::vector<TypeHandle>& args,
    const base::SourceRange use_range
) {
    if (args.size() != info.params.size()) {
        this->report(
            use_range,
            sema_generic_argument_count_message("generic method type arguments", info.name, args.size(), info.params.size())
        );
        return nullptr;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return nullptr;
        }
    }
    if (!this->validate_generic_arguments(info, args, use_range)) {
        return nullptr;
    }

    const std::string key = this->method_key(info.module, owner_type, info.name);
    if (const auto found = this->checked_.functions.find(key); found != this->checked_.functions.end()) {
        return &found->second;
    }

    const syntax::ItemNode& function = this->module_.items[info.item.value];
    GenericContext generic_context;
    this->populate_generic_concrete_context(info, args, generic_context);
    const syntax::ModuleId previous_module = this->current_module_;
    GenericContext* const previous_generic_context = this->current_generic_context_;
    const GenericSideTableScope previous_side_tables = this->current_side_tables_;
    this->current_module_ = info.module;
    this->current_generic_context_ = &generic_context;
    this->current_side_tables_.cache_syntax_types = false;

    FunctionSignature signature;
    signature.name = info.name + this->generic_instance_suffix(args);
    signature.c_name = this->method_c_symbol_name(owner_type, info.name);
    signature.module = info.module;
    signature.method_owner_type = owner_type;
    signature.return_type = syntax::is_valid(function.return_type)
        ? this->resolve_type(function.return_type)
        : INVALID_TYPE_HANDLE;
    signature.range = function.range;
    signature.is_unsafe = function.is_unsafe;
    signature.has_definition = true;
    signature.is_method = true;
    signature.has_self_param = !function.params.empty() && function.params.front().name == "self";
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
    this->index_method_lookup(info.module, owner_type, info.name, function_inserted.first->second);
    this->index_function_value(info.name, function_inserted.first->second);
    this->function_body_states_[key] = FunctionBodyState::not_started;

    GenericContext body_context;
    this->populate_generic_concrete_context(info, args, body_context);
    GenericContext* const previous_body_generic_context = this->current_generic_context_;
    const GenericSideTableScope previous_body_side_tables = this->current_side_tables_;
    this->current_generic_context_ = &body_context;
    this->current_side_tables_.side_tables = &this->checked_.generic_function_instances[instance_index].side_tables;
    this->current_side_tables_.cache_syntax_types = false;
    this->current_module_ = info.module;
    this->analyze_function_body_with_signature(
        function,
        key,
        this->checked_.generic_function_instances[instance_index].signature,
        this->function_body_states_[key]
    );
    this->current_module_ = previous_module;
    this->current_generic_context_ = previous_body_generic_context;
    this->current_side_tables_ = previous_body_side_tables;
    this->checked_.generic_function_instances[instance_index].signature = this->checked_.functions.at(key);
    return &this->checked_.functions.at(key);
}

FunctionSignature* SemanticAnalyzer::find_generic_method_in_visible_modules(
    const TypeHandle owner_type,
    const std::string_view name,
    const base::SourceRange range,
    const bool require_self,
    const bool report_unknown
) {
    FunctionSignature* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    const std::array<syntax::ModuleId, 2> modules {this->current_module_, this->owner_module(owner_type)};
    std::unordered_set<base::u32> seen_modules;
    for (const syntax::ModuleId module : modules) {
        if (!syntax::is_valid(module)) {
            continue;
        }
        if (!seen_modules.insert(module.value).second) {
            continue;
        }
        std::vector<const GenericTemplateInfo*> candidates;
        const auto append_candidate = [&candidates](const GenericTemplateInfo* const info) {
            if (std::find(candidates.begin(), candidates.end(), info) == candidates.end()) {
                candidates.push_back(info);
            }
        };
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(module, name);
        if (is_valid(lookup_key)) {
            if (const auto found = this->generic_method_templates_by_name_.find(lookup_key);
                found != this->generic_method_templates_by_name_.end()) {
                candidates.reserve(found->second.size());
                for (const GenericTemplateInfo* const info : found->second) {
                    append_candidate(info);
                }
            }
        }
        if (!this->generic_method_lookup_complete()) {
            for (const auto& entry : this->generic_method_templates_) {
                const GenericTemplateInfo& info = entry.second;
                if (info.module.value == module.value &&
                    info.name == name) {
                    append_candidate(&info);
                }
            }
        }
        for (const GenericTemplateInfo* const candidate_info : candidates) {
            if (candidate_info == nullptr) {
                continue;
            }
            const GenericTemplateInfo& info = *candidate_info;
            if (!this->can_access(module, info.visibility)) {
                continue;
            }
            std::unordered_map<std::string, TypeHandle> inferred;
            if (!this->unify_generic_type(info.impl_type_pattern, owner_type, inferred)) {
                continue;
            }
            std::vector<TypeHandle> args;
            args.reserve(info.params.size());
            bool all_inferred = true;
            for (base::usize i = 0; i < info.params.size(); ++i) {
                const auto found = inferred.find(this->generic_param_identity_key(info, i));
                if (found == inferred.end() || !is_valid(found->second)) {
                    all_inferred = false;
                    break;
                }
                args.push_back(found->second);
            }
            if (!all_inferred) {
                continue;
            }
            FunctionSignature* candidate = this->instantiate_generic_method(info, owner_type, args, range);
            if (candidate == nullptr || (require_self && !candidate->has_self_param)) {
                continue;
            }
            if (result != nullptr) {
                this->report(
                    range,
                    sema_ambiguous_method_message(
                        this->checked_.types.display_name(owner_type),
                        name,
                        this->module_name(result_module),
                        this->module_name(module)
                    )
                );
                return nullptr;
            }
            result = candidate;
            result_module = module;
        }
    }
    if (result == nullptr && report_unknown) {
        this->report(range, sema_unknown_method_message(this->checked_.types.display_name(owner_type), name));
    }
    return result;
}

void SemanticAnalyzer::analyze_generic_function_definition(const GenericTemplateInfo& info) {
    const syntax::ItemNode& function = this->module_.items[info.item.value];
    GenericContext generic_context;
    this->populate_generic_placeholder_context(info, generic_context);

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
    signature.is_unsafe = function.is_unsafe;
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
    this->populate_generic_placeholder_context(info, generic_context);
    const syntax::ModuleId previous_module = this->current_module_;
    GenericContext* const previous_generic_context = this->current_generic_context_;
    GenericSideTables side_tables = make_generic_side_tables(this->module_);
    GenericSideTableScope previous_side_tables = this->current_side_tables_;
    this->current_generic_context_ = &generic_context;
    this->current_side_tables_.side_tables = &side_tables;
    this->current_side_tables_.cache_syntax_types = false;
    this->current_module_ = info.module;
    this->analyze_function_body_with_signature(function, info.key, signature, state);
    this->current_module_ = previous_module;
    this->current_generic_context_ = previous_generic_context;
    this->current_side_tables_ = previous_side_tables;
}

} // namespace aurex::sema
