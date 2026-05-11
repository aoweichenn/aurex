#include <aurex/parse/parser.hpp>

#include <aurex/parse/recovery.hpp>

#include <utility>

namespace aurex::parse {

using syntax::TokenKind;

const syntax::Token& Parser::expect(const TokenKind kind, std::string message) {
    if (this->check(kind)) {
        return this->advance();
    }
    this->report_here(std::move(message));
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

void Parser::reset_panic() noexcept {
    this->session_.diagnostics.reset_panic();
}

} // namespace aurex::parse
