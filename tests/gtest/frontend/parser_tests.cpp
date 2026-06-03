#include <aurex/base/diagnostic.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/parse/parser.hpp>
#include <aurex/parse/parser_item_part.hpp>
#include <aurex/parse/parser_part_ranges.hpp>
#include <aurex/parse/recovery.hpp>
#include <aurex/parse/token_cursor.hpp>
#include <aurex/syntax/ast_dump.hpp>

#include <parse/bracket_suffix_classifier.hpp>
#include <support/frontend_test_support.hpp>

#include <array>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include <aurex/parse/parser_postfix_expr_part.hpp>
#undef private
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <parse/parser_recovery_sets.hpp>

namespace aurex::test {
namespace {

using base::DiagnosticCategory;
using base::DiagnosticCode;
using base::DiagnosticSink;

constexpr base::usize PARSER_TEST_DEEP_PREFIX_CHAIN_DEPTH = 8;
constexpr base::usize PARSER_TEST_DEEP_TYPE_SELECTOR_OVERFLOW_DEPTH = 10;
constexpr base::usize PARSER_TEST_LONG_BINARY_TERM_COUNT = 3'000;
constexpr base::usize PARSER_TEST_EXPRESSION_NESTING_LIMIT_DEPTH = 600;
constexpr base::usize PARSER_TEST_TYPE_NESTING_LIMIT_DEPTH = 600;
constexpr base::usize PARSER_TEST_PATTERN_NESTING_LIMIT_DEPTH = 600;
constexpr base::SourceId PARSER_TEST_PROBE_SOURCE_ID{99};

[[nodiscard]] bool diagnostics_contain(const DiagnosticSink& diagnostics, const std::string_view message)
{
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        if (diagnostic.message.find(message) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void expect_parse_error(const std::string_view source, const std::string_view message)
{
    DiagnosticSink diagnostics;
    lex::Lexer lexer({6}, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());
    EXPECT_TRUE(diagnostics_contain(diagnostics, message)) << "missing diagnostic: " << message;
}

void expect_parse_diagnostic(const std::string_view source, const std::string_view message)
{
    DiagnosticSink diagnostics;
    lex::Lexer lexer({6}, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    static_cast<void>(parser.parse_module());
    ASSERT_TRUE(diagnostics.has_error());
    EXPECT_TRUE(diagnostics_contain(diagnostics, message)) << "missing diagnostic: " << message;
}

[[nodiscard]] syntax::AstModule parse_success(const std::string_view source)
{
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
    return parsed.take_value();
}

[[nodiscard]] const syntax::ItemNode* find_item(const syntax::AstModule& module, const std::string_view name) noexcept
{
    for (base::usize i = 0; i < module.items.size(); ++i) {
        const syntax::ItemNode* item = module.items.ptr(i);
        if (item != nullptr && item->name == name) {
            return item;
        }
    }
    return nullptr;
}

class ParserPartRangeReaderProbe final : public parse::ParserPartRangeReader {
public:
    explicit ParserPartRangeReaderProbe(parse::Parser& parser) noexcept : parse::ParserPartRangeReader(parser)
    {
    }

    [[nodiscard]] syntax::AstModule& module() const noexcept
    {
        return this->session_.module;
    }

    const syntax::Token& advance_token()
    {
        return this->advance();
    }

    using parse::ParserPartRangeReader::expr_range_or;
    using parse::ParserPartRangeReader::merge;
    using parse::ParserPartRangeReader::pattern_range_or;
    using parse::ParserPartRangeReader::stmt_range_or;
    using parse::ParserPartRangeReader::type_range_or;
};

class ParserPostfixProbe final {
public:
    explicit ParserPostfixProbe(parse::Parser& parser) noexcept : reader_(parser), postfix_(parser)
    {
    }

    [[nodiscard]] syntax::AstModule& module() const noexcept
    {
        return this->reader_.module();
    }

    [[nodiscard]] bool type_like(const syntax::ExprId expr)
    {
        return this->postfix_.bracket_arg_expr_is_type_like(expr);
    }

    [[nodiscard]] syntax::TypeId convert_type_arg(const syntax::ExprId expr, const bool report_errors)
    {
        return this->postfix_.bracket_arg_expr_to_type(expr, report_errors);
    }

    [[nodiscard]] parse::BracketSuffixDecision classify_bracket_suffix(const syntax::ExprId base,
        const std::span<const parse::PostfixExprParser::BracketArg> args, const bool has_type_only_arg,
        const parse::ExprContext context)
    {
        return this->postfix_.classify_bracket_suffix(base, args, has_type_only_arg, context);
    }

    [[nodiscard]] syntax::TypeId append_selector(
        const syntax::TypeId base, const std::string_view name, const bool report_errors)
    {
        return this->postfix_.append_type_selector(base, name, base::SourceRange{}, report_errors);
    }

private:
    ParserPartRangeReaderProbe reader_;
    parse::PostfixExprParser postfix_;
};

class BracketSuffixClassifierProbe final {
public:
    explicit BracketSuffixClassifierProbe(parse::Parser& parser) noexcept : reader_(parser), classifier_(parser)
    {
    }

    [[nodiscard]] syntax::AstModule& module() const noexcept
    {
        return this->reader_.module();
    }

    [[nodiscard]] bool arg_starts_type_only() const noexcept
    {
        return this->classifier_.arg_starts_type_only();
    }

    [[nodiscard]] bool arg_expr_is_type_like(const syntax::ExprId expr) const
    {
        return this->classifier_.arg_expr_is_type_like(expr);
    }

    [[nodiscard]] parse::BracketSuffixDecision classify_after_expr(parse::BracketSuffixClassificationInput input) const
    {
        return this->classifier_.classify_after_expr(input);
    }

    [[nodiscard]] parse::BracketSuffixDecision classify_empty_suffix() const noexcept
    {
        return this->classifier_.classify_empty_suffix();
    }

    void advance_token()
    {
        static_cast<void>(this->reader_.advance_token());
    }

    void advance_tokens(const base::usize count)
    {
        for (base::usize index = 0; index < count; ++index) {
            this->advance_token();
        }
    }

private:
    ParserPartRangeReaderProbe reader_;
    parse::BracketSuffixClassifier classifier_;
};

[[nodiscard]] std::vector<syntax::Token> probe_tokens(std::string_view text = "module probe;")
{
    std::vector<syntax::Token> tokens;
    tokens.emplace_back(syntax::TokenKind::eof, base::SourceRange{PARSER_TEST_PROBE_SOURCE_ID, 0, 0}, text);
    return tokens;
}

[[nodiscard]] std::vector<syntax::Token> classifier_probe_tokens(
    std::initializer_list<syntax::TokenKind> leading_tokens)
{
    std::vector<syntax::Token> tokens;
    tokens.reserve(leading_tokens.size() + 1U);
    base::u32 offset = 0;
    for (const syntax::TokenKind kind : leading_tokens) {
        tokens.emplace_back(kind, base::SourceRange{PARSER_TEST_PROBE_SOURCE_ID, offset, 1}, "x");
        ++offset;
    }
    tokens.emplace_back(syntax::TokenKind::eof, base::SourceRange{PARSER_TEST_PROBE_SOURCE_ID, offset, 0}, "");
    return tokens;
}

} // namespace

TEST(CoreUnit, ParserAndAstDumpCoverLowLevelSyntaxBranches)
{
    parse::TokenCursor empty_cursor{std::span<const syntax::Token>{}};
    EXPECT_TRUE(empty_cursor.is_eof());
    EXPECT_EQ(empty_cursor.peek().kind, syntax::TokenKind::eof);
    EXPECT_EQ(empty_cursor.previous().kind, syntax::TokenKind::eof);
    EXPECT_EQ(empty_cursor.advance().kind, syntax::TokenKind::eof);

    constexpr std::string_view source =
        "module parser.dump;\n"
        "pub import c.host;\n"
        "extern c {\n"
        "  opaque struct Handle;\n"
        "  @name(\"puts\")\n"
        "  fn puts(s: *const u8) -> i32;\n"
        "  @name(\"printf\")\n"
        "  fn printf(format: *const u8, ...) -> i32;\n"
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
        "@name(\"exported\")\n"
        "export c fn exported(argc: i32, argv: *mut *mut u8) -> i32 {\n"
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
        "  let bytes_view: []const u8 = bytes[:];\n"
        "  let bytes_data: *const u8 = sliceptr(bytes_view);\n"
        "  let bytes_len: usize = slicelen(bytes_view);\n"
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
    expect_contains_all(token_dump,
        {
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
            "kw_sliceptr",
            "kw_slicelen",
            "ellipsis",
            "byte_literal",
            "byte_string_literal",
            "char_literal",
            "raw_string_literal",
            "string_literal",
        });

    const std::string ast = syntax::dump_ast(parsed.value());
    expect_contains_all(ast,
        {
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
            "struct_literal",
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
            "sliceptr",
            "slicelen",
        });
}

TEST(CoreUnit, ParserExpressionStorageDoesNotGrowArenaAfterInitialReserve)
{
    constexpr std::string_view source =
        "module parser.expr_arena;\n"
        "struct Pair { left: i32; right: i32; }\n"
        "fn callee(value: i32, other: i32) -> i32 { return value + other; }\n"
        "fn main(input: i32, flag: bool, values: []const i32) -> i32 {\n"
        "  let pair: Pair = Pair { left: input, right: 3 };\n"
        "  let raw: str = unsafe { strraw(strptr(\"abc\"), strblen(\"abc\")) };\n"
        "  let slice = values[0:2];\n"
        "  let idx = values[1];\n"
        "  let computed = -input + callee(pair.left, idx) * cast[i32](sizeof[*const i32]);\n"
        "  return if flag { computed } else { pair.right };\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer({90}, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    syntax::AstModule reserved_module;
    reserved_module.reserve_for_estimate(parse::ParseSession::estimate_ast_reserve(tokens.value()));
    const base::usize arena_bytes_after_reserve = reserved_module.exprs.arena_bytes();
    const base::usize arena_used_after_reserve = reserved_module.exprs.arena_used_bytes();
    const base::usize arena_blocks_after_reserve = reserved_module.exprs.arena_blocks();
    ASSERT_GT(arena_bytes_after_reserve, 0U);
    ASSERT_GT(arena_used_after_reserve, 0U);

    parse::Parser parser(tokens.value(), diagnostics);

    auto parsed = parser.parse_module();
    ASSERT_TRUE(parsed) << parsed.error().message;
    EXPECT_FALSE(diagnostics.has_error());

    const syntax::AstModule module = parsed.take_value();
    EXPECT_GT(module.exprs.size(), 0U);
    EXPECT_EQ(module.exprs.arena_bytes(), arena_bytes_after_reserve);
    EXPECT_GE(module.exprs.arena_used_bytes(), arena_used_after_reserve);
    EXPECT_LE(module.exprs.arena_used_bytes(), module.exprs.arena_bytes());
    EXPECT_EQ(module.exprs.arena_blocks(), arena_blocks_after_reserve);
}

TEST(CoreUnit, ParserAcceptsSliceTypesAndExpressions)
{
    constexpr std::string_view source = "module parser.slices;\n"
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
    expect_contains_all(ast,
        {
            "alias []const i32",
            "alias []mut i32",
            "slice",
            "slice_start",
            "slice_end",
            "index",
        });
}

TEST(CoreUnit, ParserAcceptsFunctionTypes)
{
    constexpr std::string_view source = "module parser.function_types;\n"
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
    expect_contains_all(ast,
        {
            "alias fn(i32, i32) -> i32",
            "alias extern c fn(*mut void, ...) -> void",
            "alias unsafe fn(i32) -> i32",
            "alias unsafe extern c fn(*mut void) -> void",
            "field priv run : fn(i32) -> i32",
            "param op : fn(i32, i32) -> i32",
        });
}

TEST(CoreUnit, ParserAcceptsPackageVisibilitySyntax)
{
    constexpr std::string_view source = "module parser.package_visibility;\n"
                                        "pub(package) import support.visible as visible;\n"
                                        "pub(package) type PackageInt = i32;\n"
                                        "pub(package) const PACKAGE_ANSWER: i32 = 42;\n"
                                        "pub(package) struct PackageBox {\n"
                                        "  pub(package) value: i32;\n"
                                        "}\n"
                                        "impl PackageBox {\n"
                                        "  pub(package) fn read(self: PackageBox) -> i32 {\n"
                                        "    return self.value;\n"
                                        "  }\n"
                                        "}\n";
    const syntax::AstModule module = parse_success(source);

    ASSERT_EQ(module.imports.size(), 1U);
    EXPECT_EQ(module.imports.front().visibility, syntax::Visibility::package_);
    EXPECT_TRUE(module.imports.front().explicit_visibility);

    const syntax::ItemNode* const alias = find_item(module, "PackageInt");
    const syntax::ItemNode* const constant = find_item(module, "PACKAGE_ANSWER");
    const syntax::ItemNode* const box = find_item(module, "PackageBox");
    const syntax::ItemNode* const read = find_item(module, "read");
    ASSERT_NE(alias, nullptr);
    ASSERT_NE(constant, nullptr);
    ASSERT_NE(box, nullptr);
    ASSERT_NE(read, nullptr);
    EXPECT_EQ(alias->visibility, syntax::Visibility::package_);
    EXPECT_EQ(constant->visibility, syntax::Visibility::package_);
    EXPECT_EQ(box->visibility, syntax::Visibility::package_);
    EXPECT_EQ(read->visibility, syntax::Visibility::package_);
    ASSERT_EQ(box->fields.size(), 1U);
    EXPECT_EQ(box->fields.front().visibility, syntax::Visibility::package_);

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "pub(package) import support.visible as visible",
            "item #0 pub(package) type_alias PackageInt",
            "item #1 pub(package) const PACKAGE_ANSWER",
            "item #2 pub(package) struct PackageBox",
            "field pub(package) value : i32",
            "pub(package) fn read",
        });
}

TEST(CoreUnit, ParserAcceptsTraitDeclarationsAndTraitImplScaffolding)
{
    constexpr std::string_view source = "module parser.traits;\n"
                                        "pub trait Reader[T] where T: Sized {\n"
                                        "  type Item;\n"
                                        "  fn read(self: &mut Self, buf: []mut u8) -> Self.Item;\n"
                                        "  pub fn size(self: &Self) -> usize;\n"
                                        "  fn default_size(self: &Self) -> usize { return 0; }\n"
                                        "  unsafe fn flush(self: &mut Self) -> void;\n"
                                        "}\n"
                                        "struct File { handle: i32; }\n"
                                        "impl Reader[File] for File {\n"
                                        "  type Item = usize;\n"
                                        "  fn read(self: &mut File, buf: []mut u8) -> usize {\n"
                                        "    return 0;\n"
                                        "  }\n"
                                        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* const trait_item = find_item(module, "Reader");
    ASSERT_NE(trait_item, nullptr);
    EXPECT_EQ(trait_item->kind, syntax::ItemKind::trait_decl);
    EXPECT_EQ(trait_item->visibility, syntax::Visibility::public_);
    ASSERT_EQ(trait_item->generic_params.size(), 1U);
    EXPECT_EQ(trait_item->generic_params.front().name, "T");
    ASSERT_EQ(trait_item->where_constraints.size(), 1U);
    EXPECT_EQ(trait_item->where_constraints.front().param_name, "T");
    ASSERT_EQ(trait_item->where_constraints.front().capability_names.size(), 1U);
    EXPECT_EQ(trait_item->where_constraints.front().capability_names.front(), "Sized");
    ASSERT_EQ(trait_item->trait_items.size(), 5U);

    const syntax::ItemNode associated_type = module.items[trait_item->trait_items.front().value];
    EXPECT_EQ(associated_type.kind, syntax::ItemKind::type_alias);
    EXPECT_EQ(associated_type.name, "Item");
    EXPECT_EQ(associated_type.visibility, syntax::Visibility::public_);
    EXPECT_FALSE(syntax::is_valid(associated_type.alias_type));

    const syntax::ItemNode requirement = module.items[trait_item->trait_items[1].value];
    EXPECT_EQ(requirement.kind, syntax::ItemKind::fn_decl);
    EXPECT_EQ(requirement.name, "read");
    EXPECT_TRUE(requirement.is_prototype);
    EXPECT_EQ(requirement.visibility, syntax::Visibility::public_);
    EXPECT_FALSE(syntax::is_valid(requirement.body));
    ASSERT_EQ(requirement.params.size(), 2U);
    EXPECT_EQ(requirement.params.front().name, "self");
    EXPECT_TRUE(syntax::is_valid(requirement.return_type));

    const syntax::ItemNode explicit_requirement = module.items[trait_item->trait_items[2].value];
    EXPECT_EQ(explicit_requirement.name, "size");
    EXPECT_TRUE(explicit_requirement.is_prototype);
    EXPECT_FALSE(explicit_requirement.is_trait_default_method);
    EXPECT_EQ(explicit_requirement.visibility, syntax::Visibility::public_);
    EXPECT_FALSE(explicit_requirement.is_unsafe);

    const syntax::ItemNode default_requirement = module.items[trait_item->trait_items[3].value];
    EXPECT_EQ(default_requirement.name, "default_size");
    EXPECT_FALSE(default_requirement.is_prototype);
    EXPECT_TRUE(default_requirement.is_trait_default_method);
    EXPECT_EQ(default_requirement.visibility, syntax::Visibility::public_);
    EXPECT_TRUE(syntax::is_valid(default_requirement.body));

    const syntax::ItemNode unsafe_requirement = module.items[trait_item->trait_items[4].value];
    EXPECT_EQ(unsafe_requirement.name, "flush");
    EXPECT_TRUE(unsafe_requirement.is_prototype);
    EXPECT_FALSE(unsafe_requirement.is_trait_default_method);
    EXPECT_TRUE(unsafe_requirement.is_unsafe);

    const syntax::ItemNode* impl_block = nullptr;
    for (base::usize index = 0; index < module.items.size(); ++index) {
        const syntax::ItemNode* const item = module.items.ptr(index);
        if (item != nullptr && item->kind == syntax::ItemKind::impl_block && syntax::is_valid(item->trait_type)) {
            impl_block = item;
            break;
        }
    }
    ASSERT_NE(impl_block, nullptr);
    EXPECT_TRUE(syntax::is_valid(impl_block->trait_type));
    EXPECT_TRUE(syntax::is_valid(impl_block->impl_type));
    ASSERT_EQ(impl_block->impl_items.size(), 2U);

    const syntax::ItemNode impl_associated_type = module.items[impl_block->impl_items.front().value];
    EXPECT_EQ(impl_associated_type.kind, syntax::ItemKind::type_alias);
    EXPECT_EQ(impl_associated_type.name, "Item");
    EXPECT_TRUE(syntax::is_valid(impl_associated_type.alias_type));
    EXPECT_TRUE(syntax::is_valid(impl_associated_type.trait_type));
    EXPECT_TRUE(syntax::is_valid(impl_associated_type.impl_type));

    const syntax::ItemNode impl_method = module.items[impl_block->impl_items[1].value];
    EXPECT_EQ(impl_method.kind, syntax::ItemKind::fn_decl);
    EXPECT_EQ(impl_method.name, "read");
    EXPECT_EQ(impl_method.visibility, syntax::Visibility::public_);
    EXPECT_TRUE(syntax::is_valid(impl_method.trait_type));
    EXPECT_TRUE(syntax::is_valid(impl_method.impl_type));
    EXPECT_TRUE(syntax::is_valid(impl_method.body));

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "pub trait Reader[T] where T: Sized",
            "pub type_alias Item",
            "pub fn read prototype",
            "return Self.Item",
            "pub fn size prototype",
            "pub fn default_size trait_default",
            "pub fn flush unsafe prototype",
            "impl Reader[File] for File",
            "pub type_alias Item for File in Reader[File]",
            "alias usize",
            "pub fn read for File in Reader[File]",
        });
}

TEST(CoreUnit, ParserAcceptsBorrowContractDecorators)
{
    constexpr std::string_view source = "module parser.borrow_contracts;\n"
                                        "@borrow(return = [left, right])\n"
                                        "fn choose(left: &i32, right: &i32) -> &i32 {\n"
                                        "  return left;\n"
                                        "}\n"
                                        "trait Viewer {\n"
                                        "  @borrow(return = [self])\n"
                                        "  fn view(self: &Self) -> &Self;\n"
                                        "}\n"
                                        "extern c {\n"
                                        "  @borrow(return = [static, unknown])\n"
                                        "  fn global_view() -> str;\n"
                                        "}\n"
                                        "@borrow(return = [value,])\n"
                                        "fn trailing(value: &i32) -> &i32 {\n"
                                        "  return value;\n"
                                        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* const choose = find_item(module, "choose");
    ASSERT_NE(choose, nullptr);
    ASSERT_TRUE(choose->borrow_contract.present);
    ASSERT_EQ(choose->borrow_contract.return_selectors.size(), 2U);
    EXPECT_EQ(choose->borrow_contract.return_selectors[0].kind, syntax::BorrowContractSelectorKind::parameter);
    EXPECT_EQ(choose->borrow_contract.return_selectors[0].name, "left");
    EXPECT_EQ(choose->borrow_contract.return_selectors[1].name, "right");

    const syntax::ItemNode* const viewer = find_item(module, "Viewer");
    ASSERT_NE(viewer, nullptr);
    ASSERT_EQ(viewer->trait_items.size(), 1U);
    const syntax::ItemNode requirement = module.items[viewer->trait_items.front().value];
    ASSERT_TRUE(requirement.borrow_contract.present);
    ASSERT_EQ(requirement.borrow_contract.return_selectors.size(), 1U);
    EXPECT_EQ(requirement.borrow_contract.return_selectors.front().kind, syntax::BorrowContractSelectorKind::self);

    const syntax::ItemNode* const global_view = find_item(module, "global_view");
    ASSERT_NE(global_view, nullptr);
    ASSERT_TRUE(global_view->borrow_contract.present);
    ASSERT_EQ(global_view->borrow_contract.return_selectors.size(), 2U);
    EXPECT_EQ(global_view->borrow_contract.return_selectors[0].kind, syntax::BorrowContractSelectorKind::static_);
    EXPECT_EQ(global_view->borrow_contract.return_selectors[1].kind, syntax::BorrowContractSelectorKind::unknown);

    const syntax::ItemNode* const trailing = find_item(module, "trailing");
    ASSERT_NE(trailing, nullptr);
    ASSERT_TRUE(trailing->borrow_contract.present);
    ASSERT_EQ(trailing->borrow_contract.return_selectors.size(), 1U);
    EXPECT_EQ(trailing->borrow_contract.return_selectors.front().name, "value");

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "priv fn choose @borrow(return=[left, right])",
            "pub fn view prototype @borrow(return=[self])",
            "priv fn global_view extern_c @borrow(return=[static, unknown])",
            "priv fn trailing @borrow(return=[value])",
        });
}

TEST(CoreUnit, ParserRejectsEmptyBorrowContractSelectorList)
{
    expect_parse_diagnostic("module parser.empty_borrow_contract;\n"
                            "@borrow(return = [])\n"
                            "fn empty(value: &i32) -> &i32 { return value; }\n",
        "expected borrow contract selector name");
}

TEST(CoreUnit, ParserRejectsUnknownFunctionDecoratorWithoutLegacyAbiHint)
{
    DiagnosticSink diagnostics;
    constexpr std::string_view source = "module parser.unknown_function_decorator;\n"
                                        "@brrow(return = [value])\n"
                                        "fn view(value: &i32) -> &i32 { return value; }\n";
    lex::Lexer lexer({6}, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    static_cast<void>(parser.parse_module());
    ASSERT_TRUE(diagnostics.has_error());
    EXPECT_TRUE(diagnostics_contain(diagnostics, "expected function decorator 'name' or 'borrow'"));
    EXPECT_FALSE(diagnostics_contain(diagnostics, "expected ABI decorator 'name'"));
}

TEST(CoreUnit, ParserRejectsMalformedFunctionDecorators)
{
    expect_parse_diagnostic("module parser.duplicate_name_decorator;\n"
                            "@name(\"one\")\n"
                            "@name(\"two\")\n"
                            "fn f() -> i32 { return 0; }\n",
        "duplicate ABI name decorator");
    expect_parse_diagnostic("module parser.name_decorator_missing_paren;\n"
                            "@name \"native\"\n"
                            "fn f() -> i32 { return 0; }\n",
        "expected '(' after function decorator");
    expect_parse_diagnostic("module parser.name_decorator_missing_string;\n"
                            "@name()\n"
                            "fn f() -> i32 { return 0; }\n",
        "expected string literal in ABI name");
    expect_parse_diagnostic("module parser.unknown_decorator_without_args;\n"
                            "@trace\n"
                            "fn f() -> i32 { return 0; }\n",
        "expected function decorator 'name' or 'borrow'");
    expect_parse_diagnostic("module parser.borrow_decorator_missing_paren;\n"
                            "@borrow return = [value])\n"
                            "fn f(value: &i32) -> &i32 { return value; }\n",
        "expected '(' after function decorator");
    expect_parse_diagnostic("module parser.duplicate_borrow_decorator;\n"
                            "@borrow(return = [value])\n"
                            "@borrow(return = [value])\n"
                            "fn f(value: &i32) -> &i32 { return value; }\n",
        "duplicate borrow contract decorator");
    expect_parse_diagnostic("module parser.postfix_borrow_decorator;\n"
                            "fn f(value: &i32) -> &i32 @borrow(return = [value]) { return value; }\n",
        "function decorators must appear before 'fn'");
    expect_parse_diagnostic("module parser.duplicate_postfix_borrow_decorator;\n"
                            "@borrow(return = [value])\n"
                            "fn f(value: &i32) -> &i32 @borrow(return = [value]) { return value; }\n",
        "duplicate borrow contract decorator");
    expect_parse_diagnostic("module parser.borrow_decorator_missing_separator;\n"
                            "@borrow(return = [left right])\n"
                            "fn f(left: &i32, right: &i32) -> &i32 { return left; }\n",
        "expected ',' or ']' after borrow contract selector");
    expect_parse_diagnostic("module parser.borrow_decorator_syncs_bad_separator;\n"
                            "@borrow(return = [left +, right])\n"
                            "fn f(left: &i32, right: &i32) -> &i32 { return left; }\n",
        "expected ',' or ']' after borrow contract selector");
    expect_parse_diagnostic("module parser.borrow_decorator_missing_selector_end;\n"
                            "@borrow(return = [value)\n"
                            "fn f(value: &i32) -> &i32 { return value; }\n",
        "expected ']' after borrow contract selector list");
}

TEST(CoreUnit, ParserAcceptsTraitDefaultMethodBodiesInWp2)
{
    constexpr std::string_view source = "module parser.trait_defaults;\n"
                                        "trait Reader {\n"
                                        "  fn read() -> i32 { return 1; }\n"
                                        "  fn next() -> i32;\n"
                                        "}\n"
                                        "fn after() -> i32 { return 0; }\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* const trait_item = find_item(module, "Reader");
    ASSERT_NE(trait_item, nullptr);
    ASSERT_EQ(trait_item->trait_items.size(), 2U);

    const syntax::ItemNode default_requirement = module.items[trait_item->trait_items.front().value];
    EXPECT_EQ(default_requirement.kind, syntax::ItemKind::fn_decl);
    EXPECT_EQ(default_requirement.name, "read");
    EXPECT_FALSE(default_requirement.is_prototype);
    EXPECT_TRUE(default_requirement.is_trait_default_method);
    EXPECT_TRUE(syntax::is_valid(default_requirement.body));

    const syntax::ItemNode prototype_requirement = module.items[trait_item->trait_items.back().value];
    EXPECT_EQ(prototype_requirement.name, "next");
    EXPECT_TRUE(prototype_requirement.is_prototype);
    EXPECT_FALSE(prototype_requirement.is_trait_default_method);
    EXPECT_FALSE(syntax::is_valid(prototype_requirement.body));

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "priv trait Reader",
            "pub fn read trait_default",
            "return i32",
            "integer_literal `1`",
            "pub fn next prototype",
            "fn after",
        });
}

TEST(CoreUnit, ParserRejectsMalformedTraitItemsInWp2)
{
    expect_parse_diagnostic("module parser.bad_trait_item;\n"
                            "trait Reader {\n"
                            "  const answer: i32 = 1;\n"
                            "  fn next() -> i32;\n"
                            "}\n"
                            "fn after() -> i32 { return 0; }\n",
        "expected function requirement or associated type in trait declaration");
    expect_parse_diagnostic("module parser.bad_trait_requirement_terminator;\n"
                            "trait Reader {\n"
                            "  fn read() -> i32\n"
                            "}\n"
                            "fn after() -> i32 { return 0; }\n",
        "expected block");
}

TEST(CoreUnit, ParserRejectsNonFunctionItemsInInherentImplBlocks)
{
    expect_parse_diagnostic("module parser.bad_inherent_impl_item;\n"
                            "struct Box { value: i32; }\n"
                            "impl Box {\n"
                            "  type Item = i32;\n"
                            "  fn ok(self: &Box) -> i32 { return self.value; }\n"
                            "}\n",
        "expected function declaration in impl block");
}

TEST(CoreUnit, ParserAcceptsUnsafeExternFunctions)
{
    const syntax::AstModule module = parse_success("module parser.unsafe_extern;\n"
                                                   "extern c {\n"
                                                   "  unsafe fn read() -> i32;\n"
                                                   "}\n");

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "extern_block extern_c",
            "fn read extern_c unsafe",
        });
}

TEST(CoreUnit, ParserAcceptsSelectiveUseReexports)
{
    constexpr std::string_view source = "module parser.use_reexports;\n"
                                        "pub use support.visible.Value;\n"
                                        "pub(package) use support.visible.make as make_visible;\n"
                                        "fn local() -> i32 { return 0; }\n";
    const syntax::AstModule module = parse_success(source);

    ASSERT_EQ(module.reexports.size(), 2U);
    EXPECT_EQ(module.reexports[0].visibility, syntax::Visibility::public_);
    EXPECT_EQ(module.reexports[0].module_path.parts, std::vector<std::string_view>({"support", "visible"}));
    EXPECT_EQ(module.reexports[0].target_name, "Value");
    EXPECT_EQ(module.reexports[0].alias, "Value");
    EXPECT_EQ(module.reexports[0].target_name_id, module.find_identifier("Value"));
    EXPECT_EQ(module.reexports[0].alias_id, module.find_identifier("Value"));
    EXPECT_EQ(module.reexports[1].visibility, syntax::Visibility::package_);
    EXPECT_EQ(module.reexports[1].target_name, "make");
    EXPECT_EQ(module.reexports[1].alias, "make_visible");
    EXPECT_EQ(module.reexports[1].alias_id, module.find_identifier("make_visible"));

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "pub use support.visible.Value",
            "pub(package) use support.visible.make as make_visible",
            "priv fn local",
        });
}

TEST(CoreUnit, ParserAcceptsCAsOrdinaryIdentifier)
{
    constexpr std::string_view source = "module parser.c_identifier;\n"
                                        "fn c(value: i32) -> i32 { return value; }\n"
                                        "fn mul_add(a: i32, b: i32, c: i32) -> i32 {\n"
                                        "  return a * b + c;\n"
                                        "}\n"
                                        "fn use_local(seed: i32) -> i32 {\n"
                                        "  let c: i32 = seed;\n"
                                        "  return c;\n"
                                        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* const function = find_item(module, "mul_add");
    ASSERT_NE(function, nullptr);
    ASSERT_EQ(function->params.size(), 3U);
    EXPECT_EQ(function->params[2].name, "c");

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "priv fn c",
            "param c : i32",
            "let c : i32",
            "name `c`",
        });
}

TEST(CoreUnit, ParserAcceptsPrimaryModulePartDeclarations)
{
    constexpr std::string_view source = "module parser.module_parts;\n"
                                        "part parser;\n"
                                        "part emitter;\n"
                                        "import regex.ast as ast;\n"
                                        "fn compile(value: i32) -> i32 { return value; }\n";
    const syntax::AstModule module = parse_success(source);

    EXPECT_EQ(module.file_kind, syntax::ModuleFileKind::primary);
    ASSERT_EQ(module.part_declarations.size(), 2U);
    EXPECT_EQ(module.part_declarations[0].name, "parser");
    EXPECT_EQ(module.part_declarations[0].name_id, module.find_identifier("parser"));
    EXPECT_EQ(module.part_declarations[1].name, "emitter");
    EXPECT_EQ(module.part_declarations[1].name_id, module.find_identifier("emitter"));
    ASSERT_EQ(module.imports.size(), 1U);

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "module parser.module_parts",
            "part parser",
            "part emitter",
            "priv import regex.ast as ast",
            "priv fn compile",
        });
}

TEST(CoreUnit, ParserAcceptsModulePartFileHeader)
{
    constexpr std::string_view source = "module parser.module_parts part parser;\n"
                                        "import regex.ast as ast;\n"
                                        "fn parse(value: i32) -> i32 { return value; }\n";
    const syntax::AstModule module = parse_success(source);

    EXPECT_EQ(module.file_kind, syntax::ModuleFileKind::part);
    EXPECT_EQ(module.part_header.name, "parser");
    EXPECT_EQ(module.part_header.name_id, module.find_identifier("parser"));
    EXPECT_TRUE(module.part_declarations.empty());
    ASSERT_EQ(module.imports.size(), 1U);

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "module parser.module_parts part parser",
            "priv import regex.ast as ast",
            "priv fn parse",
        });
}

TEST(CoreUnit, ParserModulePartHelpersDiagnoseUnexpectedContextualKeyword)
{
    DiagnosticSink decl_diagnostics;
    lex::Lexer decl_lexer({6}, "wrong parser;", decl_diagnostics);
    auto decl_tokens = decl_lexer.tokenize();
    ASSERT_TRUE(decl_tokens) << decl_tokens.error().message;

    parse::Parser decl_parser(decl_tokens.value(), decl_diagnostics);
    parse::ItemParser decl_items(decl_parser);
    const syntax::ModulePartDecl decl = decl_items.parse_module_part_decl();
    EXPECT_EQ(decl.name, "parser");
    ASSERT_TRUE(decl_diagnostics.has_error());
    EXPECT_TRUE(diagnostics_contain(decl_diagnostics, "expected part name after 'part'"));

    DiagnosticSink header_diagnostics;
    lex::Lexer header_lexer({6}, "wrong parser;", header_diagnostics);
    auto header_tokens = header_lexer.tokenize();
    ASSERT_TRUE(header_tokens) << header_tokens.error().message;

    parse::Parser header_parser(header_tokens.value(), header_diagnostics);
    parse::ItemParser header_items(header_parser);
    const syntax::ModulePartHeader header = header_items.parse_module_part_header();
    EXPECT_EQ(header.name, "parser");
    ASSERT_TRUE(header_diagnostics.has_error());
    EXPECT_TRUE(diagnostics_contain(header_diagnostics, "expected part name after 'part'"));
}

TEST(CoreUnit, ParserModulePartHelpersRecoverMissingContextualKeyword)
{
    DiagnosticSink decl_diagnostics;
    lex::Lexer decl_lexer({6}, "+ parser;", decl_diagnostics);
    auto decl_tokens = decl_lexer.tokenize();
    ASSERT_TRUE(decl_tokens) << decl_tokens.error().message;

    parse::Parser decl_parser(decl_tokens.value(), decl_diagnostics);
    parse::ItemParser decl_items(decl_parser);
    const syntax::ModulePartDecl decl = decl_items.parse_module_part_decl();
    EXPECT_EQ(decl.name, "parser");
    EXPECT_TRUE(decl_diagnostics.has_error());

    DiagnosticSink header_diagnostics;
    lex::Lexer header_lexer({6}, "+ parser;", header_diagnostics);
    auto header_tokens = header_lexer.tokenize();
    ASSERT_TRUE(header_tokens) << header_tokens.error().message;

    parse::Parser header_parser(header_tokens.value(), header_diagnostics);
    parse::ItemParser header_items(header_parser);
    const syntax::ModulePartHeader header = header_items.parse_module_part_header();
    EXPECT_EQ(header.name, "parser");
    EXPECT_TRUE(header_diagnostics.has_error());
}

TEST(CoreUnit, ParserKeepsPartAsOrdinaryIdentifierOutsideModuleDeclarationArea)
{
    constexpr std::string_view source = "module parser.part_identifier;\n"
                                        "fn part(part: i32) -> i32 {\n"
                                        "  let part: i32 = part;\n"
                                        "  return part;\n"
                                        "}\n";
    const syntax::AstModule module = parse_success(source);

    EXPECT_TRUE(module.part_declarations.empty());
    const syntax::ItemNode* const function = find_item(module, "part");
    ASSERT_NE(function, nullptr);
    ASSERT_EQ(function->params.size(), 1U);
    EXPECT_EQ(function->params.front().name, "part");

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "priv fn part",
            "param part : i32",
            "let part : i32",
            "name `part`",
        });
}

TEST(CoreUnit, ParserRejectsMalformedModulePartDeclarations)
{
    expect_parse_error("module parser.missing_part_name;\n"
                       "part ;\n"
                       "fn ok() -> i32 { return 0; }\n",
        "expected part name after 'part'");
    expect_parse_error("module parser.bad_part_header part ;\n"
                       "fn ok() -> i32 { return 0; }\n",
        "expected part name after 'part'");
    expect_parse_error("module parser.late_part;\n"
                       "import regex.ast as ast;\n"
                       "part parser;\n"
                       "fn ok() -> i32 { return 0; }\n",
        "part declarations must appear before imports and items");
    expect_parse_error("part parser;\n"
                       "fn ok() -> i32 { return 0; }\n",
        "part declarations must appear after module declaration");
    expect_parse_error("module parser.part_file part parser;\n"
                       "part nested;\n"
                       "fn ok() -> i32 { return 0; }\n",
        "module part files cannot declare nested part lists");
    expect_parse_error("module parser.part_file part parser;\n"
                       "import regex.ast as ast;\n"
                       "part nested;\n"
                       "fn ok() -> i32 { return 0; }\n",
        "module part files cannot declare nested part lists");
    expect_parse_error("module parser.glob_use;\n"
                       "pub use support.visible.*;\n",
        "glob use is not supported");
    expect_parse_error("module parser.private_use;\n"
                       "priv use support.visible.Value;\n",
        "selective use re-export must use pub or pub(package)");
    expect_parse_error("module parser.part_file part parser;\n"
                       "pub use support.visible.Value;\n",
        "module part files cannot declare selective re-exports");
    expect_parse_error("module parser.late_use;\n"
                       "fn first() -> i32 { return 0; }\n"
                       "pub use support.visible.Value;\n",
        "use declarations must appear before items");
}

TEST(CoreUnit, ParserRejectsBareSliceType)
{
    expect_parse_error("module parser.bad_slice_type;\n"
                       "type Bad = []i32;\n",
        "expected 'mut' or 'const' after '[]'");
}

TEST(CoreUnit, ParserRejectsUnsafeWithoutBlock)
{
    expect_parse_error("module parser.bad_unsafe_block;\n"
                       "fn value() -> i32 {\n"
                       "  return {\n"
                       "    unsafe 1\n"
                       "  };\n"
                       "}\n",
        "expected block after 'unsafe'");
}

TEST(CoreUnit, ParserCoversRecoveryNumericEnumValuesAndShiftLookahead)
{
    {
        DiagnosticSink diagnostics;
        constexpr std::string_view source = "module parser.recover;\n"
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
        constexpr std::string_view source = "module parser.numeric;\n"
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
        expect_contains_all(ast,
            {
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
                "struct_literal",
                "try_expr",
                "binary",
            });
    }
}

TEST(CoreUnit, ParserRecoveryStopsAtNextItemWithoutSemicolon)
{
    constexpr base::SourceId PARSER_TEST_RECOVERY_SOURCE_ID{8};
    constexpr std::string_view source = "module parser.recovery_boundary;\n"
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

TEST(CoreUnit, ParserAcceptsFrozenTrailingSeparatorPolicy)
{
    constexpr std::string_view source = "module parser.trailing_separators;\n"
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

TEST(CoreUnit, ParserAcceptsTupleTypesLiteralsAndDestructuring)
{
    constexpr std::string_view source = "module parser.tuples;\n"
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
                                        "  let (value, _) = pair;\n"
                                        "  return value;\n"
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
    ASSERT_EQ(module.exprs.kind(pair_stmt.init.value), syntax::ExprKind::tuple_literal);
    const syntax::AstArenaVector<syntax::ExprId>* const pair_init = module.exprs.tuple_elements(pair_stmt.init.value);
    ASSERT_NE(pair_init, nullptr);
    EXPECT_EQ(pair_init->size(), 2U);

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
    expect_contains_all(ast,
        {
            "alias (i32, bool)",
            "alias (i32,)",
            "tuple_literal",
            "tuple_element",
            "stmt #",
            "(x, ok)",
            "(single,)",
        });
}

TEST(CoreUnit, ParserAcceptsMultiSegmentQualifiedTypeAnnotations)
{
    constexpr std::string_view source = "module parser.qualified_types;\n"
                                        "type FileBox = core.mem.Box[core.mem.File];\n"
                                        "fn use(file: core.mem.File) -> core.mem.Box[core.mem.File] {\n"
                                        "  return file;\n"
                                        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* file_box = find_item(module, "FileBox");
    ASSERT_NE(file_box, nullptr);
    ASSERT_TRUE(syntax::is_valid(file_box->alias_type));
    const syntax::TypeNode& alias_type = module.types[file_box->alias_type.value];
    ASSERT_EQ(alias_type.kind, syntax::TypeKind::named);
    EXPECT_EQ(alias_type.name, "Box");
    ASSERT_EQ(alias_type.scope_parts.size(), 2U);
    EXPECT_EQ(alias_type.scope_parts[0], "core");
    EXPECT_EQ(alias_type.scope_parts[1], "mem");
    ASSERT_EQ(alias_type.type_args.size(), 1U);
    const syntax::TypeNode& file_arg = module.types[alias_type.type_args.front().value];
    ASSERT_EQ(file_arg.scope_parts.size(), 2U);
    EXPECT_EQ(file_arg.scope_parts[0], "core");
    EXPECT_EQ(file_arg.scope_parts[1], "mem");
    EXPECT_EQ(file_arg.name, "File");

    const syntax::ItemNode* use = find_item(module, "use");
    ASSERT_NE(use, nullptr);
    ASSERT_EQ(use->params.size(), 1U);
    const syntax::TypeNode& param_type = module.types[use->params.front().type.value];
    ASSERT_EQ(param_type.scope_parts.size(), 2U);
    EXPECT_EQ(param_type.name, "File");

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "alias core.mem.Box[core.mem.File]",
            "param file : core.mem.File",
            "return core.mem.Box[core.mem.File]",
        });
}

TEST(CoreUnit, ParserRejectsEmptyTupleForms)
{
    expect_parse_error("module parser.empty_tuple_type;\n"
                       "type Empty = ();\n",
        "empty tuple type is not part of M2 syntax");
    expect_parse_error("module parser.empty_tuple_literal;\n"
                       "fn main() -> i32 {\n"
                       "  let value = ();\n"
                       "  return 0;\n"
                       "}\n",
        "empty tuple literal is not part of M2 syntax");
    expect_parse_error("module parser.empty_tuple_pattern;\n"
                       "fn main() -> i32 {\n"
                       "  let () = 1;\n"
                       "  return 0;\n"
                       "}\n",
        "empty tuple pattern is not part of M2 syntax");
}

TEST(CoreUnit, ParserAcceptsSlicePatternsAndLetElse)
{
    constexpr std::string_view source = "module parser.slice_patterns;\n"
                                        "enum Maybe { some(i32), none }\n"
                                        "fn main() -> i32 {\n"
                                        "  let values: [3]i32 = [1, 2, 3];\n"
                                        "  let [..] = values;\n"
                                        "  let [.., last] = values;\n"
                                        "  let [[nested], ..] = values;\n"
                                        "  let [dot_case, .none] = values;\n"
                                        "  let [head, .., tail] = values;\n"
                                        "  let .some(value) = Maybe.some(head) else {\n"
                                        "    return tail;\n"
                                        "  };\n"
                                        "  let .some(.some(inner)) = Maybe.some(head) else {\n"
                                        "    return 0;\n"
                                        "  };\n"
                                        "  let .some(Wrapper { _ }) = Maybe.some(head) else {\n"
                                        "    return 0;\n"
                                        "  };\n"
                                        "  let .some(choice) | .none | .some(fallback) = Maybe.some(value) else {\n"
                                        "    return dot_case;\n"
                                        "  };\n"
                                        "  return value + inner;\n"
                                        "}\n";
    const syntax::AstModule module = parse_success(source);
    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "[..]",
            "[.., last]",
            "[[nested], ..]",
            "[dot_case, .none]",
            "[head, .., tail]",
            ".some(value)",
            ".some(.some(inner))",
            ".some(choice) | .none | .some(fallback)",
        });
}

TEST(CoreUnit, ParserKeepsPatternEnumCasesInExplicitTypeNamespace)
{
    constexpr std::string_view source = "module parser.generic_enum_case_patterns;\n"
                                        "enum Option[T] { some(T), none }\n"
                                        "fn main(opt: Option[i32]) -> i32 {\n"
                                        "  return match opt {\n"
                                        "    Option[i32].some(value) => value,\n"
                                        "    Option[i32].none => 0,\n"
                                        "  };\n"
                                        "}\n";
    const syntax::AstModule module = parse_success(source);
    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "Option[i32].some(value)",
            "Option[i32].none",
        });

    const syntax::ItemNode* main = find_item(module, "main");
    ASSERT_NE(main, nullptr);
    ASSERT_TRUE(syntax::is_valid(main->body));
    const syntax::StmtNode& body = module.stmts[main->body.value];
    ASSERT_EQ(body.kind, syntax::StmtKind::block);
    ASSERT_EQ(body.statements.size(), 1U);
    const syntax::StmtNode& return_stmt = module.stmts[body.statements.front().value];
    ASSERT_EQ(return_stmt.kind, syntax::StmtKind::return_);
    ASSERT_TRUE(syntax::is_valid(return_stmt.return_value));
    ASSERT_EQ(module.exprs.kind(return_stmt.return_value.value), syntax::ExprKind::match_expr);
    const syntax::MatchExprPayload* const match_expr = module.exprs.match_payload(return_stmt.return_value.value);
    ASSERT_NE(match_expr, nullptr);
    ASSERT_EQ(match_expr->arms.size(), 2U);

    const syntax::PatternNode& some = module.patterns[match_expr->arms.front().pattern.value];
    ASSERT_EQ(some.kind, syntax::PatternKind::enum_case);
    EXPECT_TRUE(some.scoped);
    EXPECT_TRUE(syntax::is_valid(some.enum_type));
    EXPECT_EQ(some.enum_name, "");
    EXPECT_EQ(some.case_name, "some");
    ASSERT_EQ(some.payload_patterns.size(), 1U);

    const syntax::TypeNode& option_type = module.types[some.enum_type.value];
    ASSERT_EQ(option_type.kind, syntax::TypeKind::named);
    EXPECT_EQ(option_type.name, "Option");
    ASSERT_EQ(option_type.type_args.size(), 1U);
    const syntax::TypeNode& i32_arg = module.types[option_type.type_args.front().value];
    ASSERT_EQ(i32_arg.kind, syntax::TypeKind::primitive);
    EXPECT_EQ(i32_arg.primitive, syntax::PrimitiveTypeKind::i32);

    const syntax::PatternNode& value = module.patterns[some.payload_patterns.front().value];
    EXPECT_EQ(value.kind, syntax::PatternKind::binding);
    EXPECT_EQ(value.binding_name, "value");
}

TEST(CoreUnit, ParserAcceptsQualifiedGenericExplicitEnumCasePattern)
{
    constexpr std::string_view source =
        "module parser.qualified_generic_enum_case_patterns;\n"
        "enum Option[T] { some(T), none }\n"
        "fn main(opt: parser.qualified_generic_enum_case_patterns.Option[i32]) -> i32 {\n"
        "  return match opt {\n"
        "    parser.qualified_generic_enum_case_patterns.Option[i32].some(value) => value,\n"
        "    parser.qualified_generic_enum_case_patterns.Option[i32].none => 0,\n"
        "  };\n"
        "}\n";
    const syntax::AstModule module = parse_success(source);
    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "parser.qualified_generic_enum_case_patterns.Option[i32].some(value)",
            "parser.qualified_generic_enum_case_patterns.Option[i32].none",
        });
}

TEST(CoreUnit, ParserRecoversMalformedExplicitEnumCasePatternTypeArgs)
{
    expect_parse_error("module parser.malformed_explicit_enum_case_pattern;\n"
                       "enum Option[T] { some(T), none }\n"
                       "fn main(opt: Option[i32]) -> i32 {\n"
                       "  return match opt {\n"
                       "    Option[].some(value) => value,\n"
                       "    Option[i32].none => 0,\n"
                       "  };\n"
                       "}\n",
        "expected generic type argument");
    expect_parse_error("module parser.missing_explicit_enum_case_dot;\n"
                       "enum Option[T] { some(T), none }\n"
                       "fn main(opt: Option[i32]) -> i32 {\n"
                       "  return match opt {\n"
                       "    Option[i32] => 0,\n"
                       "  };\n"
                       "}\n",
        "expected '.' before enum case pattern name");
    expect_parse_error("module parser.malformed_explicit_enum_case_args;\n"
                       "enum Pair[A, B] { some(A, B), none }\n"
                       "fn main(opt: Pair[i32, i32]) -> i32 {\n"
                       "  return match opt {\n"
                       "    Pair[i32 @ i32].some(left, right) => left + right,\n"
                       "    Pair[i32, i32].none => 0,\n"
                       "  };\n"
                       "}\n",
        "expected ',' or ']' after generic type argument");
}

TEST(CoreUnit, ParserRejectsBareEnumCasePatternButRecoversAsBinding)
{
    constexpr base::SourceId PARSER_TEST_BARE_ENUM_CASE_PATTERN_SOURCE_ID{31};
    constexpr std::string_view source = "module parser.bare_enum_case_pattern;\n"
                                        "enum Option { some(i32), none }\n"
                                        "fn main(opt: Option) -> i32 {\n"
                                        "  return match opt {\n"
                                        "    some(value) => value,\n"
                                        "    .none => 0,\n"
                                        "  };\n"
                                        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer(PARSER_TEST_BARE_ENUM_CASE_PATTERN_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_FALSE(parsed);
    ASSERT_TRUE(diagnostics.has_error());

    bool found = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        if (diagnostic.message.find("bare enum case patterns are not supported") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(CoreUnit, ParserRejectsMalformedSlicePatternRest)
{
    expect_parse_error("module parser.slice_pattern_rest;\n"
                       "fn main() -> i32 {\n"
                       "  let [head, .., ..] = [1, 2];\n"
                       "  return head;\n"
                       "}\n",
        "slice pattern can contain at most one '..' rest marker");
    expect_parse_error("module parser.slice_pattern_ellipsis;\n"
                       "fn main() -> i32 {\n"
                       "  let [head, ...] = [1, 2];\n"
                       "  return head;\n"
                       "}\n",
        "slice pattern rest marker is '..'");
    expect_parse_error("module parser.slice_pattern_separator;\n"
                       "fn main() -> i32 {\n"
                       "  let [head tail] = [1, 2];\n"
                       "  return head;\n"
                       "}\n",
        "expected ',' or ']' after slice pattern element");
    expect_parse_error("module parser.slice_pattern_separator_recovery;\n"
                       "fn main() -> i32 {\n"
                       "  let [head tail, other] = [1, 2, 3];\n"
                       "  return head;\n"
                       "}\n",
        "expected ',' or ']' after slice pattern element");
    expect_parse_error("module parser.let_else_name;\n"
                       "fn main() -> i32 {\n"
                       "  let value = 1 else { return 0; };\n"
                       "  return value;\n"
                       "}\n",
        "let-else requires a destructuring or refutable pattern");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedTupleSeparators)
{
    constexpr base::SourceId PARSER_TEST_TUPLE_RECOVERY_SOURCE_ID{33};
    constexpr std::string_view source = "module parser.tuple_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryCoversTupleSynchronizationAndFunctionTypeVariadics)
{
    constexpr base::SourceId PARSER_TEST_TUPLE_SYNC_RECOVERY_SOURCE_ID{34};
    constexpr std::string_view source = "module parser.tuple_sync_recovery;\n"
                                        "type SizedA = [4usize]i32;\n"
                                        "type SizedB = [0b10u8]u8;\n"
                                        "type BadSize = [18446744073709551616]i32;\n"
                                        "type BadGeneric = Pair[i32 +, bool];\n"
                                        "type BadTuple = (i32, bool +, u8);\n"
                                        "type BadFnA = fn(..., i32) -> i32;\n"
                                        "type BadFnB = fn(i32, ..., bool) -> i32;\n"
                                        "type BadFnC = fn(i32 +, bool) -> i32;\n"
                                        "fn bad_params(value: i32, ..., extra: i32) -> i32 { return value; }\n"
                                        "fn recovered(value: i32) -> i32 {\n"
                                        "  let (only) = value;\n"
                                        "  let missing_repeat = [0;];\n"
                                        "  let bad_array = [1 ->, 2];\n"
                                        "  let bad_literal = (1, true ->, 2);\n"
                                        "  let (a, b +, c) = (1, 2, 3);\n"
                                        "  let bad_apply = value[i32 +, bool](1);\n"
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

TEST(CoreUnit, ParserRejectsNumericTupleFieldAccess)
{
    expect_parse_error("module parser.numeric_tuple_field_boundary;\n"
                       "fn main() -> i32 {\n"
                       "  let pair = (1, false);\n"
                       "  return pair.0;\n"
                       "}\n",
        "tuple fields are not directly accessible; destructure the tuple or use a named struct");
    expect_parse_error("module parser.spaced_numeric_tuple_field_boundary;\n"
                       "fn main() -> i32 {\n"
                       "  let pair = (1, false);\n"
                       "  return pair . 0;\n"
                       "}\n",
        "tuple fields are not directly accessible; destructure the tuple or use a named struct");
}

TEST(CoreUnit, ParserDoesNotTreatSuffixedFloatAsTupleField)
{
    expect_parse_error("module parser.float_tuple_field_boundary;\n"
                       "fn main() -> i32 {\n"
                       "  let pair = (1, false);\n"
                       "  return pair.0f32;\n"
                       "}\n",
        "expected ';' after return");
}

TEST(CoreUnit, ParserAcceptsUnifiedBlockExpressionBody)
{
    constexpr std::string_view source = "module parser.block_body;\n"
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
    ASSERT_EQ(module.exprs.kind(value_stmt.init.value), syntax::ExprKind::block_expr);
    const syntax::BlockExprPayload* const block_expr = module.exprs.block_payload(value_stmt.init.value);
    ASSERT_NE(block_expr, nullptr);
    ASSERT_TRUE(syntax::is_valid(block_expr->block));
    ASSERT_TRUE(syntax::is_valid(block_expr->result));
    const syntax::StmtNode& expr_block = module.stmts[block_expr->block.value];
    ASSERT_EQ(expr_block.kind, syntax::StmtKind::block);
    ASSERT_EQ(expr_block.statements.size(), 5U);
    EXPECT_EQ(module.stmts[expr_block.statements[1].value].kind, syntax::StmtKind::if_);
    EXPECT_EQ(module.stmts[expr_block.statements[2].value].kind, syntax::StmtKind::while_);
    EXPECT_EQ(module.stmts[expr_block.statements[3].value].kind, syntax::StmtKind::for_);
    EXPECT_EQ(module.stmts[expr_block.statements[4].value].kind, syntax::StmtKind::block);
    EXPECT_EQ(module.exprs.kind(block_expr->result.value), syntax::ExprKind::if_expr);
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedArrayTypeSeparators)
{
    constexpr base::SourceId PARSER_TEST_ARRAY_TYPE_RECOVERY_SOURCE_ID{9};
    constexpr std::string_view source = "module parser.array_type_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedMatchArmSeparators)
{
    constexpr base::SourceId PARSER_TEST_MATCH_ARM_RECOVERY_SOURCE_ID{10};
    constexpr std::string_view source = "module parser.match_arm_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedCallArgumentSeparators)
{
    constexpr base::SourceId PARSER_TEST_CALL_ARG_RECOVERY_SOURCE_ID{11};
    constexpr std::string_view source = "module parser.call_arg_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedArrayLiteralSeparators)
{
    constexpr base::SourceId PARSER_TEST_ARRAY_LITERAL_RECOVERY_SOURCE_ID{32};
    constexpr std::string_view source = "module parser.array_literal_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedStructLiteralFieldSeparators)
{
    constexpr base::SourceId PARSER_TEST_STRUCT_FIELD_RECOVERY_SOURCE_ID{12};
    constexpr std::string_view source = "module parser.struct_field_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedParameterSeparators)
{
    constexpr base::SourceId PARSER_TEST_PARAM_RECOVERY_SOURCE_ID{13};
    constexpr std::string_view source = "module parser.parameter_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedStructDeclarationFieldSeparators)
{
    constexpr base::SourceId PARSER_TEST_STRUCT_DECL_FIELD_RECOVERY_SOURCE_ID{14};
    constexpr std::string_view source = "module parser.struct_decl_field_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedEnumCaseSeparators)
{
    constexpr base::SourceId PARSER_TEST_ENUM_CASE_RECOVERY_SOURCE_ID{15};
    constexpr std::string_view source = "module parser.enum_case_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedStructFieldSeparators)
{
    constexpr base::SourceId PARSER_TEST_STRUCT_FIELD_RECOVERY_SOURCE_ID{16};
    constexpr std::string_view source = "module parser.struct_field_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedAbiAttributeArguments)
{
    constexpr base::SourceId PARSER_TEST_ABI_ARGUMENT_RECOVERY_SOURCE_ID{17};
    constexpr std::string_view source = "module parser.abi_argument_recovery;\n"
                                        "extern c { @name(\"puts\" @) fn puts(s: *const u8) -> i32; }\n"
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
    expect_contains(messages, "expected ')' after function decorator");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedImportPathSegments)
{
    constexpr base::SourceId PARSER_TEST_PATH_RECOVERY_SOURCE_ID{18};
    constexpr std::string_view source = "module parser.path_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedBuiltinArgumentSeparators)
{
    constexpr base::SourceId PARSER_TEST_BUILTIN_ARG_RECOVERY_SOURCE_ID{19};
    constexpr std::string_view source = "module parser.builtin_arg_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedImportAliases)
{
    constexpr base::SourceId PARSER_TEST_IMPORT_ALIAS_RECOVERY_SOURCE_ID{20};
    constexpr std::string_view source = "module parser.import_alias_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedStatementTerminators)
{
    constexpr base::SourceId PARSER_TEST_STATEMENT_TERMINATOR_RECOVERY_SOURCE_ID{21};
    constexpr std::string_view source = "module parser.statement_terminator_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedForClauseSeparators)
{
    constexpr base::SourceId PARSER_TEST_FOR_CLAUSE_RECOVERY_SOURCE_ID{22};
    constexpr std::string_view source = "module parser.for_clause_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedBlockOpeners)
{
    constexpr base::SourceId PARSER_TEST_BLOCK_OPENER_RECOVERY_SOURCE_ID{23};
    constexpr std::string_view source = "module parser.block_opener_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryStopsMissingBlockEndAtNextItem)
{
    constexpr base::SourceId PARSER_TEST_BLOCK_END_RECOVERY_SOURCE_ID{24};
    constexpr std::string_view source = "module parser.block_end_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedExpressionDelimiters)
{
    constexpr base::SourceId PARSER_TEST_EXPRESSION_DELIMITER_RECOVERY_SOURCE_ID{25};
    constexpr std::string_view source = "module parser.expression_delimiter_recovery;\n"
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
    bool found_parser_error = false;
    bool found_parser_note = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
        found_parser_error = found_parser_error
            || (diagnostic.category == DiagnosticCategory::parser && diagnostic.code == DiagnosticCode::parser_syntax);
        found_parser_note = found_parser_note
            || (diagnostic.category == DiagnosticCategory::parser && diagnostic.code == DiagnosticCode::parser_note);
    }
    expect_contains(messages, "expected ')' after expression");
    expect_contains(messages, "expected ',' or ']' after generic type argument");
    expect_contains(messages, "expected ')' after ptraddr argument");
    expect_contains(messages, "expected ',' or ')' after argument");
    expect_contains(messages, "opening delimiter is here");
    expect_contains(messages, "expected expression");
    EXPECT_TRUE(found_parser_error);
    EXPECT_TRUE(found_parser_note);
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedTypeAndPatternDelimiters)
{
    constexpr base::SourceId PARSER_TEST_TYPE_PATTERN_DELIMITER_RECOVERY_SOURCE_ID{26};
    constexpr std::string_view source = "module parser.type_pattern_delimiter_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedDeclarationDelimiters)
{
    constexpr base::SourceId PARSER_TEST_DECLARATION_DELIMITER_RECOVERY_SOURCE_ID{27};
    constexpr std::string_view source = "module parser.declaration_delimiter_recovery;\n"
                                        "import c.host @;\n"
                                        "type Alias = i32 @;\n"
                                        "struct Pair @ { value: i32; }\n"
                                        "enum Code: u8 @ { ok = 1, }\n"
                                        "extern c @ {\n"
                                        "  opaque struct Handle @;\n"
                                        "  @name(\"puts\")\n"
                                        "  fn puts(s: *const u8) -> i32 @name(\"again\");\n"
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
    expect_contains(messages, "function decorators must appear before 'fn'");
    expect_contains(messages, "expected '{' after impl type");
    expect_contains(messages, "expected ',' or ')' after parameter");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedCoreSeparators)
{
    constexpr base::SourceId PARSER_TEST_CORE_SEPARATOR_RECOVERY_SOURCE_ID{28};
    constexpr std::string_view source = "module parser.core_separator_recovery @;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedGenericSeparators)
{
    expect_parse_error("module parser.generic_bound_recovery;\n"
                       "struct Box[T: Copy] { value: T; }\n",
        "generic bounds are not part of M2 syntax");
    expect_parse_error("module parser.generic_param_recovery;\n"
                       "fn id[T U](value: T) -> T { return value; }\n",
        "expected ',' or ']' after generic parameter");
    expect_parse_error("module parser.generic_type_arg_recovery;\n"
                       "type Bad = Pair[i32 bool];\n",
        "expected ',' or ']' after generic type argument");
    expect_parse_error("module parser.generic_expr_arg_recovery;\n"
                       "fn main() -> i32 { return id[i32 bool](1); }\n",
        "expected ',' or ']' after generic type argument");
    expect_parse_error(
        "module parser.generic_struct_literal_arg_recovery;\n"
        "fn main() -> i32 { let value = Pair[i32 bool] { first: 1, second: true }; return value.first; }\n",
        "expected ',' or ']' after generic type argument");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedControlSeparators)
{
    constexpr base::SourceId PARSER_TEST_CONTROL_SEPARATOR_RECOVERY_SOURCE_ID{29};
    constexpr std::string_view source = "module parser.control_separator_recovery;\n"
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

TEST(CoreUnit, ParserRecoveryHandlesMalformedOpeningDelimiters)
{
    constexpr base::SourceId PARSER_TEST_OPENING_DELIMITER_RECOVERY_SOURCE_ID{30};
    constexpr std::string_view source = "module parser.opening_delimiter_recovery;\n"
                                        "extern c {\n"
                                        "  @name @(\"puts\")\n"
                                        "  fn puts(s: *const u8) -> i32;\n"
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
    expect_contains(messages, "expected '(' after function decorator");
    expect_contains(messages, "expected '(' after function name");
    expect_contains(messages, "expected '[' after cast builtin");
    expect_contains(messages, "expected '[' after ptrat");
    expect_contains(messages, "expected expression");
}

TEST(CoreUnit, ParserRecoveryHandlesMalformedIdentifiers)
{
    constexpr base::SourceId PARSER_TEST_IDENTIFIER_RECOVERY_SOURCE_ID{31};
    constexpr std::string_view source = "module parser.identifier_recovery;\n"
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

TEST(CoreUnit, ParserCoversShiftAndScopedEnumRegressions)
{
    constexpr std::string_view source = "module parser.shift_enum;\n"
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
    ASSERT_EQ(module.exprs.kind(call_stmt.init.value), syntax::ExprKind::try_expr);
    const syntax::TryExprPayload* const try_expr = module.exprs.try_payload(call_stmt.init.value);
    ASSERT_NE(try_expr, nullptr);
    ASSERT_TRUE(syntax::is_valid(try_expr->operand));
    ASSERT_EQ(module.exprs.kind(try_expr->operand.value), syntax::ExprKind::call);
    const syntax::CallExprPayload* const call = module.exprs.call_payload(try_expr->operand.value);
    ASSERT_NE(call, nullptr);
    ASSERT_TRUE(syntax::is_valid(call->callee));
    ASSERT_EQ(module.exprs.kind(call->callee.value), syntax::ExprKind::field);
    const syntax::FieldExprPayload* const field = module.exprs.field_payload(call->callee.value);
    ASSERT_NE(field, nullptr);
    EXPECT_EQ(field->field_name, "ok");
    ASSERT_TRUE(syntax::is_valid(field->object));
    ASSERT_EQ(module.exprs.kind(field->object.value), syntax::ExprKind::name);
    const syntax::NameExprPayload* const result_name = module.exprs.name_payload(field->object.value);
    ASSERT_NE(result_name, nullptr);
    EXPECT_EQ(result_name->text, "ResultI32");
    ASSERT_EQ(call->args.size(), 1U);

    const syntax::StmtNode& literal_stmt = module.stmts[body.statements[1].value];
    ASSERT_TRUE(syntax::is_valid(literal_stmt.init));
    ASSERT_EQ(module.exprs.kind(literal_stmt.init.value), syntax::ExprKind::struct_literal);
    const syntax::StructLiteralExprPayload* const literal =
        module.exprs.struct_literal_payload(literal_stmt.init.value);
    ASSERT_NE(literal, nullptr);
    ASSERT_TRUE(syntax::is_valid(literal->object));
    ASSERT_EQ(module.exprs.kind(literal->object.value), syntax::ExprKind::name);
    const syntax::NameExprPayload* const literal_type = module.exprs.name_payload(literal->object.value);
    ASSERT_NE(literal_type, nullptr);
    EXPECT_EQ(literal_type->text, "Wrap");
    ASSERT_EQ(literal->field_inits.size(), 1U);
    EXPECT_EQ(literal->field_inits.front().name, "value");

    const syntax::StmtNode& shifted_stmt = module.stmts[body.statements[2].value];
    ASSERT_TRUE(syntax::is_valid(shifted_stmt.init));
    ASSERT_EQ(module.exprs.kind(shifted_stmt.init.value), syntax::ExprKind::binary);
    const syntax::BinaryExprPayload* const shifted = module.exprs.binary_payload(shifted_stmt.init.value);
    ASSERT_NE(shifted, nullptr);
    EXPECT_EQ(shifted->op, syntax::BinaryOp::shr);
}

TEST(CoreUnit, ParserPreservesBinaryPrecedenceAndLeftAssociativity)
{
    constexpr std::string_view source = "module parser.precedence;\n"
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
    ASSERT_EQ(module.exprs.kind(chain_stmt.init.value), syntax::ExprKind::binary);
    const syntax::BinaryExprPayload* const chain = module.exprs.binary_payload(chain_stmt.init.value);
    ASSERT_NE(chain, nullptr);
    EXPECT_EQ(chain->op, syntax::BinaryOp::sub);
    ASSERT_TRUE(syntax::is_valid(chain->lhs));
    ASSERT_EQ(module.exprs.kind(chain->lhs.value), syntax::ExprKind::binary);
    const syntax::BinaryExprPayload* const chain_lhs = module.exprs.binary_payload(chain->lhs.value);
    ASSERT_NE(chain_lhs, nullptr);
    EXPECT_EQ(chain_lhs->op, syntax::BinaryOp::sub);

    const syntax::StmtNode& mixed_stmt = module.stmts[body.statements[1].value];
    ASSERT_TRUE(syntax::is_valid(mixed_stmt.init));
    ASSERT_EQ(module.exprs.kind(mixed_stmt.init.value), syntax::ExprKind::binary);
    const syntax::BinaryExprPayload* const mixed = module.exprs.binary_payload(mixed_stmt.init.value);
    ASSERT_NE(mixed, nullptr);
    EXPECT_EQ(mixed->op, syntax::BinaryOp::logical_or);
    ASSERT_TRUE(syntax::is_valid(mixed->lhs));
    ASSERT_EQ(module.exprs.kind(mixed->lhs.value), syntax::ExprKind::binary);
    const syntax::BinaryExprPayload* const logical_and = module.exprs.binary_payload(mixed->lhs.value);
    ASSERT_NE(logical_and, nullptr);
    EXPECT_EQ(logical_and->op, syntax::BinaryOp::logical_and);
    ASSERT_TRUE(syntax::is_valid(logical_and->lhs));
    ASSERT_EQ(module.exprs.kind(logical_and->lhs.value), syntax::ExprKind::binary);
    const syntax::BinaryExprPayload* const comparison = module.exprs.binary_payload(logical_and->lhs.value);
    ASSERT_NE(comparison, nullptr);
    EXPECT_EQ(comparison->op, syntax::BinaryOp::less);
    ASSERT_TRUE(syntax::is_valid(comparison->lhs));
    ASSERT_EQ(module.exprs.kind(comparison->lhs.value), syntax::ExprKind::binary);
    const syntax::BinaryExprPayload* const addition = module.exprs.binary_payload(comparison->lhs.value);
    ASSERT_NE(addition, nullptr);
    EXPECT_EQ(addition->op, syntax::BinaryOp::add);
    ASSERT_TRUE(syntax::is_valid(addition->rhs));
    ASSERT_EQ(module.exprs.kind(addition->rhs.value), syntax::ExprKind::binary);
    const syntax::BinaryExprPayload* const multiplication = module.exprs.binary_payload(addition->rhs.value);
    ASSERT_NE(multiplication, nullptr);
    EXPECT_EQ(multiplication->op, syntax::BinaryOp::mul);
}

TEST(CoreUnit, ParserHandlesLongOperatorAndTypePrefixChainsIteratively)
{
    constexpr std::string_view source = "module parser.deep_prefix_chains;\n"
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
        ASSERT_EQ(module.exprs.kind(unary.value), syntax::ExprKind::unary);
        const syntax::UnaryExprPayload* const node = module.exprs.unary_payload(unary.value);
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->op, syntax::UnaryOp::numeric_negate);
        unary = node->operand;
    }
    ASSERT_TRUE(syntax::is_valid(unary));
    EXPECT_EQ(module.exprs.kind(unary.value), syntax::ExprKind::integer_literal);

    const syntax::StmtNode& binary_stmt = module.stmts[body.statements[1].value];
    ASSERT_TRUE(syntax::is_valid(binary_stmt.init));
    syntax::ExprId binary = binary_stmt.init;
    for (base::usize depth = 0; depth < PARSER_TEST_DEEP_PREFIX_CHAIN_DEPTH; ++depth) {
        ASSERT_TRUE(syntax::is_valid(binary));
        ASSERT_EQ(module.exprs.kind(binary.value), syntax::ExprKind::binary);
        const syntax::BinaryExprPayload* const node = module.exprs.binary_payload(binary.value);
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->op, syntax::BinaryOp::add);
        binary = node->lhs;
    }
    ASSERT_TRUE(syntax::is_valid(binary));
    EXPECT_EQ(module.exprs.kind(binary.value), syntax::ExprKind::integer_literal);
}

TEST(CoreUnit, ParserHandlesLongBinaryChainsWithoutRecursiveDescent)
{
    std::string source = "module parser.long_binary_chain;\nfn main() -> i32 {\n  return ";
    for (base::usize index = 0; index < PARSER_TEST_LONG_BINARY_TERM_COUNT; ++index) {
        if (index != 0) {
            source += " + ";
        }
        source += "1";
    }
    source += ";\n}\n";

    const syntax::AstModule module = parse_success(source);
    EXPECT_GT(module.exprs.size(), PARSER_TEST_LONG_BINARY_TERM_COUNT);
}

TEST(CoreUnit, ParserRejectsExcessivelyNestedGroupedExpressionsWithoutCrashing)
{
    std::string source = "module parser.deep_grouped_expression;\nfn main() -> i32 {\n  return ";
    source.append(PARSER_TEST_EXPRESSION_NESTING_LIMIT_DEPTH, '(');
    source += "1";
    source.append(PARSER_TEST_EXPRESSION_NESTING_LIMIT_DEPTH, ')');
    source += ";\n}\n";

    expect_parse_error(source, "expression nesting exceeds M2 parser limit");
}

TEST(CoreUnit, ParserRejectsExcessivelyNestedTypesWithoutCrashing)
{
    std::string source = "module parser.deep_type_nesting;\ntype Deep = ";
    source.append(PARSER_TEST_TYPE_NESTING_LIMIT_DEPTH, '(');
    source += "i32";
    source.append(PARSER_TEST_TYPE_NESTING_LIMIT_DEPTH, ')');
    source += ";\n";

    expect_parse_error(source, "type nesting exceeds M2 parser limit");
}

TEST(CoreUnit, ParserRejectsExcessivelyNestedGenericTypeArgumentsWithoutCrashing)
{
    std::string source = "module parser.deep_generic_type_nesting;\ntype Deep = ";
    for (base::usize depth = 0; depth < PARSER_TEST_TYPE_NESTING_LIMIT_DEPTH; ++depth) {
        source += "Box[";
    }
    source += "i32";
    source.append(PARSER_TEST_TYPE_NESTING_LIMIT_DEPTH, ']');
    source += ";\n";

    expect_parse_error(source, "type nesting exceeds M2 parser limit");
}

TEST(CoreUnit, ParserRejectsExcessivelyNestedPatternsWithoutCrashing)
{
    std::string source = "module parser.deep_pattern_nesting;\nfn main() -> i32 {\n  let ";
    source.append(PARSER_TEST_PATTERN_NESTING_LIMIT_DEPTH, '(');
    source += "value";
    source.append(PARSER_TEST_PATTERN_NESTING_LIMIT_DEPTH, ')');
    source += " = 1;\n  return value;\n}\n";

    expect_parse_error(source, "pattern nesting exceeds M2 parser limit");
}

TEST(CoreUnit, ParserParsesReferenceTypesAndMutableReferenceExpression)
{
    constexpr std::string_view source = "module parser.references;\n"
                                        "fn borrow(shared: &i32, unique: &mut i32) -> &i32 {\n"
                                        "  var value: i32 = 1;\n"
                                        "  let unique_ref: &mut i32 = &mut value;\n"
                                        "  return &value;\n"
                                        "}\n";
    const syntax::AstModule module = parse_success(source);

    const syntax::ItemNode* borrow = find_item(module, "borrow");
    ASSERT_NE(borrow, nullptr);
    ASSERT_EQ(borrow->params.size(), 2U);
    ASSERT_TRUE(syntax::is_valid(borrow->params[0].type));
    const syntax::TypeNode& shared = module.types[borrow->params[0].type.value];
    ASSERT_EQ(shared.kind, syntax::TypeKind::reference);
    EXPECT_EQ(shared.pointer_mutability, syntax::PointerMutability::const_);
    ASSERT_TRUE(syntax::is_valid(borrow->params[1].type));
    const syntax::TypeNode& unique = module.types[borrow->params[1].type.value];
    ASSERT_EQ(unique.kind, syntax::TypeKind::reference);
    EXPECT_EQ(unique.pointer_mutability, syntax::PointerMutability::mut);
    ASSERT_TRUE(syntax::is_valid(borrow->return_type));
    EXPECT_EQ(module.types[borrow->return_type.value].kind, syntax::TypeKind::reference);

    ASSERT_TRUE(syntax::is_valid(borrow->body));
    const syntax::StmtNode& body = module.stmts[borrow->body.value];
    ASSERT_GE(body.statements.size(), 2U);
    const syntax::StmtNode& local = module.stmts[body.statements[1].value];
    ASSERT_TRUE(syntax::is_valid(local.init));
    ASSERT_EQ(module.exprs.kind(local.init.value), syntax::ExprKind::unary);
    const syntax::UnaryExprPayload* const init = module.exprs.unary_payload(local.init.value);
    ASSERT_NE(init, nullptr);
    EXPECT_EQ(init->op, syntax::UnaryOp::address_of_mut);
}

TEST(CoreUnit, ParserCoversCompoundAssignmentStatements)
{
    constexpr std::string_view source = "module parser.assign_ops;\n"
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

TEST(CoreUnit, ParserParsesForRangeStatements)
{
    constexpr std::string_view source = "module parser.for_range;\n"
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

TEST(CoreUnit, ParserReportsMalformedForRangeSyntax)
{
    expect_parse_error("module parser.for_range_separator;\n"
                       "fn main() -> i32 {\n"
                       "  for i in range(0 3) {\n"
                       "  }\n"
                       "  return 0;\n"
                       "}\n",
        "expected ',' or ')' after range argument");
    expect_parse_error("module parser.for_range_missing_args;\n"
                       "fn main() -> i32 {\n"
                       "  for i in range {\n"
                       "  }\n"
                       "  return 0;\n"
                       "}\n",
        "expected '(' after range");
    expect_parse_error("module parser.for_range_too_many_args;\n"
                       "fn main() -> i32 {\n"
                       "  for i in range(0, 3, 1, 1) {\n"
                       "  }\n"
                       "  return 0;\n"
                       "}\n",
        "range expects 1 to 3 arguments");
}

TEST(CoreUnit, ParserRejectsIncrementAndDecrementSyntax)
{
    expect_parse_error("module parser.increment_syntax;\n"
                       "fn main() -> i32 {\n"
                       "  var value: i32 = 0;\n"
                       "  value++;\n"
                       "  return value;\n"
                       "}\n",
        "increment operator is not supported; use '+= 1'");
    expect_parse_error("module parser.increment_expr_syntax;\n"
                       "fn main() -> i32 {\n"
                       "  var value: i32 = 0;\n"
                       "  let old: i32 = value++;\n"
                       "  return old;\n"
                       "}\n",
        "increment operator is not supported; use '+= 1'");
    expect_parse_error("module parser.decrement_syntax;\n"
                       "fn main() -> i32 {\n"
                       "  var value: i32 = 0;\n"
                       "  value--;\n"
                       "  return value;\n"
                       "}\n",
        "decrement operator is not supported; use '-= 1'");
    expect_parse_error("module parser.prefix_increment_syntax;\n"
                       "fn main() -> i32 {\n"
                       "  var value: i32 = 0;\n"
                       "  ++value;\n"
                       "  return value;\n"
                       "}\n",
        "increment operator is not supported; use '+= 1'");
    expect_parse_error("module parser.prefix_decrement_syntax;\n"
                       "fn main() -> i32 {\n"
                       "  var value: i32 = 0;\n"
                       "  --value;\n"
                       "  return value;\n"
                       "}\n",
        "decrement operator is not supported; use '-= 1'");
}

TEST(CoreUnit, ParserRejectsStructLiteralInControlConditions)
{
    expect_parse_error("module parser.condition_struct_literal;\n"
                       "struct Flag { value: bool; }\n"
                       "fn main() -> i32 {\n"
                       "  if Flag { value: true } { return 1; }\n"
                       "  return 0;\n"
                       "}\n",
        "expected ';' after expression statement");
}

TEST(CoreUnit, ParserReportsIncompleteExpressionsWithoutCrashing)
{
    expect_parse_error("module parser.incomplete_struct_literal;\n"
                       "struct Pair { value: i32; }\n"
                       "fn main() -> i32 {\n"
                       "  let value = Pair { value: };\n"
                       "  return 0;\n"
                       "}\n",
        "expected expression");
    expect_parse_error("module parser.incomplete_binary;\n"
                       "fn main() -> i32 {\n"
                       "  let value = (1 + );\n"
                       "  return 0;\n"
                       "}\n",
        "expected expression");
}

TEST(CoreUnit, ParserCoversAbiNamesAndArrayRadicesDirectly)
{
    constexpr std::string_view source = "module parser.abi_radix;\n"
                                        "extern c { @name(\"puts\") fn c_puts(s: *const u8) -> i32; }\n"
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

TEST(CoreUnit, ParserCoversAdditionalDiagnosticBranches)
{
    expect_parse_error("module parser.private_impl;\n"
                       "struct Box {}\n"
                       "priv impl Box { fn value(self: Box) -> i32 { return 1; } }\n",
        "impl block cannot be private");
    expect_parse_error("module parser.private_extern;\n"
                       "priv extern c { fn puts(s: *const u8) -> i32; }\n",
        "extern block cannot be private");
    expect_parse_error("module parser.private_export;\n"
                       "priv export c fn main() -> i32 { return 0; }\n",
        "exported C function cannot be private");
    expect_parse_error("module parser.bad_export;\n"
                       "export c;\n",
        "expected function declaration after 'export c'");
    expect_parse_error("module parser.bad_export_abi;\n"
                       "export x fn main() -> i32 { return 0; }\n",
        "expected 'c' after 'export'");
    expect_parse_error("module parser.bad_extern_abi;\n"
                       "extern x { fn puts(s: *const u8) -> i32; }\n",
        "expected 'c' after 'extern'");
    expect_parse_error("module parser.bad_function_type_abi;\n"
                       "type Bad = extern x fn(i32) -> i32;\n",
        "expected 'c' after 'extern' in function type");
    expect_parse_error("module parser.unsupported_visibility_scope;\n"
                       "pub(crate) fn package_only() -> i32 { return 0; }\n",
        "unsupported visibility scope; only pub(package) is supported");
    expect_parse_error("module parser.missing_visibility_scope;\n"
                       "pub() fn package_only() -> i32 { return 0; }\n",
        "expected visibility scope 'package'");
    expect_parse_error("module parser.recovered_function_type_abi;\n"
                       "type Bad = extern @ c fn(i32) -> i32;\n",
        "expected 'c' after 'extern' in function type");
    expect_parse_error("module parser.bad_abi;\n"
                       "@wrong(\"x\")\n"
                       "fn f() -> i32 { return 0; }\n",
        "expected function decorator 'name' or 'borrow'");
    expect_parse_error("module parser.postfix_decorator;\n"
                       "fn f() -> i32 @name(\"f\") { return 0; }\n",
        "function decorators must appear before 'fn'");
    expect_parse_error("module parser.bad_pointer;\n"
                       "type Bad = *i32;\n",
        "expected 'mut' or 'const' after '*'");
    expect_parse_error("module parser.bad_extern_item;\n"
                       "extern c { const answer: i32 = 1; }\n",
        "expected extern item");
    expect_parse_error("module parser.bad_impl_item;\n"
                       "struct Box {}\n"
                       "impl Box { const answer: i32 = 1; }\n",
        "expected function declaration in impl block");
    expect_parse_error("module parser.bad_type;\n"
                       "fn f(value: ) -> i32 { return 0; }\n",
        "expected type");
    expect_parse_error("module parser.bad_import;\n"
                       "import c.;\n",
        "expected identifier after '.'");
    expect_parse_diagnostic("module parser.chained_comparison;\n"
                            "fn main(a: i32, b: i32, c: i32) -> bool {\n"
                            "  return a < b < c;\n"
                            "}\n",
        "comparison operators are non-associative");
    expect_parse_diagnostic("module parser.chained_equality;\n"
                            "fn main(a: i32, b: i32, c: i32) -> bool {\n"
                            "  return a == b == c;\n"
                            "}\n",
        "comparison operators are non-associative");
}

TEST(CoreUnit, ParserRecoveryPredicateTablesCoverStartAndBoundarySets)
{
    using syntax::TokenKind;

    const auto expect_true_all = [](const auto predicate, const std::initializer_list<TokenKind> kinds) {
        for (const TokenKind kind : kinds) {
            EXPECT_TRUE(predicate(kind)) << static_cast<int>(kind);
        }
    };
    const auto expect_false_on = [](const auto predicate, const TokenKind kind) {
        EXPECT_FALSE(predicate(kind)) << static_cast<int>(kind);
    };

    expect_true_all(parse::detail::token_starts_item,
        {
            TokenKind::r_brace,
            TokenKind::kw_fn,
            TokenKind::kw_struct,
            TokenKind::kw_enum,
            TokenKind::kw_trait,
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
        });
    expect_false_on(parse::detail::token_starts_item, TokenKind::identifier);

    expect_true_all(parse::detail::token_starts_expression,
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
            TokenKind::kw_sliceptr,
            TokenKind::kw_slicelen,
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
        });
    expect_false_on(parse::detail::token_starts_expression, TokenKind::kw_module);

    EXPECT_TRUE(parse::detail::token_starts_statement(TokenKind::identifier));
    expect_true_all(parse::detail::token_starts_statement,
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
        });
    expect_false_on(parse::detail::token_starts_statement, TokenKind::semicolon);

    expect_true_all(parse::detail::token_starts_non_expression_statement,
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
        });
    expect_false_on(parse::detail::token_starts_non_expression_statement, TokenKind::identifier);

    expect_true_all(parse::detail::token_starts_type,
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
        });
    expect_false_on(parse::detail::token_starts_type, TokenKind::kw_module);

    expect_true_all(parse::token_starts_match_arm,
        {
            TokenKind::identifier,
            TokenKind::integer_literal,
            TokenKind::kw_true,
            TokenKind::kw_false,
            TokenKind::dot,
            TokenKind::l_paren,
            TokenKind::l_bracket,
        });
    expect_false_on(parse::token_starts_match_arm, TokenKind::kw_let);

    expect_true_all(parse::token_starts_struct_field, {TokenKind::identifier});
    expect_false_on(parse::token_starts_struct_field, TokenKind::kw_fn);

    expect_true_all(parse::token_starts_parameter, {TokenKind::identifier, TokenKind::ellipsis});
    expect_false_on(parse::token_starts_parameter, TokenKind::kw_fn);

    expect_true_all(
        parse::token_starts_struct_decl_field, {TokenKind::identifier, TokenKind::kw_pub, TokenKind::kw_priv});
    expect_false_on(parse::token_starts_struct_decl_field, TokenKind::kw_fn);

    expect_true_all(parse::token_starts_enum_case, {TokenKind::identifier});
    expect_false_on(parse::token_starts_enum_case, TokenKind::kw_fn);

    expect_true_all(parse::token_starts_path_segment, {TokenKind::identifier, TokenKind::kw_str});
    expect_false_on(parse::token_starts_path_segment, TokenKind::kw_fn);

    expect_true_all(parse::detail::token_ends_match_arm, {TokenKind::comma, TokenKind::r_brace});
    expect_false_on(parse::detail::token_ends_match_arm, TokenKind::identifier);

    expect_true_all(parse::detail::token_ends_call_argument,
        {
            TokenKind::comma,
            TokenKind::r_paren,
            TokenKind::semicolon,
            TokenKind::r_bracket,
            TokenKind::r_brace,
        });
    expect_false_on(parse::detail::token_ends_call_argument, TokenKind::identifier);

    expect_true_all(parse::detail::token_ends_struct_field,
        {
            TokenKind::comma,
            TokenKind::r_brace,
            TokenKind::semicolon,
            TokenKind::r_paren,
            TokenKind::r_bracket,
        });
    expect_false_on(parse::detail::token_ends_struct_field, TokenKind::identifier);

    expect_true_all(parse::detail::token_ends_parameter,
        {
            TokenKind::comma,
            TokenKind::r_paren,
            TokenKind::arrow,
            TokenKind::l_brace,
            TokenKind::semicolon,
            TokenKind::r_brace,
        });
    expect_false_on(parse::detail::token_ends_parameter, TokenKind::identifier);

    expect_true_all(parse::detail::token_ends_struct_decl_field,
        {
            TokenKind::semicolon,
            TokenKind::comma,
            TokenKind::r_brace,
            TokenKind::kw_fn,
            TokenKind::kw_struct,
            TokenKind::kw_enum,
            TokenKind::kw_trait,
            TokenKind::kw_impl,
            TokenKind::kw_extern,
            TokenKind::kw_export,
        });
    expect_false_on(parse::detail::token_ends_struct_decl_field, TokenKind::identifier);

    expect_true_all(parse::detail::token_ends_enum_case,
        {
            TokenKind::comma,
            TokenKind::semicolon,
            TokenKind::r_brace,
            TokenKind::kw_fn,
            TokenKind::kw_struct,
            TokenKind::kw_enum,
            TokenKind::kw_trait,
            TokenKind::kw_impl,
            TokenKind::kw_extern,
            TokenKind::kw_export,
        });
    expect_false_on(parse::detail::token_ends_enum_case, TokenKind::identifier);

    expect_true_all(parse::detail::token_ends_builtin_argument,
        {
            TokenKind::comma,
            TokenKind::r_paren,
            TokenKind::semicolon,
            TokenKind::r_bracket,
            TokenKind::r_brace,
        });
    expect_false_on(parse::detail::token_ends_builtin_argument, TokenKind::identifier);

    expect_true_all(parse::detail::token_ends_generic_type_argument,
        {
            TokenKind::comma,
            TokenKind::r_bracket,
            TokenKind::r_paren,
            TokenKind::l_brace,
            TokenKind::r_brace,
            TokenKind::semicolon,
        });
    expect_false_on(parse::detail::token_ends_generic_type_argument, TokenKind::identifier);

    expect_true_all(parse::detail::token_ends_generic_parameter,
        {
            TokenKind::comma,
            TokenKind::r_bracket,
            TokenKind::l_paren,
            TokenKind::l_brace,
            TokenKind::r_brace,
            TokenKind::semicolon,
        });
    expect_false_on(parse::detail::token_ends_generic_parameter, TokenKind::identifier);
}

TEST(CoreUnit, ParserPartRangeReaderCoversRangeFallbacks)
{
    constexpr std::string_view source = "module parser.ranges;\n"
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

    const syntax::ExprId expr_id =
        reader.module().push_literal_expr(syntax::ExprKind::integer_literal, begin_range, "0");
    const syntax::StmtId stmt_id = reader.module().push_stmt([&] {
        syntax::StmtNode stmt;
        stmt.kind = syntax::StmtKind::expr;
        stmt.range = end_range;
        return stmt;
    }());
    const syntax::TypeId type_id = reader.module().push_type([&] {
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::primitive;
        type.range = begin_range;
        type.primitive = syntax::PrimitiveTypeKind::i32;
        return type;
    }());
    const syntax::PatternId pattern_id = reader.module().push_pattern([&] {
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::wildcard;
        pattern.range = end_range;
        return pattern;
    }());

    const base::SourceRange merged = reader.merge(begin_range, end_range);
    EXPECT_EQ(merged.source.value, source_id.value);
    EXPECT_EQ(merged.begin, begin_range.begin);
    EXPECT_EQ(merged.end, end_range.end);

    const auto expect_range = [](const base::SourceRange& actual, const base::SourceRange& expected) {
        EXPECT_EQ(actual.source.value, expected.source.value);
        EXPECT_EQ(actual.begin, expected.begin);
        EXPECT_EQ(actual.end, expected.end);
    };

    expect_range(reader.expr_range_or(expr_id, fallback_range), begin_range);
    expect_range(reader.expr_range_or(syntax::INVALID_EXPR_ID, fallback_range), fallback_range);
    expect_range(
        reader.expr_range_or(syntax::ExprId{expr_id.value + PARSER_RANGE_TEST_OUT_OF_RANGE_OFFSET}, fallback_range),
        fallback_range);

    expect_range(reader.stmt_range_or(stmt_id, fallback_range), end_range);
    expect_range(reader.stmt_range_or(syntax::INVALID_STMT_ID, fallback_range), fallback_range);
    expect_range(
        reader.stmt_range_or(syntax::StmtId{stmt_id.value + PARSER_RANGE_TEST_OUT_OF_RANGE_OFFSET}, fallback_range),
        fallback_range);

    expect_range(reader.type_range_or(type_id, fallback_range), begin_range);
    expect_range(reader.type_range_or(syntax::INVALID_TYPE_ID, fallback_range), fallback_range);
    expect_range(
        reader.type_range_or(syntax::TypeId{type_id.value + PARSER_RANGE_TEST_OUT_OF_RANGE_OFFSET}, fallback_range),
        fallback_range);

    expect_range(reader.pattern_range_or(pattern_id, fallback_range), end_range);
    expect_range(reader.pattern_range_or(syntax::INVALID_PATTERN_ID, fallback_range), fallback_range);
    expect_range(reader.pattern_range_or(
                     syntax::PatternId{pattern_id.value + PARSER_RANGE_TEST_OUT_OF_RANGE_OFFSET}, fallback_range),
        fallback_range);
}

TEST(CoreUnit, ParserRecoversBuiltinArgumentSeparators)
{
    constexpr std::string_view source = "module parser.builtin_recovery;\n"
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

TEST(CoreUnit, ParserM2GenericSyntax)
{
    constexpr std::string_view source =
        "module parser.generics;\n"
        "type Alias[T] = T;\n"
        "struct Box[T] { value: T; }\n"
        "struct Pair[A, B] { first: A; second: B; }\n"
        "enum Maybe[T]: u8 { some(T) = 1, none = 2, }\n"
        "type Unary[T] = fn(T) -> T;\n"
        "extern c { fn cfunc(value: i32, ...) -> i32; }\n"
        "fn id[T](x: T) -> T { return x; }\n"
        "fn keep_fn[T](value: Unary[T]) -> Unary[T] { return id[Unary[T]](value); }\n"
        "fn keep_ref[T](value: &T) -> &T { return id[&T](value); }\n"
        "fn keep_mut_ref[T](value: &mut T) -> &mut T { return id[&mut T](value); }\n"
        "fn keep_tuple[T](value: (T, (bool, i32))) -> (T, (bool, i32)) {\n"
        "  return id[(T, (bool, i32))](value);\n"
        "}\n"
        "fn fn_arg(value: fn(i32) -> i32) -> fn(i32) -> i32 { return id[fn(i32) -> i32](value); }\n"
        "fn unsafe_arg(value: unsafe fn() -> i32) -> unsafe fn() -> i32 { return id[unsafe fn() -> i32](value); }\n"
        "fn extern_arg(value: extern c fn(i32, ...) -> i32) -> extern c fn(i32, ...) -> i32 {\n"
        "  return id[extern c fn(i32, ...) -> i32](value);\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let a: Box[i32] = Box[i32] { value: id[i32](1) };\n"
        "  let p: Pair[i32, bool] = Pair[i32, bool] { first: a.value, second: true };\n"
        "  let values: [1]i32 = [1];\n"
        "  let f = values[0];\n"
        "  return p.first;\n"
        "}\n";

    const syntax::AstModule module = parse_success(source);
    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "type_alias Alias[T]",
            "alias T",
            "struct Box[T]",
            "field priv value : T",
            "struct Pair[A, B]",
            "enum Maybe[T]",
            "case some(T) = 1",
            "type_alias Unary[T]",
            "fn id[T]",
            "fn keep_fn[T]",
            "fn keep_ref[T]",
            "fn keep_mut_ref[T]",
            "fn keep_tuple[T]",
            "param x : T",
            "return T",
            "Box[i32]",
            "Pair[i32, bool]",
            "Unary[T]",
            "generic_apply[Unary[T]]",
            "generic_apply[&T]",
            "generic_apply[&mut T]",
            "generic_apply[(T, (bool, i32))]",
            "generic_apply[i32]",
        });
}

TEST(CoreUnit, ParserKeepsNameIndexBeforeFieldAsValueIndex)
{
    constexpr std::string_view source = "module parser.index_field;\n"
                                        "struct Box { value: i32; }\n"
                                        "fn main() -> i32 {\n"
                                        "  let boxes: [1]Box = [Box { value: 41 }];\n"
                                        "  let index: usize = 0;\n"
                                        "  return boxes[index].value;\n"
                                        "}\n";

    const syntax::AstModule module = parse_success(source);
    const syntax::ItemNode* const main = find_item(module, "main");
    ASSERT_NE(main, nullptr);
    ASSERT_TRUE(syntax::is_valid(main->body));
    const syntax::StmtNode& body = module.stmts[main->body.value];
    ASSERT_GE(body.statements.size(), 3U);

    const syntax::StmtNode& return_stmt = module.stmts[body.statements[2].value];
    ASSERT_EQ(return_stmt.kind, syntax::StmtKind::return_);
    ASSERT_TRUE(syntax::is_valid(return_stmt.return_value));
    ASSERT_EQ(module.exprs.kind(return_stmt.return_value.value), syntax::ExprKind::field);
    const syntax::FieldExprPayload* const field = module.exprs.field_payload(return_stmt.return_value.value);
    ASSERT_NE(field, nullptr);
    EXPECT_EQ(field->field_name, "value");
    ASSERT_TRUE(syntax::is_valid(field->object));
    ASSERT_EQ(module.exprs.kind(field->object.value), syntax::ExprKind::index);

    const syntax::IndexExprPayload* const index = module.exprs.index_payload(field->object.value);
    ASSERT_NE(index, nullptr);
    ASSERT_TRUE(syntax::is_valid(index->index));
    ASSERT_EQ(module.exprs.kind(index->index.value), syntax::ExprKind::name);
    const syntax::NameExprPayload* const index_name = module.exprs.name_payload(index->index.value);
    ASSERT_NE(index_name, nullptr);
    EXPECT_EQ(index_name->text, "index");
}

TEST(CoreUnit, ParserKeepsGenericTypeSelectorWithGenericParamBeforeField)
{
    constexpr std::string_view source = "module parser.generic_selector;\n"
                                        "enum Maybe[T]: u8 { some(T) = 1, none = 2, }\n"
                                        "fn make_some[T](value: T) -> Maybe[T] {\n"
                                        "  return Maybe[T].some(value);\n"
                                        "}\n";

    const syntax::AstModule module = parse_success(source);
    const syntax::ItemNode* const make_some = find_item(module, "make_some");
    ASSERT_NE(make_some, nullptr);
    ASSERT_TRUE(syntax::is_valid(make_some->body));
    const syntax::StmtNode& body = module.stmts[make_some->body.value];
    ASSERT_EQ(body.statements.size(), 1U);

    const syntax::StmtNode& return_stmt = module.stmts[body.statements.front().value];
    ASSERT_EQ(return_stmt.kind, syntax::StmtKind::return_);
    ASSERT_TRUE(syntax::is_valid(return_stmt.return_value));
    ASSERT_EQ(module.exprs.kind(return_stmt.return_value.value), syntax::ExprKind::call);
    const syntax::CallExprPayload* const call = module.exprs.call_payload(return_stmt.return_value.value);
    ASSERT_NE(call, nullptr);
    ASSERT_TRUE(syntax::is_valid(call->callee));
    ASSERT_EQ(module.exprs.kind(call->callee.value), syntax::ExprKind::field);

    const syntax::FieldExprPayload* const field = module.exprs.field_payload(call->callee.value);
    ASSERT_NE(field, nullptr);
    EXPECT_EQ(field->field_name, "some");
    ASSERT_TRUE(syntax::is_valid(field->object));
    ASSERT_EQ(module.exprs.kind(field->object.value), syntax::ExprKind::generic_apply);
    const syntax::GenericApplyExprPayload* const apply = module.exprs.generic_apply_payload(field->object.value);
    ASSERT_NE(apply, nullptr);
    ASSERT_EQ(apply->type_args.size(), 1U);
}

TEST(CoreUnit, ParserClassifiesBracketSuffixesByExplicitM2Contract)
{
    constexpr std::string_view source = "module parser.bracket_contract;\n"
                                        "enum Maybe[T]: u8 { some(T) = 1, none = 2, }\n"
                                        "struct Box[T] { value: T; }\n"
                                        "fn id[T](value: T) -> T { return value; }\n"
                                        "fn main() -> i32 {\n"
                                        "  let boxes: [1]Box[i32] = [Box[i32] { value: 1 }];\n"
                                        "  let value: usize = 0;\n"
                                        "  let picked = boxes[value].value;\n"
                                        "  let made = Maybe[i32].some(id[i32](picked));\n"
                                        "  let nested: Box[Maybe[i32]] = Box[Maybe[i32]] { value: made };\n"
                                        "  return picked;\n"
                                        "}\n";

    const syntax::AstModule module = parse_success(source);
    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "index",
            "generic_apply[i32]",
            "generic_apply[Maybe[i32]]",
            " .some",
        });

    const syntax::ItemNode* const main = find_item(module, "main");
    ASSERT_NE(main, nullptr);
    ASSERT_TRUE(syntax::is_valid(main->body));
    const syntax::StmtNode& body = module.stmts[main->body.value];
    ASSERT_GE(body.statements.size(), 4U);

    const syntax::StmtNode& picked_stmt = module.stmts[body.statements[2].value];
    ASSERT_TRUE(syntax::is_valid(picked_stmt.init));
    ASSERT_EQ(module.exprs.kind(picked_stmt.init.value), syntax::ExprKind::field);
    const syntax::FieldExprPayload* const picked_field = module.exprs.field_payload(picked_stmt.init.value);
    ASSERT_NE(picked_field, nullptr);
    ASSERT_TRUE(syntax::is_valid(picked_field->object));
    ASSERT_EQ(module.exprs.kind(picked_field->object.value), syntax::ExprKind::index);

    const syntax::StmtNode& made_stmt = module.stmts[body.statements[3].value];
    ASSERT_TRUE(syntax::is_valid(made_stmt.init));
    ASSERT_EQ(module.exprs.kind(made_stmt.init.value), syntax::ExprKind::call);
    const syntax::CallExprPayload* const call = module.exprs.call_payload(made_stmt.init.value);
    ASSERT_NE(call, nullptr);
    ASSERT_TRUE(syntax::is_valid(call->callee));
    ASSERT_EQ(module.exprs.kind(call->callee.value), syntax::ExprKind::field);
    const syntax::FieldExprPayload* const field = module.exprs.field_payload(call->callee.value);
    ASSERT_NE(field, nullptr);
    ASSERT_TRUE(syntax::is_valid(field->object));
    ASSERT_EQ(module.exprs.kind(field->object.value), syntax::ExprKind::generic_apply);
}

TEST(CoreUnit, ParserClassifiesNestedGenericSelectorsAndVoidTypeArgs)
{
    constexpr std::string_view source = "module parser.bracket_postfix_edges;\n"
                                        "struct Inner[T] { value: T; }\n"
                                        "struct Outer[T] { value: T; }\n"
                                        "fn wrap[T](value: i32) -> i32 { return value; }\n"
                                        "fn main() -> i32 {\n"
                                        "  let selected = Outer[Inner[i32]].make;\n"
                                        "  let ignored = wrap[void](0);\n"
                                        "  return ignored;\n"
                                        "}\n";

    const syntax::AstModule module = parse_success(source);
    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "generic_apply[Inner[i32]]",
            "generic_apply[void]",
            " .make",
        });

    const syntax::ItemNode* const main = find_item(module, "main");
    ASSERT_NE(main, nullptr);
    ASSERT_TRUE(syntax::is_valid(main->body));
    const syntax::StmtNode& body = module.stmts[main->body.value];
    ASSERT_GE(body.statements.size(), 2U);

    const syntax::StmtNode& selected_stmt = module.stmts[body.statements[0].value];
    ASSERT_TRUE(syntax::is_valid(selected_stmt.init));
    ASSERT_EQ(module.exprs.kind(selected_stmt.init.value), syntax::ExprKind::field);
    const syntax::FieldExprPayload* const selected_field = module.exprs.field_payload(selected_stmt.init.value);
    ASSERT_NE(selected_field, nullptr);
    ASSERT_TRUE(syntax::is_valid(selected_field->object));
    ASSERT_EQ(module.exprs.kind(selected_field->object.value), syntax::ExprKind::generic_apply);

    const syntax::StmtNode& ignored_stmt = module.stmts[body.statements[1].value];
    ASSERT_TRUE(syntax::is_valid(ignored_stmt.init));
    ASSERT_EQ(module.exprs.kind(ignored_stmt.init.value), syntax::ExprKind::call);
    const syntax::CallExprPayload* const call = module.exprs.call_payload(ignored_stmt.init.value);
    ASSERT_NE(call, nullptr);
    ASSERT_TRUE(syntax::is_valid(call->callee));
    ASSERT_EQ(module.exprs.kind(call->callee.value), syntax::ExprKind::generic_apply);
    const syntax::GenericApplyExprPayload* const apply = module.exprs.generic_apply_payload(call->callee.value);
    ASSERT_NE(apply, nullptr);
    ASSERT_EQ(apply->type_args.size(), 1U);
    ASSERT_EQ(module.types[apply->type_args.front().value].kind, syntax::TypeKind::primitive);
    EXPECT_EQ(module.types[apply->type_args.front().value].primitive, syntax::PrimitiveTypeKind::void_);
}

TEST(CoreUnit, ParserConvertsDeepSelectorGenericArgWithOverflowStorage)
{
    std::string source = "module parser.deep_selector_generic_arg;\n"
                         "fn id[T](value: i32) -> i32 { return value; }\n"
                         "fn main() -> i32 { return id[A";
    for (base::usize depth = 0; depth < PARSER_TEST_DEEP_TYPE_SELECTOR_OVERFLOW_DEPTH; ++depth) {
        source += ".B";
    }
    source += "](0); }\n";

    const syntax::AstModule module = parse_success(source);
    const std::string ast = syntax::dump_ast(module);
    expect_contains(ast, "generic_apply[A.B.B.B.B.B.B.B.B.B.B]");

    const syntax::ItemNode* const main = find_item(module, "main");
    ASSERT_NE(main, nullptr);
    ASSERT_TRUE(syntax::is_valid(main->body));
    const syntax::StmtNode& body = module.stmts[main->body.value];
    ASSERT_EQ(body.statements.size(), 1U);
    const syntax::StmtNode& return_stmt = module.stmts[body.statements.front().value];
    ASSERT_TRUE(syntax::is_valid(return_stmt.return_value));
    ASSERT_EQ(module.exprs.kind(return_stmt.return_value.value), syntax::ExprKind::call);
    const syntax::CallExprPayload* const call = module.exprs.call_payload(return_stmt.return_value.value);
    ASSERT_NE(call, nullptr);
    ASSERT_TRUE(syntax::is_valid(call->callee));
    ASSERT_EQ(module.exprs.kind(call->callee.value), syntax::ExprKind::generic_apply);
    const syntax::GenericApplyExprPayload* const apply = module.exprs.generic_apply_payload(call->callee.value);
    ASSERT_NE(apply, nullptr);
    ASSERT_EQ(apply->type_args.size(), 1U);
    const syntax::TypeNode& type = module.types[apply->type_args.front().value];
    ASSERT_EQ(type.kind, syntax::TypeKind::named);
    ASSERT_EQ(type.scope_parts.size(), PARSER_TEST_DEEP_TYPE_SELECTOR_OVERFLOW_DEPTH);
    EXPECT_EQ(type.name, "B");
}

TEST(CoreUnit, ParserPostfixWhiteBoxRejectsInvalidTypeLikeExpressions)
{
    std::vector<syntax::Token> tokens = probe_tokens();
    DiagnosticSink diagnostics;
    parse::Parser parser(tokens, diagnostics);
    ParserPostfixProbe probe(parser);
    syntax::AstModule& module = probe.module();

    const syntax::ExprId empty_name = module.push_name_expr({}, "");
    const syntax::ExprId invalid_generic =
        module.push_generic_apply_expr({}, syntax::INVALID_EXPR_ID, std::vector<syntax::TypeId>{});
    const syntax::ExprId number = module.push_literal_expr(syntax::ExprKind::integer_literal, {}, "1");
    const syntax::ExprId invalid_field = module.push_field_expr({}, syntax::INVALID_EXPR_ID, "Thing");
    const syntax::ExprId invalid_apply_callee =
        module.push_generic_apply_expr({}, syntax::INVALID_EXPR_ID, std::vector<syntax::TypeId>{});
    const syntax::ExprId invalid_unary =
        module.push_unary_expr(syntax::ExprKind::unary, {}, syntax::UnaryOp::numeric_negate, number);
    const syntax::ExprId invalid_type_unary =
        module.push_unary_expr(syntax::ExprKind::unary, {}, syntax::UnaryOp::address_of, syntax::INVALID_EXPR_ID);

    EXPECT_FALSE(probe.type_like(syntax::INVALID_EXPR_ID));
    EXPECT_FALSE(probe.type_like(empty_name));
    EXPECT_FALSE(probe.type_like(invalid_generic));
    EXPECT_FALSE(probe.type_like(invalid_unary));
    EXPECT_FALSE(syntax::is_valid(probe.convert_type_arg(syntax::INVALID_EXPR_ID, false)));
    EXPECT_FALSE(syntax::is_valid(probe.convert_type_arg(number, false)));
    EXPECT_FALSE(syntax::is_valid(probe.convert_type_arg(invalid_field, false)));
    EXPECT_FALSE(syntax::is_valid(probe.convert_type_arg(invalid_apply_callee, false)));
    EXPECT_FALSE(syntax::is_valid(probe.convert_type_arg(invalid_type_unary, false)));
}

TEST(CoreUnit, BracketSuffixClassifierOwnsTypeArgumentContextDecisions)
{
    std::vector<syntax::Token> tokens = classifier_probe_tokens({syntax::TokenKind::l_paren});
    DiagnosticSink diagnostics;
    parse::Parser parser(tokens, diagnostics);
    BracketSuffixClassifierProbe probe(parser);
    syntax::AstModule& module = probe.module();

    const syntax::ExprId value = module.push_name_expr({}, "Value");
    const syntax::ExprId lower_value = module.push_name_expr({}, "value");
    const std::array<parse::PostfixExprParser::BracketArg, 1> type_like_args{parse::PostfixExprParser::BracketArg{
        value,
        syntax::INVALID_TYPE_ID,
        {},
    }};
    const std::array<parse::PostfixExprParser::BracketArg, 1> expr_like_args{parse::PostfixExprParser::BracketArg{
        lower_value,
        syntax::INVALID_TYPE_ID,
        {},
    }};

    const parse::BracketSuffixDecision generic_call = probe.classify_after_expr(parse::BracketSuffixClassificationInput{
        .base = value,
        .has_type_only_arg = false,
        .args_are_type_like = true,
        .context = parse::ExprContext::normal,
    });
    EXPECT_EQ(generic_call.kind, parse::BracketSuffixKind::generic_apply);
    EXPECT_FALSE(generic_call.report_type_arg_errors);
    EXPECT_TRUE(generic_call.base_is_type_like);
    EXPECT_TRUE(generic_call.args_are_type_like);

    const parse::BracketSuffixDecision rejected_call =
        probe.classify_after_expr(parse::BracketSuffixClassificationInput{
            .base = lower_value,
            .has_type_only_arg = false,
            .args_are_type_like = false,
            .context = parse::ExprContext::normal,
        });
    EXPECT_EQ(rejected_call.kind, parse::BracketSuffixKind::generic_apply);
    EXPECT_TRUE(rejected_call.report_type_arg_errors);
    EXPECT_FALSE(rejected_call.base_is_type_like);
    EXPECT_FALSE(rejected_call.args_are_type_like);

    EXPECT_TRUE(probe.arg_expr_is_type_like(type_like_args.front().expr));
    EXPECT_FALSE(probe.arg_expr_is_type_like(expr_like_args.front().expr));
}

TEST(CoreUnit, BracketSuffixClassifierKeepsIndexWhenSelectorIsValueLike)
{
    std::vector<syntax::Token> tokens = classifier_probe_tokens({syntax::TokenKind::dot});
    DiagnosticSink diagnostics;
    parse::Parser parser(tokens, diagnostics);
    BracketSuffixClassifierProbe probe(parser);
    syntax::AstModule& module = probe.module();

    const syntax::ExprId value = module.push_name_expr({}, "value");
    const parse::BracketSuffixDecision decision = probe.classify_after_expr(parse::BracketSuffixClassificationInput{
        .base = value,
        .has_type_only_arg = false,
        .args_are_type_like = false,
        .context = parse::ExprContext::normal,
    });
    EXPECT_EQ(decision.kind, parse::BracketSuffixKind::index);
    EXPECT_FALSE(decision.report_type_arg_errors);
    EXPECT_FALSE(decision.base_is_type_like);
}

TEST(CoreUnit, BracketSuffixClassifierRecognizesTypeOnlyStartsAndEmptyGenericContinuations)
{
    std::vector<syntax::Token> tokens = classifier_probe_tokens({});
    DiagnosticSink diagnostics;
    parse::Parser parser(tokens, diagnostics);
    BracketSuffixClassifierProbe probe(parser);

    const parse::BracketSuffixDecision empty_index = probe.classify_empty_suffix();
    EXPECT_EQ(empty_index.kind, parse::BracketSuffixKind::index);

    std::vector<syntax::Token> generic_tokens = classifier_probe_tokens({syntax::TokenKind::l_paren});
    DiagnosticSink generic_diagnostics;
    parse::Parser generic_parser(generic_tokens, generic_diagnostics);
    BracketSuffixClassifierProbe generic_probe(generic_parser);
    const parse::BracketSuffixDecision empty_generic = generic_probe.classify_empty_suffix();
    EXPECT_EQ(empty_generic.kind, parse::BracketSuffixKind::generic_apply);

    std::vector<syntax::Token> type_tokens = classifier_probe_tokens({syntax::TokenKind::kw_i32});
    DiagnosticSink type_diagnostics;
    parse::Parser type_parser(type_tokens, type_diagnostics);
    BracketSuffixClassifierProbe type_probe(type_parser);
    EXPECT_TRUE(type_probe.arg_starts_type_only());
}

TEST(CoreUnit, ParserPostfixWhiteBoxConvertsScopedNamesAndRejectsBadSelectors)
{
    std::vector<syntax::Token> tokens = probe_tokens();
    DiagnosticSink diagnostics;
    parse::Parser parser(tokens, diagnostics);
    ParserPostfixProbe probe(parser);
    syntax::AstModule& module = probe.module();

    const syntax::ExprId scoped_name = module.push_name_expr({}, "pkg", {}, "Thing");
    const syntax::TypeId scoped_type = probe.convert_type_arg(scoped_name, true);
    ASSERT_TRUE(syntax::is_valid(scoped_type));
    const syntax::TypeNode& scoped = module.types[scoped_type.value];
    ASSERT_EQ(scoped.scope_parts.size(), 1U);
    EXPECT_EQ(scoped.scope_parts.front(), "pkg");
    EXPECT_EQ(scoped.name, "Thing");

    syntax::TypeNode applied_base;
    applied_base.kind = syntax::TypeKind::named;
    applied_base.name = "Box";
    applied_base.type_args = {scoped_type};
    const syntax::TypeId applied_base_id = module.push_type(applied_base);
    EXPECT_FALSE(syntax::is_valid(probe.append_selector(applied_base_id, "Inner", true)));
    EXPECT_TRUE(diagnostics.has_error());

    syntax::TypeNode scoped_base;
    scoped_base.kind = syntax::TypeKind::named;
    scoped_base.scope_name = "pkg";
    scoped_base.name = "Outer";
    const syntax::TypeId scoped_base_id = module.push_type(scoped_base);
    const syntax::TypeId selector_id = probe.append_selector(scoped_base_id, "Inner", true);
    ASSERT_TRUE(syntax::is_valid(selector_id));
    const syntax::TypeNode& selector = module.types[selector_id.value];
    const std::vector<std::string_view> expected_scope_parts{"pkg", "Outer"};
    EXPECT_EQ(selector.scope_parts, expected_scope_parts);
    EXPECT_EQ(selector.scope_name, "pkg");
    EXPECT_EQ(selector.name, "Inner");

    EXPECT_FALSE(syntax::is_valid(probe.append_selector(syntax::INVALID_TYPE_ID, "Inner", false)));
}

TEST(CoreUnit, ParserRejectsEmptyGenericLists)
{
    expect_parse_error("module parser.empty_generic_fn;\n"
                       "fn f[]() -> i32 { return 0; }\n",
        "expected generic type parameter");
    expect_parse_error("module parser.empty_generic_struct;\n"
                       "struct Box[] { value: i32; }\n",
        "expected generic type parameter");
    expect_parse_error("module parser.empty_type_args;\n"
                       "struct Box[T] { value: T; }\n"
                       "fn main() -> i32 { let value: Box[] = Box[i32] { value: 1 }; return value.value; }\n",
        "expected generic type argument");
    expect_parse_error("module parser.empty_generic_call;\n"
                       "fn id[T](value: T) -> T { return value; }\n"
                       "fn main() -> i32 { return id[](1); }\n",
        "expected generic type argument");
    expect_parse_error("module parser.empty_struct_literal_args;\n"
                       "struct Box[T] { value: T; }\n"
                       "fn main() -> i32 { let value = Box[] { value: 1 }; return value.value; }\n",
        "expected generic type argument");
    expect_parse_error("module parser.empty_index_args;\n"
                       "fn main() -> i32 { let values: [1]i32 = [1]; return values[]; }\n",
        "expected generic type argument");
    expect_parse_error("module parser.multi_index_args;\n"
                       "fn main() -> i32 { let values: [2]i32 = [1, 2]; return values[0, 1]; }\n",
        "index expression expects one argument");
    expect_parse_error("module parser.type_slice_start;\n"
                       "fn main() -> []const i32 { let values: [2]i32 = [1, 2]; return values[i32:]; }\n",
        "expected expression");
    expect_parse_diagnostic("module parser.literal_generic_arg;\n"
                            "fn id[T](value: T) -> T { return value; }\n"
                            "fn main() -> i32 { return id[1](1); }\n",
        "expected generic type argument");
    expect_parse_diagnostic("module parser.parenthesized_generic_arg;\n"
                            "fn id[T](value: T) -> T { return value; }\n"
                            "fn main() -> i32 { return id[(1)](1); }\n",
        "expected generic type argument");
    expect_parse_diagnostic("module parser.unary_expr_generic_arg;\n"
                            "fn id[T](value: T) -> T { return value; }\n"
                            "fn main() -> i32 { let value = 1; return id[-value](value); }\n",
        "expected generic type argument");
    expect_parse_diagnostic("module parser.invalid_selected_generic_arg;\n"
                            "struct Box[T] { value: T; }\n"
                            "fn id[T](value: i32) -> i32 { return value; }\n"
                            "fn main() -> i32 { return id[Box[i32].Inner](1); }\n",
        "expected generic type argument");
    expect_parse_diagnostic("module parser.parenthesized_generic_arg_eof;\n"
                            "fn id[T](value: T) -> T { return value; }\n"
                            "fn main() -> i32 { return id[(i32,\n",
        "expected expression");
    expect_parse_diagnostic("module parser.generic_arg_separator_reaches_bracket;\n"
                            "fn id[T](value: i32) -> i32 { return value; }\n"
                            "fn main() -> i32 { return id[A B](1); }\n",
        "expected ',' or ']' after generic type argument");
    expect_parse_diagnostic("module parser.struct_field_separator_reaches_comma;\n"
                            "struct Box { a: i32; b: i32; }\n"
                            "fn main() -> i32 { let box = Box { a: 1 @, b: 2 }; return box.a; }\n",
        "expected ',' or '}' after struct literal field");
    expect_parse_diagnostic("module parser.match_arm_separator_reaches_comma;\n"
                            "fn main() -> i32 { return match true { true => 1 @, false => 0 }; }\n",
        "expected ',' or '}' after match arm");
}

TEST(CoreUnit, ParserRejectsLegacyAngleGenericSyntax)
{
    expect_parse_error("module parser.legacy_angle_generic_params;\n"
                       "fn id<T>(x: T) -> T { return x; }\n",
        "Aurex generics use '[' and ']'; '<' and '>' are not generic delimiters");
    expect_parse_error("module parser.legacy_angle_type_args;\n"
                       "struct Pair[A, B] { first: A; second: B; }\n"
                       "type Bad = Pair<i32, bool>;\n",
        "Aurex generics use '[' and ']'; '<' and '>' are not generic delimiters");
}

TEST(CoreUnit, ParserRejectsLegacyScopeSelectorSyntax)
{
    expect_parse_error("module parser.legacy_scope_value;\n"
                       "fn main() -> i32 { return vis::answer; }\n",
        "Aurex selectors use '.', not '::'");
    expect_parse_error("module parser.legacy_scope_generic_call;\n"
                       "fn id[T](x: T) -> T { return x; }\n"
                       "fn main() -> i32 { return id::[i32](1); }\n",
        "Aurex selectors use '.', not '::'");
}

TEST(CoreUnit, ParserParsesWhereCapabilityClauses)
{
    syntax::AstModule module =
        parse_success("module parser.where_clause;\n"
                      "fn id[T](value: T) -> T where T: Eq + Iterator[Item = i32, Error = bool] { return value; }\n");
    const syntax::ItemNode* id = find_item(module, "id");
    ASSERT_NE(id, nullptr);
    ASSERT_EQ(id->where_constraints.size(), 1U);
    EXPECT_EQ(id->where_constraints.front().param_name, "T");
    ASSERT_EQ(id->where_constraints.front().capability_names.size(), 2U);
    EXPECT_EQ(id->where_constraints.front().capability_names[0], "Eq");
    EXPECT_EQ(id->where_constraints.front().capability_names[1], "Iterator");
    ASSERT_EQ(id->where_constraints.front().capability_associated_constraints.size(), 2U);
    EXPECT_TRUE(id->where_constraints.front().capability_associated_constraints.front().empty());
    const std::vector<syntax::AssociatedTypeConstraintDecl>& associated_constraints =
        id->where_constraints.front().capability_associated_constraints[1];
    ASSERT_EQ(associated_constraints.size(), 2U);
    EXPECT_EQ(associated_constraints[0].name, "Item");
    EXPECT_EQ(associated_constraints[1].name, "Error");

    const std::string ast = syntax::dump_ast(module);
    expect_contains(ast, "where T: Eq + Iterator[Item = i32, Error = bool]");
}

TEST(CoreUnit, ParserParsesAssociatedTypeConstraintSeparatorEdges)
{
    const syntax::AstModule module =
        parse_success("module parser.where_associated_edges;\n"
                      "fn empty[T](value: T) -> T where T: Iterator[] { return value; }\n"
                      "fn trailing[T](value: T) -> T where T: Iterator[Item = i32,] { return value; }\n"
                      "fn pair[T, U](left: T, right: U) -> T where T: Iterator[Item = i32], U: Eq {\n"
                      "  return left;\n"
                      "}\n");

    const syntax::ItemNode* const empty = find_item(module, "empty");
    ASSERT_NE(empty, nullptr);
    ASSERT_EQ(empty->where_constraints.size(), 1U);
    ASSERT_EQ(empty->where_constraints.front().capability_associated_constraints.size(), 1U);
    EXPECT_TRUE(empty->where_constraints.front().capability_associated_constraints.front().empty());

    const syntax::ItemNode* const trailing = find_item(module, "trailing");
    ASSERT_NE(trailing, nullptr);
    ASSERT_EQ(trailing->where_constraints.size(), 1U);
    ASSERT_EQ(trailing->where_constraints.front().capability_associated_constraints.size(), 1U);
    ASSERT_EQ(trailing->where_constraints.front().capability_associated_constraints.front().size(), 1U);
    EXPECT_EQ(trailing->where_constraints.front().capability_associated_constraints.front().front().name, "Item");

    const syntax::ItemNode* const pair = find_item(module, "pair");
    ASSERT_NE(pair, nullptr);
    ASSERT_EQ(pair->where_constraints.size(), 2U);
    EXPECT_EQ(pair->where_constraints.front().param_name, "T");
    EXPECT_EQ(pair->where_constraints[1].param_name, "U");
    ASSERT_EQ(pair->where_constraints.front().capability_associated_constraints.size(), 1U);
    ASSERT_EQ(pair->where_constraints.front().capability_associated_constraints.front().size(), 1U);
    EXPECT_EQ(pair->where_constraints.front().capability_associated_constraints.front().front().name, "Item");
    ASSERT_EQ(pair->where_constraints[1].capability_associated_constraints.size(), 1U);
    EXPECT_TRUE(pair->where_constraints[1].capability_associated_constraints.front().empty());

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "where T: Iterator",
            "where T: Iterator[Item = i32]",
            "where T: Iterator[Item = i32], U: Eq",
        });
}

TEST(CoreUnit, ParserRecoversMalformedAssociatedTypeConstraints)
{
    expect_parse_diagnostic("module parser.bad_where_associated_constraint;\n"
                            "fn recovered[T](value: T) -> T where T: Iterator[Item = i32 Error bool] {\n"
                            "  return value;\n"
                            "}\n",
        "expected ',' or ']' after associated type constraint");
    expect_parse_diagnostic("module parser.bad_where_associated_equal;\n"
                            "fn recovered[T](value: T) -> T where T: Iterator[Item i32] {\n"
                            "  return value;\n"
                            "}\n",
        "expected '=' in associated type constraint");
    expect_parse_diagnostic("module parser.bad_where_associated_name;\n"
                            "fn recovered[T](value: T) -> T where T: Iterator[= i32] {\n"
                            "  return value;\n"
                            "}\n",
        "expected associated type constraint name");
    expect_parse_diagnostic("module parser.bad_where_associated_recovered_comma;\n"
                            "fn recovered[T](value: T) -> T where T: Iterator[Item = i32 @, Error = bool] {\n"
                            "  return value;\n"
                            "}\n",
        "expected ',' or ']' after associated type constraint");
}

} // namespace aurex::test
