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

[[nodiscard]] std::vector<ExampleApp> m1_examples() {
    return {
        ExampleApp {
            "m1_frontend",
            examples_root() / "m1" / "frontend" / "main.ax",
            "m1 frontend ok",
        },
        ExampleApp {
            "m1_axbuild",
            examples_root() / "m1" / "axbuild" / "main.ax",
            "m1 axbuild ok",
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

TEST_F(AurexIntegrationTest, M1ExamplesCompileAndRun) {
    for (const ExampleApp& app : m1_examples()) {
        const fs::path output = test_bin_root() / std::string(app.name);
        require_success(aurexc() + " " + q(app.source) + " -o " + q(output));
        expect_contains(require_success(q(output)).output, app.expected_output);
    }
}

TEST_F(AurexIntegrationTest, M1ExamplesExposeAcceptanceFeatureSet) {
    const fs::path frontend = examples_root() / "m1" / "frontend" / "main.ax";
    const std::string frontend_checked = require_success(aurexc() + " --emit=checked " + q(frontend)).output;
    expect_contains_all(frontend_checked, {
        "struct SourceManager fields=1",
        "struct Lexer fields=4",
        "struct TokenStream fields=2",
        "fn run_frontend -> std.core.result.Result<i32, i32>",
        "fn method m1.frontend.main.Lexer.lex_all -> std.core.result.Result<i32, i32>",
        "case TokenKind_kw_fn",
    });

    const std::string frontend_ir = require_success(aurexc() + " --emit=ir " + q(frontend)).output;
    expect_contains_all(frontend_ir, {
        "record Lexer @m0_m1_frontend_main_Lexer",
        "record SourceManager @m0_m1_frontend_main_SourceManager",
        "fn run_frontend() @m0_m1_frontend_main_run_frontend",
        "call m0_m1_frontend_main_Lexer_lex_all",
    });

    const fs::path axbuild = examples_root() / "m1" / "axbuild" / "main.ax";
    const std::string axbuild_checked = require_success(aurexc() + " --emit=checked " + q(axbuild)).output;
    expect_contains_all(axbuild_checked, {
        "struct Project fields=3",
        "struct Target fields=6",
        "struct CustomCommand fields=2",
        "struct ProcessOutput fields=3",
        "struct FileMetadata fields=5",
        "fn project_build -> std.core.result.Result<bool, i32>",
        "fn project_sources_newer_than -> bool",
        "fn source_newer_than_stamp -> bool",
        "fn method std.sys.process.Command.run -> std.core.result.Result<i32, i32>",
        "fn method std.sys.process.Command.run_capture -> std.core.result.Result<std.sys.process.ProcessOutput, i32>",
        "fn method std.sys.process.ProcessOutput.stdout -> std.core.text.Span<u8>",
        "fn method std.fs.file.FileMetadata.modified_time_ns -> i64",
        "fn metadata -> std.core.result.Result<std.fs.file.FileMetadata, i32>",
        "fn host_file_metadata -> bool @c_name=aurex_std_v0_file_metadata extern_c",
        "fn host_run_process -> i32 @c_name=aurex_std_v0_run_process extern_c",
        "fn host_run_process_capture -> bool @c_name=aurex_std_v0_run_process_capture extern_c",
        "fn host_free_process_output_data -> void @c_name=aurex_std_v0_free_process_output_data extern_c",
        "case TargetKind_executable",
    });

    const std::string axbuild_ir = require_success(aurexc() + " --emit=ir " + q(axbuild)).output;
    expect_contains_all(axbuild_ir, {
        "call aurex_std_v0_file_metadata",
        "fn host_run_process_capture(program: *const u8",
        "call aurex_std_v0_run_process_capture",
        "call aurex_std_v0_free_process_output_data",
        "call m0_std_fs_file_FileMetadata_modified_time_ns",
        "call m0_m1_axbuild_main_project_sources_newer_than",
        "call m0_m1_axbuild_main_source_newer_than_stamp",
        "call m0_std_sys_process_Command_run_capture",
        "call m0_std_sys_process_ProcessOutput_stdout",
        "call m0_std_sys_process_ProcessOutput_destroy",
        "call m0_m1_axbuild_main_CustomCommand_run",
    });
}

TEST_F(AurexIntegrationTest, ExamplesDocumentationAndLibrariesArePresent) {
    const std::vector<fs::path> required = {
        examples_root() / "README.md",
        examples_root() / "hello.ax",
        examples_root() / "m1" / "frontend" / "main.ax",
        examples_root() / "m1" / "axbuild" / "main.ax",
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
