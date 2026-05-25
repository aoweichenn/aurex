#include <aurex/base/config.hpp>
#include <aurex/base/integer.hpp>

#include <support/test_support.hpp>

#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <driver/module_loader_support.hpp>

namespace aurex::test {

namespace {

fs::path write_import_test_source(const fs::path& path, const std::string_view text)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to open " + path.string());
    }
    out << text;
    return path;
}

} // namespace

TEST_F(AurexIntegrationTest, InstallAndImportPaths)
{
    const fs::path install_root = work_root() / "install";
    require_success(q(std::string_view(AUREX_TEST_CMAKE_COMMAND)) + " --install " + q(build_root()) + " --prefix "
        + q(install_root));

    const std::string installed_hello_llvm = require_success(
        q(install_root / "bin" / "aurexc") + " --emit=llvm-ir " + q(source_root() / "examples" / "hello.ax"))
                                                 .output;
    expect_contains(installed_hello_llvm, "define i32 @main");

    const std::string import_ll = require_success(
        aurexc() + " " + tests_import_flags() + " --emit=llvm-ir " + q(positive_sample("modules", "import_path.ax")))
                                      .output;
    expect_contains_all(import_ll,
        {
            "@m0_import_path_main",
            "@m0_shared_util_twice",
        });

    const std::string modules =
        require_success(aurexc() + " --dump-modules " + q(positive_sample("modules", "module_math.ax"))).output;
    expect_contains(modules, "lib.math");
    expect_contains(modules, "module_math");

    const std::string collision_ll = require_success(aurexc() + " " + tests_import_flags() + " --emit=llvm-ir "
        + q(positive_sample("modules", "module_name_collision.ax")))
                                         .output;
    expect_contains(collision_ll, "@m0_module_name_collision_helper");
    expect_contains(collision_ll, "@m0_collide_a_helper");
}

TEST_F(AurexIntegrationTest, ModuleLoaderRemapsExpressionPayloadsWithoutFatNodes)
{
    const fs::path import_dir = tmp_root() / "module-loader-remap";
    const fs::path library = write_import_test_source(import_dir / "stress" / "exprs.ax",
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
        "}\n");
    static_cast<void>(library);
    const fs::path main = write_import_test_source(tmp_root() / "module_loader_remap_main.ax",
        "module module_loader_remap_main;\n"
        "import stress.exprs;\n"
        "fn main() -> i32 {\n"
        "  return exprs.compute(41) - 41;\n"
        "}\n");

    const std::string llvm_ir =
        require_success(aurexc() + " -I " + q(import_dir) + " --emit=llvm-ir " + q(main)).output;
    expect_contains_all(llvm_ir,
        {
            "@m0_module_loader_remap_main_main",
            "@m0_stress_exprs_compute",
            "@m0_stress_exprs_choose",
        });
}

TEST_F(AurexIntegrationTest, ModuleLoaderLoadsPrimarySidecarParts)
{
    const fs::path work = tmp_root() / "m3-module-parts";
    const fs::path import_dir = work / "imports";
    static_cast<void>(write_import_test_source(import_dir / "lib" / "util.ax",
        "module lib.util;\n"
        "pub fn value() -> i32 {\n"
        "  return 5;\n"
        "}\n"));
    const fs::path primary = write_import_test_source(work / "vm.ax",
        "module m3.vm;\n"
        "part parser;\n"
        "part emitter;\n"
        "fn main() -> i32 {\n"
        "  return parser_value() + emitter_value() - 47;\n"
        "}\n");
    static_cast<void>(write_import_test_source(work / "vm.parts" / "parser.ax",
        "module m3.vm part parser;\n"
        "import lib.util;\n"
        "fn parser_value() -> i32 {\n"
        "  return util.value() + 40;\n"
        "}\n"));
    static_cast<void>(write_import_test_source(work / "vm.parts" / "emitter.ax",
        "module m3.vm part emitter;\n"
        "fn emitter_value() -> i32 {\n"
        "  return 2;\n"
        "}\n"));

    const std::string llvm_ir =
        require_success(aurexc() + " -I " + q(import_dir) + " --emit=llvm-ir " + q(primary)).output;
    expect_contains_all(llvm_ir,
        {
            "@m0_m3_vm_main",
            "@m0_m3_vm_parser_value",
            "@m0_m3_vm_emitter_value",
            "@m0_lib_util_value",
        });

    const std::string modules =
        require_success(aurexc() + " -I " + q(import_dir) + " --dump-modules " + q(primary)).output;
    expect_contains_all(modules,
        {
            "m3.vm",
            "lib.util",
        });
}

TEST_F(AurexIntegrationTest, ModuleLoaderDiscoversOwningPrimaryForPartCheck)
{
    const fs::path work = tmp_root() / "m3-root-part-check";
    static_cast<void>(write_import_test_source(work / "owner.ax",
        "module m3.owner;\n"
        "part parser;\n"
        "fn main() -> i32 {\n"
        "  return parser_value() - 7;\n"
        "}\n"));
    const fs::path part = write_import_test_source(work / "owner.parts" / "parser.ax",
        "module m3.owner part parser;\n"
        "fn parser_value() -> i32 {\n"
        "  return 7;\n"
        "}\n");

    static_cast<void>(require_success(aurexc() + " --check " + q(part)));
    expect_contains(require_failure(aurexc() + " --emit=llvm-ir " + q(part)).output,
        "cannot emit artifact from module part 'parser'");
}

TEST_F(AurexIntegrationTest, ModuleLoaderDiagnosticsCoverModulePartGraph)
{
    const fs::path work = tmp_root() / "m3-module-part-diagnostics";

    const fs::path missing = write_import_test_source(work / "missing.ax",
        "module m3.missing;\n"
        "part parser;\n"
        "fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n");
    const std::string missing_output = require_failure(aurexc() + " --check " + q(missing)).output;
    expect_contains(missing_output, "module 'm3.missing' declares missing part 'parser'");
    expect_contains(missing_output, (work / "missing.parts" / "parser.ax").string());

    const fs::path duplicate = write_import_test_source(work / "duplicate.ax",
        "module m3.duplicate;\n"
        "part parser;\n"
        "part parser;\n"
        "fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(duplicate)).output,
        "duplicate module part 'parser' in module 'm3.duplicate'");

    const fs::path case_collision = write_import_test_source(work / "case_collision.ax",
        "module m3.case_collision;\n"
        "part parser;\n"
        "part Parser;\n"
        "fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(case_collision)).output, "differ only by case");

    const fs::path mismatch = write_import_test_source(work / "mismatch.ax",
        "module m3.mismatch;\n"
        "part parser;\n"
        "fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n");
    static_cast<void>(write_import_test_source(work / "mismatch.parts" / "parser.ax",
        "module m3.mismatch part other;\n"
        "fn parser_value() -> i32 {\n"
        "  return 0;\n"
        "}\n"));
    expect_contains(require_failure(aurexc() + " --check " + q(mismatch)).output,
        "does not match expected 'm3.mismatch part parser'");

    const fs::path import_dir = work / "imports";
    static_cast<void>(write_import_test_source(import_dir / "m3" / "partlike.ax",
        "module m3.partlike part parser;\n"
        "fn hidden() -> i32 {\n"
        "  return 0;\n"
        "}\n"));
    const fs::path importer = write_import_test_source(work / "import_part.ax",
        "module m3.import_part;\n"
        "import m3.partlike;\n"
        "fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(importer)).output,
        "is not an importable module");
}

TEST_F(AurexIntegrationTest, ModuleLoaderDiagnosticsCoverModulePartRootEdges)
{
    const fs::path work = tmp_root() / "m3-module-part-root-edges";
    const fs::path orphan = write_import_test_source(work / "orphan.ax",
        "module m3.orphan part parser;\n"
        "fn parser_value() -> i32 {\n"
        "  return 1;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(orphan)).output,
        "module part root 'parser' has no owning primary module");

    static_cast<void>(write_import_test_source(work / "owner.ax",
        "module m3.owner;\n"
        "fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n"));
    const fs::path unlisted = write_import_test_source(work / "owner.parts" / "ghost.ax",
        "module m3.owner part ghost;\n"
        "fn ghost_value() -> i32 {\n"
        "  return 1;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(unlisted)).output,
        "module part file 'ghost' is not listed by primary module");
}

TEST_F(AurexIntegrationTest, ModuleLoaderDiagnosticsCoverPartImportFailures)
{
    const fs::path work = tmp_root() / "m3-module-part-import-failures";
    const fs::path missing_import = write_import_test_source(work / "missing_import.ax",
        "module m3.missing_import;\n"
        "part parser;\n"
        "fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n");
    static_cast<void>(write_import_test_source(work / "missing_import.parts" / "parser.ax",
        "module m3.missing_import part parser;\n"
        "import absent.target;\n"
        "fn parser_value() -> i32 {\n"
        "  return 0;\n"
        "}\n"));
    expect_contains(
        require_failure(aurexc() + " --check " + q(missing_import)).output, "failed to resolve import: absent.target");

    const fs::path reimport = write_import_test_source(work / "reimport.ax",
        "module m3.reimport;\n"
        "part first;\n"
        "part second;\n"
        "fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n");
    static_cast<void>(write_import_test_source(work / "reimport.parts" / "first.ax",
        "module m3.reimport part first;\n"
        "fn first_value() -> i32 {\n"
        "  return 1;\n"
        "}\n"));
    static_cast<void>(write_import_test_source(work / "reimport.parts" / "second.ax",
        "module m3.reimport part second;\n"
        "import first;\n"
        "fn second_value() -> i32 {\n"
        "  return 2;\n"
        "}\n"));
    expect_contains(require_failure(aurexc() + " --check " + q(reimport)).output,
        "module part 'first' of module 'm3.reimport' is not an importable module");
}

TEST_F(AurexIntegrationTest, ModuleLoaderRejectsAmbiguousImportCandidates)
{
    const fs::path work = tmp_root() / "m3-ambiguous-import";
    const fs::path first_import_dir = work / "first";
    const fs::path second_import_dir = work / "second";
    static_cast<void>(write_import_test_source(first_import_dir / "lib" / "amb.ax",
        "module lib.amb;\n"
        "pub fn value() -> i32 {\n"
        "  return 1;\n"
        "}\n"));
    static_cast<void>(write_import_test_source(second_import_dir / "lib" / "amb.ax",
        "module lib.amb;\n"
        "pub fn value() -> i32 {\n"
        "  return 2;\n"
        "}\n"));
    const fs::path importer = write_import_test_source(work / "importer.ax",
        "module m3.importer;\n"
        "import lib.amb;\n"
        "fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n");

    const std::string output = require_failure(
        aurexc() + " -I " + q(first_import_dir) + " -I " + q(second_import_dir) + " --check " + q(importer))
                                   .output;
    expect_contains(output, "ambiguous import: lib.amb");
    expect_contains(output, (first_import_dir / "lib" / "amb.ax").string());
    expect_contains(output, (second_import_dir / "lib" / "amb.ax").string());
}

TEST_F(AurexIntegrationTest, ModuleLoaderDiagnosticsCoverSupportBranches)
{
    const fs::path missing_module_decl = write_import_test_source(tmp_root() / "missing_module_decl.ax",
        "fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(missing_module_decl)).output,
        "module declaration is required for importable files");

    const fs::path missing_import_dir = tmp_root() / "missing-import-dir";
    const fs::path alternate_import_dir = tmp_root() / "missing-import-alternate";
    fs::create_directories(missing_import_dir);
    fs::create_directories(alternate_import_dir);
    const fs::path missing_import = write_import_test_source(tmp_root() / "missing_import_with_candidates.ax",
        "module missing_import_with_candidates;\n"
        "import absent.target;\n"
        "fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n");
    const std::string missing_output = require_failure(
        aurexc() + " -I " + q(missing_import_dir) + " -I " + q(alternate_import_dir) + " --check " + q(missing_import))
                                           .output;
    expect_contains(missing_output, "failed to resolve import: absent.target");
    expect_contains(missing_output,
        (missing_import.parent_path() / "absent" / "target.ax").string() + ", "
            + (missing_import_dir / "absent" / "target.ax").string() + ", "
            + (alternate_import_dir / "absent" / "target.ax").string());

    const fs::path duplicate_root = write_import_test_source(tmp_root() / "duplicate_root.ax",
        "module duplicate_identity;\n"
        "import duplicate_identity as dup;\n"
        "fn main() -> i32 {\n"
        "  return dup.value();\n"
        "}\n");
    static_cast<void>(write_import_test_source(tmp_root() / "duplicate_identity.ax",
        "module duplicate_identity;\n"
        "pub fn value() -> i32 {\n"
        "  return 1;\n"
        "}\n"));
    expect_contains(require_failure(aurexc() + " --check " + q(duplicate_root)).output,
        "duplicate module name 'duplicate_identity'");

    const fs::path depth_dir = tmp_root() / "depth-imports";
    fs::create_directories(depth_dir);
    const fs::path depth_root = write_import_test_source(depth_dir / "depth_root.ax",
        "module depth_root;\n"
        "import depth_0;\n"
        "fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n");
    for (base::usize i = 0; i <= base::config::AUREX_MAX_INCLUDE_DEPTH; ++i) {
        const std::string current = "depth_" + std::to_string(i);
        const std::string next = "depth_" + std::to_string(i + 1);
        std::ostringstream source;
        source << "module " << current << ";\n"
               << "import " << next << ";\n"
               << "pub fn value() -> i32 {\n"
               << "  return 0;\n"
               << "}\n";
        static_cast<void>(write_import_test_source(depth_dir / (current + ".ax"), source.str()));
    }
    expect_contains(require_failure(aurexc() + " --check " + q(depth_root)).output, "maximum import depth exceeded");
}

TEST(CoreUnit, ModuleLoaderSupportValidationDefensiveBranches)
{
    base::DiagnosticSink diagnostics;
    syntax::AstModule combined;
    const auto cached_validation =
        driver::validate_cached_file_module_path(combined, syntax::INVALID_MODULE_ID, nullptr, diagnostics);
    EXPECT_TRUE(cached_validation);

    const fs::path canonical = tmp_root() / "same_module.ax";
    const auto identity_validation =
        driver::validate_unique_module_identity("same.module", canonical, canonical, base::SourceRange{}, diagnostics);
    EXPECT_TRUE(identity_validation);

    const auto cyclic_result = driver::report_cyclic_import(diagnostics, nullptr, canonical.string());
    EXPECT_FALSE(cyclic_result);
    ASSERT_FALSE(diagnostics.diagnostics().empty());
    EXPECT_TRUE(diagnostics.diagnostics().back().range.empty());

    const fs::path support_dir = tmp_root() / "module-loader-support";
    const fs::path support_module = write_import_test_source(support_dir / "lib" / "one.ax", "");
    syntax::ModulePath import_path;
    import_path.parts = {"lib", "one"};

    const auto found = driver::find_import_file(import_path, support_dir, {});
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(driver::module_loader_canonical_or_absolute(*found),
        driver::module_loader_canonical_or_absolute(support_module));

    const driver::ImportFileResolution deduplicated =
        driver::resolve_import_file(import_path, support_dir, {support_dir});
    ASSERT_EQ(deduplicated.matching_candidates.size(), 1U);
    ASSERT_TRUE(deduplicated.selected.has_value());
    EXPECT_EQ(*deduplicated.selected, deduplicated.matching_candidates.front());

    EXPECT_EQ(driver::owning_primary_for_part_file(support_dir / "plain.ax"), std::nullopt);
}

} // namespace aurex::test
