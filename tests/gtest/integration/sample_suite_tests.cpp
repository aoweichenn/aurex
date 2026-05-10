#include <support/test_support.hpp>

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
        if (skip_regular_samples().contains(stem(src))) {
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

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_const_binary) {
    run_positive_runtime_smoke_sample("types", "const_binary.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_compound_assignment) {
    run_positive_runtime_smoke_sample("expressions", "compound_assignment.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_block_expression) {
    run_positive_runtime_smoke_sample("expressions", "block_expression.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_NegativeSamples) {
    verify_negative_sample_diagnostics();
}

} // namespace aurex::test
