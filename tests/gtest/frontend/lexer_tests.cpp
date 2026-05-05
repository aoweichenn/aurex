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
        "import std.core.text as text;\n"
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
        "  let try_token: i32 = 1?;\n"
        "  extern c fn printf(format: *const u8, ...) -> i32;\n"
        "  defer printf(c\"cleanup\");\n"
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

} // namespace aurex::test
