#include <aurex/base/diagnostic.hpp>
#include <aurex/driver/native_toolchain.hpp>
#include <support/test_support.hpp>

namespace aurex::test {
namespace {

using base::ErrorCode;

} // namespace

TEST(CoreUnit, NativeToolchainRejectsSupportSourcesForNonExecutableAndReportsMissingClang) {
    driver::NativeCompileRequest unsupported;
    unsupported.emit_kind = driver::EmitKind::object;
    unsupported.input_path = source_root() / "examples" / "hello.ax";
    unsupported.output_path = tmp_root() / "hello.o";
    unsupported.support_source_paths.push_back(tmp_root() / "native_support.c");
    auto support_result = driver::invoke_clang(unsupported);
    ASSERT_FALSE(support_result);
    EXPECT_EQ(support_result.error().code, ErrorCode::codegen_error);
    expect_contains(support_result.error().message, "support sources are only supported");

    driver::NativeCompileRequest missing;
    missing.clang_path = "/definitely/not/a/real/clang";
    missing.input_path = source_root() / "examples" / "hello.ax";
    missing.output_path = tmp_root() / "missing" / "hello";
    auto missing_result = driver::invoke_clang(missing);
    ASSERT_FALSE(missing_result);
    EXPECT_EQ(missing_result.error().code, ErrorCode::codegen_error);
    expect_contains(missing_result.error().message, "exit code 127");
    EXPECT_TRUE(fs::exists(tmp_root() / "missing"));
}

} // namespace aurex::test
