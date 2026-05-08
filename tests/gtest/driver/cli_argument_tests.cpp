#include "support/test_support.hpp"

namespace aurex::test {

TEST_F(AurexIntegrationTest, CliArgumentDiagnosticsCoverParseBranches) {
    expect_contains(require_success(aurexc() + " -h").output, "usage:");

    expect_contains(require_failure(aurexc()).output, "usage:");
    expect_contains(require_failure(aurexc() + " --unknown").output, "usage:");
    expect_contains(require_failure(aurexc() + " -o").output, "usage:");
    expect_contains(require_failure(aurexc() + " -I").output, "usage:");
    expect_contains(require_failure(aurexc() + " --clang").output, "usage:");
    expect_contains(require_failure(aurexc() + " --clang-arg").output, "usage:");
    expect_contains(require_failure(aurexc() + " --opt-level").output, "usage:");
    expect_contains(require_failure(aurexc() + " --opt-level bad " + q(source_root() / "examples" / "hello.ax")).output, "invalid optimization level");

    const fs::path hello = source_root() / "examples" / "hello.ax";
    const fs::path out = test_bin_root() / "cli_args";
    require_success(
        aurexc() + " -I" + q(imports_root()) +
        " --emit=object --clang-arg -fno-color-diagnostics -O2 " +
        q(hello) + " -o " + q(out)
    );
    EXPECT_GT(fs::file_size(out), 0U);
}

} // namespace aurex::test
