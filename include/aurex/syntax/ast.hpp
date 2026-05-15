#pragma once

#include <aurex/base/config.hpp>
#include <aurex/base/source.hpp>
#include <aurex/syntax/ast_ids.hpp>

#include <cstdint>
#include <deque>
#include <string_view>
#include <utility>
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
};

struct GenericConstraintDecl {
    std::string_view param_name;
    base::SourceRange param_range {};
    std::vector<std::string_view> capability_names;
    std::vector<base::SourceRange> capability_ranges;
    base::SourceRange range {};
};

struct TypeNode {
    TypeKind kind = TypeKind::named;
    base::SourceRange range {};
    PrimitiveTypeKind primitive = PrimitiveTypeKind::void_;
    std::string_view scope_name;
    base::SourceRange scope_range {};
    std::vector<std::string_view> scope_parts;
    std::string_view name;
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
};

struct PatternNode {
    PatternKind kind = PatternKind::wildcard;
    base::SourceRange range {};
    std::string_view binding_name;
    std::string_view enum_name;
    std::string_view case_name;
    TypeId enum_type = INVALID_TYPE_ID;
    std::string_view struct_name;
    std::vector<std::string_view> binding_names;
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
    std::vector<PostfixBracketArg> bracket_args;
    bool bracket_is_slice = false;
    ExprId slice_start = INVALID_EXPR_ID;
    ExprId slice_end = INVALID_EXPR_ID;
    std::vector<ExprId> args;
    std::vector<FieldInit> field_inits;
};

struct MatchArm {
    PatternId pattern = INVALID_PATTERN_ID;
    ExprId guard = INVALID_EXPR_ID;
    ExprId value = INVALID_EXPR_ID;
    base::SourceRange range {};
};

struct ExprNode {
    ExprKind kind = ExprKind::invalid;
    base::SourceRange range {};
    std::string_view scope_name;
    base::SourceRange scope_range {};
    std::string_view text;
    UnaryOp unary_op = UnaryOp::logical_not;
    ExprId unary_operand = INVALID_EXPR_ID;
    BinaryOp binary_op = BinaryOp::add;
    ExprId binary_lhs = INVALID_EXPR_ID;
    ExprId binary_rhs = INVALID_EXPR_ID;
    ExprId callee = INVALID_EXPR_ID;
    std::vector<ExprId> args;
    ExprId condition = INVALID_EXPR_ID;
    PatternId condition_pattern = INVALID_PATTERN_ID;
    ExprId then_expr = INVALID_EXPR_ID;
    ExprId else_expr = INVALID_EXPR_ID;
    StmtId block = INVALID_STMT_ID;
    ExprId block_result = INVALID_EXPR_ID;
    ExprId match_value = INVALID_EXPR_ID;
    std::vector<MatchArm> match_arms;
    std::vector<ExprId> array_elements;
    std::vector<ExprId> tuple_elements;
    ExprId array_repeat_value = INVALID_EXPR_ID;
    ExprId array_repeat_count = INVALID_EXPR_ID;
    ExprId postfix_base = INVALID_EXPR_ID;
    std::vector<PostfixOp> postfix_ops;
    ExprId object = INVALID_EXPR_ID;
    std::string_view field_name;
    ExprId index = INVALID_EXPR_ID;
    ExprId slice_start = INVALID_EXPR_ID;
    ExprId slice_end = INVALID_EXPR_ID;
    std::string_view struct_name;
    std::vector<TypeId> type_args;
    std::vector<FieldInit> field_inits;
    TypeId cast_type = INVALID_TYPE_ID;
    ExprId cast_expr = INVALID_EXPR_ID;
};

struct TypeNodeHeader {
    base::SourceRange range {};
    base::u32 payload = UINT32_MAX;
    TypeKind kind = TypeKind::named;
};

struct NamedTypePayload {
    std::string_view scope_name;
    base::SourceRange scope_range {};
    std::vector<std::string_view> scope_parts;
    std::string_view name;
    std::vector<TypeId> type_args;
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
    std::vector<TypeId> params;
    TypeId return_type = INVALID_TYPE_ID;
};

struct TypeNodePayloadArena {
    std::vector<PrimitiveTypeKind> primitives;
    std::vector<NamedTypePayload> named;
    std::vector<PointerTypePayload> pointers;
    std::vector<PointerTypePayload> references;
    std::vector<ArrayTypePayload> arrays;
    std::vector<SliceTypePayload> slices;
    std::vector<std::vector<TypeId>> tuples;
    std::vector<FunctionTypePayload> functions;
};

class TypeNodeList final {
public:
    [[nodiscard]] base::usize size() const noexcept {
        return this->headers_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return this->headers_.empty();
    }

    [[nodiscard]] TypeKind kind(const base::usize index) const noexcept {
        return this->headers_[index].kind;
    }

    [[nodiscard]] base::SourceRange range(const base::usize index) const noexcept {
        return this->headers_[index].range;
    }

    void reserve(const base::usize size) {
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
    [[nodiscard]] base::u32 store_payload(TypeNode node) {
        switch (node.kind) {
        case TypeKind::primitive:
            return this->push_payload(this->payloads_.primitives, node.primitive);
        case TypeKind::named:
            return this->push_payload(this->payloads_.named, NamedTypePayload {
                node.scope_name,
                node.scope_range,
                std::move(node.scope_parts),
                node.name,
                std::move(node.type_args),
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
            return this->push_payload(this->payloads_.tuples, std::move(node.tuple_elements));
        case TypeKind::function:
            return this->push_payload(this->payloads_.functions, FunctionTypePayload {
                node.function_call_conv,
                node.function_is_unsafe,
                node.function_is_variadic,
                std::move(node.function_params),
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
            node.scope_parts = payload.scope_parts;
            node.name = payload.name;
            node.type_args = payload.type_args;
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
            node.tuple_elements = this->payloads_.tuples[header.payload];
            break;
        case TypeKind::function: {
            const FunctionTypePayload& payload = this->payloads_.functions[header.payload];
            node.function_call_conv = payload.call_conv;
            node.function_is_unsafe = payload.is_unsafe;
            node.function_is_variadic = payload.is_variadic;
            node.function_params = payload.params;
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
            node.scope_parts = std::move(payload.scope_parts);
            node.name = payload.name;
            node.type_args = std::move(payload.type_args);
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
            node.tuple_elements = std::move(this->payloads_.tuples[header.payload]);
            break;
        case TypeKind::function: {
            FunctionTypePayload& payload = this->payloads_.functions[header.payload];
            node.function_call_conv = payload.call_conv;
            node.function_is_unsafe = payload.is_unsafe;
            node.function_is_variadic = payload.is_variadic;
            node.function_params = std::move(payload.params);
            node.function_return = payload.return_type;
            break;
        }
        }
        return node;
    }

    template <typename T>
    [[nodiscard]] base::u32 push_payload(std::vector<T>& payloads, T payload) {
        const base::u32 index = static_cast<base::u32>(payloads.size());
        payloads.push_back(std::move(payload));
        return index;
    }

    std::vector<TypeNodeHeader> headers_;
    TypeNodePayloadArena payloads_;
};

struct LiteralExprPayload {
    std::string_view text;
};

struct NameExprPayload {
    std::string_view scope_name;
    base::SourceRange scope_range {};
    std::string_view text;
    std::vector<TypeId> type_args;
};

struct GenericApplyExprPayload {
    ExprId callee = INVALID_EXPR_ID;
    std::vector<TypeId> type_args;
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
    std::vector<ExprId> args;
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
    std::vector<MatchArm> arms;
};

struct ArrayExprPayload {
    std::vector<ExprId> elements;
    ExprId repeat_value = INVALID_EXPR_ID;
    ExprId repeat_count = INVALID_EXPR_ID;
};

struct PostfixChainExprPayload {
    ExprId base = INVALID_EXPR_ID;
    std::vector<PostfixOp> ops;
};

struct FieldExprPayload {
    ExprId object = INVALID_EXPR_ID;
    std::string_view field_name;
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
    std::vector<TypeId> type_args;
    std::vector<FieldInit> field_inits;
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
    std::vector<LiteralExprPayload> literals;
    std::vector<NameExprPayload> names;
    std::vector<GenericApplyExprPayload> generic_applies;
    std::vector<UnaryExprPayload> unaries;
    std::vector<BinaryExprPayload> binaries;
    std::vector<CallExprPayload> calls;
    std::vector<IfExprPayload> ifs;
    std::vector<BlockExprPayload> blocks;
    std::vector<MatchExprPayload> matches;
    std::vector<ArrayExprPayload> arrays;
    std::vector<std::vector<ExprId>> tuples;
    std::vector<PostfixChainExprPayload> postfix_chains;
    std::vector<FieldExprPayload> fields;
    std::vector<IndexExprPayload> indexes;
    std::vector<SliceExprPayload> slices;
    std::vector<StructLiteralExprPayload> struct_literals;
    std::vector<CastExprPayload> casts;
};

class ExprNodeList final {
public:
    [[nodiscard]] base::usize size() const noexcept {
        return this->headers_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return this->headers_.empty();
    }

    [[nodiscard]] ExprKind kind(const base::usize index) const noexcept {
        return this->headers_[index].kind;
    }

    [[nodiscard]] base::SourceRange range(const base::usize index) const noexcept {
        return this->headers_[index].range;
    }

    void reserve(const base::usize size) {
        this->headers_.reserve(size);
    }

    void push_back(ExprNode node) {
        static_cast<void>(this->append(std::move(node)));
    }

    [[nodiscard]] ExprId append(ExprNode node) {
        const ExprId id {static_cast<base::u32>(this->headers_.size())};
        ExprNodeHeader header;
        header.kind = node.kind;
        header.range = node.range;
        header.payload = this->store_payload(std::move(node));
        this->headers_.push_back(header);
        return id;
    }

    void set(const base::usize index, ExprNode node) {
        ExprNodeHeader header;
        header.kind = node.kind;
        header.range = node.range;
        header.payload = this->store_payload(std::move(node));
        this->headers_[index] = header;
    }

    [[nodiscard]] ExprNode take(const base::usize index) {
        return this->load_moved(index);
    }

    [[nodiscard]] ExprNode operator[](const base::usize index) const {
        return this->load(index);
    }

private:
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

    [[nodiscard]] base::u32 store_payload(ExprNode node) {
        if (this->is_literal(node.kind)) {
            return this->push_payload(this->payloads_.literals, LiteralExprPayload {node.text});
        }
        if (this->is_cast_like(node.kind)) {
            return this->push_payload(this->payloads_.casts, CastExprPayload {node.cast_type, node.cast_expr});
        }
        switch (node.kind) {
        case ExprKind::name:
            return this->push_payload(this->payloads_.names, NameExprPayload {
                node.scope_name,
                node.scope_range,
                node.text,
                std::move(node.type_args),
            });
        case ExprKind::generic_apply:
            return this->push_payload(this->payloads_.generic_applies, GenericApplyExprPayload {
                node.callee,
                std::move(node.type_args),
            });
        case ExprKind::unary:
        case ExprKind::try_expr:
            return this->push_payload(this->payloads_.unaries, UnaryExprPayload {
                node.unary_op,
                node.unary_operand,
            });
        case ExprKind::binary:
            return this->push_payload(this->payloads_.binaries, BinaryExprPayload {
                node.binary_op,
                node.binary_lhs,
                node.binary_rhs,
            });
        case ExprKind::call:
        case ExprKind::str_from_bytes_unchecked:
            return this->push_payload(this->payloads_.calls, CallExprPayload {
                node.callee,
                std::move(node.args),
            });
        case ExprKind::if_expr:
            return this->push_payload(this->payloads_.ifs, IfExprPayload {
                node.condition,
                node.condition_pattern,
                node.then_expr,
                node.else_expr,
            });
        case ExprKind::block_expr:
        case ExprKind::unsafe_block:
            return this->push_payload(this->payloads_.blocks, BlockExprPayload {
                node.block,
                node.block_result,
            });
        case ExprKind::match_expr:
            return this->push_payload(this->payloads_.matches, MatchExprPayload {
                node.match_value,
                std::move(node.match_arms),
            });
        case ExprKind::array_literal:
            return this->push_payload(this->payloads_.arrays, ArrayExprPayload {
                std::move(node.array_elements),
                node.array_repeat_value,
                node.array_repeat_count,
            });
        case ExprKind::tuple_literal:
            return this->push_payload(this->payloads_.tuples, std::move(node.tuple_elements));
        case ExprKind::postfix_chain:
            return this->push_payload(this->payloads_.postfix_chains, PostfixChainExprPayload {
                node.postfix_base,
                std::move(node.postfix_ops),
            });
        case ExprKind::field:
            return this->push_payload(this->payloads_.fields, FieldExprPayload {
                node.object,
                node.field_name,
            });
        case ExprKind::index:
            return this->push_payload(this->payloads_.indexes, IndexExprPayload {
                node.object,
                node.index,
            });
        case ExprKind::slice:
            return this->push_payload(this->payloads_.slices, SliceExprPayload {
                node.object,
                node.slice_start,
                node.slice_end,
            });
        case ExprKind::struct_literal:
            return this->push_payload(this->payloads_.struct_literals, StructLiteralExprPayload {
                node.object,
                node.scope_name,
                node.scope_range,
                node.struct_name,
                std::move(node.type_args),
                std::move(node.field_inits),
            });
        case ExprKind::invalid:
        case ExprKind::bool_literal:
        case ExprKind::null_literal:
        case ExprKind::integer_literal:
        case ExprKind::float_literal:
        case ExprKind::string_literal:
        case ExprKind::c_string_literal:
        case ExprKind::raw_string_literal:
        case ExprKind::byte_string_literal:
        case ExprKind::byte_literal:
        case ExprKind::char_literal:
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
            break;
        }
        return UINT32_MAX;
    }

    [[nodiscard]] ExprNode load(const base::usize index) const {
        const ExprNodeHeader& header = this->headers_[index];
        ExprNode node;
        node.kind = header.kind;
        node.range = header.range;
        if (this->is_literal(header.kind)) {
            node.text = this->payloads_.literals[header.payload].text;
            return node;
        }
        if (this->is_cast_like(header.kind)) {
            const CastExprPayload& payload = this->payloads_.casts[header.payload];
            node.cast_type = payload.type;
            node.cast_expr = payload.expr;
            return node;
        }
        switch (header.kind) {
        case ExprKind::name: {
            const NameExprPayload& payload = this->payloads_.names[header.payload];
            node.scope_name = payload.scope_name;
            node.scope_range = payload.scope_range;
            node.text = payload.text;
            node.type_args = payload.type_args;
            break;
        }
        case ExprKind::generic_apply: {
            const GenericApplyExprPayload& payload = this->payloads_.generic_applies[header.payload];
            node.callee = payload.callee;
            node.type_args = payload.type_args;
            break;
        }
        case ExprKind::unary:
        case ExprKind::try_expr: {
            const UnaryExprPayload& payload = this->payloads_.unaries[header.payload];
            node.unary_op = payload.op;
            node.unary_operand = payload.operand;
            break;
        }
        case ExprKind::binary: {
            const BinaryExprPayload& payload = this->payloads_.binaries[header.payload];
            node.binary_op = payload.op;
            node.binary_lhs = payload.lhs;
            node.binary_rhs = payload.rhs;
            break;
        }
        case ExprKind::call:
        case ExprKind::str_from_bytes_unchecked: {
            const CallExprPayload& payload = this->payloads_.calls[header.payload];
            node.callee = payload.callee;
            node.args = payload.args;
            break;
        }
        case ExprKind::if_expr: {
            const IfExprPayload& payload = this->payloads_.ifs[header.payload];
            node.condition = payload.condition;
            node.condition_pattern = payload.condition_pattern;
            node.then_expr = payload.then_expr;
            node.else_expr = payload.else_expr;
            break;
        }
        case ExprKind::block_expr:
        case ExprKind::unsafe_block: {
            const BlockExprPayload& payload = this->payloads_.blocks[header.payload];
            node.block = payload.block;
            node.block_result = payload.result;
            break;
        }
        case ExprKind::match_expr: {
            const MatchExprPayload& payload = this->payloads_.matches[header.payload];
            node.match_value = payload.value;
            node.match_arms = payload.arms;
            break;
        }
        case ExprKind::array_literal: {
            const ArrayExprPayload& payload = this->payloads_.arrays[header.payload];
            node.array_elements = payload.elements;
            node.array_repeat_value = payload.repeat_value;
            node.array_repeat_count = payload.repeat_count;
            break;
        }
        case ExprKind::tuple_literal:
            node.tuple_elements = this->payloads_.tuples[header.payload];
            break;
        case ExprKind::postfix_chain: {
            const PostfixChainExprPayload& payload = this->payloads_.postfix_chains[header.payload];
            node.postfix_base = payload.base;
            node.postfix_ops = payload.ops;
            break;
        }
        case ExprKind::field: {
            const FieldExprPayload& payload = this->payloads_.fields[header.payload];
            node.object = payload.object;
            node.field_name = payload.field_name;
            break;
        }
        case ExprKind::index: {
            const IndexExprPayload& payload = this->payloads_.indexes[header.payload];
            node.object = payload.object;
            node.index = payload.index;
            break;
        }
        case ExprKind::slice: {
            const SliceExprPayload& payload = this->payloads_.slices[header.payload];
            node.object = payload.object;
            node.slice_start = payload.start;
            node.slice_end = payload.end;
            break;
        }
        case ExprKind::struct_literal: {
            const StructLiteralExprPayload& payload = this->payloads_.struct_literals[header.payload];
            node.object = payload.object;
            node.scope_name = payload.scope_name;
            node.scope_range = payload.scope_range;
            node.struct_name = payload.name;
            node.type_args = payload.type_args;
            node.field_inits = payload.field_inits;
            break;
        }
        default:
            break;
        }
        return node;
    }

    [[nodiscard]] ExprNode load_moved(const base::usize index) {
        const ExprNodeHeader& header = this->headers_[index];
        ExprNode node;
        node.kind = header.kind;
        node.range = header.range;
        if (this->is_literal(header.kind)) {
            node.text = this->payloads_.literals[header.payload].text;
            return node;
        }
        if (this->is_cast_like(header.kind)) {
            const CastExprPayload& payload = this->payloads_.casts[header.payload];
            node.cast_type = payload.type;
            node.cast_expr = payload.expr;
            return node;
        }
        switch (header.kind) {
        case ExprKind::name: {
            NameExprPayload& payload = this->payloads_.names[header.payload];
            node.scope_name = payload.scope_name;
            node.scope_range = payload.scope_range;
            node.text = payload.text;
            node.type_args = std::move(payload.type_args);
            break;
        }
        case ExprKind::generic_apply: {
            GenericApplyExprPayload& payload = this->payloads_.generic_applies[header.payload];
            node.callee = payload.callee;
            node.type_args = std::move(payload.type_args);
            break;
        }
        case ExprKind::unary:
        case ExprKind::try_expr: {
            const UnaryExprPayload& payload = this->payloads_.unaries[header.payload];
            node.unary_op = payload.op;
            node.unary_operand = payload.operand;
            break;
        }
        case ExprKind::binary: {
            const BinaryExprPayload& payload = this->payloads_.binaries[header.payload];
            node.binary_op = payload.op;
            node.binary_lhs = payload.lhs;
            node.binary_rhs = payload.rhs;
            break;
        }
        case ExprKind::call:
        case ExprKind::str_from_bytes_unchecked: {
            CallExprPayload& payload = this->payloads_.calls[header.payload];
            node.callee = payload.callee;
            node.args = std::move(payload.args);
            break;
        }
        case ExprKind::if_expr: {
            const IfExprPayload& payload = this->payloads_.ifs[header.payload];
            node.condition = payload.condition;
            node.condition_pattern = payload.condition_pattern;
            node.then_expr = payload.then_expr;
            node.else_expr = payload.else_expr;
            break;
        }
        case ExprKind::block_expr:
        case ExprKind::unsafe_block: {
            const BlockExprPayload& payload = this->payloads_.blocks[header.payload];
            node.block = payload.block;
            node.block_result = payload.result;
            break;
        }
        case ExprKind::match_expr: {
            MatchExprPayload& payload = this->payloads_.matches[header.payload];
            node.match_value = payload.value;
            node.match_arms = std::move(payload.arms);
            break;
        }
        case ExprKind::array_literal: {
            ArrayExprPayload& payload = this->payloads_.arrays[header.payload];
            node.array_elements = std::move(payload.elements);
            node.array_repeat_value = payload.repeat_value;
            node.array_repeat_count = payload.repeat_count;
            break;
        }
        case ExprKind::tuple_literal:
            node.tuple_elements = std::move(this->payloads_.tuples[header.payload]);
            break;
        case ExprKind::postfix_chain: {
            PostfixChainExprPayload& payload = this->payloads_.postfix_chains[header.payload];
            node.postfix_base = payload.base;
            node.postfix_ops = std::move(payload.ops);
            break;
        }
        case ExprKind::field: {
            const FieldExprPayload& payload = this->payloads_.fields[header.payload];
            node.object = payload.object;
            node.field_name = payload.field_name;
            break;
        }
        case ExprKind::index: {
            const IndexExprPayload& payload = this->payloads_.indexes[header.payload];
            node.object = payload.object;
            node.index = payload.index;
            break;
        }
        case ExprKind::slice: {
            const SliceExprPayload& payload = this->payloads_.slices[header.payload];
            node.object = payload.object;
            node.slice_start = payload.start;
            node.slice_end = payload.end;
            break;
        }
        case ExprKind::struct_literal: {
            StructLiteralExprPayload& payload = this->payloads_.struct_literals[header.payload];
            node.object = payload.object;
            node.scope_name = payload.scope_name;
            node.scope_range = payload.scope_range;
            node.struct_name = payload.name;
            node.type_args = std::move(payload.type_args);
            node.field_inits = std::move(payload.field_inits);
            break;
        }
        default:
            break;
        }
        return node;
    }

    template <typename T>
    [[nodiscard]] base::u32 push_payload(std::vector<T>& payloads, T payload) {
        const base::u32 index = static_cast<base::u32>(payloads.size());
        payloads.push_back(std::move(payload));
        return index;
    }

    std::vector<ExprNodeHeader> headers_;
    ExprNodePayloadArena payloads_;
};

struct PatternNodeHeader {
    base::SourceRange range {};
    base::u32 payload = UINT32_MAX;
    PatternKind kind = PatternKind::wildcard;
};

struct EnumCasePatternPayload {
    std::string_view enum_name;
    std::string_view case_name;
    TypeId enum_type = INVALID_TYPE_ID;
    std::vector<PatternId> payload_patterns;
    std::vector<std::string_view> binding_names;
    bool scoped = false;
};

struct LiteralPatternPayload {
    std::string_view case_name;
    std::vector<std::string_view> binding_names;
};

struct SlicePatternPayload {
    std::vector<PatternId> elements;
    base::usize rest_index = 0;
    bool has_rest = false;
};

struct StructPatternPayload {
    std::string_view name;
    std::vector<FieldPattern> fields;
};

struct PatternNodePayloadArena {
    std::vector<std::string_view> bindings;
    std::vector<LiteralPatternPayload> literals;
    std::vector<EnumCasePatternPayload> enum_cases;
    std::vector<std::vector<PatternId>> tuples;
    std::vector<SlicePatternPayload> slices;
    std::vector<StructPatternPayload> structs;
    std::vector<std::vector<PatternId>> alternatives;
};

class PatternNodeList final {
public:
    [[nodiscard]] base::usize size() const noexcept {
        return this->headers_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return this->headers_.empty();
    }

    [[nodiscard]] PatternKind kind(const base::usize index) const noexcept {
        return this->headers_[index].kind;
    }

    [[nodiscard]] base::SourceRange range(const base::usize index) const noexcept {
        return this->headers_[index].range;
    }

    void reserve(const base::usize size) {
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
    [[nodiscard]] base::u32 store_payload(PatternNode node) {
        switch (node.kind) {
        case PatternKind::binding:
        case PatternKind::const_:
            return this->push_payload(this->payloads_.bindings, node.binding_name);
        case PatternKind::literal:
            return this->push_payload(this->payloads_.literals, LiteralPatternPayload {
                node.case_name,
                std::move(node.binding_names),
            });
        case PatternKind::enum_case:
            return this->push_payload(this->payloads_.enum_cases, EnumCasePatternPayload {
                node.enum_name,
                node.case_name,
                node.enum_type,
                std::move(node.payload_patterns),
                std::move(node.binding_names),
                node.scoped,
            });
        case PatternKind::tuple:
            return this->push_payload(this->payloads_.tuples, std::move(node.elements));
        case PatternKind::slice:
            return this->push_payload(this->payloads_.slices, SlicePatternPayload {
                std::move(node.elements),
                node.slice_rest_index,
                node.has_slice_rest,
            });
        case PatternKind::struct_:
            return this->push_payload(this->payloads_.structs, StructPatternPayload {
                node.struct_name,
                std::move(node.field_patterns),
            });
        case PatternKind::or_pattern:
            return this->push_payload(this->payloads_.alternatives, std::move(node.alternatives));
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
            node.binding_name = this->payloads_.bindings[header.payload];
            break;
        case PatternKind::literal: {
            const LiteralPatternPayload& payload = this->payloads_.literals[header.payload];
            node.case_name = payload.case_name;
            node.binding_names = payload.binding_names;
            break;
        }
        case PatternKind::enum_case: {
            const EnumCasePatternPayload& payload = this->payloads_.enum_cases[header.payload];
            node.enum_name = payload.enum_name;
            node.case_name = payload.case_name;
            node.enum_type = payload.enum_type;
            node.payload_patterns = payload.payload_patterns;
            node.binding_names = payload.binding_names;
            node.scoped = payload.scoped;
            break;
        }
        case PatternKind::tuple:
            node.elements = this->payloads_.tuples[header.payload];
            break;
        case PatternKind::slice: {
            const SlicePatternPayload& payload = this->payloads_.slices[header.payload];
            node.elements = payload.elements;
            node.slice_rest_index = payload.rest_index;
            node.has_slice_rest = payload.has_rest;
            break;
        }
        case PatternKind::struct_: {
            const StructPatternPayload& payload = this->payloads_.structs[header.payload];
            node.struct_name = payload.name;
            node.field_patterns = payload.fields;
            break;
        }
        case PatternKind::or_pattern:
            node.alternatives = this->payloads_.alternatives[header.payload];
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
            node.binding_name = this->payloads_.bindings[header.payload];
            break;
        case PatternKind::literal: {
            LiteralPatternPayload& payload = this->payloads_.literals[header.payload];
            node.case_name = payload.case_name;
            node.binding_names = std::move(payload.binding_names);
            break;
        }
        case PatternKind::enum_case: {
            EnumCasePatternPayload& payload = this->payloads_.enum_cases[header.payload];
            node.enum_name = payload.enum_name;
            node.case_name = payload.case_name;
            node.enum_type = payload.enum_type;
            node.payload_patterns = std::move(payload.payload_patterns);
            node.binding_names = std::move(payload.binding_names);
            node.scoped = payload.scoped;
            break;
        }
        case PatternKind::tuple:
            node.elements = std::move(this->payloads_.tuples[header.payload]);
            break;
        case PatternKind::slice: {
            SlicePatternPayload& payload = this->payloads_.slices[header.payload];
            node.elements = std::move(payload.elements);
            node.slice_rest_index = payload.rest_index;
            node.has_slice_rest = payload.has_rest;
            break;
        }
        case PatternKind::struct_: {
            StructPatternPayload& payload = this->payloads_.structs[header.payload];
            node.struct_name = payload.name;
            node.field_patterns = std::move(payload.fields);
            break;
        }
        case PatternKind::or_pattern:
            node.alternatives = std::move(this->payloads_.alternatives[header.payload]);
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

    void invalidate_materialized(const base::usize index) {
        if (index < this->materialized_valid_.size()) {
            this->materialized_valid_[index] = false;
        }
    }

    template <typename T>
    [[nodiscard]] base::u32 push_payload(std::vector<T>& payloads, T payload) {
        const base::u32 index = static_cast<base::u32>(payloads.size());
        payloads.push_back(std::move(payload));
        return index;
    }

    std::vector<PatternNodeHeader> headers_;
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

struct ParamDecl {
    std::string_view name;
    TypeId type = INVALID_TYPE_ID;
    base::SourceRange range {};
};

struct FieldDecl {
    std::string_view name;
    TypeId type = INVALID_TYPE_ID;
    base::SourceRange range {};
    Visibility visibility = Visibility::private_;
};

struct EnumCaseDecl {
    std::string_view name;
    TypeId payload_type = INVALID_TYPE_ID;
    std::vector<TypeId> payload_types;
    std::string_view value_text;
    base::SourceRange range {};
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

struct ModulePath {
    std::vector<std::string_view> parts;
    base::SourceRange range {};
};

struct ImportDecl {
    ModulePath path;
    std::string_view alias;
    base::SourceRange alias_range {};
    Visibility visibility = Visibility::private_;
    bool explicit_visibility = false;
};

struct ResolvedImport {
    ModuleId module = INVALID_MODULE_ID;
    std::string_view alias;
    base::SourceRange alias_range {};
    Visibility visibility = Visibility::private_;
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
    std::vector<StmtNode> stmts;
    std::vector<ItemNode> items;
    std::vector<ModuleId> item_modules;

    AstModule() {
        types.reserve(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
        exprs.reserve(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
        patterns.reserve(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
        stmts.reserve(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
        items.reserve(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
    }

    [[nodiscard]] TypeId push_type(TypeNode node) {
        return types.append(std::move(node));
    }

    [[nodiscard]] ExprId push_expr(ExprNode node) {
        return exprs.append(std::move(node));
    }

    [[nodiscard]] PatternId push_pattern(PatternNode node) {
        return patterns.append(std::move(node));
    }

    [[nodiscard]] StmtId push_stmt(StmtNode node) {
        const StmtId id {static_cast<base::u32>(stmts.size())};
        stmts.push_back(std::move(node));
        return id;
    }

    [[nodiscard]] ItemId push_item(ItemNode node) {
        const ItemId id {static_cast<base::u32>(items.size())};
        items.push_back(std::move(node));
        item_modules.push_back(INVALID_MODULE_ID);
        return id;
    }
};

} // namespace aurex::syntax
