#include "aurex/parse/parser_parts.hpp"

#include "aurex/parse/parser.hpp"

#include <utility>

namespace aurex::parse {

ParserPartBase::ParserPartBase(Parser& parser) noexcept
    : parser_(parser), session_(parser.session_) {}

bool ParserPartBase::is_eof() const noexcept {
    return parser_.is_eof();
}

const syntax::Token& ParserPartBase::peek() const noexcept {
    return parser_.peek();
}

const syntax::Token& ParserPartBase::previous() const noexcept {
    return parser_.previous();
}

bool ParserPartBase::check(const syntax::TokenKind kind) const noexcept {
    return parser_.check(kind);
}

bool ParserPartBase::check_next(const syntax::TokenKind kind) const noexcept {
    return parser_.check_next(kind);
}

bool ParserPartBase::check_type_arg_list_end() const noexcept {
    return parser_.check_type_arg_list_end();
}

bool ParserPartBase::next_angle_list_is_type_scope() const noexcept {
    return parser_.next_angle_list_is_type_scope();
}

bool ParserPartBase::next_angle_list_is_struct_literal() const noexcept {
    return parser_.next_angle_list_is_struct_literal();
}

bool ParserPartBase::match(const syntax::TokenKind kind) noexcept {
    return parser_.match(kind);
}

const syntax::Token& ParserPartBase::advance() noexcept {
    return parser_.advance();
}

const syntax::Token& ParserPartBase::expect(const syntax::TokenKind kind, std::string message) {
    return parser_.expect(kind, std::move(message));
}

const syntax::Token& ParserPartBase::expect_type_arg_list_end(std::string message) {
    return parser_.expect_type_arg_list_end(std::move(message));
}

void ParserPartBase::synchronize() {
    parser_.synchronize();
}

void ParserPartBase::report_here(std::string message) {
    parser_.report_here(std::move(message));
}

void ParserPartBase::report_at(const syntax::Token& token, std::string message) {
    parser_.report_at(token, std::move(message));
}

void ParserPartBase::reset_panic() noexcept {
    parser_.reset_panic();
}

syntax::TypeId ParserPartBase::parse_type() {
    return TypeParser(parser_).parse_type();
}

std::vector<syntax::TypeId> ParserPartBase::parse_type_arg_list() {
    return TypeParser(parser_).parse_type_arg_list();
}

syntax::StmtId ParserPartBase::parse_block() {
    return BlockParser(parser_).parse_block();
}

syntax::ExprId ParserPartBase::parse_block_expr(const ExprContext context) {
    return BlockParser(parser_).parse_block_expr(context);
}

syntax::StmtId ParserPartBase::parse_stmt() {
    return StmtParser(parser_).parse_stmt();
}

syntax::ExprId ParserPartBase::parse_expr(const ExprContext context) {
    return ExprParser(parser_).parse_expr(context);
}

syntax::PatternId ParserPartBase::parse_pattern() {
    return PatternParser(parser_).parse_pattern();
}

base::SourceRange ParserPartBase::merge(
    const base::SourceRange begin,
    const base::SourceRange end
) const noexcept {
    return parser_.merge(begin, end);
}

} // namespace aurex::parse
