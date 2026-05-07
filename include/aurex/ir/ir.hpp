#pragma once

#include "aurex/base/integer.hpp"
#include "aurex/sema/type.hpp"

#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace aurex::ir {

struct ValueId {
    base::u32 value = invalid_value;
    static constexpr base::u32 invalid_value = std::numeric_limits<base::u32>::max();
};

struct BlockId {
    base::u32 value = invalid_value;
    static constexpr base::u32 invalid_value = std::numeric_limits<base::u32>::max();
};

struct FunctionId {
    base::u32 value = invalid_value;
    static constexpr base::u32 invalid_value = std::numeric_limits<base::u32>::max();
};

struct GlobalConstantId {
    base::u32 value = invalid_value;
    static constexpr base::u32 invalid_value = std::numeric_limits<base::u32>::max();
};

inline constexpr ValueId invalid_value_id {ValueId::invalid_value};
inline constexpr BlockId invalid_block_id {BlockId::invalid_value};
inline constexpr FunctionId invalid_function_id {FunctionId::invalid_value};
inline constexpr GlobalConstantId invalid_global_constant_id {GlobalConstantId::invalid_value};

[[nodiscard]] inline constexpr bool is_valid(const ValueId id) noexcept {
    return id.value != ValueId::invalid_value;
}

[[nodiscard]] inline constexpr bool is_valid(const BlockId id) noexcept {
    return id.value != BlockId::invalid_value;
}

[[nodiscard]] inline constexpr bool is_valid(const FunctionId id) noexcept {
    return id.value != FunctionId::invalid_value;
}

[[nodiscard]] inline constexpr bool is_valid(const GlobalConstantId id) noexcept {
    return id.value != GlobalConstantId::invalid_value;
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
    bool_literal,
    null_literal,
    string_literal,
    c_string_literal,
    byte_literal,
    undef,
    constant_ref,
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
    cast,
    size_of,
    align_of,
    str_data,
    str_byte_len,
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
    bitcast,
    ptr_addr,
    ptr_from_addr,
};

struct FieldValue {
    std::string name;
    ValueId value = invalid_value_id;
};

struct PhiInput {
    BlockId predecessor = invalid_block_id;
    ValueId value = invalid_value_id;
};

struct FunctionParam {
    std::string name;
    sema::TypeHandle type = sema::invalid_type_handle;
};

struct GlobalConstant {
    std::string name;
    std::string symbol;
    sema::TypeHandle type = sema::invalid_type_handle;
    ValueId initializer = invalid_value_id;
};

struct RecordField {
    std::string name;
    sema::TypeHandle type = sema::invalid_type_handle;
};

struct RecordLayout {
    sema::TypeHandle type = sema::invalid_type_handle;
    std::string name;
    std::string symbol;
    bool is_opaque = false;
    std::vector<RecordField> fields;
};

struct Value {
    ValueKind kind = ValueKind::integer_literal;
    sema::TypeHandle type = sema::invalid_type_handle;
    std::string name;
    std::string text;
    FunctionId call_target = invalid_function_id;
    ValueId lhs = invalid_value_id;
    ValueId rhs = invalid_value_id;
    ValueId object = invalid_value_id;
    ValueId index = invalid_value_id;
    std::vector<ValueId> args;
    std::vector<FieldValue> fields;
    std::vector<PhiInput> incoming;
    GlobalConstantId constant = invalid_global_constant_id;
    UnaryOp unary_op = UnaryOp::logical_not;
    BinaryOp binary_op = BinaryOp::add;
    CastKind cast_kind = CastKind::numeric;
    sema::TypeHandle target_type = sema::invalid_type_handle;
};

enum class TerminatorKind {
    none,
    branch,
    cond_branch,
    return_,
};

struct Terminator {
    TerminatorKind kind = TerminatorKind::none;
    ValueId condition = invalid_value_id;
    ValueId value = invalid_value_id;
    BlockId target = invalid_block_id;
    BlockId then_target = invalid_block_id;
    BlockId else_target = invalid_block_id;
};

struct BasicBlock {
    std::string name;
    std::vector<ValueId> values;
    Terminator terminator;
};

struct Function {
    std::string name;
    std::string symbol;
    Linkage linkage = Linkage::internal;
    AbiCallConv call_conv = AbiCallConv::aurex;
    bool is_entry = false;
    bool is_variadic = false;
    sema::TypeHandle return_type = sema::invalid_type_handle;
    std::vector<FunctionParam> signature_params;
    std::vector<ValueId> param_values;
    std::vector<BasicBlock> blocks;
};

struct Module {
    sema::TypeTable types;
    std::vector<GlobalConstant> constants;
    std::vector<RecordLayout> records;
    std::vector<Value> values;
    std::vector<Function> functions;
    std::unordered_map<base::u32, base::u32> record_indices;
};

[[nodiscard]] ValueId add_value(Module& module, Value value);
[[nodiscard]] GlobalConstantId add_global_constant(Module& module, GlobalConstant constant);
[[nodiscard]] BlockId add_block(Function& function, std::string name);
[[nodiscard]] const GlobalConstant* find_global_constant(const Module& module, GlobalConstantId id) noexcept;
[[nodiscard]] const RecordLayout* find_record(const Module& module, sema::TypeHandle type) noexcept;
[[nodiscard]] const RecordField* find_record_field(const Module& module, sema::TypeHandle type, const std::string& name) noexcept;
[[nodiscard]] base::usize record_field_index(const RecordLayout& record, const std::string& name) noexcept;

} // namespace aurex::ir
