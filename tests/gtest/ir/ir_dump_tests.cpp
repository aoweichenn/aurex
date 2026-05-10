#include <aurex/ir/ir_dump.hpp>
#include <gtest/support/ir_test_helpers.hpp>

namespace aurex::test {
namespace {

using namespace irtest;

} // namespace

TEST(CoreUnit, IrDumpCoversFallbackLabelsAndOperatorNames) {
    Module module;
    const TypeHandle void_type = builtin(module, BuiltinType::void_);
    const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle str_type = builtin(module, BuiltinType::str);
    const TypeHandle record_type = module.types.named_struct("dump.Record", "dump_Record", false);
    module.records.push_back(RecordLayout {
        record_type,
        "dump.Record",
        "dump_Record",
        false,
        {RecordField {"value", i32}},
    });

    module.constants.push_back(GlobalConstant {"broken", "dump_broken", i32, INVALID_VALUE_ID});

    Function exported = make_function(module, "exported", void_type, Linkage::export_c, AbiCallConv::c);
    exported.symbol = "dump_exported";
    module.functions.push_back(exported);

    Function function = make_function(module, "dump_ops", i32);
    FunctionBuilder builder {module, function};
    const ValueId lhs = builder.add(integer_value(i32, "7"));
    const ValueId rhs = builder.add(integer_value(i32, "3"));
    const ValueId flag = builder.add(bool_value(module, true));

    Value pointer;
    pointer.kind = ValueKind::null_literal;
    pointer.type = ptr_i32;
    const ValueId ptr_value = builder.add(pointer);

    Value string_value;
    string_value.kind = ValueKind::string_literal;
    string_value.type = str_type;
    string_value.text = "\"dump\"";
    const ValueId text = builder.add(string_value);

    Value missing_constant;
    missing_constant.kind = ValueKind::constant_ref;
    missing_constant.type = i32;
    missing_constant.name = "fallback_constant";
    missing_constant.constant = INVALID_GLOBAL_CONSTANT_ID;
    const ValueId fallback_constant = builder.add(missing_constant);

    std::vector<ValueId> values {lhs, rhs, flag, ptr_value, text, fallback_constant};
    for (const UnaryOp op : {UnaryOp::bitwise_not, UnaryOp::address_of, UnaryOp::dereference}) {
        Value unary;
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
             BinaryOp::bit_and,
             BinaryOp::bit_xor,
             BinaryOp::bit_or,
             BinaryOp::logical_and,
         }) {
        Value binary;
        binary.kind = ValueKind::binary;
        binary.type = op == BinaryOp::logical_and ? bool_type : i32;
        binary.binary_op = op;
        binary.lhs = op == BinaryOp::logical_and ? flag : lhs;
        binary.rhs = op == BinaryOp::logical_and ? flag : rhs;
        values.push_back(builder.add(binary));
    }

    Value cast;
    cast.kind = ValueKind::cast;
    cast.type = ptr_i32;
    cast.target_type = ptr_i32;
    cast.cast_kind = CastKind::bitcast;
    cast.lhs = ptr_value;
    values.push_back(builder.add(cast));

    const BlockId entry = builder.block("entry");
    const BlockId dead = builder.block("dead");
    function.blocks[entry.value].values = values;
    function.blocks[entry.value].terminator.kind = TerminatorKind::none;
    function.blocks[dead.value].terminator.kind = TerminatorKind::branch;
    function.blocks[dead.value].terminator.target = BlockId {99};
    module.functions.push_back(function);

    const std::string dump = ir::dump_module(module);
    expect_contains_all(dump, {
        "const broken @dump_broken: i32 = %invalid",
        "linkage(export_c)",
        "abi(c)",
        "null",
        "string \"dump\"",
        "const_ref @fallback_constant",
        "bitnot",
        "addr_of",
        "deref",
        "mul",
        "div",
        "mod",
        "shl",
        "shr",
        "bitand",
        "bitxor",
        "bitor",
        "and",
        "bit_cast",
        "unreachable",
        "br ^invalid",
    });
}

} // namespace aurex::test
