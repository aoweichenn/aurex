#include <aurex/frontend/sema/canonical_type_builder.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_CANONICAL_TYPE_INVALID_HANDLE = "invalid type handle";
constexpr std::string_view SEMA_CANONICAL_TYPE_UNKNOWN_HANDLE = "unknown type handle";
constexpr std::string_view SEMA_CANONICAL_TYPE_UNRESOLVED_NOMINAL = "unresolved nominal type key";
constexpr std::string_view SEMA_CANONICAL_TYPE_UNRESOLVED_GENERIC_PARAM = "unresolved generic parameter key";
constexpr std::string_view SEMA_CANONICAL_TYPE_UNRESOLVED_ASSOCIATED_MEMBER = "unresolved associated type member key";
constexpr std::string_view SEMA_CANONICAL_TYPE_UNSUPPORTED_KIND = "unsupported type kind";
constexpr std::string_view SEMA_CANONICAL_TYPE_UNSUPPORTED_BUILTIN = "unsupported builtin type";
constexpr std::string_view SEMA_CANONICAL_TYPE_UNSUPPORTED_MUTABILITY = "unsupported pointer mutability";
constexpr std::string_view SEMA_CANONICAL_TYPE_UNSUPPORTED_CALL_CONV = "unsupported function call convention";
constexpr base::usize SEMA_CANONICAL_TYPE_BUILD_STACK_RESERVE = 16;

[[nodiscard]] base::Result<query::CanonicalTypeKey> canonical_type_error(const std::string_view message)
{
    return base::Result<query::CanonicalTypeKey>::fail({
        base::ErrorCode::internal_error,
        std::string(message),
    });
}

[[nodiscard]] std::optional<query::BuiltinTypeKey> canonical_builtin_type(const BuiltinType type) noexcept
{
    switch (type) {
        case BuiltinType::void_:
            return query::BuiltinTypeKey::void_;
        case BuiltinType::bool_:
            return query::BuiltinTypeKey::bool_;
        case BuiltinType::i8:
            return query::BuiltinTypeKey::i8;
        case BuiltinType::u8:
            return query::BuiltinTypeKey::u8;
        case BuiltinType::i16:
            return query::BuiltinTypeKey::i16;
        case BuiltinType::u16:
            return query::BuiltinTypeKey::u16;
        case BuiltinType::i32:
            return query::BuiltinTypeKey::i32;
        case BuiltinType::u32:
            return query::BuiltinTypeKey::u32;
        case BuiltinType::i64:
            return query::BuiltinTypeKey::i64;
        case BuiltinType::u64:
            return query::BuiltinTypeKey::u64;
        case BuiltinType::isize:
            return query::BuiltinTypeKey::isize;
        case BuiltinType::usize:
            return query::BuiltinTypeKey::usize;
        case BuiltinType::f32:
            return query::BuiltinTypeKey::f32;
        case BuiltinType::f64:
            return query::BuiltinTypeKey::f64;
        case BuiltinType::str:
            return query::BuiltinTypeKey::str;
        case BuiltinType::char_:
            return query::BuiltinTypeKey::char_;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<query::PointerMutabilityKey> canonical_mutability(
    const PointerMutability mutability) noexcept
{
    switch (mutability) {
        case PointerMutability::mut:
            return query::PointerMutabilityKey::mut;
        case PointerMutability::const_:
            return query::PointerMutabilityKey::const_;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<query::FunctionCallConvKey> canonical_call_conv(const FunctionCallConv call_conv) noexcept
{
    switch (call_conv) {
        case FunctionCallConv::aurex:
            return query::FunctionCallConvKey::aurex;
        case FunctionCallConv::c:
            return query::FunctionCallConvKey::c;
    }
    return std::nullopt;
}

struct TypeBuildFrame {
    TypeHandle source = INVALID_TYPE_HANDLE;
    query::CanonicalTypeKey* target = nullptr;
};

void push_child(std::vector<TypeBuildFrame>& pending, const TypeHandle source, query::CanonicalTypeKey& target)
{
    pending.push_back(TypeBuildFrame{source, &target});
}

void push_children_reverse(
    std::vector<TypeBuildFrame>& pending, const TypeHandleList& sources, std::vector<query::CanonicalTypeKey>& targets)
{
    for (base::usize index = sources.size(); index > 0; --index) {
        push_child(pending, sources[index - 1], targets[index - 1]);
    }
}

[[nodiscard]] base::Result<void> lower_builtin_type(const TypeInfo& info, query::CanonicalTypeKey& target)
{
    const std::optional<query::BuiltinTypeKey> builtin = canonical_builtin_type(info.builtin);
    if (!builtin.has_value()) {
        return base::Result<void>::fail({
            base::ErrorCode::internal_error,
            std::string(SEMA_CANONICAL_TYPE_UNSUPPORTED_BUILTIN),
        });
    }
    target = query::canonical_builtin(*builtin);
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<void> lower_pointer_type(
    std::vector<TypeBuildFrame>& pending, const TypeInfo& info, query::CanonicalTypeKey& target)
{
    const std::optional<query::PointerMutabilityKey> mutability = canonical_mutability(info.pointer_mutability);
    if (!mutability.has_value()) {
        return base::Result<void>::fail({
            base::ErrorCode::internal_error,
            std::string(SEMA_CANONICAL_TYPE_UNSUPPORTED_MUTABILITY),
        });
    }
    target.kind = query::CanonicalTypeKind::pointer;
    target.mutability = *mutability;
    target.children.resize(1);
    push_child(pending, info.pointee, target.children.front());
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<void> lower_reference_type(
    std::vector<TypeBuildFrame>& pending, const TypeInfo& info, query::CanonicalTypeKey& target)
{
    const std::optional<query::PointerMutabilityKey> mutability = canonical_mutability(info.pointer_mutability);
    if (!mutability.has_value()) {
        return base::Result<void>::fail({
            base::ErrorCode::internal_error,
            std::string(SEMA_CANONICAL_TYPE_UNSUPPORTED_MUTABILITY),
        });
    }
    target.kind = query::CanonicalTypeKind::reference;
    target.mutability = *mutability;
    target.children.resize(1);
    push_child(pending, info.pointee, target.children.front());
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<void> lower_array_type(
    std::vector<TypeBuildFrame>& pending, const TypeInfo& info, query::CanonicalTypeKey& target)
{
    target.kind = query::CanonicalTypeKind::array;
    target.array_count = info.array_count;
    target.children.resize(1);
    push_child(pending, info.array_element, target.children.front());
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<void> lower_slice_type(
    std::vector<TypeBuildFrame>& pending, const TypeInfo& info, query::CanonicalTypeKey& target)
{
    const std::optional<query::PointerMutabilityKey> mutability = canonical_mutability(info.slice_mutability);
    if (!mutability.has_value()) {
        return base::Result<void>::fail({
            base::ErrorCode::internal_error,
            std::string(SEMA_CANONICAL_TYPE_UNSUPPORTED_MUTABILITY),
        });
    }
    target.kind = query::CanonicalTypeKind::slice;
    target.mutability = *mutability;
    target.children.resize(1);
    push_child(pending, info.slice_element, target.children.front());
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<void> lower_tuple_type(
    std::vector<TypeBuildFrame>& pending, const TypeInfo& info, query::CanonicalTypeKey& target)
{
    target.kind = query::CanonicalTypeKind::tuple;
    target.children.resize(info.tuple_elements.size());
    push_children_reverse(pending, info.tuple_elements, target.children);
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<void> lower_function_type(
    std::vector<TypeBuildFrame>& pending, const TypeInfo& info, query::CanonicalTypeKey& target)
{
    const std::optional<query::FunctionCallConvKey> call_conv = canonical_call_conv(info.function_call_conv);
    if (!call_conv.has_value()) {
        return base::Result<void>::fail({
            base::ErrorCode::internal_error,
            std::string(SEMA_CANONICAL_TYPE_UNSUPPORTED_CALL_CONV),
        });
    }
    target.kind = query::CanonicalTypeKind::function;
    target.function_call_conv = *call_conv;
    target.function_is_unsafe = info.function_is_unsafe;
    target.function_is_variadic = info.function_is_variadic;
    target.function_param_count = static_cast<base::u32>(info.function_params.size());
    target.children.resize(info.function_params.size() + 1);
    push_child(pending, info.function_return, target.children.back());
    push_children_reverse(pending, info.function_params, target.children);
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<void> lower_nominal_type(std::vector<TypeBuildFrame>& pending,
    const CanonicalTypeKeyResolver& resolver, const TypeHandle handle, const TypeInfo& info,
    query::CanonicalTypeKey& target)
{
    const std::optional<query::DefKey> definition = resolver.nominal_type_key(handle, info);
    if (!definition.has_value()) {
        return base::Result<void>::fail({
            base::ErrorCode::internal_error,
            std::string(SEMA_CANONICAL_TYPE_UNRESOLVED_NOMINAL),
        });
    }
    target.kind = query::CanonicalTypeKind::nominal;
    target.nominal_def = *definition;
    target.children.resize(info.generic_args.size());
    push_children_reverse(pending, info.generic_args, target.children);
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<void> lower_generic_param_type(const CanonicalTypeKeyResolver& resolver,
    const TypeHandle handle, const TypeInfo& info, query::CanonicalTypeKey& target)
{
    const std::optional<query::GenericParamKey> parameter = resolver.generic_param_key(handle, info);
    if (!parameter.has_value()) {
        return base::Result<void>::fail({
            base::ErrorCode::internal_error,
            std::string(SEMA_CANONICAL_TYPE_UNRESOLVED_GENERIC_PARAM),
        });
    }
    target = query::canonical_generic_param(*parameter);
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<void> lower_associated_projection_type(
    std::vector<TypeBuildFrame>& pending, const TypeInfo& info, query::CanonicalTypeKey& target)
{
    if (!query::is_valid(info.associated_member)) {
        return base::Result<void>::fail({
            base::ErrorCode::internal_error,
            std::string(SEMA_CANONICAL_TYPE_UNRESOLVED_ASSOCIATED_MEMBER),
        });
    }
    target.kind = query::CanonicalTypeKind::associated_type_projection;
    target.associated_member = info.associated_member;
    target.children.resize(1);
    push_child(pending, info.associated_base, target.children.front());
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<void> lower_type_frame(std::vector<TypeBuildFrame>& pending, const TypeTable& types,
    const CanonicalTypeKeyResolver& resolver, const TypeBuildFrame frame)
{
    if (!is_valid(frame.source)) {
        return base::Result<void>::fail({
            base::ErrorCode::internal_error,
            std::string(SEMA_CANONICAL_TYPE_INVALID_HANDLE),
        });
    }
    if (frame.source.value >= types.size()) {
        return base::Result<void>::fail({
            base::ErrorCode::internal_error,
            std::string(SEMA_CANONICAL_TYPE_UNKNOWN_HANDLE),
        });
    }

    const TypeInfo& info = types.get(frame.source);
    switch (info.kind) {
        case TypeKind::builtin:
            return lower_builtin_type(info, *frame.target);
        case TypeKind::pointer:
            return lower_pointer_type(pending, info, *frame.target);
        case TypeKind::reference:
            return lower_reference_type(pending, info, *frame.target);
        case TypeKind::array:
            return lower_array_type(pending, info, *frame.target);
        case TypeKind::slice:
            return lower_slice_type(pending, info, *frame.target);
        case TypeKind::tuple:
            return lower_tuple_type(pending, info, *frame.target);
        case TypeKind::function:
            return lower_function_type(pending, info, *frame.target);
        case TypeKind::struct_:
        case TypeKind::enum_:
        case TypeKind::opaque_struct:
            return lower_nominal_type(pending, resolver, frame.source, info, *frame.target);
        case TypeKind::generic_param:
            return lower_generic_param_type(resolver, frame.source, info, *frame.target);
        case TypeKind::associated_projection:
            return lower_associated_projection_type(pending, info, *frame.target);
    }
    return base::Result<void>::fail({
        base::ErrorCode::internal_error,
        std::string(SEMA_CANONICAL_TYPE_UNSUPPORTED_KIND),
    });
}

} // namespace

base::Result<query::CanonicalTypeKey> build_canonical_type_key(
    const TypeTable& types, const TypeHandle type, const CanonicalTypeKeyResolver& resolver)
{
    if (!is_valid(type)) {
        return canonical_type_error(SEMA_CANONICAL_TYPE_INVALID_HANDLE);
    }

    query::CanonicalTypeKey root;
    std::vector<TypeBuildFrame> pending;
    pending.reserve(SEMA_CANONICAL_TYPE_BUILD_STACK_RESERVE);
    pending.push_back(TypeBuildFrame{type, &root});

    while (!pending.empty()) {
        const TypeBuildFrame frame = pending.back();
        pending.pop_back();
        base::Result<void> lowered = lower_type_frame(pending, types, resolver, frame);
        if (!lowered) {
            return base::Result<query::CanonicalTypeKey>::fail(lowered.error());
        }
    }

    return base::Result<query::CanonicalTypeKey>::ok(std::move(root));
}

} // namespace aurex::sema
