#include <support/test_support.hpp>

#include <fstream>
#include <stdexcept>
#include <string_view>

namespace aurex::test {
namespace {

fs::path write_native_source_file(const fs::path& path, const std::string_view text)
{
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open " + path.string());
    }
    out << text;
    out.close();
    return path;
}

} // namespace

TEST_F(AurexIntegrationTest, NativeHelloDefaultExecutableOutputs)
{
    const fs::path hello = source_root() / "examples" / "hello.ax";
    const fs::path hello_bin = test_bin_root() / "hello";
    require_success(aurexc() + " " + q(hello) + " -o " + q(hello_bin));
    EXPECT_EQ(require_success(q(hello_bin)).output, "hello from Aurex M2\n");
}

TEST_F(AurexIntegrationTest, NativeHelloAssemblyOutput)
{
    const fs::path hello = source_root() / "examples" / "hello.ax";
    const fs::path asm_out = test_bin_root() / "hello.s";
    require_success(aurexc() + " --emit=asm " + q(hello) + " -o " + q(asm_out));
    EXPECT_GT(fs::file_size(asm_out), 0U);
}

TEST_F(AurexIntegrationTest, NativeHelloObjectOutput)
{
    const fs::path hello = source_root() / "examples" / "hello.ax";
    const fs::path obj_out = test_bin_root() / "hello.o";
    require_success(aurexc() + " --emit=obj " + q(hello) + " -o " + q(obj_out));
    EXPECT_GT(fs::file_size(obj_out), 0U);
}

TEST_F(AurexIntegrationTest, NativeHelloExplicitExecutableOutputs)
{
    const fs::path hello = source_root() / "examples" / "hello.ax";
    const fs::path direct = test_bin_root() / "hello.direct";
    require_success(aurexc() + " --emit=exe " + q(hello) + " -o " + q(direct));
    EXPECT_EQ(require_success(q(direct)).output, "hello from Aurex M2\n");
}

TEST_F(AurexIntegrationTest, NativeDefaultAndNamedArgumentsUseCheckedOrder)
{
    const fs::path source = write_native_source_file(tmp_root() / "native_default_named_arguments.ax",
        "module native_default_named_arguments;\n"
        "fn mix(a: i32, b: i32 = 10, c: i32 = 100) -> i32 {\n"
        "  return a + b + c;\n"
        "}\n"
        "struct Acc { base: i32; }\n"
        "impl Acc {\n"
        "  fn add(self: &Acc, value: i32, scale: i32 = 1) -> i32 {\n"
        "    return self.base + value * scale;\n"
        "  }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let a: Acc = Acc { base: 5 };\n"
        "  if mix(1) != 111 { return 1; }\n"
        "  if mix(1, 2) != 103 { return 2; }\n"
        "  if mix(a: 1, c: 3) != 14 { return 3; }\n"
        "  if mix(1, c: 3) != 14 { return 4; }\n"
        "  if a.add(value: 2) != 7 { return 5; }\n"
        "  if a.add(scale: 3, value: 2) != 11 { return 6; }\n"
        "  if mix(c: 4, a: 2) != 16 { return 7; }\n"
        "  return 0;\n"
        "}\n");

    const fs::path binary = test_bin_root() / "native_default_named_arguments";
    require_success(aurexc() + " --emit=exe " + q(source) + " -o " + q(binary));
    EXPECT_EQ(require_success(q(binary)).output, "");
}

TEST_F(AurexIntegrationTest, NativeDynTraitDispatchUsesRuntimeVtable)
{
    const fs::path source = write_native_source_file(tmp_root() / "native_dyn_trait_dispatch.ax",
        "module native_dyn_trait_dispatch;\n"
        "trait Draw {\n"
        "  fn draw(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "struct Socket { value: i32; }\n"
        "impl Draw for File {\n"
        "  fn draw(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "impl Draw for Socket {\n"
        "  fn draw(self: &Socket) -> i32 { return self.value + 20; }\n"
        "}\n"
        "fn render(drawable: &dyn Draw) -> i32 {\n"
        "  return drawable.draw();\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 7 };\n"
        "  let socket: Socket = Socket { value: 3 };\n"
        "  let total: i32 = render(&file) + render(&socket);\n"
        "  if total == 30 { return 0; }\n"
        "  return 1;\n"
        "}\n");

    const fs::path binary = test_bin_root() / "native_dyn_trait_dispatch";
    require_success(aurexc() + " --emit=exe " + q(source) + " -o " + q(binary));
    EXPECT_EQ(require_success(q(binary)).output, "");
}

TEST_F(AurexIntegrationTest, NativeMutableDynTraitDispatchKeepsConcreteReceiver)
{
    const fs::path source = write_native_source_file(tmp_root() / "native_mut_dyn_trait_dispatch.ax",
        "module native_mut_dyn_trait_dispatch;\n"
        "trait Accumulate {\n"
        "  fn add(self: &mut Self, delta: i32) -> i32;\n"
        "}\n"
        "struct Counter { value: i32; }\n"
        "struct Weighted { value: i32; }\n"
        "impl Accumulate for Counter {\n"
        "  fn add(self: &mut Counter, delta: i32) -> i32 {\n"
        "    self.value = self.value + delta;\n"
        "    return self.value;\n"
        "  }\n"
        "}\n"
        "impl Accumulate for Weighted {\n"
        "  fn add(self: &mut Weighted, delta: i32) -> i32 {\n"
        "    self.value = self.value + delta + 10;\n"
        "    return self.value;\n"
        "  }\n"
        "}\n"
        "fn apply(accumulator: &mut dyn Accumulate, delta: i32) -> i32 {\n"
        "  return accumulator.add(delta);\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  var counter: Counter = Counter { value: 1 };\n"
        "  var weighted: Weighted = Weighted { value: 2 };\n"
        "  let first: i32 = apply(&mut counter, 4);\n"
        "  let second: i32 = apply(&mut weighted, 4);\n"
        "  if first == 5 && second == 16 && counter.value == 5 && weighted.value == 16 { return 0; }\n"
        "  return 1;\n"
        "}\n");

    const fs::path binary = test_bin_root() / "native_mut_dyn_trait_dispatch";
    require_success(aurexc() + " --emit=exe " + q(source) + " -o " + q(binary));
    EXPECT_EQ(require_success(q(binary)).output, "");
}

TEST_F(AurexIntegrationTest, NativeDynTraitDefaultMethodAndAssociatedEqualityDispatch)
{
    const fs::path source = write_native_source_file(tmp_root() / "native_dyn_trait_default_assoc_dispatch.ax",
        "module native_dyn_trait_default_assoc_dispatch;\n"
        "trait Source {\n"
        "  type Item;\n"
        "  fn read(self: &Self) -> Self.Item;\n"
        "  fn bonus(self: &Self) -> i32 {\n"
        "    return 100;\n"
        "  }\n"
        "}\n"
        "struct File { value: i32; }\n"
        "struct Socket { value: i32; }\n"
        "impl Source for File {\n"
        "  type Item = i32;\n"
        "  fn read(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "impl Source for Socket {\n"
        "  type Item = i32;\n"
        "  fn read(self: &Socket) -> i32 { return self.value + 10; }\n"
        "}\n"
        "fn score(source: &dyn Source<Item = i32>) -> i32 {\n"
        "  return source.bonus() + source.read();\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 0 };\n"
        "  let socket: Socket = Socket { value: 5 };\n"
        "  let total: i32 = score(&file) + score(&socket);\n"
        "  if total == 215 { return 0; }\n"
        "  return 1;\n"
        "}\n");

    const fs::path binary = test_bin_root() / "native_dyn_trait_default_assoc_dispatch";
    require_success(aurexc() + " --emit=exe " + q(source) + " -o " + q(binary));
    EXPECT_EQ(require_success(q(binary)).output, "");
}

TEST_F(AurexIntegrationTest, NativeDynTraitSupertraitUpcastDispatchUsesParentVtable)
{
    const fs::path source = write_native_source_file(tmp_root() / "native_dyn_trait_supertrait_upcast.ax",
        "module native_dyn_trait_supertrait_upcast;\n"
        "trait Parent {\n"
        "  fn parent(self: &Self) -> i32;\n"
        "}\n"
        "trait Child: Parent {\n"
        "  fn child(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "struct Socket { value: i32; }\n"
        "impl Parent for File { fn parent(self: &File) -> i32 { return self.value; } }\n"
        "impl Child for File { fn child(self: &File) -> i32 { return self.value + 100; } }\n"
        "impl Parent for Socket { fn parent(self: &Socket) -> i32 { return self.value + 20; } }\n"
        "impl Child for Socket { fn child(self: &Socket) -> i32 { return self.value + 200; } }\n"
        "fn score(child: &dyn Child) -> i32 {\n"
        "  return child.parent() + child.child();\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 3 };\n"
        "  let socket: Socket = Socket { value: 4 };\n"
        "  let total: i32 = score(&file) + score(&socket);\n"
        "  if total == 334 { return 0; }\n"
        "  return 1;\n"
        "}\n");

    const fs::path binary = test_bin_root() / "native_dyn_trait_supertrait_upcast";
    require_success(aurexc() + " --emit=exe " + q(source) + " -o " + q(binary));
    EXPECT_EQ(require_success(q(binary)).output, "");
}

TEST_F(AurexIntegrationTest, NativeDynTraitCompositionProjectionDispatchesSelectedPrincipals)
{
    const fs::path source = write_native_source_file(tmp_root() / "native_dyn_trait_composition_projection.ax",
        "module native_dyn_trait_composition_projection;\n"
        "trait Draw {\n"
        "  fn draw(self: &Self) -> i32;\n"
        "}\n"
        "trait Debug {\n"
        "  fn debug(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "struct Socket { value: i32; }\n"
        "impl Draw for File { fn draw(self: &File) -> i32 { return self.value + 1; } }\n"
        "impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 10; } }\n"
        "impl Draw for Socket { fn draw(self: &Socket) -> i32 { return self.value + 100; } }\n"
        "impl Debug for Socket { fn debug(self: &Socket) -> i32 { return self.value + 1000; } }\n"
        "fn score(combo: &dyn (Debug + Draw)) -> i32 {\n"
        "  let draw: &dyn Draw = combo;\n"
        "  let debug: &dyn Debug = combo;\n"
        "  return draw.draw() + debug.debug();\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 7 };\n"
        "  let socket: Socket = Socket { value: 3 };\n"
        "  let total: i32 = score(&file) + score(&socket);\n"
        "  if total == 1131 { return 0; }\n"
        "  return 1;\n"
        "}\n");

    const fs::path binary = test_bin_root() / "native_dyn_trait_composition_projection";
    require_success(aurexc() + " --emit=exe " + q(source) + " -o " + q(binary));
    EXPECT_EQ(require_success(q(binary)).output, "");
}

TEST_F(AurexIntegrationTest, NativeDynTraitCompositionDirectDispatchUsesSelectedPrincipals)
{
    const fs::path source = write_native_source_file(tmp_root() / "native_dyn_trait_composition_direct.ax",
        "module native_dyn_trait_composition_direct;\n"
        "trait Draw {\n"
        "  fn draw(self: &Self) -> i32;\n"
        "}\n"
        "trait Debug {\n"
        "  fn debug(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "struct Socket { value: i32; }\n"
        "impl Draw for File { fn draw(self: &File) -> i32 { return self.value + 1; } }\n"
        "impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 10; } }\n"
        "impl Draw for Socket { fn draw(self: &Socket) -> i32 { return self.value + 100; } }\n"
        "impl Debug for Socket { fn debug(self: &Socket) -> i32 { return self.value + 1000; } }\n"
        "fn score(combo: &dyn (Debug + Draw)) -> i32 {\n"
        "  return combo.draw() + combo.debug();\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 7 };\n"
        "  let socket: Socket = Socket { value: 3 };\n"
        "  let total: i32 = score(&file) + score(&socket);\n"
        "  if total == 1131 { return 0; }\n"
        "  return 1;\n"
        "}\n");

    const fs::path binary = test_bin_root() / "native_dyn_trait_composition_direct";
    require_success(aurexc() + " --emit=exe " + q(source) + " -o " + q(binary));
    EXPECT_EQ(require_success(q(binary)).output, "");
}

TEST_F(AurexIntegrationTest, NativeDynTraitCompositionSupertraitProjectionDispatchesParentVtables)
{
    const fs::path source = write_native_source_file(
        tmp_root() / "native_dyn_trait_composition_supertrait_projection.ax",
        "module native_dyn_trait_composition_supertrait_projection;\n"
        "trait Parent {\n"
        "  fn parent(self: &Self) -> i32;\n"
        "}\n"
        "trait Child: Parent {\n"
        "  fn child(self: &Self) -> i32;\n"
        "}\n"
        "trait Debug {\n"
        "  fn debug(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "struct Socket { value: i32; }\n"
        "impl Parent for File { fn parent(self: &File) -> i32 { return self.value; } }\n"
        "impl Child for File { fn child(self: &File) -> i32 { return self.value + 100; } }\n"
        "impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 1000; } }\n"
        "impl Parent for Socket { fn parent(self: &Socket) -> i32 { return self.value + 20; } }\n"
        "impl Child for Socket { fn child(self: &Socket) -> i32 { return self.value + 200; } }\n"
        "impl Debug for Socket { fn debug(self: &Socket) -> i32 { return self.value + 2000; } }\n"
        "fn score(view: &dyn (Child + Debug)) -> i32 {\n"
        "  let parent: &dyn Parent = dynproject<Child, Parent>(view);\n"
        "  return parent.parent();\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 3 };\n"
        "  let socket: Socket = Socket { value: 4 };\n"
        "  let total: i32 = score(&file) + score(&socket);\n"
        "  if total == 27 { return 0; }\n"
        "  return 1;\n"
        "}\n");

    const fs::path binary = test_bin_root() / "native_dyn_trait_composition_supertrait_projection";
    require_success(aurexc() + " --emit=exe " + q(source) + " -o " + q(binary));
    EXPECT_EQ(require_success(q(binary)).output, "");
}

TEST_F(AurexIntegrationTest, NativeDynTraitCompositionSupertraitDirectDispatchesParentVtables)
{
    const fs::path source = write_native_source_file(
        tmp_root() / "native_dyn_trait_composition_supertrait_direct.ax",
        "module native_dyn_trait_composition_supertrait_direct;\n"
        "trait Parent {\n"
        "  fn parent(self: &Self) -> i32;\n"
        "}\n"
        "trait Child: Parent {\n"
        "  fn child(self: &Self) -> i32;\n"
        "}\n"
        "trait Debug {\n"
        "  fn debug(self: &Self) -> i32;\n"
        "}\n"
        "struct File { value: i32; }\n"
        "struct Socket { value: i32; }\n"
        "impl Parent for File { fn parent(self: &File) -> i32 { return self.value; } }\n"
        "impl Child for File { fn child(self: &File) -> i32 { return self.value + 100; } }\n"
        "impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 1000; } }\n"
        "impl Parent for Socket { fn parent(self: &Socket) -> i32 { return self.value + 20; } }\n"
        "impl Child for Socket { fn child(self: &Socket) -> i32 { return self.value + 200; } }\n"
        "impl Debug for Socket { fn debug(self: &Socket) -> i32 { return self.value + 2000; } }\n"
        "fn score(view: &dyn (Child + Debug)) -> i32 {\n"
        "  return view.parent();\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file: File = File { value: 3 };\n"
        "  let socket: Socket = Socket { value: 4 };\n"
        "  let total: i32 = score(&file) + score(&socket);\n"
        "  if total == 27 { return 0; }\n"
        "  return 1;\n"
        "}\n");

    const fs::path binary = test_bin_root() / "native_dyn_trait_composition_supertrait_direct";
    require_success(aurexc() + " --emit=exe " + q(source) + " -o " + q(binary));
    EXPECT_EQ(require_success(q(binary)).output, "");
}

} // namespace aurex::test
