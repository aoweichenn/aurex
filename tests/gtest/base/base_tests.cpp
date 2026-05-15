#include <aurex/base/abi.hpp>
#include <aurex/base/diagnostic.hpp>
#include <aurex/base/result.hpp>
#include <aurex/base/source.hpp>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

using base::Diagnostic;
using base::DiagnosticSink;
using base::ErrorCode;
using base::Severity;

constexpr int BASE_TEST_UNKNOWN_SEVERITY_VALUE = 99;

} // namespace

TEST(CoreUnit, BaseDiagnosticsSourcesAndResult) {
    EXPECT_EQ(base::abi::AUREX_INTERNAL_SYMBOL_PREFIX, "m0");

    base::SourceRange forward {{7}, 3, 9};
    EXPECT_EQ(forward.length(), 6U);
    EXPECT_FALSE(forward.empty());

    base::SourceRange reversed {{7}, 9, 3};
    EXPECT_EQ(reversed.length(), 0U);
    EXPECT_FALSE(reversed.empty());

    base::SourceRange empty {{7}, 3, 3};
    EXPECT_EQ(empty.length(), 0U);
    EXPECT_TRUE(empty.empty());

    base::SourceManager sources;
    const base::SourceId id = sources.add_source("unit.ax", "module unit;");
    EXPECT_EQ(sources.get(id).id().value, id.value);
    EXPECT_EQ(sources.get(id).path(), "unit.ax");
    EXPECT_EQ(sources.text(id), "module unit;");
    EXPECT_EQ(sources.get(id).line_column(0).line, 1U);
    EXPECT_EQ(sources.get(id).line_column(0).column, 1U);

    DiagnosticSink diagnostics;
    EXPECT_FALSE(diagnostics.has_error());
    diagnostics.push(Diagnostic {Severity::note, forward, "note"});
    diagnostics.push(Diagnostic {Severity::warning, forward, "warning"});
    EXPECT_FALSE(diagnostics.has_error());
    diagnostics.push(Diagnostic {Severity::error, forward, "error"});
    EXPECT_TRUE(diagnostics.has_error());
    diagnostics.push(Diagnostic {Severity::fatal, forward, "fatal"});
    ASSERT_EQ(diagnostics.diagnostics().size(), 4U);

    EXPECT_EQ(base::severity_name(Severity::note), "note");
    EXPECT_EQ(base::severity_name(Severity::warning), "warning");
    EXPECT_EQ(base::severity_name(Severity::error), "error");
    EXPECT_EQ(base::severity_name(Severity::fatal), "fatal");
    EXPECT_EQ(base::severity_name(static_cast<Severity>(BASE_TEST_UNKNOWN_SEVERITY_VALUE)), "unknown");

    auto ok_int = base::Result<int>::ok(11);
    ASSERT_TRUE(ok_int);
    EXPECT_EQ(ok_int.value(), 11);
    auto failed = base::Result<int>::fail({ErrorCode::io_error, "missing"});
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error().code, ErrorCode::io_error);
    EXPECT_EQ(failed.error().message, "missing");

    auto ok_void = base::Result<void>::ok();
    EXPECT_TRUE(ok_void);
    auto failed_void = base::Result<void>::fail({ErrorCode::internal_error, "bad"});
    ASSERT_FALSE(failed_void);
    EXPECT_EQ(failed_void.error().code, ErrorCode::internal_error);
}

TEST(CoreUnit, SourceFileLineTableHandlesOffsetsAndExtents) {
    base::SourceManager sources;
    const base::SourceId id = sources.add_source("lines.ax", "first\nsecond\nthird");
    const base::SourceFile& file = sources.get(id);

    EXPECT_EQ(file.line_column(0).line, 1U);
    EXPECT_EQ(file.line_column(0).column, 1U);
    EXPECT_EQ(file.line_column(6).line, 2U);
    EXPECT_EQ(file.line_column(6).column, 1U);
    EXPECT_EQ(file.line_column(13).line, 3U);
    EXPECT_EQ(file.line_column(13).column, 1U);
    EXPECT_EQ(file.line_column(99).line, 3U);
    EXPECT_EQ(file.line_column(99).column, 6U);

    const base::SourceLineExtent first = file.line_extent(1);
    EXPECT_EQ(first.begin, 0U);
    EXPECT_EQ(first.end, 5U);
    EXPECT_EQ(file.text().substr(first.begin, first.end - first.begin), "first");

    const base::SourceLineExtent second = file.line_extent(8);
    EXPECT_EQ(second.begin, 6U);
    EXPECT_EQ(second.end, 12U);
    EXPECT_EQ(file.text().substr(second.begin, second.end - second.begin), "second");

    const base::SourceLineExtent third = file.line_extent(99);
    EXPECT_EQ(third.begin, 13U);
    EXPECT_EQ(third.end, 18U);
    EXPECT_EQ(file.text().substr(third.begin, third.end - third.begin), "third");
}

} // namespace aurex::test
