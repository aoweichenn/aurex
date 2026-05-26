#include <aurex/base/config.hpp>
#include <aurex/base/diagnostic.hpp>
#include <aurex/base/integer.hpp>
#include <aurex/base/source.hpp>
#include <aurex/driver/compiler.hpp>
#include <aurex/driver/file_cache.hpp>
#include <aurex/driver/invocation.hpp>
#include <aurex/driver/module_loader.hpp>
#include <aurex/driver/package_identity.hpp>
#include <aurex/query/query_key.hpp>
#include <aurex/query/stable_identity.hpp>
#include <aurex/sema/identifier.hpp>

#include <support/test_support.hpp>

#include <array>
#include <fstream>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <driver/module_loader_support.hpp>

namespace aurex::test {

namespace {

struct CacheFingerprint {
    std::string global;
    std::string primary;
    std::string secondary;
    std::string bytes;

    [[nodiscard]] friend bool operator==(const CacheFingerprint& lhs, const CacheFingerprint& rhs) = default;
};

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

[[nodiscard]] std::string read_import_test_text(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to read " + path.string());
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

[[nodiscard]] std::string import_test_hex_encode(const std::string_view bytes)
{
    constexpr char HEX_DIGITS[] = "0123456789abcdef";
    constexpr unsigned int HIGH_NIBBLE_SHIFT = 4;
    constexpr unsigned int LOW_NIBBLE_MASK = 0x0FU;
    constexpr base::usize HEX_CHARS_PER_BYTE = 2U;

    std::string encoded;
    encoded.reserve(bytes.size() * HEX_CHARS_PER_BYTE);
    for (const unsigned char byte : bytes) {
        encoded.push_back(HEX_DIGITS[byte >> HIGH_NIBBLE_SHIFT]);
        encoded.push_back(HEX_DIGITS[byte & LOW_NIBBLE_MASK]);
    }
    return encoded;
}

[[nodiscard]] std::vector<std::string_view> split_import_test_fields(const std::string_view line)
{
    std::vector<std::string_view> fields;
    base::usize begin = 0;
    while (begin <= line.size()) {
        const base::usize end = line.find('\t', begin);
        if (end == std::string_view::npos) {
            fields.push_back(line.substr(begin));
            break;
        }
        fields.push_back(line.substr(begin, end - begin));
        begin = end + 1;
    }
    return fields;
}

[[nodiscard]] std::optional<CacheFingerprint> module_query_fingerprint(
    const std::string& cache_text, const std::string_view query_kind, const std::string_view module_name)
{
    const std::array<std::string_view, 1> module_parts{module_name};
    const query::ModuleKey module_key = query::module_key_from_stable_id(
        driver::package_key_from_identity({}), sema::stable_module_id(std::span<const std::string_view>{module_parts}));
    const std::string stable_key = import_test_hex_encode(query::stable_serialize(module_key));

    base::usize begin = 0;
    while (begin < cache_text.size()) {
        const base::usize end = cache_text.find('\n', begin);
        const std::string_view line = end == std::string::npos
            ? std::string_view(cache_text).substr(begin)
            : std::string_view(cache_text).substr(begin, end - begin);
        const std::vector<std::string_view> fields = split_import_test_fields(line);
        if (fields.size() == 12U && fields[0] == "query" && fields[1] == query_kind && fields[11] == stable_key) {
            return CacheFingerprint{
                std::string(fields[7]),
                std::string(fields[8]),
                std::string(fields[9]),
                std::string(fields[10]),
            };
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    return std::nullopt;
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

TEST_F(AurexIntegrationTest, ModulePartsUsePartLocalImportsAndSharedItems)
{
    const fs::path work = tmp_root() / "m3-module-part-local-imports";
    const fs::path import_dir = work / "imports";
    static_cast<void>(write_import_test_source(import_dir / "lib" / "tools.ax",
        "module lib.tools;\n"
        "pub type Num = i32;\n"
        "pub fn value() -> i32 {\n"
        "  return 3;\n"
        "}\n"));
    const fs::path primary = write_import_test_source(work / "phase3.ax",
        "module m3.phase3;\n"
        "part parser;\n"
        "part emitter;\n"
        "fn main() -> i32 {\n"
        "  return cross_part_value() - 4;\n"
        "}\n");
    static_cast<void>(write_import_test_source(work / "phase3.parts" / "parser.ax",
        "module m3.phase3 part parser;\n"
        "import lib.tools;\n"
        "fn imported_value(input: tools.Num) -> i32 {\n"
        "  return input + tools.value();\n"
        "}\n"));
    static_cast<void>(write_import_test_source(work / "phase3.parts" / "emitter.ax",
        "module m3.phase3 part emitter;\n"
        "fn cross_part_value() -> i32 {\n"
        "  return imported_value(1);\n"
        "}\n"));

    const std::string llvm_ir =
        require_success(aurexc() + " -I " + q(import_dir) + " --emit=llvm-ir " + q(primary)).output;
    expect_contains_all(llvm_ir,
        {
            "@m0_m3_phase3_main",
            "@m0_m3_phase3_imported_value",
            "@m0_m3_phase3_cross_part_value",
            "@m0_lib_tools_value",
        });
}

TEST_F(AurexIntegrationTest, ModulePartsSharePrivateModuleSurface)
{
    const fs::path work = tmp_root() / "m3-module-part-private-surface";
    const fs::path primary = write_import_test_source(work / "phase4_private.ax",
        "module m3.phase4_private;\n"
        "part model;\n"
        "part use;\n"
        "fn main() -> i32 {\n"
        "  return use_private() - 44;\n"
        "}\n");
    static_cast<void>(write_import_test_source(work / "phase4_private.parts" / "model.ax",
        "module m3.phase4_private part model;\n"
        "priv struct Box {\n"
        "  pub value: i32;\n"
        "  priv secret: i32;\n"
        "}\n"
        "impl Box {\n"
        "  priv fn secret_value(self: &Box) -> i32 {\n"
        "    return self.secret;\n"
        "  }\n"
        "}\n"
        "priv fn make_box(value: i32) -> Box {\n"
        "  return Box { value: value, secret: 2 };\n"
        "}\n"));
    static_cast<void>(write_import_test_source(work / "phase4_private.parts" / "use.ax",
        "module m3.phase4_private part use;\n"
        "fn use_private() -> i32 {\n"
        "  let box: Box = make_box(40);\n"
        "  return box.value + box.secret + box.secret_value();\n"
        "}\n"));

    const std::string llvm_ir = require_success(aurexc() + " --emit=llvm-ir " + q(primary)).output;
    expect_contains_all(llvm_ir,
        {
            "@m0_m3_phase4_private_main",
            "@m0_m3_phase4_private_make_box",
            "@m0_m3_phase4_private_Box_secret_value",
        });
}

TEST_F(AurexIntegrationTest, ModulePartPrivateItemsStayHiddenFromExternalModules)
{
    const fs::path work = tmp_root() / "m3-module-part-private-external";
    const fs::path import_dir = work / "imports";
    static_cast<void>(write_import_test_source(import_dir / "lib" / "owner.ax",
        "module lib.owner;\n"
        "part model;\n"
        "pub fn value() -> i32 {\n"
        "  return 1;\n"
        "}\n"));
    static_cast<void>(write_import_test_source(import_dir / "lib" / "owner.parts" / "model.ax",
        "module lib.owner part model;\n"
        "priv struct Secret {\n"
        "  pub value: i32;\n"
        "}\n"
        "priv fn hidden() -> i32 {\n"
        "  return 7;\n"
        "}\n"));
    const fs::path type_use = write_import_test_source(work / "private_type_user.ax",
        "module m3.private_type_user;\n"
        "import lib.owner;\n"
        "fn main() -> i32 {\n"
        "  let value: owner.Secret = owner.Secret { value: 1 };\n"
        "  return value.value;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(type_use)).output,
        "type is private: lib.owner.Secret");

    const fs::path function_use = write_import_test_source(work / "private_function_user.ax",
        "module m3.private_function_user;\n"
        "import lib.owner;\n"
        "fn main() -> i32 {\n"
        "  return owner.hidden();\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(function_use)).output,
        "function is private: lib.owner.hidden");
}

TEST_F(AurexIntegrationTest, ModulePartPublicImportsDoNotBecomeModuleReexports)
{
    const fs::path work = tmp_root() / "m3-module-part-pub-import-local";
    const fs::path import_dir = work / "imports";
    static_cast<void>(write_import_test_source(import_dir / "lib" / "inner.ax",
        "module lib.inner;\n"
        "pub type Count = i32;\n"
        "pub fn value() -> Count {\n"
        "  return 3;\n"
        "}\n"));
    static_cast<void>(write_import_test_source(import_dir / "lib" / "facade.ax",
        "module lib.facade;\n"
        "part exports;\n"
        "pub fn local_value() -> i32 {\n"
        "  return part_value();\n"
        "}\n"));
    static_cast<void>(write_import_test_source(import_dir / "lib" / "facade.parts" / "exports.ax",
        "module lib.facade part exports;\n"
        "pub import lib.inner as inner;\n"
        "fn part_value() -> inner.Count {\n"
        "  return inner.value();\n"
        "}\n"));

    const fs::path importer = write_import_test_source(work / "facade_user.ax",
        "module m3.facade_user;\n"
        "import lib.facade;\n"
        "fn main() -> i32 {\n"
        "  let value: facade.Count = 1;\n"
        "  return value;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(importer)).output,
        "unknown type in module lib.facade: Count");
}

TEST_F(AurexIntegrationTest, ModulePartImportsDoNotLeakAcrossPrimaryOrParts)
{
    const fs::path work = tmp_root() / "m3-module-part-import-leakage";
    const fs::path import_dir = work / "imports";
    static_cast<void>(write_import_test_source(import_dir / "lib" / "tools.ax",
        "module lib.tools;\n"
        "pub fn value() -> i32 {\n"
        "  return 3;\n"
        "}\n"));

    const fs::path part_to_primary = write_import_test_source(work / "part_to_primary.ax",
        "module m3.part_to_primary;\n"
        "part parser;\n"
        "fn main() -> i32 {\n"
        "  return tools.value();\n"
        "}\n");
    static_cast<void>(write_import_test_source(work / "part_to_primary.parts" / "parser.ax",
        "module m3.part_to_primary part parser;\n"
        "import lib.tools;\n"
        "fn parser_value() -> i32 {\n"
        "  return tools.value();\n"
        "}\n"));
    const std::string part_to_primary_output =
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(part_to_primary)).output;
    expect_contains(part_to_primary_output, "unknown import alias: tools");
    expect_contains(part_to_primary_output, "imports are part-local");

    const fs::path primary_to_part = write_import_test_source(work / "primary_to_part.ax",
        "module m3.primary_to_part;\n"
        "import lib.tools;\n"
        "part parser;\n"
        "fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n");
    static_cast<void>(write_import_test_source(work / "primary_to_part.parts" / "parser.ax",
        "module m3.primary_to_part part parser;\n"
        "fn parser_value() -> i32 {\n"
        "  return tools.value();\n"
        "}\n"));
    const std::string primary_to_part_output =
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(primary_to_part)).output;
    expect_contains(primary_to_part_output, "unknown import alias: tools");
    expect_contains(primary_to_part_output, "imports are part-local");

    static_cast<void>(write_import_test_source(import_dir / "lib" / "holder.ax",
        "module lib.holder;\n"
        "part worker;\n"
        "pub fn holder_value() -> i32 {\n"
        "  return worker_value();\n"
        "}\n"));
    static_cast<void>(write_import_test_source(import_dir / "lib" / "holder.parts" / "worker.ax",
        "module lib.holder part worker;\n"
        "import lib.tools;\n"
        "fn worker_value() -> i32 {\n"
        "  return tools.value();\n"
        "}\n"));
    const fs::path unrelated_loaded_module = write_import_test_source(work / "unrelated_loaded_module.ax",
        "module m3.unrelated_loaded_module;\n"
        "import lib.holder;\n"
        "fn main() -> i32 {\n"
        "  return tools.value();\n"
        "}\n");
    const std::string unrelated_loaded_module_output =
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(unrelated_loaded_module)).output;
    expect_contains(unrelated_loaded_module_output, "unknown import alias: tools");
    expect_not_contains(unrelated_loaded_module_output, "imports are part-local");
}

TEST_F(AurexIntegrationTest, ModuleLoaderRejectsDuplicateItemsAcrossParts)
{
    const fs::path work = tmp_root() / "m3-module-part-duplicate-items";
    const fs::path primary = write_import_test_source(work / "duplicate.ax",
        "module m3.duplicate_items;\n"
        "part parser;\n"
        "part emitter;\n"
        "fn main() -> i32 {\n"
        "  return duplicate_value();\n"
        "}\n");
    static_cast<void>(write_import_test_source(work / "duplicate.parts" / "parser.ax",
        "module m3.duplicate_items part parser;\n"
        "fn duplicate_value() -> i32 {\n"
        "  return 1;\n"
        "}\n"));
    static_cast<void>(write_import_test_source(work / "duplicate.parts" / "emitter.ax",
        "module m3.duplicate_items part emitter;\n"
        "fn duplicate_value() -> i32 {\n"
        "  return 2;\n"
        "}\n"));

    expect_contains(
        require_failure(aurexc() + " --check " + q(primary)).output, "duplicate function definition: duplicate_value");
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

TEST_F(AurexIntegrationTest, ModuleLoaderImportVisibilityChangesGraphAndExportsOnly)
{
    driver::clear_file_cache();

    constexpr std::string_view MODULE_NAME = "incremental_cache_visibility_rank";
    constexpr std::string_view PUBLIC_REEXPORT_SOURCE = "module incremental_cache_visibility_rank;\n"
                                                        "pub import lib.visible as visible;\n"
                                                        "pub fn exported(value: i32) -> i32 {\n"
                                                        "  return visible.value() + value;\n"
                                                        "}\n";
    constexpr std::string_view PRIVATE_IMPORT_SOURCE = "module incremental_cache_visibility_rank;\n"
                                                       "import lib.visible as visible;\n"
                                                       "pub fn exported(value: i32) -> i32 {\n"
                                                       "  return visible.value() + value;\n"
                                                       "}\n";
    constexpr std::string_view IMPORT_SOURCE = "module lib.visible;\n"
                                               "pub fn value() -> i32 {\n"
                                               "  return 1;\n"
                                               "}\n";

    const fs::path cache_dir = tmp_root() / "incremental-cache-visibility-rank";
    const fs::path import_dir = cache_dir / "imports";
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    const fs::path import_source = import_dir / "lib" / "visible.ax";

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;
    invocation.import_paths.push_back(import_dir);

    static_cast<void>(write_import_test_source(import_source, IMPORT_SOURCE));
    static_cast<void>(write_import_test_source(source, PUBLIC_REEXPORT_SOURCE));
    driver::Compiler compiler;
    auto first = compiler.run(invocation);
    ASSERT_TRUE(first) << first.error().message;
    const std::string first_cache = read_import_test_text(cache);
    const std::optional<CacheFingerprint> first_graph =
        module_query_fingerprint(first_cache, "module_graph", MODULE_NAME);
    const std::optional<CacheFingerprint> first_exports =
        module_query_fingerprint(first_cache, "module_exports", MODULE_NAME);
    const std::optional<CacheFingerprint> first_items = module_query_fingerprint(first_cache, "item_list", MODULE_NAME);
    ASSERT_TRUE(first_graph.has_value());
    ASSERT_TRUE(first_exports.has_value());
    ASSERT_TRUE(first_items.has_value());

    static_cast<void>(write_import_test_source(source, PRIVATE_IMPORT_SOURCE));
    driver::clear_file_cache();
    auto second = compiler.run(invocation);
    ASSERT_TRUE(second) << second.error().message;
    const std::string second_cache = read_import_test_text(cache);
    const std::optional<CacheFingerprint> second_graph =
        module_query_fingerprint(second_cache, "module_graph", MODULE_NAME);
    const std::optional<CacheFingerprint> second_exports =
        module_query_fingerprint(second_cache, "module_exports", MODULE_NAME);
    const std::optional<CacheFingerprint> second_items =
        module_query_fingerprint(second_cache, "item_list", MODULE_NAME);
    ASSERT_TRUE(second_graph.has_value());
    ASSERT_TRUE(second_exports.has_value());
    ASSERT_TRUE(second_items.has_value());

    EXPECT_FALSE(*first_graph == *second_graph);
    EXPECT_FALSE(*first_exports == *second_exports);
    EXPECT_EQ(*first_items, *second_items);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, ModuleLoaderPackageImportVisibilityChangesPackageExportsOnly)
{
    driver::clear_file_cache();

    constexpr std::string_view MODULE_NAME = "incremental_cache_package_visibility_rank";
    constexpr std::string_view PACKAGE_REEXPORT_SOURCE = "module incremental_cache_package_visibility_rank;\n"
                                                         "pub(package) import lib.visible as visible;\n"
                                                         "pub fn exported(value: i32) -> i32 {\n"
                                                         "  return visible.value() + value;\n"
                                                         "}\n";
    constexpr std::string_view PRIVATE_IMPORT_SOURCE = "module incremental_cache_package_visibility_rank;\n"
                                                       "import lib.visible as visible;\n"
                                                       "pub fn exported(value: i32) -> i32 {\n"
                                                       "  return visible.value() + value;\n"
                                                       "}\n";
    constexpr std::string_view IMPORT_SOURCE = "module lib.visible;\n"
                                               "pub fn value() -> i32 {\n"
                                               "  return 1;\n"
                                               "}\n";

    const fs::path cache_dir = tmp_root() / "incremental-cache-package-visibility-rank";
    const fs::path import_dir = cache_dir / "imports";
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    const fs::path import_source = import_dir / "lib" / "visible.ax";

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;
    invocation.import_paths.push_back(import_dir);

    static_cast<void>(write_import_test_source(import_source, IMPORT_SOURCE));
    static_cast<void>(write_import_test_source(source, PACKAGE_REEXPORT_SOURCE));
    driver::Compiler compiler;
    auto first = compiler.run(invocation);
    ASSERT_TRUE(first) << first.error().message;
    const std::string first_cache = read_import_test_text(cache);
    const std::optional<CacheFingerprint> first_graph =
        module_query_fingerprint(first_cache, "module_graph", MODULE_NAME);
    const std::optional<CacheFingerprint> first_exports =
        module_query_fingerprint(first_cache, "module_exports", MODULE_NAME);
    const std::optional<CacheFingerprint> first_package_exports =
        module_query_fingerprint(first_cache, "module_package_exports", MODULE_NAME);
    const std::optional<CacheFingerprint> first_items = module_query_fingerprint(first_cache, "item_list", MODULE_NAME);
    ASSERT_TRUE(first_graph.has_value());
    ASSERT_TRUE(first_exports.has_value());
    ASSERT_TRUE(first_package_exports.has_value());
    ASSERT_TRUE(first_items.has_value());

    static_cast<void>(write_import_test_source(source, PRIVATE_IMPORT_SOURCE));
    driver::clear_file_cache();
    auto second = compiler.run(invocation);
    ASSERT_TRUE(second) << second.error().message;
    const std::string second_cache = read_import_test_text(cache);
    const std::optional<CacheFingerprint> second_graph =
        module_query_fingerprint(second_cache, "module_graph", MODULE_NAME);
    const std::optional<CacheFingerprint> second_exports =
        module_query_fingerprint(second_cache, "module_exports", MODULE_NAME);
    const std::optional<CacheFingerprint> second_package_exports =
        module_query_fingerprint(second_cache, "module_package_exports", MODULE_NAME);
    const std::optional<CacheFingerprint> second_items =
        module_query_fingerprint(second_cache, "item_list", MODULE_NAME);
    ASSERT_TRUE(second_graph.has_value());
    ASSERT_TRUE(second_exports.has_value());
    ASSERT_TRUE(second_items.has_value());

    EXPECT_FALSE(*first_graph == *second_graph);
    EXPECT_EQ(*first_exports, *second_exports);
    EXPECT_FALSE(second_package_exports.has_value());
    EXPECT_EQ(*first_items, *second_items);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, ModuleLoaderAssignsImportPathModulesToDistinctPackages)
{
    driver::clear_file_cache();

    constexpr std::string_view ROOT_SOURCE = "module package_identity_root;\n"
                                             "import lib.visible as visible;\n"
                                             "fn main() -> i32 {\n"
                                             "  return visible.value();\n"
                                             "}\n";
    constexpr std::string_view IMPORT_SOURCE = "module lib.visible;\n"
                                               "pub fn value() -> i32 {\n"
                                               "  return 1;\n"
                                               "}\n";

    const fs::path work = tmp_root() / "module-loader-package-identity";
    const fs::path import_dir = work / "imports";
    const fs::path source = work / "main.ax";
    const fs::path import_source = import_dir / "lib" / "visible.ax";

    static_cast<void>(write_import_test_source(source, ROOT_SOURCE));
    static_cast<void>(write_import_test_source(import_source, IMPORT_SOURCE));

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.import_paths.push_back(import_dir);

    base::SourceManager sources;
    base::DiagnosticSink diagnostics;
    driver::ModuleLoader loader(invocation, sources, diagnostics);
    auto ast = loader.load_root();
    ASSERT_TRUE(ast) << ast.error().message;
    const std::span<const driver::ModuleRecord> modules = loader.modules();
    ASSERT_EQ(modules.size(), 2U);
    ASSERT_EQ(modules.front().imports.size(), 1U);

    const query::PackageKey root_package = driver::package_key_for_invocation(invocation);
    const std::string canonical_import_root = driver::module_loader_canonical_or_absolute(import_dir).string();
    const query::PackageKey import_package = driver::package_key_for_import_root(canonical_import_root);
    const std::array<std::string_view, 2> legacy_import_root_identity{
        driver::DRIVER_IMPORT_ROOT_PACKAGE_IDENTITY_KIND,
        std::string_view(canonical_import_root),
    };
    EXPECT_EQ(modules[0].package, root_package);
    EXPECT_EQ(modules[1].package, import_package);
    EXPECT_EQ(import_package, query::package_key(legacy_import_root_identity));
    EXPECT_NE(modules[0].package, modules[1].package);
    EXPECT_EQ(modules.front().imports.front().module_package, modules[1].package);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, ModuleLoaderUsesPackageManifestIdentities)
{
    driver::clear_file_cache();

    constexpr std::string_view ROOT_MANIFEST = "[package]\n"
                                               "name = \"app.pkg\"\n"
                                               "version = \"1.2.3\"\n";
    constexpr std::string_view IMPORT_MANIFEST = "[package]\n"
                                                 "name = \"lib.pkg\"\n"
                                                 "version = \"0.4.0\"\n";
    constexpr std::string_view ROOT_SOURCE = "module manifest_identity_root;\n"
                                             "import lib.visible as visible;\n"
                                             "fn main() -> i32 {\n"
                                             "  return visible.value();\n"
                                             "}\n";
    constexpr std::string_view IMPORT_SOURCE = "module lib.visible;\n"
                                               "pub fn value() -> i32 {\n"
                                               "  return 1;\n"
                                               "}\n";

    const fs::path work = tmp_root() / "module-loader-manifest-package-identity";
    const fs::path import_dir = work / "imports";
    const fs::path source = work / "src" / "main.ax";
    const fs::path import_source = import_dir / "lib" / "visible.ax";

    static_cast<void>(
        write_import_test_source(work / std::string(driver::DRIVER_PACKAGE_MANIFEST_FILE_NAME), ROOT_MANIFEST));
    static_cast<void>(
        write_import_test_source(import_dir / std::string(driver::DRIVER_PACKAGE_MANIFEST_FILE_NAME), IMPORT_MANIFEST));
    static_cast<void>(write_import_test_source(source, ROOT_SOURCE));
    static_cast<void>(write_import_test_source(import_source, IMPORT_SOURCE));

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.import_paths.push_back(import_dir);

    const std::optional<std::string> root_identity = driver::package_manifest_identity_for_path(source);
    const std::optional<std::string> import_identity = driver::package_manifest_identity_for_path(import_dir);
    ASSERT_TRUE(root_identity.has_value());
    ASSERT_TRUE(import_identity.has_value());
    ASSERT_NE(*root_identity, *import_identity);

    base::SourceManager sources;
    base::DiagnosticSink diagnostics;
    driver::ModuleLoader loader(invocation, sources, diagnostics);
    auto ast = loader.load_root();
    ASSERT_TRUE(ast) << ast.error().message;
    const std::span<const driver::ModuleRecord> modules = loader.modules();
    ASSERT_EQ(modules.size(), 2U);
    ASSERT_EQ(modules.front().imports.size(), 1U);

    const query::PackageKey root_package = driver::package_key_for_invocation(invocation);
    const query::PackageKey import_package =
        driver::package_key_for_import_root(driver::module_loader_canonical_or_absolute(import_dir).string());
    EXPECT_EQ(driver::package_identity_for_invocation(invocation), *root_identity);
    EXPECT_EQ(modules[0].package, root_package);
    EXPECT_EQ(modules[1].package, import_package);
    EXPECT_NE(modules[0].package, modules[1].package);
    EXPECT_EQ(modules.front().imports.front().module_package, modules[1].package);

    invocation.package_identity = "cli.override.pkg";
    EXPECT_EQ(driver::package_identity_for_invocation(invocation), "cli.override.pkg");
    EXPECT_EQ(driver::package_key_for_invocation(invocation), driver::package_key_from_identity("cli.override.pkg"));

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, ModuleLoaderPackageManifestIdentityHandlesSyntaxAndFallbacks)
{
    const fs::path work = tmp_root() / "package-manifest-identity-unit";
    fs::remove_all(work);
    fs::create_directories(work);

    EXPECT_FALSE(driver::find_package_manifest_for_path(fs::path{}).has_value());
    EXPECT_FALSE(driver::package_manifest_identity_for_path(work / "missing.ax").has_value());

    const fs::path root_manifest = write_import_test_source(work / "pkg" / "aurex.toml",
        "# root package\n"
        "[tool]\n"
        "name = \"ignored\"\n"
        "[package]\n"
        "not_a_pair\n"
        "name = \"escaped\\\"pkg#core\" # trailing comment\n"
        "version = \"3.4.5\"\n");
    const fs::path source = work / "pkg" / "src" / "main.ax";
    const std::optional<fs::path> found_manifest = driver::find_package_manifest_for_path(source);
    ASSERT_TRUE(found_manifest.has_value());
    EXPECT_EQ(*found_manifest, driver::module_loader_canonical_or_absolute(root_manifest));

    const std::optional<std::string> identity = driver::package_manifest_identity_for_path(source);
    ASSERT_TRUE(identity.has_value());
    const std::string manifest_root = driver::module_loader_canonical_or_absolute(root_manifest).parent_path().string();
    const std::array<std::string_view, 4> expected_parts{
        driver::DRIVER_MANIFEST_PACKAGE_IDENTITY_KIND,
        "escaped\"pkg#core",
        "3.4.5",
        std::string_view(manifest_root),
    };
    EXPECT_EQ(driver::package_key_from_identity(*identity), query::package_key(expected_parts));

    static_cast<void>(write_import_test_source(work / "default_version" / "aurex.toml",
        "[package]\n"
        "name = \"default.version.pkg\"\n"));
    const std::optional<std::string> default_version_identity =
        driver::package_manifest_identity_for_path(work / "default_version");
    ASSERT_TRUE(default_version_identity.has_value());
    const std::string default_version_root =
        driver::module_loader_canonical_or_absolute(work / "default_version").string();
    const std::array<std::string_view, 4> default_version_parts{
        driver::DRIVER_MANIFEST_PACKAGE_IDENTITY_KIND,
        "default.version.pkg",
        driver::DRIVER_PACKAGE_MANIFEST_DEFAULT_VERSION,
        std::string_view(default_version_root),
    };
    EXPECT_EQ(driver::package_key_from_identity(*default_version_identity), query::package_key(default_version_parts));

    static_cast<void>(write_import_test_source(work / "invalid" / "aurex.toml",
        "[package]\n"
        "name = bare\n"
        "version = \"1\"\n"));
    EXPECT_FALSE(driver::package_manifest_identity_for_path(work / "invalid").has_value());

    fs::create_directories(work / "no_manifest");
    const std::string fallback_root = driver::module_loader_canonical_or_absolute(work / "no_manifest").string();
    const std::string fallback_identity = driver::package_identity_for_import_root(fallback_root);
    const std::array<std::string_view, 2> fallback_parts{
        driver::DRIVER_IMPORT_ROOT_PACKAGE_IDENTITY_KIND,
        std::string_view(fallback_root),
    };
    EXPECT_EQ(driver::package_key_from_identity(fallback_identity), query::package_key(fallback_parts));
    EXPECT_EQ(driver::package_key_for_import_root(fallback_root), query::package_key(fallback_parts));

    fs::remove_all(work);
}

} // namespace aurex::test
