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
constexpr base::usize LOSSLESS_SOURCE_FILE_NODE_INDEX = 0;
constexpr base::usize LOSSLESS_TOKEN_STREAM_NODE_INDEX = 1;

struct LosslessBuildNode {
    LosslessNodeKind kind = LosslessNodeKind::source_file;
    base::SourceRange range{};
    std::vector<LosslessElement> children;
    bool has_range = false;
};

struct LosslessBuildResult {
    LosslessNodeId root_id{};
    std::vector<LosslessNode> nodes;
    std::vector<LosslessElement> elements;
};

struct LosslessTopLevelStart {
    bool matched = false;
    LosslessNodeKind kind = LosslessNodeKind::token_stream;
    bool stop_after_container = false;
};

struct LosslessTopLevelMatch {
    bool matched = false;
    LosslessNodeKind kind = LosslessNodeKind::token_stream;
    base::usize end = 0;
};

[[nodiscard]] bool lossless_tokens_are_monotonic(const std::span<const Token> tokens) noexcept
{
    if (tokens.empty()) {
        return true;
    }

    base::usize previous_begin = tokens.front().range.begin;
    for (base::usize index = 1; index < tokens.size(); ++index) {
        if (tokens[index].range.begin < previous_begin) {
            return false;
        }
        previous_begin = tokens[index].range.begin;
    }
    return true;
}

[[nodiscard]] bool lossless_token_is_grammar_visible(const TokenKind kind) noexcept
{
    return kind != TokenKind::eof && !is_trivia_token(kind);
}

[[nodiscard]] base::usize next_grammar_token(
    const std::span<const Token> tokens, base::usize index, const base::usize limit) noexcept
{
    const base::usize capped_limit = std::min(limit, tokens.size());
    while (index < capped_limit && !lossless_token_is_grammar_visible(tokens[index].kind)) {
        ++index;
    }
    return index;
}

[[nodiscard]] base::usize next_grammar_token_after(
    const std::span<const Token> tokens, const base::usize index, const base::usize limit) noexcept
{
    return next_grammar_token(tokens, index + 1U, limit);
}

[[nodiscard]] bool lossless_token_is_visibility(const TokenKind kind) noexcept
{
    return kind == TokenKind::kw_pub || kind == TokenKind::kw_priv;
}

[[nodiscard]] bool token_opens_lossless_group(const TokenKind kind) noexcept
{
    return kind == TokenKind::l_paren || kind == TokenKind::l_brace || kind == TokenKind::l_bracket;
}

[[nodiscard]] TokenKind matching_lossless_group_close(const TokenKind kind) noexcept
{
    if (kind == TokenKind::l_paren) {
        return TokenKind::r_paren;
    }
    if (kind == TokenKind::l_brace) {
        return TokenKind::r_brace;
    }
    return TokenKind::r_bracket;
}

[[nodiscard]] LosslessNodeKind lossless_group_kind_for_open_token(const TokenKind kind) noexcept
{
    if (kind == TokenKind::l_paren) {
        return LosslessNodeKind::paren_group;
    }
    if (kind == TokenKind::l_bracket) {
        return LosslessNodeKind::bracket_group;
    }
    return LosslessNodeKind::brace_group;
}

[[nodiscard]] LosslessTopLevelStart classify_lossless_top_level_start(
    const std::span<const Token> tokens, const base::usize start) noexcept
{
    const base::usize limit = tokens.size();
    base::usize cursor = next_grammar_token(tokens, start, limit);

    const TokenKind first_kind = tokens[cursor].kind;
    if (first_kind == TokenKind::kw_module) {
        return LosslessTopLevelStart{true, LosslessNodeKind::module_decl, false};
    }
    if (first_kind == TokenKind::kw_import) {
        return LosslessTopLevelStart{true, LosslessNodeKind::import_decl, false};
    }

    if (lossless_token_is_visibility(first_kind)) {
        const base::usize after_visibility = next_grammar_token_after(tokens, cursor, limit);
        if (after_visibility < limit && tokens[after_visibility].kind == TokenKind::kw_import) {
            return LosslessTopLevelStart{true, LosslessNodeKind::import_decl, false};
        }
        cursor = after_visibility;
    }

    if (cursor >= limit) {
        return {};
    }

    if (tokens[cursor].kind == TokenKind::kw_export) {
        cursor = next_grammar_token_after(tokens, cursor, limit);
        if (cursor < limit && tokens[cursor].kind == TokenKind::identifier) {
            cursor = next_grammar_token_after(tokens, cursor, limit);
        }
    }

    if (cursor < limit && tokens[cursor].kind == TokenKind::kw_extern) {
        return LosslessTopLevelStart{true, LosslessNodeKind::extern_block, true};
    }

    if (cursor < limit && tokens[cursor].kind == TokenKind::kw_unsafe) {
        cursor = next_grammar_token_after(tokens, cursor, limit);
    }

    if (cursor >= limit) {
        return {};
    }

    switch (tokens[cursor].kind) {
        case TokenKind::kw_fn:
            return LosslessTopLevelStart{true, LosslessNodeKind::function_decl, true};
        case TokenKind::kw_struct:
            return LosslessTopLevelStart{true, LosslessNodeKind::struct_decl, true};
        case TokenKind::kw_enum:
            return LosslessTopLevelStart{true, LosslessNodeKind::enum_decl, true};
        case TokenKind::kw_impl:
            return LosslessTopLevelStart{true, LosslessNodeKind::impl_block, true};
        case TokenKind::kw_const:
            return LosslessTopLevelStart{true, LosslessNodeKind::const_decl, false};
        case TokenKind::kw_type:
            return LosslessTopLevelStart{true, LosslessNodeKind::type_alias_decl, false};
        case TokenKind::kw_opaque:
            return LosslessTopLevelStart{true, LosslessNodeKind::opaque_struct_decl, false};
        default:
            return {};
    }
}

[[nodiscard]] base::usize scan_lossless_top_level_end(
    const std::span<const Token> tokens, const base::usize start, const bool stop_after_container) noexcept
{
    base::usize paren_depth = 0;
    base::usize bracket_depth = 0;
    base::usize brace_depth = 0;
    bool saw_top_level_container = false;

    for (base::usize index = start; index < tokens.size(); ++index) {
        switch (tokens[index].kind) {
            case TokenKind::eof:
                return index;
            case TokenKind::l_paren:
                ++paren_depth;
                break;
            case TokenKind::r_paren:
                if (paren_depth > 0U) {
                    --paren_depth;
                }
                break;
            case TokenKind::l_bracket:
                ++bracket_depth;
                break;
            case TokenKind::r_bracket:
                if (bracket_depth > 0U) {
                    --bracket_depth;
                }
                break;
            case TokenKind::l_brace:
                if (paren_depth == 0U && bracket_depth == 0U && brace_depth == 0U) {
                    saw_top_level_container = true;
                }
                ++brace_depth;
                break;
            case TokenKind::r_brace:
                if (brace_depth > 0U) {
                    --brace_depth;
                }
                if (stop_after_container && saw_top_level_container && paren_depth == 0U && bracket_depth == 0U
                    && brace_depth == 0U) {
                    return index + 1U;
                }
                break;
            case TokenKind::semicolon:
                if (paren_depth == 0U && bracket_depth == 0U && brace_depth == 0U) {
                    return index + 1U;
                }
                break;
            default:
                break;
        }
    }
    return tokens.size();
}

[[nodiscard]] LosslessTopLevelMatch match_lossless_top_level_node(
    const std::span<const Token> tokens, const base::usize start) noexcept
{
    const LosslessTopLevelStart classified = classify_lossless_top_level_start(tokens, start);
    if (!classified.matched) {
        return {};
    }

    const base::usize end = scan_lossless_top_level_end(tokens, start, classified.stop_after_container);
    return LosslessTopLevelMatch{true, classified.kind, end};
}

void include_lossless_range(LosslessBuildNode& node, const base::SourceRange range) noexcept
{
    if (!node.has_range) {
        node.range = range;
        node.has_range = true;
        return;
    }
    node.range.begin = std::min(node.range.begin, range.begin);
    node.range.end = std::max(node.range.end, range.end);
}

class LosslessTreeBuilder final {
public:
    LosslessTreeBuilder(const std::span<const Token> tokens, const LosslessNodeKind root_kind) : tokens_(tokens)
    {
        this->nodes_.push_back(LosslessBuildNode{root_kind, {}, {}, false});
    }

    [[nodiscard]] LosslessNodeId root_id() const noexcept
    {
        return LosslessNodeId{LOSSLESS_SOURCE_FILE_NODE_INDEX};
    }

    void add_token_child(const LosslessNodeId parent, const base::usize token_index)
    {
        this->nodes_[parent.value].children.push_back(LosslessElement::token(LosslessTokenId{token_index}));
    }

    void add_node_child(const LosslessNodeId parent, const LosslessNodeId child)
    {
        this->nodes_[parent.value].children.push_back(LosslessElement::node(child));
    }

    [[nodiscard]] LosslessNodeId build_range_node(
        const LosslessNodeKind kind, const base::usize begin, const base::usize end)
    {
        struct OpenNode {
            LosslessNodeId id{};
            TokenKind close = TokenKind::invalid;
        };

        const LosslessNodeId node_id = this->append_node(kind);
        std::vector<OpenNode> stack;
        stack.reserve(end - begin + 1U);
        stack.push_back(OpenNode{node_id, TokenKind::invalid});

        for (base::usize index = begin; index < end; ++index) {
            const TokenKind token_kind = this->tokens_[index].kind;
            if (token_opens_lossless_group(token_kind)) {
                LosslessNodeKind group_kind = lossless_group_kind_for_open_token(token_kind);
                if (kind == LosslessNodeKind::function_decl && token_kind == TokenKind::l_brace && stack.size() == 1U) {
                    group_kind = LosslessNodeKind::block;
                }
                const LosslessNodeId group_id = this->append_node(group_kind);
                this->add_node_child(stack.back().id, group_id);
                this->add_token_child(group_id, index);
                stack.push_back(OpenNode{group_id, matching_lossless_group_close(token_kind)});
                continue;
            }
            if (stack.size() > 1U && token_kind == stack.back().close) {
                this->add_token_child(stack.back().id, index);
                stack.pop_back();
                continue;
            }
            this->add_token_child(stack.back().id, index);
        }
        return node_id;
    }

    [[nodiscard]] LosslessBuildResult finish(const base::SourceRange root_range)
    {
        this->compute_ranges(root_range);

        LosslessBuildResult result;
        result.root_id = this->root_id();
        result.nodes.reserve(this->nodes_.size());
        result.elements.reserve(this->child_element_count());

        for (const LosslessBuildNode& node : this->nodes_) {
            result.nodes.push_back(LosslessNode{
                node.kind,
                node.range,
                result.elements.size(),
                node.children.size(),
            });
            result.elements.insert(result.elements.end(), node.children.begin(), node.children.end());
        }
        return result;
    }

private:
    [[nodiscard]] LosslessNodeId append_node(const LosslessNodeKind kind)
    {
        const LosslessNodeId id{this->nodes_.size()};
        this->nodes_.push_back(LosslessBuildNode{kind, {}, {}, false});
        return id;
    }

    [[nodiscard]] base::usize child_element_count() const noexcept
    {
        base::usize count = 0;
        for (const LosslessBuildNode& node : this->nodes_) {
            count += node.children.size();
        }
        return count;
    }

    void compute_ranges(const base::SourceRange root_range) noexcept
    {
        for (base::usize reverse_index = this->nodes_.size(); reverse_index > 0U; --reverse_index) {
            const base::usize node_index = reverse_index - 1U;
            LosslessBuildNode& node = this->nodes_[node_index];
            node.range = {};
            node.has_range = false;
            for (const LosslessElement child : node.children) {
                if (child.is_token()) {
                    const Token& token = this->tokens_[child.token_id().value];
                    if (token.kind != TokenKind::eof) {
                        include_lossless_range(node, token.range);
                    }
                    continue;
                }
                const LosslessBuildNode& child_node = this->nodes_[child.node_id().value];
                if (child_node.has_range) {
                    include_lossless_range(node, child_node.range);
                }
            }
        }
        LosslessBuildNode& root = this->nodes_[this->root_id().value];
        root.range = root_range;
        root.has_range = true;
    }

    std::span<const Token> tokens_;
    std::vector<LosslessBuildNode> nodes_;
};

[[nodiscard]] LosslessBuildResult build_structured_lossless_tree(
    const LosslessNodeKind root_kind, const base::SourceRange root_range, const std::span<const Token> tokens)
{
    LosslessTreeBuilder builder(tokens, root_kind);
    base::usize index = 0;
    while (index < tokens.size()) {
        if (lossless_token_is_grammar_visible(tokens[index].kind)) {
            const LosslessTopLevelMatch top_level = match_lossless_top_level_node(tokens, index);
            if (top_level.matched) {
                const LosslessNodeId child = builder.build_range_node(top_level.kind, index, top_level.end);
                builder.add_node_child(builder.root_id(), child);
                index = top_level.end;
                continue;
            }
        }
        builder.add_token_child(builder.root_id(), index);
        ++index;
    }
    return builder.finish(root_range);
}

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

void append_indent(std::ostringstream& out, const base::usize depth)
{
    for (base::usize index = 0; index < depth; ++index) {
        out << "  ";
    }
}

void append_token(std::ostringstream& out, const Token& token)
{
    out << token.range.begin << ".." << token.range.end << " " << token_kind_name(token.kind);
    const std::string_view text = token.text();
    if (!text.empty()) {
        out << " " << LOSSLESS_DUMP_TEXT_QUOTE;
        append_escaped_text(out, text);
        out << LOSSLESS_DUMP_TEXT_QUOTE;
    }
}

void append_node_summary(std::ostringstream& out, const LosslessNode& node)
{
    out << lossless_node_kind_name(node.kind) << " " << node.range.begin << ".." << node.range.end
        << " children=" << node.child_count;
}

} // namespace

LosslessSyntaxTree::LosslessSyntaxTree()
{
    this->rebuild_structured_tree(LosslessNodeKind::source_file, {});
}

LosslessSyntaxTree::LosslessSyntaxTree(
    const LosslessNodeKind root_kind, const base::SourceRange root_range, std::vector<Token> tokens)
    : tokens_(std::move(tokens))
{
    if (lossless_tokens_are_monotonic(this->tokens_)) {
        this->rebuild_structured_tree(root_kind, root_range);
        return;
    }
    this->rebuild_token_stream(root_kind, root_range);
}

void LosslessSyntaxTree::rebuild_structured_tree(const LosslessNodeKind root_kind, const base::SourceRange root_range)
{
    LosslessBuildResult result = build_structured_lossless_tree(root_kind, root_range, this->tokens_);
    this->root_id_ = result.root_id;
    this->nodes_ = std::move(result.nodes);
    this->elements_ = std::move(result.elements);
}

void LosslessSyntaxTree::rebuild_token_stream(const LosslessNodeKind root_kind, const base::SourceRange root_range)
{
    this->nodes_.clear();
    this->elements_.clear();
    this->root_id_ = LosslessNodeId{LOSSLESS_SOURCE_FILE_NODE_INDEX};

    this->nodes_.reserve(2U);
    this->elements_.reserve(this->tokens_.size() + 1U);

    this->nodes_.push_back(LosslessNode{
        root_kind,
        root_range,
        0U,
        1U,
    });

    this->elements_.push_back(LosslessElement::node(LosslessNodeId{LOSSLESS_TOKEN_STREAM_NODE_INDEX}));
    this->nodes_.push_back(LosslessNode{
        LosslessNodeKind::token_stream,
        root_range,
        this->elements_.size(),
        this->tokens_.size(),
    });
    for (base::usize index = 0; index < this->tokens_.size(); ++index) {
        this->elements_.push_back(LosslessElement::token(LosslessTokenId{index}));
    }
}

LosslessNodeId LosslessSyntaxTree::root_id() const noexcept
{
    return this->root_id_;
}

const LosslessNode* LosslessSyntaxTree::root_node() const noexcept
{
    return this->node(this->root_id_);
}

const LosslessNode* LosslessSyntaxTree::node(const LosslessNodeId id) const noexcept
{
    if (id.value >= this->nodes_.size()) {
        return nullptr;
    }
    return &this->nodes_[id.value];
}

const Token* LosslessSyntaxTree::token(const LosslessTokenId id) const noexcept
{
    if (id.value >= this->tokens_.size()) {
        return nullptr;
    }
    return &this->tokens_[id.value];
}

LosslessNodeKind LosslessSyntaxTree::root_kind() const noexcept
{
    return this->nodes_[this->root_id_.value].kind;
}

base::SourceRange LosslessSyntaxTree::range() const noexcept
{
    return this->nodes_[this->root_id_.value].range;
}

std::span<const LosslessNode> LosslessSyntaxTree::nodes() const noexcept
{
    return this->nodes_;
}

std::span<const LosslessElement> LosslessSyntaxTree::elements() const noexcept
{
    return this->elements_;
}

std::span<const LosslessElement> LosslessSyntaxTree::children(const LosslessNodeId id) const noexcept
{
    const LosslessNode* parent = this->node(id);
    if (parent == nullptr || parent->first_child > this->elements_.size()) {
        return {};
    }
    const base::usize available = this->elements_.size() - parent->first_child;
    const base::usize count = std::min(parent->child_count, available);
    return std::span<const LosslessElement>{this->elements_.data() + parent->first_child, count};
}

std::span<const Token> LosslessSyntaxTree::tokens() const noexcept
{
    return this->tokens_;
}

LosslessTokenId LosslessSyntaxTree::token_at_offset(const base::usize offset) const noexcept
{
    for (base::usize index = 0; index < this->tokens_.size(); ++index) {
        const Token& token = this->tokens_[index];
        if (token.range.empty()) {
            if (offset == token.range.begin) {
                return LosslessTokenId{index};
            }
            continue;
        }
        if (token.range.begin <= offset && offset < token.range.end) {
            return LosslessTokenId{index};
        }
    }
    return INVALID_LOSSLESS_TOKEN_ID;
}

base::usize LosslessSyntaxTree::node_count() const noexcept
{
    return this->nodes_.size();
}

base::usize LosslessSyntaxTree::element_count() const noexcept
{
    return this->elements_.size();
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
        case LosslessNodeKind::module_decl:
            return "module_decl";
        case LosslessNodeKind::import_decl:
            return "import_decl";
        case LosslessNodeKind::function_decl:
            return "function_decl";
        case LosslessNodeKind::struct_decl:
            return "struct_decl";
        case LosslessNodeKind::enum_decl:
            return "enum_decl";
        case LosslessNodeKind::impl_block:
            return "impl_block";
        case LosslessNodeKind::extern_block:
            return "extern_block";
        case LosslessNodeKind::const_decl:
            return "const_decl";
        case LosslessNodeKind::type_alias_decl:
            return "type_alias_decl";
        case LosslessNodeKind::opaque_struct_decl:
            return "opaque_struct_decl";
        case LosslessNodeKind::block:
            return "block";
        case LosslessNodeKind::paren_group:
            return "paren_group";
        case LosslessNodeKind::bracket_group:
            return "bracket_group";
        case LosslessNodeKind::brace_group:
            return "brace_group";
        case LosslessNodeKind::token_stream:
            return "token_stream";
    }
    return "unknown";
}

std::string_view lossless_element_kind_name(const LosslessElementKind kind) noexcept
{
    switch (kind) {
        case LosslessElementKind::node:
            return "node";
        case LosslessElementKind::token:
            return "token";
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
        << " semantic=" << tree.semantic_token_count() << " nodes=" << tree.node_count()
        << " elements=" << tree.element_count() << "\n";

    struct Frame {
        LosslessElement element;
        base::usize depth = 0;
    };
    std::vector<Frame> stack;
    const std::span<const LosslessElement> root_children = tree.children(tree.root_id());
    stack.reserve(tree.element_count());
    for (base::usize offset = root_children.size(); offset > 0; --offset) {
        stack.push_back(Frame{root_children[offset - 1U], 1U});
    }

    while (!stack.empty()) {
        const Frame frame = stack.back();
        stack.pop_back();
        append_indent(out, frame.depth);
        if (frame.element.is_token()) {
            append_token(out, tree.tokens()[frame.element.token_id().value]);
            out << "\n";
            continue;
        }

        const LosslessNode& node = tree.nodes()[frame.element.node_id().value];
        append_node_summary(out, node);
        out << "\n";

        const std::span<const LosslessElement> children = tree.children(frame.element.node_id());
        for (base::usize offset = children.size(); offset > 0; --offset) {
            stack.push_back(Frame{children[offset - 1U], frame.depth + 1U});
        }
    }
    return out.str();
}

} // namespace aurex::syntax
