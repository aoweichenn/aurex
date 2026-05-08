#pragma once

#include "aurex/base/diagnostic.hpp"
#include "aurex/base/result.hpp"
#include "aurex/base/source.hpp"
#include "aurex/lex/lexer_cursor.hpp"
#include "aurex/syntax/token.hpp"

#include <string_view>
#include <vector>

namespace aurex::base {
enum class StringLiteralKind;
}

namespace aurex::lex {

struct LexerOptions {
    bool emit_invalid_tokens = true;
};

class Lexer final {
public:
    // The lexer is a small state machine over bytes. It never owns the source
    // text; every token text is a string_view into SourceManager storage.
    Lexer(
        base::SourceId source_id,
        std::string_view source_text,
        base::DiagnosticSink& diagnostics,
        LexerOptions options = {}
    ) noexcept;

    [[nodiscard]] base::Result<std::vector<syntax::Token>> tokenize();

private:
    enum class DigitSet {
        decimal,
        hexadecimal,
        binary,
    };

    struct DigitScanResult {
        bool saw_digit = false;
        bool had_error = false;
    };

    [[nodiscard]] bool is_at_end() const noexcept;
    [[nodiscard]] bool starts_with(std::string_view text) const noexcept;
    [[nodiscard]] char peek() const noexcept;
    [[nodiscard]] char peek_next() const noexcept;
    char advance() noexcept;
    void advance_bytes(base::usize byte_count) noexcept;
    [[nodiscard]] bool match(char expected) noexcept;

    void skip_trivia();
    void scan_token();
    [[nodiscard]] bool scan_punctuator(base::usize begin);
    void scan_identifier();
    void scan_number();
    [[nodiscard]] DigitScanResult scan_digits(DigitSet digit_set, std::string_view literal_kind);
    [[nodiscard]] bool scan_invalid_radix_tail(DigitSet digit_set, std::string_view message);
    [[nodiscard]] bool scan_fraction_part(bool& had_error);
    [[nodiscard]] bool scan_exponent_part(bool& had_error);
    void scan_string_body(
        base::usize begin,
        syntax::TokenKind token_kind,
        base::StringLiteralKind literal_kind,
        std::string_view unterminated_message
    );
    void scan_string(base::usize begin);
    void scan_c_string(base::usize begin);
    void scan_byte(base::usize begin);
    void scan_line_comment();
    void scan_block_comment();
    [[nodiscard]] base::SourceRange range(base::usize begin, base::usize end) const noexcept;
    [[nodiscard]] base::SourceRange current_range(base::usize begin) const noexcept;
    void finish_token(syntax::TokenKind kind, base::usize begin);
    void finish_invalid_token(base::usize begin);
    void add_token(syntax::TokenKind kind, base::usize begin, base::usize end);
    void report_current(base::usize begin, std::string_view message) const;
    void report(base::usize begin, base::usize end, std::string_view message) const;

    base::SourceId source_id_;
    detail::LexerCursor cursor_;
    base::DiagnosticSink& diagnostics_;
    LexerOptions options_;
    std::vector<syntax::Token> tokens_;
};

} // namespace aurex::lex
