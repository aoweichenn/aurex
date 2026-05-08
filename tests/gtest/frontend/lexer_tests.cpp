#include "aurex/base/diagnostic.hpp"
#include "aurex/lex/lexer.hpp"
#include "aurex/syntax/ast_dump.hpp"
#include "aurex/syntax/token.hpp"
#include "support/test_support.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::test {
namespace {

using base::DiagnosticSink;
using base::ErrorCode;
using syntax::Token;
using syntax::TokenKind;

std::vector<TokenKind> token_kinds(const std::vector<Token>& tokens) {
    std::vector<TokenKind> kinds;
    kinds.reserve(tokens.size());
    for (const Token& token : tokens) {
        kinds.push_back(token.kind);
    }
    return kinds;
}

} // namespace

TEST(CoreUnit, LexerCoversCommentsLiteralsOperatorsAndErrors) {
    DiagnosticSink diagnostics;
    constexpr std::string_view source =
        "// line comment\n"
        "module lex.unit; /* block comment */\n"
        "import common.text as text;\n"
        "const hex: i32 = 0x2A;\n"
        "const bin: i32 = 0b1010;\n"
        "const dec: i32 = 1_000;\n"
        "const s: str = \"hi\\\\n\";\n"
        "const c: *const u8 = c\"hi\\n\";\n"
        "const b: u8 = b'\\n';\n"
        "struct LexerProbe { value: i32; }\n"
        "impl LexerProbe { fn value(self: *const LexerProbe) -> i32 { return self.value; } }\n"
        "fn ops(a: i32, b: i32) -> i32 { return ((a / b) % 3) ^ (a << 1) >> 1 | ~b; }\n"
        "fn flags(flag: bool) -> void {\n"
        "  let i8_value: i8 = 1;\n"
        "  let i16_value: i16 = 2;\n"
        "  let u16_value: u16 = 3;\n"
        "  let u32_value: u32 = 4;\n"
        "  let i64_value: i64 = 5;\n"
        "  let u64_value: u64 = 6;\n"
        "  let isize_value: isize = 7;\n"
        "  let usize_value: usize = 8;\n"
        "  let f32_value: f32 = 9;\n"
        "  let f64_value: f64 = 10;\n"
        "  let span_value: text::SpanU8 = text::span_u8(c\"hi\", cast(usize, 2));\n"
        "  let moved_value: i32 = move(i8_value);\n"
        "  let try_token: i32 = 1?;\n"
        "  extern c fn printf(format: *const u8, ...) -> i32;\n"
        "  defer printf(c\"cleanup\");\n"
        "  for var index: i32 = 0; index < 1; index = index + 1 { }\n"
        "  if true && !false || flag { return; }\n"
        "  if i8_value != 0 && i16_value <= 2 && u16_value > 1 && u32_value >= 4 { return; }\n"
        "}\n";
    lex::Lexer lexer({1}, source, diagnostics);
    auto result = lexer.tokenize();
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_FALSE(diagnostics.has_error());

    const std::vector<TokenKind> kinds = token_kinds(result.value());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::integer_literal), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::string_literal), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::c_string_literal), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::byte_literal), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::slash), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::caret), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::less_less), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::greater_greater), kinds.end());
    EXPECT_EQ(kinds.back(), TokenKind::eof);

    const std::string token_dump = syntax::dump_tokens(result.value());
    expect_contains_all(token_dump, {
        "kw_true",
        "kw_false",
        "kw_as",
        "kw_void",
        "kw_bool",
        "kw_i8",
        "kw_i16",
        "kw_u16",
        "kw_u32",
        "kw_i64",
        "kw_u64",
        "kw_isize",
        "kw_usize",
        "kw_f32",
        "kw_f64",
        "kw_impl",
        "kw_defer",
        "kw_for",
        "kw_move",
        "slash",
        "percent",
        "pipe",
        "caret",
        "tilde",
        "bang",
        "bang_equal",
        "less_equal",
        "greater",
        "greater_equal",
        "less_less",
        "greater_greater",
        "amp_amp",
        "pipe_pipe",
        "question",
        "colon_colon",
        "ellipsis",
    });

    DiagnosticSink invalid_diagnostics;
    lex::Lexer invalid({2}, "@#$ \"unterminated\n c\"unterminated\n b'wide' /* unterminated", invalid_diagnostics);
    auto invalid_result = invalid.tokenize();
    ASSERT_FALSE(invalid_result);
    EXPECT_EQ(invalid_result.error().code, ErrorCode::lex_error);
    ASSERT_TRUE(invalid_diagnostics.has_error());
    EXPECT_GE(invalid_diagnostics.diagnostics().size(), 4U);
}

TEST(CoreUnit, LexerValidatesStringLiteralEscapesUtf8AndCStringNul) {
    DiagnosticSink good_diagnostics;
    constexpr std::string_view good_source =
        "module string.lex;\n"
        "const text: str = \"hi \\u{03A9}\\0\";\n"
        "const c_text: *const u8 = c\"hi \\u{03A9}\";\n";
    lex::Lexer good({3}, good_source, good_diagnostics);
    auto good_result = good.tokenize();
    ASSERT_TRUE(good_result) << good_result.error().message;
    EXPECT_FALSE(good_diagnostics.has_error());

    auto expect_lex_error = [](const std::string_view source, const std::string_view needle) {
        DiagnosticSink diagnostics;
        lex::Lexer lexer({4}, source, diagnostics);
        auto result = lexer.tokenize();
        ASSERT_FALSE(result);
        std::string messages;
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            messages += diagnostic.message;
            messages.push_back('\n');
        }
        expect_contains(messages, needle);
    };

    expect_lex_error("const text: str = \"\\q\";", "invalid escape sequence");
    expect_lex_error("const text: str = \"\\u{D800}\";", "not a valid Unicode scalar");
    expect_lex_error("const text: str = \"\\u{}\";", "unicode escape has no digits");
    expect_lex_error("const text: *const u8 = c\"a\\0b\";", "interior NUL");

    std::string invalid_utf8 = "const text: str = \"";
    invalid_utf8.push_back(static_cast<char>(0xC3));
    invalid_utf8.push_back('(');
    invalid_utf8 += "\";";
    expect_lex_error(invalid_utf8, "valid UTF-8");
}

} // namespace aurex::test
