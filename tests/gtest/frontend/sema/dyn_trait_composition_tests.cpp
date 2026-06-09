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

TEST(CoreUnit, DynTraitCompositionBorrowedSharedViewRecordsPrincipalSetFacts)
{
    const std::string_view source =
        "module dyn_trait_composition_shared_whitebox;\n"
        "trait Draw { fn draw(self: &Self) -> i32; }\n"
        "trait Debug { fn debug(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "struct Brush { value: i32; }\n"
        "impl Draw for File { fn draw(self: &File) -> i32 { return self.value; } }\n"
        "impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 1; } }\n"
        "impl Draw for Brush { fn draw(self: &Brush) -> i32 { return self.value; } }\n"
        "impl Debug for Brush { fn debug(self: &Brush) -> i32 { return self.value + 2; } }\n"
        "fn consume(view: &dyn (Draw + Debug)) -> i32 { return 0; }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 7 };\n"
        "  let brush: Brush = Brush { value: 11 };\n"
        "  let view: &dyn (Debug + Draw) = &file;\n"
        "  let other: &dyn (Draw + Debug) = &brush;\n"
        "  return consume(view) + consume(other);\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_object_callability.size(), 2U);
    ASSERT_EQ(checked.trait_object_method_slots.size(), 2U);
    ASSERT_EQ(checked.vtable_layouts.size(), 4U);
    EXPECT_TRUE(checked.trait_object_coercions.empty());
    ASSERT_EQ(checked.principal_set_composition_facts.identity_facts.size(), 1U);
    ASSERT_EQ(checked.principal_set_composition_facts.method_namespaces.size(), 1U);
    ASSERT_EQ(checked.principal_set_composition_facts.witness_sets.size(), 2U);
    ASSERT_EQ(checked.principal_set_composition_facts.projections.size(), 4U);
    EXPECT_TRUE(query::is_valid(checked.principal_set_composition_facts));
    EXPECT_EQ(checked.principal_set_composition_facts.summary.principal_set_count, 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.principal_count, 2U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.witness_set_count, 2U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.witness_count, 4U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.projection_count, 4U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.shared_borrow_projection_count, 4U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.mut_borrow_projection_count, 0U);
    EXPECT_NE(checked.principal_set_composition_facts.fingerprint.byte_count, 0U);

    const query::PrincipalSetIdentityFact& identity =
        checked.principal_set_composition_facts.identity_facts.front();
    ASSERT_EQ(identity.principals.size(), 2U);
    std::vector<std::string> canonical_principals{
        identity.principals[0].principal_name,
        identity.principals[1].principal_name,
    };
    std::vector<std::string> sorted_principals = canonical_principals;
    std::ranges::sort(sorted_principals);
    EXPECT_EQ(sorted_principals[0], "Debug");
    EXPECT_EQ(sorted_principals[1], "Draw");
    const query::StableFingerprint128 composition_identity = identity.principal_set_identity;
    EXPECT_NE(composition_identity.byte_count, 0U);
    const std::string canonical_composition_display =
        "dyn (" + canonical_principals[0] + " + " + canonical_principals[1] + ")";

    std::vector<std::string> witness_concrete_types;
    for (const query::CompositionWitnessSetFact& witness_set :
        checked.principal_set_composition_facts.witness_sets) {
        ASSERT_EQ(witness_set.witnesses.size(), 2U);
        EXPECT_EQ(witness_set.principal_set_identity, composition_identity);
        EXPECT_LT(witness_set.witnesses[0].principal_object.global_id,
            witness_set.witnesses[1].principal_object.global_id);
        witness_concrete_types.push_back(witness_set.witnesses.front().concrete_type_name);
    }
    std::ranges::sort(witness_concrete_types);
    ASSERT_EQ(witness_concrete_types.size(), 2U);
    EXPECT_EQ(witness_concrete_types[0], "dyn_trait_composition_shared_whitebox.Brush");
    EXPECT_EQ(witness_concrete_types[1], "dyn_trait_composition_shared_whitebox.File");

    bool saw_composition_type = false;
    for (base::u32 index = 0; index < checked.types.size(); ++index) {
        const sema::TypeHandle type{index};
        if (!checked.types.is_principal_set_trait_object(type)) {
            continue;
        }
        saw_composition_type = true;
        EXPECT_EQ(checked.types.display_name(type), canonical_composition_display);
    }
    EXPECT_TRUE(saw_composition_type);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "principal_set_composition 1 fingerprint=",
            "principal_set_composition_facts subject=checked dyn trait principal-set composition",
            "principal_sets=1 principals=2 witness_sets=2 witnesses=4",
            "principal_set_identity_fact #0",
            "principal=Draw object=dyn Draw",
            "principal=Debug object=dyn Debug",
            "composition_witness_set_fact #0",
            "composition_projection_fact #0",
            "kind=concrete_to_composition borrow=shared",
        });
    expect_trait_object_authority_matches_checked(checked, "main");
}

TEST(CoreUnit, DynTraitCompositionMutableBorrowAndCanonicalOrderAreStable)
{
    const std::string_view source =
        "module dyn_trait_composition_mut_whitebox;\n"
        "trait Draw { fn draw(self: &mut Self) -> i32; }\n"
        "trait Debug { fn debug(self: &mut Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Draw for File { fn draw(self: &mut File) -> i32 { return self.value; } }\n"
        "impl Debug for File { fn debug(self: &mut File) -> i32 { return self.value + 1; } }\n"
        "fn main() -> i32 {\n"
        "  var file: File = File { value: 5 };\n"
        "  let view: &mut dyn (Debug + Draw) = &mut file;\n"
        "  return 0;\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.vtable_layouts.size(), 2U);
    ASSERT_EQ(checked.principal_set_composition_facts.identity_facts.size(), 1U);
    ASSERT_EQ(checked.principal_set_composition_facts.witness_sets.size(), 1U);
    ASSERT_EQ(checked.principal_set_composition_facts.projections.size(), 2U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.shared_borrow_projection_count, 0U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.mut_borrow_projection_count, 2U);
    EXPECT_TRUE(query::is_valid(checked.principal_set_composition_facts));
    const query::PrincipalSetIdentityFact& identity =
        checked.principal_set_composition_facts.identity_facts.front();
    ASSERT_EQ(identity.principals.size(), 2U);
    std::vector<std::string> canonical_principals{
        identity.principals[0].principal_name,
        identity.principals[1].principal_name,
    };
    std::vector<std::string> sorted_principals = canonical_principals;
    std::ranges::sort(sorted_principals);
    EXPECT_EQ(sorted_principals[0], "Debug");
    EXPECT_EQ(sorted_principals[1], "Draw");
    const std::string canonical_composition_display =
        "dyn (" + canonical_principals[0] + " + " + canonical_principals[1] + ")";

    for (const query::CompositionProjectionFact& projection :
        checked.principal_set_composition_facts.projections) {
        EXPECT_EQ(projection.borrow_kind, query::DynBorrowKind::mut);
        EXPECT_EQ(projection.target_view_name, canonical_composition_display);
    }
}

TEST(CoreUnit, DynTraitCompositionProjectsToExplicitPrincipalView)
{
    const std::string_view source =
        "module dyn_trait_composition_projection_whitebox;\n"
        "trait Draw { fn draw(self: &Self) -> i32; }\n"
        "trait Debug { fn debug(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Draw for File { fn draw(self: &File) -> i32 { return self.value; } }\n"
        "impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 1; } }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 7 };\n"
        "  let combo: &dyn (Debug + Draw) = &file;\n"
        "  let draw: &dyn Draw = combo;\n"
        "  let debug: &dyn Debug = combo;\n"
        "  return draw.draw() + debug.debug();\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.principal_set_composition_facts.identity_facts.size(), 1U);
    ASSERT_EQ(checked.principal_set_composition_facts.witness_sets.size(), 1U);
    ASSERT_EQ(checked.principal_set_composition_facts.projections.size(), 4U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.projection_count, 4U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.shared_borrow_projection_count, 4U);
    EXPECT_TRUE(query::is_valid(checked.principal_set_composition_facts));

    const query::PrincipalSetIdentityFact& identity =
        checked.principal_set_composition_facts.identity_facts.front();
    ASSERT_EQ(identity.principals.size(), 2U);
    const std::string canonical_composition_display =
        "dyn (" + identity.principals[0].principal_name + " + " + identity.principals[1].principal_name + ")";

    base::usize composition_to_principal_count = 0;
    std::vector<std::string> projected_targets;
    for (const query::CompositionProjectionFact& projection :
        checked.principal_set_composition_facts.projections) {
        if (projection.kind != query::PrincipalSetProjectionKind::composition_to_principal) {
            continue;
        }
        ++composition_to_principal_count;
        projected_targets.push_back(projection.target_view_name);
        EXPECT_EQ(projection.borrow_kind, query::DynBorrowKind::shared);
        EXPECT_EQ(projection.source_view_name, canonical_composition_display);
    }
    std::ranges::sort(projected_targets);
    ASSERT_EQ(composition_to_principal_count, 2U);
    ASSERT_EQ(projected_targets.size(), 2U);
    EXPECT_EQ(projected_targets[0], "dyn Debug");
    EXPECT_EQ(projected_targets[1], "dyn Draw");
}

TEST(CoreUnit, DynTraitCompositionMutableProjectionKeepsBorrowKind)
{
    const std::string_view source =
        "module dyn_trait_composition_mut_projection_whitebox;\n"
        "trait Draw { fn draw(self: &mut Self) -> i32; }\n"
        "trait Debug { fn debug(self: &mut Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Draw for File { fn draw(self: &mut File) -> i32 { return self.value; } }\n"
        "impl Debug for File { fn debug(self: &mut File) -> i32 { return self.value + 1; } }\n"
        "fn main() -> i32 {\n"
        "  var file: File = File { value: 7 };\n"
        "  let combo: &mut dyn (Debug + Draw) = &mut file;\n"
        "  let draw: &mut dyn Draw = combo;\n"
        "  return 0;\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.principal_set_composition_facts.projections.size(), 3U);
    base::usize mutable_projection_count = 0;
    for (const query::CompositionProjectionFact& projection :
        checked.principal_set_composition_facts.projections) {
        if (projection.kind == query::PrincipalSetProjectionKind::composition_to_principal) {
            ++mutable_projection_count;
            EXPECT_EQ(projection.borrow_kind, query::DynBorrowKind::mut);
            EXPECT_EQ(projection.target_view_name, "dyn Draw");
        }
    }
    EXPECT_EQ(mutable_projection_count, 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.mut_borrow_projection_count, 3U);
}

TEST(CoreUnit, DynTraitCompositionRejectsInvalidPrincipalSets)
{
    const std::vector<std::pair<std::string_view, std::string_view>> cases{
        {
            "module dyn_trait_composition_single_reject_whitebox;\n"
            "trait Draw { fn draw(self: &Self) -> i32; }\n"
            "fn render(view: &dyn (Draw)) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "dyn trait composition requires at least two principal traits",
        },
        {
            "module dyn_trait_composition_duplicate_reject_whitebox;\n"
            "trait Draw { fn draw(self: &Self) -> i32; }\n"
            "fn render(view: &dyn (Draw + Draw)) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "duplicate dyn trait composition principal `Draw`",
        },
        {
            "module dyn_trait_composition_non_trait_reject_whitebox;\n"
            "struct File { value: i32; }\n"
            "trait Draw { fn draw(self: &Self) -> i32; }\n"
            "fn render(view: &dyn (File + Draw)) -> i32 { return 0; }\n"
            "fn main() -> i32 { return 0; }\n",
            "dyn trait composition principal must be a dyn trait",
        },
        {
            "module dyn_trait_composition_missing_impl_reject_whitebox;\n"
            "trait Draw { fn draw(self: &Self) -> i32; }\n"
            "trait Debug { fn debug(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Draw for File { fn draw(self: &File) -> i32 { return self.value; } }\n"
            "fn main() -> i32 {\n"
            "  let file: File = File { value: 1 };\n"
            "  let view: &dyn (Draw + Debug) = &file;\n"
            "  return 0;\n"
            "}\n",
            "initializer type does not match declared type",
        },
        {
            "module dyn_trait_composition_shared_to_mut_reject_whitebox;\n"
            "trait Draw { fn draw(self: &Self) -> i32; }\n"
            "trait Debug { fn debug(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Draw for File { fn draw(self: &File) -> i32 { return self.value; } }\n"
            "impl Debug for File { fn debug(self: &File) -> i32 { return self.value; } }\n"
            "fn main() -> i32 {\n"
            "  let file: File = File { value: 1 };\n"
            "  let view: &mut dyn (Draw + Debug) = &file;\n"
            "  return 0;\n"
            "}\n",
            "initializer type does not match declared type",
        },
    };

    for (const auto& [source, diagnostic] : cases) {
        expect_dyn_trait_source_diagnostic(source, diagnostic);
    }
}

TEST(CoreUnit, DynTraitCompositionAssociatedEqualityMergeDetectsConflicts)
{
    expect_dyn_trait_source_diagnostic(
        "module dyn_trait_composition_assoc_conflict_whitebox;\n"
        "trait Source { type Item; fn item(self: &Self) -> Self.Item; }\n"
        "trait Sink { type Item; fn put(self: &Self, value: Self.Item) -> i32; }\n"
        "fn render(view: &dyn (Source[Item = i32] + Sink[Item = bool])) -> i32 { return 0; }\n"
        "fn main() -> i32 { return 0; }\n",
        "dyn trait composition associated type `Item` has conflicting equality constraints");

    const sema::CheckedModule checked = analyze_dyn_trait_source(
        "module dyn_trait_composition_assoc_merge_whitebox;\n"
        "trait Source { type Item; fn item(self: &Self) -> Self.Item; }\n"
        "trait Sink { type Item; fn put(self: &Self, value: Self.Item) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Source for File { type Item = i32; fn item(self: &File) -> i32 { return self.value; } }\n"
        "impl Sink for File { type Item = i32; fn put(self: &File, value: i32) -> i32 { return value; } }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 1 };\n"
        "  let view: &dyn (Sink[Item = i32] + Source[Item = i32]) = &file;\n"
        "  return 0;\n"
        "}\n");
    ASSERT_EQ(checked.principal_set_composition_facts.associated_equality_merges.size(), 1U);
    const query::AssociatedEqualityMergeFact& merge =
        checked.principal_set_composition_facts.associated_equality_merges.front();
    EXPECT_EQ(merge.associated_type_name, "Item");
    EXPECT_EQ(merge.merged_type_name, "i32");
    EXPECT_EQ(merge.status, query::PrincipalAssociatedEqualityMergeStatus::satisfied);
    ASSERT_EQ(merge.contributing_principals.size(), 2U);
    EXPECT_TRUE(query::is_valid(checked.principal_set_composition_facts));
}

TEST(CoreUnit, DynTraitCompositionDirectMethodCallsDispatchThroughUniquePrincipal)
{
    const std::string_view source =
        "module dyn_trait_composition_unique_method_call_whitebox;\n"
        "trait Draw { fn draw(self: &Self) -> i32; }\n"
        "trait Debug { fn debug(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Draw for File { fn draw(self: &File) -> i32 { return self.value; } }\n"
        "impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 1; } }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 1 };\n"
        "  let view: &dyn (Draw + Debug) = &file;\n"
        "  return view.draw();\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_method_calls.size(), 1U);
    const sema::TraitMethodCallBinding& call = checked.trait_method_calls.front();
    EXPECT_EQ(call.method_name.view(), "draw");
    EXPECT_EQ(call.dispatch, sema::TraitMethodDispatchKind::vtable_slot);
    EXPECT_EQ(call.vtable_slot, 0U);
    ASSERT_TRUE(sema::is_valid(call.dispatch_receiver_type));
    ASSERT_TRUE(checked.types.is_reference(call.receiver_type));
    EXPECT_EQ(checked.types.display_name(call.receiver_type), "&dyn (Debug + Draw)");
    EXPECT_EQ(checked.types.display_name(call.dispatch_receiver_type), "dyn Draw");

    ASSERT_EQ(checked.principal_set_composition_facts.projections.size(), 3U);
    base::usize composition_to_principal_count = 0;
    for (const query::CompositionProjectionFact& projection :
        checked.principal_set_composition_facts.projections) {
        if (projection.kind != query::PrincipalSetProjectionKind::composition_to_principal) {
            continue;
        }
        ++composition_to_principal_count;
        EXPECT_EQ(projection.borrow_kind, query::DynBorrowKind::shared);
        EXPECT_EQ(projection.target_view_name, "dyn Draw");
    }
    EXPECT_EQ(composition_to_principal_count, 1U);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait_call #0 vtable_slot dyn (Debug + Draw).draw -> i32",
            "dispatch_receiver=dyn Draw",
            "kind=composition_to_principal borrow=shared",
            "target=dyn Draw",
        });
}

TEST(CoreUnit, DynTraitCompositionDirectMutableMethodCallsKeepBorrowKind)
{
    const std::string_view source =
        "module dyn_trait_composition_unique_mut_method_call_whitebox;\n"
        "trait Draw { fn draw(self: &mut Self) -> i32; }\n"
        "trait Debug { fn debug(self: &mut Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Draw for File { fn draw(self: &mut File) -> i32 { return self.value; } }\n"
        "impl Debug for File { fn debug(self: &mut File) -> i32 { return self.value + 1; } }\n"
        "fn main() -> i32 {\n"
        "  var file: File = File { value: 1 };\n"
        "  let view: &mut dyn (Draw + Debug) = &mut file;\n"
        "  return view.draw();\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_method_calls.size(), 1U);
    const sema::TraitMethodCallBinding& call = checked.trait_method_calls.front();
    EXPECT_EQ(call.method_name.view(), "draw");
    EXPECT_EQ(call.dispatch, sema::TraitMethodDispatchKind::vtable_slot);
    EXPECT_EQ(checked.types.display_name(call.receiver_type), "&mut dyn (Debug + Draw)");
    EXPECT_EQ(checked.types.display_name(call.dispatch_receiver_type), "dyn Draw");
    EXPECT_EQ(call.receiver_access, sema::ReceiverAccessKind::mutable_);

    base::usize composition_to_principal_count = 0;
    for (const query::CompositionProjectionFact& projection :
        checked.principal_set_composition_facts.projections) {
        if (projection.kind != query::PrincipalSetProjectionKind::composition_to_principal) {
            continue;
        }
        ++composition_to_principal_count;
        EXPECT_EQ(projection.borrow_kind, query::DynBorrowKind::mut);
        EXPECT_EQ(projection.target_view_name, "dyn Draw");
    }
    EXPECT_EQ(composition_to_principal_count, 1U);
}

TEST(CoreUnit, DynTraitCompositionDirectAssociatedReturnUsesSelectedPrincipalEqualities)
{
    const std::string_view source =
        "module dyn_trait_composition_direct_assoc_return_whitebox;\n"
        "trait Source { type Item; fn item(self: &Self) -> Self.Item; }\n"
        "trait Debug { fn debug(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Source for File { type Item = i32; fn item(self: &File) -> i32 { return self.value; } }\n"
        "impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 1; } }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 9 };\n"
        "  let view: &dyn (Debug + Source[Item = i32]) = &file;\n"
        "  return view.item();\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_method_calls.size(), 1U);
    const sema::TraitMethodCallBinding& call = checked.trait_method_calls.front();
    EXPECT_EQ(call.method_name.view(), "item");
    EXPECT_EQ(call.dispatch, sema::TraitMethodDispatchKind::vtable_slot);
    const std::string receiver_display = checked.types.display_name(call.receiver_type);
    EXPECT_NE(receiver_display.find("&dyn ("), std::string::npos) << receiver_display;
    EXPECT_NE(receiver_display.find("Debug"), std::string::npos) << receiver_display;
    EXPECT_NE(receiver_display.find("Source[Item = i32]"), std::string::npos) << receiver_display;
    EXPECT_EQ(checked.types.display_name(call.dispatch_receiver_type), "dyn Source[Item = i32]");
    EXPECT_EQ(checked.types.display_name(call.return_type), "i32");
    EXPECT_EQ(call.receiver_access, sema::ReceiverAccessKind::shared);

    ASSERT_EQ(checked.principal_set_composition_facts.associated_equality_merges.size(), 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.associated_equality_merges.front().associated_type_name,
        "Item");
    base::usize direct_projection_count = 0;
    for (const query::CompositionProjectionFact& projection :
        checked.principal_set_composition_facts.projections) {
        if (projection.kind != query::PrincipalSetProjectionKind::composition_to_principal) {
            continue;
        }
        ++direct_projection_count;
        EXPECT_EQ(projection.target_view_name, "dyn Source[Item = i32]");
    }
    EXPECT_EQ(direct_projection_count, 1U);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait_call #0 vtable_slot dyn (",
            "Source[Item = i32]",
            ".item -> i32",
            "dispatch_receiver=dyn Source[Item = i32]",
            "receiver_access=shared auto_borrow=false two_phase=false",
        });
}

TEST(CoreUnit, DynTraitCompositionDirectAndExplicitProjectionSharePrincipalProjectionFact)
{
    const std::string_view source =
        "module dyn_trait_composition_direct_explicit_projection_whitebox;\n"
        "trait Draw { fn draw(self: &Self) -> i32; }\n"
        "trait Debug { fn debug(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Draw for File { fn draw(self: &File) -> i32 { return self.value; } }\n"
        "impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 1; } }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 3 };\n"
        "  let view: &dyn (Draw + Debug) = &file;\n"
        "  let draw: &dyn Draw = view;\n"
        "  return view.draw() + draw.draw();\n"
        "}\n";

    const sema::CheckedModule checked = analyze_dyn_trait_source(source);
    ASSERT_EQ(checked.trait_method_calls.size(), 2U);
    for (const sema::TraitMethodCallBinding& call : checked.trait_method_calls) {
        EXPECT_EQ(call.dispatch, sema::TraitMethodDispatchKind::vtable_slot);
        EXPECT_EQ(call.receiver_access, sema::ReceiverAccessKind::shared);
    }

    base::usize draw_projection_count = 0;
    for (const query::CompositionProjectionFact& projection :
        checked.principal_set_composition_facts.projections) {
        if (projection.kind == query::PrincipalSetProjectionKind::composition_to_principal
            && projection.target_view_name == "dyn Draw") {
            ++draw_projection_count;
        }
    }
    EXPECT_EQ(draw_projection_count, 1U);
    EXPECT_EQ(checked.principal_set_composition_facts.summary.projection_count, 3U);
    EXPECT_EQ(query::principal_set_composition_facts_fingerprint(checked.principal_set_composition_facts),
        checked.principal_set_composition_facts.fingerprint);
}

TEST(CoreUnit, DynTraitCompositionDirectMethodCallsRejectAmbiguousAndMissingNames)
{
    const std::vector<std::pair<std::string_view, std::string_view>> cases{
        {
            "module dyn_trait_composition_ambiguous_method_call_whitebox;\n"
            "trait Draw { fn run(self: &Self) -> i32; }\n"
            "trait Debug { fn run(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Draw for File { fn run(self: &File) -> i32 { return self.value; } }\n"
            "impl Debug for File { fn run(self: &File) -> i32 { return self.value + 1; } }\n"
            "fn main() -> i32 {\n"
            "  let file: File = File { value: 1 };\n"
            "  let view: &dyn (Draw + Debug) = &file;\n"
            "  return view.run();\n"
            "}\n",
            "dyn trait composition method `run` is ambiguous across multiple principal traits",
        },
        {
            "module dyn_trait_composition_missing_method_call_whitebox;\n"
            "trait Draw { fn draw(self: &Self) -> i32; }\n"
            "trait Debug { fn debug(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Draw for File { fn draw(self: &File) -> i32 { return self.value; } }\n"
            "impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 1; } }\n"
            "fn main() -> i32 {\n"
            "  let file: File = File { value: 1 };\n"
            "  let view: &dyn (Draw + Debug) = &file;\n"
            "  return view.missing();\n"
            "}\n",
            "has no visible impl for trait method `missing`",
        },
        {
            "module dyn_trait_composition_shared_mut_method_reject_whitebox;\n"
            "trait Draw { fn draw(self: &mut Self) -> i32; }\n"
            "trait Debug { fn debug(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Draw for File { fn draw(self: &mut File) -> i32 { return self.value; } }\n"
            "impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 1; } }\n"
            "fn main() -> i32 {\n"
            "  var file: File = File { value: 1 };\n"
            "  let view: &dyn (Draw + Debug) = &mut file;\n"
            "  return view.draw();\n"
            "}\n",
            "mutable method receiver requires mutable pointer",
        },
    };

    for (const auto& [source, diagnostic] : cases) {
        expect_dyn_trait_source_diagnostic(source, diagnostic);
    }
}

} // namespace
} // namespace aurex::test
