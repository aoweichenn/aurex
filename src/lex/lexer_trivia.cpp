#include "aurex/lex/lexer.hpp"

#include "char_class.hpp"
#include "lexeme.hpp"

namespace aurex::lex {

void Lexer::skip_trivia() {
    while (!is_at_end()) {
        while (!is_at_end() && is_trivia_space(peek())) {
            advance();
        }
        if (starts_with(line_comment_prefix)) {
            scan_line_comment();
            continue;
        }
        if (starts_with(block_comment_prefix)) {
            scan_block_comment();
            continue;
        }
        break;
    }
}

void Lexer::scan_line_comment() {
    while (!is_at_end() && peek() != lexeme_line_feed) {
        advance();
    }
}

void Lexer::scan_block_comment() {
    const base::usize begin = cursor_.offset();
    advance_bytes(block_comment_prefix.size());
    while (!is_at_end()) {
        if (starts_with(block_comment_suffix)) {
            advance_bytes(block_comment_suffix.size());
            return;
        }
        advance();
    }
    report(begin, cursor_.offset(), unterminated_block_comment_message);
}

} // namespace aurex::lex
