#include "aurex/sema/type.hpp"

#include <cassert>
#include <utility>

namespace aurex::sema {

namespace {

inline constexpr base::u32 builtin_type_count = 15;

[[nodiscard]] bool builtin_is_integer(const BuiltinType type) noexcept {
    switch (type) {
    case BuiltinType::i8:
    case BuiltinType::u8:
    case BuiltinType::i16:
    case BuiltinType::u16:
    case BuiltinType::i32:
    case BuiltinType::u32:
    case BuiltinType::i64:
    case BuiltinType::u64:
    case BuiltinType::isize:
    case BuiltinType::usize:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool builtin_is_float(const BuiltinType type) noexcept {
    return type == BuiltinType::f32 || type == BuiltinType::f64;
}

[[nodiscard]] bool builtin_is_copyable(const BuiltinType type) noexcept {
    return type != BuiltinType::void_;
}

[[nodiscard]] std::string builtin_display_name(const BuiltinType type) {
    switch (type) {
    case BuiltinType::void_: return "void";
    case BuiltinType::bool_: return "bool";
    case BuiltinType::i8: return "i8";
    case BuiltinType::u8: return "u8";
    case BuiltinType::i16: return "i16";
    case BuiltinType::u16: return "u16";
    case BuiltinType::i32: return "i32";
    case BuiltinType::u32: return "u32";
    case BuiltinType::i64: return "i64";
    case BuiltinType::u64: return "u64";
    case BuiltinType::isize: return "isize";
    case BuiltinType::usize: return "usize";
    case BuiltinType::f32: return "f32";
    case BuiltinType::f64: return "f64";
    case BuiltinType::str: return "str";
    }
    return "<unknown>";
}

} // namespace

TypeTable::TypeTable() {
    types_.reserve(128);
    for (base::u32 i = 0; i < builtin_type_count; ++i) {
        TypeInfo info;
        info.kind = TypeKind::builtin;
        info.builtin = static_cast<BuiltinType>(i);
        info.is_copyable = builtin_is_copyable(info.builtin);
        types_.push_back(std::move(info));
    }
}

TypeHandle TypeTable::builtin(BuiltinType type) const noexcept {
    return TypeHandle {static_cast<base::u32>(type)};
}

TypeHandle TypeTable::pointer(const PointerMutability mutability, const TypeHandle pointee) {
    for (base::u32 i = 0; i < types_.size(); ++i) {
        const TypeInfo& info = types_[i];
        if (info.kind == TypeKind::pointer &&
            info.pointer_mutability == mutability &&
            same(info.pointee, pointee)) {
            return TypeHandle {i};
        }
    }

    TypeInfo info;
    info.kind = TypeKind::pointer;
    info.pointer_mutability = mutability;
    info.pointee = pointee;
    info.is_copyable = true;
    return push(std::move(info));
}

TypeHandle TypeTable::array(const base::u64 count, const TypeHandle element) {
    for (base::u32 i = 0; i < types_.size(); ++i) {
        const TypeInfo& info = types_[i];
        if (info.kind == TypeKind::array &&
            info.array_count == count &&
            same(info.array_element, element)) {
            return TypeHandle {i};
        }
    }

    TypeInfo info;
    info.kind = TypeKind::array;
    info.array_count = count;
    info.array_element = element;
    info.contains_array = true;
    info.is_copyable = false;
    return push(std::move(info));
}

TypeHandle TypeTable::named_struct(std::string name, std::string c_name, const bool contains_array) {
    TypeInfo info;
    info.kind = TypeKind::struct_;
    info.name = std::move(name);
    info.c_name = std::move(c_name);
    info.contains_array = contains_array;
    info.is_copyable = !contains_array;
    return push(std::move(info));
}

TypeHandle TypeTable::named_enum(std::string name, std::string c_name) {
    TypeInfo info;
    info.kind = TypeKind::enum_;
    info.name = std::move(name);
    info.c_name = std::move(c_name);
    info.is_copyable = true;
    return push(std::move(info));
}

TypeHandle TypeTable::opaque_struct(std::string name, std::string c_name) {
    TypeInfo info;
    info.kind = TypeKind::opaque_struct;
    info.name = std::move(name);
    info.c_name = std::move(c_name);
    info.is_copyable = false;
    return push(std::move(info));
}

void TypeTable::set_record_properties(const TypeHandle handle, const bool contains_array, const bool is_copyable) noexcept {
    assert(handle.value < types_.size());
    types_[handle.value].contains_array = contains_array;
    types_[handle.value].is_copyable = is_copyable;
}

void TypeTable::set_enum_underlying(const TypeHandle handle, const TypeHandle underlying) noexcept {
    assert(handle.value < types_.size());
    types_[handle.value].enum_underlying = underlying;
}

void TypeTable::set_enum_payload_layout(
    const TypeHandle handle,
    const TypeHandle storage,
    const base::u64 payload_size,
    const base::u64 payload_align
) noexcept {
    assert(handle.value < types_.size());
    types_[handle.value].enum_payload_storage = storage;
    types_[handle.value].enum_payload_size = payload_size;
    types_[handle.value].enum_payload_align = payload_align;
}

bool TypeTable::same(const TypeHandle lhs, const TypeHandle rhs) const noexcept {
    return lhs.value == rhs.value;
}

bool TypeTable::is_integer(const TypeHandle type) const noexcept {
    if (!is_valid(type) || type.value >= types_.size()) {
        return false;
    }
    const TypeInfo& info = types_[type.value];
    return info.kind == TypeKind::builtin && builtin_is_integer(info.builtin);
}

bool TypeTable::is_float(const TypeHandle type) const noexcept {
    if (!is_valid(type) || type.value >= types_.size()) {
        return false;
    }
    const TypeInfo& info = types_[type.value];
    return info.kind == TypeKind::builtin && builtin_is_float(info.builtin);
}

bool TypeTable::is_bool(const TypeHandle type) const noexcept {
    return is_valid(type) &&
           type.value < types_.size() &&
           types_[type.value].kind == TypeKind::builtin &&
           types_[type.value].builtin == BuiltinType::bool_;
}

bool TypeTable::is_str(const TypeHandle type) const noexcept {
    return is_valid(type) &&
           type.value < types_.size() &&
           types_[type.value].kind == TypeKind::builtin &&
           types_[type.value].builtin == BuiltinType::str;
}

bool TypeTable::is_void(const TypeHandle type) const noexcept {
    return is_valid(type) &&
           type.value < types_.size() &&
           types_[type.value].kind == TypeKind::builtin &&
           types_[type.value].builtin == BuiltinType::void_;
}

bool TypeTable::is_pointer(const TypeHandle type) const noexcept {
    return is_valid(type) && type.value < types_.size() && types_[type.value].kind == TypeKind::pointer;
}

bool TypeTable::is_array(const TypeHandle type) const noexcept {
    return is_valid(type) && type.value < types_.size() && types_[type.value].kind == TypeKind::array;
}

bool TypeTable::is_copyable(const TypeHandle type) const noexcept {
    return is_valid(type) && type.value < types_.size() && types_[type.value].is_copyable;
}

bool TypeTable::contains_array(const TypeHandle type) const noexcept {
    return is_valid(type) && type.value < types_.size() && types_[type.value].contains_array;
}

std::string TypeTable::display_name(const TypeHandle type) const {
    if (!is_valid(type) || type.value >= types_.size()) {
        return "<invalid>";
    }
    const TypeInfo& info = types_[type.value];
    switch (info.kind) {
    case TypeKind::builtin:
        return builtin_display_name(info.builtin);
    case TypeKind::pointer:
        return std::string(info.pointer_mutability == PointerMutability::mut ? "*mut " : "*const ") + display_name(info.pointee);
    case TypeKind::array:
        return "[" + std::to_string(info.array_count) + "]" + display_name(info.array_element);
    case TypeKind::struct_:
    case TypeKind::enum_:
    case TypeKind::opaque_struct:
        return info.name;
    }
    return "<unknown>";
}

std::string TypeTable::c_name(const TypeHandle type) const {
    if (!is_valid(type) || type.value >= types_.size()) {
        return "void";
    }
    const TypeInfo& info = types_[type.value];
    if (info.c_name.empty()) {
        return display_name(type);
    }
    return info.c_name;
}

const TypeInfo& TypeTable::get(const TypeHandle handle) const noexcept {
    assert(handle.value < types_.size());
    return types_[handle.value];
}

TypeHandle TypeTable::push(TypeInfo info) {
    const TypeHandle handle {static_cast<base::u32>(types_.size())};
    types_.push_back(std::move(info));
    return handle;
}

} // namespace aurex::sema
