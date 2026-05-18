#pragma once

#include <aurex/syntax/ast/nodes.hpp>

#include <cstdint>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

namespace aurex::syntax {

struct PatternNodeHeader {
    base::SourceRange range{};
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
        : bindings(base::BumpAllocatorAdapter<BindingPatternPayload>{arena}),
          literals(base::BumpAllocatorAdapter<LiteralPatternPayload>{arena}),
          enum_cases(base::BumpAllocatorAdapter<EnumCasePatternPayload>{arena}),
          tuples(base::BumpAllocatorAdapter<AstArenaVector<PatternId>>{arena}),
          slices(base::BumpAllocatorAdapter<SlicePatternPayload>{arena}),
          structs(base::BumpAllocatorAdapter<StructPatternPayload>{arena}),
          alternatives(base::BumpAllocatorAdapter<AstArenaVector<PatternId>>{arena})
    {
    }

    void swap(PatternNodePayloadArena& other) noexcept
    {
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
    PatternNodeList();
    PatternNodeList(const PatternNodeList& other);
    PatternNodeList& operator=(const PatternNodeList& other);
    PatternNodeList(PatternNodeList&& other) noexcept;
    PatternNodeList& operator=(PatternNodeList&& other) noexcept;
    ~PatternNodeList();

    [[nodiscard]] base::usize size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] PatternKind kind(base::usize index) const noexcept;
    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_used_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;
    [[nodiscard]] base::SourceRange range(base::usize index) const noexcept;

    void reserve(base::usize size);
    void reserve_headers(base::usize size);
    void push_back(const PatternNode& node);
    [[nodiscard]] PatternId append(const PatternNode& node);
    void set(base::usize index, const PatternNode& node);
    [[nodiscard]] PatternNode take(base::usize index);
    [[nodiscard]] PatternNode operator[](base::usize index) const;
    [[nodiscard]] const PatternNode* ptr(base::usize index) const;

private:
    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_list(const std::vector<T, Allocator>& values)
    {
        return copy_ast_arena_vector(*this->arena_, values);
    }

    [[nodiscard]] base::u32 store_payload(const PatternNode& node);
    [[nodiscard]] PatternNode load(base::usize index) const;
    [[nodiscard]] PatternNode load_moved(base::usize index);
    [[nodiscard]] const PatternNode& materialized(base::usize index) const;
    void ensure_materialized_capacity(base::usize size) const;
    void invalidate_materialized(base::usize index) const;

    template <typename T>
    [[nodiscard]] base::u32 push_payload(AstArenaVector<T>& payloads, T payload)
    {
        const base::u32 index = static_cast<base::u32>(payloads.size());
        payloads.push_back(std::move(payload));
        return index;
    }

    void copy_from(const PatternNodeList& other);
    void swap(PatternNodeList& other) noexcept;

    std::unique_ptr<base::BumpAllocator> arena_;
    AstArenaVector<PatternNodeHeader> headers_;
    PatternNodePayloadArena payloads_;
    mutable std::deque<PatternNode> materialized_;
    mutable std::vector<bool> materialized_valid_;
};

} // namespace aurex::syntax
