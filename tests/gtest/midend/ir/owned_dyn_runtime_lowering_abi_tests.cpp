#include <aurex/midend/ir/ir_dump.hpp>
#include <aurex/midend/ir/ir_owned_dyn_runtime_lowering_abi_gate.hpp>
#include <aurex/midend/ir/verify.hpp>

#include <span>
#include <string>
#include <utility>

#include <gtest/support/ir_test_helpers.hpp>

namespace aurex::test {
namespace {

using namespace irtest;

constexpr base::u32 IR_TEST_TRAIT_MODULE_ID = 0U;
constexpr base::u32 IR_TEST_TRAIT_NAME_ID = 11U;
constexpr base::u32 IR_TEST_UNEXPECTED_RUNTIME_SLOT = 0U;
constexpr std::string_view IR_TEST_MODULE_NAME = "owned_dyn_runtime_abi";
constexpr std::string_view IR_TEST_TRAIT_NAME = "Render";
constexpr std::string_view IR_TEST_PROTOTYPE_SYMBOL = "__aurex_owned_dyn_runtime_abi_render";

struct OwnedDynRuntimeAbiFixture {
    Module module;
    query::TraitObjectTypeKey object_key;
    TypeHandle object_type = sema::INVALID_TYPE_HANDLE;
    TypeHandle data_pointer_type = sema::INVALID_TYPE_HANDLE;
    TypeHandle vtable_pointer_type = sema::INVALID_TYPE_HANDLE;
};

[[nodiscard]] bool contains_text(const std::string& value, const std::string_view text)
{
    return value.find(text) != std::string::npos;
}

[[nodiscard]] query::DefKey test_trait_def_key(
    const std::string_view module_name,
    const std::string_view trait_name)
{
    return query::def_key_from_stable_id(
        query::stable_definition_id(
            query::stable_module_id(std::span<const std::string_view>{&module_name, 1U}),
            query::StableSymbolKind::type,
            trait_name),
        query::DefNamespace::trait_,
        query::DefKind::trait_);
}

[[nodiscard]] query::TraitObjectTypeKey test_trait_object_key(
    const std::string_view module_name,
    const std::string_view trait_name)
{
    return query::trait_object_type_key(test_trait_def_key(module_name, trait_name),
        std::span<const query::CanonicalTypeKey>{},
        std::span<const query::TraitObjectAssociatedTypeEqualityKey>{},
        query::stable_fingerprint(std::string("origin:runtime-abi:") + std::string(module_name) + ":"
            + std::string(trait_name)),
        query::stable_fingerprint(std::string("schema:runtime-abi:") + std::string(module_name) + ":"
            + std::string(trait_name)));
}

[[nodiscard]] TypeHandle make_trait_object_type(
    Module& module,
    const query::TraitObjectTypeKey& object_key,
    const std::string_view trait_name)
{
    return module.types.trait_object(
        object_key,
        trait_name,
        syntax::ModuleId{IR_TEST_TRAIT_MODULE_ID},
        sema::IdentId{IR_TEST_TRAIT_NAME_ID},
        std::span<const TypeHandle>{},
        std::span<const sema::TraitObjectAssociatedTypeEquality>{});
}

[[nodiscard]] query::StableFingerprint128 drop_identity_key(const std::string_view symbol)
{
    return query::stable_fingerprint(std::string("m20d.drop:") + std::string(symbol));
}

[[nodiscard]] query::StableFingerprint128 allocator_identity_key(const std::string_view symbol)
{
    return query::stable_fingerprint(std::string("m20d.allocator:") + std::string(symbol));
}

[[nodiscard]] ir::OwnedDynObjectLayoutPrototype make_owned_dyn_prototype(
    Module& module,
    const query::TraitObjectTypeKey& object_key,
    const TypeHandle object_type,
    const TypeHandle data_pointer_type,
    const TypeHandle vtable_pointer_type,
    const std::string_view symbol)
{
    ir::OwnedDynObjectLayoutPrototype prototype = module.make_owned_dyn_object_layout_prototype();
    prototype.object_type_key = object_key;
    prototype.object_type = object_type;
    prototype.data_pointer_type = data_pointer_type;
    prototype.vtable_pointer_type = vtable_pointer_type;
    prototype.symbol = module.intern(symbol);
    prototype.erased_drop_identity_key = drop_identity_key(symbol);
    prototype.allocator_identity_key = allocator_identity_key(symbol);
    return prototype;
}

void append_owned_dyn_prototype(
    Module& module,
    const query::TraitObjectTypeKey& object_key,
    const TypeHandle object_type,
    const TypeHandle data_pointer_type,
    const TypeHandle vtable_pointer_type,
    const std::string_view symbol)
{
    module.owned_dyn_object_layout_prototypes.push_back(
        make_owned_dyn_prototype(module, object_key, object_type, data_pointer_type, vtable_pointer_type, symbol));
}

[[nodiscard]] OwnedDynRuntimeAbiFixture make_valid_runtime_abi_fixture()
{
    OwnedDynRuntimeAbiFixture fixture;
    fixture.object_key = test_trait_object_key(IR_TEST_MODULE_NAME, IR_TEST_TRAIT_NAME);
    fixture.object_type = make_trait_object_type(fixture.module, fixture.object_key, IR_TEST_TRAIT_NAME);
    fixture.data_pointer_type = ptr(
        fixture.module,
        PointerMutability::mut,
        builtin(fixture.module, BuiltinType::u8));
    fixture.vtable_pointer_type = ptr(
        fixture.module,
        PointerMutability::const_,
        builtin(fixture.module, BuiltinType::u8));
    append_owned_dyn_prototype(fixture.module,
        fixture.object_key,
        fixture.object_type,
        fixture.data_pointer_type,
        fixture.vtable_pointer_type,
        IR_TEST_PROTOTYPE_SYMBOL);
    return fixture;
}

[[nodiscard]] ir::OwnedDynObjectLayoutPrototype& prototype(OwnedDynRuntimeAbiFixture& fixture)
{
    return fixture.module.owned_dyn_object_layout_prototypes.front();
}

[[nodiscard]] query::OwnedDynRuntimeLoweringAbiGate adapter_gate(
    const OwnedDynRuntimeAbiFixture& fixture)
{
    return ir::owned_dyn_runtime_lowering_abi_gate(fixture.module);
}

} // namespace

TEST(CoreUnit, OwnedDynRuntimeLoweringAbiAdapterReportsValidModuleGate)
{
    const OwnedDynRuntimeAbiFixture fixture = make_valid_runtime_abi_fixture();
    const base::Result<void> verified = ir::verify_module(fixture.module);
    ASSERT_TRUE(verified) << verified.error().message;

    const query::OwnedDynRuntimeLoweringAbiGate gate = adapter_gate(fixture);
    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_TRUE(query::is_valid_m20d_owned_dyn_runtime_lowering_abi_gate(gate));
    EXPECT_TRUE(query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(gate.drop_allocator_identity_gate));
    EXPECT_EQ(gate.summary.fact_count, 5U);
    EXPECT_EQ(gate.summary.observed_layout_prototype_total, 1U);
    ASSERT_FALSE(gate.facts.empty());
    EXPECT_EQ(gate.facts.front().subject_symbol, IR_TEST_PROTOTYPE_SYMBOL);
    EXPECT_EQ(gate.facts.front().object_type, fixture.object_key);
    EXPECT_NE(gate.facts.front().runtime_abi_descriptor_key, query::StableFingerprint128{});
    EXPECT_NE(gate.facts.front().backend_helper_identity_key, query::StableFingerprint128{});
    EXPECT_NE(gate.facts.front().runtime_abi_descriptor_key, gate.facts.front().backend_helper_identity_key);
    EXPECT_FALSE(gate.facts.front().backend_helper_callable);
    EXPECT_FALSE(gate.facts.front().executable_runtime_implemented);

    const std::string dump = query::dump_owned_dyn_runtime_lowering_abi_gate(gate);
    EXPECT_TRUE(contains_text(dump, "owned_dyn_runtime_abi_descriptor_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_backend_helper_prerequisite_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_dynamic_drop_runtime_blocker_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "prototype_count=1")) << dump;
    EXPECT_TRUE(contains_text(dump, "backend_runtime_helper_not_callable_in_m20d")) << dump;

    const std::string module_dump = ir::dump_module(fixture.module);
    EXPECT_TRUE(contains_text(module_dump, "drop_slot=blocked")) << module_dump;
    EXPECT_TRUE(contains_text(module_dump, "allocator_slot=blocked")) << module_dump;
}

TEST(CoreUnit, OwnedDynRuntimeLoweringAbiAdapterCountsMultipleValidPrototypesStably)
{
    OwnedDynRuntimeAbiFixture fixture = make_valid_runtime_abi_fixture();
    const query::TraitObjectTypeKey other_key = test_trait_object_key("owned_dyn_runtime_abi", "Shade");
    const TypeHandle other_object_type = make_trait_object_type(fixture.module, other_key, "Shade");
    append_owned_dyn_prototype(fixture.module,
        other_key,
        other_object_type,
        fixture.data_pointer_type,
        fixture.vtable_pointer_type,
        "__aurex_owned_dyn_runtime_abi_shade");

    ASSERT_TRUE(ir::verify_module(fixture.module));
    const query::OwnedDynRuntimeLoweringAbiGate gate = adapter_gate(fixture);
    EXPECT_TRUE(query::is_valid_m20d_owned_dyn_runtime_lowering_abi_gate(gate));
    EXPECT_EQ(gate.summary.observed_layout_prototype_total, 2U);
    for (const query::OwnedDynRuntimeLoweringAbiFact& fact : gate.facts) {
        EXPECT_EQ(fact.layout_prototype_count, 2U);
        EXPECT_EQ(fact.prototype_identity_set_key, gate.facts.front().prototype_identity_set_key);
        EXPECT_EQ(fact.runtime_abi_descriptor_key, gate.facts.front().runtime_abi_descriptor_key);
        EXPECT_EQ(fact.backend_helper_identity_key, gate.facts.front().backend_helper_identity_key);
    }

    const query::StableFingerprint128 initial_descriptor = gate.facts.front().runtime_abi_descriptor_key;
    const query::StableFingerprint128 initial_helper = gate.facts.front().backend_helper_identity_key;
    fixture.module.owned_dyn_object_layout_prototypes[1].allocator_identity_key =
        allocator_identity_key("__aurex_owned_dyn_runtime_abi_shade_changed");
    const query::OwnedDynRuntimeLoweringAbiGate changed_gate = adapter_gate(fixture);
    EXPECT_TRUE(query::is_valid_m20d_owned_dyn_runtime_lowering_abi_gate(changed_gate));
    EXPECT_NE(changed_gate.facts.front().runtime_abi_descriptor_key, initial_descriptor);
    EXPECT_NE(changed_gate.facts.front().backend_helper_identity_key, initial_helper);

    std::swap(
        fixture.module.owned_dyn_object_layout_prototypes[0],
        fixture.module.owned_dyn_object_layout_prototypes[1]);
    const query::OwnedDynRuntimeLoweringAbiGate reordered_gate = adapter_gate(fixture);
    EXPECT_TRUE(query::is_valid_m20d_owned_dyn_runtime_lowering_abi_gate(reordered_gate));
    EXPECT_EQ(
        reordered_gate.facts.front().runtime_abi_descriptor_key,
        changed_gate.facts.front().runtime_abi_descriptor_key);
    EXPECT_EQ(
        reordered_gate.facts.front().backend_helper_identity_key,
        changed_gate.facts.front().backend_helper_identity_key);
    EXPECT_EQ(reordered_gate.facts.front().object_type, changed_gate.facts.front().object_type);
}

TEST(CoreUnit, OwnedDynRuntimeLoweringAbiAdapterKeepsEmptyModuleInvalid)
{
    const Module module;
    const query::OwnedDynRuntimeLoweringAbiGate gate = ir::owned_dyn_runtime_lowering_abi_gate(module);
    EXPECT_FALSE(query::is_valid(gate));
    EXPECT_FALSE(query::is_valid_m20d_owned_dyn_runtime_lowering_abi_gate(gate));
    EXPECT_FALSE(query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(gate.drop_allocator_identity_gate));
    EXPECT_TRUE(gate.facts.empty());
    EXPECT_EQ(gate.summary.fact_count, 0U);
}

TEST(CoreUnit, OwnedDynRuntimeLoweringAbiAdapterRejectsPrototypeDrift)
{
    const auto expect_invalid_gate = [](auto&& mutate) {
        OwnedDynRuntimeAbiFixture fixture = make_valid_runtime_abi_fixture();
        mutate(fixture);
        const query::OwnedDynRuntimeLoweringAbiGate gate = adapter_gate(fixture);
        EXPECT_FALSE(query::is_valid(gate));
    };

    expect_invalid_gate([](OwnedDynRuntimeAbiFixture& fixture) {
        prototype(fixture).erased_drop_identity_key = query::StableFingerprint128{};
    });
    expect_invalid_gate([](OwnedDynRuntimeAbiFixture& fixture) {
        prototype(fixture).allocator_identity_key = prototype(fixture).erased_drop_identity_key;
    });
    expect_invalid_gate([](OwnedDynRuntimeAbiFixture& fixture) {
        prototype(fixture).runtime_lowering_blocked = false;
    });
    expect_invalid_gate([](OwnedDynRuntimeAbiFixture& fixture) {
        prototype(fixture).backend_helper_blocked = false;
    });
    expect_invalid_gate([](OwnedDynRuntimeAbiFixture& fixture) {
        prototype(fixture).dynamic_drop_runtime_blocked = false;
    });
    expect_invalid_gate([](OwnedDynRuntimeAbiFixture& fixture) {
        prototype(fixture).erased_drop_runtime_slot = IR_TEST_UNEXPECTED_RUNTIME_SLOT;
    });
    expect_invalid_gate([](OwnedDynRuntimeAbiFixture& fixture) {
        prototype(fixture).symbol = ir::INVALID_IR_TEXT_ID;
    });
}

TEST(CoreUnit, OwnedDynRuntimeLoweringAbiQueryDriftCannotOpenExecution)
{
    const OwnedDynRuntimeAbiFixture fixture = make_valid_runtime_abi_fixture();
    query::OwnedDynRuntimeLoweringAbiGate gate = adapter_gate(fixture);
    ASSERT_TRUE(query::is_valid_m20d_owned_dyn_runtime_lowering_abi_gate(gate));

    gate.facts[2].backend_helper_callable = true;
    gate.summary = query::summarize_owned_dyn_runtime_lowering_abi_gate_counts(gate);
    gate.fingerprint = query::owned_dyn_runtime_lowering_abi_gate_fingerprint(gate);
    EXPECT_FALSE(query::is_valid(gate));

    gate = adapter_gate(fixture);
    gate.facts.front().executable_runtime_implemented = true;
    gate.summary = query::summarize_owned_dyn_runtime_lowering_abi_gate_counts(gate);
    gate.fingerprint = query::owned_dyn_runtime_lowering_abi_gate_fingerprint(gate);
    EXPECT_FALSE(query::is_valid(gate));
}

} // namespace aurex::test
