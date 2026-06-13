#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/parse/parser_part_core.hpp>

#include <utility>

namespace aurex::parse {

ParserPartCore::ParserPartCore(Parser& parser) noexcept : parser_(parser), session_(parser.session_)
{
}

bool ParserPartCore::is_eof() const noexcept
{
    return this->parser_.is_eof();
}

const syntax::Token& ParserPartCore::peek() const noexcept
{
    return this->parser_.peek();
}

const syntax::Token& ParserPartCore::previous() const noexcept
{
    return this->parser_.previous();
}

bool ParserPartCore::check(const syntax::TokenKind kind) const noexcept
{
    return this->parser_.check(kind);
}

bool ParserPartCore::check_next(const syntax::TokenKind kind) const noexcept
{
    return this->parser_.check_next(kind);
}

const syntax::Token& ParserPartCore::peek_at(const base::usize offset) const noexcept
{
    return this->parser_.peek_at(offset);
}

TokenCursorMark ParserPartCore::mark() const noexcept
{
    return this->parser_.mark();
}

void ParserPartCore::rewind(const TokenCursorMark mark) const noexcept
{
    this->parser_.rewind(mark);
}

bool ParserPartCore::match(const syntax::TokenKind kind) const noexcept
{
    return this->parser_.match(kind);
}

bool ParserPartCore::check_generic_left_angle() const noexcept
{
    return this->parser_.check_generic_left_angle();
}

bool ParserPartCore::match_generic_left_angle() const noexcept
{
    return this->parser_.match_generic_left_angle();
}

bool ParserPartCore::check_generic_right_angle() const noexcept
{
    return this->parser_.check_generic_right_angle();
}

bool ParserPartCore::match_generic_right_angle() const noexcept
{
    return this->parser_.match_generic_right_angle();
}

const syntax::Token& ParserPartCore::advance() const noexcept
{
    return this->parser_.advance();
}

const syntax::Token& ParserPartCore::expect(const syntax::TokenKind kind, std::string message) const
{
    return this->parser_.expect(kind, std::move(message));
}

const syntax::Token& ParserPartCore::expect_generic_left_angle_recovered(
    std::string message, const RecoveryContext context) const
{
    return this->parser_.expect_generic_left_angle_recovered(std::move(message), context);
}

const syntax::Token& ParserPartCore::expect_generic_right_angle_recovered_after(
    std::string message, const RecoveryContext context, const syntax::Token& opening) const
{
    return this->parser_.expect_generic_right_angle_recovered_after(std::move(message), context, opening);
}

const syntax::Token& ParserPartCore::expect_contextual_c_keyword(std::string message) const
{
    return this->parser_.expect_contextual_c_keyword(std::move(message));
}

const syntax::Token& ParserPartCore::expect_contextual_c_keyword_recovered(
    std::string message, const RecoveryContext context) const
{
    return this->parser_.expect_contextual_c_keyword_recovered(std::move(message), context);
}

const syntax::Token& ParserPartCore::expect_recovered(
    const syntax::TokenKind kind, std::string message, const RecoveryContext context) const
{
    return this->parser_.expect_recovered(kind, std::move(message), context);
}

const syntax::Token& ParserPartCore::expect_recovered_after(const syntax::TokenKind kind, std::string message,
    const RecoveryContext context, const syntax::Token& opening) const
{
    return this->parser_.expect_recovered_after(kind, std::move(message), context, opening);
}

const syntax::Token& ParserPartCore::expect_identifier_recovered(std::string message)
{
    return this->expect_recovered(syntax::TokenKind::identifier, std::move(message), RecoveryContext::identifier);
}

const syntax::Token& ParserPartCore::expect_type_annotation_colon(std::string message)
{
    return this->expect_recovered(syntax::TokenKind::colon, std::move(message), RecoveryContext::type_annotation);
}

const syntax::Token& ParserPartCore::expect_initializer_equal(std::string message)
{
    return this->expect_recovered(syntax::TokenKind::equal, std::move(message), RecoveryContext::initializer);
}

void ParserPartCore::synchronize(const RecoveryContext context) const
{
    this->parser_.synchronize(context);
}

void ParserPartCore::report_here(std::string message) const
{
    this->parser_.report_here(std::move(message));
}

void ParserPartCore::report_at(const syntax::Token& token, std::string message) const
{
    this->parser_.report_at(token, std::move(message));
}

void ParserPartCore::report_note_at(const syntax::Token& token, std::string message) const
{
    this->parser_.report_note_at(token, std::move(message));
}

void ParserPartCore::reset_panic() const noexcept
{
    this->parser_.reset_panic();
}

} // namespace aurex::parse
