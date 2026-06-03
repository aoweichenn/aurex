#include <aurex/infrastructure/base/abi.hpp>
#include <aurex/infrastructure/base/bump_allocator.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/base/result.hpp>
#include <aurex/infrastructure/base/source.hpp>

#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

using base::Diagnostic;
using base::DiagnosticCategory;
using base::DiagnosticCode;
using base::DiagnosticSink;
using base::ErrorCode;
using base::Severity;

constexpr int BASE_TEST_UNKNOWN_SEVERITY_VALUE = 99;
constexpr int BASE_TEST_UNKNOWN_DIAGNOSTIC_CATEGORY_VALUE = 99;
constexpr int BASE_TEST_UNKNOWN_DIAGNOSTIC_CODE_VALUE = 99;
constexpr int BASE_TEST_UNKNOWN_DIAGNOSTIC_LABEL_STYLE_VALUE = 99;
constexpr base::usize BASE_TEST_BUMP_SMALL_BLOCK_BYTES = 32;
constexpr base::usize BASE_TEST_BUMP_ALIGNMENT = 16;
constexpr base::usize BASE_TEST_BUMP_NON_POWER_ALIGNMENT = 24;
constexpr base::usize BASE_TEST_BUMP_NORMALIZED_ALIGNMENT = 32;
constexpr base::usize BASE_TEST_BUMP_LARGE_TEXT_BYTES = 4096;
constexpr base::usize BASE_TEST_BUMP_TOUCHED_RESERVE_BYTES = 256;
constexpr base::usize BASE_TEST_BUMP_TOUCHED_ALLOC_BYTES = 64;
constexpr base::usize BASE_TEST_BUMP_HUGE_ALIGNMENT =
    (std::numeric_limits<base::usize>::max() / 2U) + BASE_TEST_BUMP_ALIGNMENT;
constexpr char BASE_TEST_BUMP_FILL_CHAR = 'x';

} // namespace

TEST(CoreUnit, BaseDiagnosticsSourcesAndResult)
{
    EXPECT_EQ(base::abi::AUREX_INTERNAL_SYMBOL_PREFIX, "m0");

    base::SourceRange forward{{7}, 3, 9};
    EXPECT_TRUE(forward.well_formed());
    EXPECT_EQ(forward.length(), 6U);
    EXPECT_FALSE(forward.empty());

    base::SourceRange reversed{{7}, 9, 3};
    EXPECT_FALSE(reversed.well_formed());
    EXPECT_EQ(reversed.length(), 0U);
    EXPECT_FALSE(reversed.empty());

    base::SourceRange empty{{7}, 3, 3};
    EXPECT_EQ(empty.length(), 0U);
    EXPECT_TRUE(empty.empty());

    base::SourceManager sources;
    const base::SourceId id = sources.add_source("unit.ax", "module unit;");
    EXPECT_EQ(sources.try_get(id), &sources.get(id));
    EXPECT_EQ(sources.try_get(base::SourceId{std::numeric_limits<base::u32>::max()}), nullptr);
    EXPECT_EQ(sources.get(id).id().value, id.value);
    EXPECT_EQ(sources.get(id).path(), "unit.ax");
    EXPECT_EQ(sources.text(id), "module unit;");
    EXPECT_EQ(sources.text(base::SourceId{std::numeric_limits<base::u32>::max()}), "");
    EXPECT_EQ(sources.get(id).line_column(0).line, 1U);
    EXPECT_EQ(sources.get(id).line_column(0).column, 1U);

    DiagnosticSink diagnostics;
    EXPECT_FALSE(diagnostics.has_error());
    diagnostics.push(Diagnostic{Severity::note, forward, "note"});
    diagnostics.push(Diagnostic{Severity::help, forward, "help"});
    diagnostics.push(Diagnostic{Severity::warning, forward, "warning"});
    EXPECT_FALSE(diagnostics.has_error());
    diagnostics.push(Diagnostic{
        Severity::error,
        forward,
        "error",
        DiagnosticCategory::semantic,
        DiagnosticCode::semantic_error,
    });
    EXPECT_TRUE(diagnostics.has_error());
    diagnostics.push(Diagnostic{Severity::fatal, forward, "fatal"});
    ASSERT_EQ(diagnostics.diagnostics().size(), 5U);
    EXPECT_EQ(diagnostics.diagnostics()[3].category, DiagnosticCategory::semantic);
    EXPECT_EQ(diagnostics.diagnostics()[3].code, DiagnosticCode::semantic_error);
    EXPECT_TRUE(diagnostics.diagnostics()[3].labels.empty());
    EXPECT_TRUE(diagnostics.diagnostics()[3].children.empty());

    const base::DiagnosticLabel primary = base::primary_diagnostic_label(forward, "primary span");
    const base::DiagnosticLabel secondary = base::secondary_diagnostic_label(empty, "secondary span");
    const base::DiagnosticChild note =
        base::diagnostic_note(forward, "note child", DiagnosticCategory::semantic, DiagnosticCode::semantic_error);
    const base::DiagnosticChild help = base::diagnostic_help(empty, "help child");
    const Diagnostic structured{
        Severity::error,
        forward,
        "structured",
        DiagnosticCategory::type,
        DiagnosticCode::semantic_type_mismatch,
        {primary, secondary},
        {note, help},
    };
    EXPECT_EQ(structured.labels.size(), 2U);
    EXPECT_EQ(structured.labels[0].style, base::DiagnosticLabelStyle::primary);
    EXPECT_EQ(structured.labels[1].style, base::DiagnosticLabelStyle::secondary);
    EXPECT_EQ(structured.children.size(), 2U);
    EXPECT_EQ(structured.children[0].severity, Severity::note);
    EXPECT_EQ(structured.children[1].severity, Severity::help);

    EXPECT_EQ(base::severity_name(Severity::note), "note");
    EXPECT_EQ(base::severity_name(Severity::help), "help");
    EXPECT_EQ(base::severity_name(Severity::warning), "warning");
    EXPECT_EQ(base::severity_name(Severity::error), "error");
    EXPECT_EQ(base::severity_name(Severity::fatal), "fatal");
    EXPECT_EQ(base::severity_name(static_cast<Severity>(BASE_TEST_UNKNOWN_SEVERITY_VALUE)), "unknown");
    EXPECT_EQ(base::diagnostic_category_name(DiagnosticCategory::general), "general");
    EXPECT_EQ(base::diagnostic_category_name(DiagnosticCategory::lexer), "lexer");
    EXPECT_EQ(base::diagnostic_category_name(DiagnosticCategory::parser), "parser");
    EXPECT_EQ(base::diagnostic_category_name(DiagnosticCategory::semantic), "semantic");
    EXPECT_EQ(base::diagnostic_category_name(DiagnosticCategory::type), "type");
    EXPECT_EQ(base::diagnostic_category_name(DiagnosticCategory::name_resolution), "name_resolution");
    EXPECT_EQ(base::diagnostic_category_name(DiagnosticCategory::visibility), "visibility");
    EXPECT_EQ(base::diagnostic_category_name(DiagnosticCategory::pattern), "pattern");
    EXPECT_EQ(base::diagnostic_category_name(DiagnosticCategory::safety), "safety");
    EXPECT_EQ(base::diagnostic_category_name(DiagnosticCategory::unsupported), "unsupported");
    EXPECT_EQ(base::diagnostic_category_name(DiagnosticCategory::capability), "capability");
    EXPECT_EQ(base::diagnostic_category_name(DiagnosticCategory::module), "module");
    EXPECT_EQ(base::diagnostic_category_name(DiagnosticCategory::internal), "internal");
    EXPECT_EQ(
        base::diagnostic_category_name(static_cast<DiagnosticCategory>(BASE_TEST_UNKNOWN_DIAGNOSTIC_CATEGORY_VALUE)),
        "unknown");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::none), "none");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::lexer_invalid_token), "LEX0001");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::lexer_error_budget), "LEX0002");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::parser_syntax), "PAR0001");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::parser_note), "PAR0002");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::semantic_error), "SEM0001");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::semantic_type_mismatch), "SEM0100");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::semantic_lookup), "SEM0200");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::semantic_duplicate), "SEM0201");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::semantic_visibility), "SEM0202");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::semantic_unsupported), "SEM0300");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::semantic_unsafe_required), "SEM0400");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::semantic_capability), "SEM0450");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::semantic_pattern), "SEM0500");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::semantic_pattern_exhaustiveness), "SEM0501");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::semantic_pattern_unreachable), "SEM0502");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::module_error), "MOD0001");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::internal_contract), "INT0001");
    EXPECT_EQ(base::diagnostic_label_style_name(base::DiagnosticLabelStyle::primary), "primary");
    EXPECT_EQ(base::diagnostic_label_style_name(base::DiagnosticLabelStyle::secondary), "secondary");
    EXPECT_EQ(base::diagnostic_label_style_name(
                  static_cast<base::DiagnosticLabelStyle>(BASE_TEST_UNKNOWN_DIAGNOSTIC_LABEL_STYLE_VALUE)),
        "unknown");
    EXPECT_EQ(base::diagnostic_code_name(DiagnosticCode::internal_contract), "INT0001");
    EXPECT_EQ(
        base::diagnostic_code_name(static_cast<DiagnosticCode>(BASE_TEST_UNKNOWN_DIAGNOSTIC_CODE_VALUE)), "unknown");

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

TEST(CoreUnit, CheckedIntegerHelpersRejectOverflow)
{
    EXPECT_TRUE(base::fits_u32(0U));
    EXPECT_TRUE(base::fits_u32(base::BASE_U32_MAX_AS_USIZE));
    EXPECT_FALSE(base::fits_u32(base::BASE_U32_MAX_AS_USIZE + 1U));
    EXPECT_EQ(
        base::checked_u32(base::BASE_U32_MAX_AS_USIZE, "unit checked u32"), std::numeric_limits<base::u32>::max());
    EXPECT_THROW(
        static_cast<void>(base::checked_u32(base::BASE_U32_MAX_AS_USIZE + 1U, "unit checked u32")), std::length_error);
    EXPECT_EQ(base::checked_add_usize(2U, 3U, "unit checked add"), 5U);
    EXPECT_EQ(base::checked_mul_usize(4U, 5U, "unit checked mul"), 20U);
    EXPECT_THROW(
        static_cast<void>(base::checked_add_usize(std::numeric_limits<base::usize>::max(), 1U, "unit checked add")),
        std::length_error);
    EXPECT_THROW(
        static_cast<void>(base::checked_mul_usize(std::numeric_limits<base::usize>::max(), 2U, "unit checked mul")),
        std::length_error);
}

TEST(CoreUnit, SourceFileLineTableHandlesOffsetsAndExtents)
{
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

    const base::SourceId crlf_id = sources.add_source("crlf.ax", "alpha\r\nbeta\r\n");
    const base::SourceFile& crlf_file = sources.get(crlf_id);
    const base::SourceLineExtent crlf_first = crlf_file.line_extent(0);
    EXPECT_EQ(crlf_file.text().substr(crlf_first.begin, crlf_first.end - crlf_first.begin), "alpha");
    const base::SourceLineExtent crlf_second = crlf_file.line_extent(7);
    EXPECT_EQ(crlf_file.text().substr(crlf_second.begin, crlf_second.end - crlf_second.begin), "beta");
}

TEST(CoreUnit, BumpAllocatorCopiesStringsAndResetsWholeArena)
{
    base::BumpAllocator arena(BASE_TEST_BUMP_SMALL_BLOCK_BYTES);

    EXPECT_EQ(arena.allocate(0), nullptr);
    arena.reserve(0);

    void* const aligned = arena.allocate(1, BASE_TEST_BUMP_ALIGNMENT);
    ASSERT_NE(aligned, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(aligned) % BASE_TEST_BUMP_ALIGNMENT, 0U);

    void* const normalized = arena.allocate(1, BASE_TEST_BUMP_NON_POWER_ALIGNMENT);
    ASSERT_NE(normalized, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(normalized) % BASE_TEST_BUMP_NORMALIZED_ALIGNMENT, 0U);
    EXPECT_THROW(static_cast<void>(arena.allocate(1, BASE_TEST_BUMP_HUGE_ALIGNMENT)), std::bad_array_new_length);

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

TEST(CoreUnit, BumpAllocatorMoveTransfersBlocksAndStats)
{
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

    base::BumpAllocator* const assigned_alias = &assigned;
    assigned = std::move(*assigned_alias);
    EXPECT_GT(assigned.allocated_bytes(), 0U);
    EXPECT_EQ(assigned.copy_string("self-assigned"), "self-assigned");
}

TEST(CoreUnit, BumpAllocatorTouchedReservePreallocatesWritableStorage)
{
    base::BumpAllocator arena(BASE_TEST_BUMP_SMALL_BLOCK_BYTES);

    arena.reserve_touched(0);
    EXPECT_EQ(arena.block_count(), 0U);

    arena.reserve_touched(BASE_TEST_BUMP_TOUCHED_RESERVE_BYTES);
    EXPECT_EQ(arena.block_count(), 1U);
    EXPECT_GE(arena.allocated_bytes(), BASE_TEST_BUMP_TOUCHED_RESERVE_BYTES);

    const base::usize blocks_after_reserve = arena.block_count();
    void* const first = arena.allocate(BASE_TEST_BUMP_TOUCHED_ALLOC_BYTES);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(arena.block_count(), blocks_after_reserve);

    const base::usize blocks_after_first_alloc = arena.block_count();
    arena.reserve_touched(BASE_TEST_BUMP_TOUCHED_ALLOC_BYTES);
    EXPECT_EQ(arena.block_count(), blocks_after_first_alloc);

    arena.reset();
    EXPECT_EQ(arena.block_count(), 0U);
    EXPECT_EQ(arena.used_bytes(), 0U);
    arena.reserve_touched(BASE_TEST_BUMP_TOUCHED_ALLOC_BYTES);
    EXPECT_EQ(arena.block_count(), 1U);
    EXPECT_EQ(arena.used_bytes(), 0U);
}

TEST(CoreUnit, BumpAllocatorAdapterBacksStandardVectors)
{
    base::BumpAllocator arena(BASE_TEST_BUMP_SMALL_BLOCK_BYTES);
    base::BumpVector<int> values{base::BumpAllocatorAdapter<int>{arena}};
    values.reserve(4);
    values.push_back(1);
    values.push_back(2);
    values.push_back(3);

    EXPECT_GT(arena.allocated_bytes(), 0U);
    EXPECT_GT(arena.block_count(), 0U);
    EXPECT_EQ(values.back(), 3);

    base::BumpAllocator copy_arena(BASE_TEST_BUMP_SMALL_BLOCK_BYTES);
    base::BumpVector<int> copied{base::BumpAllocatorAdapter<int>{copy_arena}};
    copied = values;

    ASSERT_EQ(copied.size(), values.size());
    EXPECT_EQ(copied.front(), 1);
    EXPECT_EQ(copied.back(), 3);
    EXPECT_GT(copy_arena.allocated_bytes(), 0U);

    base::BumpVector<int> detached{base::BumpAllocatorAdapter<int>::heap_backed()};
    detached.push_back(4);
    EXPECT_EQ(detached.front(), 4);

    base::BumpVector<int> invalid{base::BumpAllocatorAdapter<int>::strict_empty()};
    EXPECT_THROW(invalid.push_back(5), std::bad_alloc);
}

TEST(CoreUnit, BumpAllocatorAdapterBacksStandardContainers)
{
    base::BumpAllocator arena(BASE_TEST_BUMP_SMALL_BLOCK_BYTES);

    base::BumpDeque<int> queue{base::BumpAllocatorAdapter<int>{arena}};
    queue.push_back(7);
    queue.push_front(3);
    ASSERT_EQ(queue.size(), 2U);
    EXPECT_EQ(queue.front(), 3);
    EXPECT_EQ(queue.back(), 7);

    base::BumpString text{base::BumpAllocatorAdapter<char>{arena}};
    text.assign("arena-text");
    EXPECT_EQ(text, "arena-text");

    base::BumpUnorderedMap<int, int> values{
        0, std::hash<int>{}, std::equal_to<int>{}, base::BumpAllocatorAdapter<std::pair<const int, int>>{arena}};
    values.emplace(1, 11);
    values.emplace(2, 22);
    ASSERT_EQ(values.size(), 2U);
    EXPECT_EQ(values.at(2), 22);

    base::BumpUnorderedSet<int> keys{0, std::hash<int>{}, std::equal_to<int>{}, base::BumpAllocatorAdapter<int>{arena}};
    keys.insert(5);
    keys.insert(9);
    EXPECT_TRUE(keys.contains(5));
    EXPECT_TRUE(keys.contains(9));

    EXPECT_GT(arena.allocated_bytes(), 0U);
    EXPECT_GT(arena.block_count(), 0U);
}

} // namespace aurex::test
