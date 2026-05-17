#include <support/test_support.hpp>

#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace aurex::test {

namespace {

fs::path write_import_test_source(const fs::path& path, const std::string_view text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to open " + path.string());
    }
    out << text;
    return path;
}

} // namespace

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

TEST_F(AurexIntegrationTest, ModuleLoaderRemapsExpressionPayloadsWithoutFatNodes) {
    const fs::path import_dir = tmp_root() / "module-loader-remap";
    const fs::path library = write_import_test_source(
        import_dir / "stress" / "exprs.ax",
        "module stress.exprs;\n"
        "struct Box { value: i32; }\n"
        "struct Point { x: i32; y: i32; }\n"
        "enum Maybe { some(i32), none, }\n"
        "extern c { fn c_abs(value: i32) -> i32 @name(\"abs\"); }\n"
        "type BinaryOp = fn(i32, i32) -> i32;\n"
        "fn id(value: i32) -> i32 { return value; }\n"
        "fn add(left: i32, right: i32) -> i32 { return left + right; }\n"
        "fn apply(op: BinaryOp, left: i32, right: i32) -> i32 { return op(left, right); }\n"
        "fn choose(value: Maybe) -> i32 {\n"
        "  return match value {\n"
        "    .some(inner) => inner,\n"
        "    .none => 0,\n"
        "  };\n"
        "}\n"
        "fn point_score(point: Point) -> i32 {\n"
        "  return match point {\n"
        "    Point { x: 4, y } => y,\n"
        "    _ => 0,\n"
        "  };\n"
        "}\n"
        "pub fn compute(seed: i32) -> i32 {\n"
        "  let box: Box = Box { value: seed };\n"
        "  let values: [3]i32 = [box.value, id(seed + 1), cast[i32](2)];\n"
        "  let view: []const i32 = values[:];\n"
        "  let selected: Maybe = Maybe.some(view[0]);\n"
        "  let block_value: i32 = { let local = choose(selected); local };\n"
        "  let via_if: i32 = if seed == 41 { apply(add, 1, 2) } else { 0 };\n"
        "  let point: Point = Point { x: 4, y: via_if };\n"
        "  let pair: (i32, i32) = (view[1], values[2]);\n"
        "  let (left, right) = pair;\n"
        "  if block_value == seed {\n"
        "    return left + right + point_score(point) - 6;\n"
        "  }\n"
        "  return 0;\n"
        "}\n"
    );
    static_cast<void>(library);
    const fs::path main = write_import_test_source(
        tmp_root() / "module_loader_remap_main.ax",
        "module module_loader_remap_main;\n"
        "import stress.exprs;\n"
        "fn main() -> i32 {\n"
        "  return exprs.compute(41) - 41;\n"
        "}\n"
    );

    const std::string llvm_ir =
        require_success(aurexc() + " -I " + q(import_dir) + " --emit=llvm-ir " + q(main)).output;
    expect_contains_all(llvm_ir, {
        "@m0_module_loader_remap_main_main",
        "@m0_stress_exprs_compute",
        "@m0_stress_exprs_choose",
    });
}

} // namespace aurex::test
