#include <aurex/sema/sema.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aurex::sema {

namespace {

constexpr base::usize SEMA_TYPE_LAYOUT_INITIAL_STACK_CAPACITY = 16;
constexpr base::u64 SEMA_ABI_INVALID_SIZE = 0;
constexpr base::u64 SEMA_ABI_MIN_ALIGNMENT = 1;
constexpr int SEMA_INTEGER_LITERAL_DECIMAL_BASE = 10;
constexpr int SEMA_INTEGER_LITERAL_HEX_BASE = 16;
constexpr int SEMA_INTEGER_LITERAL_BINARY_BASE = 2;
constexpr base::usize SEMA_INTEGER_LITERAL_PREFIX_LENGTH = 2;
constexpr char SEMA_INTEGER_LITERAL_ZERO_CHAR = '0';
constexpr char SEMA_INTEGER_LITERAL_HEX_LOWER_PREFIX_CHAR = 'x';
constexpr char SEMA_INTEGER_LITERAL_HEX_UPPER_PREFIX_CHAR = 'X';
constexpr char SEMA_INTEGER_LITERAL_BINARY_LOWER_PREFIX_CHAR = 'b';
constexpr char SEMA_INTEGER_LITERAL_BINARY_UPPER_PREFIX_CHAR = 'B';
constexpr char SEMA_INTEGER_LITERAL_UNDERSCORE_CHAR = '_';
constexpr char SEMA_INTEGER_LITERAL_DECIMAL_FIRST_CHAR = '0';
constexpr char SEMA_INTEGER_LITERAL_DECIMAL_LAST_CHAR = '9';
constexpr char SEMA_INTEGER_LITERAL_HEX_LOWER_FIRST_CHAR = 'a';
constexpr char SEMA_INTEGER_LITERAL_HEX_LOWER_LAST_CHAR = 'f';
constexpr char SEMA_INTEGER_LITERAL_HEX_UPPER_FIRST_CHAR = 'A';
constexpr char SEMA_INTEGER_LITERAL_HEX_UPPER_LAST_CHAR = 'F';
constexpr base::u64 SEMA_INTEGER_LITERAL_HEX_DIGIT_OFFSET = 10;
constexpr base::u32 SEMA_INTEGER_LITERAL_MAX_BITS = std::numeric_limits<base::u64>::digits;
constexpr base::u32 SEMA_INTEGER_LITERAL_SIGN_BIT_SHIFT = SEMA_INTEGER_LITERAL_MAX_BITS - 1;
constexpr base::u32 SEMA_INTEGER_LITERAL_INVALID_BITS = 0;

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
    base::SourceRange range {};
    TypeLayoutFrameStage stage = TypeLayoutFrameStage::enter;
};

enum class TypeResolveActionKind {
    resolve,
    build_pointer,
    build_array,
    build_generic_struct,
    build_generic_enum,
};

struct TypeResolveAction {
    TypeResolveActionKind kind = TypeResolveActionKind::resolve;
    syntax::TypeId type = syntax::INVALID_TYPE_ID;
    bool opaque_allowed_as_pointee = false;
    syntax::PointerMutability pointer_mutability = syntax::PointerMutability::const_;
    std::optional<base::u64> array_count {};
    const GenericStructTemplateInfo* struct_template = nullptr;
    const GenericEnumTemplateInfo* enum_template = nullptr;
    std::optional<base::usize> argument_count {};
    base::SourceRange range {};
};

[[nodiscard]] BuiltinType map_builtin(const syntax::PrimitiveTypeKind kind) noexcept {
    switch (kind) {
    case syntax::PrimitiveTypeKind::void_: return BuiltinType::void_;
    case syntax::PrimitiveTypeKind::bool_: return BuiltinType::bool_;
    case syntax::PrimitiveTypeKind::i8: return BuiltinType::i8;
    case syntax::PrimitiveTypeKind::u8: return BuiltinType::u8;
    case syntax::PrimitiveTypeKind::i16: return BuiltinType::i16;
    case syntax::PrimitiveTypeKind::u16: return BuiltinType::u16;
    case syntax::PrimitiveTypeKind::i32: return BuiltinType::i32;
    case syntax::PrimitiveTypeKind::u32: return BuiltinType::u32;
    case syntax::PrimitiveTypeKind::i64: return BuiltinType::i64;
    case syntax::PrimitiveTypeKind::u64: return BuiltinType::u64;
    case syntax::PrimitiveTypeKind::isize: return BuiltinType::isize;
    case syntax::PrimitiveTypeKind::usize: return BuiltinType::usize;
    case syntax::PrimitiveTypeKind::f32: return BuiltinType::f32;
    case syntax::PrimitiveTypeKind::f64: return BuiltinType::f64;
    case syntax::PrimitiveTypeKind::str: return BuiltinType::str;
    }
    return BuiltinType::void_;
}

[[nodiscard]] PointerMutability map_mutability(const syntax::PointerMutability mutability) noexcept {
    return mutability == syntax::PointerMutability::mut ? PointerMutability::mut : PointerMutability::const_;
}

[[nodiscard]] bool builtin_is_unsigned(const BuiltinType type) noexcept {
    switch (type) {
    case BuiltinType::u8:
    case BuiltinType::u16:
    case BuiltinType::u32:
    case BuiltinType::u64:
    case BuiltinType::usize:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] base::u32 builtin_integer_bits(const BuiltinType type) noexcept {
    switch (type) {
    case BuiltinType::i8:
    case BuiltinType::u8: return std::numeric_limits<std::uint8_t>::digits;
    case BuiltinType::i16:
    case BuiltinType::u16: return std::numeric_limits<std::uint16_t>::digits;
    case BuiltinType::i32:
    case BuiltinType::u32: return std::numeric_limits<std::uint32_t>::digits;
    case BuiltinType::i64:
    case BuiltinType::u64: return std::numeric_limits<std::uint64_t>::digits;
    case BuiltinType::isize: return std::numeric_limits<std::ptrdiff_t>::digits;
    case BuiltinType::usize: return std::numeric_limits<std::size_t>::digits;
    default: return SEMA_INTEGER_LITERAL_INVALID_BITS;
    }
}

[[nodiscard]] bool parse_integer_literal_digit(const char c, const int radix, base::u64& digit) noexcept {
    if (c >= SEMA_INTEGER_LITERAL_DECIMAL_FIRST_CHAR && c <= SEMA_INTEGER_LITERAL_DECIMAL_LAST_CHAR) {
        digit = static_cast<base::u64>(c - SEMA_INTEGER_LITERAL_DECIMAL_FIRST_CHAR);
        return digit < static_cast<base::u64>(radix);
    }
    if (c >= SEMA_INTEGER_LITERAL_HEX_LOWER_FIRST_CHAR && c <= SEMA_INTEGER_LITERAL_HEX_LOWER_LAST_CHAR) {
        digit = static_cast<base::u64>(SEMA_INTEGER_LITERAL_HEX_DIGIT_OFFSET +
            static_cast<base::u64>(c - SEMA_INTEGER_LITERAL_HEX_LOWER_FIRST_CHAR));
        return digit < static_cast<base::u64>(radix);
    }
    if (c >= SEMA_INTEGER_LITERAL_HEX_UPPER_FIRST_CHAR && c <= SEMA_INTEGER_LITERAL_HEX_UPPER_LAST_CHAR) {
        digit = static_cast<base::u64>(SEMA_INTEGER_LITERAL_HEX_DIGIT_OFFSET +
            static_cast<base::u64>(c - SEMA_INTEGER_LITERAL_HEX_UPPER_FIRST_CHAR));
        return digit < static_cast<base::u64>(radix);
    }
    return false;
}

[[nodiscard]] bool parse_u64_literal_checked(const std::string_view text, base::u64& value) noexcept {
    int radix = SEMA_INTEGER_LITERAL_DECIMAL_BASE;
    base::usize index = 0;
    if (text.size() > SEMA_INTEGER_LITERAL_PREFIX_LENGTH && text[0] == SEMA_INTEGER_LITERAL_ZERO_CHAR) {
        if (text[1] == SEMA_INTEGER_LITERAL_HEX_LOWER_PREFIX_CHAR || text[1] == SEMA_INTEGER_LITERAL_HEX_UPPER_PREFIX_CHAR) {
            radix = SEMA_INTEGER_LITERAL_HEX_BASE;
            index = SEMA_INTEGER_LITERAL_PREFIX_LENGTH;
        } else if (text[1] == SEMA_INTEGER_LITERAL_BINARY_LOWER_PREFIX_CHAR ||
            text[1] == SEMA_INTEGER_LITERAL_BINARY_UPPER_PREFIX_CHAR) {
            radix = SEMA_INTEGER_LITERAL_BINARY_BASE;
            index = SEMA_INTEGER_LITERAL_PREFIX_LENGTH;
        }
    }

    value = SEMA_ABI_INVALID_SIZE;
    bool saw_digit = false;
    for (; index < text.size(); ++index) {
        const char c = text[index];
        if (c == SEMA_INTEGER_LITERAL_UNDERSCORE_CHAR) {
            continue;
        }

        base::u64 digit = SEMA_ABI_INVALID_SIZE;
        if (!parse_integer_literal_digit(c, radix, digit)) {
            return false;
        }
        saw_digit = true;
        if (value > (std::numeric_limits<base::u64>::max() - digit) / static_cast<base::u64>(radix)) {
            return false;
        }
        value = value * static_cast<base::u64>(radix) + digit;
    }
    return saw_digit;
}

template <typename Float>
[[nodiscard]] bool parse_float_literal_checked(const std::string_view text) noexcept {
    std::string digits;
    digits.reserve(text.size());
    for (const char c : text) {
        if (c != SEMA_INTEGER_LITERAL_UNDERSCORE_CHAR) {
            digits.push_back(c);
        }
    }
    Float value {};
    const char* const begin = digits.data();
    const char* const end = digits.data() + digits.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc {} && result.ptr == end && std::isfinite(value);
}

[[nodiscard]] bool literal_fits_integer_type(
    const TypeTable& types,
    const TypeHandle destination,
    const std::string_view text
) noexcept {
    if (!types.is_integer(destination)) {
        return false;
    }
    const TypeInfo& info = types.get(destination);
    const base::u32 bits = builtin_integer_bits(info.builtin);
    if (bits == SEMA_INTEGER_LITERAL_INVALID_BITS) {
        return false;
    }

    base::u64 value = 0;
    if (!parse_u64_literal_checked(text, value)) {
        return false;
    }
    if (builtin_is_unsigned(info.builtin)) {
        if (bits >= SEMA_INTEGER_LITERAL_MAX_BITS) {
            return true;
        }
        return value <= ((base::u64 {1} << bits) - 1);
    }
    if (bits >= SEMA_INTEGER_LITERAL_MAX_BITS) {
        return value <= static_cast<base::u64>(std::numeric_limits<std::int64_t>::max());
    }
    return value <= ((base::u64 {1} << (bits - 1)) - 1);
}

[[nodiscard]] bool negative_literal_fits_integer_type(
    const TypeTable& types,
    const TypeHandle destination,
    const std::string_view text
) noexcept {
    if (!types.is_integer(destination)) {
        return false;
    }
    const TypeInfo& info = types.get(destination);
    const base::u32 bits = builtin_integer_bits(info.builtin);
    if (bits == SEMA_INTEGER_LITERAL_INVALID_BITS || builtin_is_unsigned(info.builtin)) {
        return false;
    }

    base::u64 value = 0;
    if (!parse_u64_literal_checked(text, value)) {
        return false;
    }
    if (bits >= SEMA_INTEGER_LITERAL_MAX_BITS) {
        return value <= (base::u64 {1} << SEMA_INTEGER_LITERAL_SIGN_BIT_SHIFT);
    }
    return value <= (base::u64 {1} << (bits - 1));
}

[[nodiscard]] base::u64 align_forward(const base::u64 offset, const base::u64 alignment) noexcept {
    if (alignment == 0) {
        return offset;
    }
    const base::u64 mask = alignment - 1;
    if (offset > std::numeric_limits<base::u64>::max() - mask) {
        return std::numeric_limits<base::u64>::max();
    }
    return (offset + mask) & ~mask;
}

[[nodiscard]] bool checked_add_u64(const base::u64 lhs, const base::u64 rhs, base::u64& result) noexcept {
    if (lhs > std::numeric_limits<base::u64>::max() - rhs) {
        result = std::numeric_limits<base::u64>::max();
        return false;
    }
    result = lhs + rhs;
    return true;
}

[[nodiscard]] bool checked_mul_u64(const base::u64 lhs, const base::u64 rhs, base::u64& result) noexcept {
    if (lhs != 0 && rhs > std::numeric_limits<base::u64>::max() / lhs) {
        result = std::numeric_limits<base::u64>::max();
        return false;
    }
    result = lhs * rhs;
    return true;
}

[[nodiscard]] bool checked_align_forward(
    const base::u64 offset,
    const base::u64 alignment,
    base::u64& result
) noexcept {
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

[[nodiscard]] base::u64 add_saturating(const base::u64 lhs, const base::u64 rhs) noexcept {
    base::u64 result = SEMA_ABI_INVALID_SIZE;
    static_cast<void>(checked_add_u64(lhs, rhs, result));
    return result;
}

[[nodiscard]] base::u64 mul_saturating(const base::u64 lhs, const base::u64 rhs) noexcept {
    base::u64 result = SEMA_ABI_INVALID_SIZE;
    static_cast<void>(checked_mul_u64(lhs, rhs, result));
    return result;
}

[[nodiscard]] bool is_builtin_scalar_bcast_type(const TypeTable& types, const TypeHandle type) noexcept {
    if (!is_valid(type)) {
        return false;
    }
    const TypeInfo& info = types.get(type);
    return info.kind == TypeKind::builtin &&
           info.builtin != BuiltinType::void_ &&
           info.builtin != BuiltinType::bool_ &&
           info.builtin != BuiltinType::str;
}

} // namespace

TypeHandle SemanticAnalyzer::resolve_type(const syntax::TypeId type_id) {
    return this->resolve_type(type_id, false);
}

TypeHandle SemanticAnalyzer::resolve_type(const syntax::TypeId type_id, const bool opaque_allowed_as_pointee) {
    return this->resolve_type_with_substitution(type_id, this->current_type_substitution_, opaque_allowed_as_pointee);
}

TypeHandle SemanticAnalyzer::resolve_type_with_substitution(
    const syntax::TypeId type_id,
    const GenericTypeSubstitution* const substitution,
    const bool opaque_allowed_as_pointee
) {
    std::vector<TypeResolveAction> actions;
    std::vector<TypeHandle> values;
    actions.reserve(SEMA_TYPE_LAYOUT_INITIAL_STACK_CAPACITY);
    values.reserve(SEMA_TYPE_LAYOUT_INITIAL_STACK_CAPACITY);
    actions.push_back(TypeResolveAction {
        TypeResolveActionKind::resolve,
        type_id,
        opaque_allowed_as_pointee,
    });

    while (!actions.empty()) {
        const TypeResolveAction action = actions.back();
        actions.pop_back();
        switch (action.kind) {
        case TypeResolveActionKind::resolve: {
            if (!syntax::is_valid(action.type) || action.type.value >= this->module_.types.size()) {
                values.push_back(INVALID_TYPE_HANDLE);
                break;
            }

            if (substitution != nullptr && this->current_generic_syntax_type_handles_ != nullptr) {
                if (const auto found = this->current_generic_syntax_type_handles_->find(action.type.value);
                    found != this->current_generic_syntax_type_handles_->end() &&
                    is_valid(found->second)) {
                    if (this->checked_.types.get(found->second).kind == TypeKind::opaque_struct &&
                        !action.opaque_allowed_as_pointee) {
                        this->report(
                            this->module_.types[action.type.value].range,
                            "opaque struct can only be used as a pointer target"
                        );
                    }
                    values.push_back(found->second);
                    break;
                }
            }

            if (substitution == nullptr &&
                action.type.value < this->checked_.syntax_type_handles.size() &&
                is_valid(this->checked_.syntax_type_handles[action.type.value])) {
                const TypeHandle cached = this->checked_.syntax_type_handles[action.type.value];
                if (this->checked_.types.get(cached).kind == TypeKind::opaque_struct &&
                    !action.opaque_allowed_as_pointee) {
                    this->report(
                        this->module_.types[action.type.value].range,
                        "opaque struct can only be used as a pointer target"
                    );
                }
                values.push_back(cached);
                break;
            }

            const syntax::TypeNode& type = this->module_.types[action.type.value];
            switch (type.kind) {
            case syntax::TypeKind::primitive: {
                const TypeHandle resolved = this->checked_.types.builtin(map_builtin(type.primitive));
                this->record_syntax_type_handle(action.type, resolved);
                values.push_back(resolved);
                break;
            }
            case syntax::TypeKind::pointer:
                actions.push_back(TypeResolveAction {
                    TypeResolveActionKind::build_pointer,
                    action.type,
                    action.opaque_allowed_as_pointee,
                    type.pointer_mutability,
                });
                actions.push_back(TypeResolveAction {
                    TypeResolveActionKind::resolve,
                    type.pointee,
                    true,
                });
                break;
            case syntax::TypeKind::array:
                actions.push_back(TypeResolveAction {
                    TypeResolveActionKind::build_array,
                    action.type,
                    action.opaque_allowed_as_pointee,
                    syntax::PointerMutability::const_,
                    type.array_count,
                });
                actions.push_back(TypeResolveAction {
                    TypeResolveActionKind::resolve,
                    type.array_element,
                    false,
                });
                break;
            case syntax::TypeKind::named: {
                const bool qualified = !type.scope_name.empty();
                syntax::ModuleId scope_module = syntax::INVALID_MODULE_ID;
                if (qualified) {
                    scope_module = this->resolve_import_alias(type.scope_name, type.scope_range);
                    if (!syntax::is_valid(scope_module)) {
                        this->record_syntax_type_handle(action.type, INVALID_TYPE_HANDLE);
                        values.push_back(INVALID_TYPE_HANDLE);
                        break;
                    }
                }

                if (substitution != nullptr && type.type_args.empty() && !qualified) {
                    if (const auto found = substitution->types.find(std::string(type.name)); found != substitution->types.end()) {
                        this->record_syntax_type_handle(action.type, found->second);
                        values.push_back(found->second);
                        break;
                    }
                }

                if (!type.type_args.empty()) {
                    const GenericStructTemplateInfo* struct_info = qualified
                        ? this->find_generic_struct_template_in_module(scope_module, type.name, type.range, false)
                        : this->find_generic_struct_template_in_visible_modules(type.name, type.range, false);
                    if (struct_info != nullptr) {
                        if (type.type_args.size() != struct_info->params.size()) {
                            this->report(type.range, "generic struct type argument count mismatch for " + struct_info->name);
                            this->record_syntax_type_handle(action.type, INVALID_TYPE_HANDLE);
                            values.push_back(INVALID_TYPE_HANDLE);
                            break;
                        }
                actions.push_back(TypeResolveAction {
                    TypeResolveActionKind::build_generic_struct,
                    action.type,
                    action.opaque_allowed_as_pointee,
                    syntax::PointerMutability::const_,
                    std::nullopt,
                    struct_info,
                    nullptr,
                    type.type_args.size(),
                    type.range,
                        });
                        for (base::usize i = type.type_args.size(); i > 0; --i) {
                            actions.push_back(TypeResolveAction {
                                TypeResolveActionKind::resolve,
                                type.type_args[i - 1],
                                action.opaque_allowed_as_pointee,
                            });
                        }
                        break;
                    }

                    const GenericEnumTemplateInfo* enum_info = qualified
                        ? this->find_generic_enum_template_in_module(scope_module, type.name, type.range, false)
                        : this->find_generic_enum_template_in_visible_modules(type.name, type.range, false);
                    if (enum_info != nullptr) {
                        if (type.type_args.size() != enum_info->params.size()) {
                            this->report(type.range, "generic enum type argument count mismatch for " + enum_info->name);
                            this->record_syntax_type_handle(action.type, INVALID_TYPE_HANDLE);
                            values.push_back(INVALID_TYPE_HANDLE);
                            break;
                        }
                actions.push_back(TypeResolveAction {
                    TypeResolveActionKind::build_generic_enum,
                    action.type,
                    action.opaque_allowed_as_pointee,
                    syntax::PointerMutability::const_,
                    std::nullopt,
                    nullptr,
                    enum_info,
                    type.type_args.size(),
                            type.range,
                        });
                        for (base::usize i = type.type_args.size(); i > 0; --i) {
                            actions.push_back(TypeResolveAction {
                                TypeResolveActionKind::resolve,
                                type.type_args[i - 1],
                                action.opaque_allowed_as_pointee,
                            });
                        }
                        break;
                    }

                    const std::string qualifier = qualified
                        ? std::string(type.scope_name) + "::"
                        : std::string {};
                    this->report(type.range, "type arguments require a generic type: " + qualifier + std::string(type.name));
                    this->record_syntax_type_handle(action.type, INVALID_TYPE_HANDLE);
                    values.push_back(INVALID_TYPE_HANDLE);
                    break;
                }

                if (qualified) {
                    if (const GenericStructTemplateInfo* info =
                            this->find_generic_struct_template_in_module(scope_module, type.name, type.range, false);
                        info != nullptr) {
                        this->report(
                            type.range,
                            "generic struct type requires type arguments: " + std::string(type.scope_name) + "::" + info->name
                        );
                        this->record_syntax_type_handle(action.type, INVALID_TYPE_HANDLE);
                        values.push_back(INVALID_TYPE_HANDLE);
                        break;
                    }
                    if (const GenericEnumTemplateInfo* info =
                            this->find_generic_enum_template_in_module(scope_module, type.name, type.range, false);
                        info != nullptr) {
                        this->report(
                            type.range,
                            "generic enum type requires type arguments: " + std::string(type.scope_name) + "::" + info->name
                        );
                        this->record_syntax_type_handle(action.type, INVALID_TYPE_HANDLE);
                        values.push_back(INVALID_TYPE_HANDLE);
                        break;
                    }
                    const TypeHandle resolved = this->find_type_in_module(
                        scope_module,
                        type.name,
                        type.range,
                        action.opaque_allowed_as_pointee
                    );
                    if (is_valid(resolved) &&
                        this->checked_.types.get(resolved).kind == TypeKind::opaque_struct &&
                        !action.opaque_allowed_as_pointee) {
                        this->report(type.range, "opaque struct can only be used as a pointer target");
                    }
                    this->record_syntax_type_handle(action.type, resolved);
                    values.push_back(resolved);
                    break;
                }

                if (const GenericStructTemplateInfo* info =
                        this->find_generic_struct_template_in_visible_modules(type.name, type.range, false);
                    info != nullptr) {
                    this->report(type.range, "generic struct type requires type arguments: " + info->name);
                    this->record_syntax_type_handle(action.type, INVALID_TYPE_HANDLE);
                    values.push_back(INVALID_TYPE_HANDLE);
                    break;
                }
                if (const GenericEnumTemplateInfo* info =
                        this->find_generic_enum_template_in_visible_modules(type.name, type.range, false);
                    info != nullptr) {
                    this->report(type.range, "generic enum type requires type arguments: " + info->name);
                    this->record_syntax_type_handle(action.type, INVALID_TYPE_HANDLE);
                    values.push_back(INVALID_TYPE_HANDLE);
                    break;
                }

                const TypeHandle resolved = this->find_type_in_visible_modules(
                    type.name,
                    type.range,
                    action.opaque_allowed_as_pointee
                );
                if (is_valid(resolved) &&
                    this->checked_.types.get(resolved).kind == TypeKind::opaque_struct &&
                    !action.opaque_allowed_as_pointee) {
                    this->report(type.range, "opaque struct can only be used as a pointer target");
                }
                this->record_syntax_type_handle(action.type, resolved);
                values.push_back(resolved);
                break;
            }
            }
            break;
        }
        case TypeResolveActionKind::build_pointer: {
            const TypeHandle pointee = values.back();
            values.pop_back();
            const TypeHandle resolved = this->checked_.types.pointer(map_mutability(action.pointer_mutability), pointee);
            this->record_syntax_type_handle(action.type, resolved);
            values.push_back(resolved);
            break;
        }
        case TypeResolveActionKind::build_array: {
            const TypeHandle element = values.back();
            values.pop_back();
            const TypeHandle resolved = this->checked_.types.array(*action.array_count, element);
            this->record_syntax_type_handle(action.type, resolved);
            values.push_back(resolved);
            break;
        }
        case TypeResolveActionKind::build_generic_struct: {
            const base::usize argument_count = *action.argument_count;
            std::vector<TypeHandle> args(argument_count, INVALID_TYPE_HANDLE);
            for (base::usize i = argument_count; i > 0; --i) {
                args[i - 1] = values.back();
                values.pop_back();
            }
            const TypeHandle resolved = this->instantiate_generic_struct(*action.struct_template, args, action.range);
            this->record_syntax_type_handle(action.type, resolved);
            values.push_back(resolved);
            break;
        }
        case TypeResolveActionKind::build_generic_enum: {
            const base::usize argument_count = *action.argument_count;
            std::vector<TypeHandle> args(argument_count, INVALID_TYPE_HANDLE);
            for (base::usize i = argument_count; i > 0; --i) {
                args[i - 1] = values.back();
                values.pop_back();
            }
            const TypeHandle resolved = this->instantiate_generic_enum(*action.enum_template, args, action.range);
            this->record_syntax_type_handle(action.type, resolved);
            values.push_back(resolved);
            break;
        }
        }
    }

    const TypeHandle resolved = values.back();
    this->record_syntax_type_handle(type_id, resolved);
    return resolved;
}

TypeHandle SemanticAnalyzer::resolve_type_alias(const TypeAliasInfo& alias, const bool opaque_allowed_as_pointee) {
    const std::string key = module_key(alias.module, alias.name);
    if (const auto found = resolved_type_aliases_.find(key); found != resolved_type_aliases_.end()) {
        return found->second;
    }
    if (std::find(resolving_type_aliases_.begin(), resolving_type_aliases_.end(), key) != resolving_type_aliases_.end()) {
        report(alias.range, "cyclic type alias: " + alias.name);
        resolved_type_aliases_[key] = INVALID_TYPE_HANDLE;
        return INVALID_TYPE_HANDLE;
    }
    resolving_type_aliases_.push_back(key);
    const syntax::ModuleId previous_module = current_module_;
    current_module_ = alias.module;
    const TypeHandle resolved = resolve_type(alias.target, opaque_allowed_as_pointee);
    current_module_ = previous_module;
    resolving_type_aliases_.pop_back();
    resolved_type_aliases_[key] = resolved;
    return resolved;
}

bool SemanticAnalyzer::can_assign(const TypeHandle dst, const TypeHandle src, const syntax::ExprId value) const noexcept {
    if (!is_valid(dst) || !is_valid(src)) {
        return is_valid(dst) && is_null_literal(value) && checked_.types.is_pointer(dst);
    }
    if (checked_.types.is_integer(dst) && checked_.types.is_integer(src) && is_integer_literal(value)) {
        return syntax::is_valid(value) &&
               value.value < module_.exprs.size() &&
               integer_literal_fits_type(dst, module_.exprs[value.value].text);
    }
    if (checked_.types.is_pointer(dst) && checked_.types.is_pointer(src)) {
        const TypeInfo& dst_info = checked_.types.get(dst);
        const TypeInfo& src_info = checked_.types.get(src);
        if (dst_info.pointer_mutability == PointerMutability::const_ &&
            checked_.types.same(dst_info.pointee, src_info.pointee)) {
            return true;
        }
    }
    return checked_.types.same(dst, src);
}

bool SemanticAnalyzer::is_valid_storage_type(const TypeHandle type) const {
    TypeHandle current = type;
    while (true) {
        if (!is_valid(current)) {
            return false;
        }
        const TypeInfo& info = this->checked_.types.get(current);
        if (this->checked_.types.is_void(current) || info.kind == TypeKind::opaque_struct) {
            return false;
        }
        if (info.kind != TypeKind::array) {
            return true;
        }
        const base::u64 element_size = this->abi_size(info.array_element);
        if (element_size != 0 && info.array_count > std::numeric_limits<base::u64>::max() / element_size) {
            return false;
        }
        current = info.array_element;
    }
}

void SemanticAnalyzer::validate_type_layouts() {
    struct LayoutResult {
        base::u64 size = SEMA_ABI_INVALID_SIZE;
        base::u64 align = SEMA_ABI_MIN_ALIGNMENT;
        bool ok = false;
    };

    std::unordered_map<base::u32, TypeLayoutVisitState> states;
    std::unordered_map<base::u32, LayoutResult> results;

    const auto primitive_layout = [&](const TypeHandle type) -> LayoutResult {
        const TypeInfo& info = this->checked_.types.get(type);
        if (info.kind == TypeKind::builtin && info.builtin == BuiltinType::void_) {
            return {};
        }
        const TypeAbiLayout layout = this->abi_layout(type);
        return LayoutResult {layout.size, layout.align, true};
    };

    const auto cached_result = [&](const TypeHandle type) -> LayoutResult {
        if (!is_valid(type)) {
            return {};
        }
        const TypeInfo& info = this->checked_.types.get(type);
        if (info.kind == TypeKind::builtin || info.kind == TypeKind::pointer) {
            return primitive_layout(type);
        }
        if (info.kind == TypeKind::opaque_struct) {
            return {};
        }
        const auto found = results.find(type.value);
        return found == results.end() ? LayoutResult {} : found->second;
    };

    const auto finish_type = [&](const TypeHandle type, const base::SourceRange range) -> LayoutResult {
        LayoutResult result;
        result.ok = true;
        const TypeInfo& info = this->checked_.types.get(type);
        if (info.kind == TypeKind::array) {
            const LayoutResult element = cached_result(info.array_element);
            if (!element.ok) {
                result = LayoutResult {
                    SEMA_ABI_INVALID_SIZE,
                    std::max(SEMA_ABI_MIN_ALIGNMENT, element.align),
                    false,
                };
                return result;
            }
            base::u64 size = SEMA_ABI_INVALID_SIZE;
            if (!checked_mul_u64(info.array_count, element.size, size)) {
                this->report(range, "array storage size overflows ABI size");
                result = LayoutResult {size, element.align, false};
                return result;
            }
            result = LayoutResult {size, element.align, true};
            return result;
        }
        if (info.kind == TypeKind::struct_) {
            const StructInfo* struct_info = this->find_struct(type);
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
                        this->report(field.range, "struct storage size overflows ABI size");
                        result.ok = false;
                    }
                    base::u64 next_offset = SEMA_ABI_INVALID_SIZE;
                    if (!checked_add_u64(aligned_offset, field_layout.size, next_offset)) {
                        this->report(field.range, "struct storage size overflows ABI size");
                        result.ok = false;
                    }
                    offset = next_offset;
                }
                base::u64 size = SEMA_ABI_INVALID_SIZE;
                if (!checked_align_forward(offset, max_align, size)) {
                    this->report(range, "struct storage size overflows ABI size");
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
            for (const auto& entry : this->checked_.enum_cases) {
                const EnumCaseInfo& enum_case = entry.second;
                if (!this->checked_.types.same(enum_case.type, type) || !is_valid(enum_case.payload_type)) {
                    continue;
                }
                has_payload = true;
                const LayoutResult payload = cached_result(enum_case.payload_type);
                if (!payload.ok) {
                    result.ok = false;
                    continue;
                }
                if (payload.size > payload_size ||
                    (payload.size == payload_size && payload.align > payload_align)) {
                    payload_size = payload.size;
                    payload_align = payload.align;
                }
            }
            if (has_payload) {
                const base::u64 max_align = std::max(result.align, payload_align);
                base::u64 storage_offset = SEMA_ABI_INVALID_SIZE;
                if (!checked_align_forward(result.size, payload_align, storage_offset)) {
                    this->report(range, "enum storage size overflows ABI size");
                    result.ok = false;
                }
                base::u64 total = SEMA_ABI_INVALID_SIZE;
                if (!checked_add_u64(storage_offset, payload_size, total)) {
                    this->report(range, "enum storage size overflows ABI size");
                    result.ok = false;
                }
                base::u64 size = SEMA_ABI_INVALID_SIZE;
                if (!checked_align_forward(total, max_align, size)) {
                    this->report(range, "enum storage size overflows ABI size");
                    result.ok = false;
                }
                result.size = size;
                result.align = max_align;
            }
            return result;
        }
        return result;
    };

    const auto push_dependency = [&](
        std::vector<TypeLayoutFrame>& stack,
        const TypeHandle dependency,
        const base::SourceRange dependency_range
    ) {
        if (!is_valid(dependency)) {
            return;
        }
        const TypeInfo& dependency_info = this->checked_.types.get(dependency);
        if (dependency_info.kind == TypeKind::builtin ||
            dependency_info.kind == TypeKind::pointer ||
            dependency_info.kind == TypeKind::opaque_struct ||
            results.contains(dependency.value)) {
            return;
        }
        const auto state = states.find(dependency.value);
        if (state != states.end() && state->second == TypeLayoutVisitState::visiting) {
            this->report(
                dependency_range,
                "recursive value type is not valid storage: " + this->checked_.types.display_name(dependency)
            );
            results[dependency.value] = {};
            states[dependency.value] = TypeLayoutVisitState::done;
            return;
        }
        stack.push_back(TypeLayoutFrame {dependency, dependency_range, TypeLayoutFrameStage::enter});
    };

    const auto push_children = [&](
        std::vector<TypeLayoutFrame>& stack,
        const TypeHandle type,
        const base::SourceRange range
    ) {
        const TypeInfo& info = this->checked_.types.get(type);
        if (info.kind == TypeKind::array) {
            push_dependency(stack, info.array_element, range);
            return;
        }
        if (info.kind == TypeKind::struct_) {
            const StructInfo* struct_info = this->find_struct(type);
            if (struct_info != nullptr && !struct_info->is_opaque) {
                for (auto field = struct_info->fields.rbegin(); field != struct_info->fields.rend(); ++field) {
                    push_dependency(stack, field->type, field->range);
                }
            }
            return;
        }
        if (info.kind == TypeKind::enum_) {
            push_dependency(stack, info.enum_underlying, range);
            for (const auto& entry : this->checked_.enum_cases) {
                const EnumCaseInfo& enum_case = entry.second;
                if (this->checked_.types.same(enum_case.type, type) && is_valid(enum_case.payload_type)) {
                    push_dependency(stack, enum_case.payload_type, enum_case.range);
                }
            }
            return;
        }
    };

    const auto compute = [&](const TypeHandle type, const base::SourceRange range) -> LayoutResult {
        if (!is_valid(type)) {
            return {};
        }
        const TypeInfo& info = this->checked_.types.get(type);
        if (info.kind == TypeKind::builtin || info.kind == TypeKind::pointer) {
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
        stack.push_back(TypeLayoutFrame {type, range, TypeLayoutFrameStage::enter});
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
            stack.push_back(TypeLayoutFrame {frame.type, frame.range, TypeLayoutFrameStage::finish});
            push_children(stack, frame.type, frame.range);
        }
        return results.at(type.value);
    };

    for (const auto& entry : this->checked_.structs) {
        const StructInfo& info = entry.second;
        if (!info.is_opaque) {
            const base::SourceRange range = info.fields.empty() ? base::SourceRange {} : info.fields.front().range;
            static_cast<void>(compute(info.type, range));
        }
    }

    std::unordered_set<base::u32> seen_enums;
    for (const auto& entry : this->named_types_) {
        const TypeHandle type = entry.second;
        if (is_valid(type) &&
            this->checked_.types.get(type).kind == TypeKind::enum_ &&
            seen_enums.insert(type.value).second) {
            static_cast<void>(compute(type, {}));
        }
    }
    for (const auto& entry : this->checked_.enum_cases) {
        const TypeHandle type = entry.second.type;
        if (is_valid(type) && seen_enums.insert(type.value).second) {
            static_cast<void>(compute(type, entry.second.range));
        }
    }
}

bool SemanticAnalyzer::parse_integer_literal_text(const std::string_view text, base::u64& value) const noexcept {
    return parse_u64_literal_checked(text, value);
}

bool SemanticAnalyzer::integer_literal_fits_type(const TypeHandle destination, const std::string_view text) const noexcept {
    return literal_fits_integer_type(checked_.types, destination, text);
}

bool SemanticAnalyzer::negative_integer_literal_fits_type(
    const TypeHandle destination,
    const std::string_view text
) const noexcept {
    return negative_literal_fits_integer_type(checked_.types, destination, text);
}

TypeHandle SemanticAnalyzer::analyze_integer_literal(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    const TypeHandle literal_type = checked_.types.is_integer(expected_type)
        ? expected_type
        : checked_.types.builtin(BuiltinType::i32);
    if (!integer_literal_fits_type(literal_type, expr.text)) {
        report(
            expr.range,
            "integer literal out of range for " + checked_.types.display_name(literal_type)
        );
    }
    return record_expr_type(expr_id, literal_type);
}

TypeHandle SemanticAnalyzer::analyze_float_literal(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    const TypeHandle literal_type = checked_.types.is_float(expected_type)
        ? expected_type
        : checked_.types.builtin(BuiltinType::f64);
    const bool fits = checked_.types.same(literal_type, checked_.types.builtin(BuiltinType::f32))
        ? parse_float_literal_checked<float>(expr.text)
        : parse_float_literal_checked<double>(expr.text);
    if (!fits) {
        report(
            expr.range,
            "float literal out of range for " + checked_.types.display_name(literal_type)
        );
    }
    return record_expr_type(expr_id, literal_type);
}

bool SemanticAnalyzer::is_valid_cast(const syntax::ExprKind kind, const TypeHandle dst, const TypeHandle src) const {
    if (!is_valid(dst) || !is_valid(src)) {
        return false;
    }

    if (kind == syntax::ExprKind::cast) {
        return (this->checked_.types.is_integer(dst) || this->checked_.types.is_float(dst) || this->checked_.types.is_bool(dst)) &&
               (this->checked_.types.is_integer(src) || this->checked_.types.is_float(src) || this->checked_.types.is_bool(src));
    }
    if (kind == syntax::ExprKind::pcast) {
        return this->checked_.types.is_pointer(dst) && this->checked_.types.is_pointer(src);
    }
    if (kind == syntax::ExprKind::bcast) {
        if (this->checked_.types.same(dst, src)) {
            return this->checked_.types.is_copyable(dst);
        }
        if (!this->checked_.types.is_copyable(dst) || !this->checked_.types.is_copyable(src) || this->abi_size(dst) != this->abi_size(src)) {
            return false;
        }
        if (is_builtin_scalar_bcast_type(this->checked_.types, dst) &&
            is_builtin_scalar_bcast_type(this->checked_.types, src)) {
            return true;
        }
        return this->checked_.types.is_pointer(dst) && this->checked_.types.is_pointer(src);
    }
    return false;
}

SemanticAnalyzer::TypeAbiLayout SemanticAnalyzer::abi_layout(const TypeHandle type) const {
    const auto builtin_layout = [](const BuiltinType builtin) noexcept -> TypeAbiLayout {
        switch (builtin) {
        case BuiltinType::void_: return TypeAbiLayout {SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
        case BuiltinType::bool_: return TypeAbiLayout {sizeof(bool), alignof(bool)};
        case BuiltinType::i8:
        case BuiltinType::u8: return TypeAbiLayout {sizeof(std::uint8_t), alignof(std::uint8_t)};
        case BuiltinType::i16:
        case BuiltinType::u16: return TypeAbiLayout {sizeof(std::uint16_t), alignof(std::uint16_t)};
        case BuiltinType::i32:
        case BuiltinType::u32: return TypeAbiLayout {sizeof(std::uint32_t), alignof(std::uint32_t)};
        case BuiltinType::i64:
        case BuiltinType::u64: return TypeAbiLayout {sizeof(std::uint64_t), alignof(std::uint64_t)};
        case BuiltinType::isize: return TypeAbiLayout {sizeof(std::ptrdiff_t), alignof(std::ptrdiff_t)};
        case BuiltinType::usize: return TypeAbiLayout {sizeof(std::size_t), alignof(std::size_t)};
        case BuiltinType::f32: return TypeAbiLayout {sizeof(float), alignof(float)};
        case BuiltinType::f64: return TypeAbiLayout {sizeof(double), alignof(double)};
        case BuiltinType::str: return TypeAbiLayout {sizeof(void*) + sizeof(std::size_t), alignof(void*)};
        }
        return TypeAbiLayout {SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
    };

    const auto cached = [](const std::unordered_map<base::u32, TypeAbiLayout>& layouts, const TypeHandle current) noexcept {
        const auto found = layouts.find(current.value);
        return found == layouts.end() ? TypeAbiLayout {SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT} : found->second;
    };

    const auto finish_type = [&](const TypeHandle current, const std::unordered_map<base::u32, TypeAbiLayout>& layouts) {
        const TypeInfo& info = this->checked_.types.get(current);
        switch (info.kind) {
        case TypeKind::builtin:
            return builtin_layout(info.builtin);
        case TypeKind::pointer:
            return TypeAbiLayout {sizeof(void*), alignof(void*)};
        case TypeKind::array: {
            const TypeAbiLayout element = cached(layouts, info.array_element);
            return TypeAbiLayout {
                mul_saturating(info.array_count, element.size),
                element.align,
            };
        }
        case TypeKind::enum_: {
            const TypeAbiLayout tag = cached(layouts, info.enum_underlying);
            if (!is_valid(info.enum_payload_storage)) {
                return tag;
            }
            const TypeAbiLayout storage = cached(layouts, info.enum_payload_storage);
            const base::u64 max_align = std::max(tag.align, info.enum_payload_align);
            const base::u64 storage_offset = align_forward(tag.size, info.enum_payload_align);
            return TypeAbiLayout {
                align_forward(add_saturating(storage_offset, storage.size), max_align),
                max_align,
            };
        }
        case TypeKind::struct_: {
            const StructInfo* struct_info = this->find_struct(current);
            if (struct_info == nullptr) {
                return TypeAbiLayout {SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
            }
            base::u64 offset = SEMA_ABI_INVALID_SIZE;
            base::u64 max_align = SEMA_ABI_MIN_ALIGNMENT;
            for (const StructFieldInfo& field : struct_info->fields) {
                const TypeAbiLayout field_layout = cached(layouts, field.type);
                max_align = std::max(max_align, field_layout.align);
                offset = align_forward(offset, field_layout.align);
                offset = add_saturating(offset, field_layout.size);
            }
            return TypeAbiLayout {align_forward(offset, max_align), max_align};
        }
        case TypeKind::opaque_struct:
            return TypeAbiLayout {SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
        }
        return TypeAbiLayout {SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
    };

    const auto push_dependency = [&](
        std::vector<TypeLayoutFrame>& stack,
        const TypeHandle dependency,
        std::unordered_map<base::u32, TypeLayoutVisitState>& states,
        const std::unordered_map<base::u32, TypeAbiLayout>& layouts
    ) {
        if (!is_valid(dependency) || layouts.contains(dependency.value)) {
            return;
        }
        const TypeInfo& info = this->checked_.types.get(dependency);
        if (info.kind == TypeKind::builtin || info.kind == TypeKind::pointer || info.kind == TypeKind::opaque_struct) {
            stack.push_back(TypeLayoutFrame {dependency, {}, TypeLayoutFrameStage::enter});
            return;
        }
        const auto found = states.find(dependency.value);
        if (found != states.end() && found->second == TypeLayoutVisitState::visiting) {
            return;
        }
        stack.push_back(TypeLayoutFrame {dependency, {}, TypeLayoutFrameStage::enter});
    };

    const auto push_children = [&](
        std::vector<TypeLayoutFrame>& stack,
        const TypeHandle current,
        std::unordered_map<base::u32, TypeLayoutVisitState>& states,
        const std::unordered_map<base::u32, TypeAbiLayout>& layouts
    ) {
        const TypeInfo& info = this->checked_.types.get(current);
        switch (info.kind) {
        case TypeKind::array:
            push_dependency(stack, info.array_element, states, layouts);
            break;
        case TypeKind::enum_:
            push_dependency(stack, info.enum_underlying, states, layouts);
            push_dependency(stack, info.enum_payload_storage, states, layouts);
            break;
        case TypeKind::struct_: {
            const StructInfo* struct_info = this->find_struct(current);
            if (struct_info != nullptr) {
                for (auto field = struct_info->fields.rbegin(); field != struct_info->fields.rend(); ++field) {
                    push_dependency(stack, field->type, states, layouts);
                }
            }
            break;
        }
        case TypeKind::builtin:
        case TypeKind::pointer:
        case TypeKind::opaque_struct:
            break;
        }
    };

    if (!is_valid(type)) {
        return TypeAbiLayout {SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
    }

    std::unordered_map<base::u32, TypeAbiLayout> layouts;
    std::unordered_map<base::u32, TypeLayoutVisitState> states;
    std::vector<TypeLayoutFrame> stack;
    stack.reserve(SEMA_TYPE_LAYOUT_INITIAL_STACK_CAPACITY);
    stack.push_back(TypeLayoutFrame {type, {}, TypeLayoutFrameStage::enter});
    while (!stack.empty()) {
        const TypeLayoutFrame frame = stack.back();
        stack.pop_back();
        if (!is_valid(frame.type) || layouts.contains(frame.type.value)) {
            continue;
        }
        const TypeInfo& info = this->checked_.types.get(frame.type);
        if (frame.stage == TypeLayoutFrameStage::finish) {
            layouts[frame.type.value] = finish_type(frame.type, layouts);
            states[frame.type.value] = TypeLayoutVisitState::done;
            continue;
        }
        if (info.kind == TypeKind::builtin || info.kind == TypeKind::pointer || info.kind == TypeKind::opaque_struct) {
            layouts[frame.type.value] = finish_type(frame.type, layouts);
            states[frame.type.value] = TypeLayoutVisitState::done;
            continue;
        }
        const auto state = states.find(frame.type.value);
        if (state != states.end() && state->second == TypeLayoutVisitState::visiting) {
            layouts[frame.type.value] = TypeAbiLayout {SEMA_ABI_INVALID_SIZE, SEMA_ABI_MIN_ALIGNMENT};
            states[frame.type.value] = TypeLayoutVisitState::done;
            continue;
        }
        states[frame.type.value] = TypeLayoutVisitState::visiting;
        stack.push_back(TypeLayoutFrame {frame.type, {}, TypeLayoutFrameStage::finish});
        push_children(stack, frame.type, states, layouts);
    }
    return cached(layouts, type);
}

base::u64 SemanticAnalyzer::abi_size(const TypeHandle type) const {
    return this->abi_layout(type).size;
}

base::u64 SemanticAnalyzer::abi_align(const TypeHandle type) const {
    return this->abi_layout(type).align;
}

bool SemanticAnalyzer::is_integer_literal(const syntax::ExprId expr_id) const noexcept {
    return syntax::is_valid(expr_id) &&
           expr_id.value < module_.exprs.size() &&
           module_.exprs[expr_id.value].kind == syntax::ExprKind::integer_literal;
}

bool SemanticAnalyzer::is_null_literal(const syntax::ExprId expr_id) const noexcept {
    return syntax::is_valid(expr_id) &&
           expr_id.value < module_.exprs.size() &&
           module_.exprs[expr_id.value].kind == syntax::ExprKind::null_literal;
}

bool SemanticAnalyzer::is_place_expr(const syntax::ExprId expr_id) {
    syntax::ExprId current = expr_id;
    while (true) {
        if (!syntax::is_valid(current) || current.value >= this->module_.exprs.size()) {
            return false;
        }
        const syntax::ExprNode& expr = this->module_.exprs[current.value];
        switch (expr.kind) {
        case syntax::ExprKind::name: {
            const Symbol* symbol = nullptr;
            if (!expr.scope_name.empty()) {
                const syntax::ModuleId module = this->resolve_import_alias(expr.scope_name, expr.scope_range, false);
                symbol = syntax::is_valid(module) ? this->find_symbol_in_module(module, expr.text, expr.range, false) : nullptr;
            } else {
                symbol = this->find_symbol(expr.text, expr.range);
            }
            return symbol != nullptr && (symbol->kind == SymbolKind::local || symbol->kind == SymbolKind::parameter);
        }
        case syntax::ExprKind::field:
        case syntax::ExprKind::index:
            current = expr.object;
            break;
        case syntax::ExprKind::unary:
            return expr.unary_op == syntax::UnaryOp::dereference;
        default:
            return false;
        }
    }
}

bool SemanticAnalyzer::is_writable_place(const syntax::ExprId expr_id) {
    syntax::ExprId current = expr_id;
    while (true) {
        if (!syntax::is_valid(current) || current.value >= this->module_.exprs.size()) {
            return false;
        }
        const syntax::ExprNode& expr = this->module_.exprs[current.value];
        switch (expr.kind) {
        case syntax::ExprKind::name: {
            const Symbol* symbol = nullptr;
            if (!expr.scope_name.empty()) {
                const syntax::ModuleId module = this->resolve_import_alias(expr.scope_name, expr.scope_range, false);
                symbol = syntax::is_valid(module) ? this->find_symbol_in_module(module, expr.text, expr.range, false) : nullptr;
            } else {
                symbol = this->find_symbol(expr.text, expr.range);
            }
            return symbol != nullptr && symbol->is_mutable;
        }
        case syntax::ExprKind::field:
        case syntax::ExprKind::index: {
            const TypeHandle object = this->analyze_expr(expr.object);
            if (this->checked_.types.is_pointer(object)) {
                return this->checked_.types.get(object).pointer_mutability == PointerMutability::mut;
            }
            current = expr.object;
            break;
        }
        case syntax::ExprKind::unary: {
            if (expr.unary_op != syntax::UnaryOp::dereference) {
                return false;
            }
            const TypeHandle pointer = this->analyze_expr(expr.unary_operand);
            return this->checked_.types.is_pointer(pointer) &&
                   this->checked_.types.get(pointer).pointer_mutability == PointerMutability::mut;
        }
        default:
            return false;
        }
    }
}

bool SemanticAnalyzer::is_array_containing_value_type(const TypeHandle type) const noexcept {
    return is_valid(type) && this->checked_.types.contains_array(type);
}

const StructInfo* SemanticAnalyzer::find_struct(const TypeHandle type) const noexcept {
    if (!is_valid(type)) {
        return nullptr;
    }
    if (const auto found = struct_infos_by_type_.find(type.value); found != struct_infos_by_type_.end()) {
        return found->second;
    }
    for (const auto& entry : checked_.structs) {
        if (checked_.types.same(entry.second.type, type)) {
            return &entry.second;
        }
    }
    return nullptr;
}

} // namespace aurex::sema
