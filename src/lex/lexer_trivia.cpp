#include "aurex/lex/lexer.hpp"

#include "char_class.hpp"
#include "lexeme.hpp"

namespace aurex::lex {

void Lexer::skip_trivia() {
    while (!this->is_at_end()) {
        while (!this->is_at_end() && is_trivia_space(this->peek())) {
            this->advance();
        }
        if (this->starts_with(line_comment_prefix)) {
            this->scan_line_comment();
            continue;
        }
        if (this->starts_with(block_comment_prefix)) {
            this->scan_block_comment();
            continue;
        }
        break;
    }
}

void Lexer::scan_line_comment() {
    while (!this->is_at_end() && this->peek() != lexeme_line_feed) {
        this->advance();
    }
}

void Lexer::scan_block_comment() {
    const base::usize begin = this->cursor_.offset();
    this->advance_bytes(block_comment_prefix.size());
    while (!this->is_at_end()) {
        if (this->starts_with(block_comment_suffix)) {
            this->advance_bytes(block_comment_suffix.size());
            return;
        }
        this->advance();
    }
    this->report_current(begin, unterminated_block_comment_message);
}

} // namespace aurex::lex
