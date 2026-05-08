#pragma once

#include "aurex/parse/expr_context.hpp"
#include "aurex/parse/parse_session.hpp"
#include "aurex/syntax/ast.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace aurex::parse {

class Parser;
class BlockParser;
class BuiltinExprParser;
class ControlStmtParser;
class ItemParser;
class PostfixExprParser;
class PrimaryExprParser;
class TypeParser;

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
    const syntax::Token& expect_type_arg_list_end(std::string message);
    void synchronize();
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

    Parser& parser_;
    ParseSession& session_;
};

class StmtParser final : private ParserPartBase {
public:
    explicit StmtParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::StmtId parse_stmt();
    [[nodiscard]] syntax::StmtId parse_let_or_var_stmt(syntax::StmtKind kind);
    [[nodiscard]] syntax::StmtId parse_expr_or_assign_stmt();
    [[nodiscard]] syntax::StmtId parse_expr_or_assign_stmt(bool require_semicolon);
};

class BlockParser final : private ParserPartBase {
public:
    explicit BlockParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::StmtId parse_block();
    [[nodiscard]] syntax::ExprId parse_block_expr(ExprContext context = ExprContext::normal);
};

class ControlStmtParser final : private ParserPartBase {
public:
    explicit ControlStmtParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::StmtId parse_if_stmt();
    [[nodiscard]] syntax::StmtId parse_for_stmt();
    [[nodiscard]] syntax::StmtId parse_while_stmt();
    [[nodiscard]] syntax::StmtId parse_defer_stmt();
    [[nodiscard]] syntax::StmtId parse_return_stmt();
};

class TypeParser final : private ParserPartBase {
public:
    explicit TypeParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::TypeId parse_type();
    [[nodiscard]] std::vector<syntax::TypeId> parse_type_arg_list();

private:
    [[nodiscard]] syntax::TypeId parse_primitive_type();
};

class ItemParser final : private ParserPartBase {
public:
    explicit ItemParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ModulePath parse_path();
    [[nodiscard]] syntax::ImportDecl parse_import_decl();
    [[nodiscard]] syntax::ItemId parse_item();

private:
    [[nodiscard]] syntax::Visibility parse_visibility();
    [[nodiscard]] syntax::ItemId parse_const_decl();
    [[nodiscard]] syntax::ItemId parse_type_alias_decl();
    [[nodiscard]] syntax::ItemId parse_struct_decl();
    [[nodiscard]] syntax::ItemId parse_enum_decl();
    [[nodiscard]] syntax::ItemId parse_impl_block();
    [[nodiscard]] syntax::ItemId parse_extern_block();
    [[nodiscard]] syntax::ItemId parse_opaque_struct_decl();
    [[nodiscard]] syntax::ItemId parse_fn_decl(bool is_export_c, bool is_extern_c);
    [[nodiscard]] std::vector<std::string_view> parse_generic_param_list();
    [[nodiscard]] std::vector<syntax::ParamDecl> parse_param_list(bool& is_variadic);
    [[nodiscard]] syntax::TypeId parse_optional_return_type();
    void parse_optional_abi_name(syntax::ItemNode& item);
};

class ExprParser final : private ParserPartBase {
public:
    explicit ExprParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ExprId parse_expr(ExprContext context = ExprContext::normal);

private:
    [[nodiscard]] syntax::ExprId parse_if_expr(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_match_expr(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_logical_or(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_logical_and(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_bit_or(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_bit_xor(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_bit_and(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_equality(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_compare(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_shift(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_add(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_mul(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_unary(ExprContext context);
    [[nodiscard]] syntax::ExprId make_binary(
        syntax::BinaryOp op,
        syntax::ExprId lhs,
        syntax::ExprId rhs,
        base::SourceRange range
    );
};

class PrimaryExprParser final : private ParserPartBase {
public:
    explicit PrimaryExprParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ExprId parse_primary(ExprContext context);

private:
    [[nodiscard]] syntax::ExprId parse_name_or_struct_literal(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_literal(syntax::ExprKind kind);
    [[nodiscard]] syntax::ExprId make_invalid_expr();
};

class PostfixExprParser final : private ParserPartBase {
public:
    explicit PostfixExprParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ExprId parse_postfix(ExprContext context);

private:
    [[nodiscard]] syntax::ExprId parse_type_args_suffix(syntax::ExprId expr);
    [[nodiscard]] syntax::ExprId parse_field_suffix(syntax::ExprId expr);
    [[nodiscard]] syntax::ExprId parse_index_suffix(syntax::ExprId expr, ExprContext context);
    [[nodiscard]] syntax::ExprId parse_call_suffix(syntax::ExprId expr, ExprContext context);
    [[nodiscard]] syntax::ExprId parse_try_suffix(syntax::ExprId expr);
};

class BuiltinExprParser final : private ParserPartBase {
public:
    explicit BuiltinExprParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ExprId parse_cast(syntax::ExprKind kind, ExprContext context);
    [[nodiscard]] syntax::ExprId parse_type_builtin(syntax::ExprKind kind);
    [[nodiscard]] syntax::ExprId parse_ptr_addr(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_ptr_from_addr(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_move(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_str_unary(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_str_from_bytes_unchecked(ExprContext context);
};

class PatternParser final : private ParserPartBase {
public:
    explicit PatternParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::PatternId parse_pattern();

private:
    [[nodiscard]] syntax::PatternId parse_pattern_atom();
};

} // namespace aurex::parse
