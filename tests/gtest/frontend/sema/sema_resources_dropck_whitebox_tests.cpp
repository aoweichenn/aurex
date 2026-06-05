#include <gtest/frontend/sema/sema_whitebox_test_support.hpp>

namespace aurex::test {
namespace {

[[nodiscard]] sema::DestructorInfo add_custom_destructor_info(sema::SemanticAnalyzerCore& analyzer,
    const ModuleId module, const std::string_view function_name, const TypeHandle type,
    const base::u32 part_index = 0)
{
    sema::CheckedModule& checked = analyzer.state_.checked;
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    FunctionSignature signature =
        function_signature(function_name, module, void_type, intern_identifier(analyzer, function_name), checked);
    signature.param_types = {type};
    signature.has_definition = true;
    signature.is_destructor = true;
    signature.part_index = part_index;
    const sema::FunctionLookupKey function = add_function(analyzer, std::move(signature));

    sema::DestructorInfo info;
    info.module = module;
    info.self_type = type;
    info.function_key = function;
    info.part_index = part_index;
    info.fingerprint = sema::destructor_info_fingerprint(info);
    checked.destructors.emplace(type.value, info);
    return info;
}

} // namespace

TEST(CoreUnit, SemanticWhiteBoxResourceSemanticsClassifiesStructuralTypesAndFingerprints)
{
    syntax::AstModule module;
    module.modules = {module_info({"resource_semantics"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle str = types.builtin(BuiltinType::str);
    const TypeHandle pointer = types.pointer(PointerMutability::mut, i32);
    const TypeHandle reference = types.reference(PointerMutability::const_, i32);
    const TypeHandle slice = types.slice(PointerMutability::const_, i32);
    const TypeHandle array = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, i32);
    const TypeHandle tuple = types.tuple({i32, reference});
    const TypeHandle function =
        types.function(sema::FunctionCallConv::aurex, false, false, std::span<const TypeHandle>{}, i32);
    const sema::GenericParamIdentity param_identity = sema::generic_param_identity_from_text("resource_semantics.T");
    const TypeHandle param = types.generic_param(param_identity, SEMA_TEST_GENERIC_PARAM_NAME);
    const query::MemberKey item_member = query::member_key(
        query::DefKey{}, query::MemberKind::associated_type, "Item", SEMA_TEST_GENERIC_FIRST_PARAM_INDEX);
    const TypeHandle projection = types.associated_projection(param, item_member, "Item");
    const TypeHandle generic_tuple = types.tuple({i32, param});
    const TypeHandle invalid_tuple = types.tuple({i32, INVALID_TYPE_HANDLE});
    const TypeHandle empty_tuple = types.tuple(std::span<const TypeHandle>{});
    const TypeHandle record = types.named_struct("resource_semantics.Record", "resource_semantics_Record", false);
    const TypeHandle empty_record =
        types.named_struct("resource_semantics.EmptyRecord", "resource_semantics_EmptyRecord", false);
    const TypeHandle custom_empty_record =
        types.named_struct("resource_semantics.CustomEmptyRecord", "resource_semantics_CustomEmptyRecord", false);
    const TypeHandle missing_record =
        types.named_struct("resource_semantics.MissingRecord", "resource_semantics_MissingRecord", false);
    const TypeHandle recursive_record =
        types.named_struct("resource_semantics.RecursiveRecord", "resource_semantics_RecursiveRecord", false);
    const TypeHandle mutual_record_a =
        types.named_struct("resource_semantics.MutualRecordA", "resource_semantics_MutualRecordA", false);
    const TypeHandle mutual_record_b =
        types.named_struct("resource_semantics.MutualRecordB", "resource_semantics_MutualRecordB", false);
    const TypeHandle opaque = types.opaque_struct("resource_semantics.Opaque", "resource_semantics_Opaque");
    const TypeHandle choice = types.named_enum("resource_semantics.Choice", "resource_semantics_Choice");
    const TypeHandle empty_choice =
        types.named_enum("resource_semantics.EmptyChoice", "resource_semantics_EmptyChoice");
    const TypeHandle missing_choice =
        types.named_enum("resource_semantics.MissingChoice", "resource_semantics_MissingChoice");
    types.set_enum_underlying(choice, i32);
    types.set_enum_underlying(empty_choice, i32);
    TypeHandle deep_tuple = i32;
    for (base::usize depth = 0; depth < SEMA_TEST_RESOURCE_CLASSIFIER_DEEP_CHAIN_LENGTH; ++depth) {
        const std::array<TypeHandle, 1> nested{deep_tuple};
        deep_tuple = types.tuple(nested);
    }

    const StructInfo& record_info = add_struct_info(analyzer, module_id(0), "Record", record);
    const_cast<StructInfo&>(record_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("value"),
            intern_identifier(analyzer, "value"),
            checked.intern_text("resource_semantics_Record_value"),
            module_id(0),
            generic_tuple,
            {},
            syntax::Visibility::public_,
            {},
    });
    static_cast<void>(add_struct_info(analyzer, module_id(0), "EmptyRecord", empty_record));
    static_cast<void>(add_struct_info(analyzer, module_id(0), "CustomEmptyRecord", custom_empty_record));
    static_cast<void>(
        add_custom_destructor_info(analyzer, module_id(0), "drop_custom_empty_record", custom_empty_record));
    const StructInfo& recursive_record_info =
        add_struct_info(analyzer, module_id(0), "RecursiveRecord", recursive_record);
    const_cast<StructInfo&>(recursive_record_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("next"),
            intern_identifier(analyzer, "next"),
            checked.intern_text("resource_semantics_RecursiveRecord_next"),
            module_id(0),
            recursive_record,
            {},
            syntax::Visibility::public_,
            {},
        });
    const StructInfo& mutual_record_a_info = add_struct_info(analyzer, module_id(0), "MutualRecordA", mutual_record_a);
    const_cast<StructInfo&>(mutual_record_a_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("b"),
            intern_identifier(analyzer, "b"),
            checked.intern_text("resource_semantics_MutualRecordA_b"),
            module_id(0),
            mutual_record_b,
            {},
            syntax::Visibility::public_,
            {},
        });
    const StructInfo& mutual_record_b_info = add_struct_info(analyzer, module_id(0), "MutualRecordB", mutual_record_b);
    const_cast<StructInfo&>(mutual_record_b_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("a"),
            intern_identifier(analyzer, "a"),
            checked.intern_text("resource_semantics_MutualRecordB_a"),
            module_id(0),
            mutual_record_a,
            {},
            syntax::Visibility::public_,
            {},
        });
    static_cast<void>(add_enum_case(analyzer, module_id(0), "Choice_value", "value", choice, record, {record}));
    EnumCaseInfo invalid_case = checked.make_enum_case_info();
    invalid_case.name = checked.intern_text("InvalidCase");
    invalid_case.name_id = intern_identifier(analyzer, "InvalidCase");
    invalid_case.case_name = checked.intern_text("invalid");
    invalid_case.case_name_id = intern_identifier(analyzer, "invalid");
    const std::array invalid_payload_types{record};
    invalid_case.payload_types = checked.copy_type_handle_list(invalid_payload_types);
    checked.enum_cases.emplace(semantic_module_key(analyzer, module_id(0), "InvalidCase"), invalid_case);

    const ResourceSemanticsSummary copy_owned{
        ResourceCopyKind::copy,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::trivial,
        ResourceOwnershipKind::owned_value,
    };
    const ResourceSemanticsSummary borrowed{
        ResourceCopyKind::copy,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::trivial,
        ResourceOwnershipKind::borrowed_view,
    };
    const ResourceSemanticsSummary raw{
        ResourceCopyKind::copy,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::trivial,
        ResourceOwnershipKind::raw_pointer,
    };
    const ResourceSemanticsSummary conservative{
        ResourceCopyKind::move_only,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::needs_drop,
        ResourceOwnershipKind::owned_value,
    };
    const ResourceSemanticsSummary copy_needs_drop{
        ResourceCopyKind::copy,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::needs_drop,
        ResourceOwnershipKind::owned_value,
    };
    const ResourceSemanticsSummary shared_must_consume{
        ResourceCopyKind::copy,
        ResourceDiscardKind::must_consume,
        ResourceCleanupKind::needs_drop,
        ResourceOwnershipKind::shared_managed,
    };

    const ResourceSemanticsClassifier resources(checked);
    EXPECT_EQ(resources.classify(i32), copy_owned);
    EXPECT_EQ(resources.classify(str), borrowed);
    EXPECT_EQ(resources.classify(pointer), raw);
    EXPECT_EQ(resources.classify(reference), borrowed);
    EXPECT_EQ(resources.classify(slice), borrowed);
    EXPECT_EQ(resources.classify(function), copy_owned);
    EXPECT_EQ(resources.classify(array), copy_owned);
    EXPECT_EQ(resources.classify(tuple), copy_owned);
    EXPECT_EQ(resources.classify(empty_tuple), copy_owned);
    EXPECT_EQ(resources.classify(deep_tuple), copy_owned);
    EXPECT_EQ(resources.classify(param), conservative);
    EXPECT_EQ(resources.classify(projection), conservative);
    EXPECT_EQ(resources.classify(generic_tuple), conservative);
    EXPECT_EQ(resources.classify(invalid_tuple), conservative);
    EXPECT_EQ(resources.classify(record), conservative);
    EXPECT_EQ(resources.classify(empty_record), copy_owned);
    EXPECT_EQ(resources.classify(custom_empty_record), conservative);
    EXPECT_EQ(resources.classify(missing_record), conservative);
    EXPECT_EQ(resources.classify(recursive_record), conservative);
    EXPECT_EQ(resources.classify(mutual_record_a), conservative);
    EXPECT_EQ(resources.classify(mutual_record_b), conservative);
    EXPECT_EQ(resources.classify(opaque), conservative);
    EXPECT_EQ(resources.classify(choice), conservative);
    EXPECT_EQ(resources.classify(empty_choice), copy_owned);
    EXPECT_EQ(resources.classify(missing_choice), conservative);
    EXPECT_EQ(resources.classify(INVALID_TYPE_HANDLE), conservative);
    EXPECT_EQ(resources.classify(TypeHandle{static_cast<base::u32>(types.size() + SEMA_TEST_MISSING_MODULE_INDEX)}),
        conservative);

    const ResourceSemanticsClassifier copy_resources(checked, [param](const TypeHandle candidate) {
        return candidate.value == param.value;
    });
    EXPECT_EQ(copy_resources.classify(generic_tuple), copy_needs_drop);
    EXPECT_EQ(copy_resources.classify(record), copy_needs_drop);
    EXPECT_EQ(copy_resources.classify(choice), copy_needs_drop);
    const ResourceSemanticsClassifier missing_provider_resources(
        checked, {}, [](const TypeHandle) -> std::optional<std::vector<TypeHandle>> {
            return std::nullopt;
        });
    const ResourceSemanticsClassifier empty_provider_resources(
        checked, {}, [](const TypeHandle) -> std::optional<std::vector<TypeHandle>> {
            return std::vector<TypeHandle>{};
        });
    EXPECT_EQ(missing_provider_resources.classify(tuple), conservative);
    EXPECT_EQ(empty_provider_resources.classify(tuple), copy_owned);
    EXPECT_EQ(sema::resource_semantics_debug_string(copy_owned), "Copy/Discard/Trivial/OwnedValue");
    EXPECT_EQ(sema::resource_semantics_debug_string(shared_must_consume), "Copy/MustConsume/NeedsDrop/SharedManaged");
    EXPECT_TRUE(sema::resource_is_copy(copy_owned));
    EXPECT_FALSE(sema::resource_is_copy(conservative));
    EXPECT_FALSE(sema::resource_needs_drop(copy_owned));
    EXPECT_TRUE(sema::resource_needs_drop(conservative));
    EXPECT_EQ(sema::resource_copy_kind_name(static_cast<ResourceCopyKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::resource_discard_kind_name(static_cast<ResourceDiscardKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::resource_cleanup_kind_name(static_cast<ResourceCleanupKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(
        sema::resource_ownership_kind_name(static_cast<ResourceOwnershipKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::resource_semantics_fingerprint(copy_owned), sema::resource_semantics_fingerprint(copy_owned));
    EXPECT_NE(sema::resource_semantics_fingerprint(copy_owned), sema::resource_semantics_fingerprint(conservative));
    EXPECT_NE(sema::dump_checked_module(checked).find("resource_summaries"), std::string::npos);
    EXPECT_NE(sema::dump_checked_module(checked).find("Copy/Discard/Trivial/OwnedValue"), std::string::npos);
}
TEST(CoreUnit, SemanticWhiteBoxDropGluePlansStructuralGenericAndOpaqueCleanup)
{
    syntax::AstModule module;
    module.modules = {module_info({"drop_glue"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const sema::GenericParamIdentity param_identity = sema::generic_param_identity_from_text("drop_glue.T");
    const TypeHandle param = types.generic_param(param_identity, SEMA_TEST_GENERIC_PARAM_NAME);
    const TypeHandle tuple = types.tuple({i32, param});
    const TypeHandle array = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, tuple);
    const TypeHandle record = types.named_struct("drop_glue.Record", "drop_glue_Record", false);
    const TypeHandle choice = types.named_enum("drop_glue.Choice", "drop_glue_Choice");
    const TypeHandle opaque = types.opaque_struct("drop_glue.Opaque", "drop_glue_Opaque");
    types.set_enum_underlying(choice, i32);

    const StructInfo& record_info = add_struct_info(analyzer, module_id(0), "Record", record);
    const_cast<StructInfo&>(record_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("copy"),
            intern_identifier(analyzer, "copy"),
            checked.intern_text("drop_glue_Record_copy"),
            module_id(0),
            i32,
            {},
            syntax::Visibility::public_,
            {},
        });
    const_cast<StructInfo&>(record_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("owned"),
            intern_identifier(analyzer, "owned"),
            checked.intern_text("drop_glue_Record_owned"),
            module_id(0),
            array,
            {},
            syntax::Visibility::public_,
            {},
        });
    static_cast<void>(add_enum_case(analyzer, module_id(0), "Choice_record", "record", choice, record, {record}));

    base::Result<sema::DropGluePlan> record_plan = sema::build_drop_glue_plan(checked, record);
    ASSERT_TRUE(record_plan) << record_plan.error().message;
    EXPECT_TRUE(sema::drop_glue_plan_needs_drop(record_plan.value()));
    ASSERT_EQ(record_plan.value().steps.size(), 4U);
    EXPECT_EQ(record_plan.value().steps[0].kind, sema::DropGlueStepKind::struct_field);
    EXPECT_EQ(record_plan.value().steps[0].ordinal, 1U);
    EXPECT_EQ(record_plan.value().steps[1].kind, sema::DropGlueStepKind::array_element);
    EXPECT_EQ(record_plan.value().steps[2].kind, sema::DropGlueStepKind::tuple_element);
    EXPECT_EQ(record_plan.value().steps[2].ordinal, 1U);
    EXPECT_EQ(record_plan.value().steps[3].kind, sema::DropGlueStepKind::generic_value);
    EXPECT_NE(record_plan.value().fingerprint.byte_count, 0U);
    EXPECT_EQ(sema::drop_glue_step_kind_name(record_plan.value().steps[0].kind), "struct_field");

    base::Result<sema::DropGluePlan> enum_plan = sema::build_drop_glue_plan(checked, choice);
    ASSERT_TRUE(enum_plan) << enum_plan.error().message;
    ASSERT_FALSE(enum_plan.value().steps.empty());
    EXPECT_EQ(enum_plan.value().steps.front().kind, sema::DropGlueStepKind::enum_payload);

    base::Result<sema::DropGluePlan> opaque_plan = sema::build_drop_glue_plan(checked, opaque);
    ASSERT_TRUE(opaque_plan) << opaque_plan.error().message;
    ASSERT_EQ(opaque_plan.value().steps.size(), 1U);
    EXPECT_EQ(opaque_plan.value().steps.front().kind, sema::DropGlueStepKind::opaque_value);

    base::Result<sema::DropGluePlan> trivial_plan = sema::build_drop_glue_plan(checked, i32);
    ASSERT_TRUE(trivial_plan) << trivial_plan.error().message;
    EXPECT_FALSE(sema::drop_glue_plan_needs_drop(trivial_plan.value()));
    EXPECT_TRUE(trivial_plan.value().steps.empty());
    EXPECT_FALSE(sema::build_drop_glue_plan(checked, INVALID_TYPE_HANDLE));
}

TEST(CoreUnit, SemanticWhiteBoxDropGlueEmitsCustomDestructorBeforeStructuralChildren)
{
    syntax::AstModule module;
    module.modules = {module_info({"drop_glue_custom"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const sema::GenericParamIdentity param_identity = sema::generic_param_identity_from_text("drop_glue_custom.T");
    const TypeHandle param = types.generic_param(param_identity, SEMA_TEST_GENERIC_PARAM_NAME);
    const TypeHandle record = types.named_struct("drop_glue_custom.Record", "drop_glue_custom_Record", false);

    const StructInfo& record_info = add_struct_info(analyzer, module_id(0), "Record", record);
    const_cast<StructInfo&>(record_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("owned"),
            intern_identifier(analyzer, "owned"),
            checked.intern_text("drop_glue_custom_Record_owned"),
            module_id(0),
            param,
            {},
            syntax::Visibility::public_,
            {},
        });
    const sema::DestructorInfo destructor =
        add_custom_destructor_info(analyzer, module_id(0), "drop_record", record);

    base::Result<sema::DropGluePlan> plan = sema::build_drop_glue_plan(checked, record);
    ASSERT_TRUE(plan) << plan.error().message;
    EXPECT_TRUE(sema::drop_glue_plan_needs_drop(plan.value()));
    ASSERT_EQ(plan.value().steps.size(), 3U);
    EXPECT_EQ(plan.value().steps[0].kind, sema::DropGlueStepKind::custom_destructor);
    EXPECT_EQ(plan.value().steps[0].owner_type.value, record.value);
    EXPECT_EQ(plan.value().steps[0].value_type.value, record.value);
    EXPECT_EQ(plan.value().steps[0].destructor_function, destructor.function_key);
    EXPECT_TRUE(sema::resource_needs_drop(plan.value().steps[0].resource));
    EXPECT_EQ(plan.value().steps[1].kind, sema::DropGlueStepKind::struct_field);
    EXPECT_EQ(plan.value().steps[1].owner_type.value, record.value);
    EXPECT_EQ(plan.value().steps[1].value_type.value, param.value);
    EXPECT_EQ(plan.value().steps[1].destructor_function, sema::FunctionLookupKey{});
    EXPECT_EQ(plan.value().steps[2].kind, sema::DropGlueStepKind::generic_value);
    EXPECT_EQ(plan.value().steps[2].owner_type.value, param.value);

    sema::CheckedModule structural_only = checked;
    structural_only.destructors.clear();
    base::Result<sema::DropGluePlan> structural_plan = sema::build_drop_glue_plan(structural_only, record);
    ASSERT_TRUE(structural_plan) << structural_plan.error().message;
    ASSERT_FALSE(structural_plan.value().steps.empty());
    EXPECT_EQ(structural_plan.value().steps.front().kind, sema::DropGlueStepKind::struct_field);
    EXPECT_NE(structural_plan.value().fingerprint, plan.value().fingerprint);
}

TEST(CoreUnit, SemanticWhiteBoxDropGlueCoversMissingRecursiveEnumAndInvalidEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({"drop_glue_edges"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle missing_record = types.named_struct("drop_glue_edges.Missing", "drop_glue_edges_Missing", false);
    const TypeHandle recursive_record =
        types.named_struct("drop_glue_edges.Recursive", "drop_glue_edges_Recursive", false);
    const TypeHandle empty_choice = types.named_enum("drop_glue_edges.EmptyChoice", "drop_glue_edges_EmptyChoice");
    const TypeHandle missing_choice =
        types.named_enum("drop_glue_edges.MissingChoice", "drop_glue_edges_MissingChoice");
    const TypeHandle mixed_choice = types.named_enum("drop_glue_edges.MixedChoice", "drop_glue_edges_MixedChoice");
    const query::MemberKey associated_member = query::member_key(
        query::DefKey{}, query::MemberKind::associated_type, "Item", SEMA_TEST_GENERIC_FIRST_PARAM_INDEX);
    const TypeHandle associated_projection = types.associated_projection(recursive_record, associated_member, "Item");
    types.set_enum_underlying(empty_choice, i32);
    types.set_enum_underlying(mixed_choice, i32);

    const StructInfo& recursive_info = add_struct_info(analyzer, module_id(0), "Recursive", recursive_record);
    const_cast<StructInfo&>(recursive_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("next"),
            intern_identifier(analyzer, "next"),
            checked.intern_text("drop_glue_edges_Recursive_next"),
            module_id(0),
            recursive_record,
            {},
            syntax::Visibility::public_,
            {},
        });
    static_cast<void>(add_enum_case(analyzer, module_id(0), "Mixed_i32", "i32", mixed_choice, i32, {i32}));
    static_cast<void>(add_enum_case(
        analyzer, module_id(0), "Mixed_recursive", "recursive", mixed_choice, recursive_record, {recursive_record}));

    base::Result<sema::DropGluePlan> missing_record_plan = sema::build_drop_glue_plan(checked, missing_record);
    ASSERT_TRUE(missing_record_plan) << missing_record_plan.error().message;
    ASSERT_EQ(missing_record_plan.value().steps.size(), 1U);
    EXPECT_EQ(missing_record_plan.value().steps.front().kind, sema::DropGlueStepKind::opaque_value);

    base::Result<sema::DropGluePlan> recursive_plan = sema::build_drop_glue_plan(checked, recursive_record);
    ASSERT_TRUE(recursive_plan) << recursive_plan.error().message;
    ASSERT_EQ(recursive_plan.value().steps.size(), 2U);
    EXPECT_EQ(recursive_plan.value().steps[0].kind, sema::DropGlueStepKind::struct_field);
    EXPECT_EQ(recursive_plan.value().steps[1].kind, sema::DropGlueStepKind::generic_value);

    base::Result<sema::DropGluePlan> empty_enum_plan = sema::build_drop_glue_plan(checked, empty_choice);
    ASSERT_TRUE(empty_enum_plan) << empty_enum_plan.error().message;
    EXPECT_FALSE(sema::drop_glue_plan_needs_drop(empty_enum_plan.value()));
    EXPECT_TRUE(empty_enum_plan.value().steps.empty());

    base::Result<sema::DropGluePlan> missing_enum_plan = sema::build_drop_glue_plan(checked, missing_choice);
    ASSERT_TRUE(missing_enum_plan) << missing_enum_plan.error().message;
    EXPECT_TRUE(sema::drop_glue_plan_needs_drop(missing_enum_plan.value()));
    EXPECT_TRUE(missing_enum_plan.value().steps.empty());

    base::Result<sema::DropGluePlan> mixed_enum_plan = sema::build_drop_glue_plan(checked, mixed_choice);
    ASSERT_TRUE(mixed_enum_plan) << mixed_enum_plan.error().message;
    ASSERT_EQ(mixed_enum_plan.value().steps.size(), 3U);
    EXPECT_EQ(mixed_enum_plan.value().steps.front().kind, sema::DropGlueStepKind::enum_payload);

    base::Result<sema::DropGluePlan> associated_plan = sema::build_drop_glue_plan(checked, associated_projection);
    ASSERT_TRUE(associated_plan) << associated_plan.error().message;
    ASSERT_EQ(associated_plan.value().steps.size(), 1U);
    EXPECT_EQ(associated_plan.value().steps.front().kind, sema::DropGlueStepKind::generic_value);

    EXPECT_EQ(sema::drop_glue_step_kind_name(sema::DropGlueStepKind::custom_destructor), "custom_destructor");
    EXPECT_EQ(sema::drop_glue_step_kind_name(sema::DropGlueStepKind::tuple_element), "tuple_element");
    EXPECT_EQ(sema::drop_glue_step_kind_name(sema::DropGlueStepKind::array_element), "array_element");
    EXPECT_EQ(sema::drop_glue_step_kind_name(sema::DropGlueStepKind::enum_payload), "enum_payload");
    EXPECT_EQ(sema::drop_glue_step_kind_name(sema::DropGlueStepKind::generic_value), "generic_value");
    EXPECT_EQ(sema::drop_glue_step_kind_name(sema::DropGlueStepKind::opaque_value), "opaque_value");
    EXPECT_EQ(
        sema::drop_glue_step_kind_name(static_cast<sema::DropGlueStepKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "invalid");
    EXPECT_FALSE(sema::build_drop_glue_plan(
        checked, TypeHandle{static_cast<base::u32>(types.size() + SEMA_TEST_MISSING_MODULE_INDEX)}));
}
TEST(CoreUnit, SemanticWhiteBoxDropCheckFactsNamesFingerprintsAuthorityAndDump)
{
    syntax::AstModule module;
    module.modules = {module_info({"dropck"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle param =
        types.generic_param(sema::generic_param_identity_from_text("dropck.T"), SEMA_TEST_GENERIC_PARAM_NAME);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_manual");
    const sema::FunctionLookupKey destructor =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_destructor");
    const query::StableFingerprint128 drop_glue_fingerprint = query::stable_fingerprint("dropck.manual.glue");

    sema::DropCheckFact fact;
    fact.type = param;
    fact.destructor_function = destructor;
    fact.required_outlives.push_back(sema::DropCheckRequiredOutlives{
        .type = param,
        .region = 0U,
        .reason = sema::LifetimeConstraintReason::dropck,
        .range = body_loan_test_range(0),
    });
    fact.drop_glue_fingerprint = drop_glue_fingerprint;
    fact.may_observe_fields = true;
    fact.may_move_fields = false;
    fact.fingerprint = sema::drop_check_fact_fingerprint(fact);

    sema::DropActionFact action;
    action.kind = sema::DropCheckActionKind::lexical_cleanup;
    action.action = 0U;
    action.point = 1U;
    action.place = 2U;
    action.type = param;
    action.destructor_key = drop_glue_fingerprint;
    action.range = body_loan_test_range(1);

    sema::DropCheckViolation borrowed_drop;
    borrowed_drop.kind = sema::DropCheckViolationKind::borrowed_drop;
    borrowed_drop.action = action.action;
    borrowed_drop.point = action.point;
    borrowed_drop.place = action.place;
    borrowed_drop.type = param;
    borrowed_drop.region = sema::SEMA_LIFETIME_INVALID_INDEX;
    borrowed_drop.diagnostic_emitted = true;
    borrowed_drop.range = body_loan_test_range(2);

    sema::DropCheckViolation generic_outlives = borrowed_drop;
    generic_outlives.kind = sema::DropCheckViolationKind::generic_type_outlives;
    generic_outlives.region = 0U;
    generic_outlives.diagnostic_emitted = false;

    sema::DropCheckViolation glue_missing = borrowed_drop;
    glue_missing.kind = sema::DropCheckViolationKind::drop_glue_missing;
    glue_missing.action = 1U;
    glue_missing.point = 3U;
    glue_missing.diagnostic_emitted = false;

    sema::FunctionDropCheckFacts facts;
    facts.function = key;
    facts.facts.push_back(fact);
    facts.actions.push_back(action);
    facts.violations.push_back(borrowed_drop);
    facts.violations.push_back(generic_outlives);
    facts.violations.push_back(glue_missing);
    facts.solved = true;
    facts.diagnostic_mode_enforced = true;
    facts.graph_missing = true;
    facts.part_index = 1U;
    facts.fingerprint = sema::function_drop_check_facts_fingerprint(facts);
    checked.dropck_facts[key] = facts;

    EXPECT_EQ(sema::drop_check_action_kind_name(sema::DropCheckActionKind::lexical_cleanup), "lexical_cleanup");
    EXPECT_EQ(sema::drop_check_action_kind_name(sema::DropCheckActionKind::overwrite), "overwrite");
    EXPECT_EQ(sema::drop_check_action_kind_name(sema::DropCheckActionKind::early_exit), "early_exit");
    EXPECT_EQ(sema::drop_check_action_kind_name(sema::DropCheckActionKind::explicit_drop), "explicit_drop");
    EXPECT_EQ(sema::drop_check_action_kind_name(sema::DropCheckActionKind::defer_cleanup), "defer_cleanup");
    EXPECT_EQ(sema::drop_check_action_kind_name(
                  static_cast<sema::DropCheckActionKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::drop_check_violation_kind_name(sema::DropCheckViolationKind::borrowed_drop), "borrowed_drop");
    EXPECT_EQ(sema::drop_check_violation_kind_name(sema::DropCheckViolationKind::borrowed_field_dangling),
        "borrowed_field_dangling");
    EXPECT_EQ(sema::drop_check_violation_kind_name(sema::DropCheckViolationKind::generic_type_outlives),
        "generic_type_outlives");
    EXPECT_EQ(
        sema::drop_check_violation_kind_name(sema::DropCheckViolationKind::destructor_escape), "destructor_escape");
    EXPECT_EQ(
        sema::drop_check_violation_kind_name(sema::DropCheckViolationKind::drop_glue_missing), "drop_glue_missing");
    EXPECT_EQ(sema::drop_check_violation_kind_name(
                  static_cast<sema::DropCheckViolationKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::drop_check_violation_message(sema::DropCheckViolationKind::borrowed_drop),
        "drop check rejected dropping storage while it is still borrowed");
    EXPECT_EQ(sema::drop_check_violation_message(sema::DropCheckViolationKind::borrowed_field_dangling),
        "drop check rejected destructor access to a borrowed field that may dangle");
    EXPECT_EQ(sema::drop_check_violation_message(sema::DropCheckViolationKind::generic_type_outlives),
        "drop check rejected generic drop glue because borrowed contents may not outlive the drop");
    EXPECT_EQ(sema::drop_check_violation_message(sema::DropCheckViolationKind::destructor_escape),
        "drop check rejected destructor body because it may leak borrowed storage");
    EXPECT_EQ(sema::drop_check_violation_message(sema::DropCheckViolationKind::drop_glue_missing),
        "drop check could not build drop glue for this value");
    EXPECT_EQ(sema::drop_check_violation_message(
                  static_cast<sema::DropCheckViolationKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "drop check rejected generic drop glue because borrowed contents may not outlive the drop");

    const query::StableFingerprint128 baseline = sema::function_drop_check_facts_fingerprint(facts);
    sema::FunctionDropCheckFacts range_changed = facts;
    range_changed.facts.front().required_outlives.front().range = body_loan_test_range(4);
    range_changed.actions.front().range = body_loan_test_range(5);
    range_changed.violations.front().range = body_loan_test_range(6);
    EXPECT_EQ(baseline, sema::function_drop_check_facts_fingerprint(range_changed));
    sema::FunctionDropCheckFacts action_changed = facts;
    action_changed.actions.front().kind = sema::DropCheckActionKind::overwrite;
    EXPECT_NE(baseline, sema::function_drop_check_facts_fingerprint(action_changed));
    sema::FunctionDropCheckFacts violation_changed = facts;
    violation_changed.violations.front().kind = sema::DropCheckViolationKind::destructor_escape;
    EXPECT_NE(baseline, sema::function_drop_check_facts_fingerprint(violation_changed));

    const std::string dump = sema::dump_function_drop_check_facts(facts);
    EXPECT_NE(dump.find("dropck_facts"), std::string::npos) << dump;
    EXPECT_NE(dump.find("required=1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("reason=dropck"), std::string::npos) << dump;
    EXPECT_NE(dump.find("borrowed_drop"), std::string::npos) << dump;
    EXPECT_NE(dump.find("drop_glue_missing"), std::string::npos) << dump;
    EXPECT_NE(dump.find("fingerprint="), std::string::npos) << dump;

    sema::FunctionDropCheckFacts sparse_facts;
    sparse_facts.function = sema::FunctionLookupKey{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        sema::INVALID_IDENT_ID,
    };
    sema::DropCheckFact sparse_fact = fact;
    sparse_fact.required_outlives.clear();
    sparse_fact.may_observe_fields = false;
    sparse_fact.may_move_fields = true;
    sparse_facts.facts.push_back(sparse_fact);
    sparse_facts.fingerprint = sema::function_drop_check_facts_fingerprint(sparse_facts);
    const std::string sparse_dump = sema::dump_function_drop_check_facts(sparse_facts);
    const std::string invalid_function_label =
        "function=0:" + std::to_string(sema::SEMA_LOOKUP_INVALID_KEY_PART) + ":-";
    EXPECT_NE(sparse_dump.find(invalid_function_label), std::string::npos) << sparse_dump;
    EXPECT_NE(sparse_dump.find("solved=false"), std::string::npos) << sparse_dump;
    EXPECT_NE(sparse_dump.find("enforced=false"), std::string::npos) << sparse_dump;
    EXPECT_NE(sparse_dump.find("graph_missing=false"), std::string::npos) << sparse_dump;
    EXPECT_NE(sparse_dump.find("observe=false"), std::string::npos) << sparse_dump;
    EXPECT_NE(sparse_dump.find("move=true"), std::string::npos) << sparse_dump;

    sema::CheckedModule copy = checked;
    ASSERT_TRUE(copy.dropck_facts.contains(key));
    EXPECT_EQ(copy.dropck_facts.at(key).facts.size(), 1U);
    EXPECT_EQ(copy.dropck_facts.at(key).violations.size(), 3U);
    const std::string checked_dump = sema::dump_checked_module(copy);
    EXPECT_NE(checked_dump.find("dropck_facts"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("dropck_fact"), std::string::npos) << checked_dump;

    query::TypeCheckBodyAuthority authority;
    authority.checked_body = query::query_result_fingerprint(query::stable_fingerprint("dropck.checked"));
    authority.body_syntax_result = query::query_result_fingerprint(query::stable_fingerprint("dropck.body"));
    authority.signature_result = query::query_result_fingerprint(query::stable_fingerprint("dropck.signature"));
    sema::populate_type_check_body_borrow_authority(authority, checked, key);

    EXPECT_TRUE(authority.has_dropck_facts);
    EXPECT_EQ(authority.dropck_fact_count, 1U);
    EXPECT_EQ(authority.dropck_action_count, 1U);
    EXPECT_EQ(authority.dropck_required_outlives_count, 1U);
    EXPECT_EQ(authority.dropck_violation_count, 3U);
    EXPECT_TRUE(authority.dropck_graph_missing);
    EXPECT_TRUE(authority.dropck_has_emitted_diagnostics);
    EXPECT_TRUE(authority.dropck_has_generic_type_outlives);
    EXPECT_TRUE(authority.dropck_has_borrowed_drop);
    EXPECT_TRUE(authority.dropck_has_drop_glue_missing);
    EXPECT_TRUE(query::is_valid(query::type_check_body_result_fingerprint(authority)));
}
TEST(CoreUnit, SemanticWhiteBoxDropCheckAnalyzerRecordsActionsOutlivesAndBorrowedDrop)
{
    syntax::AstModule module;
    module.modules = {module_info({"dropck_analyze"})};
    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const IdentId function_id = module.intern_identifier("dropck_value");
    const TypeId generic_type = module.push_type(named_node(SEMA_TEST_GENERIC_PARAM_NAME));

    syntax::ParamDecl param_decl;
    param_decl.name = SEMA_TEST_BODY_LOAN_VALUE_NAME;
    param_decl.type = generic_type;
    param_decl.range = body_loan_test_range(0);
    param_decl.name_id = value_id;
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "dropck_value";
    function.name_id = function_id;
    function.params = {param_decl};

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle param =
        types.generic_param(sema::generic_param_identity_from_text("dropck_analyze.T"), SEMA_TEST_GENERIC_PARAM_NAME);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_value");

    FunctionSignature signature =
        function_signature("dropck_value", module_id(SEMA_TEST_ROOT_MODULE_INDEX), void_type, function_id, checked);
    signature.semantic_key = key;
    signature.param_types = {param};
    signature.has_definition = true;
    signature.part_index = 2U;

    sema::BodyFlowGraph graph;
    graph.function = key;
    graph.points.push_back(sema::BodyFlowPoint{
        .kind = sema::BodyFlowPointKind::entry,
        .range = body_loan_test_range(0),
    });
    graph.points.push_back(sema::BodyFlowPoint{
        .kind = sema::BodyFlowPointKind::cleanup_scope,
        .range = body_loan_test_range(1),
    });
    graph.points.push_back(sema::BodyFlowPoint{
        .kind = sema::BodyFlowPointKind::exit,
        .range = body_loan_test_range(2),
    });
    graph.edges.push_back(sema::BodyFlowEdge{.from = 0U, .to = 1U});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 1U, .to = 2U});
    graph.places.push_back(body_loan_local_place(value_id, syntax::INVALID_EXPR_ID, 0));
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 1U,
        .place = 0U,
        .range = body_loan_test_range(1),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 1U,
        .place = 0U,
        .range = body_loan_test_range(2),
    });
    checked.body_flow_graphs[key] = std::move(graph);

    sema::FunctionLifetimeFacts lifetime;
    lifetime.function = key;
    lifetime.return_type = void_type;
    sema::LifetimeRegion region;
    region.kind = sema::LifetimeRegionKind::inferred;
    region.range = body_loan_test_range(0);
    lifetime.regions.push_back(region);
    lifetime.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = 0U,
        .first_point = 0U,
        .last_point = 2U,
        .point_count = 3U,
        .range = body_loan_test_range(0),
    });
    checked.lifetime_facts[key] = std::move(lifetime);

    sema::BodyLoanCheckResult loan;
    loan.function = key;
    loan.diagnostic_mode = sema::BodyLoanDiagnosticMode::enforced;
    loan.conflicts.push_back(sema::BodyLoanConflict{
        .kind = sema::BodyLoanConflictKind::cleanup,
        .action = 0U,
        .point = 1U,
        .place = 0U,
        .diagnostic_emitted = false,
        .range = body_loan_test_range(1),
    });
    loan.conflicts.push_back(sema::BodyLoanConflict{
        .kind = sema::BodyLoanConflictKind::drop,
        .action = 0U,
        .point = 1U,
        .place = 0U,
        .diagnostic_emitted = false,
        .range = body_loan_test_range(1),
    });
    loan.conflicts.push_back(sema::BodyLoanConflict{
        .kind = sema::BodyLoanConflictKind::cleanup,
        .action = 1U,
        .point = 1U,
        .place = 0U,
        .diagnostic_emitted = false,
        .range = body_loan_test_range(2),
    });
    checked.body_loan_checks[key] = std::move(loan);

    sema::SemanticAnalyzerCore::DropCheckAnalyzer(analyzer).analyze(function, key, signature);

    ASSERT_TRUE(checked.dropck_facts.contains(key));
    const sema::FunctionDropCheckFacts& facts = checked.dropck_facts.at(key);
    EXPECT_TRUE(facts.solved);
    EXPECT_TRUE(facts.diagnostic_mode_enforced);
    EXPECT_FALSE(facts.graph_missing);
    ASSERT_EQ(facts.actions.size(), 2U);
    EXPECT_EQ(facts.actions.front().kind, sema::DropCheckActionKind::lexical_cleanup);
    EXPECT_EQ(facts.actions.front().point, 1U);
    ASSERT_EQ(facts.facts.size(), 1U);
    EXPECT_EQ(facts.facts.front().type.value, param.value);
    ASSERT_EQ(facts.facts.front().required_outlives.size(), 1U);
    EXPECT_EQ(facts.facts.front().required_outlives.front().type.value, param.value);
    EXPECT_EQ(facts.facts.front().required_outlives.front().region, 0U);
    EXPECT_EQ(facts.facts.front().required_outlives.front().reason, sema::LifetimeConstraintReason::dropck);
    ASSERT_EQ(facts.violations.size(), 2U);
    EXPECT_TRUE(std::ranges::all_of(facts.violations, [](const sema::DropCheckViolation& violation) {
        return violation.kind == sema::DropCheckViolationKind::borrowed_drop && violation.diagnostic_emitted;
    }));

    ASSERT_TRUE(checked.lifetime_facts.contains(key));
    const sema::FunctionLifetimeFacts& lifetime_after = checked.lifetime_facts.at(key);
    ASSERT_EQ(lifetime_after.type_outlives_constraints.size(), 1U);
    EXPECT_EQ(lifetime_after.type_outlives_constraints.front().type.value, param.value);
    EXPECT_EQ(lifetime_after.type_outlives_constraints.front().region, 0U);
    EXPECT_EQ(lifetime_after.type_outlives_constraints.front().reason, sema::LifetimeConstraintReason::dropck);
    EXPECT_NE(facts.fingerprint.byte_count, 0U);
    EXPECT_NE(lifetime_after.fingerprint.byte_count, 0U);
    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find("drop check rejected dropping storage"), std::string::npos);
    EXPECT_EQ(messages.find("drop check rejected generic drop glue"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxDropCheckAnalyzerUsesCustomDestructorFacts)
{
    syntax::AstModule module;
    module.modules = {module_info({"dropck_custom"})};
    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const IdentId function_id = module.intern_identifier("dropck_custom_value");
    const IdentId wrapper_value_id = module.intern_identifier("wrapper");
    const IdentId wrapper_function_id = module.intern_identifier("dropck_custom_wrapper");
    const TypeId record_type = module.push_type(named_node("Record"));
    const TypeId wrapper_type = module.push_type(named_node("Wrapper"));

    syntax::ParamDecl param_decl;
    param_decl.name = SEMA_TEST_BODY_LOAN_VALUE_NAME;
    param_decl.type = record_type;
    param_decl.range = body_loan_test_range(0);
    param_decl.name_id = value_id;
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "dropck_custom_value";
    function.name_id = function_id;
    function.params = {param_decl};

    syntax::ParamDecl wrapper_param_decl;
    wrapper_param_decl.name = "wrapper";
    wrapper_param_decl.type = wrapper_type;
    wrapper_param_decl.range = body_loan_test_range(3);
    wrapper_param_decl.name_id = wrapper_value_id;
    syntax::ItemNode wrapper_function;
    wrapper_function.kind = syntax::ItemKind::fn_decl;
    wrapper_function.name = "dropck_custom_wrapper";
    wrapper_function.name_id = wrapper_function_id;
    wrapper_function.params = {wrapper_param_decl};

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle record = types.named_struct("dropck_custom.Record", "dropck_custom_Record", false);
    const TypeHandle wrapper = types.named_struct("dropck_custom.Wrapper", "dropck_custom_Wrapper", false);
    static_cast<void>(add_struct_info(analyzer, module_id(0), "Record", record));
    const StructInfo& wrapper_info = add_struct_info(analyzer, module_id(0), "Wrapper", wrapper);
    const_cast<StructInfo&>(wrapper_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("inner"),
            intern_identifier(analyzer, "inner"),
            checked.intern_text("dropck_custom_Wrapper_inner"),
            module_id(0),
            record,
            {},
            syntax::Visibility::public_,
            {},
        });
    const sema::DestructorInfo destructor =
        add_custom_destructor_info(analyzer, module_id(0), "drop_record", record);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_custom_value");
    const sema::FunctionLookupKey wrapper_key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_custom_wrapper");

    FunctionSignature signature = function_signature(
        "dropck_custom_value", module_id(SEMA_TEST_ROOT_MODULE_INDEX), void_type, function_id, checked);
    signature.semantic_key = key;
    signature.param_types = {record};
    signature.has_definition = true;
    FunctionSignature wrapper_signature = function_signature(
        "dropck_custom_wrapper", module_id(SEMA_TEST_ROOT_MODULE_INDEX), void_type, wrapper_function_id, checked);
    wrapper_signature.semantic_key = wrapper_key;
    wrapper_signature.param_types = {wrapper};
    wrapper_signature.has_definition = true;

    sema::BodyFlowGraph graph;
    graph.function = key;
    graph.points.push_back(sema::BodyFlowPoint{
        .kind = sema::BodyFlowPointKind::entry,
        .range = body_loan_test_range(0),
    });
    graph.points.push_back(sema::BodyFlowPoint{
        .kind = sema::BodyFlowPointKind::cleanup_scope,
        .range = body_loan_test_range(1),
    });
    graph.points.push_back(sema::BodyFlowPoint{
        .kind = sema::BodyFlowPointKind::exit,
        .range = body_loan_test_range(2),
    });
    graph.edges.push_back(sema::BodyFlowEdge{.from = 0U, .to = 1U});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 1U, .to = 2U});
    graph.places.push_back(body_loan_local_place(value_id, syntax::INVALID_EXPR_ID, 0));
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 1U,
        .place = 0U,
        .range = body_loan_test_range(1),
    });
    checked.body_flow_graphs[key] = std::move(graph);

    sema::BodyFlowGraph wrapper_graph;
    wrapper_graph.function = wrapper_key;
    wrapper_graph.points.push_back(sema::BodyFlowPoint{
        .kind = sema::BodyFlowPointKind::entry,
        .range = body_loan_test_range(3),
    });
    wrapper_graph.points.push_back(sema::BodyFlowPoint{
        .kind = sema::BodyFlowPointKind::cleanup_scope,
        .range = body_loan_test_range(4),
    });
    wrapper_graph.points.push_back(sema::BodyFlowPoint{
        .kind = sema::BodyFlowPointKind::exit,
        .range = body_loan_test_range(5),
    });
    wrapper_graph.edges.push_back(sema::BodyFlowEdge{.from = 0U, .to = 1U});
    wrapper_graph.edges.push_back(sema::BodyFlowEdge{.from = 1U, .to = 2U});
    wrapper_graph.places.push_back(body_loan_local_place(wrapper_value_id, syntax::INVALID_EXPR_ID, 0));
    wrapper_graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 1U,
        .place = 0U,
        .range = body_loan_test_range(4),
    });
    checked.body_flow_graphs[wrapper_key] = std::move(wrapper_graph);

    sema::SemanticAnalyzerCore::DropCheckAnalyzer(analyzer).analyze(function, key, signature);
    sema::SemanticAnalyzerCore::DropCheckAnalyzer(analyzer).analyze(wrapper_function, wrapper_key, wrapper_signature);

    ASSERT_TRUE(checked.dropck_facts.contains(key));
    const sema::FunctionDropCheckFacts& facts = checked.dropck_facts.at(key);
    EXPECT_TRUE(facts.solved);
    ASSERT_EQ(facts.actions.size(), 1U);
    EXPECT_EQ(facts.actions.front().destructor_key, destructor.fingerprint);
    ASSERT_EQ(facts.facts.size(), 1U);
    EXPECT_EQ(facts.facts.front().type.value, record.value);
    EXPECT_EQ(facts.facts.front().destructor_function, destructor.function_key);
    EXPECT_NE(facts.facts.front().drop_glue_fingerprint, destructor.fingerprint);
    EXPECT_TRUE(facts.facts.front().may_observe_fields);
    EXPECT_TRUE(facts.violations.empty());
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);

    const base::Result<sema::DropGluePlan> wrapper_plan = sema::build_drop_glue_plan(checked, wrapper);
    ASSERT_TRUE(wrapper_plan) << wrapper_plan.error().message;
    ASSERT_FALSE(wrapper_plan.value().steps.empty());
    EXPECT_EQ(wrapper_plan.value().steps.front().kind, sema::DropGlueStepKind::struct_field);
    ASSERT_TRUE(checked.dropck_facts.contains(wrapper_key));
    const sema::FunctionDropCheckFacts& wrapper_facts = checked.dropck_facts.at(wrapper_key);
    ASSERT_EQ(wrapper_facts.actions.size(), 1U);
    EXPECT_EQ(wrapper_facts.actions.front().destructor_key, wrapper_plan.value().fingerprint);
    ASSERT_EQ(wrapper_facts.facts.size(), 1U);
    EXPECT_EQ(wrapper_facts.facts.front().type.value, wrapper.value);
    EXPECT_EQ(wrapper_facts.facts.front().drop_glue_fingerprint, wrapper_plan.value().fingerprint);
    EXPECT_EQ(wrapper_facts.facts.front().destructor_function, sema::FunctionLookupKey{});
    EXPECT_TRUE(wrapper_facts.violations.empty());

    sema::CheckedModule copy = checked;
    ASSERT_TRUE(copy.destructors.contains(record.value));
    EXPECT_EQ(copy.destructors.at(record.value).function_key, destructor.function_key);
    EXPECT_EQ(sema::checked_destructors_fingerprint(copy), sema::checked_destructors_fingerprint(checked));

    sema::CheckedModule changed = checked;
    changed.destructors.at(record.value).part_index = 7U;
    changed.destructors.at(record.value).fingerprint =
        sema::destructor_info_fingerprint(changed.destructors.at(record.value));
    EXPECT_NE(sema::checked_destructors_fingerprint(changed), sema::checked_destructors_fingerprint(checked));

    const std::string dump = sema::dump_checked_module(checked);
    EXPECT_NE(dump.find("destructors 1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("destructor dropck_custom.Record ->"), std::string::npos) << dump;

    query::TypeCheckBodyAuthority authority;
    authority.checked_body = query::query_result_fingerprint(query::stable_fingerprint("dropck.custom.checked"));
    authority.body_syntax_result = query::query_result_fingerprint(query::stable_fingerprint("dropck.custom.body"));
    authority.signature_result = query::query_result_fingerprint(query::stable_fingerprint("dropck.custom.signature"));
    sema::populate_type_check_body_borrow_authority(authority, checked, key);

    EXPECT_TRUE(authority.has_destructor_facts);
    EXPECT_EQ(authority.destructor_count, 1U);
    EXPECT_EQ(authority.destructor_fingerprint, sema::checked_destructors_fingerprint(checked));
    const query::QueryResultFingerprint authority_fingerprint = query::type_check_body_result_fingerprint(authority);
    EXPECT_TRUE(query::is_valid(authority_fingerprint));

    query::TypeCheckBodyAuthority without_destructors = authority;
    without_destructors.has_destructor_facts = false;
    without_destructors.destructor_count = 0U;
    without_destructors.destructor_fingerprint = {};
    EXPECT_NE(query::type_check_body_result_fingerprint(without_destructors), authority_fingerprint);
}

TEST(CoreUnit, SemanticWhiteBoxDropCheckAnalyzerRecordsMissingGraphForLoanConflict)
{
    syntax::AstModule module;
    module.modules = {module_info({"dropck_missing_graph"})};
    const IdentId function_id = module.intern_identifier("dropck_missing_graph");

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "dropck_missing_graph";
    function.name_id = function_id;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_missing_graph");

    FunctionSignature signature = function_signature(
        "dropck_missing_graph", module_id(SEMA_TEST_ROOT_MODULE_INDEX), void_type, function_id, checked);
    signature.semantic_key = key;
    signature.has_definition = true;

    sema::BodyLoanCheckResult loan;
    loan.function = key;
    loan.diagnostic_mode = sema::BodyLoanDiagnosticMode::enforced;
    loan.conflicts.push_back(sema::BodyLoanConflict{
        .kind = sema::BodyLoanConflictKind::reinit,
        .action = 0U,
        .point = 1U,
        .place = 0U,
        .range = body_loan_test_range(0),
    });
    checked.body_loan_checks[key] = std::move(loan);

    sema::SemanticAnalyzerCore::DropCheckAnalyzer(analyzer).analyze(function, key, signature);

    ASSERT_TRUE(checked.dropck_facts.contains(key));
    const sema::FunctionDropCheckFacts& facts = checked.dropck_facts.at(key);
    EXPECT_TRUE(facts.graph_missing);
    EXPECT_TRUE(facts.solved);
    EXPECT_TRUE(facts.actions.empty());
    EXPECT_TRUE(facts.violations.empty());
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}
TEST(CoreUnit, SemanticWhiteBoxDropCheckAnalyzerCoversMalformedPatternsAndProjectionEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({"dropck_edges"})};
    const IdentId function_id = module.intern_identifier("dropck_edges");
    const IdentId owned_id = module.intern_identifier("owned");
    const IdentId slice_id = module.intern_identifier("slice_value");
    const IdentId ptr_id = module.intern_identifier("ptr_value");
    const IdentId missing_id = module.intern_identifier("missing_value");
    const IdentId field_id = module.intern_identifier("field");
    const IdentId missing_field_id = module.intern_identifier("missing_field");
    const TypeId generic_type = module.push_type(named_node(SEMA_TEST_GENERIC_PARAM_NAME));
    const ExprId temporary_expr = push_name(module, "temporary");

    syntax::PatternNode field_binding;
    field_binding.kind = syntax::PatternKind::binding;
    field_binding.binding_name = "field_binding";
    field_binding.binding_name_id = module.intern_identifier("field_binding");
    const syntax::PatternId field_binding_id = module.push_pattern(field_binding);

    syntax::PatternNode non_struct_pattern;
    non_struct_pattern.kind = syntax::PatternKind::struct_;
    non_struct_pattern.struct_name = "NotAStruct";
    non_struct_pattern.field_patterns = {
        syntax::FieldPattern{"field", field_binding_id, body_loan_test_range(0), field_id}};
    const syntax::PatternId non_struct_pattern_id = module.push_pattern(non_struct_pattern);

    syntax::PatternNode missing_field_pattern;
    missing_field_pattern.kind = syntax::PatternKind::struct_;
    missing_field_pattern.struct_name = "Record";
    missing_field_pattern.field_patterns = {
        syntax::FieldPattern{"missing_field", field_binding_id, body_loan_test_range(1), missing_field_id}};
    const syntax::PatternId missing_field_pattern_id = module.push_pattern(missing_field_pattern);

    syntax::StmtNode invalid_pattern_decl;
    invalid_pattern_decl.kind = syntax::StmtKind::let;
    invalid_pattern_decl.pattern = syntax::PatternId{SEMA_TEST_BODY_FLOW_OUT_OF_RANGE_NODE};
    const syntax::StmtId invalid_pattern_decl_id = module.push_stmt(invalid_pattern_decl);

    syntax::StmtNode non_struct_decl;
    non_struct_decl.kind = syntax::StmtKind::let;
    non_struct_decl.pattern = non_struct_pattern_id;
    const syntax::StmtId non_struct_decl_id = module.push_stmt(non_struct_decl);

    syntax::StmtNode missing_field_decl;
    missing_field_decl.kind = syntax::StmtKind::let;
    missing_field_decl.pattern = missing_field_pattern_id;
    const syntax::StmtId missing_field_decl_id = module.push_stmt(missing_field_decl);

    const syntax::StmtId body = push_block(module,
        {invalid_pattern_decl_id, syntax::StmtId{SEMA_TEST_BODY_FLOW_OUT_OF_RANGE_NODE}, non_struct_decl_id,
            missing_field_decl_id});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "dropck_edges";
    function.name_id = function_id;
    function.params = {
        syntax::ParamDecl{"owned", generic_type, {}, owned_id},
        syntax::ParamDecl{"slice_value", generic_type, {}, slice_id},
        syntax::ParamDecl{"ptr_value", generic_type, {}, ptr_id},
    };
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle param =
        types.generic_param(sema::generic_param_identity_from_text("dropck_edges.T"), SEMA_TEST_GENERIC_PARAM_NAME);
    const TypeHandle record = types.named_struct("dropck_edges.Record", "dropck_edges_Record", false);
    const TypeHandle slice_param = types.slice(PointerMutability::const_, param);
    const TypeHandle pointer_param = types.pointer(PointerMutability::const_, param);
    const StructInfo& record_info = add_struct_info(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Record", record);
    const_cast<StructInfo&>(record_info)
        .fields.push_back(struct_field_info(analyzer, "field", module_id(SEMA_TEST_ROOT_MODULE_INDEX), param));
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_edges");
    FunctionSignature signature =
        function_signature("dropck_edges", module_id(SEMA_TEST_ROOT_MODULE_INDEX), void_type, function_id, checked);
    signature.semantic_key = key;
    signature.param_types = {param, slice_param, pointer_param};
    signature.has_definition = true;

    checked.stmt_local_types.assign(analyzer.ctx_.module.stmts.size(), INVALID_TYPE_HANDLE);
    analyzer.record_stmt_local_type(invalid_pattern_decl_id, i32);
    analyzer.record_stmt_local_type(non_struct_decl_id, i32);
    analyzer.record_stmt_local_type(missing_field_decl_id, record);
    prepare_expr_storage(analyzer, analyzer.ctx_.module);
    static_cast<void>(analyzer.record_expr_type(temporary_expr, param));

    sema::BodyFlowGraph graph;
    graph.function = key;
    for (base::u32 point = 0U; point < 4U; ++point) {
        graph.points.push_back(sema::BodyFlowPoint{
            .kind = point == 0U ? sema::BodyFlowPointKind::entry : sema::BodyFlowPointKind::sequence,
            .range = body_loan_test_range(point),
        });
        if (point > 0U) {
            graph.edges.push_back(sema::BodyFlowEdge{.from = point - 1U, .to = point});
        }
    }
    graph.places.push_back(body_loan_local_place(owned_id, syntax::INVALID_EXPR_ID, 0));
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::temporary,
        .root_expr = temporary_expr,
        .projections = {},
        .range = body_loan_test_range(1),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = slice_id,
        .projections = {sema::BodyFlowPlaceProjection{.kind = sema::BodyFlowPlaceProjectionKind::index}},
        .range = body_loan_test_range(2),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = slice_id,
        .projections = {sema::BodyFlowPlaceProjection{.kind = sema::BodyFlowPlaceProjectionKind::slice}},
        .range = body_loan_test_range(3),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = ptr_id,
        .projections = {sema::BodyFlowPlaceProjection{.kind = sema::BodyFlowPlaceProjectionKind::dereference}},
        .range = body_loan_test_range(4),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = missing_id,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::field,
            .field_name_id = field_id,
        }},
        .range = body_loan_test_range(5),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = static_cast<sema::BodyFlowActionKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE),
        .point = 0U,
        .place = 0U,
        .range = body_loan_test_range(0),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = sema::SEMA_BODY_FLOW_INVALID_INDEX,
        .place = 0U,
        .range = body_loan_test_range(1),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 0U,
        .place = 1U,
        .range = body_loan_test_range(2),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 1U,
        .place = 2U,
        .range = body_loan_test_range(3),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 2U,
        .place = 3U,
        .range = body_loan_test_range(4),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 3U,
        .place = 4U,
        .range = body_loan_test_range(5),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 1U,
        .place = 5U,
        .range = body_loan_test_range(6),
    });
    checked.body_flow_graphs[key] = std::move(graph);

    sema::FunctionLifetimeFacts lifetime;
    lifetime.function = key;
    lifetime.return_type = void_type;
    sema::LifetimeRegion region;
    region.kind = sema::LifetimeRegionKind::inferred;
    lifetime.regions.push_back(region);
    lifetime.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = 0U,
        .first_point = 0U,
        .last_point = 3U,
        .point_count = 4U,
        .range = body_loan_test_range(0),
    });
    checked.lifetime_facts[key] = std::move(lifetime);

    sema::SemanticAnalyzerCore::DropCheckAnalyzer(analyzer).analyze(function, key, signature);

    ASSERT_TRUE(checked.dropck_facts.contains(key));
    const sema::FunctionDropCheckFacts& facts = checked.dropck_facts.at(key);
    EXPECT_TRUE(facts.solved);
    EXPECT_FALSE(facts.graph_missing);
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.point == sema::SEMA_BODY_FLOW_INVALID_INDEX;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.place == 1U;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.place == 3U;
    }));
    ASSERT_TRUE(checked.lifetime_facts.contains(key));
    EXPECT_TRUE(std::ranges::any_of(checked.lifetime_facts.at(key).type_outlives_constraints,
        [param](const sema::LifetimeTypeOutlivesConstraint& constraint) {
            return constraint.type.value == param.value && constraint.reason == sema::LifetimeConstraintReason::dropck;
        }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}
TEST(CoreUnit, SemanticWhiteBoxDropCheckAnalyzerCoversProjectedPlacesAndTypeTraversal)
{
    syntax::AstModule module;
    module.modules = {module_info({"dropck_projected"})};
    const IdentId function_id = module.intern_identifier("dropck_projected");
    const IdentId record_id = module.intern_identifier("record");
    const IdentId items_id = module.intern_identifier("items");
    const IdentId ref_id = module.intern_identifier("ref_value");
    const IdentId choice_id = module.intern_identifier("choice");
    const IdentId associated_id = module.intern_identifier("associated");
    const IdentId opaque_id = module.intern_identifier("opaque");
    const IdentId field_id = module.intern_identifier("field");
    const IdentId missing_field_id = module.intern_identifier("missing_field");
    const TypeId generic_type = module.push_type(named_node(SEMA_TEST_GENERIC_PARAM_NAME));

    const auto param_decl = [&module, generic_type](const std::string_view name) {
        syntax::ParamDecl param;
        param.name = name;
        param.type = generic_type;
        param.name_id = module.intern_identifier(name);
        return param;
    };

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "dropck_projected";
    function.name_id = function_id;
    function.params = {
        param_decl("record"),
        param_decl("items"),
        param_decl("ref_value"),
        param_decl("choice"),
        param_decl("associated"),
        param_decl("opaque"),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle param =
        types.generic_param(sema::generic_param_identity_from_text("dropck_projected.T"), SEMA_TEST_GENERIC_PARAM_NAME);
    const TypeHandle record = types.named_struct("dropck_projected.Record", "dropck_projected_Record", false);
    const TypeHandle array = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, param);
    const TypeHandle ref_param = types.reference(PointerMutability::const_, param);
    const TypeHandle choice = types.named_enum("dropck_projected.Choice", "dropck_projected_Choice");
    const query::MemberKey associated_member = query::member_key(
        query::DefKey{}, query::MemberKind::associated_type, "Item", SEMA_TEST_GENERIC_FIRST_PARAM_INDEX);
    const TypeHandle associated_projection = types.associated_projection(record, associated_member, "Item");
    const TypeHandle opaque = types.opaque_struct("dropck_projected.Opaque", "dropck_projected_Opaque");
    const StructInfo& record_info = add_struct_info(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Record", record);
    const_cast<StructInfo&>(record_info)
        .fields.push_back(struct_field_info(analyzer, "field", module_id(SEMA_TEST_ROOT_MODULE_INDEX), param));
    static_cast<void>(add_enum_case(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Choice_payload", "payload", choice, param, {param}));

    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_projected");
    FunctionSignature signature =
        function_signature("dropck_projected", module_id(SEMA_TEST_ROOT_MODULE_INDEX), void_type, function_id, checked);
    signature.semantic_key = key;
    signature.param_types = {record, array, ref_param, choice, associated_projection, opaque};
    signature.has_definition = true;
    signature.part_index = 3U;

    sema::BodyFlowGraph graph;
    graph.function = key;
    for (base::u32 point = 0U; point < 10U; ++point) {
        graph.points.push_back(sema::BodyFlowPoint{
            .kind = point == 0U ? sema::BodyFlowPointKind::entry : sema::BodyFlowPointKind::sequence,
            .range = body_loan_test_range(point),
        });
        if (point > 0U) {
            graph.edges.push_back(sema::BodyFlowEdge{.from = point - 1U, .to = point});
        }
    }
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = record_id,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::field,
            .field_name_id = field_id,
        }},
        .range = body_loan_test_range(0),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = items_id,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::index,
        }},
        .range = body_loan_test_range(1),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = ref_id,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::dereference,
        }},
        .range = body_loan_test_range(2),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = items_id,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::slice,
        }},
        .range = body_loan_test_range(3),
    });
    graph.places.push_back(body_loan_local_place(items_id, syntax::INVALID_EXPR_ID, 4));
    graph.places.push_back(body_loan_local_place(choice_id, syntax::INVALID_EXPR_ID, 5));
    graph.places.push_back(body_loan_local_place(associated_id, syntax::INVALID_EXPR_ID, 6));
    graph.places.push_back(body_loan_local_place(opaque_id, syntax::INVALID_EXPR_ID, 7));
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::unknown,
        .projections = {},
        .range = body_loan_test_range(8),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = record_id,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::field,
            .field_name_id = missing_field_id,
        }},
        .range = body_loan_test_range(9),
    });

    const auto push_action = [&graph](const sema::BodyFlowActionKind kind, const base::u32 point, const base::u32 place,
                                 const base::usize range_index) {
        graph.actions.push_back(sema::BodyFlowAction{
            .kind = kind,
            .point = point,
            .place = place,
            .range = body_loan_test_range(range_index),
        });
    };
    push_action(sema::BodyFlowActionKind::drop, 1U, 0U, 0);
    push_action(sema::BodyFlowActionKind::reinit, 2U, 1U, 1);
    push_action(sema::BodyFlowActionKind::cleanup_scope, 3U, 2U, 2);
    push_action(sema::BodyFlowActionKind::return_, 4U, 3U, 3);
    push_action(sema::BodyFlowActionKind::cleanup_storage, 5U, 4U, 4);
    push_action(sema::BodyFlowActionKind::cleanup_storage, 6U, 5U, 5);
    push_action(sema::BodyFlowActionKind::cleanup_storage, 7U, 6U, 6);
    push_action(sema::BodyFlowActionKind::cleanup_storage, 8U, 7U, 7);
    push_action(sema::BodyFlowActionKind::cleanup_storage, 8U, 8U, 8);
    push_action(sema::BodyFlowActionKind::cleanup_storage, 8U, 9U, 9);
    checked.body_flow_graphs[key] = std::move(graph);

    sema::FunctionLifetimeFacts lifetime;
    lifetime.function = key;
    lifetime.return_type = void_type;
    lifetime.regions.push_back(sema::LifetimeRegion{.kind = sema::LifetimeRegionKind::inferred, .name = {}});
    lifetime.regions.push_back(sema::LifetimeRegion{.kind = sema::LifetimeRegionKind::local, .name = {}});
    lifetime.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = 0U,
        .first_point = 0U,
        .last_point = 4U,
        .point_count = 5U,
        .range = body_loan_test_range(0),
    });
    lifetime.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = 0U,
        .first_point = 0U,
        .last_point = 4U,
        .point_count = 5U,
        .range = body_loan_test_range(1),
    });
    lifetime.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = 1U,
        .first_point = 1U,
        .last_point = 8U,
        .point_count = 8U,
        .range = body_loan_test_range(2),
    });
    lifetime.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = sema::SEMA_LIFETIME_INVALID_INDEX,
        .first_point = 1U,
        .last_point = 0U,
        .point_count = 0U,
        .range = body_loan_test_range(3),
    });
    checked.lifetime_facts[key] = std::move(lifetime);

    sema::SemanticAnalyzerCore::DropCheckAnalyzer(analyzer).analyze(function, key, signature);

    ASSERT_TRUE(checked.dropck_facts.contains(key));
    const sema::FunctionDropCheckFacts& facts = checked.dropck_facts.at(key);
    EXPECT_FALSE(facts.graph_missing);
    EXPECT_TRUE(facts.violations.empty());
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.kind == sema::DropCheckActionKind::explicit_drop;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.kind == sema::DropCheckActionKind::overwrite;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.kind == sema::DropCheckActionKind::defer_cleanup;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.kind == sema::DropCheckActionKind::early_exit;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts.facts, [opaque](const sema::DropCheckFact& fact) {
        return fact.type.value == opaque.value && fact.required_outlives.empty();
    }));
    ASSERT_TRUE(checked.lifetime_facts.contains(key));
    const sema::FunctionLifetimeFacts& lifetime_after = checked.lifetime_facts.at(key);
    EXPECT_TRUE(std::ranges::any_of(lifetime_after.type_outlives_constraints, [array](const auto& constraint) {
        return constraint.type.value == array.value && constraint.reason == sema::LifetimeConstraintReason::dropck;
    }));
    EXPECT_TRUE(std::ranges::any_of(lifetime_after.type_outlives_constraints, [choice](const auto& constraint) {
        return constraint.type.value == choice.value && constraint.reason == sema::LifetimeConstraintReason::dropck;
    }));
    EXPECT_TRUE(
        std::ranges::any_of(lifetime_after.type_outlives_constraints, [associated_projection](const auto& constraint) {
            return constraint.type.value == associated_projection.value
                && constraint.reason == sema::LifetimeConstraintReason::dropck;
        }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}
TEST(CoreUnit, SemanticWhiteBoxBodyMoveAnalysisRecordsOwnedUseModes)
{
    syntax::AstModule module;
    module.modules = {module_info({"owned_use_modes"})};
    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId returned_value = push_name(module, "value");

    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = returned_value;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode identity;
    identity.kind = syntax::ItemKind::fn_decl;
    identity.name = "identity";
    identity.params = {syntax::ParamDecl{"value", i32_type, {}}};
    identity.return_type = i32_type;
    identity.body = body;
    const syntax::ItemId identity_id = module.push_item(identity);
    module.item_modules[identity_id.value] = module_id(0);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    const auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    const sema::CheckedModule& checked = checked_result.value();
    ASSERT_LT(returned_value.value, checked.expr_owned_use_modes.size());
    EXPECT_EQ(checked.expr_owned_use_modes[returned_value.value], sema::OwnedUseMode::owned_copy);
}
} // namespace aurex::test
