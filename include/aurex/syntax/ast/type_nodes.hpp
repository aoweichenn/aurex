#pragma once

#include <aurex/syntax/ast/nodes.hpp>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace aurex::syntax {

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
    TypeNodeList();
    TypeNodeList(const TypeNodeList& other);
    TypeNodeList& operator=(const TypeNodeList& other);
    TypeNodeList(TypeNodeList&& other) noexcept;
    TypeNodeList& operator=(TypeNodeList&& other) noexcept;
    ~TypeNodeList();

    [[nodiscard]] base::usize size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] TypeKind kind(base::usize index) const noexcept;
    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_used_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;
    [[nodiscard]] base::SourceRange range(base::usize index) const noexcept;

    void reserve(base::usize size);
    void reserve_headers(base::usize size);
    void push_back(const TypeNode& node);
    [[nodiscard]] TypeId append(const TypeNode& node);
    void set(base::usize index, const TypeNode& node);
    [[nodiscard]] TypeNode take(base::usize index);
    [[nodiscard]] TypeNode operator[](base::usize index) const;

private:
    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_list(const std::vector<T, Allocator>& values) {
        return copy_ast_arena_vector(*this->arena_, values);
    }

    [[nodiscard]] base::u32 store_payload(const TypeNode& node);
    [[nodiscard]] TypeNode load(base::usize index) const;
    [[nodiscard]] TypeNode load_moved(base::usize index);

    template <typename T>
    [[nodiscard]] base::u32 push_payload(AstArenaVector<T>& payloads, T payload) {
        const base::u32 index = static_cast<base::u32>(payloads.size());
        payloads.push_back(std::move(payload));
        return index;
    }

    void copy_from(const TypeNodeList& other);
    void swap(TypeNodeList& other) noexcept;

    std::unique_ptr<base::BumpAllocator> arena_;
    AstArenaVector<TypeNodeHeader> headers_;
    TypeNodePayloadArena payloads_;
};

} // namespace aurex::syntax
