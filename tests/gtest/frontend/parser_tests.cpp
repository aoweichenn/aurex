#include <aurex/base/diagnostic.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/parse/recovery.hpp>
#include <aurex/parse/parser_part_ranges.hpp>
#include <aurex/parse/parser.hpp>
#include <aurex/syntax/ast_dump.hpp>
#include <support/test_support.hpp>

#include <array>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <parse/parser_recovery_sets.hpp>

namespace aurex::test {
namespace {

using base::DiagnosticSink;

constexpr base::usize PARSER_TEST_DEEP_PREFIX_CHAIN_DEPTH = 8;

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

class ParserPartRangeReaderProbe final : public parse::ParserPartRangeReader {
public:
    explicit ParserPartRangeReaderProbe(parse::Parser& parser) noexcept
        : parse::ParserPartRangeReader(parser) {}

    [[nodiscard]] syntax::AstModule& module() noexcept {
        return this->session_.module;
    }

    using parse::ParserPartRangeReader::expr_range_or;
    using parse::ParserPartRangeReader::merge;
    using parse::ParserPartRangeReader::pattern_range_or;
    using parse::ParserPartRangeReader::stmt_range_or;
    using parse::ParserPartRangeReader::type_range_or;
};

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
        "enum Token { ident(str), span(usize, usize), eof }\n"
        "type UnsafeText = unsafe fn(*const u8, usize) -> str;\n"
        "unsafe fn unchecked_string(data: *const u8, len: usize) -> str {\n"
        "  return strraw(data, len);\n"
        "}\n"
        "fn unsafe_block_tail(ptr: *const i32) -> i32 {\n"
        "  return {\n"
        "    unsafe { *ptr }\n"
        "  };\n"
        "}\n"
        "fn unsafe_block_statement(ptr: *mut i32) -> i32 {\n"
        "  return {\n"
        "    unsafe { *ptr = 1; }\n"
        "    0\n"
        "  };\n"
        "}\n"
        "fn block_expr_statement(value: i32) -> i32 {\n"
        "  return {\n"
        "    value;\n"
        "    0\n"
        "  };\n"
        "}\n"
        "fn tail_else_if(first: bool, second: bool) -> i32 {\n"
        "  return {\n"
        "    if first { 1 } else if second { 2 } else { 3 }\n"
        "  };\n"
        "}\n"
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
        "  let p: *mut i32 = unsafe { ptrat[*mut i32](ptraddr(argv)) };\n"
        "  let n: *const u8 = null;\n"
        "  let s: str = \"hello\";\n"
        "  let size: usize = sizeof[*mut i32];\n"
        "  let data: *const u8 = strptr(s);\n"
        "  let len: usize = strblen(s);\n"
        "  let raw: str = unsafe { strraw(data, len) };\n"
        "  let raw_literal: str = r\"C:\\tmp\\a\";\n"
        "  let bytes: [3]u8 = b\"a\\n\\0\";\n"
        "  let b: u8 = b'\\n';\n"
        "  let ch: char = '\\u{03BB}';\n"
        "  let nums: [3]i32 = [1, 2, 3];\n"
        "  let reps: [2]u8 = [b'a'; 2];\n"
        "  let a: i32 = cast[i32](argc) + unsafe { bitcast[i32](argc) } + alignof[*mut i32];\n"
        "  let q: *mut i32 = unsafe { ptrcast[*mut i32](p) };\n"
        "  let make_text: UnsafeText = unchecked_string;\n"
        "  let from_fn_ptr: str = unsafe { make_text(data, len) };\n"
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
        "kw_unsafe",
        "kw_null",
        "kw_ptrcast",
        "kw_bitcast",
        "kw_alignof",
        "kw_ptraddr",
        "kw_ptrat",
        "ellipsis",
        "byte_literal",
        "byte_string_literal",
        "char_literal",
        "raw_string_literal",
        "string_literal",
    });

    const std::string ast = syntax::dump_ast(parsed.value());
    expect_contains_all(ast, {
        "pub import c.host",
        "opaque_struct Handle extern_c",
        "fn printf extern_c variadic @name=printf",
        "impl for Counter",
        "struct Owner",
        "enum Token",
        "case span(usize, usize)",
        "alias unsafe fn(*const u8, usize) -> str",
        "fn unchecked_string unsafe",
        "fn inc for Counter",
        "fn exported export_c @name=exported",
        "stmt #",
        "while",
        "for",
        "break",
        "continue",
        "defer",
        "expr #",
        "unsafe_block",
        "null_literal",
        "string_literal",
        "raw_string_literal",
        "byte_string_literal",
        "byte_literal",
        "char_literal",
        "array_literal",
        "array_repeat_value",
        "array_repeat_count",
        "index",
        "ptrcast",
        "bitcast",
        "alignof",
        "ptraddr",
        "ptrat",
    });
}

TEST(CoreUnit, ParserAcceptsSliceTypesAndExpressions) {
    constexpr std::string_view source =
        "module parser.slices;\n"
        "type ConstSlice = []const i32;\n"
        "type MutSlice = []mut i32;\n"
        "fn use(values: []const i32, mut_values: []mut i32) -> i32 {\n"
        "  let all = values[:];\n"
        "  let prefix = values[:2];\n"
        "  let suffix = values[1:];\n"
        "  let middle = mut_values[1:2];\n"
        "  return all[0] + prefix[0] + suffix[0] + middle[0];\n"
        "}\n";
    const syntax::AstModule module = parse_success(source);

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast, {
        "alias []const i32",
        "alias []mut i32",
        "slice",
        "slice_start",
        "slice_end",
        "index",
    });
}

TEST(CoreUnit, ParserAcceptsFunctionTypes) {
    constexpr std::string_view source =
        "module parser.function_types;\n"
        "type BinaryOp = fn(left: i32, right: i32) -> i32;\n"
        "type Callback = extern c fn(*mut void, ...) -> void;\n"
        "type UnsafeOp = unsafe fn(i32) -> i32;\n"
        "type UnsafeCallback = unsafe extern c fn(*mut void) -> void;\n"
        "struct Ops { run: fn(i32) -> i32; }\n"
        "fn apply(op: fn(i32, i32) -> i32, value: i32) -> i32 {\n"
        "  return op(value, value);\n"
        "}\n";
    const syntax::AstModule module = parse_success(source);

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast, {
        "alias fn(i32, i32) -> i32",
        "alias extern c fn(*mut void, ...) -> void",
        "alias unsafe fn(i32) -> i32",
        "alias unsafe extern c fn(*mut void) -> void",
        "field priv run : fn(i32) -> i32",
        "param op : fn(i32, i32) -> i32",
    });
}

TEST(CoreUnit, ParserRejectsBareSliceType) {
    expect_parse_error(
        "module parser.bad_slice_type;\n"
        "type Bad = []i32;\n",
        "expected 'mut' or 'const' after '[]'"
    );
}

TEST(CoreUnit, ParserRejectsUnsafeWithoutBlock) {
    expect_parse_error(
        "module parser.bad_unsafe_block;\n"
        "fn value() -> i32 {\n"
        "  return {\n"
        "    unsafe 1\n"
        "  };\n"
        "}\n",
        "expected block after 'unsafe'"
    );
}

TEST(CoreUnit, ParserCoversRecoveryNumericEnumValuesAndShiftLookahead) {
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
            "type UpperHexBytes = [0X2A]u8;\n"
            "type LowerHexBytes = [0x2a]u8;\n"
            "type BinBytes = [0b1010]u8;\n"
            "type UpperBinBytes = [0B1010]u8;\n"
            "type DecBytes = [1_000]u8;\n"
            "enum Code: u16 { hex = 0x2A, bin = 0b1010, dec = 1_000, }\n"
            "struct Wrap { value: i32; }\n"
            "struct Outer { value: Wrap; }\n"
            "enum ResultI32Bool: u8 { ok(i32) = 1, err(bool) = 2, }\n"
            "fn main() -> i32 {\n"
            "  let nested: Outer = Outer { value: Wrap { value: 1 } };\n"
            "  let ok = ResultI32Bool.err(false)?;\n"
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
            "alias [42]u8",
            "alias [10]u8",
            "alias [10]u8",
            "alias [1000]u8",
            "struct_literal Outer",
            "struct_literal Wrap",
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
        "struct Pair { left: i32; right: bool; }\n"
        "enum Choice: u8 { ok = 1, err(bool) = 2 }\n"
        "fn choose(first: i32, second: bool,) -> Choice {\n"
        "  let pair = Pair { left: first, right: second, };\n"
        "  if pair.left == first { return Choice.ok; }\n"
        "  return Choice.err(pair.right);\n"
        "}\n"
        "fn score() -> i32 {\n"
        "  let value = choose(41, false,);\n"
        "  return match value {\n"
        "    .ok => 41,\n"
        "    .err(flag) => if flag { 1 } else { 0 }\n"
        "  };\n"
        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* pair = find_item(module, "Pair");
    ASSERT_NE(pair, nullptr);
    EXPECT_EQ(pair->fields.size(), 2U);

    const syntax::ItemNode* choice = find_item(module, "Choice");
    ASSERT_NE(choice, nullptr);
    EXPECT_EQ(choice->enum_cases.size(), 2U);

    const syntax::ItemNode* choose = find_item(module, "choose");
    ASSERT_NE(choose, nullptr);
    EXPECT_EQ(choose->params.size(), 2U);
}

TEST(CoreUnit, ParserAcceptsTupleTypesLiteralsFieldsAndDestructuring) {
    constexpr std::string_view source =
        "module parser.tuples;\n"
        "type Pair = (i32, bool);\n"
        "type Single = (i32,);\n"
        "fn make_pair(value: i32) -> (i32, bool) {\n"
        "  let pair: Pair = (value, value > 0);\n"
        "  let (x, ok) = pair;\n"
        "  let (single,) = (x,);\n"
        "  return (single, ok);\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let pair = make_pair(1);\n"
        "  return pair.0;\n"
        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* pair_alias = find_item(module, "Pair");
    ASSERT_NE(pair_alias, nullptr);
    ASSERT_TRUE(syntax::is_valid(pair_alias->alias_type));
    const syntax::TypeNode& pair_type = module.types[pair_alias->alias_type.value];
    ASSERT_EQ(pair_type.kind, syntax::TypeKind::tuple);
    EXPECT_EQ(pair_type.tuple_elements.size(), 2U);

    const syntax::ItemNode* single_alias = find_item(module, "Single");
    ASSERT_NE(single_alias, nullptr);
    ASSERT_TRUE(syntax::is_valid(single_alias->alias_type));
    const syntax::TypeNode& single_type = module.types[single_alias->alias_type.value];
    ASSERT_EQ(single_type.kind, syntax::TypeKind::tuple);
    EXPECT_EQ(single_type.tuple_elements.size(), 1U);

    const syntax::ItemNode* make_pair = find_item(module, "make_pair");
    ASSERT_NE(make_pair, nullptr);
    ASSERT_TRUE(syntax::is_valid(make_pair->return_type));
    EXPECT_EQ(module.types[make_pair->return_type.value].kind, syntax::TypeKind::tuple);

    ASSERT_TRUE(syntax::is_valid(make_pair->body));
    const syntax::StmtNode& body = module.stmts[make_pair->body.value];
    ASSERT_EQ(body.kind, syntax::StmtKind::block);
    ASSERT_GE(body.statements.size(), 4U);

    const syntax::StmtNode& pair_stmt = module.stmts[body.statements[0].value];
    ASSERT_EQ(pair_stmt.kind, syntax::StmtKind::let);
    ASSERT_TRUE(syntax::is_valid(pair_stmt.init));
    const syntax::ExprNode& pair_init = module.exprs[pair_stmt.init.value];
    ASSERT_EQ(pair_init.kind, syntax::ExprKind::tuple_literal);
    EXPECT_EQ(pair_init.tuple_elements.size(), 2U);

    const syntax::StmtNode& destructure_stmt = module.stmts[body.statements[1].value];
    ASSERT_TRUE(syntax::is_valid(destructure_stmt.pattern));
    const syntax::PatternNode& destructure = module.patterns[destructure_stmt.pattern.value];
    ASSERT_EQ(destructure.kind, syntax::PatternKind::tuple);
    EXPECT_EQ(destructure.elements.size(), 2U);

    const syntax::StmtNode& single_stmt = module.stmts[body.statements[2].value];
    ASSERT_TRUE(syntax::is_valid(single_stmt.pattern));
    const syntax::PatternNode& single_pattern = module.patterns[single_stmt.pattern.value];
    ASSERT_EQ(single_pattern.kind, syntax::PatternKind::tuple);
    EXPECT_EQ(single_pattern.elements.size(), 1U);

    const syntax::ItemNode* main = find_item(module, "main");
    ASSERT_NE(main, nullptr);
    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast, {
        "alias (i32, bool)",
        "alias (i32,)",
        "tuple_literal",
        "tuple_element",
        "stmt #",
        "(x, ok)",
        "(single,)",
    });
}

TEST(CoreUnit, ParserRejectsEmptyTupleForms) {
    expect_parse_error(
        "module parser.empty_tuple_type;\n"
        "type Empty = ();\n",
        "empty tuple type is not part of M2 syntax"
    );
    expect_parse_error(
        "module parser.empty_tuple_literal;\n"
        "fn main() -> i32 {\n"
        "  let value = ();\n"
        "  return 0;\n"
        "}\n",
        "empty tuple literal is not part of M2 syntax"
    );
    expect_parse_error(
        "module parser.empty_tuple_pattern;\n"
        "fn main() -> i32 {\n"
        "  let () = 1;\n"
        "  return 0;\n"
        "}\n",
        "empty tuple pattern is not part of M2 syntax"
    );
}

TEST(CoreUnit, ParserAcceptsSlicePatternsAndLetElse) {
    constexpr std::string_view source =
        "module parser.slice_patterns;\n"
        "enum Maybe { some(i32), none }\n"
        "fn main() -> i32 {\n"
        "  let values: [3]i32 = [1, 2, 3];\n"
        "  let [..] = values;\n"
        "  let [.., last] = values;\n"
        "  let [[nested], ..] = values;\n"
        "  let [dot_case, .none] = values;\n"
        "  let [head, .., tail] = values;\n"
        "  let some(other) = Maybe.some(head) else {\n"
        "    return nested;\n"
        "  };\n"
        "  let .some(value) = Maybe.some(head) else {\n"
        "    return tail;\n"
        "  };\n"
        "  let .some(nested_case(inner)) = Maybe.some(head) else {\n"
        "    return 0;\n"
        "  };\n"
        "  let .some(Wrapper { _ }) = Maybe.some(head) else {\n"
        "    return 0;\n"
        "  };\n"
        "  let .some(choice) | .none | some(fallback) = Maybe.some(value) else {\n"
        "    return dot_case;\n"
        "  };\n"
        "  return value + other;\n"
        "}\n";
    const syntax::AstModule module = parse_success(source);
    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast, {
        "[..]",
        "[.., last]",
        "[[nested], ..]",
        "[dot_case, .none]",
        "[head, .., tail]",
        "some(other)",
        ".some(value)",
        ".some(nested_case(inner))",
        ".some(choice) | .none | some(fallback)",
    });
}

TEST(CoreUnit, ParserRejectsMalformedSlicePatternRest) {
    expect_parse_error(
        "module parser.slice_pattern_rest;\n"
        "fn main() -> i32 {\n"
        "  let [head, .., ..] = [1, 2];\n"
        "  return head;\n"
        "}\n",
        "slice pattern can contain at most one '..' rest marker"
    );
    expect_parse_error(
        "module parser.slice_pattern_ellipsis;\n"
        "fn main() -> i32 {\n"
        "  let [head, ...] = [1, 2];\n"
        "  return head;\n"
        "}\n",
        "slice pattern rest marker is '..'"
    );
    expect_parse_error(
        "module parser.slice_pattern_separator;\n"
        "fn main() -> i32 {\n"
        "  let [head tail] = [1, 2];\n"
        "  return head;\n"
        "}\n",
        "expected ',' or ']' after slice pattern element"
    );
    expect_parse_error(
        "module parser.slice_pattern_separator_recovery;\n"
        "fn main() -> i32 {\n"
        "  let [head tail, other] = [1, 2, 3];\n"
        "  return head;\n"
        "}\n",
        "expected ',' or ']' after slice pattern element"
    );
    expect_parse_error(
        "module parser.let_else_name;\n"
        "fn main() -> i32 {\n"
        "  let value = 1 else { return 0; };\n"
        "  return value;\n"
        "}\n",
        "let-else requires a destructuring or refutable pattern"
    );
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedTupleSeparators) {
    constexpr base::SourceId PARSER_TEST_TUPLE_RECOVERY_SOURCE_ID {33};
    constexpr std::string_view source =
        "module parser.tuple_recovery;\n"
        "type Bad = (i32, bool i32);\n"
        "fn recovered() -> i32 {\n"
        "  let bad_literal = (1, true false);\n"
        "  let (a, b extra) = (1, 2);\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_TUPLE_RECOVERY_SOURCE_ID, source, diagnostics);
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
    expect_contains(messages, "expected ',' or ')' after tuple type element");
    expect_contains(messages, "expected ',' or ')' after tuple element");
    expect_contains(messages, "expected ',' or ')' after tuple pattern element");
}

TEST(CoreUnit, ParserRecoveryCoversTupleSynchronizationAndFunctionTypeVariadics) {
    constexpr base::SourceId PARSER_TEST_TUPLE_SYNC_RECOVERY_SOURCE_ID {34};
    constexpr std::string_view source =
        "module parser.tuple_sync_recovery;\n"
        "type SizedA = [4usize]i32;\n"
        "type SizedB = [0b10u8]u8;\n"
        "type BadSize = [4bad]i32;\n"
        "type BadGeneric = Pair[i32 +, bool];\n"
        "type BadTuple = (i32, bool +, u8);\n"
        "type BadFnA = fn(..., i32) -> i32;\n"
        "type BadFnB = fn(i32, ..., bool) -> i32;\n"
        "type BadFnC = fn(i32 +, bool) -> i32;\n"
        "fn recovered(value: i32) -> i32 {\n"
        "  let (only) = value;\n"
        "  let missing_repeat = [0;];\n"
        "  let bad_array = [1 ->, 2];\n"
        "  let bad_literal = (1, true ->, 2);\n"
        "  let (a, b +, c) = (1, 2, 3);\n"
        "  let bad_apply = value::[i32 +, bool](1);\n"
        "  let bad_payload = match value { .some() => 0, .some(name,) => 1, _ => 2 };\n"
        "  return value;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_TUPLE_SYNC_RECOVERY_SOURCE_ID, source, diagnostics);
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
    expect_contains(messages, "array length literal is out of range");
    expect_contains(messages, "expected array repeat count");
    expect_contains(messages, "expected payload binding name");
    expect_contains(messages, "expected ',' or ']' after generic type argument");
    expect_contains(messages, "expected ',' or ')' after tuple type element");
    expect_contains(messages, "expected ',' or ')' after tuple element");
    expect_contains(messages, "expected ',' or ')' after tuple pattern element");
    expect_contains(messages, "expected ',' or ')' after function type parameter");
    expect_contains(messages, "variadic marker must be last in parameter list");
}

TEST(CoreUnit, ParserDoesNotTreatSuffixedFloatAsTupleField) {
    expect_parse_error(
        "module parser.float_tuple_field_boundary;\n"
        "fn main() -> i32 {\n"
        "  let pair = (1, false);\n"
        "  return pair.0f32;\n"
        "}\n",
        "expected ';' after return"
    );
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedArrayTypeSeparators) {
    constexpr base::SourceId PARSER_TEST_ARRAY_TYPE_RECOVERY_SOURCE_ID {9};
    constexpr std::string_view source =
        "module parser.array_type_recovery;\n"
        "type Broken = [2 i32;\n"
        "fn recovered() -> i32 {\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_ARRAY_TYPE_RECOVERY_SOURCE_ID, source, diagnostics);
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedArrayLiteralSeparators) {
    constexpr base::SourceId PARSER_TEST_ARRAY_LITERAL_RECOVERY_SOURCE_ID {32};
    constexpr std::string_view source =
        "module parser.array_literal_recovery;\n"
        "fn recovered() -> i32 {\n"
        "  let value: [3]i32 = [1 @ 2, 3];\n"
        "  let broken = ;\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_ARRAY_LITERAL_RECOVERY_SOURCE_ID, source, diagnostics);
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
    expect_contains(messages, "expected ',' or ']' after array element");
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedStructFieldSeparators) {
    constexpr base::SourceId PARSER_TEST_STRUCT_FIELD_RECOVERY_SOURCE_ID {16};
    constexpr std::string_view source =
        "module parser.struct_field_recovery;\n"
        "struct Box { value: i32 @ other: bool; }\n"
        "fn recovered() -> i32 {\n"
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
    expect_contains(messages, "expected ';' or '}' after field declaration");
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
        "  let value = cast[i32 @](argc);\n"
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
    expect_contains(messages, "expected ']' after cast type");
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
        "  let builtin = ptraddr(values @);\n"
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
    expect_contains(messages, "expected ')' after ptraddr argument");
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
    expect_contains(messages, "expected ':' after parameter name");
    expect_contains(messages, "expected initializer");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedGenericSeparators) {
    expect_parse_error(
        "module parser.generic_bound_recovery;\n"
        "struct Box[T: Copy] { value: T; }\n",
        "generic bounds are not part of M2 syntax"
    );
    expect_parse_error(
        "module parser.generic_param_recovery;\n"
        "fn id[T U](value: T) -> T { return value; }\n",
        "expected ',' or ']' after generic parameter"
    );
    expect_parse_error(
        "module parser.generic_type_arg_recovery;\n"
        "type Bad = Pair[i32 bool];\n",
        "expected ',' or ']' after generic type argument"
    );
    expect_parse_error(
        "module parser.generic_expr_arg_recovery;\n"
        "fn main() -> i32 { return id::[i32 bool](1); }\n",
        "expected ',' or ']' after generic type argument"
    );
    expect_parse_error(
        "module parser.generic_struct_literal_arg_recovery;\n"
        "fn main() -> i32 { let value = Pair[i32 bool] { first: 1, second: true }; return value.first; }\n",
        "expected ',' or ']' after generic type argument"
    );
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
    expect_contains(messages, "if expression requires an else branch");
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
        "  let casted = cast @[i32](value);\n"
        "  let broken_builtin = ptrat @[*mut i32](casted);\n"
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
    expect_contains(messages, "expected '[' after cast builtin");
    expect_contains(messages, "expected '[' after ptrat");
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

TEST(CoreUnit, ParserCoversShiftAndScopedEnumRegressions) {
    constexpr std::string_view source =
        "module parser.shift_enum;\n"
        "struct Wrap { value: i32; }\n"
        "enum ResultI32: u8 { ok(i32) = 1, err(i32) = 2, }\n"
        "fn main() -> i32 {\n"
        "  let call = ResultI32.ok(1)?;\n"
        "  let literal: Wrap = Wrap { value: 2 };\n"
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
    EXPECT_EQ(result_name.text, "ResultI32");

    const syntax::StmtNode& literal_stmt = module.stmts[body.statements[1].value];
    ASSERT_TRUE(syntax::is_valid(literal_stmt.init));
    const syntax::ExprNode& literal = module.exprs[literal_stmt.init.value];
    ASSERT_EQ(literal.kind, syntax::ExprKind::struct_literal);
    EXPECT_EQ(literal.struct_name, "Wrap");

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

TEST(CoreUnit, ParserHandlesLongOperatorAndTypePrefixChainsIteratively) {
    constexpr std::string_view source =
        "module parser.deep_prefix_chains;\n"
        "type DeepPtr = *mut *const *mut *const *mut *const *mut *const i32;\n"
        "type DeepArray = [1][2][3][4][5][6][7][8]u8;\n"
        "fn main() -> i32 {\n"
        "  let unary: i32 = - - - - - - - -1;\n"
        "  let binary: i32 = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9;\n"
        "  return unary + binary;\n"
        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* deep_ptr = find_item(module, "DeepPtr");
    ASSERT_NE(deep_ptr, nullptr);
    ASSERT_TRUE(syntax::is_valid(deep_ptr->alias_type));
    syntax::TypeId type = deep_ptr->alias_type;
    for (base::usize depth = 0; depth < PARSER_TEST_DEEP_PREFIX_CHAIN_DEPTH; ++depth) {
        ASSERT_TRUE(syntax::is_valid(type));
        const syntax::TypeNode& node = module.types[type.value];
        ASSERT_EQ(node.kind, syntax::TypeKind::pointer);
        type = node.pointee;
    }
    ASSERT_TRUE(syntax::is_valid(type));
    EXPECT_EQ(module.types[type.value].kind, syntax::TypeKind::primitive);
    EXPECT_EQ(module.types[type.value].primitive, syntax::PrimitiveTypeKind::i32);

    const syntax::ItemNode* main = find_item(module, "main");
    ASSERT_NE(main, nullptr);
    ASSERT_TRUE(syntax::is_valid(main->body));
    const syntax::StmtNode& body = module.stmts[main->body.value];
    ASSERT_GE(body.statements.size(), 2U);

    const syntax::StmtNode& unary_stmt = module.stmts[body.statements[0].value];
    ASSERT_TRUE(syntax::is_valid(unary_stmt.init));
    syntax::ExprId unary = unary_stmt.init;
    for (base::usize depth = 0; depth < PARSER_TEST_DEEP_PREFIX_CHAIN_DEPTH; ++depth) {
        ASSERT_TRUE(syntax::is_valid(unary));
        const syntax::ExprNode& node = module.exprs[unary.value];
        ASSERT_EQ(node.kind, syntax::ExprKind::unary);
        EXPECT_EQ(node.unary_op, syntax::UnaryOp::numeric_negate);
        unary = node.unary_operand;
    }
    ASSERT_TRUE(syntax::is_valid(unary));
    EXPECT_EQ(module.exprs[unary.value].kind, syntax::ExprKind::integer_literal);

    const syntax::StmtNode& binary_stmt = module.stmts[body.statements[1].value];
    ASSERT_TRUE(syntax::is_valid(binary_stmt.init));
    syntax::ExprId binary = binary_stmt.init;
    for (base::usize depth = 0; depth < PARSER_TEST_DEEP_PREFIX_CHAIN_DEPTH; ++depth) {
        ASSERT_TRUE(syntax::is_valid(binary));
        const syntax::ExprNode& node = module.exprs[binary.value];
        ASSERT_EQ(node.kind, syntax::ExprKind::binary);
        EXPECT_EQ(node.binary_op, syntax::BinaryOp::add);
        binary = node.binary_lhs;
    }
    ASSERT_TRUE(syntax::is_valid(binary));
    EXPECT_EQ(module.exprs[binary.value].kind, syntax::ExprKind::integer_literal);
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

TEST(CoreUnit, ParserParsesForRangeStatements) {
    constexpr std::string_view source =
        "module parser.for_range;\n"
        "fn main(limit: i32) -> i32 {\n"
        "  var total: i32 = 0;\n"
        "  for i in range(limit) {\n"
        "    total += i;\n"
        "  }\n"
        "  for j in range(1, limit) {\n"
        "    total += j;\n"
        "  }\n"
        "  for k in range(1, limit, 2) {\n"
        "    total += k;\n"
        "  }\n"
        "  return total;\n"
        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* main = find_item(module, "main");
    ASSERT_NE(main, nullptr);
    ASSERT_TRUE(syntax::is_valid(main->body));
    const syntax::StmtNode& body = module.stmts[main->body.value];
    ASSERT_GE(body.statements.size(), 4U);

    const syntax::StmtNode& end_loop = module.stmts[body.statements[1].value];
    ASSERT_EQ(end_loop.kind, syntax::StmtKind::for_range);
    EXPECT_EQ(end_loop.name, "i");
    EXPECT_FALSE(syntax::is_valid(end_loop.range_start));
    EXPECT_TRUE(syntax::is_valid(end_loop.range_end));
    EXPECT_FALSE(syntax::is_valid(end_loop.range_step));
    EXPECT_TRUE(syntax::is_valid(end_loop.body));

    const syntax::StmtNode& start_end_loop = module.stmts[body.statements[2].value];
    ASSERT_EQ(start_end_loop.kind, syntax::StmtKind::for_range);
    EXPECT_EQ(start_end_loop.name, "j");
    EXPECT_TRUE(syntax::is_valid(start_end_loop.range_start));
    EXPECT_TRUE(syntax::is_valid(start_end_loop.range_end));
    EXPECT_FALSE(syntax::is_valid(start_end_loop.range_step));
    EXPECT_TRUE(syntax::is_valid(start_end_loop.body));

    const syntax::StmtNode& stepped_loop = module.stmts[body.statements[3].value];
    ASSERT_EQ(stepped_loop.kind, syntax::StmtKind::for_range);
    EXPECT_EQ(stepped_loop.name, "k");
    EXPECT_TRUE(syntax::is_valid(stepped_loop.range_start));
    EXPECT_TRUE(syntax::is_valid(stepped_loop.range_end));
    EXPECT_TRUE(syntax::is_valid(stepped_loop.range_step));
    EXPECT_TRUE(syntax::is_valid(stepped_loop.body));
}

TEST(CoreUnit, ParserReportsMalformedForRangeSyntax) {
    expect_parse_error(
        "module parser.for_range_separator;\n"
        "fn main() -> i32 {\n"
        "  for i in range(0 3) {\n"
        "  }\n"
        "  return 0;\n"
        "}\n",
        "expected ',' or ')' after range argument"
    );
    expect_parse_error(
        "module parser.for_range_missing_args;\n"
        "fn main() -> i32 {\n"
        "  for i in range {\n"
        "  }\n"
        "  return 0;\n"
        "}\n",
        "expected '(' after range"
    );
    expect_parse_error(
        "module parser.for_range_too_many_args;\n"
        "fn main() -> i32 {\n"
        "  for i in range(0, 3, 1, 1) {\n"
        "  }\n"
        "  return 0;\n"
        "}\n",
        "range expects 1 to 3 arguments"
    );
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

TEST(CoreUnit, ParserRecoveryPredicateTablesCoverStartAndBoundarySets) {
    using syntax::TokenKind;

    const auto expect_true_all = [](const auto predicate, const std::initializer_list<TokenKind> kinds) {
        for (const TokenKind kind : kinds) {
            EXPECT_TRUE(predicate(kind)) << static_cast<int>(kind);
        }
    };
    const auto expect_false_on = [](const auto predicate, const TokenKind kind) {
        EXPECT_FALSE(predicate(kind)) << static_cast<int>(kind);
    };

    expect_true_all(
        parse::detail::token_starts_item,
        {
            TokenKind::r_brace,
            TokenKind::kw_fn,
            TokenKind::kw_struct,
            TokenKind::kw_enum,
            TokenKind::kw_impl,
            TokenKind::kw_opaque,
            TokenKind::kw_const,
            TokenKind::kw_type,
            TokenKind::kw_pub,
            TokenKind::kw_priv,
            TokenKind::kw_extern,
            TokenKind::kw_export,
            TokenKind::kw_import,
            TokenKind::kw_unsafe,
        }
    );
    expect_false_on(parse::detail::token_starts_item, TokenKind::identifier);

    expect_true_all(
        parse::detail::token_starts_expression,
        {
            TokenKind::identifier,
            TokenKind::integer_literal,
            TokenKind::float_literal,
            TokenKind::string_literal,
            TokenKind::c_string_literal,
            TokenKind::byte_literal,
            TokenKind::kw_if,
            TokenKind::kw_match,
            TokenKind::kw_true,
            TokenKind::kw_false,
            TokenKind::kw_null,
            TokenKind::kw_cast,
            TokenKind::kw_ptrcast,
            TokenKind::kw_bitcast,
            TokenKind::kw_sizeof,
            TokenKind::kw_alignof,
            TokenKind::kw_ptraddr,
            TokenKind::kw_ptrat,
            TokenKind::kw_strptr,
            TokenKind::kw_strblen,
            TokenKind::kw_strvalid,
            TokenKind::kw_strfromutf8,
            TokenKind::kw_strraw,
            TokenKind::kw_unsafe,
            TokenKind::l_paren,
            TokenKind::l_brace,
            TokenKind::minus,
            TokenKind::star,
            TokenKind::amp,
            TokenKind::tilde,
            TokenKind::bang,
        }
    );
    expect_false_on(parse::detail::token_starts_expression, TokenKind::kw_module);

    EXPECT_TRUE(parse::detail::token_starts_statement(TokenKind::identifier));
    expect_true_all(
        parse::detail::token_starts_statement,
        {
            TokenKind::kw_let,
            TokenKind::kw_var,
            TokenKind::kw_for,
            TokenKind::kw_while,
            TokenKind::kw_break,
            TokenKind::kw_continue,
            TokenKind::kw_defer,
            TokenKind::kw_return,
            TokenKind::kw_unsafe,
        }
    );
    expect_false_on(parse::detail::token_starts_statement, TokenKind::semicolon);

    expect_true_all(
        parse::detail::token_starts_non_expression_statement,
        {
            TokenKind::kw_let,
            TokenKind::kw_var,
            TokenKind::kw_if,
            TokenKind::kw_for,
            TokenKind::kw_while,
            TokenKind::kw_break,
            TokenKind::kw_continue,
            TokenKind::kw_defer,
            TokenKind::kw_return,
            TokenKind::kw_unsafe,
        }
    );
    expect_false_on(parse::detail::token_starts_non_expression_statement, TokenKind::identifier);

    expect_true_all(
        parse::detail::token_starts_type,
        {
            TokenKind::identifier,
            TokenKind::star,
            TokenKind::l_bracket,
            TokenKind::kw_fn,
            TokenKind::kw_extern,
            TokenKind::kw_unsafe,
            TokenKind::kw_void,
            TokenKind::kw_bool,
            TokenKind::kw_i8,
            TokenKind::kw_u8,
            TokenKind::kw_i16,
            TokenKind::kw_u16,
            TokenKind::kw_i32,
            TokenKind::kw_u32,
            TokenKind::kw_i64,
            TokenKind::kw_u64,
            TokenKind::kw_isize,
            TokenKind::kw_usize,
            TokenKind::kw_f32,
            TokenKind::kw_f64,
            TokenKind::kw_str,
        }
    );
    expect_false_on(parse::detail::token_starts_type, TokenKind::kw_module);

    expect_true_all(
        parse::token_starts_match_arm,
        {
            TokenKind::identifier,
            TokenKind::integer_literal,
            TokenKind::kw_true,
            TokenKind::kw_false,
            TokenKind::dot,
            TokenKind::l_paren,
            TokenKind::l_bracket,
        }
    );
    expect_false_on(parse::token_starts_match_arm, TokenKind::kw_let);

    expect_true_all(parse::token_starts_struct_field, {TokenKind::identifier});
    expect_false_on(parse::token_starts_struct_field, TokenKind::kw_fn);

    expect_true_all(parse::token_starts_parameter, {TokenKind::identifier, TokenKind::ellipsis});
    expect_false_on(parse::token_starts_parameter, TokenKind::kw_fn);

    expect_true_all(
        parse::token_starts_struct_decl_field,
        {TokenKind::identifier, TokenKind::kw_pub, TokenKind::kw_priv}
    );
    expect_false_on(parse::token_starts_struct_decl_field, TokenKind::kw_fn);

    expect_true_all(parse::token_starts_enum_case, {TokenKind::identifier});
    expect_false_on(parse::token_starts_enum_case, TokenKind::kw_fn);

    expect_true_all(
        parse::token_starts_path_segment,
        {TokenKind::identifier, TokenKind::kw_c, TokenKind::kw_str}
    );
    expect_false_on(parse::token_starts_path_segment, TokenKind::kw_fn);

    expect_true_all(parse::detail::token_ends_match_arm, {TokenKind::comma, TokenKind::r_brace});
    expect_false_on(parse::detail::token_ends_match_arm, TokenKind::identifier);

    expect_true_all(
        parse::detail::token_ends_call_argument,
        {
            TokenKind::comma,
            TokenKind::r_paren,
            TokenKind::semicolon,
            TokenKind::r_bracket,
            TokenKind::r_brace,
        }
    );
    expect_false_on(parse::detail::token_ends_call_argument, TokenKind::identifier);

    expect_true_all(
        parse::detail::token_ends_struct_field,
        {
            TokenKind::comma,
            TokenKind::r_brace,
            TokenKind::semicolon,
            TokenKind::r_paren,
            TokenKind::r_bracket,
        }
    );
    expect_false_on(parse::detail::token_ends_struct_field, TokenKind::identifier);

    expect_true_all(
        parse::detail::token_ends_parameter,
        {
            TokenKind::comma,
            TokenKind::r_paren,
            TokenKind::arrow,
            TokenKind::l_brace,
            TokenKind::semicolon,
            TokenKind::r_brace,
        }
    );
    expect_false_on(parse::detail::token_ends_parameter, TokenKind::identifier);

    expect_true_all(
        parse::detail::token_ends_struct_decl_field,
        {
            TokenKind::semicolon,
            TokenKind::comma,
            TokenKind::r_brace,
            TokenKind::kw_fn,
            TokenKind::kw_struct,
            TokenKind::kw_enum,
            TokenKind::kw_impl,
            TokenKind::kw_extern,
            TokenKind::kw_export,
        }
    );
    expect_false_on(parse::detail::token_ends_struct_decl_field, TokenKind::identifier);

    expect_true_all(
        parse::detail::token_ends_enum_case,
        {
            TokenKind::comma,
            TokenKind::semicolon,
            TokenKind::r_brace,
            TokenKind::kw_fn,
            TokenKind::kw_struct,
            TokenKind::kw_enum,
            TokenKind::kw_impl,
            TokenKind::kw_extern,
            TokenKind::kw_export,
        }
    );
    expect_false_on(parse::detail::token_ends_enum_case, TokenKind::identifier);

    expect_true_all(
        parse::detail::token_ends_builtin_argument,
        {
            TokenKind::comma,
            TokenKind::r_paren,
            TokenKind::semicolon,
            TokenKind::r_bracket,
            TokenKind::r_brace,
        }
    );
    expect_false_on(parse::detail::token_ends_builtin_argument, TokenKind::identifier);

    expect_true_all(
        parse::detail::token_ends_generic_type_argument,
        {
            TokenKind::comma,
            TokenKind::r_bracket,
            TokenKind::r_paren,
            TokenKind::l_brace,
            TokenKind::r_brace,
            TokenKind::semicolon,
        }
    );
    expect_false_on(parse::detail::token_ends_generic_type_argument, TokenKind::identifier);

    expect_true_all(
        parse::detail::token_ends_generic_parameter,
        {
            TokenKind::comma,
            TokenKind::r_bracket,
            TokenKind::l_paren,
            TokenKind::l_brace,
            TokenKind::r_brace,
            TokenKind::semicolon,
        }
    );
    expect_false_on(parse::detail::token_ends_generic_parameter, TokenKind::identifier);
}

TEST(CoreUnit, ParserPartRangeReaderCoversRangeFallbacks) {
    constexpr std::string_view source =
        "module parser.ranges;\n"
        "fn main() -> i32 { return 0; }\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer({18}, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;
    constexpr base::usize PARSER_RANGE_TEST_MIN_TOKEN_COUNT = 2;
    constexpr base::u32 PARSER_RANGE_TEST_OUT_OF_RANGE_OFFSET = 1;
    ASSERT_GT(tokens.value().size(), PARSER_RANGE_TEST_MIN_TOKEN_COUNT);

    parse::Parser parser(tokens.value(), diagnostics);
    ParserPartRangeReaderProbe reader(parser);

    const base::SourceId source_id = tokens.value().front().range.source;
    const base::SourceRange begin_range = tokens.value().front().range;
    const base::SourceRange end_range = tokens.value()[PARSER_RANGE_TEST_OUT_OF_RANGE_OFFSET].range;
    const base::SourceRange fallback_range = tokens.value().back().range;

    const syntax::ExprId expr_id = reader.module().push_expr(
        [&] {
            syntax::ExprNode expr;
            expr.kind = syntax::ExprKind::integer_literal;
            expr.range = begin_range;
            return expr;
        }()
    );
    const syntax::StmtId stmt_id = reader.module().push_stmt(
        [&] {
            syntax::StmtNode stmt;
            stmt.kind = syntax::StmtKind::expr;
            stmt.range = end_range;
            return stmt;
        }()
    );
    const syntax::TypeId type_id = reader.module().push_type(
        [&] {
            syntax::TypeNode type;
            type.kind = syntax::TypeKind::primitive;
            type.range = begin_range;
            type.primitive = syntax::PrimitiveTypeKind::i32;
            return type;
        }()
    );
    const syntax::PatternId pattern_id = reader.module().push_pattern(
        [&] {
            syntax::PatternNode pattern;
            pattern.kind = syntax::PatternKind::wildcard;
            pattern.range = end_range;
            return pattern;
        }()
    );

    const base::SourceRange merged = reader.merge(begin_range, end_range);
    EXPECT_EQ(merged.source.value, source_id.value);
    EXPECT_EQ(merged.begin, begin_range.begin);
    EXPECT_EQ(merged.end, end_range.end);

    const auto expect_range = [](const base::SourceRange actual, const base::SourceRange expected) {
        EXPECT_EQ(actual.source.value, expected.source.value);
        EXPECT_EQ(actual.begin, expected.begin);
        EXPECT_EQ(actual.end, expected.end);
    };

    expect_range(reader.expr_range_or(expr_id, fallback_range), begin_range);
    expect_range(reader.expr_range_or(syntax::INVALID_EXPR_ID, fallback_range), fallback_range);
    expect_range(reader.expr_range_or(syntax::ExprId {expr_id.value + PARSER_RANGE_TEST_OUT_OF_RANGE_OFFSET}, fallback_range), fallback_range);

    expect_range(reader.stmt_range_or(stmt_id, fallback_range), end_range);
    expect_range(reader.stmt_range_or(syntax::INVALID_STMT_ID, fallback_range), fallback_range);
    expect_range(reader.stmt_range_or(syntax::StmtId {stmt_id.value + PARSER_RANGE_TEST_OUT_OF_RANGE_OFFSET}, fallback_range), fallback_range);

    expect_range(reader.type_range_or(type_id, fallback_range), begin_range);
    expect_range(reader.type_range_or(syntax::INVALID_TYPE_ID, fallback_range), fallback_range);
    expect_range(reader.type_range_or(syntax::TypeId {type_id.value + PARSER_RANGE_TEST_OUT_OF_RANGE_OFFSET}, fallback_range), fallback_range);

    expect_range(reader.pattern_range_or(pattern_id, fallback_range), end_range);
    expect_range(reader.pattern_range_or(syntax::INVALID_PATTERN_ID, fallback_range), fallback_range);
    expect_range(reader.pattern_range_or(syntax::PatternId {pattern_id.value + PARSER_RANGE_TEST_OUT_OF_RANGE_OFFSET}, fallback_range), fallback_range);
}

TEST(CoreUnit, ParserRecoversBuiltinArgumentSeparators) {
    constexpr std::string_view source =
        "module parser.builtin_recovery;\n"
        "fn main() -> i32 {\n"
        "  let text: str = \"hello\";\n"
        "  let data: *const u8 = strptr(text);\n"
        "  let len: usize = strblen(text);\n"
        "  let broken_cast: i32 = cast[i32](1 @);\n"
        "  let broken_str: str = strraw(data len);\n"
        "  return 0;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer({19}, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages.push_back('\n');
    }
    expect_contains(messages, "expected ')' after cast expression");
    expect_contains(messages, "expected ',' after strraw data");
}

TEST(CoreUnit, ParserM2GenericSyntax) {
    constexpr std::string_view source =
        "module parser.generics;\n"
        "type Alias[T] = T;\n"
        "struct Box[T] { value: T; }\n"
        "struct Pair[A, B] { first: A; second: B; }\n"
        "enum Maybe[T]: u8 { some(T) = 1, none = 2, }\n"
        "fn id[T](x: T) -> T { return x; }\n"
        "fn main() -> i32 {\n"
        "  let a: Box[i32] = Box[i32] { value: id::[i32](1) };\n"
        "  let p: Pair[i32, bool] = Pair[i32, bool] { first: a.value, second: true };\n"
        "  let i: i32 = 0;\n"
        "  let f = id[i](1);\n"
        "  return p.first;\n"
        "}\n";

    const syntax::AstModule module = parse_success(source);
    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast, {
        "type_alias Alias[T]",
        "alias T",
        "struct Box[T]",
        "field priv value : T",
        "struct Pair[A, B]",
        "enum Maybe[T]",
        "case some(T) = 1",
        "fn id[T]",
        "param x : T",
        "return T",
        "Box[i32]",
        "Pair[i32, bool]",
        "generic_apply[i32]",
        "index",
    });
}

TEST(CoreUnit, ParserRejectsEmptyGenericLists) {
    expect_parse_error(
        "module parser.empty_generic_fn;\n"
        "fn f[]() -> i32 { return 0; }\n",
        "expected generic type parameter"
    );
    expect_parse_error(
        "module parser.empty_generic_struct;\n"
        "struct Box[] { value: i32; }\n",
        "expected generic type parameter"
    );
    expect_parse_error(
        "module parser.empty_type_args;\n"
        "struct Box[T] { value: T; }\n"
        "fn main() -> i32 { let value: Box[] = Box[i32] { value: 1 }; return value.value; }\n",
        "expected generic type argument"
    );
    expect_parse_error(
        "module parser.empty_generic_call;\n"
        "fn id[T](value: T) -> T { return value; }\n"
        "fn main() -> i32 { return id::[](1); }\n",
        "expected generic type argument"
    );
    expect_parse_error(
        "module parser.empty_struct_literal_args;\n"
        "struct Box[T] { value: T; }\n"
        "fn main() -> i32 { let value = Box[] { value: 1 }; return value.value; }\n",
        "expected generic type argument"
    );
}

TEST(CoreUnit, ParserRejectsLegacyAngleGenericSyntax) {
    expect_parse_error(
        "module parser.legacy_angle_generic_params;\n"
        "fn id<T>(x: T) -> T { return x; }\n",
        "Aurex generics use '[' and ']'; '<' and '>' are not generic delimiters"
    );
    expect_parse_error(
        "module parser.legacy_angle_type_args;\n"
        "struct Pair[A, B] { first: A; second: B; }\n"
        "type Bad = Pair<i32, bool>;\n",
        "Aurex generics use '[' and ']'; '<' and '>' are not generic delimiters"
    );
}

TEST(CoreUnit, ParserRejectsWhereClausesWithM2Message) {
    expect_parse_error(
        "module parser.where_clause;\n"
        "fn id[T](value: T) -> T where T: Copy { return value; }\n",
        "where clauses are not part of M2 syntax"
    );
}

} // namespace aurex::test
