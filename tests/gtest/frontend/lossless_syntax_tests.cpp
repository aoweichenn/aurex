#include <aurex/base/diagnostic.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/syntax/lossless.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::SourceId LOSSLESS_TEST_SOURCE_ID{31};
constexpr base::SourceId LOSSLESS_MANUAL_SOURCE_ID{32};
constexpr int LOSSLESS_UNKNOWN_NODE_KIND_VALUE = 99;
constexpr std::string_view LOSSLESS_TEST_SOURCE = "module lossless.sample;\n"
                                                  "// leading comment\n"
                                                  "fn main() -> i32 { /* block comment */ return 0; }\n";

[[nodiscard]] syntax::Token make_token(const syntax::TokenKind kind, const base::usize begin,
    const std::string_view text, const base::SourceId source = LOSSLESS_MANUAL_SOURCE_ID)
{
    return syntax::Token{kind, base::SourceRange{source, begin, begin + text.size()}, text};
}

[[nodiscard]] lex::TokenBuffer tokenize_lossless(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::LexerOptions options;
    options.emit_trivia_tokens = true;
    lex::Lexer lexer(LOSSLESS_TEST_SOURCE_ID, source, diagnostics, options);
    auto result = lexer.tokenize();
    EXPECT_TRUE(result) << result.error().message;
    EXPECT_FALSE(diagnostics.has_error());
    return result.take_value();
}

[[nodiscard]] lex::TokenBuffer tokenize_semantic(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(LOSSLESS_TEST_SOURCE_ID, source, diagnostics);
    auto result = lexer.tokenize();
    EXPECT_TRUE(result) << result.error().message;
    EXPECT_FALSE(diagnostics.has_error());
    return result.take_value();
}

[[nodiscard]] std::vector<syntax::Token> semantic_tokens_from_lossless(const std::span<const syntax::Token> tokens)
{
    std::vector<syntax::Token> semantic_tokens;
    semantic_tokens.reserve(tokens.size());
    for (const syntax::Token& token : tokens) {
        if (!syntax::is_trivia_token(token.kind)) {
            semantic_tokens.push_back(token);
        }
    }
    return semantic_tokens;
}

void expect_same_token_sequence(
    const std::span<const syntax::Token> actual, const std::span<const syntax::Token> expected)
{
    ASSERT_EQ(actual.size(), expected.size());
    for (base::usize index = 0; index < actual.size(); ++index) {
        EXPECT_EQ(actual[index].kind, expected[index].kind) << index;
        EXPECT_EQ(actual[index].range.source.value, expected[index].range.source.value) << index;
        EXPECT_EQ(actual[index].range.begin, expected[index].range.begin) << index;
        EXPECT_EQ(actual[index].range.end, expected[index].range.end) << index;
        EXPECT_EQ(actual[index].text(), expected[index].text()) << index;
    }
}

void expect_contains(const std::string_view text, const std::string_view needle)
{
    EXPECT_NE(text.find(needle), std::string_view::npos) << "expected text to contain: " << needle << "\nactual text:\n"
                                                         << text;
}

} // namespace

TEST(CoreUnit, LosslessSyntaxPreservesTriviaAndReconstructsSource)
{
    lex::TokenBuffer tokens = tokenize_lossless(LOSSLESS_TEST_SOURCE);
    const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens.span());

    EXPECT_EQ(tree.root_kind(), syntax::LosslessNodeKind::source_file);
    EXPECT_EQ(tree.range().source.value, LOSSLESS_TEST_SOURCE_ID.value);
    EXPECT_EQ(tree.range().begin, 0U);
    EXPECT_EQ(tree.range().end, LOSSLESS_TEST_SOURCE.size());
    EXPECT_EQ(tree.reconstruct_text(), LOSSLESS_TEST_SOURCE);
    EXPECT_GT(tree.trivia_token_count(), 0U);
    EXPECT_GT(tree.semantic_token_count(), 0U);
    EXPECT_EQ(syntax::lossless_node_kind_name(tree.root_kind()), "source_file");
    EXPECT_TRUE(syntax::is_trivia_token(syntax::TokenKind::whitespace));
    EXPECT_TRUE(syntax::is_trivia_token(syntax::TokenKind::line_comment));
    EXPECT_TRUE(syntax::is_trivia_token(syntax::TokenKind::block_comment));
    EXPECT_FALSE(syntax::is_trivia_token(syntax::TokenKind::kw_module));

    const std::string dump = syntax::dump_lossless_syntax_tree(tree);
    expect_contains(dump, "source_file 0.." + std::to_string(LOSSLESS_TEST_SOURCE.size()));
    expect_contains(dump, "whitespace `\\n`");
    expect_contains(dump, "line_comment `// leading comment`");
    expect_contains(dump, "block_comment `/* block comment */`");
    expect_contains(dump, "kw_module `module`");
    expect_contains(dump, "integer_literal `0`");
}

TEST(CoreUnit, LosslessSyntaxSemanticTokensMatchNormalLexer)
{
    lex::TokenBuffer normal_tokens = tokenize_semantic(LOSSLESS_TEST_SOURCE);
    lex::TokenBuffer lossless_tokens = tokenize_lossless(LOSSLESS_TEST_SOURCE);

    const std::vector<syntax::Token> semantic_tokens = semantic_tokens_from_lossless(lossless_tokens.span());
    expect_same_token_sequence(semantic_tokens, normal_tokens.span());
}

TEST(CoreUnit, LosslessSyntaxHandlesEmptySource)
{
    lex::TokenBuffer tokens = tokenize_lossless("");
    const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens.span());

    EXPECT_EQ(tree.range().source.value, LOSSLESS_TEST_SOURCE_ID.value);
    EXPECT_EQ(tree.range().begin, 0U);
    EXPECT_EQ(tree.range().end, 0U);
    EXPECT_EQ(tree.token_count(), 1U);
    EXPECT_EQ(tree.trivia_token_count(), 0U);
    EXPECT_EQ(tree.semantic_token_count(), 0U);
    EXPECT_TRUE(tree.reconstruct_text().empty());

    const std::string dump = syntax::dump_lossless_syntax_tree(tree);
    expect_contains(dump, "source_file 0..0 tokens=1 trivia=0 semantic=0");
    expect_contains(dump, "0..0 eof");
}

TEST(CoreUnit, LosslessSyntaxHandlesDefaultTree)
{
    const syntax::LosslessSyntaxTree tree;

    EXPECT_EQ(tree.root_kind(), syntax::LosslessNodeKind::source_file);
    EXPECT_EQ(tree.range().source.value, 0U);
    EXPECT_EQ(tree.range().begin, 0U);
    EXPECT_EQ(tree.range().end, 0U);
    EXPECT_TRUE(tree.tokens().empty());
    EXPECT_EQ(tree.token_count(), 0U);
    EXPECT_EQ(tree.trivia_token_count(), 0U);
    EXPECT_EQ(tree.semantic_token_count(), 0U);
    EXPECT_TRUE(tree.reconstruct_text().empty());

    const std::string dump = syntax::dump_lossless_syntax_tree(tree);
    EXPECT_EQ(dump, "source_file 0..0 tokens=0 trivia=0 semantic=0\n");
}

TEST(CoreUnit, LosslessSyntaxHandlesEmptyTokenSpan)
{
    const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(std::span<const syntax::Token>{});

    EXPECT_EQ(tree.range().source.value, 0U);
    EXPECT_EQ(tree.range().begin, 0U);
    EXPECT_EQ(tree.range().end, 0U);
    EXPECT_TRUE(tree.tokens().empty());
    EXPECT_EQ(tree.token_count(), 0U);
    EXPECT_EQ(tree.trivia_token_count(), 0U);
    EXPECT_EQ(tree.semantic_token_count(), 0U);
    EXPECT_TRUE(tree.reconstruct_text().empty());
}

TEST(CoreUnit, LosslessSyntaxComputesRangeAcrossAllNonEofTokens)
{
    const std::string source = "0123456789";
    const std::vector<syntax::Token> tokens{
        make_token(syntax::TokenKind::identifier, 4U, std::string_view{source}.substr(4U, 2U)),
        make_token(syntax::TokenKind::whitespace, 1U, std::string_view{source}.substr(1U, 2U)),
        make_token(syntax::TokenKind::integer_literal, 8U, std::string_view{source}.substr(8U, 1U)),
        make_token(syntax::TokenKind::eof, source.size(), {}),
    };

    const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens);

    EXPECT_EQ(tree.range().source.value, LOSSLESS_MANUAL_SOURCE_ID.value);
    EXPECT_EQ(tree.range().begin, 1U);
    EXPECT_EQ(tree.range().end, 9U);
    EXPECT_EQ(tree.token_count(), tokens.size());
    EXPECT_EQ(tree.trivia_token_count(), 1U);
    EXPECT_EQ(tree.semantic_token_count(), 2U);
    EXPECT_EQ(tree.reconstruct_text(), "45128");

    const std::string dump = syntax::dump_lossless_syntax_tree(tree);
    expect_contains(dump, "source_file 1..9 tokens=4 trivia=1 semantic=2");
    expect_contains(dump, "4..6 identifier `45`");
    expect_contains(dump, "1..3 whitespace `12`");
    expect_contains(dump, "8..9 integer_literal `8`");
    expect_contains(dump, "10..10 eof");
}

TEST(CoreUnit, LosslessSyntaxDumpEscapesTokenText)
{
    std::string text;
    text.push_back('`');
    text.push_back('\\');
    text.push_back('\r');
    text.push_back('\t');
    text.push_back('\x01');
    text.push_back(static_cast<char>(0x7fU));
    text.push_back('A');

    const std::vector<syntax::Token> tokens{
        make_token(syntax::TokenKind::whitespace, 10U, text),
        make_token(syntax::TokenKind::eof, 10U + text.size(), {}),
    };

    const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens);

    EXPECT_EQ(tree.reconstruct_text(), text);
    const std::string dump = syntax::dump_lossless_syntax_tree(tree);
    expect_contains(dump, R"(10..17 whitespace `\`\\\r\t\x01\x7fA`)");
}

TEST(CoreUnit, LosslessSyntaxNamesUnknownNodeKinds)
{
    EXPECT_EQ(syntax::lossless_node_kind_name(static_cast<syntax::LosslessNodeKind>(LOSSLESS_UNKNOWN_NODE_KIND_VALUE)),
        "unknown");
}

} // namespace aurex::test
