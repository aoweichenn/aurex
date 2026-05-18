#include <aurex/syntax/ast/type_nodes.hpp>

#include <cstdint>
#include <memory>
#include <utility>

namespace aurex::syntax {

TypeNodeList::TypeNodeList()
    : arena_(std::make_unique<base::BumpAllocator>()),
      headers_(base::BumpAllocatorAdapter<TypeNodeHeader>{*this->arena_}), payloads_(*this->arena_)
{
}

TypeNodeList::TypeNodeList(const TypeNodeList& other) : TypeNodeList()
{
    this->copy_from(other);
}

TypeNodeList& TypeNodeList::operator=(const TypeNodeList& other)
{
    if (this == &other) {
        return *this;
    }
    TypeNodeList copy(other);
    *this = std::move(copy);
    return *this;
}

TypeNodeList::TypeNodeList(TypeNodeList&& other) noexcept
    : arena_(std::move(other.arena_)), headers_(std::move(other.headers_)), payloads_(std::move(other.payloads_))
{
    other.headers_ = AstArenaVector<TypeNodeHeader>{};
    other.payloads_ = TypeNodePayloadArena{};
}

TypeNodeList& TypeNodeList::operator=(TypeNodeList&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

TypeNodeList::~TypeNodeList() = default;

base::usize TypeNodeList::size() const noexcept
{
    return this->headers_.size();
}

bool TypeNodeList::empty() const noexcept
{
    return this->headers_.empty();
}

TypeKind TypeNodeList::kind(const base::usize index) const noexcept
{
    return this->headers_[index].kind;
}

base::usize TypeNodeList::arena_bytes() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
}

base::usize TypeNodeList::arena_used_bytes() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->used_bytes();
}

base::usize TypeNodeList::arena_blocks() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->block_count();
}

base::SourceRange TypeNodeList::range(const base::usize index) const noexcept
{
    return this->headers_[index].range;
}

void TypeNodeList::reserve(const base::usize size)
{
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

void TypeNodeList::reserve_headers(const base::usize size)
{
    this->headers_.reserve(size);
}

void TypeNodeList::push_back(const TypeNode& node)
{
    static_cast<void>(this->append(node));
}

TypeId TypeNodeList::append(const TypeNode& node)
{
    const TypeId id{static_cast<base::u32>(this->headers_.size())};
    TypeNodeHeader header;
    header.kind = node.kind;
    header.range = node.range;
    header.payload = this->store_payload(node);
    this->headers_.push_back(header);
    return id;
}

void TypeNodeList::set(const base::usize index, const TypeNode& node)
{
    TypeNodeHeader header;
    header.kind = node.kind;
    header.range = node.range;
    header.payload = this->store_payload(node);
    this->headers_[index] = header;
}

TypeNode TypeNodeList::take(const base::usize index)
{
    return this->load_moved(index);
}

TypeNode TypeNodeList::operator[](const base::usize index) const
{
    return this->load(index);
}

base::u32 TypeNodeList::store_payload(const TypeNode& node)
{
    switch (node.kind) {
        case TypeKind::primitive:
            return this->push_payload(this->payloads_.primitives, node.primitive);
        case TypeKind::named:
            return this->push_payload(this->payloads_.named,
                NamedTypePayload{
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
            return this->push_payload(this->payloads_.pointers,
                PointerTypePayload{
                    node.pointer_mutability,
                    node.pointee,
                });
        case TypeKind::reference:
            return this->push_payload(this->payloads_.references,
                PointerTypePayload{
                    node.pointer_mutability,
                    node.pointee,
                });
        case TypeKind::array:
            return this->push_payload(this->payloads_.arrays,
                ArrayTypePayload{
                    node.array_count,
                    node.array_element,
                });
        case TypeKind::slice:
            return this->push_payload(this->payloads_.slices,
                SliceTypePayload{
                    node.slice_mutability,
                    node.slice_element,
                });
        case TypeKind::tuple:
            return this->push_payload(this->payloads_.tuples, this->copy_list(node.tuple_elements));
        case TypeKind::function:
            return this->push_payload(this->payloads_.functions,
                FunctionTypePayload{
                    node.function_call_conv,
                    node.function_is_unsafe,
                    node.function_is_variadic,
                    this->copy_list(node.function_params),
                    node.function_return,
                });
    }
    return UINT32_MAX;
}

TypeNode TypeNodeList::load(const base::usize index) const
{
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

TypeNode TypeNodeList::load_moved(const base::usize index)
{
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

void TypeNodeList::copy_from(const TypeNodeList& other)
{
    this->reserve(other.size());
    for (base::usize i = 0; i < other.size(); ++i) {
        static_cast<void>(this->append(other.load(i)));
    }
}

void TypeNodeList::swap(TypeNodeList& other) noexcept
{
    using std::swap;
    swap(this->arena_, other.arena_);
    this->headers_.swap(other.headers_);
    this->payloads_.swap(other.payloads_);
}

} // namespace aurex::syntax
