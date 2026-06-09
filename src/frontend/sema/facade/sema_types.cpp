#include <aurex/frontend/sema/sema_messages.hpp>
#include <aurex/infrastructure/query/principal_set_composition_facts.hpp>

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

#include <frontend/sema/internal/core/private/sema_core.hpp>
#include <frontend/sema/internal/services/private/sema_type_services.hpp>

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
constexpr int SEMA_TUPLE_FIELD_INDEX_DECIMAL_BASE = 10;
constexpr std::string_view SEMA_PRINCIPAL_SET_WITNESS_TAG = "sema.principal_set.witness.v1";
constexpr std::string_view SEMA_PRINCIPAL_SET_PROJECTION_TAG = "sema.principal_set.projection.v1";
constexpr std::string_view SEMA_PRINCIPAL_SET_SUPERTRAIT_PROJECTION_TAG =
    "sema.principal_set.supertrait_projection.v1";

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

struct TupleFieldIndexParse {
    bool numeric = false;
    bool in_range = false;
    base::u64 value = 0;
};

[[nodiscard]] bool is_decimal_digit(const char c) noexcept
{
    return c >= '0' && c <= '9';
}

[[nodiscard]] TupleFieldIndexParse parse_tuple_field_index(const std::string_view field_name) noexcept
{
    if (field_name.empty()) {
        return {};
    }
    if (!std::ranges::all_of(field_name, is_decimal_digit)) {
        return {};
    }
    base::u64 value = 0;
    const char* const begin = field_name.data();
    const char* const end = begin + field_name.size();
    const auto result = std::from_chars(begin, end, value, SEMA_TUPLE_FIELD_INDEX_DECIMAL_BASE);
    if (result.ec != std::errc{} || result.ptr != end) {
        return TupleFieldIndexParse{.numeric = true};
    }
    return TupleFieldIndexParse{.numeric = true, .in_range = true, .value = value};
}

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

[[nodiscard]] query::DynBorrowKind principal_set_borrow_kind(const PointerMutability mutability) noexcept
{
    return mutability == PointerMutability::mut ? query::DynBorrowKind::mut : query::DynBorrowKind::shared;
}

[[nodiscard]] query::TraitObjectBorrowKindKey trait_object_borrow_kind(
    const PointerMutability mutability) noexcept
{
    return mutability == PointerMutability::mut ? query::TraitObjectBorrowKindKey::mut
                                                : query::TraitObjectBorrowKindKey::shared;
}

[[nodiscard]] query::StableFingerprint128 principal_witness_fingerprint(
    const TypeTable& types,
    const TypeHandle concrete_type,
    const query::TraitObjectTypeKey& principal,
    const query::VTableLayoutKey& layout)
{
    query::StableKeyWriter writer;
    writer.write_string(SEMA_PRINCIPAL_SET_WITNESS_TAG);
    writer.write_string(types.display_name(concrete_type));
    query::append_stable_key(writer, principal);
    query::append_stable_key(writer, layout);
    return writer.fingerprint();
}

[[nodiscard]] query::StableFingerprint128 principal_projection_fingerprint(
    const TypeTable& types,
    const TypeHandle concrete_type,
    const query::StableFingerprint128 principal_set_identity,
    const query::TraitObjectTypeKey& principal,
    const query::DynBorrowKind borrow_kind)
{
    query::StableKeyWriter writer;
    writer.write_string(SEMA_PRINCIPAL_SET_PROJECTION_TAG);
    writer.write_string(types.display_name(concrete_type));
    writer.write_fingerprint(principal_set_identity);
    query::append_stable_key(writer, principal);
    writer.write_u8(static_cast<base::u8>(borrow_kind));
    return writer.fingerprint();
}

[[nodiscard]] query::StableFingerprint128 principal_composition_projection_fingerprint(
    const TypeTable& types,
    const TypeHandle source_type,
    const TypeHandle target_type,
    const query::StableFingerprint128 principal_set_identity,
    const query::TraitObjectTypeKey& principal,
    const query::DynBorrowKind borrow_kind)
{
    query::StableKeyWriter writer;
    writer.write_string(SEMA_PRINCIPAL_SET_PROJECTION_TAG);
    writer.write_string("composition_to_principal");
    writer.write_string(types.display_name(source_type));
    writer.write_string(types.display_name(target_type));
    writer.write_fingerprint(principal_set_identity);
    query::append_stable_key(writer, principal);
    writer.write_u8(static_cast<base::u8>(borrow_kind));
    return writer.fingerprint();
}

[[nodiscard]] query::StableFingerprint128 principal_composition_supertrait_projection_fingerprint(
    const TypeTable& types,
    const TypeHandle source_type,
    const TypeHandle source_principal_type,
    const TypeHandle target_type,
    const query::StableFingerprint128 principal_set_identity,
    const query::TraitObjectTypeKey& source_principal,
    const query::TraitObjectTypeKey& target_object,
    const query::StableFingerprint128 edge_fingerprint,
    const query::DynBorrowKind borrow_kind)
{
    query::StableKeyWriter writer;
    writer.write_string(SEMA_PRINCIPAL_SET_SUPERTRAIT_PROJECTION_TAG);
    writer.write_string(types.display_name(source_type));
    writer.write_string(types.display_name(source_principal_type));
    writer.write_string(types.display_name(target_type));
    writer.write_fingerprint(principal_set_identity);
    query::append_stable_key(writer, source_principal);
    query::append_stable_key(writer, target_object);
    writer.write_fingerprint(edge_fingerprint);
    writer.write_u8(static_cast<base::u8>(borrow_kind));
    return writer.fingerprint();
}

[[nodiscard]] bool composition_witness_sets_match(
    const query::CompositionWitnessSetFact& lhs,
    const query::CompositionWitnessSetFact& rhs) noexcept
{
    if (lhs.principal_set_identity != rhs.principal_set_identity || lhs.witnesses.size() != rhs.witnesses.size()) {
        return false;
    }
    for (base::usize index = 0; index < lhs.witnesses.size(); ++index) {
        if (lhs.witnesses[index].witness_fingerprint != rhs.witnesses[index].witness_fingerprint) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] base::u32 builtin_integer_bits(const BuiltinType type, const base::u32 target_pointer_bits) noexcept
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
        case BuiltinType::usize:
            return target_pointer_bits;
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

[[nodiscard]] bool literal_fits_integer_type(const TypeTable& types, const TypeHandle destination,
    const std::string_view text, const base::u32 target_pointer_bits) noexcept
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
    const base::u32 bits = builtin_integer_bits(info.builtin, target_pointer_bits);
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

[[nodiscard]] bool negative_literal_fits_integer_type(const TypeTable& types, const TypeHandle destination,
    const std::string_view text, const base::u32 target_pointer_bits) noexcept
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
    const base::u32 bits = builtin_integer_bits(info.builtin, target_pointer_bits);
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
    const TypeHandle dst, const TypeHandle src, const syntax::ExprId value) const
{
    return this->type_validator().can_assign(dst, src, value);
}

bool SemanticAnalyzerCore::can_borrowed_dyn_trait_coerce(const TypeHandle dst, const TypeHandle src) const noexcept
{
    if (!this->state_.checked.types.is_reference(dst) || !this->state_.checked.types.is_reference(src)) {
        return false;
    }
    const TypeInfo& dst_ref = this->state_.checked.types.get(dst);
    const TypeInfo& src_ref = this->state_.checked.types.get(src);
    if (dst_ref.pointer_mutability == PointerMutability::mut
        && src_ref.pointer_mutability != PointerMutability::mut) {
        return false;
    }
    if (!is_valid(dst_ref.pointee) || dst_ref.pointee.value >= this->state_.checked.types.size()
        || !is_valid(src_ref.pointee) || src_ref.pointee.value >= this->state_.checked.types.size()) {
        return false;
    }
    const TypeInfo& object_info = this->state_.checked.types.get(dst_ref.pointee);
    const TypeInfo& source_info = this->state_.checked.types.get(src_ref.pointee);
    if (object_info.kind != TypeKind::trait_object || source_info.kind == TypeKind::trait_object) {
        return false;
    }
    if (!object_info.trait_object_principal_types.empty()) {
        SemanticAnalyzerCore& mutable_core = const_cast<SemanticAnalyzerCore&>(*this);
        for (const TypeHandle principal : object_info.trait_object_principal_types) {
            if (!is_valid(principal) || principal.value >= this->state_.checked.types.size()) {
                return false;
            }
            const TypeInfo& principal_info = this->state_.checked.types.get(principal);
            if (principal_info.kind != TypeKind::trait_object
                || mutable_core.find_trait_object_impl(src_ref.pointee, principal_info, {}, false) == nullptr) {
                return false;
            }
        }
        return true;
    }
    SemanticAnalyzerCore& mutable_core = const_cast<SemanticAnalyzerCore&>(*this);
    return mutable_core.find_trait_object_impl(src_ref.pointee, object_info, {}, false) != nullptr;
}

bool SemanticAnalyzerCore::can_borrowed_dyn_trait_composition_project(
    const TypeHandle dst, const TypeHandle src) const noexcept
{
    if (!this->state_.checked.types.is_reference(dst) || !this->state_.checked.types.is_reference(src)) {
        return false;
    }
    const TypeInfo& dst_ref = this->state_.checked.types.get(dst);
    const TypeInfo& src_ref = this->state_.checked.types.get(src);
    if (dst_ref.pointer_mutability == PointerMutability::mut
        && src_ref.pointer_mutability != PointerMutability::mut) {
        return false;
    }
    if (!is_valid(dst_ref.pointee) || dst_ref.pointee.value >= this->state_.checked.types.size()
        || !is_valid(src_ref.pointee) || src_ref.pointee.value >= this->state_.checked.types.size()) {
        return false;
    }
    const TypeInfo& target_object = this->state_.checked.types.get(dst_ref.pointee);
    const TypeInfo& source_object = this->state_.checked.types.get(src_ref.pointee);
    if (target_object.kind != TypeKind::trait_object || source_object.kind != TypeKind::trait_object) {
        return false;
    }
    if (!target_object.trait_object_principal_types.empty()
        || source_object.trait_object_principal_types.empty()
        || source_object.trait_object_principal_set_identity.byte_count == 0
        || !query::is_valid(target_object.trait_object_key)) {
        return false;
    }
    return std::ranges::any_of(source_object.trait_object_principal_types, [&](const TypeHandle principal) {
        return is_valid(principal) && principal.value < this->state_.checked.types.size()
            && this->state_.checked.types.same(principal, dst_ref.pointee);
    });
}

bool SemanticAnalyzerCore::can_borrowed_dyn_trait_upcast(const TypeHandle dst, const TypeHandle src) const
{
    if (!this->state_.checked.types.is_reference(dst) || !this->state_.checked.types.is_reference(src)) {
        return false;
    }
    const TypeInfo& dst_ref = this->state_.checked.types.get(dst);
    const TypeInfo& src_ref = this->state_.checked.types.get(src);
    if (dst_ref.pointer_mutability == PointerMutability::mut
        && src_ref.pointer_mutability != PointerMutability::mut) {
        return false;
    }
    if (!is_valid(dst_ref.pointee) || dst_ref.pointee.value >= this->state_.checked.types.size()
        || !is_valid(src_ref.pointee) || src_ref.pointee.value >= this->state_.checked.types.size()) {
        return false;
    }
    const TypeInfo& target_object = this->state_.checked.types.get(dst_ref.pointee);
    const TypeInfo& source_object = this->state_.checked.types.get(src_ref.pointee);
    if (target_object.kind != TypeKind::trait_object || source_object.kind != TypeKind::trait_object) {
        return false;
    }
    return this->find_supertrait_edge_path(source_object, target_object) != nullptr;
}

void SemanticAnalyzerCore::record_borrowed_dyn_trait_coercion_if_needed(const syntax::ExprId expr,
    const TypeHandle from_type,
    const TypeHandle to_type,
    const base::SourceRange& range)
{
    if (!syntax::is_valid(expr) || !this->can_borrowed_dyn_trait_coerce(to_type, from_type)) {
        return;
    }
    const TypeInfo& source_ref = this->state_.checked.types.get(from_type);
    const TypeInfo& target_ref = this->state_.checked.types.get(to_type);
    const TypeHandle concrete_type = source_ref.pointee;
    const TypeHandle object_type = target_ref.pointee;
    const TypeInfo& object_info = this->state_.checked.types.get(object_type);
    if (!object_info.trait_object_principal_types.empty()) {
        std::vector<query::CompositionWitnessDescriptor> witnesses;
        witnesses.reserve(object_info.trait_object_principal_types.size());
        base::Result<query::CanonicalTypeKey> concrete_key = this->checked_canonical_type_key(concrete_type);
        if (!concrete_key) {
            this->report_internal_contract(range, concrete_key.error().message);
            return;
        }
        const query::CanonicalTypeKey concrete_type_key = concrete_key.take_value();
        const query::DynBorrowKind borrow_kind = principal_set_borrow_kind(target_ref.pointer_mutability);
        for (const TypeHandle principal : object_info.trait_object_principal_types) {
            if (!is_valid(principal) || principal.value >= this->state_.checked.types.size()) {
                return;
            }
            const TypeInfo& principal_info = this->state_.checked.types.get(principal);
            const query::VTableLayoutKey layout = this->record_vtable_layout(concrete_type, principal, range);
            if (!query::is_valid(layout)) {
                return;
            }
            this->bind_dyn_trait_upcast_vtable_layouts_for_composition_witness(
                concrete_type, principal, layout, range);
            query::CompositionWitnessDescriptor witness;
            witness.principal_object = principal_info.trait_object_key;
            witness.vtable_layout = layout;
            witness.witness_fingerprint =
                principal_witness_fingerprint(this->state_.checked.types, concrete_type, witness.principal_object,
                    layout);
            witness.principal_name = std::string(principal_info.trait_object_name.view());
            witness.concrete_type_name = this->state_.checked.types.display_name(concrete_type);
            witnesses.push_back(std::move(witness));

            query::CompositionProjectionFact projection;
            projection.principal_set_identity = object_info.trait_object_principal_set_identity;
            projection.kind = query::PrincipalSetProjectionKind::concrete_to_composition;
            projection.concrete_type = concrete_type_key;
            projection.source_principal = principal_info.trait_object_key;
            projection.target_object = principal_info.trait_object_key;
            projection.projection_path = principal_projection_fingerprint(this->state_.checked.types, concrete_type,
                object_info.trait_object_principal_set_identity, principal_info.trait_object_key, borrow_kind);
            projection.borrow_kind = borrow_kind;
            projection.source_view_name = this->state_.checked.types.display_name(concrete_type);
            projection.target_view_name = this->state_.checked.types.display_name(object_type);
            if (!std::ranges::any_of(this->state_.checked.principal_set_composition_facts.projections,
                    [&](const query::CompositionProjectionFact& existing) {
                        return existing.projection_path == projection.projection_path;
                    })) {
                query::record_composition_projection_fact(
                    this->state_.checked.principal_set_composition_facts, std::move(projection));
            }
        }
        std::ranges::sort(witnesses, [](const query::CompositionWitnessDescriptor& lhs,
                                         const query::CompositionWitnessDescriptor& rhs) {
            if (lhs.principal_object.principal_trait.global_id != rhs.principal_object.principal_trait.global_id) {
                return lhs.principal_object.principal_trait.global_id < rhs.principal_object.principal_trait.global_id;
            }
            return lhs.principal_object.global_id < rhs.principal_object.global_id;
        });

        query::CompositionWitnessSetFact witness_set;
        witness_set.principal_set_identity = object_info.trait_object_principal_set_identity;
        witness_set.metadata_policy = query::PrincipalSetMetadataPolicy::principal_set_metadata_v1;
        witness_set.witnesses = std::move(witnesses);
        if (!std::ranges::any_of(this->state_.checked.principal_set_composition_facts.witness_sets,
                [&](const query::CompositionWitnessSetFact& existing) {
                    return composition_witness_sets_match(existing, witness_set);
                })) {
            query::record_composition_witness_set_fact(
                this->state_.checked.principal_set_composition_facts, std::move(witness_set));
        }

        this->state_.checked.principal_set_composition_facts.fingerprint =
            query::principal_set_composition_facts_fingerprint(this->state_.checked.principal_set_composition_facts);
        this->record_coercion(expr, from_type, to_type, CoercionKind::borrowed_dyn_trait);
        return;
    }
    const query::VTableLayoutKey layout = this->record_vtable_layout(concrete_type, object_type, range);
    if (!query::is_valid(layout)) {
        return;
    }
    base::Result<query::CanonicalTypeKey> source_key = this->checked_canonical_type_key(concrete_type);
    if (!source_key) {
        this->report_internal_contract(range, source_key.error().message);
        return;
    }
    const query::TraitObjectBorrowKindKey borrow_kind = trait_object_borrow_kind(target_ref.pointer_mutability);
    const query::TraitObjectCoercionKey coercion_key = query::trait_object_coercion_key(source_key.take_value(),
        object_info.trait_object_key.object_origin, object_info.trait_object_key, layout, borrow_kind);
    if (!query::is_valid(coercion_key)) {
        this->report_internal_contract(range, "failed to create borrowed dyn trait coercion key");
        return;
    }
    this->record_coercion(expr, from_type, to_type, CoercionKind::borrowed_dyn_trait);
    for (const TraitObjectCoercionFact& existing : this->state_.checked.trait_object_coercions) {
        if (existing.expr.value == expr.value && existing.coercion_key == coercion_key) {
            return;
        }
    }
    TraitObjectCoercionFact fact = this->state_.checked.make_trait_object_coercion_fact();
    fact.coercion_key = coercion_key;
    fact.expr = expr;
    fact.source_reference_type = from_type;
    fact.target_reference_type = to_type;
    fact.source_type = concrete_type;
    fact.object_type = object_type;
    fact.vtable_layout = layout;
    fact.borrow_kind = borrow_kind;
    fact.range = range;
    fact.part_index = syntax::is_valid(this->state_.flow.current_item)
        ? this->item_part_index(this->state_.flow.current_item)
        : 0U;
    this->state_.checked.trait_object_coercions.push_back(fact);
    this->bind_dyn_trait_upcast_vtable_layouts_for_coercion(this->state_.checked.trait_object_coercions.back());
}

void SemanticAnalyzerCore::record_borrowed_dyn_trait_composition_projection_if_needed(const syntax::ExprId expr,
    const TypeHandle from_type,
    const TypeHandle to_type,
    const base::SourceRange& range)
{
    if (!syntax::is_valid(expr) || !this->can_borrowed_dyn_trait_composition_project(to_type, from_type)) {
        return;
    }

    const TypeInfo& source_ref = this->state_.checked.types.get(from_type);
    const TypeInfo& target_ref = this->state_.checked.types.get(to_type);
    const TypeHandle source_object_type = source_ref.pointee;
    const TypeHandle target_object_type = target_ref.pointee;
    const TypeInfo& source_object = this->state_.checked.types.get(source_object_type);
    const TypeInfo& target_object = this->state_.checked.types.get(target_object_type);

    base::Result<query::CanonicalTypeKey> source_key = this->checked_canonical_type_key(source_object_type);
    if (!source_key) {
        this->report_internal_contract(range, source_key.error().message);
        return;
    }

    const query::DynBorrowKind borrow_kind = principal_set_borrow_kind(target_ref.pointer_mutability);
    query::CompositionProjectionFact projection;
    projection.principal_set_identity = source_object.trait_object_principal_set_identity;
    projection.kind = query::PrincipalSetProjectionKind::composition_to_principal;
    projection.concrete_type = source_key.take_value();
    projection.source_principal = target_object.trait_object_key;
    projection.target_object = target_object.trait_object_key;
    projection.projection_path = principal_composition_projection_fingerprint(this->state_.checked.types,
        source_object_type, target_object_type, source_object.trait_object_principal_set_identity,
        target_object.trait_object_key, borrow_kind);
    projection.borrow_kind = borrow_kind;
    projection.source_view_name = this->state_.checked.types.display_name(source_object_type);
    projection.target_view_name = this->state_.checked.types.display_name(target_object_type);

    if (!std::ranges::any_of(this->state_.checked.principal_set_composition_facts.projections,
            [&](const query::CompositionProjectionFact& existing) {
                return existing.projection_path == projection.projection_path;
            })) {
        query::record_composition_projection_fact(
            this->state_.checked.principal_set_composition_facts, std::move(projection));
    }
    this->state_.checked.principal_set_composition_facts.fingerprint =
        query::principal_set_composition_facts_fingerprint(this->state_.checked.principal_set_composition_facts);
    this->record_coercion(expr, from_type, to_type, CoercionKind::borrowed_dyn_trait);
}

void SemanticAnalyzerCore::record_borrowed_dyn_trait_composition_supertrait_projection_if_needed(
    const syntax::ExprId expr,
    const TypeHandle from_type,
    const TypeHandle to_type,
    const TypeHandle source_principal_type,
    const base::SourceRange& range)
{
    if (!syntax::is_valid(expr)
        || !this->state_.checked.types.is_reference(from_type)
        || !this->state_.checked.types.is_reference(to_type)
        || !is_valid(source_principal_type) || source_principal_type.value >= this->state_.checked.types.size()) {
        return;
    }

    const TypeInfo& source_ref = this->state_.checked.types.get(from_type);
    const TypeInfo& target_ref = this->state_.checked.types.get(to_type);
    if (target_ref.pointer_mutability == PointerMutability::mut
        && source_ref.pointer_mutability != PointerMutability::mut) {
        return;
    }
    if (!is_valid(source_ref.pointee) || source_ref.pointee.value >= this->state_.checked.types.size()
        || !is_valid(target_ref.pointee) || target_ref.pointee.value >= this->state_.checked.types.size()) {
        return;
    }

    const TypeHandle source_object_type = source_ref.pointee;
    const TypeHandle target_object_type = target_ref.pointee;
    const TypeInfo& source_object = this->state_.checked.types.get(source_object_type);
    const TypeInfo& source_principal = this->state_.checked.types.get(source_principal_type);
    const TypeInfo& target_object = this->state_.checked.types.get(target_object_type);
    if (source_object.kind != TypeKind::trait_object || target_object.kind != TypeKind::trait_object
        || source_principal.kind != TypeKind::trait_object
        || source_object.trait_object_principal_types.empty()
        || source_object.trait_object_principal_set_identity.byte_count == 0
        || !query::is_valid(source_principal.trait_object_key)
        || !query::is_valid(target_object.trait_object_key)) {
        return;
    }
    if (!std::ranges::any_of(source_object.trait_object_principal_types, [&](const TypeHandle principal) {
            return this->state_.checked.types.same(principal, source_principal_type);
        })) {
        return;
    }

    const TraitSupertraitEdgeFact* const edge = this->find_supertrait_edge_path(source_principal, target_object);
    if (edge == nullptr) {
        return;
    }

    base::Result<query::CanonicalTypeKey> source_key = this->checked_canonical_type_key(source_object_type);
    if (!source_key) {
        this->report_internal_contract(range, source_key.error().message);
        return;
    }

    const query::DynBorrowKind borrow_kind = principal_set_borrow_kind(target_ref.pointer_mutability);
    query::CompositionProjectionFact projection;
    projection.principal_set_identity = source_object.trait_object_principal_set_identity;
    projection.kind = query::PrincipalSetProjectionKind::composition_to_supertrait;
    projection.concrete_type = source_key.take_value();
    projection.source_principal = source_principal.trait_object_key;
    projection.target_object = target_object.trait_object_key;
    projection.projection_path = principal_composition_supertrait_projection_fingerprint(this->state_.checked.types,
        source_object_type, source_principal_type, target_object_type,
        source_object.trait_object_principal_set_identity, source_principal.trait_object_key,
        target_object.trait_object_key, edge->edge_fingerprint, borrow_kind);
    projection.borrow_kind = borrow_kind;
    projection.source_view_name = this->state_.checked.types.display_name(source_object_type);
    projection.target_view_name = this->state_.checked.types.display_name(target_object_type);

    if (!std::ranges::any_of(this->state_.checked.principal_set_composition_facts.projections,
            [&](const query::CompositionProjectionFact& existing) {
                return existing.projection_path == projection.projection_path;
            })) {
        query::record_composition_projection_fact(
            this->state_.checked.principal_set_composition_facts, std::move(projection));
    }
    this->state_.checked.principal_set_composition_facts.fingerprint =
        query::principal_set_composition_facts_fingerprint(this->state_.checked.principal_set_composition_facts);
    this->record_coercion(expr, from_type, to_type, CoercionKind::borrowed_dyn_trait);

    const TypeHandle source_principal_reference_type =
        this->state_.checked.types.reference(source_ref.pointer_mutability, source_principal_type);
    this->record_borrowed_dyn_trait_projection_upcast_if_needed(
        expr, source_principal_reference_type, to_type, range);
}

void SemanticAnalyzerCore::record_borrowed_dyn_trait_projection_upcast_if_needed(
    const syntax::ExprId expr,
    const TypeHandle from_type,
    const TypeHandle to_type,
    const base::SourceRange& range)
{
    if (!syntax::is_valid(expr) || !this->can_borrowed_dyn_trait_upcast(to_type, from_type)) {
        return;
    }
    const TypeInfo& source_ref = this->state_.checked.types.get(from_type);
    const TypeInfo& target_ref = this->state_.checked.types.get(to_type);
    const TypeHandle source_object_type = source_ref.pointee;
    const TypeHandle target_object_type = target_ref.pointee;
    const TypeInfo& source_object = this->state_.checked.types.get(source_object_type);
    const TypeInfo& target_object = this->state_.checked.types.get(target_object_type);
    const TraitSupertraitEdgeFact* const edge = this->find_supertrait_edge_path(source_object, target_object);
    if (edge == nullptr) {
        return;
    }

    const query::TraitObjectBorrowKindKey borrow_kind = trait_object_borrow_kind(target_ref.pointer_mutability);
    const query::TraitObjectUpcastCoercionKey upcast_key = query::trait_object_upcast_coercion_key(
        source_object.trait_object_key, source_object.trait_object_key.object_origin,
        target_object.trait_object_key, edge->edge_fingerprint, borrow_kind);
    if (!query::is_valid(upcast_key)) {
        this->report_internal_contract(range, "failed to create borrowed dyn trait projection upcast key");
        return;
    }

    for (TraitObjectUpcastCoercionFact& existing : this->state_.checked.trait_object_upcast_coercions) {
        if (existing.expr.value == expr.value && existing.upcast_key == upcast_key) {
            this->bind_dyn_trait_projection_upcast_layouts_from_existing_composition_witnesses(existing, range);
            return;
        }
    }

    TraitObjectUpcastCoercionFact fact = this->state_.checked.make_trait_object_upcast_coercion_fact();
    fact.upcast_key = upcast_key;
    fact.expr = expr;
    fact.source_reference_type = from_type;
    fact.target_reference_type = to_type;
    fact.source_object_type = source_object_type;
    fact.target_object_type = target_object_type;
    fact.edge_fingerprint = edge->edge_fingerprint;
    fact.borrow_kind = borrow_kind;
    fact.range = range;
    fact.part_index = syntax::is_valid(this->state_.flow.current_item)
        ? this->item_part_index(this->state_.flow.current_item)
        : 0U;
    this->bind_dyn_trait_projection_upcast_layouts_from_existing_composition_witnesses(fact, range);
    this->state_.checked.trait_object_upcast_coercions.push_back(fact);
}

void SemanticAnalyzerCore::bind_dyn_trait_projection_upcast_layouts_from_existing_composition_witnesses(
    TraitObjectUpcastCoercionFact& upcast,
    const base::SourceRange& range)
{
    if (!is_valid(upcast.source_object_type) || !is_valid(upcast.target_object_type)
        || upcast.source_object_type.value >= this->state_.checked.types.size()) {
        return;
    }
    const TypeInfo& source_object = this->state_.checked.types.get(upcast.source_object_type);
    if (!this->state_.checked.types.is_trait_object(upcast.source_object_type)
        || !query::is_valid(source_object.trait_object_key)) {
        return;
    }

    for (const query::CompositionWitnessSetFact& witness_set :
        this->state_.checked.principal_set_composition_facts.witness_sets) {
        for (const query::CompositionWitnessDescriptor& witness : witness_set.witnesses) {
            if (witness.principal_object != source_object.trait_object_key
                || !query::is_valid(witness.vtable_layout)) {
                continue;
            }
            TypeHandle concrete_type = INVALID_TYPE_HANDLE;
            for (const VTableLayoutFact& layout : this->state_.checked.vtable_layouts) {
                if (layout.layout_key == witness.vtable_layout
                    && layout.object_type.value == upcast.source_object_type.value) {
                    concrete_type = layout.concrete_type;
                    break;
                }
            }
            if (!is_valid(concrete_type)) {
                continue;
            }
            const query::VTableLayoutKey target_layout =
                this->record_vtable_layout(concrete_type, upcast.target_object_type, range);
            if (!query::is_valid(target_layout)) {
                continue;
            }
            if (!query::is_valid(upcast.source_vtable_layout)) {
                upcast.source_vtable_layout = witness.vtable_layout;
            }
            if (!query::is_valid(upcast.target_vtable_layout)) {
                upcast.target_vtable_layout = target_layout;
            }
        }
    }
}

void SemanticAnalyzerCore::bind_dyn_trait_upcast_vtable_layouts_for_composition_witness(
    const TypeHandle concrete_type,
    const TypeHandle source_object_type,
    const query::VTableLayoutKey source_vtable_layout,
    const base::SourceRange& range)
{
    if (!query::is_valid(source_vtable_layout) || !is_valid(concrete_type) || !is_valid(source_object_type)) {
        return;
    }
    for (TraitObjectUpcastCoercionFact& upcast : this->state_.checked.trait_object_upcast_coercions) {
        if (upcast.source_object_type.value != source_object_type.value) {
            continue;
        }
        const query::VTableLayoutKey target_layout =
            this->record_vtable_layout(concrete_type, upcast.target_object_type, range);
        if (!query::is_valid(upcast.source_vtable_layout)) {
            upcast.source_vtable_layout = source_vtable_layout;
        }
        if (!query::is_valid(upcast.target_vtable_layout) && query::is_valid(target_layout)) {
            upcast.target_vtable_layout = target_layout;
        }
    }
}

void SemanticAnalyzerCore::bind_dyn_trait_upcast_vtable_layouts_for_coercion(
    const TraitObjectCoercionFact& source_coercion)
{
    if (!query::is_valid(source_coercion.vtable_layout) || !is_valid(source_coercion.source_type)
        || !is_valid(source_coercion.object_type)) {
        return;
    }
    for (TraitObjectUpcastCoercionFact& upcast : this->state_.checked.trait_object_upcast_coercions) {
        if (upcast.source_object_type.value != source_coercion.object_type.value) {
            continue;
        }
        if (!query::is_valid(upcast.source_vtable_layout)) {
            upcast.source_vtable_layout = source_coercion.vtable_layout;
        }
        const query::VTableLayoutKey target_layout =
            this->record_vtable_layout(source_coercion.source_type, upcast.target_object_type, upcast.range);
        if (query::is_valid(target_layout) && !query::is_valid(upcast.target_vtable_layout)) {
            upcast.target_vtable_layout = target_layout;
        }
        if (!query::is_valid(upcast.target_vtable_layout)) {
            continue;
        }
        for (TraitMethodCallBinding& binding : this->state_.checked.trait_method_calls) {
            if (binding.dispatch != TraitMethodDispatchKind::vtable_slot
                || query::is_valid(binding.vtable_layout)
                || binding.receiver_type.value != upcast.source_reference_type.value
                || binding.dispatch_receiver_type.value != upcast.target_object_type.value) {
                continue;
            }
            binding.vtable_layout = upcast.target_vtable_layout;
        }
    }
}

void SemanticAnalyzerCore::record_borrowed_dyn_trait_upcast_if_needed(const syntax::ExprId expr,
    const TypeHandle from_type,
    const TypeHandle to_type,
    const base::SourceRange& range)
{
    if (!syntax::is_valid(expr) || !this->can_borrowed_dyn_trait_upcast(to_type, from_type)) {
        return;
    }
    const TypeInfo& source_ref = this->state_.checked.types.get(from_type);
    const TypeInfo& target_ref = this->state_.checked.types.get(to_type);
    const TypeHandle source_object_type = source_ref.pointee;
    const TypeHandle target_object_type = target_ref.pointee;
    const TypeInfo& source_object = this->state_.checked.types.get(source_object_type);
    const TypeInfo& target_object = this->state_.checked.types.get(target_object_type);
    const TraitSupertraitEdgeFact* const edge = this->find_supertrait_edge_path(source_object, target_object);
    if (edge == nullptr) {
        return;
    }
    query::VTableLayoutKey source_vtable_layout;
    query::VTableLayoutKey target_vtable_layout;
    TypeHandle concrete_type = INVALID_TYPE_HANDLE;
    for (const TraitObjectCoercionFact& coercion : this->state_.checked.trait_object_coercions) {
        if (coercion.target_reference_type.value == from_type.value
            && coercion.object_type.value == source_object_type.value
            && query::is_valid(coercion.vtable_layout)) {
            source_vtable_layout = coercion.vtable_layout;
            concrete_type = coercion.source_type;
            break;
        }
    }
    if (is_valid(concrete_type)) {
        target_vtable_layout = this->record_vtable_layout(concrete_type, target_object_type, range);
    }
    if (query::is_valid(source_vtable_layout) && !query::is_valid(target_vtable_layout)) {
        return;
    }
    const query::TraitObjectBorrowKindKey borrow_kind = target_ref.pointer_mutability == PointerMutability::mut
        ? query::TraitObjectBorrowKindKey::mut
        : query::TraitObjectBorrowKindKey::shared;
    const query::TraitObjectUpcastCoercionKey upcast_key = query::trait_object_upcast_coercion_key(
        source_object.trait_object_key, source_object.trait_object_key.object_origin,
        target_object.trait_object_key, edge->edge_fingerprint, borrow_kind);
    if (!query::is_valid(upcast_key)) {
        this->report_internal_contract(range, "failed to create borrowed dyn trait upcast key");
        return;
    }

    this->record_coercion(expr, from_type, to_type, CoercionKind::borrowed_dyn_trait);
    for (TraitObjectUpcastCoercionFact& existing : this->state_.checked.trait_object_upcast_coercions) {
        if (existing.expr.value == expr.value && existing.upcast_key == upcast_key) {
            if (!query::is_valid(existing.source_vtable_layout)) {
                existing.source_vtable_layout = source_vtable_layout;
            }
            if (!query::is_valid(existing.target_vtable_layout)) {
                existing.target_vtable_layout = target_vtable_layout;
            }
            return;
        }
    }
    TraitObjectUpcastCoercionFact fact = this->state_.checked.make_trait_object_upcast_coercion_fact();
    fact.upcast_key = upcast_key;
    fact.expr = expr;
    fact.source_reference_type = from_type;
    fact.target_reference_type = to_type;
    fact.source_object_type = source_object_type;
    fact.target_object_type = target_object_type;
    fact.source_vtable_layout = source_vtable_layout;
    fact.target_vtable_layout = target_vtable_layout;
    fact.edge_fingerprint = edge->edge_fingerprint;
    fact.borrow_kind = borrow_kind;
    fact.range = range;
    fact.part_index = syntax::is_valid(this->state_.flow.current_item)
        ? this->item_part_index(this->state_.flow.current_item)
        : 0U;
    this->state_.checked.trait_object_upcast_coercions.push_back(fact);
    for (const TraitObjectCoercionFact& coercion : this->state_.checked.trait_object_coercions) {
        this->bind_dyn_trait_upcast_vtable_layouts_for_coercion(coercion);
    }
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
    return literal_fits_integer_type(this->state_.checked.types, destination, text, this->target_pointer_bit_width());
}

bool SemanticAnalyzerCore::negative_integer_literal_fits_type(
    const TypeHandle destination, const std::string_view text) const noexcept
{
    return negative_literal_fits_integer_type(
        this->state_.checked.types, destination, text, this->target_pointer_bit_width());
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

base::u32 SemanticAnalyzerCore::target_pointer_bit_width() const noexcept
{
    constexpr base::u64 BITS_PER_BYTE = 8;
    return static_cast<base::u32>(this->ctx_.options.target_layout.pointer_size * BITS_PER_BYTE);
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
                const TypeInfo& tuple = this->state_.checked.types.get(object_type);
                const TupleFieldIndexParse field_index = parse_tuple_field_index(field_name);
                if (!field_index.numeric) {
                    if (emit_diagnostics) {
                        this->report_general(projection_range, std::string(SEMA_TUPLE_FIELD_ACCESS_NUMERIC));
                    }
                } else if (!field_index.in_range || field_index.value >= tuple.tuple_elements.size()) {
                    if (emit_diagnostics) {
                        this->report_general(projection_range, std::string(SEMA_TUPLE_FIELD_ACCESS_OUT_OF_RANGE));
                    }
                } else {
                    output_type = tuple.tuple_elements[static_cast<base::usize>(field_index.value)];
                    output_is_place = projection_is_indirect || place.is_place;
                    output_is_writable = output_is_place && output_is_writable;
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
