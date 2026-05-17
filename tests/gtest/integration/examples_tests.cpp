#include <support/test_support.hpp>

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
        "struct Regex fields=10",
        "struct MatchResult fields=4",
        "case RegexStatus_repeat_too_large",
        "fn compile -> regex.types.Regex",
        "fn search_compiled -> regex.types.MatchResult",
        "fn method regex.types.Regex.valid -> bool",
        "fn method regex.types.MatchResult.ok -> bool",
        "type priv Matcher = fn(str, str) -> bool",
    });
    require_success(aurexc() + " " + examples_import_flags() + " " + q(examples_root() / "regex_demo.ax") + " -o " + q(output));
    EXPECT_EQ(require_success(q(output)).output, "");
}

TEST_F(AurexIntegrationTest, ExamplesDocumentationAndLibrariesArePresent) {
    EXPECT_TRUE(fs::exists(examples_root() / "README.md"));
    EXPECT_TRUE(fs::exists(examples_root() / "hello.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "common" / "algorithms.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "common" / "metrics.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "common" / "result.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "common" / "status.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "api.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "alloc.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "ascii.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "engine.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "parser.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "program.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "regex" / "types.ax"));
    EXPECT_FALSE(fs::exists(examples_root() / "libs" / "text" / "regex.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "regex_demo.ax"));
    EXPECT_FALSE(fs::exists(examples_root() / "system"));
    EXPECT_FALSE(fs::exists(examples_root() / "m1"));
}

} // namespace aurex::test
