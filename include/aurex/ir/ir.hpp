#pragma once

#include <aurex/base/bump_allocator.hpp>
#include <aurex/base/integer.hpp>
#include <aurex/sema/identifier.hpp>
#include <aurex/sema/type.hpp>

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

template <
    typename Key,
    typename Value,
    typename Hash = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>>
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

inline constexpr ValueId INVALID_VALUE_ID {ValueId::INVALID_VALUE};
inline constexpr BlockId INVALID_BLOCK_ID {BlockId::INVALID_VALUE};
inline constexpr FunctionId INVALID_FUNCTION_ID {FunctionId::INVALID_VALUE};
inline constexpr GlobalConstantId INVALID_GLOBAL_CONSTANT_ID {GlobalConstantId::INVALID_VALUE};

[[nodiscard]] inline constexpr bool is_valid(const ValueId id) noexcept {
    return id.value != ValueId::INVALID_VALUE;
}

[[nodiscard]] inline constexpr bool is_valid(const BlockId id) noexcept {
    return id.value != BlockId::INVALID_VALUE;
}

[[nodiscard]] inline constexpr bool is_valid(const FunctionId id) noexcept {
    return id.value != FunctionId::INVALID_VALUE;
}

[[nodiscard]] inline constexpr bool is_valid(const GlobalConstantId id) noexcept {
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
    [[nodiscard]] IrVector<T> make_vector() {
        return IrVector<T> {base::BumpAllocatorAdapter<T> {*this->arena_}};
    }

    template <
        typename Key,
        typename Value,
        typename Hash = std::hash<Key>,
        typename KeyEqual = std::equal_to<Key>>
    [[nodiscard]] IrMap<Key, Value, Hash, KeyEqual> make_map(
        Hash hash = Hash {},
        KeyEqual equal = KeyEqual {}
    ) {
        return IrMap<Key, Value, Hash, KeyEqual>(
            0,
            std::move(hash),
            std::move(equal),
            base::BumpAllocatorAdapter<std::pair<const Key, Value>> {*this->arena_}
        );
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

    template <typename T>
    [[nodiscard]] IrVector<T> copy_vector(const std::span<const T> values) {
        IrVector<T> copy = this->make_vector<T>();
        copy.reserve(values.size());
        copy.insert(copy.end(), values.begin(), values.end());
        return copy;
    }

    void reserve(
        base::usize value_count,
        base::usize function_count,
        base::usize record_count,
        base::usize constant_count
    );

    sema::TypeTable types;
    sema::IdentifierInterner identifiers;
    IrVector<GlobalConstant> constants;
    IrVector<RecordLayout> records;
    IrVector<Value> values;
    IrVector<Function> functions;
    RecordIndexMap record_indices;

private:
    void swap(Module& other) noexcept;
    void copy_from(const Module& other);
    void ensure_arena();
};

[[nodiscard]] ValueId add_value(Module& module, Value value);
[[nodiscard]] GlobalConstantId add_global_constant(Module& module, GlobalConstant constant);
[[nodiscard]] FunctionId add_function(Module& module, Function function);
[[nodiscard]] base::u32 add_record(Module& module, RecordLayout record);
[[nodiscard]] BlockId add_block(Module& module, Function& function, std::string_view name);
[[nodiscard]] const GlobalConstant* find_global_constant(const Module& module, GlobalConstantId id) noexcept;
[[nodiscard]] const RecordLayout* find_record(const Module& module, sema::TypeHandle type) noexcept;
[[nodiscard]] const RecordField* find_record_field(Module& module, sema::TypeHandle type, std::string_view name) noexcept;
[[nodiscard]] const RecordField* find_record_field(const Module& module, sema::TypeHandle type, IrTextId name) noexcept;
[[nodiscard]] base::usize record_field_index(const RecordLayout& record, IrTextId name) noexcept;

} // namespace aurex::ir
