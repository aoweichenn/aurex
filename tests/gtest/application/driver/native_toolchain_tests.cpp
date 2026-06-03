#include <aurex/application/driver/native_toolchain.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>

#include <support/test_support.hpp>

#include <cstdlib>
#include <fstream>
#include <optional>

namespace aurex::test {
namespace {

using base::ErrorCode;

constexpr std::string_view NATIVE_TOOLCHAIN_SUCCESS_SCRIPT = "#!/bin/sh\nexit 0\n";
constexpr std::string_view NATIVE_TOOLCHAIN_SIGNAL_SCRIPT = "#!/bin/sh\nkill -TERM \"$$\"\n";

class ScopedPathEnvironment final {
public:
    explicit ScopedPathEnvironment(const std::filesystem::path& prefix)
    {
        const char* existing = std::getenv("PATH");
        if (existing != nullptr) {
            this->old_value_ = existing;
            this->had_old_value_ = true;
        }

        std::string value = prefix.string();
        if (existing != nullptr && *existing != '\0') {
            value += ":";
            value += existing;
        }
        ::setenv("PATH", value.c_str(), 1);
    }

    ScopedPathEnvironment(const ScopedPathEnvironment&) = delete;
    ScopedPathEnvironment& operator=(const ScopedPathEnvironment&) = delete;

    ~ScopedPathEnvironment()
    {
        if (this->had_old_value_) {
            ::setenv("PATH", this->old_value_.c_str(), 1);
        } else {
            ::unsetenv("PATH");
        }
    }

private:
    std::string old_value_;
    bool had_old_value_ = false;
};

void write_executable_script(const fs::path& path, const std::string_view contents)
{
    fs::create_directories(path.parent_path());
    {
        std::ofstream out(path, std::ios::binary);
        out << contents;
    }
    std::error_code error;
    fs::permissions(path, fs::perms::owner_all, fs::perm_options::replace, error);
    ASSERT_FALSE(error) << error.message();
}

} // namespace

TEST(CoreUnit, NativeToolchainRejectsSupportSourcesForNonExecutableAndReportsMissingClang)
{
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

TEST(CoreUnit, NativeToolchainCoversDefaultClangPathSupportSourcesAndEmptyOutputParent)
{
    const fs::path clang_dir = tmp_root() / "native_toolchain_clang";
    const fs::path clang_script = clang_dir / "clang";
    write_executable_script(clang_script, NATIVE_TOOLCHAIN_SUCCESS_SCRIPT);
    const ScopedPathEnvironment path_guard(clang_dir);

    const fs::path input = tmp_root() / "native_toolchain_input.ll";
    {
        std::ofstream out(input, std::ios::binary);
        out << "; dummy llvm ir\n";
    }
    const fs::path support = tmp_root() / "native_toolchain_support.c";
    {
        std::ofstream out(support, std::ios::binary);
        out << "int native_toolchain_support(void) { return 0; }\n";
    }

    driver::NativeCompileRequest request;
    request.clang_path.clear();
    request.input_is_llvm_ir = true;
    request.support_source_paths.push_back(support);
    request.input_path = input;
    request.output_path = "native_toolchain_output.o";
    request.emit_kind = driver::EmitKind::executable;

    const auto result = driver::invoke_clang(request);
    ASSERT_TRUE(result) << result.error().message;
}

TEST(CoreUnit, NativeToolchainReportsDirectoryCreationAndSignalFailures)
{
    const fs::path input = tmp_root() / "native_toolchain_signal_input.ll";
    {
        std::ofstream out(input, std::ios::binary);
        out << "; dummy llvm ir\n";
    }

    const fs::path blocked_parent = tmp_root() / "native_toolchain_blocked_parent";
    {
        std::ofstream out(blocked_parent, std::ios::binary);
        out << "blocked";
    }
    const fs::path blocked_output = blocked_parent / "out.o";

    const fs::path success_script = tmp_root() / "native_toolchain_success.sh";
    write_executable_script(success_script, NATIVE_TOOLCHAIN_SUCCESS_SCRIPT);

    driver::NativeCompileRequest blocked_request;
    blocked_request.clang_path = success_script.string();
    blocked_request.input_path = input;
    blocked_request.output_path = blocked_output;
    blocked_request.emit_kind = driver::EmitKind::object;
    const auto blocked_result = driver::invoke_clang(blocked_request);
    ASSERT_FALSE(blocked_result);
    EXPECT_EQ(blocked_result.error().code, ErrorCode::io_error);
    expect_contains(blocked_result.error().message, "failed to create native output directory");

    const fs::path signal_script = tmp_root() / "native_toolchain_signal.sh";
    write_executable_script(signal_script, NATIVE_TOOLCHAIN_SIGNAL_SCRIPT);

    driver::NativeCompileRequest signal_request;
    signal_request.clang_path = signal_script.string();
    signal_request.input_path = input;
    signal_request.output_path = tmp_root() / "native_toolchain_signal.o";
    signal_request.emit_kind = driver::EmitKind::assembly;
    const auto signal_result = driver::invoke_clang(signal_request);
    ASSERT_FALSE(signal_result);
    EXPECT_EQ(signal_result.error().code, ErrorCode::codegen_error);
    expect_contains(signal_result.error().message, "signal");
}

} // namespace aurex::test
