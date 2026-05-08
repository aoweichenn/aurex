#include "aurex/parse/parser_parts.hpp"

#include <limits>
#include <string_view>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

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
    int base = 10;
    base::usize begin = 0;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        begin = 2;
    } else if (text.size() > 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
        base = 2;
        begin = 2;
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
            digit = static_cast<base::u64>(10 + c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            digit = static_cast<base::u64>(10 + c - 'A');
        } else {
            return false;
        }
        if (digit >= static_cast<base::u64>(base)) {
            return false;
        }
        if (value > (std::numeric_limits<base::u64>::max() - digit) / static_cast<base::u64>(base)) {
            return false;
        }
        value = (value * static_cast<base::u64>(base)) + digit;
    }
    return true;
}

} // namespace

std::vector<syntax::TypeId> TypeParser::parse_type_arg_list() {
    std::vector<syntax::TypeId> args;
    expect(TokenKind::less, "expected '<' before type argument list");
    if (!check_type_arg_list_end()) {
        do {
            args.push_back(parse_type());
            reset_panic();
            if (check_type_arg_list_end()) {
                break;
            }
        } while (match(TokenKind::comma) && !check_type_arg_list_end());
    }
    expect_type_arg_list_end("expected '>' after type argument list");
    reset_panic();
    return args;
}

syntax::TypeId TypeParser::parse_type() {
    reset_panic();
    if (is_primitive_type_token(peek().kind)) {
        return parse_primitive_type();
    }
    if (check(TokenKind::identifier)) {
        const syntax::Token& name = advance();
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::named;
        type.range = name.range;
        type.name = name.text;
        if (match(TokenKind::colon_colon)) {
            const syntax::Token& scoped_name = expect(TokenKind::identifier, "expected type name after '::'");
            type.scope_name = name.text;
            type.scope_range = name.range;
            type.name = scoped_name.text;
            type.range = merge(name.range, scoped_name.range);
        }
        if (check(TokenKind::less)) {
            type.type_args = parse_type_arg_list();
            type.range = merge(type.range, previous().range);
        }
        return session_.module.push_type(type);
    }
    if (match(TokenKind::star)) {
        const syntax::Token& begin = previous();
        syntax::PointerMutability mutability = syntax::PointerMutability::const_;
        if (match(TokenKind::kw_mut)) {
            mutability = syntax::PointerMutability::mut;
        } else if (match(TokenKind::kw_const)) {
            mutability = syntax::PointerMutability::const_;
        } else {
            report_here("expected 'mut' or 'const' after '*'");
        }
        const syntax::TypeId pointee = parse_type();
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::pointer;
        type.range = merge(begin.range, session_.module.types[pointee.value].range);
        type.pointer_mutability = mutability;
        type.pointee = pointee;
        return session_.module.push_type(type);
    }
    if (match(TokenKind::l_bracket)) {
        const syntax::Token& begin = previous();
        const syntax::Token& count = expect(TokenKind::integer_literal, "expected array length");
        expect(TokenKind::r_bracket, "expected ']' after array length");
        const syntax::TypeId element = parse_type();
        base::u64 array_count = 0;
        if (count.kind == TokenKind::integer_literal && !parse_u64_literal(count.text, array_count)) {
            report_at(count, "array length literal is out of range");
        }
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::array;
        type.range = merge(begin.range, session_.module.types[element.value].range);
        type.array_count = array_count;
        type.array_element = element;
        return session_.module.push_type(type);
    }

    report_here("expected type");
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::primitive;
    type.primitive = syntax::PrimitiveTypeKind::void_;
    type.range = peek().range;
    return session_.module.push_type(type);
}

syntax::TypeId TypeParser::parse_primitive_type() {
    const syntax::Token& token = advance();
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::primitive;
    type.range = token.range;
    type.primitive = primitive_from_token(token.kind);
    return session_.module.push_type(type);
}

} // namespace aurex::parse
