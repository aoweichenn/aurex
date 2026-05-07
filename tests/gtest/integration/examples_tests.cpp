#include "support/test_support.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace aurex::test {
namespace {

struct ExampleApp {
    std::string_view name;
    fs::path source;
    std::string_view expected_output;
};

[[nodiscard]] fs::path examples_root() {
    return source_root() / "examples";
}

[[nodiscard]] std::string examples_import_flags() {
    return "-I " + q(examples_root() / "libs");
}

[[nodiscard]] std::vector<ExampleApp> system_examples() {
    return {
        ExampleApp {
            "cli_probe",
            examples_root() / "system" / "cli_probe" / "main.ax",
            "cli probe ok",
        },
        ExampleApp {
            "file_journal",
            examples_root() / "system" / "file_journal" / "main.ax",
            "file journal ok",
        },
        ExampleApp {
            "memory_probe",
            examples_root() / "system" / "memory_probe" / "main.ax",
            "memory probe ok",
        },
    };
}

} // namespace

TEST_F(AurexIntegrationTest, SystemExamplesCompileAndRun) {
    for (const ExampleApp& app : system_examples()) {
        if (app.name == "cli_probe") {
            const fs::path output = test_bin_root() / std::string(app.name);
            require_success(aurexc() + " " + examples_import_flags() + " " + q(app.source) + " -o " + q(output));
            expect_contains(require_success(q(output)).output, app.expected_output);
            continue;
        }
        require_success(aurexc() + " " + examples_import_flags() + " --emit=llvm-ir " + q(app.source));
    }
}

TEST_F(AurexIntegrationTest, SystemExamplesExposeCurrentFeatureSet) {
    const fs::path file_journal = examples_root() / "system" / "file_journal" / "main.ax";
    const std::string checked = require_success(aurexc() + " " + examples_import_flags() + " --emit=checked " + q(file_journal)).output;
    expect_contains_all(checked, {
        "fn write_entry -> common.result.Outcome<i32>",
        "struct JournalEntry fields=3",
        "type Count = i32",
        "fn method common.status.Health.code -> i32",
        "fn method common.status.Health.healthy -> bool",
        "case Outcome<i32>_ok",
        "case Health_degraded",
    });

    const fs::path memory_probe = examples_root() / "system" / "memory_probe" / "main.ax";
    const std::string ir = require_success(aurexc() + " " + examples_import_flags() + " --emit=ir " + q(memory_probe)).output;
    expect_contains_all(ir, {
        "record PacketPage",
        ".bytes: [8]u8",
        "ptr_addr",
        "ptr_from_addr",
        "field_addr",
        "phi [",
    });
}

TEST_F(AurexIntegrationTest, ExamplesDocumentationAndLibrariesArePresent) {
    const std::vector<fs::path> required = {
        examples_root() / "README.md",
        examples_root() / "hello.ax",
        examples_root() / "libs" / "common" / "algorithms.ax",
        examples_root() / "libs" / "common" / "prelude.ax",
        examples_root() / "libs" / "common" / "status.ax",
        examples_root() / "libs" / "common" / "result.ax",
        examples_root() / "libs" / "common" / "metrics.ax",
        examples_root() / "libs" / "common" / "buffers.ax",
    };
    for (const fs::path& path : required) {
        EXPECT_TRUE(fs::exists(path)) << path;
    }
}

} // namespace aurex::test
