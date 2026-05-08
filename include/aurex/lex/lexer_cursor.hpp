#pragma once

#include "aurex/base/integer.hpp"

#include <string_view>

namespace aurex::lex::detail {

inline constexpr char cursor_eof_sentinel = '\0';
inline constexpr base::usize cursor_current_lookahead = 0;
inline constexpr base::usize cursor_next_lookahead = 1;

class LexerCursor final {
public:
    explicit LexerCursor(std::string_view source_text) noexcept
        : source_text_(source_text) {}

    [[nodiscard]] base::usize source_size() const noexcept {
        return source_text_.size();
    }

    [[nodiscard]] std::string_view remaining_text() const noexcept {
        return source_text_.substr(offset_);
    }

    [[nodiscard]] base::usize offset() const noexcept {
        return offset_;
    }

    [[nodiscard]] bool is_at_end() const noexcept {
        return offset_ >= source_text_.size();
    }

    [[nodiscard]] bool starts_with(const std::string_view text) const noexcept {
        return source_text_.size() - offset_ >= text.size() &&
               source_text_.substr(offset_, text.size()) == text;
    }

    [[nodiscard]] char peek_at(const base::usize lookahead) const noexcept {
        const base::usize target = offset_ + lookahead;
        if (target >= source_text_.size()) {
            return cursor_eof_sentinel;
        }
        return source_text_[target];
    }

    [[nodiscard]] char peek() const noexcept {
        return peek_at(cursor_current_lookahead);
    }

    [[nodiscard]] char peek_next() const noexcept {
        return peek_at(cursor_next_lookahead);
    }

    char advance() noexcept {
        if (is_at_end()) {
            return cursor_eof_sentinel;
        }
        const char c = source_text_[offset_];
        ++offset_;
        return c;
    }

    void advance_bytes(const base::usize byte_count) noexcept {
        const base::usize remaining = source_text_.size() - offset_;
        offset_ += byte_count < remaining ? byte_count : remaining;
    }

    [[nodiscard]] bool match(const char expected) noexcept {
        if (peek() != expected) {
            return false;
        }
        ++offset_;
        return true;
    }

    [[nodiscard]] std::string_view slice(const base::usize begin, const base::usize end) const noexcept {
        return source_text_.substr(begin, end - begin);
    }

private:
    std::string_view source_text_;
    base::usize offset_ {};
};

} // namespace aurex::lex::detail
