#include "aurex/parse/parser.hpp"

#include "parser_angle_lookahead.hpp"

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

bool Parser::check_type_arg_list_end() const noexcept {
    return this->session_.cursor.check_type_arg_list_end();
}

bool Parser::next_angle_list_is_type_scope() const noexcept {
    return next_angle_list_has_follower(
        this->session_.cursor.tokens(),
        this->session_.cursor.position(),
        AngleListFollower::type_scope
    );
}

bool Parser::next_angle_list_is_struct_literal() const noexcept {
    return next_angle_list_has_follower(
        this->session_.cursor.tokens(),
        this->session_.cursor.position(),
        AngleListFollower::struct_literal
    );
}

bool Parser::match(const TokenKind kind) noexcept {
    return this->session_.cursor.match(kind);
}

const syntax::Token& Parser::advance() noexcept {
    return this->session_.cursor.advance();
}

} // namespace aurex::parse
