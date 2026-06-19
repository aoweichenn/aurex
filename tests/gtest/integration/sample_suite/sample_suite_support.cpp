#include <gtest/integration/sample_suite/sample_suite_support.hpp>

#include <array>
#include <string>
#include <string_view>

namespace aurex::test {
namespace {

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

inline constexpr auto IMPORT_RUNTIME_SAMPLES = std::to_array<ImportRuntimeSample>({
    {"modules", "full_module_type_path.ax"},
    {"modules", "import_alias_qualified_call.ax"},
    {"modules", "import_path.ax"},
    {"modules", "module_math.ax"},
    {"modules", "module_name_collision.ax"},
    {"types", "type_alias_import.ax"},
    {"visibility", "reexport_import.ax"},
    {"visibility", "visibility_import.ax"},
});

void verify_sample_llvm_ir(const fs::path& src, const bool use_sample_imports)
{
    driver::CompilerInvocation invocation = sample_invocation(src, driver::EmitKind::llvm_ir);
    if (use_sample_imports) {
        add_sample_import_path(invocation);
    }
    require_compiler_success(invocation);
}

void verify_import_runtime_sample(const ImportRuntimeSample& sample)
{
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

} // namespace

driver::CompilerInvocation sample_invocation(const fs::path& src, const driver::EmitKind emit_kind)
{
    driver::CompilerInvocation invocation;
    invocation.tool_path = aurexc_path();
    invocation.input_path = src;
    invocation.emit_kind = emit_kind;
    return invocation;
}

bool is_native_emit_kind(const driver::EmitKind emit_kind) noexcept
{
    return emit_kind == driver::EmitKind::assembly || emit_kind == driver::EmitKind::object
        || emit_kind == driver::EmitKind::executable;
}

bool sample_uses_import_path(const fs::path& src)
{
    const std::string source = read_text(src);
    for (const std::string_view marker : SAMPLE_IMPORT_MARKERS) {
        if (source.find(marker) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void add_sample_import_path(driver::CompilerInvocation& invocation)
{
    invocation.import_paths.push_back(imports_root());
}

void configure_sample_imports_if_needed(driver::CompilerInvocation& invocation)
{
    if (sample_uses_import_path(invocation.input_path)) {
        add_sample_import_path(invocation);
    }
}

void compile_sample_native(
    const fs::path& src, const fs::path& output, const driver::EmitKind emit_kind, const bool use_sample_imports)
{
    driver::CompilerInvocation invocation = sample_invocation(src, emit_kind);
    invocation.output_path = output;
    if (use_sample_imports) {
        add_sample_import_path(invocation);
    }
    require_compiler_success(invocation);
}

void verify_positive_samples_llvm_ir()
{
    for (const fs::path& src : sorted_files(positive_samples_root(), ".ax")) {
        verify_sample_llvm_ir(src, sample_uses_import_path(src));
    }
}

void verify_const_enum_lowering()
{
    const std::string const_enum = require_compiler_success(
        sample_invocation(positive_sample("types", "const_enum.ax"), driver::EmitKind::llvm_ir))
                                       .output;
    expect_contains(const_enum, "@m0_const_enum_answer = internal unnamed_addr constant i32 42");
    expect_contains(const_enum, "load i32, ptr @m0_const_enum_answer");
}

void verify_generic_builtin_ir()
{
    const std::string generic_builtins = require_compiler_success(
        sample_invocation(positive_sample("generics", "builtins_m3_1.ax"), driver::EmitKind::ir))
                                             .output;
    expect_contains_all(generic_builtins,
        {
            "sizeof i32",
            "alignof i32",
            "ptraddr",
            "ptrat",
            "ptrcast",
            "bitcast",
            "slice_data",
            "slice_len",
            "fn mut_slice_ops<i32>(values: []mut i32)",
            "str_data",
            "str_byte_len",
            "strvalid",
            "strfromutf8",
            "strraw",
        });
}

void run_positive_runtime_smoke_sample(const std::string_view area, const std::string_view filename)
{
    const fs::path src = positive_sample(area, filename);
    const fs::path bin = test_bin_root() / stem(src);
    compile_sample_native(src, bin, driver::EmitKind::executable, true);
    require_success(q(bin));
}

void verify_import_runtime_samples()
{
    for (const ImportRuntimeSample& sample : IMPORT_RUNTIME_SAMPLES) {
        verify_import_runtime_sample(sample);
    }
}

} // namespace aurex::test
