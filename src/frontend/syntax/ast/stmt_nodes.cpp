#include <aurex/frontend/syntax/ast/stmt_nodes.hpp>

#include <cstdint>
#include <memory>
#include <utility>

namespace aurex::syntax {

StmtNodeList::StmtNodeList()
    : arena_(std::make_unique<base::BumpAllocator>()),
      headers_(base::BumpAllocatorAdapter<StmtNodeHeader>{*this->arena_}), payloads_(*this->arena_)
{
}

StmtNodeList::StmtNodeList(const StmtNodeList& other) : StmtNodeList()
{
    this->copy_from(other);
}

StmtNodeList& StmtNodeList::operator=(const StmtNodeList& other)
{
    if (this == &other) {
        return *this;
    }
    StmtNodeList copy(other);
    *this = std::move(copy);
    return *this;
}

StmtNodeList::StmtNodeList(StmtNodeList&& other) noexcept
    : arena_(std::move(other.arena_)), headers_(std::move(other.headers_)), payloads_(std::move(other.payloads_))
{
    other.headers_ = AstArenaVector<StmtNodeHeader>{};
    other.payloads_ = StmtNodePayloadArena{};
}

StmtNodeList& StmtNodeList::operator=(StmtNodeList&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

StmtNodeList::~StmtNodeList() = default;

base::usize StmtNodeList::size() const noexcept
{
    return this->headers_.size();
}

bool StmtNodeList::empty() const noexcept
{
    return this->headers_.empty();
}

StmtKind StmtNodeList::kind(const base::usize index) const noexcept
{
    return static_cast<StmtKind>(this->headers_[index].kind);
}

base::usize StmtNodeList::arena_bytes() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
}

base::usize StmtNodeList::arena_used_bytes() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->used_bytes();
}

base::usize StmtNodeList::arena_blocks() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->block_count();
}

base::SourceRange StmtNodeList::range(const base::usize index) const noexcept
{
    return this->headers_[index].range;
}

const AstArenaVector<StmtId>* StmtNodeList::block_statements(const base::usize index) const noexcept
{
    if (this->kind(index) != StmtKind::block) {
        return nullptr;
    }
    return &this->payloads_.blocks[this->headers_[index].payload];
}

void StmtNodeList::reserve(const base::usize size)
{
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

void StmtNodeList::reserve_headers(const base::usize size)
{
    this->headers_.reserve(size);
}

void StmtNodeList::push_back(StmtNode node)
{
    static_cast<void>(this->append(std::move(node)));
}

StmtId StmtNodeList::append(StmtNode node)
{
    const StmtId id{base::checked_u32(this->headers_.size(), SYNTAX_STMT_NODE_ID_CONTEXT)};
    StmtNodeHeader header;
    header.kind = pack_kind(node.kind);
    header.range = node.range;
    header.payload = this->store_payload(std::move(node));
    this->headers_.push_back(header);
    return id;
}

void StmtNodeList::set(const base::usize index, StmtNode node)
{
    StmtNodeHeader header;
    header.kind = pack_kind(node.kind);
    header.range = node.range;
    header.payload = this->store_payload(std::move(node));
    this->headers_[index] = header;
}

void StmtNodeList::set_range(const base::usize index, const base::SourceRange& range)
{
    this->headers_[index].range = range;
}

StmtNode StmtNodeList::take(const base::usize index)
{
    return this->load_moved(index);
}

StmtNode StmtNodeList::operator[](const base::usize index) const
{
    return this->load(index);
}

base::u8 StmtNodeList::pack_kind(const StmtKind kind) noexcept
{
    return static_cast<base::u8>(kind);
}

base::u32 StmtNodeList::store_payload(StmtNode node)
{
    switch (node.kind) {
        case StmtKind::let:
        case StmtKind::var:
            return this->push_payload(this->payloads_.locals,
                LocalStmtPayload{
                    node.name,
                    node.name_id,
                    node.pattern,
                    node.declared_type,
                    node.init,
                    node.else_block,
                });
        case StmtKind::assign:
            return this->push_payload(this->payloads_.assigns,
                AssignStmtPayload{
                    node.assign_op,
                    node.lhs,
                    node.rhs,
                });
        case StmtKind::if_:
            return this->push_payload(this->payloads_.ifs,
                IfStmtPayload{
                    node.condition,
                    node.pattern,
                    node.then_block,
                    node.else_block,
                    node.else_if,
                });
        case StmtKind::for_:
            return this->push_payload(this->payloads_.fors,
                ForStmtPayload{
                    node.for_init,
                    node.condition,
                    node.for_update,
                    node.body,
                });
        case StmtKind::for_range:
            return this->push_payload(this->payloads_.for_ranges,
                ForRangeStmtPayload{
                    node.name,
                    node.name_id,
                    node.range_start,
                    node.range_end,
                    node.range_step,
                    node.range_iterable,
                    node.body,
                });
        case StmtKind::while_:
            return this->push_payload(this->payloads_.whiles,
                WhileStmtPayload{
                    node.condition,
                    node.pattern,
                    node.body,
                });
        case StmtKind::expr:
            return this->push_payload(this->payloads_.exprs, ExprStmtPayload{node.init});
        case StmtKind::defer:
            return this->push_payload(this->payloads_.defers, ExprStmtPayload{node.init});
        case StmtKind::return_:
            return this->push_payload(this->payloads_.returns, ExprStmtPayload{node.return_value});
        case StmtKind::block:
            return this->push_payload(this->payloads_.blocks, this->copy_list(node.statements));
        case StmtKind::break_:
        case StmtKind::continue_:
            break;
    }
    return this->push_payload(this->payloads_.unknowns, std::move(node));
}

void StmtNodeList::load_header(const StmtNodeHeader& header, StmtNode& node) const noexcept
{
    node.kind = static_cast<StmtKind>(header.kind);
    node.range = header.range;
}

StmtNode StmtNodeList::load(const base::usize index) const
{
    const StmtNodeHeader& header = this->headers_[index];
    StmtNode node;
    this->load_header(header, node);
    bool loaded_known_payload = false;
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
            loaded_known_payload = true;
            break;
        }
        case StmtKind::assign: {
            const AssignStmtPayload& payload = this->payloads_.assigns[header.payload];
            node.assign_op = payload.op;
            node.lhs = payload.lhs;
            node.rhs = payload.rhs;
            loaded_known_payload = true;
            break;
        }
        case StmtKind::if_: {
            const IfStmtPayload& payload = this->payloads_.ifs[header.payload];
            node.condition = payload.condition;
            node.pattern = payload.pattern;
            node.then_block = payload.then_block;
            node.else_block = payload.else_block;
            node.else_if = payload.else_if;
            loaded_known_payload = true;
            break;
        }
        case StmtKind::for_: {
            const ForStmtPayload& payload = this->payloads_.fors[header.payload];
            node.for_init = payload.init;
            node.condition = payload.condition;
            node.for_update = payload.update;
            node.body = payload.body;
            loaded_known_payload = true;
            break;
        }
        case StmtKind::for_range: {
            const ForRangeStmtPayload& payload = this->payloads_.for_ranges[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.range_start = payload.start;
            node.range_end = payload.end;
            node.range_step = payload.step;
            node.range_iterable = payload.iterable;
            node.body = payload.body;
            loaded_known_payload = true;
            break;
        }
        case StmtKind::while_: {
            const WhileStmtPayload& payload = this->payloads_.whiles[header.payload];
            node.condition = payload.condition;
            node.pattern = payload.pattern;
            node.body = payload.body;
            loaded_known_payload = true;
            break;
        }
        case StmtKind::expr:
            node.init = this->payloads_.exprs[header.payload].value;
            loaded_known_payload = true;
            break;
        case StmtKind::defer:
            node.init = this->payloads_.defers[header.payload].value;
            loaded_known_payload = true;
            break;
        case StmtKind::return_:
            node.return_value = this->payloads_.returns[header.payload].value;
            loaded_known_payload = true;
            break;
        case StmtKind::block:
            node.statements = copy_std_vector(this->payloads_.blocks[header.payload]);
            loaded_known_payload = true;
            break;
        case StmtKind::break_:
        case StmtKind::continue_:
            loaded_known_payload = true;
            break;
    }
    if (!loaded_known_payload) {
        node = this->payloads_.unknowns[header.payload];
        this->load_header(header, node);
    }
    return node;
}

StmtNode StmtNodeList::load_moved(const base::usize index)
{
    const StmtNodeHeader& header = this->headers_[index];
    StmtNode node;
    this->load_header(header, node);
    bool loaded_known_payload = false;
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
            loaded_known_payload = true;
            break;
        }
        case StmtKind::assign: {
            const AssignStmtPayload& payload = this->payloads_.assigns[header.payload];
            node.assign_op = payload.op;
            node.lhs = payload.lhs;
            node.rhs = payload.rhs;
            loaded_known_payload = true;
            break;
        }
        case StmtKind::if_: {
            const IfStmtPayload& payload = this->payloads_.ifs[header.payload];
            node.condition = payload.condition;
            node.pattern = payload.pattern;
            node.then_block = payload.then_block;
            node.else_block = payload.else_block;
            node.else_if = payload.else_if;
            loaded_known_payload = true;
            break;
        }
        case StmtKind::for_: {
            const ForStmtPayload& payload = this->payloads_.fors[header.payload];
            node.for_init = payload.init;
            node.condition = payload.condition;
            node.for_update = payload.update;
            node.body = payload.body;
            loaded_known_payload = true;
            break;
        }
        case StmtKind::for_range: {
            const ForRangeStmtPayload& payload = this->payloads_.for_ranges[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.range_start = payload.start;
            node.range_end = payload.end;
            node.range_step = payload.step;
            node.range_iterable = payload.iterable;
            node.body = payload.body;
            loaded_known_payload = true;
            break;
        }
        case StmtKind::while_: {
            const WhileStmtPayload& payload = this->payloads_.whiles[header.payload];
            node.condition = payload.condition;
            node.pattern = payload.pattern;
            node.body = payload.body;
            loaded_known_payload = true;
            break;
        }
        case StmtKind::expr:
            node.init = this->payloads_.exprs[header.payload].value;
            loaded_known_payload = true;
            break;
        case StmtKind::defer:
            node.init = this->payloads_.defers[header.payload].value;
            loaded_known_payload = true;
            break;
        case StmtKind::return_:
            node.return_value = this->payloads_.returns[header.payload].value;
            loaded_known_payload = true;
            break;
        case StmtKind::block:
            node.statements = copy_std_vector(this->payloads_.blocks[header.payload]);
            loaded_known_payload = true;
            break;
        case StmtKind::break_:
        case StmtKind::continue_:
            loaded_known_payload = true;
            break;
    }
    if (!loaded_known_payload) {
        node = std::move(this->payloads_.unknowns[header.payload]);
        this->load_header(header, node);
    }
    return node;
}

void StmtNodeList::copy_from(const StmtNodeList& other)
{
    this->reserve(other.size());
    for (base::usize i = 0; i < other.size(); ++i) {
        static_cast<void>(this->append(other.load(i)));
    }
}

void StmtNodeList::swap(StmtNodeList& other) noexcept
{
    using std::swap;
    swap(this->arena_, other.arena_);
    this->headers_.swap(other.headers_);
    this->payloads_.swap(other.payloads_);
}

} // namespace aurex::syntax
