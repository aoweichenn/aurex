#pragma once

#include <aurex/base/source.hpp>
#include <aurex/syntax/token.hpp>

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace aurex::syntax {

enum class LosslessNodeKind {
    source_file,
    module_decl,
    import_decl,
    function_decl,
    struct_decl,
    enum_decl,
    impl_block,
    extern_block,
    const_decl,
    type_alias_decl,
    opaque_struct_decl,
    block,
    paren_group,
    bracket_group,
    brace_group,
    token_stream,
};

enum class LosslessElementKind {
    node,
    token,
};

struct LosslessNodeId {
    static constexpr base::usize INVALID_VALUE = static_cast<base::usize>(-1);

    base::usize value = INVALID_VALUE;

    [[nodiscard]] friend constexpr bool operator==(LosslessNodeId lhs, LosslessNodeId rhs) noexcept = default;
};

struct LosslessTokenId {
    static constexpr base::usize INVALID_VALUE = static_cast<base::usize>(-1);

    base::usize value = INVALID_VALUE;

    [[nodiscard]] friend constexpr bool operator==(LosslessTokenId lhs, LosslessTokenId rhs) noexcept = default;
};

inline constexpr LosslessNodeId INVALID_LOSSLESS_NODE_ID{};
inline constexpr LosslessTokenId INVALID_LOSSLESS_TOKEN_ID{};

struct LosslessElement {
    LosslessElementKind kind = LosslessElementKind::token;
    base::usize index = LosslessTokenId::INVALID_VALUE;

    [[nodiscard]] static constexpr LosslessElement node(LosslessNodeId id) noexcept;
    [[nodiscard]] static constexpr LosslessElement token(LosslessTokenId id) noexcept;
    [[nodiscard]] constexpr bool is_node() const noexcept;
    [[nodiscard]] constexpr bool is_token() const noexcept;
    [[nodiscard]] constexpr LosslessNodeId node_id() const noexcept;
    [[nodiscard]] constexpr LosslessTokenId token_id() const noexcept;

    [[nodiscard]] friend constexpr bool operator==(LosslessElement lhs, LosslessElement rhs) noexcept = default;
};

struct LosslessNode {
    LosslessNodeKind kind = LosslessNodeKind::source_file;
    base::SourceRange range{};
    LosslessNodeId parent = INVALID_LOSSLESS_NODE_ID;
    base::usize first_child = 0;
    base::usize child_count = 0;
    base::usize first_token = 0;
    base::usize token_count = 0;
};

struct LosslessNodeKey {
    LosslessNodeKind kind = LosslessNodeKind::source_file;
    base::SourceRange range{};
    base::usize first_token = 0;
    base::usize token_count = 0;
    base::usize depth = 0;

    [[nodiscard]] friend constexpr bool operator==(LosslessNodeKey lhs, LosslessNodeKey rhs) noexcept
    {
        return lhs.kind == rhs.kind && lhs.range.source.value == rhs.range.source.value
            && lhs.range.begin == rhs.range.begin && lhs.range.end == rhs.range.end
            && lhs.first_token == rhs.first_token && lhs.token_count == rhs.token_count && lhs.depth == rhs.depth;
    }
};

class LosslessSyntaxTree final {
public:
    LosslessSyntaxTree();
    LosslessSyntaxTree(LosslessNodeKind root_kind, base::SourceRange root_range, std::vector<Token> tokens);

    [[nodiscard]] LosslessNodeId root_id() const noexcept;
    [[nodiscard]] const LosslessNode* root_node() const noexcept;
    [[nodiscard]] const LosslessNode* node(LosslessNodeId id) const noexcept;
    [[nodiscard]] LosslessNodeId parent(LosslessNodeId id) const noexcept;
    [[nodiscard]] const Token* token(LosslessTokenId id) const noexcept;
    [[nodiscard]] LosslessNodeKind root_kind() const noexcept;
    [[nodiscard]] base::SourceRange range() const noexcept;
    [[nodiscard]] std::span<const LosslessNode> nodes() const noexcept;
    [[nodiscard]] std::span<const LosslessElement> elements() const noexcept;
    [[nodiscard]] std::span<const LosslessElement> children(LosslessNodeId id) const noexcept;
    [[nodiscard]] std::span<const Token> tokens() const noexcept;
    [[nodiscard]] std::span<const Token> token_span(LosslessNodeId id) const noexcept;
    [[nodiscard]] std::optional<LosslessNodeKey> node_key(LosslessNodeId id) const noexcept;
    [[nodiscard]] LosslessNodeId node_at_offset(base::usize offset) const noexcept;
    [[nodiscard]] LosslessTokenId token_at_offset(base::usize offset) const noexcept;
    [[nodiscard]] bool is_structurally_valid() const noexcept;
    [[nodiscard]] base::usize node_count() const noexcept;
    [[nodiscard]] base::usize element_count() const noexcept;
    [[nodiscard]] base::usize token_count() const noexcept;
    [[nodiscard]] base::usize trivia_token_count() const noexcept;
    [[nodiscard]] base::usize semantic_token_count() const noexcept;
    [[nodiscard]] std::string reconstruct_text() const;
    [[nodiscard]] std::string reconstruct_text(LosslessNodeId id) const;

private:
    void rebuild_structured_tree(LosslessNodeKind root_kind, base::SourceRange root_range);
    void rebuild_token_stream(LosslessNodeKind root_kind, base::SourceRange root_range);

    LosslessNodeId root_id_{};
    std::vector<LosslessNode> nodes_;
    std::vector<LosslessElement> elements_;
    std::vector<Token> tokens_;
};

constexpr LosslessElement LosslessElement::node(const LosslessNodeId id) noexcept
{
    return LosslessElement{LosslessElementKind::node, id.value};
}

constexpr LosslessElement LosslessElement::token(const LosslessTokenId id) noexcept
{
    return LosslessElement{LosslessElementKind::token, id.value};
}

constexpr bool LosslessElement::is_node() const noexcept
{
    return this->kind == LosslessElementKind::node;
}

constexpr bool LosslessElement::is_token() const noexcept
{
    return this->kind == LosslessElementKind::token;
}

constexpr LosslessNodeId LosslessElement::node_id() const noexcept
{
    return this->is_node() ? LosslessNodeId{this->index} : INVALID_LOSSLESS_NODE_ID;
}

constexpr LosslessTokenId LosslessElement::token_id() const noexcept
{
    return this->is_token() ? LosslessTokenId{this->index} : INVALID_LOSSLESS_TOKEN_ID;
}

[[nodiscard]] std::string_view lossless_node_kind_name(LosslessNodeKind kind) noexcept;
[[nodiscard]] std::string_view lossless_element_kind_name(LosslessElementKind kind) noexcept;
[[nodiscard]] LosslessSyntaxTree build_lossless_syntax_tree(std::span<const Token> tokens);
[[nodiscard]] std::string dump_lossless_syntax_tree(const LosslessSyntaxTree& tree);

} // namespace aurex::syntax
