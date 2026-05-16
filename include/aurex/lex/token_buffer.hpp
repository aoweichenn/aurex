#pragma once

#include <aurex/base/bump_allocator.hpp>
#include <aurex/syntax/token.hpp>

#include <memory>
#include <span>

namespace aurex::lex {

class TokenBuffer final {
public:
    TokenBuffer();
    TokenBuffer(const TokenBuffer& other);
    TokenBuffer& operator=(const TokenBuffer& other);
    TokenBuffer(TokenBuffer&& other) noexcept;
    TokenBuffer& operator=(TokenBuffer&& other) noexcept;
    ~TokenBuffer() = default;

    void reserve(base::usize token_count);
    void push_back(const syntax::Token& token);

    [[nodiscard]] base::usize size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;
    [[nodiscard]] std::span<const syntax::Token> span() const noexcept;

    [[nodiscard]] const syntax::Token& operator[](base::usize index) const noexcept;
    [[nodiscard]] const syntax::Token& front() const noexcept;
    [[nodiscard]] const syntax::Token& back() const noexcept;
    [[nodiscard]] const syntax::Token* begin() const noexcept;
    [[nodiscard]] const syntax::Token* end() const noexcept;
    [[nodiscard]] operator std::span<const syntax::Token>() const noexcept;

private:
    void ensure_storage();
    void swap(TokenBuffer& other) noexcept;

    std::unique_ptr<base::BumpAllocator> arena_;
    base::BumpVector<syntax::Token> tokens_;
};

} // namespace aurex::lex
