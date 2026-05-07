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
        "struct GraphDiagnostic fields=3",
        "struct CustomCommand fields=2",
        "struct ProcessOutput fields=5",
        "struct HostProcessOutput fields=5",
        "struct FileMetadata fields=5",
        "fn graph_diagnostic -> m1.axbuild.main.GraphDiagnostic",
        "fn graph_diagnostic_message -> *const u8",
        "fn graph_diagnostic_related_index -> usize",
        "fn graph_diagnostic_status -> m1.axbuild.main.GraphStatus",
        "fn graph_diagnostic_target_index -> usize",
        "fn graph_status_message -> *const u8",
        "fn project_build -> std.core.result.Result<bool, i32>",
        "fn project_build_order -> std.core.result.Result<std.core.vec.Vec<usize>, i32>",
        "fn project_cycle_diagnostic -> std.core.result.Result<m1.axbuild.main.GraphDiagnostic, i32>",
        "fn project_cycle_diagnostic_visit -> m1.axbuild.main.GraphDiagnostic",
        "fn project_cycle_path_from_diagnostic -> std.core.result.Result<std.core.vec.Vec<usize>, i32>",
        "fn project_cycle_path_matches -> std.core.result.Result<bool, i32>",
        "fn project_cycle_path_names_from_diagnostic -> std.core.result.Result<std.core.vec.Vec<*const u8>, i32>",
        "fn project_cycle_path_names_match -> std.core.result.Result<bool, i32>",
        "fn project_dependencies_in_bounds -> bool",
        "fn project_duplicate_target_diagnostic -> m1.axbuild.main.GraphDiagnostic",
        "fn project_find_dependency_path -> std.core.result.Result<bool, i32>",
        "fn project_find_target -> std.core.result.Result<usize, i32>",
        "fn project_graph_diagnostic -> std.core.result.Result<m1.axbuild.main.GraphDiagnostic, i32>",
        "fn project_graph_diagnostic_related_name -> *const u8",
        "fn project_graph_diagnostic_target_name -> *const u8",
        "fn project_graph_ok_diagnostic_matches -> std.core.result.Result<bool, i32>",
        "fn project_graph_status -> m1.axbuild.main.GraphStatus",
        "fn project_invalid_dependency_diagnostic -> m1.axbuild.main.GraphDiagnostic",
        "fn project_source_count -> i32",
        "fn project_sources_match_directory -> std.core.result.Result<bool, i32>",
        "fn project_sources_newer_than -> bool",
        "fn project_target_index_matches -> std.core.result.Result<bool, i32>",
        "fn project_target_name_or -> *const u8",
        "fn project_target_names_unique -> bool",
        "fn project_visit_target -> std.core.result.Result<bool, i32>",
        "fn source_newer_than_stamp -> bool",
        "fn validate_cycle_detection -> std.core.result.Result<bool, i32>",
        "fn validate_duplicate_target_detection -> std.core.result.Result<bool, i32>",
        "fn validate_invalid_dependency_detection -> std.core.result.Result<bool, i32>",
        "fn validate_missing_target_lookup -> std.core.result.Result<bool, i32>",
        "fn validate_stderr_capture -> std.core.result.Result<bool, i32>",
        "fn count_files_with_suffix -> std.core.result.Result<i32, i32>",
        "fn method m1.axbuild.main.GraphDiagnostic.message -> *const u8",
        "fn method m1.axbuild.main.GraphDiagnostic.related_index -> usize",
        "fn method m1.axbuild.main.GraphDiagnostic.status -> m1.axbuild.main.GraphStatus",
        "fn method m1.axbuild.main.GraphDiagnostic.target_index -> usize",
        "fn method m1.axbuild.main.Project.build_order -> std.core.result.Result<std.core.vec.Vec<usize>, i32>",
        "fn method m1.axbuild.main.Project.cycle_path -> std.core.result.Result<std.core.vec.Vec<usize>, i32>",
        "fn method m1.axbuild.main.Project.cycle_path_names -> std.core.result.Result<std.core.vec.Vec<*const u8>, i32>",
        "fn method m1.axbuild.main.Project.find_target -> std.core.result.Result<usize, i32>",
        "fn method m1.axbuild.main.Project.graph_diagnostic -> std.core.result.Result<m1.axbuild.main.GraphDiagnostic, i32>",
        "fn method m1.axbuild.main.Project.graph_diagnostic_related_name -> *const u8",
        "fn method m1.axbuild.main.Project.graph_diagnostic_target_name -> *const u8",
        "fn method m1.axbuild.main.Project.graph_status -> m1.axbuild.main.GraphStatus",
        "fn method std.sys.process.Command.run -> std.core.result.Result<i32, i32>",
        "fn method std.sys.process.Command.run_capture -> std.core.result.Result<std.sys.process.ProcessOutput, i32>",
        "fn method std.sys.process.ProcessOutput.stderr -> std.core.text.Span<u8>",
        "fn method std.sys.process.ProcessOutput.stderr_c_data -> *const u8",
        "fn method std.sys.process.ProcessOutput.stdout -> std.core.text.Span<u8>",
        "fn method std.sys.process.ProcessOutput.stdout_c_data -> *const u8",
        "fn process_output_stderr -> std.core.text.Span<u8>",
        "fn process_output_stderr_c_data -> *const u8",
        "fn method std.fs.file.FileMetadata.modified_time_ns -> i64",
        "fn metadata -> std.core.result.Result<std.fs.file.FileMetadata, i32>",
        "fn host_directory_count_files_with_suffix -> bool @c_name=aurex_std_v0_directory_count_files_with_suffix extern_c",
        "fn host_file_metadata -> bool @c_name=aurex_std_v0_file_metadata extern_c",
        "fn host_run_process -> i32 @c_name=aurex_std_v0_run_process extern_c",
        "fn host_run_process_capture -> bool @c_name=aurex_std_v0_run_process_capture extern_c",
        "fn host_free_process_output_data -> void @c_name=aurex_std_v0_free_process_output_data extern_c",
        "case GraphStatus_cycle",
        "case GraphStatus_duplicate_target",
        "case GraphStatus_invalid_dependency",
        "case GraphStatus_ok",
        "case Option<usize>_some",
        "case Result<m1.axbuild.main.GraphDiagnostic, i32>_ok",
        "case Result<std.core.vec.Vec<*const u8>, i32>_ok",
        "case Result<std.core.vec.Vec<usize>, i32>_ok",
        "case TargetKind_executable",
    });

    const std::string axbuild_ir = require_success(aurexc() + " --emit=ir " + q(axbuild)).output;
    expect_contains_all(axbuild_ir, {
        "record GraphDiagnostic @m0_m1_axbuild_main_GraphDiagnostic",
        "call aurex_std_v0_directory_count_files_with_suffix",
        "call aurex_std_v0_file_metadata",
        "fn host_run_process_capture(program: *const u8",
        "call aurex_std_v0_run_process_capture",
        "call aurex_std_v0_free_process_output_data",
        "call m0_std_fs_dir_count_files_with_suffix",
        "call m0_std_fs_file_FileMetadata_modified_time_ns",
        "call m0_m1_axbuild_main_Project_cycle_path",
        "call m0_m1_axbuild_main_Project_cycle_path_names",
        "call m0_m1_axbuild_main_graph_diagnostic",
        "call m0_m1_axbuild_main_graph_diagnostic_message",
        "call m0_m1_axbuild_main_graph_diagnostic_related_index",
        "call m0_m1_axbuild_main_graph_diagnostic_status",
        "call m0_m1_axbuild_main_graph_diagnostic_target_index",
        "call m0_m1_axbuild_main_graph_status_message",
        "call m0_m1_axbuild_main_project_build_order",
        "call m0_m1_axbuild_main_project_cycle_diagnostic",
        "call m0_m1_axbuild_main_project_cycle_diagnostic_visit",
        "call m0_m1_axbuild_main_project_cycle_path_from_diagnostic",
        "call m0_m1_axbuild_main_project_cycle_path_matches",
        "call m0_m1_axbuild_main_project_cycle_path_names_from_diagnostic",
        "call m0_m1_axbuild_main_project_cycle_path_names_match",
        "call m0_m1_axbuild_main_project_dependencies_in_bounds",
        "call m0_m1_axbuild_main_project_duplicate_target_diagnostic",
        "call m0_m1_axbuild_main_project_find_dependency_path",
        "call m0_m1_axbuild_main_project_find_target",
        "call m0_m1_axbuild_main_project_graph_diagnostic",
        "call m0_m1_axbuild_main_project_graph_diagnostic_related_name",
        "call m0_m1_axbuild_main_project_graph_diagnostic_target_name",
        "call m0_m1_axbuild_main_project_graph_ok_diagnostic_matches",
        "call m0_m1_axbuild_main_project_graph_status",
        "call m0_m1_axbuild_main_project_invalid_dependency_diagnostic",
        "call m0_m1_axbuild_main_project_source_count",
        "call m0_m1_axbuild_main_project_sources_match_directory",
        "call m0_m1_axbuild_main_project_sources_newer_than",
        "call m0_m1_axbuild_main_project_target_index_matches",
        "call m0_m1_axbuild_main_project_target_name_or",
        "call m0_m1_axbuild_main_project_target_names_unique",
        "call m0_m1_axbuild_main_source_newer_than_stamp",
        "call m0_m1_axbuild_main_project_visit_target",
        "call m0_m1_axbuild_main_validate_cycle_detection",
        "call m0_m1_axbuild_main_validate_duplicate_target_detection",
        "call m0_m1_axbuild_main_validate_invalid_dependency_detection",
        "call m0_m1_axbuild_main_validate_missing_target_lookup",
        "call m0_m1_axbuild_main_validate_stderr_capture",
        "call m0_std_sys_process_Command_run_capture",
        "call m0_std_sys_process_ProcessOutput_stderr",
        "call m0_std_sys_process_ProcessOutput_stderr_c_data",
        "call m0_std_sys_process_ProcessOutput_stdout",
        "call m0_std_sys_process_ProcessOutput_destroy",
        "call m0_std_sys_process_process_output_stderr",
        "call m0_std_sys_process_process_output_stderr_c_data",
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
