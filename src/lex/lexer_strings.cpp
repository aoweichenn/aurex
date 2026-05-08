#include "aurex/lex/lexer.hpp"

#include "aurex/base/string_literal.hpp"
#include "lexeme.hpp"

namespace aurex::lex {

void Lexer::scan_string(const base::usize begin) {
    this->scan_string_body(
        begin,
        syntax::TokenKind::string_literal,
        base::StringLiteralKind::string,
        unterminated_string_message
    );
}

void Lexer::scan_c_string(const base::usize begin) {
    this->advance_bytes(c_string_prefix.size());
    this->scan_string_body(
        begin,
        syntax::TokenKind::c_string_literal,
        base::StringLiteralKind::c_string,
        unterminated_c_string_message
    );
}

void Lexer::scan_string_body(
    const base::usize begin,
    const syntax::TokenKind token_kind,
    const base::StringLiteralKind literal_kind,
    const std::string_view unterminated_message
) {
    bool escaped = false;
    while (!this->is_at_end()) {
        const char c = this->advance();
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == lexeme_escape) {
            escaped = true;
            continue;
        }
        if (c == lexeme_double_quote) {
            const base::StringLiteralDecode decoded = base::decode_string_literal(
                this->cursor_.slice(begin, this->cursor_.offset()),
                literal_kind
            );
            for (const base::StringLiteralError& error : decoded.errors) {
                this->report(begin + error.begin, begin + error.end, error.message);
            }
            if (decoded.ok()) {
                this->finish_token(token_kind, begin);
            } else {
                this->finish_invalid_token(begin);
            }
            return;
        }
        if (c == lexeme_line_feed) {
            this->report_current(begin, unterminated_message);
            this->finish_invalid_token(begin);
            return;
        }
    }
    this->report_current(begin, unterminated_message);
    this->finish_invalid_token(begin);
}

void Lexer::scan_byte(const base::usize begin) {
    this->advance_bytes(byte_literal_prefix.size());

    if (this->is_at_end() || this->peek() == lexeme_line_feed) {
        this->report_current(begin, unterminated_byte_message);
        this->finish_invalid_token(begin);
        return;
    }

    if (this->peek() == lexeme_escape) {
        this->advance();
        if (!this->is_at_end()) {
            this->advance();
        }
    } else {
        this->advance();
    }

    if (!this->match(lexeme_single_quote)) {
        while (!this->is_at_end() && this->peek() != lexeme_single_quote && this->peek() != lexeme_line_feed) {
            this->advance();
        }
        if (this->match(lexeme_single_quote)) {
            this->report_current(begin, oversized_byte_message);
        } else {
            this->report_current(begin, unterminated_byte_message);
        }
        this->finish_invalid_token(begin);
        return;
    }

    this->finish_token(syntax::TokenKind::byte_literal, begin);
}

} // namespace aurex::lex
