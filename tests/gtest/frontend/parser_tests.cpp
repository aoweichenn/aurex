#include "aurex/base/diagnostic.hpp"
#include "aurex/lex/lexer.hpp"
#include "aurex/parse/parser.hpp"
#include "aurex/syntax/ast_dump.hpp"
#include "support/test_support.hpp"

#include <string>
#include <string_view>

namespace aurex::test {
namespace {

using base::DiagnosticSink;

} // namespace

TEST(CoreUnit, ParserAndAstDumpCoverLowLevelSyntaxBranches) {
    constexpr std::string_view source =
        "module parser.dump;\n"
        "pub import c.host;\n"
        "extern c {\n"
        "  opaque struct Handle;\n"
        "  fn puts(s: *const u8) -> i32 @name(\"puts\");\n"
        "}\n"
        "export c fn exported(argc: i32, argv: *mut *mut u8) -> i32 @name(\"exported\") {\n"
        "  var i: i32 = 0;\n"
        "  while i < argc {\n"
        "    i = i + 1;\n"
        "    if i == 1 { continue; } else { break; }\n"
        "  }\n"
        "  let p: *mut i32 = ptr_from_addr(*mut i32, ptr_addr(argv));\n"
        "  let n: *const u8 = null;\n"
        "  let s: str = \"hello\";\n"
        "  let b: u8 = b'\\n';\n"
        "  let a: i32 = cast(i32, argc) + bit_cast(i32, argc) + align_of(*mut i32);\n"
        "  let q: *mut i32 = ptr_cast(*mut i32, p);\n"
        "  let idx: u8 = argv[0][0];\n"
        "  return a;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer({3}, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_TRUE(parsed) << parsed.error().message;
    EXPECT_FALSE(diagnostics.has_error());

    const std::string token_dump = syntax::dump_tokens(tokens.value());
    expect_contains_all(token_dump, {
        "kw_export",
        "kw_opaque",
        "kw_while",
        "kw_break",
        "kw_continue",
        "kw_null",
        "kw_ptr_cast",
        "kw_bit_cast",
        "kw_align_of",
        "kw_ptr_addr",
        "kw_ptr_from_addr",
        "byte_literal",
        "string_literal",
    });

    const std::string ast = syntax::dump_ast(parsed.value());
    expect_contains_all(ast, {
        "pub import c.host",
        "opaque_struct Handle extern_c",
        "fn exported export_c @name=exported",
        "stmt #",
        "while",
        "break",
        "continue",
        "expr #",
        "null_literal",
        "string_literal",
        "byte_literal",
        "index",
        "ptr_cast",
        "bit_cast",
        "align_of",
        "ptr_addr",
        "ptr_from_addr",
    });
}

TEST(CoreUnit, ParserCoversRecoveryNumericEnumValuesAndNestedGenericLookahead) {
    {
        DiagnosticSink diagnostics;
        constexpr std::string_view source =
            "module parser.recover;\n"
            "let top_level = 1;\n"
            "fn ok() -> i32 { return 0; }\n";
        lex::Lexer lexer({4}, source, diagnostics);
        auto tokens = lexer.tokenize();
        ASSERT_TRUE(tokens) << tokens.error().message;

        parse::Parser parser(tokens.value(), diagnostics);
        auto parsed = parser.parse_module();
        ASSERT_FALSE(parsed);
        ASSERT_TRUE(diagnostics.has_error());
        EXPECT_GE(diagnostics.diagnostics().size(), 1U);
        expect_contains(diagnostics.diagnostics().front().message, "expected item declaration");
    }

    {
        DiagnosticSink diagnostics;
        constexpr std::string_view source =
            "module parser.numeric;\n"
            "type VoidAlias = void;\n"
            "type BoolAlias = bool;\n"
            "type I8Alias = i8;\n"
            "type I16Alias = i16;\n"
            "type U16Alias = u16;\n"
            "type U32Alias = u32;\n"
            "type I64Alias = i64;\n"
            "type U64Alias = u64;\n"
            "type IsizeAlias = isize;\n"
            "type UsizeAlias = usize;\n"
            "type F32Alias = f32;\n"
            "type F64Alias = f64;\n"
            "type HexBytes = [0x2A]u8;\n"
            "type BinBytes = [0b1010]u8;\n"
            "type DecBytes = [1_000]u8;\n"
            "enum Code: u16 { hex = 0x2A, bin = 0b1010, dec = 1_000, }\n"
            "struct Wrap<T> { value: T; }\n"
            "enum Result<T, E>: u8 { ok(T) = 1, err(E) = 2, }\n"
            "fn main() -> i32 {\n"
            "  let nested: Wrap<Wrap<i32> > = Wrap<Wrap<i32> > { value: Wrap<i32> { value: 1 } };\n"
            "  let ok = Result<Wrap<i32>, bool>.err(false);\n"
            "  return 0;\n"
            "}\n";
        lex::Lexer lexer({5}, source, diagnostics);
        auto tokens = lexer.tokenize();
        ASSERT_TRUE(tokens) << tokens.error().message;

        parse::Parser parser(tokens.value(), diagnostics);
        auto parsed = parser.parse_module();
        ASSERT_TRUE(parsed) << parsed.error().message;
        EXPECT_FALSE(diagnostics.has_error());

        const std::string ast = syntax::dump_ast(parsed.value());
        expect_contains_all(ast, {
            "alias void",
            "alias bool",
            "alias i8",
            "alias i16",
            "alias u16",
            "alias u32",
            "alias i64",
            "alias u64",
            "alias isize",
            "alias usize",
            "alias f32",
            "alias f64",
            "alias [42]u8",
            "alias [10]u8",
            "alias [1000]u8",
            "struct_literal Wrap<Wrap<i32>>",
            "name `Result`<Wrap<i32>, bool>",
        });
    }
}

} // namespace aurex::test
