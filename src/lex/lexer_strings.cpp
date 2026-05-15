#include <aurex/lex/lexer.hpp>

#include <aurex/base/string_literal.hpp>
#include <aurex/base/string_literal_messages.hpp>
#include <lex/lexeme.hpp>

#include <string_view>

namespace aurex::lex {

namespace {

constexpr unsigned char LEXER_ASCII_SINGLE_BYTE_MAX = 0x7F;

[[nodiscard]] bool is_non_ascii_byte(const char c) noexcept {
    return static_cast<unsigned char>(c) > LEXER_ASCII_SINGLE_BYTE_MAX;
}

} // namespace

void Lexer::scan_string(const base::usize begin) {
    this->scan_string_body(
        begin,
        syntax::TokenKind::string_literal,
        base::StringLiteralKind::string,
        LEXEME_UNTERMINATED_STRING_MESSAGE
    );
}

void Lexer::scan_c_string(const base::usize begin) {
    this->advance_bytes(LEXEME_C_STRING_PREFIX.size());
    this->scan_string_body(
        begin,
        syntax::TokenKind::c_string_literal,
        base::StringLiteralKind::c_string,
        LEXEME_UNTERMINATED_C_STRING_MESSAGE
    );
}

void Lexer::scan_raw_string(const base::usize begin) {
    this->advance_bytes(LEXEME_RAW_STRING_PREFIX.size());
    this->scan_string_body(
        begin,
        syntax::TokenKind::raw_string_literal,
        base::StringLiteralKind::raw_string,
        LEXEME_UNTERMINATED_RAW_STRING_MESSAGE,
        true
    );
}

void Lexer::scan_byte_string(const base::usize begin) {
    this->advance_bytes(LEXEME_BYTE_STRING_PREFIX.size());
    this->scan_string_body(
        begin,
        syntax::TokenKind::byte_string_literal,
        base::StringLiteralKind::byte_string,
        LEXEME_UNTERMINATED_BYTE_STRING_MESSAGE
    );
}

void Lexer::scan_string_body(
    const base::usize begin,
    const syntax::TokenKind token_kind,
    const base::StringLiteralKind literal_kind,
    const std::string_view unterminated_message,
    const bool allow_newline
) {
    const bool is_raw_string = literal_kind == base::StringLiteralKind::raw_string;
    bool escaped = false;
    bool needs_decode_validation = false;
    while (!this->is_at_end()) {
        const char c = this->advance();
        if (escaped) {
            escaped = false;
            needs_decode_validation = true;
            continue;
        }
        if (!is_raw_string && c == LEXEME_ESCAPE) {
            escaped = true;
            needs_decode_validation = true;
            continue;
        }
        if (c == LEXEME_DOUBLE_QUOTE) {
            if (!needs_decode_validation) {
                this->add_nonempty_token(token_kind, begin, this->cursor_.offset());
                return;
            }
            const base::StringLiteralDecode decoded = base::decode_string_literal(
                this->cursor_.slice(begin, this->cursor_.offset()),
                literal_kind
            );
            for (const base::StringLiteralError& error : decoded.errors) {
                this->report(begin + error.begin, begin + error.end, error.message);
            }
            if (decoded.ok()) {
                this->add_nonempty_token(token_kind, begin, this->cursor_.offset());
            } else {
                this->finish_invalid_token(begin, this->cursor_.offset());
            }
            return;
        }
        if (is_non_ascii_byte(c) ||
            (literal_kind == base::StringLiteralKind::c_string && c == LEXEME_NUL)) {
            needs_decode_validation = true;
        }
        if (!allow_newline && c == LEXEME_LINE_FEED) {
            this->report_current(begin, unterminated_message);
            this->finish_invalid_token(begin, this->cursor_.offset());
            return;
        }
    }
    this->report_current(begin, unterminated_message);
    this->finish_invalid_token(begin, this->cursor_.offset());
}

void Lexer::scan_byte(const base::usize begin) {
    this->advance_bytes(LEXEME_BYTE_LITERAL_PREFIX.size());

    bool escaped = false;
    while (!this->is_at_end()) {
        const char c = this->advance();
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == LEXEME_ESCAPE) {
            escaped = true;
            continue;
        }
        if (c == LEXEME_SINGLE_QUOTE) {
            const base::ByteLiteralDecode decoded =
                base::decode_byte_literal(this->cursor_.slice(begin, this->cursor_.offset()));
            for (const base::StringLiteralError& error : decoded.errors) {
                if (error.message == base::STRING_LITERAL_BYTE_LITERAL_ONE_BYTE) {
                    this->report_current(begin, LEXEME_OVERSIZED_BYTE_MESSAGE);
                } else {
                    this->report(begin + error.begin, begin + error.end, error.message);
                }
            }
            if (decoded.ok()) {
                this->add_nonempty_token(syntax::TokenKind::byte_literal, begin, this->cursor_.offset());
            } else {
                this->finish_invalid_token(begin, this->cursor_.offset());
            }
            return;
        }
        if (c == LEXEME_LINE_FEED) {
            this->report(begin, this->cursor_.offset() - LEXEME_SINGLE_BYTE_WIDTH, LEXEME_UNTERMINATED_BYTE_MESSAGE);
            this->finish_invalid_token(begin, this->cursor_.offset());
            return;
        }
    }

    this->report_current(begin, LEXEME_UNTERMINATED_BYTE_MESSAGE);
    this->finish_invalid_token(begin, this->cursor_.offset());
}

void Lexer::scan_char(const base::usize begin) {
    bool escaped = false;
    while (!this->is_at_end()) {
        const char c = this->advance();
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == LEXEME_ESCAPE) {
            escaped = true;
            continue;
        }
        if (c == LEXEME_SINGLE_QUOTE) {
            const base::CharLiteralDecode decoded =
                base::decode_char_literal(this->cursor_.slice(begin, this->cursor_.offset()));
            for (const base::StringLiteralError& error : decoded.errors) {
                this->report(begin + error.begin, begin + error.end, error.message);
            }
            if (decoded.ok()) {
                this->add_nonempty_token(syntax::TokenKind::char_literal, begin, this->cursor_.offset());
            } else {
                this->finish_invalid_token(begin, this->cursor_.offset());
            }
            return;
        }
        if (c == LEXEME_LINE_FEED) {
            this->report(begin, this->cursor_.offset() - LEXEME_SINGLE_BYTE_WIDTH, LEXEME_UNTERMINATED_CHAR_MESSAGE);
            this->finish_invalid_token(begin, this->cursor_.offset());
            return;
        }
    }

    this->report_current(begin, LEXEME_UNTERMINATED_CHAR_MESSAGE);
    this->finish_invalid_token(begin, this->cursor_.offset());
}

} // namespace aurex::lex
