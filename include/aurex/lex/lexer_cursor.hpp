#pragma once

#include <aurex/base/integer.hpp>

#include <cassert>
#include <string_view>

namespace aurex::lex::detail {

inline constexpr char LEXER_CURSOR_EOF_SENTINEL = '\0';
inline constexpr base::usize LEXER_CURSOR_CURRENT_LOOKAHEAD = 0;
inline constexpr base::usize LEXER_CURSOR_NEXT_LOOKAHEAD = 1;

class LexerCursor final {
public:
    explicit LexerCursor(std::string_view source_text) noexcept : source_text_(source_text)
    {
    }

    [[nodiscard]] base::usize source_size() const noexcept
    {
        return source_text_.size();
    }

    [[nodiscard]] base::usize remaining_size() const noexcept
    {
        return offset_ < source_text_.size() ? source_text_.size() - offset_ : 0;
    }

    [[nodiscard]] std::string_view remaining_text() const noexcept
    {
        const base::usize remaining = remaining_size();
        if (remaining == 0) {
            return {};
        }
        return std::string_view{source_text_.data() + offset_, remaining};
    }

    [[nodiscard]] base::usize offset() const noexcept
    {
        return offset_;
    }

    [[nodiscard]] bool is_at_end() const noexcept
    {
        return offset_ >= source_text_.size();
    }

    [[nodiscard]] bool starts_with(const std::string_view text) const noexcept
    {
        if (remaining_size() < text.size()) {
            return false;
        }
        if (text.empty()) {
            return true;
        }
        return std::string_view{source_text_.data() + offset_, text.size()} == text;
    }

    [[nodiscard]] char peek_at(const base::usize lookahead) const noexcept
    {
        const base::usize target = offset_ + lookahead;
        if (target >= source_text_.size()) {
            return LEXER_CURSOR_EOF_SENTINEL;
        }
        return source_text_[target];
    }

    [[nodiscard]] char peek() const noexcept
    {
        return peek_at(LEXER_CURSOR_CURRENT_LOOKAHEAD);
    }

    [[nodiscard]] char peek_next() const noexcept
    {
        const base::usize target = offset_ + LEXER_CURSOR_NEXT_LOOKAHEAD;
        if (target >= source_text_.size()) {
            return LEXER_CURSOR_EOF_SENTINEL;
        }
        return source_text_[target];
    }

    char advance() noexcept
    {
        if (is_at_end()) {
            return LEXER_CURSOR_EOF_SENTINEL;
        }
        const char c = source_text_[offset_];
        ++offset_;
        return c;
    }

    void advance_bytes(const base::usize byte_count) noexcept
    {
        const base::usize remaining = source_text_.size() - offset_;
        offset_ += byte_count < remaining ? byte_count : remaining;
    }

    [[nodiscard]] bool match(const char expected) noexcept
    {
        if (offset_ >= source_text_.size() || source_text_[offset_] != expected) {
            return false;
        }
        ++offset_;
        return true;
    }

    [[nodiscard]] std::string_view slice(const base::usize begin, const base::usize end) const noexcept
    {
        assert(begin <= end);
        assert(end <= source_text_.size());
        if (begin == end) {
            return {};
        }
        return std::string_view{source_text_.data() + begin, end - begin};
    }

    [[nodiscard]] std::string_view nonempty_slice(const base::usize begin, const base::usize end) const noexcept
    {
        assert(begin < end);
        assert(end <= source_text_.size());
        return std::string_view{source_text_.data() + begin, end - begin};
    }

    [[nodiscard]] std::string_view current_slice(const base::usize begin) const noexcept
    {
        return slice(begin, offset_);
    }

private:
    std::string_view source_text_;
    base::usize offset_{};
};

} // namespace aurex::lex::detail
