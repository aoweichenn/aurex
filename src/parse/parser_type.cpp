#include <aurex/parse/parser_type_part.hpp>

#include <aurex/parse/parser_messages.hpp>
#include <aurex/parse/recovery.hpp>

#include <limits>
#include <string_view>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

constexpr int PARSER_TYPE_BINARY_RADIX = 2;
constexpr int PARSER_TYPE_DECIMAL_RADIX = 10;
constexpr int PARSER_TYPE_HEX_RADIX = 16;
constexpr base::usize PARSER_TYPE_RADIX_MARKER_OFFSET = 1;
constexpr base::usize PARSER_TYPE_RADIX_PREFIX_LENGTH = 2;
constexpr base::u64 PARSER_TYPE_DECIMAL_DIGIT_COUNT = 10;
constexpr char PARSER_TYPE_ASCII_ZERO = '0';
constexpr char PARSER_TYPE_ASCII_HEX_PREFIX_LOWER = 'x';
constexpr char PARSER_TYPE_ASCII_HEX_PREFIX_UPPER = 'X';
constexpr char PARSER_TYPE_ASCII_BINARY_PREFIX_LOWER = 'b';
constexpr char PARSER_TYPE_ASCII_BINARY_PREFIX_UPPER = 'B';
constexpr char PARSER_TYPE_ASCII_DECIMAL_FIRST = '0';
constexpr char PARSER_TYPE_ASCII_DECIMAL_LAST = '9';
constexpr char PARSER_TYPE_ASCII_HEX_LOWER_FIRST = 'a';
constexpr char PARSER_TYPE_ASCII_HEX_LOWER_LAST = 'f';
constexpr char PARSER_TYPE_ASCII_HEX_UPPER_FIRST = 'A';
constexpr char PARSER_TYPE_ASCII_HEX_UPPER_LAST = 'F';
constexpr char PARSER_TYPE_DIGIT_SEPARATOR = '_';
constexpr base::usize PARSER_TYPE_CONSTRUCTOR_STACK_INITIAL_CAPACITY = 8;

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
    int radix = PARSER_TYPE_DECIMAL_RADIX;
    base::usize begin = 0;
    if (text.size() > PARSER_TYPE_RADIX_PREFIX_LENGTH &&
        text.front() == PARSER_TYPE_ASCII_ZERO &&
        (text[PARSER_TYPE_RADIX_MARKER_OFFSET] == PARSER_TYPE_ASCII_HEX_PREFIX_LOWER ||
         text[PARSER_TYPE_RADIX_MARKER_OFFSET] == PARSER_TYPE_ASCII_HEX_PREFIX_UPPER)) {
        radix = PARSER_TYPE_HEX_RADIX;
        begin = PARSER_TYPE_RADIX_PREFIX_LENGTH;
    } else if (text.size() > PARSER_TYPE_RADIX_PREFIX_LENGTH &&
               text.front() == PARSER_TYPE_ASCII_ZERO &&
               (text[PARSER_TYPE_RADIX_MARKER_OFFSET] == PARSER_TYPE_ASCII_BINARY_PREFIX_LOWER ||
                text[PARSER_TYPE_RADIX_MARKER_OFFSET] == PARSER_TYPE_ASCII_BINARY_PREFIX_UPPER)) {
        radix = PARSER_TYPE_BINARY_RADIX;
        begin = PARSER_TYPE_RADIX_PREFIX_LENGTH;
    }
    for (base::usize i = begin; i < text.size(); ++i) {
        const char c = text[i];
        if (c == PARSER_TYPE_DIGIT_SEPARATOR) {
            continue;
        }
        base::u64 digit = 0;
        if (c >= PARSER_TYPE_ASCII_DECIMAL_FIRST && c <= PARSER_TYPE_ASCII_DECIMAL_LAST) {
            digit = static_cast<base::u64>(c - PARSER_TYPE_ASCII_DECIMAL_FIRST);
        } else if (c >= PARSER_TYPE_ASCII_HEX_LOWER_FIRST && c <= PARSER_TYPE_ASCII_HEX_LOWER_LAST) {
            digit = static_cast<base::u64>(PARSER_TYPE_DECIMAL_DIGIT_COUNT + c - PARSER_TYPE_ASCII_HEX_LOWER_FIRST);
        } else if (c >= PARSER_TYPE_ASCII_HEX_UPPER_FIRST && c <= PARSER_TYPE_ASCII_HEX_UPPER_LAST) {
            digit = static_cast<base::u64>(PARSER_TYPE_DECIMAL_DIGIT_COUNT + c - PARSER_TYPE_ASCII_HEX_UPPER_FIRST);
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

syntax::TypeId TypeParser::parse_type() {
    this->reset_panic();
    enum class TypeConstructorKind {
        pointer,
        array,
        slice,
    };

    struct TypeConstructor {
        TypeConstructorKind kind = TypeConstructorKind::pointer;
        base::SourceRange begin_range {};
        syntax::PointerMutability pointer_mutability = syntax::PointerMutability::const_;
        base::u64 array_count = 0;
    };

    std::vector<TypeConstructor> constructors;
    constructors.reserve(PARSER_TYPE_CONSTRUCTOR_STACK_INITIAL_CAPACITY);
    while (true) {
        if (this->match(TokenKind::star)) {
            const syntax::Token& begin = this->previous();
            syntax::PointerMutability mutability = syntax::PointerMutability::const_;
            if (this->match(TokenKind::kw_mut)) {
                mutability = syntax::PointerMutability::mut;
            } else if (this->match(TokenKind::kw_const)) {
                mutability = syntax::PointerMutability::const_;
            } else {
                this->report_here(std::string(PARSER_EXPECT_TYPE_POINTER_MUTABILITY));
            }
            constructors.push_back(TypeConstructor {
                TypeConstructorKind::pointer,
                begin.range,
                mutability,
                0,
            });
            continue;
        }

        if (this->match(TokenKind::l_bracket)) {
            const syntax::Token& begin = this->previous();
            if (this->match(TokenKind::r_bracket)) {
                syntax::PointerMutability mutability = syntax::PointerMutability::const_;
                if (this->match(TokenKind::kw_mut)) {
                    mutability = syntax::PointerMutability::mut;
                } else if (this->match(TokenKind::kw_const)) {
                    mutability = syntax::PointerMutability::const_;
                } else {
                    this->report_here(std::string(PARSER_EXPECT_TYPE_SLICE_MUTABILITY));
                }
                constructors.push_back(TypeConstructor {
                    TypeConstructorKind::slice,
                    begin.range,
                    mutability,
                    0,
                });
                continue;
            }
            const syntax::Token& count = this->expect(TokenKind::integer_literal, std::string(PARSER_EXPECT_ARRAY_LENGTH));
            this->expect_array_length_end();
            base::u64 array_count = 0;
            if (count.kind == TokenKind::integer_literal && !parse_u64_literal(count.text, array_count)) {
                this->report_at(count, std::string(PARSER_ARRAY_LENGTH_OUT_OF_RANGE));
            }
            constructors.push_back(TypeConstructor {
                TypeConstructorKind::array,
                begin.range,
                syntax::PointerMutability::const_,
                array_count,
            });
            continue;
        }
        break;
    }

    syntax::TypeId type = this->parse_type_atom();
    for (base::usize index = constructors.size(); index > 0; --index) {
        const TypeConstructor& constructor = constructors[index - 1];
        syntax::TypeNode node;
        node.range = this->merge(constructor.begin_range, this->type_range_or(type, constructor.begin_range));
        if (constructor.kind == TypeConstructorKind::pointer) {
            node.kind = syntax::TypeKind::pointer;
            node.pointer_mutability = constructor.pointer_mutability;
            node.pointee = type;
        } else if (constructor.kind == TypeConstructorKind::array) {
            node.kind = syntax::TypeKind::array;
            node.array_count = constructor.array_count;
            node.array_element = type;
        } else {
            node.kind = syntax::TypeKind::slice;
            node.slice_mutability = constructor.pointer_mutability;
            node.slice_element = type;
        }
        type = this->session_.module.push_type(node);
    }
    return type;
}

syntax::TypeId TypeParser::parse_type_atom() {
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
                this->expect_identifier_recovered(std::string(PARSER_EXPECT_TYPE_NAME_AFTER_SCOPE));
            type.scope_name = name.text;
            type.scope_range = name.range;
            type.name = scoped_name.text;
            type.range = this->merge(name.range, scoped_name.range);
        }
        if (this->match(TokenKind::l_bracket)) {
            if (this->check(TokenKind::r_bracket)) {
                this->report_here(std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT));
            }
            this->parse_generic_type_args(type.type_args);
            const syntax::Token& end = this->expect_recovered(
                TokenKind::r_bracket,
                std::string(PARSER_EXPECT_GENERIC_TYPE_ARGS_END),
                RecoveryContext::generic_type_argument
            );
            type.range = this->merge(type.range, end.range);
        } else if (this->check(TokenKind::less)) {
            this->reject_legacy_angle_type_args();
        }
        return this->session_.module.push_type(type);
    }

    this->report_here(std::string(PARSER_EXPECT_TYPE));
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::primitive;
    type.primitive = syntax::PrimitiveTypeKind::void_;
    type.range = this->peek().range;
    return this->session_.module.push_type(type);
}

void TypeParser::parse_generic_type_args(std::vector<syntax::TypeId>& args) {
    while (!this->is_eof() && !this->check(TokenKind::r_bracket)) {
        args.push_back(this->parse_type());
        this->reset_panic();
        if (!this->recover_generic_type_arg_separator()) {
            break;
        }
    }
}

bool TypeParser::recover_generic_type_arg_separator() {
    if (this->check(TokenKind::r_bracket)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }

    this->report_here(std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::generic_type_argument)) {
        this->synchronize(RecoveryContext::generic_type_argument);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_bracket);
    }
    this->reset_panic();
    return false;
}

void TypeParser::reject_legacy_angle_type_args() {
    const syntax::Token& begin = this->expect(TokenKind::less, std::string(PARSER_EXPECT_LEGACY_GENERIC_BEGIN));
    this->report_at(begin, std::string(PARSER_M2_LEGACY_ANGLE_GENERIC_UNSUPPORTED));
    while (!this->is_eof()) {
        if (this->match(TokenKind::greater)) {
            this->reset_panic();
            return;
        }
        if (this->check(TokenKind::r_paren) ||
            this->check(TokenKind::r_brace) ||
            this->check(TokenKind::r_bracket) ||
            this->check(TokenKind::equal) ||
            this->check(TokenKind::semicolon)) {
            this->reset_panic();
            return;
        }
        this->advance();
    }
}

void TypeParser::expect_array_length_end() {
    this->expect_recovered(
        TokenKind::r_bracket,
        std::string(PARSER_EXPECT_ARRAY_LENGTH_END),
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
