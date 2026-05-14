#include <aurex/syntax/ast_dump.hpp>
#include <aurex/syntax/token.hpp>
#include <support/test_support.hpp>

#include <string>
#include <vector>

namespace aurex::test {
namespace {

using syntax::Token;
using syntax::TokenKind;

[[nodiscard]] syntax::TypeId push_primitive_type(
    syntax::AstModule& module,
    const syntax::PrimitiveTypeKind kind
) {
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::primitive;
    type.primitive = kind;
    return module.push_type(type);
}

[[nodiscard]] syntax::StmtId push_expr_stmt(syntax::AstModule& module, const syntax::ExprId expr) {
    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::expr;
    stmt.init = expr;
    return module.push_stmt(stmt);
}

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
    scoped_pattern.binding_names = {"value", "shade"};
    module.patterns.push_back(scoped_pattern);

    syntax::ExprNode invalid_expr;
    invalid_expr.kind = syntax::ExprKind::invalid;
    module.exprs.push_back(invalid_expr);

    syntax::ExprNode unknown_expr;
    unknown_expr.kind = static_cast<syntax::ExprKind>(255);
    module.exprs.push_back(unknown_expr);

    syntax::ExprNode struct_literal;
    struct_literal.kind = syntax::ExprKind::struct_literal;
    struct_literal.struct_name = "Pair";
    module.exprs.push_back(struct_literal);

    syntax::ExprNode match_expr;
    match_expr.kind = syntax::ExprKind::match_expr;
    match_expr.match_value = syntax::ExprId {2};
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
    expr_stmt.init = syntax::ExprId {3};
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
        "field pub bad : <invalid-type>",
        "stmt <invalid>",
        "stmt #0 unknown",
        "expr #0 invalid",
        "expr #1 unknown",
        "struct_literal Pair",
        "match_arm Color.red(value, shade)",
        "match_arm <invalid-pattern>",
        "expr <invalid>",
        "item <invalid>",
        "item #3 priv unknown mystery",
        "for",
    });
}

TEST(CoreUnit, AstDumpCoversSelectorTypePatternAndExpressionLabels) {
    std::vector<Token> tokens = {
        Token {TokenKind::kw_where, {{1}, 0, 5}, "where"},
        Token {TokenKind::kw_in, {{1}, 6, 8}, "in"},
        Token {TokenKind::kw_is, {{1}, 9, 11}, "is"},
        Token {TokenKind::kw_strvalid, {{1}, 12, 20}, "strvalid"},
        Token {TokenKind::kw_strfromutf8, {{1}, 21, 33}, "strfromutf8"},
        Token {TokenKind::colon_colon, {{1}, 34, 36}, "::"},
        Token {TokenKind::plus_plus, {{1}, 37, 39}, "++"},
        Token {TokenKind::minus_minus, {{1}, 40, 42}, "--"},
        Token {TokenKind::star_equal, {{1}, 43, 45}, "*="},
        Token {TokenKind::slash_equal, {{1}, 46, 48}, "/="},
        Token {TokenKind::percent_equal, {{1}, 49, 51}, "%="},
        Token {TokenKind::amp_equal, {{1}, 52, 54}, "&="},
        Token {TokenKind::pipe_equal, {{1}, 55, 57}, "|="},
        Token {TokenKind::caret_equal, {{1}, 58, 60}, "^="},
        Token {TokenKind::less_less_equal, {{1}, 61, 64}, "<<="},
        Token {TokenKind::greater_greater_equal, {{1}, 65, 68}, ">>="},
    };
    const std::string token_dump = syntax::dump_tokens(tokens);
    expect_contains_all(token_dump, {
        "kw_where",
        "kw_in",
        "kw_is",
        "kw_strvalid",
        "kw_strfromutf8",
        "colon_colon",
        "plus_plus",
        "minus_minus",
        "star_equal",
        "slash_equal",
        "percent_equal",
        "amp_equal",
        "pipe_equal",
        "caret_equal",
        "less_less_equal",
        "greater_greater_equal",
    });

    syntax::AstModule module;
    module.module_path.parts = {"app", "main"};
    module.imports.push_back(syntax::ImportDecl {
        syntax::ModulePath {{"core", "mem"}, {}},
        {},
        {},
        syntax::Visibility::private_,
        false,
    });

    const syntax::TypeId i32_type = push_primitive_type(module, syntax::PrimitiveTypeKind::i32);
    const syntax::TypeId bool_type = push_primitive_type(module, syntax::PrimitiveTypeKind::bool_);
    syntax::TypeNode scoped_type;
    scoped_type.kind = syntax::TypeKind::named;
    scoped_type.scope_name = "pkg";
    scoped_type.name = "Scoped";
    const syntax::TypeId scoped_type_id = module.push_type(scoped_type);
    syntax::TypeNode reference_type;
    reference_type.kind = syntax::TypeKind::reference;
    reference_type.pointer_mutability = syntax::PointerMutability::mut;
    reference_type.pointee = scoped_type_id;
    const syntax::TypeId reference_type_id = module.push_type(reference_type);
    syntax::TypeNode fn_type;
    fn_type.kind = syntax::TypeKind::function;
    fn_type.function_call_conv = syntax::FunctionCallConv::c;
    fn_type.function_is_unsafe = true;
    fn_type.function_is_variadic = true;
    fn_type.function_params = {reference_type_id, i32_type};
    fn_type.function_return = bool_type;
    const syntax::TypeId fn_type_id = module.push_type(fn_type);

    syntax::PatternNode x_binding;
    x_binding.kind = syntax::PatternKind::binding;
    x_binding.binding_name = "x";
    const syntax::PatternId x_pattern = module.push_pattern(x_binding);
    syntax::PatternNode y_binding = x_binding;
    y_binding.binding_name = "y";
    const syntax::PatternId y_pattern = module.push_pattern(y_binding);
    syntax::PatternNode tuple_pattern;
    tuple_pattern.kind = syntax::PatternKind::tuple;
    tuple_pattern.elements = {x_pattern, y_pattern};
    const syntax::PatternId tuple_pattern_id = module.push_pattern(tuple_pattern);
    syntax::PatternNode const_pattern;
    const_pattern.kind = syntax::PatternKind::const_;
    const_pattern.binding_name = "LIMIT";
    const syntax::PatternId const_pattern_id = module.push_pattern(const_pattern);
    syntax::PatternNode enum_payload_pattern;
    enum_payload_pattern.kind = syntax::PatternKind::enum_case;
    enum_payload_pattern.scoped = true;
    enum_payload_pattern.enum_name = "Option";
    enum_payload_pattern.case_name = "some";
    enum_payload_pattern.payload_patterns = {x_pattern, y_pattern};
    const syntax::PatternId enum_payload_pattern_id = module.push_pattern(enum_payload_pattern);
    syntax::PatternNode or_pattern;
    or_pattern.kind = syntax::PatternKind::or_pattern;
    or_pattern.alternatives = {const_pattern_id, enum_payload_pattern_id};
    const syntax::PatternId or_pattern_id = module.push_pattern(or_pattern);

    syntax::ExprNode scoped_name;
    scoped_name.kind = syntax::ExprKind::name;
    scoped_name.scope_name = "pkg";
    scoped_name.text = "make";
    scoped_name.type_args = {i32_type, bool_type};
    const syntax::ExprId scoped_name_id = module.push_expr(scoped_name);
    syntax::ExprNode float_literal;
    float_literal.kind = syntax::ExprKind::float_literal;
    float_literal.text = "1.0";
    const syntax::ExprId float_literal_id = module.push_expr(float_literal);
    syntax::ExprNode call;
    call.kind = syntax::ExprKind::call;
    call.callee = scoped_name_id;
    call.args = {float_literal_id};
    const syntax::ExprId call_id = module.push_expr(call);
    syntax::ExprNode generic_apply;
    generic_apply.kind = syntax::ExprKind::generic_apply;
    generic_apply.callee = scoped_name_id;
    generic_apply.type_args = {reference_type_id, fn_type_id};
    const syntax::ExprId generic_apply_id = module.push_expr(generic_apply);
    syntax::ExprNode field;
    field.kind = syntax::ExprKind::field;
    field.object = scoped_name_id;
    field.field_name = "fd";
    const syntax::ExprId field_id = module.push_expr(field);
    syntax::ExprNode index;
    index.kind = syntax::ExprKind::index;
    index.object = scoped_name_id;
    index.index = float_literal_id;
    const syntax::ExprId index_id = module.push_expr(index);
    syntax::ExprNode slice;
    slice.kind = syntax::ExprKind::slice;
    slice.object = scoped_name_id;
    slice.slice_start = float_literal_id;
    slice.slice_end = call_id;
    const syntax::ExprId slice_id = module.push_expr(slice);
    syntax::ExprNode typed_struct_literal;
    typed_struct_literal.kind = syntax::ExprKind::struct_literal;
    typed_struct_literal.scope_name = "pkg";
    typed_struct_literal.struct_name = "Box";
    typed_struct_literal.type_args = {i32_type, bool_type};
    typed_struct_literal.field_inits = {syntax::FieldInit {"value", call_id, {}}};
    const syntax::ExprId typed_struct_literal_id = module.push_expr(typed_struct_literal);
    syntax::ExprNode selector_struct_literal;
    selector_struct_literal.kind = syntax::ExprKind::struct_literal;
    selector_struct_literal.object = field_id;
    selector_struct_literal.field_inits = {syntax::FieldInit {"fd", index_id, {}}};
    const syntax::ExprId selector_struct_literal_id = module.push_expr(selector_struct_literal);
    syntax::ExprNode if_expr;
    if_expr.kind = syntax::ExprKind::if_expr;
    if_expr.condition = scoped_name_id;
    if_expr.condition_pattern = tuple_pattern_id;
    if_expr.then_expr = call_id;
    if_expr.else_expr = selector_struct_literal_id;
    const syntax::ExprId if_expr_id = module.push_expr(if_expr);
    syntax::ExprNode strvalid_expr;
    strvalid_expr.kind = syntax::ExprKind::str_is_valid_utf8;
    strvalid_expr.args = {scoped_name_id, float_literal_id};
    const syntax::ExprId strvalid_expr_id = module.push_expr(strvalid_expr);
    syntax::ExprNode strfromutf8_expr = strvalid_expr;
    strfromutf8_expr.kind = syntax::ExprKind::str_from_utf8_checked;
    const syntax::ExprId strfromutf8_expr_id = module.push_expr(strfromutf8_expr);
    syntax::ExprNode try_expr;
    try_expr.kind = syntax::ExprKind::try_expr;
    try_expr.callee = call_id;
    const syntax::ExprId try_expr_id = module.push_expr(try_expr);

    syntax::StmtNode for_range;
    for_range.kind = syntax::StmtKind::for_range;
    for_range.name = "item";
    for_range.range_start = scoped_name_id;
    for_range.range_end = call_id;
    for_range.range_step = float_literal_id;
    const syntax::StmtId for_range_id = module.push_stmt(for_range);

    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    block.statements = {
        push_expr_stmt(module, call_id),
        push_expr_stmt(module, generic_apply_id),
        push_expr_stmt(module, field_id),
        push_expr_stmt(module, index_id),
        push_expr_stmt(module, slice_id),
        push_expr_stmt(module, typed_struct_literal_id),
        push_expr_stmt(module, if_expr_id),
        push_expr_stmt(module, strvalid_expr_id),
        push_expr_stmt(module, strfromutf8_expr_id),
        push_expr_stmt(module, try_expr_id),
        for_range_id,
    };
    const syntax::StmtId body_id = module.push_stmt(block);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "dumped";
    function.generic_params = {syntax::GenericParamDecl {"T", {}}, syntax::GenericParamDecl {"U", {}}};
    function.where_constraints = {
        syntax::GenericConstraintDecl {"T", {}, {"copy", "drop"}, {}, {}},
        syntax::GenericConstraintDecl {"U", {}, {"fmt"}, {}, {}},
    };
    function.return_type = fn_type_id;
    function.body = body_id;
    static_cast<void>(module.push_item(function));

    syntax::ItemNode enum_item;
    enum_item.kind = syntax::ItemKind::enum_decl;
    enum_item.name = "LegacyPayload";
    enum_item.enum_cases = {
        syntax::EnumCaseDecl {"wrapped", reference_type_id, {}, {}, {}},
    };
    static_cast<void>(module.push_item(enum_item));

    syntax::ItemNode pattern_user;
    pattern_user.kind = syntax::ItemKind::fn_decl;
    pattern_user.name = "patterns";
    syntax::StmtNode let_stmt;
    let_stmt.kind = syntax::StmtKind::let;
    let_stmt.pattern = or_pattern_id;
    let_stmt.init = scoped_name_id;
    pattern_user.body = module.push_stmt(let_stmt);
    static_cast<void>(module.push_item(pattern_user));

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast, {
        "priv import core.mem",
        "expr #1 float_literal `1.0`",
        "expr #2 call",
        "expr #3 generic_apply[&mut pkg.Scoped, unsafe extern c fn(&mut pkg.Scoped, i32, ...) -> bool]",
        "expr #4 field .fd",
        "expr #5 index",
        "expr #6 slice",
        "expr #7 struct_literal pkg.Box[i32, bool]",
        "expr #8 struct_literal <selector>",
        "condition_pattern (x, y)",
        "expr #10 strvalid",
        "expr #11 strfromutf8",
        "expr #12 try_expr",
        "stmt #0 for_range item",
        "item #0 priv fn dumped[T, U] where T: copy + drop, U: fmt",
        "return unsafe extern c fn(&mut pkg.Scoped, i32, ...) -> bool",
        "case wrapped(&mut pkg.Scoped)",
        "const LIMIT | Option.some(x, y)",
    });
}

} // namespace aurex::test
