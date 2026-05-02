#pragma once

#include "aurex/base/integer.hpp"

#include <limits>
#include <string>
#include <vector>

namespace aurex::sema {

struct TypeHandle {
    base::u32 value = invalid_value;
    static constexpr base::u32 invalid_value = std::numeric_limits<base::u32>::max();
};

inline constexpr TypeHandle invalid_type_handle {TypeHandle::invalid_value};

[[nodiscard]] inline constexpr bool is_valid(const TypeHandle handle) noexcept {
    return handle.value != TypeHandle::invalid_value;
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
    struct_,
    enum_,
    opaque_struct,
};

enum class PointerMutability {
    mut,
    const_,
};

struct TypeInfo {
    TypeKind kind = TypeKind::builtin;
    BuiltinType builtin = BuiltinType::void_;
    PointerMutability pointer_mutability = PointerMutability::const_;
    TypeHandle pointee = invalid_type_handle;
    base::u64 array_count = 0;
    TypeHandle array_element = invalid_type_handle;
    TypeHandle enum_underlying = invalid_type_handle;
    std::string name;
    std::string c_name;
    bool contains_array = false;
    bool is_copyable = true;
};

class TypeTable final {
public:
    TypeTable();

    [[nodiscard]] TypeHandle builtin(BuiltinType type) const noexcept;
    [[nodiscard]] TypeHandle pointer(PointerMutability mutability, TypeHandle pointee);
    [[nodiscard]] TypeHandle array(base::u64 count, TypeHandle element);
    [[nodiscard]] TypeHandle named_struct(std::string name, std::string c_name, bool contains_array);
    [[nodiscard]] TypeHandle named_enum(std::string name, std::string c_name);
    [[nodiscard]] TypeHandle opaque_struct(std::string name, std::string c_name);

    void set_record_properties(TypeHandle handle, bool contains_array, bool is_copyable) noexcept;
    void set_enum_underlying(TypeHandle handle, TypeHandle underlying) noexcept;

    [[nodiscard]] bool same(TypeHandle lhs, TypeHandle rhs) const noexcept;
    [[nodiscard]] bool is_integer(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_float(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_bool(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_str(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_void(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_pointer(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_array(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_copyable(TypeHandle type) const noexcept;
    [[nodiscard]] bool contains_array(TypeHandle type) const noexcept;
    [[nodiscard]] std::string display_name(TypeHandle type) const;
    [[nodiscard]] std::string c_name(TypeHandle type) const;
    [[nodiscard]] const TypeInfo& get(TypeHandle handle) const noexcept;

private:
    [[nodiscard]] TypeHandle push(TypeInfo info);

    std::vector<TypeInfo> types_;
};

} // namespace aurex::sema
