#include <support/test_support.hpp>

#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::test {
namespace {

constexpr std::string_view CLI_ARGUMENT_TEST_SUCCESS_SCRIPT = "#!/bin/sh\nexit 0\n";
constexpr std::string_view CLI_ARGUMENT_TEST_HELLO = "hello.ax";
constexpr std::string_view CLI_ARGUMENT_TEST_INVALID_OPT_LEVEL = "bad";

void write_executable_script(const fs::path& path, const std::string_view contents)
{
    fs::create_directories(path.parent_path());
    {
        std::ofstream out(path, std::ios::binary);
        ASSERT_TRUE(out.is_open());
        out << contents;
    }
    std::error_code error;
    fs::permissions(path, fs::perms::owner_all, fs::perm_options::replace, error);
    ASSERT_FALSE(error) << error.message();
}

[[nodiscard]] std::string real_aurexc(const std::string& args = "")
{
    return "/bin/sh -c " + q(std::string_view{aurexc_path().string() + args});
}

} // namespace

TEST_F(AurexIntegrationTest, CliArgumentDiagnosticsCoverParseBranches)
{
    expect_contains(require_success(aurexc() + " -h").output, "usage:");

    expect_contains(require_failure(aurexc()).output, "usage:");
    expect_contains(require_failure(aurexc() + " --unknown").output, "usage:");
    expect_contains(require_failure(aurexc() + " -o").output, "usage:");
    expect_contains(require_failure(aurexc() + " -I").output, "usage:");
    expect_contains(require_failure(aurexc() + " --clang").output, "usage:");
    expect_contains(require_failure(aurexc() + " --clang-arg").output, "usage:");
    expect_contains(require_failure(aurexc() + " --opt-level").output, "usage:");
    expect_contains(require_failure(aurexc() + " --opt-level " + std::string(CLI_ARGUMENT_TEST_INVALID_OPT_LEVEL) + " "
                        + q(source_root() / "examples" / fs::path(CLI_ARGUMENT_TEST_HELLO)))
                        .output,
        "invalid optimization level");
    expect_contains(
        require_failure(aurexc() + " --emit=obj " + q(source_root() / "examples" / fs::path(CLI_ARGUMENT_TEST_HELLO)))
            .output,
        "native output requires -o");
    expect_contains(require_failure(aurexc() + " --dump-tokens " + q(tmp_root() / "missing.ax")).output,
        "failed to open input file");
    expect_contains(require_failure(aurexc() + " " + q(source_root() / "examples" / fs::path(CLI_ARGUMENT_TEST_HELLO))
                        + " " + q(source_root() / "examples" / fs::path(CLI_ARGUMENT_TEST_HELLO)))
                        .output,
        "multiple input files are not supported");

    const fs::path hello = source_root() / "examples" / fs::path(CLI_ARGUMENT_TEST_HELLO);
    const fs::path out = test_bin_root() / "cli_args";
    require_success(aurexc() + " -I" + q(imports_root()) + " --emit=object --clang-arg -fno-color-diagnostics -O2 "
        + q(hello) + " -o " + q(out));
    EXPECT_GT(fs::file_size(out), 0U);
}

TEST_F(AurexIntegrationTest, CliArgumentFormsCoverOptimizationAndNativeEmissionAliases)
{
    const fs::path hello = source_root() / "examples" / fs::path(CLI_ARGUMENT_TEST_HELLO);
    const fs::path import_source = positive_sample("modules", "import_path.ax");
    const fs::path clang_script = tmp_root() / "fake_clang" / "clang";
    write_executable_script(clang_script, CLI_ARGUMENT_TEST_SUCCESS_SCRIPT);

    const std::vector<std::string> check_commands{
        " --emit=tokens " + q(hello),
        " --emit=ast " + q(hello),
        " --emit=modules " + q(hello),
        " --emit=checked " + q(hello),
        " --emit=typed " + q(hello),
        " --emit=ir " + q(hello),
        " --emit=llvm-ir " + q(hello),
        " --emit llvm-ir " + q(hello),
        " --emit=check " + q(hello),
        " --check " + q(hello),
        " -fsyntax-only " + q(hello),
        " -I " + q(imports_root()) + " --emit=llvm-ir " + q(import_source),
        " -I" + q(imports_root()) + " --emit=llvm-ir " + q(import_source),
        " --import-path " + q(imports_root()) + " --emit=llvm-ir " + q(import_source),
        " -O 2 --emit=check " + q(hello),
        " --opt-level=2 --emit=check " + q(hello),
        " -O0 --emit=check " + q(hello),
        " -O1 --emit=check " + q(hello),
        " -O2 --emit=check " + q(hello),
        " -O3 --emit=check " + q(hello),
        " --opt-level 0 --emit=check " + q(hello),
        " --opt-level 1 --emit=check " + q(hello),
        " --opt-level 2 --emit=check " + q(hello),
        " --opt-level 3 --emit=check " + q(hello),
    };

    for (const std::string& command : check_commands) {
        require_success(aurexc() + command);
    }

    const fs::path asm_output = tmp_root() / "native_aliases" / "hello.s";
    const fs::path object_output = tmp_root() / "native_aliases" / "hello.o";
    const fs::path exe_output = tmp_root() / "native_aliases" / "hello";

    require_success(aurexc() + " --clang " + q(clang_script) + " --emit=asm " + q(hello) + " -o " + q(asm_output));
    require_success(aurexc() + " --clang=" + q(clang_script) + " -S " + q(hello) + " -o "
        + q(tmp_root() / "native_aliases" / "hello.driver.s"));
    require_success(
        aurexc() + " --clang " + q(clang_script) + " --emit=object " + q(hello) + " -o " + q(object_output));
    require_success(aurexc() + " --clang=" + q(clang_script) + " --clang-arg=-fno-color-diagnostics -c " + q(hello)
        + " -o " + q(tmp_root() / "native_aliases" / "hello.driver.o"));
    require_success(aurexc() + " --clang " + q(clang_script) + " --emit=exe " + q(hello) + " -o " + q(exe_output));
}

TEST_F(AurexIntegrationTest, CliArgumentDirectBinaryExecutionCoversMainParserBranches)
{
    const fs::path hello = source_root() / "examples" / fs::path(CLI_ARGUMENT_TEST_HELLO);
    const fs::path import_source = positive_sample("modules", "import_path.ax");
    const fs::path clang_script = tmp_root() / "fake_clang_direct" / "clang";
    write_executable_script(clang_script, CLI_ARGUMENT_TEST_SUCCESS_SCRIPT);

    expect_contains(require_success(real_aurexc(" --help")).output, "usage:");
    expect_contains(require_success(real_aurexc(" --version")).output, "0.1.2");

    require_success(real_aurexc(" --dump-tokens " + q(hello)));
    require_success(real_aurexc(" --dump-ast " + q(hello)));
    require_success(real_aurexc(" --dump-modules " + q(hello)));
    require_success(real_aurexc(" --dump-checked " + q(hello)));
    require_success(real_aurexc(" --dump-ir " + q(hello)));
    require_success(real_aurexc(" --check " + q(hello)));

    require_success(real_aurexc(" -O0 --check " + q(hello)));
    require_success(real_aurexc(" -O1 --check " + q(hello)));
    require_success(real_aurexc(" -O2 --check " + q(hello)));
    require_success(real_aurexc(" -O3 --check " + q(hello)));
    require_success(real_aurexc(" -O 2 --check " + q(hello)));
    require_success(real_aurexc(" -I" + imports_root().string() + " --check " + q(import_source)));
    require_success(real_aurexc(" -I " + q(imports_root()) + " --check " + q(import_source)));

    const fs::path asm_output = tmp_root() / "main_parser" / "hello.s";
    const fs::path object_output = tmp_root() / "main_parser" / "hello.o";
    const fs::path exe_output = tmp_root() / "main_parser" / "hello";
    require_success(real_aurexc(" --clang " + q(clang_script) + " --clang-arg -fno-color-diagnostics --emit=asm "
        + q(hello) + " -o " + q(asm_output)));
    require_success(real_aurexc(" --clang " + q(clang_script) + " --clang-arg -fno-color-diagnostics --emit=obj "
        + q(hello) + " -o " + q(object_output)));
    require_success(real_aurexc(" --clang " + q(clang_script) + " --clang-arg -fno-color-diagnostics --emit=exe "
        + q(hello) + " -o " + q(exe_output)));

    expect_contains(require_failure(real_aurexc()).output, "usage:");
    expect_contains(require_failure(real_aurexc(" --unknown")).output, "usage:");
    expect_contains(require_failure(real_aurexc(" -o")).output, "usage:");
    expect_contains(require_failure(real_aurexc(" -I")).output, "usage:");
    expect_contains(require_failure(real_aurexc(" --clang")).output, "usage:");
    expect_contains(require_failure(real_aurexc(" --clang-arg")).output, "usage:");
    expect_contains(require_failure(real_aurexc(" --opt-level")).output, "usage:");
    expect_contains(require_failure(real_aurexc(" --opt-level bad " + q(hello))).output, "invalid optimization level");
    expect_contains(
        require_failure(real_aurexc(" " + q(hello) + " " + q(hello))).output, "multiple input files are not supported");
}

} // namespace aurex::test
