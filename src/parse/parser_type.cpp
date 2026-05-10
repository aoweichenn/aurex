#include <aurex/parse/parser_type_part.hpp>

#include <aurex/parse/recovery.hpp>

#include <limits>
#include <string_view>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

constexpr int kBinaryRadix = 2;
constexpr int kDecimalRadix = 10;
constexpr int kHexRadix = 16;
constexpr base::usize kRadixMarkerOffset = 1;
constexpr base::usize kRadixPrefixLength = 2;
constexpr base::u64 kDecimalDigitCount = 10;

[[nodiscard]] bool is_primitive_type_token(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::kw_void:
    case TokenKind::kw_bool:
    case TokenKind::kw_i8:
    case TokenKind::kw_u8:
    case TokenKind::kw_i16:
    case TokenKind::kw_u16:
    case TokenKind::kw_i32:
    case TokenKind::kw_u32:
    case TokenKind::kw_i64:
    case TokenKind::kw_u64:
    case TokenKind::kw_isize:
    case TokenKind::kw_usize:
    case TokenKind::kw_f32:
    case TokenKind::kw_f64:
    case TokenKind::kw_str:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] syntax::PrimitiveTypeKind primitive_from_token(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::kw_void: return syntax::PrimitiveTypeKind::void_;
    case TokenKind::kw_bool: return syntax::PrimitiveTypeKind::bool_;
    case TokenKind::kw_i8: return syntax::PrimitiveTypeKind::i8;
    case TokenKind::kw_u8: return syntax::PrimitiveTypeKind::u8;
    case TokenKind::kw_i16: return syntax::PrimitiveTypeKind::i16;
    case TokenKind::kw_u16: return syntax::PrimitiveTypeKind::u16;
    case TokenKind::kw_i32: return syntax::PrimitiveTypeKind::i32;
    case TokenKind::kw_u32: return syntax::PrimitiveTypeKind::u32;
    case TokenKind::kw_i64: return syntax::PrimitiveTypeKind::i64;
    case TokenKind::kw_u64: return syntax::PrimitiveTypeKind::u64;
    case TokenKind::kw_isize: return syntax::PrimitiveTypeKind::isize;
    case TokenKind::kw_usize: return syntax::PrimitiveTypeKind::usize;
    case TokenKind::kw_f32: return syntax::PrimitiveTypeKind::f32;
    case TokenKind::kw_f64: return syntax::PrimitiveTypeKind::f64;
    case TokenKind::kw_str: return syntax::PrimitiveTypeKind::str;
    default: return syntax::PrimitiveTypeKind::void_;
    }
}

[[nodiscard]] bool parse_u64_literal(const std::string_view text, base::u64& value) noexcept {
    value = 0;
    int radix = kDecimalRadix;
    base::usize begin = 0;
    if (text.size() > kRadixPrefixLength &&
        text.front() == '0' &&
        (text[kRadixMarkerOffset] == 'x' || text[kRadixMarkerOffset] == 'X')) {
        radix = kHexRadix;
        begin = kRadixPrefixLength;
    } else if (text.size() > kRadixPrefixLength &&
               text.front() == '0' &&
               (text[kRadixMarkerOffset] == 'b' || text[kRadixMarkerOffset] == 'B')) {
        radix = kBinaryRadix;
        begin = kRadixPrefixLength;
    }
    for (base::usize i = begin; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '_') {
            continue;
        }
        base::u64 digit = 0;
        if (c >= '0' && c <= '9') {
            digit = static_cast<base::u64>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = static_cast<base::u64>(kDecimalDigitCount + c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            digit = static_cast<base::u64>(kDecimalDigitCount + c - 'A');
        } else {
            return false;
        }
        if (digit >= static_cast<base::u64>(radix)) {
            return false;
        }
        if (value > (std::numeric_limits<base::u64>::max() - digit) / static_cast<base::u64>(radix)) {
            return false;
        }
        value = (value * static_cast<base::u64>(radix)) + digit;
    }
    return true;
}

} // namespace

std::vector<syntax::TypeId> TypeParser::parse_type_arg_list() {
    std::vector<syntax::TypeId> args;
    this->expect(TokenKind::less, "expected '<' before type argument list");
    while (!this->is_eof() && !this->check_type_arg_list_end()) {
        args.push_back(this->parse_type());
        this->reset_panic();
        if (!this->recover_type_arg_separator()) {
            break;
        }
    }
    this->expect_type_arg_list_end("expected '>' after type argument list");
    this->reset_panic();
    return args;
}

bool TypeParser::recover_type_arg_separator() {
    if (this->check_type_arg_list_end()) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        return !this->check_type_arg_list_end();
    }

    this->report_here("expected ',' or '>' after type argument");
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::type_argument)) {
        this->synchronize(RecoveryContext::type_argument);
    }
    if (this->match(TokenKind::comma)) {
        return !this->check_type_arg_list_end();
    }
    return false;
}

syntax::TypeId TypeParser::parse_type() {
    this->reset_panic();
    if (is_primitive_type_token(this->peek().kind)) {
        return this->parse_primitive_type();
    }
    if (this->check(TokenKind::identifier)) {
        const syntax::Token& name = this->advance();
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::named;
        type.range = name.range;
        type.name = name.text;
        if (this->match(TokenKind::colon_colon)) {
            const syntax::Token& scoped_name =
                this->expect_identifier_recovered("expected type name after '::'");
            type.scope_name = name.text;
            type.scope_range = name.range;
            type.name = scoped_name.text;
            type.range = this->merge(name.range, scoped_name.range);
        }
        if (this->check(TokenKind::less)) {
            type.type_args = this->parse_type_arg_list();
            type.range = this->merge(type.range, this->previous().range);
        }
        return this->session_.module.push_type(type);
    }
    if (this->match(TokenKind::star)) {
        const syntax::Token& begin = this->previous();
        syntax::PointerMutability mutability = syntax::PointerMutability::const_;
        if (this->match(TokenKind::kw_mut)) {
            mutability = syntax::PointerMutability::mut;
        } else if (this->match(TokenKind::kw_const)) {
            mutability = syntax::PointerMutability::const_;
        } else {
            this->report_here("expected 'mut' or 'const' after '*'");
        }
        const syntax::TypeId pointee = this->parse_type();
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::pointer;
        type.range = this->merge(begin.range, this->type_range_or(pointee, begin.range));
        type.pointer_mutability = mutability;
        type.pointee = pointee;
        return this->session_.module.push_type(type);
    }
    if (this->match(TokenKind::l_bracket)) {
        const syntax::Token& begin = this->previous();
        const syntax::Token& count = this->expect(TokenKind::integer_literal, "expected array length");
        this->expect_array_length_end();
        const syntax::TypeId element = this->parse_type();
        base::u64 array_count = 0;
        if (count.kind == TokenKind::integer_literal && !parse_u64_literal(count.text, array_count)) {
            this->report_at(count, "array length literal is out of range");
        }
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::array;
        type.range = this->merge(begin.range, this->type_range_or(element, begin.range));
        type.array_count = array_count;
        type.array_element = element;
        return this->session_.module.push_type(type);
    }

    this->report_here("expected type");
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::primitive;
    type.primitive = syntax::PrimitiveTypeKind::void_;
    type.range = this->peek().range;
    return this->session_.module.push_type(type);
}

void TypeParser::expect_array_length_end() {
    this->expect_recovered(
        TokenKind::r_bracket,
        "expected ']' after array length",
        RecoveryContext::array_type_length
    );
}

syntax::TypeId TypeParser::parse_primitive_type() {
    const syntax::Token& token = this->advance();
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::primitive;
    type.range = token.range;
    type.primitive = primitive_from_token(token.kind);
    return this->session_.module.push_type(type);
}

} // namespace aurex::parse
