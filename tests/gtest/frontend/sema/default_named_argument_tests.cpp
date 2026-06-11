#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/frontend/sema/sema_messages.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>

#include <algorithm>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::SourceId DEFAULT_NAMED_ARGUMENT_TEST_SOURCE_ID{212};

[[nodiscard]] bool contains_text(const std::string_view text, const std::string_view needle) noexcept
{
    return text.find(needle) != std::string_view::npos;
}

[[nodiscard]] syntax::AstModule parse_default_named_argument_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(DEFAULT_NAMED_ARGUMENT_TEST_SOURCE_ID, source, diagnostics);
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

[[nodiscard]] sema::CheckedModule analyze_default_named_argument_source(const std::string_view source)
{
    syntax::AstModule module = parse_default_named_argument_source(source);
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

[[nodiscard]] std::string analyze_default_named_argument_failure(const std::string_view source)
{
    syntax::AstModule module = parse_default_named_argument_source(source);
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
    } else {
        output += result.error().message;
        output += '\n';
    }
    return output;
}

void expect_default_named_argument_diagnostic(
    const std::string_view source, const std::string_view diagnostic)
{
    const std::string output = analyze_default_named_argument_failure(source);
    EXPECT_TRUE(contains_text(output, diagnostic)) << "missing diagnostic: " << diagnostic << "\n" << output;
}

void expect_default_named_argument_diagnostic_without(
    const std::string_view source, const std::string_view diagnostic, const std::string_view absent)
{
    const std::string output = analyze_default_named_argument_failure(source);
    EXPECT_TRUE(contains_text(output, diagnostic)) << "missing diagnostic: " << diagnostic << "\n" << output;
    EXPECT_FALSE(contains_text(output, absent)) << "unexpected diagnostic: " << absent << "\n" << output;
}

} // namespace

TEST(CoreUnit, DefaultNamedArgumentSemaRecordsParameterDefaultsAndOrderedBindings)
{
    constexpr std::string_view source =
        "module default_named_argument.checked;\n"
        "fn mix(a: i32, b: i32 = 10, c: i32 = 100) -> i32 { return a + b + c; }\n"
        "struct Acc { base: i32; }\n"
        "impl Acc {\n"
        "  fn add(self: &Acc, value: i32, scale: i32 = 1) -> i32 {\n"
        "    return self.base + value * scale;\n"
        "  }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let a: Acc = Acc { base: 5 };\n"
        "  let direct: i32 = mix(a: 1, c: 3);\n"
        "  let method: i32 = a.add(scale: 3, value: 2);\n"
        "  return direct + method;\n"
        "}\n";

    const sema::CheckedModule checked = analyze_default_named_argument_source(source);
    const std::string dump = sema::dump_checked_module(checked);
    EXPECT_TRUE(contains_text(dump, "mix -> i32"));
    EXPECT_TRUE(contains_text(dump, "params=[a:i32, b:i32=e"));
    EXPECT_TRUE(contains_text(dump, ", c:i32=e"));
    EXPECT_TRUE(contains_text(dump, "method default_named_argument.checked.Acc.add -> i32"));
    EXPECT_TRUE(contains_text(dump, "params=[self:&"));
    EXPECT_TRUE(contains_text(dump, "value:i32, scale:i32=e"));
    EXPECT_TRUE(contains_text(dump, "function_call #"));
    EXPECT_TRUE(contains_text(dump, "ordered_args=["));
    EXPECT_GE(checked.function_calls.size(), 2U);
    EXPECT_TRUE(std::any_of(checked.function_calls.begin(), checked.function_calls.end(),
        [](const sema::FunctionCallBinding& binding) {
        return binding.ordered_args.size() == 3U && binding.receiver_arg_count == 0U;
    }));
    EXPECT_TRUE(std::any_of(checked.function_calls.begin(), checked.function_calls.end(),
        [](const sema::FunctionCallBinding& binding) {
        return binding.ordered_args.size() == 2U && binding.receiver_arg_count == 1U;
    }));
}

TEST(CoreUnit, DefaultNamedArgumentSemaSupportsGenericCallNormalization)
{
    constexpr std::string_view source =
        "module default_named_argument.generic;\n"
        "fn choose[T](value: T, amount: i32 = 2) -> i32 { return amount; }\n"
        "fn main() -> i32 {\n"
        "  return choose[i32](value: 3);\n"
        "}\n";

    const sema::CheckedModule checked = analyze_default_named_argument_source(source);
    EXPECT_TRUE(std::any_of(checked.function_calls.begin(), checked.function_calls.end(),
        [](const sema::FunctionCallBinding& binding) {
        return binding.ordered_args.size() == 2U;
    }));
}

TEST(CoreUnit, DefaultNamedArgumentSemaSupportsTraitMethodNamedArguments)
{
    constexpr std::string_view source =
        "module default_named_argument.trait_method;\n"
        "trait Reader {\n"
        "  fn read(self: &Self, value: i32, scale: i32) -> i32;\n"
        "}\n"
        "struct File { base: i32; }\n"
        "impl Reader for File {\n"
        "  fn read(self: &File, value: i32, scale: i32) -> i32 {\n"
        "    return self.base + value * scale;\n"
        "  }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { base: 5 };\n"
        "  return file.read(scale: 3, value: 2);\n"
        "}\n";

    const sema::CheckedModule checked = analyze_default_named_argument_source(source);
    EXPECT_TRUE(std::any_of(checked.trait_method_calls.begin(), checked.trait_method_calls.end(),
        [](const sema::TraitMethodCallBinding& binding) {
        return binding.method_name == "read" && binding.ordered_args.size() == 2U
            && binding.receiver_access == sema::ReceiverAccessKind::shared;
    }));
}

TEST(CoreUnit, DefaultNamedArgumentSemaRejectsInvalidDeclarations)
{
    expect_default_named_argument_diagnostic(
        "module default_named_argument.required_after_default;\n"
        "fn bad(a: i32 = 1, b: i32) -> i32 { return b; }\n"
        "fn main() -> i32 { return 0; }\n",
        sema::SEMA_DEFAULT_PARAMETER_AFTER_REQUIRED);

    expect_default_named_argument_diagnostic(
        "module default_named_argument.extern_default;\n"
        "extern c { fn bad(a: i32 = 1) -> i32; }\n"
        "fn main() -> i32 { return 0; }\n",
        sema::SEMA_DEFAULT_PARAMETER_C_ABI);

    expect_default_named_argument_diagnostic(
        "module default_named_argument.variadic_default;\n"
        "extern c { fn bad(fmt: *const u8 = null, ...) -> i32; }\n"
        "fn main() -> i32 { return 0; }\n",
        sema::SEMA_DEFAULT_PARAMETER_C_ABI);

    expect_default_named_argument_diagnostic(
        "module default_named_argument.trait_default;\n"
        "trait Reader { fn read(self: &Self, value: i32 = 1) -> i32; }\n"
        "fn main() -> i32 { return 0; }\n",
        sema::SEMA_DEFAULT_PARAMETER_TRAIT_UNSUPPORTED);
}

TEST(CoreUnit, DefaultNamedArgumentSemaRejectsInvalidCalls)
{
    expect_default_named_argument_diagnostic(
        "module default_named_argument.unknown_name;\n"
        "fn f(a: i32) -> i32 { return a; }\n"
        "fn main() -> i32 { return f(x: 1); }\n",
        "unknown named argument `x` in call to f");

    expect_default_named_argument_diagnostic(
        "module default_named_argument.duplicate_name;\n"
        "fn f(a: i32) -> i32 { return a; }\n"
        "fn main() -> i32 { return f(a: 1, a: 2); }\n",
        "duplicate named argument `a`");

    expect_default_named_argument_diagnostic(
        "module default_named_argument.positional_after_named;\n"
        "fn f(a: i32, b: i32) -> i32 { return a + b; }\n"
        "fn main() -> i32 { return f(a: 1, 2); }\n",
        sema::SEMA_POSITIONAL_ARGUMENT_AFTER_NAMED);

    expect_default_named_argument_diagnostic(
        "module default_named_argument.missing_required;\n"
        "fn f(a: i32, b: i32 = 1) -> i32 { return a + b; }\n"
        "fn main() -> i32 { return f(b: 2); }\n",
        "missing required argument `a` in call to f");

    expect_default_named_argument_diagnostic(
        "module default_named_argument.named_enum;\n"
        "enum Choice { some(i32) }\n"
        "fn main() -> i32 { let value: Choice = Choice.some(value: 1); return 0; }\n",
        sema::SEMA_NAMED_ARGUMENT_NOT_SUPPORTED);

    expect_default_named_argument_diagnostic_without(
        "module default_named_argument.named_function_value;\n"
        "fn add(a: i32, b: i32) -> i32 { return a + b; }\n"
        "fn main() -> i32 { let op: fn(i32, i32) -> i32 = add; return op(a: 1, b: 2); }\n",
        sema::SEMA_NAMED_ARGUMENT_NOT_SUPPORTED, "unknown named argument");

    expect_default_named_argument_diagnostic(
        "module default_named_argument.named_variadic;\n"
        "extern c { fn printf(fmt: *const u8, ...) -> i32; }\n"
        "fn main() -> i32 { return printf(fmt: null); }\n",
        sema::SEMA_NAMED_ARGUMENT_NOT_SUPPORTED);
}

TEST(CoreUnit, DefaultNamedArgumentBorrowFactsUseOrderedCallArguments)
{
    expect_default_named_argument_diagnostic(
        "module default_named_argument.borrow_order;\n"
        "@borrow(return = [right])\n"
        "fn choose(left: &i32, right: &i32) -> &i32 { return right; }\n"
        "fn main() -> void {\n"
        "  var left: i32 = 1;\n"
        "  var right: i32 = 2;\n"
        "  let view: &i32 = choose(right: &right, left: &left);\n"
        "  right = 3;\n"
        "  let _: i32 = *view;\n"
        "}\n",
        sema::SEMA_ACTIVE_BORROW_CONFLICT);
}

} // namespace aurex::test
