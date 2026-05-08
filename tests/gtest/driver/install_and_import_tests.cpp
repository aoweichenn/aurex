#include "support/test_support.hpp"

#include <string>
#include <string_view>

namespace aurex::test {

TEST_F(AurexIntegrationTest, InstallAndImportPaths) {
    const fs::path install_root = work_root() / "install";
    require_success(q(std::string_view(AUREX_TEST_CMAKE_COMMAND)) + " --install " + q(build_root()) + " --prefix " + q(install_root));

    const std::string installed_hello_llvm =
        require_success(q(install_root / "bin" / "aurexc") + " --emit=llvm-ir " +
                        q(source_root() / "examples" / "hello.ax")).output;
    expect_contains(installed_hello_llvm, "define i32 @main");

    const std::string import_ll =
        require_success(aurexc() + " " + tests_import_flags() + " --emit=llvm-ir " +
                        q(positive_sample("modules", "import_path.ax"))).output;
    expect_contains_all(import_ll, {
        "@m0_import_path_main",
        "@m0_shared_util_twice",
    });

    const std::string modules =
        require_success(aurexc() + " --dump-modules " + q(positive_sample("modules", "module_math.ax"))).output;
    expect_contains(modules, "lib.math");
    expect_contains(modules, "module_math");

    const std::string collision_ll =
        require_success(aurexc() + " " + tests_import_flags() + " --emit=llvm-ir " +
                        q(positive_sample("modules", "module_name_collision.ax"))).output;
    expect_contains(collision_ll, "@m0_module_name_collision_helper");
    expect_contains(collision_ll, "@m0_collide_a_helper");

}

} // namespace aurex::test
