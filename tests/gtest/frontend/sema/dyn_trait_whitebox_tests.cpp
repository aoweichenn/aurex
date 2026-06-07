#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <gtest/frontend/sema/sema_whitebox_test_support.hpp>
#include <support/test_support.hpp>

#include <frontend/sema/internal/traits/private/sema_trait_analyzer.hpp>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aurex::test {
namespace {

[[nodiscard]] syntax::AstModule parse_dyn_trait_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer({191}, source, diagnostics);
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

[[nodiscard]] sema::CheckedModule analyze_dyn_trait_source(const std::string_view source)
{
    syntax::AstModule module = parse_dyn_trait_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics);
    auto result = analyzer.analyze();
    if (!result) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        ADD_FAILURE() << result.error().message;
        return {};
    }
    return result.take_value();
}

[[nodiscard]] bool dyn_trait_diagnostics_contain(
    const base::DiagnosticSink& diagnostics, const std::string_view message)
{
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        if (diagnostic.message.find(message) != std::string::npos) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string analyze_dyn_trait_source_failure(const std::string_view source)
{
    syntax::AstModule module = parse_dyn_trait_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics);
    auto result = analyzer.analyze();

    std::string output;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        output += diagnostic.message;
        output += '\n';
    }
    if (result) {
        ADD_FAILURE() << "expected semantic analysis to fail";
        return output;
    }
    output += result.error().message;
    output += '\n';
    return output;
}

void expect_dyn_trait_source_diagnostic(const std::string_view source, const std::string_view diagnostic)
{
    const std::string output = analyze_dyn_trait_source_failure(source);
    EXPECT_NE(output.find(diagnostic), std::string::npos) << "missing diagnostic: " << diagnostic << "\n" << output;
}

[[nodiscard]] std::optional<sema::FunctionLookupKey> find_dyn_trait_function(
    const sema::CheckedModule& checked, const std::string_view name)
{
    for (const auto& entry : checked.functions) {
        if (entry.second.name.view() == name) {
            return entry.first;
        }
    }
    return std::nullopt;
}

void expect_trait_object_authority_matches_checked(
    const sema::CheckedModule& checked, const std::string_view function_name)
{
    const std::optional<sema::FunctionLookupKey> key = find_dyn_trait_function(checked, function_name);
    ASSERT_TRUE(key.has_value());
    query::TypeCheckBodyAuthority authority;
    authority.checked_body = query::query_result_fingerprint(query::stable_fingerprint("dyn.checked"));
    authority.body_syntax_result = query::query_result_fingerprint(query::stable_fingerprint("dyn.body"));
    authority.signature_result = query::query_result_fingerprint(query::stable_fingerprint("dyn.signature"));
    sema::populate_type_check_body_borrow_authority(authority, checked, *key);

    EXPECT_TRUE(authority.has_trait_object_facts);
    EXPECT_EQ(authority.trait_object_method_slot_count, checked.trait_object_method_slots.size());
    EXPECT_EQ(authority.trait_object_callability_count, checked.trait_object_callability.size());
    EXPECT_EQ(authority.vtable_layout_count, checked.vtable_layouts.size());
    EXPECT_EQ(authority.trait_object_coercion_count, checked.trait_object_coercions.size());
    EXPECT_EQ(authority.trait_object_fingerprint, sema::trait_object_facts_fingerprint(checked));
    EXPECT_NE(authority.trait_object_fingerprint.byte_count, 0U);
}

[[nodiscard]] query::TraitObjectTypeKey make_test_trait_object_key(
    const std::string_view trait_name, const std::span<const query::CanonicalTypeKey> trait_args = {})
{
    const query::PackageKey package = query::package_key(std::span<const std::string_view>{});
    const query::DefKey trait = query::def_key_from_stable_id(
        package,
        query::stable_definition_id(
            query::stable_module_id(std::span<const std::string_view>{std::array<std::string_view, 1>{"dyn_type"}}),
            query::StableSymbolKind::type,
            trait_name),
        query::DefNamespace::trait_,
        query::DefKind::trait_);
    const query::StableFingerprint128 origin = query::stable_fingerprint(std::string("origin:") + std::string(trait_name));
    const query::StableFingerprint128 schema = query::stable_fingerprint(std::string("schema:") + std::string(trait_name));
    return query::trait_object_type_key(trait, trait_args, std::span<const query::TraitObjectAssociatedTypeEqualityKey>{},
        origin, schema);
}

struct DynTraitWhiteBoxHarness {
    syntax::AstModule module;
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer;
    sema::SemanticAnalyzerCore::TraitAnalyzer traits;
    sema::TypeTable& types;
    sema::TypeHandle i32 = sema::INVALID_TYPE_HANDLE;
    sema::TypeHandle bool_type = sema::INVALID_TYPE_HANDLE;
    syntax::ModuleId module_id = syntax::ModuleId{0U};

    explicit DynTraitWhiteBoxHarness(const std::string_view module_name)
        : module(), diagnostics(), analyzer(this->module, this->diagnostics), traits(this->analyzer),
          types(this->analyzer.state_.checked.types)
    {
        this->module.modules = {module_info({module_name})};
        this->analyzer.state_.flow.current_module = this->module_id;
        this->i32 = this->types.builtin(sema::BuiltinType::i32);
        this->bool_type = this->types.builtin(sema::BuiltinType::bool_);
    }

    [[nodiscard]] sema::TraitSignature trait_signature(const std::string_view trait_name)
    {
        sema::TraitSignature trait = this->analyzer.state_.checked.make_trait_signature();
        trait.name = this->analyzer.state_.checked.intern_text(trait_name);
        trait.name_id = this->module.intern_identifier(trait_name);
        trait.module = this->module_id;
        trait.visibility = syntax::Visibility::public_;
        trait.stable_id = this->analyzer.stable_definition_id(
            this->module_id, sema::StableSymbolKind::type, trait.name_id, trait_name);
        return trait;
    }

    [[nodiscard]] sema::TraitAssociatedTypeRequirement associated_type(
        const sema::TraitSignature& trait, const std::string_view name, const base::u32 ordinal)
    {
        sema::TraitAssociatedTypeRequirement associated =
            this->analyzer.state_.checked.make_trait_associated_type_requirement();
        associated.name = this->analyzer.state_.checked.intern_text(name);
        associated.name_id = this->module.intern_identifier(name);
        associated.module = this->module_id;
        associated.member_key = query::member_key(
            this->traits.trait_query_key(trait), query::MemberKind::associated_type, name, ordinal);
        associated.ordinal = ordinal;
        return associated;
    }

    [[nodiscard]] sema::TraitImplAssociatedTypeInfo equality(
        const sema::TraitAssociatedTypeRequirement& associated, const sema::TypeHandle value_type)
    {
        sema::TraitImplAssociatedTypeInfo equality = this->analyzer.state_.checked.make_trait_impl_associated_type_info();
        equality.name = associated.name;
        equality.name_id = associated.name_id;
        equality.value_type = value_type;
        equality.member_key = associated.member_key;
        equality.requirement_ordinal = associated.ordinal;
        return equality;
    }

    [[nodiscard]] sema::TraitMethodRequirement method(
        const std::string_view name, const sema::TypeHandle receiver_type, const sema::TypeHandle return_type,
        const base::u32 ordinal)
    {
        sema::TraitMethodRequirement requirement = this->analyzer.state_.checked.make_trait_method_requirement();
        requirement.name = this->analyzer.state_.checked.intern_text(name);
        requirement.name_id = this->module.intern_identifier(name);
        requirement.module = this->module_id;
        requirement.param_types.push_back(receiver_type);
        requirement.return_type = return_type;
        requirement.has_self_param = true;
        requirement.ordinal = ordinal;
        return requirement;
    }

    [[nodiscard]] sema::TraitObjectAssociatedTypeEquality object_equality(
        const sema::TraitAssociatedTypeRequirement& associated, const sema::TypeHandle value_type)
    {
        sema::TraitObjectAssociatedTypeEquality equality;
        equality.associated_member = associated.member_key;
        equality.name = associated.name;
        equality.value_type = value_type;
        return equality;
    }

    [[nodiscard]] sema::TypeHandle trait_object(const sema::TraitSignature& trait,
        const std::span<const sema::TypeHandle> trait_args = {},
        const std::span<const sema::TraitObjectAssociatedTypeEquality> associated_equalities = {})
    {
        std::vector<query::CanonicalTypeKey> canonical_args;
        canonical_args.reserve(trait_args.size());
        for (const sema::TypeHandle arg : trait_args) {
            base::Result<query::CanonicalTypeKey> key = this->analyzer.checked_canonical_type_key(arg);
            if (!key) {
                ADD_FAILURE() << key.error().message;
                return sema::INVALID_TYPE_HANDLE;
            }
            canonical_args.push_back(key.take_value());
        }

        std::vector<query::TraitObjectAssociatedTypeEqualityKey> equality_keys;
        equality_keys.reserve(associated_equalities.size());
        for (const sema::TraitObjectAssociatedTypeEquality& equality : associated_equalities) {
            base::Result<query::CanonicalTypeKey> value_key =
                this->analyzer.checked_canonical_type_key(equality.value_type);
            if (!value_key) {
                ADD_FAILURE() << value_key.error().message;
                return sema::INVALID_TYPE_HANDLE;
            }
            equality_keys.push_back(query::TraitObjectAssociatedTypeEqualityKey{
                equality.associated_member,
                value_key.take_value(),
            });
        }

        const query::StableFingerprint128 slot_schema =
            query::stable_fingerprint(std::string("whitebox-slot-schema:") + std::string(trait.name.view()));
        const query::StableFingerprint128 origin =
            query::stable_fingerprint(std::string("whitebox-origin:") + std::string(trait.name.view()));
        const query::TraitObjectTypeKey key =
            query::trait_object_type_key(this->traits.trait_query_key(trait), canonical_args, equality_keys, origin,
                slot_schema);
        return this->types.trait_object(
            key, trait.name.view(), trait.module, trait.name_id, trait_args, associated_equalities);
    }

    [[nodiscard]] sema::TraitImplInfo impl_info(
        const sema::TraitSignature& trait, const sema::TypeHandle self_type)
    {
        sema::TraitImplInfo impl = this->analyzer.state_.checked.make_trait_impl_info();
        impl.trait_name = trait.name;
        impl.trait_name_id = trait.name_id;
        impl.trait_module = trait.module;
        impl.self_type = self_type;
        impl.key = sema::TraitImplLookupKey{trait.module.value, trait.name_id, self_type.value, {}};
        impl.coherence_fingerprint =
            query::stable_fingerprint(std::string("whitebox-impl:") + std::string(trait.name.view()));
        return impl;
    }

    void add_impl_method(sema::TraitImplInfo& impl,
        const sema::TraitMethodRequirement& requirement,
        const std::string_view c_name,
        const sema::TraitImplMethodOrigin origin = sema::TraitImplMethodOrigin::impl_override)
    {
        const sema::FunctionLookupKey key{this->module_id.value, impl.self_type.value, requirement.name_id};

        sema::FunctionSignature signature = this->analyzer.state_.checked.make_function_signature();
        signature.name = this->analyzer.state_.checked.intern_text(requirement.name);
        signature.name_id = requirement.name_id;
        signature.semantic_key = key;
        signature.c_name = this->analyzer.state_.checked.intern_text(c_name);
        signature.module = this->module_id;
        signature.method_owner_type = impl.self_type;
        signature.trait_module = impl.trait_module;
        signature.trait_name_id = impl.trait_name_id;
        signature.return_type = requirement.return_type;
        signature.param_types = this->analyzer.state_.checked.make_type_handle_list();
        signature.param_types.reserve(requirement.param_types.size());
        for (base::usize index = 0; index < requirement.param_types.size(); ++index) {
            sema::TypeHandle param = requirement.param_types[index];
            if (index == 0 && this->types.is_reference(param)) {
                const sema::TypeInfo& reference = this->types.get(param);
                if (sema::is_valid(reference.pointee)
                    && this->types.get(reference.pointee).kind == sema::TypeKind::generic_param) {
                    param = this->types.reference(reference.pointer_mutability, impl.self_type);
                }
            }
            signature.param_types.push_back(param);
        }
        signature.is_method = true;
        signature.has_self_param = requirement.has_self_param;
        signature.is_trait_impl_method = true;
        signature.has_definition = true;
        this->analyzer.state_.checked.functions.emplace(key, std::move(signature));

        sema::TraitImplMethodInfo method = this->analyzer.state_.checked.make_trait_impl_method_info();
        method.name = this->analyzer.state_.checked.intern_text(requirement.name);
        method.name_id = requirement.name_id;
        method.function_key = key;
        method.requirement_ordinal = requirement.ordinal;
        method.origin = origin;
        impl.methods.push_back(method);
    }

    [[nodiscard]] sema::TypeHandle struct_type(const std::string_view name)
    {
        const std::string display = std::string(this->module.modules.front().path.parts.front()) + "."
            + std::string(name);
        const std::string c_name = std::string(this->module.modules.front().path.parts.front()) + "_"
            + std::string(name);
        const sema::TypeHandle type = this->types.named_struct(display, c_name, false);
        sema::StructInfo info = this->analyzer.state_.checked.make_struct_info();
        info.name = this->analyzer.state_.checked.intern_text(name);
        info.name_id = this->module.intern_identifier(name);
        info.c_name = this->analyzer.state_.checked.intern_text(c_name);
        info.module = this->module_id;
        info.type = type;
        info.visibility = syntax::Visibility::public_;
        info.stable_id =
            this->analyzer.stable_definition_id(this->module_id, sema::StableSymbolKind::type, info.name_id, name);
        const sema::ModuleLookupKey key = this->analyzer.module_lookup_key(this->module_id, info.name_id);
        const auto inserted = this->analyzer.state_.checked.structs.emplace(key, info);
        this->analyzer.state_.types.struct_infos_by_type[type.value] = &inserted.first->second;
        return type;
    }
};

} // namespace

TEST(CoreUnit, DynTraitWhiteBoxCallabilityRejectsAndRecordsBoundaryFacts)
{
    DynTraitWhiteBoxHarness harness("dyn_trait_callability_boundary");
    sema::TraitSignature trait = harness.trait_signature("Source");
    const sema::TraitAssociatedTypeRequirement item = harness.associated_type(trait, "Item", 0U);
    const sema::TraitAssociatedTypeRequirement error = harness.associated_type(trait, "Error", 1U);
    trait.associated_types.push_back(item);
    trait.associated_types.push_back(error);
    trait.requirements.push_back(harness.method("item", harness.types.reference(sema::PointerMutability::const_,
        harness.types.generic_param(sema::generic_param_identity_from_text("Self"), "Self")), harness.i32, 0U));

    const sema::TraitImplAssociatedTypeInfo item_equality = harness.equality(item, harness.i32);
    const std::array<sema::TraitImplAssociatedTypeInfo, 1> partial_equalities{item_equality};
    EXPECT_FALSE(harness.traits.validate_trait_object_callability(
        trait, sema::INVALID_TYPE_HANDLE, {}, partial_equalities, {}, false));
    EXPECT_FALSE(harness.diagnostics.has_error());
    EXPECT_FALSE(harness.traits.validate_trait_object_callability(
        trait, sema::INVALID_TYPE_HANDLE, {}, partial_equalities, {}, true));
    EXPECT_TRUE(harness.diagnostics.has_error());
    EXPECT_TRUE(dyn_trait_diagnostics_contain(
        harness.diagnostics, "dyn trait `Source` requires associated type equality `Error = ...`"));

    const sema::TraitImplAssociatedTypeInfo error_equality = harness.equality(error, harness.bool_type);
    const std::array<sema::TraitImplAssociatedTypeInfo, 2> complete_equalities{item_equality, error_equality};
    const std::array<sema::TraitObjectAssociatedTypeEquality, 2> object_equalities{
        harness.object_equality(item, harness.i32),
        harness.object_equality(error, harness.bool_type),
    };
    const sema::TypeHandle object = harness.trait_object(trait, {}, object_equalities);
    ASSERT_TRUE(sema::is_valid(object));

    harness.traits.record_trait_object_callability(object, trait, {}, complete_equalities, {});
    ASSERT_EQ(harness.analyzer.state_.checked.trait_object_callability.size(), 1U);
    ASSERT_EQ(harness.analyzer.state_.checked.trait_object_method_slots.size(), 1U);
    EXPECT_EQ(harness.analyzer.state_.checked.trait_object_callability.front().method_slot_count, 1U);
    EXPECT_EQ(harness.analyzer.state_.checked.trait_object_method_slots.front().method_name, "item");

    harness.traits.record_trait_object_callability(object, trait, {}, complete_equalities, {});
    EXPECT_EQ(harness.analyzer.state_.checked.trait_object_callability.size(), 1U);
    EXPECT_EQ(harness.analyzer.state_.checked.trait_object_method_slots.size(), 1U);

    harness.traits.record_trait_object_callability(sema::INVALID_TYPE_HANDLE, trait, {}, complete_equalities, {});
    harness.traits.record_trait_object_callability(harness.i32, trait, {}, complete_equalities, {});
    EXPECT_EQ(harness.analyzer.state_.checked.trait_object_callability.size(), 1U);
}

TEST(CoreUnit, DynTraitWhiteBoxImplLookupAndVtableLayoutHandleBoundaries)
{
    DynTraitWhiteBoxHarness harness("dyn_trait_vtable_boundary");
    sema::TraitSignature trait = harness.trait_signature("Draw");
    const sema::TypeHandle self = harness.types.generic_param(sema::generic_param_identity_from_text("Self"), "Self");
    trait.requirements.push_back(
        harness.method("draw", harness.types.reference(sema::PointerMutability::const_, self), harness.i32, 0U));
    const sema::TypeHandle object = harness.trait_object(trait);
    ASSERT_TRUE(sema::is_valid(object));
    harness.traits.record_trait_object_callability(object, trait, {}, {}, {});
    harness.analyzer.state_.checked.traits.emplace(
        sema::ModuleLookupKey{harness.module_id.value, trait.name_id}, trait);

    const sema::TypeHandle file = harness.struct_type("File");
    const sema::TypeInfo& object_info = harness.types.get(object);
    EXPECT_EQ(harness.traits.find_trait_object_impl(sema::INVALID_TYPE_HANDLE, object_info, {}, false), nullptr);
    EXPECT_EQ(harness.traits.find_trait_object_impl(file, harness.types.get(harness.i32), {}, false), nullptr);
    EXPECT_EQ(harness.traits.find_trait_object_impl(file, object_info, {}, false), nullptr);

    sema::TraitImplInfo impl = harness.impl_info(trait, file);
    harness.add_impl_method(impl, trait.requirements.front(), "dyn_trait_vtable_boundary_File_draw");
    harness.analyzer.state_.checked.trait_impls.emplace(impl.key, impl);
    const sema::TraitImplInfo* const found = harness.traits.find_trait_object_impl(file, object_info, {}, true);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->trait_name, "Draw");

    const query::VTableLayoutKey first_layout = harness.traits.record_vtable_layout(file, object, {});
    EXPECT_TRUE(query::is_valid(first_layout));
    ASSERT_EQ(harness.analyzer.state_.checked.vtable_layouts.size(), 1U);
    EXPECT_EQ(harness.analyzer.state_.checked.vtable_layouts.front().method_slot_count, 1U);

    const query::VTableLayoutKey second_layout = harness.traits.record_vtable_layout(file, object, {});
    EXPECT_EQ(first_layout, second_layout);
    EXPECT_EQ(harness.analyzer.state_.checked.vtable_layouts.size(), 1U);

    EXPECT_FALSE(query::is_valid(harness.traits.record_vtable_layout(file, sema::INVALID_TYPE_HANDLE, {})));
    EXPECT_FALSE(query::is_valid(harness.traits.record_vtable_layout(file, harness.i32, {})));
}

TEST(CoreUnit, DynTraitWhiteBoxBorrowedCoercionDeduplicatesSameExpressionFact)
{
    DynTraitWhiteBoxHarness harness("dyn_trait_coercion_same_expr_boundary");
    sema::TraitSignature trait = harness.trait_signature("Draw");
    const sema::TypeHandle self = harness.types.generic_param(sema::generic_param_identity_from_text("Self"), "Self");
    trait.requirements.push_back(
        harness.method("draw", harness.types.reference(sema::PointerMutability::const_, self), harness.i32, 0U));
    const sema::TypeHandle object = harness.trait_object(trait);
    ASSERT_TRUE(sema::is_valid(object));
    harness.traits.record_trait_object_callability(object, trait, {}, {}, {});
    harness.analyzer.state_.checked.traits.emplace(
        sema::ModuleLookupKey{harness.module_id.value, trait.name_id}, trait);

    const sema::TypeHandle file = harness.struct_type("File");
    sema::TraitImplInfo impl = harness.impl_info(trait, file);
    harness.add_impl_method(impl, trait.requirements.front(), "dyn_trait_coercion_same_expr_boundary_File_draw");
    harness.analyzer.state_.checked.trait_impls.emplace(impl.key, impl);
    const sema::TypeHandle file_ref = harness.types.reference(sema::PointerMutability::const_, file);
    const sema::TypeHandle object_ref = harness.types.reference(sema::PointerMutability::const_, object);
    const sema::TypeHandle invalid_ref =
        harness.types.reference(sema::PointerMutability::const_, sema::TypeHandle{999999U});

    EXPECT_TRUE(harness.analyzer.can_borrowed_dyn_trait_coerce(object_ref, file_ref));
    EXPECT_TRUE(harness.analyzer.can_assign(object_ref, file_ref, syntax::INVALID_EXPR_ID));
    EXPECT_FALSE(harness.analyzer.can_borrowed_dyn_trait_coerce(file_ref, object_ref));
    EXPECT_FALSE(harness.analyzer.can_borrowed_dyn_trait_coerce(object_ref, invalid_ref));
    EXPECT_FALSE(harness.analyzer.can_borrowed_dyn_trait_coerce(invalid_ref, file_ref));
    harness.analyzer.record_borrowed_dyn_trait_coercion_if_needed(
        syntax::INVALID_EXPR_ID, file_ref, object_ref, {});
    EXPECT_TRUE(harness.analyzer.state_.checked.trait_object_coercions.empty());

    const syntax::ExprId expr{41U};
    harness.analyzer.record_borrowed_dyn_trait_coercion_if_needed(expr, file_ref, object_ref, {});
    ASSERT_EQ(harness.analyzer.state_.checked.trait_object_coercions.size(), 1U);
    ASSERT_EQ(harness.analyzer.state_.checked.vtable_layouts.size(), 1U);
    EXPECT_EQ(harness.analyzer.state_.checked.coercions.size(), 1U);
    EXPECT_EQ(harness.analyzer.state_.checked.coercions.front().kind, sema::CoercionKind::borrowed_dyn_trait);

    const query::TraitObjectCoercionKey first_key =
        harness.analyzer.state_.checked.trait_object_coercions.front().coercion_key;
    const query::VTableLayoutKey first_layout =
        harness.analyzer.state_.checked.trait_object_coercions.front().vtable_layout;
    harness.analyzer.record_borrowed_dyn_trait_coercion_if_needed(expr, file_ref, object_ref, {});
    ASSERT_EQ(harness.analyzer.state_.checked.trait_object_coercions.size(), 1U);
    EXPECT_EQ(harness.analyzer.state_.checked.trait_object_coercions.front().coercion_key, first_key);
    EXPECT_EQ(harness.analyzer.state_.checked.trait_object_coercions.front().vtable_layout, first_layout);
    EXPECT_EQ(harness.analyzer.state_.checked.coercions.size(), 2U);
    EXPECT_EQ(harness.analyzer.state_.checked.coercions.back().expr.value, expr.value);
    EXPECT_EQ(harness.analyzer.state_.checked.coercions.back().kind, sema::CoercionKind::borrowed_dyn_trait);
}

TEST(CoreUnit, DynTraitWhiteBoxImplLookupReportsAmbiguousObjectImpls)
{
    DynTraitWhiteBoxHarness harness("dyn_trait_ambiguous_impl_boundary");
    sema::TraitSignature trait = harness.trait_signature("Draw");
    const sema::TypeHandle self = harness.types.generic_param(sema::generic_param_identity_from_text("Self"), "Self");
    trait.requirements.push_back(
        harness.method("draw", harness.types.reference(sema::PointerMutability::const_, self), harness.i32, 0U));
    const sema::TypeHandle object = harness.trait_object(trait);
    ASSERT_TRUE(sema::is_valid(object));
    const sema::TypeHandle file = harness.struct_type("File");

    sema::TraitImplInfo primary_impl = harness.impl_info(trait, file);
    sema::TraitImplInfo overlapping_impl = primary_impl;
    overlapping_impl.key.trait_args = query::stable_fingerprint("whitebox-overlapping-dyn-trait-impl");
    overlapping_impl.coherence_fingerprint = query::stable_fingerprint("whitebox-overlapping-dyn-trait-evidence");
    harness.analyzer.state_.checked.trait_impls.emplace(primary_impl.key, primary_impl);
    harness.analyzer.state_.checked.trait_impls.emplace(overlapping_impl.key, overlapping_impl);
    harness.analyzer.state_.checked.traits.emplace(
        sema::ModuleLookupKey{harness.module_id.value, trait.name_id}, std::move(trait));

    const sema::TypeInfo& object_info = harness.types.get(object);
    EXPECT_EQ(harness.traits.find_trait_object_impl(file, object_info, {}, false), nullptr);
    EXPECT_FALSE(harness.diagnostics.has_error());

    EXPECT_EQ(harness.traits.find_trait_object_impl(file, object_info, {}, true), nullptr);
    EXPECT_TRUE(harness.diagnostics.has_error());
    EXPECT_TRUE(dyn_trait_diagnostics_contain(
        harness.diagnostics, "ambiguous trait method `Draw` for type dyn_trait_ambiguous_impl_boundary.File"));
}

TEST(CoreUnit, DynTraitWhiteBoxDynMethodResolutionReportsMissingSlotFact)
{
    DynTraitWhiteBoxHarness harness("dyn_trait_missing_slot_boundary");
    sema::TraitSignature trait = harness.trait_signature("Draw");
    const sema::TypeHandle self = harness.types.generic_param(sema::generic_param_identity_from_text("Self"), "Self");
    trait.requirements.push_back(
        harness.method("draw", harness.types.reference(sema::PointerMutability::const_, self), harness.i32, 0U));
    const sema::TypeHandle object = harness.trait_object(trait);
    ASSERT_TRUE(sema::is_valid(object));
    const sema::IdentId draw_name_id = trait.requirements.front().name_id;
    harness.analyzer.state_.checked.traits.emplace(
        sema::ModuleLookupKey{harness.module_id.value, trait.name_id}, std::move(trait));

    const sema::SemanticAnalyzerCore::TraitMethodCallResolution no_report =
        harness.traits.resolve_dyn_trait_method_call(object, draw_name_id, "draw", {}, false);
    EXPECT_FALSE(no_report.found);
    EXPECT_FALSE(no_report.reported_failure);
    EXPECT_FALSE(harness.diagnostics.has_error());

    const sema::SemanticAnalyzerCore::TraitMethodCallResolution reported =
        harness.traits.resolve_dyn_trait_method_call(object, draw_name_id, "draw", {}, true);
    EXPECT_FALSE(reported.found);
    EXPECT_TRUE(reported.reported_failure);
    EXPECT_TRUE(harness.diagnostics.has_error());
    EXPECT_TRUE(dyn_trait_diagnostics_contain(
        harness.diagnostics, "dyn trait `Draw` method `draw` receiver must be &Self or &mut Self"));
}

TEST(CoreUnit, DynTraitWhiteBoxObjectSafetyRejectsNestedSelfCarriers)
{
    DynTraitWhiteBoxHarness harness("dyn_trait_nested_self_boundary");
    sema::TraitSignature trait = harness.trait_signature("Bad");
    const sema::TypeHandle self = harness.types.generic_param(sema::generic_param_identity_from_text("Self"), "Self");
    const sema::TypeHandle self_ref = harness.types.reference(sema::PointerMutability::const_, self);
    const sema::TraitAssociatedTypeRequirement item = harness.associated_type(trait, "Item", 0U);

    const sema::TypeHandle projected_item = harness.types.associated_projection(self, item.member_key, "Item");
    trait.requirements.push_back(harness.method("projected", self_ref, projected_item, 0U));

    const sema::TypeHandle wrapper = harness.types.named_struct("Wrapper", "Wrapper", false);
    harness.types.set_generic_instance(wrapper, "Wrapper", std::array<sema::TypeHandle, 1>{self});
    trait.requirements.push_back(harness.method("wrapped", self_ref, wrapper, 1U));

    const sema::TypeHandle tagged = harness.types.named_enum("Tagged", "Tagged");
    harness.types.set_generic_instance(tagged, "Tagged", std::array<sema::TypeHandle, 1>{self});
    trait.requirements.push_back(harness.method("tagged", self_ref, tagged, 2U));

    const sema::TypeHandle opaque = harness.types.opaque_struct("Opaque", "Opaque");
    harness.types.set_generic_instance(opaque, "Opaque", std::array<sema::TypeHandle, 1>{self});
    trait.requirements.push_back(harness.method("opaque", self_ref, opaque, 3U));

    sema::TraitSignature inner = harness.trait_signature("Inner");
    inner.generic_params.push_back(harness.module.intern_identifier("T"));
    const query::TraitObjectTypeKey self_arg_object_key = query::trait_object_type_key(harness.traits.trait_query_key(inner),
        std::span<const query::CanonicalTypeKey>{}, std::span<const query::TraitObjectAssociatedTypeEqualityKey>{},
        query::stable_fingerprint("whitebox-inner-self-arg-origin"),
        query::stable_fingerprint("whitebox-inner-self-arg-schema"));
    const sema::TypeHandle dyn_with_self_arg = harness.types.trait_object(self_arg_object_key, inner.name.view(),
        inner.module, inner.name_id, std::array<sema::TypeHandle, 1>{self},
        std::span<const sema::TraitObjectAssociatedTypeEquality>{});
    ASSERT_TRUE(sema::is_valid(dyn_with_self_arg));
    trait.requirements.push_back(harness.method("object_arg", self_ref, dyn_with_self_arg, 4U));

    const sema::TraitAssociatedTypeRequirement inner_item = harness.associated_type(inner, "Assoc", 0U);
    const sema::TraitObjectAssociatedTypeEquality dyn_self_equality = harness.object_equality(inner_item, self);
    const query::TraitObjectTypeKey self_equality_object_key = query::trait_object_type_key(
        harness.traits.trait_query_key(inner), std::span<const query::CanonicalTypeKey>{},
        std::span<const query::TraitObjectAssociatedTypeEqualityKey>{},
        query::stable_fingerprint("whitebox-inner-self-equality-origin"),
        query::stable_fingerprint("whitebox-inner-self-equality-schema"));
    const sema::TypeHandle dyn_with_self_equality = harness.types.trait_object(self_equality_object_key,
        inner.name.view(), inner.module, inner.name_id, std::span<const sema::TypeHandle>{},
        std::array<sema::TraitObjectAssociatedTypeEquality, 1>{dyn_self_equality});
    ASSERT_TRUE(sema::is_valid(dyn_with_self_equality));
    trait.requirements.push_back(harness.method("object_equality", self_ref, dyn_with_self_equality, 5U));

    EXPECT_NE(harness.traits.trait_object_slot_schema(trait, {}, {}), query::StableFingerprint128{});
    EXPECT_FALSE(harness.traits.validate_trait_object_callability(trait, sema::INVALID_TYPE_HANDLE, {}, {}, {}, false));
    EXPECT_FALSE(harness.diagnostics.has_error());
    EXPECT_FALSE(harness.traits.validate_trait_object_callability(trait, sema::INVALID_TYPE_HANDLE, {}, {}, {}, true));
    EXPECT_TRUE(harness.diagnostics.has_error());
    EXPECT_TRUE(dyn_trait_diagnostics_contain(
        harness.diagnostics, "dyn trait `Bad` method `projected` can only use Self in the receiver"));
    EXPECT_TRUE(dyn_trait_diagnostics_contain(
        harness.diagnostics, "dyn trait `Bad` method `wrapped` can only use Self in the receiver"));
    EXPECT_TRUE(dyn_trait_diagnostics_contain(
        harness.diagnostics, "dyn trait `Bad` method `tagged` can only use Self in the receiver"));
    EXPECT_TRUE(dyn_trait_diagnostics_contain(
        harness.diagnostics, "dyn trait `Bad` method `opaque` can only use Self in the receiver"));
    EXPECT_TRUE(dyn_trait_diagnostics_contain(
        harness.diagnostics, "dyn trait `Bad` method `object_arg` can only use Self in the receiver"));
    EXPECT_TRUE(dyn_trait_diagnostics_contain(
        harness.diagnostics, "dyn trait `Bad` method `object_equality` can only use Self in the receiver"));
}

TEST(CoreUnit, DynTraitWhiteBoxRejectsNonMatchingImplAndResolutionInputs)
{
    DynTraitWhiteBoxHarness harness("dyn_trait_impl_match_boundary");
    sema::TraitSignature trait = harness.trait_signature("Reader");
    trait.generic_params.push_back(harness.module.intern_identifier("T"));
    const sema::TraitAssociatedTypeRequirement item = harness.associated_type(trait, "Item", 0U);
    trait.associated_types.push_back(item);
    const sema::TypeHandle self = harness.types.generic_param(sema::generic_param_identity_from_text("Self"), "Self");
    trait.requirements.push_back(
        harness.method("read", harness.types.reference(sema::PointerMutability::const_, self), harness.i32, 0U));

    const std::array<sema::TypeHandle, 1> object_args{harness.i32};
    const sema::TraitObjectAssociatedTypeEquality item_equals_i32 = harness.object_equality(item, harness.i32);
    const sema::TypeHandle object =
        harness.trait_object(trait, object_args, std::array<sema::TraitObjectAssociatedTypeEquality, 1>{item_equals_i32});
    ASSERT_TRUE(sema::is_valid(object));
    EXPECT_TRUE(harness.analyzer.validate_trait_object_callability(trait, object, object_args,
        std::array<sema::TraitImplAssociatedTypeInfo, 1>{harness.equality(item, harness.i32)}, {}, false));

    const sema::TypeHandle file = harness.struct_type("File");
    const sema::TypeHandle buffer = harness.struct_type("Buffer");
    sema::TraitImplInfo wrong_module = harness.impl_info(trait, file);
    wrong_module.trait_module = syntax::ModuleId{1U};
    wrong_module.key =
        sema::TraitImplLookupKey{wrong_module.trait_module.value, wrong_module.trait_name_id, file.value,
            query::stable_fingerprint("wrong-module")};

    sema::TraitImplInfo wrong_name = harness.impl_info(trait, file);
    wrong_name.trait_name_id = harness.module.intern_identifier("Source");
    wrong_name.key = sema::TraitImplLookupKey{harness.module_id.value, wrong_name.trait_name_id, file.value,
        query::stable_fingerprint("wrong-name")};

    sema::TraitImplInfo wrong_self = harness.impl_info(trait, buffer);
    wrong_self.trait_args.push_back(harness.i32);
    wrong_self.associated_types.push_back(harness.equality(item, harness.i32));
    wrong_self.key.trait_args = query::stable_fingerprint("wrong-self");

    sema::TraitImplInfo wrong_arg_count = harness.impl_info(trait, file);
    wrong_arg_count.associated_types.push_back(harness.equality(item, harness.i32));
    wrong_arg_count.key.trait_args = query::stable_fingerprint("wrong-arg-count");

    sema::TraitImplInfo wrong_arg_value = harness.impl_info(trait, file);
    wrong_arg_value.trait_args.push_back(harness.bool_type);
    wrong_arg_value.associated_types.push_back(harness.equality(item, harness.i32));
    wrong_arg_value.key.trait_args = query::stable_fingerprint("wrong-arg-value");

    sema::TraitImplInfo wrong_associated_value = harness.impl_info(trait, file);
    wrong_associated_value.trait_args.push_back(harness.i32);
    wrong_associated_value.associated_types.push_back(harness.equality(item, harness.bool_type));
    wrong_associated_value.key.trait_args = query::stable_fingerprint("wrong-associated-value");

    sema::TraitImplInfo matching = harness.impl_info(trait, file);
    matching.trait_args.push_back(harness.i32);
    matching.associated_types.push_back(harness.equality(item, harness.i32));
    matching.key.trait_args = query::stable_fingerprint("matching-reader");

    harness.analyzer.state_.checked.trait_impls.emplace(wrong_module.key, wrong_module);
    harness.analyzer.state_.checked.trait_impls.emplace(wrong_name.key, wrong_name);
    harness.analyzer.state_.checked.trait_impls.emplace(wrong_self.key, wrong_self);
    harness.analyzer.state_.checked.trait_impls.emplace(wrong_arg_count.key, wrong_arg_count);
    harness.analyzer.state_.checked.trait_impls.emplace(wrong_arg_value.key, wrong_arg_value);
    harness.analyzer.state_.checked.trait_impls.emplace(wrong_associated_value.key, wrong_associated_value);
    harness.analyzer.state_.checked.trait_impls.emplace(matching.key, matching);

    const sema::TypeInfo& object_info = harness.types.get(object);
    const sema::TraitImplInfo* const found = harness.traits.find_trait_object_impl(file, object_info, {}, true);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->key.trait_args, matching.key.trait_args);
    EXPECT_FALSE(harness.diagnostics.has_error());

    const sema::IdentId read_name_id = trait.requirements.front().name_id;
    EXPECT_FALSE(harness.traits.resolve_dyn_trait_method_call(
        sema::INVALID_TYPE_HANDLE, read_name_id, "read", {}, false).found);
    EXPECT_FALSE(harness.traits.resolve_dyn_trait_method_call(harness.i32, read_name_id, "read", {}, false).found);
    EXPECT_FALSE(harness.traits.resolve_dyn_trait_method_call(object, read_name_id, "read", {}, false).found);
}

TEST(CoreUnit, DynTraitTypeTableInternsCopiesMovesAndDisplaysObjects)
{
    sema::TypeTable types;
    const sema::TypeHandle i32 = types.builtin(sema::BuiltinType::i32);
    const sema::TypeHandle bool_type = types.builtin(sema::BuiltinType::bool_);
    const std::array<query::CanonicalTypeKey, 2> args{
        query::canonical_builtin(query::BuiltinTypeKey::i32),
        query::canonical_builtin(query::BuiltinTypeKey::bool_),
    };
    const query::TraitObjectTypeKey key = make_test_trait_object_key("Mapper", args);
    ASSERT_TRUE(query::is_valid(key));

    const sema::TypeHandle object = types.trait_object(
        key, "Mapper", syntax::ModuleId{0U}, sema::IdentId{7U}, std::array<sema::TypeHandle, 2>{i32, bool_type}, {});
    ASSERT_TRUE(sema::is_valid(object));
    const sema::TypeHandle duplicate = types.trait_object(
        key, "Mapper", syntax::ModuleId{0U}, sema::IdentId{7U}, std::array<sema::TypeHandle, 2>{i32, bool_type}, {});
    EXPECT_EQ(object.value, duplicate.value);
    EXPECT_TRUE(types.is_trait_object(object));
    EXPECT_FALSE(types.is_trait_object(i32));
    EXPECT_FALSE(types.is_trait_object(sema::TypeHandle{static_cast<base::u32>(types.size())}));
    EXPECT_EQ(types.display_name(object), "dyn Mapper[i32,bool]");

    const sema::TypeTable copied = types;
    EXPECT_TRUE(copied.is_trait_object(object));
    EXPECT_EQ(copied.display_name(object), "dyn Mapper[i32,bool]");

    sema::TypeTable moved = copied;
    moved = std::move(types);
    EXPECT_TRUE(moved.is_trait_object(object));
    EXPECT_EQ(moved.display_name(object), "dyn Mapper[i32,bool]");

    sema::TypeTable invalid_key_types;
    EXPECT_FALSE(sema::is_valid(invalid_key_types.trait_object(
        query::TraitObjectTypeKey{}, "Missing", syntax::ModuleId{0U}, sema::IdentId{9U}, {}, {})));

    sema::TypeTable corrupted = moved;
    ASSERT_LT(object.value, TypeTableTestAccess::entries(corrupted).size());
    TypeTableTestAccess::entries(corrupted)[object.value].kind = sema::TypeKind::builtin;
    EXPECT_FALSE(corrupted.is_trait_object(object));
    TypeTableTestAccess::entries(corrupted)[object.value].kind = static_cast<sema::TypeKind>(255);
    EXPECT_EQ(corrupted.display_name(object), "<unknown>");
}

TEST(CoreUnit, DynTraitBorrowedSharedViewRecordsCallabilityCoercionAndVtableFacts)
{
    const std::string_view source =
        "module dyn_trait_shared_view_whitebox;\n"
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
        "  let file: File = File { value: 7 };\n"
        "  let drawable: &dyn Draw = &file;\n"
        "  return render(drawable);\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_object_callability.size(), 1U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_callability.front().object_type), "dyn Draw");
    EXPECT_TRUE(checked.types.is_trait_object(checked.trait_object_callability.front().object_type));
    EXPECT_FALSE(checked.types.is_trait_object(checked.trait_object_method_slots.front().receiver_type));
    EXPECT_FALSE(checked.types.is_trait_object(sema::INVALID_TYPE_HANDLE));
    EXPECT_EQ(checked.trait_object_callability.front().method_slot_count, 1U);
    EXPECT_TRUE(query::is_valid(checked.trait_object_callability.front().object_type_key));

    ASSERT_EQ(checked.trait_object_method_slots.size(), 1U);
    const sema::TraitObjectMethodSlotFact& slot = checked.trait_object_method_slots.front();
    EXPECT_EQ(slot.method_name, "draw");
    EXPECT_EQ(slot.slot, 0U);
    EXPECT_EQ(slot.receiver_access, sema::ReceiverAccessKind::shared);
    EXPECT_EQ(checked.types.display_name(slot.receiver_type), "&dyn Draw");
    EXPECT_EQ(checked.types.display_name(slot.return_type), "i32");

    ASSERT_EQ(checked.trait_object_coercions.size(), 1U);
    const sema::TraitObjectCoercionFact& coercion = checked.trait_object_coercions.front();
    EXPECT_EQ(checked.types.display_name(coercion.source_reference_type), "&dyn_trait_shared_view_whitebox.File");
    EXPECT_EQ(checked.types.display_name(coercion.target_reference_type), "&dyn Draw");
    EXPECT_EQ(checked.types.display_name(coercion.source_type), "dyn_trait_shared_view_whitebox.File");
    EXPECT_EQ(checked.types.display_name(coercion.object_type), "dyn Draw");
    EXPECT_EQ(coercion.borrow_kind, query::TraitObjectBorrowKindKey::shared);
    EXPECT_TRUE(query::is_valid(coercion.coercion_key));
    EXPECT_TRUE(query::is_valid(coercion.vtable_layout));

    ASSERT_EQ(checked.vtable_layouts.size(), 1U);
    const sema::VTableLayoutFact& layout = checked.vtable_layouts.front();
    EXPECT_EQ(checked.types.display_name(layout.concrete_type), "dyn_trait_shared_view_whitebox.File");
    EXPECT_EQ(checked.types.display_name(layout.object_type), "dyn Draw");
    EXPECT_EQ(layout.method_slot_count, 1U);
    EXPECT_TRUE(query::is_valid(layout.layout_key));

    bool saw_dyn_call = false;
    for (const sema::TraitMethodCallBinding& call : checked.trait_method_calls) {
        if (call.method_name != "draw" || call.dispatch != sema::TraitMethodDispatchKind::vtable_slot) {
            continue;
        }
        saw_dyn_call = true;
        EXPECT_EQ(call.vtable_slot, 0U);
        EXPECT_EQ(checked.types.display_name(call.self_type), "dyn Draw");
        EXPECT_EQ(checked.types.display_name(call.return_type), "i32");
        EXPECT_EQ(call.receiver_access, sema::ReceiverAccessKind::shared);
    }
    EXPECT_TRUE(saw_dyn_call);

    const sema::CheckedModule copied = checked;
    ASSERT_EQ(copied.trait_object_callability.size(), 1U);
    ASSERT_EQ(copied.trait_object_method_slots.size(), 1U);
    ASSERT_EQ(copied.vtable_layouts.size(), 1U);
    ASSERT_EQ(copied.trait_object_coercions.size(), 1U);
    EXPECT_EQ(copied.trait_object_method_slots.front().method_name, "draw");
    EXPECT_EQ(copied.trait_object_coercions.front().borrow_kind, query::TraitObjectBorrowKindKey::shared);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait_object_callability 1",
            "trait_object #0 dyn Draw slots=1",
            "trait_object_method_slots 1",
            "slot #0 dyn Draw.draw slot=0 requirement=0 receiver_access=shared receiver=&dyn Draw return=i32",
            "vtable_layouts 1",
            "dyn_trait_shared_view_whitebox.File as dyn Draw slots=1",
            "trait_object_coercions 1",
            "dyn_coercion #0",
            "&dyn_trait_shared_view_whitebox.File -> &dyn Draw",
            "borrow=shared",
            "vtable_slot dyn Draw.draw -> i32 requirement=0 slot=0",
        });
}

TEST(CoreUnit, DynTraitBorrowedMutableViewRecordsMutableBorrowAndSlotAccess)
{
    const std::string_view source =
        "module dyn_trait_mut_view_whitebox;\n"
        "trait Draw {\n"
        "  fn draw(self: &mut Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Draw for File {\n"
        "  fn draw(self: &mut File) -> i32 { return self.value; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  var file: File = File { value: 2 };\n"
        "  let drawable: &mut dyn Draw = &mut file;\n"
        "  return drawable.draw();\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_object_callability.size(), 1U);
    ASSERT_EQ(checked.trait_object_method_slots.size(), 1U);
    EXPECT_EQ(checked.trait_object_method_slots.front().receiver_access, sema::ReceiverAccessKind::mutable_);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots.front().receiver_type), "&mut dyn Draw");

    ASSERT_EQ(checked.trait_object_coercions.size(), 1U);
    EXPECT_EQ(checked.trait_object_coercions.front().borrow_kind, query::TraitObjectBorrowKindKey::mut);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_coercions.front().source_reference_type),
        "&mut dyn_trait_mut_view_whitebox.File");
    EXPECT_EQ(checked.types.display_name(checked.trait_object_coercions.front().target_reference_type),
        "&mut dyn Draw");

    bool saw_mut_dyn_call = false;
    for (const sema::TraitMethodCallBinding& call : checked.trait_method_calls) {
        if (call.method_name != "draw" || call.dispatch != sema::TraitMethodDispatchKind::vtable_slot) {
            continue;
        }
        saw_mut_dyn_call = true;
        EXPECT_EQ(call.vtable_slot, 0U);
        EXPECT_EQ(call.receiver_access, sema::ReceiverAccessKind::mutable_);
        EXPECT_FALSE(call.receiver_auto_borrow);
        EXPECT_FALSE(call.receiver_two_phase_eligible);
    }
    EXPECT_TRUE(saw_mut_dyn_call);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "slot #0 dyn Draw.draw slot=0 requirement=0 receiver_access=mutable receiver=&mut dyn Draw return=i32",
            "&mut dyn_trait_mut_view_whitebox.File -> &mut dyn Draw",
            "borrow=mut",
            "vtable_slot dyn Draw.draw -> i32 requirement=0 slot=0",
            "receiver_access=mutable auto_borrow=false two_phase=false",
        });
    expect_trait_object_authority_matches_checked(checked, "main");
}

TEST(CoreUnit, DynTraitAssociatedEqualitySubstitutesMethodReturnType)
{
    const std::string_view source =
        "module dyn_trait_associated_view_whitebox;\n"
        "trait Source {\n"
        "  type Item;\n"
        "  fn item(self: &Self) -> Self.Item;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Source for File {\n"
        "  type Item = i32;\n"
        "  fn item(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 3 };\n"
        "  let source: &dyn Source[Item = i32] = &file;\n"
        "  return source.item();\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_object_callability.size(), 1U);
    ASSERT_EQ(checked.trait_object_method_slots.size(), 1U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_callability.front().object_type),
        "dyn Source[Item = i32]");
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots.front().return_type), "i32");
    ASSERT_EQ(checked.trait_object_coercions.size(), 1U);
    ASSERT_EQ(checked.vtable_layouts.size(), 1U);

    bool saw_item_call = false;
    for (const sema::TraitMethodCallBinding& call : checked.trait_method_calls) {
        if (call.method_name != "item" || call.dispatch != sema::TraitMethodDispatchKind::vtable_slot) {
            continue;
        }
        saw_item_call = true;
        EXPECT_EQ(call.vtable_slot, 0U);
        EXPECT_EQ(checked.types.display_name(call.return_type), "i32");
        EXPECT_EQ(checked.types.display_name(call.self_type), "dyn Source[Item = i32]");
    }
    EXPECT_TRUE(saw_item_call);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait_object #0 dyn Source[Item = i32] slots=1",
            "slot #0 dyn Source[Item = i32].item slot=0 requirement=1 receiver_access=shared "
            "receiver=&dyn Source[Item = i32] return=i32",
            "dyn_trait_associated_view_whitebox.File as dyn Source[Item = i32] slots=1",
            "&dyn_trait_associated_view_whitebox.File -> &dyn Source[Item = i32]",
            "vtable_slot dyn Source[Item = i32].item -> i32 requirement=1 slot=0",
        });
}

TEST(CoreUnit, DynTraitGenericObjectRecordsMultipleSlotsAndSubstitutedParams)
{
    const std::string_view source =
        "module dyn_trait_generic_view_whitebox;\n"
        "trait Reader[T] {\n"
        "  fn read(self: &Self, value: T) -> T;\n"
        "  fn reset(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Reader[i32] for File {\n"
        "  fn read(self: &File, value: i32) -> i32 { return value; }\n"
        "  fn reset(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 11 };\n"
        "  let reader: &dyn Reader[i32] = &file;\n"
        "  return reader.read(4) + reader.reset();\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_object_callability.size(), 1U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_callability.front().object_type), "dyn Reader[i32]");
    EXPECT_EQ(checked.trait_object_callability.front().method_slot_count, 2U);
    ASSERT_EQ(checked.trait_object_method_slots.size(), 2U);
    EXPECT_EQ(checked.trait_object_method_slots[0].method_name, "read");
    EXPECT_EQ(checked.trait_object_method_slots[0].slot, 0U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots[0].receiver_type), "&dyn Reader[i32]");
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots[0].return_type), "i32");
    EXPECT_EQ(checked.trait_object_method_slots[1].method_name, "reset");
    EXPECT_EQ(checked.trait_object_method_slots[1].slot, 1U);
    ASSERT_EQ(checked.vtable_layouts.size(), 1U);
    EXPECT_EQ(checked.vtable_layouts.front().method_slot_count, 2U);

    bool saw_read_call = false;
    bool saw_reset_call = false;
    for (const sema::TraitMethodCallBinding& call : checked.trait_method_calls) {
        if (call.dispatch != sema::TraitMethodDispatchKind::vtable_slot) {
            continue;
        }
        if (call.method_name == "read") {
            saw_read_call = true;
            EXPECT_EQ(call.vtable_slot, 0U);
            EXPECT_EQ(checked.types.display_name(call.receiver_type), "&dyn Reader[i32]");
            EXPECT_EQ(checked.types.display_name(call.return_type), "i32");
        } else if (call.method_name == "reset") {
            saw_reset_call = true;
            EXPECT_EQ(call.vtable_slot, 1U);
            EXPECT_EQ(checked.types.display_name(call.receiver_type), "&dyn Reader[i32]");
            EXPECT_EQ(checked.types.display_name(call.return_type), "i32");
        }
    }
    EXPECT_TRUE(saw_read_call);
    EXPECT_TRUE(saw_reset_call);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait_object #0 dyn Reader[i32] slots=2",
            "slot #0 dyn Reader[i32].read slot=0",
            "slot #1 dyn Reader[i32].reset slot=1",
            "dyn_trait_generic_view_whitebox.File as dyn Reader[i32] slots=2",
            "&dyn_trait_generic_view_whitebox.File -> &dyn Reader[i32]",
            "vtable_slot dyn Reader[i32].read -> i32 requirement=0 slot=0",
            "vtable_slot dyn Reader[i32].reset -> i32 requirement=1 slot=1",
        });
}

TEST(CoreUnit, DynTraitComplexReturnTypesSubstituteGenericSlots)
{
    const std::string_view source =
        "module dyn_trait_complex_slots_whitebox;\n"
        "trait Complex[T] {\n"
        "  fn ptr(self: &Self, value: *const T) -> *const T;\n"
        "  fn arr_ptr(self: &Self, value: *const [2]T) -> *const [2]T;\n"
        "  @borrow(return = [value])\n"
        "  fn slice(self: &Self, value: []const T) -> []const T;\n"
        "  @borrow(return = [self])\n"
        "  fn item_ref(self: &Self) -> &T;\n"
        "  fn tuple(self: &Self, value: (T, i32)) -> (T, i32);\n"
        "  fn callback(self: &Self, value: fn(T) -> T) -> fn(T) -> T;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Complex[i32] for File {\n"
        "  fn ptr(self: &File, value: *const i32) -> *const i32 { return value; }\n"
        "  fn arr_ptr(self: &File, value: *const [2]i32) -> *const [2]i32 { return value; }\n"
        "  @borrow(return = [value])\n"
        "  fn slice(self: &File, value: []const i32) -> []const i32 { return value; }\n"
        "  @borrow(return = [self])\n"
        "  fn item_ref(self: &File) -> &i32 { return &self.value; }\n"
        "  fn tuple(self: &File, value: (i32, i32)) -> (i32, i32) { return value; }\n"
        "  fn callback(self: &File, value: fn(i32) -> i32) -> fn(i32) -> i32 { return value; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 29 };\n"
        "  let complex: &dyn Complex[i32] = &file;\n"
        "  return 0;\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_object_callability.size(), 1U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_callability.front().object_type), "dyn Complex[i32]");
    EXPECT_TRUE(checked.types.is_trait_object(checked.trait_object_callability.front().object_type));
    EXPECT_EQ(checked.trait_object_callability.front().method_slot_count, 6U);
    ASSERT_EQ(checked.trait_object_method_slots.size(), 6U);

    EXPECT_EQ(checked.trait_object_method_slots[0].method_name, "ptr");
    EXPECT_EQ(checked.trait_object_method_slots[0].slot, 0U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots[0].return_type), "*const i32");
    EXPECT_EQ(checked.trait_object_method_slots[1].method_name, "arr_ptr");
    EXPECT_EQ(checked.trait_object_method_slots[1].slot, 1U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots[1].return_type), "*const [2]i32");
    EXPECT_EQ(checked.trait_object_method_slots[2].method_name, "slice");
    EXPECT_EQ(checked.trait_object_method_slots[2].slot, 2U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots[2].return_type), "[]const i32");
    EXPECT_EQ(checked.trait_object_method_slots[3].method_name, "item_ref");
    EXPECT_EQ(checked.trait_object_method_slots[3].slot, 3U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots[3].return_type), "&i32");
    EXPECT_EQ(checked.trait_object_method_slots[4].method_name, "tuple");
    EXPECT_EQ(checked.trait_object_method_slots[4].slot, 4U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots[4].return_type), "(i32, i32)");
    EXPECT_EQ(checked.trait_object_method_slots[5].method_name, "callback");
    EXPECT_EQ(checked.trait_object_method_slots[5].slot, 5U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots[5].return_type), "fn(i32) -> i32");

    ASSERT_EQ(checked.trait_object_coercions.size(), 1U);
    ASSERT_EQ(checked.vtable_layouts.size(), 1U);
    EXPECT_EQ(checked.vtable_layouts.front().method_slot_count, 6U);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait_object #0 dyn Complex[i32] slots=6",
            "slot #0 dyn Complex[i32].ptr slot=0",
            "slot #1 dyn Complex[i32].arr_ptr slot=1",
            "slot #2 dyn Complex[i32].slice slot=2",
            "slot #3 dyn Complex[i32].item_ref slot=3",
            "slot #4 dyn Complex[i32].tuple slot=4",
            "slot #5 dyn Complex[i32].callback slot=5",
            "return=*const i32",
            "return=*const [2]i32",
            "return=[]const i32",
            "return=&i32",
            "return=(i32, i32)",
            "return=fn(i32) -> i32",
        });
}

TEST(CoreUnit, DynTraitQualifiedCurrentModulePathResolvesObjectTrait)
{
    const std::string_view source =
        "module dyn_trait_qualified_path_whitebox.root;\n"
        "trait Draw {\n"
        "  fn draw(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Draw for File {\n"
        "  fn draw(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 31 };\n"
        "  let drawable: &dyn dyn_trait_qualified_path_whitebox.root.Draw = &file;\n"
        "  return drawable.draw();\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_object_callability.size(), 1U);
    EXPECT_TRUE(checked.types.is_trait_object(checked.trait_object_callability.front().object_type));
    EXPECT_EQ(checked.types.display_name(checked.trait_object_callability.front().object_type), "dyn Draw");
    ASSERT_EQ(checked.trait_object_method_slots.size(), 1U);
    EXPECT_EQ(checked.trait_object_method_slots.front().method_name, "draw");
    ASSERT_EQ(checked.trait_object_coercions.size(), 1U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_coercions.front().target_reference_type), "&dyn Draw");
}

TEST(CoreUnit, DynTraitMultipleGenericArgumentsKeepCanonicalIdentityAndDisplay)
{
    const std::string_view source =
        "module dyn_trait_two_generic_args_whitebox;\n"
        "trait Mapper[K, V] {\n"
        "  fn pick(self: &Self, key: K, value: V) -> V;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Mapper[i32, bool] for File {\n"
        "  fn pick(self: &File, key: i32, value: bool) -> bool { return value; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 23 };\n"
        "  let mapper: &dyn Mapper[i32, bool] = &file;\n"
        "  if mapper.pick(1, true) { return 1; }\n"
        "  return 0;\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_object_callability.size(), 1U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_callability.front().object_type),
        "dyn Mapper[i32,bool]");
    ASSERT_EQ(checked.trait_object_method_slots.size(), 1U);
    EXPECT_EQ(checked.trait_object_method_slots.front().method_name, "pick");
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots.front().receiver_type),
        "&dyn Mapper[i32,bool]");
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots.front().return_type), "bool");
    ASSERT_EQ(checked.trait_object_coercions.size(), 1U);
    ASSERT_EQ(checked.vtable_layouts.size(), 1U);

    bool saw_pick_call = false;
    for (const sema::TraitMethodCallBinding& call : checked.trait_method_calls) {
        if (call.method_name == "pick" && call.dispatch == sema::TraitMethodDispatchKind::vtable_slot) {
            saw_pick_call = true;
            EXPECT_EQ(call.vtable_slot, 0U);
            EXPECT_EQ(checked.types.display_name(call.receiver_type), "&dyn Mapper[i32,bool]");
            EXPECT_EQ(checked.types.display_name(call.return_type), "bool");
        }
    }
    EXPECT_TRUE(saw_pick_call);
}

TEST(CoreUnit, DynTraitAssociatedEqualityOrderIsCanonicalAndImplMustMatch)
{
    const std::string_view source =
        "module dyn_trait_multi_assoc_view_whitebox;\n"
        "trait Source[T] {\n"
        "  type Item;\n"
        "  type Error;\n"
        "  fn item(self: &Self, value: T) -> Self.Item;\n"
        "  fn err(self: &Self) -> Self.Error;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Source[i32] for File {\n"
        "  type Item = i32;\n"
        "  type Error = bool;\n"
        "  fn item(self: &File, value: i32) -> i32 { return value; }\n"
        "  fn err(self: &File) -> bool { return true; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 17 };\n"
        "  let source: &dyn Source[i32, Error = bool, Item = i32] = &file;\n"
        "  if source.err() { return source.item(5); }\n"
        "  return 0;\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_object_callability.size(), 1U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_callability.front().object_type),
        "dyn Source[i32,Error = bool,Item = i32]");
    EXPECT_EQ(checked.trait_object_callability.front().method_slot_count, 2U);
    ASSERT_EQ(checked.trait_object_method_slots.size(), 2U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots[0].return_type), "i32");
    EXPECT_EQ(checked.types.display_name(checked.trait_object_method_slots[1].return_type), "bool");
    ASSERT_EQ(checked.trait_object_coercions.size(), 1U);
    ASSERT_EQ(checked.vtable_layouts.size(), 1U);
    EXPECT_EQ(checked.vtable_layouts.front().method_slot_count, 2U);

    bool saw_item_call = false;
    bool saw_err_call = false;
    for (const sema::TraitMethodCallBinding& call : checked.trait_method_calls) {
        if (call.dispatch != sema::TraitMethodDispatchKind::vtable_slot) {
            continue;
        }
        if (call.method_name == "item") {
            saw_item_call = true;
            EXPECT_EQ(call.vtable_slot, 0U);
            EXPECT_EQ(checked.types.display_name(call.return_type), "i32");
        } else if (call.method_name == "err") {
            saw_err_call = true;
            EXPECT_EQ(call.vtable_slot, 1U);
            EXPECT_EQ(checked.types.display_name(call.return_type), "bool");
        }
    }
    EXPECT_TRUE(saw_item_call);
    EXPECT_TRUE(saw_err_call);

    expect_dyn_trait_source_diagnostic(
        "module dyn_trait_multi_assoc_impl_mismatch_whitebox;\n"
        "trait Source { type Item; fn item(self: &Self) -> Self.Item; }\n"
        "struct File { value: i32; }\n"
        "impl Source for File {\n"
        "  type Item = bool;\n"
        "  fn item(self: &File) -> bool { return true; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 1 };\n"
        "  let source: &dyn Source[Item = i32] = &file;\n"
        "  return 0;\n"
        "}\n",
        "type dyn_trait_multi_assoc_impl_mismatch_whitebox.File cannot be coerced to dyn trait `Source` "
        "because no matching trait impl is visible");
}

TEST(CoreUnit, DynTraitCoercionFactsAreDeduplicatedForSameExpressionAndLayout)
{
    const std::string_view source =
        "module dyn_trait_coercion_dedup_whitebox;\n"
        "trait Draw { fn draw(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Draw for File {\n"
        "  fn draw(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "fn consume(drawable: &dyn Draw) -> i32 { return drawable.draw(); }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 19 };\n"
        "  return consume(&file) + consume(&file);\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_object_callability.size(), 1U);
    ASSERT_EQ(checked.vtable_layouts.size(), 1U);
    ASSERT_EQ(checked.trait_object_coercions.size(), 2U);
    EXPECT_EQ(checked.trait_object_coercions[0].coercion_key, checked.trait_object_coercions[1].coercion_key);
    EXPECT_EQ(checked.trait_object_coercions[0].vtable_layout, checked.trait_object_coercions[1].vtable_layout);
    EXPECT_NE(checked.trait_object_coercions[0].expr.value, checked.trait_object_coercions[1].expr.value);

    base::u32 dyn_call_count = 0;
    for (const sema::TraitMethodCallBinding& call : checked.trait_method_calls) {
        if (call.method_name == "draw" && call.dispatch == sema::TraitMethodDispatchKind::vtable_slot) {
            ++dyn_call_count;
            EXPECT_EQ(call.vtable_slot, 0U);
        }
    }
    EXPECT_EQ(dyn_call_count, 1U);
}

TEST(CoreUnit, DynTraitExistingObjectReferenceAssignsWithoutNewBorrowedCoercion)
{
    const std::string_view source =
        "module dyn_trait_existing_object_reference_whitebox;\n"
        "trait Draw { fn draw(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Draw for File {\n"
        "  fn draw(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 37 };\n"
        "  let drawable: &dyn Draw = &file;\n"
        "  let again: &dyn Draw = drawable;\n"
        "  return again.draw();\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_object_callability.size(), 1U);
    ASSERT_EQ(checked.trait_object_coercions.size(), 1U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_coercions.front().source_reference_type),
        "&dyn_trait_existing_object_reference_whitebox.File");
    EXPECT_EQ(checked.types.display_name(checked.trait_object_coercions.front().target_reference_type), "&dyn Draw");

    base::u32 dyn_call_count = 0;
    for (const sema::TraitMethodCallBinding& call : checked.trait_method_calls) {
        if (call.method_name == "draw" && call.dispatch == sema::TraitMethodDispatchKind::vtable_slot) {
            ++dyn_call_count;
            EXPECT_EQ(call.vtable_slot, 0U);
            EXPECT_EQ(checked.types.display_name(call.receiver_type), "&dyn Draw");
        }
    }
    EXPECT_EQ(dyn_call_count, 1U);
}

TEST(CoreUnit, DynTraitRejectsInvalidObjectSurfacesAndMissingImplCoercions)
{
    const std::vector<std::pair<std::string_view, std::string_view>> cases{
        {
            "module dyn_trait_bare_storage_whitebox;\n"
            "trait Draw { fn draw(self: &Self) -> i32; }\n"
            "fn render(drawable: dyn Draw) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "function parameter type is not valid storage",
        },
        {
            "module dyn_trait_missing_associated_whitebox;\n"
            "trait Source { type Item; fn item(self: &Self) -> Self.Item; }\n"
            "fn render(source: &dyn Source) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "dyn trait `Source` requires associated type equality `Item = ...`",
        },
        {
            "module dyn_trait_unknown_associated_whitebox;\n"
            "trait Source { type Item; fn item(self: &Self) -> Self.Item; }\n"
            "fn render(source: &dyn Source[Other = i32]) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "trait Source has no associated type `Other`",
        },
        {
            "module dyn_trait_duplicate_associated_whitebox;\n"
            "trait Source { type Item; fn item(self: &Self) -> Self.Item; }\n"
            "fn render(source: &dyn Source[Item = i32, Item = i32]) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "duplicate associated type equality for Source.Item",
        },
        {
            "module dyn_trait_static_method_whitebox;\n"
            "trait Bad { fn run(value: i32) -> i32; }\n"
            "fn render(value: &dyn Bad) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "dyn trait `Bad` method `run` must have a self receiver",
        },
        {
            "module dyn_trait_value_receiver_whitebox;\n"
            "trait Bad { fn run(self: Self) -> i32; }\n"
            "fn render(value: &dyn Bad) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "dyn trait `Bad` method `run` receiver must be &Self or &mut Self",
        },
        {
            "module dyn_trait_self_return_whitebox;\n"
            "trait Bad { fn clone(self: &Self) -> Self; }\n"
            "fn render(value: &dyn Bad) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "dyn trait `Bad` method `clone` can only use Self in the receiver",
        },
        {
            "module dyn_trait_missing_impl_coercion_whitebox;\n"
            "trait Draw { fn draw(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "fn main() -> i32 {\n"
            "  let file: File = File { value: 1 };\n"
            "  let drawable: &dyn Draw = &file;\n"
            "  return 0;\n"
            "}\n",
            "type dyn_trait_missing_impl_coercion_whitebox.File cannot be coerced to dyn trait `Draw` "
            "because no matching trait impl is visible",
        },
        {
            "module dyn_trait_missing_generic_args_whitebox;\n"
            "trait Reader[T] { fn read(self: &Self, value: T) -> i32; }\n"
            "fn render(reader: &dyn Reader) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "too few trait type arguments for Reader: expected 1, got 0",
        },
        {
            "module dyn_trait_plain_trait_with_args_whitebox;\n"
            "trait Draw { fn draw(self: &Self) -> i32; }\n"
            "fn render(drawable: &dyn Draw[i32]) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "trait Draw is not generic",
        },
        {
            "module dyn_trait_generic_arity_mismatch_whitebox;\n"
            "trait Reader[A, B] { fn read(self: &Self, value: A) -> B; }\n"
            "fn render(reader: &dyn Reader[i32]) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "too few trait type arguments for Reader: expected 2, got 1",
        },
        {
            "module dyn_trait_self_param_whitebox;\n"
            "trait Bad { fn eq(self: &Self, other: &Self) -> bool; }\n"
            "fn render(value: &dyn Bad) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "dyn trait `Bad` method `eq` can only use Self in the receiver",
        },
        {
            "module dyn_trait_self_composite_param_whitebox;\n"
            "trait Bad { fn take(self: &Self, value: (Self, i32)) -> i32; }\n"
            "fn render(value: &dyn Bad) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "dyn trait `Bad` method `take` can only use Self in the receiver",
        },
        {
            "module dyn_trait_self_pointer_param_whitebox;\n"
            "trait Bad { fn take(self: &Self, value: *const Self) -> i32; }\n"
            "fn render(value: &dyn Bad) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "dyn trait `Bad` method `take` can only use Self in the receiver",
        },
        {
            "module dyn_trait_self_slice_param_whitebox;\n"
            "trait Bad { fn take(self: &Self, value: []const Self) -> i32; }\n"
            "fn render(value: &dyn Bad) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "dyn trait `Bad` method `take` can only use Self in the receiver",
        },
        {
            "module dyn_trait_self_array_param_whitebox;\n"
            "trait Bad { fn take(self: &Self, value: [2]Self) -> i32; }\n"
            "fn render(value: &dyn Bad) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "dyn trait `Bad` method `take` can only use Self in the receiver",
        },
        {
            "module dyn_trait_self_function_param_whitebox;\n"
            "trait Bad { fn take(self: &Self, value: fn(Self) -> i32) -> i32; }\n"
            "fn render(value: &dyn Bad) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "dyn trait `Bad` method `take` can only use Self in the receiver",
        },
        {
            "module dyn_trait_missing_method_call_whitebox;\n"
            "trait Draw { fn draw(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Draw for File {\n"
            "  fn draw(self: &File) -> i32 { return self.value; }\n"
            "}\n"
            "fn main() -> i32 {\n"
            "  let file: File = File { value: 1 };\n"
            "  let drawable: &dyn Draw = &file;\n"
            "  return drawable.missing();\n"
            "}\n",
            "type dyn Draw has no visible impl for trait method `missing`",
        },
    };

    for (const auto& [source, diagnostic] : cases) {
        expect_dyn_trait_source_diagnostic(source, diagnostic);
    }
}

} // namespace aurex::test
