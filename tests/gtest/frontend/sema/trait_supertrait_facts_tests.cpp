#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <support/frontend_test_support.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

[[nodiscard]] syntax::AstModule parse_supertrait_sema_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer({641}, source, diagnostics);
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

[[nodiscard]] sema::CheckedModule analyze_supertrait_source(const std::string_view source)
{
    syntax::AstModule module = parse_supertrait_sema_source(source);
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

[[nodiscard]] std::string analyze_supertrait_source_failure(const std::string_view source)
{
    syntax::AstModule module = parse_supertrait_sema_source(source);
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

void expect_supertrait_source_diagnostic(
    const std::string_view source, const std::string_view diagnostic)
{
    const std::string output = analyze_supertrait_source_failure(source);
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

TEST(CoreUnit, TraitSupertraitFactsRecordDirectAndTransitiveEdges)
{
    const sema::CheckedModule checked = analyze_supertrait_source(
        "module trait_supertrait_edges_whitebox;\n"
        "trait Parent {}\n"
        "trait Mid: Parent {}\n"
        "trait Child: Mid {}\n"
        "fn main() -> i32 { return 0; }\n");

    ASSERT_EQ(checked.traits.size(), 3U);
    ASSERT_EQ(checked.trait_supertrait_edges.size(), 3U);
    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait_supertrait_edges 3",
            "supertrait_edge #",
            "Mid -> Parent ordinal=0 depth=1",
            "Child -> Mid ordinal=0 depth=1",
            "Child -> Parent ordinal=0 depth=2",
        });

    query::TypeCheckBodyAuthority authority;
    authority.checked_body = query::query_result_fingerprint(query::stable_fingerprint("supertrait.checked"));
    authority.body_syntax_result = query::query_result_fingerprint(query::stable_fingerprint("supertrait.body"));
    authority.signature_result = query::query_result_fingerprint(query::stable_fingerprint("supertrait.signature"));
    const std::optional<sema::FunctionLookupKey> main_key = find_function(checked, "main");
    ASSERT_TRUE(main_key.has_value());
    sema::populate_type_check_body_borrow_authority(authority, checked, *main_key);
    EXPECT_TRUE(authority.has_trait_object_facts);
    EXPECT_EQ(authority.trait_supertrait_edge_count, checked.trait_supertrait_edges.size());
}

TEST(CoreUnit, TraitSupertraitFactsInstantiateGenericTransitiveArgs)
{
    const sema::CheckedModule checked = analyze_supertrait_source(
        "module trait_supertrait_generic_edges_whitebox;\n"
        "trait Parent<T> {}\n"
        "trait Mid<T>: Parent<T> {}\n"
        "trait Child<T>: Mid<T> {}\n"
        "fn main() -> i32 { return 0; }\n");

    ASSERT_EQ(checked.trait_supertrait_edges.size(), 3U);
    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "Mid -> Parent ordinal=0 depth=1",
            "Child -> Mid ordinal=0 depth=1",
            "Child -> Parent ordinal=0 depth=2",
            "args=[T]",
        });

    sema::CheckedModule copied = checked;
    ASSERT_EQ(copied.trait_supertrait_edges.size(), 3U);
    EXPECT_EQ(copied.trait_supertrait_edges.front().child_trait_name,
        checked.trait_supertrait_edges.front().child_trait_name);
}

TEST(CoreUnit, TraitSupertraitImplRequiresDirectParentEvidence)
{
    expect_supertrait_source_diagnostic(
        "module trait_supertrait_impl_missing_parent_whitebox;\n"
        "trait Parent {}\n"
        "trait Child: Parent {}\n"
        "struct File { value: i32; }\n"
        "impl Child for File {}\n"
        "fn main() -> i32 { return 0; }\n",
        "impl `Child` requires supertrait evidence `Parent` for "
        "trait_supertrait_impl_missing_parent_whitebox.File");
}

TEST(CoreUnit, TraitSupertraitImplObligationSubstitutesGenericParentArgs)
{
    const sema::CheckedModule checked = analyze_supertrait_source(
        "module trait_supertrait_impl_generic_parent_whitebox;\n"
        "trait Parent<T> {}\n"
        "trait Child<T>: Parent<T> {}\n"
        "struct File { value: i32; }\n"
        "impl Parent<i32> for File {}\n"
        "impl Child<i32> for File {}\n"
        "fn main() -> i32 { return 0; }\n");

    ASSERT_EQ(checked.trait_impls.size(), 2U);
    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "impl Parent<i32> for trait_supertrait_impl_generic_parent_whitebox.File",
            "impl Child<i32> for trait_supertrait_impl_generic_parent_whitebox.File",
            "Child -> Parent ordinal=0 depth=1",
            "args=[T]",
        });

    expect_supertrait_source_diagnostic(
        "module trait_supertrait_impl_generic_mismatch_whitebox;\n"
        "trait Parent<T> {}\n"
        "trait Child<T>: Parent<T> {}\n"
        "struct File { value: i32; }\n"
        "impl Parent<bool> for File {}\n"
        "impl Child<i32> for File {}\n"
        "fn main() -> i32 { return 0; }\n",
        "impl `Child` requires supertrait evidence `Parent` for "
        "trait_supertrait_impl_generic_mismatch_whitebox.File");
}

TEST(CoreUnit, TraitSupertraitDiagnosticsRejectDuplicateAndCycles)
{
    expect_supertrait_source_diagnostic(
        "module trait_supertrait_duplicate_whitebox;\n"
        "trait Parent {}\n"
        "trait Child: Parent, Parent {}\n"
        "fn main() -> i32 { return 0; }\n",
        "duplicate supertrait `Parent` in trait `Child`");

    expect_supertrait_source_diagnostic(
        "module trait_supertrait_self_cycle_whitebox;\n"
        "trait Child: Child {}\n"
        "fn main() -> i32 { return 0; }\n",
        "supertrait cycle detected: trait `Child` inherits itself");

    expect_supertrait_source_diagnostic(
        "module trait_supertrait_indirect_cycle_whitebox;\n"
        "trait A: B {}\n"
        "trait B: A {}\n"
        "fn main() -> i32 { return 0; }\n",
        "supertrait cycle detected: trait `A` reaches itself");
}

TEST(CoreUnit, TraitSupertraitDiagnosticsRejectPublicPrivateLeak)
{
    expect_supertrait_source_diagnostic(
        "module trait_supertrait_visibility_leak_whitebox;\n"
        "trait Parent {}\n"
        "pub trait Child: Parent {}\n"
        "fn main() -> i32 { return 0; }\n",
        "public trait exposes private supertrait `Parent`");
}

} // namespace aurex::test
