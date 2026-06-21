#pragma once

#include <aurex/frontend/parse/parser_part_base.hpp>

#include <optional>
#include <vector>

namespace aurex::parse {

class PostfixExprParser final : private ParserPartBase {
public:
    explicit PostfixExprParser(Parser& parser) noexcept : ParserPartBase(parser)
    {
    }

    [[nodiscard]] syntax::ExprId parse_postfix(ExprContext context);

private:
    struct GenericSuffixArgs {
        syntax::AstArenaVector<syntax::TypeId> type_args;
        syntax::AstArenaVector<syntax::GenericArgDecl> generic_args;
        base::SourceRange range{};
    };

    [[nodiscard]] std::optional<syntax::ExprId> parse_next_suffix(syntax::ExprId base, ExprContext context);
    [[nodiscard]] syntax::ExprId parse_bracket_suffix(syntax::ExprId base, ExprContext context);
    [[nodiscard]] std::optional<syntax::ExprId> parse_angle_generic_suffix(
        syntax::ExprId base, ExprContext context);
    [[nodiscard]] bool angle_generic_suffix_has_valid_continuation(ExprContext context) const noexcept;
    [[nodiscard]] GenericSuffixArgs parse_angle_generic_args(const syntax::Token& opening);
    [[nodiscard]] syntax::GenericArgDecl parse_angle_generic_arg();
    [[nodiscard]] syntax::ExprId parse_const_generic_expr_atom(std::string message);
    [[nodiscard]] bool recover_angle_generic_arg_separator() const;
    [[nodiscard]] bool recover_bracket_arg_separator();
    [[nodiscard]] syntax::ExprId parse_field_suffix(syntax::ExprId base);
    [[nodiscard]] syntax::ExprId parse_numeric_tuple_field_suffix(
        syntax::ExprId base, const base::SourceRange& fallback_range);
    [[nodiscard]] syntax::ExprId parse_rejected_legacy_scope_suffix(
        syntax::ExprId base, const base::SourceRange& fallback_range);
    [[nodiscard]] syntax::ExprId parse_struct_literal_suffix(syntax::ExprId base, ExprContext context);
    void parse_struct_fields(syntax::AstArenaVector<syntax::FieldInit>& fields, ExprContext context);
    [[nodiscard]] syntax::FieldInit parse_struct_field(ExprContext context);
    [[nodiscard]] bool recover_struct_field_separator();
    [[nodiscard]] const syntax::Token& expect_index_suffix_end(const syntax::Token& opening);
    [[nodiscard]] const syntax::Token& expect_slice_suffix_end(const syntax::Token& opening);
    [[nodiscard]] syntax::ExprId parse_call_suffix(syntax::ExprId base, ExprContext context);
    [[nodiscard]] std::optional<syntax::ExprId> parse_layout_query_call_suffix(
        syntax::ExprId base, const syntax::CallExprPayload& payload, const base::SourceRange& call_range);
    void parse_call_args(syntax::CallExprPayload& payload, ExprContext context);
    void parse_call_arg(syntax::CallExprPayload& payload, ExprContext context);
    [[nodiscard]] bool recover_call_arg_separator();
    [[nodiscard]] syntax::ExprId parse_try_suffix(syntax::ExprId base);
    [[nodiscard]] syntax::ExprId parse_rejected_update_suffix(
        syntax::ExprId base, const base::SourceRange& fallback_range, syntax::TokenKind kind, std::string message);
};

} // namespace aurex::parse
