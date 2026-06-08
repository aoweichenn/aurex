#include <aurex/frontend/parse/parser_messages.hpp>
#include <aurex/frontend/parse/parser_type_part.hpp>
#include <aurex/frontend/parse/recovery.hpp>

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
constexpr base::usize PARSER_MAX_TYPE_NESTING_DEPTH = 512;

class TypeNestingGuard final {
public:
    explicit TypeNestingGuard(ParseSession& session) noexcept : session_(&session)
    {
        ++this->session_->type_nesting_depth;
    }

    ~TypeNestingGuard() noexcept
    {
        --this->session_->type_nesting_depth;
    }

    TypeNestingGuard(const TypeNestingGuard&) = delete;
    TypeNestingGuard& operator=(const TypeNestingGuard&) = delete;
    TypeNestingGuard(TypeNestingGuard&&) = delete;
    TypeNestingGuard& operator=(TypeNestingGuard&&) = delete;

private:
    ParseSession* session_;
};

[[nodiscard]] bool is_primitive_type_token(const TokenKind kind) noexcept
{
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

[[nodiscard]] syntax::PrimitiveTypeKind primitive_from_token(const TokenKind kind) noexcept
{
    switch (kind) {
        case TokenKind::kw_void:
            return syntax::PrimitiveTypeKind::void_;
        case TokenKind::kw_bool:
            return syntax::PrimitiveTypeKind::bool_;
        case TokenKind::kw_i8:
            return syntax::PrimitiveTypeKind::i8;
        case TokenKind::kw_u8:
            return syntax::PrimitiveTypeKind::u8;
        case TokenKind::kw_i16:
            return syntax::PrimitiveTypeKind::i16;
        case TokenKind::kw_u16:
            return syntax::PrimitiveTypeKind::u16;
        case TokenKind::kw_i32:
            return syntax::PrimitiveTypeKind::i32;
        case TokenKind::kw_u32:
            return syntax::PrimitiveTypeKind::u32;
        case TokenKind::kw_i64:
            return syntax::PrimitiveTypeKind::i64;
        case TokenKind::kw_u64:
            return syntax::PrimitiveTypeKind::u64;
        case TokenKind::kw_isize:
            return syntax::PrimitiveTypeKind::isize;
        case TokenKind::kw_usize:
            return syntax::PrimitiveTypeKind::usize;
        case TokenKind::kw_f32:
            return syntax::PrimitiveTypeKind::f32;
        case TokenKind::kw_f64:
            return syntax::PrimitiveTypeKind::f64;
        case TokenKind::kw_str:
            return syntax::PrimitiveTypeKind::str;
        case TokenKind::kw_char:
            return syntax::PrimitiveTypeKind::char_;
        default:
            return syntax::PrimitiveTypeKind::void_;
    }
}

[[nodiscard]] bool parse_u64_literal(const std::string_view text, base::u64& value) noexcept
{
    value = 0;
    int radix = PARSER_TYPE_DECIMAL_RADIX;
    base::usize begin = 0;
    if (text.size() > PARSER_TYPE_RADIX_PREFIX_LENGTH && text.front() == PARSER_TYPE_ASCII_ZERO
        && (text[PARSER_TYPE_RADIX_MARKER_OFFSET] == PARSER_TYPE_ASCII_HEX_PREFIX_LOWER
            || text[PARSER_TYPE_RADIX_MARKER_OFFSET] == PARSER_TYPE_ASCII_HEX_PREFIX_UPPER)) {
        radix = PARSER_TYPE_HEX_RADIX;
        begin = PARSER_TYPE_RADIX_PREFIX_LENGTH;
    } else if (text.size() > PARSER_TYPE_RADIX_PREFIX_LENGTH && text.front() == PARSER_TYPE_ASCII_ZERO
        && (text[PARSER_TYPE_RADIX_MARKER_OFFSET] == PARSER_TYPE_ASCII_BINARY_PREFIX_LOWER
            || text[PARSER_TYPE_RADIX_MARKER_OFFSET] == PARSER_TYPE_ASCII_BINARY_PREFIX_UPPER)) {
        radix = PARSER_TYPE_BINARY_RADIX;
        begin = PARSER_TYPE_RADIX_PREFIX_LENGTH;
    }
    const auto suffix_is_valid = [](const std::string_view suffix) noexcept {
        return suffix.empty() || suffix == PARSER_TYPE_INTEGER_SUFFIX_I8 || suffix == PARSER_TYPE_INTEGER_SUFFIX_I16
            || suffix == PARSER_TYPE_INTEGER_SUFFIX_I32 || suffix == PARSER_TYPE_INTEGER_SUFFIX_I64
            || suffix == PARSER_TYPE_INTEGER_SUFFIX_ISIZE || suffix == PARSER_TYPE_INTEGER_SUFFIX_U8
            || suffix == PARSER_TYPE_INTEGER_SUFFIX_U16 || suffix == PARSER_TYPE_INTEGER_SUFFIX_U32
            || suffix == PARSER_TYPE_INTEGER_SUFFIX_U64 || suffix == PARSER_TYPE_INTEGER_SUFFIX_USIZE;
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

syntax::TypeId TypeParser::parse_type()
{
    this->reset_panic();
    if (this->session_.type_nesting_depth >= PARSER_MAX_TYPE_NESTING_DEPTH) {
        const base::SourceRange range = this->peek().range;
        this->report_here(std::string(PARSER_TYPE_NESTING_LIMIT));
        this->synchronize(RecoveryContext::type_annotation);
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::primitive;
        type.primitive = syntax::PrimitiveTypeKind::void_;
        type.range = range;
        return this->session_.module.push_type(type);
    }
    const TypeNestingGuard nesting_guard(this->session_);
    enum class TypeConstructorKind {
        pointer,
        reference,
        array,
        slice,
    };

    struct TypeConstructor {
        TypeConstructorKind kind = TypeConstructorKind::pointer;
        base::SourceRange begin_range{};
        syntax::PointerMutability pointer_mutability = syntax::PointerMutability::const_;
        base::u64 array_count = 0;
        syntax::TypeOriginQualifier reference_origin;
    };

    const auto reference_origin_qualifier_follows = [&]() noexcept {
        if (this->peek().kind != TokenKind::l_bracket || this->peek_at(1).kind != TokenKind::identifier) {
            return false;
        }
        base::usize offset = 2;
        while (this->peek_at(offset).kind == TokenKind::pipe) {
            if (this->peek_at(offset + 1U).kind != TokenKind::identifier) {
                return false;
            }
            offset += 2U;
        }
        return this->peek_at(offset).kind == TokenKind::r_bracket;
    };

    const auto parse_reference_origin_qualifier = [&]() {
        syntax::TypeOriginQualifier qualifier;
        if (!reference_origin_qualifier_follows()) {
            return qualifier;
        }
        const syntax::Token& begin = this->expect(TokenKind::l_bracket, std::string(PARSER_EXPECT_TYPE));
        qualifier.explicit_ = true;
        qualifier.range = begin.range;
        while (!this->is_eof() && !this->check(TokenKind::r_bracket)) {
            const syntax::Token& origin = this->expect_identifier_recovered(std::string(PARSER_EXPECT_TYPE));
            if (origin.kind == TokenKind::identifier) {
                qualifier.names.push_back(origin.text());
                qualifier.name_ids.push_back(syntax::INVALID_IDENT_ID);
                qualifier.ranges.push_back(origin.range);
                qualifier.range = this->merge(qualifier.range, origin.range);
            }
            if (!this->match(TokenKind::pipe)) {
                break;
            }
        }
        const syntax::Token& end = this->expect(TokenKind::r_bracket, std::string(PARSER_EXPECT_TYPE));
        qualifier.range = this->merge(qualifier.range, end.range);
        return qualifier;
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
            constructors.push_back(TypeConstructor{
                TypeConstructorKind::pointer,
                begin.range,
                mutability,
                0,
                {},
            });
            continue;
        }

        if (this->match(TokenKind::amp)) {
            const syntax::Token& begin = this->previous();
            syntax::PointerMutability mutability = syntax::PointerMutability::const_;
            if (this->match(TokenKind::kw_mut)) {
                mutability = syntax::PointerMutability::mut;
            }
            syntax::TypeOriginQualifier origin = parse_reference_origin_qualifier();
            constructors.push_back(TypeConstructor{
                TypeConstructorKind::reference,
                begin.range,
                mutability,
                0,
                std::move(origin),
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
                constructors.push_back(TypeConstructor{
                    TypeConstructorKind::slice,
                    begin.range,
                    mutability,
                    0,
                    {},
                });
                continue;
            }
            const syntax::Token& count =
                this->expect(TokenKind::integer_literal, std::string(PARSER_EXPECT_ARRAY_LENGTH));
            this->expect_array_length_end();
            base::u64 array_count = 0;
            if (count.kind == TokenKind::integer_literal && !parse_u64_literal(count.text(), array_count)) {
                this->report_at(count, std::string(PARSER_ARRAY_LENGTH_OUT_OF_RANGE));
            }
            constructors.push_back(TypeConstructor{
                TypeConstructorKind::array,
                begin.range,
                syntax::PointerMutability::const_,
                array_count,
                {},
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
        } else if (constructor.kind == TypeConstructorKind::reference) {
            node.kind = syntax::TypeKind::reference;
            node.pointer_mutability = constructor.pointer_mutability;
            node.pointee = type;
            node.reference_origin = constructor.reference_origin;
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

syntax::TypeId TypeParser::parse_type_atom()
{
    if (this->check(TokenKind::kw_fn) || this->check(TokenKind::kw_extern) || this->check(TokenKind::kw_unsafe)) {
        return this->parse_function_type();
    }
    if (this->check(TokenKind::l_paren)) {
        return this->parse_tuple_or_parenthesized_type();
    }
    if (this->check(TokenKind::kw_dyn)) {
        return this->parse_dyn_trait_type();
    }
    if (is_primitive_type_token(this->peek().kind)) {
        return this->parse_primitive_type();
    }
    if (this->check(TokenKind::identifier)) {
        return this->parse_named_type();
    }

    this->report_here(std::string(PARSER_EXPECT_TYPE));
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::primitive;
    type.primitive = syntax::PrimitiveTypeKind::void_;
    type.range = this->peek().range;
    return this->session_.module.push_type(type);
}

syntax::TypeId TypeParser::parse_named_type()
{
    std::vector<syntax::Token> parts;
    parts.push_back(this->advance());
    while (this->match(TokenKind::dot)) {
        parts.push_back(this->expect_identifier_recovered(std::string(PARSER_EXPECT_TYPE_NAME_AFTER_SCOPE)));
    }

    syntax::TypeNode type;
    type.kind = syntax::TypeKind::named;
    type.range = this->merge(parts.front().range, parts.back().range);
    type.name = parts.back().text();
    if (parts.size() > 1) {
        type.scope_range = this->merge(parts.front().range, parts[parts.size() - 2].range);
        type.scope_parts.reserve(parts.size() - 1);
        for (base::usize i = 0; i + 1 < parts.size(); ++i) {
            type.scope_parts.push_back(parts[i].text());
        }
        type.scope_name = type.scope_parts.front();
    }

    if (this->match(TokenKind::l_bracket)) {
        const syntax::Token& generic_begin = this->previous();
        if (this->check(TokenKind::r_bracket)) {
            this->report_here(std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT));
        }
        this->parse_generic_type_args(type.type_args);
        const syntax::Token& end = this->expect_recovered_after(TokenKind::r_bracket,
            std::string(PARSER_EXPECT_GENERIC_TYPE_ARGS_END), RecoveryContext::generic_type_argument, generic_begin);
        type.range = this->merge(type.range, end.range);
    } else if (this->check(TokenKind::less)) {
        this->reject_legacy_angle_type_args();
    }
    return this->session_.module.push_type(type);
}

syntax::TypeId TypeParser::parse_dyn_trait_type()
{
    const syntax::Token& begin = this->expect(TokenKind::kw_dyn, std::string(PARSER_EXPECT_TYPE));
    if (this->match(TokenKind::l_paren)) {
        const syntax::Token& composition_begin = this->previous();
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::dyn_trait;
        type.range = this->merge(begin.range, composition_begin.range);
        if (this->check(TokenKind::r_paren)) {
            this->report_here(std::string(PARSER_EXPECT_TYPE));
        }
        while (!this->is_eof() && !this->check(TokenKind::r_paren)) {
            const base::SourceRange principal_begin = this->peek().range;
            const syntax::TypeId principal = this->parse_dyn_trait_principal_type(principal_begin);
            syntax::DynTraitPrincipalDecl decl;
            decl.trait_type = principal;
            decl.range = this->type_range_or(principal, this->peek().range);
            type.dyn_trait_principals.push_back(decl);
            type.range = this->merge(type.range, decl.range);
            this->reset_panic();
            if (!this->recover_dyn_trait_principal_separator()) {
                break;
            }
        }
        const syntax::Token& end = this->expect_recovered_after(TokenKind::r_paren,
            std::string(PARSER_EXPECT_DYN_TRAIT_COMPOSITION_END), RecoveryContext::type_annotation,
            composition_begin);
        type.range = this->merge(type.range, end.range);
        return this->session_.module.push_type(std::move(type));
    }
    return this->parse_dyn_trait_principal_type(begin.range);
}

syntax::TypeId TypeParser::parse_dyn_trait_principal_type(const base::SourceRange& begin_range)
{
    std::vector<syntax::Token> parts;
    parts.push_back(this->expect_identifier_recovered(std::string(PARSER_EXPECT_TYPE)));
    while (this->match(TokenKind::dot)) {
        parts.push_back(this->expect_identifier_recovered(std::string(PARSER_EXPECT_TYPE_NAME_AFTER_SCOPE)));
    }

    syntax::TypeNode type;
    type.kind = syntax::TypeKind::dyn_trait;
    type.range = this->merge(begin_range, parts.back().range);
    type.name = parts.back().text();
    if (parts.size() > 1) {
        type.scope_range = this->merge(parts.front().range, parts[parts.size() - 2].range);
        type.scope_parts.reserve(parts.size() - 1);
        for (base::usize i = 0; i + 1 < parts.size(); ++i) {
            type.scope_parts.push_back(parts[i].text());
        }
        type.scope_name = type.scope_parts.front();
    }

    if (this->match(TokenKind::l_bracket)) {
        const syntax::Token& args_begin = this->previous();
        if (this->check(TokenKind::r_bracket)) {
            this->report_here(std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT));
        }
        this->parse_dyn_trait_args(type);
        const syntax::Token& end = this->expect_recovered_after(TokenKind::r_bracket,
            std::string(PARSER_EXPECT_GENERIC_TYPE_ARGS_END), RecoveryContext::generic_type_argument, args_begin);
        type.range = this->merge(type.range, end.range);
    } else if (this->check(TokenKind::less)) {
        this->reject_legacy_angle_type_args();
    }
    return this->session_.module.push_type(std::move(type));
}

bool TypeParser::recover_dyn_trait_principal_separator() const
{
    if (this->check(TokenKind::r_paren)) {
        return false;
    }
    if (this->match(TokenKind::plus)) {
        this->reset_panic();
        if (this->check(TokenKind::r_paren)) {
            this->report_here(std::string(PARSER_EXPECT_TYPE));
            return false;
        }
        return !this->check(TokenKind::r_paren);
    }

    this->report_here(std::string(PARSER_EXPECT_DYN_TRAIT_COMPOSITION_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::type_annotation)) {
        this->synchronize(RecoveryContext::type_annotation);
    }
    if (this->match(TokenKind::plus)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }
    this->reset_panic();
    return false;
}

void TypeParser::parse_dyn_trait_args(syntax::TypeNode& type)
{
    while (!this->is_eof() && !this->check(TokenKind::r_bracket)) {
        if (this->check(TokenKind::identifier) && this->check_next(TokenKind::equal)) {
            const syntax::Token& name = this->advance();
            this->expect(TokenKind::equal, std::string(PARSER_EXPECT_ASSOCIATED_TYPE_CONSTRAINT_EQUAL));
            const syntax::TypeId value_type = this->parse_type();
            syntax::AssociatedTypeConstraintDecl constraint;
            constraint.name = name.text();
            constraint.name_range = name.range;
            constraint.value_type = value_type;
            constraint.range = this->merge(name.range, this->type_range_or(value_type, name.range));
            type.associated_type_constraints.push_back(std::move(constraint));
        } else {
            type.type_args.push_back(this->parse_type());
        }
        this->reset_panic();
        if (!this->recover_dyn_trait_arg_separator()) {
            break;
        }
    }
}

bool TypeParser::recover_dyn_trait_arg_separator() const
{
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

syntax::TypeId TypeParser::parse_tuple_or_parenthesized_type()
{
    const syntax::Token& begin = this->expect(TokenKind::l_paren, std::string(PARSER_EXPECT_TUPLE_TYPE_END));
    if (this->check(TokenKind::r_paren)) {
        this->report_here(std::string(PARSER_EMPTY_TUPLE_TYPE_UNSUPPORTED));
        const syntax::Token& end = this->expect_tuple_type_end(begin);
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::primitive;
        type.primitive = syntax::PrimitiveTypeKind::void_;
        type.range = this->merge(begin.range, end.range);
        return this->session_.module.push_type(type);
    }

    const syntax::TypeId first = this->parse_type();
    if (!this->match(TokenKind::comma)) {
        static_cast<void>(this->expect_tuple_type_end(begin));
        return first;
    }

    syntax::TypeNode type;
    type.kind = syntax::TypeKind::tuple;
    type.tuple_elements.push_back(first);
    while (!this->is_eof() && !this->check(TokenKind::r_paren)) {
        type.tuple_elements.push_back(this->parse_type());
        this->reset_panic();
        if (!this->recover_tuple_type_separator()) {
            break;
        }
    }
    const syntax::Token& end = this->expect_tuple_type_end(begin);
    type.range = this->merge(begin.range, end.range);
    return this->session_.module.push_type(std::move(type));
}

bool TypeParser::recover_tuple_type_separator() const
{
    if (this->check(TokenKind::r_paren)) {
        return false;
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }

    this->report_here(std::string(PARSER_EXPECT_TUPLE_TYPE_SEPARATOR));
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::type_annotation)) {
        this->synchronize(RecoveryContext::type_annotation);
    }
    if (this->match(TokenKind::comma)) {
        this->reset_panic();
        return !this->check(TokenKind::r_paren);
    }
    this->reset_panic();
    return false;
}

const syntax::Token& TypeParser::expect_tuple_type_end(const syntax::Token& opening) const
{
    return this->expect_recovered_after(
        TokenKind::r_paren, std::string(PARSER_EXPECT_TUPLE_TYPE_END), RecoveryContext::type_annotation, opening);
}

void TypeParser::parse_generic_type_args(std::vector<syntax::TypeId>& args)
{
    while (!this->is_eof() && !this->check(TokenKind::r_bracket)) {
        args.push_back(this->parse_type());
        this->reset_panic();
        if (!this->recover_generic_type_arg_separator()) {
            break;
        }
    }
}

bool TypeParser::recover_generic_type_arg_separator() const
{
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

syntax::TypeId TypeParser::parse_function_type()
{
    syntax::FunctionCallConv call_conv = syntax::FunctionCallConv::aurex;
    bool is_unsafe = false;
    base::SourceRange begin_range = this->peek().range;
    if (this->match(TokenKind::kw_unsafe)) {
        begin_range = this->previous().range;
        is_unsafe = true;
    }
    if (this->match(TokenKind::kw_extern)) {
        if (!is_unsafe) {
            begin_range = this->previous().range;
        }
        call_conv = syntax::FunctionCallConv::c;
        this->expect_contextual_c_keyword_recovered(
            std::string(PARSER_EXPECT_C_AFTER_EXTERN_FUNCTION_TYPE), RecoveryContext::type_annotation);
        this->expect_recovered(TokenKind::kw_fn, std::string(PARSER_EXPECT_FN_AFTER_EXTERN_C_FUNCTION_TYPE),
            RecoveryContext::parameter_list_start);
        return this->parse_function_type_after_fn(begin_range, call_conv, is_unsafe);
    }

    const syntax::Token& begin = this->expect(TokenKind::kw_fn, std::string(PARSER_EXPECT_FN_KEYWORD));
    return this->parse_function_type_after_fn(is_unsafe ? begin_range : begin.range, call_conv, is_unsafe);
}

syntax::TypeId TypeParser::parse_function_type_after_fn(
    const base::SourceRange& begin_range, const syntax::FunctionCallConv call_conv, const bool is_unsafe)
{
    this->expect_recovered(
        TokenKind::l_paren, std::string(PARSER_EXPECT_FUNCTION_TYPE_PARAM_LIST), RecoveryContext::parameter_list_start);
    std::vector<syntax::TypeId> params;
    bool is_variadic = false;
    if (!this->check(TokenKind::r_paren)) {
        this->parse_function_type_params(params, is_variadic);
    }
    this->expect_recovered(
        TokenKind::r_paren, std::string(PARSER_EXPECT_FUNCTION_TYPE_PARAM_LIST_END), RecoveryContext::parameter);
    const syntax::Token& arrow = this->expect_recovered(
        TokenKind::arrow, std::string(PARSER_EXPECT_FUNCTION_TYPE_RETURN_ARROW), RecoveryContext::type_annotation);
    const syntax::TypeId return_type = this->parse_type();

    syntax::TypeNode type;
    type.kind = syntax::TypeKind::function;
    type.range = this->merge(begin_range, this->type_range_or(return_type, arrow.range));
    type.function_call_conv = call_conv;
    type.function_is_unsafe = is_unsafe;
    type.function_is_variadic = is_variadic;
    type.function_params = std::move(params);
    type.function_return = return_type;
    return this->session_.module.push_type(std::move(type));
}

void TypeParser::parse_function_type_params(std::vector<syntax::TypeId>& params, bool& is_variadic)
{
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

bool TypeParser::recover_function_type_param_separator(bool& is_variadic) const
{
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

void TypeParser::reject_legacy_angle_type_args() const
{
    const syntax::Token& begin = this->expect(TokenKind::less, std::string(PARSER_EXPECT_LEGACY_GENERIC_BEGIN));
    this->report_at(begin, std::string(PARSER_M2_LEGACY_ANGLE_GENERIC_UNSUPPORTED));
    while (!this->is_eof()) {
        if (this->match(TokenKind::greater)) {
            this->reset_panic();
            return;
        }
        if (this->check(TokenKind::r_paren) || this->check(TokenKind::r_brace) || this->check(TokenKind::r_bracket)
            || this->check(TokenKind::equal) || this->check(TokenKind::semicolon)) {
            this->reset_panic();
            return;
        }
        this->advance();
    }
}

void TypeParser::expect_array_length_end() const
{
    this->expect_recovered(
        TokenKind::r_bracket, std::string(PARSER_EXPECT_ARRAY_LENGTH_END), RecoveryContext::array_type_length);
}

syntax::TypeId TypeParser::parse_primitive_type() const
{
    const syntax::Token& token = this->advance();
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::primitive;
    type.range = token.range;
    type.primitive = primitive_from_token(token.kind);
    return this->session_.module.push_type(type);
}

} // namespace aurex::parse
