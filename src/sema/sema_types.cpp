#include <aurex/sema/sema.hpp>

#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
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
constexpr char SEMA_FLOAT_LITERAL_DOT = '.';
constexpr char SEMA_FLOAT_LITERAL_EXPONENT_LOWER = 'e';
constexpr char SEMA_FLOAT_LITERAL_EXPONENT_UPPER = 'E';
constexpr char SEMA_FLOAT_LITERAL_PLUS = '+';
constexpr char SEMA_FLOAT_LITERAL_MINUS = '-';
constexpr std::string_view SEMA_INTEGER_LITERAL_SUFFIX_I8 = "i8";
constexpr std::string_view SEMA_INTEGER_LITERAL_SUFFIX_I16 = "i16";
constexpr std::string_view SEMA_INTEGER_LITERAL_SUFFIX_I32 = "i32";
constexpr std::string_view SEMA_INTEGER_LITERAL_SUFFIX_I64 = "i64";
constexpr std::string_view SEMA_INTEGER_LITERAL_SUFFIX_ISIZE = "isize";
constexpr std::string_view SEMA_INTEGER_LITERAL_SUFFIX_U8 = "u8";
constexpr std::string_view SEMA_INTEGER_LITERAL_SUFFIX_U16 = "u16";
constexpr std::string_view SEMA_INTEGER_LITERAL_SUFFIX_U32 = "u32";
constexpr std::string_view SEMA_INTEGER_LITERAL_SUFFIX_U64 = "u64";
constexpr std::string_view SEMA_INTEGER_LITERAL_SUFFIX_USIZE = "usize";
constexpr std::string_view SEMA_FLOAT_LITERAL_SUFFIX_F32 = "f32";
constexpr std::string_view SEMA_FLOAT_LITERAL_SUFFIX_F64 = "f64";

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
    build_reference,
    build_array,
    build_slice,
    build_tuple,
    build_function,
};

struct TypeResolveAction {
    TypeResolveActionKind kind = TypeResolveActionKind::resolve;
    syntax::TypeId type = syntax::INVALID_TYPE_ID;
    bool opaque_allowed_as_pointee = false;
    syntax::PointerMutability pointer_mutability = syntax::PointerMutability::const_;
    syntax::FunctionCallConv function_call_conv = syntax::FunctionCallConv::aurex;
    bool function_is_unsafe = false;
    bool function_is_variadic = false;
    std::optional<base::u64> array_count {};
    base::usize tuple_element_count = 0;
    base::usize function_param_count = 0;
};

struct IntegerLiteralParts {
    std::string_view digits;
    std::string_view suffix;
};

struct FloatLiteralParts {
    std::string_view digits;
    std::string_view suffix;
};

struct PlaceIntegerLiteralExpr {
    syntax::ExprId literal = syntax::INVALID_EXPR_ID;
    bool negated = false;
};

[[nodiscard]] PlaceIntegerLiteralExpr place_integer_literal_expr(
    const syntax::AstModule& module,
    const syntax::ExprId candidate
) noexcept {
    if (!syntax::is_valid(candidate) || candidate.value >= module.exprs.size()) {
        return {};
    }
    const syntax::ExprNode& node = module.exprs[candidate.value];
    if (node.kind == syntax::ExprKind::integer_literal) {
        return {candidate, false};
    }
    if (node.kind == syntax::ExprKind::unary &&
        node.unary_op == syntax::UnaryOp::numeric_negate &&
        syntax::is_valid(node.unary_operand) &&
        node.unary_operand.value < module.exprs.size() &&
        module.exprs[node.unary_operand.value].kind == syntax::ExprKind::integer_literal) {
        return {node.unary_operand, true};
    }
    return {};
}

[[nodiscard]] base::SourceRange place_expr_range_or(
    const syntax::AstModule& module,
    const syntax::ExprId expr,
    const base::SourceRange fallback
) noexcept {
    return syntax::is_valid(expr) && expr.value < module.exprs.size()
        ? module.exprs[expr.value].range
        : fallback;
}

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
    case syntax::PrimitiveTypeKind::char_: return BuiltinType::char_;
    }
    return BuiltinType::void_;
}

[[nodiscard]] PointerMutability map_mutability(const syntax::PointerMutability mutability) noexcept {
    return mutability == syntax::PointerMutability::mut ? PointerMutability::mut : PointerMutability::const_;
}

[[nodiscard]] FunctionCallConv map_function_call_conv(const syntax::FunctionCallConv call_conv) noexcept {
    return call_conv == syntax::FunctionCallConv::c ? FunctionCallConv::c : FunctionCallConv::aurex;
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

[[nodiscard]] bool is_decimal_digit_char(const char c) noexcept {
    return c >= SEMA_INTEGER_LITERAL_DECIMAL_FIRST_CHAR && c <= SEMA_INTEGER_LITERAL_DECIMAL_LAST_CHAR;
}

[[nodiscard]] IntegerLiteralParts split_integer_literal_text(const std::string_view text) noexcept {
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

    while (index < text.size()) {
        if (text[index] == SEMA_INTEGER_LITERAL_UNDERSCORE_CHAR) {
            ++index;
            continue;
        }
        base::u64 digit = 0;
        if (!parse_integer_literal_digit(text[index], radix, digit)) {
            break;
        }
        ++index;
    }
    return IntegerLiteralParts {text.substr(0, index), text.substr(index)};
}

[[nodiscard]] FloatLiteralParts split_float_literal_text(const std::string_view text) noexcept {
    base::usize index = 0;
    const auto consume_digits = [&]() noexcept {
        while (index < text.size() &&
               (is_decimal_digit_char(text[index]) || text[index] == SEMA_INTEGER_LITERAL_UNDERSCORE_CHAR)) {
            ++index;
        }
    };

    if (index < text.size() && text[index] == SEMA_FLOAT_LITERAL_DOT) {
        ++index;
    }
    consume_digits();
    if (index < text.size() && text[index] == SEMA_FLOAT_LITERAL_DOT) {
        ++index;
        consume_digits();
    }
    if (index < text.size() &&
        (text[index] == SEMA_FLOAT_LITERAL_EXPONENT_LOWER || text[index] == SEMA_FLOAT_LITERAL_EXPONENT_UPPER)) {
        ++index;
        if (index < text.size() &&
            (text[index] == SEMA_FLOAT_LITERAL_PLUS || text[index] == SEMA_FLOAT_LITERAL_MINUS)) {
            ++index;
        }
        consume_digits();
    }
    return FloatLiteralParts {text.substr(0, index), text.substr(index)};
}

[[nodiscard]] std::optional<BuiltinType> integer_suffix_type(const std::string_view suffix) noexcept {
    if (suffix == SEMA_INTEGER_LITERAL_SUFFIX_I8) {
        return BuiltinType::i8;
    }
    if (suffix == SEMA_INTEGER_LITERAL_SUFFIX_I16) {
        return BuiltinType::i16;
    }
    if (suffix == SEMA_INTEGER_LITERAL_SUFFIX_I32) {
        return BuiltinType::i32;
    }
    if (suffix == SEMA_INTEGER_LITERAL_SUFFIX_I64) {
        return BuiltinType::i64;
    }
    if (suffix == SEMA_INTEGER_LITERAL_SUFFIX_ISIZE) {
        return BuiltinType::isize;
    }
    if (suffix == SEMA_INTEGER_LITERAL_SUFFIX_U8) {
        return BuiltinType::u8;
    }
    if (suffix == SEMA_INTEGER_LITERAL_SUFFIX_U16) {
        return BuiltinType::u16;
    }
    if (suffix == SEMA_INTEGER_LITERAL_SUFFIX_U32) {
        return BuiltinType::u32;
    }
    if (suffix == SEMA_INTEGER_LITERAL_SUFFIX_U64) {
        return BuiltinType::u64;
    }
    if (suffix == SEMA_INTEGER_LITERAL_SUFFIX_USIZE) {
        return BuiltinType::usize;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<BuiltinType> float_suffix_type(const std::string_view suffix) noexcept {
    if (suffix == SEMA_FLOAT_LITERAL_SUFFIX_F32) {
        return BuiltinType::f32;
    }
    if (suffix == SEMA_FLOAT_LITERAL_SUFFIX_F64) {
        return BuiltinType::f64;
    }
    return std::nullopt;
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
    const FloatLiteralParts parts = split_float_literal_text(text);
    if (!parts.suffix.empty() && !float_suffix_type(parts.suffix).has_value()) {
        return false;
    }
    std::string digits;
    digits.reserve(parts.digits.size());
    for (const char c : parts.digits) {
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
    const IntegerLiteralParts parts = split_integer_literal_text(text);
    if (!parts.suffix.empty()) {
        const std::optional<BuiltinType> suffix = integer_suffix_type(parts.suffix);
        if (!suffix.has_value() || !types.same(types.builtin(*suffix), destination)) {
            return false;
        }
    }
    const TypeInfo& info = types.get(destination);
    const base::u32 bits = builtin_integer_bits(info.builtin);
    if (bits == SEMA_INTEGER_LITERAL_INVALID_BITS) {
        return false;
    }

    base::u64 value = 0;
    if (!parse_u64_literal_checked(parts.digits, value)) {
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
    const IntegerLiteralParts parts = split_integer_literal_text(text);
    if (!parts.suffix.empty()) {
        const std::optional<BuiltinType> suffix = integer_suffix_type(parts.suffix);
        if (!suffix.has_value() || !types.same(types.builtin(*suffix), destination)) {
            return false;
        }
    }
    const TypeInfo& info = types.get(destination);
    const base::u32 bits = builtin_integer_bits(info.builtin);
    if (bits == SEMA_INTEGER_LITERAL_INVALID_BITS || builtin_is_unsigned(info.builtin)) {
        return false;
    }

    base::u64 value = 0;
    if (!parse_u64_literal_checked(parts.digits, value)) {
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

[[nodiscard]] bool is_bitcast_type(const TypeTable& types, const TypeHandle type) noexcept {
    if (!is_valid(type)) {
        return false;
    }
    return is_builtin_scalar_bcast_type(types, type) || types.is_pointer(type);
}

} // namespace

TypeHandle SemanticAnalyzer::resolve_type(const syntax::TypeId type_id) {
    return this->resolve_type(type_id, false);
}

TypeHandle SemanticAnalyzer::resolve_type(const syntax::TypeId type_id, const bool opaque_allowed_as_pointee) {
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
            if (!syntax::is_valid(action.type) || action.type.value >= this->module_.types.size()) {
                values.push_back(INVALID_TYPE_HANDLE);
                break;
            }

            const TypeHandle cached = this->cached_syntax_type(action.type);
            if (is_valid(cached)) {
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
            {
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
            case syntax::TypeKind::reference:
            {
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
            case syntax::TypeKind::array:
            {
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
            case syntax::TypeKind::slice:
            {
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
            case syntax::TypeKind::tuple:
            {
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
            case syntax::TypeKind::function:
            {
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
                const TypeHandle resolved = this->resolve_named_type(
                    action.type,
                    type,
                    action.opaque_allowed_as_pointee
                );
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
        case TypeResolveActionKind::build_reference: {
            const TypeHandle pointee = values.back();
            values.pop_back();
            if (!this->is_valid_storage_type(pointee)) {
                this->report(this->module_.types[action.type.value].range, std::string(SEMA_REFERENCE_STORAGE));
            }
            const TypeHandle resolved = this->checked_.types.reference(map_mutability(action.pointer_mutability), pointee);
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
        case TypeResolveActionKind::build_slice: {
            const TypeHandle element = values.back();
            values.pop_back();
            const TypeHandle resolved = this->checked_.types.slice(map_mutability(action.pointer_mutability), element);
            this->record_syntax_type_handle(action.type, resolved);
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
                if (!this->is_valid_storage_type(element)) {
                    this->report(this->module_.types[action.type.value].range, std::string(SEMA_FIELD_STORAGE));
                }
            }
            const TypeHandle resolved = this->checked_.types.tuple(std::move(elements));
            this->record_syntax_type_handle(action.type, resolved);
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
            if (action.function_is_variadic &&
                action.function_call_conv != syntax::FunctionCallConv::c) {
                this->report(
                    this->module_.types[action.type.value].range,
                    std::string(SEMA_VARIADIC_FUNCTION_TYPE_EXTERN_C_ONLY)
                );
            }
            for (const TypeHandle param : params) {
                if (!this->is_valid_storage_type(param)) {
                    this->report(this->module_.types[action.type.value].range, std::string(SEMA_FUNCTION_TYPE_PARAMETER_STORAGE));
                }
                if (this->checked_.types.is_array(param) || this->checked_.types.contains_array(param)) {
                    this->report(this->module_.types[action.type.value].range, std::string(SEMA_ARRAY_FUNCTION_TYPE_PARAMETER_UNSUPPORTED));
                }
            }
            if (is_valid(return_type) &&
                !this->checked_.types.is_void(return_type) &&
                !this->is_valid_storage_type(return_type)) {
                this->report(this->module_.types[action.type.value].range, std::string(SEMA_FUNCTION_TYPE_RETURN_STORAGE));
            }
            if (this->checked_.types.is_array(return_type) || this->checked_.types.contains_array(return_type)) {
                this->report(this->module_.types[action.type.value].range, std::string(SEMA_ARRAY_FUNCTION_TYPE_RETURN_UNSUPPORTED));
            }
            const TypeHandle resolved = this->checked_.types.function(
                map_function_call_conv(action.function_call_conv),
                action.function_is_unsafe,
                action.function_is_variadic,
                std::move(params),
                return_type
            );
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

TypeHandle SemanticAnalyzer::resolve_named_type(
    const syntax::TypeId type_id,
    const syntax::TypeNode& type,
    const bool opaque_allowed_as_pointee
) {
    const std::vector<std::string_view> scope_parts = this->type_scope_parts(type);
    const bool qualified = !scope_parts.empty();
    syntax::ModuleId scope_module = syntax::INVALID_MODULE_ID;
    if (qualified) {
        scope_module = this->resolve_type_scope(type, true);
        if (!syntax::is_valid(scope_module)) {
            return INVALID_TYPE_HANDLE;
        }
    }
    if (!qualified && this->current_generic_context_ != nullptr) {
        if (const auto found = this->current_generic_context_->params.find(std::string(type.name));
            found != this->current_generic_context_->params.end()) {
            if (!type.type_args.empty()) {
                this->report(type.range, sema_generic_param_type_args_message(type.name));
                return INVALID_TYPE_HANDLE;
            }
            return found->second;
        }
    }

    NamedTypeSelector selector;
    selector.module = scope_module;
    selector.name = type.name;
    selector.range = type.range;
    selector.type_args = type.type_args;
    selector.qualified = qualified;
    if (!selector.type_args.empty()) {
        return this->resolve_generic_type_selector(selector, type_id, opaque_allowed_as_pointee, true);
    }
    if (qualified && this->generic_type_template_exists_in_module(scope_module, type.name)) {
        this->report_generic_type_template_in_module(scope_module, type.name, type.range);
        return INVALID_TYPE_HANDLE;
    }
    return this->resolve_named_type_selector_type(selector, opaque_allowed_as_pointee, true);
}

TypeHandle SemanticAnalyzer::resolve_type_alias(const TypeAliasInfo& alias, const bool opaque_allowed_as_pointee) {
    const std::string key = module_key(alias.module, alias.name);
    if (const auto found = resolved_type_aliases_.find(key); found != resolved_type_aliases_.end()) {
        return found->second;
    }
    if (std::find(resolving_type_aliases_.begin(), resolving_type_aliases_.end(), key) != resolving_type_aliases_.end()) {
        report(alias.range, sema_cyclic_type_alias_message(alias.name));
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
        return is_valid(dst) && this->is_null_literal(value) && this->checked_.types.is_pointer(dst);
    }
    if (this->checked_.types.get(dst).kind == TypeKind::generic_param ||
        this->checked_.types.get(src).kind == TypeKind::generic_param) {
        return this->checked_.types.same(dst, src);
    }
    if (this->checked_.types.is_integer(dst) && this->checked_.types.is_integer(src) && this->is_integer_literal(value)) {
        return syntax::is_valid(value) &&
               value.value < this->module_.exprs.size() &&
               this->integer_literal_fits_type(dst, this->module_.exprs[value.value].text);
    }
    if (this->checked_.types.is_pointer(dst) && this->checked_.types.is_pointer(src)) {
        const TypeInfo& dst_info = this->checked_.types.get(dst);
        const TypeInfo& src_info = this->checked_.types.get(src);
        if (dst_info.pointer_mutability == PointerMutability::const_ &&
            this->checked_.types.same(dst_info.pointee, src_info.pointee)) {
            return true;
        }
    }
    if (this->checked_.types.is_reference(dst) && this->checked_.types.is_reference(src)) {
        const TypeInfo& dst_info = this->checked_.types.get(dst);
        const TypeInfo& src_info = this->checked_.types.get(src);
        if (!this->checked_.types.same(dst_info.pointee, src_info.pointee)) {
            return false;
        }
        return dst_info.pointer_mutability == PointerMutability::const_ ||
               src_info.pointer_mutability == PointerMutability::mut;
    }
    if (this->checked_.types.is_slice(dst) && this->checked_.types.is_slice(src)) {
        const TypeInfo& dst_info = this->checked_.types.get(dst);
        const TypeInfo& src_info = this->checked_.types.get(src);
        if (!this->checked_.types.same(dst_info.slice_element, src_info.slice_element)) {
            return false;
        }
        return dst_info.slice_mutability == PointerMutability::const_ ||
               src_info.slice_mutability == PointerMutability::mut;
    }
    return this->checked_.types.same(dst, src);
}

bool SemanticAnalyzer::is_valid_storage_type(const TypeHandle type) const {
    std::vector<TypeHandle> pending;
    pending.push_back(type);
    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!is_valid(current)) {
            return false;
        }
        const TypeInfo& info = this->checked_.types.get(current);
        if (info.kind == TypeKind::generic_param) {
            return true;
        }
        if (this->checked_.types.is_void(current) || info.kind == TypeKind::opaque_struct) {
            return false;
        }
        if (info.kind == TypeKind::slice) {
            pending.push_back(info.slice_element);
            continue;
        }
        if (info.kind == TypeKind::reference) {
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
        const base::u64 element_size = this->abi_size(info.array_element);
        if (element_size != 0 && info.array_count > std::numeric_limits<base::u64>::max() / element_size) {
            return false;
        }
        pending.push_back(info.array_element);
    }
    return true;
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
        if (info.kind == TypeKind::builtin ||
            info.kind == TypeKind::pointer ||
            info.kind == TypeKind::reference ||
            info.kind == TypeKind::slice ||
            info.kind == TypeKind::function ||
            info.kind == TypeKind::generic_param) {
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
                this->report(range, std::string(SEMA_ARRAY_STORAGE_OVERFLOW));
                result = LayoutResult {size, element.align, false};
                return result;
            }
            result = LayoutResult {size, element.align, true};
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
                    this->report(range, std::string(SEMA_STRUCT_STORAGE_OVERFLOW));
                    result.ok = false;
                }
                base::u64 next_offset = SEMA_ABI_INVALID_SIZE;
                if (!checked_add_u64(aligned_offset, element.size, next_offset)) {
                    this->report(range, std::string(SEMA_STRUCT_STORAGE_OVERFLOW));
                    result.ok = false;
                }
                offset = next_offset;
            }
            base::u64 size = SEMA_ABI_INVALID_SIZE;
            if (!checked_align_forward(offset, max_align, size)) {
                this->report(range, std::string(SEMA_STRUCT_STORAGE_OVERFLOW));
                result.ok = false;
            }
            result.size = size;
            result.align = max_align;
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
                        this->report(field.range, std::string(SEMA_STRUCT_STORAGE_OVERFLOW));
                        result.ok = false;
                    }
                    base::u64 next_offset = SEMA_ABI_INVALID_SIZE;
                    if (!checked_add_u64(aligned_offset, field_layout.size, next_offset)) {
                        this->report(field.range, std::string(SEMA_STRUCT_STORAGE_OVERFLOW));
                        result.ok = false;
                    }
                    offset = next_offset;
                }
                base::u64 size = SEMA_ABI_INVALID_SIZE;
                if (!checked_align_forward(offset, max_align, size)) {
                    this->report(range, std::string(SEMA_STRUCT_STORAGE_OVERFLOW));
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
                    this->report(range, std::string(SEMA_ENUM_STORAGE_OVERFLOW));
                    result.ok = false;
                }
                base::u64 total = SEMA_ABI_INVALID_SIZE;
                if (!checked_add_u64(storage_offset, payload_size, total)) {
                    this->report(range, std::string(SEMA_ENUM_STORAGE_OVERFLOW));
                    result.ok = false;
                }
                base::u64 size = SEMA_ABI_INVALID_SIZE;
                if (!checked_align_forward(total, max_align, size)) {
                    this->report(range, std::string(SEMA_ENUM_STORAGE_OVERFLOW));
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
            dependency_info.kind == TypeKind::reference ||
            dependency_info.kind == TypeKind::slice ||
            dependency_info.kind == TypeKind::function ||
            dependency_info.kind == TypeKind::generic_param ||
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
        if (info.kind == TypeKind::tuple) {
            for (auto element = info.tuple_elements.rbegin(); element != info.tuple_elements.rend(); ++element) {
                push_dependency(stack, *element, range);
            }
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
        if (info.kind == TypeKind::builtin ||
            info.kind == TypeKind::pointer ||
            info.kind == TypeKind::reference ||
            info.kind == TypeKind::slice ||
            info.kind == TypeKind::function ||
            info.kind == TypeKind::generic_param) {
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
    const IntegerLiteralParts parts = split_integer_literal_text(text);
    if (!parts.suffix.empty() && !integer_suffix_type(parts.suffix).has_value()) {
        return false;
    }
    return parse_u64_literal_checked(parts.digits, value);
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
    const TypeHandle default_type = checked_.types.builtin(BuiltinType::i32);
    TypeHandle natural_type = default_type;
    TypeHandle literal_type = checked_.types.is_integer(expected_type) ? expected_type : natural_type;
    bool suffix_valid = true;
    bool has_suffix = false;
    const IntegerLiteralParts parts = split_integer_literal_text(expr.text);
    if (!parts.suffix.empty()) {
        has_suffix = true;
        const std::optional<BuiltinType> suffix = integer_suffix_type(parts.suffix);
        if (!suffix.has_value()) {
            suffix_valid = false;
            report(expr.range, sema_invalid_integer_literal_suffix_message(parts.suffix));
        } else {
            natural_type = checked_.types.builtin(*suffix);
            literal_type = natural_type;
            if (checked_.types.is_integer(expected_type) && !checked_.types.same(literal_type, expected_type)) {
                report(
                    expr.range,
                    sema_integer_literal_suffix_type_mismatch_message(
                        checked_.types.display_name(literal_type),
                        checked_.types.display_name(expected_type)
                    )
                );
            }
        }
    }
    if (suffix_valid && !integer_literal_fits_type(literal_type, expr.text)) {
        report(
            expr.range,
            sema_integer_literal_out_of_range_message(checked_.types.display_name(literal_type))
        );
    }
    if (suffix_valid &&
        !has_suffix &&
        checked_.types.is_integer(expected_type) &&
        !checked_.types.same(default_type, expected_type) &&
        integer_literal_fits_type(default_type, expr.text)) {
        this->record_coercion(expr_id, default_type, expected_type, CoercionKind::contextual_integer_literal);
    }
    return record_expr_type(expr_id, literal_type);
}

TypeHandle SemanticAnalyzer::analyze_negative_integer_literal(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    const TypeHandle default_type = checked_.types.builtin(BuiltinType::i32);
    TypeHandle natural_type = default_type;
    TypeHandle literal_type = checked_.types.is_integer(expected_type) ? expected_type : natural_type;
    bool suffix_valid = true;
    bool has_suffix = false;
    const IntegerLiteralParts parts = split_integer_literal_text(expr.text);
    if (!parts.suffix.empty()) {
        has_suffix = true;
        const std::optional<BuiltinType> suffix = integer_suffix_type(parts.suffix);
        if (!suffix.has_value()) {
            suffix_valid = false;
            report(expr.range, sema_invalid_integer_literal_suffix_message(parts.suffix));
        } else {
            natural_type = checked_.types.builtin(*suffix);
            literal_type = natural_type;
            if (checked_.types.is_integer(expected_type) && !checked_.types.same(literal_type, expected_type)) {
                report(
                    expr.range,
                    sema_integer_literal_suffix_type_mismatch_message(
                        checked_.types.display_name(literal_type),
                        checked_.types.display_name(expected_type)
                    )
                );
            }
        }
    }
    if (suffix_valid && !negative_integer_literal_fits_type(literal_type, expr.text)) {
        report(
            expr.range,
            sema_integer_literal_out_of_range_message(checked_.types.display_name(literal_type))
        );
    }
    if (suffix_valid &&
        !has_suffix &&
        checked_.types.is_integer(expected_type) &&
        !checked_.types.same(default_type, expected_type) &&
        negative_integer_literal_fits_type(default_type, expr.text)) {
        this->record_coercion(expr_id, default_type, expected_type, CoercionKind::contextual_integer_literal);
    }
    return record_expr_type(expr_id, literal_type);
}

TypeHandle SemanticAnalyzer::analyze_float_literal(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    const TypeHandle default_type = checked_.types.builtin(BuiltinType::f64);
    TypeHandle natural_type = default_type;
    TypeHandle literal_type = checked_.types.is_float(expected_type) ? expected_type : natural_type;
    bool suffix_valid = true;
    bool has_suffix = false;
    const FloatLiteralParts parts = split_float_literal_text(expr.text);
    if (!parts.suffix.empty()) {
        has_suffix = true;
        const std::optional<BuiltinType> suffix = float_suffix_type(parts.suffix);
        if (!suffix.has_value()) {
            suffix_valid = false;
            report(expr.range, sema_invalid_float_literal_suffix_message(parts.suffix));
        } else {
            natural_type = checked_.types.builtin(*suffix);
            literal_type = natural_type;
            if (checked_.types.is_float(expected_type) && !checked_.types.same(literal_type, expected_type)) {
                report(
                    expr.range,
                    sema_float_literal_suffix_type_mismatch_message(
                        checked_.types.display_name(literal_type),
                        checked_.types.display_name(expected_type)
                    )
                );
            }
        }
    }
    const bool fits = !suffix_valid ||
        (checked_.types.same(literal_type, checked_.types.builtin(BuiltinType::f32))
        ? parse_float_literal_checked<float>(expr.text)
        : parse_float_literal_checked<double>(expr.text));
    if (suffix_valid && !fits) {
        report(
            expr.range,
            sema_float_literal_out_of_range_message(checked_.types.display_name(literal_type))
        );
    }
    if (suffix_valid &&
        !has_suffix &&
        checked_.types.is_float(expected_type) &&
        !checked_.types.same(default_type, expected_type) &&
        parse_float_literal_checked<double>(expr.text)) {
        this->record_coercion(expr_id, default_type, expected_type, CoercionKind::contextual_float_literal);
    }
    return record_expr_type(expr_id, literal_type);
}

bool SemanticAnalyzer::is_valid_cast(const syntax::ExprKind kind, const TypeHandle dst, const TypeHandle src) const {
    if (!is_valid(dst) || !is_valid(src)) {
        return false;
    }
    if (this->checked_.types.get(dst).kind == TypeKind::generic_param ||
        this->checked_.types.get(src).kind == TypeKind::generic_param) {
        return false;
    }

    if (kind == syntax::ExprKind::cast) {
        return (this->checked_.types.is_integer(dst) || this->checked_.types.is_float(dst) || this->checked_.types.is_bool(dst)) &&
               (this->checked_.types.is_integer(src) || this->checked_.types.is_float(src) || this->checked_.types.is_bool(src));
    }
    if (kind == syntax::ExprKind::pcast) {
        return this->checked_.types.is_pointer(dst) &&
               (this->checked_.types.is_pointer(src) || this->checked_.types.is_reference(src));
    }
    if (kind == syntax::ExprKind::bcast) {
        if (this->checked_.types.same(dst, src)) {
            return is_bitcast_type(this->checked_.types, dst);
        }
        if (!is_bitcast_type(this->checked_.types, dst) ||
            !is_bitcast_type(this->checked_.types, src) ||
            this->abi_size(dst) != this->abi_size(src)) {
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
        case BuiltinType::char_: return TypeAbiLayout {sizeof(std::uint32_t), alignof(std::uint32_t)};
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
        case TypeKind::reference:
            return TypeAbiLayout {sizeof(void*), alignof(void*)};
        case TypeKind::function:
            return TypeAbiLayout {sizeof(void*), alignof(void*)};
        case TypeKind::slice:
            return TypeAbiLayout {sizeof(void*) + sizeof(std::size_t), alignof(void*)};
        case TypeKind::array: {
            const TypeAbiLayout element = cached(layouts, info.array_element);
            return TypeAbiLayout {
                mul_saturating(info.array_count, element.size),
                element.align,
            };
        }
        case TypeKind::tuple: {
            base::u64 offset = SEMA_ABI_INVALID_SIZE;
            base::u64 max_align = SEMA_ABI_MIN_ALIGNMENT;
            for (const TypeHandle element_type : info.tuple_elements) {
                const TypeAbiLayout element = cached(layouts, element_type);
                max_align = std::max(max_align, element.align);
                offset = align_forward(offset, element.align);
                offset = add_saturating(offset, element.size);
            }
            return TypeAbiLayout {align_forward(offset, max_align), max_align};
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
        case TypeKind::generic_param:
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
        if (info.kind == TypeKind::builtin ||
            info.kind == TypeKind::pointer ||
            info.kind == TypeKind::reference ||
            info.kind == TypeKind::slice ||
            info.kind == TypeKind::function ||
            info.kind == TypeKind::generic_param ||
            info.kind == TypeKind::opaque_struct) {
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
        case TypeKind::reference:
        case TypeKind::slice:
        case TypeKind::function:
        case TypeKind::generic_param:
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
        if (info.kind == TypeKind::builtin ||
            info.kind == TypeKind::pointer ||
            info.kind == TypeKind::reference ||
            info.kind == TypeKind::slice ||
            info.kind == TypeKind::function ||
            info.kind == TypeKind::generic_param ||
            info.kind == TypeKind::opaque_struct) {
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

SemanticAnalyzer::PlaceInfo SemanticAnalyzer::analyze_place_info(
    const syntax::ExprId expr_id,
    const bool emit_diagnostics
) {
    enum class PendingProjectionKind {
        deref,
        field,
        index,
    };

    struct PendingProjection {
        PendingProjectionKind kind = PendingProjectionKind::field;
        syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    };

    syntax::ExprId root = expr_id;
    if (syntax::is_valid(root) &&
        root.value < this->module_.exprs.size() &&
        this->module_.exprs[root.value].kind == syntax::ExprKind::postfix_chain) {
        root = this->materialize_postfix_chain(root);
    }

    std::vector<PendingProjection> pending;
    syntax::ExprId current = root;
    while (syntax::is_valid(current) && current.value < this->module_.exprs.size()) {
        const syntax::ExprNode& expr = this->module_.exprs[current.value];
        if (expr.kind == syntax::ExprKind::field) {
            pending.push_back(PendingProjection {PendingProjectionKind::field, current});
            current = expr.object;
            continue;
        }
        if (expr.kind == syntax::ExprKind::index) {
            pending.push_back(PendingProjection {PendingProjectionKind::index, current});
            current = expr.object;
            continue;
        }
        if (expr.kind == syntax::ExprKind::unary &&
            expr.unary_op == syntax::UnaryOp::dereference) {
            pending.push_back(PendingProjection {PendingProjectionKind::deref, current});
            current = expr.unary_operand;
            continue;
        }
        break;
    }

    PlaceInfo place;
    if (!syntax::is_valid(current) || current.value >= this->module_.exprs.size()) {
        return place;
    }

    const syntax::ExprNode& base_expr = this->module_.exprs[current.value];
    if (base_expr.kind == syntax::ExprKind::name && base_expr.scope_name.empty()) {
        const Symbol* symbol = this->symbols_.find(base_expr.text);
        if (symbol == nullptr) {
            const auto global = this->global_values_.find(this->module_key(this->current_module_, base_expr.text));
            if (global != this->global_values_.end()) {
                symbol = &global->second;
            }
        }
        if (symbol != nullptr) {
            place.type = symbol->type;
            place.is_place = symbol->kind == SymbolKind::local || symbol->kind == SymbolKind::parameter;
            place.is_writable = symbol->is_mutable;
            this->record_expr_c_name(current, symbol->c_name);
            static_cast<void>(this->record_expr_type(current, symbol->type));
            this->record_expr_expected_type(current, INVALID_TYPE_HANDLE);
        } else {
            place.type = this->analyze_expr(current);
        }
    } else {
        place.type = this->analyze_expr(current);
    }

    for (auto projection = pending.rbegin(); projection != pending.rend(); ++projection) {
        if (!syntax::is_valid(projection->expr) ||
            projection->expr.value >= this->module_.exprs.size()) {
            place.type = INVALID_TYPE_HANDLE;
            place.is_place = false;
            place.is_writable = false;
            continue;
        }

        const syntax::ExprNode& projection_expr = this->module_.exprs[projection->expr.value];
        const TypeHandle input_type = place.type;
        TypeHandle output_type = INVALID_TYPE_HANDLE;
        bool output_is_place = false;
        bool output_is_writable = false;

        if (projection->kind == PendingProjectionKind::deref) {
            if (this->checked_.types.is_pointer(input_type)) {
                const TypeInfo& pointer = this->checked_.types.get(input_type);
                output_type = pointer.pointee;
                output_is_place = true;
                output_is_writable = pointer.pointer_mutability == PointerMutability::mut;
                place.crosses_raw_pointer = true;
            } else if (this->checked_.types.is_reference(input_type)) {
                const TypeInfo& reference = this->checked_.types.get(input_type);
                output_type = reference.pointee;
                output_is_place = true;
                output_is_writable = reference.pointer_mutability == PointerMutability::mut;
            } else if (emit_diagnostics) {
                this->report(projection_expr.range, std::string(SEMA_DEREF_POINTER));
            }
        } else if (projection->kind == PendingProjectionKind::field) {
            TypeHandle object_type = input_type;
            bool projection_is_indirect = false;
            output_is_writable = place.is_writable;
            if (this->checked_.types.is_pointer(object_type)) {
                const TypeInfo& pointer = this->checked_.types.get(object_type);
                projection_is_indirect = true;
                output_is_writable = pointer.pointer_mutability == PointerMutability::mut;
                object_type = pointer.pointee;
                place.crosses_raw_pointer = true;
            } else if (this->checked_.types.is_reference(object_type)) {
                const TypeInfo& reference = this->checked_.types.get(object_type);
                projection_is_indirect = true;
                output_is_writable = reference.pointer_mutability == PointerMutability::mut;
                object_type = reference.pointee;
            }
            if (this->checked_.types.is_tuple(object_type)) {
                if (emit_diagnostics) {
                    this->report(projection_expr.range, std::string(SEMA_TUPLE_FIELD_ACCESS_UNSUPPORTED));
                }
            } else if (const StructInfo* info = this->find_struct(object_type); info != nullptr && !info->is_opaque) {
                bool saw_field = false;
                for (const StructFieldInfo& field : info->fields) {
                    if (field.name != projection_expr.field_name) {
                        continue;
                    }
                    saw_field = true;
                    if (!this->can_access(info->module, field.visibility)) {
                        if (emit_diagnostics) {
                            this->report(projection_expr.range, sema_private_field_message(projection_expr.field_name));
                        }
                    } else {
                        output_type = field.type;
                        output_is_place = projection_is_indirect || place.is_place;
                    }
                    break;
                }
                if (!saw_field && emit_diagnostics) {
                    this->report(projection_expr.range, sema_unknown_field_message(projection_expr.field_name));
                }
            } else if (emit_diagnostics) {
                this->report(projection_expr.range, std::string(SEMA_FIELD_STRUCT_VALUE));
            }
        } else {
            const TypeHandle index_type = this->analyze_expr(projection_expr.index);
            if (emit_diagnostics && !this->checked_.types.is_integer(index_type)) {
                this->report(
                    projection_expr.index.value < this->module_.exprs.size()
                        ? this->module_.exprs[projection_expr.index.value].range
                        : projection_expr.range,
                    std::string(SEMA_ARRAY_INDEX_INTEGER)
                );
            }
            if (this->checked_.types.is_array(input_type)) {
                const PlaceIntegerLiteralExpr literal_index =
                    place_integer_literal_expr(this->module_, projection_expr.index);
                if (emit_diagnostics && syntax::is_valid(literal_index.literal)) {
                    base::u64 index_value = 0;
                    const syntax::ExprNode& literal_expr = this->module_.exprs[literal_index.literal.value];
                    if (this->parse_integer_literal_text(literal_expr.text, index_value)) {
                        const bool out_of_bounds =
                            (literal_index.negated && index_value != 0) ||
                            (!literal_index.negated && index_value >= this->checked_.types.get(input_type).array_count);
                        if (out_of_bounds) {
                            this->report(
                                place_expr_range_or(this->module_, projection_expr.index, literal_expr.range),
                                std::string(SEMA_ARRAY_INDEX_OUT_OF_BOUNDS)
                            );
                        }
                    }
                }
                output_type = this->checked_.types.get(input_type).array_element;
                output_is_place = place.is_place;
                output_is_writable = place.is_writable;
            } else if (this->checked_.types.is_slice(input_type)) {
                const TypeInfo& slice = this->checked_.types.get(input_type);
                output_type = slice.slice_element;
                output_is_place = place.is_place;
                output_is_writable = slice.slice_mutability == PointerMutability::mut;
            } else if (this->checked_.types.is_pointer(input_type)) {
                const TypeInfo& pointer = this->checked_.types.get(input_type);
                place.crosses_raw_pointer = true;
                if (this->checked_.types.is_array(pointer.pointee)) {
                    if (emit_diagnostics) {
                        this->report(projection_expr.range, std::string(SEMA_INDEX_POINTER_ARRAY_DEREF));
                    }
                } else if (!this->is_valid_storage_type(pointer.pointee)) {
                    if (emit_diagnostics) {
                        this->report(projection_expr.range, std::string(SEMA_INDEX_POINTER_STORAGE));
                    }
                } else {
                    output_type = pointer.pointee;
                    output_is_place = true;
                    output_is_writable = pointer.pointer_mutability == PointerMutability::mut;
                }
            } else if (this->checked_.types.is_reference(input_type)) {
                const TypeInfo& reference = this->checked_.types.get(input_type);
                if (this->checked_.types.is_array(reference.pointee)) {
                    output_type = this->checked_.types.get(reference.pointee).array_element;
                    output_is_place = true;
                    output_is_writable = reference.pointer_mutability == PointerMutability::mut;
                } else if (this->checked_.types.is_slice(reference.pointee)) {
                    const TypeInfo& slice = this->checked_.types.get(reference.pointee);
                    output_type = slice.slice_element;
                    output_is_place = true;
                    output_is_writable = slice.slice_mutability == PointerMutability::mut;
                } else if (emit_diagnostics) {
                    this->report(projection_expr.range, std::string(SEMA_INDEX_ARRAY_OR_POINTER));
                }
            } else if (emit_diagnostics) {
                this->report(projection_expr.range, std::string(SEMA_INDEX_ARRAY_OR_POINTER));
            }
        }

        place.type = output_type;
        place.is_place = is_valid(output_type) && output_is_place;
        place.is_writable = is_valid(output_type) && output_is_writable;
        if (syntax::is_valid(projection->expr)) {
            static_cast<void>(this->record_expr_type(projection->expr, output_type));
        }
    }

    return place;
}

void SemanticAnalyzer::require_place_projection_safety(
    const PlaceInfo& place,
    const base::SourceRange range
) {
    if (place.crosses_raw_pointer) {
        this->require_unsafe_context(range, SEMA_UNSAFE_RAW_POINTER_PROJECTION);
    }
}

bool SemanticAnalyzer::is_place_expr(const syntax::ExprId expr_id) {
    return this->analyze_place_info(expr_id, false).is_place;
}

bool SemanticAnalyzer::is_writable_place(const syntax::ExprId expr_id) {
    return this->analyze_place_info(expr_id, false).is_writable;
}

bool SemanticAnalyzer::is_array_containing_value_type(const TypeHandle type) const noexcept {
    return is_valid(type) && this->checked_.types.contains_array(type);
}

const StructInfo* SemanticAnalyzer::find_struct(const TypeHandle type) const noexcept {
    if (!is_valid(type)) {
        return nullptr;
    }
    if (const auto found = struct_infos_by_type_.find(type.value); found != struct_infos_by_type_.end()) {
        if (found->second != nullptr && this->checked_.types.same(found->second->type, type)) {
            return found->second;
        }
        return nullptr;
    }
    for (const auto& entry : this->checked_.structs) {
        if (this->checked_.types.same(entry.second.type, type)) {
            return &entry.second;
        }
    }
    return nullptr;
}

} // namespace aurex::sema
