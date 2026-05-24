#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <sema/internal/sema_type_services.hpp>

namespace aurex::sema {

namespace {

constexpr base::usize SEMA_TYPE_LAYOUT_INITIAL_STACK_CAPACITY = 16;
constexpr base::u64 SEMA_ABI_INVALID_SIZE = 0;
constexpr base::u64 SEMA_ABI_MIN_ALIGNMENT = 1;

enum class TypeLayoutFrameStage {
    enter,
    finish,
};

enum class TypeLayoutVisitState {
    visiting,
    done,
};

struct TypeLayoutFrame {
    TypeHandle type = INVALID_TYPE_HANDLE;
    base::SourceRange range{};
    TypeLayoutFrameStage stage = TypeLayoutFrameStage::enter;
};

enum class TypeResolveActionKind {
    resolve,
    build_pointer,
    build_reference,
    build_array,
    build_slice,
    build_tuple,
    build_function,
};

struct TypeResolveAction {
    TypeResolveActionKind kind = TypeResolveActionKind::resolve;
    syntax::TypeId type = syntax::INVALID_TYPE_ID;
    bool opaque_allowed_as_pointee = false;
    syntax::PointerMutability pointer_mutability = syntax::PointerMutability::const_;
    syntax::FunctionCallConv function_call_conv = syntax::FunctionCallConv::aurex;
    bool function_is_unsafe = false;
    bool function_is_variadic = false;
    std::optional<base::u64> array_count{};
    base::usize tuple_element_count = 0;
    base::usize function_param_count = 0;
};

struct LayoutResult {
    base::u64 size = SEMA_ABI_INVALID_SIZE;
    base::u64 align = SEMA_ABI_MIN_ALIGNMENT;
    bool ok = false;
};

[[nodiscard]] BuiltinType map_builtin(const syntax::PrimitiveTypeKind kind) noexcept
{
    switch (kind) {
        case syntax::PrimitiveTypeKind::void_:
            return BuiltinType::void_;
        case syntax::PrimitiveTypeKind::bool_:
            return BuiltinType::bool_;
        case syntax::PrimitiveTypeKind::i8:
            return BuiltinType::i8;
        case syntax::PrimitiveTypeKind::u8:
            return BuiltinType::u8;
        case syntax::PrimitiveTypeKind::i16:
            return BuiltinType::i16;
        case syntax::PrimitiveTypeKind::u16:
            return BuiltinType::u16;
        case syntax::PrimitiveTypeKind::i32:
            return BuiltinType::i32;
        case syntax::PrimitiveTypeKind::u32:
            return BuiltinType::u32;
        case syntax::PrimitiveTypeKind::i64:
            return BuiltinType::i64;
        case syntax::PrimitiveTypeKind::u64:
            return BuiltinType::u64;
        case syntax::PrimitiveTypeKind::isize:
            return BuiltinType::isize;
        case syntax::PrimitiveTypeKind::usize:
            return BuiltinType::usize;
        case syntax::PrimitiveTypeKind::f32:
            return BuiltinType::f32;
        case syntax::PrimitiveTypeKind::f64:
            return BuiltinType::f64;
        case syntax::PrimitiveTypeKind::str:
            return BuiltinType::str;
        case syntax::PrimitiveTypeKind::char_:
            return BuiltinType::char_;
    }
    return BuiltinType::void_;
}

[[nodiscard]] PointerMutability map_mutability(const syntax::PointerMutability mutability) noexcept
{
    return mutability == syntax::PointerMutability::mut ? PointerMutability::mut : PointerMutability::const_;
}

[[nodiscard]] FunctionCallConv map_function_call_conv(const syntax::FunctionCallConv call_conv) noexcept
{
    return call_conv == syntax::FunctionCallConv::c ? FunctionCallConv::c : FunctionCallConv::aurex;
}

[[nodiscard]] base::u64 align_forward(const base::u64 offset, const base::u64 alignment) noexcept
{
    if (alignment == 0) {
        return offset;
    }
    const base::u64 mask = alignment - 1;
    if (offset > std::numeric_limits<base::u64>::max() - mask) {
        return std::numeric_limits<base::u64>::max();
    }
    return (offset + mask) & ~mask;
}

[[nodiscard]] bool checked_add_u64(const base::u64 lhs, const base::u64 rhs, base::u64& result) noexcept
{
    if (lhs > std::numeric_limits<base::u64>::max() - rhs) {
        result = std::numeric_limits<base::u64>::max();
        return false;
    }
    result = lhs + rhs;
    return true;
}

[[nodiscard]] bool checked_mul_u64(const base::u64 lhs, const base::u64 rhs, base::u64& result) noexcept
{
    if (lhs != 0 && rhs > std::numeric_limits<base::u64>::max() / lhs) {
        result = std::numeric_limits<base::u64>::max();
        return false;
    }
    result = lhs * rhs;
    return true;
}

[[nodiscard]] bool checked_align_forward(const base::u64 offset, const base::u64 alignment, base::u64& result) noexcept
{
    if (alignment == 0) {
        result = offset;
        return true;
    }
    const base::u64 mask = alignment - 1;
    if (offset > std::numeric_limits<base::u64>::max() - mask) {
        result = std::numeric_limits<base::u64>::max();
        return false;
    }
    result = (offset + mask) & ~mask;
    return true;
}

[[nodiscard]] base::u64 add_saturating(const base::u64 lhs, const base::u64 rhs) noexcept
{
    base::u64 result = SEMA_ABI_INVALID_SIZE;
    static_cast<void>(checked_add_u64(lhs, rhs, result));
    return result;
}

[[nodiscard]] base::u64 mul_saturating(const base::u64 lhs, const base::u64 rhs) noexcept
{
    base::u64 result = SEMA_ABI_INVALID_SIZE;
    static_cast<void>(checked_mul_u64(lhs, rhs, result));
    return result;
}

[[nodiscard]] bool is_builtin_scalar_bcast_type(const TypeTable& types, const TypeHandle type) noexcept
{
    if (!is_valid(type)) {
        return false;
    }
    const TypeInfo& info = types.get(type);
    return info.kind == TypeKind::builtin && info.builtin != BuiltinType::void_ && info.builtin != BuiltinType::bool_
        && info.builtin != BuiltinType::str;
}

[[nodiscard]] bool is_bitcast_type(const TypeTable& types, const TypeHandle type) noexcept
{
    if (!is_valid(type)) {
        return false;
    }
    return is_builtin_scalar_bcast_type(types, type) || types.is_pointer(type);
}

} // namespace

SemanticTypeResolver::SemanticTypeResolver(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

TypeHandle SemanticTypeResolver::resolve_type(const syntax::TypeId type)
{
    return this->resolve_type(type, false);
}

TypeHandle SemanticTypeResolver::resolve_type(const syntax::TypeId type_id, const bool opaque_allowed_as_pointee)
{
    std::vector<TypeResolveAction> actions;
    std::vector<TypeHandle> values;
    actions.reserve(SEMA_TYPE_LAYOUT_INITIAL_STACK_CAPACITY);
    values.reserve(SEMA_TYPE_LAYOUT_INITIAL_STACK_CAPACITY);
    TypeResolveAction root;
    root.kind = TypeResolveActionKind::resolve;
    root.type = type_id;
    root.opaque_allowed_as_pointee = opaque_allowed_as_pointee;
    actions.push_back(root);

    while (!actions.empty()) {
        const TypeResolveAction action = actions.back();
        actions.pop_back();
        switch (action.kind) {
            case TypeResolveActionKind::resolve: {
                if (!syntax::is_valid(action.type) || action.type.value >= this->core_.ctx_.module.types.size()) {
                    values.push_back(INVALID_TYPE_HANDLE);
                    break;
                }

                const TypeHandle cached = this->core_.cached_syntax_type(action.type);
                if (is_valid(cached)) {
                    if (this->core_.state_.checked.types.get(cached).kind == TypeKind::opaque_struct
                        && !action.opaque_allowed_as_pointee) {
                        this->core_.report_general(this->core_.ctx_.module.types[action.type.value].range,
                            "opaque struct can only be used as a pointer target");
                    }
                    values.push_back(cached);
                    break;
                }

                const syntax::TypeNode& type = this->core_.ctx_.module.types[action.type.value];
                switch (type.kind) {
                    case syntax::TypeKind::primitive: {
                        const TypeHandle resolved =
                            this->core_.state_.checked.types.builtin(map_builtin(type.primitive));
                        this->core_.record_syntax_type_handle(action.type, resolved);
                        values.push_back(resolved);
                        break;
                    }
                    case syntax::TypeKind::pointer: {
                        TypeResolveAction build;
                        build.kind = TypeResolveActionKind::build_pointer;
                        build.type = action.type;
                        build.opaque_allowed_as_pointee = action.opaque_allowed_as_pointee;
                        build.pointer_mutability = type.pointer_mutability;
                        actions.push_back(build);
                        TypeResolveAction resolve_pointee;
                        resolve_pointee.kind = TypeResolveActionKind::resolve;
                        resolve_pointee.type = type.pointee;
                        resolve_pointee.opaque_allowed_as_pointee = true;
                        actions.push_back(resolve_pointee);
                        break;
                    }
                    case syntax::TypeKind::reference: {
                        TypeResolveAction build;
                        build.kind = TypeResolveActionKind::build_reference;
                        build.type = action.type;
                        build.opaque_allowed_as_pointee = action.opaque_allowed_as_pointee;
                        build.pointer_mutability = type.pointer_mutability;
                        actions.push_back(build);
                        TypeResolveAction resolve_pointee;
                        resolve_pointee.kind = TypeResolveActionKind::resolve;
                        resolve_pointee.type = type.pointee;
                        resolve_pointee.opaque_allowed_as_pointee = false;
                        actions.push_back(resolve_pointee);
                        break;
                    }
                    case syntax::TypeKind::array: {
                        TypeResolveAction build;
                        build.kind = TypeResolveActionKind::build_array;
                        build.type = action.type;
                        build.opaque_allowed_as_pointee = action.opaque_allowed_as_pointee;
                        build.array_count = type.array_count;
                        actions.push_back(build);
                        TypeResolveAction resolve_element;
                        resolve_element.kind = TypeResolveActionKind::resolve;
                        resolve_element.type = type.array_element;
                        actions.push_back(resolve_element);
                        break;
                    }
                    case syntax::TypeKind::slice: {
                        TypeResolveAction build;
                        build.kind = TypeResolveActionKind::build_slice;
                        build.type = action.type;
                        build.opaque_allowed_as_pointee = action.opaque_allowed_as_pointee;
                        build.pointer_mutability = type.slice_mutability;
                        actions.push_back(build);
                        TypeResolveAction resolve_element;
                        resolve_element.kind = TypeResolveActionKind::resolve;
                        resolve_element.type = type.slice_element;
                        actions.push_back(resolve_element);
                        break;
                    }
                    case syntax::TypeKind::tuple: {
                        TypeResolveAction build;
                        build.kind = TypeResolveActionKind::build_tuple;
                        build.type = action.type;
                        build.opaque_allowed_as_pointee = action.opaque_allowed_as_pointee;
                        build.tuple_element_count = type.tuple_elements.size();
                        actions.push_back(build);
                        for (base::usize index = type.tuple_elements.size(); index > 0; --index) {
                            TypeResolveAction resolve_element;
                            resolve_element.kind = TypeResolveActionKind::resolve;
                            resolve_element.type = type.tuple_elements[index - 1];
                            actions.push_back(resolve_element);
                        }
                        break;
                    }
                    case syntax::TypeKind::function: {
                        TypeResolveAction build;
                        build.kind = TypeResolveActionKind::build_function;
                        build.type = action.type;
                        build.opaque_allowed_as_pointee = action.opaque_allowed_as_pointee;
                        build.function_call_conv = type.function_call_conv;
                        build.function_is_unsafe = type.function_is_unsafe;
                        build.function_is_variadic = type.function_is_variadic;
                        build.function_param_count = type.function_params.size();
                        actions.push_back(build);
                        TypeResolveAction resolve_return;
                        resolve_return.kind = TypeResolveActionKind::resolve;
                        resolve_return.type = type.function_return;
                        actions.push_back(resolve_return);
                        for (base::usize index = type.function_params.size(); index > 0; --index) {
                            TypeResolveAction resolve_param;
                            resolve_param.kind = TypeResolveActionKind::resolve;
                            resolve_param.type = type.function_params[index - 1];
                            actions.push_back(resolve_param);
                        }
                        break;
                    }
                    case syntax::TypeKind::named: {
                        const TypeHandle resolved =
                            this->resolve_named_type(action.type, type, action.opaque_allowed_as_pointee);
                        this->core_.record_syntax_type_handle(action.type, resolved);
                        values.push_back(resolved);
                        break;
                    }
                }
                break;
            }
            case TypeResolveActionKind::build_pointer: {
                const TypeHandle pointee = values.back();
                values.pop_back();
                const TypeHandle resolved =
                    this->core_.state_.checked.types.pointer(map_mutability(action.pointer_mutability), pointee);
                this->core_.record_syntax_type_handle(action.type, resolved);
                values.push_back(resolved);
                break;
            }
            case TypeResolveActionKind::build_reference: {
                const TypeHandle pointee = values.back();
                values.pop_back();
                if (!this->core_.is_valid_storage_type(pointee)) {
                    this->core_.report_general(
                        this->core_.ctx_.module.types[action.type.value].range, std::string(SEMA_REFERENCE_STORAGE));
                }
                const TypeHandle resolved =
                    this->core_.state_.checked.types.reference(map_mutability(action.pointer_mutability), pointee);
                this->core_.record_syntax_type_handle(action.type, resolved);
                values.push_back(resolved);
                break;
            }
            case TypeResolveActionKind::build_array: {
                const TypeHandle element = values.back();
                values.pop_back();
                const TypeHandle resolved = this->core_.state_.checked.types.array(*action.array_count, element);
                this->core_.record_syntax_type_handle(action.type, resolved);
                values.push_back(resolved);
                break;
            }
            case TypeResolveActionKind::build_slice: {
                const TypeHandle element = values.back();
                values.pop_back();
                const TypeHandle resolved =
                    this->core_.state_.checked.types.slice(map_mutability(action.pointer_mutability), element);
                this->core_.record_syntax_type_handle(action.type, resolved);
                values.push_back(resolved);
                break;
            }
            case TypeResolveActionKind::build_tuple: {
                std::vector<TypeHandle> elements(action.tuple_element_count, INVALID_TYPE_HANDLE);
                for (base::usize index = action.tuple_element_count; index > 0; --index) {
                    if (values.empty()) {
                        break;
                    }
                    elements[index - 1] = values.back();
                    values.pop_back();
                }
                for (const TypeHandle element : elements) {
                    if (!this->core_.is_valid_storage_type(element)) {
                        this->core_.report_general(
                            this->core_.ctx_.module.types[action.type.value].range, std::string(SEMA_FIELD_STORAGE));
                    }
                }
                const TypeHandle resolved = this->core_.state_.checked.types.tuple(elements);
                this->core_.record_syntax_type_handle(action.type, resolved);
                values.push_back(resolved);
                break;
            }
            case TypeResolveActionKind::build_function: {
                std::vector<TypeHandle> params(action.function_param_count, INVALID_TYPE_HANDLE);
                TypeHandle return_type = INVALID_TYPE_HANDLE;
                if (!values.empty()) {
                    return_type = values.back();
                    values.pop_back();
                }
                for (base::usize index = action.function_param_count; index > 0; --index) {
                    if (values.empty()) {
                        break;
                    }
                    params[index - 1] = values.back();
                    values.pop_back();
                }
                if (action.function_is_variadic && action.function_call_conv != syntax::FunctionCallConv::c) {
                    this->core_.report_general(this->core_.ctx_.module.types[action.type.value].range,
                        std::string(SEMA_VARIADIC_FUNCTION_TYPE_EXTERN_C_ONLY));
                }
                for (const TypeHandle param : params) {
                    if (!this->core_.is_valid_storage_type(param)) {
                        this->core_.report_general(this->core_.ctx_.module.types[action.type.value].range,
                            std::string(SEMA_FUNCTION_TYPE_PARAMETER_STORAGE));
                    }
                    static_cast<void>(this->core_.check_m2_value_abi(param, ValueAbiContext::function_type_parameter,
                        this->core_.ctx_.module.types[action.type.value].range));
                }
                if (is_valid(return_type) && !this->core_.state_.checked.types.is_void(return_type)
                    && !this->core_.is_valid_storage_type(return_type)) {
                    this->core_.report_general(this->core_.ctx_.module.types[action.type.value].range,
                        std::string(SEMA_FUNCTION_TYPE_RETURN_STORAGE));
                }
                static_cast<void>(this->core_.check_m2_value_abi(return_type, ValueAbiContext::function_type_return,
                    this->core_.ctx_.module.types[action.type.value].range));
                const TypeHandle resolved =
                    this->core_.state_.checked.types.function(map_function_call_conv(action.function_call_conv),
                        action.function_is_unsafe, action.function_is_variadic, params, return_type);
                this->core_.record_syntax_type_handle(action.type, resolved);
                values.push_back(resolved);
                break;
            }
        }
    }

    const TypeHandle resolved = values.back();
    this->core_.record_syntax_type_handle(type_id, resolved);
    return resolved;
}

TypeHandle SemanticTypeResolver::resolve_named_type(
    const syntax::TypeId type_id, const syntax::TypeNode& type, const bool opaque_allowed_as_pointee)
{
    const std::vector<std::string_view> scope_parts = this->core_.type_scope_parts(type);
    const bool qualified = !scope_parts.empty();
    syntax::ModuleId scope_module = syntax::INVALID_MODULE_ID;
    if (qualified) {
        scope_module = this->core_.resolve_type_scope(type, true);
        if (!syntax::is_valid(scope_module)) {
            return INVALID_TYPE_HANDLE;
        }
    }
    if (!qualified && this->core_.state_.flow.current_generic_context != nullptr) {
        if (const auto found = this->core_.state_.flow.current_generic_context->params.find(type.name_id);
            found != this->core_.state_.flow.current_generic_context->params.end()) {
            if (!type.type_args.empty()) {
                this->core_.report_type(type.range, sema_generic_param_type_args_message(type.name));
                return INVALID_TYPE_HANDLE;
            }
            return found->second;
        }
    }

    SemanticAnalyzerCore::NamedTypeSelector selector;
    selector.module = scope_module;
    selector.name = type.name;
    selector.name_id = type.name_id;
    selector.range = type.range;
    selector.type_args = type.type_args;
    selector.qualified = qualified;
    if (!selector.type_args.empty()) {
        return this->core_.resolve_generic_type_selector(selector, type_id, opaque_allowed_as_pointee, true);
    }
    if (qualified && this->core_.generic_type_template_exists_in_module(scope_module, type.name_id, type.name)) {
        this->core_.report_generic_type_template_in_module(scope_module, type.name_id, type.name, type.range);
        return INVALID_TYPE_HANDLE;
    }
    return this->core_.resolve_named_type_selector_type(selector, opaque_allowed_as_pointee, true);
}

TypeHandle SemanticTypeResolver::resolve_type_alias(const TypeAliasInfo& alias, const bool opaque_allowed_as_pointee)
{
    const ModuleLookupKey key = this->core_.module_lookup_key(alias.module, alias.name_id);
    if (const auto found = this->core_.state_.types.resolved_type_aliases.find(key);
        found != this->core_.state_.types.resolved_type_aliases.end()) {
        return found->second;
    }
    if (std::find(this->core_.state_.types.resolving_type_aliases.begin(),
            this->core_.state_.types.resolving_type_aliases.end(), key)
        != this->core_.state_.types.resolving_type_aliases.end()) {
        this->core_.report_general(alias.range, sema_cyclic_type_alias_message(alias.name));
        this->core_.state_.types.resolved_type_aliases[key] = INVALID_TYPE_HANDLE;
        return INVALID_TYPE_HANDLE;
    }
    this->core_.state_.types.resolving_type_aliases.push_back(key);
    const syntax::ModuleId previous_module = this->core_.state_.flow.current_module;
    this->core_.state_.flow.current_module = alias.module;
    const TypeHandle resolved = this->resolve_type(alias.target, opaque_allowed_as_pointee);
    this->core_.state_.flow.current_module = previous_module;
    this->core_.state_.types.resolving_type_aliases.pop_back();
    this->core_.state_.types.resolved_type_aliases[key] = resolved;
    return resolved;
}

SemanticTypeValidator::SemanticTypeValidator(const SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

bool SemanticTypeValidator::can_assign(
    const TypeHandle dst, const TypeHandle src, const syntax::ExprId value) const noexcept
{
    if (!is_valid(dst) || !is_valid(src)) {
        return is_valid(dst) && this->core_.is_null_literal(value) && this->core_.state_.checked.types.is_pointer(dst);
    }
    if (this->core_.state_.checked.types.get(dst).kind == TypeKind::generic_param
        || this->core_.state_.checked.types.get(src).kind == TypeKind::generic_param) {
        return this->core_.state_.checked.types.same(dst, src);
    }
    if (this->core_.state_.checked.types.is_integer(dst) && this->core_.state_.checked.types.is_integer(src)
        && this->core_.is_integer_literal(value)) {
        const syntax::LiteralExprPayload* const literal =
            syntax::is_valid(value) && value.value < this->core_.ctx_.module.exprs.size()
            ? this->core_.ctx_.module.exprs.literal_payload(value.value)
            : nullptr;
        return literal != nullptr && this->core_.integer_literal_fits_type(dst, literal->text);
    }
    if (this->core_.state_.checked.types.is_pointer(dst) && this->core_.state_.checked.types.is_pointer(src)) {
        const TypeInfo& dst_info = this->core_.state_.checked.types.get(dst);
        const TypeInfo& src_info = this->core_.state_.checked.types.get(src);
        if (dst_info.pointer_mutability == PointerMutability::const_
            && this->core_.state_.checked.types.same(dst_info.pointee, src_info.pointee)) {
            return true;
        }
    }
    if (this->core_.state_.checked.types.is_reference(dst) && this->core_.state_.checked.types.is_reference(src)) {
        const TypeInfo& dst_info = this->core_.state_.checked.types.get(dst);
        const TypeInfo& src_info = this->core_.state_.checked.types.get(src);
        if (!this->core_.state_.checked.types.same(dst_info.pointee, src_info.pointee)) {
            return false;
        }
        return dst_info.pointer_mutability == PointerMutability::const_
            || src_info.pointer_mutability == PointerMutability::mut;
    }
    if (this->core_.state_.checked.types.is_slice(dst) && this->core_.state_.checked.types.is_slice(src)) {
        const TypeInfo& dst_info = this->core_.state_.checked.types.get(dst);
        const TypeInfo& src_info = this->core_.state_.checked.types.get(src);
        if (!this->core_.state_.checked.types.same(dst_info.slice_element, src_info.slice_element)) {
            return false;
        }
        return dst_info.slice_mutability == PointerMutability::const_
            || src_info.slice_mutability == PointerMutability::mut;
    }
    return this->core_.state_.checked.types.same(dst, src);
}

bool SemanticTypeValidator::is_valid_storage_type(const TypeHandle type) const
{
    std::vector<TypeHandle> pending;
    pending.push_back(type);
    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!is_valid(current)) {
            return false;
        }
        const TypeInfo& info = this->core_.state_.checked.types.get(current);
        if (info.kind == TypeKind::generic_param) {
            return true;
        }
        if (this->core_.state_.checked.types.is_void(current) || info.kind == TypeKind::opaque_struct) {
            return false;
        }
        if (info.kind == TypeKind::slice) {
            pending.push_back(info.slice_element);
            continue;
        }
        if (info.kind == TypeKind::reference) {
            pending.push_back(info.pointee);
            continue;
        }
        if (info.kind == TypeKind::tuple) {
            for (const TypeHandle element : info.tuple_elements) {
                pending.push_back(element);
            }
            continue;
        }
        if (info.kind != TypeKind::array) {
            continue;
        }
        const base::u64 element_size = this->core_.abi_size(info.array_element);
        if (element_size != 0 && info.array_count > std::numeric_limits<base::u64>::max() / element_size) {
            return false;
        }
        pending.push_back(info.array_element);
    }
    return true;
}

bool SemanticTypeValidator::check_m2_value_abi(
    const TypeHandle type, const ValueAbiContext context, const base::SourceRange& range) const
{
    if (!is_valid(type)) {
        return true;
    }

    switch (context) {
        case ValueAbiContext::parameter:
            if (this->core_.state_.checked.types.is_array(type)) {
                this->core_.report_unsupported(range, std::string(SEMA_ARRAY_PARAMETER_UNSUPPORTED));
                return false;
            }
            if (this->core_.state_.checked.types.contains_array(type)) {
                this->core_.report_unsupported(range, std::string(SEMA_ARRAY_STRUCT_PARAMETER_UNSUPPORTED));
                return false;
            }
            return true;
        case ValueAbiContext::function_type_parameter:
            if (this->core_.state_.checked.types.is_array(type)
                || this->core_.state_.checked.types.contains_array(type)) {
                this->core_.report_unsupported(range, std::string(SEMA_ARRAY_FUNCTION_TYPE_PARAMETER_UNSUPPORTED));
                return false;
            }
            return true;
        case ValueAbiContext::function_type_return:
            if (this->core_.state_.checked.types.is_array(type)
                || this->core_.state_.checked.types.contains_array(type)) {
                this->core_.report_unsupported(range, std::string(SEMA_ARRAY_FUNCTION_TYPE_RETURN_UNSUPPORTED));
                return false;
            }
            return true;
        case ValueAbiContext::return_value:
            if (this->core_.state_.checked.types.is_array(type)) {
                this->core_.report_unsupported(range, std::string(SEMA_ARRAY_RETURN_UNSUPPORTED));
                return false;
            }
            if (this->core_.state_.checked.types.contains_array(type)) {
                this->core_.report_unsupported(range, std::string(SEMA_ARRAY_STRUCT_RETURN_UNSUPPORTED));
                return false;
            }
            return true;
        case ValueAbiContext::assignment:
            if (this->core_.state_.checked.types.contains_array(type)) {
                this->core_.report_unsupported(range, std::string(SEMA_ARRAY_ASSIGNMENT_UNSUPPORTED));
                return false;
            }
            return true;
        case ValueAbiContext::enum_payload:
            if (this->core_.state_.checked.types.contains_array(type)) {
                this->core_.report_unsupported(range, std::string(SEMA_ENUM_PAYLOAD_ARRAY_UNSUPPORTED));
                return false;
            }
            return true;
        case ValueAbiContext::enum_payload_argument:
            if (this->core_.state_.checked.types.contains_array(type)) {
                this->core_.report_unsupported(range, std::string(SEMA_ENUM_PAYLOAD_ARRAY_ARGUMENT_UNSUPPORTED));
                return false;
            }
            return true;
        case ValueAbiContext::argument:
            if (this->core_.state_.checked.types.contains_array(type)) {
                this->core_.report_unsupported(range, std::string(SEMA_ARGUMENT_ARRAY_UNSUPPORTED));
                return false;
            }
            return true;
    }
    return true;
}

bool SemanticTypeValidator::is_valid_cast(const syntax::ExprKind kind, const TypeHandle dst, const TypeHandle src) const
{
    if (!is_valid(dst) || !is_valid(src)) {
        return false;
    }
    if (this->core_.state_.checked.types.get(dst).kind == TypeKind::generic_param
        || this->core_.state_.checked.types.get(src).kind == TypeKind::generic_param) {
        return false;
    }

    if (kind == syntax::ExprKind::cast) {
        return (this->core_.state_.checked.types.is_integer(dst) || this->core_.state_.checked.types.is_float(dst)
                   || this->core_.state_.checked.types.is_bool(dst))
            && (this->core_.state_.checked.types.is_integer(src) || this->core_.state_.checked.types.is_float(src)
                || this->core_.state_.checked.types.is_bool(src));
    }
    if (kind == syntax::ExprKind::pcast) {
        return this->core_.state_.checked.types.is_pointer(dst)
            && (this->core_.state_.checked.types.is_pointer(src) || this->core_.state_.checked.types.is_reference(src));
    }
    if (kind == syntax::ExprKind::bcast) {
        if (this->core_.state_.checked.types.same(dst, src)) {
            return is_bitcast_type(this->core_.state_.checked.types, dst);
        }
        if (!is_bitcast_type(this->core_.state_.checked.types, dst)
            || !is_bitcast_type(this->core_.state_.checked.types, src)
            || this->core_.abi_size(dst) != this->core_.abi_size(src)) {
            return false;
        }
        if (is_builtin_scalar_bcast_type(this->core_.state_.checked.types, dst)
            && is_builtin_scalar_bcast_type(this->core_.state_.checked.types, src)) {
            return true;
        }
        return this->core_.state_.checked.types.is_pointer(dst) && this->core_.state_.checked.types.is_pointer(src);
    }
    return false;
}

bool SemanticTypeValidator::is_array_containing_value_type(const TypeHandle type) const noexcept
{
    return is_valid(type) && this->core_.state_.checked.types.contains_array(type);
}

const StructInfo* SemanticTypeValidator::find_struct(const TypeHandle type) const noexcept
{
    if (!is_valid(type)) {
        return nullptr;
    }
    if (const auto found = this->core_.state_.types.struct_infos_by_type.find(type.value);
        found != this->core_.state_.types.struct_infos_by_type.end()) {
        if (found->second != nullptr && this->core_.state_.checked.types.same(found->second->type, type)) {
            return found->second;
        }
        return nullptr;
    }
    for (const auto& entry : this->core_.state_.checked.structs) {
        if (this->core_.state_.checked.types.same(entry.second.type, type)) {
            return &entry.second;
        }
    }
    return nullptr;
}

SemanticAbiChecker::SemanticAbiChecker(const SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

void SemanticAbiChecker::validate_type_layouts() const
{
    std::unordered_map<base::u32, TypeLayoutVisitState> states;
    std::unordered_map<base::u32, LayoutResult> results;

    const auto primitive_layout = [&](const TypeHandle type) -> LayoutResult {
        const TypeInfo& info = this->core_.state_.checked.types.get(type);
        if (info.kind == TypeKind::builtin && info.builtin == BuiltinType::void_) {
            return {};
        }
        const SemanticAnalyzerCore::TypeAbiLayout layout = this->abi_layout(type);
        return LayoutResult{layout.size, layout.align, true};
    };

    const auto cached_result = [&](const TypeHandle type) -> LayoutResult {
        if (!is_valid(type)) {
            return {};
        }
        const TypeInfo& info = this->core_.state_.checked.types.get(type);
        if (info.kind == TypeKind::builtin || info.kind == TypeKind::pointer || info.kind == TypeKind::reference
            || info.kind == TypeKind::slice || info.kind == TypeKind::function
            || info.kind == TypeKind::generic_param) {
            return primitive_layout(type);
        }
        if (info.kind == TypeKind::opaque_struct) {
            return {};
        }
        const auto found = results.find(type.value);
        return found == results.end() ? LayoutResult{} : found->second;
    };

    const auto finish_type = [&](const TypeHandle type, const base::SourceRange& range) -> LayoutResult {
        LayoutResult result;
        result.ok = true;
        const TypeInfo& info = this->core_.state_.checked.types.get(type);
        if (info.kind == TypeKind::array) {
            const LayoutResult element = cached_result(info.array_element);
            if (!element.ok) {
                result = LayoutResult{
                    SEMA_ABI_INVALID_SIZE,
                    std::max(SEMA_ABI_MIN_ALIGNMENT, element.align),
                    false,
                };
                return result;
            }
            base::u64 size = SEMA_ABI_INVALID_SIZE;
            if (!checked_mul_u64(info.array_count, element.size, size)) {
                this->core_.report_general(range, std::string(SEMA_ARRAY_STORAGE_OVERFLOW));
                result = LayoutResult{size, element.align, false};
                return result;
            }
            result = LayoutResult{size, element.align, true};
            return result;
        }
        if (info.kind == TypeKind::tuple) {
            base::u64 offset = SEMA_ABI_INVALID_SIZE;
            base::u64 max_align = SEMA_ABI_MIN_ALIGNMENT;
            for (const TypeHandle element_type : info.tuple_elements) {
                const LayoutResult element = cached_result(element_type);
                if (!element.ok) {
                    result.ok = false;
                    continue;
                }
                max_align = std::max(max_align, element.align);
                base::u64 aligned_offset = SEMA_ABI_INVALID_SIZE;
                if (!checked_align_forward(offset, element.align, aligned_offset)) {
                    this->core_.report_general(range, std::string(SEMA_STRUCT_STORAGE_OVERFLOW));
                    result.ok = false;
                }
                base::u64 next_offset = SEMA_ABI_INVALID_SIZE;
                if (!checked_add_u64(aligned_offset, element.size, next_offset)) {
                    this->core_.report_general(range, std::string(SEMA_STRUCT_STORAGE_OVERFLOW));
                    result.ok = false;
                }
                offset = next_offset;
            }
            base::u64 size = SEMA_ABI_INVALID_SIZE;
            if (!checked_align_forward(offset, max_align, size)) {
                this->core_.report_general(range, std::string(SEMA_STRUCT_STORAGE_OVERFLOW));
                result.ok = false;
            }
            result.size = size;
            result.align = max_align;
            return result;
        }
        if (info.kind == TypeKind::struct_) {
            const StructInfo* struct_info = this->core_.find_struct(type);
            if (struct_info == nullptr || struct_info->is_opaque) {
                result.ok = false;
            } else {
                base::u64 offset = SEMA_ABI_INVALID_SIZE;
                base::u64 max_align = SEMA_ABI_MIN_ALIGNMENT;
                for (const StructFieldInfo& field : struct_info->fields) {
                    const LayoutResult field_layout = cached_result(field.type);
                    if (!field_layout.ok) {
                        result.ok = false;
                        continue;
                    }
                    max_align = std::max(max_align, field_layout.align);
                    base::u64 aligned_offset = SEMA_ABI_INVALID_SIZE;
                    if (!checked_align_forward(offset, field_layout.align, aligned_offset)) {
                        this->core_.report_general(field.range, std::string(SEMA_STRUCT_STORAGE_OVERFLOW));
                        result.ok = false;
                    }
                    base::u64 next_offset = SEMA_ABI_INVALID_SIZE;
                    if (!checked_add_u64(aligned_offset, field_layout.size, next_offset)) {
                        this->core_.report_general(field.range, std::string(SEMA_STRUCT_STORAGE_OVERFLOW));
                        result.ok = false;
                    }
                    offset = next_offset;
                }
                base::u64 size = SEMA_ABI_INVALID_SIZE;
                if (!checked_align_forward(offset, max_align, size)) {
                    this->core_.report_general(range, std::string(SEMA_STRUCT_STORAGE_OVERFLOW));
                    result.ok = false;
                }
                result.size = size;
                result.align = max_align;
            }
            return result;
        }
        if (info.kind == TypeKind::enum_) {
            const LayoutResult underlying = cached_result(info.enum_underlying);
            result.size = is_valid(info.enum_underlying) ? underlying.size : SEMA_ABI_INVALID_SIZE;
            result.align = is_valid(info.enum_underlying) ? underlying.align : SEMA_ABI_MIN_ALIGNMENT;
            result.ok = !is_valid(info.enum_underlying) || underlying.ok;
            base::u64 payload_size = SEMA_ABI_INVALID_SIZE;
            base::u64 payload_align = SEMA_ABI_MIN_ALIGNMENT;
            bool has_payload = false;
            for (const auto& entry : this->core_.state_.checked.enum_cases) {
                const EnumCaseInfo& enum_case = entry.second;
                if (!this->core_.state_.checked.types.same(enum_case.type, type) || !is_valid(enum_case.payload_type)) {
                    continue;
                }
                has_payload = true;
                const LayoutResult payload = cached_result(enum_case.payload_type);
                if (!payload.ok) {
                    result.ok = false;
                    continue;
                }
                if (payload.size > payload_size || (payload.size == payload_size && payload.align > payload_align)) {
                    payload_size = payload.size;
                    payload_align = payload.align;
                }
            }
            if (has_payload) {
                const base::u64 max_align = std::max(result.align, payload_align);
                base::u64 storage_offset = SEMA_ABI_INVALID_SIZE;
                if (!checked_align_forward(result.size, payload_align, storage_offset)) {
                    this->core_.report_general(range, std::string(SEMA_ENUM_STORAGE_OVERFLOW));
                    result.ok = false;
                }
                base::u64 total = SEMA_ABI_INVALID_SIZE;
                if (!checked_add_u64(storage_offset, payload_size, total)) {
                    this->core_.report_general(range, std::string(SEMA_ENUM_STORAGE_OVERFLOW));
                    result.ok = false;
                }
                base::u64 size = SEMA_ABI_INVALID_SIZE;
                if (!checked_align_forward(total, max_align, size)) {
                    this->core_.report_general(range, std::string(SEMA_ENUM_STORAGE_OVERFLOW));
                    result.ok = false;
                }
                result.size = size;
                result.align = max_align;
            }
            return result;
        }
        return result;
    };

    const auto push_dependency = [&](std::vector<TypeLayoutFrame>& stack, const TypeHandle dependency,
                                     const base::SourceRange& dependency_range) {
        if (!is_valid(dependency)) {
            return;
        }
        const TypeInfo& dependency_info = this->core_.state_.checked.types.get(dependency);
        if (dependency_info.kind == TypeKind::builtin || dependency_info.kind == TypeKind::pointer
            || dependency_info.kind == TypeKind::reference || dependency_info.kind == TypeKind::slice
            || dependency_info.kind == TypeKind::function || dependency_info.kind == TypeKind::generic_param
            || dependency_info.kind == TypeKind::opaque_struct || results.contains(dependency.value)) {
            return;
        }
        const auto state = states.find(dependency.value);
        if (state != states.end() && state->second == TypeLayoutVisitState::visiting) {
            this->core_.report_general(dependency_range,
                "recursive value type is not valid storage: "
                    + this->core_.state_.checked.types.display_name(dependency));
            results[dependency.value] = {};
            states[dependency.value] = TypeLayoutVisitState::done;
            return;
        }
        stack.push_back(TypeLayoutFrame{dependency, dependency_range, TypeLayoutFrameStage::enter});
    };

    const auto push_children = [&](std::vector<TypeLayoutFrame>& stack, const TypeHandle type,
                                   const base::SourceRange& range) {
        const TypeInfo& info = this->core_.state_.checked.types.get(type);
        if (info.kind == TypeKind::array) {
            push_dependency(stack, info.array_element, range);
            return;
        }
        if (info.kind == TypeKind::tuple) {
            for (auto element = info.tuple_elements.rbegin(); element != info.tuple_elements.rend(); ++element) {
                push_dependency(stack, *element, range);
            }
            return;
        }
        if (info.kind == TypeKind::struct_) {
            const StructInfo* struct_info = this->core_.find_struct(type);
            if (struct_info != nullptr && !struct_info->is_opaque) {
                for (auto field = struct_info->fields.rbegin(); field != struct_info->fields.rend(); ++field) {
                    push_dependency(stack, field->type, field->range);
                }
            }
            return;
        }
        if (info.kind == TypeKind::enum_) {
            push_dependency(stack, info.enum_underlying, range);
            for (const auto& entry : this->core_.state_.checked.enum_cases) {
                const EnumCaseInfo& enum_case = entry.second;
                if (this->core_.state_.checked.types.same(enum_case.type, type) && is_valid(enum_case.payload_type)) {
                    push_dependency(stack, enum_case.payload_type, enum_case.range);
                }
            }
            return;
        }
    };

    const auto compute = [&](const TypeHandle type, const base::SourceRange& range) -> LayoutResult {
        if (!is_valid(type)) {
            return {};
        }
        const TypeInfo& info = this->core_.state_.checked.types.get(type);
        if (info.kind == TypeKind::builtin || info.kind == TypeKind::pointer || info.kind == TypeKind::reference
            || info.kind == TypeKind::slice || info.kind == TypeKind::function
            || info.kind == TypeKind::generic_param) {
            return primitive_layout(type);
        }
        if (info.kind == TypeKind::opaque_struct) {
            return {};
        }
        if (const auto found = results.find(type.value); found != results.end()) {
            return found->second;
        }

        std::vector<TypeLayoutFrame> stack;
        stack.reserve(SEMA_TYPE_LAYOUT_INITIAL_STACK_CAPACITY);
        stack.push_back(TypeLayoutFrame{type, range, TypeLayoutFrameStage::enter});
        while (!stack.empty()) {
            const TypeLayoutFrame frame = stack.back();
            stack.pop_back();
            if (frame.stage == TypeLayoutFrameStage::finish) {
                if (!results.contains(frame.type.value)) {
                    results[frame.type.value] = finish_type(frame.type, frame.range);
                }
                states[frame.type.value] = TypeLayoutVisitState::done;
                continue;
            }
            if (results.contains(frame.type.value)) {
                states[frame.type.value] = TypeLayoutVisitState::done;
                continue;
            }
            states[frame.type.value] = TypeLayoutVisitState::visiting;
            stack.push_back(TypeLayoutFrame{frame.type, frame.range, TypeLayoutFrameStage::finish});
            push_children(stack, frame.type, frame.range);
        }
        return results.at(type.value);
    };

    for (const auto& entry : this->core_.state_.checked.structs) {
        const StructInfo& info = entry.second;
        if (!info.is_opaque) {
            const base::SourceRange range = info.fields.empty() ? base::SourceRange{} : info.fields.front().range;
            static_cast<void>(compute(info.type, range));
        }
    }

    std::unordered_set<base::u32> seen_enums;
    for (const auto& entry : this->core_.state_.types.named_types) {
        const TypeHandle type = entry.second;
        if (is_valid(type) && this->core_.state_.checked.types.get(type).kind == TypeKind::enum_
            && seen_enums.insert(type.value).second) {
            static_cast<void>(compute(type, {}));
        }
    }
    for (const auto& entry : this->core_.state_.checked.enum_cases) {
        const TypeHandle type = entry.second.type;
        if (is_valid(type) && seen_enums.insert(type.value).second) {
            static_cast<void>(compute(type, entry.second.range));
        }
    }
}

SemanticAnalyzerCore::TypeAbiLayout SemanticAbiChecker::abi_layout(const TypeHandle type) const
{
    const auto builtin_layout = [](const BuiltinType builtin) noexcept -> SemanticAnalyzerCore::TypeAbiLayout {
        switch (builtin) {
            case BuiltinType::void_:
                return SemanticAnalyzerCore::TypeAbiLayout{SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
            case BuiltinType::bool_:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(bool), alignof(bool)};
            case BuiltinType::i8:
            case BuiltinType::u8:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(std::uint8_t), alignof(std::uint8_t)};
            case BuiltinType::i16:
            case BuiltinType::u16:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(std::uint16_t), alignof(std::uint16_t)};
            case BuiltinType::i32:
            case BuiltinType::u32:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(std::uint32_t), alignof(std::uint32_t)};
            case BuiltinType::i64:
            case BuiltinType::u64:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(std::uint64_t), alignof(std::uint64_t)};
            case BuiltinType::isize:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(std::ptrdiff_t), alignof(std::ptrdiff_t)};
            case BuiltinType::usize:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(std::size_t), alignof(std::size_t)};
            case BuiltinType::f32:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(float), alignof(float)};
            case BuiltinType::f64:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(double), alignof(double)};
            case BuiltinType::str:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(void*) + sizeof(std::size_t), alignof(void*)};
            case BuiltinType::char_:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(std::uint32_t), alignof(std::uint32_t)};
        }
        return SemanticAnalyzerCore::TypeAbiLayout{SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
    };

    const auto cached = [](const std::unordered_map<base::u32, SemanticAnalyzerCore::TypeAbiLayout>& layouts,
                            const TypeHandle current) noexcept {
        const auto found = layouts.find(current.value);
        return found == layouts.end()
            ? SemanticAnalyzerCore::TypeAbiLayout{SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT}
            : found->second;
    };

    const auto finish_type = [&](const TypeHandle current,
                                 const std::unordered_map<base::u32, SemanticAnalyzerCore::TypeAbiLayout>& layouts) {
        const TypeInfo& info = this->core_.state_.checked.types.get(current);
        switch (info.kind) {
            case TypeKind::builtin:
                return builtin_layout(info.builtin);
            case TypeKind::pointer:
            case TypeKind::reference:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(void*), alignof(void*)};
            case TypeKind::function:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(void*), alignof(void*)};
            case TypeKind::slice:
                return SemanticAnalyzerCore::TypeAbiLayout{sizeof(void*) + sizeof(std::size_t), alignof(void*)};
            case TypeKind::array: {
                const SemanticAnalyzerCore::TypeAbiLayout element = cached(layouts, info.array_element);
                return SemanticAnalyzerCore::TypeAbiLayout{
                    mul_saturating(info.array_count, element.size),
                    element.align,
                };
            }
            case TypeKind::tuple: {
                base::u64 offset = SEMA_ABI_INVALID_SIZE;
                base::u64 max_align = SEMA_ABI_MIN_ALIGNMENT;
                for (const TypeHandle element_type : info.tuple_elements) {
                    const SemanticAnalyzerCore::TypeAbiLayout element = cached(layouts, element_type);
                    max_align = std::max(max_align, element.align);
                    offset = align_forward(offset, element.align);
                    offset = add_saturating(offset, element.size);
                }
                return SemanticAnalyzerCore::TypeAbiLayout{align_forward(offset, max_align), max_align};
            }
            case TypeKind::enum_: {
                const SemanticAnalyzerCore::TypeAbiLayout tag = cached(layouts, info.enum_underlying);
                if (!is_valid(info.enum_payload_storage)) {
                    return tag;
                }
                const SemanticAnalyzerCore::TypeAbiLayout storage = cached(layouts, info.enum_payload_storage);
                const base::u64 max_align = std::max(tag.align, info.enum_payload_align);
                const base::u64 storage_offset = align_forward(tag.size, info.enum_payload_align);
                return SemanticAnalyzerCore::TypeAbiLayout{
                    align_forward(add_saturating(storage_offset, storage.size), max_align),
                    max_align,
                };
            }
            case TypeKind::struct_: {
                const StructInfo* struct_info = this->core_.find_struct(current);
                if (struct_info == nullptr) {
                    return SemanticAnalyzerCore::TypeAbiLayout{SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
                }
                base::u64 offset = SEMA_ABI_INVALID_SIZE;
                base::u64 max_align = SEMA_ABI_MIN_ALIGNMENT;
                for (const StructFieldInfo& field : struct_info->fields) {
                    const SemanticAnalyzerCore::TypeAbiLayout field_layout = cached(layouts, field.type);
                    max_align = std::max(max_align, field_layout.align);
                    offset = align_forward(offset, field_layout.align);
                    offset = add_saturating(offset, field_layout.size);
                }
                return SemanticAnalyzerCore::TypeAbiLayout{align_forward(offset, max_align), max_align};
            }
            case TypeKind::opaque_struct:
            case TypeKind::generic_param:
                return SemanticAnalyzerCore::TypeAbiLayout{SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
        }
        return SemanticAnalyzerCore::TypeAbiLayout{SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
    };

    const auto push_dependency =
        [&](std::vector<TypeLayoutFrame>& stack, const TypeHandle dependency,
            std::unordered_map<base::u32, TypeLayoutVisitState>& states,
            const std::unordered_map<base::u32, SemanticAnalyzerCore::TypeAbiLayout>& layouts) {
            if (!is_valid(dependency) || layouts.contains(dependency.value)) {
                return;
            }
            const TypeInfo& info = this->core_.state_.checked.types.get(dependency);
            if (info.kind == TypeKind::builtin || info.kind == TypeKind::pointer || info.kind == TypeKind::reference
                || info.kind == TypeKind::slice || info.kind == TypeKind::function
                || info.kind == TypeKind::generic_param || info.kind == TypeKind::opaque_struct) {
                stack.push_back(TypeLayoutFrame{dependency, {}, TypeLayoutFrameStage::enter});
                return;
            }
            const auto found = states.find(dependency.value);
            if (found != states.end() && found->second == TypeLayoutVisitState::visiting) {
                return;
            }
            stack.push_back(TypeLayoutFrame{dependency, {}, TypeLayoutFrameStage::enter});
        };

    const auto push_children = [&](std::vector<TypeLayoutFrame>& stack, const TypeHandle current,
                                   std::unordered_map<base::u32, TypeLayoutVisitState>& states,
                                   const std::unordered_map<base::u32, SemanticAnalyzerCore::TypeAbiLayout>& layouts) {
        const TypeInfo& info = this->core_.state_.checked.types.get(current);
        switch (info.kind) {
            case TypeKind::array:
                push_dependency(stack, info.array_element, states, layouts);
                break;
            case TypeKind::tuple:
                for (auto element = info.tuple_elements.rbegin(); element != info.tuple_elements.rend(); ++element) {
                    push_dependency(stack, *element, states, layouts);
                }
                break;
            case TypeKind::enum_:
                push_dependency(stack, info.enum_underlying, states, layouts);
                push_dependency(stack, info.enum_payload_storage, states, layouts);
                break;
            case TypeKind::struct_: {
                const StructInfo* struct_info = this->core_.find_struct(current);
                if (struct_info != nullptr) {
                    for (auto field = struct_info->fields.rbegin(); field != struct_info->fields.rend(); ++field) {
                        push_dependency(stack, field->type, states, layouts);
                    }
                }
                break;
            }
            case TypeKind::builtin:
            case TypeKind::pointer:
            case TypeKind::reference:
            case TypeKind::slice:
            case TypeKind::function:
            case TypeKind::generic_param:
            case TypeKind::opaque_struct:
                break;
        }
    };

    if (!is_valid(type)) {
        return SemanticAnalyzerCore::TypeAbiLayout{SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
    }

    std::unordered_map<base::u32, SemanticAnalyzerCore::TypeAbiLayout> layouts;
    std::unordered_map<base::u32, TypeLayoutVisitState> states;
    std::vector<TypeLayoutFrame> stack;
    stack.reserve(SEMA_TYPE_LAYOUT_INITIAL_STACK_CAPACITY);
    stack.push_back(TypeLayoutFrame{type, {}, TypeLayoutFrameStage::enter});
    while (!stack.empty()) {
        const TypeLayoutFrame frame = stack.back();
        stack.pop_back();
        if (!is_valid(frame.type) || layouts.contains(frame.type.value)) {
            continue;
        }
        const TypeInfo& info = this->core_.state_.checked.types.get(frame.type);
        if (frame.stage == TypeLayoutFrameStage::finish) {
            layouts[frame.type.value] = finish_type(frame.type, layouts);
            states[frame.type.value] = TypeLayoutVisitState::done;
            continue;
        }
        if (info.kind == TypeKind::builtin || info.kind == TypeKind::pointer || info.kind == TypeKind::reference
            || info.kind == TypeKind::slice || info.kind == TypeKind::function || info.kind == TypeKind::generic_param
            || info.kind == TypeKind::opaque_struct) {
            layouts[frame.type.value] = finish_type(frame.type, layouts);
            states[frame.type.value] = TypeLayoutVisitState::done;
            continue;
        }
        const auto state = states.find(frame.type.value);
        if (state != states.end() && state->second == TypeLayoutVisitState::visiting) {
            layouts[frame.type.value] =
                SemanticAnalyzerCore::TypeAbiLayout{SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
            states[frame.type.value] = TypeLayoutVisitState::done;
            continue;
        }
        states[frame.type.value] = TypeLayoutVisitState::visiting;
        stack.push_back(TypeLayoutFrame{frame.type, {}, TypeLayoutFrameStage::finish});
        push_children(stack, frame.type, states, layouts);
    }
    return cached(layouts, type);
}

base::u64 SemanticAbiChecker::abi_size(const TypeHandle type) const
{
    return this->abi_layout(type).size;
}

base::u64 SemanticAbiChecker::abi_align(const TypeHandle type) const
{
    return this->abi_layout(type).align;
}

} // namespace aurex::sema
