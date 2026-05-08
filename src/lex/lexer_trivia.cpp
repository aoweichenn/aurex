#include "aurex/lex/lexer.hpp"

#include "char_class.hpp"

namespace aurex::lex {

void Lexer::skip_trivia() {
    while (!is_at_end()) {
        while (!is_at_end() && is_trivia_space(peek())) {
            advance();
        }
        if (peek() == '/' && peek_next() == '/') {
            scan_line_comment();
            continue;
        }
        if (peek() == '/' && peek_next() == '*') {
            scan_block_comment();
            continue;
        }
        break;
    }
}

void Lexer::scan_line_comment() {
    while (!is_at_end() && peek() != '\n') {
        advance();
    }
}

void Lexer::scan_block_comment() {
    const base::usize begin = offset_;
    advance();
    advance();
    while (!is_at_end()) {
        if (peek() == '*' && peek_next() == '/') {
            advance();
            advance();
            return;
        }
        advance();
    }
    report(begin, offset_, "unterminated block comment");
}

} // namespace aurex::lex
