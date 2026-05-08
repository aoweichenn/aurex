#pragma once

#include "aurex/base/diagnostic.hpp"
#include "aurex/base/result.hpp"
#include "aurex/syntax/ast.hpp"
#include "aurex/syntax/token.hpp"

#include <span>
#include <string>

namespace aurex::parse {

class Parser final {
public:
    // Parser depends only on tokens, not on Lexer. This keeps syntax tests and
    // future parser replacements independent from the scanning implementation.
    Parser(
        std::span<const syntax::Token> tokens,
        base::DiagnosticSink& diagnostics
    ) noexcept;

    [[nodiscard]] base::Result<syntax::AstModule> parse_module();

private:
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
    const syntax::Token& expect_type_arg_list_end(std::string message);
    void synchronize();
    void report_here(std::string message);
    void report_at(const syntax::Token& token, std::string message);

    [[nodiscard]] syntax::ModulePath parse_path();
    [[nodiscard]] syntax::ImportDecl parse_import_decl();
    [[nodiscard]] syntax::Visibility parse_visibility();
    [[nodiscard]] syntax::ItemId parse_item();
    [[nodiscard]] syntax::ItemId parse_const_decl();
    [[nodiscard]] syntax::ItemId parse_type_alias_decl();
    [[nodiscard]] syntax::ItemId parse_struct_decl();
    [[nodiscard]] syntax::ItemId parse_enum_decl();
    [[nodiscard]] syntax::ItemId parse_impl_block();
    [[nodiscard]] syntax::ItemId parse_extern_block();
    [[nodiscard]] syntax::ItemId parse_opaque_struct_decl();
    [[nodiscard]] syntax::ItemId parse_fn_decl(bool is_export_c, bool is_extern_c);
    [[nodiscard]] std::vector<std::string_view> parse_generic_param_list();
    [[nodiscard]] std::vector<syntax::TypeId> parse_type_arg_list();
    [[nodiscard]] std::vector<syntax::ParamDecl> parse_param_list(bool& is_variadic);
    [[nodiscard]] syntax::TypeId parse_optional_return_type();
    void parse_optional_abi_name(syntax::ItemNode& item);

    [[nodiscard]] syntax::TypeId parse_type();
    [[nodiscard]] syntax::TypeId parse_primitive_type();

    [[nodiscard]] syntax::StmtId parse_block();
    [[nodiscard]] syntax::ExprId parse_block_expr();
    [[nodiscard]] syntax::StmtId parse_stmt();
    [[nodiscard]] syntax::StmtId parse_let_or_var_stmt(syntax::StmtKind kind);
    [[nodiscard]] syntax::StmtId parse_if_stmt();
    [[nodiscard]] syntax::StmtId parse_for_stmt();
    [[nodiscard]] syntax::StmtId parse_while_stmt();
    [[nodiscard]] syntax::StmtId parse_defer_stmt();
    [[nodiscard]] syntax::StmtId parse_return_stmt();
    [[nodiscard]] syntax::StmtId parse_expr_or_assign_stmt();
    [[nodiscard]] syntax::StmtId parse_expr_or_assign_stmt(bool require_semicolon);

    [[nodiscard]] syntax::ExprId parse_expr();
    [[nodiscard]] syntax::ExprId parse_if_expr();
    [[nodiscard]] syntax::ExprId parse_match_expr();
    [[nodiscard]] syntax::PatternId parse_pattern();
    [[nodiscard]] syntax::PatternId parse_pattern_atom();
    [[nodiscard]] syntax::ExprId parse_logical_or();
    [[nodiscard]] syntax::ExprId parse_logical_and();
    [[nodiscard]] syntax::ExprId parse_bit_or();
    [[nodiscard]] syntax::ExprId parse_bit_xor();
    [[nodiscard]] syntax::ExprId parse_bit_and();
    [[nodiscard]] syntax::ExprId parse_equality();
    [[nodiscard]] syntax::ExprId parse_compare();
    [[nodiscard]] syntax::ExprId parse_shift();
    [[nodiscard]] syntax::ExprId parse_add();
    [[nodiscard]] syntax::ExprId parse_mul();
    [[nodiscard]] syntax::ExprId parse_unary();
    [[nodiscard]] syntax::ExprId parse_postfix();
    [[nodiscard]] syntax::ExprId parse_primary();
    [[nodiscard]] syntax::ExprId parse_builtin_cast(syntax::ExprKind kind);
    [[nodiscard]] syntax::ExprId parse_type_builtin(syntax::ExprKind kind);
    [[nodiscard]] syntax::ExprId parse_move_expr();
    [[nodiscard]] syntax::ExprId make_binary(syntax::BinaryOp op, syntax::ExprId lhs, syntax::ExprId rhs, base::SourceRange range);
    [[nodiscard]] syntax::ExprId make_invalid_expr();

    [[nodiscard]] base::SourceRange merge(base::SourceRange begin, base::SourceRange end) const noexcept;

    std::span<const syntax::Token> tokens_;
    base::DiagnosticSink& diagnostics_;
    syntax::AstModule module_;
    base::usize current_ = 0;
    syntax::Token split_greater_tail_ {};
    syntax::Token last_split_greater_ {};
    bool pending_split_greater_tail_ = false;
    bool previous_was_split_greater_ = false;
    bool panic_ = false;
    bool allow_struct_literal_ = true;
};

} // namespace aurex::parse
