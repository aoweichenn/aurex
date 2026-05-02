#pragma once

#include "aurex/base/diagnostic.hpp"
#include "aurex/base/result.hpp"
#include "aurex/base/source.hpp"
#include "aurex/syntax/token.hpp"

#include <string_view>
#include <vector>

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
    [[nodiscard]] bool is_at_end() const noexcept;
    [[nodiscard]] char peek() const noexcept;
    [[nodiscard]] char peek_next() const noexcept;
    char advance() noexcept;
    [[nodiscard]] bool match(char expected) noexcept;

    void skip_trivia();
    void scan_token();
    void scan_identifier();
    void scan_integer();
    void scan_string();
    void scan_c_string();
    void scan_byte();
    void scan_line_comment();
    void scan_block_comment();
    void add_token(syntax::TokenKind kind, base::usize begin, base::usize end);
    void report(base::usize begin, base::usize end, std::string message) const;

    base::SourceId source_id_;
    std::string_view source_text_;
    base::DiagnosticSink& diagnostics_;
    LexerOptions options_;
    base::usize offset_ = 0;
    std::vector<syntax::Token> tokens_;
};

} // namespace aurex::lex
