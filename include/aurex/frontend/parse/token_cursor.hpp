#pragma once

#include <aurex/frontend/syntax/core/token.hpp>
#include <aurex/infrastructure/base/integer.hpp>

#include <array>
#include <deque>
#include <span>

namespace aurex::parse {

inline constexpr base::usize TOKEN_CURSOR_MAX_PENDING_TOKENS = 2;

struct TokenCursorMark final {
    base::usize current = 0;
    base::usize pending_index = 0;
    base::usize pending_count = 0;
    std::array<syntax::Token, TOKEN_CURSOR_MAX_PENDING_TOKENS> pending_tokens{};
    syntax::Token previous_token{};
    bool has_previous_token = false;
};

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
        if (this->has_pending_tokens()) {
            return this->pending_tokens_[this->pending_index_];
        }
        if (this->current_ >= this->tokens_.size()) {
            return this->tokens_.back();
        }
        return this->tokens_[this->current_];
    }

    [[nodiscard]] const syntax::Token& previous() const noexcept
    {
        if (this->previous_token_ref_ != nullptr) {
            return *this->previous_token_ref_;
        }
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
        const base::usize remaining_pending = this->pending_remaining();
        if (remaining_pending > 1) {
            return this->pending_tokens_[this->pending_index_ + 1].kind == kind;
        }
        const base::usize token_offset = remaining_pending == 0 ? 1 : 0;
        const base::usize index = this->current_ + token_offset;
        if (index >= this->tokens_.size()) {
            return false;
        }
        return this->tokens_[index].kind == kind;
    }

    [[nodiscard]] const syntax::Token& peek_at(const base::usize offset) const noexcept
    {
        const base::usize remaining_pending = this->pending_remaining();
        if (offset < remaining_pending) {
            return this->pending_tokens_[this->pending_index_ + offset];
        }
        const base::usize index = this->current_ + (offset - remaining_pending);
        if (index >= this->tokens_.size()) {
            return this->tokens_.back();
        }
        return this->tokens_[index];
    }

    [[nodiscard]] TokenCursorMark mark() const noexcept
    {
        return TokenCursorMark{
            this->current_,
            this->pending_index_,
            this->pending_count_,
            this->pending_tokens_,
            this->previous_token_,
            this->has_previous_token_,
        };
    }

    void rewind(const TokenCursorMark mark) noexcept
    {
        this->current_ = mark.current < this->tokens_.size() ? mark.current : this->tokens_.size() - 1;
        this->pending_index_ = mark.pending_index;
        this->pending_count_ = mark.pending_count;
        this->pending_tokens_ = mark.pending_tokens;
        this->previous_token_ = mark.previous_token;
        this->has_previous_token_ = mark.has_previous_token;
        this->previous_token_ref_ = this->has_previous_token_ ? &this->previous_token_ : nullptr;
    }

    void rewind(const base::usize position) noexcept
    {
        this->current_ = position < this->tokens_.size() ? position : this->tokens_.size() - 1;
        this->pending_index_ = 0;
        this->pending_count_ = 0;
        if (this->current_ == 0) {
            this->has_previous_token_ = false;
            this->previous_token_ = {};
            this->previous_token_ref_ = nullptr;
            return;
        }
        this->previous_token_ = this->tokens_[this->current_ - 1];
        this->has_previous_token_ = true;
        this->previous_token_ref_ = &this->tokens_[this->current_ - 1];
    }

    bool match(const syntax::TokenKind kind) noexcept
    {
        if (!this->check(kind)) {
            return false;
        }
        this->advance();
        return true;
    }

    [[nodiscard]] bool check_generic_right_angle() const noexcept
    {
        switch (this->peek().kind) {
            case syntax::TokenKind::greater:
            case syntax::TokenKind::greater_equal:
            case syntax::TokenKind::greater_greater:
            case syntax::TokenKind::greater_greater_equal:
                return true;
            default:
                return false;
        }
    }

    [[nodiscard]] bool check_generic_left_angle() const noexcept
    {
        switch (this->peek().kind) {
            case syntax::TokenKind::less:
            case syntax::TokenKind::less_equal:
                return true;
            default:
                return false;
        }
    }

    bool match_generic_left_angle() noexcept
    {
        if (!this->check_generic_left_angle()) {
            return false;
        }
        if (this->peek().kind == syntax::TokenKind::less) {
            this->advance();
            return true;
        }

        const syntax::Token& token = this->peek();
        this->pending_index_ = 0;
        this->pending_count_ = 0;
        this->pending_tokens_[this->pending_count_++] =
            split_token(token, syntax::TokenKind::equal, TOKEN_CURSOR_SECOND_CHAR_OFFSET);
        this->record_synthetic_previous(split_token(token, syntax::TokenKind::less, TOKEN_CURSOR_FIRST_CHAR_OFFSET));
        ++this->current_;
        return true;
    }

    bool match_generic_right_angle() noexcept
    {
        if (!this->check_generic_right_angle()) {
            return false;
        }
        if (this->peek().kind == syntax::TokenKind::greater) {
            this->advance();
            return true;
        }

        const syntax::Token& token = this->peek();
        this->pending_index_ = 0;
        this->pending_count_ = 0;
        switch (token.kind) {
            case syntax::TokenKind::greater_equal:
                this->pending_tokens_[this->pending_count_++] =
                    split_token(token, syntax::TokenKind::equal, TOKEN_CURSOR_SECOND_CHAR_OFFSET);
                break;
            case syntax::TokenKind::greater_greater:
                this->pending_tokens_[this->pending_count_++] =
                    split_token(token, syntax::TokenKind::greater, TOKEN_CURSOR_SECOND_CHAR_OFFSET);
                break;
            case syntax::TokenKind::greater_greater_equal:
                this->pending_tokens_[this->pending_count_++] =
                    split_token(token, syntax::TokenKind::greater, TOKEN_CURSOR_SECOND_CHAR_OFFSET);
                this->pending_tokens_[this->pending_count_++] =
                    split_token(token, syntax::TokenKind::equal, TOKEN_CURSOR_THIRD_CHAR_OFFSET);
                break;
            default:
                break;
        }

        this->record_synthetic_previous(split_token(token, syntax::TokenKind::greater, TOKEN_CURSOR_FIRST_CHAR_OFFSET));
        ++this->current_;
        return true;
    }

    const syntax::Token& advance()
    {
        if (this->has_pending_tokens()) {
            this->record_synthetic_previous(this->pending_tokens_[this->pending_index_++]);
            this->clear_pending_tokens_if_consumed();
            return this->previous();
        }
        if (!this->is_eof()) {
            const syntax::Token& token = this->peek();
            ++this->current_;
            this->record_source_previous(token);
            return token;
        }
        return this->previous();
    }

private:
    inline static constexpr base::usize TOKEN_CURSOR_FIRST_CHAR_OFFSET = 0;
    inline static constexpr base::usize TOKEN_CURSOR_SECOND_CHAR_OFFSET = 1;
    inline static constexpr base::usize TOKEN_CURSOR_THIRD_CHAR_OFFSET = 2;

    inline static const std::array<syntax::Token, 1> TOKEN_CURSOR_SYNTHETIC_EOF{
        syntax::Token{syntax::TokenKind::eof, base::SourceRange{}, std::string_view{}},
    };

    [[nodiscard]] bool has_pending_tokens() const noexcept
    {
        return this->pending_index_ < this->pending_count_;
    }

    [[nodiscard]] base::usize pending_remaining() const noexcept
    {
        return this->pending_count_ - this->pending_index_;
    }

    void clear_pending_tokens_if_consumed() noexcept
    {
        if (this->has_pending_tokens()) {
            return;
        }
        this->pending_index_ = 0;
        this->pending_count_ = 0;
    }

    void record_source_previous(const syntax::Token& token) noexcept
    {
        this->previous_token_ = token;
        this->previous_token_ref_ = &token;
        this->has_previous_token_ = true;
    }

    void record_synthetic_previous(const syntax::Token& token)
    {
        this->synthetic_previous_tokens_.push_back(token);
        this->previous_token_ = this->synthetic_previous_tokens_.back();
        this->previous_token_ref_ = &this->synthetic_previous_tokens_.back();
        this->has_previous_token_ = true;
    }

    [[nodiscard]] static syntax::Token split_token(
        const syntax::Token& token, const syntax::TokenKind kind, const base::usize char_offset) noexcept
    {
        base::SourceRange range = token.range;
        range.begin += char_offset;
        range.end = range.begin + 1;
        std::string_view text = token.text();
        if (char_offset < text.size()) {
            text = text.substr(char_offset, 1);
        } else {
            text = {};
        }
        return syntax::Token{kind, range, text};
    }

    std::span<const syntax::Token> tokens_;
    std::deque<syntax::Token> synthetic_previous_tokens_;
    base::usize current_ = 0;
    base::usize pending_index_ = 0;
    base::usize pending_count_ = 0;
    std::array<syntax::Token, TOKEN_CURSOR_MAX_PENDING_TOKENS> pending_tokens_{};
    syntax::Token previous_token_{};
    const syntax::Token* previous_token_ref_ = nullptr;
    bool has_previous_token_ = false;
};

} // namespace aurex::parse
