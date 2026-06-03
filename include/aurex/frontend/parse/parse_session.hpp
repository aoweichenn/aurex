#pragma once

#include <aurex/frontend/parse/parse_diagnostics.hpp>
#include <aurex/frontend/parse/token_cursor.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>

#include <span>

namespace aurex::parse {

namespace detail {

inline constexpr base::usize PARSER_EXPR_RESERVE_SLACK_TOKEN_DIVISOR = 64;

[[nodiscard]] constexpr base::usize parser_expr_extra_capacity_for_tokens(const base::usize token_count) noexcept
{
    return syntax::ast_reserve_fraction(token_count, PARSER_EXPR_RESERVE_SLACK_TOKEN_DIVISOR);
}

[[nodiscard]] constexpr base::usize parser_expr_reserved_node_count(
    const syntax::AstReserveEstimate::Exprs& exprs) noexcept
{
    return exprs.literals + exprs.names + exprs.generic_applies + exprs.unaries + exprs.binaries + exprs.calls
        + exprs.ifs + exprs.blocks + exprs.matches + exprs.arrays + exprs.tuples + exprs.fields + exprs.indexes
        + exprs.slices + exprs.struct_literals + exprs.casts;
}

inline void count_parser_primary_expr_token(
    syntax::AstReserveEstimate::Exprs& exprs, const syntax::TokenKind kind) noexcept
{
    switch (kind) {
        case syntax::TokenKind::identifier:
            ++exprs.names;
            break;
        case syntax::TokenKind::integer_literal:
        case syntax::TokenKind::float_literal:
        case syntax::TokenKind::string_literal:
        case syntax::TokenKind::c_string_literal:
        case syntax::TokenKind::raw_string_literal:
        case syntax::TokenKind::byte_string_literal:
        case syntax::TokenKind::byte_literal:
        case syntax::TokenKind::char_literal:
        case syntax::TokenKind::kw_true:
        case syntax::TokenKind::kw_false:
        case syntax::TokenKind::kw_null:
            ++exprs.literals;
            break;
        case syntax::TokenKind::kw_if:
            ++exprs.ifs;
            break;
        case syntax::TokenKind::kw_match:
            ++exprs.matches;
            break;
        case syntax::TokenKind::l_brace:
        case syntax::TokenKind::kw_unsafe:
            ++exprs.blocks;
            break;
        case syntax::TokenKind::l_bracket:
            ++exprs.arrays;
            ++exprs.slices;
            ++exprs.indexes;
            ++exprs.generic_applies;
            break;
        case syntax::TokenKind::l_paren:
            ++exprs.tuples;
            break;
        case syntax::TokenKind::kw_cast:
        case syntax::TokenKind::kw_ptrcast:
        case syntax::TokenKind::kw_bitcast:
        case syntax::TokenKind::kw_sizeof:
        case syntax::TokenKind::kw_alignof:
        case syntax::TokenKind::kw_ptraddr:
        case syntax::TokenKind::kw_ptrat:
        case syntax::TokenKind::kw_sliceptr:
        case syntax::TokenKind::kw_slicelen:
        case syntax::TokenKind::kw_strptr:
        case syntax::TokenKind::kw_strblen:
        case syntax::TokenKind::kw_strvalid:
        case syntax::TokenKind::kw_strfromutf8:
            ++exprs.casts;
            break;
        case syntax::TokenKind::kw_strraw:
            ++exprs.calls;
            break;
        default:
            break;
    }
}

inline void count_parser_expr_operator_token(
    syntax::AstReserveEstimate::Exprs& exprs, const syntax::TokenKind kind) noexcept
{
    switch (kind) {
        case syntax::TokenKind::plus:
        case syntax::TokenKind::slash:
        case syntax::TokenKind::percent:
        case syntax::TokenKind::pipe:
        case syntax::TokenKind::caret:
        case syntax::TokenKind::less:
        case syntax::TokenKind::greater:
            ++exprs.binaries;
            break;
        case syntax::TokenKind::minus:
        case syntax::TokenKind::star:
        case syntax::TokenKind::amp:
            ++exprs.binaries;
            ++exprs.unaries;
            break;
        case syntax::TokenKind::bang:
        case syntax::TokenKind::tilde:
        case syntax::TokenKind::plus_plus:
        case syntax::TokenKind::minus_minus:
            ++exprs.unaries;
            break;
        case syntax::TokenKind::equal_equal:
        case syntax::TokenKind::bang_equal:
        case syntax::TokenKind::less_equal:
        case syntax::TokenKind::greater_equal:
        case syntax::TokenKind::less_less:
        case syntax::TokenKind::greater_greater:
        case syntax::TokenKind::amp_amp:
        case syntax::TokenKind::pipe_pipe:
            ++exprs.binaries;
            break;
        case syntax::TokenKind::dot:
            ++exprs.fields;
            break;
        case syntax::TokenKind::l_paren:
            ++exprs.calls;
            break;
        case syntax::TokenKind::l_brace:
            ++exprs.struct_literals;
            break;
        case syntax::TokenKind::question:
            ++exprs.unaries;
            break;
        default:
            break;
    }
}

inline void finalize_parser_expr_reserve(
    syntax::AstReserveEstimate::Exprs& exprs, const base::usize token_count) noexcept
{
    const base::usize extra = parser_expr_extra_capacity_for_tokens(token_count);
    exprs.headers = parser_expr_reserved_node_count(exprs) + extra;
}

} // namespace detail

struct ParseSession final {
    TokenCursor cursor;
    ParseDiagnostics diagnostics;
    syntax::AstModule module;
    base::usize expression_nesting_depth = 0;
    base::usize type_nesting_depth = 0;
    base::usize pattern_nesting_depth = 0;

    ParseSession(std::span<const syntax::Token> tokens, base::DiagnosticSink& sink) : cursor(tokens), diagnostics(sink)
    {
        this->module.reserve_for_estimate(estimate_ast_reserve(tokens));
    }

    [[nodiscard]] static syntax::AstReserveEstimate estimate_ast_reserve(
        const std::span<const syntax::Token> tokens) noexcept
    {
        syntax::AstReserveEstimate estimate;
        estimate.tokens = tokens.size();
        for (const syntax::Token& token : tokens) {
            detail::count_parser_primary_expr_token(estimate.exprs, token.kind);
            detail::count_parser_expr_operator_token(estimate.exprs, token.kind);
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
        detail::finalize_parser_expr_reserve(estimate.exprs, tokens.size());
        return estimate;
    }
};

} // namespace aurex::parse
