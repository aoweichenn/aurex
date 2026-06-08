#include <aurex/frontend/sema/sema_messages.hpp>
#include <aurex/frontend/sema/canonical_type_builder.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <frontend/sema/internal/services/private/sema_type_services.hpp>

namespace aurex::sema {

namespace {

constexpr base::usize SEMA_TYPE_LAYOUT_INITIAL_STACK_CAPACITY = 16;
constexpr base::u64 SEMA_ABI_INVALID_SIZE = 0;
constexpr base::u64 SEMA_ABI_MIN_ALIGNMENT = 1;
constexpr base::usize SEMA_DYN_TRAIT_COMPOSITION_MIN_PRINCIPALS = 2;

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
    build_dyn_trait,
    build_dyn_trait_composition,
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
    base::usize dyn_trait_arg_count = 0;
    base::usize dyn_trait_associated_constraint_count = 0;
    base::usize dyn_trait_principal_count = 0;
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

[[nodiscard]] bool trait_object_key_less(
    const query::TraitObjectTypeKey& lhs, const query::TraitObjectTypeKey& rhs) noexcept
{
    if (lhs.principal_trait.global_id != rhs.principal_trait.global_id) {
        return lhs.principal_trait.global_id < rhs.principal_trait.global_id;
    }
    return lhs.global_id < rhs.global_id;
}

[[nodiscard]] bool type_handles_are_same(
    const TypeTable& types, const TypeHandle lhs, const TypeHandle rhs) noexcept
{
    return is_valid(lhs) && is_valid(rhs) && lhs.value < types.size() && rhs.value < types.size()
        && types.same(lhs, rhs);
}

[[nodiscard]] std::vector<TypeHandle> canonical_principal_types(
    const TypeTable& types, const std::span<const TypeHandle> principal_types)
{
    std::vector<TypeHandle> sorted;
    sorted.reserve(principal_types.size());
    for (const TypeHandle principal : principal_types) {
        if (!is_valid(principal) || principal.value >= types.size()) {
            sorted.push_back(principal);
            continue;
        }
        const TypeInfo& info = types.get(principal);
        if (info.kind != TypeKind::trait_object || !query::is_valid(info.trait_object_key)) {
            sorted.push_back(principal);
            continue;
        }
        sorted.push_back(principal);
    }
    std::ranges::sort(sorted, [&](const TypeHandle lhs, const TypeHandle rhs) {
        if (!is_valid(lhs) || lhs.value >= types.size() || !is_valid(rhs) || rhs.value >= types.size()) {
            return lhs.value < rhs.value;
        }
        return trait_object_key_less(types.get(lhs).trait_object_key, types.get(rhs).trait_object_key);
    });
    return sorted;
}

[[nodiscard]] query::PrincipalSetPrincipalDescriptor principal_descriptor(
    const TypeTable& types, const TypeHandle principal)
{
    const TypeInfo& info = types.get(principal);
    return query::PrincipalSetPrincipalDescriptor{
        info.trait_object_key,
        std::string(info.trait_object_name.view()),
        types.display_name(principal),
    };
}

[[nodiscard]] std::vector<query::PrincipalSetPrincipalDescriptor> principal_descriptors(
    const TypeTable& types, const std::span<const TypeHandle> principal_types)
{
    std::vector<query::PrincipalSetPrincipalDescriptor> descriptors;
    descriptors.reserve(principal_types.size());
    for (const TypeHandle principal : principal_types) {
        descriptors.push_back(principal_descriptor(types, principal));
    }
    return descriptors;
}

[[nodiscard]] query::PrincipalMethodNamespaceEntry principal_method_entry(
    const TypeInfo& principal_info,
    const TraitObjectMethodSlotFact& slot,
    const query::PrincipalMethodNamespaceStatus status)
{
    return query::PrincipalMethodNamespaceEntry{
        principal_info.trait_object_key,
        std::string(principal_info.trait_object_name.view()),
        std::string(slot.method_name.view()),
        slot.slot,
        status,
    };
}

[[nodiscard]] std::vector<query::PrincipalMethodNamespaceEntry> principal_method_entries(
    const CheckedModule& checked, const std::span<const TypeHandle> principal_types)
{
    std::vector<query::PrincipalMethodNamespaceEntry> entries;
    for (const TypeHandle principal : principal_types) {
        const TypeInfo& info = checked.types.get(principal);
        for (const TraitObjectMethodSlotFact& slot : checked.trait_object_method_slots) {
            if (slot.object_type.value != principal.value) {
                continue;
            }
            query::PrincipalMethodNamespaceStatus status =
                query::PrincipalMethodNamespaceStatus::unique_principal_method;
            for (const TypeHandle other : principal_types) {
                if (other.value == principal.value) {
                    continue;
                }
                const TypeInfo& other_info = checked.types.get(other);
                for (const TraitObjectMethodSlotFact& other_slot : checked.trait_object_method_slots) {
                    if (other_slot.object_type.value == other.value && other_slot.method_name_id == slot.method_name_id
                        && other_info.trait_object_key.global_id != info.trait_object_key.global_id) {
                        status = query::PrincipalMethodNamespaceStatus::ambiguous_requires_principal;
                    }
                }
            }
            entries.push_back(principal_method_entry(info, slot, status));
        }
    }
    std::ranges::sort(entries, [](const query::PrincipalMethodNamespaceEntry& lhs,
                                  const query::PrincipalMethodNamespaceEntry& rhs) {
        if (lhs.method_name != rhs.method_name) {
            return lhs.method_name < rhs.method_name;
        }
        return lhs.principal_object.global_id < rhs.principal_object.global_id;
    });
    return entries;
}

[[nodiscard]] std::vector<query::AssociatedEqualityMergeFact> associated_equality_merge_facts(
    SemanticAnalyzerCore& core,
    const query::StableFingerprint128 principal_set_identity,
    const std::span<const TypeHandle> principal_types,
    const base::SourceRange& range)
{
    struct MergeState {
        query::MemberKey member;
        query::CanonicalTypeKey merged_type;
        TypeHandle merged_handle = INVALID_TYPE_HANDLE;
        std::vector<query::TraitObjectTypeKey> contributors;
        std::string name;
        bool conflict = false;
    };

    std::vector<MergeState> states;
    for (const TypeHandle principal : principal_types) {
        const TypeInfo& info = core.state_.checked.types.get(principal);
        for (const TraitObjectAssociatedTypeEquality& equality : info.trait_object_associated_equalities) {
            const std::string equality_name(equality.name.view());
            auto found = std::ranges::find_if(states, [&](const MergeState& candidate) {
                return candidate.name == equality_name;
            });
            if (found == states.end()) {
                base::Result<query::CanonicalTypeKey> key = core.checked_canonical_type_key(equality.value_type);
                if (!key) {
                    core.report_internal_contract(range, key.error().message);
                    continue;
                }
                MergeState state;
                state.member = equality.associated_member;
                state.merged_type = key.take_value();
                state.merged_handle = equality.value_type;
                state.contributors.push_back(info.trait_object_key);
                state.name = equality_name;
                states.push_back(std::move(state));
                continue;
            }
            found->contributors.push_back(info.trait_object_key);
            if (!type_handles_are_same(core.state_.checked.types, found->merged_handle, equality.value_type)) {
                found->conflict = true;
            }
        }
    }

    std::vector<query::AssociatedEqualityMergeFact> facts;
    facts.reserve(states.size());
    for (MergeState& state : states) {
        std::ranges::sort(state.contributors, trait_object_key_less);
        const auto unique_end = std::ranges::unique(state.contributors,
            [](const query::TraitObjectTypeKey& lhs, const query::TraitObjectTypeKey& rhs) {
                return lhs.global_id == rhs.global_id;
            });
        state.contributors.erase(unique_end.begin(), unique_end.end());
        query::AssociatedEqualityMergeFact fact;
        fact.principal_set_identity = principal_set_identity;
        fact.associated_type = state.member;
        fact.merged_type = state.merged_type;
        fact.status = state.conflict ? query::PrincipalAssociatedEqualityMergeStatus::conflict
                                     : query::PrincipalAssociatedEqualityMergeStatus::satisfied;
        fact.contributing_principals = std::move(state.contributors);
        fact.associated_type_name = state.name;
        fact.merged_type_name = state.conflict ? std::string("<conflict>")
                                               : core.state_.checked.types.display_name(state.merged_handle);
        facts.push_back(std::move(fact));
    }
    std::ranges::sort(facts, [](const query::AssociatedEqualityMergeFact& lhs,
                                const query::AssociatedEqualityMergeFact& rhs) {
        if (lhs.associated_type_name != rhs.associated_type_name) {
            return lhs.associated_type_name < rhs.associated_type_name;
        }
        return lhs.associated_type.global_id < rhs.associated_type.global_id;
    });
    return facts;
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
                    case syntax::TypeKind::dyn_trait: {
                        if (!type.dyn_trait_principals.empty()) {
                            TypeResolveAction build;
                            build.kind = TypeResolveActionKind::build_dyn_trait_composition;
                            build.type = action.type;
                            build.opaque_allowed_as_pointee = action.opaque_allowed_as_pointee;
                            build.dyn_trait_principal_count = type.dyn_trait_principals.size();
                            actions.push_back(build);
                            for (base::usize index = type.dyn_trait_principals.size(); index > 0; --index) {
                                TypeResolveAction resolve_principal;
                                resolve_principal.kind = TypeResolveActionKind::resolve;
                                resolve_principal.type = type.dyn_trait_principals[index - 1].trait_type;
                                resolve_principal.opaque_allowed_as_pointee = false;
                                actions.push_back(resolve_principal);
                            }
                            break;
                        }
                        TypeResolveAction build;
                        build.kind = TypeResolveActionKind::build_dyn_trait;
                        build.type = action.type;
                        build.opaque_allowed_as_pointee = action.opaque_allowed_as_pointee;
                        build.dyn_trait_arg_count = type.type_args.size();
                        build.dyn_trait_associated_constraint_count = type.associated_type_constraints.size();
                        actions.push_back(build);
                        for (base::usize index = type.type_args.size(); index > 0; --index) {
                            TypeResolveAction resolve_arg;
                            resolve_arg.kind = TypeResolveActionKind::resolve;
                            resolve_arg.type = type.type_args[index - 1];
                            actions.push_back(resolve_arg);
                        }
                        for (base::usize index = type.associated_type_constraints.size(); index > 0; --index) {
                            TypeResolveAction resolve_value;
                            resolve_value.kind = TypeResolveActionKind::resolve;
                            resolve_value.type = type.associated_type_constraints[index - 1].value_type;
                            actions.push_back(resolve_value);
                        }
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
                const bool pointee_is_trait_object = is_valid(pointee)
                    && this->core_.state_.checked.types.get(pointee).kind == TypeKind::trait_object;
                if (!pointee_is_trait_object && !this->core_.is_valid_storage_type(pointee)) {
                    this->core_.report_general(
                        this->core_.ctx_.module.types[action.type.value].range, std::string(SEMA_REFERENCE_STORAGE));
                }
                const syntax::TypeNode& syntax_type = this->core_.ctx_.module.types[action.type.value];
                const std::span<const std::string_view> origin_names = syntax_type.reference_origin.explicit_
                    ? std::span<const std::string_view>(
                          syntax_type.reference_origin.names.data(), syntax_type.reference_origin.names.size())
                    : std::span<const std::string_view>{};
                const TypeHandle resolved = this->core_.state_.checked.types.reference(
                    map_mutability(action.pointer_mutability), pointee, origin_names);
                if (syntax_type.reference_origin.explicit_) {
                    ReferenceOriginFact fact;
                    fact.module = this->core_.state_.flow.current_module;
                    fact.item = this->core_.state_.flow.current_item;
                    fact.syntax_type = action.type;
                    fact.semantic_type = resolved;
                    fact.origin_name_ids = syntax_type.reference_origin.name_ids;
                    fact.origin_names.reserve(syntax_type.reference_origin.names.size());
                    for (const std::string_view origin : syntax_type.reference_origin.names) {
                        fact.origin_names.push_back(this->core_.state_.checked.intern_text(origin));
                    }
                    fact.range = syntax_type.reference_origin.range;
                    fact.part_index = syntax::is_valid(this->core_.state_.flow.current_item)
                        ? this->core_.item_part_index(this->core_.state_.flow.current_item)
                        : 0U;
                    this->core_.state_.checked.reference_origin_facts.push_back(std::move(fact));
                }
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
            case TypeResolveActionKind::build_dyn_trait: {
                std::vector<TypeHandle> trait_args(action.dyn_trait_arg_count, INVALID_TYPE_HANDLE);
                for (base::usize index = action.dyn_trait_arg_count; index > 0; --index) {
                    if (values.empty()) {
                        break;
                    }
                    trait_args[index - 1] = values.back();
                    values.pop_back();
                }
                std::vector<TypeHandle> associated_values(
                    action.dyn_trait_associated_constraint_count, INVALID_TYPE_HANDLE);
                for (base::usize index = action.dyn_trait_associated_constraint_count; index > 0; --index) {
                    if (values.empty()) {
                        break;
                    }
                    associated_values[index - 1] = values.back();
                    values.pop_back();
                }
                const syntax::TypeNode& syntax_type = this->core_.ctx_.module.types[action.type.value];
                const TypeHandle resolved =
                    this->resolve_dyn_trait_type(action.type, syntax_type, trait_args, associated_values);
                values.push_back(resolved);
                break;
            }
            case TypeResolveActionKind::build_dyn_trait_composition: {
                std::vector<TypeHandle> principal_types(action.dyn_trait_principal_count, INVALID_TYPE_HANDLE);
                for (base::usize index = action.dyn_trait_principal_count; index > 0; --index) {
                    if (values.empty()) {
                        break;
                    }
                    principal_types[index - 1] = values.back();
                    values.pop_back();
                }
                const syntax::TypeNode& syntax_type = this->core_.ctx_.module.types[action.type.value];
                const TypeHandle resolved =
                    this->resolve_dyn_trait_composition_type(action.type, syntax_type, principal_types);
                values.push_back(resolved);
                break;
            }
        }
    }

    const TypeHandle resolved = values.back();
    this->core_.record_syntax_type_handle(type_id, resolved);
    return resolved;
}

TypeHandle SemanticTypeResolver::resolve_dyn_trait_type(const syntax::TypeId type_id,
    const syntax::TypeNode& type,
    const std::span<const TypeHandle> resolved_args,
    const std::span<const TypeHandle> associated_value_types)
{
    const bool qualified = !type.scope_parts.empty();
    const TraitSignature* trait = nullptr;
    if (qualified) {
        const syntax::ModuleId module = this->core_.resolve_type_scope(type, true);
        trait = this->core_.find_trait_in_module(module, type.name_id, type.name, type.range, true);
    } else {
        trait = this->core_.find_trait_in_visible_modules(type.name_id, type.name, type.range, true);
    }
    if (trait == nullptr) {
        return INVALID_TYPE_HANDLE;
    }

    if (resolved_args.empty() && !trait->generic_params.empty()) {
        this->core_.report_type(type.range,
            sema_generic_argument_count_message("trait type arguments", trait->name, 0, trait->generic_params.size()));
        return INVALID_TYPE_HANDLE;
    }
    if (!resolved_args.empty() && trait->generic_params.empty()) {
        this->core_.report_type(type.range, sema_trait_not_generic_message(trait->name));
        return INVALID_TYPE_HANDLE;
    }
    if (resolved_args.size() != trait->generic_params.size()) {
        this->core_.report_type(type.range,
            sema_generic_argument_count_message(
                "trait type arguments", trait->name, resolved_args.size(), trait->generic_params.size()));
        return INVALID_TYPE_HANDLE;
    }
    if (associated_value_types.size() != type.associated_type_constraints.size()) {
        this->core_.report_internal_contract(type.range, "dyn trait associated equality value count mismatch");
        return INVALID_TYPE_HANDLE;
    }

    std::unordered_map<IdentId, base::SourceRange, IdentIdHash> seen_equalities;
    seen_equalities.reserve(type.associated_type_constraints.size());
    std::vector<TraitImplAssociatedTypeInfo> associated_equalities;
    associated_equalities.reserve(type.associated_type_constraints.size());
    std::vector<TraitObjectAssociatedTypeEquality> object_equalities;
    object_equalities.reserve(type.associated_type_constraints.size());
    std::vector<query::TraitObjectAssociatedTypeEqualityKey> equality_keys;
    equality_keys.reserve(type.associated_type_constraints.size());
    bool ok = true;
    for (base::usize index = 0; index < type.associated_type_constraints.size(); ++index) {
        const syntax::AssociatedTypeConstraintDecl& constraint = type.associated_type_constraints[index];
        const auto inserted = seen_equalities.emplace(constraint.name_id, constraint.name_range);
        if (!inserted.second) {
            this->core_.report_type(
                constraint.range, sema_duplicate_associated_type_constraint_message(trait->name, constraint.name));
            this->core_.report_note(inserted.first->second, SemanticDiagnosticKind::duplicate,
                sema_previous_declaration_note_message(constraint.name));
            ok = false;
            continue;
        }
        const auto requirement = std::ranges::find_if(trait->associated_types,
            [&](const TraitAssociatedTypeRequirement& candidate) { return candidate.name_id == constraint.name_id; });
        if (requirement == trait->associated_types.end()) {
            this->core_.report_type(
                constraint.range, sema_unknown_associated_type_constraint_message(trait->name, constraint.name));
            ok = false;
            continue;
        }
        TraitImplAssociatedTypeInfo equality = this->core_.state_.checked.make_trait_impl_associated_type_info();
        equality.name = this->core_.source_name_text(constraint.name_id, constraint.name);
        equality.name_id = constraint.name_id;
        equality.syntax_type = constraint.value_type;
        equality.value_type = associated_value_types[index];
        equality.member_key = requirement->member_key;
        equality.requirement_ordinal = requirement->ordinal;
        associated_equalities.push_back(equality);

        TraitObjectAssociatedTypeEquality object_equality;
        object_equality.associated_member = requirement->member_key;
        object_equality.name = this->core_.source_name_text(constraint.name_id, constraint.name);
        object_equality.value_type = associated_value_types[index];
        object_equalities.push_back(object_equality);

        base::Result<query::CanonicalTypeKey> canonical_value =
            this->core_.checked_canonical_type_key(associated_value_types[index]);
        if (!canonical_value) {
            this->core_.report_internal_contract(constraint.range, canonical_value.error().message);
            ok = false;
            continue;
        }
        equality_keys.push_back(query::TraitObjectAssociatedTypeEqualityKey{
            requirement->member_key,
            canonical_value.take_value(),
        });
    }

    for (const TraitAssociatedTypeRequirement& requirement : trait->associated_types) {
        if (!seen_equalities.contains(requirement.name_id)) {
            this->core_.report_type(
                type.range, sema_dyn_trait_missing_associated_type_message(trait->name, requirement.name));
            ok = false;
        }
    }
    if (!ok) {
        return INVALID_TYPE_HANDLE;
    }

    std::vector<query::CanonicalTypeKey> canonical_args;
    canonical_args.reserve(resolved_args.size());
    for (const TypeHandle arg : resolved_args) {
        base::Result<query::CanonicalTypeKey> canonical_arg = this->core_.checked_canonical_type_key(arg);
        if (!canonical_arg) {
            this->core_.report_internal_contract(type.range, canonical_arg.error().message);
            return INVALID_TYPE_HANDLE;
        }
        canonical_args.push_back(canonical_arg.take_value());
    }

    const query::StableFingerprint128 slot_schema =
        this->core_.trait_object_slot_schema(*trait, resolved_args, associated_equalities);
    query::StableKeyWriter origin_writer;
    origin_writer.write_string("dyn-trait-object-origin:v1");
    query::append_stable_key(origin_writer, this->core_.trait_query_key(*trait));
    origin_writer.write_u64(static_cast<base::u64>(canonical_args.size()));
    for (const query::CanonicalTypeKey& arg : canonical_args) {
        query::append_stable_key(origin_writer, arg);
    }
    std::ranges::sort(equality_keys, [](const query::TraitObjectAssociatedTypeEqualityKey& lhs,
                                         const query::TraitObjectAssociatedTypeEqualityKey& rhs) {
        if (lhs.associated_type.global_id != rhs.associated_type.global_id) {
            return lhs.associated_type.global_id < rhs.associated_type.global_id;
        }
        return query::stable_serialize(lhs.value_type) < query::stable_serialize(rhs.value_type);
    });
    origin_writer.write_u64(static_cast<base::u64>(equality_keys.size()));
    for (const query::TraitObjectAssociatedTypeEqualityKey& equality : equality_keys) {
        query::append_stable_key(origin_writer, equality.associated_type);
        query::append_stable_key(origin_writer, equality.value_type);
    }
    origin_writer.write_fingerprint(slot_schema);

    const query::TraitObjectTypeKey key = query::trait_object_type_key(this->core_.trait_query_key(*trait),
        canonical_args, equality_keys, origin_writer.fingerprint(), slot_schema);
    const TypeHandle resolved = this->core_.state_.checked.types.trait_object(
        key, trait->name, trait->module, trait->name_id, resolved_args, object_equalities);
    if (!is_valid(resolved)) {
        this->core_.report_internal_contract(type.range, "failed to create dyn trait object type");
        return INVALID_TYPE_HANDLE;
    }
    this->core_.record_trait_object_callability(resolved, *trait, resolved_args, associated_equalities, type.range);
    this->core_.record_syntax_type_handle(type_id, resolved);
    return resolved;
}

TypeHandle SemanticTypeResolver::resolve_dyn_trait_composition_type(const syntax::TypeId type_id,
    const syntax::TypeNode& type,
    const std::span<const TypeHandle> principal_types)
{
    if (principal_types.size() < SEMA_DYN_TRAIT_COMPOSITION_MIN_PRINCIPALS) {
        this->core_.report_type(type.range, sema_dyn_trait_composition_min_principals_message());
        return INVALID_TYPE_HANDLE;
    }

    bool ok = true;
    for (const TypeHandle principal : principal_types) {
        if (!is_valid(principal) || principal.value >= this->core_.state_.checked.types.size()
            || this->core_.state_.checked.types.get(principal).kind != TypeKind::trait_object
            || !query::is_valid(this->core_.state_.checked.types.get(principal).trait_object_key)) {
            this->core_.report_type(type.range, sema_dyn_trait_composition_principal_message());
            ok = false;
        }
    }
    if (!ok) {
        return INVALID_TYPE_HANDLE;
    }

    std::vector<TypeHandle> sorted_principals =
        canonical_principal_types(this->core_.state_.checked.types, principal_types);
    for (base::usize index = 1; index < sorted_principals.size(); ++index) {
        const TypeInfo& previous = this->core_.state_.checked.types.get(sorted_principals[index - 1]);
        const TypeInfo& current = this->core_.state_.checked.types.get(sorted_principals[index]);
        if (previous.trait_object_key.principal_trait.global_id
            == current.trait_object_key.principal_trait.global_id) {
            this->core_.report_type(
                type.range, sema_dyn_trait_composition_duplicate_principal_message(current.trait_object_name));
            ok = false;
        }
    }
    if (!ok) {
        return INVALID_TYPE_HANDLE;
    }

    std::vector<query::PrincipalSetPrincipalDescriptor> descriptors =
        principal_descriptors(this->core_.state_.checked.types, sorted_principals);
    query::PrincipalSetIdentityFact identity = query::principal_set_identity_fact(descriptors);
    if (!query::is_valid(identity)) {
        this->core_.report_internal_contract(type.range, "failed to create dyn trait principal-set identity fact");
        return INVALID_TYPE_HANDLE;
    }

    const TypeHandle resolved = this->core_.state_.checked.types.principal_set_trait_object(
        identity.principal_set_identity, sorted_principals);
    if (!is_valid(resolved)) {
        this->core_.report_internal_contract(type.range, "failed to create dyn trait principal-set type");
        return INVALID_TYPE_HANDLE;
    }

    if (!std::ranges::any_of(this->core_.state_.checked.principal_set_composition_facts.identity_facts,
            [&](const query::PrincipalSetIdentityFact& fact) {
                return fact.principal_set_identity == identity.principal_set_identity;
            })) {
        query::record_principal_set_identity_fact(
            this->core_.state_.checked.principal_set_composition_facts, std::move(identity));
    }

    query::PrincipalMethodNamespaceFact method_namespace;
    method_namespace.principal_set_identity =
        this->core_.state_.checked.types.get(resolved).trait_object_principal_set_identity;
    method_namespace.methods = principal_method_entries(this->core_.state_.checked, sorted_principals);
    if (!method_namespace.methods.empty()
        && !std::ranges::any_of(this->core_.state_.checked.principal_set_composition_facts.method_namespaces,
            [&](const query::PrincipalMethodNamespaceFact& fact) {
                return fact.principal_set_identity == method_namespace.principal_set_identity;
            })) {
        query::record_principal_method_namespace_fact(
            this->core_.state_.checked.principal_set_composition_facts, std::move(method_namespace));
    }

    std::vector<query::AssociatedEqualityMergeFact> merges = associated_equality_merge_facts(this->core_,
        this->core_.state_.checked.types.get(resolved).trait_object_principal_set_identity, sorted_principals,
        type.range);
    for (query::AssociatedEqualityMergeFact& merge : merges) {
        if (!std::ranges::any_of(this->core_.state_.checked.principal_set_composition_facts.associated_equality_merges,
                [&](const query::AssociatedEqualityMergeFact& fact) {
                    return fact.principal_set_identity == merge.principal_set_identity
                        && fact.associated_type_name == merge.associated_type_name;
                })) {
            if (merge.status == query::PrincipalAssociatedEqualityMergeStatus::conflict) {
                this->core_.report_type(
                    type.range, sema_dyn_trait_composition_associated_conflict_message(merge.associated_type_name));
                ok = false;
            }
            query::record_associated_equality_merge_fact(
                this->core_.state_.checked.principal_set_composition_facts, std::move(merge));
        }
    }

    this->core_.state_.checked.principal_set_composition_facts.subject = "checked dyn trait principal-set composition";
    this->core_.state_.checked.principal_set_composition_facts.fingerprint =
        query::principal_set_composition_facts_fingerprint(
            this->core_.state_.checked.principal_set_composition_facts);
    this->core_.record_syntax_type_handle(type_id, resolved);
    return ok ? resolved : INVALID_TYPE_HANDLE;
}

TypeHandle SemanticTypeResolver::resolve_named_type(
    const syntax::TypeId type_id, const syntax::TypeNode& type, const bool opaque_allowed_as_pointee)
{
    const std::vector<std::string_view> scope_parts = this->core_.type_scope_parts(type);
    if (scope_parts.size() == 1 && this->core_.state_.flow.current_generic_context != nullptr) {
        const IdentId scope_name_id = type.scope_part_ids.empty()
            ? this->core_.ctx_.module.find_identifier(scope_parts.front())
            : type.scope_part_ids.front();
        if (const auto found = this->core_.state_.flow.current_generic_context->params.find(scope_name_id);
            found != this->core_.state_.flow.current_generic_context->params.end()) {
            if (!type.type_args.empty()) {
                this->core_.report_type(type.range, sema_generic_param_type_args_message(scope_parts.front()));
                return INVALID_TYPE_HANDLE;
            }
            return this->core_.resolve_associated_type_projection(found->second, type.name_id, type.name, type.range);
        }
    }
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
    const syntax::ItemId previous_item = this->core_.state_.flow.current_item;
    this->core_.state_.flow.current_module = alias.module;
    this->core_.state_.flow.current_item = alias.item;
    const TypeHandle resolved = this->resolve_type(alias.target, opaque_allowed_as_pointee);
    this->core_.state_.flow.current_module = previous_module;
    this->core_.state_.flow.current_item = previous_item;
    this->core_.state_.types.resolving_type_aliases.pop_back();
    this->core_.state_.types.resolved_type_aliases[key] = resolved;
    return resolved;
}

SemanticTypeValidator::SemanticTypeValidator(const SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

bool SemanticTypeValidator::can_assign(
    const TypeHandle dst, const TypeHandle src, const syntax::ExprId value) const
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
        if (this->core_.can_borrowed_dyn_trait_coerce(dst, src)) {
            return true;
        }
        if (this->core_.can_borrowed_dyn_trait_upcast(dst, src)) {
            return true;
        }
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
        if (info.kind == TypeKind::generic_param || info.kind == TypeKind::associated_projection) {
            return true;
        }
        if (this->core_.state_.checked.types.is_void(current) || info.kind == TypeKind::opaque_struct) {
            return false;
        }
        if (info.kind == TypeKind::trait_object) {
            return false;
        }
        if (info.kind == TypeKind::slice) {
            pending.push_back(info.slice_element);
            continue;
        }
        if (info.kind == TypeKind::reference) {
            if (is_valid(info.pointee)
                && this->core_.state_.checked.types.get(info.pointee).kind == TypeKind::trait_object) {
                continue;
            }
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
            || info.kind == TypeKind::slice || info.kind == TypeKind::function || info.kind == TypeKind::generic_param
            || info.kind == TypeKind::associated_projection) {
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
            || dependency_info.kind == TypeKind::associated_projection
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
            || info.kind == TypeKind::slice || info.kind == TypeKind::function || info.kind == TypeKind::generic_param
            || info.kind == TypeKind::associated_projection) {
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
    const SemanticTargetLayout& target = this->core_.ctx_.options.target_layout;
    const auto builtin_layout = [&target](const BuiltinType builtin) noexcept -> SemanticAnalyzerCore::TypeAbiLayout {
        switch (builtin) {
            case BuiltinType::void_:
                return SemanticAnalyzerCore::TypeAbiLayout{SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
            case BuiltinType::bool_:
                return SemanticAnalyzerCore::TypeAbiLayout{target.bool_size, target.bool_align};
            case BuiltinType::i8:
            case BuiltinType::u8:
                return SemanticAnalyzerCore::TypeAbiLayout{target.i8_size, target.i8_align};
            case BuiltinType::i16:
            case BuiltinType::u16:
                return SemanticAnalyzerCore::TypeAbiLayout{target.i16_size, target.i16_align};
            case BuiltinType::i32:
            case BuiltinType::u32:
                return SemanticAnalyzerCore::TypeAbiLayout{target.i32_size, target.i32_align};
            case BuiltinType::i64:
            case BuiltinType::u64:
                return SemanticAnalyzerCore::TypeAbiLayout{target.i64_size, target.i64_align};
            case BuiltinType::isize:
            case BuiltinType::usize:
                return SemanticAnalyzerCore::TypeAbiLayout{target.pointer_size, target.pointer_align};
            case BuiltinType::f32:
                return SemanticAnalyzerCore::TypeAbiLayout{target.f32_size, target.f32_align};
            case BuiltinType::f64:
                return SemanticAnalyzerCore::TypeAbiLayout{target.f64_size, target.f64_align};
            case BuiltinType::str:
                return SemanticAnalyzerCore::TypeAbiLayout{
                    target.pointer_size + target.pointer_size, target.pointer_align};
            case BuiltinType::char_:
                return SemanticAnalyzerCore::TypeAbiLayout{target.char_size, target.char_align};
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
                return SemanticAnalyzerCore::TypeAbiLayout{target.pointer_size, target.pointer_align};
            case TypeKind::function:
                return SemanticAnalyzerCore::TypeAbiLayout{target.pointer_size, target.pointer_align};
            case TypeKind::slice:
                return SemanticAnalyzerCore::TypeAbiLayout{
                    target.pointer_size + target.pointer_size, target.pointer_align};
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
            case TypeKind::associated_projection:
            case TypeKind::trait_object:
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
                || info.kind == TypeKind::generic_param || info.kind == TypeKind::associated_projection
                || info.kind == TypeKind::opaque_struct || info.kind == TypeKind::trait_object) {
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
            case TypeKind::associated_projection:
            case TypeKind::trait_object:
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
            || info.kind == TypeKind::associated_projection || info.kind == TypeKind::opaque_struct) {
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
