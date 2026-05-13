#include <support/test_support.hpp>

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace aurex::test {
namespace {

struct ExpectedDiagnostic {
    std::string_view sample_stem;
    std::string_view message;
};

struct CrossStageNegativeSample {
    std::string_view area;
    std::string_view filename;
    std::string_view diagnostic;
};

struct ImportRuntimeSample {
    std::string_view area;
    std::string_view filename;
};

inline constexpr auto SAMPLE_IMPORT_MARKERS = std::to_array<std::string_view>({
    "import ambiguous.",
    "import bad.",
    "import collide.",
    "import const_mangle_collision.",
    "import cycle.",
    "import lib.",
    "import mangle_collision.",
    "import method_collision.",
    "import qualified_literal.",
    "import samplelib.",
    "import shared.",
});

inline constexpr auto EXPECTED_NEGATIVE_DIAGNOSTICS = std::to_array<ExpectedDiagnostic>({
    {"ambiguous_import_alias", "ambiguous import alias: lib"},
    {"ambiguous_import_name", "ambiguous function name"},
    {"array_literal_empty_infer", "empty array literal requires an array type context"},
    {"increment_syntax", "increment operator is not supported"},
    {"match_expression_missing_case", "match expression is not exhaustive for enum case"},
    {"method_abi_collision", "duplicate ABI symbol"},
    {"module_name_mismatch", "does not match import 'bad.name'"},
    {"private_field_access", "field is private: secret"},
    {"private_function_import", "unknown function: add_secret"},
    {"private_qualified_function", "function is private: samplelib.visibility.add_secret"},
    {"reference_assign_through_shared", "left side of assignment must be writable"},
    {"reference_invalid_pointee", "reference requires a valid storage type"},
    {"reference_mut_from_immutable", "mutable reference requires a writable place expression"},
    {"reference_mut_not_raw_pointer", "initializer type does not match declared type"},
    {"str_slice_bound_non_integer", "slice bound must be an integer"},
    {"strfromutf8_non_slice", "str UTF-8 builtin requires a []const u8 or []mut u8 byte slice"},
    {"try_result_return_mismatch", "try expression on result-like enum requires enclosing function"},
    {"unsafe_fn_call_required", "call to unsafe function read_raw requires unsafe context"},
});

inline constexpr auto CROSS_STAGE_NEGATIVE_SAMPLES = std::to_array<CrossStageNegativeSample>({
    {"expressions", "increment_syntax.ax", "increment operator is not supported"},
    {"modules", "module_name_mismatch.ax", "does not match import 'bad.name'"},
    {"visibility", "private_qualified_function.ax", "function is private: samplelib.visibility.add_secret"},
    {"types", "array_literal_empty_infer.ax", "empty array literal requires an array type context"},
    {"pattern_matching", "match_expression_missing_case.ax", "match expression is not exhaustive for enum case"},
    {"functions", "unsafe_fn_call_required.ax", "call to unsafe function read_raw requires unsafe context"},
    {"error_handling", "try_result_return_mismatch.ax", "try expression on result-like enum requires enclosing function"},
});

inline constexpr auto CROSS_STAGE_EMIT_KINDS = std::to_array<driver::EmitKind>({
    driver::EmitKind::check,
    driver::EmitKind::ir,
    driver::EmitKind::llvm_ir,
    driver::EmitKind::executable,
});

inline constexpr auto IMPORT_RUNTIME_SAMPLES = std::to_array<ImportRuntimeSample>({
    {"modules", "import_alias_qualified_call.ax"},
    {"modules", "import_path.ax"},
    {"modules", "module_math.ax"},
    {"modules", "module_name_collision.ax"},
    {"types", "type_alias_import.ax"},
    {"visibility", "reexport_import.ax"},
    {"visibility", "visibility_import.ax"},
});

driver::CompilerInvocation sample_invocation(const fs::path& src, const driver::EmitKind emit_kind) {
    driver::CompilerInvocation invocation;
    invocation.tool_path = aurexc_path();
    invocation.input_path = src;
    invocation.emit_kind = emit_kind;
    return invocation;
}

[[nodiscard]] bool is_native_emit_kind(const driver::EmitKind emit_kind) noexcept {
    return emit_kind == driver::EmitKind::assembly ||
           emit_kind == driver::EmitKind::object ||
           emit_kind == driver::EmitKind::executable;
}

[[nodiscard]] bool sample_uses_import_path(const fs::path& src) {
    const std::string source = read_text(src);
    for (const std::string_view marker : SAMPLE_IMPORT_MARKERS) {
        if (source.find(marker) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void add_sample_import_path(driver::CompilerInvocation& invocation) {
    invocation.import_paths.push_back(imports_root());
}

void configure_sample_imports_if_needed(driver::CompilerInvocation& invocation) {
    if (sample_uses_import_path(invocation.input_path)) {
        add_sample_import_path(invocation);
    }
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
        verify_sample_llvm_ir(src, sample_uses_import_path(src));
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

[[nodiscard]] std::optional<std::string_view> expected_negative_diagnostic(const fs::path& src) {
    const std::string name = stem(src);
    for (const ExpectedDiagnostic& expected : EXPECTED_NEGATIVE_DIAGNOSTICS) {
        if (expected.sample_stem == name) {
            return expected.message;
        }
    }
    return std::nullopt;
}

void verify_negative_sample_diagnostics() {
    for (const fs::path& src : sorted_files(negative_samples_root(), ".ax")) {
        driver::CompilerInvocation invocation = sample_invocation(src, driver::EmitKind::check);
        configure_sample_imports_if_needed(invocation);
        const CommandResult result = require_compiler_failure(invocation);
        expect_contains(result.output, "error:");
        if (const std::optional<std::string_view> expected = expected_negative_diagnostic(src)) {
            expect_contains(result.output, *expected);
        }
    }
}

void verify_negative_sample_fails_in_emit_kind(
    const fs::path& src,
    const driver::EmitKind emit_kind,
    const std::string_view diagnostic
) {
    driver::CompilerInvocation invocation = sample_invocation(src, emit_kind);
    configure_sample_imports_if_needed(invocation);
    if (is_native_emit_kind(emit_kind)) {
        invocation.output_path = test_bin_root() / (stem(src) + ".negative");
    }
    const CommandResult result = require_compiler_failure(invocation);
    expect_contains(result.output, diagnostic);
}

void verify_cross_stage_negative_samples() {
    for (const CrossStageNegativeSample& sample : CROSS_STAGE_NEGATIVE_SAMPLES) {
        const fs::path src = negative_sample(sample.area, sample.filename);
        for (const driver::EmitKind emit_kind : CROSS_STAGE_EMIT_KINDS) {
            verify_negative_sample_fails_in_emit_kind(src, emit_kind, sample.diagnostic);
        }
    }
}

void verify_import_runtime_sample(const ImportRuntimeSample& sample) {
    const fs::path src = positive_sample(sample.area, sample.filename);

    driver::CompilerInvocation modules = sample_invocation(src, driver::EmitKind::modules);
    add_sample_import_path(modules);
    const std::string module_dump = require_compiler_success(modules).output;
    expect_contains(module_dump, stem(src));

    driver::CompilerInvocation checked = sample_invocation(src, driver::EmitKind::check);
    add_sample_import_path(checked);
    require_compiler_success(checked);

    driver::CompilerInvocation ir = sample_invocation(src, driver::EmitKind::ir);
    add_sample_import_path(ir);
    expect_contains(require_compiler_success(ir).output, "aurex_ir v0");

    driver::CompilerInvocation llvm_ir = sample_invocation(src, driver::EmitKind::llvm_ir);
    add_sample_import_path(llvm_ir);
    expect_contains(require_compiler_success(llvm_ir).output, "define i32 @main");

    const fs::path bin = test_bin_root() / stem(src);
    compile_sample_native(src, bin, driver::EmitKind::executable, true);
    EXPECT_EQ(require_success(q(bin)).output, "");
}

void verify_import_runtime_samples() {
    for (const ImportRuntimeSample& sample : IMPORT_RUNTIME_SAMPLES) {
        verify_import_runtime_sample(sample);
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

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_tuple_basic) {
    run_positive_runtime_smoke_sample("types", "tuple_basic.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_str_checked) {
    run_positive_runtime_smoke_sample("types", "str_checked.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_str_slice) {
    run_positive_runtime_smoke_sample("types", "str_slice.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_pattern_ergonomics) {
    run_positive_runtime_smoke_sample("pattern_matching", "pattern_ergonomics.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_pattern_remaining) {
    run_positive_runtime_smoke_sample("pattern_matching", "pattern_remaining.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_imported_samples) {
    verify_import_runtime_samples();
}

TEST_F(AurexIntegrationTest, SampleSuite_NegativeSamples) {
    verify_negative_sample_diagnostics();
}

TEST_F(AurexIntegrationTest, SampleSuite_NegativeCrossStageSamples) {
    verify_cross_stage_negative_samples();
}

} // namespace aurex::test
