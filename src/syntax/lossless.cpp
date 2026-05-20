#include <aurex/syntax/lossless.hpp>

#include <algorithm>
#include <sstream>
#include <utility>

namespace aurex::syntax {

namespace {

constexpr char LOSSLESS_DUMP_TEXT_QUOTE = '`';
constexpr char LOSSLESS_DUMP_ESCAPE = '\\';
constexpr char LOSSLESS_DUMP_NEWLINE = '\n';
constexpr char LOSSLESS_DUMP_CARRIAGE_RETURN = '\r';
constexpr char LOSSLESS_DUMP_TAB = '\t';
constexpr unsigned char LOSSLESS_DUMP_CONTROL_BYTE_LIMIT = 0x20U;
constexpr unsigned char LOSSLESS_DUMP_DELETE_BYTE = 0x7fU;
constexpr unsigned int LOSSLESS_DUMP_NIBBLE_BITS = 4U;
constexpr unsigned int LOSSLESS_DUMP_LOW_NIBBLE_MASK = 0x0fU;
constexpr char LOSSLESS_DUMP_HEX_DIGITS[] = "0123456789abcdef";

[[nodiscard]] base::SourceRange lossless_root_range(const std::span<const Token> tokens) noexcept
{
    base::SourceRange range{};
    if (tokens.empty()) {
        return range;
    }

    range.source = tokens.front().range.source;
    range.begin = tokens.front().range.begin;
    range.end = range.begin;

    bool saw_content = false;
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::eof) {
            continue;
        }
        if (!saw_content) {
            range.source = token.range.source;
            range.begin = token.range.begin;
            range.end = token.range.end;
            saw_content = true;
            continue;
        }
        range.begin = std::min(range.begin, token.range.begin);
        range.end = std::max(range.end, token.range.end);
    }
    return range;
}

void append_escaped_text(std::ostringstream& out, const std::string_view text)
{
    for (const unsigned char byte : text) {
        switch (byte) {
            case LOSSLESS_DUMP_TEXT_QUOTE:
                out << LOSSLESS_DUMP_ESCAPE << LOSSLESS_DUMP_TEXT_QUOTE;
                break;
            case LOSSLESS_DUMP_ESCAPE:
                out << LOSSLESS_DUMP_ESCAPE << LOSSLESS_DUMP_ESCAPE;
                break;
            case LOSSLESS_DUMP_NEWLINE:
                out << "\\n";
                break;
            case LOSSLESS_DUMP_CARRIAGE_RETURN:
                out << "\\r";
                break;
            case LOSSLESS_DUMP_TAB:
                out << "\\t";
                break;
            default:
                if (byte < LOSSLESS_DUMP_CONTROL_BYTE_LIMIT || byte == LOSSLESS_DUMP_DELETE_BYTE) {
                    out << "\\x"
                        << LOSSLESS_DUMP_HEX_DIGITS[(byte >> LOSSLESS_DUMP_NIBBLE_BITS) & LOSSLESS_DUMP_LOW_NIBBLE_MASK]
                        << LOSSLESS_DUMP_HEX_DIGITS[byte & LOSSLESS_DUMP_LOW_NIBBLE_MASK];
                    break;
                }
                out << static_cast<char>(byte);
                break;
        }
    }
}

} // namespace

LosslessSyntaxTree::LosslessSyntaxTree(
    const LosslessNodeKind root_kind, const base::SourceRange root_range, std::vector<Token> tokens)
    : root_kind_(root_kind), range_(root_range), tokens_(std::move(tokens))
{
}

LosslessNodeKind LosslessSyntaxTree::root_kind() const noexcept
{
    return this->root_kind_;
}

base::SourceRange LosslessSyntaxTree::range() const noexcept
{
    return this->range_;
}

std::span<const Token> LosslessSyntaxTree::tokens() const noexcept
{
    return this->tokens_;
}

base::usize LosslessSyntaxTree::token_count() const noexcept
{
    return this->tokens_.size();
}

base::usize LosslessSyntaxTree::trivia_token_count() const noexcept
{
    base::usize count = 0;
    for (const Token& token : this->tokens_) {
        if (is_trivia_token(token.kind)) {
            ++count;
        }
    }
    return count;
}

base::usize LosslessSyntaxTree::semantic_token_count() const noexcept
{
    base::usize count = 0;
    for (const Token& token : this->tokens_) {
        if (token.kind != TokenKind::eof && !is_trivia_token(token.kind)) {
            ++count;
        }
    }
    return count;
}

std::string LosslessSyntaxTree::reconstruct_text() const
{
    base::usize size = 0;
    for (const Token& token : this->tokens_) {
        if (token.kind != TokenKind::eof) {
            size += token.text().size();
        }
    }

    std::string text;
    text.reserve(size);
    for (const Token& token : this->tokens_) {
        if (token.kind != TokenKind::eof) {
            text += token.text();
        }
    }
    return text;
}

std::string_view lossless_node_kind_name(const LosslessNodeKind kind) noexcept
{
    switch (kind) {
        case LosslessNodeKind::source_file:
            return "source_file";
    }
    return "unknown";
}

LosslessSyntaxTree build_lossless_syntax_tree(const std::span<const Token> tokens)
{
    std::vector<Token> owned_tokens;
    owned_tokens.assign(tokens.begin(), tokens.end());
    return LosslessSyntaxTree{LosslessNodeKind::source_file, lossless_root_range(tokens), std::move(owned_tokens)};
}

std::string dump_lossless_syntax_tree(const LosslessSyntaxTree& tree)
{
    std::ostringstream out;
    const base::SourceRange range = tree.range();
    out << lossless_node_kind_name(tree.root_kind()) << " " << range.begin << ".." << range.end
        << " tokens=" << tree.token_count() << " trivia=" << tree.trivia_token_count()
        << " semantic=" << tree.semantic_token_count() << "\n";
    for (const Token& token : tree.tokens()) {
        out << "  " << token.range.begin << ".." << token.range.end << " " << token_kind_name(token.kind);
        const std::string_view text = token.text();
        if (!text.empty()) {
            out << " " << LOSSLESS_DUMP_TEXT_QUOTE;
            append_escaped_text(out, text);
            out << LOSSLESS_DUMP_TEXT_QUOTE;
        }
        out << "\n";
    }
    return out.str();
}

} // namespace aurex::syntax
