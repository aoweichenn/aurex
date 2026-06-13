#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/infrastructure/query/principal_set_composition_facts.hpp>

#include <gtest/frontend/sema/dyn_trait_test_support.hpp>
#include <support/frontend_test_support.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

[[nodiscard]] std::string_view composition_supertrait_source()
{
    return
        "trait Parent { fn parent(self: &Self) -> i32; }\n"
        "trait Child: Parent { fn child(self: &Self) -> i32; }\n"
        "trait Debug { fn debug(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Parent for File { fn parent(self: &File) -> i32 { return self.value; } }\n"
        "impl Child for File { fn child(self: &File) -> i32 { return self.value + 1; } }\n"
        "impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 2; } }\n";
}

[[nodiscard]] std::vector<const query::CompositionProjectionFact*> composition_supertrait_projections(
    const sema::CheckedModule& checked)
{
    std::vector<const query::CompositionProjectionFact*> projections;
    for (const query::CompositionProjectionFact& projection :
        checked.principal_set_composition_facts.projections) {
        if (projection.kind == query::PrincipalSetProjectionKind::composition_to_supertrait) {
            projections.push_back(&projection);
        }
    }
    return projections;
}

TEST(CoreUnit, DynTraitCompositionProjectsBorrowedViewToSupertraitExplicitly)
{
    const std::string source =
        "module dyn_trait_composition_supertrait_projection_whitebox;\n"
        + std::string(composition_supertrait_source())
        + "fn score(view: &dyn (Child + Debug)) -> i32 {\n"
          "  let parent: &dyn Parent = dynproject<Child, Parent>(view);\n"
          "  return parent.parent();\n"
          "}\n"
          "fn main() -> i32 {\n"
          "  let file: File = File { value: 7 };\n"
          "  let view: &dyn (Debug + Child) = &file;\n"
          "  return score(view);\n"
          "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    const std::vector<const query::CompositionProjectionFact*> projections =
        composition_supertrait_projections(checked);
    ASSERT_EQ(projections.size(), 1U);
    const query::CompositionProjectionFact& projection = *projections.front();
    EXPECT_EQ(projection.borrow_kind, query::DynBorrowKind::shared);
    EXPECT_NE(projection.source_view_name.find("dyn ("), std::string::npos) << projection.source_view_name;
    EXPECT_NE(projection.source_view_name.find("Child"), std::string::npos) << projection.source_view_name;
    EXPECT_NE(projection.source_view_name.find("Debug"), std::string::npos) << projection.source_view_name;
    EXPECT_EQ(projection.target_view_name, "dyn Parent");
    EXPECT_TRUE(projection.data_pointer_preserved);
    EXPECT_TRUE(projection.origin_preserved);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.supertrait_projection_count, 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.borrowed_view_path_count, 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.borrowed_view_path_dispatch_count, 0U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.borrowed_view_path_expected_projection_count, 0U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.shared_borrow_projection_count, 3U);
    EXPECT_TRUE(query::is_valid(checked.principal_set_composition_facts));
    EXPECT_EQ(query::principal_set_composition_facts_fingerprint(checked.principal_set_composition_facts),
        checked.principal_set_composition_facts.fingerprint);

    ASSERT_EQ(checked.trait_method_calls.size(), 1U);
    const sema::TraitMethodCallBinding& call = checked.trait_method_calls.front();
    EXPECT_EQ(call.method_name.view(), "parent");
    EXPECT_EQ(call.dispatch, sema::TraitMethodDispatchKind::vtable_slot);
    EXPECT_EQ(checked.types.display_name(call.receiver_type), "&dyn Parent");
    EXPECT_EQ(checked.types.display_name(call.dispatch_receiver_type), "dyn Parent");

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "kind=composition_to_supertrait borrow=shared",
            "borrowed_dyn_view_path_fact",
            "use=explicit_projection",
            "target=dyn Parent",
            "supertrait_projections=1",
        });
    expect_trait_object_authority_matches_checked(checked, "score");
}

TEST(CoreUnit, DynTraitCompositionProjectsMutableViewToMutableSupertrait)
{
    const std::string_view source =
        "module dyn_trait_composition_mut_supertrait_projection_whitebox;\n"
        "trait Parent { fn parent(self: &mut Self) -> i32; }\n"
        "trait Child: Parent { fn child(self: &mut Self) -> i32; }\n"
        "trait Debug { fn debug(self: &mut Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Parent for File { fn parent(self: &mut File) -> i32 { return self.value; } }\n"
        "impl Child for File { fn child(self: &mut File) -> i32 { return self.value + 1; } }\n"
        "impl Debug for File { fn debug(self: &mut File) -> i32 { return self.value + 2; } }\n"
        "fn score(view: &mut dyn (Child + Debug)) -> i32 {\n"
        "  let parent: &mut dyn Parent = dynproject<Child, Parent>(view);\n"
        "  return parent.parent();\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  var file: File = File { value: 7 };\n"
        "  let view: &mut dyn (Debug + Child) = &mut file;\n"
        "  return score(view);\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    const std::vector<const query::CompositionProjectionFact*> projections =
        composition_supertrait_projections(checked);
    ASSERT_EQ(projections.size(), 1U);
    EXPECT_EQ(projections.front()->borrow_kind, query::DynBorrowKind::mut);
    EXPECT_EQ(projections.front()->target_view_name, "dyn Parent");
    EXPECT_EQ(checked.principal_set_composition_facts.summary.supertrait_projection_count, 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.borrowed_view_path_count, 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.mut_borrow_projection_count, 3U);
    EXPECT_TRUE(query::is_valid(checked.principal_set_composition_facts));

    ASSERT_EQ(checked.trait_method_calls.size(), 1U);
    const sema::TraitMethodCallBinding& call = checked.trait_method_calls.front();
    EXPECT_EQ(checked.types.display_name(call.receiver_type), "&mut dyn Parent");
    EXPECT_EQ(call.receiver_access, sema::ReceiverAccessKind::mutable_);
}

TEST(CoreUnit, DynTraitCompositionAllowsMutableProjectionToSharedSupertraitContext)
{
    const std::string source =
        "module dyn_trait_composition_mut_to_shared_supertrait_projection_whitebox;\n"
        + std::string(composition_supertrait_source())
        + "fn score(view: &mut dyn (Child + Debug)) -> i32 {\n"
          "  let parent: &dyn Parent = dynproject<Child, Parent>(view);\n"
          "  return parent.parent();\n"
          "}\n"
          "fn main() -> i32 {\n"
          "  var file: File = File { value: 7 };\n"
          "  let view: &mut dyn (Debug + Child) = &mut file;\n"
          "  return score(view);\n"
          "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    const std::vector<const query::CompositionProjectionFact*> projections =
        composition_supertrait_projections(checked);
    ASSERT_EQ(projections.size(), 1U);
    EXPECT_EQ(projections.front()->borrow_kind, query::DynBorrowKind::shared);
    EXPECT_EQ(projections.front()->target_view_name, "dyn Parent");
    ASSERT_EQ(checked.trait_object_upcast_coercions.size(), 1U);
    EXPECT_EQ(checked.trait_object_upcast_coercions.front().borrow_kind,
        query::TraitObjectBorrowKindKey::shared);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_upcast_coercions.front().source_reference_type),
        "&mut dyn Child");
    EXPECT_EQ(checked.types.display_name(checked.trait_object_upcast_coercions.front().target_reference_type),
        "&dyn Parent");
    ASSERT_EQ(checked.trait_method_calls.size(), 1U);
    EXPECT_EQ(checked.types.display_name(checked.trait_method_calls.front().receiver_type), "&dyn Parent");
}

TEST(CoreUnit, DynTraitCompositionProjectsGenericTransitiveSupertraitExplicitly)
{
    const std::string_view source =
        "module dyn_trait_composition_generic_supertrait_projection_whitebox;\n"
        "trait Parent<T> { fn parent(self: &Self, value: T) -> T; }\n"
        "trait Mid<T>: Parent<T> { fn mid(self: &Self) -> i32; }\n"
        "trait Child<T>: Mid<T> { fn child(self: &Self) -> i32; }\n"
        "trait Debug { fn debug(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Parent<i32> for File { fn parent(self: &File, value: i32) -> i32 { return value + self.value; } }\n"
        "impl Mid<i32> for File { fn mid(self: &File) -> i32 { return self.value + 1; } }\n"
        "impl Child<i32> for File { fn child(self: &File) -> i32 { return self.value + 2; } }\n"
        "impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 3; } }\n"
        "fn score(view: &dyn (Child<i32> + Debug)) -> i32 {\n"
        "  let parent: &dyn Parent<i32> = dynproject<Child<i32>, Parent<i32>>(view);\n"
        "  return parent.parent(5);\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 7 };\n"
        "  let view: &dyn (Debug + Child<i32>) = &file;\n"
        "  return score(view);\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    const std::vector<const query::CompositionProjectionFact*> projections =
        composition_supertrait_projections(checked);
    ASSERT_EQ(projections.size(), 1U);
    EXPECT_EQ(projections.front()->borrow_kind, query::DynBorrowKind::shared);
    EXPECT_EQ(projections.front()->target_view_name, "dyn Parent<i32>");
    EXPECT_EQ(checked.principal_set_composition_facts.summary.borrowed_view_path_count, 1U);
    ASSERT_EQ(checked.trait_method_calls.size(), 1U);
    const sema::TraitMethodCallBinding& call = checked.trait_method_calls.front();
    EXPECT_EQ(call.method_name.view(), "parent");
    EXPECT_EQ(call.dispatch, sema::TraitMethodDispatchKind::vtable_slot);
    EXPECT_EQ(checked.types.display_name(call.receiver_type), "&dyn Parent<i32>");
    EXPECT_EQ(checked.types.display_name(call.return_type), "i32");
    EXPECT_TRUE(query::is_valid(checked.principal_set_composition_facts));
}

TEST(CoreUnit, DynTraitCompositionDirectSupertraitMethodDispatchInfersBorrowedViewPath)
{
    const std::string source =
        "module dyn_trait_composition_direct_supertrait_dispatch_whitebox;\n"
        + std::string(composition_supertrait_source())
        + "fn score(view: &dyn (Child + Debug)) -> i32 {\n"
          "  return view.parent();\n"
          "}\n"
          "fn main() -> i32 {\n"
          "  let file: File = File { value: 7 };\n"
          "  return score(&file);\n"
          "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    EXPECT_TRUE(query::is_valid(checked.principal_set_composition_facts));
    EXPECT_EQ(checked.principal_set_composition_facts.summary.supertrait_projection_count, 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.borrowed_view_path_count, 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.borrowed_view_path_dispatch_count, 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.borrowed_view_path_expected_projection_count, 0U);
    ASSERT_EQ(checked.principal_set_composition_facts.borrowed_view_paths.size(), 1U);
    const query::BorrowedDynViewPathFact& path =
        checked.principal_set_composition_facts.borrowed_view_paths.front();
    EXPECT_EQ(path.use, query::BorrowedDynViewPathUse::method_dispatch);
    EXPECT_EQ(path.method_name, "parent");
    EXPECT_TRUE(path.vtable_dispatch_step);
    EXPECT_NE(path.source_view_name.find("&dyn ("), std::string::npos) << path.source_view_name;
    EXPECT_EQ(path.projected_view_name, "&dyn Child");
    EXPECT_EQ(path.target_view_name, "&dyn Parent");

    ASSERT_EQ(checked.trait_object_upcast_coercions.size(), 1U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_upcast_coercions.front().source_reference_type),
        "&dyn Child");
    EXPECT_EQ(checked.types.display_name(checked.trait_object_upcast_coercions.front().target_reference_type),
        "&dyn Parent");

    ASSERT_EQ(checked.trait_method_calls.size(), 1U);
    const sema::TraitMethodCallBinding& call = checked.trait_method_calls.front();
    EXPECT_EQ(call.method_name.view(), "parent");
    EXPECT_EQ(call.dispatch, sema::TraitMethodDispatchKind::vtable_slot);
    EXPECT_NE(checked.types.display_name(call.receiver_type).find("&dyn ("), std::string::npos)
        << checked.types.display_name(call.receiver_type);
    EXPECT_EQ(checked.types.display_name(call.dispatch_receiver_type), "dyn Parent");
    EXPECT_EQ(query::principal_set_composition_facts_fingerprint(checked.principal_set_composition_facts),
        checked.principal_set_composition_facts.fingerprint);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "borrowed_dyn_view_path_fact",
            "use=method_dispatch",
            "method=parent",
            "projected=&dyn Child",
            "target=&dyn Parent",
        });
}

TEST(CoreUnit, DynTraitCompositionExpectedTypeProjectionInfersBorrowedViewPath)
{
    const std::string source =
        "module dyn_trait_composition_expected_supertrait_projection_whitebox;\n"
        + std::string(composition_supertrait_source())
        + "fn score(view: &dyn (Child + Debug)) -> i32 {\n"
          "  let parent: &dyn Parent = view;\n"
          "  return parent.parent();\n"
          "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    EXPECT_TRUE(query::is_valid(checked.principal_set_composition_facts));
    EXPECT_EQ(checked.principal_set_composition_facts.summary.supertrait_projection_count, 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.borrowed_view_path_count, 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.borrowed_view_path_dispatch_count, 0U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.borrowed_view_path_expected_projection_count, 1U);
    ASSERT_EQ(checked.principal_set_composition_facts.borrowed_view_paths.size(), 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.borrowed_view_paths.front().use,
        query::BorrowedDynViewPathUse::expected_type_projection);
    EXPECT_EQ(checked.principal_set_composition_facts.borrowed_view_paths.front().projected_view_name,
        "&dyn Child");
    EXPECT_EQ(checked.principal_set_composition_facts.borrowed_view_paths.front().target_view_name,
        "&dyn Parent");
    ASSERT_EQ(checked.trait_object_upcast_coercions.size(), 1U);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_upcast_coercions.front().source_reference_type),
        "&dyn Child");
    EXPECT_EQ(checked.types.display_name(checked.trait_object_upcast_coercions.front().target_reference_type),
        "&dyn Parent");
}

TEST(CoreUnit, DynTraitCompositionDirectSupertraitMethodDispatchRejectsAmbiguousPath)
{
    const std::string_view source =
        "module dyn_trait_composition_ambiguous_supertrait_dispatch_whitebox;\n"
        "trait Parent { fn parent(self: &Self) -> i32; }\n"
        "trait ChildA: Parent { fn child_a(self: &Self) -> i32; }\n"
        "trait ChildB: Parent { fn child_b(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Parent for File { fn parent(self: &File) -> i32 { return self.value; } }\n"
        "impl ChildA for File { fn child_a(self: &File) -> i32 { return self.value + 1; } }\n"
        "impl ChildB for File { fn child_b(self: &File) -> i32 { return self.value + 2; } }\n"
        "fn score(view: &dyn (ChildA + ChildB)) -> i32 {\n"
        "  return view.parent();\n"
        "}\n";

    expect_dyn_trait_source_diagnostic(
        source, "dyn trait composition method `parent` is ambiguous across multiple principal traits");
}

TEST(CoreUnit, DynProjectNameRemainsAvailableForOrdinaryFunction)
{
    const std::string source =
        "module dyn_trait_composition_dynproject_name_whitebox;\n"
        "fn dynproject(value: i32) -> i32 {\n"
        "  return value + 1;\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  return dynproject(41);\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    EXPECT_TRUE(query::is_valid(checked.principal_set_composition_facts));
    EXPECT_EQ(checked.principal_set_composition_facts.summary.supertrait_projection_count, 0U);
}

TEST(CoreUnit, DynTraitCompositionRejectsInvalidSupertraitProjectionForms)
{
    const std::vector<std::pair<std::string, std::string_view>> cases{
        {
            "module dyn_trait_composition_dynproject_type_arity_reject_whitebox;\n"
            + std::string(composition_supertrait_source())
            + "fn score(view: &dyn (Child + Debug)) -> i32 {\n"
              "  let parent: &dyn Parent = dynproject<Child>(view);\n"
              "  return parent.parent();\n"
              "}\n",
            "dynproject requires exactly two type arguments",
        },
        {
            "module dyn_trait_composition_dynproject_arg_arity_reject_whitebox;\n"
            + std::string(composition_supertrait_source())
            + "fn score(view: &dyn (Child + Debug)) -> i32 {\n"
              "  let parent: &dyn Parent = dynproject<Child, Parent>(view, view);\n"
              "  return parent.parent();\n"
              "}\n",
            "dynproject requires exactly one argument",
        },
        {
            "module dyn_trait_composition_dynproject_source_not_trait_reject_whitebox;\n"
            + std::string(composition_supertrait_source())
            + "fn score(view: &dyn (Child + Debug)) -> i32 {\n"
              "  let parent: &dyn Parent = dynproject<File, Parent>(view);\n"
              "  return parent.parent();\n"
              "}\n",
            "dynproject source must be a dyn trait principal",
        },
        {
            "module dyn_trait_composition_dynproject_target_not_trait_reject_whitebox;\n"
            + std::string(composition_supertrait_source())
            + "fn score(view: &dyn (Child + Debug)) -> i32 {\n"
              "  let parent: &dyn Parent = dynproject<Child, File>(view);\n"
              "  return parent.parent();\n"
              "}\n",
            "dynproject target must be a dyn trait supertrait",
        },
        {
            "module dyn_trait_composition_dynproject_arg_not_composition_reject_whitebox;\n"
            + std::string(composition_supertrait_source())
            + "fn score(view: &dyn Child) -> i32 {\n"
              "  let parent: &dyn Parent = dynproject<Child, Parent>(view);\n"
              "  return parent.parent();\n"
              "}\n",
            "dynproject argument must be a borrowed dyn trait composition",
        },
        {
            "module dyn_trait_composition_dynproject_source_missing_reject_whitebox;\n"
            + std::string(composition_supertrait_source())
            + "trait Other { fn other(self: &Self) -> i32; }\n"
              "fn score(view: &dyn (Child + Debug)) -> i32 {\n"
              "  let parent: &dyn Parent = dynproject<Other, Parent>(view);\n"
              "  return parent.parent();\n"
              "}\n",
            "dynproject source principal is not in the composition",
        },
        {
            "module dyn_trait_composition_dynproject_target_not_supertrait_reject_whitebox;\n"
            + std::string(composition_supertrait_source())
            + "fn score(view: &dyn (Child + Debug)) -> i32 {\n"
              "  let debug: &dyn Debug = dynproject<Child, Debug>(view);\n"
              "  return debug.debug();\n"
              "}\n",
            "dynproject target is not a supertrait of the selected source principal",
        },
    };

    for (const auto& [source, diagnostic] : cases) {
        expect_dyn_trait_source_diagnostic(source, diagnostic);
    }
}

TEST(CoreUnit, DynTraitCompositionAllowsImplicitDirectSupertraitDispatch)
{
    const std::string source =
        "module dyn_trait_composition_direct_parent_call_whitebox;\n"
        + std::string(composition_supertrait_source())
        + "fn score(view: &dyn (Child + Debug)) -> i32 {\n"
          "  return view.parent();\n"
          "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_method_calls.size(), 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.borrowed_view_path_dispatch_count, 1U);
}

} // namespace
} // namespace aurex::test
