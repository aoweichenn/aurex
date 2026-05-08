#pragma once

#include "aurex/base/integer.hpp"
#include "aurex/syntax/token.hpp"

#include <span>

namespace aurex::parse {

class TokenCursor final {
public:
    explicit TokenCursor(std::span<const syntax::Token> tokens) noexcept
        : tokens_(tokens) {}

    [[nodiscard]] bool is_eof() const noexcept {
        return peek().kind == syntax::TokenKind::eof;
    }

    [[nodiscard]] const syntax::Token& peek() const noexcept {
        if (pending_split_greater_tail_) {
            return split_greater_tail_;
        }
        if (current_ >= tokens_.size()) {
            return tokens_.back();
        }
        return tokens_[current_];
    }

    [[nodiscard]] const syntax::Token& previous() const noexcept {
        if (previous_was_split_greater_) {
            return last_split_greater_;
        }
        if (current_ == 0) {
            return tokens_.front();
        }
        return tokens_[current_ - 1];
    }

    [[nodiscard]] bool check(const syntax::TokenKind kind) const noexcept {
        return peek().kind == kind;
    }

    [[nodiscard]] bool check_next(const syntax::TokenKind kind) const noexcept {
        const base::usize next = current_ + (pending_split_greater_tail_ ? 0 : 1);
        if (next >= tokens_.size()) {
            return false;
        }
        return tokens_[next].kind == kind;
    }

    [[nodiscard]] bool check_type_arg_list_end() const noexcept {
        return check(syntax::TokenKind::greater) ||
               can_split_greater_greater();
    }

    [[nodiscard]] bool can_split_greater_greater() const noexcept {
        return !pending_split_greater_tail_ &&
               current_ < tokens_.size() &&
               tokens_[current_].kind == syntax::TokenKind::greater_greater;
    }

    [[nodiscard]] std::span<const syntax::Token> tokens() const noexcept {
        return tokens_;
    }

    [[nodiscard]] base::usize position() const noexcept {
        return current_;
    }

    bool match(const syntax::TokenKind kind) noexcept {
        if (!check(kind)) {
            return false;
        }
        advance();
        return true;
    }

    const syntax::Token& advance() noexcept {
        if (pending_split_greater_tail_) {
            pending_split_greater_tail_ = false;
            previous_was_split_greater_ = true;
            last_split_greater_ = split_greater_tail_;
            return last_split_greater_;
        }
        previous_was_split_greater_ = false;
        if (!is_eof()) {
            ++current_;
        }
        return previous();
    }

    const syntax::Token& consume_type_arg_list_end() noexcept {
        if (check(syntax::TokenKind::greater)) {
            return advance();
        }
        if (can_split_greater_greater()) {
            const syntax::Token token = tokens_[current_];
            last_split_greater_ = token;
            last_split_greater_.kind = syntax::TokenKind::greater;
            last_split_greater_.range.end = last_split_greater_.range.begin + 1;
            split_greater_tail_ = token;
            split_greater_tail_.kind = syntax::TokenKind::greater;
            split_greater_tail_.range.begin = split_greater_tail_.range.end > split_greater_tail_.range.begin
                ? split_greater_tail_.range.end - 1
                : split_greater_tail_.range.begin;
            ++current_;
            pending_split_greater_tail_ = true;
            previous_was_split_greater_ = true;
            return last_split_greater_;
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
