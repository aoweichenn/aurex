#include <aurex/parse/parser_block_part.hpp>

#include <aurex/parse/recovery.hpp>
#include <aurex/parse/parser_stmt_part.hpp>

#include <span>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

constexpr int PARSER_BLOCK_NO_DELIMITER_DEPTH = 0;

[[nodiscard]] bool stmt_is_kind(
    const syntax::AstModule& module,
    const syntax::StmtId stmt,
    const syntax::StmtKind kind
) noexcept {
    return syntax::is_valid(stmt) &&
           stmt.value < module.stmts.size() &&
           module.stmts[stmt.value].kind == kind;
}

[[nodiscard]] bool token_opens_delimiter(const TokenKind kind) noexcept {
    return kind == TokenKind::l_paren ||
           kind == TokenKind::l_brace ||
           kind == TokenKind::l_bracket;
}

[[nodiscard]] bool token_closes_delimiter(const TokenKind kind) noexcept {
    return kind == TokenKind::r_paren ||
           kind == TokenKind::r_brace ||
           kind == TokenKind::r_bracket;
}

[[nodiscard]] bool scan_balanced_delimited(
    const std::span<const syntax::Token> tokens,
    base::usize& index
) noexcept {
    if (index >= tokens.size() || !token_opens_delimiter(tokens[index].kind)) {
        return false;
    }

    int depth = PARSER_BLOCK_NO_DELIMITER_DEPTH;
    while (index < tokens.size()) {
        const TokenKind kind = tokens[index].kind;
        if (token_opens_delimiter(kind)) {
            ++depth;
        } else if (token_closes_delimiter(kind)) {
            --depth;
            if (depth == PARSER_BLOCK_NO_DELIMITER_DEPTH) {
                ++index;
                return true;
            }
        }
        ++index;
    }
    return false;
}

[[nodiscard]] bool scan_if_expression_tail(
    const std::span<const syntax::Token> tokens,
    base::usize& index
) noexcept {
    while (index < tokens.size() && tokens[index].kind == TokenKind::kw_if) {
        ++index;

        while (index < tokens.size() && tokens[index].kind != TokenKind::l_brace) {
            if (tokens[index].kind == TokenKind::semicolon ||
                tokens[index].kind == TokenKind::r_brace ||
                tokens[index].kind == TokenKind::eof) {
                return false;
            }
            ++index;
        }
        if (!scan_balanced_delimited(tokens, index)) {
            return false;
        }
        if (index >= tokens.size() || tokens[index].kind != TokenKind::kw_else) {
            return false;
        }
        ++index;
        if (index < tokens.size() && tokens[index].kind == TokenKind::kw_if) {
            continue;
        }
        return scan_balanced_delimited(tokens, index);
    }
    return false;
}

} // namespace

syntax::StmtId BlockParser::parse_block() {
    return this->parse_block_body(
        BlockBodyMode::statement,
        ExprContext::normal,
        "expected block",
        "expected '}' after block"
    ).block;
}

syntax::ExprId BlockParser::parse_block_expr(const ExprContext context) {
    const BlockBody body = this->parse_block_body(
        BlockBodyMode::expression,
        context,
        "expected block expression",
        "expected '}' after block expression"
    );

    const base::SourceRange block_range = this->stmt_range_or(body.block, this->previous().range);
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::block_expr;
    expr.range = block_range;
    expr.block = body.block;
    expr.block_result = body.result;
    this->reset_panic();
    return this->session_.module.push_expr(std::move(expr));
}

BlockParser::BlockBody BlockParser::parse_block_body(
    const BlockBodyMode mode,
    const ExprContext context,
    const std::string_view start_message,
    const std::string_view end_message
) {
    const syntax::Token& begin = this->expect_block_start(std::string(start_message));
    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    syntax::ExprId result = syntax::INVALID_EXPR_ID;

    while (!this->is_eof() && !this->at_block_recovery_boundary()) {
        if (mode == BlockBodyMode::statement || this->token_starts_required_statement()) {
            const syntax::StmtId stmt = this->parse_stmt();
            if (syntax::is_valid(stmt)) {
                block.statements.push_back(stmt);
            } else {
                this->synchronize(RecoveryContext::statement);
            }
            this->reset_panic();
            continue;
        }

        if (!this->token_starts_tail_expression()) {
            const syntax::StmtId stmt = this->parse_stmt();
            if (syntax::is_valid(stmt)) {
                block.statements.push_back(stmt);
            } else {
                this->synchronize(RecoveryContext::statement);
            }
            this->reset_panic();
            continue;
        }

        const syntax::StmtId stmt = StmtParser(this->parser_).parse_expr_or_assign_stmt(context, false);
        if (stmt_is_kind(this->session_.module, stmt, syntax::StmtKind::assign)) {
            const syntax::Token& end = this->expect_recovered(
                TokenKind::semicolon,
                "expected ';' after assignment",
                RecoveryContext::statement_terminator
            );
            this->session_.module.stmts[stmt.value].range =
                this->merge(this->stmt_range_or(stmt, end.range), end.range);
            block.statements.push_back(stmt);
            this->reset_panic();
            continue;
        }

        if (this->match(TokenKind::semicolon)) {
            if (syntax::is_valid(stmt) && stmt.value < this->session_.module.stmts.size()) {
                this->session_.module.stmts[stmt.value].range =
                    this->merge(this->stmt_range_or(stmt, this->previous().range), this->previous().range);
                block.statements.push_back(stmt);
            }
            this->reset_panic();
            continue;
        }

        if (stmt_is_kind(this->session_.module, stmt, syntax::StmtKind::expr)) {
            result = this->session_.module.stmts[stmt.value].init;
        }
        break;
    }

    const syntax::Token& end = this->expect_block_end(std::string(end_message));
    block.range = this->merge(begin.range, end.range);
    this->reset_panic();
    return BlockBody {
        this->session_.module.push_stmt(std::move(block)),
        result,
    };
}

bool BlockParser::token_starts_tail_expression() const noexcept {
    if (this->check(TokenKind::kw_if)) {
        return this->next_if_is_tail_expression();
    }
    if (this->check(TokenKind::l_brace)) {
        return this->next_block_is_tail_expression();
    }

    switch (this->peek().kind) {
    case TokenKind::identifier:
    case TokenKind::integer_literal:
    case TokenKind::float_literal:
    case TokenKind::string_literal:
    case TokenKind::c_string_literal:
    case TokenKind::byte_literal:
    case TokenKind::kw_if:
    case TokenKind::kw_match:
    case TokenKind::kw_true:
    case TokenKind::kw_false:
    case TokenKind::kw_null:
    case TokenKind::kw_cast:
    case TokenKind::kw_pcast:
    case TokenKind::kw_bit_cast:
    case TokenKind::kw_size_of:
    case TokenKind::kw_align_of:
    case TokenKind::kw_ptr_addr:
    case TokenKind::kw_ptr_from_addr:
    case TokenKind::kw_str_data:
    case TokenKind::kw_str_byte_len:
    case TokenKind::kw_str_from_bytes_unchecked:
    case TokenKind::l_paren:
    case TokenKind::minus:
    case TokenKind::star:
    case TokenKind::amp:
    case TokenKind::tilde:
    case TokenKind::bang:
        return true;
    default:
        return false;
    }
}

bool BlockParser::token_starts_required_statement() const noexcept {
    switch (this->peek().kind) {
    case TokenKind::kw_let:
    case TokenKind::kw_var:
    case TokenKind::kw_for:
    case TokenKind::kw_while:
    case TokenKind::kw_break:
    case TokenKind::kw_continue:
    case TokenKind::kw_defer:
    case TokenKind::kw_return:
        return true;
    default:
        return false;
    }
}

bool BlockParser::next_if_is_tail_expression() const noexcept {
    base::usize index = this->session_.cursor.position();
    if (!scan_if_expression_tail(this->session_.cursor.tokens(), index)) {
        return false;
    }
    return index < this->session_.cursor.tokens().size() &&
           this->session_.cursor.tokens()[index].kind == TokenKind::r_brace;
}

bool BlockParser::next_block_is_tail_expression() const noexcept {
    base::usize index = this->session_.cursor.position();
    if (!scan_balanced_delimited(this->session_.cursor.tokens(), index)) {
        return false;
    }
    return index < this->session_.cursor.tokens().size() &&
           this->session_.cursor.tokens()[index].kind == TokenKind::r_brace;
}

const syntax::Token& BlockParser::expect_block_start(std::string message) {
    return this->expect_recovered(
        TokenKind::l_brace,
        std::move(message),
        RecoveryContext::block_start
    );
}

const syntax::Token& BlockParser::expect_block_end(std::string message) {
    return this->expect_recovered(
        TokenKind::r_brace,
        std::move(message),
        RecoveryContext::block_end
    );
}

bool BlockParser::at_block_recovery_boundary() const noexcept {
    return token_matches_recovery_context(this->peek().kind, RecoveryContext::item);
}

} // namespace aurex::parse
