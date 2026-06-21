#pragma once

#include <aurex/frontend/syntax/ast/nodes.hpp>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace aurex::syntax {

inline constexpr std::string_view SYNTAX_STMT_PAYLOAD_ID_CONTEXT = "syntax statement payload id";
inline constexpr std::string_view SYNTAX_STMT_NODE_ID_CONTEXT = "syntax statement node id";

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
    base::SourceRange range{};
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
    ExprId range_iterable = INVALID_EXPR_ID;
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
    base::SourceRange range{};
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
    ExprId iterable = INVALID_EXPR_ID;
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
        : locals(base::BumpAllocatorAdapter<LocalStmtPayload>{arena}),
          assigns(base::BumpAllocatorAdapter<AssignStmtPayload>{arena}),
          ifs(base::BumpAllocatorAdapter<IfStmtPayload>{arena}),
          fors(base::BumpAllocatorAdapter<ForStmtPayload>{arena}),
          for_ranges(base::BumpAllocatorAdapter<ForRangeStmtPayload>{arena}),
          whiles(base::BumpAllocatorAdapter<WhileStmtPayload>{arena}),
          exprs(base::BumpAllocatorAdapter<ExprStmtPayload>{arena}),
          defers(base::BumpAllocatorAdapter<ExprStmtPayload>{arena}),
          returns(base::BumpAllocatorAdapter<ExprStmtPayload>{arena}),
          blocks(base::BumpAllocatorAdapter<AstArenaVector<StmtId>>{arena}),
          unknowns(base::BumpAllocatorAdapter<StmtNode>{arena})
    {
    }

    void swap(StmtNodePayloadArena& other) noexcept
    {
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
    StmtNodeList();
    StmtNodeList(const StmtNodeList& other);
    StmtNodeList& operator=(const StmtNodeList& other);
    StmtNodeList(StmtNodeList&& other) noexcept;
    StmtNodeList& operator=(StmtNodeList&& other) noexcept;
    ~StmtNodeList();

    [[nodiscard]] base::usize size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] StmtKind kind(base::usize index) const noexcept;
    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_used_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;
    [[nodiscard]] base::SourceRange range(base::usize index) const noexcept;
    [[nodiscard]] const AstArenaVector<StmtId>* block_statements(base::usize index) const noexcept;

    void reserve(base::usize size);
    void reserve_headers(base::usize size);
    void push_back(StmtNode node);
    [[nodiscard]] StmtId append(StmtNode node);
    void set(base::usize index, StmtNode node);
    void set_range(base::usize index, const base::SourceRange& range);
    [[nodiscard]] StmtNode take(base::usize index);
    [[nodiscard]] StmtNode operator[](base::usize index) const;

private:
    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_list(const std::vector<T, Allocator>& values)
    {
        return copy_ast_arena_vector(*this->arena_, values);
    }

    [[nodiscard]] static base::u8 pack_kind(StmtKind kind) noexcept;
    [[nodiscard]] base::u32 store_payload(StmtNode node);
    void load_header(const StmtNodeHeader& header, StmtNode& node) const noexcept;
    [[nodiscard]] StmtNode load(base::usize index) const;
    [[nodiscard]] StmtNode load_moved(base::usize index);

    template <typename T>
    [[nodiscard]] base::u32 push_payload(AstArenaVector<T>& payloads, T payload)
    {
        const base::u32 index = base::checked_u32(payloads.size(), SYNTAX_STMT_PAYLOAD_ID_CONTEXT);
        payloads.push_back(std::move(payload));
        return index;
    }

    void copy_from(const StmtNodeList& other);
    void swap(StmtNodeList& other) noexcept;

    std::unique_ptr<base::BumpAllocator> arena_;
    AstArenaVector<StmtNodeHeader> headers_;
    StmtNodePayloadArena payloads_;
};

} // namespace aurex::syntax
