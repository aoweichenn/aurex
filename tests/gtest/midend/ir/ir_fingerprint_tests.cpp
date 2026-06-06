#include <aurex/midend/ir/ir_cleanup_marker_facts.hpp>
#include <aurex/midend/ir/ir_fingerprint.hpp>

#include <gtest/support/ir_test_helpers.hpp>

#include <array>

namespace aurex::test {
namespace {

using namespace irtest;

constexpr std::string_view IR_FINGERPRINT_TEST_SYMBOL = "test_answer";
constexpr base::u32 IR_FINGERPRINT_INVALID_CLEANUP_POLICY_VALUE = 240U;
constexpr base::u32 IR_FINGERPRINT_OUT_OF_RANGE_TYPE_ID = 999U;

[[nodiscard]] const query::CleanupMarkerFact* cleanup_marker_with_policy(
    const query::FunctionCleanupMarkerFacts& facts, const query::CleanupMarkerPolicy policy) noexcept
{
    for (const query::CleanupMarkerFact& marker : facts.markers) {
        if (marker.policy == policy) {
            return &marker;
        }
    }
    return nullptr;
}

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

TEST(CoreUnit, IrFingerprintIncludesCleanupAbiPolicy)
{
    Module module;
    const TypeHandle void_type = builtin(module, BuiltinType::void_);
    const TypeHandle generic = module.types.generic_param(sema::generic_param_identity_from_text("fp.T"), "T");
    const TypeHandle generic_ptr = ptr(module, PointerMutability::mut, generic);

    Function function = make_function(module, "cleanup_policy", void_type);
    FunctionBuilder builder{module, function};
    Value slot = module.make_value();
    slot.kind = ValueKind::param;
    slot.type = generic_ptr;
    const ValueId slot_id = builder.add(slot);
    function.signature_params.push_back(function_param(module, "slot", generic_ptr));
    function.param_values.push_back(slot_id);

    Value drop = module.make_value();
    drop.kind = ValueKind::drop;
    drop.type = void_type;
    drop.object = slot_id;
    drop.target_type = generic;
    drop.cleanup_policy = CleanupAbiPolicy::generic_marker_only;
    const ValueId drop_id = builder.add(drop);

    const BlockId entry = builder.block("entry");
    function.blocks[entry.value].values = {slot_id, drop_id};
    function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    append_function(module, function);

    const query::QueryResultFingerprint before = ir::function_ir_unit_fingerprint(module, module.functions.front());
    module.values[drop_id.value].cleanup_policy = CleanupAbiPolicy::unknown_marker_only;
    const query::QueryResultFingerprint after = ir::function_ir_unit_fingerprint(module, module.functions.front());
    EXPECT_NE(before, after);
}

TEST(CoreUnit, IrCleanupMarkerFactsExposePolicySummaries)
{
    Module module;
    const TypeHandle void_type = builtin(module, BuiltinType::void_);
    const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
    const TypeHandle generic = module.types.generic_param(sema::generic_param_identity_from_text("fp.T"), "T");
    const TypeHandle generic_ptr = ptr(module, PointerMutability::mut, generic);

    Function function = make_function(module, "cleanup_facts", void_type);
    FunctionBuilder builder{module, function};

    Value slot = module.make_value();
    slot.kind = ValueKind::param;
    slot.type = generic_ptr;
    const ValueId slot_id = builder.add(slot);
    function.signature_params.push_back(function_param(module, "slot", generic_ptr));
    function.param_values.push_back(slot_id);

    Value flag = module.make_value();
    flag.kind = ValueKind::param;
    flag.type = bool_type;
    const ValueId flag_id = builder.add(flag);
    function.signature_params.push_back(function_param(module, "flag", bool_type));
    function.param_values.push_back(flag_id);

    Value drop = module.make_value();
    drop.kind = ValueKind::drop;
    drop.type = void_type;
    drop.object = slot_id;
    drop.target_type = generic;
    drop.cleanup_policy = CleanupAbiPolicy::generic_marker_only;
    const ValueId drop_id = builder.add(drop);

    Value drop_if = module.make_value();
    drop_if.kind = ValueKind::drop_if;
    drop_if.type = void_type;
    drop_if.lhs = flag_id;
    drop_if.object = slot_id;
    drop_if.target_type = generic;
    drop_if.cleanup_policy = CleanupAbiPolicy::unknown_marker_only;
    const ValueId drop_if_id = builder.add(drop_if);

    const BlockId entry = builder.block("entry");
    function.blocks[entry.value].values = {slot_id, flag_id, drop_id, drop_if_id};
    function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    append_function(module, function);

    query::FunctionCleanupMarkerFacts facts =
        ir::function_cleanup_marker_facts(module, module.functions.front());
    EXPECT_EQ(facts.symbol, "test_cleanup_facts");
    ASSERT_EQ(facts.markers.size(), 2U);
    EXPECT_EQ(facts.summary.drop_count, 1U);
    EXPECT_EQ(facts.summary.drop_if_count, 1U);
    EXPECT_EQ(facts.summary.generic_marker_only_count, 1U);
    EXPECT_EQ(facts.summary.unknown_marker_only_count, 1U);
    EXPECT_EQ(facts.markers[0].kind, query::CleanupMarkerKind::drop);
    EXPECT_EQ(facts.markers[0].policy, query::CleanupMarkerPolicy::generic_marker_only);
    EXPECT_EQ(facts.markers[1].kind, query::CleanupMarkerKind::drop_if);
    EXPECT_EQ(facts.markers[1].condition_value_id, flag_id.value);
    EXPECT_EQ(facts.fingerprint, query::function_cleanup_marker_facts_fingerprint(facts));
    EXPECT_NE(query::summarize_function_cleanup_marker_facts(facts).find("drop_if=1"), std::string::npos);
    EXPECT_NE(query::dump_function_cleanup_marker_facts(facts).find("unknown_marker_only"), std::string::npos);

    const std::optional<query::FunctionCleanupMarkerFacts> by_symbol =
        ir::function_cleanup_marker_facts_by_symbol(module, "test_cleanup_facts");
    ASSERT_TRUE(by_symbol.has_value());
    EXPECT_EQ(by_symbol->fingerprint, facts.fingerprint);
    EXPECT_EQ(ir::function_cleanup_marker_facts(module).size(), 1U);
    EXPECT_EQ(ir::query_cleanup_marker_policy(static_cast<CleanupAbiPolicy>(240U)), query::CleanupMarkerPolicy::none);
    EXPECT_FALSE(ir::function_cleanup_marker_facts_by_symbol(module, "missing").has_value());
}

TEST(CoreUnit, IrCleanupMarkerFactsMapEveryPolicyAndReferencedValueShape)
{
    const std::array<std::pair<CleanupAbiPolicy, query::CleanupMarkerPolicy>, 7> policy_mappings{{
        {CleanupAbiPolicy::none, query::CleanupMarkerPolicy::none},
        {CleanupAbiPolicy::structural_static, query::CleanupMarkerPolicy::structural_static},
        {CleanupAbiPolicy::generic_marker_only, query::CleanupMarkerPolicy::generic_marker_only},
        {CleanupAbiPolicy::associated_projection_marker_only,
            query::CleanupMarkerPolicy::associated_projection_marker_only},
        {CleanupAbiPolicy::opaque_marker_only, query::CleanupMarkerPolicy::opaque_marker_only},
        {CleanupAbiPolicy::unknown_marker_only, query::CleanupMarkerPolicy::unknown_marker_only},
        {CleanupAbiPolicy::static_custom_destructor, query::CleanupMarkerPolicy::static_custom_destructor},
    }};
    for (const auto& [ir_policy, query_policy] : policy_mappings) {
        EXPECT_EQ(ir::query_cleanup_marker_policy(ir_policy), query_policy);
    }
    EXPECT_EQ(ir::query_cleanup_marker_policy(
                  static_cast<CleanupAbiPolicy>(IR_FINGERPRINT_INVALID_CLEANUP_POLICY_VALUE)),
        query::CleanupMarkerPolicy::none);

    Module module;
    const TypeHandle void_type = builtin(module, BuiltinType::void_);
    const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle i32_ptr = ptr(module, PointerMutability::mut, i32);

    Function function = make_function(module, "cleanup_closure", void_type);
    function.symbol = ir::INVALID_IR_TEXT_ID;
    FunctionBuilder builder{module, function};
    const BlockId entry = builder.block("entry");
    const BlockId predecessor = builder.block("predecessor");

    Value slot = module.make_value();
    slot.kind = ValueKind::param;
    slot.type = i32_ptr;
    const ValueId slot_id = builder.add(slot);
    function.signature_params.push_back(function_param(module, "slot", i32_ptr));
    function.param_values.push_back(slot_id);

    Value flag = module.make_value();
    flag.kind = ValueKind::param;
    flag.type = bool_type;
    const ValueId flag_id = builder.add(flag);
    function.signature_params.push_back(function_param(module, "flag", bool_type));
    function.param_values.push_back(flag_id);

    Value none_drop = module.make_value();
    none_drop.kind = ValueKind::drop;
    none_drop.type = void_type;
    none_drop.cleanup_policy = CleanupAbiPolicy::none;
    const ValueId none_drop_id = builder.add(none_drop);

    Value structural_drop = module.make_value();
    structural_drop.kind = ValueKind::drop;
    structural_drop.type = void_type;
    structural_drop.object = slot_id;
    structural_drop.target_type = TypeHandle{IR_FINGERPRINT_OUT_OF_RANGE_TYPE_ID};
    structural_drop.cleanup_policy = CleanupAbiPolicy::structural_static;
    const ValueId structural_drop_id = builder.add(structural_drop);

    Value associated_drop_if = module.make_value();
    associated_drop_if.kind = ValueKind::drop_if;
    associated_drop_if.type = void_type;
    associated_drop_if.lhs = flag_id;
    associated_drop_if.object = slot_id;
    associated_drop_if.target_type = i32;
    associated_drop_if.cleanup_policy = CleanupAbiPolicy::associated_projection_marker_only;
    const ValueId associated_drop_if_id = builder.add(associated_drop_if);

    Value opaque_drop = module.make_value();
    opaque_drop.kind = ValueKind::drop;
    opaque_drop.type = void_type;
    opaque_drop.object = slot_id;
    opaque_drop.target_type = i32;
    opaque_drop.cleanup_policy = CleanupAbiPolicy::opaque_marker_only;
    const ValueId opaque_drop_id = builder.add(opaque_drop);

    Value carrier = module.make_value();
    carrier.kind = ValueKind::aggregate;
    carrier.type = i32;
    carrier.args = {none_drop_id, ValueId{IR_FINGERPRINT_OUT_OF_RANGE_TYPE_ID}};
    carrier.fields = {field_value(module, "cleanup", structural_drop_id)};
    carrier.elements = {opaque_drop_id};
    const ValueId carrier_id = builder.add(carrier);

    Value phi = module.make_value();
    phi.kind = ValueKind::phi;
    phi.type = void_type;
    phi.incoming = {PhiInput{predecessor, associated_drop_if_id}};
    const ValueId phi_id = builder.add(phi);

    Value bad_constant_ref = module.make_value();
    bad_constant_ref.kind = ValueKind::constant_ref;
    bad_constant_ref.type = i32;
    bad_constant_ref.constant = GlobalConstantId{IR_FINGERPRINT_OUT_OF_RANGE_TYPE_ID};
    const ValueId bad_constant_ref_id = builder.add(bad_constant_ref);

    function.blocks[entry.value].values = {carrier_id, phi_id, bad_constant_ref_id};
    function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    function.blocks[entry.value].terminator.condition = flag_id;
    function.blocks[entry.value].terminator.value = phi_id;
    function.blocks[predecessor.value].values = {};
    function.blocks[predecessor.value].terminator.kind = TerminatorKind::branch;
    function.blocks[predecessor.value].terminator.target = entry;
    append_function(module, function);

    const query::FunctionCleanupMarkerFacts facts =
        ir::function_cleanup_marker_facts(module, module.functions.front());
    EXPECT_TRUE(facts.symbol.empty());
    ASSERT_EQ(facts.markers.size(), 4U);
    EXPECT_EQ(facts.summary.drop_count, 3U);
    EXPECT_EQ(facts.summary.drop_if_count, 1U);
    EXPECT_EQ(facts.summary.none_count, 1U);
    EXPECT_EQ(facts.summary.structural_static_count, 1U);
    EXPECT_EQ(facts.summary.associated_projection_marker_only_count, 1U);
    EXPECT_EQ(facts.summary.opaque_marker_only_count, 1U);

    const query::CleanupMarkerFact* const none_marker =
        cleanup_marker_with_policy(facts, query::CleanupMarkerPolicy::none);
    ASSERT_NE(none_marker, nullptr);
    EXPECT_EQ(none_marker->object_value_id, ValueId::INVALID_VALUE);
    EXPECT_EQ(none_marker->condition_value_id, ValueId::INVALID_VALUE);
    EXPECT_EQ(none_marker->target_type_id, sema::TypeHandle::INVALID_VALUE);
    EXPECT_TRUE(none_marker->target_type.empty());

    const query::CleanupMarkerFact* const structural_marker =
        cleanup_marker_with_policy(facts, query::CleanupMarkerPolicy::structural_static);
    ASSERT_NE(structural_marker, nullptr);
    EXPECT_EQ(structural_marker->object_value_id, slot_id.value);
    EXPECT_EQ(structural_marker->target_type_id, IR_FINGERPRINT_OUT_OF_RANGE_TYPE_ID);
    EXPECT_TRUE(structural_marker->target_type.empty());

    const query::CleanupMarkerFact* const associated_marker =
        cleanup_marker_with_policy(facts, query::CleanupMarkerPolicy::associated_projection_marker_only);
    ASSERT_NE(associated_marker, nullptr);
    EXPECT_EQ(associated_marker->kind, query::CleanupMarkerKind::drop_if);
    EXPECT_EQ(associated_marker->condition_value_id, flag_id.value);
    EXPECT_EQ(associated_marker->target_type, "i32");

    const std::optional<query::FunctionCleanupMarkerFacts> anonymous =
        ir::function_cleanup_marker_facts_by_symbol(module, "");
    ASSERT_TRUE(anonymous.has_value());
    EXPECT_EQ(anonymous->fingerprint, facts.fingerprint);
}

} // namespace aurex::test
