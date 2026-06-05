#include <gtest/frontend/sema/sema_whitebox_test_support.hpp>

namespace aurex::test {

TEST(CoreUnit, SemanticWhiteBoxVisibilityLatticeAccessAndSurfaceLeaks)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
        module_info({"lib", "two"}),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const sema::DeclContext root_declaration = analyzer.declaration_context(module_id(SEMA_TEST_ROOT_MODULE_INDEX));
    const sema::DeclContext lib_declaration = analyzer.declaration_context(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX));
    const sema::AccessContext root_access = analyzer.current_access_context();
    const sema::VisibilityPolicy policy;

    EXPECT_TRUE(query::is_valid(root_declaration.module));
    EXPECT_TRUE(query::is_valid(root_access.module));
    EXPECT_TRUE(sema::access_context_same_module(root_declaration, root_access));
    EXPECT_FALSE(sema::access_context_same_module(lib_declaration, root_access));
    EXPECT_TRUE(sema::access_context_same_package(lib_declaration, root_access));
    EXPECT_TRUE(policy.can_access(syntax::Visibility::public_, lib_declaration, root_access));
    EXPECT_TRUE(policy.can_access(syntax::Visibility::package_, lib_declaration, root_access));
    EXPECT_FALSE(policy.can_access(syntax::Visibility::private_, lib_declaration, root_access));
    EXPECT_TRUE(policy.can_expose_type(syntax::Visibility::package_, syntax::Visibility::package_));
    EXPECT_FALSE(policy.can_expose_type(syntax::Visibility::public_, syntax::Visibility::package_));

    const std::array<std::string_view, 1> package_one{"package-one"};
    const std::array<std::string_view, 1> package_two{"package-two"};
    const std::array<std::string_view, 1> package_module{"visible"};
    const query::ModuleKey package_one_module = query::module_key(query::package_key(package_one), package_module);
    const query::ModuleKey package_two_module = query::module_key(query::package_key(package_two), package_module);
    const sema::DeclContext package_one_declaration = sema::decl_context_from_module_key(package_one_module);
    const sema::AccessContext package_two_access = sema::access_context_from_module_key(package_two_module);
    EXPECT_FALSE(sema::access_context_same_package(package_one_declaration, package_two_access));
    EXPECT_FALSE(policy.can_access(syntax::Visibility::package_, package_one_declaration, package_two_access));

    EXPECT_TRUE(analyzer.can_access_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), syntax::Visibility::public_));
    EXPECT_TRUE(analyzer.can_access_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), syntax::Visibility::package_));
    EXPECT_FALSE(analyzer.can_access_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), syntax::Visibility::private_));
    EXPECT_TRUE(analyzer.can_access_module(module_id(SEMA_TEST_ROOT_MODULE_INDEX), syntax::Visibility::private_));

    sema::SemanticOptions split_package_options;
    split_package_options.module_packages = {
        query::package_key(package_one),
        query::package_key(package_two),
        query::package_key(package_two),
    };
    sema::SemanticAnalyzerCore split_package_analyzer(module, diagnostics, split_package_options);
    split_package_analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    EXPECT_FALSE(split_package_analyzer.can_access_module(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), syntax::Visibility::package_));
    EXPECT_NE(split_package_analyzer.query_module_key(module_id(SEMA_TEST_ROOT_MODULE_INDEX)).package,
        split_package_analyzer.query_module_key(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX)).package);

    analyzer.state_.flow.current_module = syntax::INVALID_MODULE_ID;
    EXPECT_TRUE(analyzer.can_access_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), syntax::Visibility::public_));
    EXPECT_FALSE(analyzer.can_access_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), syntax::Visibility::package_));
    EXPECT_FALSE(query::is_valid(analyzer.current_access_context().module));

    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    EXPECT_FALSE(analyzer.can_access_module(syntax::INVALID_MODULE_ID, syntax::Visibility::package_));

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle package_type = types.named_struct("root.PackageOnly", "root_PackageOnly", false);
    static_cast<void>(add_struct_info(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "PackageOnly", package_type, syntax::Visibility::package_));

    FunctionSignature public_function =
        indexed_function_signature(analyzer, "leaks_package", module_id(SEMA_TEST_ROOT_MODULE_INDEX), package_type);
    public_function.visibility = syntax::Visibility::public_;
    static_cast<void>(add_function(analyzer, public_function));

    analyzer.validate_exported_signature_surfaces();
    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find("public function `leaks_package` exposes package-visible type `root.PackageOnly`"),
        std::string::npos);

    base::DiagnosticSink package_diagnostics;
    sema::SemanticAnalyzerCore package_analyzer(module, package_diagnostics);
    package_analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    sema::TypeTable& package_types = package_analyzer.state_.checked.types;
    const TypeHandle private_type = package_types.named_struct("root.PrivateOnly", "root_PrivateOnly", false);
    static_cast<void>(add_struct_info(package_analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "PrivateOnly",
        private_type, syntax::Visibility::private_));

    FunctionSignature package_function = indexed_function_signature(
        package_analyzer, "leaks_private", module_id(SEMA_TEST_ROOT_MODULE_INDEX), private_type);
    package_function.visibility = syntax::Visibility::package_;
    static_cast<void>(add_function(package_analyzer, package_function));

    package_analyzer.validate_exported_signature_surfaces();
    const std::string package_messages = diagnostic_messages(package_diagnostics);
    EXPECT_NE(package_messages.find("package-visible function `leaks_private` exposes private type `root.PrivateOnly`"),
        std::string::npos);

    const std::optional<sema::SemanticAnalyzerCore::DeclarationAnalyzer::ExportSurfaceRestrictedType> package_leak =
        sema::SemanticAnalyzerCore::DeclarationAnalyzer(package_analyzer)
            .restricted_type_exposed_by_surface_type(private_type, syntax::Visibility::package_);
    ASSERT_TRUE(package_leak.has_value());
    EXPECT_EQ(package_leak->name, "root.PrivateOnly");
    EXPECT_EQ(package_leak->visibility, syntax::Visibility::private_);
}
TEST(CoreUnit, SemanticWhiteBoxModulePartContextsTrackCurrentItem)
{
    constexpr base::u32 SEMA_TEST_PRIMARY_PART_INDEX = 0;
    constexpr base::u32 SEMA_TEST_FRAGMENT_PART_INDEX = 2;
    constexpr base::usize SEMA_TEST_PART_KEY_TABLE_SIZE = SEMA_TEST_FRAGMENT_PART_INDEX + 1;
    const std::array<std::string_view, 1> package_parts{"part-package"};
    const std::array<std::string_view, 1> module_parts{"root"};
    const query::PackageKey package = query::package_key(package_parts);
    const query::ModuleKey module_key = query::module_key_from_stable_id(
        package, sema::stable_module_id(std::span<const std::string_view>{module_parts.data(), module_parts.size()}));
    const query::FileKey primary_file = query::file_key(package, "/workspace/root.ax");
    const query::FileKey part_file = query::file_key(package, "/workspace/root.parts/types.ax");
    const query::ModulePartKey primary_key = query::module_part_key(
        module_key, primary_file, query::ModulePartKind::primary, "<primary>", SEMA_TEST_PRIMARY_PART_INDEX);
    const query::ModulePartKey part_key = query::module_part_key(
        module_key, part_file, query::ModulePartKind::fragment, "types", SEMA_TEST_FRAGMENT_PART_INDEX);

    syntax::AstModule module;
    module.modules = {module_info({"root"})};
    syntax::ItemNode primary_item;
    primary_item.kind = syntax::ItemKind::fn_decl;
    primary_item.name = "primary";
    const syntax::ItemId primary_id =
        module.push_item_for_module(primary_item, module_id(0), SEMA_TEST_PRIMARY_PART_INDEX);
    syntax::ItemNode part_item;
    part_item.kind = syntax::ItemKind::fn_decl;
    part_item.name = "from_part";
    const syntax::ItemId part_id = module.push_item_for_module(part_item, module_id(0), SEMA_TEST_FRAGMENT_PART_INDEX);
    syntax::ItemImportScope part_import_scope;
    part_import_scope.item_begin = part_id.value;
    part_import_scope.item_count = 1;
    part_import_scope.part_index = SEMA_TEST_FRAGMENT_PART_INDEX;
    module.item_import_scopes.push_back(std::move(part_import_scope));

    sema::SemanticOptions options;
    options.module_packages.push_back(package);
    options.module_part_keys.resize(module.modules.size());
    options.module_part_keys[0].resize(SEMA_TEST_PART_KEY_TABLE_SIZE);
    options.module_part_keys[0][SEMA_TEST_PRIMARY_PART_INDEX] = primary_key;
    options.module_part_keys[0][SEMA_TEST_FRAGMENT_PART_INDEX] = part_key;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics, options);
    EXPECT_EQ(analyzer.item_part_index(primary_id), SEMA_TEST_PRIMARY_PART_INDEX);
    EXPECT_EQ(analyzer.item_part_index(part_id), SEMA_TEST_FRAGMENT_PART_INDEX);
    EXPECT_EQ(analyzer.query_module_part_key(primary_id), primary_key);
    EXPECT_EQ(analyzer.query_module_part_key(part_id), part_key);
    const syntax::ItemImportScope* const found_part_scope = analyzer.item_import_scope(part_id);
    ASSERT_NE(found_part_scope, nullptr);
    EXPECT_EQ(found_part_scope->part_index, SEMA_TEST_FRAGMENT_PART_INDEX);

    const sema::DeclContext part_declaration = analyzer.declaration_context(part_id);
    EXPECT_EQ(part_declaration.module, module_key);
    EXPECT_EQ(part_declaration.part, part_key);
    EXPECT_FALSE(query::is_valid(analyzer.declaration_context(module_id(0)).part));

    analyzer.state_.flow.current_module = module_id(0);
    analyzer.state_.flow.current_item = part_id;
    const sema::AccessContext part_access = analyzer.current_access_context();
    EXPECT_EQ(part_access.module, module_key);
    EXPECT_EQ(part_access.part, part_key);

    analyzer.state_.flow.current_item = syntax::INVALID_ITEM_ID;
    const sema::AccessContext module_only_access = analyzer.current_access_context();
    EXPECT_EQ(module_only_access.module, module_key);
    EXPECT_FALSE(query::is_valid(module_only_access.part));
    EXPECT_FALSE(query::is_valid(analyzer.query_module_part_key(syntax::INVALID_ITEM_ID)));
}
TEST(CoreUnit, SemanticWhiteBoxCheckedDumpSurfacesModulePartOrigins)
{
    constexpr base::u32 SEMA_TEST_PRIMARY_PART_INDEX = 0;
    constexpr base::u32 SEMA_TEST_FRAGMENT_PART_INDEX = 2;
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId generic_type = module.push_type(named_node(SEMA_TEST_GENERIC_PARAM_NAME));
    const ExprId zero = push_integer_text(module, "0");
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = zero;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode primary_function;
    primary_function.kind = syntax::ItemKind::fn_decl;
    primary_function.name = "primary";
    primary_function.return_type = i32_type;
    primary_function.body = body;
    static_cast<void>(module.push_item_for_module(
        primary_function, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_PRIMARY_PART_INDEX));

    syntax::ItemNode part_function;
    part_function.kind = syntax::ItemKind::fn_decl;
    part_function.name = "from_part";
    part_function.return_type = i32_type;
    part_function.body = body;
    const syntax::ItemId first_part_item = module.push_item_for_module(
        part_function, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_FRAGMENT_PART_INDEX);

    syntax::ItemNode part_struct;
    part_struct.kind = syntax::ItemKind::struct_decl;
    part_struct.name = "PartRecord";
    static_cast<void>(module.push_item_for_module(
        part_struct, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_FRAGMENT_PART_INDEX));

    syntax::ItemNode part_alias;
    part_alias.kind = syntax::ItemKind::type_alias;
    part_alias.name = "PartAlias";
    part_alias.alias_type = i32_type;
    static_cast<void>(
        module.push_item_for_module(part_alias, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_FRAGMENT_PART_INDEX));

    syntax::EnumCaseDecl enum_case;
    enum_case.name = "some";
    syntax::ItemNode part_enum;
    part_enum.kind = syntax::ItemKind::enum_decl;
    part_enum.name = "PartChoice";
    part_enum.enum_cases = {enum_case};
    static_cast<void>(
        module.push_item_for_module(part_enum, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_FRAGMENT_PART_INDEX));

    syntax::ItemNode generic_struct;
    generic_struct.kind = syntax::ItemKind::struct_decl;
    generic_struct.name = "Box";
    generic_struct.generic_params = {syntax::GenericParamDecl{std::string(SEMA_TEST_GENERIC_PARAM_NAME), {}}};
    generic_struct.fields = {syntax::FieldDecl{"value", generic_type, {}}};
    static_cast<void>(module.push_item_for_module(
        generic_struct, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_FRAGMENT_PART_INDEX));

    syntax::ItemImportScope part_scope;
    part_scope.item_begin = first_part_item.value;
    part_scope.item_count = static_cast<base::u32>(module.items.size() - first_part_item.value);
    part_scope.part_index = SEMA_TEST_FRAGMENT_PART_INDEX;
    module.item_import_scopes.push_back(std::move(part_scope));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    const sema::CheckedModule& checked = checked_result.value();

    const auto function_part_index = [&](const std::string_view name) -> std::optional<base::u32> {
        for (const auto& entry : checked.functions) {
            const FunctionSignature& signature = entry.second;
            if (signature.name == name) {
                return signature.part_index;
            }
        }
        return std::nullopt;
    };
    ASSERT_TRUE(function_part_index("primary").has_value());
    ASSERT_TRUE(function_part_index("from_part").has_value());
    EXPECT_EQ(function_part_index("primary").value(), SEMA_TEST_PRIMARY_PART_INDEX);
    EXPECT_EQ(function_part_index("from_part").value(), SEMA_TEST_FRAGMENT_PART_INDEX);

    bool struct_part_found = false;
    for (const auto& entry : checked.structs) {
        const StructInfo& info = entry.second;
        if (info.name == "PartRecord") {
            struct_part_found = true;
            EXPECT_EQ(info.part_index, SEMA_TEST_FRAGMENT_PART_INDEX);
        }
    }
    EXPECT_TRUE(struct_part_found);

    bool alias_part_found = false;
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& alias = entry.second;
        if (alias.name == "PartAlias") {
            alias_part_found = true;
            EXPECT_EQ(alias.part_index, SEMA_TEST_FRAGMENT_PART_INDEX);
        }
    }
    EXPECT_TRUE(alias_part_found);

    bool enum_part_found = false;
    for (const auto& entry : checked.enum_cases) {
        const EnumCaseInfo& info = entry.second;
        if (info.enum_name == "PartChoice" && info.case_name == "some") {
            enum_part_found = true;
            EXPECT_EQ(info.part_index, SEMA_TEST_FRAGMENT_PART_INDEX);
        }
    }
    EXPECT_TRUE(enum_part_found);

    bool template_part_found = false;
    for (const sema::GenericTemplateSignatureInfo& info : checked.generic_template_signatures) {
        if (info.name == "Box") {
            template_part_found = true;
            EXPECT_EQ(info.part_index, SEMA_TEST_FRAGMENT_PART_INDEX);
        }
    }
    EXPECT_TRUE(template_part_found);

    const std::string checked_dump = sema::dump_checked_module(checked);
    EXPECT_NE(checked_dump.find("template priv type Box params=1 @part=2"), std::string::npos);
    EXPECT_NE(checked_dump.find("fn priv primary -> i32"), std::string::npos);
    EXPECT_NE(checked_dump.find("fn priv from_part -> i32"), std::string::npos);
    EXPECT_NE(checked_dump.find("struct priv PartRecord @part=2 fields=0"), std::string::npos);
    EXPECT_NE(checked_dump.find("type priv PartAlias = i32 @part=2"), std::string::npos);
    EXPECT_NE(checked_dump.find("case PartChoice_some"), std::string::npos);
    EXPECT_NE(checked_dump.find("@part=0"), std::string::npos);
    EXPECT_NE(checked_dump.find("@part=2"), std::string::npos);
}
TEST(CoreUnit, SemanticWhiteBoxCheckedDumpCoversPrimaryOnlyAndTemplateNamespaces)
{
    sema::CheckedModule primary_checked;
    const sema::InternedText primary_name = primary_checked.intern_text("primary_only");
    FunctionSignature primary_signature = primary_checked.make_function_signature();
    primary_signature.name = primary_name;
    primary_signature.name_id = primary_name.id;
    primary_signature.c_name = primary_name;
    primary_signature.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    primary_signature.return_type = primary_checked.types.builtin(BuiltinType::i32);
    primary_signature.semantic_key = sema::FunctionLookupKey{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        primary_name.id,
    };
    primary_checked.functions.emplace(primary_signature.semantic_key, std::move(primary_signature));

    const std::string primary_dump = sema::dump_checked_module(primary_checked);
    EXPECT_NE(primary_dump.find("fn primary_only -> i32"), std::string::npos);
    EXPECT_EQ(primary_dump.find("@part="), std::string::npos);

    sema::CheckedModule template_checked;
    constexpr base::u32 SEMA_TEST_TEMPLATE_PART_INDEX = 1;
    const std::array<query::DefNamespace, 5> namespaces{
        query::DefNamespace::value,
        query::DefNamespace::member,
        query::DefNamespace::trait_,
        query::DefNamespace::impl_,
        query::DefNamespace::synthetic,
    };
    for (base::usize index = 0; index < namespaces.size(); ++index) {
        sema::GenericTemplateSignatureInfo info;
        info.name = template_checked.intern_text("Template" + std::to_string(index));
        info.name_id = info.name.id;
        info.name_space = namespaces[index];
        info.param_count = static_cast<base::u32>(index);
        info.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
        template_checked.generic_template_signatures.push_back(info);
    }
    sema::GenericTemplateSignatureInfo unknown_template;
    unknown_template.name = template_checked.intern_text("TemplateUnknown");
    unknown_template.name_id = unknown_template.name.id;
    unknown_template.name_space = static_cast<query::DefNamespace>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    unknown_template.param_count = namespaces.size();
    unknown_template.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    template_checked.generic_template_signatures.push_back(unknown_template);

    const std::string template_dump = sema::dump_checked_module(template_checked);
    EXPECT_NE(template_dump.find("template value Template0 params=0 @part=1"), std::string::npos);
    EXPECT_NE(template_dump.find("template member Template1 params=1 @part=1"), std::string::npos);
    EXPECT_NE(template_dump.find("template trait Template2 params=2 @part=1"), std::string::npos);
    EXPECT_NE(template_dump.find("template impl Template3 params=3 @part=1"), std::string::npos);
    EXPECT_NE(template_dump.find("template synthetic Template4 params=4 @part=1"), std::string::npos);
    EXPECT_NE(template_dump.find("template unknown TemplateUnknown params=5 @part=1"), std::string::npos);

    sema::CheckedModule trait_checked;
    const TypeHandle trait_i32 = trait_checked.types.builtin(BuiltinType::i32);
    const sema::InternedText reader_name = trait_checked.intern_text("Reader");
    const sema::InternedText item_name = trait_checked.intern_text("Item");
    const sema::InternedText read_name = trait_checked.intern_text("read");
    const sema::InternedText fallback_name = trait_checked.intern_text("fallback");

    sema::TraitSignature trait = trait_checked.make_trait_signature();
    trait.name = reader_name;
    trait.name_id = reader_name.id;
    trait.visibility = syntax::Visibility::private_;
    trait.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    trait.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    trait.generic_params.push_back(reader_name.id);
    trait.generic_params.push_back(item_name.id);

    sema::TraitAssociatedTypeRequirement associated_type = trait_checked.make_trait_associated_type_requirement();
    associated_type.name = item_name;
    associated_type.name_id = item_name.id;
    trait.associated_types.push_back(associated_type);

    sema::TraitMethodRequirement requirement = trait_checked.make_trait_method_requirement();
    requirement.name = read_name;
    requirement.name_id = read_name.id;
    requirement.return_type = trait_i32;
    requirement.param_types.push_back(trait_i32);
    requirement.is_unsafe = true;
    requirement.is_variadic = true;
    requirement.has_default_body = true;
    requirement.has_borrow_contract = true;
    requirement.borrow_contract.source = sema::FunctionBorrowContractSource::declared;
    requirement.borrow_contract.unknown_return_allowed = true;
    requirement.borrow_contract.return_selectors.push_back(sema::BorrowContractSelector{
        .kind = sema::BorrowContractSelectorKind::parameter,
        .param_index = 0U,
        .name_id = read_name.id,
        .range = {},
    });
    trait.requirements.push_back(requirement);

    const sema::ModuleLookupKey reader_key{SEMA_TEST_ROOT_MODULE_INDEX, reader_name.id};
    trait_checked.traits.emplace(reader_key, std::move(trait));

    sema::TraitImplInfo trait_impl = trait_checked.make_trait_impl_info();
    trait_impl.trait_name = reader_name;
    trait_impl.trait_name_id = reader_name.id;
    trait_impl.trait_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    trait_impl.self_type = trait_i32;
    trait_impl.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    trait_impl.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    trait_impl.key = sema::TraitImplLookupKey{SEMA_TEST_ROOT_MODULE_INDEX, reader_name.id, trait_i32.value, {}};

    sema::TraitImplAssociatedTypeInfo associated_impl = trait_checked.make_trait_impl_associated_type_info();
    associated_impl.name = item_name;
    associated_impl.name_id = item_name.id;
    associated_impl.value_type = trait_i32;
    associated_impl.requirement_ordinal = 0U;
    trait_impl.associated_types.push_back(associated_impl);

    sema::TraitImplMethodInfo override_method = trait_checked.make_trait_impl_method_info();
    override_method.name = read_name;
    override_method.name_id = read_name.id;
    override_method.requirement_ordinal = 0U;
    override_method.origin = sema::TraitImplMethodOrigin::impl_override;
    trait_impl.methods.push_back(override_method);

    sema::TraitImplMethodInfo default_method = trait_checked.make_trait_impl_method_info();
    default_method.name = fallback_name;
    default_method.name_id = fallback_name.id;
    default_method.requirement_ordinal = 1U;
    default_method.origin = sema::TraitImplMethodOrigin::trait_default;
    trait_impl.methods.push_back(default_method);

    sema::TraitImplMethodInfo invalid_origin_method = trait_checked.make_trait_impl_method_info();
    invalid_origin_method.name = trait_checked.intern_text("invalid_origin");
    invalid_origin_method.name_id = invalid_origin_method.name.id;
    invalid_origin_method.requirement_ordinal = 2U;
    invalid_origin_method.origin = static_cast<sema::TraitImplMethodOrigin>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    trait_impl.methods.push_back(invalid_origin_method);

    trait_checked.trait_impls.emplace(trait_impl.key, std::move(trait_impl));

    const std::string trait_dump = sema::dump_checked_module(trait_checked);
    EXPECT_NE(trait_dump.find("trait priv Reader[T0, T1] params=2 associated_types=1 requirements=1 @part=1"),
        std::string::npos);
    EXPECT_NE(trait_dump.find("assoc_type Item"), std::string::npos);
    EXPECT_NE(
        trait_dump.find(
            "requirement unsafe read(i32) -> i32 variadic default borrow_contract=declared/selectors=1/unknown=true"),
        std::string::npos);
    EXPECT_NE(trait_dump.find("impl Reader for i32 associated_types=1 methods=3 @part=1"), std::string::npos);
    EXPECT_NE(trait_dump.find("assoc_type Item = i32 requirement=0"), std::string::npos);
    EXPECT_NE(trait_dump.find("method read requirement=0 origin=impl_override"), std::string::npos);
    EXPECT_NE(trait_dump.find("method fallback requirement=1 origin=trait_default"), std::string::npos);
    EXPECT_NE(trait_dump.find("method invalid_origin requirement=2 origin=impl_override"), std::string::npos);

    sema::CheckedModule fallback_checked;
    const TypeHandle fallback_i32 = fallback_checked.types.builtin(BuiltinType::i32);
    const TypeHandle fallback_struct_type = fallback_checked.types.named_struct("DumpStruct", "DumpStruct", false);
    const TypeHandle fallback_enum_type = fallback_checked.types.named_enum("DumpEnum", "DumpEnum");
    const sema::InternedText predicate_trait = fallback_checked.intern_text("DumpTrait");
    const sema::InternedText function_name = fallback_checked.intern_text("dump_function");
    const std::string invalid_lookup_key =
        std::to_string(SEMA_TEST_ROOT_MODULE_INDEX) + ":" + std::to_string(sema::SEMA_LOOKUP_INVALID_KEY_PART) + ":-";
    const sema::FunctionLookupKey invalid_function_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        sema::INVALID_IDENT_ID,
    };

    sema::TraitPredicate predicate = fallback_checked.make_trait_predicate();
    predicate.index = 0U;
    predicate.kind = static_cast<sema::TraitPredicateKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    predicate.origin = static_cast<sema::TraitPredicateOrigin>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    predicate.subject_type = fallback_i32;
    predicate.trait_name = predicate_trait;
    predicate.trait_name_id = predicate_trait.id;
    predicate.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.trait_predicates.push_back(predicate);

    sema::TraitEvidence evidence = fallback_checked.make_trait_evidence();
    evidence.kind = static_cast<sema::TraitEvidenceKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    evidence.predicate_index = 0U;
    fallback_checked.trait_evidence.push_back(evidence);

    sema::TraitMethodCallBinding trait_call = fallback_checked.make_trait_method_call_binding();
    trait_call.dispatch = static_cast<sema::TraitMethodDispatchKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    trait_call.self_type = fallback_i32;
    trait_call.return_type = fallback_i32;
    trait_call.receiver_access = static_cast<sema::ReceiverAccessKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    trait_call.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.append_trait_method_call_binding(trait_call);

    sema::FunctionCallBinding function_call = fallback_checked.make_function_call_binding();
    function_call.call_expr = ExprId{SEMA_TEST_PATTERN_FIRST_INDEX};
    function_call.function_key = invalid_function_key;
    function_call.return_type = fallback_i32;
    function_call.receiver_access = sema::ReceiverAccessKind::shared;
    function_call.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.append_function_call_binding(function_call);

    sema::FunctionBorrowSummary borrow_summary;
    borrow_summary.function = invalid_function_key;
    borrow_summary.return_type = fallback_i32;
    borrow_summary.has_unknown_return_origin = true;
    borrow_summary.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.borrow_summaries.emplace(invalid_function_key, borrow_summary);

    sema::FunctionBorrowContract borrow_contract;
    borrow_contract.function = invalid_function_key;
    borrow_contract.return_type = fallback_i32;
    borrow_contract.source = static_cast<sema::FunctionBorrowContractSource>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    borrow_contract.unknown_return_allowed = true;
    borrow_contract.has_contract_mismatch = true;
    borrow_contract.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    borrow_contract.return_selectors.push_back(sema::BorrowContractSelector{
        .kind = static_cast<sema::BorrowContractSelectorKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE),
        .param_index = SEMA_TEST_PATTERN_FIRST_INDEX,
        .name_id = sema::INVALID_IDENT_ID,
        .range = {},
    });
    fallback_checked.borrow_contracts.emplace(invalid_function_key, borrow_contract);

    sema::ReferenceOriginFact reference_origin;
    reference_origin.syntax_type = TypeId{SEMA_TEST_PATTERN_FIRST_INDEX};
    reference_origin.semantic_type = fallback_i32;
    reference_origin.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.reference_origin_facts.push_back(reference_origin);

    sema::TypeLifetimeInfo type_lifetime;
    type_lifetime.type = fallback_i32;
    type_lifetime.can_contain_borrow = true;
    type_lifetime.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.type_lifetime_infos.push_back(type_lifetime);

    sema::FunctionLifetimeFacts lifetime_facts;
    lifetime_facts.function = invalid_function_key;
    lifetime_facts.return_type = fallback_i32;
    lifetime_facts.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    lifetime_facts.regions.push_back(sema::LifetimeRegion{
        .kind = sema::LifetimeRegionKind::inferred,
        .name_id = function_name.id,
        .name = {},
        .param_index = SEMA_TEST_PATTERN_FIRST_INDEX,
        .range = {},
    });
    lifetime_facts.regions.push_back(sema::LifetimeRegion{
        .kind = sema::LifetimeRegionKind::unknown,
        .name_id = sema::INVALID_IDENT_ID,
        .name = {},
        .param_index = SEMA_TEST_PATTERN_SECOND_INDEX,
        .range = {},
    });
    fallback_checked.lifetime_facts.emplace(invalid_function_key, lifetime_facts);

    sema::FunctionDropCheckFacts dropck_facts;
    dropck_facts.function = invalid_function_key;
    dropck_facts.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    dropck_facts.graph_missing = true;
    fallback_checked.dropck_facts.emplace(invalid_function_key, dropck_facts);

    sema::BodyLoanCheckResult loan_result;
    loan_result.function = invalid_function_key;
    loan_result.diagnostic_mode = sema::BodyLoanDiagnosticMode::enforced;
    loan_result.graph_missing = true;
    fallback_checked.body_loan_checks.emplace(invalid_function_key, loan_result);

    sema::ParamEnvInfo param_env = fallback_checked.make_param_env_info();
    param_env.owner_name = fallback_checked.intern_text("DumpParamEnv");
    param_env.owner_name_id = param_env.owner_name.id;
    param_env.predicate_indices.push_back(SEMA_TEST_PATTERN_FIRST_INDEX);
    param_env.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.param_envs.push_back(param_env);

    FunctionSignature export_signature = fallback_checked.make_function_signature();
    export_signature.name = function_name;
    export_signature.name_id = function_name.id;
    export_signature.c_name = function_name;
    export_signature.return_type = fallback_i32;
    export_signature.is_export_c = true;
    export_signature.semantic_key = sema::FunctionLookupKey{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_name.id,
    };
    fallback_checked.functions.emplace(export_signature.semantic_key, std::move(export_signature));

    StructInfo opaque_struct = fallback_checked.make_struct_info();
    opaque_struct.name = fallback_checked.intern_text("DumpStruct");
    opaque_struct.name_id = opaque_struct.name.id;
    opaque_struct.c_name = opaque_struct.name;
    opaque_struct.type = fallback_struct_type;
    opaque_struct.is_opaque = true;
    fallback_checked.structs.emplace(
        sema::ModuleLookupKey{SEMA_TEST_ROOT_MODULE_INDEX, opaque_struct.name_id}, std::move(opaque_struct));

    EnumCaseInfo payload_case = fallback_checked.make_enum_case_info();
    payload_case.enum_name = fallback_checked.intern_text("DumpEnum");
    payload_case.case_name = fallback_checked.intern_text("single");
    payload_case.name = payload_case.case_name;
    payload_case.name_id = payload_case.case_name.id;
    payload_case.c_name = fallback_checked.intern_text("DumpEnum_single");
    payload_case.type = fallback_enum_type;
    payload_case.payload_type = fallback_i32;
    fallback_checked.enum_cases.emplace(
        sema::ModuleLookupKey{SEMA_TEST_ROOT_MODULE_INDEX, payload_case.name_id}, std::move(payload_case));

    const std::string fallback_dump = sema::dump_checked_module(fallback_checked);
    EXPECT_NE(fallback_dump.find("predicate #0 trait i32: DumpTrait origin=where @part=1"), std::string::npos);
    EXPECT_NE(fallback_dump.find("evidence #0 param_env predicate=0"), std::string::npos);
    EXPECT_NE(fallback_dump.find("trait_call #0 param_env i32.<invalid> -> i32"), std::string::npos);
    EXPECT_NE(fallback_dump.find("receiver_access=<invalid>"), std::string::npos);
    EXPECT_NE(fallback_dump.find("function_call #0 expr=e0 -> " + invalid_lookup_key), std::string::npos);
    EXPECT_NE(fallback_dump.find("borrow_summary " + invalid_lookup_key), std::string::npos);
    EXPECT_NE(fallback_dump.find("borrow_contract " + invalid_lookup_key + " source=<invalid>"), std::string::npos);
    EXPECT_NE(fallback_dump.find("selector #0 <invalid> param=0 name=-"), std::string::npos);
    EXPECT_NE(fallback_dump.find("reference_origin #0 t0 i32 origins=- @part=1"), std::string::npos);
    EXPECT_NE(fallback_dump.find("type_lifetime #0 i32 can_borrow=true concrete=false origins=-"), std::string::npos);
    EXPECT_NE(fallback_dump.find("lifetime_fact " + invalid_lookup_key), std::string::npos);
    EXPECT_NE(fallback_dump.find("region #0 inferred param=0 name=#"), std::string::npos);
    EXPECT_NE(fallback_dump.find("region #1 unknown param=1 name=-"), std::string::npos);
    EXPECT_NE(fallback_dump.find("dropck_fact " + invalid_lookup_key), std::string::npos);
    EXPECT_NE(fallback_dump.find("body_loan_check " + invalid_lookup_key + " mode=enforced"), std::string::npos);
    EXPECT_NE(fallback_dump.find("param_env DumpParamEnv predicates=1"), std::string::npos);
    EXPECT_NE(fallback_dump.find("fn dump_function -> i32 export_c"), std::string::npos);
    EXPECT_NE(fallback_dump.find("struct DumpStruct opaque @part=0 fields=0"), std::string::npos);
    EXPECT_NE(fallback_dump.find("case DumpEnum_single : DumpEnum(i32)"), std::string::npos);
}
TEST(CoreUnit, SemanticWhiteBoxRecordsOriginParamsAndReferenceFacts)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_ORIGIN_FACT_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    syntax::TypeNode reference_type = reference_node(i32_type);
    reference_type.reference_origin.explicit_ = true;
    reference_type.reference_origin.names = {SEMA_TEST_ORIGIN_FACT_PARAM_NAME};
    const TypeId explicit_origin_ref = module.push_type(std::move(reference_type));

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = SEMA_TEST_ORIGIN_FACT_FUNCTION_NAME;
    function.generic_params = {syntax::GenericParamDecl{
        SEMA_TEST_ORIGIN_FACT_PARAM_NAME,
        {},
        syntax::INVALID_IDENT_ID,
        syntax::GenericParamKind::origin,
    }};
    function.return_type = explicit_origin_ref;
    function.is_prototype = true;
    function.is_extern_c = true;
    const syntax::ItemId function_id =
        module.push_item_for_module(function, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_ORIGIN_FACT_PART_INDEX);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    const sema::CheckedModule& checked = checked_result.value();

    ASSERT_EQ(checked.lifetime_origin_params.size(), 1U);
    const sema::LifetimeOriginParamInfo& origin_param = checked.lifetime_origin_params.front();
    EXPECT_EQ(origin_param.module.value, SEMA_TEST_ROOT_MODULE_INDEX);
    EXPECT_EQ(origin_param.item.value, function_id.value);
    EXPECT_EQ(origin_param.name, SEMA_TEST_ORIGIN_FACT_PARAM_NAME);
    EXPECT_TRUE(sema::is_valid(origin_param.name_id));
    EXPECT_EQ(origin_param.ordinal, 0U);
    EXPECT_EQ(origin_param.part_index, SEMA_TEST_ORIGIN_FACT_PART_INDEX);

    ASSERT_EQ(checked.reference_origin_facts.size(), 1U);
    const sema::ReferenceOriginFact& origin_fact = checked.reference_origin_facts.front();
    EXPECT_EQ(origin_fact.module.value, SEMA_TEST_ROOT_MODULE_INDEX);
    EXPECT_EQ(origin_fact.item.value, function_id.value);
    EXPECT_EQ(origin_fact.syntax_type.value, explicit_origin_ref.value);
    EXPECT_TRUE(sema::is_valid(origin_fact.semantic_type));
    EXPECT_EQ(checked.types.display_name(origin_fact.semantic_type), SEMA_TEST_ORIGIN_FACT_TYPE_DISPLAY);
    ASSERT_EQ(origin_fact.origin_names.size(), 1U);
    EXPECT_EQ(origin_fact.origin_names.front(), SEMA_TEST_ORIGIN_FACT_PARAM_NAME);
    ASSERT_EQ(origin_fact.origin_name_ids.size(), 1U);
    EXPECT_EQ(origin_fact.origin_name_ids.front(), origin_param.name_id);
    EXPECT_EQ(origin_fact.part_index, SEMA_TEST_ORIGIN_FACT_PART_INDEX);

    EXPECT_GT(checked.type_lifetime_infos.size(), 0U);
    EXPECT_GT(checked.generic_lifetime_predicates.size(), 0U);
    const bool has_explicit_predicate =
        std::ranges::any_of(checked.generic_lifetime_predicates, [](const sema::GenericLifetimePredicate& predicate) {
            return predicate.source == sema::GenericLifetimePredicateSource::explicit_origin
                && predicate.origin_name.view() == SEMA_TEST_ORIGIN_FACT_PARAM_NAME;
        });
    EXPECT_TRUE(has_explicit_predicate);

    const std::string checked_dump = sema::dump_checked_module(checked);
    EXPECT_NE(checked_dump.find("lifetime_origin_params 1"), std::string::npos);
    EXPECT_NE(checked_dump.find("origin_param #0 data ordinal=0"), std::string::npos);
    EXPECT_NE(checked_dump.find("reference_origin_facts 1"), std::string::npos);
    EXPECT_NE(checked_dump.find("reference_origin #0"), std::string::npos);
    EXPECT_NE(checked_dump.find("origins=data"), std::string::npos);
    EXPECT_NE(checked_dump.find("type_lifetime_infos"), std::string::npos);
    EXPECT_NE(checked_dump.find("generic_lifetime_predicates"), std::string::npos);
    EXPECT_NE(checked_dump.find("source=explicit_origin"), std::string::npos);
}
TEST(CoreUnit, SemanticWhiteBoxLookupsAndMethodReceivers)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
        module_info({"lib", "two"}),
    };
    module.modules[0].imports = {
        resolved_import(module_id(1), "one"),
        resolved_import(module_id(2), "two"),
    };
    const ExprId value_expr = push_name(module, "value");
    const ExprId literal_expr = push_integer(module);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(0);
    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    const TypeHandle array_record = types.named_struct("ArrayRecord", "ArrayRecord", true);
    types.set_record_contains_array(array_record, true);
    const TypeHandle ptr_record = types.pointer(PointerMutability::mut, record_type);
    const TypeHandle const_ptr_record = types.pointer(PointerMutability::const_, record_type);
    const TypeHandle enum_type = types.named_enum("Choice", "Choice");

    const IdentId shared_id = intern_identifier(analyzer, "Shared");
    const IdentId private_id = intern_identifier(analyzer, "Private");
    const IdentId ambiguous_id = intern_identifier(analyzer, "ambiguous");
    const IdentId hidden_id = intern_identifier(analyzer, "hidden");
    const IdentId missing_id = intern_identifier(analyzer, "missing");
    const IdentId same_id = intern_identifier(analyzer, "same");
    const IdentId private_value_id = intern_identifier(analyzer, "private_value");
    const IdentId run_id = intern_identifier(analyzer, "run");
    const IdentId static_only_id = intern_identifier(analyzer, "static_only");
    const IdentId free_id = intern_identifier(analyzer, "free");

    static_cast<void>(add_global_value(analyzer, module_id(0), "value", record_type, SymbolKind::local, true));

    static_cast<void>(add_named_type(analyzer, module_id(1), "Shared", i32));
    static_cast<void>(add_named_type(analyzer, module_id(2), "Shared", bool_type));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_visible_modules(shared_id, "Shared", {}, false)));

    static_cast<void>(add_named_type(analyzer, module_id(1), "Private", record_type, syntax::Visibility::private_));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(module_id(1), private_id, "Private", {}, false)));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(syntax::INVALID_MODULE_ID, missing_id, "Missing", {}, false)));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(module_id(1), missing_id, "Missing", {}, false)));

    FunctionSignature one = indexed_function_signature(analyzer, "ambiguous", module_id(1), i32);
    FunctionSignature two = indexed_function_signature(analyzer, "ambiguous", module_id(2), i32);
    static_cast<void>(add_function(analyzer, one));
    static_cast<void>(add_function(analyzer, two));
    EXPECT_EQ(analyzer.find_function_in_visible_modules(ambiguous_id, "ambiguous", {}), nullptr);
    FunctionSignature private_function = indexed_function_signature(analyzer, "hidden", module_id(1), i32);
    private_function.visibility = syntax::Visibility::private_;
    static_cast<void>(add_function(analyzer, private_function));
    EXPECT_EQ(analyzer.find_function_in_module(module_id(1), hidden_id, "hidden", {}), nullptr);
    EXPECT_EQ(analyzer.find_function_in_module(module_id(1), missing_id, "missing", {}), nullptr);
    EXPECT_EQ(analyzer.find_function_in_module(syntax::INVALID_MODULE_ID, missing_id, "missing", {}), nullptr);

    EnumCaseInfo one_case;
    one_case.name = checked_text(analyzer.state_.checked, "same");
    one_case.name_id = same_id;
    one_case.case_name = checked_text(analyzer.state_.checked, "same");
    one_case.case_name_id = same_id;
    one_case.module = module_id(1);
    one_case.type = enum_type;
    EnumCaseInfo two_case = one_case;
    two_case.module = module_id(2);
    auto one_case_inserted =
        analyzer.state_.checked.enum_cases.emplace(semantic_module_key(analyzer, module_id(1), "same"), one_case);
    auto two_case_inserted =
        analyzer.state_.checked.enum_cases.emplace(semantic_module_key(analyzer, module_id(2), "same"), two_case);
    analyzer.index_enum_case(one_case_inserted.first->second);
    analyzer.index_enum_case(two_case_inserted.first->second);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules(same_id, "same", {}), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules(missing_id, "missing", {}), nullptr);

    static_cast<void>(add_named_type(analyzer, module_id(0), "Record", record_type));
    static_cast<void>(add_named_type(analyzer, module_id(0), "Choice", enum_type));
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(
                  intern_identifier(analyzer, "Record"), "Record", missing_id, "missing", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(
                  intern_identifier(analyzer, "Choice"), "Choice", missing_id, "missing", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_enum_constructor(syntax::INVALID_EXPR_ID, true), nullptr);

    static_cast<void>(add_global_value(analyzer, module_id(1), "ambiguous", i32));
    static_cast<void>(add_global_value(analyzer, module_id(2), "ambiguous", i32));
    EXPECT_EQ(analyzer.find_symbol(ambiguous_id, "ambiguous", {}), nullptr);
    EXPECT_EQ(analyzer.find_symbol(missing_id, "missing", {}), nullptr);
    static_cast<void>(add_global_value(
        analyzer, module_id(1), "private_value", i32, SymbolKind::const_, false, syntax::Visibility::private_));
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(1), missing_id, "missing", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(1), private_value_id, "private_value", {}, true), nullptr);

    FunctionSignature no_self;
    EXPECT_FALSE(analyzer.method_receiver_matches(no_self, record_type, value_expr));
    FunctionSignature by_value;
    by_value.has_self_param = true;
    by_value.param_types = {array_record};
    EXPECT_FALSE(analyzer.method_receiver_matches(by_value, array_record, value_expr));
    FunctionSignature plain_self;
    plain_self.has_self_param = true;
    plain_self.param_types = {record_type};
    EXPECT_TRUE(analyzer.method_receiver_matches(plain_self, record_type, value_expr));
    FunctionSignature pointer_self;
    pointer_self.has_self_param = true;
    pointer_self.param_types = {ptr_record};
    EXPECT_TRUE(analyzer.method_receiver_matches(pointer_self, ptr_record, value_expr));
    EXPECT_FALSE(analyzer.method_receiver_matches(pointer_self, const_ptr_record, value_expr));
    EXPECT_FALSE(analyzer.method_receiver_matches(pointer_self, record_type, literal_expr));
    EXPECT_FALSE(analyzer.method_receiver_matches(plain_self, bool_type, value_expr));
    EXPECT_FALSE(analyzer.method_receiver_matches(pointer_self, bool_type, value_expr));

    FunctionSignature method = indexed_function_signature(analyzer, "run", module_id(1), i32);
    method.is_method = true;
    method.has_self_param = true;
    method.method_owner_type = record_type;
    FunctionSignature other_method = method;
    other_method.module = module_id(2);
    static_cast<void>(add_method(analyzer, method, record_type));
    static_cast<void>(add_method(analyzer, other_method, record_type));
    FunctionSignature static_method = indexed_function_signature(analyzer, "static_only", module_id(1), i32);
    static_method.is_method = true;
    static_method.method_owner_type = record_type;
    static_cast<void>(add_method(analyzer, static_method, record_type));
    FunctionSignature not_method = indexed_function_signature(analyzer, "free", module_id(1), i32);
    not_method.method_owner_type = record_type;
    analyzer.state_.checked.functions.emplace(
        semantic_method_key(analyzer, module_id(1), record_type, "free"), not_method);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, run_id, "run", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, static_only_id, "static_only", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, free_id, "free", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, missing_id, "missing", {}, true), nullptr);
}
TEST(CoreUnit, SemanticWhiteBoxModuleVisibilityResolverEdges)
{
    constexpr base::u32 SEMA_TEST_CORE_MODULE_INDEX = 3;
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
        module_info({"lib", "two"}),
        module_info({"core"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
        resolved_import(module_id(2), ""),
        resolved_import(module_id(SEMA_TEST_MISSING_MODULE_INDEX), "ghost", syntax::Visibility::public_),
    };
    module.modules[SEMA_TEST_LIB_ONE_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_CORE_MODULE_INDEX), "core", syntax::Visibility::package_),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    EXPECT_EQ(analyzer.resolve_import_alias("lib", {}).value, SEMA_TEST_LIB_ONE_MODULE_INDEX);
    EXPECT_EQ(analyzer.resolve_import_alias("ghost", {}).value, SEMA_TEST_MISSING_MODULE_INDEX);
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("libs", {})));
    EXPECT_FALSE(analyzer.module_alias_visible("missing"));
    EXPECT_FALSE(syntax::is_valid(analyzer.find_visible_module_path({})));

    analyzer.state_.modules.visible_modules_cache.clear();
    const auto& visible = analyzer.visible_modules(module_id(SEMA_TEST_ROOT_MODULE_INDEX));
    EXPECT_NE(std::find_if(visible.begin(), visible.end(),
                  [](const syntax::ModuleId candidate_module) {
                      return candidate_module.value == SEMA_TEST_MISSING_MODULE_INDEX;
                  }),
        visible.end());
    EXPECT_NE(std::find_if(visible.begin(), visible.end(),
                  [](const syntax::ModuleId candidate_module) {
                      return candidate_module.value == SEMA_TEST_CORE_MODULE_INDEX;
                  }),
        visible.end());
    const auto& public_exports = analyzer.module_export_modules(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX));
    EXPECT_EQ(std::find_if(public_exports.begin(), public_exports.end(),
                  [](const syntax::ModuleId candidate_module) {
                      return candidate_module.value == SEMA_TEST_CORE_MODULE_INDEX;
                  }),
        public_exports.end());
    const auto accessible_exports =
        analyzer.accessible_module_export_modules(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX));
    EXPECT_NE(std::find_if(accessible_exports.begin(), accessible_exports.end(),
                  [](const syntax::ModuleId candidate_module) {
                      return candidate_module.value == SEMA_TEST_CORE_MODULE_INDEX;
                  }),
        accessible_exports.end());
    EXPECT_FALSE(analyzer.visible_root_module_name_exists("ghost"));
    EXPECT_FALSE(syntax::is_valid(analyzer.find_visible_module_path({"ghost", "child"})));
    EXPECT_FALSE(analyzer.visible_module_path_prefix_exists({"ghost", "child", "leaf"}));

    analyzer.state_.flow.current_module = module_id(SEMA_TEST_MISSING_MODULE_INDEX);
    EXPECT_FALSE(analyzer.module_alias_visible("ghost"));
    EXPECT_FALSE(analyzer.visible_root_module_name_exists("root"));
    EXPECT_FALSE(syntax::is_valid(analyzer.find_visible_module_path({"root"})));
    EXPECT_FALSE(analyzer.visible_module_path_prefix_exists({"root", "child"}));
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("ghost", {}, false)));
}
TEST(CoreUnit, SemanticWhiteBoxDotOnlyModuleSelectorAndShadowingEdges)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"app"}),
        module_info({"lib"}),
        module_info({SEMA_TEST_ROOT_MODULE_NAME, SEMA_TEST_CHILD_MODULE_NAME}),
        module_info({SEMA_TEST_ROOT_MODULE_NAME, SEMA_TEST_CHILD_MODULE_NAME, SEMA_TEST_LEAF_MODULE_NAME}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
        resolved_import(module_id(2), "mem"),
        resolved_import(module_id(3), "io"),
    };

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::fn_decl;
    item.name = "owned";
    const syntax::ItemId item_id = module.push_item(item);
    module.item_modules[item_id.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle local_type = types.named_struct("LocalType", "LocalType", false);
    const IdentId local_type_id = intern_identifier(analyzer, "LocalType");
    const IdentId global_id = intern_identifier(analyzer, "global");
    const IdentId lib_id = intern_identifier(analyzer, "lib");
    const IdentId root_module_id = intern_identifier(analyzer, SEMA_TEST_ROOT_MODULE_NAME);
    const IdentId fresh_id = intern_identifier(analyzer, "fresh");
    const IdentId t_id = intern_identifier(analyzer, "T");
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "LocalType", local_type));
    static_cast<void>(add_global_value(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "global", i32));
    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "local", module_id(SEMA_TEST_ROOT_MODULE_INDEX), i32),
        diagnostics));

    const ExprId alias_expr = push_name(analyzer.ctx_.module, "lib");
    const ExprId root_expr = push_name(analyzer.ctx_.module, SEMA_TEST_ROOT_MODULE_NAME);
    const ExprId core_mem = push_field(analyzer.ctx_.module, root_expr, SEMA_TEST_CHILD_MODULE_NAME);
    const ExprId core_mem_io = push_field(analyzer.ctx_.module, core_mem, SEMA_TEST_LEAF_MODULE_NAME);
    const ExprId invalid_expr = push_name(analyzer.ctx_.module, "missing");
    const ExprId missing_root_child =
        push_field(analyzer.ctx_.module, push_name(analyzer.ctx_.module, SEMA_TEST_ROOT_MODULE_NAME), "missing");
    const ExprId core_mem_file = push_field(analyzer.ctx_.module, core_mem, "File");
    const ExprId local_member = push_field(analyzer.ctx_.module, push_name(analyzer.ctx_.module, "local"), "member");
    const ExprId alias_member = push_field(analyzer.ctx_.module, push_name(analyzer.ctx_.module, "mem"), "File");
    const ExprId empty_field = push_field(analyzer.ctx_.module, push_name(analyzer.ctx_.module, "lib"), {});
    const ExprId scoped_name = push_name(analyzer.ctx_.module, "Name", "scope");
    const ExprId not_selector = push_integer(analyzer.ctx_.module);
    analyzer.state_.checked.expr_types.assign(analyzer.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.assign(analyzer.ctx_.module.exprs.size(), sema::INVALID_IDENT_ID);

    const sema::SemanticAnalyzerCore::ModuleSelectorPath invalid_path =
        analyzer.expr_selector_path(syntax::INVALID_EXPR_ID);
    EXPECT_TRUE(invalid_path.parts.empty());
    EXPECT_TRUE(analyzer.expr_selector_path(empty_field).parts.empty());
    EXPECT_TRUE(analyzer.expr_selector_path(scoped_name).parts.empty());
    EXPECT_TRUE(analyzer.expr_selector_path(not_selector).parts.empty());

    const sema::SemanticAnalyzerCore::ModuleSelectorPath core_mem_io_path = analyzer.expr_selector_path(core_mem_io);
    ASSERT_EQ(core_mem_io_path.parts.size(), 3U);
    EXPECT_EQ(core_mem_io_path.parts[0], SEMA_TEST_ROOT_MODULE_NAME);
    EXPECT_EQ(core_mem_io_path.parts[1], SEMA_TEST_CHILD_MODULE_NAME);
    EXPECT_EQ(core_mem_io_path.parts[2], SEMA_TEST_LEAF_MODULE_NAME);

    const sema::SemanticAnalyzerCore::ModuleSelector alias_selector =
        analyzer.resolve_module_selector(alias_expr, true);
    EXPECT_EQ(alias_selector.module.value, SEMA_TEST_LIB_ONE_MODULE_INDEX);
    EXPECT_FALSE(alias_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector root_selector = analyzer.resolve_module_selector(core_mem, true);
    EXPECT_EQ(root_selector.module.value, 2U);
    EXPECT_FALSE(root_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector leaf_selector =
        analyzer.resolve_module_selector(core_mem_io, true);
    EXPECT_EQ(leaf_selector.module.value, 3U);
    EXPECT_FALSE(leaf_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector unknown_alias_selector =
        analyzer.resolve_module_selector(invalid_expr, true);
    EXPECT_FALSE(syntax::is_valid(unknown_alias_selector.module));
    EXPECT_TRUE(unknown_alias_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector missing_child_selector =
        analyzer.resolve_module_selector(missing_root_child, true);
    EXPECT_FALSE(syntax::is_valid(missing_child_selector.module));
    EXPECT_TRUE(missing_child_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector prefix_member_selector =
        analyzer.resolve_module_selector(core_mem_file, true);
    EXPECT_FALSE(syntax::is_valid(prefix_member_selector.module));
    EXPECT_FALSE(prefix_member_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector local_selector =
        analyzer.resolve_module_selector(local_member, true);
    EXPECT_FALSE(syntax::is_valid(local_selector.module));
    EXPECT_FALSE(local_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector alias_member_selector =
        analyzer.resolve_module_selector(alias_member, true);
    EXPECT_FALSE(syntax::is_valid(alias_member_selector.module));
    EXPECT_FALSE(alias_member_selector.failed_as_module_selector);

    EXPECT_TRUE(analyzer.visible_root_module_name_exists(SEMA_TEST_ROOT_MODULE_NAME));
    EXPECT_FALSE(analyzer.visible_root_module_name_exists({}));
    EXPECT_TRUE(
        analyzer.visible_module_path_prefix_exists({SEMA_TEST_ROOT_MODULE_NAME, SEMA_TEST_CHILD_MODULE_NAME, "File"}));
    EXPECT_FALSE(analyzer.visible_module_path_prefix_exists({SEMA_TEST_ROOT_MODULE_NAME}));
    EXPECT_FALSE(analyzer.visible_module_path_prefix_exists({"missing", "path"}));

    EXPECT_FALSE(analyzer.can_define_local_name(lib_id, "lib", {}));
    EXPECT_FALSE(analyzer.can_define_local_name(root_module_id, SEMA_TEST_ROOT_MODULE_NAME, {}));
    EXPECT_FALSE(analyzer.can_define_local_name(local_type_id, "LocalType", {}));
    EXPECT_TRUE(analyzer.can_define_local_name(fresh_id, "fresh", {}));

    sema::SemanticAnalyzerCore::GenericContext generic_context;
    generic_context.params.emplace(t_id, bool_type);
    analyzer.state_.flow.current_generic_context = &generic_context;
    EXPECT_TRUE(analyzer.current_generic_param_exists(t_id, "T"));
    EXPECT_TRUE(analyzer.visible_type_name_exists(t_id, "T"));
    EXPECT_FALSE(analyzer.can_define_local_name(t_id, "T", {}));
    analyzer.state_.flow.current_generic_context = nullptr;

    const IdentId missing_id = intern_identifier(analyzer, "missing");
    EXPECT_TRUE(analyzer.top_level_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), global_id, "global"));
    EXPECT_FALSE(analyzer.top_level_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), missing_id, "missing"));
    EXPECT_TRUE(
        analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), local_type_id, "LocalType"));
    EXPECT_TRUE(analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), global_id, "global"));
    EXPECT_FALSE(
        analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), missing_id, "missing"));

    EXPECT_EQ(analyzer.item_module(item_id).value, SEMA_TEST_ROOT_MODULE_INDEX);
    EXPECT_FALSE(syntax::is_valid(analyzer.item_module(syntax::ItemId{99})));
    analyzer.ctx_.module.item_modules.clear();
    EXPECT_FALSE(syntax::is_valid(analyzer.item_module(item_id)));
}
TEST(CoreUnit, SemanticWhiteBoxTypedLookupRejectsAmbiguousPublicReexports)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
        module_info({"lib", "two"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(1), "one", syntax::Visibility::public_),
        resolved_import(module_id(2), "two", syntax::Visibility::public_),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle one_type = types.named_struct("lib.one.Shared", "lib_one_Shared", false);
    const TypeHandle two_type = types.named_struct("lib.two.Shared", "lib_two_Shared", false);
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const IdentId shared_id = intern_identifier(analyzer, "Shared");
    const IdentId run_id = intern_identifier(analyzer, "run");
    const IdentId value_id = intern_identifier(analyzer, "VALUE");

    static_cast<void>(add_named_type(analyzer, module_id(1), "Shared", one_type));
    static_cast<void>(add_named_type(analyzer, module_id(2), "Shared", two_type));
    EXPECT_FALSE(
        is_valid(analyzer.find_type_in_module(module_id(SEMA_TEST_ROOT_MODULE_INDEX), shared_id, "Shared", {}, false)));

    static_cast<void>(add_function(analyzer, indexed_function_signature(analyzer, "run", module_id(1), i32)));
    static_cast<void>(add_function(analyzer, indexed_function_signature(analyzer, "run", module_id(2), i32)));
    EXPECT_EQ(analyzer.find_function_in_module(module_id(SEMA_TEST_ROOT_MODULE_INDEX), run_id, "run", {}), nullptr);

    static_cast<void>(add_global_value(analyzer, module_id(1), "VALUE", i32));
    static_cast<void>(add_global_value(analyzer, module_id(2), "VALUE", i32));
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(SEMA_TEST_ROOT_MODULE_INDEX), value_id, "VALUE", {}), nullptr);
}
TEST(CoreUnit, SemanticWhiteBoxFunctionAndEnumLookupFallbackEdges)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"owner"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "owner"),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle array_i32 = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, i32);
    const TypeHandle fn_type = types.function(sema::FunctionCallConv::aurex, false, false, {i32}, bool_type);
    const TypeHandle unsafe_fn_type = types.function(sema::FunctionCallConv::aurex, true, false, {}, bool_type);

    FunctionSignature callable =
        indexed_function_signature(analyzer, "callable", module_id(SEMA_TEST_ROOT_MODULE_INDEX), bool_type);
    callable.param_types = {i32};
    static_cast<void>(add_function(analyzer, callable));
    const Symbol callable_symbol = indexed_symbol(
        analyzer, SymbolKind::function, "callable", module_id(SEMA_TEST_ROOT_MODULE_INDEX), INVALID_TYPE_HANDLE);
    EXPECT_TRUE(types.is_function(analyzer.function_type_from_symbol(callable_symbol, {})));

    const Symbol fallback_symbol =
        indexed_symbol(analyzer, SymbolKind::local, "local_fn", syntax::INVALID_MODULE_ID, fn_type);
    EXPECT_TRUE(types.same(analyzer.function_type_from_symbol(fallback_symbol, {}), fn_type));

    analyzer.validate_unsafe_function_value_call(i32, {});
    analyzer.validate_unsafe_function_value_call(unsafe_fn_type, {});
    EXPECT_TRUE(diagnostics.has_error());
    analyzer.state_.flow.unsafe_context_depth += 1;
    analyzer.validate_unsafe_function_value_call(unsafe_fn_type, {});
    analyzer.state_.flow.unsafe_context_depth -= 1;

    EXPECT_FALSE(analyzer.find_enum_cases_by_type(INVALID_TYPE_HANDLE));
    EnumCaseInfo invalid_case;
    invalid_case.name = checked_text(analyzer.state_.checked, "invalid");
    invalid_case.case_name = checked_text(analyzer.state_.checked, "invalid");
    invalid_case.type = INVALID_TYPE_HANDLE;
    analyzer.index_enum_case(invalid_case);
    EXPECT_TRUE(analyzer.state_.names.enum_cases_by_type.empty());

    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    const TypeHandle enum_type = types.named_enum("Choice", "Choice");
    types.set_enum_underlying(enum_type, i32);
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Record", record_type));
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Choice", enum_type));
    const EnumCaseInfo* const yes_case = add_enum_case(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Choice_yes",
        "yes", enum_type, INVALID_TYPE_HANDLE, {array_i32})
                                             .second;
    ASSERT_NE(yes_case, nullptr);
    ASSERT_NE(analyzer.find_enum_cases_by_type(enum_type), nullptr);
    const IdentId yes_id = intern_identifier(analyzer, "yes");
    const IdentId case_id = intern_identifier(analyzer, "case");
    const IdentId record_id = intern_identifier(analyzer, "Record");
    const IdentId missing_id = intern_identifier(analyzer, "missing");
    const IdentId missing_enum_id = intern_identifier(analyzer, "Missing");
    const base::usize interned_cases = analyzer.ctx_.module.identifiers.size();
    const EnumCaseInfo* indexed_yes = analyzer.find_enum_case_by_type_and_case(enum_type, yes_id, "yes");
    ASSERT_NE(indexed_yes, nullptr);
    EXPECT_EQ(indexed_yes->case_name, "yes");
    EXPECT_EQ(analyzer.find_enum_case_by_type_and_case(record_type, yes_id, "yes"), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_by_type_and_case(enum_type, missing_id, "missing"), nullptr);
    EXPECT_EQ(analyzer.ctx_.module.identifiers.size(), interned_cases);
    EXPECT_EQ(analyzer.state_.names.enum_cases_by_type_and_case.size(), 1U);
    EXPECT_TRUE(sema::is_valid(analyzer.ctx_.module.find_identifier("yes")));

    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(missing_enum_id, "Missing", case_id, "case", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(record_id, "Record", case_id, "case", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(
                  intern_identifier(analyzer, "Choice"), "Choice", yes_id, "yes", {}, true),
        yes_case);

    const ExprId record_name = push_name(analyzer.ctx_.module, "Record");
    const ExprId record_case = push_field(analyzer.ctx_.module, record_name, "case");
    const ExprId choice_name = push_name(analyzer.ctx_.module, "Choice");
    const ExprId choice_missing = push_field(analyzer.ctx_.module, choice_name, "missing");
    const ExprId choice_yes = push_field(analyzer.ctx_.module, choice_name, "yes");
    const ExprId payload_value = push_name(analyzer.ctx_.module, "payload");
    const ExprId enum_call_id = push_call(analyzer.ctx_.module, choice_yes, {payload_value});

    analyzer.state_.checked.expr_types.assign(analyzer.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.assign(analyzer.ctx_.module.exprs.size(), sema::INVALID_IDENT_ID);
    static_cast<void>(
        add_global_value(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "payload", array_i32, SymbolKind::local));

    EXPECT_EQ(analyzer.find_enum_constructor(record_case, true), nullptr);
    EXPECT_EQ(analyzer.find_enum_constructor(choice_missing, true), nullptr);
    EXPECT_NE(analyzer.find_enum_constructor(choice_yes, true), nullptr);
    EXPECT_TRUE(types.same(
        analyzer.analyze_enum_constructor_call(enum_call_id, analyzer.expr_view(enum_call_id), *yes_case), enum_type));

    const ExprId argument_call_id = push_call(analyzer.ctx_.module, syntax::INVALID_EXPR_ID, {payload_value});
    const std::vector<TypeHandle> array_param_types{array_i32};
    analyzer.validate_call_arguments(analyzer.expr_view(argument_call_id), "array_arg", array_param_types, 0, false);
    const ExprId variadic_argument_call_id =
        push_call(analyzer.ctx_.module, syntax::INVALID_EXPR_ID, {payload_value, payload_value});
    const std::vector<TypeHandle> no_param_types;
    analyzer.validate_call_arguments(
        analyzer.expr_view(variadic_argument_call_id), "array_vararg", no_param_types, 0, true);
}
TEST(CoreUnit, SemanticWhiteBoxTypedLookupIndexesCoverHotPaths)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle record_type = types.named_struct("lib.Record", "lib_Record", false);
    const TypeHandle enum_type = types.named_enum("lib.Choice", "lib_Choice");
    types.set_enum_underlying(enum_type, i32);

    const IdentId record_id = intern_identifier(analyzer, "Record");
    const IdentId alias_id = intern_identifier(analyzer, "Alias");
    const IdentId box_id = intern_identifier(analyzer, "Box");
    const IdentId make_id = intern_identifier(analyzer, "make");
    const IdentId generic_method_id = intern_identifier(analyzer, "generic_method");
    const IdentId run_id = intern_identifier(analyzer, "run");
    const IdentId method_id = intern_identifier(analyzer, "method");
    const IdentId value_id = intern_identifier(analyzer, "VALUE");
    const IdentId choice_yes_id = intern_identifier(analyzer, "Choice_yes");
    const IdentId yes_id = intern_identifier(analyzer, "yes");
    const IdentId missing_id = intern_identifier(analyzer, "missing");

    const sema::ModuleLookupKey record_key =
        add_named_type(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Record", record_type);
    StructInfo record_info;
    record_info.name = checked_text(analyzer.state_.checked, "Record");
    record_info.name_id = record_id;
    record_info.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    record_info.type = record_type;
    const auto record_inserted = analyzer.state_.checked.structs.emplace(record_key, record_info);
    ASSERT_TRUE(record_inserted.second);
    analyzer.state_.types.struct_infos_by_type[record_type.value] = &record_inserted.first->second;

    const TypeId i32_type_id = analyzer.ctx_.module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    sema::TypeAliasInfo alias;
    alias.name = checked_text(analyzer.state_.checked, "Alias");
    alias.name_id = alias_id;
    alias.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    alias.target = i32_type_id;
    alias.visibility = syntax::Visibility::public_;
    const auto alias_inserted = analyzer.state_.checked.type_aliases.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Alias"), alias);
    ASSERT_TRUE(alias_inserted.second);
    analyzer.index_type_alias(alias_inserted.first->second);

    sema::SemanticAnalyzerCore::GenericTemplateInfo generic_type;
    generic_type.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    generic_type.name = checked_text(analyzer.state_.checked, "Box");
    generic_type.name_id = box_id;
    generic_type.key = semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Box");
    generic_type.visibility = syntax::Visibility::public_;
    const auto generic_type_inserted =
        analyzer.state_.generics.struct_templates.emplace(generic_type.key, generic_type);
    ASSERT_TRUE(generic_type_inserted.second);
    analyzer.index_generic_struct_template(generic_type_inserted.first->second);

    sema::SemanticAnalyzerCore::GenericTemplateInfo generic_function;
    generic_function.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    generic_function.name = checked_text(analyzer.state_.checked, "make");
    generic_function.name_id = make_id;
    generic_function.key = semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "make");
    generic_function.visibility = syntax::Visibility::public_;
    const auto generic_function_inserted =
        analyzer.state_.generics.function_templates.emplace(generic_function.key, generic_function);
    ASSERT_TRUE(generic_function_inserted.second);
    analyzer.index_generic_function_template(generic_function_inserted.first->second);

    sema::SemanticAnalyzerCore::GenericTemplateInfo generic_method;
    generic_method.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    generic_method.name = checked_text(analyzer.state_.checked, "generic_method");
    generic_method.name_id = generic_method_id;
    generic_method.key = semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "generic_method");
    generic_method.function_key =
        semantic_method_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_type, "generic_method");
    generic_method.impl_type_pattern = record_type;
    generic_method.visibility = syntax::Visibility::public_;
    const auto generic_method_inserted =
        analyzer.state_.generics.method_templates.emplace(generic_method.function_key, generic_method);
    ASSERT_TRUE(generic_method_inserted.second);
    analyzer.index_generic_method_template(generic_method_inserted.first->second);

    FunctionSignature run = indexed_function_signature(analyzer, "run", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32);
    run.visibility = syntax::Visibility::public_;
    run.semantic_key = semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "run");
    const auto run_inserted = analyzer.state_.checked.functions.emplace(run.semantic_key, run);
    ASSERT_TRUE(run_inserted.second);
    analyzer.index_function_lookup(run_inserted.first->second);

    FunctionSignature method =
        indexed_function_signature(analyzer, "method", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32);
    method.is_method = true;
    method.has_self_param = true;
    method.method_owner_type = record_type;
    method.visibility = syntax::Visibility::public_;
    method.semantic_key =
        semantic_method_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_type, "method");
    const auto method_inserted = analyzer.state_.checked.functions.emplace(method.semantic_key, method);
    ASSERT_TRUE(method_inserted.second);
    analyzer.index_method_lookup(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_type, method_id, method_inserted.first->second);

    const auto value_inserted = analyzer.state_.functions.global_values.emplace(
        semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "VALUE"),
        indexed_symbol(analyzer, SymbolKind::const_, "VALUE", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32));
    ASSERT_TRUE(value_inserted.second);
    analyzer.index_global_value(value_inserted.first->second);

    EnumCaseInfo case_info;
    case_info.name = checked_text(analyzer.state_.checked, "Choice_yes");
    case_info.name_id = choice_yes_id;
    case_info.case_name = checked_text(analyzer.state_.checked, "yes");
    case_info.case_name_id = yes_id;
    case_info.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    case_info.type = enum_type;
    const auto case_inserted = analyzer.state_.checked.enum_cases.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Choice_yes"), case_info);
    ASSERT_TRUE(case_inserted.second);
    analyzer.index_enum_case(case_inserted.first->second);

    EXPECT_TRUE(analyzer.named_type_lookup_complete());
    EXPECT_TRUE(analyzer.type_alias_lookup_complete());
    EXPECT_TRUE(analyzer.generic_struct_lookup_complete());
    EXPECT_TRUE(analyzer.generic_function_lookup_complete());
    EXPECT_TRUE(analyzer.generic_method_lookup_complete());
    EXPECT_TRUE(analyzer.function_lookup_complete());
    EXPECT_TRUE(analyzer.global_value_lookup_complete());
    EXPECT_TRUE(analyzer.enum_case_module_lookup_complete());

    EXPECT_TRUE(analyzer.top_level_value_name_exists(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), value_id, "VALUE"));
    EXPECT_TRUE(
        analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_id, "Record"));
    EXPECT_TRUE(
        analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), value_id, "VALUE"));
    EXPECT_TRUE(analyzer.visible_type_name_exists(record_id, "Record"));
    EXPECT_TRUE(types.same(
        analyzer.find_type_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_id, "Record", {}, false),
        record_type));
    EXPECT_TRUE(types.same(
        analyzer.find_type_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), alias_id, "Alias", {}, false), i32));
    EXPECT_EQ(
        analyzer.find_any_generic_type_template_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), box_id, "Box"),
        &generic_type_inserted.first->second);
    EXPECT_EQ(analyzer.find_generic_function_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), make_id, "make", {}),
        &generic_function_inserted.first->second);
    EXPECT_EQ(analyzer.find_function_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), run_id, "run", {}),
        &run_inserted.first->second);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, method_id, "method", {}, true),
        &method_inserted.first->second);
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), value_id, "VALUE", {}),
        &value_inserted.first->second);

    analyzer.state_.flow.current_module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    EXPECT_EQ(
        analyzer.find_enum_case_in_visible_modules(choice_yes_id, "Choice_yes", {}), &case_inserted.first->second);
    const base::usize interned_before_miss = analyzer.ctx_.module.identifiers.size();
    EXPECT_EQ(
        analyzer.find_symbol_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "missing", {}, false),
        nullptr);
    EXPECT_EQ(
        analyzer.find_function_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "missing", {}, false),
        nullptr);
    EXPECT_EQ(analyzer.ctx_.module.identifiers.size(), interned_before_miss);
}
TEST(CoreUnit, SemanticWhiteBoxGenericLookupUsesSelectiveReexports)
{
    constexpr u32 SEMA_TEST_SELECTIVE_FACADE_MODULE_INDEX = 1;
    constexpr u32 SEMA_TEST_SELECTIVE_INNER_MODULE_INDEX = 2;

    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"facade"}),
        module_info({"inner"}),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const ModuleId facade_module = module_id(SEMA_TEST_SELECTIVE_FACADE_MODULE_INDEX);
    const ModuleId inner_module = module_id(SEMA_TEST_SELECTIVE_INNER_MODULE_INDEX);
    const IdentId box_id = intern_identifier(analyzer, "Box");
    const IdentId re_box_id = intern_identifier(analyzer, "ReBox");
    const IdentId choice_id = intern_identifier(analyzer, "Choice");
    const IdentId re_choice_id = intern_identifier(analyzer, "ReChoice");
    const IdentId alias_box_id = intern_identifier(analyzer, "AliasBox");
    const IdentId re_alias_box_id = intern_identifier(analyzer, "ReAliasBox");
    const IdentId make_id = intern_identifier(analyzer, "make");
    const IdentId re_make_id = intern_identifier(analyzer, "re_make");

    analyzer.ctx_.module.modules[SEMA_TEST_SELECTIVE_FACADE_MODULE_INDEX].reexports = {
        resolved_use(inner_module, "Box", "ReBox", syntax::Visibility::public_, box_id, re_box_id),
        resolved_use(inner_module, "Choice", "ReChoice", syntax::Visibility::public_, choice_id, re_choice_id),
        resolved_use(
            inner_module, "AliasBox", "ReAliasBox", syntax::Visibility::public_, alias_box_id, re_alias_box_id),
        resolved_use(inner_module, "make", "re_make", syntax::Visibility::public_, make_id, re_make_id),
    };

    sema::SemanticAnalyzerCore::GenericTemplateInfo box = generic_template_info(analyzer, inner_module, "Box");
    const auto box_inserted = analyzer.state_.generics.struct_templates.emplace(box.key, std::move(box));
    ASSERT_TRUE(box_inserted.second);
    analyzer.index_generic_struct_template(box_inserted.first->second);

    sema::SemanticAnalyzerCore::GenericTemplateInfo choice = generic_template_info(analyzer, inner_module, "Choice");
    const auto choice_inserted = analyzer.state_.generics.enum_templates.emplace(choice.key, std::move(choice));
    ASSERT_TRUE(choice_inserted.second);
    analyzer.index_generic_enum_template(choice_inserted.first->second);

    sema::SemanticAnalyzerCore::GenericTemplateInfo alias_box =
        generic_template_info(analyzer, inner_module, "AliasBox");
    const auto alias_box_inserted =
        analyzer.state_.generics.type_alias_templates.emplace(alias_box.key, std::move(alias_box));
    ASSERT_TRUE(alias_box_inserted.second);
    analyzer.index_generic_type_alias_template(alias_box_inserted.first->second);

    sema::SemanticAnalyzerCore::GenericTemplateInfo make = generic_template_info(analyzer, inner_module, "make");
    const auto make_inserted = analyzer.state_.generics.function_templates.emplace(make.key, std::move(make));
    ASSERT_TRUE(make_inserted.second);
    analyzer.index_generic_function_template(make_inserted.first->second);

    EXPECT_EQ(analyzer.find_generic_struct_in_module(facade_module, re_box_id, "ReBox", {}, false),
        &box_inserted.first->second);
    EXPECT_EQ(analyzer.find_generic_enum_in_module(facade_module, re_choice_id, "ReChoice", {}, false),
        &choice_inserted.first->second);
    EXPECT_EQ(analyzer.find_generic_type_alias_in_module(facade_module, re_alias_box_id, "ReAliasBox", {}, false),
        &alias_box_inserted.first->second);
    EXPECT_TRUE(analyzer.generic_type_template_exists_in_module(facade_module, re_box_id, "ReBox"));
    EXPECT_EQ(analyzer.find_generic_function_in_module(facade_module, re_make_id, "re_make", {}, false),
        &make_inserted.first->second);

    const base::usize diagnostic_count = diagnostics.diagnostics().size();
    analyzer.report_generic_type_template_in_module(facade_module, re_box_id, "ReBox", {});
    ASSERT_EQ(diagnostics.diagnostics().size(), diagnostic_count + 1U);
    EXPECT_EQ(diagnostics.diagnostics()[diagnostic_count].message, "generic type ReBox requires type arguments");
}
TEST(CoreUnit, SemanticWhiteBoxTypedLookupRejectsUnindexedLegacyMaps)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle record_type = types.named_struct("lib.LegacyRecord", "lib_LegacyRecord", false);
    const TypeHandle enum_type = types.named_enum("lib.LegacyChoice", "lib_LegacyChoice");
    types.set_enum_underlying(enum_type, i32);

    const IdentId legacy_record_id = intern_identifier(analyzer, "LegacyRecord");
    const IdentId legacy_alias_id = intern_identifier(analyzer, "LegacyAlias");
    const IdentId legacy_box_id = intern_identifier(analyzer, "LegacyBox");
    const IdentId private_box_id = intern_identifier(analyzer, "PrivateBox");
    const IdentId legacy_enum_id = intern_identifier(analyzer, "LegacyEnum");
    const IdentId legacy_alias_box_id = intern_identifier(analyzer, "LegacyAliasBox");
    const IdentId legacy_make_id = intern_identifier(analyzer, "LegacyMake");
    const IdentId legacy_fn_id = intern_identifier(analyzer, "legacy_fn");
    const IdentId owner_method_id = intern_identifier(analyzer, "owner_method");
    const IdentId legacy_value_id = intern_identifier(analyzer, "LEGACY_VALUE");
    const IdentId legacy_choice_yes_id = intern_identifier(analyzer, "LegacyChoice_yes");
    const IdentId yes_id = intern_identifier(analyzer, "yes");
    const IdentId missing_id = intern_identifier(analyzer, "missing");

    const sema::ModuleLookupKey record_key =
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyRecord");
    analyzer.state_.types.named_types.emplace(record_key, record_type);

    StructInfo record_info;
    record_info.name = checked_text(analyzer.state_.checked, "LegacyRecord");
    record_info.name_id = legacy_record_id;
    record_info.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    record_info.type = record_type;
    const auto record_inserted = analyzer.state_.checked.structs.emplace(record_key, record_info);
    ASSERT_TRUE(record_inserted.second);
    analyzer.state_.types.struct_infos_by_type[record_type.value] = &record_inserted.first->second;

    const TypeId i32_type_id = analyzer.ctx_.module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    sema::TypeAliasInfo alias;
    alias.name = checked_text(analyzer.state_.checked, "LegacyAlias");
    alias.name_id = legacy_alias_id;
    alias.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    alias.target = i32_type_id;
    alias.visibility = syntax::Visibility::public_;
    analyzer.state_.checked.type_aliases.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAlias"), alias);

    analyzer.state_.generics.struct_templates.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyBox"),
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyBox"));
    analyzer.state_.generics.struct_templates.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "PrivateBox"),
        generic_template_info(
            analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "PrivateBox", syntax::Visibility::private_));
    analyzer.state_.generics.enum_templates.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyEnum"),
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyEnum"));
    analyzer.state_.generics.type_alias_templates.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAliasBox"),
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAliasBox"));
    analyzer.state_.generics.function_templates.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyMake"),
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyMake"));

    FunctionSignature fallback_function =
        indexed_function_signature(analyzer, "legacy_fn", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32);
    fallback_function.visibility = syntax::Visibility::public_;
    fallback_function.semantic_key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "legacy_fn");
    analyzer.state_.checked.functions.emplace(fallback_function.semantic_key, fallback_function);

    FunctionSignature owner_method =
        indexed_function_signature(analyzer, "owner_method", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32);
    owner_method.is_method = true;
    owner_method.has_self_param = true;
    owner_method.method_owner_type = record_type;
    owner_method.visibility = syntax::Visibility::public_;
    owner_method.param_types = {record_type};
    owner_method.semantic_key =
        semantic_method_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_type, "owner_method");
    const auto owner_method_inserted =
        analyzer.state_.checked.functions.emplace(owner_method.semantic_key, owner_method);
    ASSERT_TRUE(owner_method_inserted.second);

    const auto value_inserted = analyzer.state_.functions.global_values.emplace(
        semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LEGACY_VALUE"),
        indexed_symbol(analyzer, SymbolKind::const_, "LEGACY_VALUE", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32));
    ASSERT_TRUE(value_inserted.second);

    EnumCaseInfo enum_case;
    enum_case.name = checked_text(analyzer.state_.checked, "LegacyChoice_yes");
    enum_case.name_id = legacy_choice_yes_id;
    enum_case.case_name = checked_text(analyzer.state_.checked, "yes");
    enum_case.case_name_id = yes_id;
    enum_case.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    enum_case.type = enum_type;
    const auto enum_case_inserted = analyzer.state_.checked.enum_cases.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyChoice_yes"), enum_case);
    ASSERT_TRUE(enum_case_inserted.second);

    EXPECT_FALSE(analyzer.named_type_lookup_complete());
    EXPECT_FALSE(analyzer.type_alias_lookup_complete());
    EXPECT_FALSE(analyzer.generic_struct_lookup_complete());
    EXPECT_FALSE(analyzer.generic_enum_lookup_complete());
    EXPECT_FALSE(analyzer.generic_type_alias_lookup_complete());
    EXPECT_FALSE(analyzer.generic_function_lookup_complete());
    EXPECT_FALSE(analyzer.function_lookup_complete());
    EXPECT_FALSE(analyzer.global_value_lookup_complete());
    EXPECT_FALSE(analyzer.enum_case_module_lookup_complete());

    EXPECT_FALSE(analyzer.top_level_value_name_exists(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_value_id, "LEGACY_VALUE"));
    EXPECT_FALSE(analyzer.module_type_or_value_name_exists(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_record_id, "LegacyRecord"));
    EXPECT_FALSE(analyzer.visible_type_name_exists(legacy_alias_id, "LegacyAlias"));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_alias_id, "LegacyAlias", {}, false)));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_record_id, "LegacyRecord", {}, false)));

    analyzer.state_.flow.current_module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    EXPECT_EQ(analyzer.find_generic_struct_in_visible_modules(legacy_box_id, "LegacyBox", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_generic_enum_in_visible_modules(legacy_enum_id, "LegacyEnum", {}, false), nullptr);
    EXPECT_EQ(
        analyzer.find_generic_type_alias_in_visible_modules(legacy_alias_box_id, "LegacyAliasBox", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_generic_function_in_visible_modules(legacy_make_id, "LegacyMake", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_function_in_visible_modules(legacy_fn_id, "legacy_fn", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_method_in_owner_module(record_type, owner_method_id, "owner_method", true), nullptr);
    EXPECT_EQ(analyzer.find_symbol_in_module(
                  module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_value_id, "LEGACY_VALUE", {}, false),
        nullptr);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules(legacy_choice_yes_id, "LegacyChoice_yes", {}, false), nullptr);

    analyzer.index_named_type(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_record_id, record_type, syntax::Visibility::public_);
    analyzer.index_type_alias(analyzer.state_.checked.type_aliases
            .find(semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAlias"))
            ->second);
    analyzer.index_generic_struct_template(analyzer.state_.generics.struct_templates
            .find(semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyBox"))
            ->second);
    analyzer.index_generic_enum_template(analyzer.state_.generics.enum_templates
            .find(semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyEnum"))
            ->second);
    analyzer.index_generic_type_alias_template(analyzer.state_.generics.type_alias_templates
            .find(semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAliasBox"))
            ->second);
    analyzer.index_generic_function_template(analyzer.state_.generics.function_templates
            .find(semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyMake"))
            ->second);
    analyzer.index_function_lookup(analyzer.state_.checked.functions
            .find(semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "legacy_fn"))
            ->second);
    analyzer.index_method_lookup(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_type, owner_method_id, owner_method_inserted.first->second);
    analyzer.index_global_value(value_inserted.first->second);
    analyzer.index_enum_case(enum_case_inserted.first->second);

    EXPECT_TRUE(analyzer.top_level_value_name_exists(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_value_id, "LEGACY_VALUE"));
    EXPECT_TRUE(analyzer.module_type_or_value_name_exists(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_record_id, "LegacyRecord"));
    EXPECT_TRUE(analyzer.visible_type_name_exists(legacy_alias_id, "LegacyAlias"));
    EXPECT_TRUE(types.same(analyzer.find_type_in_module(
                               module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_alias_id, "LegacyAlias", {}, false),
        i32));
    EXPECT_NE(analyzer.find_generic_struct_in_visible_modules(legacy_box_id, "LegacyBox", {}, false), nullptr);
    EXPECT_NE(analyzer.find_generic_enum_in_visible_modules(legacy_enum_id, "LegacyEnum", {}, false), nullptr);
    EXPECT_NE(
        analyzer.find_generic_type_alias_in_visible_modules(legacy_alias_box_id, "LegacyAliasBox", {}, false), nullptr);
    EXPECT_NE(analyzer.find_generic_function_in_visible_modules(legacy_make_id, "LegacyMake", {}, false), nullptr);
    EXPECT_TRUE(analyzer.generic_type_template_exists_in_module(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_box_id, "LegacyBox"));
    EXPECT_TRUE(analyzer.report_generic_type_requires_args_if_visible(legacy_box_id, "LegacyBox", {}));
    analyzer.report_generic_type_template_in_module(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_box_id, "LegacyBox", {});
    analyzer.report_generic_type_template_in_module(syntax::INVALID_MODULE_ID, missing_id, "MissingGeneric", {});
    EXPECT_EQ(analyzer.find_generic_struct_in_module(syntax::INVALID_MODULE_ID, missing_id, "MissingStruct", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_struct_in_module(
                  module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "MissingStruct", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_struct_in_module(
                  module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), private_box_id, "PrivateBox", {}, false),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_enum_in_visible_modules(missing_id, "MissingEnum", {}, true), nullptr);
    EXPECT_EQ(
        analyzer.find_generic_enum_in_module(syntax::INVALID_MODULE_ID, missing_id, "MissingEnum", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_generic_enum_in_module(
                  module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "MissingEnum", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_type_alias_in_visible_modules(missing_id, "MissingAlias", {}, true), nullptr);
    EXPECT_EQ(
        analyzer.find_generic_type_alias_in_module(syntax::INVALID_MODULE_ID, missing_id, "MissingAlias", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_type_alias_in_module(
                  module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "MissingAlias", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_function_in_visible_modules(missing_id, "missing_function", {}, true), nullptr);
    EXPECT_EQ(
        analyzer.find_generic_function_in_module(syntax::INVALID_MODULE_ID, missing_id, "missing_function", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_function_in_module(
                  module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "missing", {}, false),
        nullptr);
    EXPECT_EQ(analyzer.find_function_in_visible_modules(legacy_fn_id, "legacy_fn", {}, false),
        &analyzer.state_.checked.functions
            .find(semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "legacy_fn"))
            ->second);
    EXPECT_EQ(analyzer.find_method_in_owner_module(record_type, owner_method_id, "owner_method", true),
        &owner_method_inserted.first->second);
    EXPECT_EQ(analyzer.find_method_in_owner_module(record_type, missing_id, "missing", true), nullptr);
    EXPECT_EQ(
        analyzer.find_method_in_owner_module(INVALID_TYPE_HANDLE, owner_method_id, "owner_method", true), nullptr);
    EXPECT_EQ(
        analyzer.find_symbol_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_value_id, "LEGACY_VALUE", {}),
        &value_inserted.first->second);
    EXPECT_EQ(analyzer.find_symbol_in_module(syntax::INVALID_MODULE_ID, missing_id, "missing", {}), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules(legacy_choice_yes_id, "LegacyChoice_yes", {}),
        &enum_case_inserted.first->second);
}
TEST(CoreUnit, SemanticWhiteBoxParserOnlyModuleContractIsNormalized)
{
    using CheckedExprCNameTable = decltype(sema::CheckedModule{}.expr_c_name_ids);
    using CheckedPatternCNameTable = decltype(sema::CheckedModule{}.pattern_c_name_ids);
    using CheckedItemCNameTable = decltype(sema::CheckedModule{}.item_c_name_ids);
    using CheckedGenericLayoutTable = decltype(sema::CheckedModule{}.generic_side_table_layouts);
    using CheckedGenericInstanceTable = decltype(sema::CheckedModule{}.generic_function_instances);
    using AnalyzerGenericMethodIndex =
        decltype(std::declval<sema::SemanticAnalyzerCore&>().state_.names.generic_method_templates_by_name);
    using AnalyzerEnumCaseTypeIndex =
        decltype(std::declval<sema::SemanticAnalyzerCore&>().state_.names.enum_cases_by_type);
    using AnalyzerVisibleModuleCache =
        decltype(std::declval<sema::SemanticAnalyzerCore&>().state_.modules.visible_modules_cache);
    using GenericTemplateParams = decltype(std::declval<sema::SemanticAnalyzerCore::GenericTemplateInfo&>().params);
    using GenericTemplateConstraints =
        decltype(std::declval<sema::SemanticAnalyzerCore::GenericTemplateInfo&>().constraints);
    using FunctionParamTypes = decltype(std::declval<sema::FunctionSignature&>().param_types);
    using FunctionGenericArgs = decltype(std::declval<sema::FunctionSignature&>().generic_args);
    using StructFields = decltype(std::declval<sema::StructInfo&>().fields);
    using EnumPayloadTypes = decltype(std::declval<sema::EnumCaseInfo&>().payload_types);
    using TypeTupleElements = decltype(std::declval<sema::TypeInfo&>().tuple_elements);
    using TypeFunctionParams = decltype(std::declval<sema::TypeInfo&>().function_params);
    using TypeGenericArgs = decltype(std::declval<sema::TypeInfo&>().generic_args);

    static_assert(std::is_same_v<CheckedExprCNameTable::value_type, sema::IdentId>);
    static_assert(std::is_same_v<CheckedPatternCNameTable::value_type, sema::IdentId>);
    static_assert(std::is_same_v<CheckedItemCNameTable::value_type, sema::IdentId>);
    static_assert(std::is_same_v<CheckedGenericLayoutTable, sema::SemaDeque<sema::GenericSideTableLayout>>);
    static_assert(std::is_same_v<CheckedGenericInstanceTable, sema::SemaDeque<sema::GenericFunctionInstanceInfo>>);
    static_assert(
        std::is_same_v<AnalyzerGenericMethodIndex::mapped_type, sema::SemanticAnalyzerCore::GenericTemplateList>);
    static_assert(std::is_same_v<AnalyzerEnumCaseTypeIndex::mapped_type, sema::SemanticAnalyzerCore::EnumCaseList>);
    static_assert(std::is_same_v<AnalyzerVisibleModuleCache::mapped_type, sema::SemanticAnalyzerCore::ModuleIdList>);
    static_assert(std::is_same_v<GenericTemplateParams, sema::SemaVector<sema::IdentId>>);
    static_assert(std::is_same_v<GenericTemplateConstraints, sema::SemanticAnalyzerCore::CapabilityMap>);
    static_assert(std::is_same_v<FunctionParamTypes, sema::TypeHandleList>);
    static_assert(std::is_same_v<FunctionGenericArgs, sema::TypeHandleList>);
    static_assert(std::is_same_v<StructFields, sema::SemaVector<sema::StructFieldInfo>>);
    static_assert(std::is_same_v<EnumPayloadTypes, sema::TypeHandleList>);
    static_assert(std::is_same_v<TypeTupleElements, sema::TypeHandleList>);
    static_assert(std::is_same_v<TypeFunctionParams, sema::TypeHandleList>);
    static_assert(std::is_same_v<TypeGenericArgs, sema::TypeHandleList>);
    static_assert(std::is_same_v<decltype(sema::CheckedModule{}.normalized_ast), sema::NormalizedAstOverlay>);
    static_assert(std::is_same_v<decltype(sema::NormalizedAstOverlay{}.original_expr_count), base::u64>);
    static_assert(std::is_same_v<decltype(sema::NormalizedAstOverlay{}.final_type_count), base::u64>);
    static_assert(sizeof(sema::NormalizedAstOverlay) < sizeof(syntax::AstModule));

    syntax::AstModule module;
    module.module_path = module_path({"parser_only"});

    syntax::TypeNode i32_type_node = primitive_node(syntax::PrimitiveTypeKind::i32);
    const TypeId i32_type = module.push_type(i32_type_node);

    const ExprId zero = push_integer_text(module, "0");

    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = zero;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode main_function;
    main_function.kind = syntax::ItemKind::fn_decl;
    main_function.name = "main";
    main_function.return_type = i32_type;
    main_function.body = body;
    static_cast<void>(module.push_item(main_function));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    EXPECT_GT(checked_result.value().arena_bytes(), 0U);
    EXPECT_GT(checked_result.value().arena_blocks(), 0U);
    EXPECT_TRUE(checked_result.value().normalized_ast.parser_only_module_contract_added);
    EXPECT_EQ(checked_result.value().normalized_ast.original_expr_count, 1U);
    EXPECT_EQ(checked_result.value().normalized_ast.final_expr_count, module.exprs.size());
    EXPECT_EQ(checked_result.value().normalized_ast.original_type_count, 1U);
    EXPECT_EQ(checked_result.value().normalized_ast.final_type_count, module.types.size());
    EXPECT_EQ(module.modules.size(), 1U);
    EXPECT_EQ(module.item_modules.size(), 1U);
    EXPECT_EQ(module.item_modules.front().value, 0U);
    EXPECT_EQ(module.item_part_indices.size(), 1U);
    EXPECT_EQ(module.item_part_indices.front(), 0U);

    syntax::AstModule discard_module;
    discard_module.modules = {module_info({"root"})};
    const TypeId discard_i32_type = discard_module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId discard_zero = push_integer(discard_module);
    syntax::StmtNode discard_return_stmt;
    discard_return_stmt.kind = syntax::StmtKind::return_;
    discard_return_stmt.return_value = discard_zero;
    const syntax::StmtId discard_return_stmt_id = discard_module.push_stmt(discard_return_stmt);
    const syntax::StmtId discard_body = push_block(discard_module, {discard_return_stmt_id});

    syntax::ItemNode discard_main_function;
    discard_main_function.kind = syntax::ItemKind::fn_decl;
    discard_main_function.name = "main";
    discard_main_function.return_type = discard_i32_type;
    discard_main_function.body = discard_body;
    const syntax::ItemId discard_main_item = discard_module.push_item(discard_main_function);
    discard_module.item_modules[discard_main_item.value] = module_id(0);

    base::DiagnosticSink snapshot_diagnostics;
    sema::SemanticAnalyzerCore snapshot_analyzer(std::move(discard_module), snapshot_diagnostics);
    auto snapshot_result = snapshot_analyzer.analyze();
    ASSERT_TRUE(snapshot_result) << snapshot_result.error().message;
    EXPECT_FALSE(snapshot_result.value().normalized_ast.parser_only_module_contract_added);
    EXPECT_EQ(snapshot_result.value().normalized_ast.original_expr_count, 1U);
    EXPECT_EQ(snapshot_result.value().normalized_ast.final_expr_count, 1U);
    EXPECT_EQ(snapshot_result.value().normalized_ast.original_type_count, 1U);
    EXPECT_EQ(snapshot_result.value().normalized_ast.final_type_count, 1U);
}
TEST(CoreUnit, SemanticWhiteBoxParserAstRequiresItemModulesWhenModulesExist)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    syntax::TypeNode i32_type_node = primitive_node(syntax::PrimitiveTypeKind::i32);
    const TypeId i32_type = module.push_type(i32_type_node);

    syntax::ItemNode main_function;
    main_function.kind = syntax::ItemKind::fn_decl;
    main_function.name = "main";
    main_function.return_type = i32_type;
    static_cast<void>(module.push_item(main_function));
    module.item_modules.clear();

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    ASSERT_TRUE(diagnostics.has_error());
    bool found = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        found = found
            || diagnostic.message.find("item_modules must contain one module owner per item") != std::string::npos;
    }
    EXPECT_TRUE(found);
}
TEST(CoreUnit, SemanticWhiteBoxItemImportScopeRangeContractIsValidated)
{
    constexpr base::u32 SEMA_TEST_PRIMARY_PART_INDEX = 0;
    constexpr base::u32 SEMA_TEST_TOO_WIDE_SCOPE_ITEM_COUNT = 2;
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "main";
    const syntax::ItemId item = module.push_item_for_module(function, module_id(0), SEMA_TEST_PRIMARY_PART_INDEX);

    syntax::ItemImportScope out_of_range_scope;
    out_of_range_scope.item_begin = item.value;
    out_of_range_scope.item_count = SEMA_TEST_TOO_WIDE_SCOPE_ITEM_COUNT;
    out_of_range_scope.part_index = SEMA_TEST_PRIMARY_PART_INDEX;
    module.item_import_scopes.push_back(std::move(out_of_range_scope));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    ASSERT_TRUE(diagnostics.has_error());
    constexpr std::string_view SEMA_TEST_EXPECTED_MESSAGE = sema::SEMA_AST_ITEM_MODULE_CONTRACT;
    bool found = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        found = found || diagnostic.message.find(SEMA_TEST_EXPECTED_MESSAGE) != std::string::npos;
    }
    EXPECT_TRUE(found);
}
TEST(CoreUnit, SemanticWhiteBoxItemImportScopeModuleContractIsValidated)
{
    constexpr base::u32 SEMA_TEST_PRIMARY_PART_INDEX = 0;
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"dep"}),
    };

    syntax::ItemNode root_function;
    root_function.kind = syntax::ItemKind::fn_decl;
    root_function.name = "root_fn";
    const syntax::ItemId root_item =
        module.push_item_for_module(root_function, module_id(0), SEMA_TEST_PRIMARY_PART_INDEX);

    syntax::ItemNode dep_function;
    dep_function.kind = syntax::ItemKind::fn_decl;
    dep_function.name = "dep_fn";
    static_cast<void>(module.push_item_for_module(dep_function, module_id(1), SEMA_TEST_PRIMARY_PART_INDEX));

    syntax::ItemImportScope cross_module_scope;
    cross_module_scope.item_begin = root_item.value;
    cross_module_scope.item_count = 2;
    cross_module_scope.part_index = SEMA_TEST_PRIMARY_PART_INDEX;
    module.item_import_scopes.push_back(std::move(cross_module_scope));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    ASSERT_TRUE(diagnostics.has_error());
    constexpr std::string_view SEMA_TEST_EXPECTED_MESSAGE = sema::SEMA_AST_ITEM_IMPORT_SCOPE_MODULE_INVALID;
    bool found = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        found = found || diagnostic.message.find(SEMA_TEST_EXPECTED_MESSAGE) != std::string::npos;
    }
    EXPECT_TRUE(found);
}
TEST(CoreUnit, SemanticWhiteBoxItemImportScopePartContractIsValidated)
{
    constexpr base::u32 SEMA_TEST_PRIMARY_PART_INDEX = 0;
    constexpr base::u32 SEMA_TEST_FRAGMENT_PART_INDEX = 1;
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    syntax::ItemNode primary_function;
    primary_function.kind = syntax::ItemKind::fn_decl;
    primary_function.name = "primary";
    const syntax::ItemId primary_item =
        module.push_item_for_module(primary_function, module_id(0), SEMA_TEST_PRIMARY_PART_INDEX);

    syntax::ItemNode part_function;
    part_function.kind = syntax::ItemKind::fn_decl;
    part_function.name = "from_part";
    static_cast<void>(module.push_item_for_module(part_function, module_id(0), SEMA_TEST_FRAGMENT_PART_INDEX));

    syntax::ItemImportScope cross_part_scope;
    cross_part_scope.item_begin = primary_item.value;
    cross_part_scope.item_count = 2;
    cross_part_scope.part_index = SEMA_TEST_PRIMARY_PART_INDEX;
    module.item_import_scopes.push_back(std::move(cross_part_scope));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    ASSERT_TRUE(diagnostics.has_error());
    constexpr std::string_view SEMA_TEST_EXPECTED_MESSAGE = sema::SEMA_AST_ITEM_IMPORT_SCOPE_PART_INVALID;
    bool found = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        found = found || diagnostic.message.find(SEMA_TEST_EXPECTED_MESSAGE) != std::string::npos;
    }
    EXPECT_TRUE(found);
}
TEST(CoreUnit, SemanticWhiteBoxItemImportScopePartKeyContractIsValidated)
{
    constexpr base::u32 SEMA_TEST_FRAGMENT_PART_INDEX = 1;
    constexpr base::usize SEMA_TEST_PART_KEY_TABLE_SIZE = SEMA_TEST_FRAGMENT_PART_INDEX + 1;
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    syntax::ItemNode part_function;
    part_function.kind = syntax::ItemKind::fn_decl;
    part_function.name = "from_part";
    const syntax::ItemId part_item =
        module.push_item_for_module(part_function, module_id(0), SEMA_TEST_FRAGMENT_PART_INDEX);

    syntax::ItemImportScope part_scope;
    part_scope.item_begin = part_item.value;
    part_scope.item_count = 1;
    part_scope.part_index = SEMA_TEST_FRAGMENT_PART_INDEX;
    module.item_import_scopes.push_back(std::move(part_scope));

    sema::SemanticOptions options;
    options.module_part_keys.resize(module.modules.size());
    options.module_part_keys[0].resize(SEMA_TEST_PART_KEY_TABLE_SIZE);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics, options);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    ASSERT_TRUE(diagnostics.has_error());
    constexpr std::string_view SEMA_TEST_EXPECTED_MESSAGE = sema::SEMA_AST_ITEM_IMPORT_SCOPE_PART_INVALID;
    bool found = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        found = found || diagnostic.message.find(SEMA_TEST_EXPECTED_MESSAGE) != std::string::npos;
    }
    EXPECT_TRUE(found);
}
} // namespace aurex::test
