#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/checked_dyn_abi_facts.hpp>
#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/query/dyn_abi_facts.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <support/frontend_test_support.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

[[nodiscard]] syntax::AstModule parse_dyn_upcast_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer({651}, source, diagnostics);
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

[[nodiscard]] sema::CheckedModule analyze_dyn_upcast_source(const std::string_view source)
{
    syntax::AstModule module = parse_dyn_upcast_source(source);
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

[[nodiscard]] std::string analyze_dyn_upcast_source_failure(const std::string_view source)
{
    syntax::AstModule module = parse_dyn_upcast_source(source);
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

void expect_dyn_upcast_diagnostic(const std::string_view source, const std::string_view diagnostic)
{
    const std::string output = analyze_dyn_upcast_source_failure(source);
    EXPECT_NE(output.find(diagnostic), std::string::npos)
        << "missing diagnostic: " << diagnostic << "\n" << output;
}

[[nodiscard]] std::optional<sema::FunctionLookupKey> find_function(
    const sema::CheckedModule& checked, const std::string_view name)
{
    for (const auto& entry : checked.functions) {
        if (entry.second.name.view() == name) {
            return entry.first;
        }
    }
    return std::nullopt;
}

} // namespace

TEST(CoreUnit, DynTraitUpcastRecordsBorrowedSharedFactAndAbiDescriptor)
{
    const sema::CheckedModule checked = analyze_dyn_upcast_source(
        "module dyn_trait_upcast_shared_whitebox;\n"
        "trait Parent { fn parent(self: &Self) -> i32; }\n"
        "trait Child: Parent { fn child(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Parent for File { fn parent(self: &File) -> i32 { return self.value; } }\n"
        "impl Child for File { fn child(self: &File) -> i32 { return self.value + 1; } }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 7 };\n"
        "  let child: &dyn Child = &file;\n"
        "  let parent: &dyn Parent = child;\n"
        "  return 0;\n"
        "}\n");

    ASSERT_EQ(checked.trait_object_coercions.size(), 1U);
    ASSERT_EQ(checked.trait_object_upcast_coercions.size(), 1U);
    const sema::TraitObjectUpcastCoercionFact& upcast = checked.trait_object_upcast_coercions.front();
    EXPECT_EQ(checked.types.display_name(upcast.source_reference_type), "&dyn Child");
    EXPECT_EQ(checked.types.display_name(upcast.target_reference_type), "&dyn Parent");
    EXPECT_EQ(checked.types.display_name(upcast.source_object_type), "dyn Child");
    EXPECT_EQ(checked.types.display_name(upcast.target_object_type), "dyn Parent");
    EXPECT_EQ(upcast.borrow_kind, query::TraitObjectBorrowKindKey::shared);
    EXPECT_TRUE(query::is_valid(upcast.upcast_key));
    EXPECT_NE(upcast.edge_fingerprint.byte_count, 0U);

    const query::FunctionDynAbiFacts facts = sema::checked_dyn_abi_facts(checked);
    ASSERT_EQ(facts.upcasts.size(), 1U);
    EXPECT_TRUE(query::is_valid(facts));
    EXPECT_EQ(facts.summary.upcast_count, 1U);
    EXPECT_EQ(facts.summary.shared_borrow_count, 2U);
    EXPECT_EQ(facts.upcasts.front().metadata_policy, query::DynMetadataPolicy::supertrait_vptr_metadata_v1);
    EXPECT_EQ(facts.upcasts.front().borrow_kind, query::DynBorrowKind::shared);
    EXPECT_EQ(facts.upcasts.front().source_object_type_name, "dyn Child");
    EXPECT_EQ(facts.upcasts.front().target_object_type_name, "dyn Parent");

    const std::string checked_dump = sema::dump_checked_module(checked);
    expect_contains_all(checked_dump,
        {
            "trait_supertrait_edges 1",
            "Child -> Parent ordinal=0 depth=1",
            "trait_object_upcast_coercions 1",
            "dyn_upcast #0",
            "&dyn Child -> &dyn Parent",
            "borrow=shared",
        });
    const std::string abi_dump = query::dump_function_dyn_abi_facts(facts);
    expect_contains_all(abi_dump,
        {
            "dyn_upcast #0",
            "&dyn Child -> &dyn Parent",
            "metadata=supertrait_vptr_metadata_v1",
        });

    query::TypeCheckBodyAuthority authority;
    authority.checked_body = query::query_result_fingerprint(query::stable_fingerprint("upcast.checked"));
    authority.body_syntax_result = query::query_result_fingerprint(query::stable_fingerprint("upcast.body"));
    authority.signature_result = query::query_result_fingerprint(query::stable_fingerprint("upcast.signature"));
    const std::optional<sema::FunctionLookupKey> main_key = find_function(checked, "main");
    ASSERT_TRUE(main_key.has_value());
    sema::populate_type_check_body_borrow_authority(authority, checked, *main_key);
    EXPECT_TRUE(authority.has_trait_object_facts);
    EXPECT_EQ(authority.trait_object_upcast_coercion_count, 1U);
}

TEST(CoreUnit, DynTraitUpcastPreservesMutableBorrowKindForMutableTarget)
{
    const sema::CheckedModule checked = analyze_dyn_upcast_source(
        "module dyn_trait_upcast_mut_whitebox;\n"
        "trait Parent { fn parent(self: &mut Self) -> i32; }\n"
        "trait Child: Parent { fn child(self: &mut Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Parent for File { fn parent(self: &mut File) -> i32 { return self.value; } }\n"
        "impl Child for File { fn child(self: &mut File) -> i32 { return self.value + 1; } }\n"
        "fn main() -> i32 {\n"
        "  var file: File = File { value: 7 };\n"
        "  let child: &mut dyn Child = &mut file;\n"
        "  let parent: &mut dyn Parent = child;\n"
        "  return 0;\n"
        "}\n");

    ASSERT_EQ(checked.trait_object_upcast_coercions.size(), 1U);
    const sema::TraitObjectUpcastCoercionFact& upcast = checked.trait_object_upcast_coercions.front();
    EXPECT_EQ(checked.types.display_name(upcast.source_reference_type), "&mut dyn Child");
    EXPECT_EQ(checked.types.display_name(upcast.target_reference_type), "&mut dyn Parent");
    EXPECT_EQ(upcast.borrow_kind, query::TraitObjectBorrowKindKey::mut);

    const query::FunctionDynAbiFacts facts = sema::checked_dyn_abi_facts(checked);
    ASSERT_EQ(facts.upcasts.size(), 1U);
    EXPECT_EQ(facts.upcasts.front().borrow_kind, query::DynBorrowKind::mut);
    EXPECT_EQ(facts.summary.mut_borrow_count, 2U);
}

TEST(CoreUnit, DynTraitUpcastAllowsMutableSourceToSharedParentView)
{
    const sema::CheckedModule checked = analyze_dyn_upcast_source(
        "module dyn_trait_upcast_mut_to_shared_whitebox;\n"
        "trait Parent { fn parent(self: &Self) -> i32; }\n"
        "trait Child: Parent { fn child(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Parent for File { fn parent(self: &File) -> i32 { return self.value; } }\n"
        "impl Child for File { fn child(self: &File) -> i32 { return self.value + 1; } }\n"
        "fn main() -> i32 {\n"
        "  var file: File = File { value: 7 };\n"
        "  let child: &mut dyn Child = &mut file;\n"
        "  let parent: &dyn Parent = child;\n"
        "  return 0;\n"
        "}\n");

    ASSERT_EQ(checked.trait_object_upcast_coercions.size(), 1U);
    EXPECT_EQ(checked.trait_object_upcast_coercions.front().borrow_kind,
        query::TraitObjectBorrowKindKey::shared);
    EXPECT_EQ(checked.types.display_name(checked.trait_object_upcast_coercions.front().target_reference_type),
        "&dyn Parent");
}

TEST(CoreUnit, DynTraitUpcastRecordsTransitiveGenericPath)
{
    const sema::CheckedModule checked = analyze_dyn_upcast_source(
        "module dyn_trait_upcast_generic_transitive_whitebox;\n"
        "trait Parent<T> { fn parent(self: &Self, value: T) -> T; }\n"
        "trait Mid<T>: Parent<T> { fn mid(self: &Self) -> i32; }\n"
        "trait Child<T>: Mid<T> { fn child(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Parent<i32> for File { fn parent(self: &File, value: i32) -> i32 { return value; } }\n"
        "impl Mid<i32> for File { fn mid(self: &File) -> i32 { return self.value; } }\n"
        "impl Child<i32> for File { fn child(self: &File) -> i32 { return self.value + 1; } }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 7 };\n"
        "  let child: &dyn Child<i32> = &file;\n"
        "  let parent: &dyn Parent<i32> = child;\n"
        "  return 0;\n"
        "}\n");

    ASSERT_EQ(checked.trait_object_upcast_coercions.size(), 1U);
    const sema::TraitObjectUpcastCoercionFact& upcast = checked.trait_object_upcast_coercions.front();
    EXPECT_EQ(checked.types.display_name(upcast.source_reference_type), "&dyn Child<i32>");
    EXPECT_EQ(checked.types.display_name(upcast.target_reference_type), "&dyn Parent<i32>");
    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "Child -> Parent ordinal=0 depth=2",
            "args=[T]",
            "&dyn Child<i32> -> &dyn Parent<i32>",
        });
}

TEST(CoreUnit, DynTraitUpcastRejectsInvalidBorrowAndNonSupertraitTargets)
{
    expect_dyn_upcast_diagnostic(
        "module dyn_trait_upcast_shared_to_mut_reject_whitebox;\n"
        "trait Parent { fn parent(self: &mut Self) -> i32; }\n"
        "trait Child: Parent { fn child(self: &mut Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Parent for File { fn parent(self: &mut File) -> i32 { return self.value; } }\n"
        "impl Child for File { fn child(self: &mut File) -> i32 { return self.value + 1; } }\n"
        "fn main() -> i32 {\n"
        "  var file: File = File { value: 7 };\n"
        "  let child: &dyn Child = &mut file;\n"
        "  let parent: &mut dyn Parent = child;\n"
        "  return 0;\n"
        "}\n",
        "initializer type does not match declared type");

    expect_dyn_upcast_diagnostic(
        "module dyn_trait_upcast_not_supertrait_reject_whitebox;\n"
        "trait Parent {}\n"
        "trait Other {}\n"
        "trait Child: Parent {}\n"
        "struct File { value: i32; }\n"
        "impl Parent for File {}\n"
        "impl Other for File {}\n"
        "impl Child for File {}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 7 };\n"
        "  let child: &dyn Child = &file;\n"
        "  let other: &dyn Other = child;\n"
        "  return 0;\n"
        "}\n",
        "initializer type does not match declared type");
}

TEST(CoreUnit, DynTraitUpcastRejectsGenericTargetMismatch)
{
    expect_dyn_upcast_diagnostic(
        "module dyn_trait_upcast_generic_mismatch_reject_whitebox;\n"
        "trait Parent<T> { fn parent(self: &Self, value: T) -> T; }\n"
        "trait Child<T>: Parent<T> { fn child(self: &Self) -> i32; }\n"
        "struct File { value: i32; }\n"
        "impl Parent<i32> for File { fn parent(self: &File, value: i32) -> i32 { return value; } }\n"
        "impl Child<i32> for File { fn child(self: &File) -> i32 { return self.value; } }\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 7 };\n"
        "  let child: &dyn Child<i32> = &file;\n"
        "  let parent: &dyn Parent<bool> = child;\n"
        "  return 0;\n"
        "}\n",
        "initializer type does not match declared type");
}

} // namespace aurex::test
