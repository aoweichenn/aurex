#include "aurex/lex/lexer.hpp"

#include "aurex/base/string_literal.hpp"
#include "lexeme.hpp"

#include <string_view>

namespace aurex::lex {

namespace {

constexpr unsigned char ascii_single_byte_max = 0x7F;

[[nodiscard]] bool is_non_ascii_byte(const char c) noexcept {
    return static_cast<unsigned char>(c) > ascii_single_byte_max;
}

} // namespace

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
    bool needs_decode_validation = false;
    while (!this->is_at_end()) {
        const char c = this->advance();
        if (escaped) {
            escaped = false;
            needs_decode_validation = true;
            continue;
        }
        if (c == lexeme_escape) {
            escaped = true;
            needs_decode_validation = true;
            continue;
        }
        if (c == lexeme_double_quote) {
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
                this->finish_invalid_token(begin);
            }
            return;
        }
        if (is_non_ascii_byte(c) ||
            (literal_kind == base::StringLiteralKind::c_string && c == lexeme_nul)) {
            needs_decode_validation = true;
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
        const std::string_view remaining = this->cursor_.remaining_text();
        const base::usize recovery_offset = remaining.find_first_of(byte_literal_recovery_chars);
        if (recovery_offset == std::string_view::npos) {
            this->advance_bytes(remaining.size());
            this->report_current(begin, unterminated_byte_message);
        } else if (remaining[recovery_offset] == lexeme_single_quote) {
            this->advance_bytes(recovery_offset + single_byte_lexeme_width);
            this->report_current(begin, oversized_byte_message);
        } else {
            this->advance_bytes(recovery_offset);
            this->report_current(begin, unterminated_byte_message);
        }
        this->finish_invalid_token(begin);
        return;
    }

    this->add_nonempty_token(syntax::TokenKind::byte_literal, begin, this->cursor_.offset());
}

} // namespace aurex::lex
