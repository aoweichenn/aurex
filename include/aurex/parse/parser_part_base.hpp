#pragma once

#include "aurex/parse/expr_context.hpp"
#include "aurex/parse/parse_session.hpp"
#include "aurex/parse/recovery_context.hpp"
#include "aurex/syntax/ast.hpp"

#include <string>
#include <vector>

namespace aurex::parse {

class Parser;

class ParserPartBase {
protected:
    explicit ParserPartBase(Parser& parser) noexcept;

    [[nodiscard]] bool is_eof() const noexcept;
    [[nodiscard]] const syntax::Token& peek() const noexcept;
    [[nodiscard]] const syntax::Token& previous() const noexcept;
    [[nodiscard]] bool check(syntax::TokenKind kind) const noexcept;
    [[nodiscard]] bool check_next(syntax::TokenKind kind) const noexcept;
    [[nodiscard]] bool check_type_arg_list_end() const noexcept;
    [[nodiscard]] bool next_angle_list_is_type_scope() const noexcept;
    [[nodiscard]] bool next_angle_list_is_struct_literal() const noexcept;
    bool match(syntax::TokenKind kind) noexcept;
    const syntax::Token& advance() noexcept;
    const syntax::Token& expect(syntax::TokenKind kind, std::string message);
    const syntax::Token& expect_recovered(
        syntax::TokenKind kind,
        std::string message,
        RecoveryContext context
    );
    const syntax::Token& expect_identifier_recovered(std::string message);
    const syntax::Token& expect_type_annotation_colon(std::string message);
    const syntax::Token& expect_initializer_equal(std::string message);
    const syntax::Token& expect_type_arg_list_end(std::string message);
    void synchronize(RecoveryContext context = RecoveryContext::item_or_statement);
    void report_here(std::string message);
    void report_at(const syntax::Token& token, std::string message);
    void reset_panic() noexcept;

    [[nodiscard]] syntax::TypeId parse_type();
    [[nodiscard]] std::vector<syntax::TypeId> parse_type_arg_list();
    [[nodiscard]] syntax::StmtId parse_block();
    [[nodiscard]] syntax::ExprId parse_block_expr(ExprContext context = ExprContext::normal);
    [[nodiscard]] syntax::StmtId parse_stmt();
    [[nodiscard]] syntax::ExprId parse_expr(ExprContext context = ExprContext::normal);
    [[nodiscard]] syntax::PatternId parse_pattern();

    [[nodiscard]] base::SourceRange merge(base::SourceRange begin, base::SourceRange end) const noexcept;
    [[nodiscard]] base::SourceRange expr_range_or(syntax::ExprId id, base::SourceRange fallback) const noexcept;
    [[nodiscard]] base::SourceRange stmt_range_or(syntax::StmtId id, base::SourceRange fallback) const noexcept;
    [[nodiscard]] base::SourceRange type_range_or(syntax::TypeId id, base::SourceRange fallback) const noexcept;
    [[nodiscard]] base::SourceRange pattern_range_or(syntax::PatternId id, base::SourceRange fallback) const noexcept;

    Parser& parser_;
    ParseSession& session_;
};

} // namespace aurex::parse
