#include <aurex/parse/parser.hpp>

namespace aurex::parse {

using syntax::TokenKind;

bool Parser::is_eof() const noexcept {
    return this->session_.cursor.is_eof();
}

const syntax::Token& Parser::peek() const noexcept {
    return this->session_.cursor.peek();
}

const syntax::Token& Parser::previous() const noexcept {
    return this->session_.cursor.previous();
}

bool Parser::check(const TokenKind kind) const noexcept {
    return this->session_.cursor.check(kind);
}

bool Parser::check_next(const TokenKind kind) const noexcept {
    return this->session_.cursor.check_next(kind);
}

const syntax::Token& Parser::peek_at(const base::usize offset) const noexcept {
    return this->session_.cursor.peek_at(offset);
}

base::usize Parser::mark() const noexcept {
    return this->session_.cursor.mark();
}

void Parser::rewind(const base::usize position) noexcept {
    this->session_.cursor.rewind(position);
}

bool Parser::match(const TokenKind kind) noexcept {
    return this->session_.cursor.match(kind);
}

const syntax::Token& Parser::advance() noexcept {
    return this->session_.cursor.advance();
}

} // namespace aurex::parse
