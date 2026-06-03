#include <aurex/midend/ir/enum_layout.hpp>

#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/support/ir_test_helpers.hpp>

namespace aurex::test {
namespace {

using namespace irtest;

constexpr base::u64 TYPE_TABLE_TEST_ARRAY_COUNT = 4;
constexpr base::usize TYPE_TABLE_TEST_BUILTIN_COUNT = 16;
constexpr base::u64 TYPE_TABLE_TEST_ENUM_PAYLOAD_SIZE = 8;
constexpr base::u64 TYPE_TABLE_TEST_ENUM_PAYLOAD_ALIGNMENT = 4;
constexpr base::usize TYPE_TABLE_TEST_ENUM_LAYOUT_FIELD_COUNT = 2;
constexpr base::u32 TYPE_TABLE_TEST_OUT_OF_RANGE_TYPE = 9999;
constexpr base::usize IR_MODULE_COPY_TEST_VALUE_RESERVE = 4;
constexpr base::usize IR_MODULE_COPY_TEST_FUNCTION_RESERVE = 1;
constexpr base::usize IR_MODULE_COPY_TEST_RECORD_RESERVE = 1;
constexpr base::usize IR_MODULE_COPY_TEST_CONSTANT_RESERVE = 1;
constexpr base::u32 IR_MODULE_COPY_TEST_RECORD_INDEX = 0;
constexpr base::u32 IR_MODULE_COPY_TEST_ENTRY_INDEX = 0;
constexpr base::u32 IR_MODULE_COPY_TEST_MISSING_TEXT_OFFSET = 1;

} // namespace

TEST(CoreUnit, TypeTableAndIrHelpersCoverInvalidAndCompositePaths)
{
    Module module;
    const TypeHandle void_type = builtin(module, BuiltinType::void_);
    const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle u32 = builtin(module, BuiltinType::u32);
    const TypeHandle f64 = builtin(module, BuiltinType::f64);
    const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle ptr_i32_again = ptr(module, PointerMutability::mut, i32);
    const TypeHandle const_ptr_i32 = ptr(module, PointerMutability::const_, i32);
    const TypeHandle ref_i32 = module.types.reference(PointerMutability::const_, i32);
    const TypeHandle ref_i32_again = module.types.reference(PointerMutability::const_, i32);
    const TypeHandle mut_ref_i32 = module.types.reference(PointerMutability::mut, i32);
    const std::array<std::string_view, 1> data_origin{"data"};
    const std::array<std::string_view, 2> union_origin{"right", "left"};
    const TypeHandle origin_ref_i32 = module.types.reference(PointerMutability::const_, i32, data_origin);
    const TypeHandle origin_ref_i32_again = module.types.reference(PointerMutability::const_, i32, data_origin);
    const TypeHandle union_origin_mut_ref_i32 = module.types.reference(PointerMutability::mut, i32, union_origin);
    const std::array<std::string_view, 3> duplicate_origin{"data", "", "data"};
    const TypeHandle dedup_origin_ref_i32 = module.types.reference(PointerMutability::const_, i32, duplicate_origin);
    const TypeHandle empty_origin_ref_i32 = module.types.reference_with_origin_key(PointerMutability::const_, i32, "");
    const TypeHandle array_i32 = module.types.array(TYPE_TABLE_TEST_ARRAY_COUNT, i32);
    const TypeHandle array_i32_again = module.types.array(TYPE_TABLE_TEST_ARRAY_COUNT, i32);
    const TypeHandle array_ptr_i32 = module.types.array(TYPE_TABLE_TEST_ARRAY_COUNT, ptr_i32);
    const TypeHandle ptr_array_i32 = ptr(module, PointerMutability::mut, array_i32);
    const TypeHandle const_slice_i32 = module.types.slice(PointerMutability::const_, i32);
    const TypeHandle const_slice_i32_again = module.types.slice(PointerMutability::const_, i32);
    const TypeHandle mut_slice_i32 = module.types.slice(PointerMutability::mut, i32);
    const TypeHandle tuple_i32_bool = module.types.tuple(std::vector<TypeHandle>{i32, bool_type});
    const TypeHandle tuple_i32_bool_again = module.types.tuple(std::vector<TypeHandle>{i32, bool_type});
    const TypeHandle single_tuple_i32 = module.types.tuple(std::vector<TypeHandle>{i32});
    const TypeHandle tuple_with_array = module.types.tuple(std::vector<TypeHandle>{array_i32, bool_type});
    const TypeHandle function_i32_u32 =
        module.types.function(sema::FunctionCallConv::aurex, false, std::vector<TypeHandle>{i32, u32}, i32);
    const TypeHandle function_i32_u32_again =
        module.types.function(sema::FunctionCallConv::aurex, false, std::vector<TypeHandle>{i32, u32}, i32);
    const TypeHandle unsafe_function_i32_u32 =
        module.types.function(sema::FunctionCallConv::aurex, true, false, std::vector<TypeHandle>{i32, u32}, i32);
    const TypeHandle unsafe_extern_function =
        module.types.function(sema::FunctionCallConv::c, true, false, std::vector<TypeHandle>{const_ptr_i32}, i32);
    const TypeHandle extern_variadic_function =
        module.types.function(sema::FunctionCallConv::c, true, std::vector<TypeHandle>{const_ptr_i32}, i32);
    const TypeHandle function_void =
        module.types.function(sema::FunctionCallConv::aurex, false, std::vector<TypeHandle>{}, void_type);
    const TypeHandle extern_variadic_void =
        module.types.function(sema::FunctionCallConv::c, true, std::vector<TypeHandle>{}, void_type);
    const std::string array_display = "[" + std::to_string(TYPE_TABLE_TEST_ARRAY_COUNT) + "]";
    const TypeHandle record_type = module.types.named_struct("unit.Pair", "unit_Pair", false);
    const TypeHandle enum_type = module.types.named_enum("unit.Tag", "unit_Tag");
    const TypeHandle opaque = module.types.opaque_struct("unit.Opaque", "unit_Opaque");
    const TypeHandle generic_record = module.types.named_struct("unit.Box", "unit_Box__aurexg_t2", false);
    module.types.set_generic_instance(generic_record, "unit:Box", std::vector<TypeHandle>{i32});
    const TypeHandle generic_enum = module.types.named_enum("unit.Maybe", "unit_Maybe__aurexg_t2");
    module.types.set_generic_instance(generic_enum, "unit:Maybe", std::vector<TypeHandle>{i32});

    EXPECT_TRUE(module.types.same(ptr_i32, ptr_i32_again));
    EXPECT_FALSE(module.types.same(ptr_i32, const_ptr_i32));
    EXPECT_TRUE(module.types.same(ref_i32, ref_i32_again));
    EXPECT_FALSE(module.types.same(ref_i32, mut_ref_i32));
    EXPECT_TRUE(module.types.same(origin_ref_i32, origin_ref_i32_again));
    EXPECT_TRUE(module.types.same(origin_ref_i32, dedup_origin_ref_i32));
    EXPECT_TRUE(module.types.same(ref_i32, empty_origin_ref_i32));
    EXPECT_FALSE(module.types.same(ref_i32, origin_ref_i32));
    EXPECT_FALSE(module.types.same(ref_i32, const_ptr_i32));
    EXPECT_TRUE(module.types.same(array_i32, array_i32_again));
    EXPECT_TRUE(module.types.same(const_slice_i32, const_slice_i32_again));
    EXPECT_FALSE(module.types.same(const_slice_i32, mut_slice_i32));
    EXPECT_TRUE(module.types.same(tuple_i32_bool, tuple_i32_bool_again));
    EXPECT_FALSE(module.types.same(tuple_i32_bool, single_tuple_i32));
    EXPECT_TRUE(module.types.same(function_i32_u32, function_i32_u32_again));
    EXPECT_FALSE(module.types.same(function_i32_u32, unsafe_function_i32_u32));
    EXPECT_FALSE(module.types.same(function_i32_u32, extern_variadic_function));
    EXPECT_TRUE(module.types.is_integer(i32));
    EXPECT_TRUE(module.types.is_integer(u32));
    EXPECT_TRUE(module.types.is_float(f64));
    EXPECT_TRUE(module.types.is_pointer(ptr_i32));
    EXPECT_TRUE(module.types.is_reference(ref_i32));
    EXPECT_TRUE(module.types.is_reference(mut_ref_i32));
    EXPECT_TRUE(module.types.is_array(array_i32));
    EXPECT_TRUE(module.types.is_slice(const_slice_i32));
    EXPECT_TRUE(module.types.is_slice(mut_slice_i32));
    EXPECT_TRUE(module.types.is_tuple(tuple_i32_bool));
    EXPECT_TRUE(module.types.is_function(function_i32_u32));
    EXPECT_TRUE(module.types.contains_array(array_i32));
    EXPECT_FALSE(module.types.contains_array(const_slice_i32));
    EXPECT_TRUE(module.types.contains_array(tuple_with_array));
    EXPECT_FALSE(module.types.contains_array(tuple_i32_bool));
    EXPECT_EQ(module.types.display_name(ptr_i32), "*mut i32");
    EXPECT_EQ(module.types.display_name(ref_i32), "&i32");
    EXPECT_EQ(module.types.display_name(mut_ref_i32), "&mut i32");
    EXPECT_EQ(module.types.display_name(origin_ref_i32), "&[data] i32");
    EXPECT_EQ(module.types.display_name(dedup_origin_ref_i32), "&[data] i32");
    EXPECT_EQ(module.types.display_name(union_origin_mut_ref_i32), "&mut[left | right] i32");
    EXPECT_EQ(module.types.display_name(array_i32), array_display + "i32");
    EXPECT_EQ(module.types.display_name(array_ptr_i32), array_display + "*mut i32");
    EXPECT_EQ(module.types.display_name(ptr_array_i32), std::string("*mut ") + array_display + "i32");
    EXPECT_EQ(module.types.display_name(const_slice_i32), "[]const i32");
    EXPECT_EQ(module.types.display_name(mut_slice_i32), "[]mut i32");
    EXPECT_EQ(module.types.display_name(tuple_i32_bool), "(i32, bool)");
    EXPECT_EQ(module.types.display_name(single_tuple_i32), "(i32,)");
    EXPECT_EQ(module.types.display_name(function_i32_u32), "fn(i32, u32) -> i32");
    EXPECT_EQ(module.types.display_name(unsafe_function_i32_u32), "unsafe fn(i32, u32) -> i32");
    EXPECT_EQ(module.types.display_name(unsafe_extern_function), "unsafe extern c fn(*const i32) -> i32");
    EXPECT_EQ(module.types.display_name(extern_variadic_function), "extern c fn(*const i32, ...) -> i32");
    EXPECT_EQ(module.types.display_name(function_void), "fn() -> void");
    EXPECT_EQ(module.types.display_name(extern_variadic_void), "extern c fn(...) -> void");
    EXPECT_EQ(module.types.c_name(function_void), "fn() -> void");
    EXPECT_EQ(module.types.display_name(record_type), "unit.Pair");
    EXPECT_EQ(module.types.display_name(enum_type), "unit.Tag");
    EXPECT_EQ(module.types.display_name(opaque), "unit.Opaque");
    EXPECT_EQ(module.types.get(generic_record).name, "unit.Box");
    EXPECT_EQ(module.types.display_name(generic_record), "unit.Box[i32]");
    EXPECT_EQ(module.types.display_name("Box", module.types.get(generic_record).generic_args), "Box[i32]");
    EXPECT_EQ(module.types.get(generic_enum).name, "unit.Maybe");
    EXPECT_EQ(module.types.display_name(generic_enum), "unit.Maybe[i32]");
    EXPECT_EQ(module.types.display_name(sema::INVALID_TYPE_HANDLE), "<invalid>");
    EXPECT_EQ(module.types.c_name(sema::INVALID_TYPE_HANDLE), "void");
    EXPECT_EQ(module.types.c_name(record_type), "unit_Pair");
    EXPECT_EQ(module.types.c_name(enum_type), "unit_Tag");
    EXPECT_EQ(module.types.c_name(opaque), "unit_Opaque");

    module.types.set_record_contains_array(record_type, true);
    module.types.set_enum_underlying(enum_type, u32);
    module.types.set_enum_payload_layout(
        enum_type, record_type, TYPE_TABLE_TEST_ENUM_PAYLOAD_SIZE, TYPE_TABLE_TEST_ENUM_PAYLOAD_ALIGNMENT);
    EXPECT_TRUE(module.types.contains_array(record_type));
    EXPECT_EQ(module.types.get(enum_type).enum_payload_size, TYPE_TABLE_TEST_ENUM_PAYLOAD_SIZE);
    EXPECT_TRUE(ir::is_payload_enum(module.types, enum_type));
    EXPECT_FALSE(ir::is_payload_enum(module.types, record_type));
    EXPECT_EQ(ir::enum_tag_type(module.types, enum_type).value, u32.value);
    EXPECT_EQ(ir::enum_tag_type(module.types, record_type).value, sema::INVALID_TYPE_HANDLE.value);
    EXPECT_EQ(ir::enum_tag_type(module.types, sema::INVALID_TYPE_HANDLE).value, sema::INVALID_TYPE_HANDLE.value);
    EXPECT_EQ(ir::enum_payload_storage_type(module.types, enum_type).value, record_type.value);
    EXPECT_EQ(ir::enum_payload_storage_type(module.types, record_type).value, sema::INVALID_TYPE_HANDLE.value);
    EXPECT_EQ(
        ir::enum_payload_storage_type(module.types, sema::INVALID_TYPE_HANDLE).value, sema::INVALID_TYPE_HANDLE.value);

    const RecordLayout payload_record = ir::make_payload_enum_record(module, enum_type);
    ASSERT_EQ(payload_record.fields.size(), TYPE_TABLE_TEST_ENUM_LAYOUT_FIELD_COUNT);
    EXPECT_EQ(module.text(payload_record.fields[0].name), ir::IR_ENUM_TAG_FIELD_NAME);
    EXPECT_EQ(payload_record.fields[0].type.value, u32.value);
    EXPECT_EQ(module.text(payload_record.fields[1].name), ir::IR_ENUM_PAYLOAD_FIELD_NAME);
    EXPECT_EQ(payload_record.fields[1].type.value, record_type.value);

    RecordLayout record = record_layout(module, record_type, "unit.Pair", "unit_Pair", false);
    record.fields.push_back(record_field(module, "left", i32));
    record.fields.push_back(record_field(module, "right", ptr_i32));
    append_record(module, record);
    const RecordLayout& stored_record = module.records.back();

    EXPECT_NE(ir::find_record(module, record_type), nullptr);
    EXPECT_EQ(ir::find_record(module, sema::INVALID_TYPE_HANDLE), nullptr);
    EXPECT_NE(ir::find_record_field(module, record_type, "right"), nullptr);
    EXPECT_EQ(ir::find_record_field(module, record_type, "missing"), nullptr);
    EXPECT_EQ(ir::record_field_index(stored_record, text_id(module, "left")), 0U);
    EXPECT_EQ(ir::record_field_index(stored_record, text_id(module, "missing")), static_cast<base::usize>(-1));

    const ValueId literal = add_value(module, integer_value(module, i32, "7"));
    const GlobalConstantId constant = add_global_constant(module, GlobalConstant{"seven", "unit_seven", i32, literal});
    EXPECT_TRUE(is_valid(literal));
    EXPECT_TRUE(is_valid(constant));
    EXPECT_NE(ir::find_global_constant(module, constant), nullptr);
    EXPECT_EQ(ir::find_global_constant(module, INVALID_GLOBAL_CONSTANT_ID), nullptr);

    Function function = make_function(module, "helper", i32);
    const BlockId entry = add_block(module, function, "entry");
    EXPECT_TRUE(is_valid(entry));
    EXPECT_FALSE(is_valid(INVALID_BLOCK_ID));
}

TEST(CoreUnit, IrModuleArenaBackedCopyMovePreservesInternedPayloads)
{
    Module module;
    module.reserve(IR_MODULE_COPY_TEST_VALUE_RESERVE, IR_MODULE_COPY_TEST_FUNCTION_RESERVE,
        IR_MODULE_COPY_TEST_RECORD_RESERVE, IR_MODULE_COPY_TEST_CONSTANT_RESERVE);

    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle record_type = module.types.named_struct("copy.Pair", "copy_Pair", false);
    const IrTextId payload_text = text_id(module, "payload");
    EXPECT_TRUE(module.has_text(payload_text));
    EXPECT_EQ(module.find_text("payload").value, payload_text.value);
    EXPECT_FALSE(module.has_text(IrTextId{payload_text.value + IR_MODULE_COPY_TEST_MISSING_TEXT_OFFSET}));
    EXPECT_FALSE(module.has_text(module.find_text("missing")));

    Value literal = integer_value(module, i32, "99");
    set_name(module, literal, "literal");
    const ValueId literal_id = add_value(module, literal);

    Value aggregate = module.make_value();
    aggregate.kind = ValueKind::aggregate;
    aggregate.type = record_type;
    set_name(module, aggregate, "aggregate");
    aggregate.args.push_back(literal_id);
    aggregate.fields.push_back(field_value(module, "left", literal_id));
    aggregate.incoming.push_back(PhiInput{BlockId{IR_MODULE_COPY_TEST_ENTRY_INDEX}, literal_id});
    aggregate.elements.push_back(literal_id);
    const ValueId aggregate_id = add_value(module, aggregate);

    RecordLayout record = record_layout(module, record_type, "copy.Pair", "copy_Pair", false);
    record.fields.push_back(record_field(module, "left", i32));
    const base::u32 record_index = add_record(module, record);
    ASSERT_EQ(record_index, IR_MODULE_COPY_TEST_RECORD_INDEX);
    module.record_indices.emplace(record_type.value, record_index);

    const GlobalConstantId constant_id =
        add_global_constant(module, GlobalConstant{"global", "copy_global", i32, literal_id});

    Function function = make_function(module, "copy", i32);
    function.signature_params.push_back(function_param(module, "input", i32));
    function.param_values.push_back(literal_id);
    const BlockId entry = add_block(module, function, "entry");
    function.blocks[entry.value].values.push_back(aggregate_id);
    function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    function.blocks[entry.value].terminator.value = aggregate_id;
    append_function(module, function);

    Module copied(module);
    ASSERT_EQ(copied.values.size(), module.values.size());
    EXPECT_EQ(copied.text(copied.values[literal_id.value].name), "literal");
    EXPECT_EQ(copied.text(copied.values[literal_id.value].text), "99");
    EXPECT_EQ(copied.text(copied.values[aggregate_id.value].name), "aggregate");
    ASSERT_EQ(copied.values[aggregate_id.value].args.size(), 1U);
    ASSERT_EQ(copied.values[aggregate_id.value].fields.size(), 1U);
    ASSERT_EQ(copied.values[aggregate_id.value].incoming.size(), 1U);
    ASSERT_EQ(copied.values[aggregate_id.value].elements.size(), 1U);
    EXPECT_EQ(copied.values[aggregate_id.value].args.front().value, literal_id.value);
    EXPECT_EQ(copied.text(copied.values[aggregate_id.value].fields.front().name), "left");
    EXPECT_EQ(copied.values[aggregate_id.value].incoming.front().predecessor.value, entry.value);
    EXPECT_EQ(copied.values[aggregate_id.value].elements.front().value, literal_id.value);
    ASSERT_EQ(copied.records.size(), 1U);
    EXPECT_EQ(copied.text(copied.records.front().name), "copy.Pair");
    EXPECT_EQ(copied.text(copied.records.front().fields.front().name), "left");
    ASSERT_EQ(copied.constants.size(), 1U);
    EXPECT_EQ(copied.text(copied.constants[constant_id.value].symbol), "copy_global");
    ASSERT_EQ(copied.functions.size(), 1U);
    ASSERT_EQ(copied.functions.front().signature_params.size(), 1U);
    ASSERT_EQ(copied.functions.front().blocks.size(), 1U);
    EXPECT_EQ(copied.text(copied.functions.front().signature_params.front().name), "input");
    EXPECT_EQ(copied.text(copied.functions.front().blocks.front().name), "entry");
    EXPECT_EQ(copied.record_indices.at(record_type.value), record_index);

    Module assigned;
    assigned = module;
    Module& assigned_alias = assigned;
    assigned = assigned_alias;
    EXPECT_EQ(assigned.text(assigned.functions.front().symbol), "test_copy");
    EXPECT_EQ(assigned.text(assigned.records.front().symbol), "copy_Pair");

    Module moved(std::move(assigned));
    EXPECT_EQ(moved.text(moved.values[aggregate_id.value].fields.front().name), "left");

    Module move_assigned;
    move_assigned = std::move(moved);
    Module& move_assigned_alias = move_assigned;
    move_assigned = std::move(move_assigned_alias);
    EXPECT_EQ(move_assigned.text(move_assigned.constants[constant_id.value].name), "global");
    EXPECT_EQ(move_assigned.functions.front().blocks.front().terminator.value.value, aggregate_id.value);
}

TEST(CoreUnit, TypeTableBuiltinDisplayNamesAndPredicates)
{
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

    const TypeHandle out_of_range{TYPE_TABLE_TEST_OUT_OF_RANGE_TYPE};
    EXPECT_FALSE(module.types.is_bool(sema::INVALID_TYPE_HANDLE));
    EXPECT_FALSE(module.types.is_bool(out_of_range));
    EXPECT_FALSE(module.types.is_str(sema::INVALID_TYPE_HANDLE));
    EXPECT_FALSE(module.types.is_str(out_of_range));
    EXPECT_FALSE(module.types.is_char(sema::INVALID_TYPE_HANDLE));
    EXPECT_FALSE(module.types.is_char(out_of_range));
    EXPECT_FALSE(module.types.is_void(sema::INVALID_TYPE_HANDLE));
    EXPECT_FALSE(module.types.is_void(out_of_range));
    EXPECT_FALSE(module.types.is_pointer(sema::INVALID_TYPE_HANDLE));
    EXPECT_FALSE(module.types.is_pointer(out_of_range));
    EXPECT_FALSE(module.types.is_reference(sema::INVALID_TYPE_HANDLE));
    EXPECT_FALSE(module.types.is_reference(out_of_range));
    EXPECT_FALSE(module.types.is_array(sema::INVALID_TYPE_HANDLE));
    EXPECT_FALSE(module.types.is_array(out_of_range));
    EXPECT_FALSE(module.types.is_slice(sema::INVALID_TYPE_HANDLE));
    EXPECT_FALSE(module.types.is_slice(out_of_range));
    EXPECT_FALSE(module.types.is_tuple(sema::INVALID_TYPE_HANDLE));
    EXPECT_FALSE(module.types.is_tuple(out_of_range));
    EXPECT_FALSE(module.types.is_function(sema::INVALID_TYPE_HANDLE));
    EXPECT_FALSE(module.types.is_function(out_of_range));
    EXPECT_FALSE(module.types.contains_array(sema::INVALID_TYPE_HANDLE));
    EXPECT_FALSE(module.types.contains_array(out_of_range));
    EXPECT_EQ(module.types.display_name(out_of_range), "<invalid>");
    EXPECT_EQ(module.types.c_name(out_of_range), "void");
}

TEST(CoreUnit, TypeTableGenericParamsUseStableIdentities)
{
    Module module;

    const sema::GenericParamIdentity first_identity = sema::generic_param_identity_from_text("template_a#param:0:T");
    const sema::GenericParamIdentity second_identity = sema::generic_param_identity_from_text("template_b#param:0:T");
    const TypeHandle first = module.types.generic_param(first_identity, "T");
    const TypeHandle second = module.types.generic_param(second_identity, "T");
    const TypeHandle first_again = module.types.generic_param(first_identity, "T");
    const TypeHandle legacy = module.types.generic_param("T");

    EXPECT_EQ(first.value, first_again.value);
    EXPECT_NE(first.value, second.value);
    EXPECT_NE(first.value, legacy.value);
    EXPECT_EQ(module.types.display_name(first), "T");
    EXPECT_EQ(module.types.display_name(second), "T");
    EXPECT_EQ(module.types.get(first).generic_identity, first_identity);
    EXPECT_EQ(module.types.get(second).generic_identity, second_identity);
    EXPECT_EQ(module.types.get(legacy).generic_identity, sema::generic_param_identity_from_text("T"));
}

} // namespace aurex::test
