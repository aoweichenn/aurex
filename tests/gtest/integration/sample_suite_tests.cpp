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
    compile_and_run_std_positive_sample("std_file.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_Std_std_mem) {
    compile_and_run_std_positive_sample("std_mem.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_Std_std_text) {
    compile_and_run_std_positive_sample("std_text.ax");
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
        "fn method std.core.result.Option<i32>.is_none -> bool",
        "fn method std.core.result.Option<i32>.unwrap_or -> i32",
        "fn method std.core.result.Option<i32>.ok_or<u8> -> std.core.result.Result<i32, u8>",
        "fn method std.core.result.Result<i32, u8>.is_ok -> bool",
        "fn method std.core.result.Result<i32, u8>.is_err -> bool",
        "fn method std.core.result.Result<i32, u8>.unwrap_or -> i32",
        "fn method std.core.string.String.from_c -> std.core.result.Result<std.core.string.String, i32>",
        "fn insert -> bool @c_name=m0_std_core_string_insert",
        "fn pop -> std.core.result.Option<u8> @c_name=m0_std_core_string_pop",
        "fn remove -> std.core.result.Option<u8> @c_name=m0_std_core_string_remove",
        "fn truncate -> void @c_name=m0_std_core_string_truncate",
        "fn clear -> void @c_name=m0_std_core_string_clear",
        "fn as_mut_span -> std.core.text.MutSpan<u8> @c_name=m0_std_core_string_as_mut_span",
        "fn method std.core.string.String.insert -> bool",
        "fn method std.core.string.String.pop -> std.core.result.Option<u8>",
        "fn method std.core.string.String.remove -> std.core.result.Option<u8>",
        "fn method std.core.string.String.truncate -> void",
        "fn method std.core.string.String.clear -> void",
        "fn method std.core.string.String.as_mut_span -> std.core.text.MutSpan<u8>",
        "fn from_span -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn is_absolute -> bool @c_name=m0_std_fs_path_is_absolute",
        "fn parent -> std.core.result.Option<std.core.text.Span<u8>>",
        "fn extension -> std.core.result.Option<std.core.text.Span<u8>>",
        "fn file_stem -> std.core.text.Span<u8>",
        "fn join_span -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn with_extension -> std.core.result.Result<std.fs.path.Path, i32>",
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
        "call m0_std_core_result_Option__i32_is_none",
        "call m0_std_core_result_Option__i32_unwrap_or",
        "call m0_std_core_result_Result__i32__u8_is_ok",
        "call m0_std_core_result_Result__i32__u8_is_err",
        "call m0_std_core_result_Result__i32__u8_unwrap_or",
        "call m0_std_core_string_from_c",
        "call m0_std_core_string_insert",
        "call m0_std_core_string_remove",
        "call m0_std_core_string_pop",
        "call m0_std_core_string_truncate",
        "call m0_std_core_string_as_mut_span",
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
