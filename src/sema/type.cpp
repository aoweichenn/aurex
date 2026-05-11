#include <aurex/sema/type.hpp>

#include <cassert>
#include <cstddef>
#include <string_view>
#include <utility>

namespace aurex::sema {

namespace {

inline constexpr base::u32 SEMA_BUILTIN_TYPE_COUNT = static_cast<base::u32>(BuiltinType::str) + 1U;
constexpr base::usize SEMA_TYPE_TABLE_INITIAL_CAPACITY = 128;
constexpr std::string_view SEMA_TYPE_DISPLAY_INVALID_NAME = "<invalid>";
constexpr std::string_view SEMA_TYPE_DISPLAY_UNKNOWN_NAME = "<unknown>";
constexpr std::string_view SEMA_TYPE_DISPLAY_POINTER_MUT_PREFIX = "*mut ";
constexpr std::string_view SEMA_TYPE_DISPLAY_POINTER_CONST_PREFIX = "*const ";
constexpr std::string_view SEMA_TYPE_DISPLAY_ARRAY_OPEN = "[";
constexpr std::string_view SEMA_TYPE_DISPLAY_ARRAY_CLOSE = "]";

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
    this->types_.reserve(SEMA_TYPE_TABLE_INITIAL_CAPACITY);
    for (base::u32 i = 0; i < SEMA_BUILTIN_TYPE_COUNT; ++i) {
        TypeInfo info;
        info.kind = TypeKind::builtin;
        info.builtin = static_cast<BuiltinType>(i);
        this->types_.push_back(std::move(info));
    }
}

TypeHandle TypeTable::builtin(BuiltinType type) const noexcept {
    return TypeHandle {static_cast<base::u32>(type)};
}

TypeHandle TypeTable::pointer(const PointerMutability mutability, const TypeHandle pointee) {
    const PointerKey key {pointee.value, mutability};
    if (const auto found = this->pointer_types_.find(key); found != this->pointer_types_.end()) {
        return found->second;
    }

    TypeInfo info;
    info.kind = TypeKind::pointer;
    info.pointer_mutability = mutability;
    info.pointee = pointee;
    const TypeHandle handle = this->push(std::move(info));
    this->pointer_types_.emplace(key, handle);
    return handle;
}

TypeHandle TypeTable::array(const base::u64 count, const TypeHandle element) {
    const ArrayKey key {count, element.value};
    if (const auto found = this->array_types_.find(key); found != this->array_types_.end()) {
        return found->second;
    }

    TypeInfo info;
    info.kind = TypeKind::array;
    info.array_count = count;
    info.array_element = element;
    info.contains_array = true;
    const TypeHandle handle = this->push(std::move(info));
    this->array_types_.emplace(key, handle);
    return handle;
}

TypeHandle TypeTable::named_struct(std::string name, std::string c_name, const bool contains_array) {
    TypeInfo info;
    info.kind = TypeKind::struct_;
    info.name = std::move(name);
    info.c_name = std::move(c_name);
    info.contains_array = contains_array;
    return this->push(std::move(info));
}

TypeHandle TypeTable::named_enum(std::string name, std::string c_name) {
    TypeInfo info;
    info.kind = TypeKind::enum_;
    info.name = std::move(name);
    info.c_name = std::move(c_name);
    return this->push(std::move(info));
}

TypeHandle TypeTable::opaque_struct(std::string name, std::string c_name) {
    TypeInfo info;
    info.kind = TypeKind::opaque_struct;
    info.name = std::move(name);
    info.c_name = std::move(c_name);
    return this->push(std::move(info));
}

TypeHandle TypeTable::generic_param(std::string name) {
    if (const auto found = this->generic_param_types_.find(name); found != this->generic_param_types_.end()) {
        return found->second;
    }

    TypeInfo info;
    info.kind = TypeKind::generic_param;
    info.name = name;
    const TypeHandle handle = this->push(std::move(info));
    this->generic_param_types_.emplace(std::move(name), handle);
    return handle;
}

void TypeTable::set_record_contains_array(const TypeHandle handle, const bool contains_array) noexcept {
    assert(handle.value < this->types_.size());
    this->types_[handle.value].contains_array = contains_array;
}

void TypeTable::set_enum_underlying(const TypeHandle handle, const TypeHandle underlying) noexcept {
    assert(handle.value < this->types_.size());
    this->types_[handle.value].enum_underlying = underlying;
}

void TypeTable::set_enum_payload_layout(
    const TypeHandle handle,
    const TypeHandle storage,
    const base::u64 payload_size,
    const base::u64 payload_align
) noexcept {
    assert(handle.value < this->types_.size());
    this->types_[handle.value].enum_payload_storage = storage;
    this->types_[handle.value].enum_payload_size = payload_size;
    this->types_[handle.value].enum_payload_align = payload_align;
}

void TypeTable::set_generic_instance(
    const TypeHandle handle,
    std::string origin_key,
    std::vector<TypeHandle> args
) {
    assert(handle.value < this->types_.size());
    this->types_[handle.value].generic_origin_key = std::move(origin_key);
    this->types_[handle.value].generic_args = std::move(args);
}

bool TypeTable::same(const TypeHandle lhs, const TypeHandle rhs) const noexcept {
    return lhs.value == rhs.value;
}

bool TypeTable::is_integer(const TypeHandle type) const noexcept {
    if (!is_valid(type) || type.value >= this->types_.size()) {
        return false;
    }
    const TypeInfo& info = this->types_[type.value];
    return info.kind == TypeKind::builtin && builtin_is_integer(info.builtin);
}

bool TypeTable::is_float(const TypeHandle type) const noexcept {
    if (!is_valid(type) || type.value >= this->types_.size()) {
        return false;
    }
    const TypeInfo& info = this->types_[type.value];
    return info.kind == TypeKind::builtin && builtin_is_float(info.builtin);
}

bool TypeTable::is_bool(const TypeHandle type) const noexcept {
    return is_valid(type) &&
           type.value < this->types_.size() &&
           this->types_[type.value].kind == TypeKind::builtin &&
           this->types_[type.value].builtin == BuiltinType::bool_;
}

bool TypeTable::is_str(const TypeHandle type) const noexcept {
    return is_valid(type) &&
           type.value < this->types_.size() &&
           this->types_[type.value].kind == TypeKind::builtin &&
           this->types_[type.value].builtin == BuiltinType::str;
}

bool TypeTable::is_void(const TypeHandle type) const noexcept {
    return is_valid(type) &&
           type.value < this->types_.size() &&
           this->types_[type.value].kind == TypeKind::builtin &&
           this->types_[type.value].builtin == BuiltinType::void_;
}

bool TypeTable::is_pointer(const TypeHandle type) const noexcept {
    return is_valid(type) && type.value < this->types_.size() && this->types_[type.value].kind == TypeKind::pointer;
}

bool TypeTable::is_array(const TypeHandle type) const noexcept {
    return is_valid(type) && type.value < this->types_.size() && this->types_[type.value].kind == TypeKind::array;
}

bool TypeTable::contains_array(const TypeHandle type) const noexcept {
    return is_valid(type) && type.value < this->types_.size() && this->types_[type.value].contains_array;
}

std::string TypeTable::display_name(const TypeHandle type) const {
    std::string name;
    TypeHandle current = type;
    while (true) {
        if (!is_valid(current) || current.value >= this->types_.size()) {
            name.append(SEMA_TYPE_DISPLAY_INVALID_NAME);
            return name;
        }
        const TypeInfo& info = this->types_[current.value];
        switch (info.kind) {
        case TypeKind::builtin:
            name += builtin_display_name(info.builtin);
            return name;
        case TypeKind::pointer:
            name.append(info.pointer_mutability == PointerMutability::mut
                ? SEMA_TYPE_DISPLAY_POINTER_MUT_PREFIX
                : SEMA_TYPE_DISPLAY_POINTER_CONST_PREFIX);
            current = info.pointee;
            break;
        case TypeKind::array:
            name.append(SEMA_TYPE_DISPLAY_ARRAY_OPEN);
            name += std::to_string(info.array_count);
            name.append(SEMA_TYPE_DISPLAY_ARRAY_CLOSE);
            current = info.array_element;
            break;
        case TypeKind::struct_:
        case TypeKind::enum_:
        case TypeKind::opaque_struct:
        case TypeKind::generic_param:
            name += info.name;
            return name;
        default:
            name.append(SEMA_TYPE_DISPLAY_UNKNOWN_NAME);
            return name;
        }
    }
}

std::string TypeTable::c_name(const TypeHandle type) const {
    if (!is_valid(type) || type.value >= this->types_.size()) {
        return "void";
    }
    const TypeInfo& info = this->types_[type.value];
    if (info.c_name.empty()) {
        return this->display_name(type);
    }
    return info.c_name;
}

const TypeInfo& TypeTable::get(const TypeHandle handle) const noexcept {
    assert(handle.value < this->types_.size());
    return this->types_[handle.value];
}

TypeHandle TypeTable::push(TypeInfo info) {
    const TypeHandle handle {static_cast<base::u32>(this->types_.size())};
    this->types_.push_back(std::move(info));
    return handle;
}

std::size_t TypeTable::PointerKeyHash::operator()(const PointerKey& key) const noexcept {
    return (static_cast<std::size_t>(key.pointee) << 1) ^
           static_cast<std::size_t>(key.mutability == PointerMutability::mut ? 1U : 0U);
}

std::size_t TypeTable::ArrayKeyHash::operator()(const ArrayKey& key) const noexcept {
    return static_cast<std::size_t>(key.element) ^
           (static_cast<std::size_t>(key.count) * static_cast<std::size_t>(1099511628211ULL));
}

} // namespace aurex::sema
