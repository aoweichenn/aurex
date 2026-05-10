#include <gtest/support/ir_test_helpers.hpp>

namespace aurex::test {
namespace {

using namespace irtest;

} // namespace

TEST(CoreUnit, TypeTableAndIrHelpersCoverInvalidAndCompositePaths) {
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle u32 = builtin(module, BuiltinType::u32);
    const TypeHandle f64 = builtin(module, BuiltinType::f64);
    const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle ptr_i32_again = ptr(module, PointerMutability::mut, i32);
    const TypeHandle const_ptr_i32 = ptr(module, PointerMutability::const_, i32);
    const TypeHandle array_i32 = module.types.array(4, i32);
    const TypeHandle array_i32_again = module.types.array(4, i32);
    const TypeHandle record_type = module.types.named_struct("unit.Pair", "unit_Pair", false);
    const TypeHandle enum_type = module.types.named_enum("unit.Tag", "unit_Tag");
    const TypeHandle opaque = module.types.opaque_struct("unit.Opaque", "unit_Opaque");

    EXPECT_TRUE(module.types.same(ptr_i32, ptr_i32_again));
    EXPECT_FALSE(module.types.same(ptr_i32, const_ptr_i32));
    EXPECT_TRUE(module.types.same(array_i32, array_i32_again));
    EXPECT_TRUE(module.types.is_integer(i32));
    EXPECT_TRUE(module.types.is_integer(u32));
    EXPECT_TRUE(module.types.is_float(f64));
    EXPECT_TRUE(module.types.is_pointer(ptr_i32));
    EXPECT_TRUE(module.types.is_array(array_i32));
    EXPECT_TRUE(module.types.contains_array(array_i32));
    EXPECT_FALSE(module.types.is_copyable(array_i32));
    EXPECT_EQ(module.types.display_name(ptr_i32), "*mut i32");
    EXPECT_EQ(module.types.display_name(array_i32), "[4]i32");
    EXPECT_EQ(module.types.display_name(sema::invalid_type_handle), "<invalid>");
    EXPECT_EQ(module.types.c_name(sema::invalid_type_handle), "void");

    module.types.set_record_properties(record_type, true, false);
    module.types.set_enum_underlying(enum_type, u32);
    module.types.set_enum_payload_layout(enum_type, record_type, 8, 4);
    EXPECT_TRUE(module.types.contains_array(record_type));
    EXPECT_FALSE(module.types.is_copyable(record_type));
    EXPECT_EQ(module.types.get(enum_type).enum_payload_size, 8U);
    EXPECT_FALSE(module.types.is_copyable(opaque));

    RecordLayout record;
    record.type = record_type;
    record.name = "unit.Pair";
    record.symbol = "unit_Pair";
    record.fields = {RecordField {"left", i32}, RecordField {"right", ptr_i32}};
    module.records.push_back(record);

    EXPECT_NE(ir::find_record(module, record_type), nullptr);
    EXPECT_EQ(ir::find_record(module, sema::invalid_type_handle), nullptr);
    EXPECT_NE(ir::find_record_field(module, record_type, "right"), nullptr);
    EXPECT_EQ(ir::find_record_field(module, record_type, "missing"), nullptr);
    EXPECT_EQ(ir::record_field_index(record, "left"), 0U);
    EXPECT_EQ(ir::record_field_index(record, "missing"), static_cast<base::usize>(-1));

    const ValueId literal = add_value(module, integer_value(i32, "7"));
    const GlobalConstantId constant = add_global_constant(module, GlobalConstant {"seven", "unit_seven", i32, literal});
    EXPECT_TRUE(is_valid(literal));
    EXPECT_TRUE(is_valid(constant));
    EXPECT_NE(ir::find_global_constant(module, constant), nullptr);
    EXPECT_EQ(ir::find_global_constant(module, invalid_global_constant_id), nullptr);

    Function function = make_function(module, "helper", i32);
    const BlockId entry = add_block(function, "entry");
    EXPECT_TRUE(is_valid(entry));
    EXPECT_FALSE(is_valid(invalid_block_id));
}

} // namespace aurex::test
