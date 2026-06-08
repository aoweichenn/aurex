#include <aurex/backend/llvm_backend.hpp>
#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/infrastructure/query/trait_object_key.hpp>
#include <aurex/midend/ir/ir_dyn_abi_facts.hpp>
#include <aurex/midend/ir/ir_dump.hpp>
#include <aurex/midend/ir/ir_fingerprint.hpp>
#include <aurex/midend/ir/lower_ast.hpp>
#include <aurex/midend/ir/verify.hpp>

#include <gtest/support/ir_test_helpers.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::test {
namespace {

using namespace irtest;
using ir::TraitObjectVTableLayout;

constexpr base::SourceId LOWER_AST_DYN_TRAIT_SOURCE_ID{811};

struct DynTraitLoweringFixture {
    syntax::AstModule ast;
    sema::CheckedModule checked;
    ir::Module ir;
};

struct DynTraitIrFixture {
    Module module;
    TypeHandle i32 = sema::INVALID_TYPE_HANDLE;
    TypeHandle file = sema::INVALID_TYPE_HANDLE;
    TypeHandle file_ref = sema::INVALID_TYPE_HANDLE;
    TypeHandle draw = sema::INVALID_TYPE_HANDLE;
    TypeHandle draw_ref = sema::INVALID_TYPE_HANDLE;
    TypeHandle erased_data = sema::INVALID_TYPE_HANDLE;
    TypeHandle vtable_ptr = sema::INVALID_TYPE_HANDLE;
    TypeHandle draw_function = sema::INVALID_TYPE_HANDLE;
    query::VTableLayoutKey layout_key;
    FunctionId draw_function_id = INVALID_FUNCTION_ID;
    FunctionId fill_function_id = INVALID_FUNCTION_ID;
};

[[nodiscard]] syntax::AstModule parse_dyn_trait_lowering_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(LOWER_AST_DYN_TRAIT_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        ADD_FAILURE() << tokens.error().message;
        return {};
    }

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (!parsed) {
        ADD_FAILURE() << parsed.error().message;
        return {};
    }
    if (diagnostics.has_error()) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        return {};
    }
    return parsed.take_value();
}

[[nodiscard]] DynTraitLoweringFixture lower_dyn_trait_source(const std::string_view source)
{
    DynTraitLoweringFixture fixture;
    fixture.ast = parse_dyn_trait_lowering_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(fixture.ast, diagnostics);
    auto checked = analyzer.analyze();
    if (!checked) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        ADD_FAILURE() << checked.error().message;
        return fixture;
    }
    fixture.checked = checked.take_value();
    auto lowered = ir::lower_ast(fixture.ast, fixture.checked);
    if (!lowered) {
        ADD_FAILURE() << lowered.error().message;
        return fixture;
    }
    fixture.ir = lowered.take_value();
    return fixture;
}

[[nodiscard]] base::usize count_values_of_kind(const ir::Module& module, const ValueKind kind) noexcept
{
    base::usize count = 0;
    for (const Value& value : module.values) {
        if (value.kind == kind) {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] const Value* find_value_of_kind(const ir::Module& module, const ValueKind kind) noexcept
{
    for (const Value& value : module.values) {
        if (value.kind == kind) {
            return &value;
        }
    }
    return nullptr;
}

[[nodiscard]] std::optional<query::FunctionDynAbiFacts> find_dyn_abi_facts_by_symbol_fragment(
    const std::vector<query::FunctionDynAbiFacts>& facts, const std::string_view symbol_fragment)
{
    for (const query::FunctionDynAbiFacts& function_facts : facts) {
        if (function_facts.symbol.find(symbol_fragment) != std::string::npos) {
            return function_facts;
        }
    }
    return std::nullopt;
}

[[nodiscard]] Value dyn_trait_typed_value(Module& module, const ValueKind kind, const TypeHandle type)
{
    Value value = module.make_value();
    value.kind = kind;
    value.type = type;
    return value;
}

[[nodiscard]] query::DefKey test_trait_def_key(const std::string_view module_name, const std::string_view trait_name)
{
    return query::def_key_from_stable_id(
        query::stable_definition_id(
            query::stable_module_id(std::span<const std::string_view>{&module_name, 1}),
            query::StableSymbolKind::type,
            trait_name),
        query::DefNamespace::trait_,
        query::DefKind::trait_);
}

[[nodiscard]] query::CanonicalTypeKey test_nominal_type_key(
    const std::string_view module_name, const std::string_view type_name)
{
    const query::DefKey def = query::def_key_from_stable_id(
        query::stable_definition_id(
            query::stable_module_id(std::span<const std::string_view>{&module_name, 1}),
            query::StableSymbolKind::type,
            type_name),
        query::DefNamespace::type,
        query::DefKind::struct_);
    return query::canonical_nominal(def, {});
}

[[nodiscard]] query::TraitObjectTypeKey test_trait_object_key(
    const std::string_view module_name, const std::string_view trait_name)
{
    return query::trait_object_type_key(test_trait_def_key(module_name, trait_name), {},
        std::span<const query::TraitObjectAssociatedTypeEqualityKey>{},
        query::stable_fingerprint(std::string("origin:") + std::string(module_name) + ":" + std::string(trait_name)),
        query::stable_fingerprint(std::string("schema:") + std::string(module_name) + ":" + std::string(trait_name)));
}

[[nodiscard]] DynTraitIrFixture make_dyn_trait_ir_fixture()
{
    DynTraitIrFixture fixture;
    fixture.i32 = builtin(fixture.module, BuiltinType::i32);
    fixture.file = fixture.module.types.named_struct("dyn_ir.File", "dyn_ir_File", false);
    fixture.file_ref = fixture.module.types.reference(PointerMutability::const_, fixture.file);
    const query::TraitObjectTypeKey object_key = test_trait_object_key("dyn_ir", "Draw");
    fixture.draw =
        fixture.module.types.trait_object(object_key, "Draw", syntax::ModuleId{0U}, sema::IdentId{7U}, {}, {});
    fixture.draw_ref = fixture.module.types.reference(PointerMutability::const_, fixture.draw);
    fixture.erased_data = ptr(fixture.module, PointerMutability::const_, builtin(fixture.module, BuiltinType::u8));
    fixture.vtable_ptr = ptr(fixture.module, PointerMutability::const_, builtin(fixture.module, BuiltinType::u8));
    fixture.draw_function =
        fixture.module.types.function(sema::FunctionCallConv::aurex, false, {fixture.erased_data}, fixture.i32);
    fixture.layout_key = query::vtable_layout_key(test_nominal_type_key("dyn_ir", "File"), object_key,
        object_key.object_callability_schema, query::stable_fingerprint("dyn_ir.File:Draw"), 1U);

    Function draw = make_function(fixture.module, "draw", fixture.i32);
    draw.signature_params.push_back(function_param(fixture.module, "self", fixture.file_ref));
    const ValueId self_param =
        add_value(fixture.module, dyn_trait_typed_value(fixture.module, ValueKind::param, fixture.file_ref));
    draw.param_values.push_back(self_param);
    const ValueId result = add_value(fixture.module, integer_value(fixture.module, fixture.i32, "9"));
    const BlockId draw_entry = add_block(fixture.module, draw, "entry");
    draw.blocks[draw_entry.value].values.push_back(self_param);
    draw.blocks[draw_entry.value].values.push_back(result);
    draw.blocks[draw_entry.value].terminator.kind = TerminatorKind::return_;
    draw.blocks[draw_entry.value].terminator.value = result;
    fixture.draw_function_id = add_function(fixture.module, draw);

    ir::TraitObjectVTableLayout layout = fixture.module.make_trait_object_vtable_layout();
    layout.layout_key = fixture.layout_key;
    layout.concrete_type = fixture.file;
    layout.object_type = fixture.draw;
    layout.symbol = fixture.module.intern("__aurex_vtable_dyn_ir_file_draw");
    layout.method_slots.push_back(ir::TraitObjectVTableMethodSlot{
        0U,
        fixture.draw_function_id,
        fixture.draw_function,
        fixture.file_ref,
        fixture.i32,
        fixture.module.intern("draw"),
    });
    fixture.module.trait_object_vtables.push_back(std::move(layout));
    return fixture;
}

void add_second_vtable_slot(DynTraitIrFixture& fixture)
{
    Function fill = make_function(fixture.module, "fill", fixture.i32);
    fill.signature_params.push_back(function_param(fixture.module, "self", fixture.file_ref));
    fill.signature_params.push_back(function_param(fixture.module, "amount", fixture.i32));
    const ValueId self_param =
        add_value(fixture.module, dyn_trait_typed_value(fixture.module, ValueKind::param, fixture.file_ref));
    const ValueId amount_param =
        add_value(fixture.module, dyn_trait_typed_value(fixture.module, ValueKind::param, fixture.i32));
    fill.param_values.push_back(self_param);
    fill.param_values.push_back(amount_param);
    const BlockId fill_entry = add_block(fixture.module, fill, "entry");
    fill.blocks[fill_entry.value].values.push_back(self_param);
    fill.blocks[fill_entry.value].values.push_back(amount_param);
    fill.blocks[fill_entry.value].terminator.kind = TerminatorKind::return_;
    fill.blocks[fill_entry.value].terminator.value = amount_param;
    fixture.fill_function_id = add_function(fixture.module, fill);

    const TypeHandle fill_function =
        fixture.module.types.function(sema::FunctionCallConv::aurex, false, {fixture.erased_data, fixture.i32},
            fixture.i32);
    fixture.module.trait_object_vtables.front().layout_key =
        query::vtable_layout_key(test_nominal_type_key("dyn_ir", "File"), fixture.layout_key.object_type,
            fixture.layout_key.slot_schema, fixture.layout_key.impl_evidence, 2U);
    fixture.layout_key = fixture.module.trait_object_vtables.front().layout_key;
    fixture.module.trait_object_vtables.front().method_slots.push_back(ir::TraitObjectVTableMethodSlot{
        1U,
        fixture.fill_function_id,
        fill_function,
        fixture.file_ref,
        fixture.i32,
        fixture.module.intern("fill"),
    });
}

} // namespace

TEST(CoreUnit, LowerAstDynTraitDynamicDispatchEmitsExplicitIrNodes)
{
    const std::string_view source =
        "module lower_ast_dyn_trait_dispatch;\n"
        "trait Draw {\n"
        "  fn draw(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Draw for File {\n"
        "  fn draw(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "fn render(drawable: &dyn Draw) -> i32 {\n"
        "  return drawable.draw();\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 11 };\n"
        "  let drawable: &dyn Draw = &file;\n"
        "  return render(drawable);\n"
        "}\n";

    const DynTraitLoweringFixture fixture = lower_dyn_trait_source(source);
    ASSERT_EQ(fixture.checked.vtable_layouts.size(), 1U);
    ASSERT_EQ(fixture.checked.vtable_layouts.front().method_slots.size(), 1U);
    EXPECT_EQ(fixture.checked.vtable_layouts.front().method_slots.front().method_name, "draw");
    EXPECT_EQ(fixture.checked.vtable_layouts.front().method_slots.front().origin,
        sema::TraitImplMethodOrigin::impl_override);

    ASSERT_EQ(fixture.ir.trait_object_vtables.size(), 1U);
    const ir::TraitObjectVTableLayout& layout = fixture.ir.trait_object_vtables.front();
    ASSERT_EQ(layout.method_slots.size(), 1U);
    EXPECT_EQ(layout.method_slots.front().slot, 0U);
    EXPECT_EQ(fixture.ir.text(layout.method_slots.front().method_name), "draw");
    EXPECT_EQ(fixture.ir.types.display_name(layout.method_slots.front().receiver_type),
        "&lower_ast_dyn_trait_dispatch.File");
    EXPECT_EQ(fixture.ir.types.display_name(layout.method_slots.front().return_type), "i32");

    EXPECT_EQ(count_values_of_kind(fixture.ir, ValueKind::trait_object_pack), 1U);
    EXPECT_EQ(count_values_of_kind(fixture.ir, ValueKind::trait_object_data), 1U);
    EXPECT_EQ(count_values_of_kind(fixture.ir, ValueKind::trait_object_vtable), 1U);
    EXPECT_EQ(count_values_of_kind(fixture.ir, ValueKind::vtable_slot), 1U);
    const Value* slot = find_value_of_kind(fixture.ir, ValueKind::vtable_slot);
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->vtable_slot, 0U);
    EXPECT_EQ(slot->vtable_layout, layout.layout_key);
    EXPECT_TRUE(fixture.ir.types.is_function(slot->type));

    const base::Result<void> verified = ir::verify_module(fixture.ir);
    ASSERT_TRUE(verified) << verified.error().message;
    const std::string dump = ir::dump_module(fixture.ir);
    expect_contains_all(dump,
        {
            "vtable @__aurex_vtable_",
            "lower_ast_dyn_trait_dispatch.File as dyn Draw",
            "slot 0 @m0_lower_ast_dyn_trait_dispatch_File_trait_impl_Draw__draw",
            "dyn.pack",
            "dyn.data",
            "dyn.vtable",
            "vtable_slot",
            "layout=",
        });
}

TEST(CoreUnit, IrDynAbiFactsProjectFunctionLocalBorrowedDispatch)
{
    const std::string_view source =
        "module ir_dyn_abi_facts;\n"
        "trait Draw {\n"
        "  fn draw(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Draw for File {\n"
        "  fn draw(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "fn render(drawable: &dyn Draw) -> i32 {\n"
        "  return drawable.draw();\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 11 };\n"
        "  let drawable: &dyn Draw = &file;\n"
        "  return render(drawable);\n"
        "}\n";

    const DynTraitLoweringFixture fixture = lower_dyn_trait_source(source);
    const std::vector<query::FunctionDynAbiFacts> all_facts = ir::function_dyn_abi_facts(fixture.ir);
    const std::optional<query::FunctionDynAbiFacts> render_facts =
        find_dyn_abi_facts_by_symbol_fragment(all_facts, "render");
    ASSERT_TRUE(render_facts.has_value());
    EXPECT_TRUE(query::is_valid(*render_facts));
    EXPECT_EQ(render_facts->objects.size(), 1U);
    EXPECT_EQ(render_facts->vtables.size(), 1U);
    EXPECT_EQ(render_facts->vtables.front().slots.size(), 1U);
    EXPECT_EQ(render_facts->coercions.size(), 0U);
    ASSERT_EQ(render_facts->dispatches.size(), 1U);
    EXPECT_EQ(render_facts->dispatches.front().slot, 0U);
    EXPECT_EQ(render_facts->dispatches.front().method_name, "draw");
    EXPECT_TRUE(query::is_valid(render_facts->dispatches.front().layout));
    EXPECT_TRUE(query::is_valid(render_facts->dispatches.front().object_type));
    EXPECT_EQ(render_facts->vtables.front().abi_policy, query::DynAbiPolicy::borrowed_view_v1);
    EXPECT_EQ(render_facts->vtables.front().metadata_policy, query::DynMetadataPolicy::borrowed_methods_only_v1);
    EXPECT_EQ(render_facts->vtables.front().slots.front().method_name, "draw");
    EXPECT_EQ(render_facts->fingerprint, query::function_dyn_abi_facts_fingerprint(*render_facts));

    const std::optional<query::FunctionDynAbiFacts> main_facts =
        find_dyn_abi_facts_by_symbol_fragment(all_facts, "main");
    ASSERT_TRUE(main_facts.has_value());
    EXPECT_TRUE(query::is_valid(*main_facts));
    EXPECT_EQ(main_facts->vtables.size(), 1U);
    EXPECT_EQ(main_facts->dispatches.size(), 0U);
    EXPECT_NE(main_facts->fingerprint, render_facts->fingerprint);

    EXPECT_EQ(all_facts.size(), fixture.ir.functions.size());
    EXPECT_FALSE(ir::function_dyn_abi_facts_by_symbol(fixture.ir, "missing").has_value());

    const std::string summary = query::summarize_function_dyn_abi_facts(*render_facts);
    EXPECT_NE(summary.find("abi=borrowed_view_v1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("metadata=borrowed_methods_only_v1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("first_dispatch=vtable_slot slot=0"), std::string::npos) << summary;
    const std::string dump = query::dump_function_dyn_abi_facts(*render_facts);
    EXPECT_NE(dump.find("dyn_vtable_slot slot=0"), std::string::npos) << dump;
    EXPECT_NE(dump.find("dispatch=vtable_slot"), std::string::npos) << dump;
}

TEST(CoreUnit, IrDynAbiFactsCoverDescriptorEdgesAndInvalidValues)
{
    DynTraitIrFixture fixture = make_dyn_trait_ir_fixture();
    add_second_vtable_slot(fixture);

    Function caller = fixture.module.make_function();
    caller.name = fixture.module.intern("anonymous_dyn_edges");
    FunctionBuilder builder{fixture.module, caller};
    const ValueId data = builder.add(dyn_trait_typed_value(fixture.module, ValueKind::param, fixture.file_ref));

    Value missing_layout_pack = fixture.module.make_value();
    missing_layout_pack.kind = ValueKind::trait_object_pack;
    missing_layout_pack.type = fixture.draw_ref;
    missing_layout_pack.lhs = data;
    const ValueId missing_layout = builder.add(missing_layout_pack);

    Value pack = fixture.module.make_value();
    pack.kind = ValueKind::trait_object_pack;
    pack.type = fixture.draw_ref;
    pack.lhs = data;
    pack.vtable_layout = fixture.layout_key;
    const ValueId packed = builder.add(pack);

    Value dyn_data = fixture.module.make_value();
    dyn_data.kind = ValueKind::trait_object_data;
    dyn_data.type = sema::TypeHandle{base::checked_u32(fixture.module.types.size() + 1U, "dyn abi test type")};
    dyn_data.object = packed;
    dyn_data.vtable_layout = fixture.layout_key;
    const ValueId receiver_data = builder.add(dyn_data);

    Value dyn_vtable = fixture.module.make_value();
    dyn_vtable.kind = ValueKind::trait_object_vtable;
    dyn_vtable.type = fixture.vtable_ptr;
    dyn_vtable.object = packed;
    dyn_vtable.vtable_layout = fixture.layout_key;
    const ValueId vtable = builder.add(dyn_vtable);

    Value first_slot = fixture.module.make_value();
    first_slot.kind = ValueKind::vtable_slot;
    first_slot.type = fixture.draw_function;
    first_slot.object = vtable;
    first_slot.vtable_layout = fixture.layout_key;
    first_slot.vtable_slot = 1U;
    const ValueId callee = builder.add(first_slot);

    Value duplicate_slot = first_slot;
    const ValueId duplicate_callee = builder.add(duplicate_slot);

    Value missing_slot = first_slot;
    missing_slot.vtable_slot = 3U;
    const ValueId missing_callee = builder.add(missing_slot);

    Value call = fixture.module.make_value();
    call.kind = ValueKind::call;
    call.type = fixture.i32;
    call.object = callee;
    call.args.push_back(receiver_data);
    const ValueId result = builder.add(call);

    const BlockId entry = builder.block("entry");
    assign_ir_vector(caller.param_values, {data});
    assign_ir_vector(caller.signature_params, {function_param(fixture.module, "data", fixture.file_ref)});
    assign_ir_vector(caller.blocks[entry.value].values,
        {data, missing_layout, packed, receiver_data, vtable, callee, duplicate_callee, missing_callee, result});
    caller.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    caller.blocks[entry.value].terminator.value = result;
    const FunctionId caller_id = add_function(fixture.module, caller);

    const query::FunctionDynAbiFacts facts = ir::function_dyn_abi_facts(fixture.module,
        fixture.module.functions[caller_id.value]);
    EXPECT_TRUE(query::is_valid(facts));
    EXPECT_TRUE(facts.symbol.empty());
    EXPECT_EQ(facts.objects.size(), 1U);
    ASSERT_EQ(facts.vtables.size(), 1U);
    ASSERT_EQ(facts.vtables.front().slots.size(), 2U);
    EXPECT_EQ(facts.vtables.front().slots[0].method_name, "draw");
    EXPECT_EQ(facts.vtables.front().slots[1].method_name, "fill");
    ASSERT_EQ(facts.dispatches.size(), 1U);
    EXPECT_EQ(facts.dispatches.front().slot, 1U);
    EXPECT_EQ(facts.dispatches.front().method_name, "fill");
    EXPECT_EQ(facts.dispatches.front().function_type_name.find("fn("), 0U);

    const std::string dump = query::dump_function_dyn_abi_facts(facts);
    EXPECT_NE(dump.find("function=<anonymous>"), std::string::npos) << dump;
    EXPECT_NE(dump.find("dyn_vtable_slot slot=1"), std::string::npos) << dump;
    ASSERT_TRUE(ir::function_dyn_abi_facts_by_symbol(fixture.module, "").has_value());
    ASSERT_TRUE(ir::function_dyn_abi_facts_by_symbol(fixture.module, "test_draw").has_value());
}

TEST(CoreUnit, LowerAstDynTraitDefaultMethodSlotBindsInstantiatedFunction)
{
    const std::string_view source =
        "module lower_ast_dyn_trait_default_dispatch;\n"
        "trait Reader {\n"
        "  fn read(self: &Self) -> i32;\n"
        "  fn is_empty(self: &Self) -> bool {\n"
        "    return self.read() == 0;\n"
        "  }\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Reader for File {\n"
        "  fn read(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 0 };\n"
        "  let reader: &dyn Reader = &file;\n"
        "  if reader.is_empty() { return 0; }\n"
        "  return reader.read();\n"
        "}\n";

    const DynTraitLoweringFixture fixture = lower_dyn_trait_source(source);
    ASSERT_EQ(fixture.checked.vtable_layouts.size(), 1U);
    ASSERT_EQ(fixture.checked.vtable_layouts.front().method_slots.size(), 2U);
    bool saw_default_slot = false;
    bool saw_override_slot = false;
    for (const sema::VTableMethodSlotFact& slot : fixture.checked.vtable_layouts.front().method_slots) {
        if (slot.method_name == "read") {
            saw_override_slot = true;
            EXPECT_EQ(slot.origin, sema::TraitImplMethodOrigin::impl_override);
        }
        if (slot.method_name == "is_empty") {
            saw_default_slot = true;
            EXPECT_EQ(slot.origin, sema::TraitImplMethodOrigin::trait_default);
            const auto function = fixture.checked.functions.find(slot.function_key);
            ASSERT_NE(function, fixture.checked.functions.end());
            EXPECT_TRUE(function->second.is_trait_default_method_instance);
        }
    }
    EXPECT_TRUE(saw_override_slot);
    EXPECT_TRUE(saw_default_slot);

    ASSERT_EQ(fixture.ir.trait_object_vtables.size(), 1U);
    ASSERT_EQ(fixture.ir.trait_object_vtables.front().method_slots.size(), 2U);
    EXPECT_EQ(fixture.ir.trait_object_vtables.front().method_slots[0].slot, 0U);
    EXPECT_EQ(fixture.ir.trait_object_vtables.front().method_slots[1].slot, 1U);
    const std::string dump = ir::dump_module(fixture.ir);
    expect_contains_all(dump,
        {
            "slot 0 @m0_lower_ast_dyn_trait_default_dispatch_File_trait_impl_Reader__read",
            "slot 1 @m0_lower_ast_dyn_trait_default_dispatch_File_trait_default_Reader",
            "method=read",
            "method=is_empty",
        });
    const base::Result<void> verified = ir::verify_module(fixture.ir);
    ASSERT_TRUE(verified) << verified.error().message;
}

TEST(CoreUnit, LowerAstDynTraitSupertraitUpcastEmitsRuntimeProjection)
{
    const std::string_view source =
        "module lower_ast_dyn_trait_supertrait_upcast;\n"
        "trait Parent { fn parent(self: &Self) -> i32; }\n"
        "trait Child: Parent { fn child(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Parent for File { fn parent(self: &File) -> i32 { return self.value; } }\n"
        "impl Child for File { fn child(self: &File) -> i32 { return self.value + 1; } }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 21 };\n"
        "  let child: &dyn Child = &file;\n"
        "  let parent: &dyn Parent = child;\n"
        "  return parent.parent();\n"
        "}\n";

    const DynTraitLoweringFixture fixture = lower_dyn_trait_source(source);
    ASSERT_EQ(fixture.checked.trait_object_upcast_coercions.size(), 1U);
    ASSERT_EQ(fixture.ir.trait_object_vtables.size(), 2U);
    EXPECT_EQ(count_values_of_kind(fixture.ir, ValueKind::trait_object_upcast), 1U);
    const Value* upcast = find_value_of_kind(fixture.ir, ValueKind::trait_object_upcast);
    ASSERT_NE(upcast, nullptr);
    EXPECT_TRUE(query::is_valid(upcast->vtable_layout));
    EXPECT_TRUE(query::is_valid(upcast->target_vtable_layout));
    EXPECT_TRUE(query::is_valid(upcast->upcast_key));
    EXPECT_EQ(upcast->vtable_supertrait_edge, 0U);

    const TraitObjectVTableLayout* child_layout = nullptr;
    const TraitObjectVTableLayout* parent_layout = nullptr;
    for (const TraitObjectVTableLayout& layout : fixture.ir.trait_object_vtables) {
        if (fixture.ir.types.display_name(layout.object_type) == "dyn Child") {
            child_layout = &layout;
        }
        if (fixture.ir.types.display_name(layout.object_type) == "dyn Parent") {
            parent_layout = &layout;
        }
    }
    ASSERT_NE(child_layout, nullptr);
    ASSERT_NE(parent_layout, nullptr);
    ASSERT_EQ(child_layout->supertrait_edges.size(), 1U);
    EXPECT_EQ(child_layout->supertrait_edges.front().target_layout, parent_layout->layout_key);
    EXPECT_EQ(child_layout->layout_key.metadata_policy,
        query::TraitObjectMetadataPolicyKey::supertrait_vptr_metadata_v1);

    const base::Result<void> verified = ir::verify_module(fixture.ir);
    ASSERT_TRUE(verified) << verified.error().message;
    const std::string dump = ir::dump_module(fixture.ir);
    expect_contains_all(dump,
        {
            "supertrait_edge 0",
            "dyn Child -> dyn Parent",
            "dyn.upcast",
            "source_layout=",
            "target_layout=",
        });
}

TEST(CoreUnit, LowerAstDynTraitInheritedSupertraitDispatchUpcastsReceiver)
{
    const std::string_view source =
        "module lower_ast_dyn_trait_inherited_dispatch;\n"
        "trait Parent { fn parent(self: &Self) -> i32; }\n"
        "trait Child: Parent { fn child(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Parent for File { fn parent(self: &File) -> i32 { return self.value; } }\n"
        "impl Child for File { fn child(self: &File) -> i32 { return self.value + 1; } }\n"
        "fn render(child: &dyn Child) -> i32 { return child.parent(); }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 34 };\n"
        "  let child: &dyn Child = &file;\n"
        "  return render(child);\n"
        "}\n";

    const DynTraitLoweringFixture fixture = lower_dyn_trait_source(source);
    ASSERT_EQ(fixture.checked.trait_object_upcast_coercions.size(), 1U);
    const sema::TraitMethodCallBinding* parent_call = nullptr;
    for (const sema::TraitMethodCallBinding& call : fixture.checked.trait_method_calls) {
        if (call.method_name == "parent" && call.dispatch == sema::TraitMethodDispatchKind::vtable_slot) {
            parent_call = &call;
            break;
        }
    }
    ASSERT_NE(parent_call, nullptr);
    EXPECT_EQ(fixture.checked.types.display_name(parent_call->receiver_type), "&dyn Child");
    EXPECT_EQ(fixture.checked.types.display_name(parent_call->dispatch_receiver_type), "dyn Parent");
    EXPECT_TRUE(query::is_valid(parent_call->vtable_layout));

    EXPECT_EQ(count_values_of_kind(fixture.ir, ValueKind::trait_object_upcast), 1U);
    EXPECT_EQ(count_values_of_kind(fixture.ir, ValueKind::vtable_slot), 1U);
    const Value* slot = find_value_of_kind(fixture.ir, ValueKind::vtable_slot);
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->vtable_slot, 0U);

    const std::vector<query::FunctionDynAbiFacts> all_facts = ir::function_dyn_abi_facts(fixture.ir);
    const std::optional<query::FunctionDynAbiFacts> render_facts =
        find_dyn_abi_facts_by_symbol_fragment(all_facts, "render");
    ASSERT_TRUE(render_facts.has_value());
    EXPECT_TRUE(query::is_valid(*render_facts));
    EXPECT_EQ(render_facts->upcasts.size(), 1U);
    ASSERT_EQ(render_facts->dispatches.size(), 1U);
    EXPECT_EQ(render_facts->dispatches.front().method_name, "parent");
    EXPECT_EQ(render_facts->upcasts.front().metadata_policy, query::DynMetadataPolicy::supertrait_vptr_metadata_v1);

    const base::Result<void> verified = ir::verify_module(fixture.ir);
    ASSERT_TRUE(verified) << verified.error().message;
}

TEST(CoreUnit, LowerAstDynTraitSupertraitEdgesCoverEveryConcreteChildVtable)
{
    const std::string_view source =
        "module lower_ast_dyn_trait_supertrait_multi_concrete;\n"
        "trait Parent { fn parent(self: &Self) -> i32; }\n"
        "trait Child: Parent { fn child(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "struct Socket { value: i32; }\n"
        "impl Parent for File { fn parent(self: &File) -> i32 { return self.value; } }\n"
        "impl Child for File { fn child(self: &File) -> i32 { return self.value + 100; } }\n"
        "impl Parent for Socket { fn parent(self: &Socket) -> i32 { return self.value + 20; } }\n"
        "impl Child for Socket { fn child(self: &Socket) -> i32 { return self.value + 200; } }\n"
        "fn score(child: &dyn Child) -> i32 { return child.parent() + child.child(); }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 3 };\n"
        "  let socket: Socket = Socket { value: 4 };\n"
        "  return score(&file) + score(&socket);\n"
        "}\n";

    const DynTraitLoweringFixture fixture = lower_dyn_trait_source(source);
    ASSERT_EQ(fixture.checked.vtable_layouts.size(), 4U);
    EXPECT_EQ(count_values_of_kind(fixture.ir, ValueKind::trait_object_upcast), 1U);

    base::usize child_layout_count = 0;
    for (const TraitObjectVTableLayout& layout : fixture.ir.trait_object_vtables) {
        if (fixture.ir.types.display_name(layout.object_type) != "dyn Child") {
            continue;
        }
        ++child_layout_count;
        ASSERT_EQ(layout.supertrait_edges.size(), 1U);
        EXPECT_EQ(layout.supertrait_edges.front().edge_index, 0U);

        const auto target = std::ranges::find_if(fixture.ir.trait_object_vtables,
            [&](const TraitObjectVTableLayout& candidate) {
                return candidate.layout_key == layout.supertrait_edges.front().target_layout;
            });
        ASSERT_NE(target, fixture.ir.trait_object_vtables.end());
        EXPECT_EQ(fixture.ir.types.display_name(target->object_type), "dyn Parent");
        EXPECT_EQ(target->concrete_type.value, layout.concrete_type.value);
    }
    EXPECT_EQ(child_layout_count, 2U);
}

TEST(CoreUnit, IrVerifierCoversDynTraitVtableAndValueInvariants)
{
    {
        DynTraitIrFixture fixture = make_dyn_trait_ir_fixture();
        Function caller = make_function(fixture.module, "caller", fixture.i32);
        FunctionBuilder builder{fixture.module, caller};
        const ValueId data = builder.add(dyn_trait_typed_value(fixture.module, ValueKind::param, fixture.file_ref));

        Value pack = fixture.module.make_value();
        pack.kind = ValueKind::trait_object_pack;
        pack.type = fixture.draw_ref;
        pack.lhs = data;
        pack.vtable_layout = fixture.layout_key;
        const ValueId packed = builder.add(pack);

        Value dyn_data = fixture.module.make_value();
        dyn_data.kind = ValueKind::trait_object_data;
        dyn_data.type = fixture.erased_data;
        dyn_data.object = packed;
        dyn_data.vtable_layout = fixture.layout_key;
        const ValueId receiver_data = builder.add(dyn_data);

        Value dyn_vtable = fixture.module.make_value();
        dyn_vtable.kind = ValueKind::trait_object_vtable;
        dyn_vtable.type = fixture.vtable_ptr;
        dyn_vtable.object = packed;
        dyn_vtable.vtable_layout = fixture.layout_key;
        const ValueId vtable = builder.add(dyn_vtable);

        Value slot = fixture.module.make_value();
        slot.kind = ValueKind::vtable_slot;
        slot.type = fixture.draw_function;
        slot.object = vtable;
        slot.vtable_layout = fixture.layout_key;
        slot.vtable_slot = 0U;
        const ValueId callee = builder.add(slot);

        Value call = fixture.module.make_value();
        call.kind = ValueKind::call;
        call.type = fixture.i32;
        call.object = callee;
        call.args.push_back(receiver_data);
        const ValueId result = builder.add(call);

        const BlockId entry = builder.block("entry");
        assign_ir_vector(caller.param_values, {data});
        assign_ir_vector(caller.signature_params, {function_param(fixture.module, "data", fixture.file_ref)});
        assign_ir_vector(caller.blocks[entry.value].values, {data, packed, receiver_data, vtable, callee, result});
        caller.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        caller.blocks[entry.value].terminator.value = result;
        append_function(fixture.module, caller);

        const base::Result<void> verified = ir::verify_module(fixture.module);
        ASSERT_TRUE(verified) << verified.error().message;
    }
    {
        DynTraitIrFixture fixture = make_dyn_trait_ir_fixture();
        fixture.module.trait_object_vtables.front().method_slots.front().slot = 1U;
        expect_error_contains(ir::verify_module(fixture.module), "vtable slot index is out of range");
    }
    {
        DynTraitIrFixture fixture = make_dyn_trait_ir_fixture();
        fixture.module.trait_object_vtables.front().method_slots.push_back(
            fixture.module.trait_object_vtables.front().method_slots.front());
        fixture.module.trait_object_vtables.front().layout_key.method_slot_count = 2U;
        expect_error_contains(ir::verify_module(fixture.module), "vtable slot index is out of range");
    }
    {
        DynTraitIrFixture fixture = make_dyn_trait_ir_fixture();
        Function bad = make_function(fixture.module, "bad_pack", fixture.draw_ref);
        FunctionBuilder builder{fixture.module, bad};
        const ValueId data = builder.add(dyn_trait_typed_value(fixture.module, ValueKind::param, fixture.i32));
        Value pack = fixture.module.make_value();
        pack.kind = ValueKind::trait_object_pack;
        pack.type = fixture.draw_ref;
        pack.lhs = data;
        pack.vtable_layout = fixture.layout_key;
        const ValueId packed = builder.add(pack);
        const BlockId entry = builder.block("entry");
        assign_ir_vector(bad.param_values, {data});
        assign_ir_vector(bad.signature_params, {function_param(fixture.module, "data", fixture.i32)});
        assign_ir_vector(bad.blocks[entry.value].values, {data, packed});
        bad.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        bad.blocks[entry.value].terminator.value = packed;
        append_function(fixture.module, bad);
        expect_error_contains(ir::verify_module(fixture.module), "dyn pack data must be a pointer or reference");
    }
    {
        DynTraitLoweringFixture fixture = lower_dyn_trait_source(
            "module verifier_dyn_supertrait_edge;\n"
            "trait Parent { fn parent(self: &Self) -> i32; }\n"
            "trait Child: Parent { fn child(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Parent for File { fn parent(self: &File) -> i32 { return self.value; } }\n"
            "impl Child for File { fn child(self: &File) -> i32 { return self.value + 1; } }\n"
            "fn main() -> i32 {\n"
            "  let file: File = File { value: 1 };\n"
            "  let child: &dyn Child = &file;\n"
            "  let parent: &dyn Parent = child;\n"
            "  return parent.parent();\n"
            "}\n");
        ASSERT_EQ(fixture.ir.trait_object_vtables.size(), 2U);
        for (TraitObjectVTableLayout& layout : fixture.ir.trait_object_vtables) {
            if (!layout.supertrait_edges.empty()) {
                layout.supertrait_edges.front().target_layout = {};
                break;
            }
        }
        expect_error_contains(ir::verify_module(fixture.ir), "dyn trait vtable supertrait edge is invalid");
    }
}

TEST(CoreUnit, LlvmBackendDynTraitDispatchEmitsFatViewVtableGlobalAndIndirectCall)
{
    const std::string_view source =
        "module llvm_dyn_trait_dispatch;\n"
        "trait Draw {\n"
        "  fn draw(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Draw for File {\n"
        "  fn draw(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "fn render(drawable: &dyn Draw) -> i32 {\n"
        "  return drawable.draw();\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 13 };\n"
        "  let drawable: &dyn Draw = &file;\n"
        "  return render(drawable);\n"
        "}\n";

    const DynTraitLoweringFixture fixture = lower_dyn_trait_source(source);
    auto output = backend::emit_llvm_ir({&fixture.ir, "llvm_dyn_trait_dispatch_whitebox"});
    ASSERT_TRUE(output) << output.error().message;
    expect_contains_all(output.value().text,
        {
            "@__aurex_vtable_",
            "internal unnamed_addr constant { [1 x ptr], [0 x ptr] }",
            "insertvalue { ptr, ptr }",
            "extractvalue { ptr, ptr }",
            "getelementptr inbounds { [1 x ptr], [0 x ptr] }",
            "load ptr, ptr",
            "call i32 %",
        });
}

TEST(CoreUnit, LlvmBackendDynTraitSupertraitUpcastProjectsParentVtable)
{
    const std::string_view source =
        "module llvm_dyn_trait_supertrait_upcast;\n"
        "trait Parent { fn parent(self: &Self) -> i32; }\n"
        "trait Child: Parent { fn child(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Parent for File { fn parent(self: &File) -> i32 { return self.value; } }\n"
        "impl Child for File { fn child(self: &File) -> i32 { return self.value + 1; } }\n"
        "fn render(child: &dyn Child) -> i32 { return child.parent(); }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 13 };\n"
        "  let child: &dyn Child = &file;\n"
        "  return render(child);\n"
        "}\n";

    const DynTraitLoweringFixture fixture = lower_dyn_trait_source(source);
    auto output = backend::emit_llvm_ir({&fixture.ir, "llvm_dyn_trait_supertrait_upcast_whitebox"});
    ASSERT_TRUE(output) << output.error().message;
    expect_contains_all(output.value().text,
        {
            "{ [1 x ptr], [1 x ptr] }",
            "ptr @__aurex_vtable_",
            "getelementptr inbounds { [1 x ptr], [1 x ptr] }",
            "load ptr, ptr",
            "insertvalue { ptr, ptr }",
            "call i32 %",
        });
}

TEST(CoreUnit, IrDynTraitVtableParticipatesInLayoutFingerprint)
{
    DynTraitIrFixture fixture = make_dyn_trait_ir_fixture();
    const query::QueryResultFingerprint with_slot = ir::layout_abi_fingerprint(fixture.module);
    const query::QueryResultFingerprint function_with_slot =
        ir::function_ir_unit_fingerprint(fixture.module, fixture.module.functions.front());
    fixture.module.trait_object_vtables.front().method_slots.front().method_name = fixture.module.intern("paint");
    const query::QueryResultFingerprint renamed_slot = ir::layout_abi_fingerprint(fixture.module);
    const query::QueryResultFingerprint function_with_renamed_slot =
        ir::function_ir_unit_fingerprint(fixture.module, fixture.module.functions.front());
    EXPECT_NE(with_slot, renamed_slot);
    EXPECT_NE(function_with_slot, function_with_renamed_slot);
}

} // namespace aurex::test
