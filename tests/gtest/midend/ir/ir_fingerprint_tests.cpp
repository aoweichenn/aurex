#include <aurex/midend/ir/ir_fingerprint.hpp>

#include <gtest/support/ir_test_helpers.hpp>

namespace aurex::test {
namespace {

using namespace irtest;

constexpr std::string_view IR_FINGERPRINT_TEST_SYMBOL = "test_answer";

} // namespace

TEST(CoreUnit, IrFingerprintsSeparateFunctionLayoutAndLlvmEmissionUnits)
{
    Module module = make_simple_module();
    ASSERT_FALSE(module.functions.empty());

    const query::QueryResultFingerprint initial_layout = ir::layout_abi_fingerprint(module);
    const query::QueryResultFingerprint direct_ir_unit =
        ir::function_ir_unit_fingerprint(module, module.functions.front());
    const query::QueryResultFingerprint direct_llvm_unit =
        ir::llvm_emission_unit_fingerprint(module, module.functions.front());
    const std::optional<ir::FunctionIRUnitFingerprint> initial_unit =
        ir::function_ir_unit_fingerprint_by_symbol(module, IR_FINGERPRINT_TEST_SYMBOL);
    ASSERT_TRUE(initial_unit.has_value());
    EXPECT_TRUE(query::is_valid(initial_layout));
    EXPECT_EQ(direct_ir_unit, initial_unit->target_independent_ir);
    EXPECT_EQ(direct_llvm_unit, initial_unit->llvm_emission_unit);
    EXPECT_TRUE(query::is_valid(initial_unit->target_independent_ir));
    EXPECT_TRUE(query::is_valid(initial_unit->llvm_emission_unit));
    EXPECT_EQ(initial_unit->symbol, IR_FINGERPRINT_TEST_SYMBOL);

    module.values[0].text = text_id(module, "43");
    const std::optional<ir::FunctionIRUnitFingerprint> changed_body_unit =
        ir::function_ir_unit_fingerprint_by_symbol(module, IR_FINGERPRINT_TEST_SYMBOL);
    ASSERT_TRUE(changed_body_unit.has_value());
    EXPECT_NE(changed_body_unit->target_independent_ir, initial_unit->target_independent_ir);
    EXPECT_NE(changed_body_unit->llvm_emission_unit, initial_unit->llvm_emission_unit);
    EXPECT_EQ(ir::layout_abi_fingerprint(module), initial_layout);

    const TypeHandle record_type = module.types.named_struct("fp.Record", "fp_Record", false);
    RecordLayout record = record_layout(module, record_type, "fp.Record", "fp_Record", false);
    record.fields.push_back(record_field(module, "value", builtin(module, BuiltinType::i32)));
    append_record(module, record);
    EXPECT_NE(ir::layout_abi_fingerprint(module), initial_layout);

    const std::vector<ir::FunctionIRUnitFingerprint> units = ir::function_ir_unit_fingerprints(module);
    ASSERT_EQ(units.size(), 1U);
    EXPECT_EQ(units[0].symbol, IR_FINGERPRINT_TEST_SYMBOL);
    EXPECT_FALSE(ir::function_ir_unit_fingerprint_by_symbol(module, "missing").has_value());
}

} // namespace aurex::test
