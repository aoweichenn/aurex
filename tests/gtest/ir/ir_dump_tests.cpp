#include <aurex/ir/ir_dump.hpp>
#include <gtest/support/ir_test_helpers.hpp>

#include <utility>

namespace aurex::test {
namespace {

using namespace irtest;

} // namespace

TEST(CoreUnit, IrDumpCoversFallbackLabelsAndOperatorNames) {
    Module module;
    const TypeHandle void_type = builtin(module, BuiltinType::void_);
    const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
    const TypeHandle u8 = builtin(module, BuiltinType::u8);
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle usize = builtin(module, BuiltinType::usize);
    const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle const_u8_ptr = ptr(module, PointerMutability::const_, u8);
    const TypeHandle array_i32 = module.types.array(2, i32);
    const TypeHandle slice_u8 = module.types.slice(PointerMutability::const_, u8);
    const TypeHandle slice_i32 = module.types.slice(PointerMutability::mut, i32);
    const TypeHandle callback_type = module.types.function(sema::FunctionCallConv::aurex, false, {i32}, i32);
    const TypeHandle str_type = builtin(module, BuiltinType::str);
    const TypeHandle record_type = module.types.named_struct("dump.Record", "dump_Record", false);
    const TypeHandle opaque_type = module.types.opaque_struct("dump.Opaque", "dump_Opaque");
    RecordLayout record = record_layout(module, record_type, "dump.Record", "dump_Record", false);
    record.fields.push_back(record_field(module, "value", i32));
    append_record(module, record);
    append_record(module, record_layout(module, opaque_type, "dump.Opaque", "dump_Opaque", true));

    static_cast<void>(add_global_constant(module, GlobalConstant {"broken", "dump_broken", i32, INVALID_VALUE_ID}));

    Function exported = make_function(module, "exported", void_type, Linkage::export_c, AbiCallConv::c);
    set_symbol(module, exported, "dump_exported");
    append_function(module, exported);

    Function function = make_function(module, "dump_ops", i32);
    FunctionBuilder builder {module, function};
    const ValueId lhs = builder.add(integer_value(module, i32, "7"));
    const ValueId rhs = builder.add(integer_value(module, i32, "3"));
    const ValueId len = builder.add(integer_value(module, usize, "2"));
    const ValueId flag = builder.add(bool_value(module, true));

    Value pointer = module.make_value();
    pointer.kind = ValueKind::null_literal;
    pointer.type = ptr_i32;
    const ValueId ptr_value = builder.add(pointer);

    Value string_value = module.make_value();
    string_value.kind = ValueKind::string_literal;
    string_value.type = str_type;
    set_text(module, string_value, "\"dump\"");
    const ValueId text = builder.add(string_value);

    Value str_data = module.make_value();
    str_data.kind = ValueKind::str_data;
    str_data.type = const_u8_ptr;
    str_data.object = text;
    const ValueId string_data = builder.add(str_data);
    Value str_byte_len = module.make_value();
    str_byte_len.kind = ValueKind::str_byte_len;
    str_byte_len.type = usize;
    str_byte_len.object = text;
    const ValueId string_byte_len = builder.add(str_byte_len);
    Value from_bytes = module.make_value();
    from_bytes.kind = ValueKind::str_from_bytes_unchecked;
    from_bytes.type = str_type;
    from_bytes.args = {string_data, string_byte_len};
    const ValueId rebuilt_string = builder.add(from_bytes);
    Value byte_slice = module.make_value();
    byte_slice.kind = ValueKind::slice;
    byte_slice.type = slice_u8;
    byte_slice.lhs = string_data;
    byte_slice.rhs = string_byte_len;
    const ValueId utf8_bytes = builder.add(byte_slice);
    Value str_valid = module.make_value();
    str_valid.kind = ValueKind::str_is_valid_utf8;
    str_valid.type = bool_type;
    str_valid.object = utf8_bytes;
    const ValueId utf8_valid = builder.add(str_valid);
    Value checked_string = module.make_value();
    checked_string.kind = ValueKind::str_from_utf8_checked;
    checked_string.type = str_type;
    checked_string.object = utf8_bytes;
    const ValueId utf8_checked = builder.add(checked_string);
    Value checked_slice = module.make_value();
    checked_slice.kind = ValueKind::str_slice_checked;
    checked_slice.type = str_type;
    checked_slice.object = text;
    checked_slice.lhs = len;
    checked_slice.rhs = string_byte_len;
    const ValueId string_slice = builder.add(checked_slice);

    Value missing_constant = module.make_value();
    missing_constant.kind = ValueKind::constant_ref;
    missing_constant.type = i32;
    set_name(module, missing_constant, "fallback_constant");
    missing_constant.constant = INVALID_GLOBAL_CONSTANT_ID;
    const ValueId fallback_constant = builder.add(missing_constant);

    Value function_ref = module.make_value();
    function_ref.kind = ValueKind::function_ref;
    function_ref.type = callback_type;
    set_name(module, function_ref, "dump_callback");
    const ValueId callback = builder.add(function_ref);
    Value indirect_call = module.make_value();
    indirect_call.kind = ValueKind::call;
    indirect_call.type = i32;
    indirect_call.object = callback;
    indirect_call.args = {lhs};
    const ValueId callback_call = builder.add(indirect_call);

    Value array_aggregate = module.make_value();
    array_aggregate.kind = ValueKind::aggregate;
    array_aggregate.type = array_i32;
    array_aggregate.elements = {lhs, rhs};
    const ValueId array_value = builder.add(array_aggregate);
    Value slice = module.make_value();
    slice.kind = ValueKind::slice;
    slice.type = slice_i32;
    slice.lhs = ptr_value;
    slice.rhs = len;
    const ValueId slice_value = builder.add(slice);
    Value slice_data = module.make_value();
    slice_data.kind = ValueKind::slice_data;
    slice_data.type = ptr_i32;
    slice_data.object = slice_value;
    const ValueId slice_data_value = builder.add(slice_data);
    Value slice_len = module.make_value();
    slice_len.kind = ValueKind::slice_len;
    slice_len.type = usize;
    slice_len.object = slice_value;
    const ValueId slice_len_value = builder.add(slice_len);

    std::vector<ValueId> values {
        lhs,
        rhs,
        len,
        flag,
        ptr_value,
        text,
        string_data,
        string_byte_len,
        rebuilt_string,
        utf8_bytes,
        utf8_valid,
        utf8_checked,
        string_slice,
        fallback_constant,
        callback,
        callback_call,
        array_value,
        slice_value,
        slice_data_value,
        slice_len_value,
    };
    for (const UnaryOp op : {UnaryOp::bitwise_not, UnaryOp::address_of, UnaryOp::dereference}) {
        Value unary = module.make_value();
        unary.kind = ValueKind::unary;
        unary.type = op == UnaryOp::bitwise_not ? i32 : ptr_i32;
        unary.unary_op = op;
        unary.lhs = op == UnaryOp::bitwise_not ? lhs : ptr_value;
        values.push_back(builder.add(unary));
    }
    for (const BinaryOp op : {
             BinaryOp::mul,
             BinaryOp::div,
             BinaryOp::mod,
             BinaryOp::shl,
             BinaryOp::shr,
             BinaryOp::less_equal,
             BinaryOp::greater_equal,
             BinaryOp::bit_and,
             BinaryOp::bit_xor,
             BinaryOp::bit_or,
             BinaryOp::logical_and,
         }) {
        Value binary = module.make_value();
        binary.kind = ValueKind::binary;
        const bool is_comparison = op == BinaryOp::less_equal || op == BinaryOp::greater_equal;
        const bool is_logical = op == BinaryOp::logical_and;
        binary.type = is_logical || is_comparison ? bool_type : i32;
        binary.binary_op = op;
        binary.lhs = is_logical ? flag : lhs;
        binary.rhs = is_logical ? flag : rhs;
        values.push_back(builder.add(binary));
    }

    Value cast = module.make_value();
    cast.kind = ValueKind::cast;
    cast.type = ptr_i32;
    cast.target_type = ptr_i32;
    cast.cast_kind = CastKind::bcast;
    cast.lhs = ptr_value;
    values.push_back(builder.add(cast));
    Value paddr = cast;
    paddr.cast_kind = CastKind::paddr;
    paddr.lhs = lhs;
    values.push_back(builder.add(paddr));
    Value ptraddr = cast;
    ptraddr.type = usize;
    ptraddr.target_type = usize;
    ptraddr.cast_kind = CastKind::ptr_addr;
    ptraddr.lhs = ptr_value;
    values.push_back(builder.add(ptraddr));

    const BlockId entry = builder.block("entry");
    const BlockId dead = builder.block("dead");
    assign_ir_vector(function.blocks[entry.value].values, values);
    function.blocks[entry.value].terminator.kind = TerminatorKind::none;
    function.blocks[dead.value].terminator.kind = TerminatorKind::branch;
    function.blocks[dead.value].terminator.target = BlockId {99};
    append_function(module, function);

    const std::string dump = ir::dump_module(module);
    expect_contains_all(dump, {
        "const broken @dump_broken: i32 = %invalid",
        "linkage(export_c)",
        "abi(c)",
        "record dump.Opaque @dump_Opaque opaque",
        "null",
        "string \"dump\"",
        "strptr",
        "strblen",
        "strraw",
        "strvalid",
        "strfromutf8",
        "strslice.checked",
        "const_ref @fallback_constant",
        "function_ref @dump_callback",
        "call %",
        "aggregate [",
        "slice ",
        "slice_data",
        "slice_len",
        "bitnot",
        "addr_of",
        "deref",
        "mul",
        "div",
        "mod",
        "shl",
        "shr",
        "le",
        "ge",
        "bitand",
        "bitxor",
        "bitor",
        "and",
        "bitcast",
        "ptrat",
        "ptraddr",
        "unreachable",
        "br ^invalid",
    });
}

} // namespace aurex::test
