#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/parse/parse_diagnostics.hpp>
#include <aurex/parse/token_cursor.hpp>
#include <aurex/syntax/ast.hpp>

#include <span>

namespace aurex::parse {

struct ParseSession final {
    TokenCursor cursor;
    ParseDiagnostics diagnostics;
    syntax::AstModule module;
    base::usize expression_nesting_depth = 0;

    ParseSession(
        std::span<const syntax::Token> tokens,
        base::DiagnosticSink& sink
    )
        : cursor(tokens), diagnostics(sink) {
        this->module.reserve_for_estimate(estimate_ast_reserve(tokens));
    }

    [[nodiscard]] static syntax::AstReserveEstimate estimate_ast_reserve(
        const std::span<const syntax::Token> tokens
    ) noexcept {
        syntax::AstReserveEstimate estimate;
        estimate.tokens = tokens.size();
        for (const syntax::Token& token : tokens) {
            switch (token.kind) {
            case syntax::TokenKind::identifier:
                ++estimate.identifier_tokens;
                break;
            case syntax::TokenKind::semicolon:
            case syntax::TokenKind::kw_if:
            case syntax::TokenKind::kw_for:
            case syntax::TokenKind::kw_while:
            case syntax::TokenKind::kw_defer:
                ++estimate.statements;
                break;
            case syntax::TokenKind::kw_fn:
            case syntax::TokenKind::kw_struct:
            case syntax::TokenKind::kw_opaque:
            case syntax::TokenKind::kw_enum:
            case syntax::TokenKind::kw_const:
            case syntax::TokenKind::kw_type:
            case syntax::TokenKind::kw_impl:
            case syntax::TokenKind::kw_extern:
                ++estimate.items;
                ++estimate.type_sites;
                break;
            case syntax::TokenKind::colon:
            case syntax::TokenKind::arrow:
                ++estimate.type_sites;
                break;
            case syntax::TokenKind::kw_let:
            case syntax::TokenKind::kw_var:
            case syntax::TokenKind::kw_match:
            case syntax::TokenKind::fat_arrow:
                ++estimate.pattern_sites;
                break;
            default:
                break;
            }
        }
        return estimate;
    }
};

} // namespace aurex::parse
