#pragma once

#include <aurex/parse/parser_part_base.hpp>

#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace aurex::parse {

class PostfixExprParser final : private ParserPartBase {
public:
    explicit PostfixExprParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ExprId parse_postfix(ExprContext context);

private:
    struct BracketArg {
        syntax::ExprId expr = syntax::INVALID_EXPR_ID;
        syntax::TypeId type = syntax::INVALID_TYPE_ID;
        base::SourceRange range {};
    };

    [[nodiscard]] std::optional<syntax::ExprId> parse_next_suffix(syntax::ExprId base, ExprContext context);
    [[nodiscard]] syntax::ExprId parse_bracket_suffix(syntax::ExprId base, ExprContext context);
    [[nodiscard]] BracketArg parse_bracket_arg(ExprContext context);
    [[nodiscard]] bool bracket_arg_starts_type_only() const noexcept;
    [[nodiscard]] bool bracket_parenthesized_arg_is_type() const noexcept;
    [[nodiscard]] bool bracket_suffix_is_inside_generic_continuation() const noexcept;
    [[nodiscard]] bool bracket_args_contain_type_only(std::span<const BracketArg> args) const noexcept;
    [[nodiscard]] bool bracket_args_are_type_like(std::span<const BracketArg> args) const;
    [[nodiscard]] bool bracket_arg_expr_is_type_like(syntax::ExprId expr) const;
    [[nodiscard]] std::optional<syntax::AstArenaVector<syntax::TypeId>> bracket_args_to_type_args(
        std::span<const BracketArg> args,
        bool report_errors
    );
    [[nodiscard]] syntax::TypeId bracket_arg_expr_to_type(syntax::ExprId expr, bool report_errors);
    [[nodiscard]] syntax::TypeId append_type_selector(
        syntax::TypeId base,
        std::string_view name,
        const base::SourceRange& range,
        bool report_errors
    );
    [[nodiscard]] bool recover_bracket_arg_separator();
    [[nodiscard]] syntax::ExprId parse_field_suffix(syntax::ExprId base);
    [[nodiscard]] syntax::ExprId parse_rejected_legacy_scope_suffix(
        syntax::ExprId base,
        const base::SourceRange& fallback_range
    );
    [[nodiscard]] syntax::ExprId parse_struct_literal_suffix(syntax::ExprId base, ExprContext context);
    void parse_struct_fields(syntax::AstArenaVector<syntax::FieldInit>& fields, ExprContext context);
    [[nodiscard]] syntax::FieldInit parse_struct_field(ExprContext context);
    [[nodiscard]] bool recover_struct_field_separator();
    [[nodiscard]] syntax::ExprId parse_rejected_numeric_tuple_field_suffix(
        syntax::ExprId base,
        const base::SourceRange& fallback_range
    );
    [[nodiscard]] const syntax::Token& expect_index_suffix_end(const syntax::Token& opening);
    [[nodiscard]] const syntax::Token& expect_slice_suffix_end(const syntax::Token& opening);
    [[nodiscard]] syntax::ExprId parse_call_suffix(syntax::ExprId base, ExprContext context);
    void parse_call_args(syntax::AstArenaVector<syntax::ExprId>& args, ExprContext context);
    [[nodiscard]] bool recover_call_arg_separator();
    [[nodiscard]] syntax::ExprId parse_try_suffix(syntax::ExprId base);
    [[nodiscard]] syntax::ExprId parse_rejected_update_suffix(
        syntax::ExprId base,
        const base::SourceRange& fallback_range,
        syntax::TokenKind kind,
        std::string message
    );
};

} // namespace aurex::parse
