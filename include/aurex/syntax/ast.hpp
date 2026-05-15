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

    [[nodiscard]] const GenericApplyExprPayload* generic_apply_payload(const base::usize index) const noexcept {
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

    [[nodiscard]] const BinaryExprPayload* binary_payload(const base::usize index) const noexcept {
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

    [[nodiscard]] const IfExprPayload* if_payload(const base::usize index) const noexcept {
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

    [[nodiscard]] const MatchExprPayload* match_payload(const base::usize index) const noexcept {
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

    [[nodiscard]] const std::vector<ExprId>* tuple_elements(const base::usize index) const noexcept {
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

    [[nodiscard]] PostfixChainExprPayload take_postfix_chain_payload(const base::usize index) {
        if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::postfix_chain) {
            return {};
        }
        return std::move(this->payloads_.postfix_chains[this->headers_[index].payload]);
    }

    [[nodiscard]] const FieldExprPayload* field_payload(const base::usize index) const noexcept {
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

    [[nodiscard]] const SliceExprPayload* slice_payload(const base::usize index) const noexcept {
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

    [[nodiscard]] const CastExprPayload* cast_payload(const base::usize index) const noexcept {
        if (!this->payload_available(index) || !this->is_cast_like(this->headers_[index].kind)) {
            return nullptr;
        }
        return &this->payloads_.casts[this->headers_[index].payload];
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

    [[nodiscard]] bool retag_block_expr(
        const base::usize index,
        const ExprKind kind,
        const base::SourceRange range
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

    [[nodiscard]] ExprNode take(const base::usize index) {
        return this->load_moved(index);
    }

    [[nodiscard]] ExprNode operator[](const base::usize index) const {
        return this->load(index);
    }

private:
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

struct StmtNodeHeader {
    base::SourceRange range {};
    base::u32 payload = UINT32_MAX;
    base::u8 kind = static_cast<base::u8>(StmtKind::expr);
};

struct LocalStmtPayload {
    std::string_view name;
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
    std::vector<LocalStmtPayload> locals;
    std::vector<AssignStmtPayload> assigns;
    std::vector<IfStmtPayload> ifs;
    std::vector<ForStmtPayload> fors;
    std::vector<ForRangeStmtPayload> for_ranges;
    std::vector<WhileStmtPayload> whiles;
    std::vector<ExprStmtPayload> exprs;
    std::vector<ExprStmtPayload> defers;
    std::vector<ExprStmtPayload> returns;
    std::vector<std::vector<StmtId>> blocks;
    std::vector<StmtNode> unknowns;
};

class StmtNodeList final {
public:
    [[nodiscard]] base::usize size() const noexcept {
        return this->headers_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return this->headers_.empty();
    }

    [[nodiscard]] StmtKind kind(const base::usize index) const noexcept {
        return static_cast<StmtKind>(this->headers_[index].kind);
    }

    [[nodiscard]] base::SourceRange range(const base::usize index) const noexcept {
        return this->headers_[index].range;
    }

    [[nodiscard]] const std::vector<StmtId>* block_statements(const base::usize index) const noexcept {
        if (this->kind(index) != StmtKind::block) {
            return nullptr;
        }
        return &this->payloads_.blocks[this->headers_[index].payload];
    }

    void reserve(const base::usize size) {
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

    void set_range(const base::usize index, const base::SourceRange range) {
        this->headers_[index].range = range;
    }

    [[nodiscard]] StmtNode take(const base::usize index) {
        return this->load_moved(index);
    }

    [[nodiscard]] StmtNode operator[](const base::usize index) const {
        return this->load(index);
    }

private:
    [[nodiscard]] static base::u8 pack_kind(const StmtKind kind) noexcept {
        return static_cast<base::u8>(kind);
    }

    [[nodiscard]] base::u32 store_payload(StmtNode node) {
        switch (node.kind) {
        case StmtKind::let:
        case StmtKind::var:
            return this->push_payload(this->payloads_.locals, LocalStmtPayload {
                node.name,
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
            return this->push_payload(this->payloads_.blocks, std::move(node.statements));
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
            node.statements = this->payloads_.blocks[header.payload];
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
            node.statements = std::move(this->payloads_.blocks[header.payload]);
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
    [[nodiscard]] base::u32 push_payload(std::vector<T>& payloads, T payload) {
        const base::u32 index = static_cast<base::u32>(payloads.size());
        payloads.push_back(std::move(payload));
        return index;
    }

    std::vector<StmtNodeHeader> headers_;
    StmtNodePayloadArena payloads_;
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

struct ItemNodeHeader {
    base::SourceRange range {};
    base::u32 payload = UINT32_MAX;
    base::u8 kind = static_cast<base::u8>(ItemKind::fn_decl);
    base::u8 visibility = static_cast<base::u8>(Visibility::private_);
    base::u8 flags = 0;
};

struct ConstItemPayload {
    std::string_view name;
    TypeId type = INVALID_TYPE_ID;
    ExprId value = INVALID_EXPR_ID;
};

struct TypeAliasItemPayload {
    std::string_view name;
    std::vector<GenericParamDecl> generic_params;
    std::vector<GenericConstraintDecl> where_constraints;
    TypeId target = INVALID_TYPE_ID;
};

struct StructItemPayload {
    std::string_view name;
    std::vector<GenericParamDecl> generic_params;
    std::vector<GenericConstraintDecl> where_constraints;
    std::vector<FieldDecl> fields;
};

struct EnumItemPayload {
    std::string_view name;
    std::vector<GenericParamDecl> generic_params;
    std::vector<GenericConstraintDecl> where_constraints;
    TypeId base_type = INVALID_TYPE_ID;
    std::vector<EnumCaseDecl> cases;
};

struct OpaqueStructItemPayload {
    std::string_view name;
};

struct FunctionItemPayload {
    std::string_view name;
    std::vector<GenericParamDecl> generic_params;
    std::vector<GenericConstraintDecl> where_constraints;
    std::vector<ParamDecl> params;
    TypeId return_type = INVALID_TYPE_ID;
    StmtId body = INVALID_STMT_ID;
    TypeId impl_type = INVALID_TYPE_ID;
    std::string_view abi_name;
};

struct ExternBlockItemPayload {
    std::vector<ItemId> items;
};

struct ImplBlockItemPayload {
    std::vector<GenericParamDecl> generic_params;
    std::vector<GenericConstraintDecl> where_constraints;
    TypeId impl_type = INVALID_TYPE_ID;
    std::vector<ItemId> items;
};

struct ItemNodePayloadArena {
    std::vector<ConstItemPayload> consts;
    std::vector<TypeAliasItemPayload> type_aliases;
    std::vector<StructItemPayload> structs;
    std::vector<EnumItemPayload> enums;
    std::vector<OpaqueStructItemPayload> opaque_structs;
    std::vector<FunctionItemPayload> functions;
    std::vector<ExternBlockItemPayload> extern_blocks;
    std::vector<ImplBlockItemPayload> impl_blocks;
    std::vector<ItemNode> unknowns;
};

class ItemNodeList final {
public:
    [[nodiscard]] base::usize size() const noexcept {
        return this->headers_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return this->headers_.empty();
    }

    [[nodiscard]] ItemKind kind(const base::usize index) const noexcept {
        return static_cast<ItemKind>(this->headers_[index].kind);
    }

    [[nodiscard]] base::SourceRange range(const base::usize index) const noexcept {
        return this->headers_[index].range;
    }

    [[nodiscard]] Visibility visibility(const base::usize index) const noexcept {
        return static_cast<Visibility>(this->headers_[index].visibility);
    }

    void reserve(const base::usize size) {
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

    [[nodiscard]] base::u32 store_payload(ItemNode node) {
        switch (node.kind) {
        case ItemKind::const_decl:
            return this->push_payload(this->payloads_.consts, ConstItemPayload {
                node.name,
                node.const_type,
                node.const_value,
            });
        case ItemKind::type_alias:
            return this->push_payload(this->payloads_.type_aliases, TypeAliasItemPayload {
                node.name,
                std::move(node.generic_params),
                std::move(node.where_constraints),
                node.alias_type,
            });
        case ItemKind::struct_decl:
            return this->push_payload(this->payloads_.structs, StructItemPayload {
                node.name,
                std::move(node.generic_params),
                std::move(node.where_constraints),
                std::move(node.fields),
            });
        case ItemKind::enum_decl:
            return this->push_payload(this->payloads_.enums, EnumItemPayload {
                node.name,
                std::move(node.generic_params),
                std::move(node.where_constraints),
                node.enum_base_type,
                std::move(node.enum_cases),
            });
        case ItemKind::opaque_struct_decl:
            return this->push_payload(this->payloads_.opaque_structs, OpaqueStructItemPayload {
                node.name,
            });
        case ItemKind::fn_decl:
            return this->push_payload(this->payloads_.functions, FunctionItemPayload {
                node.name,
                std::move(node.generic_params),
                std::move(node.where_constraints),
                std::move(node.params),
                node.return_type,
                node.body,
                node.impl_type,
                node.abi_name,
            });
        case ItemKind::extern_block:
            return this->push_payload(this->payloads_.extern_blocks, ExternBlockItemPayload {
                std::move(node.extern_items),
            });
        case ItemKind::impl_block:
            return this->push_payload(this->payloads_.impl_blocks, ImplBlockItemPayload {
                std::move(node.generic_params),
                std::move(node.where_constraints),
                node.impl_type,
                std::move(node.impl_items),
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
            node.const_type = payload.type;
            node.const_value = payload.value;
            break;
        }
        case ItemKind::type_alias: {
            const TypeAliasItemPayload& payload = this->payloads_.type_aliases[header.payload];
            node.name = payload.name;
            node.generic_params = payload.generic_params;
            node.where_constraints = payload.where_constraints;
            node.alias_type = payload.target;
            break;
        }
        case ItemKind::struct_decl: {
            const StructItemPayload& payload = this->payloads_.structs[header.payload];
            node.name = payload.name;
            node.generic_params = payload.generic_params;
            node.where_constraints = payload.where_constraints;
            node.fields = payload.fields;
            break;
        }
        case ItemKind::enum_decl: {
            const EnumItemPayload& payload = this->payloads_.enums[header.payload];
            node.name = payload.name;
            node.generic_params = payload.generic_params;
            node.where_constraints = payload.where_constraints;
            node.enum_base_type = payload.base_type;
            node.enum_cases = payload.cases;
            break;
        }
        case ItemKind::opaque_struct_decl:
            node.name = this->payloads_.opaque_structs[header.payload].name;
            break;
        case ItemKind::fn_decl: {
            const FunctionItemPayload& payload = this->payloads_.functions[header.payload];
            node.name = payload.name;
            node.generic_params = payload.generic_params;
            node.where_constraints = payload.where_constraints;
            node.params = payload.params;
            node.return_type = payload.return_type;
            node.body = payload.body;
            node.impl_type = payload.impl_type;
            node.abi_name = payload.abi_name;
            break;
        }
        case ItemKind::extern_block:
            node.extern_items = this->payloads_.extern_blocks[header.payload].items;
            break;
        case ItemKind::impl_block: {
            const ImplBlockItemPayload& payload = this->payloads_.impl_blocks[header.payload];
            node.generic_params = payload.generic_params;
            node.where_constraints = payload.where_constraints;
            node.impl_type = payload.impl_type;
            node.impl_items = payload.items;
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
            node.const_type = payload.type;
            node.const_value = payload.value;
            break;
        }
        case ItemKind::type_alias: {
            TypeAliasItemPayload& payload = this->payloads_.type_aliases[header.payload];
            node.name = payload.name;
            node.generic_params = std::move(payload.generic_params);
            node.where_constraints = std::move(payload.where_constraints);
            node.alias_type = payload.target;
            break;
        }
        case ItemKind::struct_decl: {
            StructItemPayload& payload = this->payloads_.structs[header.payload];
            node.name = payload.name;
            node.generic_params = std::move(payload.generic_params);
            node.where_constraints = std::move(payload.where_constraints);
            node.fields = std::move(payload.fields);
            break;
        }
        case ItemKind::enum_decl: {
            EnumItemPayload& payload = this->payloads_.enums[header.payload];
            node.name = payload.name;
            node.generic_params = std::move(payload.generic_params);
            node.where_constraints = std::move(payload.where_constraints);
            node.enum_base_type = payload.base_type;
            node.enum_cases = std::move(payload.cases);
            break;
        }
        case ItemKind::opaque_struct_decl:
            node.name = this->payloads_.opaque_structs[header.payload].name;
            break;
        case ItemKind::fn_decl: {
            FunctionItemPayload& payload = this->payloads_.functions[header.payload];
            node.name = payload.name;
            node.generic_params = std::move(payload.generic_params);
            node.where_constraints = std::move(payload.where_constraints);
            node.params = std::move(payload.params);
            node.return_type = payload.return_type;
            node.body = payload.body;
            node.impl_type = payload.impl_type;
            node.abi_name = payload.abi_name;
            break;
        }
        case ItemKind::extern_block:
            node.extern_items = std::move(this->payloads_.extern_blocks[header.payload].items);
            break;
        case ItemKind::impl_block: {
            ImplBlockItemPayload& payload = this->payloads_.impl_blocks[header.payload];
            node.generic_params = std::move(payload.generic_params);
            node.where_constraints = std::move(payload.where_constraints);
            node.impl_type = payload.impl_type;
            node.impl_items = std::move(payload.items);
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

    std::vector<ItemNodeHeader> headers_;
    ItemNodePayloadArena payloads_;
    mutable std::deque<ItemNode> materialized_;
    mutable std::vector<bool> materialized_valid_;
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
    StmtNodeList stmts;
    ItemNodeList items;
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
        return stmts.append(std::move(node));
    }

    [[nodiscard]] ItemId push_item(ItemNode node) {
        const ItemId id = items.append(std::move(node));
        item_modules.push_back(INVALID_MODULE_ID);
        return id;
    }
};

} // namespace aurex::syntax
