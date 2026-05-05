#include "support/test_support.hpp"

#include <string>
#include <string_view>

namespace aurex::test {

TEST_F(AurexIntegrationTest, InstallAndImportPaths) {
    const fs::path install_root = work_root() / "install";
    require_success(q(std::string_view(AUREX_TEST_CMAKE_COMMAND)) + " --install " + q(build_root()) + " --prefix " + q(install_root));

    const fs::path installed_bin = test_bin_root() / "std_text.installed";
    require_success(q(install_root / "bin" / "aurexc") + " " +
                    q(positive_sample("std", "std_text.ax")) + " -o " + q(installed_bin));
    require_success(q(installed_bin));

    const fs::path import_bin = test_bin_root() / "import_path";
    require_success(aurexc() + " " + tests_import_flags() + " " +
                    q(positive_sample("modules", "import_path.ax")) + " -o " + q(import_bin));
    require_success(q(import_bin));

    const std::string modules =
        require_success(aurexc() + " --dump-modules " + q(positive_sample("modules", "module_math.ax"))).output;
    expect_contains(modules, "lib.math");
    expect_contains(modules, "module_math");

    const std::string collision_ll =
        require_success(aurexc() + " " + tests_import_flags() + " --emit=llvm-ir " +
                        q(positive_sample("modules", "module_name_collision.ax"))).output;
    expect_contains(collision_ll, "@m0_module_name_collision_helper");
    expect_contains(collision_ll, "@m0_collide_a_helper");

    const fs::path collision_bin = test_bin_root() / "module_name_collision";
    require_success(aurexc() + " " + tests_import_flags() + " " +
                    q(positive_sample("modules", "module_name_collision.ax")) + " -o " + q(collision_bin));
    require_success(q(collision_bin));
}

} // namespace aurex::test
