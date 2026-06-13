#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/syntax/core/ast_dump.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>

#include <support/frontend_test_support.hpp>

#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

[[nodiscard]] bool diagnostics_contain(
    const base::DiagnosticSink& diagnostics, const std::string_view message)
{
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        if (diagnostic.message.find(message) != std::string::npos) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] syntax::AstModule parse_supertrait_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer({631}, source, diagnostics);
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

void expect_supertrait_parse_diagnostic(
    const std::string_view source, const std::string_view diagnostic)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer({632}, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    static_cast<void>(parser.parse_module());
    ASSERT_TRUE(diagnostics.has_error());
    EXPECT_TRUE(diagnostics_contain(diagnostics, diagnostic))
        << "missing diagnostic: " << diagnostic;
}

[[nodiscard]] const syntax::ItemNode* find_item(
    const syntax::AstModule& module, const std::string_view name) noexcept
{
    for (base::usize index = 0; index < module.items.size(); ++index) {
        const syntax::ItemNode* const item = module.items.ptr(index);
        if (item != nullptr && item->name == name) {
            return item;
        }
    }
    return nullptr;
}

} // namespace

TEST(CoreUnit, TraitSupertraitParserRecordsDirectList)
{
    const syntax::AstModule module = parse_supertrait_source(
        "module trait_supertrait_parse_direct;\n"
        "trait Parent {}\n"
        "trait Other {}\n"
        "trait Child: Parent, Other {}\n");

    const syntax::ItemNode* const child = find_item(module, "Child");
    ASSERT_NE(child, nullptr);
    ASSERT_EQ(child->trait_supertraits.size(), 2U);
    EXPECT_EQ(child->trait_supertraits[0].ordinal, 0U);
    EXPECT_EQ(child->trait_supertraits[1].ordinal, 1U);
    ASSERT_TRUE(syntax::is_valid(child->trait_supertraits[0].trait_type));
    ASSERT_TRUE(syntax::is_valid(child->trait_supertraits[1].trait_type));
    EXPECT_EQ(module.types[child->trait_supertraits[0].trait_type.value].name, "Parent");
    EXPECT_EQ(module.types[child->trait_supertraits[1].trait_type.value].name, "Other");

    const std::string ast = syntax::dump_ast(module);
    expect_contains(ast, "item #");
    expect_contains(ast, "trait Child : Parent, Other");
}

TEST(CoreUnit, TraitSupertraitParserKeepsSupertraitsBeforeWhereConstraints)
{
    const syntax::AstModule module = parse_supertrait_source(
        "module trait_supertrait_parse_generic;\n"
        "trait Sized {}\n"
        "trait Parent<T> {}\n"
        "trait Child<T>: Parent<T> where T: Sized {}\n");

    const syntax::ItemNode* const child = find_item(module, "Child");
    ASSERT_NE(child, nullptr);
    ASSERT_EQ(child->trait_supertraits.size(), 1U);
    ASSERT_EQ(child->where_constraints.size(), 1U);
    const syntax::TypeNode& parent = module.types[child->trait_supertraits.front().trait_type.value];
    EXPECT_EQ(parent.name, "Parent");
    ASSERT_EQ(parent.type_args.size(), 1U);
    EXPECT_EQ(module.types[parent.type_args.front().value].name, "T");

    const std::string ast = syntax::dump_ast(module);
    expect_contains(ast, "trait Child<T> : Parent<T>");
    expect_contains(ast, "where T: Sized");
}

TEST(CoreUnit, TraitSupertraitParserReportsMissingSupertraitName)
{
    expect_supertrait_parse_diagnostic(
        "module trait_supertrait_parse_missing;\n"
        "trait Child: {}\n",
        "expected supertrait name after ':'");
}

TEST(CoreUnit, TraitSupertraitParserReportsMissingSeparator)
{
    expect_supertrait_parse_diagnostic(
        "module trait_supertrait_parse_separator;\n"
        "trait Parent {}\n"
        "trait Other {}\n"
        "trait Child: Parent Other {}\n",
        "expected ',' or trait body after supertrait");
}

} // namespace aurex::test
