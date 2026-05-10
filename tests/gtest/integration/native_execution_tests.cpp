#include "support/test_support.hpp"

namespace aurex::test {

TEST_F(AurexIntegrationTest, NativeHelloDefaultExecutableOutputs) {
    const fs::path hello = source_root() / "examples" / "hello.ax";
    const fs::path hello_bin = test_bin_root() / "hello";
    require_success(aurexc() + " " + q(hello) + " -o " + q(hello_bin));
    EXPECT_EQ(require_success(q(hello_bin)).output, "hello from Aurex M2\n");
}

TEST_F(AurexIntegrationTest, NativeHelloAssemblyOutput) {
    const fs::path hello = source_root() / "examples" / "hello.ax";
    const fs::path asm_out = test_bin_root() / "hello.s";
    require_success(aurexc() + " --emit=asm " + q(hello) + " -o " + q(asm_out));
    EXPECT_GT(fs::file_size(asm_out), 0U);
}

TEST_F(AurexIntegrationTest, NativeHelloObjectOutput) {
    const fs::path hello = source_root() / "examples" / "hello.ax";
    const fs::path obj_out = test_bin_root() / "hello.o";
    require_success(aurexc() + " --emit=obj " + q(hello) + " -o " + q(obj_out));
    EXPECT_GT(fs::file_size(obj_out), 0U);
}

TEST_F(AurexIntegrationTest, NativeHelloExplicitExecutableOutputs) {
    const fs::path hello = source_root() / "examples" / "hello.ax";
    const fs::path direct = test_bin_root() / "hello.direct";
    require_success(aurexc() + " --emit=exe " + q(hello) + " -o " + q(direct));
    EXPECT_EQ(require_success(q(direct)).output, "hello from Aurex M2\n");
}

} // namespace aurex::test
