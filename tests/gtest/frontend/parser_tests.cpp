#include <aurex/base/diagnostic.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/parse/parser.hpp>
#include <aurex/syntax/ast_dump.hpp>
#include <support/test_support.hpp>

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
        "struct Owner { value: i32; }\n"
        "impl Counter {\n"
        "  pub fn inc(self: *mut Counter) -> i32 {\n"
        "    self.value = self.value + 1;\n"
        "    return self.value;\n"
        "  }\n"
        "}\n"
        "export c fn exported(argc: i32, argv: *mut *mut u8) -> i32 @name(\"exported\") {\n"
        "  var i: i32 = 0;\n"
        "  let owner: Owner = Owner { value: 1 };\n"
        "  let copied_owner: Owner = owner;\n"
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
        "struct Owner",
        "fn inc for Counter",
        "fn exported export_c @name=exported",
        "stmt #",
        "while",
        "for",
        "break",
        "continue",
        "defer",
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

TEST(CoreUnit, ParserRecoveryStopsAtNextItemWithoutSemicolon) {
    constexpr base::SourceId PARSER_TEST_RECOVERY_SOURCE_ID {8};
    constexpr std::string_view source =
        "module parser.recovery_boundary;\n"
        "let top_level = 1\n"
        "fn recovered() -> i32 {\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected item declaration");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserAcceptsFrozenTrailingSeparatorPolicy) {
    constexpr std::string_view source =
        "module parser.trailing_separators;\n"
        "struct Pair<T, U,> { left: T; right: U; }\n"
        "enum Choice<T, E>: u8 { ok = 1, err(E) = 2 }\n"
        "fn choose<T, E,>(first: T, second: E,) -> Choice<T, E> {\n"
        "  let pair = Pair<T, E> { left: first, right: second, };\n"
        "  if pair.left == first { return Choice<T, E>.ok; }\n"
        "  return Choice<T, E>.err(pair.right);\n"
        "}\n"
        "fn score() -> i32 {\n"
        "  let value = choose<i32, bool>(41, false,);\n"
        "  return match value {\n"
        "    .ok => 41,\n"
        "    .err(flag) => if flag { 1 } else { 0 }\n"
        "  };\n"
        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* pair = find_item(module, "Pair");
    ASSERT_NE(pair, nullptr);
    EXPECT_EQ(pair->generic_params.size(), 2U);
    EXPECT_EQ(pair->fields.size(), 2U);

    const syntax::ItemNode* choice = find_item(module, "Choice");
    ASSERT_NE(choice, nullptr);
    EXPECT_EQ(choice->generic_params.size(), 2U);
    EXPECT_EQ(choice->enum_cases.size(), 2U);

    const syntax::ItemNode* choose = find_item(module, "choose");
    ASSERT_NE(choose, nullptr);
    EXPECT_EQ(choose->generic_params.size(), 2U);
    EXPECT_EQ(choose->params.size(), 2U);
}

TEST(CoreUnit, ParserAcceptsUnifiedBlockExpressionBody) {
    constexpr std::string_view source =
        "module parser.block_body;\n"
        "fn main() -> i32 {\n"
        "  let value = {\n"
        "    var total: i32 = 0;\n"
        "    if total == 0 {\n"
        "      total += 1;\n"
        "    }\n"
        "    while total < 2 {\n"
        "      total += 1;\n"
        "    }\n"
        "    for var i: i32 = 0; i < 3; i += 1 {\n"
        "      if i == 1 { continue; }\n"
        "      total += i;\n"
        "    }\n"
        "    {\n"
        "      total += 10;\n"
        "    }\n"
        "    if total == 14 { total } else { 0 }\n"
        "  };\n"
        "  return value;\n"
        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* main = find_item(module, "main");
    ASSERT_NE(main, nullptr);
    ASSERT_TRUE(syntax::is_valid(main->body));
    const syntax::StmtNode& body = module.stmts[main->body.value];
    ASSERT_GE(body.statements.size(), 2U);
    const syntax::StmtNode& value_stmt = module.stmts[body.statements.front().value];
    ASSERT_EQ(value_stmt.kind, syntax::StmtKind::let);
    ASSERT_TRUE(syntax::is_valid(value_stmt.init));
    const syntax::ExprNode& block_expr = module.exprs[value_stmt.init.value];
    ASSERT_EQ(block_expr.kind, syntax::ExprKind::block_expr);
    ASSERT_TRUE(syntax::is_valid(block_expr.block));
    ASSERT_TRUE(syntax::is_valid(block_expr.block_result));
    const syntax::StmtNode& expr_block = module.stmts[block_expr.block.value];
    ASSERT_EQ(expr_block.kind, syntax::StmtKind::block);
    ASSERT_EQ(expr_block.statements.size(), 5U);
    EXPECT_EQ(module.stmts[expr_block.statements[1].value].kind, syntax::StmtKind::if_);
    EXPECT_EQ(module.stmts[expr_block.statements[2].value].kind, syntax::StmtKind::while_);
    EXPECT_EQ(module.stmts[expr_block.statements[3].value].kind, syntax::StmtKind::for_);
    EXPECT_EQ(module.stmts[expr_block.statements[4].value].kind, syntax::StmtKind::block);
    EXPECT_EQ(module.exprs[block_expr.block_result.value].kind, syntax::ExprKind::if_expr);
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedTypeArgumentSeparators) {
    constexpr base::SourceId PARSER_TEST_TYPE_ARG_RECOVERY_SOURCE_ID {9};
    constexpr std::string_view source =
        "module parser.type_arg_recovery;\n"
        "type Broken = Pair<i32 bool, str>;\n"
        "fn recovered() -> i32 {\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_TYPE_ARG_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ',' or '>' after type argument");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedMatchArmSeparators) {
    constexpr base::SourceId PARSER_TEST_MATCH_ARM_RECOVERY_SOURCE_ID {10};
    constexpr std::string_view source =
        "module parser.match_arm_recovery;\n"
        "fn recovered(value: i32) -> i32 {\n"
        "  let matched = match value {\n"
        "    0 => 0 @\n"
        "    1 => 1,\n"
        "  };\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_MATCH_ARM_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ',' or '}' after match arm");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedCallArgumentSeparators) {
    constexpr base::SourceId PARSER_TEST_CALL_ARG_RECOVERY_SOURCE_ID {11};
    constexpr std::string_view source =
        "module parser.call_arg_recovery;\n"
        "fn recovered() -> i32 {\n"
        "  let value = id(1 @ 2, 3);\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_CALL_ARG_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ',' or ')' after argument");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedStructLiteralFieldSeparators) {
    constexpr base::SourceId PARSER_TEST_STRUCT_FIELD_RECOVERY_SOURCE_ID {12};
    constexpr std::string_view source =
        "module parser.struct_field_recovery;\n"
        "struct Pair { a: i32; b: i32; }\n"
        "fn recovered() -> i32 {\n"
        "  let value = Pair { a: 1 @ b: 2 };\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_STRUCT_FIELD_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ',' or '}' after struct literal field");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedParameterSeparators) {
    constexpr base::SourceId PARSER_TEST_PARAM_RECOVERY_SOURCE_ID {13};
    constexpr std::string_view source =
        "module parser.parameter_recovery;\n"
        "fn recovered(a: i32 @ b: i32) -> i32 {\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_PARAM_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ',' or ')' after parameter");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedStructDeclarationFieldSeparators) {
    constexpr base::SourceId PARSER_TEST_STRUCT_DECL_FIELD_RECOVERY_SOURCE_ID {14};
    constexpr std::string_view source =
        "module parser.struct_decl_field_recovery;\n"
        "struct Pair { a: i32 @ b: i32; }\n"
        "fn recovered() -> i32 {\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_STRUCT_DECL_FIELD_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ';' or '}' after field declaration");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedEnumCaseSeparators) {
    constexpr base::SourceId PARSER_TEST_ENUM_CASE_RECOVERY_SOURCE_ID {15};
    constexpr std::string_view source =
        "module parser.enum_case_recovery;\n"
        "enum Code: u8 { a = 1 @ b = 2, }\n"
        "fn recovered() -> i32 {\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_ENUM_CASE_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ',' or '}' after enum case");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedGenericParameterSeparators) {
    constexpr base::SourceId PARSER_TEST_GENERIC_PARAM_RECOVERY_SOURCE_ID {16};
    constexpr std::string_view source =
        "module parser.generic_param_recovery;\n"
        "struct Box<T @ U> { value: T; }\n"
        "fn recovered() -> i32 {\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_GENERIC_PARAM_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ',' or '>' after generic parameter");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedAbiAttributeArguments) {
    constexpr base::SourceId PARSER_TEST_ABI_ARGUMENT_RECOVERY_SOURCE_ID {17};
    constexpr std::string_view source =
        "module parser.abi_argument_recovery;\n"
        "extern c { fn puts(s: *const u8) -> i32 @name(\"puts\" @); }\n"
        "fn recovered() -> i32 {\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_ABI_ARGUMENT_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ')' after ABI attribute");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedImportPathSegments) {
    constexpr base::SourceId PARSER_TEST_PATH_RECOVERY_SOURCE_ID {18};
    constexpr std::string_view source =
        "module parser.path_recovery;\n"
        "import c.@;\n"
        "fn recovered() -> i32 {\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_PATH_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected identifier after '.'");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedBuiltinArgumentSeparators) {
    constexpr base::SourceId PARSER_TEST_BUILTIN_ARG_RECOVERY_SOURCE_ID {19};
    constexpr std::string_view source =
        "module parser.builtin_arg_recovery;\n"
        "fn recovered(argc: i32) -> i32 {\n"
        "  let value = cast(i32 @ argc);\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_BUILTIN_ARG_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ',' after cast type");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedImportAliases) {
    constexpr base::SourceId PARSER_TEST_IMPORT_ALIAS_RECOVERY_SOURCE_ID {20};
    constexpr std::string_view source =
        "module parser.import_alias_recovery;\n"
        "import c.host as @;\n"
        "fn recovered() -> i32 {\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_IMPORT_ALIAS_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected import alias after 'as'");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedStatementTerminators) {
    constexpr base::SourceId PARSER_TEST_STATEMENT_TERMINATOR_RECOVERY_SOURCE_ID {21};
    constexpr std::string_view source =
        "module parser.statement_terminator_recovery;\n"
        "fn recovered() -> i32 {\n"
        "  let value = 1 @;\n"
        "  value @;\n"
        "  return 1 @;\n"
        "  let broken = ;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_STATEMENT_TERMINATOR_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ';' after local declaration");
    expect_contains(messages, "expected ';' after expression statement");
    expect_contains(messages, "expected ';' after return");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedForClauseSeparators) {
    constexpr base::SourceId PARSER_TEST_FOR_CLAUSE_RECOVERY_SOURCE_ID {22};
    constexpr std::string_view source =
        "module parser.for_clause_recovery;\n"
        "fn recovered() -> i32 {\n"
        "  for var i: i32 = 0; i < 3 @; i = i + 1 {\n"
        "  }\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_FOR_CLAUSE_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ';' after for condition");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedBlockOpeners) {
    constexpr base::SourceId PARSER_TEST_BLOCK_OPENER_RECOVERY_SOURCE_ID {23};
    constexpr std::string_view source =
        "module parser.block_opener_recovery;\n"
        "fn recovered(value: i32) -> i32 {\n"
        "  if value > 0 @ {\n"
        "    return 1;\n"
        "  }\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_BLOCK_OPENER_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected block");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryStopsMissingBlockEndAtNextItem) {
    constexpr base::SourceId PARSER_TEST_BLOCK_END_RECOVERY_SOURCE_ID {24};
    constexpr std::string_view source =
        "module parser.block_end_recovery;\n"
        "fn first() -> i32 {\n"
        "  return 1;\n"
        "fn second() -> i32 {\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_BLOCK_END_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected '}' after block");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedExpressionDelimiters) {
    constexpr base::SourceId PARSER_TEST_EXPRESSION_DELIMITER_RECOVERY_SOURCE_ID {25};
    constexpr std::string_view source =
        "module parser.expression_delimiter_recovery;\n"
        "fn recovered(values: *mut i32) -> i32 {\n"
        "  let grouped = (1 @);\n"
        "  let indexed = values[0 @];\n"
        "  let builtin = ptr_addr(values @);\n"
        "  let called = id(1 @);\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_EXPRESSION_DELIMITER_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ')' after expression");
    expect_contains(messages, "expected ']' after index");
    expect_contains(messages, "expected ')' after ptr_addr argument");
    expect_contains(messages, "expected ',' or ')' after argument");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedTypeAndPatternDelimiters) {
    constexpr base::SourceId PARSER_TEST_TYPE_PATTERN_DELIMITER_RECOVERY_SOURCE_ID {26};
    constexpr std::string_view source =
        "module parser.type_pattern_delimiter_recovery;\n"
        "type Bytes = [4 @]u8;\n"
        "enum Maybe: u8 { some(i32 @) = 1, none = 2, }\n"
        "fn recovered(value: Maybe) -> i32 {\n"
        "  let matched = match value {\n"
        "    .some(payload @) => 1,\n"
        "    .none => 0,\n"
        "  };\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_TYPE_PATTERN_DELIMITER_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ']' after array length");
    expect_contains(messages, "expected ')' after enum case payload type");
    expect_contains(messages, "expected ')' after payload binding");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedDeclarationDelimiters) {
    constexpr base::SourceId PARSER_TEST_DECLARATION_DELIMITER_RECOVERY_SOURCE_ID {27};
    constexpr std::string_view source =
        "module parser.declaration_delimiter_recovery;\n"
        "import c.host @;\n"
        "type Alias = i32 @;\n"
        "struct Pair @ { value: i32; }\n"
        "enum Code: u8 @ { ok = 1, }\n"
        "extern c @ {\n"
        "  opaque struct Handle @;\n"
        "  fn puts(s: *const u8) -> i32 @name(\"puts\") @;\n"
        "}\n"
        "impl Pair @ {\n"
        "  fn value(self: *const Pair) -> i32 { return 0; }\n"
        "}\n"
        "fn recovered(a: i32 @) -> i32 {\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_DECLARATION_DELIMITER_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ';' after import declaration");
    expect_contains(messages, "expected ';' after type alias declaration");
    expect_contains(messages, "expected '{' after struct name");
    expect_contains(messages, "expected '{' after enum base type");
    expect_contains(messages, "expected '{' after 'extern c'");
    expect_contains(messages, "expected ';' after opaque struct declaration");
    expect_contains(messages, "expected ';' after extern function declaration");
    expect_contains(messages, "expected '{' after impl type");
    expect_contains(messages, "expected ',' or ')' after parameter");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedCoreSeparators) {
    constexpr base::SourceId PARSER_TEST_CORE_SEPARATOR_RECOVERY_SOURCE_ID {28};
    constexpr std::string_view source =
        "module parser.core_separator_recovery @;\n"
        "const Broken: i32 @= 1;\n"
        "type Alias @= i32;\n"
        "struct Pair { value @: i32; }\n"
        "enum Code @: u8 { ok @= 1, }\n"
        "fn recovered(a @: i32) -> i32 {\n"
        "  let value: i32 @= 1;\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_CORE_SEPARATOR_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected ';' after module declaration");
    expect_contains(messages, "expected '=' in const declaration");
    expect_contains(messages, "expected '=' in type alias declaration");
    expect_contains(messages, "expected ':' after field name");
    expect_contains(messages, "expected ':' after enum name");
    expect_contains(messages, "expected '=' after enum case name");
    expect_contains(messages, "expected ':' after parameter name");
    expect_contains(messages, "expected initializer");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedControlSeparators) {
    constexpr base::SourceId PARSER_TEST_CONTROL_SEPARATOR_RECOVERY_SOURCE_ID {29};
    constexpr std::string_view source =
        "module parser.control_separator_recovery;\n"
        "fn recovered(value: i32) -> i32 {\n"
        "  let selected = if value > 0 { 1 } @ else { 0 };\n"
        "  let matched = match value {\n"
        "    0 @=> 1,\n"
        "    1 => 2,\n"
        "  };\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_CONTROL_SEPARATOR_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "if expression requires else branch");
    expect_contains(messages, "expected '=>' after match case");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedOpeningDelimiters) {
    constexpr base::SourceId PARSER_TEST_OPENING_DELIMITER_RECOVERY_SOURCE_ID {30};
    constexpr std::string_view source =
        "module parser.opening_delimiter_recovery;\n"
        "extern c {\n"
        "  fn puts(s: *const u8) -> i32 @name @(\"puts\");\n"
        "}\n"
        "fn opened @(a: i32) -> i32 { return a; }\n"
        "fn recovered(value: i32) -> i32 {\n"
        "  let casted = cast @(i32, value);\n"
        "  let broken_builtin = ptr_from_addr @(*mut i32, casted);\n"
        "  let broken = ;\n"
        "  return casted;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_OPENING_DELIMITER_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected '(' after ABI attribute");
    expect_contains(messages, "expected '(' after function name");
    expect_contains(messages, "expected '(' after cast builtin");
    expect_contains(messages, "expected '(' after ptr_from_addr");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedIdentifiers) {
    constexpr base::SourceId PARSER_TEST_IDENTIFIER_RECOVERY_SOURCE_ID {31};
    constexpr std::string_view source =
        "module parser.identifier_recovery;\n"
        "import c.host as @host;\n"
        "struct @Pair { @value: i32; }\n"
        "enum @Code: u8 { @ok = 1, }\n"
        "fn @recovered(@value: i32) -> i32 {\n"
        "  let @local = value;\n"
        "  let field = local.@value;\n"
        "  let matched = match 1 {\n"
        "    .@ok => 1,\n"
        "  };\n"
        "  let broken = ;\n"
        "  return matched;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_IDENTIFIER_RECOVERY_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    expect_contains(messages, "expected import alias after 'as'");
    expect_contains(messages, "expected struct name");
    expect_contains(messages, "expected field name");
    expect_contains(messages, "expected enum name");
    expect_contains(messages, "expected enum case name");
    expect_contains(messages, "expected function name");
    expect_contains(messages, "expected parameter name");
    expect_contains(messages, "expected local name");
    expect_contains(messages, "expected field name after '.'");
    expect_contains(messages, "expected enum case name after '.'");
    expect_contains(messages, "expected expression");
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

TEST(CoreUnit, ParserCoversCompoundAssignmentStatements) {
    constexpr std::string_view source =
        "module parser.assign_ops;\n"
        "fn main() -> i32 {\n"
        "  var value: i32 = 1;\n"
        "  value += 2;\n"
        "  value <<= 1;\n"
        "  value -= 1;\n"
        "  for var i: i32 = 0; i < 2; i += 1 {\n"
        "    value |= i;\n"
        "  }\n"
        "  return value;\n"
        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* main = find_item(module, "main");
    ASSERT_NE(main, nullptr);
    ASSERT_TRUE(syntax::is_valid(main->body));
    const syntax::StmtNode& body = module.stmts[main->body.value];
    ASSERT_GE(body.statements.size(), 6U);

    const syntax::StmtNode& add_assign = module.stmts[body.statements[1].value];
    ASSERT_EQ(add_assign.kind, syntax::StmtKind::assign);
    EXPECT_EQ(add_assign.assign_op, syntax::AssignOp::add);

    const syntax::StmtNode& shl_assign = module.stmts[body.statements[2].value];
    ASSERT_EQ(shl_assign.kind, syntax::StmtKind::assign);
    EXPECT_EQ(shl_assign.assign_op, syntax::AssignOp::shl);

    const syntax::StmtNode& decrement = module.stmts[body.statements[3].value];
    ASSERT_EQ(decrement.kind, syntax::StmtKind::assign);
    EXPECT_EQ(decrement.assign_op, syntax::AssignOp::sub);

    const syntax::StmtNode& loop = module.stmts[body.statements[4].value];
    ASSERT_EQ(loop.kind, syntax::StmtKind::for_);
    ASSERT_TRUE(syntax::is_valid(loop.for_update));
    const syntax::StmtNode& update = module.stmts[loop.for_update.value];
    EXPECT_EQ(update.kind, syntax::StmtKind::assign);
    EXPECT_EQ(update.assign_op, syntax::AssignOp::add);
}

TEST(CoreUnit, ParserRejectsIncrementAndDecrementSyntax) {
    expect_parse_error(
        "module parser.increment_syntax;\n"
        "fn main() -> i32 {\n"
        "  var value: i32 = 0;\n"
        "  value++;\n"
        "  return value;\n"
        "}\n",
        "increment operator is not supported; use '+= 1'"
    );
    expect_parse_error(
        "module parser.increment_expr_syntax;\n"
        "fn main() -> i32 {\n"
        "  var value: i32 = 0;\n"
        "  let old: i32 = value++;\n"
        "  return old;\n"
        "}\n",
        "increment operator is not supported; use '+= 1'"
    );
    expect_parse_error(
        "module parser.decrement_syntax;\n"
        "fn main() -> i32 {\n"
        "  var value: i32 = 0;\n"
        "  value--;\n"
        "  return value;\n"
        "}\n",
        "decrement operator is not supported; use '-= 1'"
    );
    expect_parse_error(
        "module parser.prefix_increment_syntax;\n"
        "fn main() -> i32 {\n"
        "  var value: i32 = 0;\n"
        "  ++value;\n"
        "  return value;\n"
        "}\n",
        "increment operator is not supported; use '+= 1'"
    );
    expect_parse_error(
        "module parser.prefix_decrement_syntax;\n"
        "fn main() -> i32 {\n"
        "  var value: i32 = 0;\n"
        "  --value;\n"
        "  return value;\n"
        "}\n",
        "decrement operator is not supported; use '-= 1'"
    );
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
