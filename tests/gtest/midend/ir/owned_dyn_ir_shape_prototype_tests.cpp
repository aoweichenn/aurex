#include <aurex/midend/ir/ir_dump.hpp>
#include <aurex/midend/ir/ir_fingerprint.hpp>
#include <aurex/midend/ir/ir_owned_dyn_ir_shape_prototype_gate.hpp>
#include <aurex/midend/ir/verify.hpp>

#include <gtest/support/ir_test_helpers.hpp>

#include <span>
#include <string>
#include <utility>

namespace aurex::test {
namespace {

using namespace irtest;

constexpr base::u8 IR_TEST_INVALID_POLICY_VALUE = 0xffU;
constexpr base::u32 IR_TEST_UNEXPECTED_RUNTIME_SLOT = 0U;
constexpr base::u32 IR_TEST_WRONG_HANDLE_FIELD_COUNT = 3U;
constexpr base::u32 IR_TEST_TRAIT_MODULE_ID = 0U;
constexpr base::u32 IR_TEST_TRAIT_NAME_ID = 7U;
constexpr std::string_view IR_TEST_MODULE_NAME = "owned_dyn_ir";
constexpr std::string_view IR_TEST_TRAIT_NAME = "Draw";
constexpr std::string_view IR_TEST_PROTOTYPE_SYMBOL = "__aurex_owned_dyn_object_owned_dyn_ir_draw";

struct OwnedDynPrototypeFixture {
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
        query::stable_fingerprint(std::string("origin:") + std::string(module_name) + ":"
            + std::string(trait_name)),
        query::stable_fingerprint(std::string("schema:") + std::string(module_name) + ":"
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
    prototype.erased_drop_identity_key =
        query::stable_fingerprint(std::string("drop:") + std::string(symbol));
    prototype.allocator_identity_key =
        query::stable_fingerprint(std::string("allocator:") + std::string(symbol));
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

[[nodiscard]] OwnedDynPrototypeFixture make_valid_owned_dyn_prototype_fixture()
{
    OwnedDynPrototypeFixture fixture;
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

[[nodiscard]] ir::OwnedDynObjectLayoutPrototype& prototype(OwnedDynPrototypeFixture& fixture)
{
    return fixture.module.owned_dyn_object_layout_prototypes.front();
}

[[nodiscard]] query::OwnedDynIrShapePrototypeGate adapter_gate(const OwnedDynPrototypeFixture& fixture)
{
    return ir::owned_dyn_ir_shape_prototype_gate(fixture.module);
}

} // namespace

TEST(CoreUnit, OwnedDynIrShapePrototypePolicyNamesUseInvalidFallback)
{
    EXPECT_EQ(ir::owned_dyn_object_layout_prototype_policy_name(
                  ir::OwnedDynObjectLayoutPrototypePolicy::compiler_owned_handle_metadata_v1),
        "compiler_owned_handle_metadata_v1");
    EXPECT_TRUE(ir::is_valid(
        ir::OwnedDynObjectLayoutPrototypePolicy::compiler_owned_handle_metadata_v1));

    const auto invalid_policy =
        static_cast<ir::OwnedDynObjectLayoutPrototypePolicy>(IR_TEST_INVALID_POLICY_VALUE);
    EXPECT_EQ(ir::owned_dyn_object_layout_prototype_policy_name(invalid_policy), "invalid");
    EXPECT_FALSE(ir::is_valid(invalid_policy));
}

TEST(CoreUnit, OwnedDynIrShapePrototypeVerifiesDumpsAndFingerprints)
{
    OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
    const base::Result<void> verified = ir::verify_module(fixture.module);
    ASSERT_TRUE(verified) << verified.error().message;

    const std::string dump = ir::dump_module(fixture.module);
    EXPECT_TRUE(contains_text(dump, "owned_dyn_object_layout_prototype")) << dump;
    EXPECT_TRUE(contains_text(dump, std::string(IR_TEST_PROTOTYPE_SYMBOL))) << dump;
    EXPECT_TRUE(contains_text(dump, "policy=compiler_owned_handle_metadata_v1")) << dump;
    EXPECT_TRUE(contains_text(dump, "fields=2")) << dump;
    EXPECT_TRUE(contains_text(dump, "data_field=0:*mut u8")) << dump;
    EXPECT_TRUE(contains_text(dump, "vtable_field=1:*const u8")) << dump;
    EXPECT_TRUE(contains_text(dump, "drop_slot=blocked")) << dump;
    EXPECT_TRUE(contains_text(dump, "allocator_slot=blocked")) << dump;
    EXPECT_TRUE(contains_text(dump, "drop_identity=")) << dump;
    EXPECT_TRUE(contains_text(dump, "allocator_identity=")) << dump;
    EXPECT_TRUE(contains_text(dump, "compiler_owned=yes")) << dump;
    EXPECT_TRUE(contains_text(dump, "runtime_blocked=yes")) << dump;
    EXPECT_TRUE(contains_text(dump, "dynamic_drop_blocked=yes")) << dump;

    const query::QueryResultFingerprint initial = ir::layout_abi_fingerprint(fixture.module);
    prototype(fixture).runtime_lowering_blocked = false;
    EXPECT_NE(ir::layout_abi_fingerprint(fixture.module), initial);
}

TEST(CoreUnit, OwnedDynIrShapePrototypeModuleCopyAndMovePreservePrototype)
{
    OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();

    Module copied = fixture.module;
    ASSERT_EQ(copied.owned_dyn_object_layout_prototypes.size(), 1U);
    EXPECT_EQ(copied.text(copied.owned_dyn_object_layout_prototypes.front().symbol),
        IR_TEST_PROTOTYPE_SYMBOL);
    EXPECT_TRUE(ir::verify_module(copied));

    Module moved = std::move(copied);
    ASSERT_EQ(moved.owned_dyn_object_layout_prototypes.size(), 1U);
    EXPECT_EQ(moved.text(moved.owned_dyn_object_layout_prototypes.front().symbol),
        IR_TEST_PROTOTYPE_SYMBOL);
    EXPECT_TRUE(ir::verify_module(moved));
}

TEST(CoreUnit, OwnedDynIrShapePrototypeVerifierRejectsShapeDrift)
{
    {
        OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
        const query::TraitObjectTypeKey other_key = test_trait_object_key("owned_dyn_ir", "Paint");
        prototype(fixture).object_type = make_trait_object_type(fixture.module, other_key, "Paint");
        expect_error_contains(
            ir::verify_module(fixture.module),
            "owned dyn object layout prototype does not match its dyn trait object type");
    }
    {
        OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
        prototype(fixture).data_pointer_type =
            ptr(fixture.module, PointerMutability::const_, builtin(fixture.module, BuiltinType::u8));
        expect_error_contains(
            ir::verify_module(fixture.module),
            "owned dyn object layout prototype pointer field is invalid");
    }
    {
        OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
        prototype(fixture).vtable_pointer_type =
            ptr(fixture.module, PointerMutability::mut, builtin(fixture.module, BuiltinType::u8));
        expect_error_contains(
            ir::verify_module(fixture.module),
            "owned dyn object layout prototype pointer field is invalid");
    }
    {
        OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
        prototype(fixture).handle_field_count = IR_TEST_WRONG_HANDLE_FIELD_COUNT;
        expect_error_contains(
            ir::verify_module(fixture.module),
            "owned dyn object layout prototype must remain a two-field data/vtable handle");
    }
    {
        OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
        prototype(fixture).erased_drop_runtime_slot = IR_TEST_UNEXPECTED_RUNTIME_SLOT;
        expect_error_contains(
            ir::verify_module(fixture.module),
            "owned dyn object layout prototype must remain a two-field data/vtable handle");
    }
    {
        OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
        prototype(fixture).erased_drop_identity_key = query::StableFingerprint128{};
        expect_error_contains(
            ir::verify_module(fixture.module),
            "owned dyn object layout prototype drop/allocator identity is invalid");
    }
    {
        OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
        prototype(fixture).allocator_identity_key = prototype(fixture).erased_drop_identity_key;
        expect_error_contains(
            ir::verify_module(fixture.module),
            "owned dyn object layout prototype drop/allocator identity is invalid");
    }
    {
        OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
        prototype(fixture).runtime_lowering_blocked = false;
        expect_error_contains(
            ir::verify_module(fixture.module),
            "owned dyn object layout prototype must keep stdlib/runtime surfaces blocked");
    }
    {
        OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
        prototype(fixture).box_surface_blocked = false;
        expect_error_contains(
            ir::verify_module(fixture.module),
            "owned dyn object layout prototype must keep stdlib/runtime surfaces blocked");
    }
}

TEST(CoreUnit, OwnedDynIrShapePrototypeVerifierRejectsInvalidAndDuplicateIdentity)
{
    {
        OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
        prototype(fixture).symbol = ir::INVALID_IR_TEXT_ID;
        expect_error_contains(
            ir::verify_module(fixture.module),
            "owned dyn object layout prototype is invalid");
    }
    {
        OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
        ir::OwnedDynObjectLayoutPrototype duplicate = prototype(fixture);
        duplicate.symbol = fixture.module.intern("__aurex_owned_dyn_object_owned_dyn_ir_draw_duplicate");
        duplicate.erased_drop_identity_key = query::stable_fingerprint("duplicate drop");
        duplicate.allocator_identity_key = query::stable_fingerprint("duplicate allocator");
        fixture.module.owned_dyn_object_layout_prototypes.push_back(duplicate);
        expect_error_contains(
            ir::verify_module(fixture.module),
            "owned dyn object layout prototype is invalid");
    }
    {
        OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
        const query::TraitObjectTypeKey other_key = test_trait_object_key("owned_dyn_ir", "Paint");
        const TypeHandle other_object_type = make_trait_object_type(fixture.module, other_key, "Paint");
        append_owned_dyn_prototype(fixture.module,
            other_key,
            other_object_type,
            fixture.data_pointer_type,
            fixture.vtable_pointer_type,
            IR_TEST_PROTOTYPE_SYMBOL);
        expect_error_contains(
            ir::verify_module(fixture.module),
            "owned dyn object layout prototype is invalid");
    }
}

TEST(CoreUnit, OwnedDynIrShapePrototypeAdapterReportsValidModuleGate)
{
    const OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
    const query::OwnedDynIrShapePrototypeGate gate = adapter_gate(fixture);

    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_TRUE(query::is_valid_m20b_owned_dyn_ir_shape_prototype_gate(gate));
    EXPECT_EQ(gate.summary.fact_count, 6U);
    EXPECT_EQ(gate.summary.observed_layout_prototype_total, 1U);
    ASSERT_FALSE(gate.facts.empty());
    EXPECT_EQ(gate.facts.front().subject_symbol, IR_TEST_PROTOTYPE_SYMBOL);
    EXPECT_EQ(gate.facts.front().object_type, fixture.object_key);

    const std::string dump = query::dump_owned_dyn_ir_shape_prototype_gate(gate);
    EXPECT_TRUE(contains_text(dump, "owned_dyn_handle_metadata_ir_shape_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "prototype_count=1")) << dump;
    EXPECT_TRUE(contains_text(dump, "runtime_abi_lowering_not_in_m20b")) << dump;
}

TEST(CoreUnit, OwnedDynIrShapePrototypeAdapterCountsMultipleValidPrototypes)
{
    OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
    const query::TraitObjectTypeKey other_key = test_trait_object_key("owned_dyn_ir", "Paint");
    const TypeHandle other_object_type = make_trait_object_type(fixture.module, other_key, "Paint");
    append_owned_dyn_prototype(fixture.module,
        other_key,
        other_object_type,
        fixture.data_pointer_type,
        fixture.vtable_pointer_type,
        "__aurex_owned_dyn_object_owned_dyn_ir_paint");

    ASSERT_TRUE(ir::verify_module(fixture.module));
    const query::OwnedDynIrShapePrototypeGate gate = adapter_gate(fixture);
    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_TRUE(query::is_valid_m20b_owned_dyn_ir_shape_prototype_gate(gate));
    EXPECT_EQ(gate.summary.observed_layout_prototype_total, 2U);
    for (const query::OwnedDynIrShapePrototypeFact& fact : gate.facts) {
        EXPECT_EQ(fact.layout_prototype_count, 2U);
    }
}

TEST(CoreUnit, OwnedDynIrShapePrototypeAdapterKeepsEmptyModuleInvalid)
{
    const Module module;
    const query::OwnedDynIrShapePrototypeGate gate = ir::owned_dyn_ir_shape_prototype_gate(module);
    EXPECT_FALSE(query::is_valid(gate));
    EXPECT_FALSE(query::is_valid_m20b_owned_dyn_ir_shape_prototype_gate(gate));
    EXPECT_TRUE(gate.facts.empty());
    EXPECT_EQ(gate.summary.fact_count, 0U);
}

TEST(CoreUnit, OwnedDynIrShapePrototypeAdapterRejectsPrototypeDrift)
{
    const auto expect_invalid_gate = [](auto&& mutate) {
        OwnedDynPrototypeFixture fixture = make_valid_owned_dyn_prototype_fixture();
        mutate(fixture);
        const query::OwnedDynIrShapePrototypeGate gate = adapter_gate(fixture);
        EXPECT_FALSE(query::is_valid(gate));
    };

    expect_invalid_gate([](OwnedDynPrototypeFixture& fixture) {
        prototype(fixture).handle_field_count = IR_TEST_WRONG_HANDLE_FIELD_COUNT;
    });
    expect_invalid_gate([](OwnedDynPrototypeFixture& fixture) {
        prototype(fixture).data_pointer_type =
            ptr(fixture.module, PointerMutability::const_, builtin(fixture.module, BuiltinType::u8));
    });
    expect_invalid_gate([](OwnedDynPrototypeFixture& fixture) {
        prototype(fixture).vtable_pointer_type =
            ptr(fixture.module, PointerMutability::mut, builtin(fixture.module, BuiltinType::u8));
    });
    expect_invalid_gate([](OwnedDynPrototypeFixture& fixture) {
        prototype(fixture).erased_drop_runtime_slot = IR_TEST_UNEXPECTED_RUNTIME_SLOT;
    });
    expect_invalid_gate([](OwnedDynPrototypeFixture& fixture) {
        prototype(fixture).allocator_runtime_slot = IR_TEST_UNEXPECTED_RUNTIME_SLOT;
    });
    expect_invalid_gate([](OwnedDynPrototypeFixture& fixture) {
        prototype(fixture).erased_drop_identity_key = query::StableFingerprint128{};
    });
    expect_invalid_gate([](OwnedDynPrototypeFixture& fixture) {
        prototype(fixture).allocator_identity_key = prototype(fixture).erased_drop_identity_key;
    });
    expect_invalid_gate([](OwnedDynPrototypeFixture& fixture) {
        prototype(fixture).runtime_lowering_blocked = false;
    });
    expect_invalid_gate([](OwnedDynPrototypeFixture& fixture) {
        prototype(fixture).symbol = ir::INVALID_IR_TEXT_ID;
    });
    expect_invalid_gate([](OwnedDynPrototypeFixture& fixture) {
        prototype(fixture).policy =
            static_cast<ir::OwnedDynObjectLayoutPrototypePolicy>(IR_TEST_INVALID_POLICY_VALUE);
    });
    expect_invalid_gate([](OwnedDynPrototypeFixture& fixture) {
        ir::OwnedDynObjectLayoutPrototype duplicate = prototype(fixture);
        duplicate.symbol = fixture.module.intern("__aurex_owned_dyn_object_duplicate_key");
        duplicate.erased_drop_identity_key = query::stable_fingerprint("duplicate drop");
        duplicate.allocator_identity_key = query::stable_fingerprint("duplicate allocator");
        fixture.module.owned_dyn_object_layout_prototypes.push_back(duplicate);
    });
    expect_invalid_gate([](OwnedDynPrototypeFixture& fixture) {
        const query::TraitObjectTypeKey other_key = test_trait_object_key("owned_dyn_ir", "Paint");
        prototype(fixture).object_type = make_trait_object_type(fixture.module, other_key, "Paint");
    });
}

} // namespace aurex::test
