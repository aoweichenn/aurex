#include <aurex/base/diagnostic.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/syntax/ast_dump.hpp>
#include <aurex/syntax/token.hpp>

#include <gtest/gtest.h>

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

void expect_contains(const std::string_view text, const std::string_view needle) {
    EXPECT_NE(text.find(needle), std::string_view::npos)
        << "expected text to contain: " << needle << "\nactual text:\n" << text;
}

void expect_contains_all(const std::string_view text, const std::vector<std::string_view>& needles) {
    for (const std::string_view needle : needles) {
        expect_contains(text, needle);
    }
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
        "const flt: f64 = 1.25e+2;\n"
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
        "  let span_value: text::SpanU8 = text::span_u8(c\"hi\", cast[usize](2));\n"
        "  let copied_value: i32 = i8_value;\n"
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
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::float_literal), kinds.end());
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
        "float_literal",
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

TEST(CoreUnit, LexerTokenizesEmptySourceToEofOnly) {
    DiagnosticSink diagnostics;
    lex::Lexer lexer({13}, "", diagnostics);
    auto result = lexer.tokenize();
    ASSERT_TRUE(result) << result.error().message;
    ASSERT_EQ(result.value().size(), 1U);
    EXPECT_FALSE(diagnostics.has_error());
    EXPECT_EQ(result.value().front().kind, TokenKind::eof);
    EXPECT_TRUE(result.value().front().text.empty());
    EXPECT_EQ(result.value().front().range.begin, 0U);
    EXPECT_EQ(result.value().front().range.end, 0U);
}

TEST(CoreUnit, LexerRecognizesEveryKeyword) {
    DiagnosticSink diagnostics;
    constexpr std::string_view source =
        "module import as pub priv extern export c fn struct opaque enum const type impl match "
        "let var if else for in while break continue defer return true false null "
        "void bool i8 u8 i16 u16 i32 u32 i64 u64 isize usize f32 f64 str mut cast "
        "ptrcast bitcast sizeof alignof ptraddr ptrat strptr strblen strraw";
    lex::Lexer lexer({8}, source, diagnostics);
    auto result = lexer.tokenize();
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_FALSE(diagnostics.has_error());

    const std::vector<TokenKind> expected {
        TokenKind::kw_module,
        TokenKind::kw_import,
        TokenKind::kw_as,
        TokenKind::kw_pub,
        TokenKind::kw_priv,
        TokenKind::kw_extern,
        TokenKind::kw_export,
        TokenKind::kw_c,
        TokenKind::kw_fn,
        TokenKind::kw_struct,
        TokenKind::kw_opaque,
        TokenKind::kw_enum,
        TokenKind::kw_const,
        TokenKind::kw_type,
        TokenKind::kw_impl,
        TokenKind::kw_match,
        TokenKind::kw_let,
        TokenKind::kw_var,
        TokenKind::kw_if,
        TokenKind::kw_else,
        TokenKind::kw_for,
        TokenKind::kw_in,
        TokenKind::kw_while,
        TokenKind::kw_break,
        TokenKind::kw_continue,
        TokenKind::kw_defer,
        TokenKind::kw_return,
        TokenKind::kw_true,
        TokenKind::kw_false,
        TokenKind::kw_null,
        TokenKind::kw_void,
        TokenKind::kw_bool,
        TokenKind::kw_i8,
        TokenKind::kw_u8,
        TokenKind::kw_i16,
        TokenKind::kw_u16,
        TokenKind::kw_i32,
        TokenKind::kw_u32,
        TokenKind::kw_i64,
        TokenKind::kw_u64,
        TokenKind::kw_isize,
        TokenKind::kw_usize,
        TokenKind::kw_f32,
        TokenKind::kw_f64,
        TokenKind::kw_str,
        TokenKind::kw_mut,
        TokenKind::kw_cast,
        TokenKind::kw_ptrcast,
        TokenKind::kw_bitcast,
        TokenKind::kw_sizeof,
        TokenKind::kw_alignof,
        TokenKind::kw_ptraddr,
        TokenKind::kw_ptrat,
        TokenKind::kw_strptr,
        TokenKind::kw_strblen,
        TokenKind::kw_strraw,
        TokenKind::eof,
    };
    EXPECT_EQ(token_kinds(result.value()), expected);
}

TEST(CoreUnit, LexerKeepsKeywordLikeIdentifiersAsIdentifiers) {
    DiagnosticSink diagnostics;
    constexpr std::string_view source =
        "modulee importable pub_ fnx for2 strptrx strlen_bytes ptraddress";
    lex::Lexer lexer({11}, source, diagnostics);
    auto result = lexer.tokenize();
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_FALSE(diagnostics.has_error());

    const std::vector<TokenKind> expected {
        TokenKind::identifier,
        TokenKind::identifier,
        TokenKind::identifier,
        TokenKind::identifier,
        TokenKind::identifier,
        TokenKind::identifier,
        TokenKind::identifier,
        TokenKind::identifier,
        TokenKind::eof,
    };
    EXPECT_EQ(token_kinds(result.value()), expected);
}

TEST(CoreUnit, LexerRejectsNonAsciiBytesOutsideStrings) {
    DiagnosticSink diagnostics;
    std::string source;
    source.push_back(static_cast<char>(0xC3));
    lex::Lexer lexer({12}, source, diagnostics);
    auto result = lexer.tokenize();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::lex_error);
    ASSERT_EQ(diagnostics.diagnostics().size(), 1U);
    EXPECT_EQ(diagnostics.diagnostics().front().message, "invalid character");
    EXPECT_EQ(diagnostics.diagnostics().front().range.begin, 0U);
    EXPECT_EQ(diagnostics.diagnostics().front().range.end, 1U);
}

TEST(CoreUnit, LexerRecognizesLongestPunctuatorMatches) {
    DiagnosticSink diagnostics;
    constexpr std::string_view source =
        "... . :: : -> -= -- - => == = != ! <= <<= << < >= >>= >> > && &= & || |= | "
        "( ) { } [ ] , ; ++ += + *= * /= / %= % ^= ^ ~ @ ?";
    lex::Lexer lexer({9}, source, diagnostics);
    auto result = lexer.tokenize();
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_FALSE(diagnostics.has_error());

    const std::vector<TokenKind> expected {
        TokenKind::ellipsis,
        TokenKind::dot,
        TokenKind::colon_colon,
        TokenKind::colon,
        TokenKind::arrow,
        TokenKind::minus_equal,
        TokenKind::minus_minus,
        TokenKind::minus,
        TokenKind::fat_arrow,
        TokenKind::equal_equal,
        TokenKind::equal,
        TokenKind::bang_equal,
        TokenKind::bang,
        TokenKind::less_equal,
        TokenKind::less_less_equal,
        TokenKind::less_less,
        TokenKind::less,
        TokenKind::greater_equal,
        TokenKind::greater_greater_equal,
        TokenKind::greater_greater,
        TokenKind::greater,
        TokenKind::amp_amp,
        TokenKind::amp_equal,
        TokenKind::amp,
        TokenKind::pipe_pipe,
        TokenKind::pipe_equal,
        TokenKind::pipe,
        TokenKind::l_paren,
        TokenKind::r_paren,
        TokenKind::l_brace,
        TokenKind::r_brace,
        TokenKind::l_bracket,
        TokenKind::r_bracket,
        TokenKind::comma,
        TokenKind::semicolon,
        TokenKind::plus_plus,
        TokenKind::plus_equal,
        TokenKind::plus,
        TokenKind::star_equal,
        TokenKind::star,
        TokenKind::slash_equal,
        TokenKind::slash,
        TokenKind::percent_equal,
        TokenKind::percent,
        TokenKind::caret_equal,
        TokenKind::caret,
        TokenKind::tilde,
        TokenKind::at,
        TokenKind::question,
        TokenKind::eof,
    };
    EXPECT_EQ(token_kinds(result.value()), expected);
}

TEST(CoreUnit, LexerRejectsMalformedNumericSeparatorsAndFloatExponents) {
    DiagnosticSink diagnostics;
    constexpr std::string_view source =
        "module bad.numbers;\n"
        "const trailing: i32 = 1_;\n"
        "const repeated: i32 = 1__2;\n"
        "const hex: i32 = 0x_FF;\n"
        "const bin: i32 = 0b_1010;\n"
        "const frac: f64 = 1.0_;\n"
        "const exp: f64 = 1e+;\n";
    lex::Lexer lexer({7}, source, diagnostics);
    auto result = lexer.tokenize();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::lex_error);
    ASSERT_TRUE(diagnostics.has_error());
    expect_contains(diagnostics.diagnostics().front().message, "digit separator must be between digits");
}

TEST(CoreUnit, LexerPreservesNumericDiagnosticRanges) {
    DiagnosticSink diagnostics;
    constexpr std::string_view source = "0x_FF 0b102 1e+";
    lex::Lexer lexer({10}, source, diagnostics);
    auto result = lexer.tokenize();
    ASSERT_FALSE(result);
    ASSERT_GE(diagnostics.diagnostics().size(), 3U);

    const base::Diagnostic& separator = diagnostics.diagnostics()[0];
    EXPECT_EQ(separator.message, "digit separator must be between digits");
    EXPECT_EQ(separator.range.begin, 2U);
    EXPECT_EQ(separator.range.end, 3U);

    const base::Diagnostic& invalid_binary_digit = diagnostics.diagnostics()[1];
    EXPECT_EQ(invalid_binary_digit.message, "invalid digit in binary literal");
    EXPECT_EQ(invalid_binary_digit.range.begin, 10U);
    EXPECT_EQ(invalid_binary_digit.range.end, 11U);

    const base::Diagnostic& missing_exponent_digits = diagnostics.diagnostics()[2];
    EXPECT_EQ(missing_exponent_digits.message, "float exponent literal has no digits");
    EXPECT_EQ(missing_exponent_digits.range.begin, source.size());
    EXPECT_EQ(missing_exponent_digits.range.end, source.size());
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

    std::string raw_nul_c_string = "const text: *const u8 = c\"a";
    raw_nul_c_string.push_back('\0');
    raw_nul_c_string += "b\";";
    expect_lex_error(raw_nul_c_string, "interior NUL");

    std::string invalid_utf8 = "const text: str = \"";
    invalid_utf8.push_back(static_cast<char>(0xC3));
    invalid_utf8.push_back('(');
    invalid_utf8 += "\";";
    expect_lex_error(invalid_utf8, "valid UTF-8");
}

TEST(CoreUnit, LexerPreservesStringAndByteRecoveryBoundaries) {
    DiagnosticSink diagnostics;
    constexpr std::string_view source = "\"escaped\\\nmissing\" b'wide' b'unterminated\nnext";
    lex::Lexer lexer({14}, source, diagnostics);
    auto result = lexer.tokenize();
    ASSERT_FALSE(result);
    ASSERT_GE(diagnostics.diagnostics().size(), 3U);

    const base::Diagnostic& invalid_escape = diagnostics.diagnostics()[0];
    EXPECT_EQ(invalid_escape.message, "invalid escape sequence");

    const base::Diagnostic& oversized_byte = diagnostics.diagnostics()[1];
    EXPECT_EQ(oversized_byte.message, "byte literal must contain one byte");
    EXPECT_EQ(oversized_byte.range.begin, 19U);
    EXPECT_EQ(oversized_byte.range.end, 26U);

    const base::Diagnostic& unterminated_byte = diagnostics.diagnostics()[2];
    EXPECT_EQ(unterminated_byte.message, "unterminated byte literal");
    EXPECT_EQ(unterminated_byte.range.begin, 27U);
    EXPECT_EQ(unterminated_byte.range.end, 41U);
}

} // namespace aurex::test
