#include <support/test_support.hpp>

#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace aurex::test {
namespace {

constexpr int DIAGNOSTIC_STRESS_ERROR_COUNT = 140;

fs::path write_source_file(const fs::path& path, const std::string_view text)
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

TEST_F(AurexIntegrationTest, StructAndEnumValidationRegressions)
{
    const fs::path missing_field = write_source_file(tmp_root() / "missing_field.ax",
        "module missing_field;\n"
        "struct Pair { left: i32; right: i32; }\n"
        "fn main() -> i32 {\n"
        "  let value = Pair { left: 1 };\n"
        "  return value.left;\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(missing_field)).output, "struct literal is missing field: right");

    const fs::path duplicate_init = write_source_file(tmp_root() / "duplicate_init.ax",
        "module duplicate_init;\n"
        "struct Pair { left: i32; right: i32; }\n"
        "fn main() -> i32 {\n"
        "  let value = Pair { left: 1, left: 2, right: 3 };\n"
        "  return value.left;\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(duplicate_init)).output, "duplicate struct literal field: left");

    const fs::path duplicate_decl = write_source_file(tmp_root() / "duplicate_decl.ax",
        "module duplicate_decl;\n"
        "struct Pair { left: i32; left: i32; }\n"
        "fn main() -> i32 { return 0; }\n");
    const std::string duplicate_decl_output = require_failure(aurexc() + " --check " + q(duplicate_decl)).output;
    expect_contains(duplicate_decl_output, "duplicate struct field: left");
    expect_contains(duplicate_decl_output, "note: previous declaration of `left` is here");

    const fs::path duplicate_case = write_source_file(tmp_root() / "duplicate_case.ax",
        "module duplicate_case;\n"
        "enum Payload: u8 {\n"
        "  some(i32) = 1,\n"
        "  some(i32) = 2,\n"
        "}\n"
        "fn main() -> i32 { return 0; }\n");
    const std::string duplicate_case_output = require_failure(aurexc() + " --check " + q(duplicate_case)).output;
    expect_contains(duplicate_case_output, "duplicate enum case: Payload.some");
    expect_contains(duplicate_case_output, "note: previous declaration of `some` is here");
}

TEST_F(AurexIntegrationTest, DiagnosticQualityRegressions)
{
    const fs::path type_mismatch = write_source_file(tmp_root() / "diagnostic_type_mismatch.ax",
        "module diagnostic_type_mismatch;\n"
        "fn main() -> i32 {\n"
        "  let value: i32 = true;\n"
        "  return value;\n"
        "}\n");
    expect_contains_all(require_failure(aurexc() + " --check " + q(type_mismatch)).output,
        {
            "initializer type does not match declared type",
            "note: expected type: i32",
            "note: actual type: bool",
        });

    const fs::path suggestion = write_source_file(tmp_root() / "diagnostic_suggestion.ax",
        "module diagnostic_suggestion;\n"
        "fn main() -> i32 {\n"
        "  let count: i32 = 1;\n"
        "  return coutn;\n"
        "}\n");
    expect_contains_all(require_failure(aurexc() + " --check " + q(suggestion)).output,
        {
            "unknown name: coutn",
            "help: did you mean `count`?",
        });

    const fs::path type_suggestion = write_source_file(tmp_root() / "diagnostic_type_suggestion.ax",
        "module diagnostic_type_suggestion;\n"
        "struct Point { value: i32; }\n"
        "fn main() -> i32 {\n"
        "  let value: Piont = Point { value: 1 };\n"
        "  return value.value;\n"
        "}\n");
    expect_contains_all(require_failure(aurexc() + " --check " + q(type_suggestion)).output,
        {
            "unknown type: Piont",
            "help: did you mean `Point`?",
        });

    const fs::path function_suggestion = write_source_file(tmp_root() / "diagnostic_function_suggestion.ax",
        "module diagnostic_function_suggestion;\n"
        "fn compute(value: i32) -> i32 { return value; }\n"
        "fn main() -> i32 { return cmopute(1); }\n");
    expect_contains_all(require_failure(aurexc() + " --check " + q(function_suggestion)).output,
        {
            "unknown function: cmopute",
            "help: did you mean `compute`?",
        });

    const fs::path duplicate = write_source_file(tmp_root() / "diagnostic_previous_declaration.ax",
        "module diagnostic_previous_declaration;\n"
        "fn main() -> i32 {\n"
        "  let value: i32 = 1;\n"
        "  let value: i32 = 2;\n"
        "  return value;\n"
        "}\n");
    expect_contains_all(require_failure(aurexc() + " --check " + q(duplicate)).output,
        {
            "duplicate definition or shadowing is not allowed: value",
            "note: previous declaration of `value` is here",
        });

    const fs::path qualified_suggestions = write_source_file(tmp_root() / "diagnostic_qualified_suggestions.ax",
        "module diagnostic_qualified_suggestions;\n"
        "import samplelib.visibility as vis;\n"
        "fn main() -> i32 {\n"
        "  let value: vis.PubicInt = vis.answe;\n"
        "  return vis.exproted(value);\n"
        "}\n");
    expect_contains_all(
        require_failure(aurexc() + " " + sample_import_flags() + " --check " + q(qualified_suggestions)).output,
        {
            "unknown type in module samplelib.visibility: PubicInt",
            "help: did you mean `PublicInt`?",
            "unknown name in module samplelib.visibility: answe",
            "help: did you mean `answer`?",
            "unknown function in module samplelib.visibility: exproted",
            "help: did you mean `exported`?",
        });

    const fs::path alias_suggestion = write_source_file(tmp_root() / "diagnostic_alias_suggestion.ax",
        "module diagnostic_alias_suggestion;\n"
        "import samplelib.visibility as vis;\n"
        "fn main() -> i32 {\n"
        "  let value: vix.PublicInt = 1;\n"
        "  return value;\n"
        "}\n");
    expect_contains_all(
        require_failure(aurexc() + " " + sample_import_flags() + " --check " + q(alias_suggestion)).output,
        {
            "unknown import alias: vix",
            "help: did you mean `vis`?",
        });

    const fs::path field_suggestion = write_source_file(tmp_root() / "diagnostic_field_suggestion.ax",
        "module diagnostic_field_suggestion;\n"
        "struct Point { count: i32; }\n"
        "fn main() -> i32 {\n"
        "  let value = Point { coutn: 1 };\n"
        "  return value.coutn;\n"
        "}\n");
    expect_contains_all(require_failure(aurexc() + " --check " + q(field_suggestion)).output,
        {
            "unknown field in struct literal: coutn",
            "unknown field: coutn",
            "help: did you mean `count`?",
        });

    const fs::path invalid_initializer_move_recovery =
        write_source_file(tmp_root() / "diagnostic_invalid_initializer_move_recovery.ax",
            "module diagnostic_invalid_initializer_move_recovery;\n"
            "fn main() -> i32 {\n"
            "  let failed = (1 + 2)();\n"
            "  let again = failed;\n"
            "  return again;\n"
            "}\n");
    const std::string invalid_initializer_move_recovery_output =
        require_failure(aurexc() + " --check " + q(invalid_initializer_move_recovery)).output;
    expect_contains(invalid_initializer_move_recovery_output, "callee must be a function value");
    expect_not_contains(invalid_initializer_move_recovery_output, "use of moved value `failed`");

    const fs::path void_local_move_recovery = write_source_file(tmp_root() / "diagnostic_void_local_move_recovery.ax",
        "module diagnostic_void_local_move_recovery;\n"
        "fn main() -> i32 {\n"
        "  let failed: void = 0;\n"
        "  let again = failed;\n"
        "  return 0;\n"
        "}\n");
    const std::string void_local_move_recovery_output =
        require_failure(aurexc() + " --check " + q(void_local_move_recovery)).output;
    expect_contains(void_local_move_recovery_output, "local variable type is not valid storage");
    expect_not_contains(void_local_move_recovery_output, "use of moved value `failed`");

    const fs::path enum_case_suggestion = write_source_file(tmp_root() / "diagnostic_enum_case_suggestion.ax",
        "module diagnostic_enum_case_suggestion;\n"
        "enum Choice { ready, failed }\n"
        "fn main() -> i32 {\n"
        "  let value: Choice = Choice.reday;\n"
        "  return 0;\n"
        "}\n");
    expect_contains_all(require_failure(aurexc() + " --check " + q(enum_case_suggestion)).output,
        {
            "unknown enum case:",
            "Choice.reday",
            "help: did you mean `ready`?",
        });

    const fs::path multiline = write_source_file(tmp_root() / "diagnostic_multiline_span.ax",
        "module diagnostic_multiline_span;\n"
        "struct Pair { left: i32; right: i32; }\n"
        "fn main() -> i32 {\n"
        "  let value = Pair {\n"
        "    left: 1\n"
        "  };\n"
        "  return value.left;\n"
        "}\n");
    const std::string multiline_output = require_failure(aurexc() + " --check " + q(multiline)).output;
    expect_contains_all(multiline_output,
        {
            "struct literal is missing field: right",
            "  let value = Pair {",
            "    left: 1",
        });

    const std::string color_output =
        require_failure("AUREX_COLOR_DIAGNOSTICS=always " + aurexc() + " --check " + q(enum_case_suggestion)).output;
    expect_contains(color_output, "\033[1;31merror\033[0m");
    expect_contains(color_output, "\033[1;32m^\033[0m");

    const std::string no_color_output =
        require_failure("AUREX_COLOR_DIAGNOSTICS=never " + aurexc() + " --check " + q(enum_case_suggestion)).output;
    EXPECT_EQ(no_color_output.find("\033["), std::string::npos);

    const std::string invalid_color_mode_output =
        require_failure("AUREX_COLOR_DIAGNOSTICS=bogus " + aurexc() + " --check " + q(enum_case_suggestion)).output;
    EXPECT_EQ(invalid_color_mode_output.find("\033["), std::string::npos);

    const std::string no_color_env_output =
        require_failure("NO_COLOR=1 AUREX_COLOR_DIAGNOSTICS=auto " + aurexc() + " --check " + q(enum_case_suggestion))
            .output;
    EXPECT_EQ(no_color_env_output.find("\033["), std::string::npos);

    const std::string auto_color_output = require_failure(
        "env -u NO_COLOR AUREX_COLOR_DIAGNOSTICS=auto " + aurexc() + " --check " + q(enum_case_suggestion))
                                              .output;
    EXPECT_EQ(auto_color_output.find("\033["), std::string::npos);

    std::string many_errors_source = "module diagnostic_many_errors;\nfn main() -> i32 {\n";
    for (int index = 0; index < DIAGNOSTIC_STRESS_ERROR_COUNT; ++index) {
        many_errors_source += "  let value";
        many_errors_source += std::to_string(index);
        many_errors_source += " = missing";
        many_errors_source += std::to_string(index);
        many_errors_source += ";\n";
    }
    many_errors_source += "  return 0;\n}\n";
    const fs::path many_errors = write_source_file(tmp_root() / "diagnostic_many_errors.ax", many_errors_source);
    expect_contains(
        require_failure(aurexc() + " --check " + q(many_errors)).output, "error: too many diagnostics; suppressing");
}

TEST_F(AurexIntegrationTest, DiagnosticDeclarationQualityRegressions)
{
    const fs::path duplicate_type_alias = write_source_file(tmp_root() / "diagnostic_duplicate_type_alias.ax",
        "module diagnostic_duplicate_type_alias;\n"
        "type Value = i32;\n"
        "type Value = bool;\n"
        "fn main() -> i32 { return 0; }\n");
    expect_contains_all(require_failure(aurexc() + " --check " + q(duplicate_type_alias)).output,
        {
            "duplicate type definition in module diagnostic_duplicate_type_alias: Value",
            "note: previous declaration of `Value` is here",
        });

    const fs::path unknown_where_param = write_source_file(tmp_root() / "diagnostic_unknown_where_param.ax",
        "module diagnostic_unknown_where_param;\n"
        "fn id[T](value: T) -> T where U: Eq { return value; }\n"
        "fn main() -> i32 { return id(1); }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(unknown_where_param)).output,
        "where constraint references unknown generic parameter `U`");

    const fs::path enum_discriminant = write_source_file(tmp_root() / "diagnostic_enum_discriminant.ax",
        "module diagnostic_enum_discriminant;\n"
        "enum Choice: u8 { too_big = 18446744073709551616 }\n"
        "fn main() -> i32 { return 0; }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(enum_discriminant)).output,
        "enum discriminant literal is out of range");

    const fs::path function_value_collision = write_source_file(tmp_root() / "diagnostic_function_value_collision.ax",
        "module diagnostic_function_value_collision;\n"
        "const value: i32 = 1;\n"
        "fn value() -> i32 { return 2; }\n"
        "fn main() -> i32 { return value; }\n");
    expect_contains_all(require_failure(aurexc() + " --check " + q(function_value_collision)).output,
        {
            "duplicate value definition in module: value",
            "note: previous declaration of `value` is here",
        });
}

TEST_F(AurexIntegrationTest, IntegerLiteralRegressions)
{
    const fs::path overflow = write_source_file(tmp_root() / "overflow.ax",
        "module overflow;\n"
        "fn main() -> i32 {\n"
        "  let value: u8 = 300;\n"
        "  return cast[i32](value);\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(overflow)).output, "initializer type does not match declared type");

    const fs::path underscored = write_source_file(tmp_root() / "underscored.ax",
        "module underscored;\n"
        "fn main() -> i32 { return 1_000; }\n");
    const std::string llvm_ir = require_success(aurexc() + " --emit=llvm-ir " + q(underscored)).output;
    expect_contains(llvm_ir, "ret i32 1000");

    const fs::path shift_widths = write_source_file(tmp_root() / "shift_widths.ax",
        "module shift_widths;\n"
        "fn main() -> i32 {\n"
        "  let narrow_signed: i8 = 1i8 << 7i8;\n"
        "  let narrow_unsigned: u8 = 1u8 << 7u8;\n"
        "  let medium_signed: i16 = 1i16 << 15i16;\n"
        "  let medium_unsigned: u16 = 1u16 << 15u16;\n"
        "  let wide_signed: i64 = 1i64 << 63i64;\n"
        "  let wide_unsigned: u64 = 1u64 << 63u64;\n"
        "  let pointer_signed: isize = 1isize << 1isize;\n"
        "  let pointer_unsigned: usize = 1usize << 1usize;\n"
        "  return 0;\n"
        "}\n");
    require_success(aurexc() + " --check " + q(shift_widths));

    const fs::path aggregate_consts = write_source_file(tmp_root() / "aggregate_consts.ax",
        "module aggregate_consts;\n"
        "const REPEATED: [3]i32 = [1; 3];\n"
        "const PAIR: (i32, bool) = (1, true);\n"
        "fn main() -> i32 { return 0; }\n");
    require_success(aurexc() + " --check " + q(aggregate_consts));
}

TEST_F(AurexIntegrationTest, M2UnsafeBoundaries)
{
    const fs::path positive = positive_sample("pointers", "unsafe_minimal.ax");
    const std::string ast = require_success(aurexc() + " --emit=ast " + q(positive)).output;
    expect_contains_all(ast,
        {
            "fn from_raw unsafe",
            "unsafe_block",
            "alias unsafe fn(*const i32) -> i32",
        });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(positive)).output;
    expect_contains_all(checked,
        {
            "fn priv from_raw -> str unsafe",
            "type priv UnsafeRead = unsafe fn(*const i32) -> i32",
        });

    const fs::path binary = test_bin_root() / "unsafe_minimal";
    require_success(aurexc() + " " + q(positive) + " -o " + q(binary));
    require_success(q(binary));

    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("pointers", "unsafe_deref_required.ax"))).output,
        "raw pointer dereference requires unsafe context");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("pointers", "unsafe_ptrcast_required.ax"))).output,
        "ptrcast requires unsafe context");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("pointers", "unsafe_bitcast_required.ax"))).output,
        "bitcast requires unsafe context");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("pointers", "unsafe_ptrat_required.ax"))).output,
        "ptrat requires unsafe context");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("pointers", "unsafe_strraw_required.ax"))).output,
        "strraw requires unsafe context");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("pointers", "raw_pointer_field_requires_unsafe.ax")))
            .output,
        "raw pointer projection requires unsafe context");
    expect_contains(require_failure(aurexc() + " --check "
                        + q(negative_sample("pointers", "raw_pointer_field_write_requires_unsafe.ax")))
                        .output,
        "raw pointer projection requires unsafe context");
    expect_contains(require_failure(aurexc() + " --check "
                        + q(negative_sample("pointers", "raw_pointer_field_address_requires_unsafe.ax")))
                        .output,
        "raw pointer projection requires unsafe context");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("pointers", "raw_pointer_index_requires_unsafe.ax")))
            .output,
        "raw pointer projection requires unsafe context");
    expect_contains(require_failure(aurexc() + " --check "
                        + q(negative_sample("pointers", "raw_pointer_index_write_requires_unsafe.ax")))
                        .output,
        "raw pointer projection requires unsafe context");
    expect_contains(require_failure(aurexc() + " --check "
                        + q(negative_sample("pointers", "implicit_address_to_raw_pointer_rejected.ax")))
                        .output,
        "initializer type does not match declared type");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "unsafe_fn_call_required.ax"))).output,
        "call to unsafe function read_raw requires unsafe context");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "unsafe_fn_pointer_call_required.ax")))
            .output,
        "call to unsafe function");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("expressions", "unsafe_block_const_initializer.ax")))
            .output,
        "unsafe block cannot be used in const initializer");
}

TEST_F(AurexIntegrationTest, M2SafeReferences)
{
    const fs::path positive = positive_sample("types", "reference_basic.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(positive)).output;
    expect_contains_all(checked,
        {
            "fn priv read -> i32",
            "fn priv write -> void",
            "fn priv id_ref[i32] -> &i32",
        });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(positive)).output;
    expect_contains_all(ir,
        {
            "fn read(value: &i32)",
            "fn write(value: &mut i32",
            "ptrcast",
            "field_addr",
        });

    const fs::path binary = test_bin_root() / "reference_basic";
    require_success(aurexc() + " " + q(positive) + " -o " + q(binary));
    require_success(q(binary));

    const fs::path reference_slice = positive_sample("types", "reference_slice_index.ax");
    const std::string reference_slice_ir = require_success(aurexc() + " --emit=ir " + q(reference_slice)).output;
    expect_contains_all(reference_slice_ir,
        {
            "fn first(values: &[]const i32)",
            "slice_data",
            "index_addr",
        });
    require_success(aurexc() + " --emit=llvm-ir " + q(reference_slice));

    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "reference_mut_from_immutable.ax"))).output,
        "mutable reference requires a writable place expression");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "reference_assign_through_shared.ax")))
            .output,
        "left side of assignment must be writable");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "reference_invalid_pointee.ax"))).output,
        "reference requires a valid storage type");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "reference_mut_not_raw_pointer.ax")))
            .output,
        "initializer type does not match declared type");
}

TEST_F(AurexIntegrationTest, StringCheckedBoundary)
{
    const fs::path positive = positive_sample("types", "str_checked.ax");
    const std::string ir = require_success(aurexc() + " --emit=ir " + q(positive)).output;
    expect_contains_all(ir,
        {
            "strvalid",
            "strfromutf8",
            "str",
        });

    const std::string llvm_ir = require_success(aurexc() + " --emit=llvm-ir " + q(positive)).output;
    expect_contains_all(llvm_ir,
        {
            "__aurex_utf8_validate",
            "str.checked",
            "select i1",
        });

    const fs::path binary = test_bin_root() / "str_checked";
    require_success(aurexc() + " " + q(positive) + " -o " + q(binary));
    require_success(q(binary));

    const fs::path str_slice = positive_sample("types", "str_slice.ax");
    const std::string slice_ir = require_success(aurexc() + " --emit=ir " + q(str_slice)).output;
    expect_contains_all(slice_ir,
        {
            "strslice.checked",
            "strblen",
            "strptr",
        });

    const std::string slice_llvm_ir = require_success(aurexc() + " --emit=llvm-ir " + q(str_slice)).output;
    expect_contains_all(slice_llvm_ir,
        {
            "str.slice.ok",
            "__aurex_utf8_boundary",
            "select i1",
        });

    const fs::path slice_binary = test_bin_root() / "str_slice";
    require_success(aurexc() + " " + q(str_slice) + " -o " + q(slice_binary));
    require_success(q(slice_binary));

    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "strvalid_non_slice.ax"))).output,
        "str UTF-8 builtin requires a []const u8 or []mut u8 byte slice");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "strfromutf8_non_slice.ax"))).output,
        "str UTF-8 builtin requires a []const u8 or []mut u8 byte slice");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "str_slice_bound_non_integer.ax"))).output,
        "slice bound must be an integer");
}

TEST_F(AurexIntegrationTest, ArrayLiteralRegressions)
{
    const fs::path array_literal = positive_sample("types", "array_literal.ax");
    const std::string ir = require_success(aurexc() + " --emit=ir " + q(array_literal)).output;
    expect_contains_all(ir,
        {
            "aggregate [",
            "index_addr",
        });

    const fs::path array_binary = test_bin_root() / "array_literal";
    require_success(aurexc() + " " + q(array_literal) + " -o " + q(array_binary));
    require_success(q(array_binary));

    const fs::path repeat_literal = positive_sample("types", "array_repeat_literal.ax");
    const fs::path repeat_binary = test_bin_root() / "array_repeat_literal";
    require_success(aurexc() + " " + q(repeat_literal) + " -o " + q(repeat_binary));
    require_success(q(repeat_binary));

    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_literal_empty_infer.ax"))).output,
        "empty array literal requires an array type context");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_literal_expected_non_array.ax")))
            .output,
        "array literal requires an array expected type");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_literal_element_type_mismatch.ax")))
            .output,
        "array literal element type mismatch");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_literal_length_mismatch.ax")))
            .output,
        "array literal length mismatch");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_repeat_count_mismatch.ax"))).output,
        "array literal length mismatch");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_repeat_count_not_literal.ax")))
            .output,
        "array repeat count must be an integer literal");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_repeat_count_overflow.ax"))).output,
        "array repeat count literal is out of range");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_repeat_value_type_mismatch.ax")))
            .output,
        "array repeat value type mismatch");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_repeat_non_copy.ax"))).output,
        "array repeat value must be Copy when repeated more than once");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_constant_index_out_of_bounds.ax")))
            .output,
        "array constant index is out of bounds");
}

TEST_F(AurexIntegrationTest, SliceRegressions)
{
    const fs::path slice_basic = positive_sample("types", "slice_basic.ax");
    const std::string basic_ir = require_success(aurexc() + " --emit=ir " + q(slice_basic)).output;
    expect_contains_all(basic_ir,
        {
            "[]const i32",
            "slice ",
            "slice_data",
            "slice_len",
            "index_addr",
        });

    const fs::path basic_binary = test_bin_root() / "slice_basic";
    require_success(aurexc() + " " + q(slice_basic) + " -o " + q(basic_binary));
    require_success(q(basic_binary));

    const fs::path slice_mut = positive_sample("types", "slice_mut.ax");
    const std::string mut_ir = require_success(aurexc() + " --emit=ir " + q(slice_mut)).output;
    expect_contains_all(mut_ir,
        {
            "[]mut i32",
            "[]const i32",
            "slice_data",
        });

    const fs::path mut_binary = test_bin_root() / "slice_mut";
    require_success(aurexc() + " " + q(slice_mut) + " -o " + q(mut_binary));
    require_success(q(mut_binary));

    const fs::path slice_generic = positive_sample("types", "slice_generic.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(slice_generic)).output;
    expect_contains_all(checked,
        {
            "first[i32] -> i32",
        });
    const std::string generic_ir = require_success(aurexc() + " --emit=ir " + q(slice_generic)).output;
    expect_contains_all(generic_ir,
        {
            "fn first[i32](values: []const i32)",
            "[]mut i32",
        });

    const fs::path generic_binary = test_bin_root() / "slice_generic";
    require_success(aurexc() + " " + q(slice_generic) + " -o " + q(generic_binary));
    require_success(q(generic_binary));

    expect_contains(require_failure(aurexc() + " --check " + q(negative_sample("types", "slice_non_slice.ax"))).output,
        "slicing requires array, slice, or str value");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "slice_bound_non_integer.ax"))).output,
        "slice bound must be an integer");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_slice_bound_out_of_bounds.ax")))
            .output,
        "array constant slice bound is out of bounds");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_slice_bounds_order.ax"))).output,
        "array constant slice start exceeds end");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "slice_const_write.ax"))).output,
        "left side of assignment must be writable");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "slice_mut_from_const.ax"))).output,
        "initializer type does not match declared type");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "slice_invalid_object.ax"))).output,
        "unknown name: missing");
    const std::string invalid_element_output =
        require_failure(aurexc() + " --check " + q(negative_sample("types", "slice_invalid_element.ax"))).output;
    expect_contains(invalid_element_output, "function parameter type is not valid storage");
    expect_contains(invalid_element_output, "slice element type is not valid storage");
}

TEST_F(AurexIntegrationTest, TupleRegressions)
{
    const fs::path tuple_basic = positive_sample("types", "tuple_basic.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(tuple_basic)).output;
    expect_contains_all(checked,
        {
            "fn priv make_pair -> (i32, bool)",
            "fn priv first[i32,bool] -> i32",
            "fn priv second[i32,bool] -> bool",
        });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(tuple_basic)).output;
    expect_contains_all(ir,
        {
            "record tuple.",
            ".0: i32",
            ".1: bool",
            "fn make_pair(value: i32)",
            "field_addr",
            "aggregate {.0",
        });

    const fs::path binary = test_bin_root() / "tuple_basic";
    require_success(aurexc() + " " + q(tuple_basic) + " -o " + q(binary));
    require_success(q(binary));

    expect_contains(require_failure(aurexc() + " --check " + q(negative_sample("types", "empty_tuple_type.ax"))).output,
        "empty tuple type is not part of M2 syntax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "empty_tuple_literal.ax"))).output,
        "empty tuple literal is not part of M2 syntax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "empty_tuple_pattern.ax"))).output,
        "empty tuple pattern is not part of M2 syntax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_field_named_access.ax"))).output,
        "tuple field access requires a numeric field");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_field_out_of_range.ax"))).output,
        "tuple field index is out of range");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_literal_arity_mismatch.ax"))).output,
        "tuple literal arity does not match expected tuple type");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_literal_expected_non_tuple.ax")))
            .output,
        "tuple literal requires a tuple expected type");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_destructure_arity_mismatch.ax")))
            .output,
        "tuple destructuring pattern arity does not match tuple type");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_destructure_non_tuple.ax"))).output,
        "tuple destructuring requires a tuple value");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_invalid_storage.ax"))).output,
        "tuple literal type is not valid storage");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_literal_infer_invalid.ax"))).output,
        "local variable type cannot be inferred");
}

TEST_F(AurexIntegrationTest, EnumConstructorMatchArmRegressions)
{
    const fs::path source = write_source_file(tmp_root() / "enum_match_arm.ax",
        "module enum_match_arm;\n"
        "enum OptionI32: u8 { some(i32) = 1, none = 2, }\n"
        "fn copy(input: OptionI32) -> OptionI32 {\n"
        "  return match input {\n"
        "    .some(value) => OptionI32.some(value),\n"
        "    .none => OptionI32.none,\n"
        "  };\n"
        "}\n"
        "fn copy_none_first(input: OptionI32) -> OptionI32 {\n"
        "  return match input {\n"
        "    .none => OptionI32.none,\n"
        "    .some(value) => OptionI32.some(value),\n"
        "  };\n"
        "}\n"
        "fn main() -> i32 { return 0; }\n");
    require_success(aurexc() + " --check " + q(source));
}

TEST_F(AurexIntegrationTest, EnumAdtRegressions)
{
    const fs::path no_payload = positive_sample("types", "enum_adt_no_payload.ax");
    const fs::path no_payload_binary = test_bin_root() / "enum_adt_no_payload";
    require_success(aurexc() + " " + q(no_payload) + " -o " + q(no_payload_binary));
    require_success(q(no_payload_binary));

    const fs::path payload = positive_sample("pattern_matching", "enum_adt_payload.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(payload)).output;
    expect_contains_all(checked,
        {
            "struct priv enum_adt_payload.Token_span.payload fields=2",
            "case OptionI32_some : enum_adt_payload.OptionI32(i32)",
            "case Token_span : enum_adt_payload.Token(usize,usize)",
        });

    const fs::path payload_binary = test_bin_root() / "enum_adt_payload";
    require_success(aurexc() + " " + q(payload) + " -o " + q(payload_binary));
    require_success(q(payload_binary));

    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "enum_adt_duplicate_auto_discriminant.ax")))
            .output,
        "duplicate enum discriminant value");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "enum_adt_multi_payload_arity.ax"))).output,
        "enum payload constructor requires 2 arguments");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "enum_case_empty_payload.ax"))).output,
        "expected enum case payload type");
    expect_contains(require_failure(aurexc() + " --check "
                        + q(negative_sample("pattern_matching", "bare_enum_case_constructor.ax")))
                        .output,
        "unknown function: some");
}

TEST_F(AurexIntegrationTest, QualifiedStaticMethodRegressions)
{
    static_cast<void>(write_source_file(tmp_root() / "box.ax",
        "module box;\n"
        "pub struct BoxI32 { pub value: i32; }\n"
        "impl BoxI32 {\n"
        "  pub fn new(value: i32) -> BoxI32 { return BoxI32 { value: value }; }\n"
        "}\n"));
    const fs::path source = write_source_file(tmp_root() / "qualified_static_method.ax",
        "module qualified_static_method;\n"
        "import box as box;\n"
        "fn main() -> i32 {\n"
        "  let value: box.BoxI32 = box.BoxI32.new(7);\n"
        "  return value.value;\n"
        "}\n");
    require_success(aurexc() + " --check " + q(source));
}

TEST_F(AurexIntegrationTest, MainAndCliRegressions)
{
    const fs::path const_argv = write_source_file(tmp_root() / "const_argv.ax",
        "module const_argv;\n"
        "fn main(argc: i32, argv: *const *const u8) -> i32 {\n"
        "  return argc;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(const_argv)).output,
        "ordinary fn main parameters must be (argc: i32, argv: *mut *mut u8)");

    const fs::path second = write_source_file(tmp_root() / "second.ax",
        "module second;\n"
        "fn main() -> i32 { return 0; }\n");
    const CommandResult multiple_inputs =
        require_failure(aurexc() + " --emit=llvm-ir " + q(source_root() / "examples" / "hello.ax") + " " + q(second));
    expect_contains(multiple_inputs.output, "multiple input files are not supported");

    const fs::path export_c_main = write_source_file(tmp_root() / "export_c_main.ax",
        "module export_c_main;\n"
        "@name(\"main\")\n"
        "export c fn main() -> i32 {\n"
        "  return 0;\n"
        "}\n");
    const std::string llvm_ir = require_success(aurexc() + " --emit=llvm-ir " + q(export_c_main)).output;
    expect_contains(llvm_ir, "define i32 @main()");
    expect_not_contains(llvm_ir, "aurex.main.result");
    expect_not_contains(llvm_ir, "call i32 @main(i32");
}

TEST_F(AurexIntegrationTest, SymlinkedImportStillValidatesExpectedModuleName)
{
    const fs::path import_dir = tmp_root() / "symlink_imports";
    fs::create_directories(import_dir);
    static_cast<void>(write_source_file(import_dir / "a.ax",
        "module a;\n"
        "pub fn f() -> i32 { return 1; }\n"));

    std::error_code error;
    fs::remove(import_dir / "b.ax", error);
    error.clear();
    fs::create_symlink("a.ax", import_dir / "b.ax", error);
    if (error) {
        GTEST_SKIP() << "filesystem symlink creation failed: " << error.message();
    }

    const fs::path source = write_source_file(tmp_root() / "symlink_import_mismatch.ax",
        "module symlink_import_mismatch;\n"
        "import a as alpha;\n"
        "import b as beta;\n"
        "fn main() -> i32 { return alpha.f() + beta.f(); }\n");

    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(source)).output,
        "module declaration 'a' does not match import 'b'");
}

TEST_F(AurexIntegrationTest, M2GenericRegressions)
{
    const fs::path source = positive_sample("generics", "basic_m2.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked,
        {
            "struct priv Box[i32]",
            "struct priv Pair[i32,bool]",
            "id[i32] -> i32",
            "id[bool] -> bool",
            "make_pair[i32,bool]",
            "make_box[basic_m2.Pair[i32,bool]]",
            "unwrap[i32]",
            "same_ptr[i32]",
            "first[i32,bool]",
            "struct priv Box[basic_m2.Pair[i32,bool]]",
        });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir,
        {
            "fn id[i32](x: i32)",
            "fn id[bool](x: bool)",
            "fn make_pair[i32,bool](a: i32, b: bool)",
            "fn make_box[basic_m2.Pair[i32,bool]](value: basic_m2.Pair[i32,bool])",
            "fn unwrap[i32](box: basic_m2.Box[i32])",
            "fn same_ptr[i32](value: *const i32)",
            "fn first[i32,bool](p: basic_m2.Pair[i32,bool])",
        });

    const fs::path binary = test_bin_root() / "basic_m2_generics";
    require_success(aurexc() + " " + q(source) + " -o " + q(binary));
    require_success(q(binary));

    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "duplicate_param.ax"))).output,
        "duplicate generic parameter");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "type_arity_mismatch.ax"))).output,
        "too few generic arguments");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "type_arity_too_many.ax"))).output,
        "too many generic arguments");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "non_generic_type_args.ax"))).output,
        "type Plain is not generic");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "missing_type_args.ax"))).output,
        "generic type Box requires type arguments");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "inference_failure.ax"))).output,
        "cannot infer generic type argument");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "operator_unconstrained.ax"))).output,
        "generic type parameter `T` has no known operator");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "non_generic_fn_type_args.ax"))).output,
        "function plain is not generic");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_fn_type_args_too_many.ax")))
            .output,
        "too many generic arguments");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "empty_generic_params_fn.ax"))).output,
        "expected generic type parameter");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "empty_generic_params_struct.ax")))
            .output,
        "expected generic type parameter");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "empty_type_args.ax"))).output,
        "expected generic type argument");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "empty_explicit_generic_call.ax")))
            .output,
        "expected generic type argument");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_body_return_unknown.ax")))
            .output,
        "return type mismatch");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "duplicate_generic_param_fn.ax")))
            .output,
        "duplicate generic parameter");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "expression_index_not_generic_call.ax")))
            .output,
        "expects a type argument");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_bound_rejected_m2.ax"))).output,
        "generic bounds are not part of M2 syntax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "duplicate_with_plain_struct.ax")))
            .output,
        "duplicate type definition");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "duplicate_generic_struct.ax"))).output,
        "duplicate type definition");
    const fs::path duplicate_generic_enum = write_source_file(tmp_root() / "duplicate_generic_enum.ax",
        "module duplicate_generic_enum;\n"
        "enum Maybe[T] { none }\n"
        "enum Maybe[T] { some(T) }\n"
        "fn main() -> i32 { return 0; }\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(duplicate_generic_enum)).output, "duplicate type definition");
    const fs::path duplicate_generic_alias = write_source_file(tmp_root() / "duplicate_generic_alias.ax",
        "module duplicate_generic_alias;\n"
        "type Ptr[T] = *const T;\n"
        "type Ptr[T] = *mut T;\n"
        "fn main() -> i32 { return 0; }\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(duplicate_generic_alias)).output, "duplicate type definition");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "duplicate_with_plain_fn.ax"))).output,
        "duplicate function definition");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "duplicate_generic_fn.ax"))).output,
        "duplicate function definition");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "where_unknown_capability.ax"))).output,
        "unknown generic capability or trait predicate");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "where_empty_clause.ax"))).output,
        "expected generic parameter name in where clause");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "where_trailing_comma.ax"))).output,
        "expected generic parameter name in where clause");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "where_unknown_param.ax"))).output,
        "where constraint references unknown generic parameter");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "where_duplicate_capability.ax")))
            .output,
        "duplicate capability `Eq`");
    expect_contains_all(require_failure(aurexc() + " --check --diagnostics=json "
                            + q(negative_sample("generics", "where_duplicate_capability.ax")))
                            .output,
        {
            "\"category\": \"capability\"",
            "\"code\": \"SEM0450\"",
        });
    require_success(aurexc() + " --check " + q(positive_sample("generics", "where_copy_capability.ax")));
    expect_contains_all(require_failure(aurexc() + " --check --diagnostics=json "
                            + q(negative_sample("pointers", "raw_pointer_field_requires_unsafe.ax")))
                            .output,
        {
            "\"category\": \"safety\"",
            "\"code\": \"SEM0400\"",
        });
    expect_contains_all(require_failure(aurexc() + " --check --diagnostics=json "
                            + q(negative_sample("functions", "raw_pointer_method_reference_receiver_rejected.ax")))
                            .output,
        {
            "\"category\": \"type\"",
            "\"code\": \"SEM0100\"",
        });
    expect_contains_all(require_failure(aurexc() + " --check --diagnostics=json "
                            + q(negative_sample("functions", "unknown_method.ax")))
                            .output,
        {
            "\"category\": \"name_resolution\"",
            "\"code\": \"SEM0200\"",
        });
    expect_contains_all(require_failure(aurexc() + " --check --diagnostics=json "
                            + q(negative_sample("pattern_matching", "match_expression_missing_case.ax")))
                            .output,
        {
            "\"category\": \"pattern\"",
            "\"code\": \"SEM0501\"",
        });
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "where_drop_resource_capability.ax")))
            .output,
        "resource capabilities are not part of M2 where constraints");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "where_unsatisfied_eq.ax"))).output,
        "does not satisfy capability `Eq`");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_enum_arg_count.ax"))).output,
        "generic arguments");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_alias_arg_count.ax"))).output,
        "generic arguments");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_alias_cycle.ax"))).output,
        "cyclic type alias");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_enum_where_unsatisfied.ax")))
            .output,
        "does not satisfy capability `Eq`");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_alias_where_unsatisfied.ax")))
            .output,
        "does not satisfy capability `Eq`");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_impl_where_unsatisfied.ax")))
            .output,
        "does not satisfy capability `Eq`");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_impl_pointer_target.ax")))
            .output,
        "impl target must be a named type");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "public_generic_inferred_return.ax")))
            .output,
        "public function return type must be explicit");
    expect_contains(require_failure(aurexc() + " --check "
                        + q(negative_sample("generics", "qualified_explicit_generic_unknown_import.ax")))
                        .output,
        "unknown import alias: missing");
}

TEST_F(AurexIntegrationTest, M2GenericWhereEnumAliasAndImplRegressions)
{
    const fs::path source = positive_sample("generics", "constraints_enum_alias_impl_m2.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked,
        {
            "same[i32] -> bool",
            "lower[i32] -> bool",
            "hashable[i32] -> i32",
            "unwrap_or[i32]",
            "use_result -> i32",
            "read_sized_ptr[i32]",
            "case Option[i32]_some",
            "case Result[i32,i32]_ok",
            "method constraints_enum_alias_impl_m2.Box[i32].get[i32] -> i32",
            "method constraints_enum_alias_impl_m2.Box[i32].same_value[i32] -> bool",
        });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir,
        {
            "fn same[i32](left: i32, right: i32)",
            "fn lower[i32](left: i32, right: i32)",
            "fn hashable[i32](value: i32)",
            "fn unwrap_or[i32](value: constraints_enum_alias_impl_m2.Option[i32], fallback: i32)",
            "fn get[i32](self: &constraints_enum_alias_impl_m2.Box[i32])",
            "fn same_value[i32](self: &constraints_enum_alias_impl_m2.Box[i32], other: "
            "&constraints_enum_alias_impl_m2.Box[i32])",
        });

    const fs::path binary = test_bin_root() / "constraints_enum_alias_impl_m2";
    require_success(aurexc() + " " + q(source) + " -o " + q(binary));
    require_success(q(binary));
}

TEST_F(AurexIntegrationTest, M2GenericEnumAliasImportRegressions)
{
    const fs::path library = write_source_file(tmp_root() / "generic_ext.ax",
        "module generic_ext;\n"
        "pub enum Maybe[T] { some(T), none }\n"
        "pub type Ptr[T] = *const T;\n"
        "pub fn hashable[T](value: T) -> i32 where T: Hash { return 1; }\n");
    static_cast<void>(library);

    const fs::path source = write_source_file(tmp_root() / "generic_import_main.ax",
        "module generic_import_main;\n"
        "import generic_ext as g;\n"
        "fn unwrap(value: g.Maybe[i32]) -> i32 {\n"
        "  return match value { .some(inner) => inner, .none => 0 };\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let value: i32 = 3;\n"
        "  let ptr: g.Ptr[i32] = unsafe { ptrat[g.Ptr[i32]](ptraddr(&value)) };\n"
        "  let maybe: g.Maybe[i32] = g.Maybe[i32].some(39);\n"
        "  if ptraddr(ptr) == 0usize { return 1; }\n"
        "  return g.hashable[i32](value) + unwrap(maybe) - 40;\n"
        "}\n");

    const std::string checked =
        require_success(aurexc() + " --emit=checked -I " + q(tmp_root()) + " " + q(source)).output;
    expect_contains_all(checked,
        {
            "generic_ext.Maybe[i32]",
            "case Maybe[i32]_some",
            "hashable[i32] -> i32",
        });

    const fs::path enum_a = write_source_file(tmp_root() / "generic_enum_a.ax",
        "module generic_enum_a;\n"
        "pub enum Maybe[T] { none }\n");
    const fs::path enum_b = write_source_file(tmp_root() / "generic_enum_b.ax",
        "module generic_enum_b;\n"
        "pub enum Maybe[T] { none }\n");
    static_cast<void>(enum_a);
    static_cast<void>(enum_b);
    const fs::path ambiguous_enum = write_source_file(tmp_root() / "generic_enum_ambiguous.ax",
        "module generic_enum_ambiguous;\n"
        "import generic_enum_a;\n"
        "import generic_enum_b;\n"
        "fn main() -> i32 {\n"
        "  let value: Maybe[i32] = Maybe[i32].none;\n"
        "  return 0;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check -I " + q(tmp_root()) + " " + q(ambiguous_enum)).output,
        "unknown generic type: Maybe");
    const fs::path enum_facade = write_source_file(tmp_root() / "generic_enum_facade.ax",
        "module generic_enum_facade;\n"
        "pub import generic_enum_a as a;\n"
        "pub import generic_enum_b as b;\n");
    static_cast<void>(enum_facade);
    const fs::path ambiguous_enum_selector = write_source_file(tmp_root() / "generic_enum_selector_ambiguous.ax",
        "module generic_enum_selector_ambiguous;\n"
        "import generic_enum_facade as facade;\n"
        "fn main() -> i32 {\n"
        "  let value: facade.Maybe[i32] = facade.Maybe[i32].none;\n"
        "  return 0;\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " --check -I " + q(tmp_root()) + " " + q(ambiguous_enum_selector)).output,
        "ambiguous generic type name 'Maybe'");

    const fs::path alias_a = write_source_file(tmp_root() / "generic_alias_a.ax",
        "module generic_alias_a;\n"
        "pub type Ptr[T] = *const T;\n");
    const fs::path alias_b = write_source_file(tmp_root() / "generic_alias_b.ax",
        "module generic_alias_b;\n"
        "pub type Ptr[T] = *const T;\n");
    static_cast<void>(alias_a);
    static_cast<void>(alias_b);
    const fs::path ambiguous_alias = write_source_file(tmp_root() / "generic_alias_ambiguous.ax",
        "module generic_alias_ambiguous;\n"
        "import generic_alias_a;\n"
        "import generic_alias_b;\n"
        "fn main() -> i32 {\n"
        "  let value: i32 = 1;\n"
        "  let ptr: Ptr[i32] = null;\n"
        "  return 0;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check -I " + q(tmp_root()) + " " + q(ambiguous_alias)).output,
        "unknown generic type: Ptr");
    const fs::path alias_facade = write_source_file(tmp_root() / "generic_alias_facade.ax",
        "module generic_alias_facade;\n"
        "pub import generic_alias_a as a;\n"
        "pub import generic_alias_b as b;\n");
    static_cast<void>(alias_facade);
    const fs::path ambiguous_alias_selector = write_source_file(tmp_root() / "generic_alias_selector_ambiguous.ax",
        "module generic_alias_selector_ambiguous;\n"
        "import generic_alias_facade as facade;\n"
        "fn main() -> i32 {\n"
        "  let value: i32 = 1;\n"
        "  let ptr: facade.Ptr[i32] = null;\n"
        "  return 0;\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " --check -I " + q(tmp_root()) + " " + q(ambiguous_alias_selector)).output,
        "ambiguous generic type name 'Ptr'");
}

TEST_F(AurexIntegrationTest, M2GenericCapabilityConcreteTypeRegressions)
{
    const fs::path source = write_source_file(tmp_root() / "generic_capability_concrete_types.ax",
        "module generic_capability_concrete_types;\n"
        "enum Flag { no, yes }\n"
        "struct Box[T] { value: T; }\n"
        "fn accept_eq[T](value: T) -> i32 where T: Eq { return 1; }\n"
        "fn accept_hash[T](value: T) -> i32 where T: Hash { return 1; }\n"
        "fn same[T](left: T, right: T) -> i32 where T: Eq {\n"
        "  if left == right { return 1; }\n"
        "  return 0;\n"
        "}\n"
        "fn less_than[T](left: T, right: T) -> i32 where T: Ord {\n"
        "  if left < right { return 1; }\n"
        "  return 0;\n"
        "}\n"
        "impl[T] Box[T] {\n"
        "  fn get(self: &Box[T]) -> T where T: Copy { return self.value; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  var left: i32 = 1;\n"
        "  var right: i32 = 2;\n"
        "  let left_ptr: *const i32 = unsafe { ptrat[*const i32](ptraddr(&left)) };\n"
        "  let right_ptr: *const i32 = unsafe { ptrat[*const i32](ptraddr(&right)) };\n"
        "  let flag: Flag = Flag.yes;\n"
        "  let box: Box[i32] = Box[i32] { value: 1 };\n"
        "  return accept_eq(true) + accept_eq('\\u{03BB}') +\n"
        "    accept_eq(left_ptr) + accept_eq(flag) +\n"
        "    accept_hash(true) + accept_hash('\\u{03BB}') + accept_hash(left) + accept_hash(right_ptr) +\n"
        "    same(flag, Flag.yes) + less_than(left, right) + box.get() + box.get() - 12;\n"
        "}\n");

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked,
        {
            "accept_eq[bool] -> i32",
            "accept_eq[char] -> i32",
            "accept_eq[*const i32] -> i32",
            "accept_eq[generic_capability_concrete_types.Flag] -> i32",
            "accept_hash[bool] -> i32",
            "accept_hash[char] -> i32",
            "accept_hash[i32] -> i32",
            "accept_hash[*const i32] -> i32",
            "same[generic_capability_concrete_types.Flag] -> i32",
            "less_than[i32] -> i32",
            "method generic_capability_concrete_types.Box[i32].get[i32] -> i32",
        });

    const fs::path float_eq = write_source_file(tmp_root() / "generic_float_eq_rejected.ax",
        "module generic_float_eq_rejected;\n"
        "fn accept_eq[T](value: T) -> i32 where T: Eq { return 1; }\n"
        "fn main() -> i32 {\n"
        "  return accept_eq(cast[f64](1));\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(float_eq)).output, "type f64 does not satisfy capability `Eq`");

    const fs::path float_ord = write_source_file(tmp_root() / "generic_float_ord_rejected.ax",
        "module generic_float_ord_rejected;\n"
        "fn less_than[T](left: T, right: T) -> i32 where T: Ord {\n"
        "  if left < right { return 1; }\n"
        "  return 0;\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  return less_than(cast[f64](1), cast[f64](2));\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(float_ord)).output, "type f64 does not satisfy capability `Ord`");

    const fs::path reference_eq = write_source_file(tmp_root() / "generic_reference_eq_rejected.ax",
        "module generic_reference_eq_rejected;\n"
        "fn accept_eq[T](value: T) -> i32 where T: Eq { return 1; }\n"
        "fn main() -> i32 {\n"
        "  var value: i32 = 1;\n"
        "  let ref: &i32 = &value;\n"
        "  return accept_eq(ref);\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(reference_eq)).output, "type &i32 does not satisfy capability `Eq`");

    const fs::path reference_hash = write_source_file(tmp_root() / "generic_reference_hash_rejected.ax",
        "module generic_reference_hash_rejected;\n"
        "fn accept_hash[T](value: T) -> i32 where T: Hash { return 1; }\n"
        "fn main() -> i32 {\n"
        "  var value: i32 = 1;\n"
        "  let ref: &i32 = &value;\n"
        "  return accept_hash(ref);\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(reference_hash)).output,
        "type &i32 does not satisfy capability `Hash`");

    const fs::path enum_hash = write_source_file(tmp_root() / "generic_enum_hash_rejected.ax",
        "module generic_enum_hash_rejected;\n"
        "enum Flag { no, yes }\n"
        "fn accept_hash[T](value: T) -> i32 where T: Hash { return 1; }\n"
        "fn main() -> i32 {\n"
        "  let flag: Flag = Flag.yes;\n"
        "  return accept_hash(flag);\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(enum_hash)).output, "does not satisfy capability `Hash`");
}

TEST_F(AurexIntegrationTest, BuiltinDeriveCapabilityRegressions)
{
    const fs::path source = write_source_file(tmp_root() / "derive_capability_regressions.ax",
        "module derive_capability_regressions;\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Key { value: i32; }\n"
        "#[derive(Eq, Hash)]\n"
        "enum MaybeKey { some(Key), none }\n"
        "#[derive(Eq, Hash)]\n"
        "struct Box[T] where T: Eq + Hash { value: T; }\n"
        "fn accept_eq_hash[T](value: T) -> i32 where T: Eq + Hash { return 1; }\n"
        "fn accept_copy[T](value: T) -> i32 where T: Copy { return 1; }\n"
        "fn main() -> i32 {\n"
        "  let key = Key { value: 1 };\n"
        "  let boxed = Box[Key] { value: key };\n"
        "  let maybe = MaybeKey.some(key);\n"
        "  return accept_eq_hash(key) + accept_eq_hash(boxed) + accept_eq_hash(maybe) + accept_copy(key) - 4;\n"
        "}\n");

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked,
        {
            "struct priv Key derives=Copy,Eq,Hash",
            "struct priv Box[derive_capability_regressions.Key] derives=Eq,Hash",
            "case MaybeKey_some : derive_capability_regressions.MaybeKey"
            "(derive_capability_regressions.Key) derives=Eq,Hash",
            "accept_eq_hash[derive_capability_regressions.Key] -> i32",
            "accept_eq_hash[derive_capability_regressions.Box[derive_capability_regressions.Key]] -> i32",
            "accept_eq_hash[derive_capability_regressions.MaybeKey] -> i32",
            "accept_copy[derive_capability_regressions.Key] -> i32",
        });
}

TEST_F(AurexIntegrationTest, BuiltinDeriveCapabilityDiagnostics)
{
    const fs::path invalid_target = write_source_file(tmp_root() / "derive_invalid_target.ax",
        "module derive_invalid_target;\n"
        "#[derive(Eq)]\n"
        "fn main() -> i32 { return 0; }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(invalid_target)).output,
        "derive attributes are only supported on struct and enum declarations");

    const fs::path unsupported = write_source_file(tmp_root() / "derive_unsupported.ax",
        "module derive_unsupported;\n"
        "#[derive(Clone)]\n"
        "struct Bad { value: i32; }\n"
        "fn main() -> i32 { return 0; }\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(unsupported)).output, "unsupported derive capability: Clone");

    const fs::path macro_attribute = write_source_file(tmp_root() / "item_attribute_macro_unimplemented.ax",
        "module item_attribute_macro_unimplemented;\n"
        "#[builder(defaults(threads = 4))]\n"
        "struct Config { threads: i32; }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(macro_attribute)).output,
        "item attribute macros are parsed but macro expansion is not implemented yet: builder");

    const fs::path duplicate = write_source_file(tmp_root() / "derive_duplicate.ax",
        "module derive_duplicate;\n"
        "#[derive(Eq, Eq)]\n"
        "struct Bad { value: i32; }\n"
        "fn main() -> i32 { return 0; }\n");
    expect_contains_all(require_failure(aurexc() + " --check " + q(duplicate)).output,
        {
            "duplicate derive capability: Eq",
            "note: previous declaration of `Eq` is here",
        });

    const fs::path eq_field = write_source_file(tmp_root() / "derive_eq_field_rejected.ax",
        "module derive_eq_field_rejected;\n"
        "#[derive(Eq)]\n"
        "struct Bad { value: f64; }\n"
        "fn main() -> i32 { return 0; }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(eq_field)).output,
        "cannot derive Eq because field value does not satisfy Eq");

    const fs::path enum_payload = write_source_file(tmp_root() / "derive_enum_payload_rejected.ax",
        "module derive_enum_payload_rejected;\n"
        "#[derive(Hash)]\n"
        "enum Bad { some(f64), none }\n"
        "fn main() -> i32 { return 0; }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(enum_payload)).output,
        "cannot derive Hash because enum case some does not satisfy Hash");

    const fs::path copy_drop = write_source_file(tmp_root() / "derive_copy_drop_rejected.ax",
        "module derive_copy_drop_rejected;\n"
        "struct File { fd: i32; }\n"
        "impl Drop for File {\n"
        "  fn drop(self: deinit File) -> void {}\n"
        "}\n"
        "#[derive(Copy)]\n"
        "struct Wrapper { file: File; }\n"
        "fn main() -> i32 { return 0; }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(copy_drop)).output,
        "cannot derive Copy because field file does not satisfy Copy");

    const fs::path generic_unsatisfied = write_source_file(tmp_root() / "derive_generic_unsatisfied.ax",
        "module derive_generic_unsatisfied;\n"
        "#[derive(Eq)]\n"
        "struct Box[T] { value: T; }\n"
        "fn accept_eq[T](value: T) -> i32 where T: Eq { return 1; }\n"
        "fn main() -> i32 {\n"
        "  let value = Box[f64] { value: cast[f64](1) };\n"
        "  return accept_eq(value);\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_unsatisfied)).output,
        "does not satisfy capability `Eq`");
}

TEST_F(AurexIntegrationTest, M2GenericImplNestedOwnerRegressions)
{
    const fs::path source = write_source_file(tmp_root() / "generic_impl_nested_owner.ax",
        "module generic_impl_nested_owner;\n"
        "type Unary[T] = fn(T) -> T;\n"
        "struct Box[T] { value: T; }\n"
        "fn id_i32(value: i32) -> i32 { return value; }\n"
        "impl[T] Box[*const T] {\n"
        "  fn ptr_marker(self: &Box[*const T]) -> i32 { return 1; }\n"
        "}\n"
        "impl[T] Box[[]const T] {\n"
        "  fn slice_marker(self: &Box[[]const T]) -> i32 { return 2; }\n"
        "}\n"
        "impl[T] Box[[4]T] {\n"
        "  fn array_marker(self: &Box[[4]T]) -> i32 { return 3; }\n"
        "}\n"
        "impl[T] Box[(T, bool)] {\n"
        "  fn tuple_marker(self: &Box[(T, bool)]) -> i32 { return 4; }\n"
        "}\n"
        "impl[T] Box[Unary[T]] {\n"
        "  fn fn_marker(self: &Box[Unary[T]]) -> i32 { return 5; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let value: i32 = 1;\n"
        "  let ptr_box: Box[*const i32] = Box[*const i32] { value: unsafe { ptrat[*const i32](ptraddr(&value)) } };\n"
        "  let array: [4]i32 = [1, 2, 3, 4];\n"
        "  let slice: []const i32 = array[:];\n"
        "  let slice_box: Box[[]const i32] = Box[[]const i32] { value: slice };\n"
        "  let array_box: Box[[4]i32] = Box[[4]i32] { value: array };\n"
        "  let tuple_box: Box[(i32, bool)] = Box[(i32, bool)] { value: (1, true) };\n"
        "  let fn_box: Box[Unary[i32]] = Box[Unary[i32]] { value: id_i32 };\n"
        "  return ptr_box.ptr_marker() + slice_box.slice_marker() + array_box.array_marker() + "
        "tuple_box.tuple_marker() + fn_box.fn_marker() - 15;\n"
        "}\n");

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked,
        {
            "method generic_impl_nested_owner.Box[*const i32].ptr_marker[i32] -> i32",
            "method generic_impl_nested_owner.Box[[]const i32].slice_marker[i32] -> i32",
            "method generic_impl_nested_owner.Box[[4]i32].array_marker[i32] -> i32",
            "method generic_impl_nested_owner.Box[(i32, bool)].tuple_marker[i32] -> i32",
            "method generic_impl_nested_owner.Box[fn(i32) -> i32].fn_marker[i32] -> i32",
        });
}

TEST_F(AurexIntegrationTest, M2NestedGenericInstantiationRegressions)
{
    const fs::path source = write_source_file(tmp_root() / "nested_m2_generics.ax",
        "module nested_m2_generics;\n"
        "struct Holder[T] { value: T; }\n"
        "fn id[T](value: T) -> T { return value; }\n"
        "fn make_holder[T](value: T) -> Holder[T] {\n"
        "  return Holder[T] { value: id(value) };\n"
        "}\n"
        "fn unwrap_holder[T](holder: Holder[T]) -> T {\n"
        "  return id(holder.value);\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let holder = make_holder(7);\n"
        "  return unwrap_holder(holder) - id[i32](7);\n"
        "}\n");

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked,
        {
            "id[i32] -> i32",
            "make_holder[i32]",
            "unwrap_holder[i32]",
            "Holder[i32]",
        });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir,
        {
            "fn id[i32](value: i32)",
            "fn make_holder[i32](value: i32)",
            "fn unwrap_holder[i32]",
        });

    const fs::path binary = test_bin_root() / "nested_m2_generics";
    require_success(aurexc() + " " + q(source) + " -o " + q(binary));
    require_success(q(binary));
}

TEST_F(AurexIntegrationTest, M2GenericEdgeCasesAndImports)
{
    const fs::path import_dir = tmp_root() / "generic_imports";
    fs::create_directories(import_dir);
    static_cast<void>(write_source_file(import_dir / "generic_a.ax",
        "module generic_a;\n"
        "pub struct RemoteBox[T] { pub value: T; }\n"
        "priv struct SecretBox[T] { pub value: T; }\n"
        "pub enum RemoteMaybe[T] { none }\n"
        "priv enum SecretMaybe[T] { none }\n"
        "pub type RemotePtr[T] = *const T;\n"
        "priv type SecretPtr[T] = *const T;\n"
        "pub fn remote_id[T](value: T) -> T { return value; }\n"
        "priv fn secret_id[T](value: T) -> T { return value; }\n"
        "pub fn make_box[T](value: T) -> RemoteBox[T] {\n"
        "  return RemoteBox[T] { value: value };\n"
        "}\n"));
    static_cast<void>(write_source_file(import_dir / "generic_b.ax",
        "module generic_b;\n"
        "pub struct RemoteBox[T] { pub value: T; }\n"
        "pub fn remote_id[T](value: T) -> T { return value; }\n"));
    static_cast<void>(write_source_file(import_dir / "generic_method_owner.ax",
        "module generic_method_owner;\n"
        "pub struct Box[T] { pub value: T; }\n"));
    static_cast<void>(write_source_file(import_dir / "generic_method_ext_a.ax",
        "module generic_method_ext_a;\n"
        "import generic_method_owner as owner;\n"
        "impl[T] owner.Box[T] {\n"
        "  pub fn read(self: &owner.Box[T]) -> T where T: Copy { return self.value; }\n"
        "}\n"));
    static_cast<void>(write_source_file(import_dir / "generic_method_ext_b.ax",
        "module generic_method_ext_b;\n"
        "import generic_method_owner as owner;\n"
        "impl[T] owner.Box[T] {\n"
        "  pub fn read(self: &owner.Box[T]) -> T where T: Copy { return self.value; }\n"
        "}\n"));
    static_cast<void>(write_source_file(import_dir / "generic_method_ext_private.ax",
        "module generic_method_ext_private;\n"
        "import generic_method_owner as owner;\n"
        "impl[T] owner.Box[T] {\n"
        "  priv fn hidden(self: &owner.Box[T]) -> T where T: Copy { return self.value; }\n"
        "}\n"));

    const fs::path use_imported = write_source_file(tmp_root() / "use_imported_generics.ax",
        "module use_imported_generics;\n"
        "import generic_a as ga;\n"
        "fn main() -> i32 {\n"
        "  let box: ga.RemoteBox[i32] = ga.make_box[i32](ga.remote_id(5));\n"
        "  let ptr: *const i32 = unsafe { ptrat[*const i32](ptraddr(&box.value)) };\n"
        "  return ga.remote_id(unsafe { *ptr }) - 5;\n"
        "}\n");
    require_success(aurexc() + " -I " + q(import_dir) + " --check " + q(use_imported));

    const fs::path imported_checked_source = write_source_file(tmp_root() / "imported_checked_generics.ax",
        "module imported_checked_generics;\n"
        "import generic_a as ga;\n"
        "fn main() -> i32 {\n"
        "  let box: ga.RemoteBox[i32] = ga.make_box(9);\n"
        "  return ga.remote_id(box.value) - 9;\n"
        "}\n");
    const std::string checked =
        require_success(aurexc() + " -I " + q(import_dir) + " --emit=checked " + q(imported_checked_source)).output;
    expect_contains_all(checked,
        {
            "RemoteBox[i32]",
            "remote_id[i32]",
            "make_box[i32]",
        });

    const fs::path private_generic_type = write_source_file(tmp_root() / "private_generic_type.ax",
        "module private_generic_type;\n"
        "import generic_a as ga;\n"
        "fn main() -> i32 {\n"
        "  let box: ga.SecretBox[i32] = ga.SecretBox[i32] { value: 1 };\n"
        "  return box.value;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_generic_type)).output,
        "generic type is private: generic_a.SecretBox");
    const fs::path private_generic_enum = write_source_file(tmp_root() / "private_generic_enum.ax",
        "module private_generic_enum;\n"
        "import generic_a as ga;\n"
        "fn main() -> i32 {\n"
        "  let value: ga.SecretMaybe[i32] = ga.SecretMaybe[i32].none;\n"
        "  return 0;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_generic_enum)).output,
        "generic type is private: generic_a.SecretMaybe");
    const fs::path private_generic_alias = write_source_file(tmp_root() / "private_generic_alias.ax",
        "module private_generic_alias;\n"
        "import generic_a as ga;\n"
        "fn main() -> i32 {\n"
        "  let value: i32 = 1;\n"
        "  let ptr: ga.SecretPtr[i32] = null;\n"
        "  return 0;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_generic_alias)).output,
        "generic type is private: generic_a.SecretPtr");
    const fs::path unqualified_private_generic_enum =
        write_source_file(tmp_root() / "unqualified_private_generic_enum.ax",
            "module unqualified_private_generic_enum;\n"
            "import generic_a;\n"
            "fn main() -> i32 {\n"
            "  let value: SecretMaybe[i32] = SecretMaybe[i32].none;\n"
            "  return 0;\n"
            "}\n");
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(unqualified_private_generic_enum)).output,
        "unknown generic type: SecretMaybe");
    const fs::path unqualified_private_generic_alias =
        write_source_file(tmp_root() / "unqualified_private_generic_alias.ax",
            "module unqualified_private_generic_alias;\n"
            "import generic_a;\n"
            "fn main() -> i32 {\n"
            "  let value: i32 = 1;\n"
            "  let ptr: SecretPtr[i32] = null;\n"
            "  return 0;\n"
            "}\n");
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(unqualified_private_generic_alias)).output,
        "unknown generic type: SecretPtr");

    const fs::path private_generic_fn = write_source_file(tmp_root() / "private_generic_fn.ax",
        "module private_generic_fn;\n"
        "import generic_a as ga;\n"
        "fn main() -> i32 { return ga.secret_id[i32](1); }\n");
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_generic_fn)).output,
        "generic function is private: generic_a.secret_id");

    const fs::path ambiguous_generic = write_source_file(tmp_root() / "ambiguous_generic.ax",
        "module ambiguous_generic;\n"
        "import generic_a;\n"
        "import generic_b;\n"
        "fn main() -> i32 {\n"
        "  let box: RemoteBox[i32] = RemoteBox[i32] { value: 1 };\n"
        "  return remote_id(box.value);\n"
        "}\n");
    const std::string ambiguous_output =
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(ambiguous_generic)).output;
    expect_contains(ambiguous_output, "unknown generic type: RemoteBox");
    expect_contains(ambiguous_output, "unknown function: remote_id");

    static_cast<void>(write_source_file(import_dir / "generic_facade.ax",
        "module generic_facade;\n"
        "pub import generic_a as a;\n"
        "pub import generic_b as b;\n"));
    const fs::path ambiguous_generic_selector = write_source_file(tmp_root() / "ambiguous_generic_selector.ax",
        "module ambiguous_generic_selector;\n"
        "import generic_facade as facade;\n"
        "fn main() -> i32 {\n"
        "  let box: facade.RemoteBox[i32] = facade.RemoteBox[i32] { value: 1 };\n"
        "  return facade.remote_id(box.value);\n"
        "}\n");
    const std::string selector_ambiguous_output =
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(ambiguous_generic_selector)).output;
    expect_contains(selector_ambiguous_output, "ambiguous generic type name");
    expect_contains(selector_ambiguous_output, "ambiguous generic function name");

    const fs::path ambiguous_generic_method = write_source_file(tmp_root() / "ambiguous_generic_method.ax",
        "module ambiguous_generic_method;\n"
        "import generic_method_owner as owner;\n"
        "import generic_method_ext_a;\n"
        "import generic_method_ext_b;\n"
        "fn main() -> i32 {\n"
        "  let box: owner.Box[i32] = owner.Box[i32] { value: 1 };\n"
        "  return box.read();\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(ambiguous_generic_method)).output,
        "unknown method");

    const fs::path private_generic_method = write_source_file(tmp_root() / "private_generic_method.ax",
        "module private_generic_method;\n"
        "import generic_method_owner as owner;\n"
        "import generic_method_ext_private;\n"
        "fn main() -> i32 {\n"
        "  let box: owner.Box[i32] = owner.Box[i32] { value: 1 };\n"
        "  return box.hidden();\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_generic_method)).output,
        "unknown method");

    const fs::path unqualified_private_generics = write_source_file(tmp_root() / "unqualified_private_generics.ax",
        "module unqualified_private_generics;\n"
        "import generic_a;\n"
        "fn main() -> i32 {\n"
        "  let box: SecretBox[i32] = SecretBox[i32] { value: 1 };\n"
        "  return box.value;\n"
        "}\n");
    const std::string unqualified_private_output =
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(unqualified_private_generics)).output;
    expect_contains(unqualified_private_output, "unknown generic type: SecretBox");

    const fs::path unqualified_private_generic_fn = write_source_file(tmp_root() / "unqualified_private_generic_fn.ax",
        "module unqualified_private_generic_fn;\n"
        "import generic_a;\n"
        "fn main() -> i32 { return secret_id[i32](1); }\n");
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(unqualified_private_generic_fn)).output,
        "unknown generic function: secret_id");

    const fs::path generic_inference_edges = write_source_file(tmp_root() / "generic_inference_edges.ax",
        "module generic_inference_edges;\n"
        "struct Box[T] { value: T; }\n"
        "struct ArrayBox[T] { data: [2]T; }\n"
        "fn pick[T](lhs: T, rhs: T) -> T { return lhs; }\n"
        "fn ptr_id[T](value: *const T) -> *const T { return value; }\n"
        "fn array_ptr_id[T](value: *mut [2]T) -> *mut [2]T { return value; }\n"
        "fn main() -> i32 {\n"
        "  let b: Box[i32] = Box[i32] { value: 1 };\n"
        "  let chosen: Box[i32] = pick(b, Box[i32] { value: 2 });\n"
        "  let ptr: *const i32 = unsafe { ptrat[*const i32](ptraddr(&chosen.value)) };\n"
        "  let same_ptr = ptr_id(ptr);\n"
        "  let values: *mut [2]i32 = null;\n"
        "  let same_values = array_ptr_id(values);\n"
        "  let size: usize = sizeof[ArrayBox[i32]];\n"
        "  return unsafe { *same_ptr } + unsafe { (*same_values)[0] } + cast[i32](size) - 9;\n"
        "}\n");
    require_success(aurexc() + " --check " + q(generic_inference_edges));

    const fs::path generic_placeholder_edges = write_source_file(tmp_root() / "generic_placeholder_edges.ax",
        "module generic_placeholder_edges;\n"
        "type Unary[T] = fn(T) -> T;\n"
        "struct Box[T] { value: T; }\n"
        "fn id[T](value: T) -> T { return value; }\n"
        "fn id_i32(value: i32) -> i32 { return value; }\n"
        "fn ptr_box[T](value: *const T) -> Box[*const T] {\n"
        "  return Box[*const T] { value: id[*const T](value) };\n"
        "}\n"
        "fn array_ptr_box[T](value: *mut [2]T) -> Box[*mut [2]T] {\n"
        "  return Box[*mut [2]T] { value: id[*mut [2]T](value) };\n"
        "}\n"
        "fn slice_box[T](value: []const T) -> Box[[]const T] {\n"
        "  return Box[[]const T] { value: id[[]const T](value) };\n"
        "}\n"
        "fn function_box[T](value: Unary[T]) -> Box[Unary[T]] {\n"
        "  return Box[Unary[T]] { value: id[Unary[T]](value) };\n"
        "}\n"
        "fn tuple_box[T](value: (T, bool)) -> Box[(T, bool)] {\n"
        "  return Box[(T, bool)] { value: id[(T, bool)](value) };\n"
        "}\n"
        "fn wrap_ptr[U](value: *const U) -> Box[*const U] {\n"
        "  return ptr_box[U](value);\n"
        "}\n"
        "fn wrap_array[V](value: *mut [2]V) -> Box[*mut [2]V] {\n"
        "  return array_ptr_box[V](value);\n"
        "}\n"
        "fn wrap_slice[W](value: []const W) -> Box[[]const W] {\n"
        "  return slice_box[W](value);\n"
        "}\n"
        "fn wrap_function[X](value: Unary[X]) -> Box[Unary[X]] {\n"
        "  return function_box[X](value);\n"
        "}\n"
        "fn wrap_tuple[Y](value: (Y, bool)) -> Box[(Y, bool)] {\n"
        "  return tuple_box[Y](value);\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let ptr: *const i32 = null;\n"
        "  let boxed = wrap_ptr(ptr);\n"
        "  let values: *mut [2]i32 = null;\n"
        "  let array_boxed = wrap_array(values);\n"
        "  let size: usize = sizeof[Box[*const i32]] + sizeof[Box[*mut [2]i32]];\n"
        "  return cast[i32](size) - cast[i32](sizeof[Box[*const i32]] + sizeof[Box[*mut [2]i32]]);\n"
        "}\n");
    require_success(aurexc() + " --check " + q(generic_placeholder_edges));

    const fs::path missing_visible_generic_type = write_source_file(tmp_root() / "missing_visible_generic_type.ax",
        "module missing_visible_generic_type;\n"
        "fn main() -> i32 {\n"
        "  let value: Missing[i32] = Missing[i32] { value: 1 };\n"
        "  return 0;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(missing_visible_generic_type)).output,
        "unknown generic type: Missing");

    const fs::path missing_qualified_generic_type = write_source_file(tmp_root() / "missing_qualified_generic_type.ax",
        "module missing_qualified_generic_type;\n"
        "import generic_a as ga;\n"
        "fn main() -> i32 {\n"
        "  let value: ga.Missing[i32] = ga.Missing[i32] { value: 1 };\n"
        "  return 0;\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(missing_qualified_generic_type)).output,
        "unknown generic type in module generic_a: Missing");

    const fs::path missing_visible_generic_fn = write_source_file(tmp_root() / "missing_visible_generic_fn.ax",
        "module missing_visible_generic_fn;\n"
        "fn main() -> i32 { return missing[i32](1); }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(missing_visible_generic_fn)).output,
        "unknown generic function: missing");

    const fs::path invalid_generic_type_arg = write_source_file(tmp_root() / "invalid_generic_type_arg.ax",
        "module invalid_generic_type_arg;\n"
        "struct Box[T] { value: T; }\n"
        "fn id[T](value: T) -> T { return value; }\n"
        "fn main() -> i32 {\n"
        "  let value: Box[Missing] = Box[Missing] { value: 1 };\n"
        "  return id[Missing](1);\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(invalid_generic_type_arg)).output, "unknown type: Missing");

    const fs::path missing_qualified_generic_fn = write_source_file(tmp_root() / "missing_qualified_generic_fn.ax",
        "module missing_qualified_generic_fn;\n"
        "import generic_a as ga;\n"
        "fn main() -> i32 { return ga.missing[i32](1); }\n");
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(missing_qualified_generic_fn)).output,
        "unknown generic function in module generic_a: missing");

    const fs::path generic_struct_duplicate_field = write_source_file(tmp_root() / "generic_struct_duplicate_field.ax",
        "module generic_struct_duplicate_field;\n"
        "struct Bad[T] { value: T; value: T; }\n"
        "fn main() -> i32 {\n"
        "  let size: usize = sizeof[Bad[i32]];\n"
        "  return cast[i32](size);\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_struct_duplicate_field)).output,
        "duplicate struct field: value");

    const fs::path generic_struct_invalid_field = write_source_file(tmp_root() / "generic_struct_invalid_field.ax",
        "module generic_struct_invalid_field;\n"
        "struct Bad[T] { value: void; }\n"
        "fn main() -> i32 {\n"
        "  let size: usize = sizeof[Bad[i32]];\n"
        "  return cast[i32](size);\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_struct_invalid_field)).output,
        "field type is not valid storage");

    const fs::path generic_impl_missing_target = write_source_file(tmp_root() / "generic_impl_missing_target.ax",
        "module generic_impl_missing_target;\n"
        "impl[T] Missing[T] {\n"
        "  fn value(self: &Missing[T]) -> T { return self.value; }\n"
        "}\n"
        "fn main() -> i32 { return 0; }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_impl_missing_target)).output,
        "unknown generic type: Missing");

    const fs::path generic_struct_where_unsatisfied =
        write_source_file(tmp_root() / "generic_struct_where_unsatisfied.ax",
            "module generic_struct_where_unsatisfied;\n"
            "struct Pair { first: i32; second: i32; }\n"
            "struct EqBox[T] where T: Eq { value: T; }\n"
            "fn main() -> i32 {\n"
            "  let value: EqBox[Pair] = EqBox[Pair] { value: Pair { first: 1, second: 2 } };\n"
            "  return 0;\n"
            "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_struct_where_unsatisfied)).output,
        "does not satisfy capability `Eq`");

    const fs::path invalid_generic_enum_arg = write_source_file(tmp_root() / "invalid_generic_enum_arg.ax",
        "module invalid_generic_enum_arg;\n"
        "enum Maybe[T] { none }\n"
        "fn main() -> i32 {\n"
        "  let value: Maybe[Missing] = Maybe[Missing].none;\n"
        "  return 0;\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(invalid_generic_enum_arg)).output, "unknown type: Missing");

    const fs::path invalid_generic_alias_arg = write_source_file(tmp_root() / "invalid_generic_alias_arg.ax",
        "module invalid_generic_alias_arg;\n"
        "type Ptr[T] = *const T;\n"
        "fn main() -> i32 {\n"
        "  let value: Ptr[Missing] = null;\n"
        "  return 0;\n"
        "}\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(invalid_generic_alias_arg)).output, "unknown type: Missing");

    const fs::path mismatched_inference = write_source_file(tmp_root() / "mismatched_inference.ax",
        "module mismatched_inference;\n"
        "fn pick[T](lhs: T, rhs: T) -> T { return lhs; }\n"
        "fn main() -> i32 { return pick(1, true); }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(mismatched_inference)).output,
        "cannot infer generic type argument for call to pick");

    const fs::path pointer_inference_mutability_mismatch =
        write_source_file(tmp_root() / "pointer_inference_mutability_mismatch.ax",
            "module pointer_inference_mutability_mismatch;\n"
            "fn ptr_id[T](value: *mut T) -> *mut T { return value; }\n"
            "fn main() -> i32 {\n"
            "  let ptr: *const i32 = null;\n"
            "  let same = ptr_id(ptr);\n"
            "  return 0;\n"
            "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(pointer_inference_mutability_mismatch)).output,
        "cannot infer generic type argument for call to ptr_id");

    const fs::path reference_inference_mutability_mismatch =
        write_source_file(tmp_root() / "reference_inference_mutability_mismatch.ax",
            "module reference_inference_mutability_mismatch;\n"
            "fn ref_id[T](value: &mut T) -> &mut T { return value; }\n"
            "fn main() -> i32 {\n"
            "  var value: i32 = 1;\n"
            "  let shared: &i32 = &value;\n"
            "  let same = ref_id(shared);\n"
            "  return 0;\n"
            "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(reference_inference_mutability_mismatch)).output,
        "cannot infer generic type argument for call to ref_id");

    const fs::path slice_inference_mutability_mismatch =
        write_source_file(tmp_root() / "slice_inference_mutability_mismatch.ax",
            "module slice_inference_mutability_mismatch;\n"
            "fn slice_id[T](value: []mut T) -> []mut T { return value; }\n"
            "fn main() -> i32 {\n"
            "  let values: [2]i32 = [1, 2];\n"
            "  let slice: []const i32 = values[:];\n"
            "  let same = slice_id(slice);\n"
            "  return 0;\n"
            "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(slice_inference_mutability_mismatch)).output,
        "cannot infer generic type argument for call to slice_id");

    const fs::path array_inference_count_mismatch = write_source_file(tmp_root() / "array_inference_count_mismatch.ax",
        "module array_inference_count_mismatch;\n"
        "fn array_ptr_id[T](value: *mut [2]T) -> *mut [2]T { return value; }\n"
        "fn main() -> i32 {\n"
        "  let values: *mut [3]i32 = null;\n"
        "  let same = array_ptr_id(values);\n"
        "  return 0;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(array_inference_count_mismatch)).output,
        "cannot infer generic type argument for call to array_ptr_id");

    const fs::path builtin_inference_mismatch = write_source_file(tmp_root() / "builtin_inference_mismatch.ax",
        "module builtin_inference_mismatch;\n"
        "fn require_i32[T](value: i32, payload: T) -> T { return payload; }\n"
        "fn main() -> i32 { return require_i32(true, 1); }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(builtin_inference_mismatch)).output,
        "cannot infer generic type argument for call to require_i32");

    const fs::path struct_origin_inference_mismatch =
        write_source_file(tmp_root() / "struct_origin_inference_mismatch.ax",
            "module struct_origin_inference_mismatch;\n"
            "struct Box[T] { value: T; }\n"
            "struct Wrap[T] { value: T; }\n"
            "fn take_box[T](value: Box[T]) -> T { return value.value; }\n"
            "fn main() -> i32 { return take_box(Wrap[i32] { value: 1 }); }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(struct_origin_inference_mismatch)).output,
        "cannot infer generic type argument for call to take_box");

    const fs::path function_inference_signature_mismatch =
        write_source_file(tmp_root() / "function_inference_signature_mismatch.ax",
            "module function_inference_signature_mismatch;\n"
            "fn apply[T](callback: fn(T) -> T, value: T) -> T { return callback(value); }\n"
            "fn zero() -> i32 { return 1; }\n"
            "fn main() -> i32 { return apply(zero, 1); }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(function_inference_signature_mismatch)).output,
        "cannot infer generic type argument for call to apply");

    const fs::path tuple_inference_arity_mismatch = write_source_file(tmp_root() / "tuple_inference_arity_mismatch.ax",
        "module tuple_inference_arity_mismatch;\n"
        "fn take_pair[T](value: (T, bool)) -> T {\n"
        "  let (first, _) = value;\n"
        "  return first;\n"
        "}\n"
        "fn make_triple() -> (i32, bool, i32) { return (1, true, 2); }\n"
        "fn main() -> i32 { return take_pair(make_triple()); }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(tuple_inference_arity_mismatch)).output,
        "cannot infer generic type argument for call to take_pair");

    const fs::path generic_call_arity = write_source_file(tmp_root() / "generic_call_arity.ax",
        "module generic_call_arity;\n"
        "fn id[T](value: T) -> T { return value; }\n"
        "fn main() -> i32 { return id(1, 2); }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_call_arity)).output, "argument count mismatch");

    const fs::path generic_arity = write_source_file(tmp_root() / "generic_arity.ax",
        "module generic_arity;\n"
        "fn id[T](value: T) -> T { return value; }\n"
        "fn main() -> i32 { return id[i32, bool](1); }\n");
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_arity)).output, "too many generic arguments");

    const fs::path generic_prototype = write_source_file(tmp_root() / "generic_prototype.ax",
        "module generic_prototype;\n"
        "fn id[T](value: T) -> T;\n"
        "fn main() -> i32 { return 0; }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_prototype)).output,
        "generic functions with C ABI or prototypes are not supported by M2 semantic analysis");

    const fs::path generic_export = write_source_file(tmp_root() / "generic_export.ax",
        "module generic_export;\n"
        "@name(\"id\")\n"
        "export c fn id[T](value: T) -> T { return value; }\n"
        "fn main() -> i32 { return 0; }\n");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_export)).output,
        "generic functions with C ABI or prototypes are not supported by M2 semantic analysis");

    const fs::path generic_method = write_source_file(tmp_root() / "generic_method.ax",
        "module generic_method;\n"
        "struct Box { value: i32; }\n"
        "impl Box {\n"
        "  fn keep[T](value: T) -> T { return value; }\n"
        "  fn id[T](self: &Box, value: T) -> T { return value; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let box: Box = Box { value: 1 };\n"
        "  let box_ref: &Box = &box;\n"
        "  let flag: bool = box.id[bool](true);\n"
        "  if flag { return box.id(41) + box_ref.id[i32](1) + Box.keep[i32](1); }\n"
        "  return 1;\n"
        "}\n");
    expect_contains_all(require_success(aurexc() + " --emit=checked " + q(generic_method)).output,
        {
            "method generic_method.Box.keep[i32] -> i32",
            "method generic_method.Box.id[bool] -> bool",
            "method generic_method.Box.id[i32] -> i32",
            "receiver_access=shared auto_borrow=false two_phase=false",
        });

    const fs::path generic_method_call = write_source_file(tmp_root() / "generic_method_call.ax",
        "module generic_method_call;\n"
        "struct Box { value: i32; }\n"
        "impl Box {\n"
        "  fn read(self: &Box) -> i32 { return self.value; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let box: Box = Box { value: 1 };\n"
        "  return box.read[i32]();\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_method_call)).output,
        "method generic_method_call.Box.read is not generic");

    const fs::path generic_return_inference = write_source_file(tmp_root() / "generic_return_inference.ax",
        "module generic_return_inference;\n"
        "fn identity[T](value: T) { return value; }\n"
        "fn main() -> i32 { return identity(1); }\n");
    const std::string inferred_return_checked =
        require_success(aurexc() + " --emit=checked " + q(generic_return_inference)).output;
    expect_contains(inferred_return_checked, "identity[i32] -> i32");
}

} // namespace aurex::test
