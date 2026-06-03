#pragma once

#include <aurex/frontend/syntax/ast/nodes.hpp>

#include <cassert>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace aurex::syntax {

inline constexpr std::string_view SYNTAX_EXPR_PAYLOAD_ID_CONTEXT = "syntax expression payload id";
inline constexpr std::string_view SYNTAX_EXPR_NODE_ID_CONTEXT = "syntax expression node id";

struct LiteralExprPayload {
    std::string_view text;
};

struct NameExprPayload {
    std::string_view scope_name;
    base::SourceRange scope_range{};
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

struct TryExprPayload {
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
    base::SourceRange scope_range{};
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
    base::SourceRange range{};
    base::u32 payload = UINT32_MAX;
    ExprKind kind = ExprKind::invalid;
};

struct ExprNodePayloadArena {
    ExprNodePayloadArena() = default;

    explicit ExprNodePayloadArena(base::BumpAllocator& arena)
        : literals(base::BumpAllocatorAdapter<LiteralExprPayload>{arena}),
          names(base::BumpAllocatorAdapter<NameExprPayload>{arena}),
          generic_applies(base::BumpAllocatorAdapter<GenericApplyExprPayload>{arena}),
          unaries(base::BumpAllocatorAdapter<UnaryExprPayload>{arena}),
          tries(base::BumpAllocatorAdapter<TryExprPayload>{arena}),
          binaries(base::BumpAllocatorAdapter<BinaryExprPayload>{arena}),
          calls(base::BumpAllocatorAdapter<CallExprPayload>{arena}),
          ifs(base::BumpAllocatorAdapter<IfExprPayload>{arena}),
          blocks(base::BumpAllocatorAdapter<BlockExprPayload>{arena}),
          matches(base::BumpAllocatorAdapter<MatchExprPayload>{arena}),
          arrays(base::BumpAllocatorAdapter<ArrayExprPayload>{arena}),
          tuples(base::BumpAllocatorAdapter<AstArenaVector<ExprId>>{arena}),
          fields(base::BumpAllocatorAdapter<FieldExprPayload>{arena}),
          indexes(base::BumpAllocatorAdapter<IndexExprPayload>{arena}),
          slices(base::BumpAllocatorAdapter<SliceExprPayload>{arena}),
          struct_literals(base::BumpAllocatorAdapter<StructLiteralExprPayload>{arena}),
          casts(base::BumpAllocatorAdapter<CastExprPayload>{arena})
    {
    }

    void swap(ExprNodePayloadArena& other) noexcept
    {
        this->literals.swap(other.literals);
        this->names.swap(other.names);
        this->generic_applies.swap(other.generic_applies);
        this->unaries.swap(other.unaries);
        this->tries.swap(other.tries);
        this->binaries.swap(other.binaries);
        this->calls.swap(other.calls);
        this->ifs.swap(other.ifs);
        this->blocks.swap(other.blocks);
        this->matches.swap(other.matches);
        this->arrays.swap(other.arrays);
        this->tuples.swap(other.tuples);
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
    AstArenaVector<TryExprPayload> tries;
    AstArenaVector<BinaryExprPayload> binaries;
    AstArenaVector<CallExprPayload> calls;
    AstArenaVector<IfExprPayload> ifs;
    AstArenaVector<BlockExprPayload> blocks;
    AstArenaVector<MatchExprPayload> matches;
    AstArenaVector<ArrayExprPayload> arrays;
    AstArenaVector<AstArenaVector<ExprId>> tuples;
    AstArenaVector<FieldExprPayload> fields;
    AstArenaVector<IndexExprPayload> indexes;
    AstArenaVector<SliceExprPayload> slices;
    AstArenaVector<StructLiteralExprPayload> struct_literals;
    AstArenaVector<CastExprPayload> casts;
};

class ExprNodeList final {
public:
    ExprNodeList();
    ExprNodeList(const ExprNodeList& other);
    ExprNodeList& operator=(const ExprNodeList& other);
    ExprNodeList(ExprNodeList&& other) noexcept;
    ExprNodeList& operator=(ExprNodeList&& other) noexcept;
    ~ExprNodeList();

    [[nodiscard]] base::usize size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] ExprKind kind(base::usize index) const noexcept;
    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_used_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;
    [[nodiscard]] base::SourceRange range(base::usize index) const noexcept;

    template <typename T>
    [[nodiscard]] AstArenaVector<T> make_list()
    {
        return make_ast_arena_vector<T>(*this->arena_);
    }

    [[nodiscard]] const LiteralExprPayload* literal_payload(base::usize index) const noexcept;
    [[nodiscard]] const NameExprPayload* name_payload(base::usize index) const noexcept;
    [[nodiscard]] NameExprPayload* name_payload(base::usize index) noexcept;
    [[nodiscard]] const GenericApplyExprPayload* generic_apply_payload(base::usize index) const noexcept;
    [[nodiscard]] GenericApplyExprPayload* generic_apply_payload(base::usize index) noexcept;
    [[nodiscard]] const UnaryExprPayload* unary_payload(base::usize index) const noexcept;
    [[nodiscard]] UnaryExprPayload* unary_payload(base::usize index) noexcept;
    [[nodiscard]] const TryExprPayload* try_payload(base::usize index) const noexcept;
    [[nodiscard]] TryExprPayload* try_payload(base::usize index) noexcept;
    [[nodiscard]] const BinaryExprPayload* binary_payload(base::usize index) const noexcept;
    [[nodiscard]] BinaryExprPayload* binary_payload(base::usize index) noexcept;
    [[nodiscard]] const CallExprPayload* call_payload(base::usize index) const noexcept;
    [[nodiscard]] CallExprPayload* call_payload(base::usize index) noexcept;
    [[nodiscard]] const IfExprPayload* if_payload(base::usize index) const noexcept;
    [[nodiscard]] IfExprPayload* if_payload(base::usize index) noexcept;
    [[nodiscard]] const BlockExprPayload* block_payload(base::usize index) const noexcept;
    [[nodiscard]] BlockExprPayload* block_payload(base::usize index) noexcept;
    [[nodiscard]] const MatchExprPayload* match_payload(base::usize index) const noexcept;
    [[nodiscard]] MatchExprPayload* match_payload(base::usize index) noexcept;
    [[nodiscard]] const ArrayExprPayload* array_payload(base::usize index) const noexcept;
    [[nodiscard]] ArrayExprPayload* array_payload(base::usize index) noexcept;
    [[nodiscard]] const AstArenaVector<ExprId>* tuple_elements(base::usize index) const noexcept;
    [[nodiscard]] AstArenaVector<ExprId>* tuple_elements(base::usize index) noexcept;
    [[nodiscard]] const FieldExprPayload* field_payload(base::usize index) const noexcept;
    [[nodiscard]] FieldExprPayload* field_payload(base::usize index) noexcept;
    [[nodiscard]] const IndexExprPayload* index_payload(base::usize index) const noexcept;
    [[nodiscard]] IndexExprPayload* index_payload(base::usize index) noexcept;
    [[nodiscard]] const SliceExprPayload* slice_payload(base::usize index) const noexcept;
    [[nodiscard]] SliceExprPayload* slice_payload(base::usize index) noexcept;
    [[nodiscard]] const StructLiteralExprPayload* struct_literal_payload(base::usize index) const noexcept;
    [[nodiscard]] StructLiteralExprPayload* struct_literal_payload(base::usize index) noexcept;
    [[nodiscard]] const CastExprPayload* cast_payload(base::usize index) const noexcept;
    [[nodiscard]] CastExprPayload* cast_payload(base::usize index) noexcept;

    void reserve(base::usize size);
    void reserve_touched(base::usize size);
    void reserve_touched(const AstReserveEstimate::Exprs& plan);
    void reserve_headers(base::usize size);
    [[nodiscard]] ExprId append_invalid(const base::SourceRange& range);
    [[nodiscard]] ExprId append_literal(ExprKind kind, const base::SourceRange& range, std::string_view text);
    [[nodiscard]] ExprId append_name(const base::SourceRange& range, NameExprPayload payload);

    template <typename TypeArgAllocator>
    [[nodiscard]] ExprId append_name(const base::SourceRange& range, const std::string_view scope_name,
        const base::SourceRange& scope_range, const std::string_view text, const IdentId scope_name_id,
        const IdentId text_id, std::vector<TypeId, TypeArgAllocator> type_args)
    {
        return this->append_header(ExprKind::name, range,
            this->emplace_payload(this->payloads_.names, scope_name, scope_range, text, scope_name_id, text_id,
                this->copy_or_move_list(std::move(type_args))));
    }

    [[nodiscard]] ExprId append_generic_apply(const base::SourceRange& range, GenericApplyExprPayload payload)
    {
        return this->append_generic_apply(range, payload.callee, std::move(payload.type_args));
    }

    template <typename TypeArgAllocator>
    [[nodiscard]] ExprId append_generic_apply(
        const base::SourceRange& range, const ExprId callee, std::vector<TypeId, TypeArgAllocator> type_args)
    {
        return this->append_header(ExprKind::generic_apply, range,
            this->emplace_payload(
                this->payloads_.generic_applies, callee, this->copy_or_move_list(std::move(type_args))));
    }

    [[nodiscard]] ExprId append_unary(ExprKind kind, const base::SourceRange& range, UnaryExprPayload payload);
    [[nodiscard]] ExprId append_unary(ExprKind kind, const base::SourceRange& range, UnaryOp op, ExprId operand);
    [[nodiscard]] ExprId append_try(const base::SourceRange& range, TryExprPayload payload);
    [[nodiscard]] ExprId append_try(const base::SourceRange& range, ExprId operand);
    [[nodiscard]] ExprId append_binary(const base::SourceRange& range, BinaryExprPayload payload);
    [[nodiscard]] ExprId append_binary(const base::SourceRange& range, BinaryOp op, ExprId lhs, ExprId rhs);
    [[nodiscard]] ExprId append_call(ExprKind kind, const base::SourceRange& range, CallExprPayload payload);

    template <typename ArgAllocator>
    [[nodiscard]] ExprId append_call(const ExprKind kind, const base::SourceRange& range, const ExprId callee,
        std::vector<ExprId, ArgAllocator> args)
    {
        return this->append_header(kind, range,
            this->emplace_payload(this->payloads_.calls, callee, this->copy_or_move_list(std::move(args))));
    }

    [[nodiscard]] ExprId append_if(const base::SourceRange& range, IfExprPayload payload);
    [[nodiscard]] ExprId append_if(const base::SourceRange& range, ExprId condition, PatternId condition_pattern,
        ExprId then_expr, ExprId else_expr);
    [[nodiscard]] ExprId append_block(ExprKind kind, const base::SourceRange& range, BlockExprPayload payload);
    [[nodiscard]] ExprId append_block(ExprKind kind, const base::SourceRange& range, StmtId block, ExprId result);
    [[nodiscard]] ExprId append_match(const base::SourceRange& range, MatchExprPayload payload);

    template <typename ArmAllocator>
    [[nodiscard]] ExprId append_match(
        const base::SourceRange& range, const ExprId value, std::vector<MatchArm, ArmAllocator> arms)
    {
        return this->append_header(ExprKind::match_expr, range,
            this->emplace_payload(this->payloads_.matches, value, this->copy_or_move_list(std::move(arms))));
    }

    [[nodiscard]] ExprId append_array(const base::SourceRange& range, ArrayExprPayload payload);

    template <typename ElementAllocator>
    [[nodiscard]] ExprId append_array(const base::SourceRange& range, std::vector<ExprId, ElementAllocator> elements,
        const ExprId repeat_value, const ExprId repeat_count)
    {
        return this->append_header(ExprKind::array_literal, range,
            this->emplace_payload(
                this->payloads_.arrays, this->copy_or_move_list(std::move(elements)), repeat_value, repeat_count));
    }

    template <typename ElementAllocator>
    [[nodiscard]] ExprId append_tuple(const base::SourceRange& range, std::vector<ExprId, ElementAllocator> elements)
    {
        return this->append_header(ExprKind::tuple_literal, range,
            this->emplace_payload(this->payloads_.tuples, this->copy_or_move_list(std::move(elements))));
    }

    [[nodiscard]] ExprId append_field(const base::SourceRange& range, const FieldExprPayload& payload);
    [[nodiscard]] ExprId append_field(
        const base::SourceRange& range, ExprId object, std::string_view field_name, IdentId field_name_id);
    [[nodiscard]] ExprId append_index(const base::SourceRange& range, IndexExprPayload payload);
    [[nodiscard]] ExprId append_index(const base::SourceRange& range, ExprId object, ExprId index);
    [[nodiscard]] ExprId append_slice(const base::SourceRange& range, SliceExprPayload payload);
    [[nodiscard]] ExprId append_slice(const base::SourceRange& range, ExprId object, ExprId start, ExprId end);
    [[nodiscard]] ExprId append_struct_literal(const base::SourceRange& range, StructLiteralExprPayload payload);

    template <typename TypeArgAllocator, typename FieldInitAllocator>
    [[nodiscard]] ExprId append_struct_literal(const base::SourceRange& range, const ExprId object,
        const std::string_view scope_name, const base::SourceRange& scope_range, const std::string_view name,
        const IdentId scope_name_id, const IdentId name_id, std::vector<TypeId, TypeArgAllocator> type_args,
        std::vector<FieldInit, FieldInitAllocator> field_inits)
    {
        return this->append_header(ExprKind::struct_literal, range,
            this->emplace_payload(this->payloads_.struct_literals, object, scope_name, scope_range, name, scope_name_id,
                name_id, this->copy_or_move_list(std::move(type_args)),
                this->copy_or_move_list(std::move(field_inits))));
    }

    [[nodiscard]] ExprId append_cast_like(ExprKind kind, const base::SourceRange& range, CastExprPayload payload);
    [[nodiscard]] ExprId append_cast_like(ExprKind kind, const base::SourceRange& range, TypeId type, ExprId expr);

    void set_invalid(base::usize index, const base::SourceRange& range);
    void set_generic_apply(base::usize index, const base::SourceRange& range, GenericApplyExprPayload payload);

    template <typename TypeArgAllocator>
    void set_generic_apply(const base::usize index, const base::SourceRange& range, const ExprId callee,
        std::vector<TypeId, TypeArgAllocator> type_args)
    {
        this->set_header(index, ExprKind::generic_apply, range,
            this->emplace_payload(
                this->payloads_.generic_applies, callee, this->copy_or_move_list(std::move(type_args))));
    }

    void set_unary(base::usize index, ExprKind kind, const base::SourceRange& range, UnaryExprPayload payload);
    void set_unary(base::usize index, ExprKind kind, const base::SourceRange& range, UnaryOp op, ExprId operand);
    void set_try(base::usize index, const base::SourceRange& range, TryExprPayload payload);
    void set_try(base::usize index, const base::SourceRange& range, ExprId operand);
    void set_call(base::usize index, ExprKind kind, const base::SourceRange& range, CallExprPayload payload);

    template <typename ArgAllocator>
    void set_call(const base::usize index, const ExprKind kind, const base::SourceRange& range, const ExprId callee,
        std::vector<ExprId, ArgAllocator> args)
    {
        this->set_header(index, kind, range,
            this->emplace_payload(this->payloads_.calls, callee, this->copy_or_move_list(std::move(args))));
    }

    void set_field(base::usize index, const base::SourceRange& range, const FieldExprPayload& payload);
    void set_field(base::usize index, const base::SourceRange& range, ExprId object, std::string_view field_name,
        IdentId field_name_id);
    void set_index(base::usize index, const base::SourceRange& range, IndexExprPayload payload);
    void set_index(base::usize index, const base::SourceRange& range, ExprId object, ExprId index_expr);
    void set_slice(base::usize index, const base::SourceRange& range, SliceExprPayload payload);
    void set_slice(base::usize index, const base::SourceRange& range, ExprId object, ExprId start, ExprId end);
    void set_struct_literal(base::usize index, const base::SourceRange& range, StructLiteralExprPayload payload);

    template <typename TypeArgAllocator, typename FieldInitAllocator>
    void set_struct_literal(const base::usize index, const base::SourceRange& range, const ExprId object,
        const std::string_view scope_name, const base::SourceRange& scope_range, const std::string_view name,
        const IdentId scope_name_id, const IdentId name_id, std::vector<TypeId, TypeArgAllocator> type_args,
        std::vector<FieldInit, FieldInitAllocator> field_inits)
    {
        this->set_header(index, ExprKind::struct_literal, range,
            this->emplace_payload(this->payloads_.struct_literals, object, scope_name, scope_range, name, scope_name_id,
                name_id, this->copy_or_move_list(std::move(type_args)),
                this->copy_or_move_list(std::move(field_inits))));
    }

    [[nodiscard]] bool retag_block_expr(base::usize index, ExprKind kind, const base::SourceRange& range) noexcept;

private:
    [[nodiscard]] ExprId append_header(ExprKind kind, const base::SourceRange& range, base::u32 payload);
    void set_header(base::usize index, ExprKind kind, const base::SourceRange& range, base::u32 payload);
    [[nodiscard]] bool payload_available(base::usize index) const noexcept;
    [[nodiscard]] bool is_literal(ExprKind kind) const noexcept;
    [[nodiscard]] bool is_cast_like(ExprKind kind) const noexcept;
    [[nodiscard]] bool is_block_payload_kind(ExprKind kind) const noexcept;
    [[nodiscard]] static base::usize allocation_bytes(base::usize count, base::usize element_size);
    [[nodiscard]] static base::usize estimated_arena_bytes(const AstReserveEstimate::Exprs& plan);
    void reserve_payloads(const AstReserveEstimate::Exprs& plan);

    template <typename T>
    [[nodiscard]] base::u32 push_payload(AstArenaVector<T>& payloads, T payload)
    {
        return this->emplace_payload(payloads, std::move(payload));
    }

    template <typename T, typename... Args>
    [[nodiscard]] base::u32 emplace_payload(AstArenaVector<T>& payloads, Args&&... args)
    {
        const base::u32 index = base::checked_u32(payloads.size(), SYNTAX_EXPR_PAYLOAD_ID_CONTEXT);
        payloads.emplace_back(std::forward<Args>(args)...);
        return index;
    }

    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_list(const std::vector<T, Allocator>& values)
    {
        return copy_ast_arena_vector(*this->arena_, values);
    }

    template <typename T>
    [[nodiscard]] AstArenaVector<T> copy_or_move_list(AstArenaVector<T>&& values)
    {
        return move_or_copy_ast_arena_vector(*this->arena_, std::move(values));
    }

    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_or_move_list(std::vector<T, Allocator>&& values)
    {
        return this->copy_list(values);
    }

    void copy_append_from(const ExprNodeList& other, base::usize index);
    void copy_from(const ExprNodeList& other);
    void swap(ExprNodeList& other) noexcept;

    std::unique_ptr<base::BumpAllocator> arena_;
    AstArenaVector<ExprNodeHeader> headers_;
    ExprNodePayloadArena payloads_;
};

} // namespace aurex::syntax
