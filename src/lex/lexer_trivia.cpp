#include "aurex/lex/lexer.hpp"

#include "char_class.hpp"
#include "lexeme.hpp"

#include <string_view>

namespace aurex::lex {

void Lexer::skip_trivia() {
    while (!this->is_at_end()) {
        const std::string_view remaining = this->cursor_.remaining_text();
        base::usize trivia_width = 0;
        while (trivia_width < remaining.size() && is_trivia_space(remaining[trivia_width])) {
            ++trivia_width;
        }
        this->advance_bytes(trivia_width);
        if (this->peek() == lexeme_slash) {
            const char next = this->peek_next();
            if (next == lexeme_slash) {
                this->scan_line_comment();
                continue;
            }
            if (next == lexeme_star) {
                this->scan_block_comment();
                continue;
            }
        }
        break;
    }
}

void Lexer::scan_line_comment() {
    const std::string_view remaining = this->cursor_.remaining_text();
    const base::usize line_end = remaining.find(lexeme_line_feed);
    if (line_end == std::string_view::npos) {
        this->advance_bytes(remaining.size());
        return;
    }
    this->advance_bytes(line_end + single_byte_lexeme_width);
}

void Lexer::scan_block_comment() {
    const base::usize begin = this->cursor_.offset();
    const std::string_view remaining = this->cursor_.remaining_text();
    const base::usize suffix_offset = remaining.find(block_comment_suffix, block_comment_prefix.size());
    if (suffix_offset != std::string_view::npos) {
        this->advance_bytes(suffix_offset + block_comment_suffix.size());
        return;
    }
    this->advance_bytes(remaining.size());
    this->report_current(begin, unterminated_block_comment_message);
}

} // namespace aurex::lex
