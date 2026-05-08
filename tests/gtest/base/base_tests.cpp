#include "aurex/base/abi.hpp"
#include "aurex/base/diagnostic.hpp"
#include "aurex/base/result.hpp"
#include "aurex/base/source.hpp"

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

using base::Diagnostic;
using base::DiagnosticSink;
using base::ErrorCode;
using base::Severity;

} // namespace

TEST(CoreUnit, BaseDiagnosticsSourcesAndResult) {
    EXPECT_EQ(base::abi::internal_symbol_prefix, "m0");

    base::SourceRange forward {{7}, 3, 9};
    EXPECT_EQ(forward.length(), 6U);
    EXPECT_FALSE(forward.empty());

    base::SourceRange reversed {{7}, 9, 3};
    EXPECT_EQ(reversed.length(), 0U);
    EXPECT_FALSE(reversed.empty());

    base::SourceManager sources;
    const base::SourceId id = sources.add_source("unit.ax", "module unit;");
    EXPECT_EQ(sources.get(id).id().value, id.value);
    EXPECT_EQ(sources.get(id).path(), "unit.ax");
    EXPECT_EQ(sources.text(id), "module unit;");

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

} // namespace aurex::test
