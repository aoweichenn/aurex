#include <aurex/parse/parser_type_part.hpp>

#include <aurex/parse/parser_messages.hpp>
#include <aurex/parse/recovery.hpp>

#include <limits>
#include <string_view>
#include <utility>

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
constexpr std::string_view PARSER_TYPE_INTEGER_SUFFIX_I8 = "i8";
constexpr std::string_view PARSER_TYPE_INTEGER_SUFFIX_I16 = "i16";
constexpr std::string_view PARSER_TYPE_INTEGER_SUFFIX_I32 = "i32";
constexpr std::string_view PARSER_TYPE_INTEGER_SUFFIX_I64 = "i64";
constexpr std::string_view PARSER_TYPE_INTEGER_SUFFIX_ISIZE = "isize";
constexpr std::string_view PARSER_TYPE_INTEGER_SUFFIX_U8 = "u8";
constexpr std::string_view PARSER_TYPE_INTEGER_SUFFIX_U16 = "u16";
constexpr std::string_view PARSER_TYPE_INTEGER_SUFFIX_U32 = "u32";
constexpr std::string_view PARSER_TYPE_INTEGER_SUFFIX_U64 = "u64";
constexpr std::string_view PARSER_TYPE_INTEGER_SUFFIX_USIZE = "usize";
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
    case TokenKind::kw_char:
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
    case TokenKind::kw_char: return syntax::PrimitiveTypeKind::char_;
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
    const auto suffix_is_valid = [](const std::string_view suffix) noexcept {
        return suffix.empty() ||
               suffix == PARSER_TYPE_INTEGER_SUFFIX_I8 ||
               suffix == PARSER_TYPE_INTEGER_SUFFIX_I16 ||
               suffix == PARSER_TYPE_INTEGER_SUFFIX_I32 ||
               suffix == PARSER_TYPE_INTEGER_SUFFIX_I64 ||
               suffix == PARSER_TYPE_INTEGER_SUFFIX_ISIZE ||
               suffix == PARSER_TYPE_INTEGER_SUFFIX_U8 ||
               suffix == PARSER_TYPE_INTEGER_SUFFIX_U16 ||
               suffix == PARSER_TYPE_INTEGER_SUFFIX_U32 ||
               suffix == PARSER_TYPE_INTEGER_SUFFIX_U64 ||
               suffix == PARSER_TYPE_INTEGER_SUFFIX_USIZE;
    };
    base::usize digit_end = begin;
    for (; digit_end < text.size(); ++digit_end) {
        const char c = text[digit_end];
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
            break;
        }
        if (digit >= static_cast<base::u64>(radix)) {
            return false;
        }
    }
    if (!suffix_is_valid(text.substr(digit_end))) {
        return false;
    }
    for (base::usize i = begin; i < digit_end; ++i) {
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
    if (this->check(TokenKind::kw_fn) || this->check(TokenKind::kw_extern)) {
        return this->parse_function_type();
    }
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

syntax::TypeId TypeParser::parse_function_type() {
    syntax::FunctionCallConv call_conv = syntax::FunctionCallConv::aurex;
    base::SourceRange begin_range = this->peek().range;
    if (this->match(TokenKind::kw_extern)) {
        begin_range = this->previous().range;
        call_conv = syntax::FunctionCallConv::c;
        this->expect_recovered(
            TokenKind::kw_c,
            std::string(PARSER_EXPECT_C_AFTER_EXTERN_FUNCTION_TYPE),
            RecoveryContext::type_annotation
        );
        this->expect_recovered(
            TokenKind::kw_fn,
            std::string(PARSER_EXPECT_FN_AFTER_EXTERN_C_FUNCTION_TYPE),
            RecoveryContext::parameter_list_start
        );
        return this->parse_function_type_after_fn(begin_range, call_conv);
    }

    const syntax::Token& begin = this->expect(TokenKind::kw_fn, std::string(PARSER_EXPECT_FN_KEYWORD));
    return this->parse_function_type_after_fn(begin.range, call_conv);
}

syntax::TypeId TypeParser::parse_function_type_after_fn(
    const base::SourceRange begin_range,
    const syntax::FunctionCallConv call_conv
) {
    this->expect_recovered(
        TokenKind::l_paren,
        std::string(PARSER_EXPECT_FUNCTION_TYPE_PARAM_LIST),
        RecoveryContext::parameter_list_start
    );
    std::vector<syntax::TypeId> params;
    bool is_variadic = false;
    if (!this->check(TokenKind::r_paren)) {
        this->parse_function_type_params(params, is_variadic);
    }
    this->expect_recovered(
        TokenKind::r_paren,
        std::string(PARSER_EXPECT_FUNCTION_TYPE_PARAM_LIST_END),
        RecoveryContext::parameter
    );
    const syntax::Token& arrow = this->expect_recovered(
        TokenKind::arrow,
        std::string(PARSER_EXPECT_FUNCTION_TYPE_RETURN_ARROW),
        RecoveryContext::type_annotation
    );
    const syntax::TypeId return_type = this->parse_type();

    syntax::TypeNode type;
    type.kind = syntax::TypeKind::function;
    type.range = this->merge(begin_range, this->type_range_or(return_type, arrow.range));
    type.function_call_conv = call_conv;
    type.function_is_variadic = is_variadic;
    type.function_params = std::move(params);
    type.function_return = return_type;
    return this->session_.module.push_type(std::move(type));
}

void TypeParser::parse_function_type_params(std::vector<syntax::TypeId>& params, bool& is_variadic) {
    while (!this->is_eof() && !this->check(TokenKind::r_paren)) {
        if (this->match(TokenKind::ellipsis)) {
            is_variadic = true;
            if (!this->check(TokenKind::r_paren)) {
                this->report_here(std::string(PARSER_VARIADIC_MARKER_MUST_BE_LAST));
                this->synchronize(RecoveryContext::parameter);
            }
            break;
        }
        if (this->check(TokenKind::identifier) && this->check_next(TokenKind::colon)) {
            this->advance();
            this->expect_type_annotation_colon(std::string(PARSER_EXPECT_PARAMETER_TYPE_COLON));
        }
        params.push_back(this->parse_type());
        this->reset_panic();
        if (!this->recover_function_type_param_separator(is_variadic)) {
            break;
        }
    }
}

bool TypeParser::recover_function_type_param_separator(bool& is_variadic) {
    if (this->check(TokenKind::r_paren)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        if (this->match(TokenKind::ellipsis)) {
            is_variadic = true;
            if (!this->check(TokenKind::r_paren)) {
                this->report_here(std::string(PARSER_VARIADIC_MARKER_MUST_BE_LAST));
                this->synchronize(RecoveryContext::parameter);
            }
            return false;
        }
        return !this->check(TokenKind::r_paren);
    }

    this->report_here(std::string(PARSER_EXPECT_FUNCTION_TYPE_PARAM_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::parameter)) {
        this->synchronize(RecoveryContext::parameter);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
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
