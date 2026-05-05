#include "support/test_support.hpp"

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::test {

TEST_F(AurexIntegrationTest, DocumentationLayoutIsStable) {
    const std::vector<fs::path> required = {
        "docs/README.md",
        "docs/zh/README.md",
        "docs/zh/architecture.md",
        "docs/zh/requirements.md",
        "docs/zh/runtime-flow.md",
        "docs/zh/api.md",
        "docs/zh/implementation.md",
        "docs/zh/usage.md",
        "docs/zh/introduction.md",
        "docs/zh/version.md",
        "docs/zh/next-steps.md",
        "docs/en/README.md",
        "docs/en/architecture.md",
        "docs/en/requirements.md",
        "docs/en/runtime-flow.md",
        "docs/en/api.md",
        "docs/en/implementation.md",
        "docs/en/usage.md",
        "docs/en/introduction.md",
        "docs/en/version.md",
        "docs/en/next-steps.md",
    };
    for (const fs::path& path : required) {
        EXPECT_TRUE(fs::exists(source_root() / path)) << path;
    }

    const std::vector<fs::path> obsolete = {
        "docs/ARCHITECTURE.zh.md",
        "docs/DESIGN.en.md",
        "docs/DESIGN.zh.md",
        "docs/SELFHOST.md",
        "docs/SEMANTICS.md",
        "docs/USAGE.en.md",
        "docs/USAGE.zh.md",
    };
    for (const fs::path& path : obsolete) {
        EXPECT_FALSE(fs::exists(source_root() / path)) << path;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(source_root() / "docs")) {
        EXPECT_FALSE(entry.path().filename().string().rfind("M0V0.1.", 0) == 0) << entry.path();
    }
}

TEST_F(AurexIntegrationTest, CliAndFrontendDumps) {
    expect_contains(require_success(aurexc() + " --version").output, "0.1.2");

    const std::string help = require_success(aurexc() + " --help").output;
    expect_contains_all(help, {
        "--check",
        "--emit=ast",
        "--emit=ir",
        "--emit=llvm-ir",
        "--emit=asm",
        "--emit=obj",
        "--emit=exe",
        "--no-stdlib",
        "--dump-modules",
        "--opt-level",
        "--stdlib",
        "--std-backend",
    });

    const fs::path hello = source_root() / "examples" / "hello.ax";
    require_success(aurexc() + " --check " + q(hello));
    require_success(aurexc() + " --emit=check " + q(hello));

    const std::string tokens = require_success(aurexc() + " --dump-tokens " + q(hello)).output;
    const std::string ast = require_success(aurexc() + " --emit=ast " + q(hello)).output;
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(hello)).output;
    const std::string ir = require_success(aurexc() + " --emit=ir " + q(hello)).output;
    const std::string llvm_ir = require_success(aurexc() + " --emit=llvm-ir " + q(hello)).output;

    EXPECT_EQ(read_text(source_root() / "tests" / "golden" / "hello.tokens"), tokens);
    expect_contains(tokens, "c_string_literal");
    expect_contains(ast, "extern_block");
    expect_contains(checked, "checked_module");
    expect_contains(ir, "aurex_ir v0");
    expect_contains(ir, "fn puts(s: *const u8) @puts linkage(extern_c) abi(c) -> i32");
    expect_contains(ir, "call puts");
    expect_contains(llvm_ir, "define i32 @main");

    const std::string eval_order =
        require_success(aurexc() + " --emit=ir --opt-level O1 " + q(source_root() / "tests" / "positive" / "eval_order_assign.ax")).output;
    expect_contains(eval_order, "call m0_eval_order_assign_next(%");

    const std::string std_text =
        require_success(aurexc() + " --emit=ir " + q(source_root() / "tests" / "positive" / "std_text.ax")).output;
    expect_contains(std_text, "phi [");
    expect_contains(std_text, "usize = cast");

    const std::string pointer_field =
        require_success(aurexc() + " --emit=ir " + q(source_root() / "tests" / "positive" / "pointer_field_write.ax")).output;
    expect_contains(pointer_field, "field_addr ");
    expect_contains(pointer_field, ".value");
}

TEST_F(AurexIntegrationTest, NativeHelloOutputs) {
    const fs::path hello = source_root() / "examples" / "hello.ax";
    const fs::path hello_bin = test_bin_root() / "hello";
    require_success(aurexc() + " " + q(hello) + " -o " + q(hello_bin));
    EXPECT_EQ(require_success(q(hello_bin)).output, "hello from Aurex M0\n");

    const fs::path asm_out = test_bin_root() / "hello.s";
    require_success(aurexc() + " --emit=asm " + q(hello) + " -o " + q(asm_out));
    EXPECT_GT(fs::file_size(asm_out), 0U);

    const fs::path obj_out = test_bin_root() / "hello.o";
    require_success(aurexc() + " --emit=obj " + q(hello) + " -o " + q(obj_out));
    EXPECT_GT(fs::file_size(obj_out), 0U);

    const fs::path direct = test_bin_root() / "hello.direct";
    require_success(aurexc() + " --emit=exe " + q(hello) + " -o " + q(direct));
    EXPECT_EQ(require_success(q(direct)).output, "hello from Aurex M0\n");

    const fs::path stdnone = test_bin_root() / "hello.stdnone";
    require_success(aurexc() + " --std-backend none " + q(hello) + " -o " + q(stdnone));
    EXPECT_EQ(require_success(q(stdnone)).output, "hello from Aurex M0\n");
}

TEST_F(AurexIntegrationTest, PositiveAndNegativeSamples) {
    const std::set<std::string> skip_regular = {
        "import_path",
        "module_name_collision",
        "std_text",
        "std_mem",
        "std_file",
    };
    const std::set<std::string> run_regular = {
        "condition_regression",
        "pointer_ops",
        "address_of_let",
        "pointer_field_write",
        "eval_order_call_stmt",
        "eval_order_return",
        "eval_order_assign",
        "eval_order_condition",
        "builtins",
    };
    for (const fs::path& src : sorted_files(source_root() / "tests" / "positive", ".ax")) {
        const std::string name = stem(src);
        if (skip_regular.contains(name)) {
            continue;
        }
        const fs::path bin = test_bin_root() / name;
        require_success(aurexc() + " " + q(src) + " -o " + q(bin));
        if (run_regular.contains(name)) {
            require_success(q(bin));
        }
    }

    for (const fs::path& src : sorted_files(source_root() / "tests" / "positive", ".ax")) {
        const std::string name = stem(src);
        if (name.rfind("std_", 0) != 0) {
            continue;
        }
        const fs::path bin = test_bin_root() / name;
        const fs::path direct = test_bin_root() / (name + ".direct");
        require_success(aurexc() + " " + q(src) + " -o " + q(bin));
        require_success(q(bin));
        require_success(aurexc() + " --emit=exe " + q(src) + " -o " + q(direct));
        require_success(q(direct));
    }

    const std::string const_enum =
        require_success(aurexc() + " --emit=llvm-ir " + q(source_root() / "tests" / "positive" / "const_enum.ax")).output;
    expect_contains(const_enum, "@m0_const_enum_answer = internal unnamed_addr constant i32 42");
    expect_contains(const_enum, "load i32, ptr @m0_const_enum_answer");

    for (const fs::path& src : sorted_files(source_root() / "tests" / "negative", ".ax")) {
        std::string command = aurexc() + " --check " + q(src);
        if (stem(src) == "module_name_mismatch" || stem(src) == "cyclic_import" || stem(src) == "ambiguous_import_name") {
            command = aurexc() + " " + tests_import_flags() + " --check " + q(src);
        }
        const CommandResult result = require_failure(command);
        if (stem(src) == "ambiguous_import_name") {
            expect_contains(result.output, "ambiguous function name");
        }
    }
}

TEST_F(AurexIntegrationTest, InstallAndImportPaths) {
    const fs::path install_root = work_root() / "install";
    require_success(q(std::string_view(AUREX_TEST_CMAKE_COMMAND)) + " --install " + q(build_root()) + " --prefix " + q(install_root));

    const fs::path installed_bin = test_bin_root() / "std_text.installed";
    require_success(q(install_root / "bin" / "aurexc") + " " +
                    q(source_root() / "tests" / "positive" / "std_text.ax") + " -o " + q(installed_bin));
    require_success(q(installed_bin));

    const fs::path import_bin = test_bin_root() / "import_path";
    require_success(aurexc() + " " + tests_import_flags() + " " +
                    q(source_root() / "tests" / "positive" / "import_path.ax") + " -o " + q(import_bin));
    require_success(q(import_bin));

    const std::string modules =
        require_success(aurexc() + " --dump-modules " + q(source_root() / "tests" / "positive" / "module_math.ax")).output;
    expect_contains(modules, "lib.math");
    expect_contains(modules, "module_math");

    const std::string collision_ll =
        require_success(aurexc() + " " + tests_import_flags() + " --emit=llvm-ir " +
                        q(source_root() / "tests" / "positive" / "module_name_collision.ax")).output;
    expect_contains(collision_ll, "@m0_module_name_collision_helper");
    expect_contains(collision_ll, "@m0_collide_a_helper");

    const fs::path collision_bin = test_bin_root() / "module_name_collision";
    require_success(aurexc() + " " + tests_import_flags() + " " +
                    q(source_root() / "tests" / "positive" / "module_name_collision.ax") + " -o " + q(collision_bin));
    require_success(q(collision_bin));
}

} // namespace aurex::test
