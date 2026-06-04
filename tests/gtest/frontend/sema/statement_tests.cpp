#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/frontend/sema/sema_messages.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::SourceId STATEMENT_TEST_SOURCE_ID{809};

[[nodiscard]] syntax::AstModule parse_statement_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(STATEMENT_TEST_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        ADD_FAILURE() << tokens.error().message;
        return {};
    }

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (!parsed) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
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

[[nodiscard]] std::string analyze_statement_source(const std::string_view source, const bool expect_success)
{
    syntax::AstModule module = parse_statement_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics);
    auto result = analyzer.analyze();

    std::string output;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        output += diagnostic.message;
        output += '\n';
    }
    if (expect_success) {
        if (!result) {
            output += result.error().message;
            output += '\n';
            ADD_FAILURE() << output;
        }
        return output;
    }
    if (result) {
        ADD_FAILURE() << "expected semantic analysis to fail";
    } else {
        output += result.error().message;
        output += '\n';
    }
    return output;
}

void expect_statement_success(const std::string_view source)
{
    EXPECT_TRUE(analyze_statement_source(source, true).empty());
}

void expect_statement_diagnostic(const std::string_view source, const std::string_view diagnostic)
{
    const std::string output = analyze_statement_source(source, false);
    EXPECT_NE(output.find(diagnostic), std::string::npos) << output;
}

} // namespace

TEST(CoreUnit, StatementSemaCoversForRangeLoopAndLetElseSuccessPaths)
{
    constexpr std::string_view source =
        "module statement.success_paths;\n"
        "fn push(log: &mut i32, value: i32) -> void {\n"
        "  *log = *log + value;\n"
        "}\n"
        "enum Maybe {\n"
        "  some(i32),\n"
        "  none,\n"
        "}\n"
        "fn loops(limit: i32, value: Maybe) -> i32 {\n"
        "  var total: i32 = 0;\n"
        "  var log: i32 = 0;\n"
        "  {\n"
        "    let block_value: i32 = 1;\n"
        "    total += block_value;\n"
        "  }\n"
        "  for var i: i32 = 0; i < limit; i += 1 {\n"
        "    total += i;\n"
        "  }\n"
        "  for j in range(0, limit, 1) {\n"
        "    defer push(&mut log, j);\n"
        "    if j == 1 {\n"
        "      continue;\n"
        "    }\n"
        "    if j == 3 {\n"
        "      break;\n"
        "    }\n"
        "    total += j;\n"
        "  }\n"
        "  let .some(inner) = value else {\n"
        "    return total + log;\n"
        "  };\n"
        "  return total + log + inner;\n"
        "}\n"
        "fn main() -> void {}\n";

    expect_statement_success(source);
}

TEST(CoreUnit, StatementSemaCoversStorageEscapeGuardForRangeTraversal)
{
    constexpr std::string_view source =
        "module statement.storage_escape_for_range;\n"
        "struct Counter {\n"
        "  value: i32;\n"
        "}\n"
        "fn update(limit: i32) -> i32 {\n"
        "  var counter: Counter = Counter { value: 0 };\n"
        "  for i in range(0, limit, 1) {\n"
        "    counter.value = counter.value + i;\n"
        "  }\n"
        "  return counter.value;\n"
        "}\n"
        "fn main() -> void {}\n";

    expect_statement_success(source);
}

TEST(CoreUnit, StatementSemaCoversStorageEscapeGuardExpressionShapes)
{
    constexpr std::string_view source =
        "module statement.storage_escape_guard_shapes;\n"
        "struct Leaf {\n"
        "  value: i32;\n"
        "  other: i32;\n"
        "}\n"
        "struct Cell {\n"
        "  value: i32;\n"
        "  size: usize;\n"
        "  values: [2]i32;\n"
        "  pair: (i32, i32);\n"
        "  leaf: Leaf;\n"
        "  ptr: *const i32;\n"
        "  op: fn(i32) -> i32;\n"
        "}\n"
        "enum Choice: u8 {\n"
        "  some(i32) = 1,\n"
        "  none = 2,\n"
        "}\n"
        "fn id[T](value: T) -> T where T: Copy {\n"
        "  return value;\n"
        "}\n"
        "fn inc(value: i32) -> i32 {\n"
        "  return value + 1;\n"
        "}\n"
        "fn choose(value: i32, flag: bool) -> Choice {\n"
        "  if flag {\n"
        "    return Choice.some(value);\n"
        "  }\n"
        "  return Choice.none;\n"
        "}\n"
        "fn update(input: i32, flag: bool) -> i32 {\n"
        "  var cell: Cell = Cell {\n"
        "    value: 0,\n"
        "    size: 0usize,\n"
        "    values: [0, 0],\n"
        "    pair: (0, 0),\n"
        "    leaf: Leaf { value: 0, other: 0 },\n"
        "    ptr: null,\n"
        "    op: inc,\n"
        "  };\n"
        "  let values: [2]i32 = [input, 1];\n"
        "  let ptr: *const i32 = unsafe { ptrat[*const i32](ptraddr(&cell.value)) };\n"
        "  let erased: *const void = unsafe { ptrcast[*const void](ptr) };\n"
        "  cell.value = id[i32](cell.value);\n"
        "  cell.value = if flag { input } else { 0 };\n"
        "  cell.value = { let tmp: i32 = input + 1; tmp };\n"
        "  cell.value = [input, 1][0];\n"
        "  cell.pair = (input, 2);\n"
        "  cell.leaf = Leaf { value: input, other: 2 };\n"
        "  cell.ptr = unsafe { ptrcast[*const i32](erased) };\n"
        "  cell.value = unsafe { bitcast[i32](cast[u32](input)) };\n"
        "  cell.value = values[0];\n"
        "  cell.value = cast[i32](input);\n"
        "  cell.size = sizeof[i32];\n"
        "  cell.size = alignof[Leaf];\n"
        "  cell.value = match choose(input, flag) {\n"
        "    .some(found) => found,\n"
        "    .none => 0,\n"
        "  };\n"
        "  return cell.op(cell.value);\n"
        "}\n"
        "fn main() -> void {}\n";

    expect_statement_success(source);
}

TEST(CoreUnit, StatementSemaCoversStorageEscapeGuardStatementTraversalShapes)
{
    constexpr std::string_view source =
        "module statement.storage_escape_guard_statement_shapes;\n"
        "struct Cell {\n"
        "  value: i32;\n"
        "}\n"
        "enum ResultI32I32: u8 {\n"
        "  ok(i32) = 1,\n"
        "  err(i32) = 2,\n"
        "}\n"
        "fn id(value: i32) -> i32 {\n"
        "  return value;\n"
        "}\n"
        "fn fallible(value: i32) -> ResultI32I32 {\n"
        "  return ResultI32I32.ok(value);\n"
        "}\n"
        "fn observe(value: i32) -> void {\n"
        "  let sink: i32 = value;\n"
        "}\n"
        "fn update(limit: i32, input: i32, flag: bool) -> ResultI32I32 {\n"
        "  var cell: Cell = Cell { value: 0 };\n"
        "  var local: i32 = 0;\n"
        "  var ptr: *const i32 = unsafe { ptrat[*const i32](ptraddr(&local)) };\n"
        "  cell.value = fallible(input)?;\n"
        "  local = id(input);\n"
        "  ptr = unsafe { ptrat[*const i32](ptraddr(&local)) };\n"
        "  if flag {\n"
        "    local = local + 1;\n"
        "  } else if input > 0 {\n"
        "    local = local + 2;\n"
        "  } else {\n"
        "    local = local + 3;\n"
        "  }\n"
        "  while local < limit {\n"
        "    local += 1;\n"
        "    if local == input {\n"
        "      continue;\n"
        "    }\n"
        "    if local > input + limit {\n"
        "      break;\n"
        "    }\n"
        "  }\n"
        "  for var i: i32 = 0; i < limit; i += 1 {\n"
        "    local += i;\n"
        "  }\n"
        "  for j in range(0, limit, 1) {\n"
        "    local += j;\n"
        "  }\n"
        "  {\n"
        "    let scoped: i32 = local;\n"
        "    local = scoped + 1;\n"
        "  }\n"
        "  defer observe(local);\n"
        "  observe(unsafe { *ptr });\n"
        "  return ResultI32I32.ok(local + cell.value);\n"
        "}\n"
        "fn main() -> void {}\n";

    expect_statement_success(source);
}

TEST(CoreUnit, StatementSemaCoversForRangeAndCompoundAssignmentDiagnostics)
{
    const std::vector<std::pair<std::string_view, std::string_view>> cases = {
        {
            "module statement.bad_for_condition;\n"
            "fn main() -> i32 {\n"
            "  for var i: i32 = 0; i; i = i + 1 {\n"
            "    return i;\n"
            "  }\n"
            "  return 0;\n"
            "}\n",
            sema::SEMA_FOR_CONDITION_BOOL,
        },
        {
            "module statement.bad_range_start;\n"
            "fn main() -> i32 {\n"
            "  for i in range(false, 3) {\n"
            "    return i;\n"
            "  }\n"
            "  return 0;\n"
            "}\n",
            sema::SEMA_RANGE_BOUNDS_INTEGER,
        },
        {
            "module statement.bad_range_contextual_binary;\n"
            "fn main() -> i32 {\n"
            "  for i in range(0 < 1, 3) {\n"
            "    return i;\n"
            "  }\n"
            "  return 0;\n"
            "}\n",
            sema::SEMA_RANGE_BOUNDS_INTEGER,
        },
        {
            "module statement.bad_range_end;\n"
            "fn main() -> i32 {\n"
            "  for i in range(true) {\n"
            "    return i;\n"
            "  }\n"
            "  return 0;\n"
            "}\n",
            sema::SEMA_RANGE_BOUNDS_INTEGER,
        },
        {
            "module statement.bad_range_bounds_type;\n"
            "fn main() -> i32 {\n"
            "  let start: i32 = 0;\n"
            "  let end: i64 = 3;\n"
            "  for i in range(start, end) {\n"
            "    return i;\n"
            "  }\n"
            "  return 0;\n"
            "}\n",
            sema::SEMA_RANGE_BOUNDS_SAME_TYPE,
        },
        {
            "module statement.bad_range_step_integer;\n"
            "fn main() -> i32 {\n"
            "  for i in range(0, 3, false) {\n"
            "    return i;\n"
            "  }\n"
            "  return 0;\n"
            "}\n",
            sema::SEMA_RANGE_STEP_INTEGER,
        },
        {
            "module statement.bad_range_step_type;\n"
            "fn main() -> i32 {\n"
            "  let start: i32 = 0;\n"
            "  let end: i32 = 3;\n"
            "  let step: i64 = 1;\n"
            "  for i in range(start, end, step) {\n"
            "    return i;\n"
            "  }\n"
            "  return 0;\n"
            "}\n",
            sema::SEMA_RANGE_STEP_SAME_TYPE,
        },
        {
            "module statement.bad_defer_try_argument;\n"
            "enum ResultI32I32: u8 {\n"
            "  ok(i32) = 1,\n"
            "  err(i32) = 2,\n"
            "}\n"
            "fn mark(value: i32) -> void {}\n"
            "fn fail() -> ResultI32I32 {\n"
            "  return ResultI32I32.err(1);\n"
            "}\n"
            "fn main() -> ResultI32I32 {\n"
            "  defer mark(fail()?);\n"
            "  return ResultI32I32.ok(0);\n"
            "}\n",
            sema::SEMA_DEFER_EARLY_EXIT,
        },
        {
            "module statement.bad_compound_assignment;\n"
            "fn main() -> i32 {\n"
            "  var flag: bool = true;\n"
            "  flag += 1;\n"
            "  return 0;\n"
            "}\n",
            sema::SEMA_BINARY_OPERANDS_SAME_TYPE,
        },
    };

    for (const auto& [source, diagnostic] : cases) {
        SCOPED_TRACE(source);
        expect_statement_diagnostic(source, diagnostic);
    }
}

} // namespace aurex::test
