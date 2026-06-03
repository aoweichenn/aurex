#pragma once

#include <aurex/frontend/syntax/ast/arena.hpp>
#include <aurex/frontend/syntax/core/ast_ids.hpp>
#include <aurex/frontend/syntax/core/identifier.hpp>
#include <aurex/infrastructure/base/source.hpp>

#include <string_view>
#include <vector>

namespace aurex::syntax {

enum class PrimitiveTypeKind {
    void_,
    bool_,
    i8,
    u8,
    i16,
    u16,
    i32,
    u32,
    i64,
    u64,
    isize,
    usize,
    f32,
    f64,
    str,
    char_,
};

enum class PointerMutability {
    mut,
    const_,
};

enum class FunctionCallConv {
    aurex,
    c,
};

inline constexpr base::u8 VISIBILITY_RANK_PRIVATE = 0;
inline constexpr base::u8 VISIBILITY_RANK_PACKAGE = 1;
inline constexpr base::u8 VISIBILITY_RANK_PUBLIC = 2;

enum class Visibility {
    private_,
    package_,
    public_,
};

[[nodiscard]] inline constexpr base::u8 visibility_rank(const Visibility visibility) noexcept
{
    switch (visibility) {
        case Visibility::private_:
            return VISIBILITY_RANK_PRIVATE;
        case Visibility::package_:
            return VISIBILITY_RANK_PACKAGE;
        case Visibility::public_:
            return VISIBILITY_RANK_PUBLIC;
    }
    return VISIBILITY_RANK_PRIVATE;
}

[[nodiscard]] inline constexpr bool visibility_at_least(const Visibility actual, const Visibility required) noexcept
{
    return visibility_rank(actual) >= visibility_rank(required);
}

[[nodiscard]] inline constexpr Visibility effective_visibility(const Visibility owner, const Visibility member) noexcept
{
    return visibility_rank(owner) < visibility_rank(member) ? owner : member;
}

[[nodiscard]] inline constexpr bool visibility_is_public(const Visibility visibility) noexcept
{
    return visibility == Visibility::public_;
}

[[nodiscard]] inline constexpr bool visibility_is_module_private(const Visibility visibility) noexcept
{
    return visibility == Visibility::private_;
}

[[nodiscard]] inline constexpr std::string_view visibility_name(const Visibility visibility) noexcept
{
    switch (visibility) {
        case Visibility::private_:
            return "priv";
        case Visibility::package_:
            return "pub(package)";
        case Visibility::public_:
            return "pub";
    }
    return "priv";
}

enum class TypeKind {
    primitive,
    named,
    pointer,
    reference,
    array,
    slice,
    tuple,
    function,
};

enum class GenericParamKind {
    type,
    origin,
};

struct GenericParamDecl {
    std::string_view name;
    base::SourceRange range{};
    IdentId name_id = INVALID_IDENT_ID;
    GenericParamKind kind = GenericParamKind::type;
};

struct TypeOriginQualifier {
    std::vector<std::string_view> names;
    std::vector<IdentId> name_ids;
    std::vector<base::SourceRange> ranges;
    base::SourceRange range{};
    bool explicit_ = false;
};

struct AssociatedTypeConstraintDecl {
    std::string_view name;
    base::SourceRange name_range{};
    TypeId value_type = INVALID_TYPE_ID;
    base::SourceRange range{};
    IdentId name_id = INVALID_IDENT_ID;
};

struct GenericConstraintDecl {
    std::string_view param_name;
    base::SourceRange param_range{};
    AstArenaVector<std::string_view> capability_names;
    AstArenaVector<base::SourceRange> capability_ranges;
    std::vector<std::vector<AssociatedTypeConstraintDecl>> capability_associated_constraints;
    base::SourceRange range{};
    IdentId param_name_id = INVALID_IDENT_ID;
    AstArenaVector<IdentId> capability_name_ids;
};

struct TypeNode {
    TypeKind kind = TypeKind::named;
    base::SourceRange range{};
    PrimitiveTypeKind primitive = PrimitiveTypeKind::void_;
    std::string_view scope_name;
    base::SourceRange scope_range{};
    std::vector<std::string_view> scope_parts;
    std::string_view name;
    IdentId scope_name_id = INVALID_IDENT_ID;
    std::vector<IdentId> scope_part_ids;
    IdentId name_id = INVALID_IDENT_ID;
    std::vector<TypeId> type_args;
    PointerMutability pointer_mutability = PointerMutability::const_;
    TypeId pointee = INVALID_TYPE_ID;
    TypeOriginQualifier reference_origin;
    base::u64 array_count = 0;
    TypeId array_element = INVALID_TYPE_ID;
    PointerMutability slice_mutability = PointerMutability::const_;
    TypeId slice_element = INVALID_TYPE_ID;
    std::vector<TypeId> tuple_elements;
    FunctionCallConv function_call_conv = FunctionCallConv::aurex;
    bool function_is_unsafe = false;
    bool function_is_variadic = false;
    std::vector<TypeId> function_params;
    TypeId function_return = INVALID_TYPE_ID;
};

enum class ExprKind {
    invalid,
    integer_literal,
    float_literal,
    bool_literal,
    null_literal,
    string_literal,
    c_string_literal,
    raw_string_literal,
    byte_string_literal,
    byte_literal,
    char_literal,
    name,
    generic_apply,
    unary,
    binary,
    call,
    try_expr,
    if_expr,
    block_expr,
    unsafe_block,
    match_expr,
    array_literal,
    tuple_literal,
    field,
    index,
    slice,
    struct_literal,
    cast,
    pcast,
    bcast,
    size_of,
    align_of,
    ptr_addr,
    paddr,
    slice_data,
    slice_len,
    str_data,
    str_byte_len,
    str_is_valid_utf8,
    str_from_utf8_checked,
    str_from_bytes_unchecked,
};

enum class UnaryOp {
    logical_not,
    numeric_negate,
    bitwise_not,
    address_of,
    address_of_mut,
    dereference,
};

enum class BinaryOp {
    add,
    sub,
    mul,
    div,
    mod,
    shl,
    shr,
    less,
    less_equal,
    greater,
    greater_equal,
    equal,
    not_equal,
    bit_and,
    bit_xor,
    bit_or,
    logical_and,
    logical_or,
};

enum class AssignOp {
    assign,
    add,
    sub,
    mul,
    div,
    mod,
    shl,
    shr,
    bit_and,
    bit_xor,
    bit_or,
};

enum class PatternKind {
    wildcard,
    binding,
    tuple,
    slice,
    struct_,
    enum_case,
    const_,
    literal,
    or_pattern,
};

struct FieldPattern {
    std::string_view name;
    PatternId pattern = INVALID_PATTERN_ID;
    base::SourceRange range{};
    IdentId name_id = INVALID_IDENT_ID;
};

struct PatternNode {
    PatternKind kind = PatternKind::wildcard;
    base::SourceRange range{};
    std::string_view binding_name;
    std::string_view enum_name;
    std::string_view case_name;
    TypeId enum_type = INVALID_TYPE_ID;
    std::string_view struct_name;
    IdentId binding_name_id = INVALID_IDENT_ID;
    IdentId enum_name_id = INVALID_IDENT_ID;
    IdentId case_name_id = INVALID_IDENT_ID;
    IdentId struct_name_id = INVALID_IDENT_ID;
    std::vector<std::string_view> binding_names;
    std::vector<IdentId> binding_name_ids;
    std::vector<PatternId> payload_patterns;
    std::vector<PatternId> elements;
    std::vector<FieldPattern> field_patterns;
    std::vector<PatternId> alternatives;
    base::usize slice_rest_index = 0;
    bool has_slice_rest = false;
    bool scoped = false;
};

struct FieldInit {
    std::string_view name;
    ExprId value = INVALID_EXPR_ID;
    base::SourceRange range{};
    IdentId name_id = INVALID_IDENT_ID;
};

struct MatchArm {
    PatternId pattern = INVALID_PATTERN_ID;
    ExprId guard = INVALID_EXPR_ID;
    ExprId value = INVALID_EXPR_ID;
    base::SourceRange range{};
};

} // namespace aurex::syntax
