#include <aurex/syntax/ast_dump.hpp>
#include <aurex/syntax/token.hpp>
#include <support/test_support.hpp>

#include <string>
#include <vector>

namespace aurex::test {
namespace {

using syntax::Token;
using syntax::TokenKind;

} // namespace

TEST(CoreUnit, AstDumpCoversInvalidAndFallbackLabels) {
    std::vector<Token> tokens = {
        Token {TokenKind::invalid, {{6}, 0, 1}, "?"},
        Token {static_cast<TokenKind>(255), {{6}, 1, 2}, ""},
    };
    const std::string token_dump = syntax::dump_tokens(tokens);
    expect_contains_all(token_dump, {
        "invalid",
        "unknown",
    });

    syntax::AstModule module;
    syntax::TypeNode i32_type;
    i32_type.kind = syntax::TypeKind::primitive;
    i32_type.primitive = syntax::PrimitiveTypeKind::i32;
    module.types.push_back(i32_type);
    syntax::TypeNode bool_type = i32_type;
    bool_type.primitive = syntax::PrimitiveTypeKind::bool_;
    module.types.push_back(bool_type);

    syntax::PatternNode scoped_pattern;
    scoped_pattern.kind = syntax::PatternKind::enum_case;
    scoped_pattern.scoped = true;
    scoped_pattern.enum_name = "Color";
    scoped_pattern.case_name = "red";
    scoped_pattern.binding_name = "value";
    module.patterns.push_back(scoped_pattern);

    syntax::ExprNode invalid_expr;
    invalid_expr.kind = syntax::ExprKind::invalid;
    module.exprs.push_back(invalid_expr);

    syntax::ExprNode unknown_expr;
    unknown_expr.kind = static_cast<syntax::ExprKind>(255);
    module.exprs.push_back(unknown_expr);

    syntax::ExprNode typed_name;
    typed_name.kind = syntax::ExprKind::name;
    typed_name.text = "Generic";
    typed_name.type_args = {syntax::TypeId {0}, syntax::TypeId {1}};
    module.exprs.push_back(typed_name);

    syntax::ExprNode struct_literal;
    struct_literal.kind = syntax::ExprKind::struct_literal;
    struct_literal.struct_name = "Pair";
    struct_literal.struct_type_args = {syntax::TypeId {0}, syntax::TypeId {1}};
    module.exprs.push_back(struct_literal);

    syntax::ExprNode match_expr;
    match_expr.kind = syntax::ExprKind::match_expr;
    match_expr.match_value = syntax::ExprId {3};
    match_expr.match_arms = {
        syntax::MatchArm {syntax::PatternId {0}, syntax::INVALID_EXPR_ID, syntax::ExprId {1}, {}},
        syntax::MatchArm {syntax::INVALID_PATTERN_ID, syntax::INVALID_EXPR_ID, syntax::ExprId {99}, {}},
    };
    module.exprs.push_back(match_expr);

    syntax::StmtNode unknown_stmt;
    unknown_stmt.kind = static_cast<syntax::StmtKind>(255);
    unknown_stmt.init = syntax::ExprId {2};
    module.stmts.push_back(unknown_stmt);

    syntax::StmtNode expr_stmt;
    expr_stmt.kind = syntax::StmtKind::expr;
    expr_stmt.init = syntax::ExprId {4};
    module.stmts.push_back(expr_stmt);

    syntax::StmtNode if_stmt;
    if_stmt.kind = syntax::StmtKind::if_;
    if_stmt.condition = syntax::ExprId {0};
    if_stmt.else_if = syntax::StmtId {0};
    module.stmts.push_back(if_stmt);

    syntax::StmtNode for_stmt;
    for_stmt.kind = syntax::StmtKind::for_;
    for_stmt.condition = syntax::ExprId {0};
    for_stmt.for_init = syntax::StmtId {1};
    for_stmt.for_update = syntax::StmtId {0};
    module.stmts.push_back(for_stmt);

    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    block.statements = {syntax::StmtId {99}, syntax::StmtId {1}, syntax::StmtId {2}, syntax::StmtId {3}};
    module.stmts.push_back(block);

    syntax::ItemNode broken_struct;
    broken_struct.kind = syntax::ItemKind::struct_decl;
    broken_struct.name = "Broken";
    broken_struct.fields.push_back(syntax::FieldDecl {"bad", syntax::INVALID_TYPE_ID, {}, syntax::Visibility::public_});
    module.items.push_back(broken_struct);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "body";
    function.body = syntax::StmtId {4};
    module.items.push_back(function);

    syntax::ItemNode extern_block;
    extern_block.kind = syntax::ItemKind::extern_block;
    extern_block.extern_items = {syntax::INVALID_ITEM_ID, syntax::ItemId {99}};
    module.items.push_back(extern_block);

    syntax::ItemNode unknown_item;
    unknown_item.kind = static_cast<syntax::ItemKind>(255);
    unknown_item.name = "mystery";
    module.items.push_back(unknown_item);

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast, {
        "field bad : <invalid-type>",
        "stmt <invalid>",
        "stmt #0 unknown",
        "expr #0 invalid",
        "expr #1 unknown",
        "name `Generic`<i32, bool>",
        "struct_literal Pair<i32, bool>",
        "match_arm Color.red(value)",
        "match_arm <invalid-pattern>",
        "expr <invalid>",
        "item <invalid>",
        "item #3 unknown mystery",
        "for",
    });
}

} // namespace aurex::test
