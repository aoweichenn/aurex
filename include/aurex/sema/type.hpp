#pragma once

#include <aurex/base/integer.hpp>
#include <aurex/sema/identifier.hpp>
#include <aurex/sema/storage.hpp>

#include <initializer_list>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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
    tuple,
    function,
    struct_,
    enum_,
    opaque_struct,
    generic_param,
};

enum class PointerMutability {
    mut,
    const_,
};

enum class FunctionCallConv {
    aurex,
    c,
};

struct TypeInfo {
    TypeKind kind = TypeKind::builtin;
    BuiltinType builtin = BuiltinType::void_;
    PointerMutability pointer_mutability = PointerMutability::const_;
    TypeHandle pointee = INVALID_TYPE_HANDLE;
    base::u64 array_count = 0;
    TypeHandle array_element = INVALID_TYPE_HANDLE;
    PointerMutability slice_mutability = PointerMutability::const_;
    TypeHandle slice_element = INVALID_TYPE_HANDLE;
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
    InternedText name;
    InternedText c_name;
    InternedText generic_origin_key;
    TypeHandleList generic_args;
    bool contains_array = false;
};

class TypeTable final {
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
    [[nodiscard]] TypeHandle array(base::u64 count, TypeHandle element);
    [[nodiscard]] TypeHandle slice(PointerMutability mutability, TypeHandle element);
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
    [[nodiscard]] bool is_tuple(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_function(TypeHandle type) const noexcept;
    [[nodiscard]] bool contains_array(TypeHandle type) const noexcept;
    [[nodiscard]] std::string display_name(TypeHandle type) const;
    [[nodiscard]] std::string display_name(std::string_view base_name, std::span<const TypeHandle> generic_args) const;
    [[nodiscard]] std::string c_name(TypeHandle type) const;
    [[nodiscard]] const TypeInfo& get(TypeHandle handle) const noexcept;
    [[nodiscard]] base::usize size() const noexcept;

#if defined(AUREX_SEMA_WHITEBOX_TESTS)
public:
#else
private:
#endif
    struct PointerKey {
        base::u32 pointee = TypeHandle::INVALID_VALUE;
        PointerMutability mutability = PointerMutability::const_;

        [[nodiscard]] bool operator==(const PointerKey& other) const noexcept
        {
            return pointee == other.pointee && mutability == other.mutability;
        }
    };

    struct ArrayKey {
        base::u64 count = 0;
        base::u32 element = TypeHandle::INVALID_VALUE;

        [[nodiscard]] bool operator==(const ArrayKey& other) const noexcept
        {
            return count == other.count && element == other.element;
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

    struct PointerKeyHash {
        [[nodiscard]] std::size_t operator()(const PointerKey& key) const noexcept;
    };

    struct ArrayKeyHash {
        [[nodiscard]] std::size_t operator()(const ArrayKey& key) const noexcept;
    };

    struct SliceKeyHash {
        [[nodiscard]] std::size_t operator()(const SliceKey& key) const noexcept;
    };

    struct FunctionKeyHash {
        [[nodiscard]] std::size_t operator()(const FunctionKey& key) const noexcept;
    };

    struct TupleKeyHash {
        [[nodiscard]] std::size_t operator()(const TupleKey& key) const noexcept;
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
    [[nodiscard]] FunctionKey clone_function_key(const FunctionKey& other) const;
    [[nodiscard]] TupleKey clone_tuple_key(const TupleKey& other) const;
    [[nodiscard]] TypeHandle push(TypeInfo info);

    std::unique_ptr<base::BumpAllocator> arena_;
    SemaVector<TypeInfo> types_;
    SemaMap<PointerKey, TypeHandle, PointerKeyHash> pointer_types_;
    SemaMap<PointerKey, TypeHandle, PointerKeyHash> reference_types_;
    SemaMap<ArrayKey, TypeHandle, ArrayKeyHash> array_types_;
    SemaMap<SliceKey, TypeHandle, SliceKeyHash> slice_types_;
    SemaMap<TupleKey, TypeHandle, TupleKeyHash> tuple_types_;
    SemaMap<FunctionKey, TypeHandle, FunctionKeyHash> function_types_;
    IdentifierInterner texts_;
    SemaMap<GenericParamIdentity, TypeHandle, GenericParamIdentityHash> generic_param_types_;
};

} // namespace aurex::sema
