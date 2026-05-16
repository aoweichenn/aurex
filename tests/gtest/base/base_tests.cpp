#include <aurex/base/abi.hpp>
#include <aurex/base/bump_allocator.hpp>
#include <aurex/base/diagnostic.hpp>
#include <aurex/base/result.hpp>
#include <aurex/base/source.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

using base::Diagnostic;
using base::DiagnosticSink;
using base::ErrorCode;
using base::Severity;

constexpr int BASE_TEST_UNKNOWN_SEVERITY_VALUE = 99;
constexpr base::usize BASE_TEST_BUMP_SMALL_BLOCK_BYTES = 32;
constexpr base::usize BASE_TEST_BUMP_ALIGNMENT = 16;
constexpr base::usize BASE_TEST_BUMP_NON_POWER_ALIGNMENT = 24;
constexpr base::usize BASE_TEST_BUMP_NORMALIZED_ALIGNMENT = 32;
constexpr base::usize BASE_TEST_BUMP_LARGE_TEXT_BYTES = 4096;
constexpr char BASE_TEST_BUMP_FILL_CHAR = 'x';

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

TEST(CoreUnit, BumpAllocatorCopiesStringsAndResetsWholeArena) {
    base::BumpAllocator arena(BASE_TEST_BUMP_SMALL_BLOCK_BYTES);

    EXPECT_EQ(arena.allocate(0), nullptr);
    arena.reserve(0);

    void* const aligned = arena.allocate(1, BASE_TEST_BUMP_ALIGNMENT);
    ASSERT_NE(aligned, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(aligned) % BASE_TEST_BUMP_ALIGNMENT, 0U);

    void* const normalized = arena.allocate(1, BASE_TEST_BUMP_NON_POWER_ALIGNMENT);
    ASSERT_NE(normalized, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(normalized) % BASE_TEST_BUMP_NORMALIZED_ALIGNMENT, 0U);

    const std::string_view hello = arena.copy_string("hello");
    EXPECT_EQ(hello, "hello");
    ASSERT_NE(hello.data(), nullptr);
    EXPECT_EQ(hello.data()[hello.size()], '\0');
    EXPECT_GT(arena.allocated_bytes(), 0U);
    EXPECT_GT(arena.block_count(), 0U);

    const base::usize blocks_before_reserved_fit = arena.block_count();
    arena.reserve(1);
    EXPECT_EQ(arena.block_count(), blocks_before_reserved_fit);

    const base::usize blocks_before_large = arena.block_count();
    const std::string large(BASE_TEST_BUMP_LARGE_TEXT_BYTES, BASE_TEST_BUMP_FILL_CHAR);
    const std::string_view copied_large = arena.copy_string(large);
    EXPECT_EQ(copied_large, large);
    EXPECT_GT(arena.block_count(), blocks_before_large);

    arena.reset();
    EXPECT_EQ(arena.allocated_bytes(), 0U);
    EXPECT_EQ(arena.block_count(), 0U);
    EXPECT_EQ(arena.copy_string(""), "");
}

TEST(CoreUnit, BumpAllocatorMoveTransfersBlocksAndStats) {
    base::BumpAllocator source(BASE_TEST_BUMP_SMALL_BLOCK_BYTES);
    const std::string_view copied = source.copy_string("move-source");
    ASSERT_EQ(copied, "move-source");
    const base::usize allocated = source.allocated_bytes();
    ASSERT_GT(allocated, 0U);

    base::BumpAllocator moved(std::move(source));
    EXPECT_EQ(moved.allocated_bytes(), allocated);
    EXPECT_EQ(moved.copy_string("after-move"), "after-move");
    EXPECT_EQ(source.allocated_bytes(), 0U);

    base::BumpAllocator assigned(BASE_TEST_BUMP_SMALL_BLOCK_BYTES);
    EXPECT_EQ(assigned.copy_string("old"), "old");
    assigned = std::move(moved);
    EXPECT_GE(assigned.allocated_bytes(), allocated);
    EXPECT_EQ(assigned.copy_string("assigned"), "assigned");
    EXPECT_EQ(moved.allocated_bytes(), 0U);
}

TEST(CoreUnit, BumpAllocatorAdapterBacksStandardVectors) {
    base::BumpAllocator arena(BASE_TEST_BUMP_SMALL_BLOCK_BYTES);
    base::BumpVector<int> values {base::BumpAllocatorAdapter<int> {arena}};
    values.reserve(4);
    values.push_back(1);
    values.push_back(2);
    values.push_back(3);

    EXPECT_GT(arena.allocated_bytes(), 0U);
    EXPECT_GT(arena.block_count(), 0U);
    EXPECT_EQ(values.back(), 3);

    base::BumpAllocator copy_arena(BASE_TEST_BUMP_SMALL_BLOCK_BYTES);
    base::BumpVector<int> copied {base::BumpAllocatorAdapter<int> {copy_arena}};
    copied = values;

    ASSERT_EQ(copied.size(), values.size());
    EXPECT_EQ(copied.front(), 1);
    EXPECT_EQ(copied.back(), 3);
    EXPECT_GT(copy_arena.allocated_bytes(), 0U);
}

} // namespace aurex::test
