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

TEST(CoreUnit, DynTraitCompositionMethodCallsRequirePrincipalSelection)
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
            "dyn trait composition method `run` is ambiguous; principal-qualified dispatch is not part of this stage",
        },
        {
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
            "}\n",
            "dyn trait composition method `draw` is ambiguous; principal-qualified dispatch is not part of this stage",
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
    };

    for (const auto& [source, diagnostic] : cases) {
        expect_dyn_trait_source_diagnostic(source, diagnostic);
    }
}

} // namespace
} // namespace aurex::test
