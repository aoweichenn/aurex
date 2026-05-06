#include "support/test_support.hpp"

#include <fstream>
#include <stdexcept>
#include <string_view>

namespace aurex::test {
namespace {

fs::path write_source_file(const fs::path& path, const std::string_view text) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open " + path.string());
    }
    out << text;
    out.close();
    return path;
}

} // namespace

TEST_F(AurexIntegrationTest, StructAndEnumValidationRegressions) {
    const fs::path missing_field = write_source_file(
        tmp_root() / "missing_field.ax",
        "module missing_field;\n"
        "struct Pair { left: i32; right: i32; }\n"
        "fn main() -> i32 {\n"
        "  let value = Pair { left: 1 };\n"
        "  return value.left;\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(missing_field)).output,
        "struct literal is missing field: right"
    );

    const fs::path duplicate_init = write_source_file(
        tmp_root() / "duplicate_init.ax",
        "module duplicate_init;\n"
        "struct Pair { left: i32; right: i32; }\n"
        "fn main() -> i32 {\n"
        "  let value = Pair { left: 1, left: 2, right: 3 };\n"
        "  return value.left;\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(duplicate_init)).output,
        "duplicate struct literal field: left"
    );

    const fs::path duplicate_decl = write_source_file(
        tmp_root() / "duplicate_decl.ax",
        "module duplicate_decl;\n"
        "struct Pair { left: i32; left: i32; }\n"
        "fn main() -> i32 { return 0; }\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(duplicate_decl)).output,
        "duplicate struct field: left"
    );

    const fs::path generic_duplicate_decl = write_source_file(
        tmp_root() / "generic_duplicate_decl.ax",
        "module generic_duplicate_decl;\n"
        "struct Pair<T> { left: T; left: T; }\n"
        "fn main() -> i32 {\n"
        "  let value = Pair<i32> { left: 1 };\n"
        "  return value.left;\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_duplicate_decl)).output,
        "duplicate struct field: left"
    );

    const fs::path duplicate_case = write_source_file(
        tmp_root() / "duplicate_case.ax",
        "module duplicate_case;\n"
        "enum Payload: u8 {\n"
        "  some(i32) = 1,\n"
        "  some(i32) = 2,\n"
        "}\n"
        "fn main() -> i32 { return 0; }\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(duplicate_case)).output,
        "duplicate enum case: Payload.some"
    );

    const fs::path generic_duplicate_case = write_source_file(
        tmp_root() / "generic_duplicate_case.ax",
        "module generic_duplicate_case;\n"
        "enum Payload<T>: u8 {\n"
        "  some(T) = 1,\n"
        "  some(T) = 2,\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let value = Payload<i32>.some(1);\n"
        "  return 0;\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_duplicate_case)).output,
        "duplicate enum case: Payload.some"
    );
}

TEST_F(AurexIntegrationTest, IntegerLiteralRegressions) {
    const fs::path overflow = write_source_file(
        tmp_root() / "overflow.ax",
        "module overflow;\n"
        "fn main() -> i32 {\n"
        "  let value: u8 = 300;\n"
        "  return cast(i32, value);\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(overflow)).output,
        "initializer type does not match declared type"
    );

    const fs::path underscored = write_source_file(
        tmp_root() / "underscored.ax",
        "module underscored;\n"
        "fn main() -> i32 { return 1_000; }\n"
    );
    const std::string llvm_ir = require_success(aurexc() + " --emit=llvm-ir " + q(underscored)).output;
    expect_contains(llvm_ir, "ret i32 1000");
}

TEST_F(AurexIntegrationTest, GenericEnumConstructorMatchArmRegressions) {
    const fs::path source = write_source_file(
        tmp_root() / "generic_enum_match_arm.ax",
        "module generic_enum_match_arm;\n"
        "enum Option<T>: u8 { some(T) = 1, none = 2, }\n"
        "fn copy(input: Option<i32>) -> Option<i32> {\n"
        "  return match input {\n"
        "    .some(value) => Option.some(value),\n"
        "    .none => Option.none,\n"
        "  };\n"
        "}\n"
        "fn copy_none_first(input: Option<i32>) -> Option<i32> {\n"
        "  return match input {\n"
        "    .none => Option.none,\n"
        "    .some(value) => Option.some(value),\n"
        "  };\n"
        "}\n"
        "fn main() -> i32 { return 0; }\n"
    );
    require_success(aurexc() + " --check " + q(source));
}

TEST_F(AurexIntegrationTest, QualifiedGenericStaticMethodRegressions) {
    static_cast<void>(write_source_file(
        tmp_root() / "box.ax",
        "module box;\n"
        "pub struct Box<T> { value: T; }\n"
        "impl<T> Box<T> {\n"
        "  pub fn new(value: T) -> Box<T> { return Box<T> { value: value }; }\n"
        "}\n"
    ));
    const fs::path source = write_source_file(
        tmp_root() / "qualified_generic_static_method.ax",
        "module qualified_generic_static_method;\n"
        "import box as box;\n"
        "fn main() -> i32 {\n"
        "  let value: box::Box<i32> = box::Box<i32>.new(7);\n"
        "  return value.value;\n"
        "}\n"
    );
    require_success(aurexc() + " --check " + q(source));
}

TEST_F(AurexIntegrationTest, MainAndCliRegressions) {
    const fs::path const_argv = write_source_file(
        tmp_root() / "const_argv.ax",
        "module const_argv;\n"
        "fn main(argc: i32, argv: *const *const u8) -> i32 {\n"
        "  return argc;\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(const_argv)).output,
        "ordinary fn main parameters must be (argc: i32, argv: *mut *mut u8)"
    );

    const fs::path second = write_source_file(
        tmp_root() / "second.ax",
        "module second;\n"
        "fn main() -> i32 { return 0; }\n"
    );
    const CommandResult multiple_inputs =
        require_failure(aurexc() + " --emit=llvm-ir " + q(source_root() / "examples" / "hello.ax") + " " + q(second));
    expect_contains(multiple_inputs.output, "multiple input files are not supported");

    const fs::path export_c_main = write_source_file(
        tmp_root() / "export_c_main.ax",
        "module export_c_main;\n"
        "export c fn main() -> i32 @name(\"main\") {\n"
        "  return 0;\n"
        "}\n"
    );
    const std::string llvm_ir = require_success(aurexc() + " --emit=llvm-ir " + q(export_c_main)).output;
    expect_contains(llvm_ir, "define i32 @main()");
    expect_not_contains(llvm_ir, "aurex.main.result");
    expect_not_contains(llvm_ir, "call i32 @main(i32");
}

} // namespace aurex::test
