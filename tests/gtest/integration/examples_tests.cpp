#include <support/test_support.hpp>

#include <cstddef>
#include <fstream>
#include <string>
#include <string_view>

namespace aurex::test {
namespace {

[[nodiscard]] fs::path examples_root() {
    return source_root() / "examples";
}

[[nodiscard]] std::string examples_import_flags() {
    return "-I " + q(examples_root() / "libs");
}

constexpr std::size_t REGEX_MAX_PATTERN_BYTES = 4096U;
constexpr std::size_t REGEX_OVERSIZED_PATTERN_BYTES = REGEX_MAX_PATTERN_BYTES + 1U;

} // namespace

TEST_F(AurexIntegrationTest, ExamplesHelloCompilesAndRuns) {
    const fs::path output = test_bin_root() / "hello_example";
    require_success(aurexc() + " " + q(examples_root() / "hello.ax") + " -o " + q(output));
    EXPECT_EQ(require_success(q(output)).output, "hello from Aurex M2\n");
}

TEST_F(AurexIntegrationTest, ExamplesCommonLibrariesRemainLanguageCoreOnly) {
    const fs::path source = tmp_root() / "examples_common_usage.ax";
    std::ofstream out(source);
    out
        << "module examples_common_usage;\n"
        << "import common.algorithms as algorithms;\n"
        << "import common.metrics as metrics;\n"
        << "import common.result as result;\n"
        << "import common.status as status;\n"
        << "fn main() -> i32 {\n"
        << "    let health: status.Health = status.Health.from_errors(2);\n"
        << "    let counter: metrics.CounterI32 = metrics.counter_i32(5, metrics.default_limit);\n"
        << "    let outcome: result.OutcomeI32 = result.OutcomeI32.ok(metrics.counter_delta(counter));\n"
        << "    if algorithms.is_even(4) && health.code() == 1 && result.outcome_code_i32(outcome) == 3 {\n"
        << "        return 0;\n"
        << "    }\n"
        << "    return 1;\n"
        << "}\n";
    out.close();

    const std::string checked = require_success(aurexc() + " " + examples_import_flags() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "case Health_degraded : common.status.Health",
        "struct CounterI32 fields=3",
        "case OutcomeI32_ok",
        "fn is_even -> bool",
    });

    const fs::path output = test_bin_root() / "examples_common_usage";
    require_success(aurexc() + " " + examples_import_flags() + " " + q(source) + " -o " + q(output));
    EXPECT_EQ(require_success(q(output)).output, "");
}

TEST_F(AurexIntegrationTest, ExamplesRegexLibraryCompilesAndRuns) {
    const fs::path output = test_bin_root() / "regex_demo";
    const std::string checked =
        require_success(aurexc() + " " + examples_import_flags() + " --emit=checked " + q(examples_root() / "regex_demo.ax")).output;
    expect_contains_all(checked, {
        "struct Regex fields=15",
        "struct MatchResult fields=4",
        "struct Captures fields=7",
        "struct CaptureSpan fields=3",
        "struct FindIter fields=5",
        "struct SplitIter fields=6",
        "struct ReplaceResult fields=4",
        "case RegexStatus_pattern_too_large",
        "case RegexStatus_program_too_large",
        "case RegexStatus_repeat_too_large",
        "case RegexStatus_workspace_too_large",
        "case RegexStatus_buffer_too_small",
        "fn compile -> regex.core.types.Regex",
        "fn search_compiled -> regex.core.types.MatchResult",
        "fn captures_compiled -> regex.core.types.Captures",
        "fn replace_all -> regex.core.types.ReplaceResult",
        "fn program_bytes -> usize",
        "fn workspace_bytes -> usize",
        "fn max_bounded_repeat -> usize",
        "fn method regex.core.types.Regex.valid -> bool",
        "fn method regex.core.types.MatchResult.ok -> bool",
        "type priv Matcher = fn(str, str) -> bool",
    });
    require_success(aurexc() + " " + examples_import_flags() + " " + q(examples_root() / "regex_demo.ax") + " -o " + q(output));
    EXPECT_EQ(require_success(q(output)).output, "");
}

TEST_F(AurexIntegrationTest, ExamplesRegexPhase1CompilesAndRuns) {
    const fs::path output = test_bin_root() / "regex_phase1";
    const std::string checked =
        require_success(aurexc() + " " + examples_import_flags() + " --emit=checked " + q(examples_root() / "regex_phase1.ax")).output;
    expect_contains_all(checked, {
        "struct Captures fields=7",
        "struct FindIter fields=5",
        "struct CaptureIter fields=5",
        "struct SplitPart fields=4",
        "struct ReplaceResult fields=4",
        "case RegexStatus_invalid_group_name",
        "case RegexStatus_buffer_too_small",
        "fn capture_index -> usize",
        "fn capture_text -> str",
        "fn find_iter -> regex.core.types.FindIter",
        "fn captures_iter -> regex.core.types.CaptureIter",
        "fn split_iter -> regex.core.types.SplitIter",
        "fn replace_all -> regex.core.types.ReplaceResult",
        "fn error_offset -> usize",
        "fn error_kind_code -> i32",
    });
    require_success(aurexc() + " " + examples_import_flags() + " " + q(examples_root() / "regex_phase1.ax") + " -o " + q(output));
    EXPECT_EQ(require_success(q(output)).output, "");
}

TEST_F(AurexIntegrationTest, ExamplesRegexStressCompilesAndRuns) {
    const fs::path output = test_bin_root() / "regex_stress";
    const std::string checked =
        require_success(aurexc() + " " + examples_import_flags() + " --emit=checked " + q(examples_root() / "regex_stress.ax")).output;
    expect_contains_all(checked, {
        "type priv CompiledCheck = fn(&regex.core.types.Regex, str) -> regex.core.types.MatchResult",
        "struct priv StressCase fields=4",
        "fn priv run_repeated_searches -> i32",
        "fn priv run_repeated_fullmatches -> i32",
        "fn priv require_budget -> i32",
        "fn state_count -> usize",
        "fn range_count -> usize",
        "fn program_bytes -> usize",
        "fn workspace_bytes -> usize",
        "fn max_state_capacity -> usize",
    });
    require_success(aurexc() + " " + examples_import_flags() + " " + q(examples_root() / "regex_stress.ax") + " -o " + q(output));
    EXPECT_EQ(require_success(q(output)).output, "");
}

TEST_F(AurexIntegrationTest, ExamplesRegexRejectsOversizedPattern) {
    const fs::path source = tmp_root() / "regex_oversized_pattern.ax";
    std::ofstream out(source);
    out
        << "module regex_oversized_pattern;\n"
        << "import regex.api as regex;\n"
        << "fn main() -> i32 {\n"
        << "    var compiled: regex.Regex = regex.compile(\""
        << std::string(REGEX_OVERSIZED_PATTERN_BYTES, 'a')
        << "\");\n"
        << "    defer regex.destroy(&mut compiled);\n"
        << "    let rejected: bool = match compiled.status {\n"
        << "        .pattern_too_large => true,\n"
        << "        _ => false,\n"
        << "    };\n"
        << "    if rejected && regex.max_pattern_bytes() == "
        << REGEX_MAX_PATTERN_BYTES
        << "usize {\n"
        << "        return 0;\n"
        << "    }\n"
        << "    return 1 + regex.status_code(compiled.status);\n"
        << "}\n";
    out.close();

    const fs::path output = test_bin_root() / "regex_oversized_pattern";
    require_success(aurexc() + " " + examples_import_flags() + " " + q(source) + " -o " + q(output));
    EXPECT_EQ(require_success(q(output)).output, "");
}

TEST_F(AurexIntegrationTest, ExamplesDiagnosticShowcaseEmitsRepresentativeDiagnostics) {
    const fs::path showcase = examples_root() / "diagnostic_showcase.ax";
    const std::string text = require_failure(aurexc() + " --check " + q(showcase)).output;
    expect_contains_all(text, {
        "duplicate value definition in module diagnostic_showcase: duplicate_answer",
        "previous declaration of `duplicate_answer` is here",
        "initializer type does not match declared type",
        "unknown field in struct literal: missing",
        "integer literal out of range for u8",
        "logical not requires bool operand",
        "comparison operator requires numeric operands",
        "unknown name: typo_value",
        "if condition must be bool",
        "break and continue are only valid inside loops",
        "cannot infer generic type argument `T` for call to phantom",
        "raw pointer dereference requires unsafe context",
        "call to unsafe function read_raw requires unsafe context",
        "match expression is not exhaustive for enum case: Choice_one",
    });

    const std::string json = require_failure(
        aurexc() + " --check --diagnostics=json " + q(showcase)
    ).output;
    expect_contains_all(json, {
        "\"format\": \"aurex-diagnostics-v1\"",
        "\"category\": \"name_resolution\"",
        "\"category\": \"type\"",
        "\"category\": \"semantic\"",
        "\"category\": \"safety\"",
        "\"category\": \"pattern\"",
        "\"code\": \"SEM0201\"",
        "\"code\": \"SEM0100\"",
        "\"code\": \"SEM0400\"",
        "\"code\": \"SEM0501\"",
        "\"suppressed\": 0",
    });
}

TEST_F(AurexIntegrationTest, ExamplesDocumentationAndLibrariesArePresent) {
    EXPECT_TRUE(fs::exists(examples_root() / "README.md"));
    EXPECT_TRUE(fs::exists(examples_root() / "hello.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "diagnostic_showcase.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "common" / "algorithms.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "common" / "metrics.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "common" / "result.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "common" / "status.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "api.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "compile" / "parser.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "compile" / "program.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "config" / "limits.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "core" / "results.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "core" / "types.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "ops" / "iter.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "ops" / "replace.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "ops" / "split.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "runtime" / "alloc.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "syntax" / "ascii.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "vm" / "engine.ax"));
    EXPECT_FALSE(fs::exists(examples_root() / "libs" / "regex" / "alloc.ax"));
    EXPECT_FALSE(fs::exists(examples_root() / "libs" / "regex" / "ascii.ax"));
    EXPECT_FALSE(fs::exists(examples_root() / "libs" / "regex" / "engine.ax"));
    EXPECT_FALSE(fs::exists(examples_root() / "libs" / "regex" / "parser.ax"));
    EXPECT_FALSE(fs::exists(examples_root() / "libs" / "regex" / "program.ax"));
    EXPECT_FALSE(fs::exists(examples_root() / "libs" / "regex" / "types.ax"));
    EXPECT_FALSE(fs::exists(examples_root() / "libs" / "text" / "regex.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "regex_demo.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "regex_phase1.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "regex_stress.ax"));
    EXPECT_FALSE(fs::exists(examples_root() / "system"));
    EXPECT_FALSE(fs::exists(examples_root() / "m1"));
}

} // namespace aurex::test
