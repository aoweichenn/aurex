#include <aurex/parse/parser.hpp>

#include <aurex/parse/recovery.hpp>

#include <string_view>
#include <utility>

namespace aurex::parse {

using syntax::TokenKind;

namespace {

constexpr std::string_view PARSER_CONTEXTUAL_C_KEYWORD_TEXT = "c";
constexpr std::string_view PARSER_OPENING_DELIMITER_NOTE =
    "opening delimiter is here";

} // namespace

bool Parser::check_contextual_c_keyword() const noexcept {
    const syntax::Token& token = this->peek();
    return token.kind == TokenKind::identifier &&
           token.text == PARSER_CONTEXTUAL_C_KEYWORD_TEXT;
}

const syntax::Token& Parser::expect(const TokenKind kind, std::string message) {
    if (this->check(kind)) {
        return this->advance();
    }
    this->report_here(std::move(message));
    static const syntax::Token fallback {};
    return fallback;
}

const syntax::Token& Parser::expect_contextual_c_keyword(std::string message) {
    if (this->check_contextual_c_keyword()) {
        return this->advance();
    }
    this->report_here(std::move(message));
    static const syntax::Token fallback {};
    return fallback;
}

const syntax::Token& Parser::expect_contextual_c_keyword_recovered(
    std::string message,
    const RecoveryContext context
) {
    if (this->check_contextual_c_keyword()) {
        return this->advance();
    }

    this->report_here(std::move(message));
    if (!token_matches_recovery_context(this->peek().kind, context)) {
        this->synchronize(context);
    }
    if (this->check_contextual_c_keyword()) {
        const syntax::Token& token = this->advance();
        this->reset_panic();
        return token;
    }
    this->reset_panic();
    static const syntax::Token fallback {};
    return fallback;
}

const syntax::Token& Parser::expect_recovered(
    const TokenKind kind,
    std::string message,
    const RecoveryContext context
) {
    if (this->check(kind)) {
        return this->advance();
    }

    this->report_here(std::move(message));
    if (!token_matches_recovery_context(this->peek().kind, context)) {
        this->synchronize(context);
    }
    if (this->check(kind)) {
        const syntax::Token& token = this->advance();
        this->reset_panic();
        return token;
    }
    this->reset_panic();
    static const syntax::Token fallback {};
    return fallback;
}

const syntax::Token& Parser::expect_recovered_after(
    const TokenKind kind,
    std::string message,
    const RecoveryContext context,
    const syntax::Token& opening
) {
    if (this->check(kind)) {
        return this->advance();
    }

    this->report_here(std::move(message));
    this->report_note_at(opening, std::string(PARSER_OPENING_DELIMITER_NOTE));
    if (!token_matches_recovery_context(this->peek().kind, context)) {
        this->synchronize(context);
    }
    if (this->check(kind)) {
        const syntax::Token& token = this->advance();
        this->reset_panic();
        return token;
    }
    this->reset_panic();
    static const syntax::Token fallback {};
    return fallback;
}

void Parser::synchronize(const RecoveryContext context) {
    this->reset_panic();
    if (this->is_eof()) {
        return;
    }
    this->advance();
    while (!this->is_eof()) {
        if (this->previous().kind == TokenKind::semicolon) {
            return;
        }
        if (token_matches_recovery_context(this->peek().kind, context)) {
            return;
        }
        this->advance();
    }
}

void Parser::report_here(std::string message) {
    this->report_at(this->peek(), std::move(message));
}

void Parser::report_at(const syntax::Token& token, std::string message) {
    this->session_.diagnostics.report_at(token, std::move(message));
}

void Parser::report_note_at(const syntax::Token& token, std::string message) {
    this->session_.diagnostics.report_note_at(token, std::move(message));
}

void Parser::reset_panic() noexcept {
    this->session_.diagnostics.reset_panic();
}

} // namespace aurex::parse
