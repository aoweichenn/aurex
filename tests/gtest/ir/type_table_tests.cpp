#include <array>
#include <vector>

#include <aurex/ir/enum_layout.hpp>
#include <gtest/support/ir_test_helpers.hpp>

namespace aurex::test {
namespace {

using namespace irtest;

constexpr base::u64 TYPE_TABLE_TEST_ARRAY_COUNT = 4;
constexpr base::usize TYPE_TABLE_TEST_BUILTIN_COUNT = 16;
constexpr base::u64 TYPE_TABLE_TEST_ENUM_PAYLOAD_SIZE = 8;
constexpr base::u64 TYPE_TABLE_TEST_ENUM_PAYLOAD_ALIGNMENT = 4;
constexpr base::usize TYPE_TABLE_TEST_ENUM_LAYOUT_FIELD_COUNT = 2;

} // namespace

TEST(CoreUnit, TypeTableAndIrHelpersCoverInvalidAndCompositePaths) {
    Module module;
    const TypeHandle void_type = builtin(module, BuiltinType::void_);
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle u32 = builtin(module, BuiltinType::u32);
    const TypeHandle f64 = builtin(module, BuiltinType::f64);
    const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle ptr_i32_again = ptr(module, PointerMutability::mut, i32);
    const TypeHandle const_ptr_i32 = ptr(module, PointerMutability::const_, i32);
    const TypeHandle array_i32 = module.types.array(TYPE_TABLE_TEST_ARRAY_COUNT, i32);
    const TypeHandle array_i32_again = module.types.array(TYPE_TABLE_TEST_ARRAY_COUNT, i32);
    const TypeHandle array_ptr_i32 = module.types.array(TYPE_TABLE_TEST_ARRAY_COUNT, ptr_i32);
    const TypeHandle ptr_array_i32 = ptr(module, PointerMutability::mut, array_i32);
    const TypeHandle const_slice_i32 = module.types.slice(PointerMutability::const_, i32);
    const TypeHandle const_slice_i32_again = module.types.slice(PointerMutability::const_, i32);
    const TypeHandle mut_slice_i32 = module.types.slice(PointerMutability::mut, i32);
    const TypeHandle function_i32_u32 = module.types.function(
        sema::FunctionCallConv::aurex,
        false,
        std::vector<TypeHandle> {i32, u32},
        i32
    );
    const TypeHandle function_i32_u32_again = module.types.function(
        sema::FunctionCallConv::aurex,
        false,
        std::vector<TypeHandle> {i32, u32},
        i32
    );
    const TypeHandle extern_variadic_function = module.types.function(
        sema::FunctionCallConv::c,
        true,
        std::vector<TypeHandle> {const_ptr_i32},
        i32
    );
    const TypeHandle function_void = module.types.function(
        sema::FunctionCallConv::aurex,
        false,
        std::vector<TypeHandle> {},
        void_type
    );
    const TypeHandle extern_variadic_void = module.types.function(
        sema::FunctionCallConv::c,
        true,
        std::vector<TypeHandle> {},
        void_type
    );
    const std::string array_display = "[" + std::to_string(TYPE_TABLE_TEST_ARRAY_COUNT) + "]";
    const TypeHandle record_type = module.types.named_struct("unit.Pair", "unit_Pair", false);
    const TypeHandle enum_type = module.types.named_enum("unit.Tag", "unit_Tag");
    const TypeHandle opaque = module.types.opaque_struct("unit.Opaque", "unit_Opaque");

    EXPECT_TRUE(module.types.same(ptr_i32, ptr_i32_again));
    EXPECT_FALSE(module.types.same(ptr_i32, const_ptr_i32));
    EXPECT_TRUE(module.types.same(array_i32, array_i32_again));
    EXPECT_TRUE(module.types.same(const_slice_i32, const_slice_i32_again));
    EXPECT_FALSE(module.types.same(const_slice_i32, mut_slice_i32));
    EXPECT_TRUE(module.types.same(function_i32_u32, function_i32_u32_again));
    EXPECT_FALSE(module.types.same(function_i32_u32, extern_variadic_function));
    EXPECT_TRUE(module.types.is_integer(i32));
    EXPECT_TRUE(module.types.is_integer(u32));
    EXPECT_TRUE(module.types.is_float(f64));
    EXPECT_TRUE(module.types.is_pointer(ptr_i32));
    EXPECT_TRUE(module.types.is_array(array_i32));
    EXPECT_TRUE(module.types.is_slice(const_slice_i32));
    EXPECT_TRUE(module.types.is_slice(mut_slice_i32));
    EXPECT_TRUE(module.types.is_function(function_i32_u32));
    EXPECT_TRUE(module.types.contains_array(array_i32));
    EXPECT_FALSE(module.types.contains_array(const_slice_i32));
    EXPECT_EQ(module.types.display_name(ptr_i32), "*mut i32");
    EXPECT_EQ(module.types.display_name(array_i32), array_display + "i32");
    EXPECT_EQ(module.types.display_name(array_ptr_i32), array_display + "*mut i32");
    EXPECT_EQ(module.types.display_name(ptr_array_i32), std::string("*mut ") + array_display + "i32");
    EXPECT_EQ(module.types.display_name(const_slice_i32), "[]const i32");
    EXPECT_EQ(module.types.display_name(mut_slice_i32), "[]mut i32");
    EXPECT_EQ(module.types.display_name(function_i32_u32), "fn(i32, u32) -> i32");
    EXPECT_EQ(module.types.display_name(extern_variadic_function), "extern c fn(*const i32, ...) -> i32");
    EXPECT_EQ(module.types.display_name(function_void), "fn() -> void");
    EXPECT_EQ(module.types.display_name(extern_variadic_void), "extern c fn(...) -> void");
    EXPECT_EQ(module.types.c_name(function_void), "fn() -> void");
    EXPECT_EQ(module.types.display_name(record_type), "unit.Pair");
    EXPECT_EQ(module.types.display_name(enum_type), "unit.Tag");
    EXPECT_EQ(module.types.display_name(opaque), "unit.Opaque");
    EXPECT_EQ(module.types.display_name(sema::INVALID_TYPE_HANDLE), "<invalid>");
    EXPECT_EQ(module.types.c_name(sema::INVALID_TYPE_HANDLE), "void");
    EXPECT_EQ(module.types.c_name(record_type), "unit_Pair");
    EXPECT_EQ(module.types.c_name(enum_type), "unit_Tag");
    EXPECT_EQ(module.types.c_name(opaque), "unit_Opaque");

    module.types.set_record_contains_array(record_type, true);
    module.types.set_enum_underlying(enum_type, u32);
    module.types.set_enum_payload_layout(
        enum_type,
        record_type,
        TYPE_TABLE_TEST_ENUM_PAYLOAD_SIZE,
        TYPE_TABLE_TEST_ENUM_PAYLOAD_ALIGNMENT
    );
    EXPECT_TRUE(module.types.contains_array(record_type));
    EXPECT_EQ(module.types.get(enum_type).enum_payload_size, TYPE_TABLE_TEST_ENUM_PAYLOAD_SIZE);
    EXPECT_TRUE(ir::is_payload_enum(module.types, enum_type));
    EXPECT_FALSE(ir::is_payload_enum(module.types, record_type));
    EXPECT_EQ(ir::enum_tag_type(module.types, enum_type).value, u32.value);
    EXPECT_EQ(ir::enum_tag_type(module.types, record_type).value, sema::INVALID_TYPE_HANDLE.value);
    EXPECT_EQ(ir::enum_tag_type(module.types, sema::INVALID_TYPE_HANDLE).value, sema::INVALID_TYPE_HANDLE.value);
    EXPECT_EQ(ir::enum_payload_storage_type(module.types, enum_type).value, record_type.value);
    EXPECT_EQ(ir::enum_payload_storage_type(module.types, record_type).value, sema::INVALID_TYPE_HANDLE.value);
    EXPECT_EQ(ir::enum_payload_storage_type(module.types, sema::INVALID_TYPE_HANDLE).value, sema::INVALID_TYPE_HANDLE.value);

    const RecordLayout payload_record = ir::make_payload_enum_record(module.types, enum_type);
    ASSERT_EQ(payload_record.fields.size(), TYPE_TABLE_TEST_ENUM_LAYOUT_FIELD_COUNT);
    EXPECT_EQ(payload_record.fields[0].name, ir::IR_ENUM_TAG_FIELD_NAME);
    EXPECT_EQ(payload_record.fields[0].type.value, u32.value);
    EXPECT_EQ(payload_record.fields[1].name, ir::IR_ENUM_PAYLOAD_FIELD_NAME);
    EXPECT_EQ(payload_record.fields[1].type.value, record_type.value);

    RecordLayout record;
    record.type = record_type;
    record.name = "unit.Pair";
    record.symbol = "unit_Pair";
    record.fields = {RecordField {"left", i32}, RecordField {"right", ptr_i32}};
    module.records.push_back(record);

    EXPECT_NE(ir::find_record(module, record_type), nullptr);
    EXPECT_EQ(ir::find_record(module, sema::INVALID_TYPE_HANDLE), nullptr);
    EXPECT_NE(ir::find_record_field(module, record_type, "right"), nullptr);
    EXPECT_EQ(ir::find_record_field(module, record_type, "missing"), nullptr);
    EXPECT_EQ(ir::record_field_index(record, "left"), 0U);
    EXPECT_EQ(ir::record_field_index(record, "missing"), static_cast<base::usize>(-1));

    const ValueId literal = add_value(module, integer_value(i32, "7"));
    const GlobalConstantId constant = add_global_constant(module, GlobalConstant {"seven", "unit_seven", i32, literal});
    EXPECT_TRUE(is_valid(literal));
    EXPECT_TRUE(is_valid(constant));
    EXPECT_NE(ir::find_global_constant(module, constant), nullptr);
    EXPECT_EQ(ir::find_global_constant(module, INVALID_GLOBAL_CONSTANT_ID), nullptr);

    Function function = make_function(module, "helper", i32);
    const BlockId entry = add_block(function, "entry");
    EXPECT_TRUE(is_valid(entry));
    EXPECT_FALSE(is_valid(INVALID_BLOCK_ID));
}

TEST(CoreUnit, TypeTableBuiltinDisplayNamesAndPredicates) {
    Module module;

    const TypeHandle void_type = builtin(module, BuiltinType::void_);
    const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
    const TypeHandle i8 = builtin(module, BuiltinType::i8);
    const TypeHandle u8 = builtin(module, BuiltinType::u8);
    const TypeHandle i16 = builtin(module, BuiltinType::i16);
    const TypeHandle u16 = builtin(module, BuiltinType::u16);
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle u32 = builtin(module, BuiltinType::u32);
    const TypeHandle i64 = builtin(module, BuiltinType::i64);
    const TypeHandle u64 = builtin(module, BuiltinType::u64);
    const TypeHandle isize = builtin(module, BuiltinType::isize);
    const TypeHandle usize = builtin(module, BuiltinType::usize);
    const TypeHandle f32 = builtin(module, BuiltinType::f32);
    const TypeHandle f64 = builtin(module, BuiltinType::f64);
    const TypeHandle str = builtin(module, BuiltinType::str);
    const TypeHandle char_type = builtin(module, BuiltinType::char_);

    const std::array<std::pair<TypeHandle, std::string_view>, TYPE_TABLE_TEST_BUILTIN_COUNT> builtins = {{
        {void_type, "void"},
        {bool_type, "bool"},
        {i8, "i8"},
        {u8, "u8"},
        {i16, "i16"},
        {u16, "u16"},
        {i32, "i32"},
        {u32, "u32"},
        {i64, "i64"},
        {u64, "u64"},
        {isize, "isize"},
        {usize, "usize"},
        {f32, "f32"},
        {f64, "f64"},
        {str, "str"},
        {char_type, "char"},
    }};

    for (const auto& [type, display] : builtins) {
        EXPECT_EQ(module.types.display_name(type), display);
        EXPECT_EQ(module.types.c_name(type), display);
    }

    EXPECT_TRUE(module.types.is_bool(bool_type));
    EXPECT_TRUE(module.types.is_integer(i8));
    EXPECT_TRUE(module.types.is_integer(u8));
    EXPECT_TRUE(module.types.is_integer(i16));
    EXPECT_TRUE(module.types.is_integer(u16));
    EXPECT_TRUE(module.types.is_integer(i32));
    EXPECT_TRUE(module.types.is_integer(u32));
    EXPECT_TRUE(module.types.is_integer(i64));
    EXPECT_TRUE(module.types.is_integer(u64));
    EXPECT_TRUE(module.types.is_integer(isize));
    EXPECT_TRUE(module.types.is_integer(usize));
    EXPECT_TRUE(module.types.is_float(f32));
    EXPECT_TRUE(module.types.is_float(f64));
    EXPECT_TRUE(module.types.is_str(str));
    EXPECT_TRUE(module.types.is_char(char_type));
    EXPECT_TRUE(module.types.is_void(void_type));
    EXPECT_FALSE(module.types.is_integer(bool_type));
    EXPECT_FALSE(module.types.is_integer(char_type));
    EXPECT_FALSE(module.types.is_float(i32));
}

} // namespace aurex::test
