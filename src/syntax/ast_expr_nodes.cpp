#include <aurex/syntax/ast/expr_nodes.hpp>

#include <cassert>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace aurex::syntax {

ExprNodeList::ExprNodeList()
    : arena_(std::make_unique<base::BumpAllocator>()),
      headers_(base::BumpAllocatorAdapter<ExprNodeHeader> {*this->arena_}),
      payloads_(*this->arena_) {
}

ExprNodeList::ExprNodeList(const ExprNodeList& other)
    : ExprNodeList() {
    this->copy_from(other);
}

ExprNodeList& ExprNodeList::operator=(const ExprNodeList& other) {
    if (this == &other) {
        return *this;
    }
    ExprNodeList copy(other);
    *this = std::move(copy);
    return *this;
}

ExprNodeList::ExprNodeList(ExprNodeList&& other) noexcept
    : arena_(std::move(other.arena_)),
      headers_(std::move(other.headers_)),
      payloads_(std::move(other.payloads_)) {
    other.headers_ = AstArenaVector<ExprNodeHeader> {};
    other.payloads_ = ExprNodePayloadArena {};
}

ExprNodeList& ExprNodeList::operator=(ExprNodeList&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

ExprNodeList::~ExprNodeList() = default;

base::usize ExprNodeList::size() const noexcept {
    return this->headers_.size();
}

bool ExprNodeList::empty() const noexcept {
    return this->headers_.empty();
}

ExprKind ExprNodeList::kind(const base::usize index) const noexcept {
    return this->headers_[index].kind;
}

base::usize ExprNodeList::arena_bytes() const noexcept {
    return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
}

base::usize ExprNodeList::arena_used_bytes() const noexcept {
    return this->arena_ == nullptr ? 0 : this->arena_->used_bytes();
}

base::usize ExprNodeList::arena_blocks() const noexcept {
    return this->arena_ == nullptr ? 0 : this->arena_->block_count();
}

base::SourceRange ExprNodeList::range(const base::usize index) const noexcept {
    return this->headers_[index].range;
}

const LiteralExprPayload* ExprNodeList::literal_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || !this->is_literal(this->headers_[index].kind)) {
        return nullptr;
    }
    return &this->payloads_.literals[this->headers_[index].payload];
}

const NameExprPayload* ExprNodeList::name_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::name) {
        return nullptr;
    }
    return &this->payloads_.names[this->headers_[index].payload];
}

NameExprPayload* ExprNodeList::name_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::name) {
        return nullptr;
    }
    return &this->payloads_.names[this->headers_[index].payload];
}

const GenericApplyExprPayload* ExprNodeList::generic_apply_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::generic_apply) {
        return nullptr;
    }
    return &this->payloads_.generic_applies[this->headers_[index].payload];
}

GenericApplyExprPayload* ExprNodeList::generic_apply_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::generic_apply) {
        return nullptr;
    }
    return &this->payloads_.generic_applies[this->headers_[index].payload];
}

const UnaryExprPayload* ExprNodeList::unary_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::unary) {
        return nullptr;
    }
    return &this->payloads_.unaries[this->headers_[index].payload];
}

UnaryExprPayload* ExprNodeList::unary_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::unary) {
        return nullptr;
    }
    return &this->payloads_.unaries[this->headers_[index].payload];
}

const TryExprPayload* ExprNodeList::try_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::try_expr) {
        return nullptr;
    }
    return &this->payloads_.tries[this->headers_[index].payload];
}

TryExprPayload* ExprNodeList::try_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::try_expr) {
        return nullptr;
    }
    return &this->payloads_.tries[this->headers_[index].payload];
}

const BinaryExprPayload* ExprNodeList::binary_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::binary) {
        return nullptr;
    }
    return &this->payloads_.binaries[this->headers_[index].payload];
}

BinaryExprPayload* ExprNodeList::binary_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::binary) {
        return nullptr;
    }
    return &this->payloads_.binaries[this->headers_[index].payload];
}

const CallExprPayload* ExprNodeList::call_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || (this->headers_[index].kind != ExprKind::call && this->headers_[index].kind != ExprKind::str_from_bytes_unchecked)) {
        return nullptr;
    }
    return &this->payloads_.calls[this->headers_[index].payload];
}

CallExprPayload* ExprNodeList::call_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || (this->headers_[index].kind != ExprKind::call && this->headers_[index].kind != ExprKind::str_from_bytes_unchecked)) {
        return nullptr;
    }
    return &this->payloads_.calls[this->headers_[index].payload];
}

const IfExprPayload* ExprNodeList::if_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::if_expr) {
        return nullptr;
    }
    return &this->payloads_.ifs[this->headers_[index].payload];
}

IfExprPayload* ExprNodeList::if_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::if_expr) {
        return nullptr;
    }
    return &this->payloads_.ifs[this->headers_[index].payload];
}

const BlockExprPayload* ExprNodeList::block_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || (this->headers_[index].kind != ExprKind::block_expr && this->headers_[index].kind != ExprKind::unsafe_block)) {
        return nullptr;
    }
    return &this->payloads_.blocks[this->headers_[index].payload];
}

BlockExprPayload* ExprNodeList::block_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || (this->headers_[index].kind != ExprKind::block_expr && this->headers_[index].kind != ExprKind::unsafe_block)) {
        return nullptr;
    }
    return &this->payloads_.blocks[this->headers_[index].payload];
}

const MatchExprPayload* ExprNodeList::match_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::match_expr) {
        return nullptr;
    }
    return &this->payloads_.matches[this->headers_[index].payload];
}

MatchExprPayload* ExprNodeList::match_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::match_expr) {
        return nullptr;
    }
    return &this->payloads_.matches[this->headers_[index].payload];
}

const ArrayExprPayload* ExprNodeList::array_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::array_literal) {
        return nullptr;
    }
    return &this->payloads_.arrays[this->headers_[index].payload];
}

ArrayExprPayload* ExprNodeList::array_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::array_literal) {
        return nullptr;
    }
    return &this->payloads_.arrays[this->headers_[index].payload];
}

const AstArenaVector<ExprId>* ExprNodeList::tuple_elements(const base::usize index) const noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::tuple_literal) {
        return nullptr;
    }
    return &this->payloads_.tuples[this->headers_[index].payload];
}

AstArenaVector<ExprId>* ExprNodeList::tuple_elements(const base::usize index) noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::tuple_literal) {
        return nullptr;
    }
    return &this->payloads_.tuples[this->headers_[index].payload];
}

const FieldExprPayload* ExprNodeList::field_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::field) {
        return nullptr;
    }
    return &this->payloads_.fields[this->headers_[index].payload];
}

FieldExprPayload* ExprNodeList::field_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::field) {
        return nullptr;
    }
    return &this->payloads_.fields[this->headers_[index].payload];
}

const IndexExprPayload* ExprNodeList::index_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::index) {
        return nullptr;
    }
    return &this->payloads_.indexes[this->headers_[index].payload];
}

IndexExprPayload* ExprNodeList::index_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::index) {
        return nullptr;
    }
    return &this->payloads_.indexes[this->headers_[index].payload];
}

const SliceExprPayload* ExprNodeList::slice_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::slice) {
        return nullptr;
    }
    return &this->payloads_.slices[this->headers_[index].payload];
}

SliceExprPayload* ExprNodeList::slice_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::slice) {
        return nullptr;
    }
    return &this->payloads_.slices[this->headers_[index].payload];
}

const StructLiteralExprPayload* ExprNodeList::struct_literal_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::struct_literal) {
        return nullptr;
    }
    return &this->payloads_.struct_literals[this->headers_[index].payload];
}

StructLiteralExprPayload* ExprNodeList::struct_literal_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || this->headers_[index].kind != ExprKind::struct_literal) {
        return nullptr;
    }
    return &this->payloads_.struct_literals[this->headers_[index].payload];
}

const CastExprPayload* ExprNodeList::cast_payload(const base::usize index) const noexcept {
    if (!this->payload_available(index) || !this->is_cast_like(this->headers_[index].kind)) {
        return nullptr;
    }
    return &this->payloads_.casts[this->headers_[index].payload];
}

CastExprPayload* ExprNodeList::cast_payload(const base::usize index) noexcept {
    if (!this->payload_available(index) || !this->is_cast_like(this->headers_[index].kind)) {
        return nullptr;
    }
    return &this->payloads_.casts[this->headers_[index].payload];
}

void ExprNodeList::reserve(const base::usize size) {
    const AstReserveEstimate::Exprs plan = ast_expr_reserve_for_node_capacity(size);
    this->reserve_headers(plan.headers);
    this->reserve_payloads(plan);
}

void ExprNodeList::reserve_touched(const base::usize size) {
    this->reserve_touched(ast_expr_reserve_for_node_capacity(size));
}

void ExprNodeList::reserve_touched(const AstReserveEstimate::Exprs& plan) {
    if (this->arena_ != nullptr) {
        this->arena_->reserve_touched(estimated_arena_bytes(plan));
    }
    this->reserve_headers(plan.headers);
    this->reserve_payloads(plan);
}

void ExprNodeList::reserve_headers(const base::usize size) {
    this->headers_.reserve(size);
}

ExprId ExprNodeList::append_invalid(const base::SourceRange& range) {
    return this->append_header(ExprKind::invalid, range, UINT32_MAX);
}

ExprId ExprNodeList::append_literal(
    const ExprKind kind,
    const base::SourceRange& range,
    const std::string_view text) {
    return this->append_header(kind, range, this->emplace_payload(this->payloads_.literals, text));
}

ExprId ExprNodeList::append_name(const base::SourceRange& range, NameExprPayload payload) {
    return this->append_name(
        range,
        payload.scope_name,
        payload.scope_range,
        payload.text,
        payload.scope_name_id,
        payload.text_id,
        std::move(payload.type_args));
}

ExprId ExprNodeList::append_unary(
    const ExprKind kind,
    const base::SourceRange& range,
    const UnaryExprPayload payload) {
    return this->append_unary(kind, range, payload.op, payload.operand);
}

ExprId ExprNodeList::append_unary(
    const ExprKind kind,
    const base::SourceRange& range,
    const UnaryOp op,
    const ExprId operand) {
    assert(kind == ExprKind::unary);
    return this->append_header(kind, range, this->emplace_payload(this->payloads_.unaries, op, operand));
}

ExprId ExprNodeList::append_try(const base::SourceRange& range, const TryExprPayload payload) {
    return this->append_try(range, payload.operand);
}

ExprId ExprNodeList::append_try(const base::SourceRange& range, const ExprId operand) {
    return this->append_header(ExprKind::try_expr, range, this->emplace_payload(this->payloads_.tries, operand));
}

ExprId ExprNodeList::append_binary(const base::SourceRange& range, const BinaryExprPayload payload) {
    return this->append_binary(range, payload.op, payload.lhs, payload.rhs);
}

ExprId ExprNodeList::append_binary(
    const base::SourceRange& range,
    const BinaryOp op,
    const ExprId lhs,
    const ExprId rhs) {
    return this->append_header(ExprKind::binary, range, this->emplace_payload(this->payloads_.binaries, op, lhs, rhs));
}

ExprId ExprNodeList::append_call(
    const ExprKind kind,
    const base::SourceRange& range,
    CallExprPayload payload) {
    return this->append_call(kind, range, payload.callee, std::move(payload.args));
}

ExprId ExprNodeList::append_if(const base::SourceRange& range, const IfExprPayload payload) {
    return this->append_if(range, payload.condition, payload.condition_pattern, payload.then_expr, payload.else_expr);
}

ExprId ExprNodeList::append_if(
    const base::SourceRange& range,
    const ExprId condition,
    const PatternId condition_pattern,
    const ExprId then_expr,
    const ExprId else_expr) {
    return this->append_header(
        ExprKind::if_expr,
        range,
        this->emplace_payload(this->payloads_.ifs, condition, condition_pattern, then_expr, else_expr));
}

ExprId ExprNodeList::append_block(
    const ExprKind kind,
    const base::SourceRange& range,
    const BlockExprPayload payload) {
    return this->append_block(kind, range, payload.block, payload.result);
}

ExprId ExprNodeList::append_block(
    const ExprKind kind,
    const base::SourceRange& range,
    const StmtId block,
    const ExprId result) {
    return this->append_header(kind, range, this->emplace_payload(this->payloads_.blocks, block, result));
}

ExprId ExprNodeList::append_match(const base::SourceRange& range, MatchExprPayload payload) {
    return this->append_match(range, payload.value, std::move(payload.arms));
}

ExprId ExprNodeList::append_array(const base::SourceRange& range, ArrayExprPayload payload) {
    return this->append_array(range, std::move(payload.elements), payload.repeat_value, payload.repeat_count);
}

ExprId ExprNodeList::append_field(const base::SourceRange& range, const FieldExprPayload& payload) {
    return this->append_field(range, payload.object, payload.field_name, payload.field_name_id);
}

ExprId ExprNodeList::append_field(
    const base::SourceRange& range,
    const ExprId object,
    const std::string_view field_name,
    const IdentId field_name_id) {
    return this->append_header(ExprKind::field, range, this->emplace_payload(this->payloads_.fields, object, field_name, field_name_id));
}

ExprId ExprNodeList::append_index(const base::SourceRange& range, const IndexExprPayload payload) {
    return this->append_index(range, payload.object, payload.index);
}

ExprId ExprNodeList::append_index(
    const base::SourceRange& range,
    const ExprId object,
    const ExprId index) {
    return this->append_header(ExprKind::index, range, this->emplace_payload(this->payloads_.indexes, object, index));
}

ExprId ExprNodeList::append_slice(const base::SourceRange& range, const SliceExprPayload payload) {
    return this->append_slice(range, payload.object, payload.start, payload.end);
}

ExprId ExprNodeList::append_slice(
    const base::SourceRange& range,
    const ExprId object,
    const ExprId start,
    const ExprId end) {
    return this->append_header(ExprKind::slice, range, this->emplace_payload(this->payloads_.slices, object, start, end));
}

ExprId ExprNodeList::append_struct_literal(const base::SourceRange& range, StructLiteralExprPayload payload) {
    return this->append_struct_literal(
        range,
        payload.object,
        payload.scope_name,
        payload.scope_range,
        payload.name,
        payload.scope_name_id,
        payload.name_id,
        std::move(payload.type_args),
        std::move(payload.field_inits));
}

ExprId ExprNodeList::append_cast_like(
    const ExprKind kind,
    const base::SourceRange& range,
    const CastExprPayload payload) {
    return this->append_cast_like(kind, range, payload.type, payload.expr);
}

ExprId ExprNodeList::append_cast_like(
    const ExprKind kind,
    const base::SourceRange& range,
    const TypeId type,
    const ExprId expr) {
    return this->append_header(kind, range, this->emplace_payload(this->payloads_.casts, type, expr));
}

void ExprNodeList::set_invalid(const base::usize index, const base::SourceRange& range) {
    this->set_header(index, ExprKind::invalid, range, UINT32_MAX);
}

void ExprNodeList::set_generic_apply(const base::usize index, const base::SourceRange& range, GenericApplyExprPayload payload) {
    this->set_generic_apply(index, range, payload.callee, std::move(payload.type_args));
}

void ExprNodeList::set_unary(
    const base::usize index,
    const ExprKind kind,
    const base::SourceRange& range,
    const UnaryExprPayload payload) {
    this->set_unary(index, kind, range, payload.op, payload.operand);
}

void ExprNodeList::set_unary(
    const base::usize index,
    const ExprKind kind,
    const base::SourceRange& range,
    const UnaryOp op,
    const ExprId operand) {
    assert(kind == ExprKind::unary);
    this->set_header(index, kind, range, this->emplace_payload(this->payloads_.unaries, op, operand));
}

void ExprNodeList::set_try(const base::usize index, const base::SourceRange& range, const TryExprPayload payload) {
    this->set_try(index, range, payload.operand);
}

void ExprNodeList::set_try(const base::usize index, const base::SourceRange& range, const ExprId operand) {
    this->set_header(index, ExprKind::try_expr, range, this->emplace_payload(this->payloads_.tries, operand));
}

void ExprNodeList::set_call(const base::usize index, const ExprKind kind, const base::SourceRange& range, CallExprPayload payload) {
    this->set_call(index, kind, range, payload.callee, std::move(payload.args));
}

void ExprNodeList::set_field(const base::usize index, const base::SourceRange& range, const FieldExprPayload& payload) {
    this->set_field(index, range, payload.object, payload.field_name, payload.field_name_id);
}

void ExprNodeList::set_field(
    const base::usize index,
    const base::SourceRange& range,
    const ExprId object,
    const std::string_view field_name,
    const IdentId field_name_id) {
    this->set_header(
        index,
        ExprKind::field,
        range,
        this->emplace_payload(this->payloads_.fields, object, field_name, field_name_id));
}

void ExprNodeList::set_index(const base::usize index, const base::SourceRange& range, const IndexExprPayload payload) {
    this->set_index(index, range, payload.object, payload.index);
}

void ExprNodeList::set_index(
    const base::usize index,
    const base::SourceRange& range,
    const ExprId object,
    const ExprId index_expr) {
    this->set_header(index, ExprKind::index, range, this->emplace_payload(this->payloads_.indexes, object, index_expr));
}

void ExprNodeList::set_slice(const base::usize index, const base::SourceRange& range, const SliceExprPayload payload) {
    this->set_slice(index, range, payload.object, payload.start, payload.end);
}

void ExprNodeList::set_slice(
    const base::usize index,
    const base::SourceRange& range,
    const ExprId object,
    const ExprId start,
    const ExprId end) {
    this->set_header(index, ExprKind::slice, range, this->emplace_payload(this->payloads_.slices, object, start, end));
}

void ExprNodeList::set_struct_literal(const base::usize index, const base::SourceRange& range, StructLiteralExprPayload payload) {
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
        std::move(payload.field_inits));
}

bool ExprNodeList::retag_block_expr(
    const base::usize index,
    const ExprKind kind,
    const base::SourceRange& range) noexcept {
    if (index >= this->headers_.size() || !this->payload_available(index) || !this->is_block_payload_kind(this->headers_[index].kind) || !this->is_block_payload_kind(kind)) {
        return false;
    }
    this->headers_[index].kind = kind;
    this->headers_[index].range = range;
    return true;
}

ExprId ExprNodeList::append_header(
    const ExprKind kind,
    const base::SourceRange& range,
    const base::u32 payload) {
    const ExprId id {static_cast<base::u32>(this->headers_.size())};
    this->headers_.push_back(ExprNodeHeader {range, payload, kind});
    return id;
}

void ExprNodeList::set_header(
    const base::usize index,
    const ExprKind kind,
    const base::SourceRange& range,
    const base::u32 payload) {
    this->headers_[index] = ExprNodeHeader {range, payload, kind};
}

bool ExprNodeList::payload_available(const base::usize index) const noexcept {
    return index < this->headers_.size() && this->headers_[index].payload != UINT32_MAX;
}

bool ExprNodeList::is_literal(const ExprKind kind) const noexcept {
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

bool ExprNodeList::is_cast_like(const ExprKind kind) const noexcept {
    switch (kind) {
    case ExprKind::cast:
    case ExprKind::pcast:
    case ExprKind::bcast:
    case ExprKind::size_of:
    case ExprKind::align_of:
    case ExprKind::ptr_addr:
    case ExprKind::paddr:
    case ExprKind::slice_data:
    case ExprKind::slice_len:
    case ExprKind::str_data:
    case ExprKind::str_byte_len:
    case ExprKind::str_is_valid_utf8:
    case ExprKind::str_from_utf8_checked:
        return true;
    default:
        return false;
    }
}

bool ExprNodeList::is_block_payload_kind(const ExprKind kind) const noexcept {
    return kind == ExprKind::block_expr || kind == ExprKind::unsafe_block;
}

base::usize ExprNodeList::allocation_bytes(
    const base::usize count,
    const base::usize element_size) noexcept {
    if (count == 0) {
        return 0;
    }
    return count * element_size + SYNTAX_AST_ARENA_ALLOCATION_PADDING_BYTES;
}

base::usize ExprNodeList::estimated_arena_bytes(
    const AstReserveEstimate::Exprs& plan) noexcept {
    base::usize bytes = allocation_bytes(plan.headers, sizeof(ExprNodeHeader));
    bytes += allocation_bytes(plan.literals, sizeof(LiteralExprPayload));
    bytes += allocation_bytes(plan.names, sizeof(NameExprPayload));
    bytes += allocation_bytes(plan.generic_applies, sizeof(GenericApplyExprPayload));
    bytes += allocation_bytes(plan.unaries, sizeof(UnaryExprPayload));
    bytes += allocation_bytes(plan.tries, sizeof(TryExprPayload));
    bytes += allocation_bytes(plan.binaries, sizeof(BinaryExprPayload));
    bytes += allocation_bytes(plan.calls, sizeof(CallExprPayload));
    bytes += allocation_bytes(plan.ifs, sizeof(IfExprPayload));
    bytes += allocation_bytes(plan.blocks, sizeof(BlockExprPayload));
    bytes += allocation_bytes(plan.matches, sizeof(MatchExprPayload));
    bytes += allocation_bytes(plan.arrays, sizeof(ArrayExprPayload));
    bytes += allocation_bytes(plan.tuples, sizeof(AstArenaVector<ExprId>));
    bytes += allocation_bytes(plan.fields, sizeof(FieldExprPayload));
    bytes += allocation_bytes(plan.indexes, sizeof(IndexExprPayload));
    bytes += allocation_bytes(plan.slices, sizeof(SliceExprPayload));
    bytes += allocation_bytes(plan.struct_literals, sizeof(StructLiteralExprPayload));
    bytes += allocation_bytes(plan.casts, sizeof(CastExprPayload));
    return bytes;
}

void ExprNodeList::reserve_payloads(const AstReserveEstimate::Exprs& plan) {
    this->payloads_.literals.reserve(plan.literals);
    this->payloads_.names.reserve(plan.names);
    this->payloads_.generic_applies.reserve(plan.generic_applies);
    this->payloads_.unaries.reserve(plan.unaries);
    this->payloads_.tries.reserve(plan.tries);
    this->payloads_.binaries.reserve(plan.binaries);
    this->payloads_.calls.reserve(plan.calls);
    this->payloads_.ifs.reserve(plan.ifs);
    this->payloads_.blocks.reserve(plan.blocks);
    this->payloads_.matches.reserve(plan.matches);
    this->payloads_.arrays.reserve(plan.arrays);
    this->payloads_.tuples.reserve(plan.tuples);
    this->payloads_.fields.reserve(plan.fields);
    this->payloads_.indexes.reserve(plan.indexes);
    this->payloads_.slices.reserve(plan.slices);
    this->payloads_.struct_literals.reserve(plan.struct_literals);
    this->payloads_.casts.reserve(plan.casts);
}

void ExprNodeList::copy_append_from(const ExprNodeList& other, const base::usize index) {
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
                      std::vector<TypeId> {})
                : this->append_name(
                      range,
                      payload->scope_name,
                      payload->scope_range,
                      payload->text,
                      payload->scope_name_id,
                      payload->text_id,
                      copy_std_vector(payload->type_args)));
        break;
    }
    case ExprKind::generic_apply: {
        const GenericApplyExprPayload* const payload = other.generic_apply_payload(index);
        static_cast<void>(payload == nullptr
                ? this->append_generic_apply(range, INVALID_EXPR_ID, std::vector<TypeId> {})
                : this->append_generic_apply(range, payload->callee, copy_std_vector(payload->type_args)));
        break;
    }
    case ExprKind::unary: {
        const UnaryExprPayload* const payload = other.unary_payload(index);
        if (payload == nullptr) {
            static_cast<void>(this->append_unary(ExprKind::unary, range, UnaryOp::logical_not, INVALID_EXPR_ID));
        } else {
            static_cast<void>(this->append_unary(ExprKind::unary, range, payload->op, payload->operand));
        }
        break;
    }
    case ExprKind::try_expr: {
        const TryExprPayload* const payload = other.try_payload(index);
        if (payload == nullptr) {
            static_cast<void>(this->append_try(range, INVALID_EXPR_ID));
        } else {
            static_cast<void>(this->append_try(range, payload->operand));
        }
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
                      payload->repeat_count));
        break;
    }
    case ExprKind::tuple_literal: {
        const AstArenaVector<ExprId>* const payload = other.tuple_elements(index);
        static_cast<void>(this->append_tuple(range, payload == nullptr ? std::vector<ExprId> {} : copy_std_vector(*payload)));
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
                      std::vector<FieldInit> {})
                : this->append_struct_literal(
                      range,
                      payload->object,
                      payload->scope_name,
                      payload->scope_range,
                      payload->name,
                      payload->scope_name_id,
                      payload->name_id,
                      copy_std_vector(payload->type_args),
                      copy_std_vector(payload->field_inits)));
        break;
    }
    case ExprKind::cast:
    case ExprKind::pcast:
    case ExprKind::bcast:
    case ExprKind::size_of:
    case ExprKind::align_of:
    case ExprKind::ptr_addr:
    case ExprKind::paddr:
    case ExprKind::slice_data:
    case ExprKind::slice_len:
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

void ExprNodeList::copy_from(const ExprNodeList& other) {
    this->reserve(other.size());
    for (base::usize i = 0; i < other.size(); ++i) {
        this->copy_append_from(other, i);
    }
}

void ExprNodeList::swap(ExprNodeList& other) noexcept {
    using std::swap;
    swap(this->arena_, other.arena_);
    this->headers_.swap(other.headers_);
    this->payloads_.swap(other.payloads_);
}

} // namespace aurex::syntax
