#include <aurex/midend/ir/ir_dump.hpp>
#include <aurex/midend/ir/ir_fingerprint.hpp>
#include <aurex/midend/ir/ir_owned_dyn_drop_allocator_identity_gate.hpp>
#include <aurex/midend/ir/verify.hpp>

#include <span>
#include <string>
#include <utility>

#include <gtest/support/ir_test_helpers.hpp>

namespace aurex::test {
namespace {

using namespace irtest;

constexpr base::u32 IR_TEST_TRAIT_MODULE_ID = 0U;
constexpr base::u32 IR_TEST_TRAIT_NAME_ID = 9U;
constexpr std::string_view IR_TEST_MODULE_NAME = "owned_dyn_identity";
constexpr std::string_view IR_TEST_TRAIT_NAME = "Paint";
constexpr std::string_view IR_TEST_PROTOTYPE_SYMBOL = "__aurex_owned_dyn_identity_owned_dyn_identity_paint";
constexpr base::u32 IR_TEST_UNEXPECTED_RUNTIME_SLOT = 0U;

struct OwnedDynIdentityFixture {
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

[[nodiscard]] query::DefKey test_trait_def_key(const std::string_view module_name, const std::string_view trait_name)
{
    return query::def_key_from_stable_id(
        query::stable_definition_id(query::stable_module_id(std::span<const std::string_view>{&module_name, 1U}),
            query::StableSymbolKind::type, trait_name),
        query::DefNamespace::trait_, query::DefKind::trait_);
}

[[nodiscard]] query::TraitObjectTypeKey test_trait_object_key(
    const std::string_view module_name, const std::string_view trait_name)
{
    return query::trait_object_type_key(test_trait_def_key(module_name, trait_name),
        std::span<const query::CanonicalTypeKey>{}, std::span<const query::TraitObjectAssociatedTypeEqualityKey>{},
        query::stable_fingerprint(
            std::string("origin:identity:") + std::string(module_name) + ":" + std::string(trait_name)),
        query::stable_fingerprint(
            std::string("schema:identity:") + std::string(module_name) + ":" + std::string(trait_name)));
}

[[nodiscard]] TypeHandle make_trait_object_type(
    Module& module, const query::TraitObjectTypeKey& object_key, const std::string_view trait_name)
{
    return module.types.trait_object(object_key, trait_name, syntax::ModuleId{IR_TEST_TRAIT_MODULE_ID},
        sema::IdentId{IR_TEST_TRAIT_NAME_ID}, std::span<const TypeHandle>{},
        std::span<const sema::TraitObjectAssociatedTypeEquality>{});
}

[[nodiscard]] query::StableFingerprint128 drop_identity_key(const std::string_view symbol)
{
    return query::stable_fingerprint(std::string("m20c.drop:") + std::string(symbol));
}

[[nodiscard]] query::StableFingerprint128 allocator_identity_key(const std::string_view symbol)
{
    return query::stable_fingerprint(std::string("m20c.allocator:") + std::string(symbol));
}

[[nodiscard]] ir::OwnedDynObjectLayoutPrototype make_owned_dyn_prototype(Module& module,
    const query::TraitObjectTypeKey& object_key, const TypeHandle object_type, const TypeHandle data_pointer_type,
    const TypeHandle vtable_pointer_type, const std::string_view symbol)
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

void append_owned_dyn_prototype(Module& module, const query::TraitObjectTypeKey& object_key,
    const TypeHandle object_type, const TypeHandle data_pointer_type, const TypeHandle vtable_pointer_type,
    const std::string_view symbol)
{
    module.owned_dyn_object_layout_prototypes.push_back(
        make_owned_dyn_prototype(module, object_key, object_type, data_pointer_type, vtable_pointer_type, symbol));
}

[[nodiscard]] OwnedDynIdentityFixture make_valid_identity_fixture()
{
    OwnedDynIdentityFixture fixture;
    fixture.object_key = test_trait_object_key(IR_TEST_MODULE_NAME, IR_TEST_TRAIT_NAME);
    fixture.object_type = make_trait_object_type(fixture.module, fixture.object_key, IR_TEST_TRAIT_NAME);
    fixture.data_pointer_type = ptr(fixture.module, PointerMutability::mut, builtin(fixture.module, BuiltinType::u8));
    fixture.vtable_pointer_type =
        ptr(fixture.module, PointerMutability::const_, builtin(fixture.module, BuiltinType::u8));
    append_owned_dyn_prototype(fixture.module, fixture.object_key, fixture.object_type, fixture.data_pointer_type,
        fixture.vtable_pointer_type, IR_TEST_PROTOTYPE_SYMBOL);
    return fixture;
}

[[nodiscard]] ir::OwnedDynObjectLayoutPrototype& prototype(OwnedDynIdentityFixture& fixture)
{
    return fixture.module.owned_dyn_object_layout_prototypes.front();
}

[[nodiscard]] query::OwnedDynDropAllocatorIdentityGate adapter_gate(const OwnedDynIdentityFixture& fixture)
{
    return ir::owned_dyn_drop_allocator_identity_gate(fixture.module);
}

} // namespace

TEST(CoreUnit, OwnedDynDropAllocatorIdentityVerifiesDumpsAndFingerprints)
{
    OwnedDynIdentityFixture fixture = make_valid_identity_fixture();
    const base::Result<void> verified = ir::verify_module(fixture.module);
    ASSERT_TRUE(verified) << verified.error().message;

    const std::string dump = ir::dump_module(fixture.module);
    EXPECT_TRUE(contains_text(dump, "drop_identity=")) << dump;
    EXPECT_TRUE(contains_text(dump, "allocator_identity=")) << dump;
    EXPECT_TRUE(contains_text(dump, "drop_slot=blocked")) << dump;
    EXPECT_TRUE(contains_text(dump, "allocator_slot=blocked")) << dump;

    const query::QueryResultFingerprint initial = ir::layout_abi_fingerprint(fixture.module);
    prototype(fixture).erased_drop_identity_key = query::stable_fingerprint("changed drop identity");
    EXPECT_NE(ir::layout_abi_fingerprint(fixture.module), initial);
}

TEST(CoreUnit, OwnedDynDropAllocatorIdentityVerifierRejectsIdentityDrift)
{
    {
        OwnedDynIdentityFixture fixture = make_valid_identity_fixture();
        prototype(fixture).erased_drop_identity_key = query::StableFingerprint128{};
        expect_error_contains(
            ir::verify_module(fixture.module), "owned dyn object layout prototype drop/allocator identity is invalid");
    }
    {
        OwnedDynIdentityFixture fixture = make_valid_identity_fixture();
        prototype(fixture).allocator_identity_key = prototype(fixture).erased_drop_identity_key;
        expect_error_contains(
            ir::verify_module(fixture.module), "owned dyn object layout prototype drop/allocator identity is invalid");
    }
    {
        OwnedDynIdentityFixture fixture = make_valid_identity_fixture();
        ir::OwnedDynObjectLayoutPrototype duplicate = prototype(fixture);
        duplicate.object_type_key = test_trait_object_key("owned_dyn_identity", "Blend");
        duplicate.object_type = make_trait_object_type(fixture.module, duplicate.object_type_key, "Blend");
        duplicate.symbol = fixture.module.intern("__aurex_owned_dyn_identity_blend");
        duplicate.allocator_identity_key = allocator_identity_key("__aurex_owned_dyn_identity_blend");
        fixture.module.owned_dyn_object_layout_prototypes.push_back(duplicate);
        expect_error_contains(
            ir::verify_module(fixture.module), "owned dyn object layout prototype drop/allocator identity is invalid");
    }
    {
        OwnedDynIdentityFixture fixture = make_valid_identity_fixture();
        prototype(fixture).allocator_runtime_slot = IR_TEST_UNEXPECTED_RUNTIME_SLOT;
        expect_error_contains(ir::verify_module(fixture.module),
            "owned dyn object layout prototype must remain a two-field data/vtable handle");
    }
}

TEST(CoreUnit, OwnedDynDropAllocatorIdentityAdapterReportsValidModuleGate)
{
    const OwnedDynIdentityFixture fixture = make_valid_identity_fixture();
    const query::OwnedDynDropAllocatorIdentityGate gate = adapter_gate(fixture);

    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_TRUE(query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(gate));
    EXPECT_EQ(gate.summary.fact_count, 5U);
    EXPECT_EQ(gate.summary.observed_layout_prototype_total, 1U);
    ASSERT_FALSE(gate.facts.empty());
    EXPECT_EQ(gate.facts.front().subject_symbol, IR_TEST_PROTOTYPE_SYMBOL);
    EXPECT_EQ(gate.facts.front().object_type, fixture.object_key);
    EXPECT_NE(gate.facts.front().drop_identity_key, query::StableFingerprint128{});
    EXPECT_NE(gate.facts.front().allocator_identity_key, query::StableFingerprint128{});
    EXPECT_NE(gate.facts.front().prototype_identity_set_key, query::StableFingerprint128{});

    const std::string dump = query::dump_owned_dyn_drop_allocator_identity_gate(gate);
    EXPECT_TRUE(contains_text(dump, "owned_dyn_erased_drop_identity_prerequisite_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_allocator_identity_prerequisite_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_handle_identity_binding_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "prototype_count=1")) << dump;
    EXPECT_TRUE(contains_text(dump, "runtime_abi_lowering_not_in_m20c")) << dump;
}

TEST(CoreUnit, OwnedDynDropAllocatorIdentityAdapterCountsMultipleValidPrototypes)
{
    OwnedDynIdentityFixture fixture = make_valid_identity_fixture();
    const query::TraitObjectTypeKey other_key = test_trait_object_key("owned_dyn_identity", "Blend");
    const TypeHandle other_object_type = make_trait_object_type(fixture.module, other_key, "Blend");
    append_owned_dyn_prototype(fixture.module, other_key, other_object_type, fixture.data_pointer_type,
        fixture.vtable_pointer_type, "__aurex_owned_dyn_identity_blend");

    ASSERT_TRUE(ir::verify_module(fixture.module));
    const query::OwnedDynDropAllocatorIdentityGate gate = adapter_gate(fixture);
    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_TRUE(query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(gate));
    EXPECT_EQ(gate.summary.observed_layout_prototype_total, 2U);
    for (const query::OwnedDynDropAllocatorIdentityFact& fact : gate.facts) {
        EXPECT_EQ(fact.layout_prototype_count, 2U);
        EXPECT_EQ(fact.prototype_identity_set_key, gate.facts.front().prototype_identity_set_key);
    }

    const query::StableFingerprint128 initial_fingerprint = gate.fingerprint;
    fixture.module.owned_dyn_object_layout_prototypes[1].allocator_identity_key =
        allocator_identity_key("__aurex_owned_dyn_identity_blend_changed");
    const query::OwnedDynDropAllocatorIdentityGate changed_gate = adapter_gate(fixture);
    EXPECT_TRUE(query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(changed_gate));
    EXPECT_NE(changed_gate.fingerprint, initial_fingerprint);
    EXPECT_NE(changed_gate.facts.front().prototype_identity_set_key, gate.facts.front().prototype_identity_set_key);

    std::swap(
        fixture.module.owned_dyn_object_layout_prototypes[0], fixture.module.owned_dyn_object_layout_prototypes[1]);
    const query::OwnedDynDropAllocatorIdentityGate reordered_gate = adapter_gate(fixture);
    EXPECT_TRUE(query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(reordered_gate));
    EXPECT_EQ(
        reordered_gate.facts.front().prototype_identity_set_key, changed_gate.facts.front().prototype_identity_set_key);
    EXPECT_EQ(reordered_gate.facts.front().object_type, changed_gate.facts.front().object_type);
    EXPECT_EQ(reordered_gate.facts.front().drop_identity_key, changed_gate.facts.front().drop_identity_key);
    EXPECT_EQ(reordered_gate.facts.front().allocator_identity_key, changed_gate.facts.front().allocator_identity_key);
}

TEST(CoreUnit, OwnedDynDropAllocatorIdentityAdapterKeepsEmptyModuleInvalid)
{
    const Module module;
    const query::OwnedDynDropAllocatorIdentityGate gate = ir::owned_dyn_drop_allocator_identity_gate(module);
    EXPECT_FALSE(query::is_valid(gate));
    EXPECT_FALSE(query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(gate));
    EXPECT_TRUE(gate.facts.empty());
    EXPECT_EQ(gate.summary.fact_count, 0U);
}

TEST(CoreUnit, OwnedDynDropAllocatorIdentityAdapterRejectsPrototypeDrift)
{
    const auto expect_invalid_gate = [](auto&& mutate) {
        OwnedDynIdentityFixture fixture = make_valid_identity_fixture();
        mutate(fixture);
        const query::OwnedDynDropAllocatorIdentityGate gate = adapter_gate(fixture);
        EXPECT_FALSE(query::is_valid(gate));
    };

    expect_invalid_gate([](OwnedDynIdentityFixture& fixture) {
        prototype(fixture).erased_drop_identity_key = query::StableFingerprint128{};
    });
    expect_invalid_gate([](OwnedDynIdentityFixture& fixture) {
        prototype(fixture).allocator_identity_key = prototype(fixture).erased_drop_identity_key;
    });
    expect_invalid_gate([](OwnedDynIdentityFixture& fixture) {
        prototype(fixture).allocator_api_blocked = false;
    });
    expect_invalid_gate([](OwnedDynIdentityFixture& fixture) {
        prototype(fixture).dynamic_drop_runtime_blocked = false;
    });
    expect_invalid_gate([](OwnedDynIdentityFixture& fixture) {
        prototype(fixture).allocator_runtime_slot = IR_TEST_UNEXPECTED_RUNTIME_SLOT;
    });
    expect_invalid_gate([](OwnedDynIdentityFixture& fixture) {
        prototype(fixture).symbol = ir::INVALID_IR_TEXT_ID;
    });
}

} // namespace aurex::test
