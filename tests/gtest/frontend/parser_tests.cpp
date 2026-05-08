#include "aurex/base/diagnostic.hpp"
#include "aurex/lex/lexer.hpp"
#include "aurex/parse/parser.hpp"
#include "aurex/syntax/ast_dump.hpp"
#include "support/test_support.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aurex::test {
namespace {

using base::DiagnosticSink;

void expect_parse_error(const std::string_view source, const std::string_view message) {
    DiagnosticSink diagnostics;
    lex::Lexer lexer({6}, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());
    bool found = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        if (diagnostic.message.find(message) != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "missing diagnostic: " << message;
}

[[nodiscard]] syntax::AstModule parse_success(const std::string_view source) {
    DiagnosticSink diagnostics;
    lex::Lexer lexer({7}, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        ADD_FAILURE() << tokens.error().message;
        return {};
    }

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (!parsed) {
        ADD_FAILURE() << parsed.error().message;
        return {};
    }
    if (diagnostics.has_error()) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        return {};
    }
    return std::move(parsed.value());
}

[[nodiscard]] const syntax::ItemNode* find_item(
    const syntax::AstModule& module,
    const std::string_view name
) noexcept {
    for (const syntax::ItemNode& item : module.items) {
        if (item.name == name) {
            return &item;
        }
    }
    return nullptr;
}

} // namespace

TEST(CoreUnit, ParserAndAstDumpCoverLowLevelSyntaxBranches) {
    constexpr std::string_view source =
        "module parser.dump;\n"
        "pub import c.host;\n"
        "extern c {\n"
        "  opaque struct Handle;\n"
        "  fn puts(s: *const u8) -> i32 @name(\"puts\");\n"
        "  fn printf(format: *const u8, ...) -> i32 @name(\"printf\");\n"
        "}\n"
        "struct Counter { value: i32; }\n"
        "noncopy struct Owner { value: i32; }\n"
        "impl Counter {\n"
        "  pub fn inc(self: *mut Counter) -> i32 {\n"
        "    self.value = self.value + 1;\n"
        "    return self.value;\n"
        "  }\n"
        "}\n"
        "export c fn exported(argc: i32, argv: *mut *mut u8) -> i32 @name(\"exported\") {\n"
        "  var i: i32 = 0;\n"
        "  let owner: Owner = Owner { value: 1 };\n"
        "  let moved_owner: Owner = move(owner);\n"
        "  for var f: i32 = 0; f < 2; f = f + 1 {\n"
        "    if f == 1 { continue; }\n"
        "  }\n"
        "  while i < argc {\n"
        "    i = i + 1;\n"
        "    if i == 1 { continue; } else { break; }\n"
        "  }\n"
        "  defer puts(c\"cleanup\");\n"
        "  let p: *mut i32 = ptr_from_addr(*mut i32, ptr_addr(argv));\n"
        "  let n: *const u8 = null;\n"
        "  let s: str = \"hello\";\n"
        "  let b: u8 = b'\\n';\n"
        "  let a: i32 = cast(i32, argc) + bit_cast(i32, argc) + align_of(*mut i32);\n"
        "  let q: *mut i32 = ptr_cast(*mut i32, p);\n"
        "  let idx: u8 = argv[0][0];\n"
        "  return a;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer({3}, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_TRUE(parsed) << parsed.error().message;
    EXPECT_FALSE(diagnostics.has_error());

    const std::string token_dump = syntax::dump_tokens(tokens.value());
    expect_contains_all(token_dump, {
        "kw_export",
        "kw_impl",
        "kw_opaque",
        "kw_while",
        "kw_for",
        "kw_noncopy",
        "kw_move",
        "kw_break",
        "kw_continue",
        "kw_defer",
        "kw_null",
        "kw_ptr_cast",
        "kw_bit_cast",
        "kw_align_of",
        "kw_ptr_addr",
        "kw_ptr_from_addr",
        "ellipsis",
        "byte_literal",
        "string_literal",
    });

    const std::string ast = syntax::dump_ast(parsed.value());
    expect_contains_all(ast, {
        "pub import c.host",
        "opaque_struct Handle extern_c",
        "fn printf extern_c variadic @name=printf",
        "impl for Counter",
        "struct Owner noncopy",
        "fn inc for Counter",
        "fn exported export_c @name=exported",
        "stmt #",
        "while",
        "for",
        "break",
        "continue",
        "defer",
        "move_expr",
        "expr #",
        "null_literal",
        "string_literal",
        "byte_literal",
        "index",
        "ptr_cast",
        "bit_cast",
        "align_of",
        "ptr_addr",
        "ptr_from_addr",
    });
}

TEST(CoreUnit, ParserCoversRecoveryNumericEnumValuesAndNestedGenericLookahead) {
    {
        DiagnosticSink diagnostics;
        constexpr std::string_view source =
            "module parser.recover;\n"
            "let top_level = 1;\n"
            "fn ok() -> i32 { return 0; }\n";
        lex::Lexer lexer({4}, source, diagnostics);
        auto tokens = lexer.tokenize();
        ASSERT_TRUE(tokens) << tokens.error().message;

        parse::Parser parser(tokens.value(), diagnostics);
        auto parsed = parser.parse_module();
        ASSERT_FALSE(parsed);
        ASSERT_TRUE(diagnostics.has_error());
        EXPECT_GE(diagnostics.diagnostics().size(), 1U);
        expect_contains(diagnostics.diagnostics().front().message, "expected item declaration");
    }

    {
        DiagnosticSink diagnostics;
        constexpr std::string_view source =
            "module parser.numeric;\n"
            "type VoidAlias = void;\n"
            "type BoolAlias = bool;\n"
            "type I8Alias = i8;\n"
            "type I16Alias = i16;\n"
            "type U16Alias = u16;\n"
            "type U32Alias = u32;\n"
            "type I64Alias = i64;\n"
            "type U64Alias = u64;\n"
            "type IsizeAlias = isize;\n"
            "type UsizeAlias = usize;\n"
            "type F32Alias = f32;\n"
            "type F64Alias = f64;\n"
            "type HexBytes = [0x2A]u8;\n"
            "type LowerHexBytes = [0x2a]u8;\n"
            "type BinBytes = [0b1010]u8;\n"
            "type DecBytes = [1_000]u8;\n"
            "enum Code: u16 { hex = 0x2A, bin = 0b1010, dec = 1_000, }\n"
            "struct Wrap<T> { value: T; }\n"
            "enum Result<T, E>: u8 { ok(T) = 1, err(E) = 2, }\n"
            "fn main() -> i32 {\n"
            "  let nested: Wrap<Wrap<i32>> = Wrap<Wrap<i32>> { value: Wrap<i32> { value: 1 } };\n"
            "  let triple: Wrap<Wrap<Wrap<i32>>> = Wrap<Wrap<Wrap<i32>>> { value: Wrap<Wrap<i32>> { value: Wrap<i32> { value: 2 } } };\n"
            "  let ok = Result<Wrap<Wrap<i32>>, bool>.err(false)?;\n"
            "  let shifted: i32 = 8 >> 1;\n"
            "  return 0;\n"
            "}\n";
        lex::Lexer lexer({5}, source, diagnostics);
        auto tokens = lexer.tokenize();
        ASSERT_TRUE(tokens) << tokens.error().message;

        parse::Parser parser(tokens.value(), diagnostics);
        auto parsed = parser.parse_module();
        ASSERT_TRUE(parsed) << parsed.error().message;
        EXPECT_FALSE(diagnostics.has_error());

        const std::string ast = syntax::dump_ast(parsed.value());
        expect_contains_all(ast, {
            "alias void",
            "alias bool",
            "alias i8",
            "alias i16",
            "alias u16",
            "alias u32",
            "alias i64",
            "alias u64",
            "alias isize",
            "alias usize",
            "alias f32",
            "alias f64",
            "alias [42]u8",
            "alias [42]u8",
            "alias [10]u8",
            "alias [1000]u8",
            "struct_literal Wrap<Wrap<i32>>",
            "struct_literal Wrap<Wrap<Wrap<i32>>>",
            "name `Result`<Wrap<Wrap<i32>>, bool>",
            "try_expr",
            "binary",
        });
    }
}

TEST(CoreUnit, ParserCoversFocusedAngleListLookaheadRegressions) {
    constexpr std::string_view source =
        "module parser.angle;\n"
        "struct Wrap<T> { value: T; }\n"
        "enum Result<T, E>: u8 { ok(T) = 1, err(E) = 2, }\n"
        "fn main() -> i32 {\n"
        "  let call = Result<Wrap<Wrap<i32>>, bool>.ok(Wrap<Wrap<i32>> { value: Wrap<i32> { value: 1 } })?;\n"
        "  let literal: Wrap<Wrap<i32>> = Wrap<Wrap<i32>> { value: Wrap<i32> { value: 2 } };\n"
        "  let shifted: i32 = 16 >> 2;\n"
        "  return shifted;\n"
        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* main = find_item(module, "main");
    ASSERT_NE(main, nullptr);
    ASSERT_TRUE(syntax::is_valid(main->body));
    const syntax::StmtNode& body = module.stmts[main->body.value];
    ASSERT_GE(body.statements.size(), 3U);

    const syntax::StmtNode& call_stmt = module.stmts[body.statements[0].value];
    ASSERT_TRUE(syntax::is_valid(call_stmt.init));
    const syntax::ExprNode& try_expr = module.exprs[call_stmt.init.value];
    ASSERT_EQ(try_expr.kind, syntax::ExprKind::try_expr);
    ASSERT_TRUE(syntax::is_valid(try_expr.unary_operand));
    const syntax::ExprNode& call_expr = module.exprs[try_expr.unary_operand.value];
    ASSERT_EQ(call_expr.kind, syntax::ExprKind::call);
    ASSERT_TRUE(syntax::is_valid(call_expr.callee));
    const syntax::ExprNode& field_expr = module.exprs[call_expr.callee.value];
    ASSERT_EQ(field_expr.kind, syntax::ExprKind::field);
    EXPECT_EQ(field_expr.field_name, "ok");
    ASSERT_TRUE(syntax::is_valid(field_expr.object));
    const syntax::ExprNode& result_name = module.exprs[field_expr.object.value];
    ASSERT_EQ(result_name.kind, syntax::ExprKind::name);
    EXPECT_EQ(result_name.text, "Result");
    EXPECT_EQ(result_name.type_args.size(), 2U);

    const syntax::StmtNode& literal_stmt = module.stmts[body.statements[1].value];
    ASSERT_TRUE(syntax::is_valid(literal_stmt.init));
    const syntax::ExprNode& literal = module.exprs[literal_stmt.init.value];
    ASSERT_EQ(literal.kind, syntax::ExprKind::struct_literal);
    EXPECT_EQ(literal.struct_name, "Wrap");
    EXPECT_EQ(literal.struct_type_args.size(), 1U);

    const syntax::StmtNode& shifted_stmt = module.stmts[body.statements[2].value];
    ASSERT_TRUE(syntax::is_valid(shifted_stmt.init));
    const syntax::ExprNode& shifted = module.exprs[shifted_stmt.init.value];
    ASSERT_EQ(shifted.kind, syntax::ExprKind::binary);
    EXPECT_EQ(shifted.binary_op, syntax::BinaryOp::shr);
}

TEST(CoreUnit, ParserPreservesBinaryPrecedenceAndLeftAssociativity) {
    constexpr std::string_view source =
        "module parser.precedence;\n"
        "fn main() -> i32 {\n"
        "  let chain: i32 = 1 - 2 - 3;\n"
        "  let mixed: bool = 1 + 2 * 3 < 10 && false || true;\n"
        "  return 0;\n"
        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* main = find_item(module, "main");
    ASSERT_NE(main, nullptr);
    ASSERT_TRUE(syntax::is_valid(main->body));
    const syntax::StmtNode& body = module.stmts[main->body.value];
    ASSERT_GE(body.statements.size(), 2U);

    const syntax::StmtNode& chain_stmt = module.stmts[body.statements[0].value];
    ASSERT_TRUE(syntax::is_valid(chain_stmt.init));
    const syntax::ExprNode& chain = module.exprs[chain_stmt.init.value];
    ASSERT_EQ(chain.kind, syntax::ExprKind::binary);
    EXPECT_EQ(chain.binary_op, syntax::BinaryOp::sub);
    ASSERT_TRUE(syntax::is_valid(chain.binary_lhs));
    const syntax::ExprNode& chain_lhs = module.exprs[chain.binary_lhs.value];
    ASSERT_EQ(chain_lhs.kind, syntax::ExprKind::binary);
    EXPECT_EQ(chain_lhs.binary_op, syntax::BinaryOp::sub);

    const syntax::StmtNode& mixed_stmt = module.stmts[body.statements[1].value];
    ASSERT_TRUE(syntax::is_valid(mixed_stmt.init));
    const syntax::ExprNode& mixed = module.exprs[mixed_stmt.init.value];
    ASSERT_EQ(mixed.kind, syntax::ExprKind::binary);
    EXPECT_EQ(mixed.binary_op, syntax::BinaryOp::logical_or);
    ASSERT_TRUE(syntax::is_valid(mixed.binary_lhs));
    const syntax::ExprNode& logical_and = module.exprs[mixed.binary_lhs.value];
    ASSERT_EQ(logical_and.kind, syntax::ExprKind::binary);
    EXPECT_EQ(logical_and.binary_op, syntax::BinaryOp::logical_and);
    ASSERT_TRUE(syntax::is_valid(logical_and.binary_lhs));
    const syntax::ExprNode& comparison = module.exprs[logical_and.binary_lhs.value];
    ASSERT_EQ(comparison.kind, syntax::ExprKind::binary);
    EXPECT_EQ(comparison.binary_op, syntax::BinaryOp::less);
    ASSERT_TRUE(syntax::is_valid(comparison.binary_lhs));
    const syntax::ExprNode& addition = module.exprs[comparison.binary_lhs.value];
    ASSERT_EQ(addition.kind, syntax::ExprKind::binary);
    EXPECT_EQ(addition.binary_op, syntax::BinaryOp::add);
    ASSERT_TRUE(syntax::is_valid(addition.binary_rhs));
    const syntax::ExprNode& multiplication = module.exprs[addition.binary_rhs.value];
    ASSERT_EQ(multiplication.kind, syntax::ExprKind::binary);
    EXPECT_EQ(multiplication.binary_op, syntax::BinaryOp::mul);
}

TEST(CoreUnit, ParserRejectsStructLiteralInControlConditions) {
    expect_parse_error(
        "module parser.condition_struct_literal;\n"
        "struct Flag { value: bool; }\n"
        "fn main() -> i32 {\n"
        "  if Flag { value: true } { return 1; }\n"
        "  return 0;\n"
        "}\n",
        "expected ';' after expression statement"
    );
}

TEST(CoreUnit, ParserReportsIncompleteExpressionsWithoutCrashing) {
    expect_parse_error(
        "module parser.incomplete_struct_literal;\n"
        "struct Pair { value: i32; }\n"
        "fn main() -> i32 {\n"
        "  let value = Pair { value: };\n"
        "  return 0;\n"
        "}\n",
        "expected expression"
    );
    expect_parse_error(
        "module parser.incomplete_binary;\n"
        "fn main() -> i32 {\n"
        "  let value = (1 + );\n"
        "  return 0;\n"
        "}\n",
        "expected expression"
    );
}

TEST(CoreUnit, ParserCoversAbiNamesAndArrayRadicesDirectly) {
    constexpr std::string_view source =
        "module parser.abi_radix;\n"
        "extern c { fn c_puts(s: *const u8) -> i32 @name(\"puts\"); }\n"
        "type BinBytes = [0b1010]u8;\n"
        "type HexBytes = [0x2A]u8;\n"
        "type DecBytes = [1_000]u8;\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* c_puts = find_item(module, "c_puts");
    ASSERT_NE(c_puts, nullptr);
    EXPECT_EQ(c_puts->abi_name, "puts");

    const syntax::ItemNode* bin = find_item(module, "BinBytes");
    const syntax::ItemNode* hex = find_item(module, "HexBytes");
    const syntax::ItemNode* dec = find_item(module, "DecBytes");
    ASSERT_NE(bin, nullptr);
    ASSERT_NE(hex, nullptr);
    ASSERT_NE(dec, nullptr);
    ASSERT_TRUE(syntax::is_valid(bin->alias_type));
    ASSERT_TRUE(syntax::is_valid(hex->alias_type));
    ASSERT_TRUE(syntax::is_valid(dec->alias_type));
    EXPECT_EQ(module.types[bin->alias_type.value].array_count, 10U);
    EXPECT_EQ(module.types[hex->alias_type.value].array_count, 42U);
    EXPECT_EQ(module.types[dec->alias_type.value].array_count, 1000U);
}

TEST(CoreUnit, ParserCoversAdditionalDiagnosticBranches) {
    expect_parse_error(
        "module parser.private_impl;\n"
        "struct Box {}\n"
        "priv impl Box { fn value(self: Box) -> i32 { return 1; } }\n",
        "impl block cannot be private"
    );
    expect_parse_error(
        "module parser.private_extern;\n"
        "priv extern c { fn puts(s: *const u8) -> i32; }\n",
        "extern block cannot be private"
    );
    expect_parse_error(
        "module parser.private_export;\n"
        "priv export c fn main() -> i32 { return 0; }\n",
        "exported C function cannot be private"
    );
    expect_parse_error(
        "module parser.bad_export;\n"
        "export c;\n",
        "expected function declaration after 'export c'"
    );
    expect_parse_error(
        "module parser.bad_abi;\n"
        "fn f() -> i32 @wrong(\"x\") { return 0; }\n",
        "expected ABI attribute 'name'"
    );
    expect_parse_error(
        "module parser.bad_pointer;\n"
        "type Bad = *i32;\n",
        "expected 'mut' or 'const' after '*'"
    );
    expect_parse_error(
        "module parser.bad_extern_item;\n"
        "extern c { const answer: i32 = 1; }\n",
        "expected extern item"
    );
    expect_parse_error(
        "module parser.bad_impl_item;\n"
        "struct Box {}\n"
        "impl Box { const answer: i32 = 1; }\n",
        "expected function declaration in impl block"
    );
    expect_parse_error(
        "module parser.bad_type;\n"
        "fn f(value: ) -> i32 { return 0; }\n",
        "expected type"
    );
    expect_parse_error(
        "module parser.bad_import;\n"
        "import c.;\n",
        "expected identifier after '.'"
    );
}

} // namespace aurex::test
