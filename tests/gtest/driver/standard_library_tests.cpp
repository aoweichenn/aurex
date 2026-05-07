#include "aurex/base/diagnostic.hpp"
#include "aurex/driver/compiler.hpp"
#include "aurex/driver/standard_library.hpp"
#include "support/test_support.hpp"

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace aurex::test {
namespace {

using base::ErrorCode;

class EnvVarGuard final {
public:
    explicit EnvVarGuard(const char* name) : name_(name) {
        if (const char* value = std::getenv(name_); value != nullptr) {
            previous_ = value;
        }
    }

    ~EnvVarGuard() {
        if (previous_) {
            static_cast<void>(::setenv(name_, previous_->c_str(), 1));
        } else {
            static_cast<void>(::unsetenv(name_));
        }
    }

private:
    const char* name_;
    std::optional<std::string> previous_;
};

} // namespace

TEST(CoreUnit, StandardLibraryHelpersCoverBackendNamesAndDisabledStdlib) {
    EXPECT_EQ(driver::standard_library_backend_name(driver::StandardLibraryBackend::host_c), "host-c");
    EXPECT_EQ(driver::standard_library_backend_name(driver::StandardLibraryBackend::none), "none");

    driver::CompilerInvocation invocation;
    invocation.use_standard_library = false;
    invocation.import_paths = {imports_root()};
    const std::vector<fs::path> import_paths = driver::standard_library_import_paths(invocation);
    ASSERT_EQ(import_paths.size(), 1U);
    EXPECT_EQ(import_paths[0], imports_root());

    const driver::StandardLibraryLayout layout {
        source_root() / "std",
        source_root() / "std" / "ffi" / "c" / "support" / "host_c.c",
        {},
    };
    const std::vector<fs::path> host_sources =
        driver::standard_library_support_sources(layout, driver::StandardLibraryBackend::host_c);
    ASSERT_EQ(host_sources.size(), 1U);
    EXPECT_EQ(host_sources[0], layout.host_c_support_source);
    EXPECT_TRUE(driver::standard_library_support_sources(layout, driver::StandardLibraryBackend::none).empty());
}

TEST(CoreUnit, StandardLibraryFindsEnvironmentRootAndAddsImportPath) {
    EnvVarGuard guard {"AUREX_STDLIB"};
    const fs::path std_root = source_root() / "std";
    ASSERT_EQ(::setenv("AUREX_STDLIB", std_root.string().c_str(), 1), 0);

    driver::CompilerInvocation invocation;
    invocation.standard_library_path = source_root() / "missing-stdlib-root";
    invocation.import_paths = {imports_root()};

    const fs::path expected_root = fs::weakly_canonical(std_root);
    const std::optional<driver::StandardLibraryLayout> layout = driver::find_standard_library(invocation);
    ASSERT_TRUE(layout);
    EXPECT_EQ(layout->root, expected_root);
    EXPECT_EQ(layout->host_c_support_source, expected_root / "ffi" / "c" / "support" / "host_c.c");

    const std::vector<fs::path> import_paths = driver::standard_library_import_paths(invocation);
    ASSERT_EQ(import_paths.size(), 2U);
    EXPECT_EQ(import_paths[0], imports_root());
    EXPECT_EQ(import_paths[1], expected_root.parent_path());
}

TEST_F(AurexIntegrationTest, CompilerRunCoversMissingInputAndNativeOutputValidation) {
    driver::Compiler compiler;

    driver::CompilerInvocation missing_input;
    missing_input.emit_kind = driver::EmitKind::tokens;
    missing_input.input_path = tmp_root() / "missing.ax";
    auto missing_result = compiler.run(missing_input);
    ASSERT_FALSE(missing_result);
    EXPECT_EQ(missing_result.error().code, ErrorCode::io_error);
    expect_contains(missing_result.error().message, "failed to open input file");

    driver::CompilerInvocation native_without_output;
    native_without_output.emit_kind = driver::EmitKind::assembly;
    native_without_output.input_path = source_root() / "examples" / "hello.ax";
    native_without_output.tool_path = aurexc_path();
    native_without_output.use_standard_library = false;
    auto output_result = compiler.run(native_without_output);
    ASSERT_FALSE(output_result);
    EXPECT_EQ(output_result.error().code, ErrorCode::io_error);
    expect_contains(output_result.error().message, "native output requires -o");
}

} // namespace aurex::test
