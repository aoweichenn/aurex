#pragma once

#include <aurex/base/integer.hpp>

#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace aurex::sema {

struct TypeHandle {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = std::numeric_limits<base::u32>::max();
};

inline constexpr TypeHandle INVALID_TYPE_HANDLE {TypeHandle::INVALID_VALUE};

[[nodiscard]] inline constexpr bool is_valid(const TypeHandle handle) noexcept {
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
};

enum class TypeKind {
    builtin,
    pointer,
    array,
    slice,
    struct_,
    enum_,
    opaque_struct,
    generic_param,
};

enum class PointerMutability {
    mut,
    const_,
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
    TypeHandle enum_underlying = INVALID_TYPE_HANDLE;
    TypeHandle enum_payload_storage = INVALID_TYPE_HANDLE;
    base::u64 enum_payload_size = 0;
    base::u64 enum_payload_align = 1;
    std::string name;
    std::string c_name;
    std::string generic_origin_key;
    std::vector<TypeHandle> generic_args;
    bool contains_array = false;
};

class TypeTable final {
public:
    TypeTable();

    [[nodiscard]] TypeHandle builtin(BuiltinType type) const noexcept;
    [[nodiscard]] TypeHandle pointer(PointerMutability mutability, TypeHandle pointee);
    [[nodiscard]] TypeHandle array(base::u64 count, TypeHandle element);
    [[nodiscard]] TypeHandle slice(PointerMutability mutability, TypeHandle element);
    [[nodiscard]] TypeHandle named_struct(std::string name, std::string c_name, bool contains_array);
    [[nodiscard]] TypeHandle named_enum(std::string name, std::string c_name);
    [[nodiscard]] TypeHandle opaque_struct(std::string name, std::string c_name);
    [[nodiscard]] TypeHandle generic_param(std::string name);

    void set_record_contains_array(TypeHandle handle, bool contains_array) noexcept;
    void set_enum_underlying(TypeHandle handle, TypeHandle underlying) noexcept;
    void set_enum_payload_layout(TypeHandle handle, TypeHandle storage, base::u64 payload_size, base::u64 payload_align) noexcept;
    void set_generic_instance(TypeHandle handle, std::string origin_key, std::vector<TypeHandle> args);

    [[nodiscard]] bool same(TypeHandle lhs, TypeHandle rhs) const noexcept;
    [[nodiscard]] bool is_integer(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_float(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_bool(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_str(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_void(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_pointer(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_array(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_slice(TypeHandle type) const noexcept;
    [[nodiscard]] bool contains_array(TypeHandle type) const noexcept;
    [[nodiscard]] std::string display_name(TypeHandle type) const;
    [[nodiscard]] std::string c_name(TypeHandle type) const;
    [[nodiscard]] const TypeInfo& get(TypeHandle handle) const noexcept;

private:
    struct PointerKey {
        base::u32 pointee = TypeHandle::INVALID_VALUE;
        PointerMutability mutability = PointerMutability::const_;

        [[nodiscard]] bool operator==(const PointerKey& other) const noexcept {
            return pointee == other.pointee && mutability == other.mutability;
        }
    };

    struct ArrayKey {
        base::u64 count = 0;
        base::u32 element = TypeHandle::INVALID_VALUE;

        [[nodiscard]] bool operator==(const ArrayKey& other) const noexcept {
            return count == other.count && element == other.element;
        }
    };

    struct SliceKey {
        base::u32 element = TypeHandle::INVALID_VALUE;
        PointerMutability mutability = PointerMutability::const_;

        [[nodiscard]] bool operator==(const SliceKey& other) const noexcept {
            return element == other.element && mutability == other.mutability;
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

    [[nodiscard]] TypeHandle push(TypeInfo info);

    std::vector<TypeInfo> types_;
    std::unordered_map<PointerKey, TypeHandle, PointerKeyHash> pointer_types_;
    std::unordered_map<ArrayKey, TypeHandle, ArrayKeyHash> array_types_;
    std::unordered_map<SliceKey, TypeHandle, SliceKeyHash> slice_types_;
    std::unordered_map<std::string, TypeHandle> generic_param_types_;
};

} // namespace aurex::sema
