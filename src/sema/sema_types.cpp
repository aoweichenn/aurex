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
#include <vector>

#include <sema/internal/sema_core.hpp>
#include <sema/internal/sema_type_services.hpp>

namespace aurex::sema {

namespace {

constexpr int SEMA_INTEGER_LITERAL_DECIMAL_BASE = 10;
constexpr int SEMA_INTEGER_LITERAL_HEX_BASE = 16;
constexpr int SEMA_INTEGER_LITERAL_BINARY_BASE = 2;
constexpr base::usize SEMA_INTEGER_LITERAL_PREFIX_LENGTH = 2;
constexpr base::u64 SEMA_INTEGER_LITERAL_INITIAL_VALUE = 0;
constexpr base::u64 SEMA_INTEGER_LITERAL_INITIAL_DIGIT = 0;
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
    const syntax::AstModule& module, const syntax::ExprId candidate) noexcept
{
    if (!syntax::is_valid(candidate) || candidate.value >= module.exprs.size()) {
        return {};
    }
    const syntax::ExprKind kind = module.exprs.kind(candidate.value);
    if (kind == syntax::ExprKind::integer_literal) {
        return {candidate, false};
    }
    const syntax::UnaryExprPayload* const unary = module.exprs.unary_payload(candidate.value);
    if (kind == syntax::ExprKind::unary && unary != nullptr && unary->op == syntax::UnaryOp::numeric_negate
        && syntax::is_valid(unary->operand) && unary->operand.value < module.exprs.size()
        && module.exprs.kind(unary->operand.value) == syntax::ExprKind::integer_literal) {
        return {unary->operand, true};
    }
    return {};
}

[[nodiscard]] base::SourceRange place_expr_range_or(
    const syntax::AstModule& module, const syntax::ExprId expr, const base::SourceRange& fallback) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size() ? module.exprs.range(expr.value) : fallback;
}

[[nodiscard]] bool builtin_is_unsigned(const BuiltinType type) noexcept
{
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

[[nodiscard]] base::u32 builtin_integer_bits(const BuiltinType type) noexcept
{
    switch (type) {
        case BuiltinType::i8:
        case BuiltinType::u8:
            return std::numeric_limits<std::uint8_t>::digits;
        case BuiltinType::i16:
        case BuiltinType::u16:
            return std::numeric_limits<std::uint16_t>::digits;
        case BuiltinType::i32:
        case BuiltinType::u32:
            return std::numeric_limits<std::uint32_t>::digits;
        case BuiltinType::i64:
        case BuiltinType::u64:
            return std::numeric_limits<std::uint64_t>::digits;
        case BuiltinType::isize:
            return std::numeric_limits<std::ptrdiff_t>::digits;
        case BuiltinType::usize:
            return std::numeric_limits<std::size_t>::digits;
        default:
            return SEMA_INTEGER_LITERAL_INVALID_BITS;
    }
}

[[nodiscard]] bool parse_integer_literal_digit(const char c, const int radix, base::u64& digit) noexcept
{
    if (c >= SEMA_INTEGER_LITERAL_DECIMAL_FIRST_CHAR && c <= SEMA_INTEGER_LITERAL_DECIMAL_LAST_CHAR) {
        digit = static_cast<base::u64>(c - SEMA_INTEGER_LITERAL_DECIMAL_FIRST_CHAR);
        return digit < static_cast<base::u64>(radix);
    }
    if (c >= SEMA_INTEGER_LITERAL_HEX_LOWER_FIRST_CHAR && c <= SEMA_INTEGER_LITERAL_HEX_LOWER_LAST_CHAR) {
        digit = static_cast<base::u64>(SEMA_INTEGER_LITERAL_HEX_DIGIT_OFFSET
            + static_cast<base::u64>(c - SEMA_INTEGER_LITERAL_HEX_LOWER_FIRST_CHAR));
        return digit < static_cast<base::u64>(radix);
    }
    if (c >= SEMA_INTEGER_LITERAL_HEX_UPPER_FIRST_CHAR && c <= SEMA_INTEGER_LITERAL_HEX_UPPER_LAST_CHAR) {
        digit = static_cast<base::u64>(SEMA_INTEGER_LITERAL_HEX_DIGIT_OFFSET
            + static_cast<base::u64>(c - SEMA_INTEGER_LITERAL_HEX_UPPER_FIRST_CHAR));
        return digit < static_cast<base::u64>(radix);
    }
    return false;
}

[[nodiscard]] bool is_decimal_digit_char(const char c) noexcept
{
    return c >= SEMA_INTEGER_LITERAL_DECIMAL_FIRST_CHAR && c <= SEMA_INTEGER_LITERAL_DECIMAL_LAST_CHAR;
}

[[nodiscard]] IntegerLiteralParts split_integer_literal_text(const std::string_view text) noexcept
{
    int radix = SEMA_INTEGER_LITERAL_DECIMAL_BASE;
    base::usize index = 0;
    if (text.size() > SEMA_INTEGER_LITERAL_PREFIX_LENGTH && text[0] == SEMA_INTEGER_LITERAL_ZERO_CHAR) {
        if (text[1] == SEMA_INTEGER_LITERAL_HEX_LOWER_PREFIX_CHAR
            || text[1] == SEMA_INTEGER_LITERAL_HEX_UPPER_PREFIX_CHAR) {
            radix = SEMA_INTEGER_LITERAL_HEX_BASE;
            index = SEMA_INTEGER_LITERAL_PREFIX_LENGTH;
        } else if (text[1] == SEMA_INTEGER_LITERAL_BINARY_LOWER_PREFIX_CHAR
            || text[1] == SEMA_INTEGER_LITERAL_BINARY_UPPER_PREFIX_CHAR) {
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
    return IntegerLiteralParts{text.substr(0, index), text.substr(index)};
}

[[nodiscard]] FloatLiteralParts split_float_literal_text(const std::string_view text) noexcept
{
    base::usize index = 0;
    const auto consume_digits = [&]() noexcept {
        while (index < text.size()
            && (is_decimal_digit_char(text[index]) || text[index] == SEMA_INTEGER_LITERAL_UNDERSCORE_CHAR)) {
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
    if (index < text.size()
        && (text[index] == SEMA_FLOAT_LITERAL_EXPONENT_LOWER || text[index] == SEMA_FLOAT_LITERAL_EXPONENT_UPPER)) {
        ++index;
        if (index < text.size()
            && (text[index] == SEMA_FLOAT_LITERAL_PLUS || text[index] == SEMA_FLOAT_LITERAL_MINUS)) {
            ++index;
        }
        consume_digits();
    }
    return FloatLiteralParts{text.substr(0, index), text.substr(index)};
}

[[nodiscard]] std::optional<BuiltinType> integer_suffix_type(const std::string_view suffix) noexcept
{
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

[[nodiscard]] std::optional<BuiltinType> float_suffix_type(const std::string_view suffix) noexcept
{
    if (suffix == SEMA_FLOAT_LITERAL_SUFFIX_F32) {
        return BuiltinType::f32;
    }
    if (suffix == SEMA_FLOAT_LITERAL_SUFFIX_F64) {
        return BuiltinType::f64;
    }
    return std::nullopt;
}

[[nodiscard]] bool parse_u64_literal_checked(const std::string_view text, base::u64& value) noexcept
{
    int radix = SEMA_INTEGER_LITERAL_DECIMAL_BASE;
    base::usize index = 0;
    if (text.size() > SEMA_INTEGER_LITERAL_PREFIX_LENGTH && text[0] == SEMA_INTEGER_LITERAL_ZERO_CHAR) {
        if (text[1] == SEMA_INTEGER_LITERAL_HEX_LOWER_PREFIX_CHAR
            || text[1] == SEMA_INTEGER_LITERAL_HEX_UPPER_PREFIX_CHAR) {
            radix = SEMA_INTEGER_LITERAL_HEX_BASE;
            index = SEMA_INTEGER_LITERAL_PREFIX_LENGTH;
        } else if (text[1] == SEMA_INTEGER_LITERAL_BINARY_LOWER_PREFIX_CHAR
            || text[1] == SEMA_INTEGER_LITERAL_BINARY_UPPER_PREFIX_CHAR) {
            radix = SEMA_INTEGER_LITERAL_BINARY_BASE;
            index = SEMA_INTEGER_LITERAL_PREFIX_LENGTH;
        }
    }

    value = SEMA_INTEGER_LITERAL_INITIAL_VALUE;
    bool saw_digit = false;
    for (; index < text.size(); ++index) {
        const char c = text[index];
        if (c == SEMA_INTEGER_LITERAL_UNDERSCORE_CHAR) {
            continue;
        }

        base::u64 digit = SEMA_INTEGER_LITERAL_INITIAL_DIGIT;
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
[[nodiscard]] bool parse_float_literal_checked(const std::string_view text) noexcept
{
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
    Float value{};
    const char* const begin = digits.data();
    const char* const end = digits.data() + digits.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end && std::isfinite(value);
}

[[nodiscard]] bool literal_fits_integer_type(
    const TypeTable& types, const TypeHandle destination, const std::string_view text) noexcept
{
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
        return value <= ((base::u64{1} << bits) - 1);
    }
    if (bits >= SEMA_INTEGER_LITERAL_MAX_BITS) {
        return value <= static_cast<base::u64>(std::numeric_limits<std::int64_t>::max());
    }
    return value <= ((base::u64{1} << (bits - 1)) - 1);
}

[[nodiscard]] bool negative_literal_fits_integer_type(
    const TypeTable& types, const TypeHandle destination, const std::string_view text) noexcept
{
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
        return value <= (base::u64{1} << SEMA_INTEGER_LITERAL_SIGN_BIT_SHIFT);
    }
    return value <= (base::u64{1} << (bits - 1));
}

} // namespace

SemanticTypeResolver SemanticAnalyzerCore::type_resolver() noexcept
{
    return SemanticTypeResolver(*this);
}

SemanticTypeValidator SemanticAnalyzerCore::type_validator() const noexcept
{
    return SemanticTypeValidator(*this);
}

SemanticAbiChecker SemanticAnalyzerCore::abi_checker() const noexcept
{
    return SemanticAbiChecker(*this);
}

TypeHandle SemanticAnalyzerCore::resolve_type(const syntax::TypeId type_id)
{
    return this->type_resolver().resolve_type(type_id);
}

TypeHandle SemanticAnalyzerCore::resolve_type(const syntax::TypeId type_id, const bool opaque_allowed_as_pointee)
{
    return this->type_resolver().resolve_type(type_id, opaque_allowed_as_pointee);
}

TypeHandle SemanticAnalyzerCore::resolve_named_type(
    const syntax::TypeId type_id, const syntax::TypeNode& type, const bool opaque_allowed_as_pointee)
{
    return this->type_resolver().resolve_named_type(type_id, type, opaque_allowed_as_pointee);
}

TypeHandle SemanticAnalyzerCore::resolve_type_alias(const TypeAliasInfo& alias, const bool opaque_allowed_as_pointee)
{
    return this->type_resolver().resolve_type_alias(alias, opaque_allowed_as_pointee);
}

bool SemanticAnalyzerCore::can_assign(
    const TypeHandle dst, const TypeHandle src, const syntax::ExprId value) const noexcept
{
    return this->type_validator().can_assign(dst, src, value);
}

bool SemanticAnalyzerCore::is_valid_storage_type(const TypeHandle type) const
{
    return this->type_validator().is_valid_storage_type(type);
}

bool SemanticAnalyzerCore::check_m2_value_abi(
    const TypeHandle type, const ValueAbiContext context, const base::SourceRange& range) const
{
    return this->type_validator().check_m2_value_abi(type, context, range);
}

void SemanticAnalyzerCore::validate_type_layouts()
{
    this->abi_checker().validate_type_layouts();
}

bool SemanticAnalyzerCore::parse_integer_literal_text(const std::string_view text, base::u64& value) const noexcept
{
    const IntegerLiteralParts parts = split_integer_literal_text(text);
    if (!parts.suffix.empty() && !integer_suffix_type(parts.suffix).has_value()) {
        return false;
    }
    return parse_u64_literal_checked(parts.digits, value);
}

bool SemanticAnalyzerCore::integer_literal_fits_type(
    const TypeHandle destination, const std::string_view text) const noexcept
{
    return literal_fits_integer_type(this->state_.checked.types, destination, text);
}

bool SemanticAnalyzerCore::negative_integer_literal_fits_type(
    const TypeHandle destination, const std::string_view text) const noexcept
{
    return negative_literal_fits_integer_type(this->state_.checked.types, destination, text);
}

TypeHandle SemanticAnalyzerCore::analyze_integer_literal(const syntax::ExprId expr_id, const std::string_view text,
    const base::SourceRange& range, const TypeHandle expected_type)
{
    const TypeHandle default_type = this->state_.checked.types.builtin(BuiltinType::i32);
    TypeHandle natural_type = default_type;
    TypeHandle literal_type = this->state_.checked.types.is_integer(expected_type) ? expected_type : natural_type;
    bool suffix_valid = true;
    bool has_suffix = false;
    const IntegerLiteralParts parts = split_integer_literal_text(text);
    if (!parts.suffix.empty()) {
        has_suffix = true;
        const std::optional<BuiltinType> suffix = integer_suffix_type(parts.suffix);
        if (!suffix.has_value()) {
            suffix_valid = false;
            this->report_general(range, sema_invalid_integer_literal_suffix_message(parts.suffix));
        } else {
            natural_type = this->state_.checked.types.builtin(*suffix);
            literal_type = natural_type;
            if (this->state_.checked.types.is_integer(expected_type)
                && !this->state_.checked.types.same(literal_type, expected_type)) {
                this->report_type(range,
                    sema_integer_literal_suffix_type_mismatch_message(
                        this->state_.checked.types.display_name(literal_type),
                        this->state_.checked.types.display_name(expected_type)));
            }
        }
    }
    if (suffix_valid && !integer_literal_fits_type(literal_type, text)) {
        this->report_general(
            range, sema_integer_literal_out_of_range_message(this->state_.checked.types.display_name(literal_type)));
    }
    if (suffix_valid && !has_suffix && this->state_.checked.types.is_integer(expected_type)
        && !this->state_.checked.types.same(default_type, expected_type)
        && integer_literal_fits_type(default_type, text)) {
        this->record_coercion(expr_id, default_type, expected_type, CoercionKind::contextual_integer_literal);
    }
    return this->record_expr_types(expr_id, natural_type, literal_type);
}

TypeHandle SemanticAnalyzerCore::analyze_negative_integer_literal(const syntax::ExprId expr_id,
    const std::string_view text, const base::SourceRange& range, const TypeHandle expected_type)
{
    const TypeHandle default_type = this->state_.checked.types.builtin(BuiltinType::i32);
    TypeHandle natural_type = default_type;
    TypeHandle literal_type = this->state_.checked.types.is_integer(expected_type) ? expected_type : natural_type;
    bool suffix_valid = true;
    bool has_suffix = false;
    const IntegerLiteralParts parts = split_integer_literal_text(text);
    if (!parts.suffix.empty()) {
        has_suffix = true;
        const std::optional<BuiltinType> suffix = integer_suffix_type(parts.suffix);
        if (!suffix.has_value()) {
            suffix_valid = false;
            this->report_general(range, sema_invalid_integer_literal_suffix_message(parts.suffix));
        } else {
            natural_type = this->state_.checked.types.builtin(*suffix);
            literal_type = natural_type;
            if (this->state_.checked.types.is_integer(expected_type)
                && !this->state_.checked.types.same(literal_type, expected_type)) {
                this->report_type(range,
                    sema_integer_literal_suffix_type_mismatch_message(
                        this->state_.checked.types.display_name(literal_type),
                        this->state_.checked.types.display_name(expected_type)));
            }
        }
    }
    if (suffix_valid && !negative_integer_literal_fits_type(literal_type, text)) {
        this->report_general(
            range, sema_integer_literal_out_of_range_message(this->state_.checked.types.display_name(literal_type)));
    }
    if (suffix_valid && !has_suffix && this->state_.checked.types.is_integer(expected_type)
        && !this->state_.checked.types.same(default_type, expected_type)
        && negative_integer_literal_fits_type(default_type, text)) {
        this->record_coercion(expr_id, default_type, expected_type, CoercionKind::contextual_integer_literal);
    }
    return this->record_expr_types(expr_id, natural_type, literal_type);
}

TypeHandle SemanticAnalyzerCore::analyze_float_literal(const syntax::ExprId expr_id, const std::string_view text,
    const base::SourceRange& range, const TypeHandle expected_type)
{
    const TypeHandle default_type = this->state_.checked.types.builtin(BuiltinType::f64);
    TypeHandle natural_type = default_type;
    TypeHandle literal_type = this->state_.checked.types.is_float(expected_type) ? expected_type : natural_type;
    bool suffix_valid = true;
    bool has_suffix = false;
    const FloatLiteralParts parts = split_float_literal_text(text);
    if (!parts.suffix.empty()) {
        has_suffix = true;
        const std::optional<BuiltinType> suffix = float_suffix_type(parts.suffix);
        if (!suffix.has_value()) {
            suffix_valid = false;
            this->report_general(range, sema_invalid_float_literal_suffix_message(parts.suffix));
        } else {
            natural_type = this->state_.checked.types.builtin(*suffix);
            literal_type = natural_type;
            if (this->state_.checked.types.is_float(expected_type)
                && !this->state_.checked.types.same(literal_type, expected_type)) {
                this->report_type(range,
                    sema_float_literal_suffix_type_mismatch_message(
                        this->state_.checked.types.display_name(literal_type),
                        this->state_.checked.types.display_name(expected_type)));
            }
        }
    }
    const bool fits = !suffix_valid
        || (this->state_.checked.types.same(literal_type, this->state_.checked.types.builtin(BuiltinType::f32))
                ? parse_float_literal_checked<float>(text)
                : parse_float_literal_checked<double>(text));
    if (suffix_valid && !fits) {
        this->report_general(
            range, sema_float_literal_out_of_range_message(this->state_.checked.types.display_name(literal_type)));
    }
    if (suffix_valid && !has_suffix && this->state_.checked.types.is_float(expected_type)
        && !this->state_.checked.types.same(default_type, expected_type) && parse_float_literal_checked<double>(text)) {
        this->record_coercion(expr_id, default_type, expected_type, CoercionKind::contextual_float_literal);
    }
    return this->record_expr_types(expr_id, natural_type, literal_type);
}

bool SemanticAnalyzerCore::is_valid_cast(const syntax::ExprKind kind, const TypeHandle dst, const TypeHandle src) const
{
    return this->type_validator().is_valid_cast(kind, dst, src);
}

SemanticAnalyzerCore::TypeAbiLayout SemanticAnalyzerCore::abi_layout(const TypeHandle type) const
{
    return this->abi_checker().abi_layout(type);
}

base::u64 SemanticAnalyzerCore::abi_size(const TypeHandle type) const
{
    return this->abi_checker().abi_size(type);
}

base::u64 SemanticAnalyzerCore::abi_align(const TypeHandle type) const
{
    return this->abi_checker().abi_align(type);
}

bool SemanticAnalyzerCore::is_integer_literal(const syntax::ExprId expr_id) const noexcept
{
    return syntax::is_valid(expr_id) && expr_id.value < this->ctx_.module.exprs.size()
        && this->ctx_.module.exprs.kind(expr_id.value) == syntax::ExprKind::integer_literal;
}

bool SemanticAnalyzerCore::is_null_literal(const syntax::ExprId expr_id) const noexcept
{
    return syntax::is_valid(expr_id) && expr_id.value < this->ctx_.module.exprs.size()
        && this->ctx_.module.exprs.kind(expr_id.value) == syntax::ExprKind::null_literal;
}

bool SemanticAnalyzerCore::is_null_result_expr(const syntax::ExprId expr_id) const noexcept
{
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->ctx_.module.exprs.size()) {
        return false;
    }
    const syntax::ExprKind kind = this->ctx_.module.exprs.kind(expr_id.value);
    const syntax::BlockExprPayload* const block = this->ctx_.module.exprs.block_payload(expr_id.value);
    return kind == syntax::ExprKind::null_literal || (block != nullptr && this->is_null_literal(block->result));
}

SemanticAnalyzerCore::PlaceInfo SemanticAnalyzerCore::analyze_place_info(
    const syntax::ExprId expr_id, const bool emit_diagnostics)
{
    enum class PendingProjectionKind {
        deref,
        field,
        index,
    };

    struct PendingProjection {
        PendingProjectionKind kind = PendingProjectionKind::field;
        syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    };

    std::vector<PendingProjection> pending;
    syntax::ExprId current = expr_id;
    while (syntax::is_valid(current) && current.value < this->ctx_.module.exprs.size()) {
        const syntax::ExprKind kind = this->ctx_.module.exprs.kind(current.value);
        if (const syntax::FieldExprPayload* const field = this->ctx_.module.exprs.field_payload(current.value);
            kind == syntax::ExprKind::field && field != nullptr) {
            pending.push_back(PendingProjection{PendingProjectionKind::field, current});
            current = field->object;
            continue;
        }
        if (const syntax::IndexExprPayload* const index = this->ctx_.module.exprs.index_payload(current.value);
            kind == syntax::ExprKind::index && index != nullptr) {
            pending.push_back(PendingProjection{PendingProjectionKind::index, current});
            current = index->object;
            continue;
        }
        if (const syntax::UnaryExprPayload* const unary = this->ctx_.module.exprs.unary_payload(current.value);
            kind == syntax::ExprKind::unary && unary != nullptr && unary->op == syntax::UnaryOp::dereference) {
            pending.push_back(PendingProjection{PendingProjectionKind::deref, current});
            current = unary->operand;
            continue;
        }
        break;
    }

    PlaceInfo place;
    if (!syntax::is_valid(current) || current.value >= this->ctx_.module.exprs.size()) {
        return place;
    }

    const syntax::NameExprPayload* const base_expr = this->ctx_.module.exprs.name_payload(current.value);
    if (base_expr != nullptr && base_expr->scope_name.empty()) {
        const Symbol* symbol = this->state_.names.symbols.find(base_expr->text_id);
        if (symbol == nullptr) {
            const ModuleLookupKey lookup_key =
                this->find_module_lookup_key(this->state_.flow.current_module, base_expr->text_id);
            if (is_valid(lookup_key)) {
                if (const auto global = this->state_.names.global_values_by_name.find(lookup_key);
                    global != this->state_.names.global_values_by_name.end()) {
                    symbol = global->second;
                }
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
        if (!syntax::is_valid(projection->expr) || projection->expr.value >= this->ctx_.module.exprs.size()) {
            place.type = INVALID_TYPE_HANDLE;
            place.is_place = false;
            place.is_writable = false;
            continue;
        }

        const base::SourceRange projection_range = this->ctx_.module.exprs.range(projection->expr.value);
        const TypeHandle input_type = place.type;
        TypeHandle output_type = INVALID_TYPE_HANDLE;
        bool output_is_place = false;
        bool output_is_writable = false;

        if (projection->kind == PendingProjectionKind::deref) {
            if (this->state_.checked.types.is_pointer(input_type)) {
                const TypeInfo& pointer = this->state_.checked.types.get(input_type);
                output_type = pointer.pointee;
                output_is_place = true;
                output_is_writable = pointer.pointer_mutability == PointerMutability::mut;
                place.crosses_raw_pointer = true;
            } else if (this->state_.checked.types.is_reference(input_type)) {
                const TypeInfo& reference = this->state_.checked.types.get(input_type);
                output_type = reference.pointee;
                output_is_place = true;
                output_is_writable = reference.pointer_mutability == PointerMutability::mut;
            } else if (emit_diagnostics) {
                this->report_general(projection_range, std::string(SEMA_DEREF_POINTER));
            }
        } else if (projection->kind == PendingProjectionKind::field) {
            const syntax::FieldExprPayload* const projection_expr =
                this->ctx_.module.exprs.field_payload(projection->expr.value);
            const std::string_view field_name =
                projection_expr == nullptr ? std::string_view{} : projection_expr->field_name;
            const IdentId field_name_id =
                projection_expr == nullptr ? INVALID_IDENT_ID : projection_expr->field_name_id;
            TypeHandle object_type = input_type;
            bool projection_is_indirect = false;
            output_is_writable = place.is_writable;
            if (this->state_.checked.types.is_pointer(object_type)) {
                const TypeInfo& pointer = this->state_.checked.types.get(object_type);
                projection_is_indirect = true;
                output_is_writable = pointer.pointer_mutability == PointerMutability::mut;
                object_type = pointer.pointee;
                place.crosses_raw_pointer = true;
            } else if (this->state_.checked.types.is_reference(object_type)) {
                const TypeInfo& reference = this->state_.checked.types.get(object_type);
                projection_is_indirect = true;
                output_is_writable = reference.pointer_mutability == PointerMutability::mut;
                object_type = reference.pointee;
            }
            if (this->state_.checked.types.is_tuple(object_type)) {
                if (emit_diagnostics) {
                    this->report_unsupported(projection_range, std::string(SEMA_TUPLE_FIELD_ACCESS_UNSUPPORTED));
                }
            } else if (const StructInfo* info = this->find_struct(object_type); info != nullptr && !info->is_opaque) {
                bool saw_field = false;
                for (const StructFieldInfo& field : info->fields) {
                    if (field.name_id != field_name_id) {
                        continue;
                    }
                    saw_field = true;
                    if (!this->can_access_module(info->module, field.visibility)) {
                        if (emit_diagnostics) {
                            this->report_visibility(projection_range, sema_private_field_message(field_name));
                        }
                    } else {
                        output_type = field.type;
                        output_is_place = projection_is_indirect || place.is_place;
                    }
                    break;
                }
                if (!saw_field && emit_diagnostics) {
                    this->report_lookup(projection_range, sema_unknown_field_message(field_name));
                    this->report_lookup_suggestion(projection_range, this->nearest_field_name(*info, field_name));
                }
            } else if (emit_diagnostics) {
                this->report_general(projection_range, std::string(SEMA_FIELD_STRUCT_VALUE));
            }
        } else {
            const syntax::IndexExprPayload* const projection_expr =
                this->ctx_.module.exprs.index_payload(projection->expr.value);
            const syntax::ExprId index_expr =
                projection_expr == nullptr ? syntax::INVALID_EXPR_ID : projection_expr->index;
            const TypeHandle index_type = this->analyze_expr(index_expr);
            if (emit_diagnostics && !this->state_.checked.types.is_integer(index_type)) {
                this->report_general(place_expr_range_or(this->ctx_.module, index_expr, projection_range),
                    std::string(SEMA_ARRAY_INDEX_INTEGER));
            }
            if (this->state_.checked.types.is_array(input_type)) {
                const PlaceIntegerLiteralExpr literal_index = place_integer_literal_expr(this->ctx_.module, index_expr);
                if (emit_diagnostics && syntax::is_valid(literal_index.literal)) {
                    base::u64 index_value = 0;
                    const syntax::LiteralExprPayload* const literal_expr =
                        this->ctx_.module.exprs.literal_payload(literal_index.literal.value);
                    if (literal_expr != nullptr && this->parse_integer_literal_text(literal_expr->text, index_value)) {
                        const bool out_of_bounds = (literal_index.negated && index_value != 0)
                            || (!literal_index.negated
                                && index_value >= this->state_.checked.types.get(input_type).array_count);
                        if (out_of_bounds) {
                            this->report_general(place_expr_range_or(this->ctx_.module, index_expr, projection_range),
                                std::string(SEMA_ARRAY_INDEX_OUT_OF_BOUNDS));
                        }
                    }
                }
                output_type = this->state_.checked.types.get(input_type).array_element;
                output_is_place = place.is_place;
                output_is_writable = place.is_writable;
            } else if (this->state_.checked.types.is_slice(input_type)) {
                const TypeInfo& slice = this->state_.checked.types.get(input_type);
                output_type = slice.slice_element;
                output_is_place = place.is_place;
                output_is_writable = slice.slice_mutability == PointerMutability::mut;
            } else if (this->state_.checked.types.is_pointer(input_type)) {
                const TypeInfo& pointer = this->state_.checked.types.get(input_type);
                place.crosses_raw_pointer = true;
                if (this->state_.checked.types.is_array(pointer.pointee)) {
                    if (emit_diagnostics) {
                        this->report_general(projection_range, std::string(SEMA_INDEX_POINTER_ARRAY_DEREF));
                    }
                } else if (!this->is_valid_storage_type(pointer.pointee)) {
                    if (emit_diagnostics) {
                        this->report_general(projection_range, std::string(SEMA_INDEX_POINTER_STORAGE));
                    }
                } else {
                    output_type = pointer.pointee;
                    output_is_place = true;
                    output_is_writable = pointer.pointer_mutability == PointerMutability::mut;
                }
            } else if (this->state_.checked.types.is_reference(input_type)) {
                const TypeInfo& reference = this->state_.checked.types.get(input_type);
                if (this->state_.checked.types.is_array(reference.pointee)) {
                    output_type = this->state_.checked.types.get(reference.pointee).array_element;
                    output_is_place = true;
                    output_is_writable = reference.pointer_mutability == PointerMutability::mut;
                } else if (this->state_.checked.types.is_slice(reference.pointee)) {
                    const TypeInfo& slice = this->state_.checked.types.get(reference.pointee);
                    output_type = slice.slice_element;
                    output_is_place = true;
                    output_is_writable = slice.slice_mutability == PointerMutability::mut;
                } else if (emit_diagnostics) {
                    this->report_general(projection_range, std::string(SEMA_INDEX_ARRAY_OR_POINTER));
                }
            } else if (emit_diagnostics) {
                this->report_general(projection_range, std::string(SEMA_INDEX_ARRAY_OR_POINTER));
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

void SemanticAnalyzerCore::require_place_projection_safety(const PlaceInfo& place, const base::SourceRange& range)
{
    if (place.crosses_raw_pointer) {
        this->require_unsafe_context(range, SEMA_UNSAFE_RAW_POINTER_PROJECTION);
    }
}

bool SemanticAnalyzerCore::is_place_expr(const syntax::ExprId expr_id)
{
    return this->analyze_place_info(expr_id, false).is_place;
}

bool SemanticAnalyzerCore::is_writable_place(const syntax::ExprId expr_id)
{
    return this->analyze_place_info(expr_id, false).is_writable;
}

bool SemanticAnalyzerCore::is_array_containing_value_type(const TypeHandle type) const noexcept
{
    return this->type_validator().is_array_containing_value_type(type);
}

const StructInfo* SemanticAnalyzerCore::find_struct(const TypeHandle type) const noexcept
{
    return this->type_validator().find_struct(type);
}

} // namespace aurex::sema
