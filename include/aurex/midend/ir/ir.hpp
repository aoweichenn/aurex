#pragma once

#include <aurex/frontend/sema/identifier.hpp>
#include <aurex/frontend/sema/type.hpp>
#include <aurex/infrastructure/base/bump_allocator.hpp>
#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/trait_object_key.hpp>

#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace aurex::ir {

using IrTextId = sema::IdentId;
inline constexpr IrTextId INVALID_IR_TEXT_ID = sema::INVALID_IDENT_ID;

template <typename T>
using IrVector = base::BumpVector<T>;

template <typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
using IrMap = base::BumpUnorderedMap<Key, Value, Hash, KeyEqual>;

struct ValueId {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = std::numeric_limits<base::u32>::max();
};

struct BlockId {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = std::numeric_limits<base::u32>::max();
};

struct FunctionId {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = std::numeric_limits<base::u32>::max();
};

struct GlobalConstantId {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = std::numeric_limits<base::u32>::max();
};

inline constexpr ValueId INVALID_VALUE_ID{ValueId::INVALID_VALUE};
inline constexpr BlockId INVALID_BLOCK_ID{BlockId::INVALID_VALUE};
inline constexpr FunctionId INVALID_FUNCTION_ID{FunctionId::INVALID_VALUE};
inline constexpr GlobalConstantId INVALID_GLOBAL_CONSTANT_ID{GlobalConstantId::INVALID_VALUE};
inline constexpr base::u32 IR_INVALID_VTABLE_SLOT = std::numeric_limits<base::u32>::max();
inline constexpr base::u32 IR_INVALID_VTABLE_SUPERTRAIT_EDGE = std::numeric_limits<base::u32>::max();

[[nodiscard]] inline constexpr bool is_valid(const ValueId id) noexcept
{
    return id.value != ValueId::INVALID_VALUE;
}

[[nodiscard]] inline constexpr bool is_valid(const BlockId id) noexcept
{
    return id.value != BlockId::INVALID_VALUE;
}

[[nodiscard]] inline constexpr bool is_valid(const FunctionId id) noexcept
{
    return id.value != FunctionId::INVALID_VALUE;
}

[[nodiscard]] inline constexpr bool is_valid(const GlobalConstantId id) noexcept
{
    return id.value != GlobalConstantId::INVALID_VALUE;
}

enum class Linkage {
    internal,
    export_c,
    extern_c,
};

enum class AbiCallConv {
    aurex,
    c,
};

enum class ValueKind {
    param,
    integer_literal,
    float_literal,
    bool_literal,
    char_literal,
    null_literal,
    string_literal,
    raw_string_literal,
    c_string_literal,
    byte_literal,
    undef,
    constant_ref,
    function_ref,
    alloca,
    load,
    store,
    unary,
    binary,
    phi,
    call,
    field_addr,
    index_addr,
    aggregate,
    slice,
    slice_data,
    slice_len,
    cast,
    size_of,
    align_of,
    str_data,
    str_byte_len,
    str_is_valid_utf8,
    str_from_utf8_checked,
    str_slice_checked,
    str_from_bytes_unchecked,
    trait_object_pack,
    trait_object_upcast,
    trait_object_data,
    trait_object_vtable,
    vtable_slot,
    drop,
    drop_if,
};

enum class UnaryOp {
    logical_not,
    numeric_negate,
    bitwise_not,
    address_of,
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

enum class CastKind {
    numeric,
    pointer,
    bcast,
    ptr_addr,
    paddr,
};

enum class CleanupAbiPolicy {
    none,
    structural_static,
    generic_marker_only,
    associated_projection_marker_only,
    opaque_marker_only,
    unknown_marker_only,
    static_custom_destructor,
};

[[nodiscard]] std::string_view cleanup_abi_policy_name(CleanupAbiPolicy policy) noexcept;

struct FieldValue {
    IrTextId name = INVALID_IR_TEXT_ID;
    ValueId value = INVALID_VALUE_ID;
};

struct PhiInput {
    BlockId predecessor = INVALID_BLOCK_ID;
    ValueId value = INVALID_VALUE_ID;
};

struct FunctionParam {
    IrTextId name = INVALID_IR_TEXT_ID;
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
};

struct GlobalConstant {
    IrTextId name = INVALID_IR_TEXT_ID;
    IrTextId symbol = INVALID_IR_TEXT_ID;
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
    ValueId initializer = INVALID_VALUE_ID;
};

struct RecordField {
    IrTextId name = INVALID_IR_TEXT_ID;
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
};

struct RecordLayout {
    RecordLayout();
    explicit RecordLayout(base::BumpAllocator& arena);

    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
    IrTextId name = INVALID_IR_TEXT_ID;
    IrTextId symbol = INVALID_IR_TEXT_ID;
    bool is_opaque = false;
    IrVector<RecordField> fields;
};

struct TraitObjectVTableMethodSlot {
    base::u32 slot = IR_INVALID_VTABLE_SLOT;
    FunctionId function = INVALID_FUNCTION_ID;
    sema::TypeHandle function_type = sema::INVALID_TYPE_HANDLE;
    sema::TypeHandle receiver_type = sema::INVALID_TYPE_HANDLE;
    sema::TypeHandle return_type = sema::INVALID_TYPE_HANDLE;
    IrTextId method_name = INVALID_IR_TEXT_ID;
};

struct TraitObjectVTableSupertraitEdge {
    base::u32 edge_index = IR_INVALID_VTABLE_SUPERTRAIT_EDGE;
    query::TraitObjectUpcastCoercionKey upcast_key;
    query::StableFingerprint128 edge_fingerprint;
    query::VTableLayoutKey source_layout;
    query::VTableLayoutKey target_layout;
    sema::TypeHandle source_reference_type = sema::INVALID_TYPE_HANDLE;
    sema::TypeHandle target_reference_type = sema::INVALID_TYPE_HANDLE;
    sema::TypeHandle source_object_type = sema::INVALID_TYPE_HANDLE;
    sema::TypeHandle target_object_type = sema::INVALID_TYPE_HANDLE;
    query::TraitObjectBorrowKindKey borrow_kind = query::TraitObjectBorrowKindKey::shared;
};

struct TraitObjectVTableLayout {
    TraitObjectVTableLayout();
    explicit TraitObjectVTableLayout(base::BumpAllocator& arena);

    query::VTableLayoutKey layout_key;
    sema::TypeHandle concrete_type = sema::INVALID_TYPE_HANDLE;
    sema::TypeHandle object_type = sema::INVALID_TYPE_HANDLE;
    IrTextId symbol = INVALID_IR_TEXT_ID;
    IrVector<TraitObjectVTableMethodSlot> method_slots;
    IrVector<TraitObjectVTableSupertraitEdge> supertrait_edges;
};

struct Value {
    Value();
    explicit Value(base::BumpAllocator& arena);

    ValueKind kind = ValueKind::integer_literal;
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
    IrTextId name = INVALID_IR_TEXT_ID;
    IrTextId text = INVALID_IR_TEXT_ID;
    FunctionId call_target = INVALID_FUNCTION_ID;
    ValueId lhs = INVALID_VALUE_ID;
    ValueId rhs = INVALID_VALUE_ID;
    ValueId object = INVALID_VALUE_ID;
    ValueId index = INVALID_VALUE_ID;
    IrVector<ValueId> args;
    IrVector<FieldValue> fields;
    IrVector<PhiInput> incoming;
    GlobalConstantId constant = INVALID_GLOBAL_CONSTANT_ID;
    IrVector<ValueId> elements;
    UnaryOp unary_op = UnaryOp::logical_not;
    BinaryOp binary_op = BinaryOp::add;
    CastKind cast_kind = CastKind::numeric;
    sema::TypeHandle target_type = sema::INVALID_TYPE_HANDLE;
    CleanupAbiPolicy cleanup_policy = CleanupAbiPolicy::none;
    query::VTableLayoutKey vtable_layout;
    query::VTableLayoutKey target_vtable_layout;
    query::TraitObjectUpcastCoercionKey upcast_key;
    base::u32 vtable_slot = IR_INVALID_VTABLE_SLOT;
    base::u32 vtable_supertrait_edge = IR_INVALID_VTABLE_SUPERTRAIT_EDGE;
};

enum class TerminatorKind {
    none,
    branch,
    cond_branch,
    return_,
};

struct Terminator {
    TerminatorKind kind = TerminatorKind::none;
    ValueId condition = INVALID_VALUE_ID;
    ValueId value = INVALID_VALUE_ID;
    BlockId target = INVALID_BLOCK_ID;
    BlockId then_target = INVALID_BLOCK_ID;
    BlockId else_target = INVALID_BLOCK_ID;
};

struct BasicBlock {
    BasicBlock();
    explicit BasicBlock(base::BumpAllocator& arena);

    IrTextId name = INVALID_IR_TEXT_ID;
    IrVector<ValueId> values;
    Terminator terminator;
};

struct Function {
    Function();
    explicit Function(base::BumpAllocator& arena);

    IrTextId name = INVALID_IR_TEXT_ID;
    IrTextId symbol = INVALID_IR_TEXT_ID;
    Linkage linkage = Linkage::internal;
    AbiCallConv call_conv = AbiCallConv::aurex;
    bool is_entry = false;
    bool is_unsafe = false;
    bool is_variadic = false;
    sema::TypeHandle return_type = sema::INVALID_TYPE_HANDLE;
    IrVector<FunctionParam> signature_params;
    IrVector<ValueId> param_values;
    IrVector<BasicBlock> blocks;
};

struct Module {
    using RecordIndexMap = IrMap<base::u32, base::u32>;

private:
    std::unique_ptr<base::BumpAllocator> arena_;

public:
    Module();
    Module(const Module& other);
    Module& operator=(const Module& other);
    Module(Module&& other) noexcept;
    Module& operator=(Module&& other) noexcept;
    ~Module() = default;

    template <typename T>
    [[nodiscard]] IrVector<T> make_vector()
    {
        return IrVector<T>{base::BumpAllocatorAdapter<T>{*this->arena_}};
    }

    template <typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
    [[nodiscard]] IrMap<Key, Value, Hash, KeyEqual> make_map(Hash hash = Hash{}, KeyEqual equal = KeyEqual{})
    {
        return IrMap<Key, Value, Hash, KeyEqual>(0, std::move(hash), std::move(equal),
            base::BumpAllocatorAdapter<std::pair<const Key, Value>>{*this->arena_});
    }

    [[nodiscard]] IrTextId intern(std::string_view text);
    [[nodiscard]] IrTextId find_text(std::string_view text) const noexcept;
    [[nodiscard]] std::string_view text(IrTextId id) const noexcept;
    [[nodiscard]] bool has_text(IrTextId id) const noexcept;

    [[nodiscard]] Value make_value();
    [[nodiscard]] Function make_function();
    [[nodiscard]] BasicBlock make_block();
    [[nodiscard]] RecordLayout make_record_layout();
    [[nodiscard]] Value clone_value(const Value& other);
    [[nodiscard]] Function clone_function(const Function& other);
    [[nodiscard]] BasicBlock clone_block(const BasicBlock& other);
    [[nodiscard]] RecordLayout clone_record_layout(const RecordLayout& other);
    [[nodiscard]] TraitObjectVTableLayout make_trait_object_vtable_layout();
    [[nodiscard]] TraitObjectVTableLayout clone_trait_object_vtable_layout(const TraitObjectVTableLayout& other);

    template <typename T>
    [[nodiscard]] IrVector<T> copy_vector(const std::span<const T> source_values)
    {
        IrVector<T> copy = this->make_vector<T>();
        copy.reserve(source_values.size());
        copy.insert(copy.end(), source_values.begin(), source_values.end());
        return copy;
    }

    void reserve(
        base::usize value_count, base::usize function_count, base::usize record_count, base::usize constant_count);

    sema::TypeTable types;
    sema::IdentifierInterner identifiers;
    IrVector<GlobalConstant> constants;
    IrVector<RecordLayout> records;
    IrVector<TraitObjectVTableLayout> trait_object_vtables;
    IrVector<Value> values;
    IrVector<Function> functions;
    RecordIndexMap record_indices;

private:
    void swap(Module& other) noexcept;
    void copy_from(const Module& other);
    void ensure_arena();
};

[[nodiscard]] ValueId add_value(Module& module, const Value& value);
[[nodiscard]] GlobalConstantId add_global_constant(Module& module, const GlobalConstant& constant);
[[nodiscard]] FunctionId add_function(Module& module, const Function& function);
[[nodiscard]] base::u32 add_record(Module& module, const RecordLayout& record);
[[nodiscard]] BlockId add_block(Module& module, Function& function, std::string_view name);
[[nodiscard]] const GlobalConstant* find_global_constant(const Module& module, GlobalConstantId id) noexcept;
[[nodiscard]] const RecordLayout* find_record(const Module& module, sema::TypeHandle type) noexcept;
[[nodiscard]] const RecordField* find_record_field(
    Module& module, sema::TypeHandle type, std::string_view name) noexcept;
[[nodiscard]] const RecordField* find_record_field(const Module& module, sema::TypeHandle type, IrTextId name) noexcept;
[[nodiscard]] base::usize record_field_index(const RecordLayout& record, IrTextId name) noexcept;

} // namespace aurex::ir
