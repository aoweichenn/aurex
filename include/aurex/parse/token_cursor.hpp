#pragma once

#include <aurex/base/integer.hpp>
#include <aurex/syntax/token.hpp>

#include <span>

namespace aurex::parse {

class TokenCursor final {
public:
    explicit TokenCursor(std::span<const syntax::Token> tokens) noexcept
        : tokens_(tokens) {}

    [[nodiscard]] bool is_eof() const noexcept {
        return this->peek().kind == syntax::TokenKind::eof;
    }

    [[nodiscard]] const syntax::Token& peek() const noexcept {
        if (this->pending_split_greater_tail_) {
            return this->split_greater_tail_;
        }
        if (this->current_ >= this->tokens_.size()) {
            return this->tokens_.back();
        }
        return this->tokens_[this->current_];
    }

    [[nodiscard]] const syntax::Token& previous() const noexcept {
        if (this->previous_was_split_greater_) {
            return this->last_split_greater_;
        }
        if (this->current_ == 0) {
            return this->tokens_.front();
        }
        return this->tokens_[this->current_ - 1];
    }

    [[nodiscard]] bool check(const syntax::TokenKind kind) const noexcept {
        return this->peek().kind == kind;
    }

    [[nodiscard]] bool check_next(const syntax::TokenKind kind) const noexcept {
        const base::usize next = this->current_ + (this->pending_split_greater_tail_ ? 0 : 1);
        if (next >= this->tokens_.size()) {
            return false;
        }
        return this->tokens_[next].kind == kind;
    }

    [[nodiscard]] bool check_type_arg_list_end() const noexcept {
        return this->check(syntax::TokenKind::greater) ||
               this->can_split_greater_greater();
    }

    [[nodiscard]] bool can_split_greater_greater() const noexcept {
        return !this->pending_split_greater_tail_ &&
               this->current_ < this->tokens_.size() &&
               this->tokens_[this->current_].kind == syntax::TokenKind::greater_greater;
    }

    [[nodiscard]] std::span<const syntax::Token> tokens() const noexcept {
        return this->tokens_;
    }

    [[nodiscard]] base::usize position() const noexcept {
        return this->current_;
    }

    bool match(const syntax::TokenKind kind) noexcept {
        if (!this->check(kind)) {
            return false;
        }
        this->advance();
        return true;
    }

    const syntax::Token& advance() noexcept {
        if (this->pending_split_greater_tail_) {
            this->pending_split_greater_tail_ = false;
            this->previous_was_split_greater_ = true;
            this->last_split_greater_ = this->split_greater_tail_;
            return this->last_split_greater_;
        }
        this->previous_was_split_greater_ = false;
        if (!this->is_eof()) {
            ++this->current_;
        }
        return this->previous();
    }

    const syntax::Token& consume_type_arg_list_end() noexcept {
        if (this->check(syntax::TokenKind::greater)) {
            return this->advance();
        }
        if (this->can_split_greater_greater()) {
            const syntax::Token token = this->tokens_[this->current_];
            this->last_split_greater_ = token;
            this->last_split_greater_.kind = syntax::TokenKind::greater;
            this->last_split_greater_.range.end = this->last_split_greater_.range.begin + 1;
            this->split_greater_tail_ = token;
            this->split_greater_tail_.kind = syntax::TokenKind::greater;
            this->split_greater_tail_.range.begin = this->split_greater_tail_.range.end > this->split_greater_tail_.range.begin
                ? this->split_greater_tail_.range.end - 1
                : this->split_greater_tail_.range.begin;
            ++this->current_;
            this->pending_split_greater_tail_ = true;
            this->previous_was_split_greater_ = true;
            return this->last_split_greater_;
        }
        static const syntax::Token fallback {};
        return fallback;
    }

private:
    std::span<const syntax::Token> tokens_;
    base::usize current_ = 0;
    syntax::Token split_greater_tail_ {};
    syntax::Token last_split_greater_ {};
    bool pending_split_greater_tail_ = false;
    bool previous_was_split_greater_ = false;
};

} // namespace aurex::parse
