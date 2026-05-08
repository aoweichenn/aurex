#include "aurex/parse/parser.hpp"

#include "aurex/parse/parser_parts.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

Parser::Parser(
    const std::span<const syntax::Token> tokens,
    base::DiagnosticSink& diagnostics
) noexcept
    : session_(tokens, diagnostics) {}

base::Result<syntax::AstModule> Parser::parse_module() {
    ItemParser items(*this);

    if (match(TokenKind::kw_module)) {
        session_.module.module_path = items.parse_path();
        expect(TokenKind::semicolon, "expected ';' after module declaration");
    }

    while (check(TokenKind::kw_import) ||
           ((check(TokenKind::kw_pub) || check(TokenKind::kw_priv)) && check_next(TokenKind::kw_import))) {
        session_.module.imports.push_back(items.parse_import_decl());
    }

    while (!is_eof()) {
        const syntax::ItemId item = items.parse_item();
        if (!syntax::is_valid(item)) {
            synchronize();
        }
    }

    if (session_.diagnostics.has_error()) {
        return base::Result<syntax::AstModule>::fail(
            {base::ErrorCode::parse_error, "parsing failed"}
        );
    }
    return base::Result<syntax::AstModule>::ok(std::move(session_.module));
}

bool Parser::is_eof() const noexcept {
    return session_.cursor.is_eof();
}

const syntax::Token& Parser::peek() const noexcept {
    return session_.cursor.peek();
}

const syntax::Token& Parser::previous() const noexcept {
    return session_.cursor.previous();
}

bool Parser::check(const TokenKind kind) const noexcept {
    return session_.cursor.check(kind);
}

bool Parser::check_next(const TokenKind kind) const noexcept {
    return session_.cursor.check_next(kind);
}

bool Parser::check_type_arg_list_end() const noexcept {
    return session_.cursor.check_type_arg_list_end();
}

bool Parser::next_angle_list_is_type_scope() const noexcept {
    if (!check(TokenKind::less)) {
        return false;
    }
    const std::span<const syntax::Token> tokens = session_.cursor.tokens();
    base::usize index = session_.cursor.position() + 1;
    int depth = 1;
    while (index < tokens.size()) {
        const TokenKind kind = tokens[index].kind;
        if (kind == TokenKind::less) {
            ++depth;
        } else if (kind == TokenKind::greater) {
            --depth;
            if (depth == 0) {
                const base::usize after = index + 1;
                return after < tokens.size() &&
                       (tokens[after].kind == TokenKind::dot || tokens[after].kind == TokenKind::l_paren);
            }
        } else if (kind == TokenKind::greater_greater) {
            for (int i = 0; i < 2; ++i) {
                --depth;
                if (depth == 0) {
                    const base::usize after = index + 1;
                    return after < tokens.size() &&
                           (tokens[after].kind == TokenKind::dot || tokens[after].kind == TokenKind::l_paren);
                }
            }
        } else if (kind == TokenKind::semicolon ||
                   kind == TokenKind::l_brace ||
                   kind == TokenKind::r_brace ||
                   kind == TokenKind::fat_arrow) {
            return false;
        }
        ++index;
    }
    return false;
}

bool Parser::next_angle_list_is_struct_literal() const noexcept {
    if (!check(TokenKind::less)) {
        return false;
    }
    const std::span<const syntax::Token> tokens = session_.cursor.tokens();
    base::usize index = session_.cursor.position() + 1;
    int depth = 1;
    while (index < tokens.size()) {
        const TokenKind kind = tokens[index].kind;
        if (kind == TokenKind::less) {
            ++depth;
        } else if (kind == TokenKind::greater) {
            --depth;
            if (depth == 0) {
                const base::usize after = index + 1;
                return after < tokens.size() && tokens[after].kind == TokenKind::l_brace;
            }
        } else if (kind == TokenKind::greater_greater) {
            for (int i = 0; i < 2; ++i) {
                --depth;
                if (depth == 0) {
                    const base::usize after = index + 1;
                    return after < tokens.size() && tokens[after].kind == TokenKind::l_brace;
                }
            }
        } else if (kind == TokenKind::semicolon ||
                   kind == TokenKind::r_brace ||
                   kind == TokenKind::fat_arrow) {
            return false;
        }
        ++index;
    }
    return false;
}

bool Parser::match(const TokenKind kind) noexcept {
    return session_.cursor.match(kind);
}

const syntax::Token& Parser::advance() noexcept {
    return session_.cursor.advance();
}

const syntax::Token& Parser::expect(const TokenKind kind, std::string message) {
    if (check(kind)) {
        return advance();
    }
    report_here(std::move(message));
    static const syntax::Token fallback {};
    return fallback;
}

const syntax::Token& Parser::expect_type_arg_list_end(std::string message) {
    if (check_type_arg_list_end()) {
        return session_.cursor.consume_type_arg_list_end();
    }
    report_here(std::move(message));
    static const syntax::Token fallback {};
    return fallback;
}

void Parser::synchronize() {
    reset_panic();
    if (is_eof()) {
        return;
    }
    advance();
    while (!is_eof()) {
        if (previous().kind == TokenKind::semicolon) {
            return;
        }
        switch (peek().kind) {
        case TokenKind::r_brace:
        case TokenKind::kw_fn:
        case TokenKind::kw_struct:
        case TokenKind::kw_enum:
        case TokenKind::kw_impl:
        case TokenKind::kw_opaque:
        case TokenKind::kw_const:
        case TokenKind::kw_type:
        case TokenKind::kw_pub:
        case TokenKind::kw_priv:
        case TokenKind::kw_extern:
        case TokenKind::kw_export:
        case TokenKind::kw_let:
        case TokenKind::kw_var:
        case TokenKind::kw_if:
        case TokenKind::kw_for:
        case TokenKind::kw_while:
        case TokenKind::kw_defer:
        case TokenKind::kw_return:
        case TokenKind::kw_noncopy:
        case TokenKind::kw_import:
            return;
        default:
            advance();
            break;
        }
    }
}

void Parser::report_here(std::string message) {
    report_at(peek(), std::move(message));
}

void Parser::report_at(const syntax::Token& token, std::string message) {
    session_.diagnostics.report_at(token, std::move(message));
}

void Parser::reset_panic() noexcept {
    session_.diagnostics.reset_panic();
}

base::SourceRange Parser::merge(base::SourceRange begin, base::SourceRange end) const noexcept {
    return base::SourceRange {begin.source, begin.begin, end.end};
}

} // namespace aurex::parse
