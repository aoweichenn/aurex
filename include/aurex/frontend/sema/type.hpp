#pragma once

#include <aurex/frontend/sema/identifier.hpp>
#include <aurex/frontend/sema/storage.hpp>
#include <aurex/frontend/syntax/core/ast_ids.hpp>
#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/principal_set_composition_facts.hpp>
#include <aurex/infrastructure/query/query_key.hpp>
#include <aurex/infrastructure/query/trait_object_key.hpp>

#include <initializer_list>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::test {
struct TypeTableTestAccess;
} // namespace aurex::test

namespace aurex::sema {

struct TypeHandle {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = std::numeric_limits<base::u32>::max();
};

inline constexpr TypeHandle INVALID_TYPE_HANDLE{TypeHandle::INVALID_VALUE};

using TypeHandleList = SemaVector<TypeHandle>;

[[nodiscard]] inline constexpr bool is_valid(const TypeHandle handle) noexcept
{
    return handle.value != TypeHandle::INVALID_VALUE;
}

enum class BuiltinType {
    void_,
    bool_,
    i8,
    u8,
    i16,
    u16,
    i32,
    u32,
    i64,
    u64,
    isize,
    usize,
    f32,
    f64,
    str,
    char_,
};

enum class TypeKind {
    builtin,
    pointer,
    reference,
    array,
    slice,
    range,
    tuple,
    function,
    struct_,
    enum_,
    opaque_struct,
    generic_param,
    associated_projection,
    trait_object,
};

struct TraitObjectAssociatedTypeEquality {
    query::MemberKey associated_member;
    InternedText name;
    TypeHandle value_type = INVALID_TYPE_HANDLE;
};

using TraitObjectAssociatedTypeEqualityList = SemaVector<TraitObjectAssociatedTypeEquality>;

enum class PointerMutability {
    mut,
    const_,
};

enum class FunctionCallConv {
    aurex,
    c,
};

enum class ArrayLengthKind {
    literal,
    const_param,
};

struct ArrayLengthInfo {
    ArrayLengthKind kind = ArrayLengthKind::literal;
    base::u64 literal = 0;
    GenericParamIdentity const_param_identity = INVALID_GENERIC_PARAM_IDENTITY;
    InternedText const_param_name;
    TypeHandle const_param_type = INVALID_TYPE_HANDLE;
    query::StableFingerprint128 fingerprint;
};

[[nodiscard]] inline bool operator==(const ArrayLengthInfo& lhs, const ArrayLengthInfo& rhs) noexcept
{
    return lhs.kind == rhs.kind && lhs.literal == rhs.literal && lhs.const_param_identity == rhs.const_param_identity
        && lhs.const_param_type.value == rhs.const_param_type.value && lhs.fingerprint == rhs.fingerprint;
}

[[nodiscard]] inline bool operator!=(const ArrayLengthInfo& lhs, const ArrayLengthInfo& rhs) noexcept
{
    return !(lhs == rhs);
}

struct TypeInfo {
    TypeKind kind = TypeKind::builtin;
    BuiltinType builtin = BuiltinType::void_;
    PointerMutability pointer_mutability = PointerMutability::const_;
    TypeHandle pointee = INVALID_TYPE_HANDLE;
    InternedText reference_origin_key;
    base::u64 array_count = 0;
    ArrayLengthInfo array_length;
    TypeHandle array_element = INVALID_TYPE_HANDLE;
    PointerMutability slice_mutability = PointerMutability::const_;
    TypeHandle slice_element = INVALID_TYPE_HANDLE;
    TypeHandle range_element = INVALID_TYPE_HANDLE;
    TypeHandleList tuple_elements;
    FunctionCallConv function_call_conv = FunctionCallConv::aurex;
    bool function_is_unsafe = false;
    bool function_is_variadic = false;
    TypeHandleList function_params;
    TypeHandle function_return = INVALID_TYPE_HANDLE;
    TypeHandle enum_underlying = INVALID_TYPE_HANDLE;
    TypeHandle enum_payload_storage = INVALID_TYPE_HANDLE;
    base::u64 enum_payload_size = 0;
    base::u64 enum_payload_align = 1;
    GenericParamIdentity generic_identity = INVALID_GENERIC_PARAM_IDENTITY;
    TypeHandle associated_base = INVALID_TYPE_HANDLE;
    query::MemberKey associated_member;
    query::TraitObjectTypeKey trait_object_key;
    InternedText trait_object_name;
    syntax::ModuleId trait_object_module = syntax::INVALID_MODULE_ID;
    IdentId trait_object_name_id = INVALID_IDENT_ID;
    TypeHandleList trait_object_args;
    TraitObjectAssociatedTypeEqualityList trait_object_associated_equalities;
    query::StableFingerprint128 trait_object_principal_set_identity;
    TypeHandleList trait_object_principal_types;
    InternedText name;
    InternedText c_name;
    InternedText generic_origin_key;
    TypeHandleList generic_args;
    bool contains_array = false;
};

class TypeTable final {
    friend struct ::aurex::test::TypeTableTestAccess;

public:
    TypeTable();
    TypeTable(const TypeTable& other);
    TypeTable& operator=(const TypeTable& other);
    TypeTable(TypeTable&& other) noexcept;
    TypeTable& operator=(TypeTable&& other) noexcept;
    ~TypeTable() = default;

    [[nodiscard]] TypeHandle builtin(BuiltinType type) const noexcept;
    [[nodiscard]] TypeHandle pointer(PointerMutability mutability, TypeHandle pointee);
    [[nodiscard]] TypeHandle reference(PointerMutability mutability, TypeHandle pointee);
    [[nodiscard]] TypeHandle reference(
        PointerMutability mutability, TypeHandle pointee, std::span<const std::string_view> origin_names);
    [[nodiscard]] TypeHandle reference_with_origin_key(
        PointerMutability mutability, TypeHandle pointee, std::string_view origin_key);
    [[nodiscard]] TypeHandle array(base::u64 count, TypeHandle element);
    [[nodiscard]] TypeHandle array_with_length(ArrayLengthInfo length, TypeHandle element);
    [[nodiscard]] TypeHandle slice(PointerMutability mutability, TypeHandle element);
    [[nodiscard]] TypeHandle range(TypeHandle element);
    [[nodiscard]] TypeHandle tuple(std::span<const TypeHandle> elements);
    [[nodiscard]] TypeHandle tuple(const std::vector<TypeHandle>& elements);
    [[nodiscard]] TypeHandle tuple(std::initializer_list<TypeHandle> elements);
    [[nodiscard]] TypeHandle function(FunctionCallConv call_conv, bool is_unsafe, bool is_variadic,
        std::span<const TypeHandle> params, TypeHandle return_type);
    [[nodiscard]] TypeHandle function(FunctionCallConv call_conv, bool is_unsafe, bool is_variadic,
        const std::vector<TypeHandle>& params, TypeHandle return_type);
    [[nodiscard]] TypeHandle function(FunctionCallConv call_conv, bool is_unsafe, bool is_variadic,
        std::initializer_list<TypeHandle> params, TypeHandle return_type);
    [[nodiscard]] TypeHandle function(
        FunctionCallConv call_conv, bool is_variadic, std::span<const TypeHandle> params, TypeHandle return_type);
    [[nodiscard]] TypeHandle function(
        FunctionCallConv call_conv, bool is_variadic, const std::vector<TypeHandle>& params, TypeHandle return_type);
    [[nodiscard]] TypeHandle function(
        FunctionCallConv call_conv, bool is_variadic, std::initializer_list<TypeHandle> params, TypeHandle return_type);
    [[nodiscard]] TypeHandle named_struct(std::string_view name, std::string_view c_name, bool contains_array);
    [[nodiscard]] TypeHandle named_enum(std::string_view name, std::string_view c_name);
    [[nodiscard]] TypeHandle opaque_struct(std::string_view name, std::string_view c_name);
    [[nodiscard]] TypeHandle generic_param(std::string_view name);
    [[nodiscard]] TypeHandle generic_param(GenericParamIdentity identity, std::string_view display_name);
    [[nodiscard]] TypeHandle associated_projection(
        TypeHandle base, query::MemberKey associated_member, std::string_view associated_name);
    [[nodiscard]] TypeHandle trait_object(query::TraitObjectTypeKey key,
        std::string_view trait_name,
        syntax::ModuleId trait_module,
        IdentId trait_name_id,
        std::span<const TypeHandle> trait_args,
        std::span<const TraitObjectAssociatedTypeEquality> associated_equalities);
    [[nodiscard]] TypeHandle principal_set_trait_object(query::StableFingerprint128 identity,
        std::span<const TypeHandle> principal_types);
    [[nodiscard]] bool is_principal_set_trait_object(TypeHandle type) const noexcept;

    void set_record_contains_array(TypeHandle handle, bool contains_array) noexcept;
    void set_enum_underlying(TypeHandle handle, TypeHandle underlying) noexcept;
    void set_enum_payload_layout(
        TypeHandle handle, TypeHandle storage, base::u64 payload_size, base::u64 payload_align) noexcept;
    void set_generic_instance(TypeHandle handle, std::string_view origin_key, std::span<const TypeHandle> args);
    void set_generic_instance(TypeHandle handle, std::string_view origin_key, const std::vector<TypeHandle>& args);
    void set_generic_instance(TypeHandle handle, std::string_view origin_key, std::initializer_list<TypeHandle> args);

    [[nodiscard]] bool same(TypeHandle lhs, TypeHandle rhs) const noexcept;
    [[nodiscard]] bool is_integer(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_float(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_bool(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_str(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_char(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_void(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_pointer(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_reference(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_array(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_slice(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_range(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_tuple(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_function(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_trait_object(TypeHandle type) const noexcept;
    [[nodiscard]] bool contains_array(TypeHandle type) const noexcept;
    [[nodiscard]] std::string display_name(TypeHandle type) const;
    [[nodiscard]] std::string display_name(std::string_view base_name, std::span<const TypeHandle> generic_args) const;
    [[nodiscard]] std::string c_name(TypeHandle type) const;
    [[nodiscard]] const TypeInfo& get(TypeHandle handle) const noexcept;
    [[nodiscard]] base::usize size() const noexcept;

private:
    struct PointerKey {
        base::u32 pointee = TypeHandle::INVALID_VALUE;
        PointerMutability mutability = PointerMutability::const_;

        [[nodiscard]] bool operator==(const PointerKey& other) const noexcept
        {
            return pointee == other.pointee && mutability == other.mutability;
        }
    };

    struct ArrayKey {
        ArrayLengthInfo length;
        base::u32 element = TypeHandle::INVALID_VALUE;

        [[nodiscard]] bool operator==(const ArrayKey& other) const noexcept
        {
            return length == other.length && element == other.element;
        }
    };

    struct ReferenceKey {
        base::u32 pointee = TypeHandle::INVALID_VALUE;
        PointerMutability mutability = PointerMutability::const_;
        std::string origin_key;

        [[nodiscard]] bool operator==(const ReferenceKey& other) const noexcept
        {
            return pointee == other.pointee && mutability == other.mutability && origin_key == other.origin_key;
        }
    };

    struct SliceKey {
        base::u32 element = TypeHandle::INVALID_VALUE;
        PointerMutability mutability = PointerMutability::const_;

        [[nodiscard]] bool operator==(const SliceKey& other) const noexcept
        {
            return element == other.element && mutability == other.mutability;
        }
    };

    struct RangeKey {
        base::u32 element = TypeHandle::INVALID_VALUE;

        [[nodiscard]] bool operator==(const RangeKey& other) const noexcept
        {
            return element == other.element;
        }
    };

    struct FunctionKey {
        FunctionCallConv call_conv = FunctionCallConv::aurex;
        bool is_unsafe = false;
        bool is_variadic = false;
        SemaVector<base::u32> params;
        base::u32 return_type = TypeHandle::INVALID_VALUE;

        [[nodiscard]] bool operator==(const FunctionKey& other) const noexcept
        {
            return call_conv == other.call_conv && is_unsafe == other.is_unsafe && is_variadic == other.is_variadic
                && params == other.params && return_type == other.return_type;
        }
    };

    struct TupleKey {
        SemaVector<base::u32> elements;

        [[nodiscard]] bool operator==(const TupleKey& other) const noexcept
        {
            return elements == other.elements;
        }
    };

    struct AssociatedProjectionKey {
        base::u32 base = TypeHandle::INVALID_VALUE;
        base::u64 member = 0;

        [[nodiscard]] bool operator==(const AssociatedProjectionKey& other) const noexcept
        {
            return base == other.base && member == other.member;
        }
    };

    struct TraitObjectKey {
        base::u64 global_id = 0;
        query::StableFingerprint128 principal_set_identity;

        [[nodiscard]] bool operator==(const TraitObjectKey& other) const noexcept
        {
            return global_id == other.global_id && principal_set_identity == other.principal_set_identity;
        }
    };

    struct PointerKeyHash {
        [[nodiscard]] std::size_t operator()(const PointerKey& key) const noexcept;
    };

    struct ReferenceKeyHash {
        [[nodiscard]] std::size_t operator()(const ReferenceKey& key) const noexcept;
    };

    struct ArrayKeyHash {
        [[nodiscard]] std::size_t operator()(const ArrayKey& key) const noexcept;
    };

    struct SliceKeyHash {
        [[nodiscard]] std::size_t operator()(const SliceKey& key) const noexcept;
    };

    struct RangeKeyHash {
        [[nodiscard]] std::size_t operator()(const RangeKey& key) const noexcept;
    };

    struct FunctionKeyHash {
        [[nodiscard]] std::size_t operator()(const FunctionKey& key) const noexcept;
    };

    struct TupleKeyHash {
        [[nodiscard]] std::size_t operator()(const TupleKey& key) const noexcept;
    };

    struct AssociatedProjectionKeyHash {
        [[nodiscard]] std::size_t operator()(const AssociatedProjectionKey& key) const noexcept;
    };

    struct TraitObjectKeyHash {
        [[nodiscard]] std::size_t operator()(const TraitObjectKey& key) const noexcept;
    };

    void initialize_builtins();
    void swap(TypeTable& other) noexcept;
    void copy_from(const TypeTable& other);
    void rebind_interned_texts() noexcept;
    [[nodiscard]] TypeHandleList make_type_handle_list() const;
    [[nodiscard]] TypeHandleList copy_type_handles(std::span<const TypeHandle> values) const;
    [[nodiscard]] SemaVector<base::u32> make_type_key_list() const;
    [[nodiscard]] SemaVector<base::u32> copy_type_key_values(std::span<const TypeHandle> values) const;
    [[nodiscard]] SemaVector<base::u32> copy_u32_values(const SemaVector<base::u32>& values) const;
    [[nodiscard]] TypeInfo make_type_info() const;
    [[nodiscard]] InternedText intern_text(std::string_view text);
    [[nodiscard]] TypeInfo clone_type_info(const TypeInfo& other);
    [[nodiscard]] TraitObjectAssociatedTypeEqualityList copy_trait_object_associated_equalities(
        std::span<const TraitObjectAssociatedTypeEquality> values);
    [[nodiscard]] FunctionKey clone_function_key(const FunctionKey& other) const;
    [[nodiscard]] TupleKey clone_tuple_key(const TupleKey& other) const;
    [[nodiscard]] TypeHandle push(TypeInfo info);

    std::unique_ptr<base::BumpAllocator> arena_;
    SemaVector<TypeInfo> types_;
    SemaMap<PointerKey, TypeHandle, PointerKeyHash> pointer_types_;
    SemaMap<ReferenceKey, TypeHandle, ReferenceKeyHash> reference_types_;
    SemaMap<ArrayKey, TypeHandle, ArrayKeyHash> array_types_;
    SemaMap<SliceKey, TypeHandle, SliceKeyHash> slice_types_;
    SemaMap<RangeKey, TypeHandle, RangeKeyHash> range_types_;
    SemaMap<TupleKey, TypeHandle, TupleKeyHash> tuple_types_;
    SemaMap<FunctionKey, TypeHandle, FunctionKeyHash> function_types_;
    IdentifierInterner texts_;
    SemaMap<GenericParamIdentity, TypeHandle, GenericParamIdentityHash> generic_param_types_;
    SemaMap<AssociatedProjectionKey, TypeHandle, AssociatedProjectionKeyHash> associated_projection_types_;
    SemaMap<TraitObjectKey, TypeHandle, TraitObjectKeyHash> trait_object_types_;
};

} // namespace aurex::sema
