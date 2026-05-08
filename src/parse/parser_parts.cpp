#include "aurex/parse/parser_parts.hpp"

#include "aurex/parse/parser.hpp"
#include "aurex/parse/recovery.hpp"

#include <utility>

namespace aurex::parse {

ParserPartBase::ParserPartBase(Parser& parser) noexcept
    : parser_(parser), session_(parser.session_) {}

bool ParserPartBase::is_eof() const noexcept {
    return this->parser_.is_eof();
}

const syntax::Token& ParserPartBase::peek() const noexcept {
    return this->parser_.peek();
}

const syntax::Token& ParserPartBase::previous() const noexcept {
    return this->parser_.previous();
}

bool ParserPartBase::check(const syntax::TokenKind kind) const noexcept {
    return this->parser_.check(kind);
}

bool ParserPartBase::check_next(const syntax::TokenKind kind) const noexcept {
    return this->parser_.check_next(kind);
}

bool ParserPartBase::check_type_arg_list_end() const noexcept {
    return this->parser_.check_type_arg_list_end();
}

bool ParserPartBase::next_angle_list_is_type_scope() const noexcept {
    return this->parser_.next_angle_list_is_type_scope();
}

bool ParserPartBase::next_angle_list_is_struct_literal() const noexcept {
    return this->parser_.next_angle_list_is_struct_literal();
}

bool ParserPartBase::match(const syntax::TokenKind kind) noexcept {
    return this->parser_.match(kind);
}

const syntax::Token& ParserPartBase::advance() noexcept {
    return this->parser_.advance();
}

const syntax::Token& ParserPartBase::expect(const syntax::TokenKind kind, std::string message) {
    return this->parser_.expect(kind, std::move(message));
}

const syntax::Token& ParserPartBase::expect_recovered(
    const syntax::TokenKind kind,
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

const syntax::Token& ParserPartBase::expect_type_arg_list_end(std::string message) {
    return this->parser_.expect_type_arg_list_end(std::move(message));
}

void ParserPartBase::synchronize(const RecoveryContext context) {
    this->parser_.synchronize(context);
}

void ParserPartBase::report_here(std::string message) {
    this->parser_.report_here(std::move(message));
}

void ParserPartBase::report_at(const syntax::Token& token, std::string message) {
    this->parser_.report_at(token, std::move(message));
}

void ParserPartBase::reset_panic() noexcept {
    this->parser_.reset_panic();
}

syntax::TypeId ParserPartBase::parse_type() {
    return TypeParser(this->parser_).parse_type();
}

std::vector<syntax::TypeId> ParserPartBase::parse_type_arg_list() {
    return TypeParser(this->parser_).parse_type_arg_list();
}

syntax::StmtId ParserPartBase::parse_block() {
    return BlockParser(this->parser_).parse_block();
}

syntax::ExprId ParserPartBase::parse_block_expr(const ExprContext context) {
    return BlockParser(this->parser_).parse_block_expr(context);
}

syntax::StmtId ParserPartBase::parse_stmt() {
    return StmtParser(this->parser_).parse_stmt();
}

syntax::ExprId ParserPartBase::parse_expr(const ExprContext context) {
    return ExprParser(this->parser_).parse_expr(context);
}

syntax::PatternId ParserPartBase::parse_pattern() {
    return PatternParser(this->parser_).parse_pattern();
}

base::SourceRange ParserPartBase::merge(
    const base::SourceRange begin,
    const base::SourceRange end
) const noexcept {
    return this->parser_.merge(begin, end);
}

base::SourceRange ParserPartBase::expr_range_or(
    const syntax::ExprId id,
    const base::SourceRange fallback
) const noexcept {
    if (!syntax::is_valid(id) || id.value >= this->session_.module.exprs.size()) {
        return fallback;
    }
    return this->session_.module.exprs[id.value].range;
}

base::SourceRange ParserPartBase::stmt_range_or(
    const syntax::StmtId id,
    const base::SourceRange fallback
) const noexcept {
    if (!syntax::is_valid(id) || id.value >= this->session_.module.stmts.size()) {
        return fallback;
    }
    return this->session_.module.stmts[id.value].range;
}

base::SourceRange ParserPartBase::type_range_or(
    const syntax::TypeId id,
    const base::SourceRange fallback
) const noexcept {
    if (!syntax::is_valid(id) || id.value >= this->session_.module.types.size()) {
        return fallback;
    }
    return this->session_.module.types[id.value].range;
}

base::SourceRange ParserPartBase::pattern_range_or(
    const syntax::PatternId id,
    const base::SourceRange fallback
) const noexcept {
    if (!syntax::is_valid(id) || id.value >= this->session_.module.patterns.size()) {
        return fallback;
    }
    return this->session_.module.patterns[id.value].range;
}

} // namespace aurex::parse
