#include "aurex/parse/parser.hpp"

#include "aurex/parse/parser_item_part.hpp"
#include "aurex/parse/recovery.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

enum class AngleListFollower {
    type_scope,
    struct_literal,
};

constexpr int kGreaterGreaterTokenArity = 2;
constexpr int kSingleGreaterTokenArity = 1;
constexpr int kInitialAngleListDepth = 1;
constexpr int kClosedAngleListDepth = 0;
constexpr base::usize kNextTokenOffset = 1;

[[nodiscard]] bool angle_list_follower_matches(
    const TokenKind kind,
    const AngleListFollower follower
) noexcept {
    switch (follower) {
    case AngleListFollower::type_scope:
        return kind == TokenKind::dot || kind == TokenKind::l_paren;
    case AngleListFollower::struct_literal:
        return kind == TokenKind::l_brace;
    }
    return false;
}

[[nodiscard]] bool angle_list_scan_should_stop(
    const TokenKind kind,
    const AngleListFollower follower
) noexcept {
    switch (kind) {
    case TokenKind::semicolon:
    case TokenKind::r_brace:
    case TokenKind::fat_arrow:
        return true;
    case TokenKind::l_brace:
        return follower == AngleListFollower::type_scope;
    default:
        return false;
    }
}

[[nodiscard]] bool next_angle_list_has_follower(
    const std::span<const syntax::Token> tokens,
    const base::usize less_position,
    const AngleListFollower follower
) noexcept {
    if (less_position >= tokens.size() || tokens[less_position].kind != TokenKind::less) {
        return false;
    }

    base::usize index = less_position + kNextTokenOffset;
    int depth = kInitialAngleListDepth;
    while (index < tokens.size()) {
        const TokenKind kind = tokens[index].kind;
        if (kind == TokenKind::less) {
            ++depth;
        } else if (kind == TokenKind::greater || kind == TokenKind::greater_greater) {
            const int close_count = kind == TokenKind::greater_greater
                ? kGreaterGreaterTokenArity
                : kSingleGreaterTokenArity;
            for (int i = 0; i < close_count; ++i) {
                --depth;
                if (depth == kClosedAngleListDepth) {
                    const base::usize after = index + kNextTokenOffset;
                    return after < tokens.size() && angle_list_follower_matches(tokens[after].kind, follower);
                }
            }
        } else if (angle_list_scan_should_stop(kind, follower)) {
            return false;
        }
        ++index;
    }
    return false;
}

} // namespace

Parser::Parser(
    const std::span<const syntax::Token> tokens,
    base::DiagnosticSink& diagnostics
) noexcept
    : session_(tokens, diagnostics) {}

base::Result<syntax::AstModule> Parser::parse_module() {
    ItemParser items(*this);

    if (this->match(TokenKind::kw_module)) {
        this->session_.module.module_path = items.parse_path();
        this->expect_recovered(
            TokenKind::semicolon,
            "expected ';' after module declaration",
            RecoveryContext::module_terminator
        );
    }

    while (this->check(TokenKind::kw_import) ||
           ((this->check(TokenKind::kw_pub) || this->check(TokenKind::kw_priv)) && this->check_next(TokenKind::kw_import))) {
        this->session_.module.imports.push_back(items.parse_import_decl());
    }

    while (!this->is_eof()) {
        const syntax::ItemId item = items.parse_item();
        if (!syntax::is_valid(item)) {
            this->synchronize(RecoveryContext::item);
        }
    }

    if (this->session_.diagnostics.has_error()) {
        return base::Result<syntax::AstModule>::fail(
            {base::ErrorCode::parse_error, "parsing failed"}
        );
    }
    return base::Result<syntax::AstModule>::ok(std::move(this->session_.module));
}

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

const syntax::Token& Parser::expect_type_arg_list_end(std::string message) {
    if (this->check_type_arg_list_end()) {
        return this->session_.cursor.consume_type_arg_list_end();
    }
    this->report_here(std::move(message));
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

base::SourceRange Parser::merge(base::SourceRange begin, base::SourceRange end) const noexcept {
    return base::SourceRange {begin.source, begin.begin, end.end};
}

} // namespace aurex::parse
