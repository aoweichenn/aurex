#include <aurex/lex/lexer.hpp>

#include <lex/char_class.hpp>
#include <lex/lexeme.hpp>

#include <string_view>

namespace aurex::lex {

void Lexer::skip_trivia()
{
    while (!this->is_at_end()) {
        const std::string_view remaining = this->cursor_.remaining_text();
        base::usize trivia_width = 0;
        while (trivia_width < remaining.size() && is_trivia_space(remaining[trivia_width])) {
            ++trivia_width;
        }
        this->advance_bytes(trivia_width);
        if (this->peek() == LEXEME_SLASH) {
            const char next = this->peek_next();
            if (next == LEXEME_SLASH) {
                this->scan_line_comment();
                continue;
            }
            if (next == LEXEME_STAR) {
                this->scan_block_comment();
                continue;
            }
        }
        break;
    }
}

void Lexer::scan_line_comment()
{
    const std::string_view remaining = this->cursor_.remaining_text();
    const base::usize line_end = remaining.find(LEXEME_LINE_FEED);
    if (line_end == std::string_view::npos) {
        this->advance_bytes(remaining.size());
        return;
    }
    this->advance_bytes(line_end + LEXEME_SINGLE_BYTE_WIDTH);
}

void Lexer::scan_block_comment()
{
    const base::usize begin = this->cursor_.offset();
    base::usize depth = 0;
    while (!this->is_at_end()) {
        if (this->peek() == LEXEME_SLASH && this->peek_next() == LEXEME_STAR) {
            this->advance_bytes(LEXEME_BLOCK_COMMENT_PREFIX.size());
            ++depth;
            continue;
        }
        if (this->peek() == LEXEME_STAR && this->peek_next() == LEXEME_SLASH) {
            this->advance_bytes(LEXEME_BLOCK_COMMENT_SUFFIX.size());
            --depth;
            if (depth == 0) {
                return;
            }
            continue;
        }
        this->advance_bytes(LEXEME_SINGLE_BYTE_WIDTH);
    }
    this->report_current(begin, LEXEME_UNTERMINATED_BLOCK_COMMENT_MESSAGE);
}

} // namespace aurex::lex
