#pragma once

#include <aurex/parse/parser_part_base.hpp>

#include <optional>
#include <vector>

namespace aurex::parse {

class PostfixExprParser final : private ParserPartBase {
public:
    explicit PostfixExprParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ExprId parse_postfix(ExprContext context);

private:
    [[nodiscard]] std::optional<syntax::PostfixOp> parse_next_suffix(ExprContext context);
    [[nodiscard]] syntax::PostfixOp parse_bracket_suffix(ExprContext context);
    [[nodiscard]] syntax::PostfixBracketArg parse_bracket_arg(ExprContext context);
    [[nodiscard]] bool bracket_arg_starts_type_only() const noexcept;
    [[nodiscard]] bool bracket_parenthesized_arg_is_type() const noexcept;
    [[nodiscard]] bool recover_bracket_arg_separator();
    [[nodiscard]] syntax::PostfixOp parse_field_suffix();
    [[nodiscard]] syntax::PostfixOp parse_rejected_legacy_scope_suffix(const base::SourceRange& fallback_range);
    [[nodiscard]] syntax::PostfixOp parse_struct_literal_suffix(ExprContext context);
    void parse_struct_fields(syntax::AstArenaVector<syntax::FieldInit>& fields, ExprContext context);
    [[nodiscard]] syntax::FieldInit parse_struct_field(ExprContext context);
    [[nodiscard]] bool recover_struct_field_separator();
    [[nodiscard]] syntax::PostfixOp parse_rejected_numeric_tuple_field_suffix(const base::SourceRange& fallback_range);
    [[nodiscard]] const syntax::Token& expect_index_suffix_end();
    [[nodiscard]] const syntax::Token& expect_slice_suffix_end();
    [[nodiscard]] syntax::PostfixOp parse_call_suffix(ExprContext context);
    void parse_call_args(syntax::AstArenaVector<syntax::ExprId>& args, ExprContext context);
    [[nodiscard]] bool recover_call_arg_separator();
    [[nodiscard]] syntax::PostfixOp parse_try_suffix() const;
    [[nodiscard]] syntax::PostfixOp parse_rejected_update_suffix(
        const base::SourceRange& fallback_range,
        syntax::TokenKind kind,
        std::string message
    );
};

} // namespace aurex::parse
