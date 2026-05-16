#pragma once

#include <aurex/base/bump_allocator.hpp>
#include <aurex/base/config.hpp>
#include <aurex/base/source.hpp>
#include <aurex/syntax/ast_ids.hpp>
#include <aurex/syntax/identifier.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace aurex::syntax {

template <typename T>
using AstArenaVector = base::BumpVector<T>;

template <typename T>
[[nodiscard]] AstArenaVector<T> make_ast_arena_vector(base::BumpAllocator& arena) {
    return AstArenaVector<T>(base::BumpAllocatorAdapter<T> {arena});
}

template <typename T>
[[nodiscard]] bool ast_arena_vector_uses_arena(
    const AstArenaVector<T>& values,
    base::BumpAllocator& arena
) noexcept {
    return values.get_allocator() == base::BumpAllocatorAdapter<T> {arena};
}

template <typename T, typename Allocator>
[[nodiscard]] AstArenaVector<T> copy_ast_arena_vector(
    base::BumpAllocator& arena,
    const std::vector<T, Allocator>& values
) {
    AstArenaVector<T> copy = make_ast_arena_vector<T>(arena);
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

template <typename T>
[[nodiscard]] AstArenaVector<T> move_or_copy_ast_arena_vector(
    base::BumpAllocator& arena,
    AstArenaVector<T>&& values
) {
    if (ast_arena_vector_uses_arena(values, arena)) {
        return std::move(values);
    }
    return copy_ast_arena_vector(arena, values);
}

template <typename T, typename Allocator>
[[nodiscard]] std::vector<T> copy_std_vector(const std::vector<T, Allocator>& values) {
    std::vector<T> copy;
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

template <typename T, typename Allocator>
[[nodiscard]] AstArenaVector<T> copy_detached_ast_vector(const std::vector<T, Allocator>& values) {
    AstArenaVector<T> copy;
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

inline constexpr base::usize SYNTAX_AST_RESERVE_EXPR_TOKEN_DIVISOR = 8;
inline constexpr base::usize SYNTAX_AST_RESERVE_STMT_TOKEN_DIVISOR = 16;
inline constexpr base::usize SYNTAX_AST_RESERVE_TYPE_TOKEN_DIVISOR = 16;
inline constexpr base::usize SYNTAX_AST_RESERVE_PATTERN_TOKEN_DIVISOR = 32;
inline constexpr base::usize SYNTAX_AST_RESERVE_ITEM_TOKEN_DIVISOR = 64;
inline constexpr base::usize SYNTAX_AST_RESERVE_IDENTIFIER_TOKEN_DIVISOR = 8;
inline constexpr base::usize SYNTAX_AST_RESERVE_EXPRS_PER_STATEMENT = 6;
inline constexpr base::usize SYNTAX_AST_RESERVE_TYPES_PER_TYPE_SITE = 2;
inline constexpr base::usize SYNTAX_AST_RESERVE_TYPES_PER_ITEM = 4;
inline constexpr base::usize SYNTAX_AST_RESERVE_PATTERNS_PER_PATTERN_SITE = 2;
inline constexpr base::usize SYNTAX_AST_RESERVE_EXPR_NAME_DIVISOR = 2;
inline constexpr base::usize SYNTAX_AST_RESERVE_PRIMARY_PAYLOAD_DIVISOR = 3;
inline constexpr base::usize SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR = 8;
inline constexpr base::usize SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR = 32;
inline constexpr base::usize SYNTAX_AST_ARENA_ALLOCATION_PADDING_BYTES = alignof(std::max_align_t);

[[nodiscard]] constexpr base::usize ast_reserve_fraction(
    const base::usize size,
    const base::usize divisor
) noexcept {
    return size == 0 ? 0 : ((size - 1) / divisor) + 1;
}

[[nodiscard]] constexpr base::usize ast_reserve_at_least(
    const base::usize minimum,
    const base::usize estimated
) noexcept {
    return estimated < minimum ? minimum : estimated;
}

[[nodiscard]] constexpr base::usize ast_reserve_larger(
    const base::usize lhs,
    const base::usize rhs
) noexcept {
    return lhs < rhs ? rhs : lhs;
}

struct AstReserveEstimate {
    struct Exprs {
        base::usize headers = 0;
        base::usize literals = 0;
        base::usize names = 0;
        base::usize generic_applies = 0;
        base::usize unaries = 0;
        base::usize binaries = 0;
        base::usize calls = 0;
        base::usize ifs = 0;
        base::usize blocks = 0;
        base::usize matches = 0;
        base::usize arrays = 0;
        base::usize tuples = 0;
        base::usize postfix_chains = 0;
        base::usize fields = 0;
        base::usize indexes = 0;
        base::usize slices = 0;
        base::usize struct_literals = 0;
        base::usize casts = 0;
    };

    base::usize tokens = 0;
    base::usize statements = 0;
    base::usize items = 0;
    base::usize type_sites = 0;
    base::usize pattern_sites = 0;
    base::usize identifier_tokens = 0;
    Exprs exprs;
};

[[nodiscard]] constexpr AstReserveEstimate::Exprs ast_expr_reserve_for_node_capacity(
    const base::usize size
) noexcept {
    return AstReserveEstimate::Exprs {
        size,
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_PRIMARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_EXPR_NAME_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_PRIMARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR),
        ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR),
    };
}

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

enum class Visibility {
    public_,
    private_,
};

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

struct GenericParamDecl {
    std::string_view name;
    base::SourceRange range {};
    IdentId name_id = INVALID_IDENT_ID;
};

struct GenericConstraintDecl {
    std::string_view param_name;
    base::SourceRange param_range {};
    AstArenaVector<std::string_view> capability_names;
    AstArenaVector<base::SourceRange> capability_ranges;
    base::SourceRange range {};
    IdentId param_name_id = INVALID_IDENT_ID;
    AstArenaVector<IdentId> capability_name_ids;
};

struct TypeNode {
    TypeKind kind = TypeKind::named;
    base::SourceRange range {};
    PrimitiveTypeKind primitive = PrimitiveTypeKind::void_;
    std::string_view scope_name;
    base::SourceRange scope_range {};
    std::vector<std::string_view> scope_parts;
    std::string_view name;
    IdentId scope_name_id = INVALID_IDENT_ID;
    std::vector<IdentId> scope_part_ids;
    IdentId name_id = INVALID_IDENT_ID;
    std::vector<TypeId> type_args;
    PointerMutability pointer_mutability = PointerMutability::const_;
    TypeId pointee = INVALID_TYPE_ID;
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
    postfix_chain,
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
    base::SourceRange range {};
    IdentId name_id = INVALID_IDENT_ID;
};

struct PatternNode {
    PatternKind kind = PatternKind::wildcard;
    base::SourceRange range {};
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
    base::SourceRange range {};
    IdentId name_id = INVALID_IDENT_ID;
};

enum class PostfixOpKind {
    select,
    bracket,
    call,
    try_,
    struct_literal,
};

struct PostfixBracketArg {
    ExprId expr = INVALID_EXPR_ID;
    TypeId type = INVALID_TYPE_ID;
    base::SourceRange range {};
};

struct PostfixOp {
    PostfixOpKind kind = PostfixOpKind::select;
    base::SourceRange range {};
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<PostfixBracketArg> bracket_args;
    bool bracket_is_slice = false;
    ExprId slice_start = INVALID_EXPR_ID;
    ExprId slice_end = INVALID_EXPR_ID;
    AstArenaVector<ExprId> args;
    AstArenaVector<FieldInit> field_inits;
};

struct MatchArm {
    PatternId pattern = INVALID_PATTERN_ID;
    ExprId guard = INVALID_EXPR_ID;
    ExprId value = INVALID_EXPR_ID;
    base::SourceRange range {};
};

struct TypeNodeHeader {
    base::SourceRange range {};
    base::u32 payload = UINT32_MAX;
    TypeKind kind = TypeKind::named;
};

struct NamedTypePayload {
    std::string_view scope_name;
    base::SourceRange scope_range {};
    AstArenaVector<std::string_view> scope_parts;
    std::string_view name;
    IdentId scope_name_id = INVALID_IDENT_ID;
    AstArenaVector<IdentId> scope_part_ids;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<TypeId> type_args;
};

struct PointerTypePayload {
    PointerMutability mutability = PointerMutability::const_;
    TypeId pointee = INVALID_TYPE_ID;
};

struct ArrayTypePayload {
    base::u64 count = 0;
    TypeId element = INVALID_TYPE_ID;
};

struct SliceTypePayload {
    PointerMutability mutability = PointerMutability::const_;
    TypeId element = INVALID_TYPE_ID;
};

struct FunctionTypePayload {
    FunctionCallConv call_conv = FunctionCallConv::aurex;
    bool is_unsafe = false;
    bool is_variadic = false;
    AstArenaVector<TypeId> params;
    TypeId return_type = INVALID_TYPE_ID;
};

struct TypeNodePayloadArena {
    TypeNodePayloadArena() = default;

    explicit TypeNodePayloadArena(base::BumpAllocator& arena)
        : primitives(base::BumpAllocatorAdapter<PrimitiveTypeKind> {arena}),
          named(base::BumpAllocatorAdapter<NamedTypePayload> {arena}),
          pointers(base::BumpAllocatorAdapter<PointerTypePayload> {arena}),
          references(base::BumpAllocatorAdapter<PointerTypePayload> {arena}),
          arrays(base::BumpAllocatorAdapter<ArrayTypePayload> {arena}),
          slices(base::BumpAllocatorAdapter<SliceTypePayload> {arena}),
          tuples(base::BumpAllocatorAdapter<AstArenaVector<TypeId>> {arena}),
          functions(base::BumpAllocatorAdapter<FunctionTypePayload> {arena}) {}

    void swap(TypeNodePayloadArena& other) noexcept {
        this->primitives.swap(other.primitives);
        this->named.swap(other.named);
        this->pointers.swap(other.pointers);
        this->references.swap(other.references);
        this->arrays.swap(other.arrays);
        this->slices.swap(other.slices);
        this->tuples.swap(other.tuples);
        this->functions.swap(other.functions);
    }

    AstArenaVector<PrimitiveTypeKind> primitives;
    AstArenaVector<NamedTypePayload> named;
    AstArenaVector<PointerTypePayload> pointers;
    AstArenaVector<PointerTypePayload> references;
    AstArenaVector<ArrayTypePayload> arrays;
    AstArenaVector<SliceTypePayload> slices;
    AstArenaVector<AstArenaVector<TypeId>> tuples;
    AstArenaVector<FunctionTypePayload> functions;
};

class TypeNodeList final {
public:
    TypeNodeList()
        : arena_(std::make_unique<base::BumpAllocator>()),
          headers_(base::BumpAllocatorAdapter<TypeNodeHeader> {*this->arena_}),
          payloads_(*this->arena_) {}

    TypeNodeList(const TypeNodeList& other)
        : TypeNodeList() {
        this->copy_from(other);
    }

    TypeNodeList& operator=(const TypeNodeList& other) {
        if (this == &other) {
            return *this;
        }
        TypeNodeList copy(other);
        *this = std::move(copy);
        return *this;
    }

    TypeNodeList(TypeNodeList&& other) noexcept
        : arena_(std::move(other.arena_)),
          headers_(std::move(other.headers_)),
          payloads_(std::move(other.payloads_)) {
        other.headers_ = AstArenaVector<TypeNodeHeader> {};
        other.payloads_ = TypeNodePayloadArena {};
    }

    TypeNodeList& operator=(TypeNodeList&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        this->swap(other);
        return *this;
    }
    ~TypeNodeList() = default;

    [[nodiscard]] base::usize size() const noexcept {
        return this->headers_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return this->headers_.empty();
    }

    [[nodiscard]] TypeKind kind(const base::usize index) const noexcept {
        return this->headers_[index].kind;
    }

    [[nodiscard]] base::usize arena_bytes() const noexcept {
        return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
    }

    [[nodiscard]] base::usize arena_blocks() const noexcept {
        return this->arena_ == nullptr ? 0 : this->arena_->block_count();
    }

    [[nodiscard]] base::SourceRange range(const base::usize index) const noexcept {
        return this->headers_[index].range;
    }

    void reserve(const base::usize size) {
        this->reserve_headers(size);
        const base::usize primary = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_PRIMARY_PAYLOAD_DIVISOR);
        const base::usize secondary = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR);
        this->payloads_.primitives.reserve(secondary);
        this->payloads_.named.reserve(primary);
        this->payloads_.pointers.reserve(secondary);
        this->payloads_.references.reserve(secondary);
        this->payloads_.arrays.reserve(secondary);
        this->payloads_.slices.reserve(secondary);
        this->payloads_.tuples.reserve(secondary);
        this->payloads_.functions.reserve(secondary);
    }

    void reserve_headers(const base::usize size) {
        this->headers_.reserve(size);
    }

    void push_back(TypeNode node) {
        static_cast<void>(this->append(std::move(node)));
    }

    [[nodiscard]] TypeId append(TypeNode node) {
        const TypeId id {static_cast<base::u32>(this->headers_.size())};
        TypeNodeHeader header;
        header.kind = node.kind;
        header.range = node.range;
        header.payload = this->store_payload(std::move(node));
        this->headers_.push_back(header);
        return id;
    }

    void set(const base::usize index, TypeNode node) {
        TypeNodeHeader header;
        header.kind = node.kind;
        header.range = node.range;
        header.payload = this->store_payload(std::move(node));
        this->headers_[index] = header;
    }

    [[nodiscard]] TypeNode take(const base::usize index) {
        return this->load_moved(index);
    }

    [[nodiscard]] TypeNode operator[](const base::usize index) const {
        return this->load(index);
    }

private:
    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_list(const std::vector<T, Allocator>& values) {
        return copy_ast_arena_vector(*this->arena_, values);
    }

    [[nodiscard]] base::u32 store_payload(TypeNode node) {
        switch (node.kind) {
        case TypeKind::primitive:
            return this->push_payload(this->payloads_.primitives, node.primitive);
        case TypeKind::named:
            return this->push_payload(this->payloads_.named, NamedTypePayload {
                node.scope_name,
                node.scope_range,
                this->copy_list(node.scope_parts),
                node.name,
                node.scope_name_id,
                this->copy_list(node.scope_part_ids),
                node.name_id,
                this->copy_list(node.type_args),
            });
        case TypeKind::pointer:
            return this->push_payload(this->payloads_.pointers, PointerTypePayload {
                node.pointer_mutability,
                node.pointee,
            });
        case TypeKind::reference:
            return this->push_payload(this->payloads_.references, PointerTypePayload {
                node.pointer_mutability,
                node.pointee,
            });
        case TypeKind::array:
            return this->push_payload(this->payloads_.arrays, ArrayTypePayload {
                node.array_count,
                node.array_element,
            });
        case TypeKind::slice:
            return this->push_payload(this->payloads_.slices, SliceTypePayload {
                node.slice_mutability,
                node.slice_element,
            });
        case TypeKind::tuple:
            return this->push_payload(this->payloads_.tuples, this->copy_list(node.tuple_elements));
        case TypeKind::function:
            return this->push_payload(this->payloads_.functions, FunctionTypePayload {
                node.function_call_conv,
                node.function_is_unsafe,
                node.function_is_variadic,
                this->copy_list(node.function_params),
                node.function_return,
            });
        }
        return UINT32_MAX;
    }

    [[nodiscard]] TypeNode load(const base::usize index) const {
        const TypeNodeHeader& header = this->headers_[index];
        TypeNode node;
        node.kind = header.kind;
        node.range = header.range;
        switch (header.kind) {
        case TypeKind::primitive:
            node.primitive = this->payloads_.primitives[header.payload];
            break;
        case TypeKind::named: {
            const NamedTypePayload& payload = this->payloads_.named[header.payload];
            node.scope_name = payload.scope_name;
            node.scope_range = payload.scope_range;
            node.scope_parts = copy_std_vector(payload.scope_parts);
            node.name = payload.name;
            node.scope_name_id = payload.scope_name_id;
            node.scope_part_ids = copy_std_vector(payload.scope_part_ids);
            node.name_id = payload.name_id;
            node.type_args = copy_std_vector(payload.type_args);
            break;
        }
        case TypeKind::pointer: {
            const PointerTypePayload& payload = this->payloads_.pointers[header.payload];
            node.pointer_mutability = payload.mutability;
            node.pointee = payload.pointee;
            break;
        }
        case TypeKind::reference: {
            const PointerTypePayload& payload = this->payloads_.references[header.payload];
            node.pointer_mutability = payload.mutability;
            node.pointee = payload.pointee;
            break;
        }
        case TypeKind::array: {
            const ArrayTypePayload& payload = this->payloads_.arrays[header.payload];
            node.array_count = payload.count;
            node.array_element = payload.element;
            break;
        }
        case TypeKind::slice: {
            const SliceTypePayload& payload = this->payloads_.slices[header.payload];
            node.slice_mutability = payload.mutability;
            node.slice_element = payload.element;
            break;
        }
        case TypeKind::tuple:
            node.tuple_elements = copy_std_vector(this->payloads_.tuples[header.payload]);
            break;
        case TypeKind::function: {
            const FunctionTypePayload& payload = this->payloads_.functions[header.payload];
            node.function_call_conv = payload.call_conv;
            node.function_is_unsafe = payload.is_unsafe;
            node.function_is_variadic = payload.is_variadic;
            node.function_params = copy_std_vector(payload.params);
            node.function_return = payload.return_type;
            break;
        }
        }
        return node;
    }

    [[nodiscard]] TypeNode load_moved(const base::usize index) {
        const TypeNodeHeader& header = this->headers_[index];
        TypeNode node;
        node.kind = header.kind;
        node.range = header.range;
        switch (header.kind) {
        case TypeKind::primitive:
            node.primitive = this->payloads_.primitives[header.payload];
            break;
        case TypeKind::named: {
            NamedTypePayload& payload = this->payloads_.named[header.payload];
            node.scope_name = payload.scope_name;
            node.scope_range = payload.scope_range;
            node.scope_parts = copy_std_vector(payload.scope_parts);
            node.name = payload.name;
            node.scope_name_id = payload.scope_name_id;
            node.scope_part_ids = copy_std_vector(payload.scope_part_ids);
            node.name_id = payload.name_id;
            node.type_args = copy_std_vector(payload.type_args);
            break;
        }
        case TypeKind::pointer: {
            const PointerTypePayload& payload = this->payloads_.pointers[header.payload];
            node.pointer_mutability = payload.mutability;
            node.pointee = payload.pointee;
            break;
        }
        case TypeKind::reference: {
            const PointerTypePayload& payload = this->payloads_.references[header.payload];
            node.pointer_mutability = payload.mutability;
            node.pointee = payload.pointee;
            break;
        }
        case TypeKind::array: {
            const ArrayTypePayload& payload = this->payloads_.arrays[header.payload];
            node.array_count = payload.count;
            node.array_element = payload.element;
            break;
        }
        case TypeKind::slice: {
            const SliceTypePayload& payload = this->payloads_.slices[header.payload];
            node.slice_mutability = payload.mutability;
            node.slice_element = payload.element;
            break;
        }
        case TypeKind::tuple:
            node.tuple_elements = copy_std_vector(this->payloads_.tuples[header.payload]);
            break;
        case TypeKind::function: {
            FunctionTypePayload& payload = this->payloads_.functions[header.payload];
            node.function_call_conv = payload.call_conv;
            node.function_is_unsafe = payload.is_unsafe;
            node.function_is_variadic = payload.is_variadic;
            node.function_params = copy_std_vector(payload.params);
            node.function_return = payload.return_type;
            break;
        }
        }
        return node;
    }

    template <typename T>
    [[nodiscard]] base::u32 push_payload(AstArenaVector<T>& payloads, T payload) {
        const base::u32 index = static_cast<base::u32>(payloads.size());
        payloads.push_back(std::move(payload));
        return index;
    }

    void copy_from(const TypeNodeList& other) {
        this->reserve(other.size());
        for (base::usize i = 0; i < other.size(); ++i) {
            static_cast<void>(this->append(other.load(i)));
        }
    }

    void swap(TypeNodeList& other) noexcept {
        using std::swap;
        swap(this->arena_, other.arena_);
        this->headers_.swap(other.headers_);
        this->payloads_.swap(other.payloads_);
    }

    std::unique_ptr<base::BumpAllocator> arena_;
    AstArenaVector<TypeNodeHeader> headers_;
    TypeNodePayloadArena payloads_;
};

struct LiteralExprPayload {
    std::string_view text;
};

struct NameExprPayload {
    std::string_view scope_name;
    base::SourceRange scope_range {};
    std::string_view text;
    IdentId scope_name_id = INVALID_IDENT_ID;
    IdentId text_id = INVALID_IDENT_ID;
    AstArenaVector<TypeId> type_args;
};

struct GenericApplyExprPayload {
    ExprId callee = INVALID_EXPR_ID;
    AstArenaVector<TypeId> type_args;
};

struct UnaryExprPayload {
    UnaryOp op = UnaryOp::logical_not;
    ExprId operand = INVALID_EXPR_ID;
};

struct BinaryExprPayload {
    BinaryOp op = BinaryOp::add;
    ExprId lhs = INVALID_EXPR_ID;
    ExprId rhs = INVALID_EXPR_ID;
};

struct CallExprPayload {
    ExprId callee = INVALID_EXPR_ID;
    AstArenaVector<ExprId> args;
};

struct IfExprPayload {
    ExprId condition = INVALID_EXPR_ID;
    PatternId condition_pattern = INVALID_PATTERN_ID;
    ExprId then_expr = INVALID_EXPR_ID;
    ExprId else_expr = INVALID_EXPR_ID;
};

struct BlockExprPayload {
    StmtId block = INVALID_STMT_ID;
    ExprId result = INVALID_EXPR_ID;
};

struct MatchExprPayload {
    ExprId value = INVALID_EXPR_ID;
    AstArenaVector<MatchArm> arms;
};

struct ArrayExprPayload {
    AstArenaVector<ExprId> elements;
    ExprId repeat_value = INVALID_EXPR_ID;
    ExprId repeat_count = INVALID_EXPR_ID;
};

struct PostfixChainExprPayload {
    ExprId base = INVALID_EXPR_ID;
    AstArenaVector<PostfixOp> ops;
};

struct FieldExprPayload {
    ExprId object = INVALID_EXPR_ID;
    std::string_view field_name;
    IdentId field_name_id = INVALID_IDENT_ID;
};

struct IndexExprPayload {
    ExprId object = INVALID_EXPR_ID;
    ExprId index = INVALID_EXPR_ID;
};

struct SliceExprPayload {
    ExprId object = INVALID_EXPR_ID;
    ExprId start = INVALID_EXPR_ID;
    ExprId end = INVALID_EXPR_ID;
};

struct StructLiteralExprPayload {
    ExprId object = INVALID_EXPR_ID;
    std::string_view scope_name;
    base::SourceRange scope_range {};
    std::string_view name;
    IdentId scope_name_id = INVALID_IDENT_ID;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<TypeId> type_args;
    AstArenaVector<FieldInit> field_inits;
};

struct CastExprPayload {
    TypeId type = INVALID_TYPE_ID;
    ExprId expr = INVALID_EXPR_ID;
};

struct ExprNodeHeader {
    base::SourceRange range {};
    base::u32 payload = UINT32_MAX;
    ExprKind kind = ExprKind::invalid;
};

struct ExprNodePayloadArena {
    ExprNodePayloadArena() = default;

    explicit ExprNodePayloadArena(base::BumpAllocator& arena)
        : literals(base::BumpAllocatorAdapter<LiteralExprPayload> {arena}),
          names(base::BumpAllocatorAdapter<NameExprPayload> {arena}),
          generic_applies(base::BumpAllocatorAdapter<GenericApplyExprPayload> {arena}),
          unaries(base::BumpAllocatorAdapter<UnaryExprPayload> {arena}),
          binaries(base::BumpAllocatorAdapter<BinaryExprPayload> {arena}),
          calls(base::BumpAllocatorAdapter<CallExprPayload> {arena}),
          ifs(base::BumpAllocatorAdapter<IfExprPayload> {arena}),
          blocks(base::BumpAllocatorAdapter<BlockExprPayload> {arena}),
          matches(base::BumpAllocatorAdapter<MatchExprPayload> {arena}),
          arrays(base::BumpAllocatorAdapter<ArrayExprPayload> {arena}),
          tuples(base::BumpAllocatorAdapter<AstArenaVector<ExprId>> {arena}),
          postfix_chains(base::BumpAllocatorAdapter<PostfixChainExprPayload> {arena}),
          fields(base::BumpAllocatorAdapter<FieldExprPayload> {arena}),
          indexes(base::BumpAllocatorAdapter<IndexExprPayload> {arena}),
          slices(base::BumpAllocatorAdapter<SliceExprPayload> {arena}),
          struct_literals(base::BumpAllocatorAdapter<StructLiteralExprPayload> {arena}),
          casts(base::BumpAllocatorAdapter<CastExprPayload> {arena}) {}

    void swap(ExprNodePayloadArena& other) noexcept {
        this->literals.swap(other.literals);
        this->names.swap(other.names);
        this->generic_applies.swap(other.generic_applies);
        this->unaries.swap(other.unaries);
        this->binaries.swap(other.binaries);
        this->calls.swap(other.calls);
        this->ifs.swap(other.ifs);
        this->blocks.swap(other.blocks);
        this->matches.swap(other.matches);
        this->arrays.swap(other.arrays);
        this->tuples.swap(other.tuples);
        this->postfix_chains.swap(other.postfix_chains);
        this->fields.swap(other.fields);
        this->indexes.swap(other.indexes);
        this->slices.swap(other.slices);
        this->struct_literals.swap(other.struct_literals);
        this->casts.swap(other.casts);
    }

    AstArenaVector<LiteralExprPayload> literals;
    AstArenaVector<NameExprPayload> names;
    AstArenaVector<GenericApplyExprPayload> generic_applies;
    AstArenaVector<UnaryExprPayload> unaries;
    AstArenaVector<BinaryExprPayload> binaries;
    AstArenaVector<CallExprPayload> calls;
    AstArenaVector<IfExprPayload> ifs;
    AstArenaVector<BlockExprPayload> blocks;
    AstArenaVector<MatchExprPayload> matches;
    AstArenaVector<ArrayExprPayload> arrays;
    AstArenaVector<AstArenaVector<ExprId>> tuples;
    AstArenaVector<PostfixChainExprPayload> postfix_chains;
    AstArenaVector<FieldExprPayload> fields;
    AstArenaVector<IndexExprPayload> indexes;
    AstArenaVector<SliceExprPayload> slices;
    AstArenaVector<StructLiteralExprPayload> struct_literals;
    AstArenaVector<CastExprPayload> casts;
};

class ExprNodeList final {
public:
    ExprNodeList()
        : arena_(std::make_unique<base::BumpAllocator>()),
          headers_(base::BumpAllocatorAdapter<ExprNodeHeader> {*this->arena_}),
          payloads_(*this->arena_) {}

    ExprNodeList(const ExprNodeList& other)
        : ExprNodeList() {
        this->copy_from(other);
    }

    ExprNodeList& operator=(const ExprNodeList& other) {
        if (this == &other) {
            return *this;
        }
        ExprNodeList copy(other);
        *this = std::move(copy);
        return *this;
    }

    ExprNodeList(ExprNodeList&& other) noexcept
        : arena_(std::move(other.arena_)),
          headers_(std::move(other.headers_)),
          payloads_(std::move(other.payloads_)) {
        other.headers_ = AstArenaVector<ExprNodeHeader> {};
        other.payloads_ = ExprNodePayloadArena {};
    }

    ExprNodeList& operator=(ExprNodeList&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        this->swap(other);
        return *this;
    }
    ~ExprNodeList() = default;

    [[nodiscard]] base::usize size() const noexcept {
        return this->headers_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return this->headers_.empty();
    }

    [[nodiscard]] ExprKind kind(const base::usize index) const noexcept {
        return this->headers_[index].kind;
    }

    [[nodiscard]] base::usize arena_bytes() const noexcept {
        return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
    }

    [[nodiscard]] base::usize arena_used_bytes() const noexcept {
        return this->arena_ == nullptr ? 0 : this->arena_->used_bytes();
    }

    [[nodiscard]] base::usize arena_blocks() const noexcept {
        return this->arena_ == nullptr ? 0 : this->arena_->block_count();
    }

    [[nodiscard]] base::SourceRange range(const base::usize index) const noexcept {
        return this->headers_[index].range;
    }

    template <typename T>
    [[nodiscard]] AstArenaVector<T> make_list() {
        return make_ast_arena_vector<T>(*this->arena_);
    }

    [[nodiscard]] PostfixOp make_postfix_op() {
        PostfixOp op;
        op.bracket_args = this->make_list<PostfixBracketArg>();
        op.args = this->make_list<ExprId>();
        op.field_inits = this->make_list<FieldInit>();
        return op;
    }

    [[nodiscard]] const LiteralExprPayload* literal_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || !this->is_literal(this->headers_[index].kind)) {
            return nullptr;
        }
        return &this->payloads_.literals[this->headers_[index].payload];
    }

    [[nodiscard]] const NameExprPayload* name_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::name) {
            return nullptr;
        }
        return &this->payloads_.names[this->headers_[index].payload];
    }

    [[nodiscard]] NameExprPayload* name_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::name) {
            return nullptr;
        }
        return &this->payloads_.names[this->headers_[index].payload];
    }

    [[nodiscard]] const GenericApplyExprPayload* generic_apply_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::generic_apply) {
            return nullptr;
        }
        return &this->payloads_.generic_applies[this->headers_[index].payload];
    }

    [[nodiscard]] GenericApplyExprPayload* generic_apply_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::generic_apply) {
            return nullptr;
        }
        return &this->payloads_.generic_applies[this->headers_[index].payload];
    }

    [[nodiscard]] const UnaryExprPayload* unary_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) ||
            (this->headers_[index].kind != ExprKind::unary && this->headers_[index].kind != ExprKind::try_expr)) {
            return nullptr;
        }
        return &this->payloads_.unaries[this->headers_[index].payload];
    }

    [[nodiscard]] UnaryExprPayload* unary_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) ||
            (this->headers_[index].kind != ExprKind::unary && this->headers_[index].kind != ExprKind::try_expr)) {
            return nullptr;
        }
        return &this->payloads_.unaries[this->headers_[index].payload];
    }

    [[nodiscard]] const BinaryExprPayload* binary_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::binary) {
            return nullptr;
        }
        return &this->payloads_.binaries[this->headers_[index].payload];
    }

    [[nodiscard]] BinaryExprPayload* binary_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::binary) {
            return nullptr;
        }
        return &this->payloads_.binaries[this->headers_[index].payload];
    }

    [[nodiscard]] const CallExprPayload* call_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) ||
            (this->headers_[index].kind != ExprKind::call &&
             this->headers_[index].kind != ExprKind::str_from_bytes_unchecked)) {
            return nullptr;
        }
        return &this->payloads_.calls[this->headers_[index].payload];
    }

    [[nodiscard]] CallExprPayload* call_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) ||
            (this->headers_[index].kind != ExprKind::call &&
             this->headers_[index].kind != ExprKind::str_from_bytes_unchecked)) {
            return nullptr;
        }
        return &this->payloads_.calls[this->headers_[index].payload];
    }

    [[nodiscard]] const IfExprPayload* if_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::if_expr) {
            return nullptr;
        }
        return &this->payloads_.ifs[this->headers_[index].payload];
    }

    [[nodiscard]] IfExprPayload* if_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::if_expr) {
            return nullptr;
        }
        return &this->payloads_.ifs[this->headers_[index].payload];
    }

    [[nodiscard]] const BlockExprPayload* block_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) ||
            (this->headers_[index].kind != ExprKind::block_expr &&
             this->headers_[index].kind != ExprKind::unsafe_block)) {
            return nullptr;
        }
        return &this->payloads_.blocks[this->headers_[index].payload];
    }

    [[nodiscard]] BlockExprPayload* block_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) ||
            (this->headers_[index].kind != ExprKind::block_expr &&
             this->headers_[index].kind != ExprKind::unsafe_block)) {
            return nullptr;
        }
        return &this->payloads_.blocks[this->headers_[index].payload];
    }

    [[nodiscard]] const MatchExprPayload* match_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::match_expr) {
            return nullptr;
        }
        return &this->payloads_.matches[this->headers_[index].payload];
    }

    [[nodiscard]] MatchExprPayload* match_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::match_expr) {
            return nullptr;
        }
        return &this->payloads_.matches[this->headers_[index].payload];
    }

    [[nodiscard]] const ArrayExprPayload* array_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::array_literal) {
            return nullptr;
        }
        return &this->payloads_.arrays[this->headers_[index].payload];
    }

    [[nodiscard]] ArrayExprPayload* array_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::array_literal) {
            return nullptr;
        }
        return &this->payloads_.arrays[this->headers_[index].payload];
    }

    [[nodiscard]] const AstArenaVector<ExprId>* tuple_elements(const base::usize index) const noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::tuple_literal) {
            return nullptr;
        }
        return &this->payloads_.tuples[this->headers_[index].payload];
    }

    [[nodiscard]] AstArenaVector<ExprId>* tuple_elements(const base::usize index) noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::tuple_literal) {
            return nullptr;
        }
        return &this->payloads_.tuples[this->headers_[index].payload];
    }

    [[nodiscard]] const PostfixChainExprPayload* postfix_chain_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::postfix_chain) {
            return nullptr;
        }
        return &this->payloads_.postfix_chains[this->headers_[index].payload];
    }

    [[nodiscard]] PostfixChainExprPayload* postfix_chain_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::postfix_chain) {
            return nullptr;
        }
        return &this->payloads_.postfix_chains[this->headers_[index].payload];
    }

    [[nodiscard]] PostfixChainExprPayload take_postfix_chain_payload(const base::usize index) {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::postfix_chain) {
            return {};
        }
        PostfixChainExprPayload& payload = this->payloads_.postfix_chains[this->headers_[index].payload];
        return PostfixChainExprPayload {
            payload.base,
            std::move(payload.ops),
        };
    }

    [[nodiscard]] const FieldExprPayload* field_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::field) {
            return nullptr;
        }
        return &this->payloads_.fields[this->headers_[index].payload];
    }

    [[nodiscard]] FieldExprPayload* field_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::field) {
            return nullptr;
        }
        return &this->payloads_.fields[this->headers_[index].payload];
    }

    [[nodiscard]] const IndexExprPayload* index_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::index) {
            return nullptr;
        }
        return &this->payloads_.indexes[this->headers_[index].payload];
    }

    [[nodiscard]] IndexExprPayload* index_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::index) {
            return nullptr;
        }
        return &this->payloads_.indexes[this->headers_[index].payload];
    }

    [[nodiscard]] const SliceExprPayload* slice_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::slice) {
            return nullptr;
        }
        return &this->payloads_.slices[this->headers_[index].payload];
    }

    [[nodiscard]] SliceExprPayload* slice_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::slice) {
            return nullptr;
        }
        return &this->payloads_.slices[this->headers_[index].payload];
    }

    [[nodiscard]] const StructLiteralExprPayload* struct_literal_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::struct_literal) {
            return nullptr;
        }
        return &this->payloads_.struct_literals[this->headers_[index].payload];
    }

    [[nodiscard]] StructLiteralExprPayload* struct_literal_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::struct_literal) {
            return nullptr;
        }
        return &this->payloads_.struct_literals[this->headers_[index].payload];
    }

    [[nodiscard]] const CastExprPayload* cast_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || !this->is_cast_like(this->headers_[index].kind)) {
            return nullptr;
        }
        return &this->payloads_.casts[this->headers_[index].payload];
    }

    [[nodiscard]] CastExprPayload* cast_payload(const base::usize index) noexcept {
        if (!this->payload_available(index) || !this->is_cast_like(this->headers_[index].kind)) {
            return nullptr;
        }
        return &this->payloads_.casts[this->headers_[index].payload];
    }

    void reserve(const base::usize size) {
        const AstReserveEstimate::Exprs plan = ast_expr_reserve_for_node_capacity(size);
        this->reserve_headers(plan.headers);
        this->reserve_payloads(plan);
    }

    void reserve_touched(const base::usize size) {
        this->reserve_touched(ast_expr_reserve_for_node_capacity(size));
    }

    void reserve_touched(const AstReserveEstimate::Exprs& plan) {
        if (this->arena_ != nullptr) {
            this->arena_->reserve_touched(estimated_arena_bytes(plan));
        }
        this->reserve_headers(plan.headers);
        this->reserve_payloads(plan);
    }

    void reserve_headers(const base::usize size) {
        this->headers_.reserve(size);
    }

    [[nodiscard]] ExprId append_invalid(const base::SourceRange& range) {
        return this->append_header(ExprKind::invalid, range, UINT32_MAX);
    }

    [[nodiscard]] ExprId append_literal(
        const ExprKind kind,
        const base::SourceRange& range,
        const std::string_view text
    ) {
        return this->append_header(kind, range, this->emplace_payload(this->payloads_.literals, text));
    }

    [[nodiscard]] ExprId append_name(const base::SourceRange& range, NameExprPayload payload) {
        return this->append_name(
            range,
            payload.scope_name,
            payload.scope_range,
            payload.text,
            payload.scope_name_id,
            payload.text_id,
            std::move(payload.type_args)
        );
    }

    template <typename TypeArgAllocator>
    [[nodiscard]] ExprId append_name(
        const base::SourceRange& range,
        const std::string_view scope_name,
        const base::SourceRange& scope_range,
        const std::string_view text,
        const IdentId scope_name_id,
        const IdentId text_id,
        std::vector<TypeId, TypeArgAllocator> type_args
    ) {
        return this->append_header(
            ExprKind::name,
            range,
            this->emplace_payload(
                this->payloads_.names,
                scope_name,
                scope_range,
                text,
                scope_name_id,
                text_id,
                this->copy_or_move_list(std::move(type_args))
            )
        );
    }

    [[nodiscard]] ExprId append_generic_apply(const base::SourceRange& range, GenericApplyExprPayload payload) {
        return this->append_generic_apply(range, payload.callee, std::move(payload.type_args));
    }

    template <typename TypeArgAllocator>
    [[nodiscard]] ExprId append_generic_apply(
        const base::SourceRange& range,
        const ExprId callee,
        std::vector<TypeId, TypeArgAllocator> type_args
    ) {
        return this->append_header(
            ExprKind::generic_apply,
            range,
            this->emplace_payload(this->payloads_.generic_applies, callee, this->copy_or_move_list(std::move(type_args)))
        );
    }

    [[nodiscard]] ExprId append_unary(
        const ExprKind kind,
        const base::SourceRange& range,
        const UnaryExprPayload payload
    ) {
        return this->append_unary(kind, range, payload.op, payload.operand);
    }

    [[nodiscard]] ExprId append_unary(
        const ExprKind kind,
        const base::SourceRange& range,
        const UnaryOp op,
        const ExprId operand
    ) {
        return this->append_header(kind, range, this->emplace_payload(this->payloads_.unaries, op, operand));
    }

    [[nodiscard]] ExprId append_binary(const base::SourceRange& range, const BinaryExprPayload payload) {
        return this->append_binary(range, payload.op, payload.lhs, payload.rhs);
    }

    [[nodiscard]] ExprId append_binary(
        const base::SourceRange& range,
        const BinaryOp op,
        const ExprId lhs,
        const ExprId rhs
    ) {
        return this->append_header(ExprKind::binary, range, this->emplace_payload(this->payloads_.binaries, op, lhs, rhs));
    }

    [[nodiscard]] ExprId append_call(
        const ExprKind kind,
        const base::SourceRange& range,
        CallExprPayload payload
    ) {
        return this->append_call(kind, range, payload.callee, std::move(payload.args));
    }

    template <typename ArgAllocator>
    [[nodiscard]] ExprId append_call(
        const ExprKind kind,
        const base::SourceRange& range,
        const ExprId callee,
        std::vector<ExprId, ArgAllocator> args
    ) {
        return this->append_header(
            kind,
            range,
            this->emplace_payload(this->payloads_.calls, callee, this->copy_or_move_list(std::move(args)))
        );
    }

    [[nodiscard]] ExprId append_if(const base::SourceRange& range, const IfExprPayload payload) {
        return this->append_if(range, payload.condition, payload.condition_pattern, payload.then_expr, payload.else_expr);
    }

    [[nodiscard]] ExprId append_if(
        const base::SourceRange& range,
        const ExprId condition,
        const PatternId condition_pattern,
        const ExprId then_expr,
        const ExprId else_expr
    ) {
        return this->append_header(
            ExprKind::if_expr,
            range,
            this->emplace_payload(this->payloads_.ifs, condition, condition_pattern, then_expr, else_expr)
        );
    }

    [[nodiscard]] ExprId append_block(
        const ExprKind kind,
        const base::SourceRange& range,
        const BlockExprPayload payload
    ) {
        return this->append_block(kind, range, payload.block, payload.result);
    }

    [[nodiscard]] ExprId append_block(
        const ExprKind kind,
        const base::SourceRange& range,
        const StmtId block,
        const ExprId result
    ) {
        return this->append_header(kind, range, this->emplace_payload(this->payloads_.blocks, block, result));
    }

    [[nodiscard]] ExprId append_match(const base::SourceRange& range, MatchExprPayload payload) {
        return this->append_match(range, payload.value, std::move(payload.arms));
    }

    template <typename ArmAllocator>
    [[nodiscard]] ExprId append_match(
        const base::SourceRange& range,
        const ExprId value,
        std::vector<MatchArm, ArmAllocator> arms
    ) {
        return this->append_header(
            ExprKind::match_expr,
            range,
            this->emplace_payload(this->payloads_.matches, value, this->copy_or_move_list(std::move(arms)))
        );
    }

    [[nodiscard]] ExprId append_array(const base::SourceRange& range, ArrayExprPayload payload) {
        return this->append_array(range, std::move(payload.elements), payload.repeat_value, payload.repeat_count);
    }

    template <typename ElementAllocator>
    [[nodiscard]] ExprId append_array(
        const base::SourceRange& range,
        std::vector<ExprId, ElementAllocator> elements,
        const ExprId repeat_value,
        const ExprId repeat_count
    ) {
        return this->append_header(
            ExprKind::array_literal,
            range,
            this->emplace_payload(this->payloads_.arrays, this->copy_or_move_list(std::move(elements)), repeat_value, repeat_count)
        );
    }

    template <typename ElementAllocator>
    [[nodiscard]] ExprId append_tuple(const base::SourceRange& range, std::vector<ExprId, ElementAllocator> elements) {
        return this->append_header(
            ExprKind::tuple_literal,
            range,
            this->emplace_payload(this->payloads_.tuples, this->copy_or_move_list(std::move(elements)))
        );
    }

    [[nodiscard]] ExprId append_postfix_chain(const base::SourceRange& range, PostfixChainExprPayload payload) {
        return this->append_postfix_chain(range, payload.base, std::move(payload.ops));
    }

    template <typename OpAllocator>
    [[nodiscard]] ExprId append_postfix_chain(
        const base::SourceRange& range,
        const ExprId base,
        std::vector<PostfixOp, OpAllocator> ops
    ) {
        return this->append_header(
            ExprKind::postfix_chain,
            range,
            this->emplace_payload(this->payloads_.postfix_chains, base, this->copy_or_move_postfix_ops(std::move(ops)))
        );
    }

    [[nodiscard]] ExprId append_field(const base::SourceRange& range, const FieldExprPayload& payload) {
        return this->append_field(range, payload.object, payload.field_name, payload.field_name_id);
    }

    [[nodiscard]] ExprId append_field(
        const base::SourceRange& range,
        const ExprId object,
        const std::string_view field_name,
        const IdentId field_name_id
    ) {
        return this->append_header(ExprKind::field, range, this->emplace_payload(this->payloads_.fields, object, field_name, field_name_id));
    }

    [[nodiscard]] ExprId append_index(const base::SourceRange& range, const IndexExprPayload payload) {
        return this->append_index(range, payload.object, payload.index);
    }

    [[nodiscard]] ExprId append_index(
        const base::SourceRange& range,
        const ExprId object,
        const ExprId index
    ) {
        return this->append_header(ExprKind::index, range, this->emplace_payload(this->payloads_.indexes, object, index));
    }

    [[nodiscard]] ExprId append_slice(const base::SourceRange& range, const SliceExprPayload payload) {
        return this->append_slice(range, payload.object, payload.start, payload.end);
    }

    [[nodiscard]] ExprId append_slice(
        const base::SourceRange& range,
        const ExprId object,
        const ExprId start,
        const ExprId end
    ) {
        return this->append_header(ExprKind::slice, range, this->emplace_payload(this->payloads_.slices, object, start, end));
    }

    [[nodiscard]] ExprId append_struct_literal(const base::SourceRange& range, StructLiteralExprPayload payload) {
        return this->append_struct_literal(
            range,
            payload.object,
            payload.scope_name,
            payload.scope_range,
            payload.name,
            payload.scope_name_id,
            payload.name_id,
            std::move(payload.type_args),
            std::move(payload.field_inits)
        );
    }

    template <typename TypeArgAllocator, typename FieldInitAllocator>
    [[nodiscard]] ExprId append_struct_literal(
        const base::SourceRange& range,
        const ExprId object,
        const std::string_view scope_name,
        const base::SourceRange& scope_range,
        const std::string_view name,
        const IdentId scope_name_id,
        const IdentId name_id,
        std::vector<TypeId, TypeArgAllocator> type_args,
        std::vector<FieldInit, FieldInitAllocator> field_inits
    ) {
        return this->append_header(
            ExprKind::struct_literal,
            range,
            this->emplace_payload(
                this->payloads_.struct_literals,
                object,
                scope_name,
                scope_range,
                name,
                scope_name_id,
                name_id,
                this->copy_or_move_list(std::move(type_args)),
                this->copy_or_move_list(std::move(field_inits))
            )
        );
    }

    [[nodiscard]] ExprId append_cast_like(
        const ExprKind kind,
        const base::SourceRange& range,
        const CastExprPayload payload
    ) {
        return this->append_cast_like(kind, range, payload.type, payload.expr);
    }

    [[nodiscard]] ExprId append_cast_like(
        const ExprKind kind,
        const base::SourceRange& range,
        const TypeId type,
        const ExprId expr
    ) {
        return this->append_header(kind, range, this->emplace_payload(this->payloads_.casts, type, expr));
    }

    void set_invalid(const base::usize index, const base::SourceRange& range) {
        this->set_header(index, ExprKind::invalid, range, UINT32_MAX);
    }

    void set_generic_apply(const base::usize index, const base::SourceRange& range, GenericApplyExprPayload payload) {
        this->set_generic_apply(index, range, payload.callee, std::move(payload.type_args));
    }

    template <typename TypeArgAllocator>
    void set_generic_apply(
        const base::usize index,
        const base::SourceRange& range,
        const ExprId callee,
        std::vector<TypeId, TypeArgAllocator> type_args
    ) {
        this->set_header(
            index,
            ExprKind::generic_apply,
            range,
            this->emplace_payload(this->payloads_.generic_applies, callee, this->copy_or_move_list(std::move(type_args)))
        );
    }

    void set_unary(const base::usize index, const ExprKind kind, const base::SourceRange& range, const UnaryExprPayload payload) {
        this->set_unary(index, kind, range, payload.op, payload.operand);
    }

    void set_unary(
        const base::usize index,
        const ExprKind kind,
        const base::SourceRange& range,
        const UnaryOp op,
        const ExprId operand
    ) {
        this->set_header(index, kind, range, this->emplace_payload(this->payloads_.unaries, op, operand));
    }

    void set_call(const base::usize index, const ExprKind kind, const base::SourceRange& range, CallExprPayload payload) {
        this->set_call(index, kind, range, payload.callee, std::move(payload.args));
    }

    template <typename ArgAllocator>
    void set_call(
        const base::usize index,
        const ExprKind kind,
        const base::SourceRange& range,
        const ExprId callee,
        std::vector<ExprId, ArgAllocator> args
    ) {
        this->set_header(
            index,
            kind,
            range,
            this->emplace_payload(this->payloads_.calls, callee, this->copy_or_move_list(std::move(args)))
        );
    }

    void set_field(const base::usize index, const base::SourceRange& range, const FieldExprPayload& payload) {
        this->set_field(index, range, payload.object, payload.field_name, payload.field_name_id);
    }

    void set_field(
        const base::usize index,
        const base::SourceRange& range,
        const ExprId object,
        const std::string_view field_name,
        const IdentId field_name_id
    ) {
        this->set_header(
            index,
            ExprKind::field,
            range,
            this->emplace_payload(this->payloads_.fields, object, field_name, field_name_id)
        );
    }

    void set_index(const base::usize index, const base::SourceRange& range, const IndexExprPayload payload) {
        this->set_index(index, range, payload.object, payload.index);
    }

    void set_index(
        const base::usize index,
        const base::SourceRange& range,
        const ExprId object,
        const ExprId index_expr
    ) {
        this->set_header(index, ExprKind::index, range, this->emplace_payload(this->payloads_.indexes, object, index_expr));
    }

    void set_slice(const base::usize index, const base::SourceRange& range, const SliceExprPayload payload) {
        this->set_slice(index, range, payload.object, payload.start, payload.end);
    }

    void set_slice(
        const base::usize index,
        const base::SourceRange& range,
        const ExprId object,
        const ExprId start,
        const ExprId end
    ) {
        this->set_header(index, ExprKind::slice, range, this->emplace_payload(this->payloads_.slices, object, start, end));
    }

    void set_struct_literal(const base::usize index, const base::SourceRange& range, StructLiteralExprPayload payload) {
        this->set_struct_literal(
            index,
            range,
            payload.object,
            payload.scope_name,
            payload.scope_range,
            payload.name,
            payload.scope_name_id,
            payload.name_id,
            std::move(payload.type_args),
            std::move(payload.field_inits)
        );
    }

    template <typename TypeArgAllocator, typename FieldInitAllocator>
    void set_struct_literal(
        const base::usize index,
        const base::SourceRange& range,
        const ExprId object,
        const std::string_view scope_name,
        const base::SourceRange& scope_range,
        const std::string_view name,
        const IdentId scope_name_id,
        const IdentId name_id,
        std::vector<TypeId, TypeArgAllocator> type_args,
        std::vector<FieldInit, FieldInitAllocator> field_inits
    ) {
        this->set_header(
            index,
            ExprKind::struct_literal,
            range,
            this->emplace_payload(
                this->payloads_.struct_literals,
                object,
                scope_name,
                scope_range,
                name,
                scope_name_id,
                name_id,
                this->copy_or_move_list(std::move(type_args)),
                this->copy_or_move_list(std::move(field_inits))
            )
        );
    }

    [[nodiscard]] bool retag_block_expr(
        const base::usize index,
        const ExprKind kind,
        const base::SourceRange& range
    ) noexcept {
        if (index >= this->headers_.size() ||
            !this->payload_available(index) ||
            !this->is_block_payload_kind(this->headers_[index].kind) ||
            !this->is_block_payload_kind(kind)) {
            return false;
        }
        this->headers_[index].kind = kind;
        this->headers_[index].range = range;
        return true;
    }

private:
    [[nodiscard]] ExprId append_header(
        const ExprKind kind,
        const base::SourceRange& range,
        const base::u32 payload
    ) {
        const ExprId id {static_cast<base::u32>(this->headers_.size())};
        this->headers_.push_back(ExprNodeHeader {range, payload, kind});
        return id;
    }

    void set_header(
        const base::usize index,
        const ExprKind kind,
        const base::SourceRange& range,
        const base::u32 payload
    ) {
        this->headers_[index] = ExprNodeHeader {range, payload, kind};
    }

    [[nodiscard]] bool payload_available(const base::usize index) const noexcept {
        return index < this->headers_.size() && this->headers_[index].payload != UINT32_MAX;
    }

    [[nodiscard]] bool is_literal(const ExprKind kind) const noexcept {
        switch (kind) {
        case ExprKind::integer_literal:
        case ExprKind::float_literal:
        case ExprKind::bool_literal:
        case ExprKind::null_literal:
        case ExprKind::string_literal:
        case ExprKind::c_string_literal:
        case ExprKind::raw_string_literal:
        case ExprKind::byte_string_literal:
        case ExprKind::byte_literal:
        case ExprKind::char_literal:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] bool is_cast_like(const ExprKind kind) const noexcept {
        switch (kind) {
        case ExprKind::cast:
        case ExprKind::pcast:
        case ExprKind::bcast:
        case ExprKind::size_of:
        case ExprKind::align_of:
        case ExprKind::ptr_addr:
        case ExprKind::paddr:
        case ExprKind::str_data:
        case ExprKind::str_byte_len:
        case ExprKind::str_is_valid_utf8:
        case ExprKind::str_from_utf8_checked:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] bool is_block_payload_kind(const ExprKind kind) const noexcept {
        return kind == ExprKind::block_expr || kind == ExprKind::unsafe_block;
    }

    [[nodiscard]] static base::usize allocation_bytes(
        const base::usize count,
        const base::usize element_size
    ) noexcept {
        if (count == 0) {
            return 0;
        }
        return count * element_size + SYNTAX_AST_ARENA_ALLOCATION_PADDING_BYTES;
    }

    [[nodiscard]] static base::usize estimated_arena_bytes(
        const AstReserveEstimate::Exprs& plan
    ) noexcept {
        return allocation_bytes(plan.headers, sizeof(ExprNodeHeader)) +
               allocation_bytes(plan.literals, sizeof(LiteralExprPayload)) +
               allocation_bytes(plan.names, sizeof(NameExprPayload)) +
               allocation_bytes(plan.generic_applies, sizeof(GenericApplyExprPayload)) +
               allocation_bytes(plan.unaries, sizeof(UnaryExprPayload)) +
               allocation_bytes(plan.binaries, sizeof(BinaryExprPayload)) +
               allocation_bytes(plan.calls, sizeof(CallExprPayload)) +
               allocation_bytes(plan.ifs, sizeof(IfExprPayload)) +
               allocation_bytes(plan.blocks, sizeof(BlockExprPayload)) +
               allocation_bytes(plan.matches, sizeof(MatchExprPayload)) +
               allocation_bytes(plan.arrays, sizeof(ArrayExprPayload)) +
               allocation_bytes(plan.tuples, sizeof(AstArenaVector<ExprId>)) +
               allocation_bytes(plan.postfix_chains, sizeof(PostfixChainExprPayload)) +
               allocation_bytes(plan.fields, sizeof(FieldExprPayload)) +
               allocation_bytes(plan.indexes, sizeof(IndexExprPayload)) +
               allocation_bytes(plan.slices, sizeof(SliceExprPayload)) +
               allocation_bytes(plan.struct_literals, sizeof(StructLiteralExprPayload)) +
               allocation_bytes(plan.casts, sizeof(CastExprPayload));
    }

    void reserve_payloads(const AstReserveEstimate::Exprs& plan) {
        this->payloads_.literals.reserve(plan.literals);
        this->payloads_.names.reserve(plan.names);
        this->payloads_.generic_applies.reserve(plan.generic_applies);
        this->payloads_.unaries.reserve(plan.unaries);
        this->payloads_.binaries.reserve(plan.binaries);
        this->payloads_.calls.reserve(plan.calls);
        this->payloads_.ifs.reserve(plan.ifs);
        this->payloads_.blocks.reserve(plan.blocks);
        this->payloads_.matches.reserve(plan.matches);
        this->payloads_.arrays.reserve(plan.arrays);
        this->payloads_.tuples.reserve(plan.tuples);
        this->payloads_.postfix_chains.reserve(plan.postfix_chains);
        this->payloads_.fields.reserve(plan.fields);
        this->payloads_.indexes.reserve(plan.indexes);
        this->payloads_.slices.reserve(plan.slices);
        this->payloads_.struct_literals.reserve(plan.struct_literals);
        this->payloads_.casts.reserve(plan.casts);
    }

    template <typename T>
    [[nodiscard]] base::u32 push_payload(AstArenaVector<T>& payloads, T payload) {
        return this->emplace_payload(payloads, std::move(payload));
    }

    template <typename T, typename... Args>
    [[nodiscard]] base::u32 emplace_payload(AstArenaVector<T>& payloads, Args&&... args) {
        const base::u32 index = static_cast<base::u32>(payloads.size());
        payloads.emplace_back(std::forward<Args>(args)...);
        return index;
    }

    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_list(const std::vector<T, Allocator>& values) {
        return copy_ast_arena_vector(*this->arena_, values);
    }

    template <typename T>
    [[nodiscard]] AstArenaVector<T> copy_or_move_list(AstArenaVector<T>&& values) {
        return move_or_copy_ast_arena_vector(*this->arena_, std::move(values));
    }

    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_or_move_list(std::vector<T, Allocator>&& values) {
        return this->copy_list(values);
    }

    [[nodiscard]] bool postfix_op_lists_use_this_arena(const PostfixOp& op) const noexcept {
        return ast_arena_vector_uses_arena(op.bracket_args, *this->arena_) &&
               ast_arena_vector_uses_arena(op.args, *this->arena_) &&
               ast_arena_vector_uses_arena(op.field_inits, *this->arena_);
    }

    template <typename Allocator>
    [[nodiscard]] bool postfix_ops_use_this_arena(const std::vector<PostfixOp, Allocator>& ops) const noexcept {
        for (const PostfixOp& op : ops) {
            if (!this->postfix_op_lists_use_this_arena(op)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] PostfixOp copy_postfix_op(const PostfixOp& op) {
        PostfixOp copy;
        copy.kind = op.kind;
        copy.range = op.range;
        copy.name = op.name;
        copy.name_id = op.name_id;
        copy.bracket_args = this->copy_list(op.bracket_args);
        copy.bracket_is_slice = op.bracket_is_slice;
        copy.slice_start = op.slice_start;
        copy.slice_end = op.slice_end;
        copy.args = this->copy_list(op.args);
        copy.field_inits = this->copy_list(op.field_inits);
        return copy;
    }

    [[nodiscard]] PostfixOp copy_or_move_postfix_op(PostfixOp&& op) {
        PostfixOp copy;
        copy.kind = op.kind;
        copy.range = op.range;
        copy.name = op.name;
        copy.name_id = op.name_id;
        copy.bracket_args = this->copy_or_move_list(std::move(op.bracket_args));
        copy.bracket_is_slice = op.bracket_is_slice;
        copy.slice_start = op.slice_start;
        copy.slice_end = op.slice_end;
        copy.args = this->copy_or_move_list(std::move(op.args));
        copy.field_inits = this->copy_or_move_list(std::move(op.field_inits));
        return copy;
    }

    template <typename Allocator>
    [[nodiscard]] AstArenaVector<PostfixOp> copy_postfix_ops(const std::vector<PostfixOp, Allocator>& ops) {
        AstArenaVector<PostfixOp> copy = make_ast_arena_vector<PostfixOp>(*this->arena_);
        copy.reserve(ops.size());
        for (const PostfixOp& op : ops) {
            copy.push_back(this->copy_postfix_op(op));
        }
        return copy;
    }

    [[nodiscard]] AstArenaVector<PostfixOp> copy_or_move_postfix_ops(AstArenaVector<PostfixOp>&& ops) {
        if (ast_arena_vector_uses_arena(ops, *this->arena_) && this->postfix_ops_use_this_arena(ops)) {
            return std::move(ops);
        }
        AstArenaVector<PostfixOp> copy = make_ast_arena_vector<PostfixOp>(*this->arena_);
        copy.reserve(ops.size());
        for (PostfixOp& op : ops) {
            copy.push_back(this->copy_or_move_postfix_op(std::move(op)));
        }
        return copy;
    }

    template <typename Allocator>
    [[nodiscard]] AstArenaVector<PostfixOp> copy_or_move_postfix_ops(std::vector<PostfixOp, Allocator>&& ops) {
        return this->copy_postfix_ops(ops);
    }

    [[nodiscard]] PostfixChainExprPayload copy_postfix_chain_payload(const PostfixChainExprPayload& payload) {
        return PostfixChainExprPayload {
            payload.base,
            this->copy_postfix_ops(payload.ops),
        };
    }

    void copy_append_from(const ExprNodeList& other, const base::usize index) {
        const ExprKind kind = other.kind(index);
        const base::SourceRange range = other.range(index);
        if (kind == ExprKind::invalid) {
            static_cast<void>(this->append_invalid(range));
            return;
        }
        if (this->is_literal(kind)) {
            const LiteralExprPayload* const payload = other.literal_payload(index);
            static_cast<void>(this->append_literal(kind, range, payload != nullptr ? payload->text : std::string_view {}));
            return;
        }
        switch (kind) {
        case ExprKind::name: {
            const NameExprPayload* const payload = other.name_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_name(
                      range,
                      {},
                      {},
                      {},
                      INVALID_IDENT_ID,
                      INVALID_IDENT_ID,
                      std::vector<TypeId> {}
                  )
                : this->append_name(
                      range,
                      payload->scope_name,
                      payload->scope_range,
                      payload->text,
                      payload->scope_name_id,
                      payload->text_id,
                      copy_std_vector(payload->type_args)
                  ));
            break;
        }
        case ExprKind::generic_apply: {
            const GenericApplyExprPayload* const payload = other.generic_apply_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_generic_apply(range, INVALID_EXPR_ID, std::vector<TypeId> {})
                : this->append_generic_apply(range, payload->callee, copy_std_vector(payload->type_args)));
            break;
        }
        case ExprKind::unary:
        case ExprKind::try_expr: {
            const UnaryExprPayload* const payload = other.unary_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_unary(kind, range, UnaryOp::logical_not, INVALID_EXPR_ID)
                : this->append_unary(kind, range, payload->op, payload->operand));
            break;
        }
        case ExprKind::binary: {
            const BinaryExprPayload* const payload = other.binary_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_binary(range, BinaryOp::add, INVALID_EXPR_ID, INVALID_EXPR_ID)
                : this->append_binary(range, payload->op, payload->lhs, payload->rhs));
            break;
        }
        case ExprKind::call:
        case ExprKind::str_from_bytes_unchecked: {
            const CallExprPayload* const payload = other.call_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_call(kind, range, INVALID_EXPR_ID, std::vector<ExprId> {})
                : this->append_call(kind, range, payload->callee, copy_std_vector(payload->args)));
            break;
        }
        case ExprKind::if_expr: {
            const IfExprPayload* const payload = other.if_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_if(range, INVALID_EXPR_ID, INVALID_PATTERN_ID, INVALID_EXPR_ID, INVALID_EXPR_ID)
                : this->append_if(range, payload->condition, payload->condition_pattern, payload->then_expr, payload->else_expr));
            break;
        }
        case ExprKind::block_expr:
        case ExprKind::unsafe_block: {
            const BlockExprPayload* const payload = other.block_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_block(kind, range, INVALID_STMT_ID, INVALID_EXPR_ID)
                : this->append_block(kind, range, payload->block, payload->result));
            break;
        }
        case ExprKind::match_expr: {
            const MatchExprPayload* const payload = other.match_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_match(range, INVALID_EXPR_ID, std::vector<MatchArm> {})
                : this->append_match(range, payload->value, copy_std_vector(payload->arms)));
            break;
        }
        case ExprKind::array_literal: {
            const ArrayExprPayload* const payload = other.array_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_array(range, std::vector<ExprId> {}, INVALID_EXPR_ID, INVALID_EXPR_ID)
                : this->append_array(
                      range,
                      copy_std_vector(payload->elements),
                      payload->repeat_value,
                      payload->repeat_count
                  ));
            break;
        }
        case ExprKind::tuple_literal: {
            const AstArenaVector<ExprId>* const payload = other.tuple_elements(index);
            static_cast<void>(this->append_tuple(range, payload == nullptr ? std::vector<ExprId> {} : copy_std_vector(*payload)));
            break;
        }
        case ExprKind::postfix_chain: {
            const PostfixChainExprPayload* const payload = other.postfix_chain_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_postfix_chain(range, INVALID_EXPR_ID, std::vector<PostfixOp> {})
                : this->append_postfix_chain(range, payload->base, copy_std_vector(payload->ops)));
            break;
        }
        case ExprKind::field: {
            const FieldExprPayload* const payload = other.field_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_field(range, INVALID_EXPR_ID, {}, INVALID_IDENT_ID)
                : this->append_field(range, payload->object, payload->field_name, payload->field_name_id));
            break;
        }
        case ExprKind::index: {
            const IndexExprPayload* const payload = other.index_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_index(range, INVALID_EXPR_ID, INVALID_EXPR_ID)
                : this->append_index(range, payload->object, payload->index));
            break;
        }
        case ExprKind::slice: {
            const SliceExprPayload* const payload = other.slice_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_slice(range, INVALID_EXPR_ID, INVALID_EXPR_ID, INVALID_EXPR_ID)
                : this->append_slice(range, payload->object, payload->start, payload->end));
            break;
        }
        case ExprKind::struct_literal: {
            const StructLiteralExprPayload* const payload = other.struct_literal_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_struct_literal(
                      range,
                      INVALID_EXPR_ID,
                      {},
                      {},
                      {},
                      INVALID_IDENT_ID,
                      INVALID_IDENT_ID,
                      std::vector<TypeId> {},
                      std::vector<FieldInit> {}
                  )
                : this->append_struct_literal(
                      range,
                      payload->object,
                      payload->scope_name,
                      payload->scope_range,
                      payload->name,
                      payload->scope_name_id,
                      payload->name_id,
                      copy_std_vector(payload->type_args),
                      copy_std_vector(payload->field_inits)
                  ));
            break;
        }
        case ExprKind::cast:
        case ExprKind::pcast:
        case ExprKind::bcast:
        case ExprKind::size_of:
        case ExprKind::align_of:
        case ExprKind::ptr_addr:
        case ExprKind::paddr:
        case ExprKind::str_data:
        case ExprKind::str_byte_len:
        case ExprKind::str_is_valid_utf8:
        case ExprKind::str_from_utf8_checked: {
            const CastExprPayload* const payload = other.cast_payload(index);
            static_cast<void>(payload == nullptr
                ? this->append_cast_like(kind, range, INVALID_TYPE_ID, INVALID_EXPR_ID)
                : this->append_cast_like(kind, range, payload->type, payload->expr));
            break;
        }
        default:
            static_cast<void>(this->append_invalid(range));
            break;
        }
    }

    void copy_from(const ExprNodeList& other) {
        this->reserve(other.size());
        for (base::usize i = 0; i < other.size(); ++i) {
            this->copy_append_from(other, i);
        }
    }

    void swap(ExprNodeList& other) noexcept {
        using std::swap;
        swap(this->arena_, other.arena_);
        this->headers_.swap(other.headers_);
        this->payloads_.swap(other.payloads_);
    }

    std::unique_ptr<base::BumpAllocator> arena_;
    AstArenaVector<ExprNodeHeader> headers_;
    ExprNodePayloadArena payloads_;
};

struct PatternNodeHeader {
    base::SourceRange range {};
    base::u32 payload = UINT32_MAX;
    PatternKind kind = PatternKind::wildcard;
};

struct BindingPatternPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
};

struct EnumCasePatternPayload {
    std::string_view enum_name;
    std::string_view case_name;
    TypeId enum_type = INVALID_TYPE_ID;
    AstArenaVector<PatternId> payload_patterns;
    AstArenaVector<std::string_view> binding_names;
    IdentId enum_name_id = INVALID_IDENT_ID;
    IdentId case_name_id = INVALID_IDENT_ID;
    AstArenaVector<IdentId> binding_name_ids;
    bool scoped = false;
};

struct LiteralPatternPayload {
    std::string_view case_name;
    AstArenaVector<std::string_view> binding_names;
    IdentId case_name_id = INVALID_IDENT_ID;
    AstArenaVector<IdentId> binding_name_ids;
};

struct SlicePatternPayload {
    AstArenaVector<PatternId> elements;
    base::usize rest_index = 0;
    bool has_rest = false;
};

struct StructPatternPayload {
    std::string_view name;
    AstArenaVector<FieldPattern> fields;
    IdentId name_id = INVALID_IDENT_ID;
};

struct PatternNodePayloadArena {
    PatternNodePayloadArena() = default;

    explicit PatternNodePayloadArena(base::BumpAllocator& arena)
        : bindings(base::BumpAllocatorAdapter<BindingPatternPayload> {arena}),
          literals(base::BumpAllocatorAdapter<LiteralPatternPayload> {arena}),
          enum_cases(base::BumpAllocatorAdapter<EnumCasePatternPayload> {arena}),
          tuples(base::BumpAllocatorAdapter<AstArenaVector<PatternId>> {arena}),
          slices(base::BumpAllocatorAdapter<SlicePatternPayload> {arena}),
          structs(base::BumpAllocatorAdapter<StructPatternPayload> {arena}),
          alternatives(base::BumpAllocatorAdapter<AstArenaVector<PatternId>> {arena}) {}

    void swap(PatternNodePayloadArena& other) noexcept {
        this->bindings.swap(other.bindings);
        this->literals.swap(other.literals);
        this->enum_cases.swap(other.enum_cases);
        this->tuples.swap(other.tuples);
        this->slices.swap(other.slices);
        this->structs.swap(other.structs);
        this->alternatives.swap(other.alternatives);
    }

    AstArenaVector<BindingPatternPayload> bindings;
    AstArenaVector<LiteralPatternPayload> literals;
    AstArenaVector<EnumCasePatternPayload> enum_cases;
    AstArenaVector<AstArenaVector<PatternId>> tuples;
    AstArenaVector<SlicePatternPayload> slices;
    AstArenaVector<StructPatternPayload> structs;
    AstArenaVector<AstArenaVector<PatternId>> alternatives;
};

class PatternNodeList final {
public:
    PatternNodeList()
        : arena_(std::make_unique<base::BumpAllocator>()),
          headers_(base::BumpAllocatorAdapter<PatternNodeHeader> {*this->arena_}),
          payloads_(*this->arena_) {}

    PatternNodeList(const PatternNodeList& other)
        : PatternNodeList() {
        this->copy_from(other);
    }

    PatternNodeList& operator=(const PatternNodeList& other) {
        if (this == &other) {
            return *this;
        }
        PatternNodeList copy(other);
        *this = std::move(copy);
        return *this;
    }

    PatternNodeList(PatternNodeList&& other) noexcept
        : arena_(std::move(other.arena_)),
          headers_(std::move(other.headers_)),
          payloads_(std::move(other.payloads_)) {
        other.headers_ = AstArenaVector<PatternNodeHeader> {};
        other.payloads_ = PatternNodePayloadArena {};
        other.materialized_.clear();
        other.materialized_valid_.clear();
    }

    PatternNodeList& operator=(PatternNodeList&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        this->swap(other);
        return *this;
    }
    ~PatternNodeList() = default;

    [[nodiscard]] base::usize size() const noexcept {
        return this->headers_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return this->headers_.empty();
    }

    [[nodiscard]] PatternKind kind(const base::usize index) const noexcept {
        return this->headers_[index].kind;
    }

    [[nodiscard]] base::usize arena_bytes() const noexcept {
        return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
    }

    [[nodiscard]] base::usize arena_blocks() const noexcept {
        return this->arena_ == nullptr ? 0 : this->arena_->block_count();
    }

    [[nodiscard]] base::SourceRange range(const base::usize index) const noexcept {
        return this->headers_[index].range;
    }

    void reserve(const base::usize size) {
        this->reserve_headers(size);
        const base::usize primary = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_PRIMARY_PAYLOAD_DIVISOR);
        const base::usize secondary = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR);
        const base::usize rare = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR);
        this->payloads_.bindings.reserve(primary);
        this->payloads_.literals.reserve(secondary);
        this->payloads_.enum_cases.reserve(primary);
        this->payloads_.tuples.reserve(secondary);
        this->payloads_.slices.reserve(secondary);
        this->payloads_.structs.reserve(secondary);
        this->payloads_.alternatives.reserve(rare);
    }

    void reserve_headers(const base::usize size) {
        this->headers_.reserve(size);
    }

    void push_back(PatternNode node) {
        static_cast<void>(this->append(std::move(node)));
    }

    [[nodiscard]] PatternId append(PatternNode node) {
        const PatternId id {static_cast<base::u32>(this->headers_.size())};
        PatternNodeHeader header;
        header.kind = node.kind;
        header.range = node.range;
        header.payload = this->store_payload(std::move(node));
        this->headers_.push_back(header);
        return id;
    }

    void set(const base::usize index, PatternNode node) {
        PatternNodeHeader header;
        header.kind = node.kind;
        header.range = node.range;
        header.payload = this->store_payload(std::move(node));
        this->headers_[index] = header;
        this->invalidate_materialized(index);
    }

    [[nodiscard]] PatternNode take(const base::usize index) {
        this->invalidate_materialized(index);
        return this->load_moved(index);
    }

    [[nodiscard]] PatternNode operator[](const base::usize index) const {
        return this->load(index);
    }

    [[nodiscard]] const PatternNode* ptr(const base::usize index) const {
        if (index >= this->headers_.size()) {
            return nullptr;
        }
        return &this->materialized(index);
    }

private:
    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_list(const std::vector<T, Allocator>& values) {
        return copy_ast_arena_vector(*this->arena_, values);
    }

    [[nodiscard]] base::u32 store_payload(PatternNode node) {
        switch (node.kind) {
        case PatternKind::binding:
        case PatternKind::const_:
            return this->push_payload(this->payloads_.bindings, BindingPatternPayload {
                node.binding_name,
                node.binding_name_id,
            });
        case PatternKind::literal:
            return this->push_payload(this->payloads_.literals, LiteralPatternPayload {
                node.case_name,
                this->copy_list(node.binding_names),
                node.case_name_id,
                this->copy_list(node.binding_name_ids),
            });
        case PatternKind::enum_case:
            return this->push_payload(this->payloads_.enum_cases, EnumCasePatternPayload {
                node.enum_name,
                node.case_name,
                node.enum_type,
                this->copy_list(node.payload_patterns),
                this->copy_list(node.binding_names),
                node.enum_name_id,
                node.case_name_id,
                this->copy_list(node.binding_name_ids),
                node.scoped,
            });
        case PatternKind::tuple:
            return this->push_payload(this->payloads_.tuples, this->copy_list(node.elements));
        case PatternKind::slice:
            return this->push_payload(this->payloads_.slices, SlicePatternPayload {
                this->copy_list(node.elements),
                node.slice_rest_index,
                node.has_slice_rest,
            });
        case PatternKind::struct_:
            return this->push_payload(this->payloads_.structs, StructPatternPayload {
                node.struct_name,
                this->copy_list(node.field_patterns),
                node.struct_name_id,
            });
        case PatternKind::or_pattern:
            return this->push_payload(this->payloads_.alternatives, this->copy_list(node.alternatives));
        case PatternKind::wildcard:
            break;
        }
        return UINT32_MAX;
    }

    [[nodiscard]] PatternNode load(const base::usize index) const {
        const PatternNodeHeader& header = this->headers_[index];
        PatternNode node;
        node.kind = header.kind;
        node.range = header.range;
        switch (header.kind) {
        case PatternKind::binding:
        case PatternKind::const_:
            node.binding_name = this->payloads_.bindings[header.payload].name;
            node.binding_name_id = this->payloads_.bindings[header.payload].name_id;
            break;
        case PatternKind::literal: {
            const LiteralPatternPayload& payload = this->payloads_.literals[header.payload];
            node.case_name = payload.case_name;
            node.binding_names = copy_std_vector(payload.binding_names);
            node.case_name_id = payload.case_name_id;
            node.binding_name_ids = copy_std_vector(payload.binding_name_ids);
            break;
        }
        case PatternKind::enum_case: {
            const EnumCasePatternPayload& payload = this->payloads_.enum_cases[header.payload];
            node.enum_name = payload.enum_name;
            node.case_name = payload.case_name;
            node.enum_type = payload.enum_type;
            node.payload_patterns = copy_std_vector(payload.payload_patterns);
            node.binding_names = copy_std_vector(payload.binding_names);
            node.enum_name_id = payload.enum_name_id;
            node.case_name_id = payload.case_name_id;
            node.binding_name_ids = copy_std_vector(payload.binding_name_ids);
            node.scoped = payload.scoped;
            break;
        }
        case PatternKind::tuple:
            node.elements = copy_std_vector(this->payloads_.tuples[header.payload]);
            break;
        case PatternKind::slice: {
            const SlicePatternPayload& payload = this->payloads_.slices[header.payload];
            node.elements = copy_std_vector(payload.elements);
            node.slice_rest_index = payload.rest_index;
            node.has_slice_rest = payload.has_rest;
            break;
        }
        case PatternKind::struct_: {
            const StructPatternPayload& payload = this->payloads_.structs[header.payload];
            node.struct_name = payload.name;
            node.field_patterns = copy_std_vector(payload.fields);
            node.struct_name_id = payload.name_id;
            break;
        }
        case PatternKind::or_pattern:
            node.alternatives = copy_std_vector(this->payloads_.alternatives[header.payload]);
            break;
        case PatternKind::wildcard:
            break;
        }
        return node;
    }

    [[nodiscard]] PatternNode load_moved(const base::usize index) {
        const PatternNodeHeader& header = this->headers_[index];
        PatternNode node;
        node.kind = header.kind;
        node.range = header.range;
        switch (header.kind) {
        case PatternKind::binding:
        case PatternKind::const_:
            node.binding_name = this->payloads_.bindings[header.payload].name;
            node.binding_name_id = this->payloads_.bindings[header.payload].name_id;
            break;
        case PatternKind::literal: {
            LiteralPatternPayload& payload = this->payloads_.literals[header.payload];
            node.case_name = payload.case_name;
            node.binding_names = copy_std_vector(payload.binding_names);
            node.case_name_id = payload.case_name_id;
            node.binding_name_ids = copy_std_vector(payload.binding_name_ids);
            break;
        }
        case PatternKind::enum_case: {
            EnumCasePatternPayload& payload = this->payloads_.enum_cases[header.payload];
            node.enum_name = payload.enum_name;
            node.case_name = payload.case_name;
            node.enum_type = payload.enum_type;
            node.payload_patterns = copy_std_vector(payload.payload_patterns);
            node.binding_names = copy_std_vector(payload.binding_names);
            node.enum_name_id = payload.enum_name_id;
            node.case_name_id = payload.case_name_id;
            node.binding_name_ids = copy_std_vector(payload.binding_name_ids);
            node.scoped = payload.scoped;
            break;
        }
        case PatternKind::tuple:
            node.elements = copy_std_vector(this->payloads_.tuples[header.payload]);
            break;
        case PatternKind::slice: {
            SlicePatternPayload& payload = this->payloads_.slices[header.payload];
            node.elements = copy_std_vector(payload.elements);
            node.slice_rest_index = payload.rest_index;
            node.has_slice_rest = payload.has_rest;
            break;
        }
        case PatternKind::struct_: {
            StructPatternPayload& payload = this->payloads_.structs[header.payload];
            node.struct_name = payload.name;
            node.field_patterns = copy_std_vector(payload.fields);
            node.struct_name_id = payload.name_id;
            break;
        }
        case PatternKind::or_pattern:
            node.alternatives = copy_std_vector(this->payloads_.alternatives[header.payload]);
            break;
        case PatternKind::wildcard:
            break;
        }
        return node;
    }

    [[nodiscard]] const PatternNode& materialized(const base::usize index) const {
        this->ensure_materialized_capacity(index + 1);
        if (!this->materialized_valid_[index]) {
            this->materialized_[index] = this->load(index);
            this->materialized_valid_[index] = true;
        }
        return this->materialized_[index];
    }

    void ensure_materialized_capacity(const base::usize size) const {
        if (this->materialized_.size() < size) {
            this->materialized_.resize(size);
        }
        if (this->materialized_valid_.size() < size) {
            this->materialized_valid_.resize(size, false);
        }
    }

    void invalidate_materialized(const base::usize index) const
    {
        if (index < this->materialized_valid_.size()) {
            this->materialized_valid_[index] = false;
        }
    }

    template <typename T>
    [[nodiscard]] base::u32 push_payload(AstArenaVector<T>& payloads, T payload) {
        const base::u32 index = static_cast<base::u32>(payloads.size());
        payloads.push_back(std::move(payload));
        return index;
    }

    void copy_from(const PatternNodeList& other) {
        this->reserve(other.size());
        for (base::usize i = 0; i < other.size(); ++i) {
            static_cast<void>(this->append(other.load(i)));
        }
    }

    void swap(PatternNodeList& other) noexcept {
        using std::swap;
        swap(this->arena_, other.arena_);
        this->headers_.swap(other.headers_);
        this->payloads_.swap(other.payloads_);
        this->materialized_.swap(other.materialized_);
        this->materialized_valid_.swap(other.materialized_valid_);
    }

    std::unique_ptr<base::BumpAllocator> arena_;
    AstArenaVector<PatternNodeHeader> headers_;
    PatternNodePayloadArena payloads_;
    mutable std::deque<PatternNode> materialized_;
    mutable std::vector<bool> materialized_valid_;
};

enum class StmtKind {
    let,
    var,
    assign,
    if_,
    for_,
    for_range,
    while_,
    break_,
    continue_,
    defer,
    return_,
    expr,
    block,
};

struct StmtNode {
    StmtKind kind = StmtKind::expr;
    base::SourceRange range {};
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    PatternId pattern = INVALID_PATTERN_ID;
    TypeId declared_type = INVALID_TYPE_ID;
    ExprId init = INVALID_EXPR_ID;
    AssignOp assign_op = AssignOp::assign;
    ExprId lhs = INVALID_EXPR_ID;
    ExprId rhs = INVALID_EXPR_ID;
    ExprId condition = INVALID_EXPR_ID;
    ExprId range_start = INVALID_EXPR_ID;
    ExprId range_end = INVALID_EXPR_ID;
    ExprId range_step = INVALID_EXPR_ID;
    StmtId then_block = INVALID_STMT_ID;
    StmtId else_block = INVALID_STMT_ID;
    StmtId else_if = INVALID_STMT_ID;
    StmtId body = INVALID_STMT_ID;
    StmtId for_init = INVALID_STMT_ID;
    StmtId for_update = INVALID_STMT_ID;
    ExprId return_value = INVALID_EXPR_ID;
    std::vector<StmtId> statements;
};

struct StmtNodeHeader {
    base::SourceRange range {};
    base::u32 payload = UINT32_MAX;
    base::u8 kind = static_cast<base::u8>(StmtKind::expr);
};

struct LocalStmtPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    PatternId pattern = INVALID_PATTERN_ID;
    TypeId declared_type = INVALID_TYPE_ID;
    ExprId init = INVALID_EXPR_ID;
    StmtId else_block = INVALID_STMT_ID;
};

struct AssignStmtPayload {
    AssignOp op = AssignOp::assign;
    ExprId lhs = INVALID_EXPR_ID;
    ExprId rhs = INVALID_EXPR_ID;
};

struct IfStmtPayload {
    ExprId condition = INVALID_EXPR_ID;
    PatternId pattern = INVALID_PATTERN_ID;
    StmtId then_block = INVALID_STMT_ID;
    StmtId else_block = INVALID_STMT_ID;
    StmtId else_if = INVALID_STMT_ID;
};

struct ForStmtPayload {
    StmtId init = INVALID_STMT_ID;
    ExprId condition = INVALID_EXPR_ID;
    StmtId update = INVALID_STMT_ID;
    StmtId body = INVALID_STMT_ID;
};

struct ForRangeStmtPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    ExprId start = INVALID_EXPR_ID;
    ExprId end = INVALID_EXPR_ID;
    ExprId step = INVALID_EXPR_ID;
    StmtId body = INVALID_STMT_ID;
};

struct WhileStmtPayload {
    ExprId condition = INVALID_EXPR_ID;
    PatternId pattern = INVALID_PATTERN_ID;
    StmtId body = INVALID_STMT_ID;
};

struct ExprStmtPayload {
    ExprId value = INVALID_EXPR_ID;
};

struct StmtNodePayloadArena {
    StmtNodePayloadArena() = default;

    explicit StmtNodePayloadArena(base::BumpAllocator& arena)
        : locals(base::BumpAllocatorAdapter<LocalStmtPayload> {arena}),
          assigns(base::BumpAllocatorAdapter<AssignStmtPayload> {arena}),
          ifs(base::BumpAllocatorAdapter<IfStmtPayload> {arena}),
          fors(base::BumpAllocatorAdapter<ForStmtPayload> {arena}),
          for_ranges(base::BumpAllocatorAdapter<ForRangeStmtPayload> {arena}),
          whiles(base::BumpAllocatorAdapter<WhileStmtPayload> {arena}),
          exprs(base::BumpAllocatorAdapter<ExprStmtPayload> {arena}),
          defers(base::BumpAllocatorAdapter<ExprStmtPayload> {arena}),
          returns(base::BumpAllocatorAdapter<ExprStmtPayload> {arena}),
          blocks(base::BumpAllocatorAdapter<AstArenaVector<StmtId>> {arena}),
          unknowns(base::BumpAllocatorAdapter<StmtNode> {arena}) {}

    void swap(StmtNodePayloadArena& other) noexcept {
        this->locals.swap(other.locals);
        this->assigns.swap(other.assigns);
        this->ifs.swap(other.ifs);
        this->fors.swap(other.fors);
        this->for_ranges.swap(other.for_ranges);
        this->whiles.swap(other.whiles);
        this->exprs.swap(other.exprs);
        this->defers.swap(other.defers);
        this->returns.swap(other.returns);
        this->blocks.swap(other.blocks);
        this->unknowns.swap(other.unknowns);
    }

    AstArenaVector<LocalStmtPayload> locals;
    AstArenaVector<AssignStmtPayload> assigns;
    AstArenaVector<IfStmtPayload> ifs;
    AstArenaVector<ForStmtPayload> fors;
    AstArenaVector<ForRangeStmtPayload> for_ranges;
    AstArenaVector<WhileStmtPayload> whiles;
    AstArenaVector<ExprStmtPayload> exprs;
    AstArenaVector<ExprStmtPayload> defers;
    AstArenaVector<ExprStmtPayload> returns;
    AstArenaVector<AstArenaVector<StmtId>> blocks;
    AstArenaVector<StmtNode> unknowns;
};

class StmtNodeList final {
public:
    StmtNodeList()
        : arena_(std::make_unique<base::BumpAllocator>()),
          headers_(base::BumpAllocatorAdapter<StmtNodeHeader> {*this->arena_}),
          payloads_(*this->arena_) {}

    StmtNodeList(const StmtNodeList& other)
        : StmtNodeList() {
        this->copy_from(other);
    }

    StmtNodeList& operator=(const StmtNodeList& other) {
        if (this == &other) {
            return *this;
        }
        StmtNodeList copy(other);
        *this = std::move(copy);
        return *this;
    }

    StmtNodeList(StmtNodeList&& other) noexcept
        : arena_(std::move(other.arena_)),
          headers_(std::move(other.headers_)),
          payloads_(std::move(other.payloads_)) {
        other.headers_ = AstArenaVector<StmtNodeHeader> {};
        other.payloads_ = StmtNodePayloadArena {};
    }

    StmtNodeList& operator=(StmtNodeList&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        this->swap(other);
        return *this;
    }
    ~StmtNodeList() = default;

    [[nodiscard]] base::usize size() const noexcept {
        return this->headers_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return this->headers_.empty();
    }

    [[nodiscard]] StmtKind kind(const base::usize index) const noexcept {
        return static_cast<StmtKind>(this->headers_[index].kind);
    }

    [[nodiscard]] base::usize arena_bytes() const noexcept {
        return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
    }

    [[nodiscard]] base::usize arena_blocks() const noexcept {
        return this->arena_ == nullptr ? 0 : this->arena_->block_count();
    }

    [[nodiscard]] base::SourceRange range(const base::usize index) const noexcept {
        return this->headers_[index].range;
    }

    [[nodiscard]] const AstArenaVector<StmtId>* block_statements(const base::usize index) const noexcept {
        if (this->kind(index) != StmtKind::block) {
            return nullptr;
        }
        return &this->payloads_.blocks[this->headers_[index].payload];
    }

    void reserve(const base::usize size) {
        this->reserve_headers(size);
        const base::usize primary = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_PRIMARY_PAYLOAD_DIVISOR);
        const base::usize secondary = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR);
        const base::usize rare = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR);
        this->payloads_.locals.reserve(primary);
        this->payloads_.assigns.reserve(size);
        this->payloads_.ifs.reserve(secondary);
        this->payloads_.fors.reserve(rare);
        this->payloads_.for_ranges.reserve(rare);
        this->payloads_.whiles.reserve(rare);
        this->payloads_.exprs.reserve(primary);
        this->payloads_.defers.reserve(rare);
        this->payloads_.returns.reserve(secondary);
        this->payloads_.blocks.reserve(secondary);
        this->payloads_.unknowns.reserve(rare);
    }

    void reserve_headers(const base::usize size) {
        this->headers_.reserve(size);
    }

    void push_back(StmtNode node) {
        static_cast<void>(this->append(std::move(node)));
    }

    [[nodiscard]] StmtId append(StmtNode node) {
        const StmtId id {static_cast<base::u32>(this->headers_.size())};
        StmtNodeHeader header;
        header.kind = pack_kind(node.kind);
        header.range = node.range;
        header.payload = this->store_payload(std::move(node));
        this->headers_.push_back(header);
        return id;
    }

    void set(const base::usize index, StmtNode node) {
        StmtNodeHeader header;
        header.kind = pack_kind(node.kind);
        header.range = node.range;
        header.payload = this->store_payload(std::move(node));
        this->headers_[index] = header;
    }

    void set_range(const base::usize index, const base::SourceRange& range) {
        this->headers_[index].range = range;
    }

    [[nodiscard]] StmtNode take(const base::usize index) {
        return this->load_moved(index);
    }

    [[nodiscard]] StmtNode operator[](const base::usize index) const {
        return this->load(index);
    }

private:
    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_list(const std::vector<T, Allocator>& values) {
        return copy_ast_arena_vector(*this->arena_, values);
    }

    [[nodiscard]] static base::u8 pack_kind(const StmtKind kind) noexcept {
        return static_cast<base::u8>(kind);
    }

    [[nodiscard]] base::u32 store_payload(StmtNode node) {
        switch (node.kind) {
        case StmtKind::let:
        case StmtKind::var:
            return this->push_payload(this->payloads_.locals, LocalStmtPayload {
                node.name,
                node.name_id,
                node.pattern,
                node.declared_type,
                node.init,
                node.else_block,
            });
        case StmtKind::assign:
            return this->push_payload(this->payloads_.assigns, AssignStmtPayload {
                node.assign_op,
                node.lhs,
                node.rhs,
            });
        case StmtKind::if_:
            return this->push_payload(this->payloads_.ifs, IfStmtPayload {
                node.condition,
                node.pattern,
                node.then_block,
                node.else_block,
                node.else_if,
            });
        case StmtKind::for_:
            return this->push_payload(this->payloads_.fors, ForStmtPayload {
                node.for_init,
                node.condition,
                node.for_update,
                node.body,
            });
        case StmtKind::for_range:
            return this->push_payload(this->payloads_.for_ranges, ForRangeStmtPayload {
                node.name,
                node.name_id,
                node.range_start,
                node.range_end,
                node.range_step,
                node.body,
            });
        case StmtKind::while_:
            return this->push_payload(this->payloads_.whiles, WhileStmtPayload {
                node.condition,
                node.pattern,
                node.body,
            });
        case StmtKind::expr:
            return this->push_payload(this->payloads_.exprs, ExprStmtPayload {node.init});
        case StmtKind::defer:
            return this->push_payload(this->payloads_.defers, ExprStmtPayload {node.init});
        case StmtKind::return_:
            return this->push_payload(this->payloads_.returns, ExprStmtPayload {node.return_value});
        case StmtKind::block:
            return this->push_payload(this->payloads_.blocks, this->copy_list(node.statements));
        case StmtKind::break_:
        case StmtKind::continue_:
            break;
        }
        return this->push_payload(this->payloads_.unknowns, std::move(node));
    }

    void load_header(const StmtNodeHeader& header, StmtNode& node) const noexcept {
        node.kind = static_cast<StmtKind>(header.kind);
        node.range = header.range;
    }

    [[nodiscard]] StmtNode load(const base::usize index) const {
        const StmtNodeHeader& header = this->headers_[index];
        StmtNode node;
        this->load_header(header, node);
        switch (node.kind) {
        case StmtKind::let:
        case StmtKind::var: {
            const LocalStmtPayload& payload = this->payloads_.locals[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.pattern = payload.pattern;
            node.declared_type = payload.declared_type;
            node.init = payload.init;
            node.else_block = payload.else_block;
            break;
        }
        case StmtKind::assign: {
            const AssignStmtPayload& payload = this->payloads_.assigns[header.payload];
            node.assign_op = payload.op;
            node.lhs = payload.lhs;
            node.rhs = payload.rhs;
            break;
        }
        case StmtKind::if_: {
            const IfStmtPayload& payload = this->payloads_.ifs[header.payload];
            node.condition = payload.condition;
            node.pattern = payload.pattern;
            node.then_block = payload.then_block;
            node.else_block = payload.else_block;
            node.else_if = payload.else_if;
            break;
        }
        case StmtKind::for_: {
            const ForStmtPayload& payload = this->payloads_.fors[header.payload];
            node.for_init = payload.init;
            node.condition = payload.condition;
            node.for_update = payload.update;
            node.body = payload.body;
            break;
        }
        case StmtKind::for_range: {
            const ForRangeStmtPayload& payload = this->payloads_.for_ranges[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.range_start = payload.start;
            node.range_end = payload.end;
            node.range_step = payload.step;
            node.body = payload.body;
            break;
        }
        case StmtKind::while_: {
            const WhileStmtPayload& payload = this->payloads_.whiles[header.payload];
            node.condition = payload.condition;
            node.pattern = payload.pattern;
            node.body = payload.body;
            break;
        }
        case StmtKind::expr:
            node.init = this->payloads_.exprs[header.payload].value;
            break;
        case StmtKind::defer:
            node.init = this->payloads_.defers[header.payload].value;
            break;
        case StmtKind::return_:
            node.return_value = this->payloads_.returns[header.payload].value;
            break;
        case StmtKind::block:
            node.statements = copy_std_vector(this->payloads_.blocks[header.payload]);
            break;
        case StmtKind::break_:
        case StmtKind::continue_:
            break;
        default:
            node = this->payloads_.unknowns[header.payload];
            this->load_header(header, node);
            break;
        }
        return node;
    }

    [[nodiscard]] StmtNode load_moved(const base::usize index) {
        const StmtNodeHeader& header = this->headers_[index];
        StmtNode node;
        this->load_header(header, node);
        switch (node.kind) {
        case StmtKind::let:
        case StmtKind::var: {
            const LocalStmtPayload& payload = this->payloads_.locals[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.pattern = payload.pattern;
            node.declared_type = payload.declared_type;
            node.init = payload.init;
            node.else_block = payload.else_block;
            break;
        }
        case StmtKind::assign: {
            const AssignStmtPayload& payload = this->payloads_.assigns[header.payload];
            node.assign_op = payload.op;
            node.lhs = payload.lhs;
            node.rhs = payload.rhs;
            break;
        }
        case StmtKind::if_: {
            const IfStmtPayload& payload = this->payloads_.ifs[header.payload];
            node.condition = payload.condition;
            node.pattern = payload.pattern;
            node.then_block = payload.then_block;
            node.else_block = payload.else_block;
            node.else_if = payload.else_if;
            break;
        }
        case StmtKind::for_: {
            const ForStmtPayload& payload = this->payloads_.fors[header.payload];
            node.for_init = payload.init;
            node.condition = payload.condition;
            node.for_update = payload.update;
            node.body = payload.body;
            break;
        }
        case StmtKind::for_range: {
            const ForRangeStmtPayload& payload = this->payloads_.for_ranges[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.range_start = payload.start;
            node.range_end = payload.end;
            node.range_step = payload.step;
            node.body = payload.body;
            break;
        }
        case StmtKind::while_: {
            const WhileStmtPayload& payload = this->payloads_.whiles[header.payload];
            node.condition = payload.condition;
            node.pattern = payload.pattern;
            node.body = payload.body;
            break;
        }
        case StmtKind::expr:
            node.init = this->payloads_.exprs[header.payload].value;
            break;
        case StmtKind::defer:
            node.init = this->payloads_.defers[header.payload].value;
            break;
        case StmtKind::return_:
            node.return_value = this->payloads_.returns[header.payload].value;
            break;
        case StmtKind::block:
            node.statements = copy_std_vector(this->payloads_.blocks[header.payload]);
            break;
        case StmtKind::break_:
        case StmtKind::continue_:
            break;
        default:
            node = std::move(this->payloads_.unknowns[header.payload]);
            this->load_header(header, node);
            break;
        }
        return node;
    }

    template <typename T>
    [[nodiscard]] base::u32 push_payload(AstArenaVector<T>& payloads, T payload) {
        const base::u32 index = static_cast<base::u32>(payloads.size());
        payloads.push_back(std::move(payload));
        return index;
    }

    void copy_from(const StmtNodeList& other) {
        this->reserve(other.size());
        for (base::usize i = 0; i < other.size(); ++i) {
            static_cast<void>(this->append(other.load(i)));
        }
    }

    void swap(StmtNodeList& other) noexcept {
        using std::swap;
        swap(this->arena_, other.arena_);
        this->headers_.swap(other.headers_);
        this->payloads_.swap(other.payloads_);
    }

    std::unique_ptr<base::BumpAllocator> arena_;
    AstArenaVector<StmtNodeHeader> headers_;
    StmtNodePayloadArena payloads_;
};

struct ParamDecl {
    std::string_view name;
    TypeId type = INVALID_TYPE_ID;
    base::SourceRange range {};
    IdentId name_id = INVALID_IDENT_ID;
};

struct FieldDecl {
    std::string_view name;
    TypeId type = INVALID_TYPE_ID;
    base::SourceRange range {};
    Visibility visibility = Visibility::private_;
    IdentId name_id = INVALID_IDENT_ID;
};

struct EnumCaseDecl {
    std::string_view name;
    TypeId payload_type = INVALID_TYPE_ID;
    AstArenaVector<TypeId> payload_types;
    std::string_view value_text;
    base::SourceRange range {};
    IdentId name_id = INVALID_IDENT_ID;
};

enum class ItemKind {
    const_decl,
    type_alias,
    struct_decl,
    enum_decl,
    opaque_struct_decl,
    fn_decl,
    extern_block,
    impl_block,
};

struct ItemNode {
    ItemKind kind = ItemKind::fn_decl;
    base::SourceRange range {};
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    std::vector<GenericParamDecl> generic_params;
    std::vector<GenericConstraintDecl> where_constraints;
    Visibility visibility = Visibility::private_;
    TypeId const_type = INVALID_TYPE_ID;
    ExprId const_value = INVALID_EXPR_ID;
    TypeId alias_type = INVALID_TYPE_ID;
    std::vector<FieldDecl> fields;
    TypeId enum_base_type = INVALID_TYPE_ID;
    std::vector<EnumCaseDecl> enum_cases;
    std::vector<ParamDecl> params;
    TypeId return_type = INVALID_TYPE_ID;
    StmtId body = INVALID_STMT_ID;
    TypeId impl_type = INVALID_TYPE_ID;
    bool is_export_c = false;
    bool is_extern_c = false;
    bool is_unsafe = false;
    bool is_variadic = false;
    bool is_prototype = false;
    std::string_view abi_name;
    std::vector<ItemId> extern_items;
    std::vector<ItemId> impl_items;
};

struct ItemNodeHeader {
    base::SourceRange range {};
    base::u32 payload = UINT32_MAX;
    base::u8 kind = static_cast<base::u8>(ItemKind::fn_decl);
    base::u8 visibility = static_cast<base::u8>(Visibility::private_);
    base::u8 flags = 0;
};

struct ConstItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    TypeId type = INVALID_TYPE_ID;
    ExprId value = INVALID_EXPR_ID;
};

struct TypeAliasItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<GenericParamDecl> generic_params;
    AstArenaVector<GenericConstraintDecl> where_constraints;
    TypeId target = INVALID_TYPE_ID;
};

struct StructItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<GenericParamDecl> generic_params;
    AstArenaVector<GenericConstraintDecl> where_constraints;
    AstArenaVector<FieldDecl> fields;
};

struct EnumItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<GenericParamDecl> generic_params;
    AstArenaVector<GenericConstraintDecl> where_constraints;
    TypeId base_type = INVALID_TYPE_ID;
    AstArenaVector<EnumCaseDecl> cases;
};

struct OpaqueStructItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
};

struct FunctionItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<GenericParamDecl> generic_params;
    AstArenaVector<GenericConstraintDecl> where_constraints;
    AstArenaVector<ParamDecl> params;
    TypeId return_type = INVALID_TYPE_ID;
    StmtId body = INVALID_STMT_ID;
    TypeId impl_type = INVALID_TYPE_ID;
    std::string_view abi_name;
};

struct ExternBlockItemPayload {
    AstArenaVector<ItemId> items;
};

struct ImplBlockItemPayload {
    AstArenaVector<GenericParamDecl> generic_params;
    AstArenaVector<GenericConstraintDecl> where_constraints;
    TypeId impl_type = INVALID_TYPE_ID;
    AstArenaVector<ItemId> items;
};

struct ItemNodePayloadArena {
    ItemNodePayloadArena() = default;

    explicit ItemNodePayloadArena(base::BumpAllocator& arena)
        : consts(base::BumpAllocatorAdapter<ConstItemPayload> {arena}),
          type_aliases(base::BumpAllocatorAdapter<TypeAliasItemPayload> {arena}),
          structs(base::BumpAllocatorAdapter<StructItemPayload> {arena}),
          enums(base::BumpAllocatorAdapter<EnumItemPayload> {arena}),
          opaque_structs(base::BumpAllocatorAdapter<OpaqueStructItemPayload> {arena}),
          functions(base::BumpAllocatorAdapter<FunctionItemPayload> {arena}),
          extern_blocks(base::BumpAllocatorAdapter<ExternBlockItemPayload> {arena}),
          impl_blocks(base::BumpAllocatorAdapter<ImplBlockItemPayload> {arena}),
          unknowns(base::BumpAllocatorAdapter<ItemNode> {arena}) {}

    void swap(ItemNodePayloadArena& other) noexcept {
        this->consts.swap(other.consts);
        this->type_aliases.swap(other.type_aliases);
        this->structs.swap(other.structs);
        this->enums.swap(other.enums);
        this->opaque_structs.swap(other.opaque_structs);
        this->functions.swap(other.functions);
        this->extern_blocks.swap(other.extern_blocks);
        this->impl_blocks.swap(other.impl_blocks);
        this->unknowns.swap(other.unknowns);
    }

    AstArenaVector<ConstItemPayload> consts;
    AstArenaVector<TypeAliasItemPayload> type_aliases;
    AstArenaVector<StructItemPayload> structs;
    AstArenaVector<EnumItemPayload> enums;
    AstArenaVector<OpaqueStructItemPayload> opaque_structs;
    AstArenaVector<FunctionItemPayload> functions;
    AstArenaVector<ExternBlockItemPayload> extern_blocks;
    AstArenaVector<ImplBlockItemPayload> impl_blocks;
    AstArenaVector<ItemNode> unknowns;
};

class ItemNodeList final {
public:
    ItemNodeList()
        : arena_(std::make_unique<base::BumpAllocator>()),
          headers_(base::BumpAllocatorAdapter<ItemNodeHeader> {*this->arena_}),
          payloads_(*this->arena_) {}

    ItemNodeList(const ItemNodeList& other)
        : ItemNodeList() {
        this->copy_from(other);
    }

    ItemNodeList& operator=(const ItemNodeList& other) {
        if (this == &other) {
            return *this;
        }
        ItemNodeList copy(other);
        *this = std::move(copy);
        return *this;
    }

    ItemNodeList(ItemNodeList&& other) noexcept
        : arena_(std::move(other.arena_)),
          headers_(std::move(other.headers_)),
          payloads_(std::move(other.payloads_)) {
        other.headers_ = AstArenaVector<ItemNodeHeader> {};
        other.payloads_ = ItemNodePayloadArena {};
        other.materialized_.clear();
        other.materialized_valid_.clear();
    }

    ItemNodeList& operator=(ItemNodeList&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        this->swap(other);
        return *this;
    }
    ~ItemNodeList() = default;

    [[nodiscard]] base::usize size() const noexcept {
        return this->headers_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return this->headers_.empty();
    }

    [[nodiscard]] ItemKind kind(const base::usize index) const noexcept {
        return static_cast<ItemKind>(this->headers_[index].kind);
    }

    [[nodiscard]] base::usize arena_bytes() const noexcept {
        return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
    }

    [[nodiscard]] base::usize arena_blocks() const noexcept {
        return this->arena_ == nullptr ? 0 : this->arena_->block_count();
    }

    [[nodiscard]] base::SourceRange range(const base::usize index) const noexcept {
        return this->headers_[index].range;
    }

    [[nodiscard]] Visibility visibility(const base::usize index) const noexcept {
        return static_cast<Visibility>(this->headers_[index].visibility);
    }

    template <typename T>
    [[nodiscard]] AstArenaVector<T> make_list() {
        return make_ast_arena_vector<T>(*this->arena_);
    }

    void reserve(const base::usize size) {
        this->reserve_headers(size);
        const base::usize primary = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_PRIMARY_PAYLOAD_DIVISOR);
        const base::usize secondary = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR);
        const base::usize rare = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR);
        this->payloads_.consts.reserve(secondary);
        this->payloads_.type_aliases.reserve(secondary);
        this->payloads_.structs.reserve(primary);
        this->payloads_.enums.reserve(primary);
        this->payloads_.opaque_structs.reserve(rare);
        this->payloads_.functions.reserve(primary);
        this->payloads_.extern_blocks.reserve(rare);
        this->payloads_.impl_blocks.reserve(secondary);
        this->payloads_.unknowns.reserve(rare);
    }

    void reserve_headers(const base::usize size) {
        this->headers_.reserve(size);
    }

    void push_back(ItemNode node) {
        static_cast<void>(this->append(std::move(node)));
    }

    [[nodiscard]] ItemId append(ItemNode node) {
        const ItemId id {static_cast<base::u32>(this->headers_.size())};
        ItemNodeHeader header;
        header.kind = pack_kind(node.kind);
        header.range = node.range;
        header.visibility = pack_visibility(node.visibility);
        header.flags = pack_flags(node);
        header.payload = this->store_payload(std::move(node));
        this->headers_.push_back(header);
        return id;
    }

    void set(const base::usize index, ItemNode node) {
        ItemNodeHeader header;
        header.kind = pack_kind(node.kind);
        header.range = node.range;
        header.visibility = pack_visibility(node.visibility);
        header.flags = pack_flags(node);
        header.payload = this->store_payload(std::move(node));
        this->headers_[index] = header;
        this->invalidate_materialized(index);
    }

    void set_range_begin(const base::usize index, const base::usize begin) {
        this->headers_[index].range.begin = begin;
        this->invalidate_materialized(index);
    }

    void set_visibility(const base::usize index, const Visibility visibility) {
        this->headers_[index].visibility = pack_visibility(visibility);
        this->invalidate_materialized(index);
    }

    [[nodiscard]] ItemNode take(const base::usize index) {
        this->invalidate_materialized(index);
        return this->load_moved(index);
    }

    [[nodiscard]] ItemNode operator[](const base::usize index) const {
        return this->load(index);
    }

    [[nodiscard]] const ItemNode* ptr(const base::usize index) const {
        if (index >= this->headers_.size()) {
            return nullptr;
        }
        return &this->materialized(index);
    }

private:
    static constexpr base::u8 ITEM_NODE_FLAG_EXPORT_C = 1U << 0U;
    static constexpr base::u8 ITEM_NODE_FLAG_EXTERN_C = 1U << 1U;
    static constexpr base::u8 ITEM_NODE_FLAG_UNSAFE = 1U << 2U;
    static constexpr base::u8 ITEM_NODE_FLAG_VARIADIC = 1U << 3U;
    static constexpr base::u8 ITEM_NODE_FLAG_PROTOTYPE = 1U << 4U;

    [[nodiscard]] static base::u8 pack_kind(const ItemKind kind) noexcept {
        return static_cast<base::u8>(kind);
    }

    [[nodiscard]] static base::u8 pack_visibility(const Visibility visibility) noexcept {
        return static_cast<base::u8>(visibility);
    }

    [[nodiscard]] static bool has_flag(const base::u8 flags, const base::u8 flag) noexcept {
        return (flags & flag) != 0;
    }

    [[nodiscard]] static base::u8 pack_flags(const ItemNode& node) noexcept {
        base::u8 flags = 0;
        if (node.is_export_c) {
            flags |= ITEM_NODE_FLAG_EXPORT_C;
        }
        if (node.is_extern_c) {
            flags |= ITEM_NODE_FLAG_EXTERN_C;
        }
        if (node.is_unsafe) {
            flags |= ITEM_NODE_FLAG_UNSAFE;
        }
        if (node.is_variadic) {
            flags |= ITEM_NODE_FLAG_VARIADIC;
        }
        if (node.is_prototype) {
            flags |= ITEM_NODE_FLAG_PROTOTYPE;
        }
        return flags;
    }

    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_list(const std::vector<T, Allocator>& values) {
        return copy_ast_arena_vector(*this->arena_, values);
    }

    template <typename T>
    [[nodiscard]] AstArenaVector<T> copy_or_move_list(AstArenaVector<T>&& values) {
        return move_or_copy_ast_arena_vector(*this->arena_, std::move(values));
    }

    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_or_move_list(std::vector<T, Allocator>&& values) {
        return this->copy_list(values);
    }

    [[nodiscard]] GenericConstraintDecl copy_generic_constraint(const GenericConstraintDecl& constraint) {
        GenericConstraintDecl copy;
        copy.param_name = constraint.param_name;
        copy.param_range = constraint.param_range;
        copy.capability_names = this->copy_list(constraint.capability_names);
        copy.capability_ranges = this->copy_list(constraint.capability_ranges);
        copy.range = constraint.range;
        copy.param_name_id = constraint.param_name_id;
        copy.capability_name_ids = this->copy_list(constraint.capability_name_ids);
        return copy;
    }

    [[nodiscard]] GenericConstraintDecl copy_or_move_generic_constraint(GenericConstraintDecl&& constraint) {
        GenericConstraintDecl copy;
        copy.param_name = constraint.param_name;
        copy.param_range = constraint.param_range;
        copy.capability_names = this->copy_or_move_list(std::move(constraint.capability_names));
        copy.capability_ranges = this->copy_or_move_list(std::move(constraint.capability_ranges));
        copy.range = constraint.range;
        copy.param_name_id = constraint.param_name_id;
        copy.capability_name_ids = this->copy_or_move_list(std::move(constraint.capability_name_ids));
        return copy;
    }

    template <typename Allocator>
    [[nodiscard]] AstArenaVector<GenericConstraintDecl> copy_generic_constraints(
        const std::vector<GenericConstraintDecl, Allocator>& constraints
    ) {
        AstArenaVector<GenericConstraintDecl> copy = make_ast_arena_vector<GenericConstraintDecl>(*this->arena_);
        copy.reserve(constraints.size());
        for (const GenericConstraintDecl& constraint : constraints) {
            copy.push_back(this->copy_generic_constraint(constraint));
        }
        return copy;
    }

    template <typename Allocator>
    [[nodiscard]] AstArenaVector<GenericConstraintDecl> copy_or_move_generic_constraints(
        std::vector<GenericConstraintDecl, Allocator>&& constraints
    ) {
        AstArenaVector<GenericConstraintDecl> copy = make_ast_arena_vector<GenericConstraintDecl>(*this->arena_);
        copy.reserve(constraints.size());
        for (GenericConstraintDecl& constraint : constraints) {
            copy.push_back(this->copy_or_move_generic_constraint(std::move(constraint)));
        }
        return copy;
    }

    [[nodiscard]] EnumCaseDecl copy_enum_case(const EnumCaseDecl& enum_case) {
        EnumCaseDecl copy;
        copy.name = enum_case.name;
        copy.payload_type = enum_case.payload_type;
        copy.payload_types = this->copy_list(enum_case.payload_types);
        copy.value_text = enum_case.value_text;
        copy.range = enum_case.range;
        copy.name_id = enum_case.name_id;
        return copy;
    }

    [[nodiscard]] EnumCaseDecl copy_or_move_enum_case(EnumCaseDecl&& enum_case) {
        EnumCaseDecl copy;
        copy.name = enum_case.name;
        copy.payload_type = enum_case.payload_type;
        copy.payload_types = this->copy_or_move_list(std::move(enum_case.payload_types));
        copy.value_text = enum_case.value_text;
        copy.range = enum_case.range;
        copy.name_id = enum_case.name_id;
        return copy;
    }

    template <typename Allocator>
    [[nodiscard]] AstArenaVector<EnumCaseDecl> copy_enum_cases(
        const std::vector<EnumCaseDecl, Allocator>& cases
    ) {
        AstArenaVector<EnumCaseDecl> copy = make_ast_arena_vector<EnumCaseDecl>(*this->arena_);
        copy.reserve(cases.size());
        for (const EnumCaseDecl& enum_case : cases) {
            copy.push_back(this->copy_enum_case(enum_case));
        }
        return copy;
    }

    template <typename Allocator>
    [[nodiscard]] AstArenaVector<EnumCaseDecl> copy_or_move_enum_cases(
        std::vector<EnumCaseDecl, Allocator>&& cases
    ) {
        AstArenaVector<EnumCaseDecl> copy = make_ast_arena_vector<EnumCaseDecl>(*this->arena_);
        copy.reserve(cases.size());
        for (EnumCaseDecl& enum_case : cases) {
            copy.push_back(this->copy_or_move_enum_case(std::move(enum_case)));
        }
        return copy;
    }

    [[nodiscard]] GenericConstraintDecl detach_generic_constraint(const GenericConstraintDecl& constraint) const {
        GenericConstraintDecl copy;
        copy.param_name = constraint.param_name;
        copy.param_range = constraint.param_range;
        copy.capability_names = copy_detached_ast_vector(constraint.capability_names);
        copy.capability_ranges = copy_detached_ast_vector(constraint.capability_ranges);
        copy.range = constraint.range;
        copy.param_name_id = constraint.param_name_id;
        copy.capability_name_ids = copy_detached_ast_vector(constraint.capability_name_ids);
        return copy;
    }

    template <typename Allocator>
    [[nodiscard]] std::vector<GenericConstraintDecl> detach_generic_constraints(
        const std::vector<GenericConstraintDecl, Allocator>& constraints
    ) const {
        std::vector<GenericConstraintDecl> copy;
        copy.reserve(constraints.size());
        for (const GenericConstraintDecl& constraint : constraints) {
            copy.push_back(this->detach_generic_constraint(constraint));
        }
        return copy;
    }

    [[nodiscard]] EnumCaseDecl detach_enum_case(const EnumCaseDecl& enum_case) const {
        EnumCaseDecl copy;
        copy.name = enum_case.name;
        copy.payload_type = enum_case.payload_type;
        copy.payload_types = copy_detached_ast_vector(enum_case.payload_types);
        copy.value_text = enum_case.value_text;
        copy.range = enum_case.range;
        copy.name_id = enum_case.name_id;
        return copy;
    }

    template <typename Allocator>
    [[nodiscard]] std::vector<EnumCaseDecl> detach_enum_cases(
        const std::vector<EnumCaseDecl, Allocator>& cases
    ) const {
        std::vector<EnumCaseDecl> copy;
        copy.reserve(cases.size());
        for (const EnumCaseDecl& enum_case : cases) {
            copy.push_back(this->detach_enum_case(enum_case));
        }
        return copy;
    }

    [[nodiscard]] base::u32 store_payload(ItemNode node) {
        switch (node.kind) {
        case ItemKind::const_decl:
            return this->push_payload(this->payloads_.consts, ConstItemPayload {
                node.name,
                node.name_id,
                node.const_type,
                node.const_value,
            });
        case ItemKind::type_alias:
            return this->push_payload(this->payloads_.type_aliases, TypeAliasItemPayload {
                node.name,
                node.name_id,
                this->copy_list(node.generic_params),
                this->copy_or_move_generic_constraints(std::move(node.where_constraints)),
                node.alias_type,
            });
        case ItemKind::struct_decl:
            return this->push_payload(this->payloads_.structs, StructItemPayload {
                node.name,
                node.name_id,
                this->copy_list(node.generic_params),
                this->copy_or_move_generic_constraints(std::move(node.where_constraints)),
                this->copy_list(node.fields),
            });
        case ItemKind::enum_decl:
            return this->push_payload(this->payloads_.enums, EnumItemPayload {
                node.name,
                node.name_id,
                this->copy_list(node.generic_params),
                this->copy_or_move_generic_constraints(std::move(node.where_constraints)),
                node.enum_base_type,
                this->copy_or_move_enum_cases(std::move(node.enum_cases)),
            });
        case ItemKind::opaque_struct_decl:
            return this->push_payload(this->payloads_.opaque_structs, OpaqueStructItemPayload {
                node.name,
                node.name_id,
            });
        case ItemKind::fn_decl:
            return this->push_payload(this->payloads_.functions, FunctionItemPayload {
                node.name,
                node.name_id,
                this->copy_list(node.generic_params),
                this->copy_or_move_generic_constraints(std::move(node.where_constraints)),
                this->copy_list(node.params),
                node.return_type,
                node.body,
                node.impl_type,
                node.abi_name,
            });
        case ItemKind::extern_block:
            return this->push_payload(this->payloads_.extern_blocks, ExternBlockItemPayload {
                this->copy_list(node.extern_items),
            });
        case ItemKind::impl_block:
            return this->push_payload(this->payloads_.impl_blocks, ImplBlockItemPayload {
                this->copy_list(node.generic_params),
                this->copy_or_move_generic_constraints(std::move(node.where_constraints)),
                node.impl_type,
                this->copy_list(node.impl_items),
            });
        }
        return this->push_payload(this->payloads_.unknowns, std::move(node));
    }

    void load_header(const ItemNodeHeader& header, ItemNode& node) const noexcept {
        node.kind = static_cast<ItemKind>(header.kind);
        node.range = header.range;
        node.visibility = static_cast<Visibility>(header.visibility);
        node.is_export_c = has_flag(header.flags, ITEM_NODE_FLAG_EXPORT_C);
        node.is_extern_c = has_flag(header.flags, ITEM_NODE_FLAG_EXTERN_C);
        node.is_unsafe = has_flag(header.flags, ITEM_NODE_FLAG_UNSAFE);
        node.is_variadic = has_flag(header.flags, ITEM_NODE_FLAG_VARIADIC);
        node.is_prototype = has_flag(header.flags, ITEM_NODE_FLAG_PROTOTYPE);
    }

    [[nodiscard]] ItemNode load(const base::usize index) const {
        const ItemNodeHeader& header = this->headers_[index];
        ItemNode node;
        this->load_header(header, node);
        switch (node.kind) {
        case ItemKind::const_decl: {
            const ConstItemPayload& payload = this->payloads_.consts[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.const_type = payload.type;
            node.const_value = payload.value;
            break;
        }
        case ItemKind::type_alias: {
            const TypeAliasItemPayload& payload = this->payloads_.type_aliases[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.alias_type = payload.target;
            break;
        }
        case ItemKind::struct_decl: {
            const StructItemPayload& payload = this->payloads_.structs[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.fields = copy_std_vector(payload.fields);
            break;
        }
        case ItemKind::enum_decl: {
            const EnumItemPayload& payload = this->payloads_.enums[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.enum_base_type = payload.base_type;
            node.enum_cases = this->detach_enum_cases(payload.cases);
            break;
        }
        case ItemKind::opaque_struct_decl:
            node.name = this->payloads_.opaque_structs[header.payload].name;
            node.name_id = this->payloads_.opaque_structs[header.payload].name_id;
            break;
        case ItemKind::fn_decl: {
            const FunctionItemPayload& payload = this->payloads_.functions[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.params = copy_std_vector(payload.params);
            node.return_type = payload.return_type;
            node.body = payload.body;
            node.impl_type = payload.impl_type;
            node.abi_name = payload.abi_name;
            break;
        }
        case ItemKind::extern_block:
            node.extern_items = copy_std_vector(this->payloads_.extern_blocks[header.payload].items);
            break;
        case ItemKind::impl_block: {
            const ImplBlockItemPayload& payload = this->payloads_.impl_blocks[header.payload];
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.impl_type = payload.impl_type;
            node.impl_items = copy_std_vector(payload.items);
            break;
        }
        default:
            node = this->payloads_.unknowns[header.payload];
            this->load_header(header, node);
            break;
        }
        return node;
    }

    [[nodiscard]] ItemNode load_moved(const base::usize index) {
        const ItemNodeHeader& header = this->headers_[index];
        ItemNode node;
        this->load_header(header, node);
        switch (node.kind) {
        case ItemKind::const_decl: {
            const ConstItemPayload& payload = this->payloads_.consts[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.const_type = payload.type;
            node.const_value = payload.value;
            break;
        }
        case ItemKind::type_alias: {
            TypeAliasItemPayload& payload = this->payloads_.type_aliases[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.alias_type = payload.target;
            break;
        }
        case ItemKind::struct_decl: {
            StructItemPayload& payload = this->payloads_.structs[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.fields = copy_std_vector(payload.fields);
            break;
        }
        case ItemKind::enum_decl: {
            EnumItemPayload& payload = this->payloads_.enums[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.enum_base_type = payload.base_type;
            node.enum_cases = this->detach_enum_cases(payload.cases);
            break;
        }
        case ItemKind::opaque_struct_decl:
            node.name = this->payloads_.opaque_structs[header.payload].name;
            node.name_id = this->payloads_.opaque_structs[header.payload].name_id;
            break;
        case ItemKind::fn_decl: {
            FunctionItemPayload& payload = this->payloads_.functions[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.params = copy_std_vector(payload.params);
            node.return_type = payload.return_type;
            node.body = payload.body;
            node.impl_type = payload.impl_type;
            node.abi_name = payload.abi_name;
            break;
        }
        case ItemKind::extern_block:
            node.extern_items = copy_std_vector(this->payloads_.extern_blocks[header.payload].items);
            break;
        case ItemKind::impl_block: {
            ImplBlockItemPayload& payload = this->payloads_.impl_blocks[header.payload];
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.impl_type = payload.impl_type;
            node.impl_items = copy_std_vector(payload.items);
            break;
        }
        default:
            node = std::move(this->payloads_.unknowns[header.payload]);
            this->load_header(header, node);
            break;
        }
        return node;
    }

    [[nodiscard]] const ItemNode& materialized(const base::usize index) const {
        this->ensure_materialized_capacity(index + 1);
        if (!this->materialized_valid_[index]) {
            this->materialized_[index] = this->load(index);
            this->materialized_valid_[index] = true;
        }
        return this->materialized_[index];
    }

    void ensure_materialized_capacity(const base::usize size) const {
        if (this->materialized_.size() < size) {
            this->materialized_.resize(size);
        }
        if (this->materialized_valid_.size() < size) {
            this->materialized_valid_.resize(size, false);
        }
    }

    void invalidate_materialized(const base::usize index) const
    {
        if (index < this->materialized_valid_.size()) {
            this->materialized_valid_[index] = false;
        }
    }

    template <typename T>
    [[nodiscard]] base::u32 push_payload(AstArenaVector<T>& payloads, T payload) {
        const base::u32 index = static_cast<base::u32>(payloads.size());
        payloads.push_back(std::move(payload));
        return index;
    }

    void copy_from(const ItemNodeList& other) {
        this->reserve(other.size());
        for (base::usize i = 0; i < other.size(); ++i) {
            static_cast<void>(this->append(other.load(i)));
        }
    }

    void swap(ItemNodeList& other) noexcept {
        using std::swap;
        swap(this->arena_, other.arena_);
        this->headers_.swap(other.headers_);
        this->payloads_.swap(other.payloads_);
        this->materialized_.swap(other.materialized_);
        this->materialized_valid_.swap(other.materialized_valid_);
    }

    std::unique_ptr<base::BumpAllocator> arena_;
    AstArenaVector<ItemNodeHeader> headers_;
    ItemNodePayloadArena payloads_;
    mutable std::deque<ItemNode> materialized_;
    mutable std::vector<bool> materialized_valid_;
};

struct ModulePath {
    std::vector<std::string_view> parts;
    base::SourceRange range {};
    std::vector<IdentId> part_ids;
};

struct ImportDecl {
    ModulePath path;
    std::string_view alias;
    base::SourceRange alias_range {};
    Visibility visibility = Visibility::private_;
    bool explicit_visibility = false;
    IdentId alias_id = INVALID_IDENT_ID;
};

struct ResolvedImport {
    ModuleId module = INVALID_MODULE_ID;
    std::string_view alias;
    base::SourceRange alias_range {};
    Visibility visibility = Visibility::private_;
    IdentId alias_id = INVALID_IDENT_ID;
};

struct ModuleInfo {
    ModulePath path;
    std::vector<ResolvedImport> imports;
};

struct AstModule {
    // The AST is intentionally stored as parallel vectors addressed by small
    // IDs. This keeps nodes compact, avoids virtual dispatch, and lets later
    // compiler stages attach side tables without changing syntax nodes.
    ModulePath module_path;
    std::vector<ImportDecl> imports;
    std::vector<ModuleInfo> modules;
    TypeNodeList types;
    ExprNodeList exprs;
    PatternNodeList patterns;
    StmtNodeList stmts;
    ItemNodeList items;
    std::vector<ModuleId> item_modules;
    IdentifierInterner identifiers;

    AstModule() {
        this->types.reserve_headers(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
        this->exprs.reserve_headers(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
        this->patterns.reserve_headers(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
        this->stmts.reserve_headers(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
        this->items.reserve_headers(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
    }

    void reserve_for_tokens(const base::usize token_count) {
        AstReserveEstimate estimate;
        estimate.tokens = token_count;
        this->reserve_for_estimate(estimate);
    }

    void reserve_for_estimate(const AstReserveEstimate& estimate) {
        constexpr base::usize INITIAL_CAPACITY = base::config::AUREX_INITIAL_AST_NODE_CAPACITY;
        const base::usize type_capacity = ast_reserve_larger(
            estimate.type_sites * SYNTAX_AST_RESERVE_TYPES_PER_TYPE_SITE,
            estimate.items * SYNTAX_AST_RESERVE_TYPES_PER_ITEM
        );
        const base::usize fallback_expr_capacity = ast_reserve_larger(
            estimate.statements * SYNTAX_AST_RESERVE_EXPRS_PER_STATEMENT,
            ast_reserve_fraction(estimate.tokens, SYNTAX_AST_RESERVE_EXPR_TOKEN_DIVISOR)
        );
        AstReserveEstimate::Exprs expr_capacity = estimate.exprs;
        if (expr_capacity.headers == 0) {
            expr_capacity = ast_expr_reserve_for_node_capacity(fallback_expr_capacity);
        }
        expr_capacity.headers = ast_reserve_at_least(INITIAL_CAPACITY, expr_capacity.headers);
        const base::usize pattern_capacity =
            estimate.pattern_sites * SYNTAX_AST_RESERVE_PATTERNS_PER_PATTERN_SITE;
        this->types.reserve(ast_reserve_at_least(
            INITIAL_CAPACITY,
            ast_reserve_larger(type_capacity, ast_reserve_fraction(
                estimate.tokens,
                SYNTAX_AST_RESERVE_TYPE_TOKEN_DIVISOR
            ))
        ));
        this->exprs.reserve_touched(expr_capacity);
        this->patterns.reserve(ast_reserve_at_least(
            INITIAL_CAPACITY,
            ast_reserve_larger(pattern_capacity, ast_reserve_fraction(
                estimate.tokens,
                SYNTAX_AST_RESERVE_PATTERN_TOKEN_DIVISOR
            ))
        ));
        this->stmts.reserve(ast_reserve_at_least(
            INITIAL_CAPACITY,
            ast_reserve_larger(estimate.statements, ast_reserve_fraction(
                estimate.tokens,
                SYNTAX_AST_RESERVE_STMT_TOKEN_DIVISOR
            ))
        ));
        const base::usize item_capacity = ast_reserve_at_least(
            INITIAL_CAPACITY,
            ast_reserve_larger(estimate.items, ast_reserve_fraction(
                estimate.tokens,
                SYNTAX_AST_RESERVE_ITEM_TOKEN_DIVISOR
            ))
        );
        this->items.reserve(item_capacity);
        this->item_modules.reserve(item_capacity);
        this->identifiers.reserve(ast_reserve_at_least(
            INITIAL_CAPACITY,
            ast_reserve_fraction(
                estimate.identifier_tokens,
                SYNTAX_AST_RESERVE_IDENTIFIER_TOKEN_DIVISOR
            )
        ));
    }

    AstModule(const AstModule& other)
        : module_path(other.module_path),
          imports(other.imports),
          modules(other.modules),
          types(other.types),
          exprs(other.exprs),
          patterns(other.patterns),
          stmts(other.stmts),
          items(other.items),
          item_modules(other.item_modules),
          identifiers(other.identifiers),
          identifiers_ready_(other.identifiers_ready_) {
        this->intern_identifiers();
    }

    AstModule& operator=(const AstModule& other) {
        if (this == &other) {
            return *this;
        }
        this->module_path = other.module_path;
        this->imports = other.imports;
        this->modules = other.modules;
        this->types = other.types;
        this->exprs = other.exprs;
        this->patterns = other.patterns;
        this->stmts = other.stmts;
        this->items = other.items;
        this->item_modules = other.item_modules;
        this->identifiers = other.identifiers;
        this->identifiers_ready_ = other.identifiers_ready_;
        this->intern_identifiers();
        return *this;
    }

    AstModule(AstModule&&) noexcept = default;
    AstModule& operator=(AstModule&&) noexcept = default;

    [[nodiscard]] TypeId push_type(TypeNode node) {
        this->intern_type_node(node);
        return this->types.append(std::move(node));
    }

    [[nodiscard]] ExprId push_invalid_expr(const base::SourceRange& range) {
        return this->exprs.append_invalid(range);
    }

    [[nodiscard]] ExprId push_literal_expr(
        const ExprKind kind,
        const base::SourceRange& range,
        const std::string_view text
    ) {
        return this->exprs.append_literal(kind, range, text);
    }

    [[nodiscard]] ExprId push_name_expr(const base::SourceRange& range, NameExprPayload payload) {
        return this->push_name_expr(
            range,
            payload.scope_name,
            payload.scope_range,
            payload.text,
            std::move(payload.type_args),
            payload.scope_name_id,
            payload.text_id
        );
    }

    template <typename TypeArgAllocator = std::allocator<TypeId>>
    [[nodiscard]] ExprId push_name_expr(
        const base::SourceRange& range,
        std::string_view scope_name,
        const base::SourceRange& scope_range,
        std::string_view text,
        std::vector<TypeId, TypeArgAllocator> type_args = std::vector<TypeId, TypeArgAllocator> {},
        IdentId scope_name_id = INVALID_IDENT_ID,
        IdentId text_id = INVALID_IDENT_ID
    ) {
        this->intern_identifier_text(scope_name, scope_name_id);
        this->intern_identifier_text(text, text_id);
        return this->exprs.append_name(
            range,
            scope_name,
            scope_range,
            text,
            scope_name_id,
            text_id,
            std::move(type_args)
        );
    }

    template <typename TypeArgAllocator = std::allocator<TypeId>>
    [[nodiscard]] ExprId push_name_expr(
        const base::SourceRange& range,
        const std::string_view text,
        std::vector<TypeId, TypeArgAllocator> type_args = std::vector<TypeId, TypeArgAllocator> {}
    ) {
        return this->push_name_expr(range, {}, {}, text, std::move(type_args));
    }

    [[nodiscard]] ExprId push_generic_apply_expr(const base::SourceRange& range, GenericApplyExprPayload payload) {
        return this->push_generic_apply_expr(range, payload.callee, std::move(payload.type_args));
    }

    template <typename TypeArgAllocator>
    [[nodiscard]] ExprId push_generic_apply_expr(
        const base::SourceRange& range,
        const ExprId callee,
        std::vector<TypeId, TypeArgAllocator> type_args
    ) {
        return this->exprs.append_generic_apply(range, callee, std::move(type_args));
    }

    [[nodiscard]] ExprId push_unary_expr(
        const ExprKind kind,
        const base::SourceRange& range,
        const UnaryExprPayload payload
    ) {
        return this->push_unary_expr(kind, range, payload.op, payload.operand);
    }

    [[nodiscard]] ExprId push_unary_expr(
        const ExprKind kind,
        const base::SourceRange& range,
        const UnaryOp op,
        const ExprId operand
    ) {
        return this->exprs.append_unary(kind, range, op, operand);
    }

    [[nodiscard]] ExprId push_binary_expr(const base::SourceRange& range, const BinaryExprPayload payload) {
        return this->push_binary_expr(range, payload.op, payload.lhs, payload.rhs);
    }

    [[nodiscard]] ExprId push_binary_expr(
        const base::SourceRange& range,
        const BinaryOp op,
        const ExprId lhs,
        const ExprId rhs
    ) {
        return this->exprs.append_binary(range, op, lhs, rhs);
    }

    [[nodiscard]] ExprId push_call_expr(
        const ExprKind kind,
        const base::SourceRange& range,
        CallExprPayload payload
    ) {
        return this->push_call_expr(kind, range, payload.callee, std::move(payload.args));
    }

    template <typename ArgAllocator>
    [[nodiscard]] ExprId push_call_expr(
        const ExprKind kind,
        const base::SourceRange& range,
        const ExprId callee,
        std::vector<ExprId, ArgAllocator> args
    ) {
        return this->exprs.append_call(kind, range, callee, std::move(args));
    }

    [[nodiscard]] ExprId push_if_expr(const base::SourceRange& range, const IfExprPayload payload) {
        return this->push_if_expr(
            range,
            payload.condition,
            payload.condition_pattern,
            payload.then_expr,
            payload.else_expr
        );
    }

    [[nodiscard]] ExprId push_if_expr(
        const base::SourceRange& range,
        const ExprId condition,
        const PatternId condition_pattern,
        const ExprId then_expr,
        const ExprId else_expr
    ) {
        return this->exprs.append_if(range, condition, condition_pattern, then_expr, else_expr);
    }

    [[nodiscard]] ExprId push_block_expr(
        const ExprKind kind,
        const base::SourceRange& range,
        const BlockExprPayload payload
    ) {
        return this->push_block_expr(kind, range, payload.block, payload.result);
    }

    [[nodiscard]] ExprId push_block_expr(
        const ExprKind kind,
        const base::SourceRange& range,
        const StmtId block,
        const ExprId result
    ) {
        return this->exprs.append_block(kind, range, block, result);
    }

    [[nodiscard]] ExprId push_match_expr(const base::SourceRange& range, MatchExprPayload payload) {
        return this->push_match_expr(range, payload.value, std::move(payload.arms));
    }

    template <typename ArmAllocator>
    [[nodiscard]] ExprId push_match_expr(
        const base::SourceRange& range,
        const ExprId value,
        std::vector<MatchArm, ArmAllocator> arms
    ) {
        return this->exprs.append_match(range, value, std::move(arms));
    }

    [[nodiscard]] ExprId push_array_expr(const base::SourceRange& range, ArrayExprPayload payload) {
        return this->push_array_expr(
            range,
            std::move(payload.elements),
            payload.repeat_value,
            payload.repeat_count
        );
    }

    template <typename ElementAllocator>
    [[nodiscard]] ExprId push_array_expr(
        const base::SourceRange& range,
        std::vector<ExprId, ElementAllocator> elements,
        const ExprId repeat_value = INVALID_EXPR_ID,
        const ExprId repeat_count = INVALID_EXPR_ID
    ) {
        return this->exprs.append_array(range, std::move(elements), repeat_value, repeat_count);
    }

    template <typename ElementAllocator>
    [[nodiscard]] ExprId push_tuple_expr(const base::SourceRange& range, std::vector<ExprId, ElementAllocator> elements) {
        return this->exprs.append_tuple(range, std::move(elements));
    }

    [[nodiscard]] ExprId push_postfix_chain_expr(const base::SourceRange& range, PostfixChainExprPayload payload) {
        return this->push_postfix_chain_expr(range, payload.base, std::move(payload.ops));
    }

    template <typename OpAllocator>
    [[nodiscard]] ExprId push_postfix_chain_expr(
        const base::SourceRange& range,
        const ExprId base,
        std::vector<PostfixOp, OpAllocator> ops
    ) {
        this->intern_postfix_ops(ops);
        return this->exprs.append_postfix_chain(range, base, std::move(ops));
    }

    [[nodiscard]] ExprId push_field_expr(const base::SourceRange& range, const FieldExprPayload& payload) {
        return this->push_field_expr(range, payload.object, payload.field_name, payload.field_name_id);
    }

    [[nodiscard]] ExprId push_field_expr(
        const base::SourceRange& range,
        const ExprId object,
        std::string_view field_name,
        IdentId field_name_id = INVALID_IDENT_ID
    ) {
        this->intern_identifier_text(field_name, field_name_id);
        return this->exprs.append_field(range, object, field_name, field_name_id);
    }

    [[nodiscard]] ExprId push_index_expr(const base::SourceRange& range, const IndexExprPayload payload) {
        return this->push_index_expr(range, payload.object, payload.index);
    }

    [[nodiscard]] ExprId push_index_expr(
        const base::SourceRange& range,
        const ExprId object,
        const ExprId index
    ) {
        return this->exprs.append_index(range, object, index);
    }

    [[nodiscard]] ExprId push_slice_expr(const base::SourceRange& range, const SliceExprPayload payload) {
        return this->push_slice_expr(range, payload.object, payload.start, payload.end);
    }

    [[nodiscard]] ExprId push_slice_expr(
        const base::SourceRange& range,
        const ExprId object,
        const ExprId start,
        const ExprId end
    ) {
        return this->exprs.append_slice(range, object, start, end);
    }

    [[nodiscard]] ExprId push_struct_literal_expr(const base::SourceRange& range, StructLiteralExprPayload payload) {
        return this->push_struct_literal_expr(
            range,
            payload.object,
            payload.scope_name,
            payload.scope_range,
            payload.name,
            std::move(payload.type_args),
            std::move(payload.field_inits),
            payload.scope_name_id,
            payload.name_id
        );
    }

    template <typename TypeArgAllocator, typename FieldInitAllocator>
    [[nodiscard]] ExprId push_struct_literal_expr(
        const base::SourceRange& range,
        const ExprId object,
        std::string_view scope_name,
        const base::SourceRange& scope_range,
        std::string_view name,
        std::vector<TypeId, TypeArgAllocator> type_args,
        std::vector<FieldInit, FieldInitAllocator> field_inits,
        IdentId scope_name_id = INVALID_IDENT_ID,
        IdentId name_id = INVALID_IDENT_ID
    ) {
        this->intern_identifier_text(scope_name, scope_name_id);
        this->intern_identifier_text(name, name_id);
        this->intern_field_inits(field_inits);
        return this->exprs.append_struct_literal(
            range,
            object,
            scope_name,
            scope_range,
            name,
            scope_name_id,
            name_id,
            std::move(type_args),
            std::move(field_inits)
        );
    }

    [[nodiscard]] ExprId push_cast_like_expr(
        const ExprKind kind,
        const base::SourceRange& range,
        const CastExprPayload payload
    ) {
        return this->push_cast_like_expr(kind, range, payload.type, payload.expr);
    }

    [[nodiscard]] ExprId push_cast_like_expr(
        const ExprKind kind,
        const base::SourceRange& range,
        const TypeId type,
        const ExprId expr
    ) {
        return this->exprs.append_cast_like(kind, range, type, expr);
    }

    [[nodiscard]] PatternId push_pattern(PatternNode node) {
        this->intern_pattern_node(node);
        return this->patterns.append(std::move(node));
    }

    [[nodiscard]] StmtId push_stmt(StmtNode node) {
        this->intern_stmt_node(node);
        return this->stmts.append(std::move(node));
    }

    [[nodiscard]] ItemId push_item(ItemNode node) {
        return this->push_item_for_module(std::move(node), INVALID_MODULE_ID);
    }

    [[nodiscard]] ItemId push_item_for_module(ItemNode node, const ModuleId module) {
        this->intern_item_node(node);
        const ItemId id = this->items.append(std::move(node));
        this->item_modules.push_back(module);
        return id;
    }

    void set_invalid_expr(const base::usize index, const base::SourceRange& range) {
        this->exprs.set_invalid(index, range);
    }

    void set_generic_apply_expr(const base::usize index, const base::SourceRange& range, GenericApplyExprPayload payload) {
        this->exprs.set_generic_apply(index, range, std::move(payload));
    }

    template <typename TypeArgAllocator>
    void set_generic_apply_expr(
        const base::usize index,
        const base::SourceRange& range,
        const ExprId callee,
        std::vector<TypeId, TypeArgAllocator> type_args
    ) {
        this->exprs.set_generic_apply(index, range, callee, std::move(type_args));
    }

    void set_unary_expr(
        const base::usize index,
        const ExprKind kind,
        const base::SourceRange& range,
        const UnaryExprPayload payload
    ) {
        this->exprs.set_unary(index, kind, range, payload);
    }

    void set_unary_expr(
        const base::usize index,
        const ExprKind kind,
        const base::SourceRange& range,
        const UnaryOp op,
        const ExprId operand
    ) {
        this->exprs.set_unary(index, kind, range, op, operand);
    }

    void set_call_expr(const base::usize index, const ExprKind kind, const base::SourceRange& range, CallExprPayload payload) {
        this->exprs.set_call(index, kind, range, std::move(payload));
    }

    template <typename ArgAllocator>
    void set_call_expr(
        const base::usize index,
        const ExprKind kind,
        const base::SourceRange& range,
        const ExprId callee,
        std::vector<ExprId, ArgAllocator> args
    ) {
        this->exprs.set_call(index, kind, range, callee, std::move(args));
    }

    void set_field_expr(const base::usize index, const base::SourceRange& range, FieldExprPayload payload) {
        this->intern_identifier_text(payload.field_name, payload.field_name_id);
        this->exprs.set_field(index, range, payload);
    }

    void set_field_expr(
        const base::usize index,
        const base::SourceRange& range,
        const ExprId object,
        std::string_view field_name,
        IdentId field_name_id = INVALID_IDENT_ID
    ) {
        this->intern_identifier_text(field_name, field_name_id);
        this->exprs.set_field(index, range, object, field_name, field_name_id);
    }

    void set_index_expr(const base::usize index, const base::SourceRange& range, const IndexExprPayload payload) {
        this->exprs.set_index(index, range, payload);
    }

    void set_index_expr(
        const base::usize index,
        const base::SourceRange& range,
        const ExprId object,
        const ExprId index_expr
    ) {
        this->exprs.set_index(index, range, object, index_expr);
    }

    void set_slice_expr(const base::usize index, const base::SourceRange& range, const SliceExprPayload payload) {
        this->exprs.set_slice(index, range, payload);
    }

    void set_slice_expr(
        const base::usize index,
        const base::SourceRange& range,
        const ExprId object,
        const ExprId start,
        const ExprId end
    ) {
        this->exprs.set_slice(index, range, object, start, end);
    }

    void set_struct_literal_expr(const base::usize index, const base::SourceRange& range, StructLiteralExprPayload payload) {
        this->intern_struct_literal_payload(payload);
        this->exprs.set_struct_literal(index, range, std::move(payload));
    }

    template <typename TypeArgAllocator, typename FieldInitAllocator>
    void set_struct_literal_expr(
        const base::usize index,
        const base::SourceRange& range,
        const ExprId object,
        std::string_view scope_name,
        const base::SourceRange& scope_range,
        std::string_view name,
        std::vector<TypeId, TypeArgAllocator> type_args,
        std::vector<FieldInit, FieldInitAllocator> field_inits,
        IdentId scope_name_id = INVALID_IDENT_ID,
        IdentId name_id = INVALID_IDENT_ID
    ) {
        this->intern_identifier_text(scope_name, scope_name_id);
        this->intern_identifier_text(name, name_id);
        this->intern_field_inits(field_inits);
        this->exprs.set_struct_literal(
            index,
            range,
            object,
            scope_name,
            scope_range,
            name,
            scope_name_id,
            name_id,
            std::move(type_args),
            std::move(field_inits)
        );
    }

    void set_item(const base::usize index, ItemNode node) {
        this->intern_item_node(node);
        this->items.set(index, std::move(node));
    }

    [[nodiscard]] IdentId intern_identifier(std::string_view text) {
        return this->identifiers.intern(text);
    }

    [[nodiscard]] IdentId find_identifier(std::string_view text) const noexcept {
        return this->identifiers.find(text);
    }

    [[nodiscard]] std::string_view identifier_text(const IdentId id) const noexcept {
        return this->identifiers.text(id);
    }

    template <typename T>
    [[nodiscard]] AstArenaVector<T> make_expr_list() {
        return this->exprs.make_list<T>();
    }

    [[nodiscard]] PostfixOp make_postfix_op() {
        return this->exprs.make_postfix_op();
    }

    template <typename T>
    [[nodiscard]] AstArenaVector<T> make_item_list() {
        return this->items.make_list<T>();
    }

    [[nodiscard]] bool identifiers_ready() const noexcept {
        return this->identifiers_ready_;
    }

    void finalize_identifiers() {
        this->intern_module_metadata();
        this->identifiers_ready_ = true;
    }

    void intern_identifiers() {
        this->intern_module_metadata();
        for (base::usize i = 0; i < this->types.size(); ++i) {
            TypeNode node = this->types.take(i);
            this->intern_type_node(node);
            this->types.set(i, std::move(node));
        }
        for (base::usize i = 0; i < this->exprs.size(); ++i) {
            this->intern_expr_payload(i);
        }
        for (base::usize i = 0; i < this->patterns.size(); ++i) {
            PatternNode node = this->patterns.take(i);
            this->intern_pattern_node(node);
            this->patterns.set(i, std::move(node));
        }
        for (base::usize i = 0; i < this->stmts.size(); ++i) {
            StmtNode node = this->stmts.take(i);
            this->intern_stmt_node(node);
            this->stmts.set(i, std::move(node));
        }
        for (base::usize i = 0; i < this->items.size(); ++i) {
            ItemNode node = this->items.take(i);
            this->intern_item_node(node);
            this->items.set(i, std::move(node));
        }
        this->identifiers_ready_ = true;
    }

    void intern_module_path(ModulePath& path) {
        this->intern_identifier_list(path.parts, path.part_ids);
    }

    void intern_import_decl(ImportDecl& import) {
        this->intern_module_path(import.path);
        this->intern_identifier_text(import.alias, import.alias_id);
    }

    void intern_resolved_import(ResolvedImport& import) {
        this->intern_identifier_text(import.alias, import.alias_id);
    }

private:
    void intern_identifier_text(std::string_view& text, IdentId& id) {
        id = this->identifiers.intern(text);
        text = this->identifiers.text(id);
    }

    template <typename TextAllocator, typename IdAllocator>
    void intern_identifier_list(
        std::vector<std::string_view, TextAllocator>& texts,
        std::vector<IdentId, IdAllocator>& ids
    ) {
        ids.resize(texts.size(), INVALID_IDENT_ID);
        for (base::usize i = 0; i < texts.size(); ++i) {
            this->intern_identifier_text(texts[i], ids[i]);
        }
    }

    template <typename Allocator>
    void intern_generic_params(std::vector<GenericParamDecl, Allocator>& params) {
        for (GenericParamDecl& param : params) {
            this->intern_identifier_text(param.name, param.name_id);
        }
    }

    template <typename Allocator>
    void intern_generic_constraints(std::vector<GenericConstraintDecl, Allocator>& constraints) {
        for (GenericConstraintDecl& constraint : constraints) {
            this->intern_identifier_text(constraint.param_name, constraint.param_name_id);
            this->intern_identifier_list(constraint.capability_names, constraint.capability_name_ids);
        }
    }

    void intern_type_node(TypeNode& node) {
        this->intern_identifier_text(node.scope_name, node.scope_name_id);
        this->intern_identifier_list(node.scope_parts, node.scope_part_ids);
        this->intern_identifier_text(node.name, node.name_id);
    }

    template <typename Allocator>
    void intern_field_inits(std::vector<FieldInit, Allocator>& inits) {
        for (FieldInit& init : inits) {
            this->intern_identifier_text(init.name, init.name_id);
        }
    }

    template <typename Allocator>
    void intern_postfix_ops(std::vector<PostfixOp, Allocator>& ops) {
        for (PostfixOp& op : ops) {
            this->intern_identifier_text(op.name, op.name_id);
            this->intern_field_inits(op.field_inits);
        }
    }

    void intern_name_expr_payload(NameExprPayload& payload) {
        this->intern_identifier_text(payload.scope_name, payload.scope_name_id);
        this->intern_identifier_text(payload.text, payload.text_id);
    }

    void intern_struct_literal_payload(StructLiteralExprPayload& payload) {
        this->intern_identifier_text(payload.scope_name, payload.scope_name_id);
        this->intern_identifier_text(payload.name, payload.name_id);
        this->intern_field_inits(payload.field_inits);
    }

    void intern_expr_payload(const base::usize index) {
        switch (this->exprs.kind(index)) {
        case ExprKind::name:
            if (NameExprPayload* const payload = this->exprs.name_payload(index); payload != nullptr) {
                this->intern_name_expr_payload(*payload);
            }
            break;
        case ExprKind::postfix_chain:
            if (PostfixChainExprPayload* const payload = this->exprs.postfix_chain_payload(index); payload != nullptr) {
                this->intern_postfix_ops(payload->ops);
            }
            break;
        case ExprKind::field:
            if (FieldExprPayload* const payload = this->exprs.field_payload(index); payload != nullptr) {
                this->intern_identifier_text(payload->field_name, payload->field_name_id);
            }
            break;
        case ExprKind::struct_literal:
            if (StructLiteralExprPayload* const payload = this->exprs.struct_literal_payload(index); payload != nullptr) {
                this->intern_struct_literal_payload(*payload);
            }
            break;
        default:
            break;
        }
    }

    template <typename Allocator>
    void intern_field_patterns(std::vector<FieldPattern, Allocator>& fields) {
        for (FieldPattern& field : fields) {
            this->intern_identifier_text(field.name, field.name_id);
        }
    }

    void intern_pattern_node(PatternNode& node) {
        this->intern_identifier_text(node.binding_name, node.binding_name_id);
        this->intern_identifier_text(node.enum_name, node.enum_name_id);
        if (node.kind == PatternKind::enum_case || node.kind == PatternKind::literal) {
            this->intern_identifier_text(node.case_name, node.case_name_id);
        }
        this->intern_identifier_text(node.struct_name, node.struct_name_id);
        this->intern_identifier_list(node.binding_names, node.binding_name_ids);
        this->intern_field_patterns(node.field_patterns);
    }

    void intern_stmt_node(StmtNode& node) {
        this->intern_identifier_text(node.name, node.name_id);
    }

    template <typename Allocator>
    void intern_param_decls(std::vector<ParamDecl, Allocator>& params) {
        for (ParamDecl& param : params) {
            this->intern_identifier_text(param.name, param.name_id);
        }
    }

    template <typename Allocator>
    void intern_field_decls(std::vector<FieldDecl, Allocator>& fields) {
        for (FieldDecl& field : fields) {
            this->intern_identifier_text(field.name, field.name_id);
        }
    }

    template <typename Allocator>
    void intern_enum_case_decls(std::vector<EnumCaseDecl, Allocator>& cases) {
        for (EnumCaseDecl& enum_case : cases) {
            this->intern_identifier_text(enum_case.name, enum_case.name_id);
        }
    }

    void intern_item_node(ItemNode& node) {
        this->intern_identifier_text(node.name, node.name_id);
        this->intern_generic_params(node.generic_params);
        this->intern_generic_constraints(node.where_constraints);
        this->intern_field_decls(node.fields);
        this->intern_enum_case_decls(node.enum_cases);
        this->intern_param_decls(node.params);
    }

    void intern_module_metadata() {
        this->intern_module_path(this->module_path);
        for (ImportDecl& import : this->imports) {
            this->intern_import_decl(import);
        }
        for (ModuleInfo& module : this->modules) {
            this->intern_module_path(module.path);
            for (ResolvedImport& import : module.imports) {
                this->intern_resolved_import(import);
            }
        }
    }

    bool identifiers_ready_ = false;
};

} // namespace aurex::syntax
