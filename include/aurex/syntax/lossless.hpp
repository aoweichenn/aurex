#pragma once

#include <aurex/base/source.hpp>
#include <aurex/syntax/token.hpp>

#include <span>
#include <string>
#include <vector>

namespace aurex::syntax {

enum class LosslessNodeKind {
    source_file,
};

class LosslessSyntaxTree final {
public:
    LosslessSyntaxTree() = default;
    LosslessSyntaxTree(LosslessNodeKind root_kind, base::SourceRange root_range, std::vector<Token> tokens);

    [[nodiscard]] LosslessNodeKind root_kind() const noexcept;
    [[nodiscard]] base::SourceRange range() const noexcept;
    [[nodiscard]] std::span<const Token> tokens() const noexcept;
    [[nodiscard]] base::usize token_count() const noexcept;
    [[nodiscard]] base::usize trivia_token_count() const noexcept;
    [[nodiscard]] base::usize semantic_token_count() const noexcept;
    [[nodiscard]] std::string reconstruct_text() const;

private:
    LosslessNodeKind root_kind_ = LosslessNodeKind::source_file;
    base::SourceRange range_{};
    std::vector<Token> tokens_;
};

[[nodiscard]] std::string_view lossless_node_kind_name(LosslessNodeKind kind) noexcept;
[[nodiscard]] LosslessSyntaxTree build_lossless_syntax_tree(std::span<const Token> tokens);
[[nodiscard]] std::string dump_lossless_syntax_tree(const LosslessSyntaxTree& tree);

} // namespace aurex::syntax
