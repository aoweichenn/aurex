#include <aurex/frontend/sema/sema_messages.hpp>

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

namespace {

constexpr base::u32 SEMA_RECEIVER_ARGUMENT_COUNT = 1;
constexpr base::usize SEMA_DYNPROJECT_REQUIRED_TYPE_ARGS = 2;
constexpr base::usize SEMA_DYNPROJECT_REQUIRED_ARGS = 1;
constexpr base::usize SEMA_RANGE_VALUE_MIN_ARG_COUNT = 1;
constexpr base::usize SEMA_RANGE_VALUE_MAX_ARG_COUNT = 3;
constexpr std::string_view SEMA_FUNCTION_VALUE_CALL_NAME = "<function>";
constexpr std::string_view SEMA_DYNPROJECT_INTRINSIC_NAME = "dynproject";
constexpr std::string_view SEMA_RANGE_VALUE_INTRINSIC_NAME = "range";

[[nodiscard]] std::string module_selector_path_name(const std::vector<std::string_view>& parts)
{
    std::string name;
    for (base::usize i = 0; i < parts.size(); ++i) {
        if (i != 0) {
            name.push_back('.');
        }
        name += parts[i];
    }
    return name;
}

[[nodiscard]] FunctionCallConv signature_call_conv(const FunctionSignature& signature) noexcept
{
    return signature.is_extern_c || signature.is_export_c ? FunctionCallConv::c : FunctionCallConv::aurex;
}

[[nodiscard]] base::SourceRange call_expr_range_or(
    const syntax::AstModule& module, const syntax::ExprId expr, const base::SourceRange& fallback) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size() ? module.exprs.range(expr.value) : fallback;
}

[[nodiscard]] bool sema_call_valid_expr(const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size();
}

[[nodiscard]] bool call_arg_has_label(
    const std::span<const syntax::CallArgLabelDecl> labels, const base::usize index) noexcept
{
    return index < labels.size() && !labels[index].name.empty();
}

[[nodiscard]] std::string_view call_param_name_or_index(
    const std::span<const FunctionParamInfo> params, const base::usize index) noexcept
{
    if (index < params.size() && !params[index].name.empty()) {
        return params[index].name;
    }
    return "<argument>";
}

[[nodiscard]] std::optional<base::usize> find_call_param_index(
    const std::span<const FunctionParamInfo> params, const std::string_view name, const base::usize receiver_count)
{
    for (base::usize index = receiver_count; index < params.size(); ++index) {
        if (params[index].name == name) {
            return index;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool range_value_binary_result_uses_operand_type(const syntax::BinaryOp op) noexcept
{
    switch (op) {
        case syntax::BinaryOp::add:
        case syntax::BinaryOp::sub:
        case syntax::BinaryOp::mul:
        case syntax::BinaryOp::div:
        case syntax::BinaryOp::mod:
        case syntax::BinaryOp::shl:
        case syntax::BinaryOp::shr:
        case syntax::BinaryOp::bit_and:
        case syntax::BinaryOp::bit_xor:
        case syntax::BinaryOp::bit_or:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] bool range_value_contextual_integer_expr(
    const syntax::AstModule& module, const syntax::ExprId candidate)
{
    std::vector<syntax::ExprId> pending;
    pending.push_back(candidate);
    while (!pending.empty()) {
        const syntax::ExprId current = pending.back();
        pending.pop_back();
        if (!syntax::is_valid(current) || current.value >= module.exprs.size()) {
            return false;
        }
        const syntax::ExprKind kind = module.exprs.kind(current.value);
        if (kind == syntax::ExprKind::integer_literal) {
            continue;
        }
        if (const syntax::UnaryExprPayload* const unary = module.exprs.unary_payload(current.value);
            kind == syntax::ExprKind::unary && unary != nullptr && unary->op == syntax::UnaryOp::numeric_negate) {
            pending.push_back(unary->operand);
            continue;
        }
        if (const syntax::BinaryExprPayload* const binary = module.exprs.binary_payload(current.value);
            kind == syntax::ExprKind::binary && binary != nullptr
            && range_value_binary_result_uses_operand_type(binary->op)) {
            pending.push_back(binary->lhs);
            pending.push_back(binary->rhs);
            continue;
        }
        return false;
    }
    return true;
}

[[nodiscard]] bool is_unqualified_dynproject_selector(
    const SemanticAnalyzerCore::NamedTypeSelector& selector) noexcept
{
    return !selector.qualified && selector.name == SEMA_DYNPROJECT_INTRINSIC_NAME;
}

[[nodiscard]] bool is_unqualified_range_value_call_callee(
    const SemanticAnalyzerCore::ExprView& callee) noexcept
{
    return callee.kind == syntax::ExprKind::name && callee.scope_name.empty()
        && callee.text == SEMA_RANGE_VALUE_INTRINSIC_NAME;
}

[[nodiscard]] bool is_single_trait_object(const TypeTable& types, const TypeHandle type) noexcept
{
    return is_valid(type) && type.value < types.size()
        && types.get(type).kind == TypeKind::trait_object
        && types.get(type).trait_object_principal_types.empty()
        && query::is_valid(types.get(type).trait_object_key);
}

[[nodiscard]] syntax::TypeNode make_dyn_trait_type_from_selector(
    const syntax::TypeNode& source_type, const base::SourceRange& fallback_range)
{
    syntax::TypeNode dyn_trait;
    dyn_trait.kind = syntax::TypeKind::dyn_trait;
    dyn_trait.range = source_type.range.empty() ? fallback_range : source_type.range;
    dyn_trait.scope_name = source_type.scope_name;
    dyn_trait.scope_range = source_type.scope_range;
    dyn_trait.scope_parts = source_type.scope_parts;
    dyn_trait.scope_name_id = source_type.scope_name_id;
    dyn_trait.scope_part_ids = source_type.scope_part_ids;
    dyn_trait.name = source_type.name;
    dyn_trait.name_id = source_type.name_id;
    dyn_trait.type_args = source_type.type_args;
    dyn_trait.associated_type_constraints = source_type.associated_type_constraints;
    return dyn_trait;
}

[[nodiscard]] std::optional<syntax::ExprId> receiver_expr_for_call_callee(
    const syntax::AstModule& module, const syntax::ExprId callee)
{
    syntax::ExprId current = callee;
    std::vector<base::u32> visited;
    while (sema_call_valid_expr(module, current)) {
        if (std::ranges::find(visited, current.value) != visited.end()) {
            return std::nullopt;
        }
        visited.push_back(current.value);
        const syntax::ExprKind kind = module.exprs.kind(current.value);
        if (kind == syntax::ExprKind::generic_apply) {
            const syntax::GenericApplyExprPayload* const apply = module.exprs.generic_apply_payload(current.value);
            if (apply == nullptr) {
                return std::nullopt;
            }
            current = apply->callee;
            continue;
        }
        if (kind == syntax::ExprKind::field) {
            const syntax::FieldExprPayload* const field = module.exprs.field_payload(current.value);
            return field == nullptr ? std::nullopt : std::optional<syntax::ExprId>{field->object};
        }
        return std::nullopt;
    }
    return std::nullopt;
}

struct ReceiverAccessFacts {
    ReceiverAccessKind access = ReceiverAccessKind::none;
    bool auto_borrow = false;
    bool two_phase_eligible = false;
};

[[nodiscard]] ReceiverAccessFacts receiver_access_facts(const TypeTable& types,
    const std::span<const TypeHandle> param_types, const base::u32 receiver_arg_count, const TypeHandle receiver_type)
{
    if (receiver_arg_count == 0 || param_types.empty() || receiver_arg_count > param_types.size()) {
        return {};
    }

    const TypeHandle self_type = param_types.front();
    if (!is_valid(self_type)) {
        return {};
    }
    if (types.is_pointer(self_type) || types.is_reference(self_type)) {
        const TypeInfo& self = types.get(self_type);
        ReceiverAccessFacts facts;
        facts.access = self.pointer_mutability == PointerMutability::mut ? ReceiverAccessKind::mutable_
                                                                         : ReceiverAccessKind::shared;
        const bool receiver_is_indirect = types.is_pointer(receiver_type) || types.is_reference(receiver_type);
        facts.auto_borrow = is_valid(receiver_type) && !receiver_is_indirect && types.same(self.pointee, receiver_type);
        facts.two_phase_eligible =
            facts.auto_borrow && types.is_reference(self_type) && self.pointer_mutability == PointerMutability::mut;
        return facts;
    }
    if (is_valid(receiver_type) && types.same(self_type, receiver_type)) {
        return ReceiverAccessFacts{
            .access = ReceiverAccessKind::consuming,
        };
    }
    return {};
}

} // namespace

TypeHandle SemanticAnalyzerCore::function_type_from_signature(const FunctionSignature& signature)
{
    return this->state_.checked.types.function(signature_call_conv(signature), signature.is_unsafe,
        signature.is_variadic, signature.param_types, signature.return_type);
}

TypeHandle SemanticAnalyzerCore::function_type_from_symbol(const Symbol& symbol, const base::SourceRange& range)
{
    const FunctionSignature* signature = syntax::is_valid(symbol.module)
        ? this->find_function_in_module(symbol.module, symbol.name_id, symbol.name, range, false)
        : nullptr;
    if (signature == nullptr) {
        return symbol.type;
    }
    this->ensure_function_return_known(*signature, range);
    return this->function_type_from_signature(*signature);
}

bool SemanticAnalyzerCore::in_unsafe_context() const noexcept
{
    return this->state_.flow.unsafe_context_depth > 0;
}

void SemanticAnalyzerCore::require_unsafe_context(
    const base::SourceRange& range, const std::string_view operation) const
{
    if (!this->in_unsafe_context()) {
        this->report_unsafe_required(range, std::string(operation));
    }
}

void SemanticAnalyzerCore::validate_unsafe_call(
    const FunctionSignature& signature, const base::SourceRange& range) const
{
    if (signature.is_unsafe && !this->in_unsafe_context()) {
        this->report_unsafe_required(range, sema_unsafe_function_call_message(signature.name));
    }
}

void SemanticAnalyzerCore::validate_unsafe_function_value_call(
    const TypeHandle callee_type, const base::SourceRange& range) const
{
    if (!this->state_.checked.types.is_function(callee_type)) {
        return;
    }
    const TypeInfo& function = this->state_.checked.types.get(callee_type);
    if (function.function_is_unsafe && !this->in_unsafe_context()) {
        this->report_unsafe_required(range, sema_unsafe_function_call_message(SEMA_FUNCTION_VALUE_CALL_NAME));
    }
}

TypeHandle SemanticAnalyzerCore::resolve_associated_type_owner(const syntax::ExprId object, const bool report_unknown)
{
    return this->resolve_type_selector(object, report_unknown);
}

TypeHandle SemanticAnalyzerCore::resolve_associated_generic_type_owner(
    const syntax::ExprId apply, const bool report_unknown)
{
    return this->resolve_type_selector(apply, report_unknown);
}

SemanticAnalyzerCore::ModuleSelectorPath SemanticAnalyzerCore::expr_selector_path(const syntax::ExprId expr_id) const
{
    ModuleSelectorPath path;
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->ctx_.module.exprs.size()) {
        return path;
    }

    path.range = this->ctx_.module.exprs.range(expr_id.value);
    syntax::ExprId current = expr_id;
    while (syntax::is_valid(current) && current.value < this->ctx_.module.exprs.size()) {
        const SemanticAnalyzerCore::ExprView expr = this->expr_view(current);
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

SemanticAnalyzerCore::ModuleSelector SemanticAnalyzerCore::resolve_module_selector(
    const syntax::ExprId expr_id, const bool report_unknown) const
{
    const ModuleSelectorPath path = this->expr_selector_path(expr_id);
    if (path.parts.empty()) {
        return {};
    }

    if (path.parts.size() == 1) {
        const std::string_view name = path.parts.front();
        const IdentId name_id = path.part_ids.front();
        const syntax::ModuleId alias_module = this->resolve_import_alias(name, path.range, false);
        if (syntax::is_valid(alias_module)) {
            return ModuleSelector{alias_module, false};
        }
        const syntax::ModuleId path_module = this->find_visible_module_path(path.parts);
        if (syntax::is_valid(path_module)) {
            return ModuleSelector{path_module, false};
        }
        const bool failed_selector = !this->selector_base_has_non_module_meaning(name_id, name);
        if (failed_selector && report_unknown) {
            if (this->visible_root_module_name_exists(name)) {
                this->report_lookup(path.range, sema_unknown_module_path_message(std::string(name)));
            } else {
                static_cast<void>(this->resolve_import_alias(name, path.range, true));
            }
        }
        return ModuleSelector{syntax::INVALID_MODULE_ID, failed_selector};
    }

    const syntax::ModuleId path_module = this->find_visible_module_path(path.parts);
    if (syntax::is_valid(path_module)) {
        return ModuleSelector{path_module, false};
    }

    const std::string_view root = path.parts.front();
    const IdentId root_id = path.part_ids.front();
    const bool failed_selector = !this->selector_base_has_non_module_meaning(root_id, root)
        && !this->module_alias_visible(root) && !this->visible_module_path_prefix_exists(path.parts);
    if (failed_selector && report_unknown) {
        if (this->visible_root_module_name_exists(root)) {
            this->report_lookup(path.range, sema_unknown_module_path_message(module_selector_path_name(path.parts)));
        } else {
            static_cast<void>(this->resolve_import_alias(root, path.range, true));
        }
    }
    return ModuleSelector{syntax::INVALID_MODULE_ID, failed_selector};
}

SemanticAnalyzerCore::NamedTypeSelector SemanticAnalyzerCore::resolve_named_type_selector(
    const syntax::ExprId expr_id, const bool report_unknown)
{
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->ctx_.module.exprs.size()) {
        return {};
    }
    const SemanticAnalyzerCore::ExprView expr = this->expr_view(expr_id);
    if (expr.kind == syntax::ExprKind::generic_apply) {
        NamedTypeSelector selector = this->resolve_named_type_selector(expr.callee, report_unknown);
        if (selector.name.empty()) {
            return {};
        }
        selector.range = expr.range;
        selector.type_args.assign(expr.type_args.begin(), expr.type_args.end());
        selector.generic_args.assign(expr.generic_args.begin(), expr.generic_args.end());
        return selector;
    }
    if (expr.kind == syntax::ExprKind::name) {
        if (!expr.scope_name.empty()) {
            const syntax::ModuleId module =
                this->resolve_import_alias(expr.scope_name, expr.scope_range, report_unknown);
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
    if (expr.kind != syntax::ExprKind::field || !syntax::is_valid(expr.object)
        || expr.object.value >= this->ctx_.module.exprs.size()) {
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

TypeHandle SemanticAnalyzerCore::resolve_type_selector(const syntax::ExprId expr_id, const bool report_unknown)
{
    const NamedTypeSelector selector = this->resolve_named_type_selector(expr_id, report_unknown);
    if (selector.name.empty()) {
        return INVALID_TYPE_HANDLE;
    }
    return this->resolve_named_type_selector_type(selector, false, report_unknown);
}

TypeHandle SemanticAnalyzerCore::resolve_named_type_selector_type(
    const NamedTypeSelector& selector, const bool opaque_allowed_as_pointee, const bool report_unknown)
{
    if (selector.name.empty()) {
        return INVALID_TYPE_HANDLE;
    }
    if (!selector.qualified && selector.type_args.empty() && selector.generic_args.empty()
        && this->state_.flow.current_generic_context != nullptr) {
        if (const auto found = this->state_.flow.current_generic_context->params.find(selector.name_id);
            found != this->state_.flow.current_generic_context->params.end()) {
            return found->second;
        }
    }
    if (!selector.type_args.empty() || !selector.generic_args.empty()) {
        return this->resolve_generic_type_selector(
            selector, syntax::INVALID_TYPE_ID, opaque_allowed_as_pointee, report_unknown);
    }
    const bool reported_missing_generic_args = !selector.qualified && report_unknown
        && this->report_generic_type_requires_args_if_visible(selector.name_id, selector.name, selector.range);
    const TypeHandle resolved = selector.qualified
        ? this->find_type_in_module(selector.module, selector.name_id, selector.name, selector.range,
              opaque_allowed_as_pointee, report_unknown)
        : this->find_type_in_visible_modules(selector.name_id, selector.name, selector.range, opaque_allowed_as_pointee,
              report_unknown && !reported_missing_generic_args);
    if (is_valid(resolved) && this->state_.checked.types.get(resolved).kind == TypeKind::opaque_struct
        && !opaque_allowed_as_pointee) {
        this->report_general(selector.range, std::string(SEMA_OPAQUE_POINTER_ONLY));
    }
    return resolved;
}

bool SemanticAnalyzerCore::selector_base_has_non_module_meaning(
    const IdentId name_id, const std::string_view name) const
{
    if (this->state_.names.symbols.find(name_id) != nullptr) {
        return true;
    }
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->state_.flow.current_module, name_id);
    if (is_valid(lookup_key)) {
        if (this->state_.names.global_values_by_name.contains(lookup_key)
            || this->state_.names.named_types_by_name.contains(lookup_key)
            || this->state_.names.type_aliases_by_name.contains(lookup_key)) {
            return true;
        }
    }
    if (this->find_any_generic_type_template_in_module(this->state_.flow.current_module, name_id, name) != nullptr) {
        return true;
    }
    return false;
}

TypeHandle SemanticAnalyzerCore::resolve_generic_type_selector(const NamedTypeSelector& selector,
    const syntax::TypeId use_type_id, const bool opaque_allowed_as_pointee, const bool report_unknown)
{
    if (selector.name.empty()) {
        return INVALID_TYPE_HANDLE;
    }

    syntax::TypeNode use_type;
    use_type.kind = syntax::TypeKind::named;
    use_type.name = selector.name;
    use_type.range = selector.range;
    use_type.type_args = selector.type_args;
    use_type.generic_args = selector.generic_args;

    std::vector<syntax::GenericArgDecl> type_only_args;
    std::span<const syntax::GenericArgDecl> ordered_args = selector.generic_args;
    if (ordered_args.empty() && !selector.type_args.empty()) {
        type_only_args.reserve(selector.type_args.size());
        for (const syntax::TypeId type_arg : selector.type_args) {
            type_only_args.push_back(syntax::GenericArgDecl{
                syntax::GenericArgKind::type,
                type_arg,
                syntax::INVALID_EXPR_ID,
                selector.range,
            });
        }
        ordered_args = type_only_args;
    }

    const GenericTemplateInfo* generic_struct = selector.qualified
        ? this->find_generic_struct_in_module(selector.module, selector.name_id, selector.name, selector.range, false)
        : this->find_generic_struct_in_visible_modules(selector.name_id, selector.name, selector.range, false);
    if (generic_struct != nullptr) {
        base::Result<GenericArgumentBundle> args =
            this->resolve_generic_argument_bundle(*generic_struct, ordered_args, selector.range);
        if (!args) {
            return INVALID_TYPE_HANDLE;
        }
        return this->instantiate_generic_struct(*generic_struct, use_type, use_type_id, args.value());
    }
    const GenericTemplateInfo* generic_enum = selector.qualified
        ? this->find_generic_enum_in_module(selector.module, selector.name_id, selector.name, selector.range, false)
        : this->find_generic_enum_in_visible_modules(selector.name_id, selector.name, selector.range, false);
    if (generic_enum != nullptr) {
        base::Result<GenericArgumentBundle> args =
            this->resolve_generic_argument_bundle(*generic_enum, ordered_args, selector.range);
        if (!args) {
            return INVALID_TYPE_HANDLE;
        }
        return this->instantiate_generic_enum(*generic_enum, use_type, use_type_id, args.value());
    }
    const GenericTemplateInfo* generic_alias = selector.qualified
        ? this->find_generic_type_alias_in_module(
              selector.module, selector.name_id, selector.name, selector.range, false)
        : this->find_generic_type_alias_in_visible_modules(selector.name_id, selector.name, selector.range, false);
    if (generic_alias != nullptr) {
        base::Result<GenericArgumentBundle> args =
            this->resolve_generic_argument_bundle(*generic_alias, ordered_args, selector.range);
        if (!args) {
            return INVALID_TYPE_HANDLE;
        }
        return this->instantiate_generic_type_alias(
            *generic_alias, use_type, use_type_id, args.value(), opaque_allowed_as_pointee);
    }

    const TypeHandle concrete = selector.qualified
        ? this->find_type_in_module(
              selector.module, selector.name_id, selector.name, selector.range, opaque_allowed_as_pointee, false)
        : this->find_type_in_visible_modules(
              selector.name_id, selector.name, selector.range, opaque_allowed_as_pointee, false);
    if (is_valid(concrete)) {
        if (report_unknown) {
            this->report_type(selector.range, sema_type_not_generic_message(selector.name));
        }
        return INVALID_TYPE_HANDLE;
    }
    if (report_unknown) {
        if (selector.qualified) {
            this->report_generic_type_template_in_module(
                selector.module, selector.name_id, selector.name, selector.range);
        } else {
            static_cast<void>(
                this->find_generic_struct_in_visible_modules(selector.name_id, selector.name, selector.range, true));
        }
    }
    return INVALID_TYPE_HANDLE;
}

const FunctionSignature* SemanticAnalyzerCore::find_function_selector(const syntax::ExprId callee,
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    if (syntax::is_valid(callee) && callee.value < this->ctx_.module.exprs.size()) {
        const SemanticAnalyzerCore::ExprView expr = this->expr_view(callee);
        if (expr.kind == syntax::ExprKind::name && !expr.scope_name.empty()) {
            const syntax::ModuleId module =
                this->resolve_import_alias(expr.scope_name, expr.scope_range, report_unknown);
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

const SemanticAnalyzerCore::GenericTemplateInfo* SemanticAnalyzerCore::find_generic_function_selector(
    const NamedTypeSelector& selector, const base::SourceRange& range, const bool report_unknown)
{
    if (selector.name.empty()) {
        return nullptr;
    }
    return selector.qualified
        ? this->find_generic_function_in_module(selector.module, selector.name_id, selector.name, range, report_unknown)
        : this->find_generic_function_in_visible_modules(selector.name_id, selector.name, range, report_unknown);
}

TypeHandle SemanticAnalyzerCore::analyze_call_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    if (!syntax::is_valid(expr.callee) || expr.callee.value >= this->ctx_.module.exprs.size()) {
        this->report_general(expr.range, std::string(SEMA_CALLEE_FUNCTION_NAME));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const SemanticAnalyzerCore::ExprView callee = this->expr_view(expr.callee);
    if (callee.kind == syntax::ExprKind::generic_apply) {
        const NamedTypeSelector selector = this->resolve_named_type_selector(expr.callee, false);
        if (is_unqualified_dynproject_selector(selector)) {
            return this->analyze_dynproject_intrinsic_call(expr_id, expr, callee, expected_type);
        }
        if (selector.name.empty()) {
            if (syntax::is_valid(callee.callee) && callee.callee.value < this->ctx_.module.exprs.size()) {
                const SemanticAnalyzerCore::ExprView generic_callee = this->expr_view(callee.callee);
                if (generic_callee.kind == syntax::ExprKind::field) {
                    const ModuleSelector module = this->resolve_module_selector(generic_callee.object, false);
                    if (module.failed_as_module_selector) {
                        static_cast<void>(this->resolve_module_selector(generic_callee.object, true));
                        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
                    }
                    return this->analyze_explicit_generic_method_call_expr(expr_id, expr, callee, expected_type);
                }
            }
            this->report_general(callee.range, std::string(SEMA_EXPLICIT_GENERIC_CALL_SYNTAX));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        return this->analyze_explicit_generic_function_call_expr(expr_id, expr, callee, selector.name);
    }
    if (is_unqualified_range_value_call_callee(callee)) {
        return this->analyze_range_value_call_expr(expr_id, expr);
    }
    if (callee.kind == syntax::ExprKind::name && callee.scope_name.empty()) {
        if (const Symbol* local = this->state_.names.symbols.find(callee.text_id); local != nullptr) {
            return this->analyze_function_value_call_expr(expr_id, expr, callee.text);
        }
    }
    if (callee.kind != syntax::ExprKind::name && callee.kind != syntax::ExprKind::field) {
        return this->analyze_function_value_call_expr(expr_id, expr, SEMA_FUNCTION_VALUE_CALL_NAME);
    }
    const std::string name =
        callee.kind == syntax::ExprKind::field ? std::string(callee.field_name) : std::string(callee.text);

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

TypeHandle SemanticAnalyzerCore::analyze_dynproject_intrinsic_call(
    const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr,
    const SemanticAnalyzerCore::ExprView& generic_apply,
    const TypeHandle expected_type)
{
    if (generic_apply.type_args.size() != SEMA_DYNPROJECT_REQUIRED_TYPE_ARGS) {
        this->report_type(generic_apply.range, std::string(sema::SEMA_DYNPROJECT_TYPE_ARGUMENT_COUNT));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (expr.args.size() != SEMA_DYNPROJECT_REQUIRED_ARGS) {
        this->report_type(expr.range, std::string(sema::SEMA_DYNPROJECT_ARGUMENT_COUNT));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    auto resolve_dyn_trait_projection_type =
        [&](const syntax::TypeId type_id, const std::string_view diagnostic) -> TypeHandle {
        if (!syntax::is_valid(type_id) || type_id.value >= this->ctx_.module.types.size()) {
            this->report_type(generic_apply.range, std::string(diagnostic));
            return INVALID_TYPE_HANDLE;
        }
        const syntax::TypeNode source_type = this->ctx_.module.types[type_id.value];
        const syntax::TypeId dyn_trait_type =
            this->ctx_.module.push_type(make_dyn_trait_type_from_selector(source_type, generic_apply.range));
        const TypeHandle resolved = this->resolve_type(dyn_trait_type);
        if (!is_single_trait_object(this->state_.checked.types, resolved)) {
            this->report_type(source_type.range, std::string(diagnostic));
            return INVALID_TYPE_HANDLE;
        }
        return resolved;
    };

    const TypeHandle source_principal_type =
        resolve_dyn_trait_projection_type(generic_apply.type_args[0], SEMA_DYNPROJECT_SOURCE_PRINCIPAL);
    const TypeHandle target_supertrait_type =
        resolve_dyn_trait_projection_type(generic_apply.type_args[1], SEMA_DYNPROJECT_TARGET_SUPERTRAIT);
    if (!is_valid(source_principal_type) || !is_valid(target_supertrait_type)) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    const TypeHandle argument_type = this->analyze_expr(expr.args.front());
    if (!this->state_.checked.types.is_reference(argument_type)) {
        this->report_type(call_expr_range_or(this->ctx_.module, expr.args.front(), expr.range),
            std::string(SEMA_DYNPROJECT_ARGUMENT_COMPOSITION));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const TypeInfo& argument_ref = this->state_.checked.types.get(argument_type);
    if (!is_valid(argument_ref.pointee) || argument_ref.pointee.value >= this->state_.checked.types.size()) {
        this->report_type(call_expr_range_or(this->ctx_.module, expr.args.front(), expr.range),
            std::string(SEMA_DYNPROJECT_ARGUMENT_COMPOSITION));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const TypeInfo& composition_object = this->state_.checked.types.get(argument_ref.pointee);
    if (composition_object.kind != TypeKind::trait_object
        || composition_object.trait_object_principal_types.empty()
        || composition_object.trait_object_principal_set_identity.byte_count == 0) {
        this->report_type(call_expr_range_or(this->ctx_.module, expr.args.front(), expr.range),
            std::string(SEMA_DYNPROJECT_ARGUMENT_COMPOSITION));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    const bool source_in_composition =
        std::ranges::any_of(composition_object.trait_object_principal_types, [&](const TypeHandle principal) {
            return this->state_.checked.types.same(principal, source_principal_type);
        });
    if (!source_in_composition) {
        this->report_type(generic_apply.range, std::string(SEMA_DYNPROJECT_SOURCE_NOT_IN_COMPOSITION));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    const TypeInfo& source_principal = this->state_.checked.types.get(source_principal_type);
    const TypeInfo& target_supertrait = this->state_.checked.types.get(target_supertrait_type);
    if (this->find_supertrait_edge_path(source_principal, target_supertrait) == nullptr) {
        this->report_type(generic_apply.range, std::string(SEMA_DYNPROJECT_TARGET_NOT_SUPERTRAIT));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    TypeHandle target_reference_type =
        this->state_.checked.types.reference(argument_ref.pointer_mutability, target_supertrait_type);
    if (this->state_.checked.types.is_reference(expected_type)) {
        const TypeInfo& expected_ref = this->state_.checked.types.get(expected_type);
        if ((expected_ref.pointer_mutability == PointerMutability::const_
                || argument_ref.pointer_mutability == PointerMutability::mut)
            && this->state_.checked.types.same(expected_ref.pointee, target_supertrait_type)) {
            target_reference_type = expected_type;
        }
    }
    this->record_borrowed_dyn_trait_composition_supertrait_projection_if_needed(
        expr.args.front(), argument_type, target_reference_type, source_principal_type,
        query::BorrowedDynViewPathUse::explicit_projection, {}, expr.range);
    return this->record_expr_type(expr_id, target_reference_type);
}

TypeHandle SemanticAnalyzerCore::analyze_range_value_call_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr)
{
    if (std::ranges::any_of(expr.arg_labels, [](const syntax::CallArgLabelDecl& label) {
            return !label.name.empty();
        })) {
        this->report_type(expr.range, std::string(SEMA_NAMED_ARGUMENT_NOT_SUPPORTED));
    }
    if (expr.args.size() < SEMA_RANGE_VALUE_MIN_ARG_COUNT || expr.args.size() > SEMA_RANGE_VALUE_MAX_ARG_COUNT) {
        for (const syntax::ExprId arg : expr.args) {
            static_cast<void>(this->analyze_expr(arg));
        }
        this->report_general(expr.range, std::string(SEMA_FOR_RANGE_ARITY));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    std::vector<TypeHandle> operand_types(expr.args.size(), INVALID_TYPE_HANDLE);
    TypeHandle range_element_type = INVALID_TYPE_HANDLE;
    for (base::usize index = 0; index < expr.args.size(); ++index) {
        if (!range_value_contextual_integer_expr(this->ctx_.module, expr.args[index])) {
            operand_types[index] = this->analyze_expr(expr.args[index]);
            range_element_type = operand_types[index];
            break;
        }
    }
    if (!is_valid(range_element_type) && !expr.args.empty()) {
        operand_types.front() = this->analyze_expr(expr.args.front());
        range_element_type = operand_types.front();
    }
    for (base::usize index = 0; index < expr.args.size(); ++index) {
        if (!is_valid(operand_types[index])) {
            operand_types[index] = this->analyze_expr(expr.args[index], range_element_type);
        }
    }

    bool ok = true;
    for (base::usize index = 0; index < expr.args.size(); ++index) {
        if (!this->state_.checked.types.is_integer(operand_types[index])) {
            const std::string_view message =
                index == 2 ? SEMA_RANGE_STEP_INTEGER : SEMA_RANGE_BOUNDS_INTEGER;
            this->report_general(
                call_expr_range_or(this->ctx_.module, expr.args[index], expr.range), std::string(message));
            ok = false;
        }
    }
    if (expr.args.size() >= 2 && is_valid(operand_types[0]) && is_valid(operand_types[1])
        && !this->state_.checked.types.same(operand_types[0], operand_types[1])) {
        this->report_general(expr.range, std::string(SEMA_RANGE_BOUNDS_SAME_TYPE));
        ok = false;
    }
    if (expr.args.size() == SEMA_RANGE_VALUE_MAX_ARG_COUNT && is_valid(operand_types[0])
        && is_valid(operand_types[2]) && this->state_.checked.types.is_integer(operand_types[0])
        && this->state_.checked.types.is_integer(operand_types[2])
        && !this->state_.checked.types.same(operand_types[0], operand_types[2])) {
        this->report_general(
            call_expr_range_or(this->ctx_.module, expr.args[2], expr.range), std::string(SEMA_RANGE_STEP_SAME_TYPE));
        ok = false;
    }

    if (!is_valid(range_element_type)) {
        for (const TypeHandle type : operand_types) {
            if (this->state_.checked.types.is_integer(type)) {
                range_element_type = type;
                break;
            }
        }
    }
    if (!ok || !this->state_.checked.types.is_integer(range_element_type)) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    const TypeHandle range_type = this->state_.checked.types.range(range_element_type);
    RangeValuePlan plan;
    plan.expr = expr_id;
    plan.element_type = range_element_type;
    plan.range_type = range_type;
    plan.default_start = expr.args.size() == 1;
    plan.default_step = expr.args.size() < SEMA_RANGE_VALUE_MAX_ARG_COUNT;
    if (expr.args.size() == 1) {
        plan.end_expr = expr.args[0];
    } else {
        plan.start_expr = expr.args[0];
        plan.end_expr = expr.args[1];
        if (expr.args.size() == SEMA_RANGE_VALUE_MAX_ARG_COUNT) {
            plan.step_expr = expr.args[2];
        }
    }
    this->record_range_value_plan(expr_id, plan);
    return this->record_expr_type(expr_id, range_type);
}

TypeHandle SemanticAnalyzerCore::analyze_explicit_generic_function_call_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const SemanticAnalyzerCore::ExprView&, const std::string_view name)
{
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
            this->report_type(callee_range, sema_function_not_generic_message(name));
        } else {
            static_cast<void>(this->find_generic_function_selector(generic_lookup, callee_range, true));
        }
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    std::vector<syntax::GenericArgDecl> type_only_args;
    std::span<const syntax::GenericArgDecl> ordered_args = selector.generic_args;
    if (ordered_args.empty() && !selector.type_args.empty()) {
        type_only_args.reserve(selector.type_args.size());
        for (const syntax::TypeId type_arg : selector.type_args) {
            type_only_args.push_back(syntax::GenericArgDecl{
                syntax::GenericArgKind::type,
                type_arg,
                syntax::INVALID_EXPR_ID,
                selector.range,
            });
        }
        ordered_args = type_only_args;
    }
    base::Result<GenericArgumentBundle> args =
        this->resolve_generic_argument_bundle(*generic, ordered_args, callee_range);
    if (!args) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    FunctionSignature* signature = this->instantiate_generic_function(*generic, args.value(), callee_range);
    if (signature == nullptr) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    this->validate_unsafe_call(*signature, callee_range);
    this->record_expr_c_name(expr.callee, signature->c_name);
    CallArgumentResolution call_args =
        this->resolve_call_arguments(expr, name, signature->param_types, signature->params, 0, signature->is_variadic);
    this->record_function_call_binding(expr_id, expr.callee, *signature, 0, call_args.ordered_args, callee_range);
    return this->record_expr_type(expr_id, signature->return_type);
}

TypeHandle SemanticAnalyzerCore::analyze_explicit_generic_method_call_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const SemanticAnalyzerCore::ExprView& generic_apply, const TypeHandle)
{
    if (!syntax::is_valid(generic_apply.callee) || generic_apply.callee.value >= this->ctx_.module.exprs.size()) {
        this->report_general(generic_apply.range, std::string(SEMA_EXPLICIT_GENERIC_CALL_SYNTAX));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const SemanticAnalyzerCore::ExprView callee = this->expr_view(generic_apply.callee);
    if (callee.kind != syntax::ExprKind::field) {
        this->report_general(generic_apply.range, std::string(SEMA_EXPLICIT_GENERIC_CALL_SYNTAX));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    const std::string_view name = callee.field_name;
    const auto finish_call = [&](const FunctionSignature& signature, const bool has_receiver,
                                 const bool receiver_valid) -> TypeHandle {
        this->ensure_function_return_known(signature, callee.range);
        this->validate_unsafe_call(signature, callee.range);
        this->record_expr_c_name(expr.callee, signature.c_name);
        const base::u32 receiver_count = has_receiver ? SEMA_RECEIVER_ARGUMENT_COUNT : 0;
        if (!receiver_valid || signature.param_types.size() < receiver_count) {
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        CallArgumentResolution call_args = this->resolve_call_arguments(
            expr, name, signature.param_types, signature.params, receiver_count, signature.is_variadic);
        this->record_function_call_binding(
            expr_id, expr.callee, signature, receiver_count, call_args.ordered_args, callee.range);
        return this->record_expr_type(expr_id, signature.return_type);
    };

    if (syntax::is_valid(callee.object) && callee.object.value < this->ctx_.module.exprs.size()) {
        const TypeHandle associated_owner = this->resolve_type_selector(callee.object, false);
        if (is_valid(associated_owner)) {
            if (const FunctionSignature* const plain = this->find_method_in_visible_modules(
                    associated_owner, callee.field_name_id, callee.field_name, callee.range, false, false);
                plain != nullptr) {
                this->report_type(callee.range,
                    sema_method_not_generic_message(
                        this->state_.checked.types.display_name(associated_owner), callee.field_name));
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
            FunctionSignature* signature =
                this->find_generic_method_in_visible_modules(associated_owner, callee.field_name_id, callee.field_name,
                    callee.range, false, true, &expr, 0, true, generic_apply.type_args);
            if (signature == nullptr) {
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
            bool receiver_valid = true;
            if (signature->has_self_param) {
                this->report_general(callee.range,
                    sema_method_requires_receiver_message(
                        this->state_.checked.types.display_name(associated_owner), name));
                receiver_valid = false;
            }
            return finish_call(*signature, false, receiver_valid);
        }
    }

    const TypeHandle receiver_type = this->analyze_expr(callee.object);
    TypeHandle owner_type = receiver_type;
    if (this->state_.checked.types.is_pointer(owner_type) || this->state_.checked.types.is_reference(owner_type)) {
        owner_type = this->state_.checked.types.get(owner_type).pointee;
    }
    if (const FunctionSignature* const plain = this->find_method_in_visible_modules(
            owner_type, callee.field_name_id, callee.field_name, callee.range, true, false);
        plain != nullptr) {
        this->report_type(callee.range,
            sema_method_not_generic_message(this->state_.checked.types.display_name(owner_type), callee.field_name));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    FunctionSignature* signature =
        this->find_generic_method_in_visible_modules(owner_type, callee.field_name_id, callee.field_name, callee.range,
            true, true, &expr, SEMA_RECEIVER_ARGUMENT_COUNT, true, generic_apply.type_args);
    if (signature == nullptr) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    return finish_call(*signature, true, this->method_receiver_matches(*signature, receiver_type, callee.object));
}

TypeHandle SemanticAnalyzerCore::analyze_enum_constructor_call(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const EnumCaseInfo& enum_case)
{
    const std::string case_display_name = enum_case_display_name(this->state_.checked.types, enum_case);
    if (std::ranges::any_of(expr.arg_labels, [](const syntax::CallArgLabelDecl& label) {
            return !label.name.empty();
        })) {
        this->report_type(expr.range, std::string(SEMA_NAMED_ARGUMENT_NOT_SUPPORTED));
    }
    if (enum_case.payload_types.empty()) {
        this->report_pattern(expr.range, sema_enum_payload_constructor_case_message(case_display_name));
    }
    if (expr.args.size() != enum_case.payload_types.size()) {
        if (enum_case.payload_types.size() == 1) {
            this->report_type(expr.range, sema_enum_payload_constructor_arity_message(case_display_name));
        } else {
            this->report_type(expr.range,
                sema_enum_payload_constructor_argument_count_message(
                    case_display_name, enum_case.payload_types.size()));
        }
    }
    const base::usize checked_arg_count = std::min(expr.args.size(), enum_case.payload_types.size());
    for (base::usize i = 0; i < checked_arg_count; ++i) {
        const TypeHandle expected = enum_case.payload_types[i];
        const TypeHandle actual = this->analyze_expr(expr.args[i], expected);
        if (!this->can_assign(expected, actual, expr.args[i])) {
            this->report_type_mismatch(call_expr_range_or(this->ctx_.module, expr.args[i], expr.range),
                std::string(SEMA_ENUM_PAYLOAD_ARGUMENT_TYPE_MISMATCH), expected, actual);
        }
        static_cast<void>(this->check_m2_value_abi(expected, ValueAbiContext::enum_payload_argument,
            call_expr_range_or(this->ctx_.module, expr.args[i], expr.range)));
    }
    this->record_expr_c_name(expr.callee, enum_case.c_name);
    return this->record_expr_type(expr_id, enum_case.type);
}

TypeHandle SemanticAnalyzerCore::analyze_field_call_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const SemanticAnalyzerCore::ExprView& callee,
    const std::string_view name, const TypeHandle)
{
    const FunctionSignature* signature = nullptr;
    TypeHandle receiver_type = INVALID_TYPE_HANDLE;
    bool has_receiver = false;
    bool receiver_valid = true;
    const auto finish_trait_method_call = [&](const TraitMethodCallResolution& resolution,
                                              const TypeHandle owner_type) -> TypeHandle {
        FunctionSignature param_env_signature = this->state_.checked.make_function_signature();
        const FunctionSignature* call_signature = resolution.signature;
        if (call_signature == nullptr) {
            param_env_signature.name = this->state_.checked.intern_text(name);
            param_env_signature.name_id = callee.field_name_id;
            param_env_signature.return_type = resolution.return_type;
            param_env_signature.param_types = this->state_.checked.copy_type_handle_list(resolution.param_types);
            if (resolution.requirement != nullptr) {
                param_env_signature.params =
                    this->state_.checked.copy_function_param_info_list(resolution.requirement->params);
            }
            param_env_signature.range = callee.range;
            param_env_signature.is_unsafe = resolution.requirement != nullptr && resolution.requirement->is_unsafe;
            param_env_signature.is_variadic = resolution.requirement != nullptr && resolution.requirement->is_variadic;
            param_env_signature.is_method = true;
            param_env_signature.has_self_param =
                resolution.requirement != nullptr && resolution.requirement->has_self_param;
            call_signature = &param_env_signature;
        } else {
            this->ensure_function_return_known(*call_signature, callee.range);
            this->record_expr_c_name(expr.callee, call_signature->c_name);
        }
        this->validate_unsafe_call(*call_signature, callee.range);
        const base::u32 receiver_count = has_receiver ? SEMA_RECEIVER_ARGUMENT_COUNT : 0;
        TypeHandle trait_receiver_type = receiver_type;
        if (has_receiver && resolution.dispatch == TraitMethodDispatchKind::vtable_slot
            && is_valid(resolution.dispatch_receiver_type)
            && this->state_.checked.types.is_reference(receiver_type)) {
            trait_receiver_type = this->state_.checked.types.reference(
                this->state_.checked.types.get(receiver_type).pointer_mutability, resolution.dispatch_receiver_type);
        }
        const bool trait_receiver_valid = !has_receiver
            || this->method_receiver_matches(*call_signature, trait_receiver_type, callee.object);
        if (!trait_receiver_valid || resolution.param_types.size() < receiver_count) {
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        CallArgumentResolution call_args = this->resolve_call_arguments(
            expr, name, resolution.param_types, call_signature->params, receiver_count, call_signature->is_variadic);
        TraitMethodCallBinding binding = this->state_.checked.make_trait_method_call_binding();
        binding.call_expr = expr_id;
        binding.callee_expr = expr.callee;
        binding.dispatch = resolution.dispatch;
        if (resolution.predicate != nullptr) {
            binding.predicate_index = resolution.predicate->index;
            binding.predicate_fingerprint = resolution.predicate->canonical_fingerprint;
        }
        if (resolution.impl != nullptr) {
            binding.impl_key = resolution.impl->key;
        }
        if (resolution.signature != nullptr) {
            binding.function_key = resolution.signature->semantic_key;
        }
        binding.vtable_layout = resolution.vtable_layout;
        binding.vtable_slot = resolution.vtable_slot;
        if (resolution.trait != nullptr) {
            binding.trait_module = resolution.trait->module;
            binding.trait_name_id = resolution.trait->name_id;
        }
        binding.method_name = this->state_.checked.intern_text(name);
        binding.method_name_id = callee.field_name_id;
        if (resolution.requirement != nullptr) {
            binding.requirement_ordinal = resolution.requirement->ordinal;
        }
        binding.receiver_type = receiver_type;
        binding.dispatch_receiver_type =
            is_valid(resolution.dispatch_receiver_type) ? resolution.dispatch_receiver_type : receiver_type;
        binding.self_type = owner_type;
        binding.return_type = resolution.return_type;
        binding.ordered_args.assign(call_args.ordered_args.begin(), call_args.ordered_args.end());
        const ReceiverAccessFacts access = receiver_access_facts(
            this->state_.checked.types, resolution.param_types, receiver_count, trait_receiver_type);
        binding.receiver_access = access.access;
        binding.receiver_auto_borrow = access.auto_borrow;
        binding.receiver_two_phase_eligible = access.two_phase_eligible;
        binding.range = callee.range;
        binding.part_index = this->item_part_index(this->state_.flow.current_item);
        this->state_.checked.append_trait_method_call_binding(std::move(binding));
        return this->record_expr_type(expr_id, resolution.return_type);
    };

    if (syntax::is_valid(callee.object) && callee.object.value < this->ctx_.module.exprs.size()) {
        const TypeHandle associated_owner = this->resolve_type_selector(callee.object, false);
        if (is_valid(associated_owner)) {
            signature = this->find_method_in_visible_modules(
                associated_owner, callee.field_name_id, callee.field_name, callee.range, false, false);
            if (signature == nullptr) {
                bool saw_generic_candidate = false;
                signature = this->find_generic_method_in_visible_modules(associated_owner, callee.field_name_id,
                    callee.field_name, callee.range, false, false, &expr, 0, false, {}, &saw_generic_candidate);
                if (signature == nullptr && saw_generic_candidate) {
                    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
                }
            }
            if (signature == nullptr) {
                const TraitMethodCallResolution trait_resolution = this->resolve_trait_method_call(
                    associated_owner, callee.field_name_id, callee.field_name, callee.range, false, false);
                if (trait_resolution.found) {
                    return finish_trait_method_call(trait_resolution, associated_owner);
                }
                const TraitMethodCallResolution reported_trait_resolution = this->resolve_trait_method_call(
                    associated_owner, callee.field_name_id, callee.field_name, callee.range, false, true);
                if (!reported_trait_resolution.found && !reported_trait_resolution.reported_failure) {
                    static_cast<void>(this->find_method_in_visible_modules(
                        associated_owner, callee.field_name_id, callee.field_name, callee.range, false));
                }
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
            if (signature->has_self_param) {
                this->report_general(callee.range,
                    sema_method_requires_receiver_message(
                        this->state_.checked.types.display_name(associated_owner), name));
                receiver_valid = false;
            }
        }
    }
    if (signature == nullptr) {
        has_receiver = true;
        receiver_type = this->analyze_expr(callee.object);
        TypeHandle owner_type = receiver_type;
        if (this->state_.checked.types.is_pointer(owner_type) || this->state_.checked.types.is_reference(owner_type)) {
            owner_type = this->state_.checked.types.get(owner_type).pointee;
        }
        if (is_valid(owner_type) && owner_type.value < this->state_.checked.types.size()
            && this->state_.checked.types.get(owner_type).kind == TypeKind::trait_object) {
            TraitMethodCallResolution dyn_resolution =
                this->resolve_dyn_trait_method_call(owner_type, callee.field_name_id, callee.field_name, callee.range,
                    false);
            if (dyn_resolution.found) {
                TypeHandle dispatch_receiver_type = dyn_resolution.dispatch_receiver_type;
                if (!is_valid(dispatch_receiver_type)) {
                    dispatch_receiver_type = owner_type;
                }
                TypeHandle dispatch_reference_type = receiver_type;
                if (dispatch_receiver_type.value != owner_type.value
                    && this->state_.checked.types.is_reference(receiver_type)) {
                    dispatch_reference_type = this->state_.checked.types.reference(
                        this->state_.checked.types.get(receiver_type).pointer_mutability, dispatch_receiver_type);
                    if (dyn_resolution.uses_composition_supertrait_path) {
                        this->record_borrowed_dyn_trait_composition_supertrait_projection_if_needed(
                            callee.object, receiver_type, dispatch_reference_type,
                            dyn_resolution.composition_source_principal_type,
                            query::BorrowedDynViewPathUse::method_dispatch, callee.field_name, callee.range);
                        for (const TraitObjectUpcastCoercionFact& fact :
                            this->state_.checked.trait_object_upcast_coercions) {
                            if (fact.expr.value == callee.object.value
                                && fact.target_reference_type.value == dispatch_reference_type.value
                                && fact.target_object_type.value == dispatch_receiver_type.value) {
                                dyn_resolution.vtable_layout = fact.target_vtable_layout;
                                break;
                            }
                        }
                    } else if (this->can_borrowed_dyn_trait_composition_project(dispatch_reference_type, receiver_type)) {
                        this->record_borrowed_dyn_trait_composition_projection_if_needed(
                            callee.object, receiver_type, dispatch_reference_type, callee.range);
                    } else {
                        this->record_borrowed_dyn_trait_upcast_if_needed(
                            callee.object, receiver_type, dispatch_reference_type, callee.range);
                        for (const TraitObjectUpcastCoercionFact& fact :
                            this->state_.checked.trait_object_upcast_coercions) {
                            if (fact.expr.value == callee.object.value
                                && fact.source_reference_type.value == receiver_type.value
                                && fact.target_reference_type.value == dispatch_reference_type.value
                                && fact.target_object_type.value == dispatch_receiver_type.value) {
                                dyn_resolution.vtable_layout = fact.target_vtable_layout;
                                break;
                            }
                        }
                    }
                } else {
                    for (const TraitObjectCoercionFact& fact : this->state_.checked.trait_object_coercions) {
                        if (fact.target_reference_type.value == receiver_type.value
                            && fact.object_type.value == owner_type.value) {
                            dyn_resolution.vtable_layout = fact.vtable_layout;
                            break;
                        }
                    }
                }
                return finish_trait_method_call(dyn_resolution, owner_type);
            }
            const TraitMethodCallResolution reported_dyn_resolution =
                this->resolve_dyn_trait_method_call(owner_type, callee.field_name_id, callee.field_name, callee.range,
                    true);
            if (reported_dyn_resolution.found) {
                return finish_trait_method_call(reported_dyn_resolution, owner_type);
            }
            if (reported_dyn_resolution.reported_failure) {
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
        }
        signature = this->find_method_in_visible_modules(
            owner_type, callee.field_name_id, callee.field_name, callee.range, true, false);
        if (signature == nullptr) {
            bool saw_generic_candidate = false;
            signature =
                this->find_generic_method_in_visible_modules(owner_type, callee.field_name_id, callee.field_name,
                    callee.range, true, false, &expr, SEMA_RECEIVER_ARGUMENT_COUNT, false, {}, &saw_generic_candidate);
            if (signature == nullptr && saw_generic_candidate) {
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
        }
        if (signature == nullptr) {
            const TraitMethodCallResolution trait_resolution = this->resolve_trait_method_call(
                owner_type, callee.field_name_id, callee.field_name, callee.range, true, false);
            if (trait_resolution.found) {
                return finish_trait_method_call(trait_resolution, owner_type);
            }
            const PlaceInfo callee_place = this->analyze_place_info(expr.callee, false);
            if (this->state_.checked.types.is_function(callee_place.type)) {
                return this->analyze_function_value_call_expr(expr_id, expr, name);
            }
            const TraitMethodCallResolution reported_trait_resolution = this->resolve_trait_method_call(
                owner_type, callee.field_name_id, callee.field_name, callee.range, true, true);
            if (reported_trait_resolution.found) {
                return finish_trait_method_call(reported_trait_resolution, owner_type);
            }
            if (reported_trait_resolution.reported_failure) {
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
            if (!reported_trait_resolution.reported_failure) {
                static_cast<void>(this->find_method_in_visible_modules(
                    owner_type, callee.field_name_id, callee.field_name, callee.range, true));
            }
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        receiver_valid = this->method_receiver_matches(*signature, receiver_type, callee.object);
    }
    this->ensure_function_return_known(*signature, callee.range);
    this->validate_unsafe_call(*signature, callee.range);
    this->record_expr_c_name(expr.callee, signature->c_name);
    const base::u32 receiver_count = has_receiver ? SEMA_RECEIVER_ARGUMENT_COUNT : 0;
    if (!receiver_valid || signature->param_types.size() < receiver_count) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    CallArgumentResolution call_args = this->resolve_call_arguments(
        expr, name, signature->param_types, signature->params, receiver_count, signature->is_variadic);
    this->record_function_call_binding(
        expr_id, expr.callee, *signature, receiver_count, call_args.ordered_args, callee.range);
    return this->record_expr_type(expr_id, signature->return_type);
}

TypeHandle SemanticAnalyzerCore::analyze_function_value_call_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const std::string_view name)
{
    const TypeHandle callee_type = this->analyze_expr(expr.callee);
    if (const CheckedLambdaInfo* const closure = this->lambda_for_environment_type(callee_type);
        closure != nullptr && is_valid(closure->function_type)) {
        this->validate_call_arguments(expr, name.empty() ? SEMA_FUNCTION_VALUE_CALL_NAME : name,
            closure->param_types, 0, false);
        return this->record_expr_type(expr_id, closure->return_type);
    }
    if (!this->state_.checked.types.is_function(callee_type)) {
        this->report_general(expr.range, std::string(SEMA_CALLEE_FUNCTION_NAME));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const TypeInfo& function = this->state_.checked.types.get(callee_type);
    this->validate_unsafe_function_value_call(callee_type, expr.range);
    this->validate_call_arguments(expr, name.empty() ? SEMA_FUNCTION_VALUE_CALL_NAME : name, function.function_params,
        0, function.function_is_variadic);
    return this->record_expr_type(expr_id, function.function_return);
}

TypeHandle SemanticAnalyzerCore::analyze_function_call_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const SemanticAnalyzerCore::ExprView& callee,
    const std::string_view name, const TypeHandle)
{
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
        CallArgumentResolution call_args = this->resolve_call_arguments(
            expr, name, signature->param_types, signature->params, 0, signature->is_variadic);
        this->record_function_call_binding(expr_id, expr.callee, *signature, 0, call_args.ordered_args, callee_range);
        return this->record_expr_type(expr_id, signature->return_type);
    }
    signature = this->find_function_selector(expr.callee,
        callee.kind == syntax::ExprKind::field ? callee.field_name_id : callee.text_id, name, callee_range, true);
    if (signature == nullptr) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    this->ensure_function_return_known(*signature, call_expr_range_or(this->ctx_.module, expr.callee, callee_range));
    this->validate_unsafe_call(*signature, callee_range);
    this->record_expr_c_name(expr.callee, signature->c_name);
    CallArgumentResolution call_args =
        this->resolve_call_arguments(expr, name, signature->param_types, signature->params, 0, signature->is_variadic);
    this->record_function_call_binding(expr_id, expr.callee, *signature, 0, call_args.ordered_args, callee_range);
    return this->record_expr_type(expr_id, signature->return_type);
}

void SemanticAnalyzerCore::record_function_call_binding(const syntax::ExprId call_expr,
    const syntax::ExprId callee_expr, const FunctionSignature& signature, const base::u32 receiver_arg_count,
    const std::span<const syntax::ExprId> ordered_args, const base::SourceRange& range)
{
    if (!syntax::is_valid(call_expr) || !sema::is_valid(signature.semantic_key)) {
        return;
    }
    FunctionCallBinding binding = this->state_.checked.make_function_call_binding();
    binding.call_expr = call_expr;
    binding.callee_expr = callee_expr;
    binding.function_key = signature.semantic_key;
    binding.return_type = signature.return_type;
    binding.receiver_arg_count = receiver_arg_count;
    binding.ordered_args.assign(ordered_args.begin(), ordered_args.end());
    const std::optional<syntax::ExprId> receiver_expr = receiver_expr_for_call_callee(this->ctx_.module, callee_expr);
    const TypeHandle receiver_type =
        receiver_expr.has_value() ? this->cached_expr_type(*receiver_expr) : INVALID_TYPE_HANDLE;
    const ReceiverAccessFacts access =
        receiver_access_facts(this->state_.checked.types, signature.param_types, receiver_arg_count, receiver_type);
    binding.receiver_access = access.access;
    binding.receiver_auto_borrow = access.auto_borrow;
    binding.receiver_two_phase_eligible = access.two_phase_eligible;
    binding.range = range;
    binding.part_index = this->item_part_index(this->state_.flow.current_item);
    this->state_.checked.append_function_call_binding(std::move(binding));
}

SemanticAnalyzerCore::CallArgumentResolution SemanticAnalyzerCore::resolve_call_arguments(
    const SemanticAnalyzerCore::ExprView& expr,
    const std::string_view name,
    const std::span<const TypeHandle> param_types,
    const std::span<const FunctionParamInfo> params,
    const base::usize receiver_count,
    const bool is_variadic,
    const bool check_argument_types)
{
    CallArgumentResolution resolution;
    resolution.ordered_args = this->state_.checked.make_expr_id_list();

    const bool has_named_args =
        std::ranges::any_of(expr.arg_labels, [](const syntax::CallArgLabelDecl& label) {
            return !label.name.empty();
        });
    const bool has_parameter_metadata = params.size() >= param_types.size();
    if (has_named_args && !has_parameter_metadata) {
        this->report_type(expr.range, std::string(SEMA_NAMED_ARGUMENT_NOT_SUPPORTED));
        resolution.ok = false;
        return resolution;
    }

    std::vector<syntax::ExprId> ordered(param_types.size(), syntax::INVALID_EXPR_ID);
    std::vector<bool> provided(param_types.size(), false);
    std::vector<syntax::ExprId> variadic_args;
    base::usize next_positional_param = receiver_count;
    bool saw_named_arg = false;
    bool reported_count = false;
    for (base::usize arg_index = 0; arg_index < expr.args.size(); ++arg_index) {
        const syntax::ExprId arg = expr.args[arg_index];
        if (call_arg_has_label(expr.arg_labels, arg_index)) {
            saw_named_arg = true;
            const syntax::CallArgLabelDecl& label = expr.arg_labels[arg_index];
            if (is_variadic) {
                this->report_type(label.range, std::string(SEMA_NAMED_ARGUMENT_NOT_SUPPORTED));
                resolution.ok = false;
                continue;
            }
            const std::optional<base::usize> param_index =
                find_call_param_index(params, label.name, receiver_count);
            if (!param_index.has_value() || *param_index >= param_types.size()) {
                this->report_type(label.range, sema_unknown_named_argument_message(name, label.name));
                resolution.ok = false;
                continue;
            }
            if (provided[*param_index]) {
                this->report_type(label.range, sema_duplicate_named_argument_message(label.name));
                resolution.ok = false;
                continue;
            }
            ordered[*param_index] = arg;
            provided[*param_index] = true;
            continue;
        }

        if (saw_named_arg) {
            this->report_type(call_expr_range_or(this->ctx_.module, arg, expr.range),
                std::string(SEMA_POSITIONAL_ARGUMENT_AFTER_NAMED));
            resolution.ok = false;
            continue;
        }
        if (next_positional_param < param_types.size()) {
            ordered[next_positional_param] = arg;
            provided[next_positional_param] = true;
            ++next_positional_param;
            continue;
        }
        if (is_variadic) {
            variadic_args.push_back(arg);
            continue;
        }
        if (!reported_count) {
            this->report_type(expr.range, sema_argument_count_message(name));
            reported_count = true;
        }
        resolution.ok = false;
    }

    for (base::usize param_index = receiver_count; param_index < param_types.size(); ++param_index) {
        if (provided[param_index]) {
            continue;
        }
        if (param_index < params.size() && syntax::is_valid(params[param_index].default_value)) {
            ordered[param_index] = params[param_index].default_value;
            provided[param_index] = true;
            continue;
        }
        if (has_named_args || !reported_count) {
            this->report_type(expr.range,
                has_named_args
                    ? sema_missing_required_argument_message(name, call_param_name_or_index(params, param_index))
                    : sema_argument_count_message(name));
            reported_count = true;
        }
        resolution.ok = false;
    }

    for (base::usize param_index = receiver_count; param_index < param_types.size(); ++param_index) {
        if (!provided[param_index]) {
            continue;
        }
        const syntax::ExprId arg = ordered[param_index];
        resolution.ordered_args.push_back(arg);
        if (!check_argument_types) {
            continue;
        }
        const TypeHandle expected = param_types[param_index];
        const TypeHandle actual = this->analyze_expr(arg, expected);
        if (!this->can_assign(expected, actual, arg)) {
            this->report_type_mismatch(call_expr_range_or(this->ctx_.module, arg, expr.range),
                sema_argument_type_message(name), expected, actual);
            resolution.ok = false;
        }
        static_cast<void>(this->check_m2_value_abi(
            expected, ValueAbiContext::argument, call_expr_range_or(this->ctx_.module, arg, expr.range)));
    }
    resolution.ordered_args.insert(resolution.ordered_args.end(), variadic_args.begin(), variadic_args.end());

    if (is_variadic && check_argument_types) {
        for (const syntax::ExprId variadic_arg : variadic_args) {
            const TypeHandle actual = this->analyze_expr(variadic_arg);
            if (!is_valid(actual)) {
                this->report_type(call_expr_range_or(this->ctx_.module, variadic_arg, expr.range),
                    sema_variadic_argument_type_infer_message(name));
                resolution.ok = false;
            } else if (!this->check_m2_value_abi(actual, ValueAbiContext::argument,
                           call_expr_range_or(this->ctx_.module, variadic_arg, expr.range))) {
                continue;
            }
        }
    }
    return resolution;
}

void SemanticAnalyzerCore::validate_call_arguments(const SemanticAnalyzerCore::ExprView& expr,
    const std::string_view name, const std::span<const TypeHandle> param_types, const base::usize receiver_count,
    const bool is_variadic)
{
    static const std::vector<FunctionParamInfo> NO_PARAM_INFO;
    if (std::ranges::any_of(expr.arg_labels, [](const syntax::CallArgLabelDecl& label) {
            return !label.name.empty();
        })) {
        this->report_type(expr.range, std::string(SEMA_NAMED_ARGUMENT_NOT_SUPPORTED));
        return;
    }
    static_cast<void>(this->resolve_call_arguments(
        expr, name, param_types, NO_PARAM_INFO, receiver_count, is_variadic));
}

} // namespace aurex::sema
