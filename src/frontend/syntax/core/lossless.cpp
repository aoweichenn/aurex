#include <aurex/frontend/syntax/core/lossless.hpp>

#include <algorithm>
#include <sstream>
#include <unordered_map>
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
constexpr base::u64 LOSSLESS_STABLE_HASH_OFFSET = 14695981039346656037ULL;
constexpr base::u64 LOSSLESS_STABLE_HASH_PRIME = 1099511628211ULL;
constexpr base::u64 LOSSLESS_STABLE_HASH_KIND_MARKER = 0xA11CE50000000001ULL;
constexpr base::u64 LOSSLESS_STABLE_HASH_TEXT_MARKER = 0xA11CE50000000002ULL;
constexpr base::u64 LOSSLESS_STABLE_HASH_ANCHOR_MARKER = 0xA11CE50000000003ULL;
constexpr base::u64 LOSSLESS_STABLE_HASH_EMPTY_ANCHOR = 0xA11CE50000000004ULL;

struct LosslessBuildNode {
    LosslessNodeKind kind = LosslessNodeKind::source_file;
    base::SourceRange range{};
    LosslessNodeId parent = INVALID_LOSSLESS_NODE_ID;
    base::usize first_token = 0;
    base::usize token_count = 0;
    std::vector<LosslessElement> children;
    bool has_range = false;
    bool has_tokens = false;
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

[[nodiscard]] LosslessBuildNode make_lossless_build_node(const LosslessNodeKind kind)
{
    LosslessBuildNode node;
    node.kind = kind;
    return node;
}

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

[[nodiscard]] base::u64 lossless_stable_mix_u64(base::u64 hash, const base::u64 value) noexcept
{
    for (base::usize shift = 0U; shift < sizeof(base::u64); ++shift) {
        hash ^= (value >> (shift * 8U)) & 0xffU;
        hash *= LOSSLESS_STABLE_HASH_PRIME;
    }
    return hash;
}

[[nodiscard]] base::u64 lossless_stable_mix_text(base::u64 hash, const std::string_view text) noexcept
{
    hash = lossless_stable_mix_u64(hash, LOSSLESS_STABLE_HASH_TEXT_MARKER);
    for (const unsigned char byte : text) {
        hash ^= static_cast<base::u64>(byte);
        hash *= LOSSLESS_STABLE_HASH_PRIME;
    }
    return hash;
}

[[nodiscard]] StableHash64 lossless_stable_subtree_hash(
    const LosslessNodeKind kind, const std::span<const Token> tokens) noexcept
{
    base::u64 hash = lossless_stable_mix_u64(LOSSLESS_STABLE_HASH_OFFSET, LOSSLESS_STABLE_HASH_KIND_MARKER);
    hash = lossless_stable_mix_u64(hash, static_cast<base::u64>(kind));
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::eof) {
            continue;
        }
        hash = lossless_stable_mix_u64(hash, static_cast<base::u64>(token.kind));
        hash = lossless_stable_mix_text(hash, token.text());
    }
    return StableHash64{hash};
}

[[nodiscard]] StableHash64 lossless_stable_anchor_hash(const std::span<const Token> tokens) noexcept
{
    base::u64 hash = lossless_stable_mix_u64(LOSSLESS_STABLE_HASH_OFFSET, LOSSLESS_STABLE_HASH_ANCHOR_MARKER);
    bool saw_anchor = false;
    for (const Token& token : tokens) {
        if (token.kind != TokenKind::identifier) {
            continue;
        }
        hash = lossless_stable_mix_text(hash, token.text());
        saw_anchor = true;
    }
    if (saw_anchor) {
        return StableHash64{hash};
    }
    return StableHash64{lossless_stable_mix_u64(hash, LOSSLESS_STABLE_HASH_EMPTY_ANCHOR)};
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
        case TokenKind::kw_trait:
            return LosslessTopLevelStart{true, LosslessNodeKind::trait_decl, true};
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

void include_lossless_token_span(
    LosslessBuildNode& node, const base::usize first_token, const base::usize token_count) noexcept
{
    if (token_count == 0U) {
        return;
    }
    if (!node.has_tokens) {
        node.first_token = first_token;
        node.token_count = token_count;
        node.has_tokens = true;
        return;
    }
    const base::usize current_end = node.first_token + node.token_count;
    const base::usize incoming_end = first_token + token_count;
    node.first_token = std::min(node.first_token, first_token);
    node.token_count = std::max(current_end, incoming_end) - node.first_token;
}

class LosslessTreeBuilder final {
public:
    LosslessTreeBuilder(const std::span<const Token> tokens, const LosslessNodeKind root_kind) : tokens_(tokens)
    {
        this->nodes_.push_back(make_lossless_build_node(root_kind));
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
        this->nodes_[child.value].parent = parent;
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
                node.parent,
                result.elements.size(),
                node.children.size(),
                node.first_token,
                node.token_count,
            });
            result.elements.insert(result.elements.end(), node.children.begin(), node.children.end());
        }
        return result;
    }

private:
    [[nodiscard]] LosslessNodeId append_node(const LosslessNodeKind kind)
    {
        const LosslessNodeId id{this->nodes_.size()};
        this->nodes_.push_back(make_lossless_build_node(kind));
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
                    include_lossless_token_span(node, child.token_id().value, 1U);
                    if (token.kind != TokenKind::eof) {
                        include_lossless_range(node, token.range);
                    }
                    continue;
                }
                const LosslessBuildNode& child_node = this->nodes_[child.node_id().value];
                include_lossless_token_span(node, child_node.first_token, child_node.token_count);
                if (child_node.has_range) {
                    include_lossless_range(node, child_node.range);
                }
            }
        }
        LosslessBuildNode& root = this->nodes_[this->root_id().value];
        root.range = root_range;
        root.has_range = true;
        root.first_token = 0U;
        root.token_count = this->tokens_.size();
        root.has_tokens = !this->tokens_.empty();
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
        << " children=" << node.child_count << " tokens=" << node.token_count;
}

[[nodiscard]] bool lossless_node_contains_offset(const LosslessNode& node, const base::usize offset) noexcept
{
    if (node.range.empty()) {
        return offset == node.range.begin;
    }
    return node.range.begin <= offset && offset < node.range.end;
}

[[nodiscard]] std::string reconstruct_lossless_token_text(const std::span<const Token> tokens)
{
    base::usize size = 0;
    for (const Token& token : tokens) {
        if (token.kind != TokenKind::eof) {
            size += token.text().size();
        }
    }

    std::string text;
    text.reserve(size);
    for (const Token& token : tokens) {
        if (token.kind != TokenKind::eof) {
            text += token.text();
        }
    }
    return text;
}

[[nodiscard]] base::usize lossless_semantic_token_count(const std::span<const Token> tokens) noexcept
{
    base::usize count = 0;
    for (const Token& token : tokens) {
        if (lossless_token_is_grammar_visible(token.kind)) {
            ++count;
        }
    }
    return count;
}

using LosslessStableKeyCountMap = std::unordered_map<LosslessNodeStableKey, base::usize, LosslessNodeStableKeyHash>;

void count_lossless_stable_key(LosslessStableKeyCountMap& counts, const std::optional<LosslessNodeStableKey>& key)
{
    if (!key.has_value()) {
        return;
    }
    counts[*key] += 1U;
}

[[nodiscard]] LosslessStableKeyCountMap lossless_stable_key_counts(const LosslessSyntaxTree& tree)
{
    LosslessStableKeyCountMap counts;
    counts.reserve(tree.node_count());
    for (base::usize index = 0; index < tree.node_count(); ++index) {
        count_lossless_stable_key(counts, tree.stable_node_key(LosslessNodeId{index}));
    }
    return counts;
}

[[nodiscard]] base::usize lossless_stable_key_collision_count(const LosslessStableKeyCountMap& counts) noexcept
{
    base::usize collisions = 0;
    for (const auto& entry : counts) {
        if (entry.second > 1U) {
            collisions += entry.second - 1U;
        }
    }
    return collisions;
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
        INVALID_LOSSLESS_NODE_ID,
        0U,
        1U,
        0U,
        this->tokens_.size(),
    });

    this->elements_.push_back(LosslessElement::node(LosslessNodeId{LOSSLESS_TOKEN_STREAM_NODE_INDEX}));
    this->nodes_.push_back(LosslessNode{
        LosslessNodeKind::token_stream,
        root_range,
        this->root_id_,
        this->elements_.size(),
        this->tokens_.size(),
        0U,
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

LosslessNodeId LosslessSyntaxTree::parent(const LosslessNodeId id) const noexcept
{
    const LosslessNode* current = this->node(id);
    if (current == nullptr) {
        return INVALID_LOSSLESS_NODE_ID;
    }
    return current->parent;
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

std::span<const Token> LosslessSyntaxTree::token_span(const LosslessNodeId id) const noexcept
{
    const LosslessNode* current = this->node(id);
    if (current == nullptr || current->first_token > this->tokens_.size() || current->token_count == 0U) {
        return {};
    }
    const base::usize available = this->tokens_.size() - current->first_token;
    const base::usize count = std::min(current->token_count, available);
    return std::span<const Token>{this->tokens_.data() + current->first_token, count};
}

std::optional<LosslessNodeKey> LosslessSyntaxTree::node_key(const LosslessNodeId id) const noexcept
{
    const LosslessNode* current = this->node(id);
    if (current == nullptr) {
        return std::nullopt;
    }

    base::usize depth = 0;
    LosslessNodeId parent_id = current->parent;
    while (parent_id != INVALID_LOSSLESS_NODE_ID) {
        if (depth >= this->nodes_.size()) {
            return std::nullopt;
        }
        const LosslessNode* parent_node = this->node(parent_id);
        if (parent_node == nullptr) {
            return std::nullopt;
        }
        ++depth;
        parent_id = parent_node->parent;
    }

    return LosslessNodeKey{
        current->kind,
        current->range,
        current->first_token,
        current->token_count,
        depth,
    };
}

std::optional<LosslessNodeStableKey> LosslessSyntaxTree::stable_node_key(const LosslessNodeId id) const noexcept
{
    const LosslessNode* current = this->node(id);
    if (current == nullptr) {
        return std::nullopt;
    }

    base::usize depth = 0;
    LosslessNodeId parent_id = current->parent;
    while (parent_id != INVALID_LOSSLESS_NODE_ID) {
        if (depth >= this->nodes_.size()) {
            return std::nullopt;
        }
        const LosslessNode* parent_node = this->node(parent_id);
        if (parent_node == nullptr) {
            return std::nullopt;
        }
        ++depth;
        parent_id = parent_node->parent;
    }

    const std::span<const Token> tokens = this->token_span(id);
    return LosslessNodeStableKey{
        current->kind,
        lossless_stable_anchor_hash(tokens),
        lossless_stable_subtree_hash(current->kind, tokens),
        current->token_count,
        lossless_semantic_token_count(tokens),
        current->child_count,
        depth,
    };
}

LosslessNodeId LosslessSyntaxTree::node_at_offset(const base::usize offset) const noexcept
{
    const LosslessNode* root = this->root_node();
    if (root == nullptr || !lossless_node_contains_offset(*root, offset)) {
        return INVALID_LOSSLESS_NODE_ID;
    }

    LosslessNodeId best = this->root_id_;
    bool advanced = true;
    while (advanced) {
        advanced = false;
        for (const LosslessElement child : this->children(best)) {
            if (!child.is_node()) {
                continue;
            }
            const LosslessNode* child_node = this->node(child.node_id());
            if (child_node != nullptr && lossless_node_contains_offset(*child_node, offset)) {
                best = child.node_id();
                advanced = true;
                break;
            }
        }
    }
    return best;
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

bool LosslessSyntaxTree::is_structurally_valid() const noexcept
{
    if (this->root_id_.value >= this->nodes_.size()) {
        return false;
    }
    if (this->nodes_[this->root_id_.value].parent != INVALID_LOSSLESS_NODE_ID) {
        return false;
    }
    for (base::usize node_index = 0; node_index < this->nodes_.size(); ++node_index) {
        const LosslessNode& current = this->nodes_[node_index];
        if (current.first_child > this->elements_.size()) {
            return false;
        }
        if (current.child_count > this->elements_.size() - current.first_child) {
            return false;
        }
        if (current.first_token > this->tokens_.size()) {
            return false;
        }
        if (current.token_count > this->tokens_.size() - current.first_token) {
            return false;
        }
        for (const LosslessElement child : this->children(LosslessNodeId{node_index})) {
            if (child.is_token()) {
                if (child.token_id().value >= this->tokens_.size()) {
                    return false;
                }
                continue;
            }
            if (child.node_id().value >= this->nodes_.size()) {
                return false;
            }
            if (this->nodes_[child.node_id().value].parent != LosslessNodeId{node_index}) {
                return false;
            }
        }
    }
    return true;
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
    return reconstruct_lossless_token_text(this->tokens_);
}

std::string LosslessSyntaxTree::reconstruct_text(const LosslessNodeId id) const
{
    return reconstruct_lossless_token_text(this->token_span(id));
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
        case LosslessNodeKind::trait_decl:
            return "trait_decl";
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

std::size_t LosslessNodeStableKeyHash::operator()(const LosslessNodeStableKey key) const noexcept
{
    base::u64 hash = lossless_stable_mix_u64(LOSSLESS_STABLE_HASH_OFFSET, static_cast<base::u64>(key.kind));
    hash = lossless_stable_mix_u64(hash, key.anchor_hash.value);
    hash = lossless_stable_mix_u64(hash, key.subtree_hash.value);
    hash = lossless_stable_mix_u64(hash, static_cast<base::u64>(key.token_count));
    hash = lossless_stable_mix_u64(hash, static_cast<base::u64>(key.semantic_token_count));
    hash = lossless_stable_mix_u64(hash, static_cast<base::u64>(key.child_count));
    hash = lossless_stable_mix_u64(hash, static_cast<base::u64>(key.depth));
    return static_cast<std::size_t>(hash);
}

LosslessSyntaxTree build_lossless_syntax_tree(const std::span<const Token> tokens)
{
    std::vector<Token> owned_tokens;
    owned_tokens.assign(tokens.begin(), tokens.end());
    return LosslessSyntaxTree{LosslessNodeKind::source_file, lossless_root_range(tokens), std::move(owned_tokens)};
}

LosslessSyntaxReuseStats compare_lossless_stable_nodes(
    const LosslessSyntaxTree& previous, const LosslessSyntaxTree& current)
{
    const LosslessStableKeyCountMap previous_counts = lossless_stable_key_counts(previous);
    const LosslessStableKeyCountMap current_counts = lossless_stable_key_counts(current);

    LosslessSyntaxReuseStats stats;
    stats.previous_nodes = previous.node_count();
    stats.current_nodes = current.node_count();
    stats.stable_key_collisions =
        lossless_stable_key_collision_count(previous_counts) + lossless_stable_key_collision_count(current_counts);
    for (const auto& entry : current_counts) {
        const auto previous_entry = previous_counts.find(entry.first);
        if (previous_entry == previous_counts.end()) {
            continue;
        }
        stats.reused_nodes += std::min(previous_entry->second, entry.second);
    }
    stats.recomputed_nodes = stats.current_nodes >= stats.reused_nodes ? stats.current_nodes - stats.reused_nodes : 0U;
    stats.invalidated_nodes =
        stats.previous_nodes >= stats.reused_nodes ? stats.previous_nodes - stats.reused_nodes : 0U;
    stats.reused = true;
    return stats;
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
