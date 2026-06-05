#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/midend/ir/ir_dump.hpp>
#include <aurex/midend/ir/lower_ast.hpp>
#include <aurex/midend/ir/verify.hpp>

#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

using ir::Module;

constexpr base::SourceId LOWER_AST_AGGREGATE_ROLLBACK_SOURCE_ID{809};
constexpr std::string_view ROLLBACK_FLAG_ALLOCA_TEXT = "alloca drop.flag.aggregate.rollback";
constexpr std::string_view ROLLBACK_DROP_IF_TEXT = "drop_if ";
constexpr std::string_view ROLLBACK_DESTRUCTOR_CALL_TEXT =
    "call m0_aggregate_rollback_Tracked_trait_impl_Drop__drop";

struct LoweredSource {
    syntax::AstModule ast;
    sema::CheckedModule checked;
    Module ir;
};

[[nodiscard]] syntax::AstModule parse_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(LOWER_AST_AGGREGATE_ROLLBACK_SOURCE_ID, source, diagnostics);
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

[[nodiscard]] LoweredSource lower_source(const std::string_view source)
{
    LoweredSource lowered;
    lowered.ast = parse_source(source);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(lowered.ast, diagnostics);
    auto checked = analyzer.analyze();
    if (!checked) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        ADD_FAILURE() << checked.error().message;
        return lowered;
    }
    lowered.checked = checked.take_value();

    auto ir_module = ir::lower_ast(lowered.ast, lowered.checked);
    if (!ir_module) {
        ADD_FAILURE() << ir_module.error().message;
        return lowered;
    }
    lowered.ir = ir_module.take_value();
    return lowered;
}

[[nodiscard]] base::usize count_occurrences(const std::string_view text, const std::string_view needle)
{
    base::usize count = 0;
    base::usize position = 0;
    while (position < text.size()) {
        const base::usize next = text.find(needle, position);
        if (next == std::string_view::npos) {
            break;
        }
        ++count;
        position = next + needle.size();
    }
    return count;
}

void expect_verified_rollback(const LoweredSource& lowered, const base::usize minimum_rollback_count)
{
    const base::Result<void> verified = ir::verify_module(lowered.ir);
    ASSERT_TRUE(verified) << verified.error().message << "\n" << ir::dump_module(lowered.ir);

    const std::string dump = ir::dump_module(lowered.ir);
    EXPECT_GE(count_occurrences(dump, ROLLBACK_FLAG_ALLOCA_TEXT), minimum_rollback_count) << dump;
    EXPECT_GE(count_occurrences(dump, ROLLBACK_DROP_IF_TEXT), minimum_rollback_count) << dump;
    EXPECT_GE(count_occurrences(dump, ROLLBACK_DESTRUCTOR_CALL_TEXT), minimum_rollback_count) << dump;
}

constexpr std::string_view ROLLBACK_PREAMBLE = R"(
module aggregate_rollback;

struct Tracked { value: i32; }

impl Drop for Tracked {
    fn drop(self: deinit Tracked) -> void {}
}

enum ResultTrackedI32: u8 {
    ok(Tracked) = 1,
    err(i32) = 2,
}
)";

} // namespace

TEST(CoreUnit, LowerAstAggregateRollbackCleansStructFieldBeforeTryFailure)
{
    const std::string source = std::string(ROLLBACK_PREAMBLE) + R"(
struct Pair {
    first: Tracked;
    second: Tracked;
}

fn build(abort: bool) -> ResultTrackedI32 {
    let pair: Pair = Pair {
        first: Tracked { value: 1 },
        second: {
            if abort {
                return ResultTrackedI32.err(-2);
            }
            Tracked { value: 2 }
        },
    };
    return ResultTrackedI32.err(0);
}
)";

    const LoweredSource lowered = lower_source(source);
    expect_verified_rollback(lowered, 1U);
}

TEST(CoreUnit, LowerAstAggregateRollbackStagesMixedStructWithoutScalarRollback)
{
    const std::string source = std::string(ROLLBACK_PREAMBLE) + R"(
struct Mixed {
    plain: i32;
    first: Tracked;
    second: Tracked;
}

fn build(abort: bool) -> ResultTrackedI32 {
    let mixed: Mixed = Mixed {
        plain: 7,
        first: Tracked { value: 1 },
        second: {
            if abort {
                return ResultTrackedI32.err(-2);
            }
            Tracked { value: 2 }
        },
    };
    return ResultTrackedI32.err(0);
}
)";

    const LoweredSource lowered = lower_source(source);
    expect_verified_rollback(lowered, 1U);

    const std::string dump = ir::dump_module(lowered.ir);
    EXPECT_EQ(count_occurrences(dump, "drop_if %"), count_occurrences(dump, " as aggregate_rollback.Tracked"))
        << dump;
    EXPECT_EQ(dump.find(" as i32"), std::string::npos) << dump;
}

TEST(CoreUnit, LowerAstAggregateRollbackCleansTupleElementBeforeTryFailure)
{
    const std::string source = std::string(ROLLBACK_PREAMBLE) + R"(
fn build(abort: bool) -> ResultTrackedI32 {
    let pair: (Tracked, Tracked) = (
        Tracked { value: 1 },
        {
            if abort {
                return ResultTrackedI32.err(-2);
            }
            Tracked { value: 2 }
        },
    );
    return ResultTrackedI32.err(0);
}
)";

    const LoweredSource lowered = lower_source(source);
    expect_verified_rollback(lowered, 1U);
}

TEST(CoreUnit, LowerAstAggregateRollbackCleansArrayElementBeforeTryFailure)
{
    const std::string source = std::string(ROLLBACK_PREAMBLE) + R"(
fn build(abort: bool) -> ResultTrackedI32 {
    let values: [2]Tracked = [
        Tracked { value: 1 },
        {
            if abort {
                return ResultTrackedI32.err(-2);
            }
            Tracked { value: 2 }
        },
    ];
    return ResultTrackedI32.err(0);
}
)";

    const LoweredSource lowered = lower_source(source);
    expect_verified_rollback(lowered, 1U);
}

TEST(CoreUnit, LowerAstAggregateRollbackCleansEnumPayloadFieldBeforeTryFailure)
{
    const std::string source = std::string(ROLLBACK_PREAMBLE) + R"(
enum Choice: u8 {
    both(Tracked, Tracked) = 1,
    empty = 2,
}

fn build(abort: bool) -> ResultTrackedI32 {
    let choice: Choice = Choice.both(
        Tracked { value: 1 },
        {
            if abort {
                return ResultTrackedI32.err(-2);
            }
            Tracked { value: 2 }
        },
    );
    return ResultTrackedI32.err(0);
}
)";

    const LoweredSource lowered = lower_source(source);
    expect_verified_rollback(lowered, 1U);
}

TEST(CoreUnit, LowerAstAggregateRollbackStagesMixedEnumPayloadWithoutScalarRollback)
{
    const std::string source = std::string(ROLLBACK_PREAMBLE) + R"(
enum Choice: u8 {
    mixed(i32, Tracked, Tracked) = 1,
    empty = 2,
}

fn build(abort: bool) -> ResultTrackedI32 {
    let choice: Choice = Choice.mixed(
        7,
        Tracked { value: 1 },
        {
            if abort {
                return ResultTrackedI32.err(-2);
            }
            Tracked { value: 2 }
        },
    );
    return ResultTrackedI32.err(0);
}
)";

    const LoweredSource lowered = lower_source(source);
    expect_verified_rollback(lowered, 1U);

    const std::string dump = ir::dump_module(lowered.ir);
    EXPECT_NE(dump.find("alloca enum.payload"), std::string::npos) << dump;
    EXPECT_EQ(dump.find(" as i32"), std::string::npos) << dump;
}

TEST(CoreUnit, LowerAstAggregateRollbackLeavesPlainScalarAggregateLightweight)
{
    const std::string source = R"(
module aggregate_rollback_plain;

struct Pair {
    first: i32;
    second: i32;
}

fn build() -> Pair {
    return Pair { first: 1, second: 2 };
}
)";

    const LoweredSource lowered = lower_source(source);
    const base::Result<void> verified = ir::verify_module(lowered.ir);
    ASSERT_TRUE(verified) << verified.error().message << "\n" << ir::dump_module(lowered.ir);
    const std::string dump = ir::dump_module(lowered.ir);
    EXPECT_EQ(count_occurrences(dump, ROLLBACK_FLAG_ALLOCA_TEXT), 0U) << dump;
    EXPECT_EQ(count_occurrences(dump, ROLLBACK_DROP_IF_TEXT), 0U) << dump;
}

} // namespace aurex::test
