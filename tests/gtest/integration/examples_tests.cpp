#include "support/test_support.hpp"

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
    EXPECT_EQ(require_success(q(output)).output, "hello from Aurex M0\n");
}

TEST_F(AurexIntegrationTest, ExamplesCommonLibrariesRemainLanguageCoreOnly) {
    const fs::path source = tmp_root() / "examples_common_usage.ax";
    std::ofstream out(source);
    out
        << "module examples_common_usage;\n"
        << "import common.algorithms;\n"
        << "import common.metrics;\n"
        << "import common.result;\n"
        << "import common.status;\n"
        << "fn main() -> i32 {\n"
        << "    let health: Health = Health.from_errors(2);\n"
        << "    let counter: Counter<Count> = counter_i32(5, default_limit);\n"
        << "    let result: Outcome<i32> = Outcome.ok(counter_delta(counter));\n"
        << "    if is_even(4) && health.code() == 1 && outcome_code_i32(result) == 3 {\n"
        << "        return 0;\n"
        << "    }\n"
        << "    return 1;\n"
        << "}\n";
    out.close();

    const std::string checked = require_success(aurexc() + " " + examples_import_flags() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "case Health_degraded : common.status.Health",
        "struct common.metrics.Counter<i32>",
        "case Outcome<i32>_ok",
        "fn is_even -> bool",
    });

    const fs::path output = test_bin_root() / "examples_common_usage";
    require_success(aurexc() + " " + examples_import_flags() + " " + q(source) + " -o " + q(output));
    EXPECT_EQ(require_success(q(output)).output, "");
}

TEST_F(AurexIntegrationTest, ExamplesDocumentationAndLibrariesArePresent) {
    EXPECT_TRUE(fs::exists(examples_root() / "README.md"));
    EXPECT_TRUE(fs::exists(examples_root() / "hello.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "common" / "algorithms.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "common" / "metrics.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "common" / "result.ax"));
    EXPECT_TRUE(fs::exists(examples_root() / "libs" / "common" / "status.ax"));
    EXPECT_FALSE(fs::exists(examples_root() / "system"));
    EXPECT_FALSE(fs::exists(examples_root() / "m1"));
}

} // namespace aurex::test
