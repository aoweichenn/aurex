#include "support/test_support.hpp"

#include <set>
#include <string>

namespace aurex::test {
namespace {

const std::set<std::string>& skip_regular_samples() {
    static const std::set<std::string> value = {
        "import_alias_qualified_call",
        "import_path",
        "generic_function_import",
        "math",
        "module_name_collision",
        "qualified_generic_inference_import",
        "qualified_generic_substitution",
        "reexport_import",
        "std_text",
        "std_mem",
        "std_file",
        "type_alias_import",
        "visibility_import",
    };
    return value;
}

driver::CompilerInvocation sample_invocation(const fs::path& src, const driver::EmitKind emit_kind) {
    driver::CompilerInvocation invocation;
    invocation.tool_path = aurexc_path();
    invocation.input_path = src;
    invocation.emit_kind = emit_kind;
    return invocation;
}

void add_sample_import_path(driver::CompilerInvocation& invocation) {
    invocation.import_paths.push_back(imports_root());
}

void compile_sample_native(
    const fs::path& src,
    const fs::path& output,
    const driver::EmitKind emit_kind,
    const bool use_sample_imports
) {
    driver::CompilerInvocation invocation = sample_invocation(src, emit_kind);
    invocation.output_path = output;
    if (use_sample_imports) {
        add_sample_import_path(invocation);
    }
    require_compiler_success(invocation);
}

void verify_sample_llvm_ir(const fs::path& src, const bool use_sample_imports) {
    driver::CompilerInvocation invocation = sample_invocation(src, driver::EmitKind::llvm_ir);
    if (use_sample_imports) {
        add_sample_import_path(invocation);
    }
    require_compiler_success(invocation);
}

void verify_positive_samples_llvm_ir() {
    for (const fs::path& src : sorted_files(positive_samples_root(), ".ax")) {
        const std::string name = stem(src);
        if (name.rfind("std_", 0) == 0) {
            continue;
        }
        if (skip_regular_samples().contains(name)) {
            continue;
        }
        verify_sample_llvm_ir(src, true);
    }
}

void run_positive_runtime_smoke_sample(const std::string_view area, const std::string_view filename) {
    const fs::path src = positive_sample(area, filename);
    const fs::path bin = test_bin_root() / stem(src);
    compile_sample_native(src, bin, driver::EmitKind::executable, true);
    require_success(q(bin));
}

void compile_and_run_std_positive_sample(const std::string_view filename) {
    const fs::path src = positive_sample("std", filename);
    const std::string name = src.stem().string();
    const fs::path bin = test_bin_root() / name;
    compile_sample_native(src, bin, driver::EmitKind::executable, false);
    require_success(q(bin));
}

void verify_const_enum_lowering() {
    const std::string const_enum = require_compiler_success(
        sample_invocation(positive_sample("types", "const_enum.ax"), driver::EmitKind::llvm_ir)
    ).output;
    expect_contains(const_enum, "@m0_const_enum_answer = internal unnamed_addr constant i32 42");
    expect_contains(const_enum, "load i32, ptr @m0_const_enum_answer");
}

void verify_negative_sample_diagnostics() {
    for (const fs::path& src : sorted_files(negative_samples_root(), ".ax")) {
        driver::CompilerInvocation invocation = sample_invocation(src, driver::EmitKind::check);
        if (stem(src) == "module_name_mismatch" ||
            stem(src) == "cyclic_import" ||
            stem(src) == "ambiguous_import_name" ||
            stem(src) == "module_mangle_abi_collision" ||
            stem(src) == "const_mangle_abi_collision" ||
            stem(src) == "method_abi_collision") {
            add_sample_import_path(invocation);
        }
        const CommandResult result = require_compiler_failure(invocation);
        if (stem(src) == "ambiguous_import_name") {
            expect_contains(result.output, "ambiguous function name");
        }
    }
}

} // namespace

TEST_F(AurexIntegrationTest, SampleSuite_PositiveSamples) {
    verify_positive_samples_llvm_ir();
    verify_const_enum_lowering();
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_pointer_field_write) {
    run_positive_runtime_smoke_sample("pointers", "pointer_field_write.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_eval_order_assign) {
    run_positive_runtime_smoke_sample("evaluation", "eval_order_assign.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_NegativeSamples) {
    verify_negative_sample_diagnostics();
}

TEST_F(AurexIntegrationTest, SampleSuite_Std_std_bootstrap) {
    compile_and_run_std_positive_sample("std_bootstrap.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_Std_std_ffi) {
    compile_and_run_std_positive_sample("std_ffi.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_Std_std_file) {
    const fs::path source = positive_sample("std", "std_file.ax");
    const std::string checked = require_compiler_success(
        sample_invocation(source, driver::EmitKind::checked)
    ).output;
    expect_contains_all(checked, {
        "struct FileBytes noncopy fields=2",
        "fn check_path_apis -> std.core.result.Result<bool, i32>",
        "fn read_bytes_path_null_is_err -> bool",
        "fn metadata_path -> std.core.result.Result<std.fs.file.FileMetadata, i32>",
        "fn read_bytes_path -> std.core.result.Result<std.fs.file.FileBytes, i32>",
        "fn read_text_path -> std.core.result.Result<std.core.string.String, i32>",
        "fn write_bytes_path -> std.core.result.Result<usize, i32>",
        "fn write_text_path -> std.core.result.Result<usize, i32>",
        "fn write_str -> std.core.result.Result<usize, i32>",
        "fn write_str_path -> std.core.result.Result<usize, i32>",
        "fn file_exists_path -> bool",
        "fn remove_file_path -> bool",
        "fn rename_file_path -> bool",
        "fn method std.fs.path.Path.from_str -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn method std.core.string.String.equals -> bool",
    });

    const std::string ir = require_compiler_success(
        sample_invocation(source, driver::EmitKind::ir)
    ).output;
    expect_contains_all(ir, {
        "call m0_std_fs_path_Path_from_str",
        "call m0_std_fs_file_write_bytes_path",
        "call m0_std_fs_file_write_text_path",
        "call m0_std_fs_file_write_str_path",
        "call m0_std_fs_file_metadata_path",
        "call m0_std_fs_file_read_bytes_path",
        "call m0_std_fs_file_read_text_path",
        "call m0_std_fs_file_file_exists_path",
        "call m0_std_fs_file_remove_file_path",
        "call m0_std_fs_file_rename_file_path",
    });

    compile_and_run_std_positive_sample("std_file.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_Std_std_dir) {
    const fs::path source = positive_sample("std", "std_dir.ax");
    const std::string checked = require_compiler_success(
        sample_invocation(source, driver::EmitKind::checked)
    ).output;
    expect_contains_all(checked, {
        "fn check_directory_path_apis -> std.core.result.Result<bool, i32>",
        "fn create_directory_path -> bool",
        "fn read_entries_path -> std.core.result.Result<std.core.vec.Vec<std.fs.dir.DirectoryEntry>, i32>",
        "fn read_entries_recursive_path -> std.core.result.Result<std.core.vec.Vec<std.fs.dir.DirectoryEntry>, i32>",
        "fn count_files_with_suffix_path -> std.core.result.Result<i32, i32>",
        "fn count_files_with_suffix_path_str -> std.core.result.Result<i32, i32>",
        "fn count_files_with_suffix_recursive_path -> std.core.result.Result<i32, i32>",
        "fn count_files_with_suffix_recursive_path_str -> std.core.result.Result<i32, i32>",
        "fn has_file_with_suffix_path -> std.core.result.Result<bool, i32>",
        "fn has_file_with_suffix_path_str -> std.core.result.Result<bool, i32>",
        "fn has_file_with_suffix_recursive_path -> std.core.result.Result<bool, i32>",
        "fn has_file_with_suffix_recursive_path_str -> std.core.result.Result<bool, i32>",
        "fn directory_entry_name_bytes -> std.core.text.Span<u8>",
        "fn directory_entry_name_utf8 -> std.core.result.Result<str, i32>",
        "fn directory_entry_path_bytes -> std.core.text.Span<u8>",
        "fn directory_entry_path_utf8 -> std.core.result.Result<str, i32>",
        "fn method std.fs.dir.DirectoryEntry.name_bytes -> std.core.text.Span<u8>",
        "fn method std.fs.dir.DirectoryEntry.name_utf8 -> std.core.result.Result<str, i32>",
        "fn method std.fs.dir.DirectoryEntry.path_bytes -> std.core.text.Span<u8>",
        "fn method std.fs.dir.DirectoryEntry.path_utf8 -> std.core.result.Result<str, i32>",
        "fn method std.fs.path.Path.from_str -> std.core.result.Result<std.fs.path.Path, i32>",
    });

    const std::string ir = require_compiler_success(
        sample_invocation(source, driver::EmitKind::ir)
    ).output;
    expect_contains_all(ir, {
        "call m0_std_fs_path_Path_from_str",
        "call m0_std_fs_file_write_str_path",
        "call m0_std_fs_dir_create_directory_path",
        "call m0_std_fs_dir_read_entries_path",
        "call m0_std_fs_dir_read_entries_recursive_path",
        "call m0_std_fs_dir_count_files_with_suffix_path_str",
        "call m0_std_fs_dir_count_files_with_suffix_recursive_path_str",
        "call m0_std_fs_dir_has_file_with_suffix_path_str",
        "call m0_std_fs_dir_has_file_with_suffix_recursive_path_str",
        "call m0_std_fs_dir_DirectoryEntry_name_utf8",
        "call m0_std_fs_dir_DirectoryEntry_name_bytes",
        "call m0_std_fs_dir_DirectoryEntry_path_utf8",
        "call m0_std_fs_dir_DirectoryEntry_path_bytes",
        "call m0_std_fs_dir_directory_entry_name_utf8",
    });

    compile_and_run_std_positive_sample("std_dir.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_Std_std_mem) {
    compile_and_run_std_positive_sample("std_mem.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_Std_std_text) {
    compile_and_run_std_positive_sample("std_text.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_Std_std_bytes) {
    const fs::path source = positive_sample("std", "std_bytes.ax");
    const std::string checked = require_compiler_success(
        sample_invocation(source, driver::EmitKind::checked)
    ).output;
    expect_contains_all(checked, {
        "struct Bytes fields=1",
        "struct Path fields=1",
        "fn check_bytes_raw_mutation -> std.core.result.Result<bool, i32>",
        "fn check_bytes_self_append_alias -> std.core.result.Result<bool, i32>",
        "fn check_path_accepts_raw_bytes_and_rejects_nul -> std.core.result.Result<bool, i32>",
        "fn append -> bool @c_name=m0_std_core_bytes_append",
        "fn as_mut_span -> std.core.text.MutSpan<u8> @c_name=m0_std_core_bytes_as_mut_span",
        "fn from_span -> std.core.result.Result<std.core.bytes.Bytes, i32> @c_name=m0_std_core_bytes_from_span",
        "fn method std.core.bytes.Bytes.append -> bool",
        "fn method std.core.bytes.Bytes.as_mut_span -> std.core.text.MutSpan<u8>",
        "fn method std.core.bytes.Bytes.from_span -> std.core.result.Result<std.core.bytes.Bytes, i32>",
        "fn method std.fs.path.Path.from_span -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn method std.fs.path.Path.from_str -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn method std.fs.path.Path.join_span -> std.core.result.Result<std.fs.path.Path, i32>",
    });

    const std::string ir = require_compiler_success(
        sample_invocation(source, driver::EmitKind::ir)
    ).output;
    expect_contains_all(ir, {
        "record Bytes",
        "record Path",
        "call m0_std_core_bytes_Bytes_from_span",
        "call m0_std_core_bytes_Bytes_as_mut_span",
        "call m0_std_core_bytes_Bytes_append",
        "call m0_std_fs_path_Path_from_span",
        "call m0_std_fs_path_Path_from_str",
        "call m0_std_fs_path_Path_join_span",
    });

    compile_and_run_std_positive_sample("std_bytes.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_Std_std_cstring) {
    const fs::path source = positive_sample("std", "std_cstring.ax");
    const std::string checked = require_compiler_success(
        sample_invocation(source, driver::EmitKind::checked)
    ).output;
    expect_contains_all(checked, {
        "struct CStr fields=2",
        "struct CString noncopy fields=2",
        "fn cstr_as_str_utf8 -> std.core.result.Result<str, i32>",
        "fn cstring_from_str -> std.core.result.Result<std.ffi.c.string.CString, i32>",
        "fn cstring_from_utf8 -> std.core.result.Result<std.ffi.c.string.CString, i32>",
        "fn cstring_as_cstr -> std.ffi.c.string.CStr",
        "fn method std.ffi.c.string.CStr.as_str_utf8 -> std.core.result.Result<str, i32>",
        "fn method std.ffi.c.string.CString.from_str -> std.core.result.Result<std.ffi.c.string.CString, i32>",
        "fn method std.ffi.c.string.CString.as_c -> *const u8",
    });

    compile_and_run_std_positive_sample("std_cstring.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_Std_std_str) {
    const fs::path source = positive_sample("std", "std_str.ax");
    const std::string checked = require_compiler_success(
        sample_invocation(source, driver::EmitKind::checked)
    ).output;
    expect_contains_all(checked, {
        "fn byte_len -> usize @c_name=m0_std_core_str_byte_len",
        "fn as_bytes -> std.core.text.Span<u8>",
        "fn equals -> bool @c_name=m0_std_core_str_equals",
        "fn from_utf8 -> std.core.result.Result<str, i32>",
        "fn slice_bytes_checked -> std.core.result.Result<str, i32>",
        "fn scalar_at -> std.core.result.Result<u32, i32>",
        "fn scalar_utf8_width -> std.core.result.Result<usize, i32>",
        "fn scalar_count -> usize @c_name=m0_std_core_str_scalar_count",
        "fn next_boundary -> std.core.result.Result<usize, i32>",
        "fn method std.core.result.Result<str, i32>.unwrap_or -> str",
    });

    const std::string ir = require_compiler_success(
        sample_invocation(source, driver::EmitKind::ir)
    ).output;
    expect_contains_all(ir, {
        "record std.core.result.Result<str, i32>",
        "str_data",
        "str_byte_len",
        "str_from_bytes_unchecked",
        "fn from_utf8",
    });

    compile_and_run_std_positive_sample("std_str.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_Std_std_string) {
    const fs::path source = positive_sample("std", "std_string.ax");
    const std::string checked = require_compiler_success(
        sample_invocation(source, driver::EmitKind::checked)
    ).output;
    expect_contains_all(checked, {
        "fn from_str -> std.core.result.Result<std.core.string.String, i32>",
        "fn from_utf8 -> std.core.result.Result<std.core.string.String, i32>",
        "fn append -> bool @c_name=m0_std_core_string_append",
        "fn push_scalar -> bool @c_name=m0_std_core_string_push_scalar",
        "fn insert_scalar -> bool @c_name=m0_std_core_string_insert_scalar",
        "fn pop_scalar -> std.core.result.Option<u32>",
        "fn remove_scalar_at -> std.core.result.Option<u32>",
        "fn as_str -> str @c_name=m0_std_core_string_as_str",
        "fn as_str_checked -> std.core.result.Result<str, i32>",
        "fn slice_bytes_checked -> std.core.result.Result<str, i32>",
        "fn truncate_bytes_checked -> bool",
        "fn method std.core.string.String.from_str -> std.core.result.Result<std.core.string.String, i32>",
        "fn method std.core.string.String.from_utf8 -> std.core.result.Result<std.core.string.String, i32>",
        "fn method std.core.string.String.append -> bool",
        "fn method std.core.string.String.push_scalar -> bool",
        "fn method std.core.string.String.insert_scalar -> bool",
        "fn method std.core.string.String.pop_scalar -> std.core.result.Option<u32>",
        "fn method std.core.string.String.remove_scalar_at -> std.core.result.Option<u32>",
        "fn method std.core.string.String.as_str -> str",
        "fn method std.core.string.String.as_str_checked -> std.core.result.Result<str, i32>",
        "fn method std.core.string.String.slice_bytes_checked -> std.core.result.Result<str, i32>",
        "fn method std.core.string.String.truncate_bytes_checked -> bool",
    });

    compile_and_run_std_positive_sample("std_string.ax");
}

TEST_F(AurexIntegrationTest, StdCollectionsPathSampleExposesM1ContainerBaseline) {
    const fs::path source = positive_sample("std", "std_collections_path.ax");

    const std::string checked = require_compiler_success(
        sample_invocation(source, driver::EmitKind::checked)
    ).output;
    expect_contains_all(checked, {
        "type VecU8 = std.core.vec.Vec<u8>",
        "fn std.core.vec.new<u8> -> std.core.vec.Vec<u8>",
        "fn std.core.vec.push<u8> -> bool",
        "fn std.core.vec.insert<u8> -> bool",
        "fn std.core.vec.remove<u8> -> std.core.result.Option<u8>",
        "fn std.core.vec.swap_remove<u8> -> std.core.result.Option<u8>",
        "fn std.core.vec.get<u8> -> std.core.result.Option<u8>",
        "fn std.core.vec.set<u8> -> bool",
        "fn std.core.vec.last<u8> -> std.core.result.Option<u8>",
        "fn std.core.vec.truncate<u8> -> void",
        "fn std.core.vec.as_span<u8> -> std.core.text.Span<u8>",
        "struct CStringUsizeEntry fields=2",
        "struct CStringUsizeMap fields=1",
        "struct std.core.vec.Vec<std.core.map.CStringUsizeEntry> fields=3",
        "fn check_cstring_usize_map -> bool",
        "fn check_cstring_usize_map_methods -> std.core.result.Result<bool, i32>",
        "fn cstring_usize_get -> std.core.result.Option<usize>",
        "fn cstring_usize_insert -> bool",
        "fn cstring_usize_insert_absent -> bool",
        "fn cstring_usize_remove -> std.core.result.Option<usize>",
        "fn method std.core.map.CStringUsizeMap.get -> std.core.result.Option<usize>",
        "fn method std.core.map.CStringUsizeMap.insert -> bool",
        "fn method std.core.map.CStringUsizeMap.insert_absent -> bool",
        "fn method std.core.map.CStringUsizeMap.with_capacity -> std.core.result.Result<std.core.map.CStringUsizeMap, i32>",
        "fn from_c -> std.core.result.Result<std.core.string.String, i32>",
        "fn join_c -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn check_vec_generic_methods -> bool",
        "fn check_result_option_methods -> bool",
        "fn method std.core.vec.Vec<u8>.push -> bool",
        "fn method std.core.vec.Vec<u8>.insert -> bool",
        "fn method std.core.vec.Vec<u8>.remove -> std.core.result.Option<u8>",
        "fn method std.core.vec.Vec<u8>.swap_remove -> std.core.result.Option<u8>",
        "fn method std.core.vec.Vec<u8>.get -> std.core.result.Option<u8>",
        "fn method std.core.vec.Vec<u8>.set -> bool",
        "fn method std.core.vec.Vec<u8>.as_mut_span -> std.core.text.MutSpan<u8>",
        "fn method std.core.vec.Vec<u8>.truncate -> void",
        "fn method std.core.vec.Vec<i32>.push -> bool",
        "fn method std.core.vec.Vec<i32>.insert -> bool",
        "fn method std.core.vec.Vec<i32>.remove -> std.core.result.Option<i32>",
        "fn method std.core.vec.Vec<i32>.get -> std.core.result.Option<i32>",
        "fn method std.core.vec.Vec<i32>.destroy -> void",
        "fn method std.core.result.Option<i32>.is_some -> bool",
        "fn method std.core.result.Option<i32>.is_some_ref -> bool",
        "fn method std.core.result.Option<i32>.is_none -> bool",
        "fn method std.core.result.Option<i32>.is_none_ref -> bool",
        "fn method std.core.result.Option<i32>.unwrap_or -> i32",
        "fn method std.core.result.Option<i32>.ok_or<u8> -> std.core.result.Result<i32, u8>",
        "fn method std.core.result.Result<i32, u8>.is_ok -> bool",
        "fn method std.core.result.Result<i32, u8>.is_ok_ref -> bool",
        "fn method std.core.result.Result<i32, u8>.is_err -> bool",
        "fn method std.core.result.Result<i32, u8>.is_err_ref -> bool",
        "fn method std.core.result.Result<i32, u8>.unwrap_or -> i32",
        "struct Bytes fields=1",
        "fn check_bytes -> std.core.result.Result<bool, i32>",
        "fn append -> bool @c_name=m0_std_core_bytes_append",
        "fn as_mut_span -> std.core.text.MutSpan<u8> @c_name=m0_std_core_bytes_as_mut_span",
        "fn from_span -> std.core.result.Result<std.core.bytes.Bytes, i32> @c_name=m0_std_core_bytes_from_span",
        "fn method std.core.bytes.Bytes.from_span -> std.core.result.Result<std.core.bytes.Bytes, i32>",
        "fn method std.core.bytes.Bytes.push -> bool",
        "fn method std.core.bytes.Bytes.append -> bool",
        "fn method std.core.bytes.Bytes.as_mut_span -> std.core.text.MutSpan<u8>",
        "fn method std.core.bytes.Bytes.remove -> std.core.result.Option<u8>",
        "fn method std.core.bytes.Bytes.pop -> std.core.result.Option<u8>",
        "fn method std.core.bytes.Bytes.equals_span -> bool",
        "fn method std.core.string.String.from_c -> std.core.result.Result<std.core.string.String, i32>",
        "fn insert -> bool @c_name=m0_std_core_string_insert",
        "fn pop -> std.core.result.Option<u8> @c_name=m0_std_core_string_pop",
        "fn remove -> std.core.result.Option<u8> @c_name=m0_std_core_string_remove",
        "fn truncate -> void @c_name=m0_std_core_string_truncate",
        "fn clear -> void @c_name=m0_std_core_string_clear",
        "fn method std.core.string.String.insert -> bool",
        "fn method std.core.string.String.pop -> std.core.result.Option<u8>",
        "fn method std.core.string.String.remove -> std.core.result.Option<u8>",
        "fn method std.core.string.String.truncate -> void",
        "fn method std.core.string.String.clear -> void",
        "fn from_span -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn from_str -> std.core.result.Result<std.fs.path.Path, i32> @c_name=m0_std_fs_path_from_str",
        "fn is_absolute -> bool @c_name=m0_std_fs_path_is_absolute",
        "fn parent -> std.core.result.Option<std.core.text.Span<u8>>",
        "fn extension -> std.core.result.Option<std.core.text.Span<u8>>",
        "fn file_stem -> std.core.text.Span<u8>",
        "fn join_span -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn with_extension -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn method std.fs.path.Path.from_span -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn method std.fs.path.Path.from_str -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn method std.fs.path.Path.join_c -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn method std.fs.path.Path.is_absolute -> bool",
        "fn method std.fs.path.Path.parent -> std.core.result.Option<std.core.text.Span<u8>>",
        "fn method std.fs.path.Path.extension -> std.core.result.Option<std.core.text.Span<u8>>",
        "fn method std.fs.path.Path.file_stem -> std.core.text.Span<u8>",
        "fn method std.fs.path.Path.join_span -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn method std.fs.path.Path.with_extension -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn run -> std.core.result.Result<i32, i32>",
    });

    const std::string ir = require_compiler_success(
        sample_invocation(source, driver::EmitKind::ir)
    ).output;
    expect_contains_all(ir, {
        "record std.core.vec.Vec<u8>",
        "record CStringUsizeEntry",
        "record CStringUsizeMap",
        "record std.core.vec.Vec<std.core.map.CStringUsizeEntry>",
        "record String",
        "record Path",
        "record Bytes",
        "call m0_std_core_map_cstring_usize_get",
        "call m0_std_core_map_cstring_usize_insert",
        "call m0_std_core_map_cstring_usize_insert_absent",
        "call m0_std_core_map_cstring_usize_remove",
        "call m0_std_core_map_CStringUsizeMap_get",
        "call m0_std_core_map_CStringUsizeMap_insert",
        "call m0_std_core_map_CStringUsizeMap_with_capacity",
        "call m0_std_core_vec_push__u8",
        "call m0_std_core_vec_insert__u8",
        "call m0_std_core_vec_remove__u8",
        "call m0_std_core_vec_swap_remove__u8",
        "call m0_std_core_vec_get__u8",
        "call m0_std_core_vec_set__u8",
        "call m0_std_core_vec_last__u8",
        "call m0_std_core_vec_truncate__u8",
        "call m0_std_core_vec_Vec__u8_insert",
        "call m0_std_core_vec_Vec__u8_get",
        "call m0_std_core_vec_Vec__u8_remove",
        "call m0_std_core_vec_Vec__u8_swap_remove",
        "call m0_std_core_vec_Vec__u8_as_mut_span",
        "call m0_std_core_vec_Vec__i32_push",
        "call m0_std_core_vec_Vec__i32_insert",
        "call m0_std_core_vec_Vec__i32_remove",
        "call m0_std_core_vec_Vec__i32_destroy",
        "call m0_std_core_result_Option__i32_ok_or__u8",
        "call m0_std_core_result_Option__i32_is_some",
        "call m0_std_core_result_Option__i32_is_some_ref",
        "call m0_std_core_result_Option__i32_is_none",
        "call m0_std_core_result_Option__i32_is_none_ref",
        "call m0_std_core_result_Option__i32_unwrap_or",
        "call m0_std_core_result_Result__i32__u8_is_ok",
        "call m0_std_core_result_Result__i32__u8_is_ok_ref",
        "call m0_std_core_result_Result__i32__u8_is_err",
        "call m0_std_core_result_Result__i32__u8_is_err_ref",
        "call m0_std_core_result_Result__i32__u8_unwrap_or",
        "call m0_std_core_bytes_Bytes_from_span",
        "call m0_std_core_bytes_Bytes_push",
        "call m0_std_core_bytes_Bytes_append",
        "call m0_std_core_bytes_Bytes_as_mut_span",
        "call m0_std_core_bytes_Bytes_remove",
        "call m0_std_core_bytes_Bytes_pop",
        "call m0_std_core_bytes_Bytes_equals_span",
        "call m0_std_core_string_from_c",
        "call m0_std_core_string_insert",
        "call m0_std_core_string_remove",
        "call m0_std_core_string_pop",
        "call m0_std_core_string_truncate",
        "call m0_std_core_string_clear",
        "call m0_std_core_string_String_insert",
        "call m0_std_core_string_String_remove",
        "call m0_std_core_string_String_pop",
        "call m0_std_core_string_String_truncate",
        "call m0_std_core_string_String_clear",
        "call m0_std_fs_path_join_c",
        "call m0_std_fs_path_is_absolute",
        "call m0_std_fs_path_parent",
        "call m0_std_fs_path_extension",
        "call m0_std_fs_path_file_stem",
        "call m0_std_fs_path_join_span",
        "call m0_std_fs_path_with_extension",
        "call m0_std_fs_path_Path_is_absolute",
        "call m0_std_fs_path_Path_parent",
        "call m0_std_fs_path_Path_extension",
        "call m0_std_fs_path_Path_file_stem",
        "call m0_std_fs_path_Path_join_span",
        "call m0_std_fs_path_Path_with_extension",
        "try.ok",
        "try.err",
    });

    const fs::path bin = test_bin_root() / "std_collections_path_explicit";
    compile_sample_native(source, bin, driver::EmitKind::executable, false);
    EXPECT_EQ(require_success(q(bin)).output, "");
}

TEST_F(AurexIntegrationTest, StdTextSampleExposesGenericSpanBaseline) {
    const fs::path source = positive_sample("std", "std_text.ax");

    const std::string checked = require_compiler_success(
        sample_invocation(source, driver::EmitKind::checked)
    ).output;
    expect_contains_all(checked, {
        "generic_functions 4",
        "fn std.core.text.span<u8> -> std.core.text.Span<u8>",
        "fn std.core.text.mut_span<u8> -> std.core.text.MutSpan<u8>",
        "fn std.core.text.span<i32> -> std.core.text.Span<i32>",
        "fn std.core.text.mut_span<i32> -> std.core.text.MutSpan<i32>",
        "struct std.core.text.Span<i32> fields=2",
        "struct std.core.text.MutSpan<i32> fields=2",
        "type SpanU8 = std.core.text.Span<u8>",
        "type MutSpanU8 = std.core.text.MutSpan<u8>",
    });

    const std::string ir = require_compiler_success(
        sample_invocation(source, driver::EmitKind::ir)
    ).output;
    expect_contains_all(ir, {
        "record std.core.text.Span<i32>",
        "record std.core.text.MutSpan<i32>",
        "fn std.core.text.span<i32>(data: *const i32, len: usize)",
        "fn std.core.text.mut_span<i32>(data: *mut i32, len: usize)",
        "call m0_std_core_text_span__i32",
        "call m0_std_core_text_mut_span__i32",
    });

    // Runtime execution for this sample is covered by SampleSuite_Std_std_text.
}

} // namespace aurex::test
