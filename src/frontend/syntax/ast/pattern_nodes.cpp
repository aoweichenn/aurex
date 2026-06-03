#include <aurex/frontend/syntax/ast/pattern_nodes.hpp>

#include <cstdint>
#include <memory>
#include <utility>

namespace aurex::syntax {

PatternNodeList::PatternNodeList()
    : arena_(std::make_unique<base::BumpAllocator>()),
      headers_(base::BumpAllocatorAdapter<PatternNodeHeader>{*this->arena_}), payloads_(*this->arena_)
{
}

PatternNodeList::PatternNodeList(const PatternNodeList& other) : PatternNodeList()
{
    this->copy_from(other);
}

PatternNodeList& PatternNodeList::operator=(const PatternNodeList& other)
{
    if (this == &other) {
        return *this;
    }
    PatternNodeList copy(other);
    *this = std::move(copy);
    return *this;
}

PatternNodeList::PatternNodeList(PatternNodeList&& other) noexcept
    : arena_(std::move(other.arena_)), headers_(std::move(other.headers_)), payloads_(std::move(other.payloads_))
{
    other.headers_ = AstArenaVector<PatternNodeHeader>{};
    other.payloads_ = PatternNodePayloadArena{};
    other.materialized_.clear();
    other.materialized_valid_.clear();
}

PatternNodeList& PatternNodeList::operator=(PatternNodeList&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

PatternNodeList::~PatternNodeList() = default;

base::usize PatternNodeList::size() const noexcept
{
    return this->headers_.size();
}

bool PatternNodeList::empty() const noexcept
{
    return this->headers_.empty();
}

PatternKind PatternNodeList::kind(const base::usize index) const noexcept
{
    return this->headers_[index].kind;
}

base::usize PatternNodeList::arena_bytes() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
}

base::usize PatternNodeList::arena_used_bytes() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->used_bytes();
}

base::usize PatternNodeList::arena_blocks() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->block_count();
}

base::SourceRange PatternNodeList::range(const base::usize index) const noexcept
{
    return this->headers_[index].range;
}

void PatternNodeList::reserve(const base::usize size)
{
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

void PatternNodeList::reserve_headers(const base::usize size)
{
    this->headers_.reserve(size);
}

void PatternNodeList::push_back(const PatternNode& node)
{
    static_cast<void>(this->append(node));
}

PatternId PatternNodeList::append(const PatternNode& node)
{
    const PatternId id{base::checked_u32(this->headers_.size(), SYNTAX_PATTERN_NODE_ID_CONTEXT)};
    PatternNodeHeader header;
    header.kind = node.kind;
    header.range = node.range;
    header.payload = this->store_payload(node);
    this->headers_.push_back(header);
    return id;
}

void PatternNodeList::set(const base::usize index, const PatternNode& node)
{
    PatternNodeHeader header;
    header.kind = node.kind;
    header.range = node.range;
    header.payload = this->store_payload(node);
    this->headers_[index] = header;
    this->invalidate_materialized(index);
}

PatternNode PatternNodeList::take(const base::usize index)
{
    this->invalidate_materialized(index);
    return this->load_moved(index);
}

PatternNode PatternNodeList::operator[](const base::usize index) const
{
    return this->load(index);
}

const PatternNode* PatternNodeList::ptr(const base::usize index) const
{
    if (index >= this->headers_.size()) {
        return nullptr;
    }
    return &this->materialized(index);
}

base::u32 PatternNodeList::store_payload(const PatternNode& node)
{
    switch (node.kind) {
        case PatternKind::binding:
        case PatternKind::const_:
            return this->push_payload(this->payloads_.bindings,
                BindingPatternPayload{
                    node.binding_name,
                    node.binding_name_id,
                });
        case PatternKind::literal:
            return this->push_payload(this->payloads_.literals,
                LiteralPatternPayload{
                    node.case_name,
                    this->copy_list(node.binding_names),
                    node.case_name_id,
                    this->copy_list(node.binding_name_ids),
                });
        case PatternKind::enum_case:
            return this->push_payload(this->payloads_.enum_cases,
                EnumCasePatternPayload{
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
            return this->push_payload(this->payloads_.slices,
                SlicePatternPayload{
                    this->copy_list(node.elements),
                    node.slice_rest_index,
                    node.has_slice_rest,
                });
        case PatternKind::struct_:
            return this->push_payload(this->payloads_.structs,
                StructPatternPayload{
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

PatternNode PatternNodeList::load(const base::usize index) const
{
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

PatternNode PatternNodeList::load_moved(const base::usize index)
{
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

const PatternNode& PatternNodeList::materialized(const base::usize index) const
{
    this->ensure_materialized_capacity(index + 1);
    if (!this->materialized_valid_[index]) {
        this->materialized_[index] = this->load(index);
        this->materialized_valid_[index] = true;
    }
    return this->materialized_[index];
}

void PatternNodeList::ensure_materialized_capacity(const base::usize size) const
{
    if (this->materialized_.size() < size) {
        this->materialized_.resize(size);
    }
    if (this->materialized_valid_.size() < size) {
        this->materialized_valid_.resize(size, false);
    }
}

void PatternNodeList::invalidate_materialized(const base::usize index) const
{
    if (index < this->materialized_valid_.size()) {
        this->materialized_valid_[index] = false;
    }
}

void PatternNodeList::copy_from(const PatternNodeList& other)
{
    this->reserve(other.size());
    for (base::usize i = 0; i < other.size(); ++i) {
        static_cast<void>(this->append(other.load(i)));
    }
}

void PatternNodeList::swap(PatternNodeList& other) noexcept
{
    using std::swap;
    swap(this->arena_, other.arena_);
    this->headers_.swap(other.headers_);
    this->payloads_.swap(other.payloads_);
    this->materialized_.swap(other.materialized_);
    this->materialized_valid_.swap(other.materialized_valid_);
}

} // namespace aurex::syntax
