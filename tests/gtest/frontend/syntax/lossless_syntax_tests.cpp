#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/lossless_parse.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/syntax/core/ast_dump.hpp>
#include <aurex/frontend/syntax/core/lossless.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>

#include <optional>
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
constexpr int LOSSLESS_UNKNOWN_ELEMENT_KIND_VALUE = 99;
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

[[nodiscard]] bool lossless_tree_has_node_kind(
    const syntax::LosslessSyntaxTree& tree, const syntax::LosslessNodeKind kind) noexcept
{
    for (const syntax::LosslessNode& node : tree.nodes()) {
        if (node.kind == kind) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] syntax::LosslessNodeId first_node_with_kind(
    const syntax::LosslessSyntaxTree& tree, const syntax::LosslessNodeKind kind) noexcept
{
    const std::span<const syntax::LosslessNode> nodes = tree.nodes();
    for (base::usize index = 0; index < nodes.size(); ++index) {
        if (nodes[index].kind == kind) {
            return syntax::LosslessNodeId{index};
        }
    }
    return syntax::INVALID_LOSSLESS_NODE_ID;
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

[[nodiscard]] syntax::AstModule parse_semantic_tokens(const std::span<const syntax::Token> tokens)
{
    base::DiagnosticSink diagnostics;
    parse::Parser parser(tokens, diagnostics);
    auto parsed = parser.parse_module();
    EXPECT_TRUE(parsed) << (parsed ? "" : parsed.error().message);
    EXPECT_FALSE(diagnostics.has_error());
    if (!parsed) {
        return {};
    }
    return parsed.take_value();
}

void expect_contains(const std::string_view text, const std::string_view needle)
{
    EXPECT_NE(text.find(needle), std::string_view::npos) << "expected text to contain: " << needle << "\nactual text:\n"
                                                         << text;
}

[[nodiscard]] syntax::LosslessNodeId require_token_stream_node(const syntax::LosslessSyntaxTree& tree)
{
    const std::span<const syntax::LosslessElement> root_children = tree.children(tree.root_id());
    EXPECT_EQ(root_children.size(), 1U);
    if (root_children.size() != 1U || !root_children.front().is_node()) {
        return syntax::INVALID_LOSSLESS_NODE_ID;
    }
    const syntax::LosslessNodeId stream_id = root_children.front().node_id();
    const syntax::LosslessNode* stream = tree.node(stream_id);
    EXPECT_NE(stream, nullptr);
    if (stream != nullptr) {
        EXPECT_EQ(stream->kind, syntax::LosslessNodeKind::token_stream);
    }
    return stream_id;
}

[[nodiscard]] const syntax::Token* first_token_child(
    const syntax::LosslessSyntaxTree& tree, const syntax::LosslessNodeId node_id) noexcept
{
    for (const syntax::LosslessElement child : tree.children(node_id)) {
        if (child.is_token()) {
            return tree.token(child.token_id());
        }
    }
    return nullptr;
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
    EXPECT_TRUE(tree.is_structurally_valid());
    EXPECT_EQ(tree.reconstruct_text(), LOSSLESS_TEST_SOURCE);
    EXPECT_EQ(tree.reconstruct_text(tree.root_id()), LOSSLESS_TEST_SOURCE);
    EXPECT_EQ(tree.parent(tree.root_id()), syntax::INVALID_LOSSLESS_NODE_ID);
    EXPECT_EQ(tree.token_span(tree.root_id()).size(), tree.token_count());
    EXPECT_GT(tree.trivia_token_count(), 0U);
    EXPECT_GT(tree.semantic_token_count(), 0U);
    EXPECT_EQ(syntax::lossless_node_kind_name(tree.root_kind()), "source_file");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::module_decl), "module_decl");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::import_decl), "import_decl");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::function_decl), "function_decl");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::struct_decl), "struct_decl");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::enum_decl), "enum_decl");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::trait_decl), "trait_decl");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::impl_block), "impl_block");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::extern_block), "extern_block");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::const_decl), "const_decl");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::type_alias_decl), "type_alias_decl");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::opaque_struct_decl), "opaque_struct_decl");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::block), "block");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::paren_group), "paren_group");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::bracket_group), "bracket_group");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::brace_group), "brace_group");
    EXPECT_EQ(syntax::lossless_node_kind_name(syntax::LosslessNodeKind::token_stream), "token_stream");
    EXPECT_EQ(syntax::lossless_element_kind_name(syntax::LosslessElementKind::node), "node");
    EXPECT_EQ(syntax::lossless_element_kind_name(syntax::LosslessElementKind::token), "token");
    EXPECT_TRUE(syntax::is_trivia_token(syntax::TokenKind::whitespace));
    EXPECT_TRUE(syntax::is_trivia_token(syntax::TokenKind::line_comment));
    EXPECT_TRUE(syntax::is_trivia_token(syntax::TokenKind::block_comment));
    EXPECT_FALSE(syntax::is_trivia_token(syntax::TokenKind::kw_module));

    EXPECT_GT(tree.node_count(), 4U);
    EXPECT_GE(tree.element_count(), tree.token_count());
    const syntax::LosslessNode* root = tree.root_node();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->kind, syntax::LosslessNodeKind::source_file);
    EXPECT_GT(root->child_count, 1U);
    const syntax::LosslessNodeId module_id = first_node_with_kind(tree, syntax::LosslessNodeKind::module_decl);
    ASSERT_NE(module_id, syntax::INVALID_LOSSLESS_NODE_ID);
    EXPECT_EQ(tree.parent(module_id), tree.root_id());
    EXPECT_EQ(tree.reconstruct_text(module_id), "module lossless.sample;");
    const std::optional<syntax::LosslessNodeKey> module_key = tree.node_key(module_id);
    ASSERT_TRUE(module_key.has_value());
    EXPECT_EQ(module_key->kind, syntax::LosslessNodeKind::module_decl);
    EXPECT_EQ(module_key->range.begin, 0U);
    EXPECT_EQ(module_key->range.end, 23U);
    EXPECT_EQ(module_key->depth, 1U);
    const syntax::LosslessNodeId function_id = first_node_with_kind(tree, syntax::LosslessNodeKind::function_decl);
    ASSERT_NE(function_id, syntax::INVALID_LOSSLESS_NODE_ID);
    EXPECT_EQ(tree.node_at_offset(0U), module_id);
    EXPECT_EQ(tree.node_at_offset(LOSSLESS_TEST_SOURCE.find("main")), function_id);
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::paren_group));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::block));
    const syntax::LosslessNodeId block_id = tree.node_at_offset(LOSSLESS_TEST_SOURCE.find("block comment"));
    ASSERT_NE(block_id, syntax::INVALID_LOSSLESS_NODE_ID);
    const syntax::LosslessNode* block = tree.node(block_id);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(block->kind, syntax::LosslessNodeKind::block);
    const syntax::Token* first_token = first_token_child(tree, module_id);
    ASSERT_NE(first_token, nullptr);
    EXPECT_EQ(first_token->kind, syntax::TokenKind::kw_module);
    EXPECT_EQ(first_token->text(), "module");
    EXPECT_EQ(tree.node(syntax::INVALID_LOSSLESS_NODE_ID), nullptr);
    EXPECT_EQ(tree.parent(syntax::INVALID_LOSSLESS_NODE_ID), syntax::INVALID_LOSSLESS_NODE_ID);
    EXPECT_EQ(tree.token(syntax::INVALID_LOSSLESS_TOKEN_ID), nullptr);
    EXPECT_TRUE(tree.children(syntax::INVALID_LOSSLESS_NODE_ID).empty());
    EXPECT_TRUE(tree.token_span(syntax::INVALID_LOSSLESS_NODE_ID).empty());
    EXPECT_FALSE(tree.node_key(syntax::INVALID_LOSSLESS_NODE_ID).has_value());

    const std::string dump = syntax::dump_lossless_syntax_tree(tree);
    expect_contains(dump, "source_file 0.." + std::to_string(LOSSLESS_TEST_SOURCE.size()) + " tokens=");
    expect_contains(dump, "nodes=" + std::to_string(tree.node_count()));
    expect_contains(dump, "  module_decl 0..23");
    expect_contains(dump, "  function_decl");
    expect_contains(dump, "    paren_group");
    expect_contains(dump, "    block");
    expect_contains(dump, "whitespace `\\n`");
    expect_contains(dump, "line_comment `// leading comment`");
    expect_contains(dump, "block_comment `/* block comment */`");
    expect_contains(dump, "kw_module `module`");
    expect_contains(dump, "integer_literal `0`");

    const syntax::LosslessTokenId comment_token = tree.token_at_offset(24U);
    ASSERT_NE(comment_token, syntax::INVALID_LOSSLESS_TOKEN_ID);
    const syntax::Token* comment = tree.token(comment_token);
    ASSERT_NE(comment, nullptr);
    EXPECT_EQ(comment->kind, syntax::TokenKind::line_comment);
    const syntax::LosslessTokenId eof_token = tree.token_at_offset(LOSSLESS_TEST_SOURCE.size());
    ASSERT_NE(eof_token, syntax::INVALID_LOSSLESS_TOKEN_ID);
    const syntax::Token* eof = tree.token(eof_token);
    ASSERT_NE(eof, nullptr);
    EXPECT_EQ(eof->kind, syntax::TokenKind::eof);
    EXPECT_EQ(tree.token_at_offset(LOSSLESS_TEST_SOURCE.size() + 1U), syntax::INVALID_LOSSLESS_TOKEN_ID);
}

TEST(CoreUnit, LosslessSyntaxSemanticTokensMatchNormalLexer)
{
    lex::TokenBuffer normal_tokens = tokenize_semantic(LOSSLESS_TEST_SOURCE);
    lex::TokenBuffer lossless_tokens = tokenize_lossless(LOSSLESS_TEST_SOURCE);

    const std::vector<syntax::Token> semantic_tokens = semantic_tokens_from_lossless(lossless_tokens.span());
    expect_same_token_sequence(semantic_tokens, normal_tokens.span());
}

TEST(CoreUnit, LosslessSyntaxLowersToSameAstAsSemanticParser)
{
    constexpr std::string_view source = "module lossless.lower;\n"
                                        "// parser bridge keeps trivia out of the semantic token stream\n"
                                        "fn add(a: i32, b: i32) -> i32 {\n"
                                        "  let value = a + b;\n"
                                        "  return value;\n"
                                        "}\n";
    lex::TokenBuffer normal_tokens = tokenize_semantic(source);
    lex::TokenBuffer lossless_tokens = tokenize_lossless(source);
    const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(lossless_tokens.span());
    ASSERT_TRUE(tree.is_structurally_valid());

    const std::vector<syntax::Token> semantic_tokens = parse::semantic_tokens_from_lossless_tree(tree);
    expect_same_token_sequence(semantic_tokens, normal_tokens.span());

    const syntax::AstModule semantic_ast = parse_semantic_tokens(normal_tokens.span());
    base::DiagnosticSink diagnostics;
    auto lossless_ast = parse::lower_lossless_syntax_to_ast(tree, diagnostics);
    ASSERT_TRUE(lossless_ast) << (lossless_ast ? "" : lossless_ast.error().message);
    EXPECT_FALSE(diagnostics.has_error());
    EXPECT_EQ(syntax::dump_ast(lossless_ast.value()), syntax::dump_ast(semantic_ast));
}

TEST(CoreUnit, LosslessSyntaxHandlesEmptySource)
{
    lex::TokenBuffer tokens = tokenize_lossless("");
    const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens.span());

    EXPECT_TRUE(tree.is_structurally_valid());
    EXPECT_EQ(tree.range().source.value, LOSSLESS_TEST_SOURCE_ID.value);
    EXPECT_EQ(tree.range().begin, 0U);
    EXPECT_EQ(tree.range().end, 0U);
    EXPECT_EQ(tree.token_count(), 1U);
    EXPECT_EQ(tree.node_count(), 1U);
    EXPECT_EQ(tree.element_count(), 1U);
    EXPECT_EQ(tree.trivia_token_count(), 0U);
    EXPECT_EQ(tree.semantic_token_count(), 0U);
    EXPECT_TRUE(tree.reconstruct_text().empty());

    const std::span<const syntax::LosslessElement> root_children = tree.children(tree.root_id());
    ASSERT_EQ(root_children.size(), 1U);
    EXPECT_TRUE(root_children.front().is_token());
    const syntax::Token* eof = tree.token(root_children.front().token_id());
    ASSERT_NE(eof, nullptr);
    EXPECT_EQ(eof->kind, syntax::TokenKind::eof);

    const std::string dump = syntax::dump_lossless_syntax_tree(tree);
    expect_contains(dump, "source_file 0..0 tokens=1 trivia=0 semantic=0 nodes=1 elements=1");
    expect_contains(dump, "0..0 eof");
}

TEST(CoreUnit, LosslessSyntaxHandlesDefaultTree)
{
    const syntax::LosslessSyntaxTree tree;

    EXPECT_TRUE(tree.is_structurally_valid());
    EXPECT_EQ(tree.root_kind(), syntax::LosslessNodeKind::source_file);
    EXPECT_EQ(tree.range().source.value, 0U);
    EXPECT_EQ(tree.range().begin, 0U);
    EXPECT_EQ(tree.range().end, 0U);
    EXPECT_NE(tree.root_node(), nullptr);
    EXPECT_EQ(tree.node_count(), 1U);
    EXPECT_EQ(tree.element_count(), 0U);
    EXPECT_TRUE(tree.tokens().empty());
    EXPECT_TRUE(tree.nodes().size() == 1U);
    EXPECT_TRUE(tree.elements().empty());
    EXPECT_TRUE(tree.children(tree.root_id()).empty());
    EXPECT_EQ(tree.token_count(), 0U);
    EXPECT_EQ(tree.trivia_token_count(), 0U);
    EXPECT_EQ(tree.semantic_token_count(), 0U);
    EXPECT_TRUE(tree.reconstruct_text().empty());

    const std::string dump = syntax::dump_lossless_syntax_tree(tree);
    EXPECT_EQ(dump, "source_file 0..0 tokens=0 trivia=0 semantic=0 nodes=1 elements=0\n");
}

TEST(CoreUnit, LosslessSyntaxHandlesEmptyTokenSpan)
{
    const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(std::span<const syntax::Token>{});

    EXPECT_TRUE(tree.is_structurally_valid());
    EXPECT_EQ(tree.range().source.value, 0U);
    EXPECT_EQ(tree.range().begin, 0U);
    EXPECT_EQ(tree.range().end, 0U);
    EXPECT_EQ(tree.node_count(), 1U);
    EXPECT_EQ(tree.element_count(), 0U);
    EXPECT_TRUE(tree.tokens().empty());
    EXPECT_TRUE(tree.children(tree.root_id()).empty());
    EXPECT_EQ(tree.token_count(), 0U);
    EXPECT_EQ(tree.trivia_token_count(), 0U);
    EXPECT_EQ(tree.semantic_token_count(), 0U);
    EXPECT_TRUE(tree.reconstruct_text().empty());

    const std::vector<syntax::Token> semantic_tokens = parse::semantic_tokens_from_lossless_tree(tree);
    ASSERT_EQ(semantic_tokens.size(), 1U);
    EXPECT_EQ(semantic_tokens.front().kind, syntax::TokenKind::eof);
    base::DiagnosticSink diagnostics;
    auto parsed = parse::lower_lossless_syntax_to_ast(tree, diagnostics);
    EXPECT_TRUE(parsed) << (parsed ? "" : parsed.error().message);
    EXPECT_FALSE(diagnostics.has_error());
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
    EXPECT_EQ(tree.node_count(), 2U);
    EXPECT_EQ(tree.element_count(), tokens.size() + 1U);
    EXPECT_TRUE(tree.is_structurally_valid());
    EXPECT_EQ(tree.parent(tree.root_id()), syntax::INVALID_LOSSLESS_NODE_ID);
    EXPECT_EQ(tree.token_count(), tokens.size());
    EXPECT_EQ(tree.trivia_token_count(), 1U);
    EXPECT_EQ(tree.semantic_token_count(), 2U);
    EXPECT_EQ(tree.reconstruct_text(), "45128");

    const syntax::LosslessNodeId stream_id = require_token_stream_node(tree);
    EXPECT_EQ(tree.parent(stream_id), tree.root_id());
    EXPECT_EQ(tree.token_span(stream_id).size(), tokens.size());
    const std::span<const syntax::LosslessElement> leaves = tree.children(stream_id);
    ASSERT_EQ(leaves.size(), tokens.size());
    EXPECT_TRUE(leaves[0].is_token());
    EXPECT_EQ(leaves[0].node_id(), syntax::INVALID_LOSSLESS_NODE_ID);
    EXPECT_EQ(leaves[0].token_id().value, 0U);
    EXPECT_EQ(syntax::LosslessElement::node(stream_id).token_id(), syntax::INVALID_LOSSLESS_TOKEN_ID);

    const std::string dump = syntax::dump_lossless_syntax_tree(tree);
    expect_contains(dump, "source_file 1..9 tokens=4 trivia=1 semantic=2 nodes=2 elements=5");
    expect_contains(dump, "token_stream 1..9 children=4");
    expect_contains(dump, "4..6 identifier `45`");
    expect_contains(dump, "1..3 whitespace `12`");
    expect_contains(dump, "8..9 integer_literal `8`");
    expect_contains(dump, "10..10 eof");
}

TEST(CoreUnit, LosslessSyntaxBuildsTopLevelDeclarationAndGroupNodes)
{
    constexpr std::string_view source = "module lossless.top;\n"
                                        "pub import core.math as math;\n"
                                        "pub const answer: i32 = 42;\n"
                                        "type Alias = i32;\n"
                                        "pub opaque struct Foreign;\n"
                                        "pub struct Box<T> { pub value: T; }\n"
                                        "enum Mode: u8 { fast = 1, slow = 2, }\n"
                                        "pub trait Reader { fn read(self: &mut Self) -> i32; }\n"
                                        "impl Box<i32> { pub fn read(self: &Box<i32>) -> i32 { return self.value; } }\n"
                                        "extern C { unsafe fn native(value: i32) -> i32; }\n"
                                        "export C unsafe fn run() -> i32 { let values: [1]i32 = [1]; return native(values[0]); }\n";

    lex::TokenBuffer tokens = tokenize_lossless(source);
    const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens.span());

    EXPECT_EQ(tree.reconstruct_text(), source);
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::module_decl));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::import_decl));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::const_decl));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::type_alias_decl));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::opaque_struct_decl));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::struct_decl));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::enum_decl));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::trait_decl));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::impl_block));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::extern_block));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::function_decl));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::block));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::paren_group));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::bracket_group));
    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::brace_group));

    const std::string dump = syntax::dump_lossless_syntax_tree(tree);
    expect_contains(dump, "module_decl");
    expect_contains(dump, "import_decl");
    expect_contains(dump, "const_decl");
    expect_contains(dump, "type_alias_decl");
    expect_contains(dump, "opaque_struct_decl");
    expect_contains(dump, "struct_decl");
    expect_contains(dump, "enum_decl");
    expect_contains(dump, "trait_decl");
    expect_contains(dump, "impl_block");
    expect_contains(dump, "extern_block");
    expect_contains(dump, "function_decl");
    expect_contains(dump, "block");
    expect_contains(dump, "paren_group");
    expect_contains(dump, "bracket_group");
    expect_contains(dump, "brace_group");
}

TEST(CoreUnit, LosslessSyntaxKeepsIncompleteAndUnknownTopLevelTokensLossless)
{
    {
        lex::TokenBuffer tokens = tokenize_lossless("import direct;\n");
        const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens.span());
        EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::import_decl));
        expect_contains(syntax::dump_lossless_syntax_tree(tree), "import_decl");
    }
    {
        lex::TokenBuffer tokens = tokenize_lossless("pub");
        const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens.span());
        EXPECT_FALSE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::import_decl));
        EXPECT_EQ(tree.reconstruct_text(), "pub");
        expect_contains(syntax::dump_lossless_syntax_tree(tree), "kw_pub `pub`");
    }
    {
        lex::TokenBuffer tokens = tokenize_lossless("unsafe");
        const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens.span());
        EXPECT_FALSE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::function_decl));
        EXPECT_EQ(tree.reconstruct_text(), "unsafe");
        expect_contains(syntax::dump_lossless_syntax_tree(tree), "kw_unsafe `unsafe`");
    }
    {
        lex::TokenBuffer tokens = tokenize_lossless("not_a_decl");
        const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens.span());
        EXPECT_EQ(tree.node_count(), 1U);
        EXPECT_EQ(tree.reconstruct_text(), "not_a_decl");
        expect_contains(syntax::dump_lossless_syntax_tree(tree), "identifier `not_a_decl`");
    }
    {
        lex::TokenBuffer tokens = tokenize_lossless("module unfinished");
        const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens.span());
        EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::module_decl));
        EXPECT_EQ(tree.reconstruct_text(), "module unfinished");
        expect_contains(syntax::dump_lossless_syntax_tree(tree), "module_decl 0..17");
    }
}

TEST(CoreUnit, LosslessSyntaxHandlesSortedTokenSpanWithoutEof)
{
    constexpr std::string_view source = "module x";
    const std::vector<syntax::Token> tokens{
        make_token(syntax::TokenKind::kw_module, 0U, std::string_view{source}.substr(0U, 6U)),
        make_token(syntax::TokenKind::whitespace, 6U, std::string_view{source}.substr(6U, 1U)),
        make_token(syntax::TokenKind::identifier, 7U, std::string_view{source}.substr(7U, 1U)),
    };

    const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens);

    EXPECT_TRUE(lossless_tree_has_node_kind(tree, syntax::LosslessNodeKind::module_decl));
    EXPECT_EQ(tree.node_count(), 2U);
    EXPECT_EQ(tree.token_count(), tokens.size());
    EXPECT_EQ(tree.reconstruct_text(), source);
    const std::string dump = syntax::dump_lossless_syntax_tree(tree);
    expect_contains(dump, "module_decl 0..8 children=3");
    expect_contains(dump, "kw_module `module`");
    expect_contains(dump, "identifier `x`");
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

TEST(CoreUnit, LosslessStableNodeKeySurvivesPrefixEdit)
{
    constexpr std::string_view source = "module stable.syntax;\n"
                                        "fn add(a: i32, b: i32) -> i32 {\n"
                                        "  return a + b;\n"
                                        "}\n"
                                        "fn main() -> i32 {\n"
                                        "  return add(1, 2);\n"
                                        "}\n";
    constexpr std::string_view changed_source = "module stable.syntax;\n"
                                                "import helper;\n"
                                                "fn add(a: i32, b: i32) -> i32 {\n"
                                                "  return a + b;\n"
                                                "}\n"
                                                "fn main() -> i32 {\n"
                                                "  return add(1, 2);\n"
                                                "}\n";
    const lex::TokenBuffer before_tokens = tokenize_lossless(source);
    const lex::TokenBuffer after_tokens = tokenize_lossless(changed_source);
    const syntax::LosslessSyntaxTree before = syntax::build_lossless_syntax_tree(before_tokens.span());
    const syntax::LosslessSyntaxTree after = syntax::build_lossless_syntax_tree(after_tokens.span());

    syntax::LosslessNodeId before_add = syntax::INVALID_LOSSLESS_NODE_ID;
    for (base::usize index = 0; index < before.node_count(); ++index) {
        const syntax::LosslessNodeId id{index};
        const syntax::LosslessNode* const node = before.node(id);
        if (node != nullptr && node->kind == syntax::LosslessNodeKind::function_decl
            && before.reconstruct_text(id).find("fn add") != std::string::npos) {
            before_add = id;
            break;
        }
    }
    syntax::LosslessNodeId after_add = syntax::INVALID_LOSSLESS_NODE_ID;
    for (base::usize index = 0; index < after.node_count(); ++index) {
        const syntax::LosslessNodeId id{index};
        const syntax::LosslessNode* const node = after.node(id);
        if (node != nullptr && node->kind == syntax::LosslessNodeKind::function_decl
            && after.reconstruct_text(id).find("fn add") != std::string::npos) {
            after_add = id;
            break;
        }
    }

    ASSERT_NE(before_add, syntax::INVALID_LOSSLESS_NODE_ID);
    ASSERT_NE(after_add, syntax::INVALID_LOSSLESS_NODE_ID);
    ASSERT_TRUE(before.node_key(before_add).has_value());
    ASSERT_TRUE(after.node_key(after_add).has_value());
    EXPECT_NE(before.node_key(before_add)->range.begin, after.node_key(after_add)->range.begin);
    ASSERT_TRUE(before.stable_node_key(before_add).has_value());
    ASSERT_TRUE(after.stable_node_key(after_add).has_value());
    EXPECT_EQ(*before.stable_node_key(before_add), *after.stable_node_key(after_add));
    EXPECT_FALSE(before.stable_node_key(syntax::INVALID_LOSSLESS_NODE_ID).has_value());

    const syntax::LosslessSyntaxReuseStats stats = syntax::compare_lossless_stable_nodes(before, after);
    EXPECT_TRUE(stats.reused);
    EXPECT_GT(stats.reused_nodes, 0U);
    EXPECT_GT(stats.recomputed_nodes, 0U);
    EXPECT_GT(stats.invalidated_nodes, 0U);
    EXPECT_EQ(stats.current_nodes, after.node_count());

    const syntax::LosslessSyntaxReuseStats identical_stats = syntax::compare_lossless_stable_nodes(before, before);
    EXPECT_EQ(identical_stats.reused_nodes, before.node_count());
    EXPECT_EQ(identical_stats.recomputed_nodes, 0U);
    EXPECT_EQ(identical_stats.invalidated_nodes, 0U);
}

TEST(CoreUnit, LosslessSyntaxNamesUnknownNodeKinds)
{
    EXPECT_EQ(syntax::lossless_node_kind_name(static_cast<syntax::LosslessNodeKind>(LOSSLESS_UNKNOWN_NODE_KIND_VALUE)),
        "unknown");
    EXPECT_EQ(syntax::lossless_element_kind_name(
                  static_cast<syntax::LosslessElementKind>(LOSSLESS_UNKNOWN_ELEMENT_KIND_VALUE)),
        "unknown");
}

} // namespace aurex::test
