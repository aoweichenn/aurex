#pragma once

#include <aurex/frontend/syntax/core/token.hpp>
#include <aurex/infrastructure/base/integer.hpp>

#include <array>
#include <span>

namespace aurex::parse {

class TokenCursor final {
public:
    explicit TokenCursor(std::span<const syntax::Token> tokens) noexcept
        : tokens_(tokens.empty() ? std::span<const syntax::Token>{TOKEN_CURSOR_SYNTHETIC_EOF} : tokens)
    {
    }

    [[nodiscard]] bool is_eof() const noexcept
    {
        return this->peek().kind == syntax::TokenKind::eof;
    }

    [[nodiscard]] const syntax::Token& peek() const noexcept
    {
        if (this->current_ >= this->tokens_.size()) {
            return this->tokens_.back();
        }
        return this->tokens_[this->current_];
    }

    [[nodiscard]] const syntax::Token& previous() const noexcept
    {
        if (this->current_ == 0) {
            return this->tokens_.front();
        }
        return this->tokens_[this->current_ - 1];
    }

    [[nodiscard]] std::span<const syntax::Token> tokens() const noexcept
    {
        return this->tokens_;
    }

    [[nodiscard]] base::usize position() const noexcept
    {
        return this->current_;
    }

    [[nodiscard]] bool check(const syntax::TokenKind kind) const noexcept
    {
        return this->peek().kind == kind;
    }

    [[nodiscard]] bool check_next(const syntax::TokenKind kind) const noexcept
    {
        const base::usize next = this->current_ + 1;
        if (next >= this->tokens_.size()) {
            return false;
        }
        return this->tokens_[next].kind == kind;
    }

    [[nodiscard]] const syntax::Token& peek_at(const base::usize offset) const noexcept
    {
        const base::usize index = this->current_ + offset;
        if (index >= this->tokens_.size()) {
            return this->tokens_.back();
        }
        return this->tokens_[index];
    }

    [[nodiscard]] base::usize mark() const noexcept
    {
        return this->current_;
    }

    void rewind(const base::usize position) noexcept
    {
        this->current_ = position < this->tokens_.size() ? position : this->tokens_.size() - 1;
    }

    bool match(const syntax::TokenKind kind) noexcept
    {
        if (!this->check(kind)) {
            return false;
        }
        this->advance();
        return true;
    }

    const syntax::Token& advance() noexcept
    {
        if (!this->is_eof()) {
            ++this->current_;
        }
        return this->previous();
    }

private:
    inline static const std::array<syntax::Token, 1> TOKEN_CURSOR_SYNTHETIC_EOF{
        syntax::Token{syntax::TokenKind::eof, base::SourceRange{}, std::string_view{}},
    };

    std::span<const syntax::Token> tokens_;
    base::usize current_ = 0;
};

} // namespace aurex::parse
