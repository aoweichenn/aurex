#include <aurex/sema/type.hpp>

#include <cassert>
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

namespace aurex::sema {

namespace {

inline constexpr base::u32 SEMA_BUILTIN_TYPE_COUNT = static_cast<base::u32>(BuiltinType::char_) + 1U;
constexpr base::usize SEMA_TYPE_TABLE_INITIAL_CAPACITY = 128;
constexpr std::string_view SEMA_TYPE_DISPLAY_INVALID_NAME = "<invalid>";
constexpr std::string_view SEMA_TYPE_DISPLAY_UNKNOWN_NAME = "<unknown>";
constexpr std::string_view SEMA_TYPE_DISPLAY_POINTER_MUT_PREFIX = "*mut ";
constexpr std::string_view SEMA_TYPE_DISPLAY_POINTER_CONST_PREFIX = "*const ";
constexpr std::string_view SEMA_TYPE_DISPLAY_REFERENCE_PREFIX = "&";
constexpr std::string_view SEMA_TYPE_DISPLAY_REFERENCE_MUT_PREFIX = "&mut ";
constexpr std::string_view SEMA_TYPE_DISPLAY_ARRAY_OPEN = "[";
constexpr std::string_view SEMA_TYPE_DISPLAY_ARRAY_CLOSE = "]";
constexpr std::string_view SEMA_TYPE_DISPLAY_SLICE_MUT_PREFIX = "[]mut ";
constexpr std::string_view SEMA_TYPE_DISPLAY_SLICE_CONST_PREFIX = "[]const ";
constexpr std::string_view SEMA_TYPE_DISPLAY_TUPLE_OPEN = "(";
constexpr std::string_view SEMA_TYPE_DISPLAY_TUPLE_CLOSE = ")";
constexpr std::string_view SEMA_TYPE_DISPLAY_TUPLE_SEPARATOR = ", ";
constexpr std::string_view SEMA_TYPE_DISPLAY_SINGLE_TUPLE_TRAILING = ",";
constexpr std::string_view SEMA_TYPE_DISPLAY_FN_PREFIX = "fn(";
constexpr std::string_view SEMA_TYPE_DISPLAY_EXTERN_C_FN_PREFIX = "extern c fn(";
constexpr std::string_view SEMA_TYPE_DISPLAY_UNSAFE_FN_PREFIX = "unsafe fn(";
constexpr std::string_view SEMA_TYPE_DISPLAY_UNSAFE_EXTERN_C_FN_PREFIX = "unsafe extern c fn(";
constexpr std::string_view SEMA_TYPE_DISPLAY_FN_VARIADIC = "...";
constexpr std::string_view SEMA_TYPE_DISPLAY_FN_RETURN = ") -> ";
constexpr std::string_view SEMA_TYPE_DISPLAY_GENERIC_ARG_LIST_OPEN = "[";
constexpr std::string_view SEMA_TYPE_DISPLAY_GENERIC_ARG_LIST_CLOSE = "]";
constexpr std::string_view SEMA_TYPE_DISPLAY_GENERIC_ARG_LIST_SEPARATOR = ",";
constexpr base::usize SEMA_TYPE_DISPLAY_GENERIC_ARG_SIZE_ESTIMATE = 16;
constexpr std::size_t SEMA_TYPE_HASH_MULTIPLIER = 1099511628211ULL;

enum class TypeDisplayTaskKind {
    type,
    text,
};

struct TypeDisplayTask {
    TypeDisplayTaskKind kind = TypeDisplayTaskKind::type;
    TypeHandle type = INVALID_TYPE_HANDLE;
    std::string text;
};

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
    case BuiltinType::char_: return "char";
    }
    return "<unknown>";
}

void push_generic_arg_display_tasks(std::vector<TypeDisplayTask>& pending, const std::span<const TypeHandle> args) {
    if (args.empty()) {
        return;
    }
    pending.push_back(TypeDisplayTask {
        TypeDisplayTaskKind::text,
        INVALID_TYPE_HANDLE,
        std::string(SEMA_TYPE_DISPLAY_GENERIC_ARG_LIST_CLOSE),
    });
    for (base::usize index = args.size(); index > 0; --index) {
        if (index < args.size()) {
            pending.push_back(TypeDisplayTask {
                TypeDisplayTaskKind::text,
                INVALID_TYPE_HANDLE,
                std::string(SEMA_TYPE_DISPLAY_GENERIC_ARG_LIST_SEPARATOR),
            });
        }
        pending.push_back(TypeDisplayTask {
            TypeDisplayTaskKind::type,
            args[index - 1],
            {},
        });
    }
}

} // namespace

TypeTable::TypeTable()
    : arena_(std::make_unique<base::BumpAllocator>()),
      types_(make_sema_vector<TypeInfo>(*this->arena_)),
      pointer_types_(make_sema_map<PointerKey, TypeHandle, PointerKeyHash>(*this->arena_, PointerKeyHash {})),
      reference_types_(make_sema_map<PointerKey, TypeHandle, PointerKeyHash>(*this->arena_, PointerKeyHash {})),
      array_types_(make_sema_map<ArrayKey, TypeHandle, ArrayKeyHash>(*this->arena_, ArrayKeyHash {})),
      slice_types_(make_sema_map<SliceKey, TypeHandle, SliceKeyHash>(*this->arena_, SliceKeyHash {})),
      tuple_types_(make_sema_map<TupleKey, TypeHandle, TupleKeyHash>(*this->arena_, TupleKeyHash {})),
      function_types_(make_sema_map<FunctionKey, TypeHandle, FunctionKeyHash>(*this->arena_, FunctionKeyHash {})),
      generic_param_types_(make_sema_map<IdentId, TypeHandle, IdentIdHash>(*this->arena_, IdentIdHash {})) {
    this->initialize_builtins();
}

TypeTable::TypeTable(const TypeTable& other)
    : arena_(std::make_unique<base::BumpAllocator>()),
      types_(make_sema_vector<TypeInfo>(*this->arena_)),
      pointer_types_(make_sema_map<PointerKey, TypeHandle, PointerKeyHash>(*this->arena_, PointerKeyHash {})),
      reference_types_(make_sema_map<PointerKey, TypeHandle, PointerKeyHash>(*this->arena_, PointerKeyHash {})),
      array_types_(make_sema_map<ArrayKey, TypeHandle, ArrayKeyHash>(*this->arena_, ArrayKeyHash {})),
      slice_types_(make_sema_map<SliceKey, TypeHandle, SliceKeyHash>(*this->arena_, SliceKeyHash {})),
      tuple_types_(make_sema_map<TupleKey, TypeHandle, TupleKeyHash>(*this->arena_, TupleKeyHash {})),
      function_types_(make_sema_map<FunctionKey, TypeHandle, FunctionKeyHash>(*this->arena_, FunctionKeyHash {})),
      generic_param_types_(make_sema_map<IdentId, TypeHandle, IdentIdHash>(*this->arena_, IdentIdHash {})) {
    this->copy_from(other);
}

TypeTable& TypeTable::operator=(const TypeTable& other) {
    if (this == &other) {
        return *this;
    }
    TypeTable copy(other);
    *this = std::move(copy);
    return *this;
}

TypeTable::TypeTable(TypeTable&& other) noexcept
    : arena_(std::move(other.arena_)),
      types_(std::move(other.types_)),
      pointer_types_(std::move(other.pointer_types_)),
      reference_types_(std::move(other.reference_types_)),
      array_types_(std::move(other.array_types_)),
      slice_types_(std::move(other.slice_types_)),
      tuple_types_(std::move(other.tuple_types_)),
      function_types_(std::move(other.function_types_)),
      texts_(std::move(other.texts_)),
      generic_param_types_(std::move(other.generic_param_types_)) {}

TypeTable& TypeTable::operator=(TypeTable&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

void TypeTable::initialize_builtins() {
    this->types_.reserve(SEMA_TYPE_TABLE_INITIAL_CAPACITY);
    for (base::u32 i = 0; i < SEMA_BUILTIN_TYPE_COUNT; ++i) {
        TypeInfo info = this->make_type_info();
        info.kind = TypeKind::builtin;
        info.builtin = static_cast<BuiltinType>(i);
        this->types_.push_back(std::move(info));
    }
}

void TypeTable::swap(TypeTable& other) noexcept {
    using std::swap;
    this->types_.swap(other.types_);
    this->pointer_types_.swap(other.pointer_types_);
    this->reference_types_.swap(other.reference_types_);
    this->array_types_.swap(other.array_types_);
    this->slice_types_.swap(other.slice_types_);
    this->tuple_types_.swap(other.tuple_types_);
    this->function_types_.swap(other.function_types_);
    swap(this->texts_, other.texts_);
    this->generic_param_types_.swap(other.generic_param_types_);
    swap(this->arena_, other.arena_);
}

void TypeTable::copy_from(const TypeTable& other) {
    this->types_.clear();
    this->types_.reserve(other.types_.size());
    for (const TypeInfo& info : other.types_) {
        this->types_.push_back(this->clone_type_info(info));
    }
    this->pointer_types_ = other.pointer_types_;
    this->reference_types_ = other.reference_types_;
    this->array_types_ = other.array_types_;
    this->slice_types_ = other.slice_types_;
    this->tuple_types_.clear();
    this->tuple_types_.reserve(other.tuple_types_.size());
    for (const auto& entry : other.tuple_types_) {
        this->tuple_types_.emplace(this->clone_tuple_key(entry.first), entry.second);
    }
    this->function_types_.clear();
    this->function_types_.reserve(other.function_types_.size());
    for (const auto& entry : other.function_types_) {
        this->function_types_.emplace(this->clone_function_key(entry.first), entry.second);
    }
    this->generic_param_types_.clear();
    for (base::u32 index = 0; index < this->types_.size(); ++index) {
        const TypeInfo& info = this->types_[index];
        if (info.kind == TypeKind::generic_param && is_valid(info.generic_identity_key.id)) {
            this->generic_param_types_.emplace(info.generic_identity_key.id, TypeHandle {index});
        }
    }
}

TypeHandleList TypeTable::make_type_handle_list() const {
    return make_sema_vector<TypeHandle>(*this->arena_);
}

TypeHandleList TypeTable::copy_type_handles(const std::span<const TypeHandle> values) const {
    TypeHandleList copy = this->make_type_handle_list();
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

SemaVector<base::u32> TypeTable::make_type_key_list() const {
    return make_sema_vector<base::u32>(*this->arena_);
}

SemaVector<base::u32> TypeTable::copy_type_key_values(const std::span<const TypeHandle> values) const {
    SemaVector<base::u32> copy = this->make_type_key_list();
    copy.reserve(values.size());
    for (const TypeHandle value : values) {
        copy.push_back(value.value);
    }
    return copy;
}

SemaVector<base::u32> TypeTable::copy_u32_values(const SemaVector<base::u32>& values) const {
    SemaVector<base::u32> copy = this->make_type_key_list();
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

TypeInfo TypeTable::make_type_info() const {
    TypeInfo info;
    info.tuple_elements = this->make_type_handle_list();
    info.function_params = this->make_type_handle_list();
    info.generic_args = this->make_type_handle_list();
    return info;
}

InternedText TypeTable::intern_text(const std::string_view text) {
    return sema::intern_text(this->texts_, text);
}

TypeInfo TypeTable::clone_type_info(const TypeInfo& other) {
    TypeInfo copy = this->make_type_info();
    copy.kind = other.kind;
    copy.builtin = other.builtin;
    copy.pointer_mutability = other.pointer_mutability;
    copy.pointee = other.pointee;
    copy.array_count = other.array_count;
    copy.array_element = other.array_element;
    copy.slice_mutability = other.slice_mutability;
    copy.slice_element = other.slice_element;
    copy.tuple_elements = this->copy_type_handles(other.tuple_elements);
    copy.function_call_conv = other.function_call_conv;
    copy.function_is_unsafe = other.function_is_unsafe;
    copy.function_is_variadic = other.function_is_variadic;
    copy.function_params = this->copy_type_handles(other.function_params);
    copy.function_return = other.function_return;
    copy.enum_underlying = other.enum_underlying;
    copy.enum_payload_storage = other.enum_payload_storage;
    copy.enum_payload_size = other.enum_payload_size;
    copy.enum_payload_align = other.enum_payload_align;
    copy.name = this->intern_text(other.name);
    copy.c_name = this->intern_text(other.c_name);
    copy.generic_identity_key = this->intern_text(other.generic_identity_key);
    copy.generic_origin_key = this->intern_text(other.generic_origin_key);
    copy.generic_args = this->copy_type_handles(other.generic_args);
    copy.contains_array = other.contains_array;
    return copy;
}

TypeTable::FunctionKey TypeTable::clone_function_key(const FunctionKey& other) const {
    FunctionKey copy;
    copy.call_conv = other.call_conv;
    copy.is_unsafe = other.is_unsafe;
    copy.is_variadic = other.is_variadic;
    copy.params = this->copy_u32_values(other.params);
    copy.return_type = other.return_type;
    return copy;
}

TypeTable::TupleKey TypeTable::clone_tuple_key(const TupleKey& other) const {
    TupleKey copy;
    copy.elements = this->copy_u32_values(other.elements);
    return copy;
}

TypeHandle TypeTable::builtin(BuiltinType type) const noexcept {
    return TypeHandle {static_cast<base::u32>(type)};
}

TypeHandle TypeTable::pointer(const PointerMutability mutability, const TypeHandle pointee) {
    const PointerKey key {pointee.value, mutability};
    if (const auto found = this->pointer_types_.find(key); found != this->pointer_types_.end()) {
        return found->second;
    }

    TypeInfo info = this->make_type_info();
    info.kind = TypeKind::pointer;
    info.pointer_mutability = mutability;
    info.pointee = pointee;
    const TypeHandle handle = this->push(std::move(info));
    this->pointer_types_.emplace(key, handle);
    return handle;
}

TypeHandle TypeTable::reference(const PointerMutability mutability, const TypeHandle pointee) {
    const PointerKey key {pointee.value, mutability};
    if (const auto found = this->reference_types_.find(key); found != this->reference_types_.end()) {
        return found->second;
    }

    TypeInfo info = this->make_type_info();
    info.kind = TypeKind::reference;
    info.pointer_mutability = mutability;
    info.pointee = pointee;
    const TypeHandle handle = this->push(std::move(info));
    this->reference_types_.emplace(key, handle);
    return handle;
}

TypeHandle TypeTable::array(const base::u64 count, const TypeHandle element) {
    const ArrayKey key {count, element.value};
    if (const auto found = this->array_types_.find(key); found != this->array_types_.end()) {
        return found->second;
    }

    TypeInfo info = this->make_type_info();
    info.kind = TypeKind::array;
    info.array_count = count;
    info.array_element = element;
    info.contains_array = true;
    const TypeHandle handle = this->push(std::move(info));
    this->array_types_.emplace(key, handle);
    return handle;
}

TypeHandle TypeTable::slice(const PointerMutability mutability, const TypeHandle element) {
    const SliceKey key {element.value, mutability};
    if (const auto found = this->slice_types_.find(key); found != this->slice_types_.end()) {
        return found->second;
    }

    TypeInfo info = this->make_type_info();
    info.kind = TypeKind::slice;
    info.slice_mutability = mutability;
    info.slice_element = element;
    const TypeHandle handle = this->push(std::move(info));
    this->slice_types_.emplace(key, handle);
    return handle;
}

TypeHandle TypeTable::tuple(const std::span<const TypeHandle> elements) {
    TupleKey key;
    key.elements = this->copy_type_key_values(elements);
    if (const auto found = this->tuple_types_.find(key); found != this->tuple_types_.end()) {
        return found->second;
    }

    bool contains_array = false;
    for (const TypeHandle element : elements) {
        contains_array = contains_array || this->contains_array(element);
    }

    TypeInfo info = this->make_type_info();
    info.kind = TypeKind::tuple;
    info.tuple_elements = this->copy_type_handles(elements);
    info.contains_array = contains_array;
    const TypeHandle handle = this->push(std::move(info));
    this->tuple_types_.emplace(std::move(key), handle);
    return handle;
}

TypeHandle TypeTable::tuple(const std::vector<TypeHandle>& elements) {
    return this->tuple(std::span<const TypeHandle>(elements.data(), elements.size()));
}

TypeHandle TypeTable::tuple(const std::initializer_list<TypeHandle> elements) {
    return this->tuple(std::span<const TypeHandle>(elements.begin(), elements.size()));
}

TypeHandle TypeTable::function(
    const FunctionCallConv call_conv,
    const bool is_unsafe,
    const bool is_variadic,
    const std::span<const TypeHandle> params,
    const TypeHandle return_type
) {
    FunctionKey key;
    key.call_conv = call_conv;
    key.is_unsafe = is_unsafe;
    key.is_variadic = is_variadic;
    key.params = this->copy_type_key_values(params);
    key.return_type = return_type.value;
    if (const auto found = this->function_types_.find(key); found != this->function_types_.end()) {
        return found->second;
    }

    TypeInfo info = this->make_type_info();
    info.kind = TypeKind::function;
    info.function_call_conv = call_conv;
    info.function_is_unsafe = is_unsafe;
    info.function_is_variadic = is_variadic;
    info.function_params = this->copy_type_handles(params);
    info.function_return = return_type;
    const TypeHandle handle = this->push(std::move(info));
    this->function_types_.emplace(std::move(key), handle);
    return handle;
}

TypeHandle TypeTable::function(
    const FunctionCallConv call_conv,
    const bool is_unsafe,
    const bool is_variadic,
    const std::vector<TypeHandle>& params,
    const TypeHandle return_type
) {
    return this->function(
        call_conv,
        is_unsafe,
        is_variadic,
        std::span<const TypeHandle>(params.data(), params.size()),
        return_type
    );
}

TypeHandle TypeTable::function(
    const FunctionCallConv call_conv,
    const bool is_unsafe,
    const bool is_variadic,
    const std::initializer_list<TypeHandle> params,
    const TypeHandle return_type
) {
    return this->function(
        call_conv,
        is_unsafe,
        is_variadic,
        std::span<const TypeHandle>(params.begin(), params.size()),
        return_type
    );
}

TypeHandle TypeTable::function(
    const FunctionCallConv call_conv,
    const bool is_variadic,
    const std::span<const TypeHandle> params,
    const TypeHandle return_type
) {
    return this->function(call_conv, false, is_variadic, params, return_type);
}

TypeHandle TypeTable::function(
    const FunctionCallConv call_conv,
    const bool is_variadic,
    const std::vector<TypeHandle>& params,
    const TypeHandle return_type
) {
    return this->function(
        call_conv,
        false,
        is_variadic,
        std::span<const TypeHandle>(params.data(), params.size()),
        return_type
    );
}

TypeHandle TypeTable::function(
    const FunctionCallConv call_conv,
    const bool is_variadic,
    const std::initializer_list<TypeHandle> params,
    const TypeHandle return_type
) {
    return this->function(
        call_conv,
        false,
        is_variadic,
        std::span<const TypeHandle>(params.begin(), params.size()),
        return_type
    );
}

TypeHandle TypeTable::named_struct(const std::string_view name, const std::string_view c_name, const bool contains_array) {
    TypeInfo info = this->make_type_info();
    info.kind = TypeKind::struct_;
    info.name = this->intern_text(name);
    info.c_name = this->intern_text(c_name);
    info.contains_array = contains_array;
    return this->push(std::move(info));
}

TypeHandle TypeTable::named_enum(const std::string_view name, const std::string_view c_name) {
    TypeInfo info = this->make_type_info();
    info.kind = TypeKind::enum_;
    info.name = this->intern_text(name);
    info.c_name = this->intern_text(c_name);
    return this->push(std::move(info));
}

TypeHandle TypeTable::opaque_struct(const std::string_view name, const std::string_view c_name) {
    TypeInfo info = this->make_type_info();
    info.kind = TypeKind::opaque_struct;
    info.name = this->intern_text(name);
    info.c_name = this->intern_text(c_name);
    return this->push(std::move(info));
}

TypeHandle TypeTable::generic_param(const std::string_view name) {
    return this->generic_param(name, name);
}

TypeHandle TypeTable::generic_param(const std::string_view identity_key, const std::string_view display_name) {
    const InternedText identity = this->intern_text(identity_key);
    const IdentId identity_id = identity.id;
    if (const auto found = this->generic_param_types_.find(identity_id); found != this->generic_param_types_.end()) {
        return found->second;
    }

    TypeInfo info = this->make_type_info();
    info.kind = TypeKind::generic_param;
    info.name = this->intern_text(display_name);
    info.generic_identity_key = identity;
    const TypeHandle handle = this->push(std::move(info));
    this->generic_param_types_.emplace(identity_id, handle);
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
    const std::string_view origin_key,
    const std::span<const TypeHandle> args
) {
    assert(handle.value < this->types_.size());
    this->types_[handle.value].generic_origin_key = this->intern_text(origin_key);
    this->types_[handle.value].generic_args = this->copy_type_handles(args);
}

void TypeTable::set_generic_instance(
    const TypeHandle handle,
    const std::string_view origin_key,
    const std::vector<TypeHandle>& args
) {
    this->set_generic_instance(
        handle,
        origin_key,
        std::span<const TypeHandle>(args.data(), args.size())
    );
}

void TypeTable::set_generic_instance(
    const TypeHandle handle,
    const std::string_view origin_key,
    const std::initializer_list<TypeHandle> args
) {
    this->set_generic_instance(
        handle,
        origin_key,
        std::span<const TypeHandle>(args.begin(), args.size())
    );
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

bool TypeTable::is_char(const TypeHandle type) const noexcept {
    return is_valid(type) &&
           type.value < this->types_.size() &&
           this->types_[type.value].kind == TypeKind::builtin &&
           this->types_[type.value].builtin == BuiltinType::char_;
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

bool TypeTable::is_reference(const TypeHandle type) const noexcept {
    return is_valid(type) && type.value < this->types_.size() && this->types_[type.value].kind == TypeKind::reference;
}

bool TypeTable::is_array(const TypeHandle type) const noexcept {
    return is_valid(type) && type.value < this->types_.size() && this->types_[type.value].kind == TypeKind::array;
}

bool TypeTable::is_slice(const TypeHandle type) const noexcept {
    return is_valid(type) && type.value < this->types_.size() && this->types_[type.value].kind == TypeKind::slice;
}

bool TypeTable::is_tuple(const TypeHandle type) const noexcept {
    return is_valid(type) && type.value < this->types_.size() && this->types_[type.value].kind == TypeKind::tuple;
}

bool TypeTable::is_function(const TypeHandle type) const noexcept {
    return is_valid(type) && type.value < this->types_.size() && this->types_[type.value].kind == TypeKind::function;
}

bool TypeTable::contains_array(const TypeHandle type) const noexcept {
    return is_valid(type) && type.value < this->types_.size() && this->types_[type.value].contains_array;
}

std::string TypeTable::display_name(const TypeHandle type) const {
    std::string name;
    std::vector<TypeDisplayTask> pending;
    pending.push_back(TypeDisplayTask {TypeDisplayTaskKind::type, type, {}});
    while (!pending.empty()) {
        TypeDisplayTask task = std::move(pending.back());
        pending.pop_back();
        if (task.kind == TypeDisplayTaskKind::text) {
            name.append(task.text);
            continue;
        }
        const TypeHandle current = task.type;
        if (!is_valid(current) || current.value >= this->types_.size()) {
            name.append(SEMA_TYPE_DISPLAY_INVALID_NAME);
            continue;
        }
        const TypeInfo& info = this->types_[current.value];
        switch (info.kind) {
        case TypeKind::builtin:
            name += builtin_display_name(info.builtin);
            break;
        case TypeKind::pointer:
            name.append(info.pointer_mutability == PointerMutability::mut
                ? SEMA_TYPE_DISPLAY_POINTER_MUT_PREFIX
                : SEMA_TYPE_DISPLAY_POINTER_CONST_PREFIX);
            pending.push_back(TypeDisplayTask {TypeDisplayTaskKind::type, info.pointee, {}});
            break;
        case TypeKind::reference:
            name.append(info.pointer_mutability == PointerMutability::mut
                ? SEMA_TYPE_DISPLAY_REFERENCE_MUT_PREFIX
                : SEMA_TYPE_DISPLAY_REFERENCE_PREFIX);
            pending.push_back(TypeDisplayTask {TypeDisplayTaskKind::type, info.pointee, {}});
            break;
        case TypeKind::array:
            name.append(SEMA_TYPE_DISPLAY_ARRAY_OPEN);
            name += std::to_string(info.array_count);
            name.append(SEMA_TYPE_DISPLAY_ARRAY_CLOSE);
            pending.push_back(TypeDisplayTask {TypeDisplayTaskKind::type, info.array_element, {}});
            break;
        case TypeKind::slice:
            name.append(info.slice_mutability == PointerMutability::mut
                ? SEMA_TYPE_DISPLAY_SLICE_MUT_PREFIX
                : SEMA_TYPE_DISPLAY_SLICE_CONST_PREFIX);
            pending.push_back(TypeDisplayTask {TypeDisplayTaskKind::type, info.slice_element, {}});
            break;
        case TypeKind::tuple:
            name.append(SEMA_TYPE_DISPLAY_TUPLE_OPEN);
            pending.push_back(TypeDisplayTask {
                TypeDisplayTaskKind::text,
                INVALID_TYPE_HANDLE,
                std::string(SEMA_TYPE_DISPLAY_TUPLE_CLOSE),
            });
            if (info.tuple_elements.size() == 1) {
                pending.push_back(TypeDisplayTask {
                    TypeDisplayTaskKind::text,
                    INVALID_TYPE_HANDLE,
                    std::string(SEMA_TYPE_DISPLAY_SINGLE_TUPLE_TRAILING),
                });
            }
            for (base::usize index = info.tuple_elements.size(); index > 0; --index) {
                pending.push_back(TypeDisplayTask {
                    TypeDisplayTaskKind::type,
                    info.tuple_elements[index - 1],
                    {},
                });
                if (index > 1) {
                    pending.push_back(TypeDisplayTask {
                        TypeDisplayTaskKind::text,
                        INVALID_TYPE_HANDLE,
                        std::string(SEMA_TYPE_DISPLAY_TUPLE_SEPARATOR),
                    });
                }
            }
            break;
        case TypeKind::function: {
            if (info.function_is_unsafe && info.function_call_conv == FunctionCallConv::c) {
                name.append(SEMA_TYPE_DISPLAY_UNSAFE_EXTERN_C_FN_PREFIX);
            } else if (info.function_is_unsafe) {
                name.append(SEMA_TYPE_DISPLAY_UNSAFE_FN_PREFIX);
            } else {
                name.append(info.function_call_conv == FunctionCallConv::c
                    ? SEMA_TYPE_DISPLAY_EXTERN_C_FN_PREFIX
                    : SEMA_TYPE_DISPLAY_FN_PREFIX);
            }
            pending.push_back(TypeDisplayTask {TypeDisplayTaskKind::type, info.function_return, {}});
            pending.push_back(TypeDisplayTask {
                TypeDisplayTaskKind::text,
                INVALID_TYPE_HANDLE,
                std::string(SEMA_TYPE_DISPLAY_FN_RETURN),
            });
            if (info.function_is_variadic) {
                pending.push_back(TypeDisplayTask {
                    TypeDisplayTaskKind::text,
                    INVALID_TYPE_HANDLE,
                    std::string(SEMA_TYPE_DISPLAY_FN_VARIADIC),
                });
                if (!info.function_params.empty()) {
                    pending.push_back(TypeDisplayTask {
                        TypeDisplayTaskKind::text,
                        INVALID_TYPE_HANDLE,
                        ", ",
                    });
                }
            }
            for (base::usize index = info.function_params.size(); index > 0; --index) {
                pending.push_back(TypeDisplayTask {
                    TypeDisplayTaskKind::type,
                    info.function_params[index - 1],
                    {},
                });
                if (index > 1) {
                    pending.push_back(TypeDisplayTask {
                        TypeDisplayTaskKind::text,
                        INVALID_TYPE_HANDLE,
                        ", ",
                    });
                }
            }
            break;
        }
        case TypeKind::struct_:
        case TypeKind::enum_:
        case TypeKind::opaque_struct:
            name += info.name.view();
            if (!info.generic_args.empty()) {
                name.append(SEMA_TYPE_DISPLAY_GENERIC_ARG_LIST_OPEN);
                push_generic_arg_display_tasks(pending, info.generic_args);
            }
            break;
        case TypeKind::generic_param:
            name += info.name.view();
            break;
        default:
            name.append(SEMA_TYPE_DISPLAY_UNKNOWN_NAME);
            break;
        }
    }
    return name;
}

std::string TypeTable::display_name(
    const std::string_view base_name,
    const std::span<const TypeHandle> generic_args
) const {
    if (generic_args.empty()) {
        return std::string(base_name);
    }

    std::string name;
    name.reserve(
        base_name.size() +
        SEMA_TYPE_DISPLAY_GENERIC_ARG_LIST_OPEN.size() +
        SEMA_TYPE_DISPLAY_GENERIC_ARG_LIST_CLOSE.size() +
        generic_args.size() * SEMA_TYPE_DISPLAY_GENERIC_ARG_SIZE_ESTIMATE
    );
    name.append(base_name);
    name.append(SEMA_TYPE_DISPLAY_GENERIC_ARG_LIST_OPEN);
    for (base::usize index = 0; index < generic_args.size(); ++index) {
        if (index != 0) {
            name.append(SEMA_TYPE_DISPLAY_GENERIC_ARG_LIST_SEPARATOR);
        }
        name += this->display_name(generic_args[index]);
    }
    name.append(SEMA_TYPE_DISPLAY_GENERIC_ARG_LIST_CLOSE);
    return name;
}

std::string TypeTable::c_name(const TypeHandle type) const {
    if (!is_valid(type) || type.value >= this->types_.size()) {
        return "void";
    }
    const TypeInfo& info = this->types_[type.value];
    if (info.c_name.empty()) {
        return this->display_name(type);
    }
    return std::string(info.c_name.view());
}

const TypeInfo& TypeTable::get(const TypeHandle handle) const noexcept {
    assert(handle.value < this->types_.size());
    return this->types_[handle.value];
}

base::usize TypeTable::size() const noexcept {
    return this->types_.size();
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
           (static_cast<std::size_t>(key.count) * SEMA_TYPE_HASH_MULTIPLIER);
}

std::size_t TypeTable::SliceKeyHash::operator()(const SliceKey& key) const noexcept {
    return (static_cast<std::size_t>(key.element) << 1) ^
           static_cast<std::size_t>(key.mutability == PointerMutability::mut ? 1U : 0U);
}

std::size_t TypeTable::FunctionKeyHash::operator()(const FunctionKey& key) const noexcept {
    std::size_t hash = static_cast<std::size_t>(key.return_type) ^
        (static_cast<std::size_t>(key.call_conv == FunctionCallConv::c ? 1U : 0U) << 1) ^
        (static_cast<std::size_t>(key.is_unsafe ? 1U : 0U) << 2) ^
        (static_cast<std::size_t>(key.is_variadic ? 1U : 0U) << 3);
    for (const base::u32 param : key.params) {
        hash = (hash * SEMA_TYPE_HASH_MULTIPLIER) ^ static_cast<std::size_t>(param);
    }
    return hash;
}

std::size_t TypeTable::TupleKeyHash::operator()(const TupleKey& key) const noexcept {
    std::size_t hash = key.elements.size();
    for (const base::u32 element : key.elements) {
        hash = (hash * SEMA_TYPE_HASH_MULTIPLIER) ^ static_cast<std::size_t>(element);
    }
    return hash;
}

} // namespace aurex::sema
