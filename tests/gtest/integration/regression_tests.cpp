#include <support/test_support.hpp>

#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

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

}

TEST_F(AurexIntegrationTest, IntegerLiteralRegressions) {
    const fs::path overflow = write_source_file(
        tmp_root() / "overflow.ax",
        "module overflow;\n"
        "fn main() -> i32 {\n"
        "  let value: u8 = 300;\n"
        "  return cast[i32](value);\n"
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

TEST_F(AurexIntegrationTest, M2UnsafeBoundaries) {
    const fs::path positive = positive_sample("pointers", "unsafe_minimal.ax");
    const std::string ast = require_success(aurexc() + " --emit=ast " + q(positive)).output;
    expect_contains_all(ast, {
        "fn from_raw unsafe",
        "unsafe_block",
        "alias unsafe fn(*const i32) -> i32",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(positive)).output;
    expect_contains_all(checked, {
        "fn priv from_raw -> str unsafe",
        "type priv UnsafeRead = unsafe fn(*const i32) -> i32",
    });

    const fs::path binary = test_bin_root() / "unsafe_minimal";
    require_success(aurexc() + " " + q(positive) + " -o " + q(binary));
    require_success(q(binary));

    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("pointers", "unsafe_deref_required.ax"))).output,
        "raw pointer dereference requires unsafe context"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("pointers", "unsafe_ptrcast_required.ax"))).output,
        "ptrcast requires unsafe context"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("pointers", "unsafe_bitcast_required.ax"))).output,
        "bitcast requires unsafe context"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("pointers", "unsafe_ptrat_required.ax"))).output,
        "ptrat requires unsafe context"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("pointers", "unsafe_strraw_required.ax"))).output,
        "strraw requires unsafe context"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "unsafe_fn_call_required.ax"))).output,
        "call to unsafe function read_raw requires unsafe context"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "unsafe_fn_pointer_call_required.ax"))).output,
        "call to unsafe function"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("expressions", "unsafe_block_const_initializer.ax"))).output,
        "unsafe block cannot be used in const initializer"
    );
}

TEST_F(AurexIntegrationTest, StringCheckedBoundary) {
    const fs::path positive = positive_sample("types", "str_checked.ax");
    const std::string ir = require_success(aurexc() + " --emit=ir " + q(positive)).output;
    expect_contains_all(ir, {
        "strvalid",
        "strfromutf8",
        "str",
    });

    const std::string llvm_ir = require_success(aurexc() + " --emit=llvm-ir " + q(positive)).output;
    expect_contains_all(llvm_ir, {
        "__aurex_utf8_validate",
        "str.checked",
        "select i1",
    });

    const fs::path binary = test_bin_root() / "str_checked";
    require_success(aurexc() + " " + q(positive) + " -o " + q(binary));
    require_success(q(binary));

    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "strvalid_non_slice.ax"))).output,
        "str UTF-8 builtin requires a []const u8 or []mut u8 byte slice"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "strfromutf8_non_slice.ax"))).output,
        "str UTF-8 builtin requires a []const u8 or []mut u8 byte slice"
    );
}

TEST_F(AurexIntegrationTest, ArrayLiteralRegressions) {
    const fs::path array_literal = positive_sample("types", "array_literal.ax");
    const std::string ir = require_success(aurexc() + " --emit=ir " + q(array_literal)).output;
    expect_contains_all(ir, {
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
        "empty array literal requires an array type context"
    );
    expect_contains(
        require_failure(
            aurexc() + " --check " + q(negative_sample("types", "array_literal_expected_non_array.ax"))
        ).output,
        "array literal requires an array expected type"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_literal_element_type_mismatch.ax"))).output,
        "array literal element type mismatch"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_literal_length_mismatch.ax"))).output,
        "array literal length mismatch"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_repeat_count_mismatch.ax"))).output,
        "array literal length mismatch"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_repeat_count_not_literal.ax"))).output,
        "array repeat count must be an integer literal"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "array_repeat_count_overflow.ax"))).output,
        "array repeat count literal is out of range"
    );
    expect_contains(
        require_failure(
            aurexc() + " --check " + q(negative_sample("types", "array_repeat_value_type_mismatch.ax"))
        ).output,
        "array repeat value type mismatch"
    );
}

TEST_F(AurexIntegrationTest, SliceRegressions) {
    const fs::path slice_basic = positive_sample("types", "slice_basic.ax");
    const std::string basic_ir = require_success(aurexc() + " --emit=ir " + q(slice_basic)).output;
    expect_contains_all(basic_ir, {
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
    expect_contains_all(mut_ir, {
        "[]mut i32",
        "[]const i32",
        "slice_data",
    });

    const fs::path mut_binary = test_bin_root() / "slice_mut";
    require_success(aurexc() + " " + q(slice_mut) + " -o " + q(mut_binary));
    require_success(q(mut_binary));

    const fs::path slice_generic = positive_sample("types", "slice_generic.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(slice_generic)).output;
    expect_contains_all(checked, {
        "first[i32] -> i32",
    });
    const std::string generic_ir = require_success(aurexc() + " --emit=ir " + q(slice_generic)).output;
    expect_contains_all(generic_ir, {
        "fn first[i32](values: []const i32)",
        "[]mut i32",
    });

    const fs::path generic_binary = test_bin_root() / "slice_generic";
    require_success(aurexc() + " " + q(slice_generic) + " -o " + q(generic_binary));
    require_success(q(generic_binary));

    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "slice_non_slice.ax"))).output,
        "slicing requires array or slice value"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "slice_bound_non_integer.ax"))).output,
        "slice bound must be an integer"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "slice_const_write.ax"))).output,
        "left side of assignment must be writable"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "slice_mut_from_const.ax"))).output,
        "initializer type does not match declared type"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "slice_invalid_object.ax"))).output,
        "unknown name: missing"
    );
    const std::string invalid_element_output =
        require_failure(aurexc() + " --check " + q(negative_sample("types", "slice_invalid_element.ax"))).output;
    expect_contains(invalid_element_output, "function parameter type is not valid storage");
    expect_contains(invalid_element_output, "slice element type is not valid storage");
}

TEST_F(AurexIntegrationTest, TupleRegressions) {
    const fs::path tuple_basic = positive_sample("types", "tuple_basic.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(tuple_basic)).output;
    expect_contains_all(checked, {
        "fn priv make_pair -> (i32, bool)",
        "fn priv first[i32,bool] -> i32",
        "fn priv second[i32,bool] -> bool",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(tuple_basic)).output;
    expect_contains_all(ir, {
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

    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "empty_tuple_type.ax"))).output,
        "empty tuple type is not part of M2 syntax"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "empty_tuple_literal.ax"))).output,
        "empty tuple literal is not part of M2 syntax"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "empty_tuple_pattern.ax"))).output,
        "empty tuple pattern is not part of M2 syntax"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_field_numeric_access.ax"))).output,
        "tuple fields are not directly accessible; destructure the tuple or use a named struct"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_field_spaced_numeric_access.ax"))).output,
        "tuple fields are not directly accessible; destructure the tuple or use a named struct"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_field_named_access.ax"))).output,
        "tuple fields are not directly accessible; destructure the tuple or use a named struct"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_literal_arity_mismatch.ax"))).output,
        "tuple literal arity does not match expected tuple type"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_literal_expected_non_tuple.ax"))).output,
        "tuple literal requires a tuple expected type"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_destructure_arity_mismatch.ax"))).output,
        "tuple destructuring pattern arity does not match tuple type"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_destructure_non_tuple.ax"))).output,
        "tuple destructuring requires a tuple value"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_invalid_storage.ax"))).output,
        "tuple literal type is not valid storage"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "tuple_literal_infer_invalid.ax"))).output,
        "local variable type cannot be inferred"
    );
}

TEST_F(AurexIntegrationTest, EnumConstructorMatchArmRegressions) {
    const fs::path source = write_source_file(
        tmp_root() / "enum_match_arm.ax",
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
        "fn main() -> i32 { return 0; }\n"
    );
    require_success(aurexc() + " --check " + q(source));
}

TEST_F(AurexIntegrationTest, EnumAdtRegressions) {
    const fs::path no_payload = positive_sample("types", "enum_adt_no_payload.ax");
    const fs::path no_payload_binary = test_bin_root() / "enum_adt_no_payload";
    require_success(aurexc() + " " + q(no_payload) + " -o " + q(no_payload_binary));
    require_success(q(no_payload_binary));

    const fs::path payload = positive_sample("pattern_matching", "enum_adt_payload.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(payload)).output;
    expect_contains_all(checked, {
        "struct priv enum_adt_payload.Token_span.payload fields=2",
        "case OptionI32_some : enum_adt_payload.OptionI32(i32)",
        "case Token_span : enum_adt_payload.Token(usize,usize)",
    });

    const fs::path payload_binary = test_bin_root() / "enum_adt_payload";
    require_success(aurexc() + " " + q(payload) + " -o " + q(payload_binary));
    require_success(q(payload_binary));

    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "enum_adt_duplicate_auto_discriminant.ax"))).output,
        "duplicate enum discriminant value"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "enum_adt_multi_payload_arity.ax"))).output,
        "enum payload constructor requires 2 arguments"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "enum_case_empty_payload.ax"))).output,
        "expected enum case payload type"
    );
}

TEST_F(AurexIntegrationTest, QualifiedStaticMethodRegressions) {
    static_cast<void>(write_source_file(
        tmp_root() / "box.ax",
        "module box;\n"
        "pub struct BoxI32 { pub value: i32; }\n"
        "impl BoxI32 {\n"
        "  pub fn new(value: i32) -> BoxI32 { return BoxI32 { value: value }; }\n"
        "}\n"
    ));
    const fs::path source = write_source_file(
        tmp_root() / "qualified_static_method.ax",
        "module qualified_static_method;\n"
        "import box as box;\n"
        "fn main() -> i32 {\n"
        "  let value: box::BoxI32 = box::BoxI32.new(7);\n"
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

TEST_F(AurexIntegrationTest, SymlinkedImportStillValidatesExpectedModuleName) {
    const fs::path import_dir = tmp_root() / "symlink_imports";
    fs::create_directories(import_dir);
    static_cast<void>(write_source_file(
        import_dir / "a.ax",
        "module a;\n"
        "pub fn f() -> i32 { return 1; }\n"
    ));

    std::error_code error;
    fs::remove(import_dir / "b.ax", error);
    error.clear();
    fs::create_symlink("a.ax", import_dir / "b.ax", error);
    if (error) {
        GTEST_SKIP() << "filesystem symlink creation failed: " << error.message();
    }

    const fs::path source = write_source_file(
        tmp_root() / "symlink_import_mismatch.ax",
        "module symlink_import_mismatch;\n"
        "import a as alpha;\n"
        "import b as beta;\n"
        "fn main() -> i32 { return alpha::f() + beta::f(); }\n"
    );

    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(source)).output,
        "module declaration 'a' does not match import 'b'"
    );
}

TEST_F(AurexIntegrationTest, M2GenericRegressions) {
    const fs::path source = positive_sample("generics", "basic_m2.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
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
    expect_contains_all(ir, {
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
        "duplicate generic parameter"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "type_arity_mismatch.ax"))).output,
        "too few generic type arguments"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "type_arity_too_many.ax"))).output,
        "too many generic type arguments"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "non_generic_type_args.ax"))).output,
        "type Plain is not generic"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "missing_type_args.ax"))).output,
        "generic type Box requires type arguments"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "inference_failure.ax"))).output,
        "cannot infer generic type argument"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "operator_unconstrained.ax"))).output,
        "generic type parameter `T` has no known operator"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "non_generic_fn_type_args.ax"))).output,
        "function plain is not generic"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_fn_type_args_too_many.ax"))).output,
        "too many generic function type arguments"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "empty_generic_params_fn.ax"))).output,
        "expected generic type parameter"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "empty_generic_params_struct.ax"))).output,
        "expected generic type parameter"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "empty_type_args.ax"))).output,
        "expected generic type argument"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "empty_explicit_generic_call.ax"))).output,
        "expected generic type argument"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_body_return_unknown.ax"))).output,
        "return type mismatch"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "duplicate_generic_param_fn.ax"))).output,
        "duplicate generic parameter"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "expression_index_not_generic_call.ax"))).output,
        "callee must be a function value"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_bound_rejected_m2.ax"))).output,
        "generic bounds are not part of M2 syntax"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "duplicate_with_plain_struct.ax"))).output,
        "duplicate type definition"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "duplicate_generic_struct.ax"))).output,
        "duplicate type definition"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "duplicate_with_plain_fn.ax"))).output,
        "duplicate function definition"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "duplicate_generic_fn.ax"))).output,
        "duplicate function definition"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_enum_unsupported.ax"))).output,
        "generic enums are not supported by M2 semantic analysis"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "generic_type_alias_unsupported.ax"))).output,
        "generic type aliases are not supported by M2 semantic analysis"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "public_generic_inferred_return.ax"))).output,
        "public function return type must be explicit"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("generics", "qualified_explicit_generic_unknown_import.ax"))).output,
        "unknown import alias: missing"
    );
}

TEST_F(AurexIntegrationTest, M2NestedGenericInstantiationRegressions) {
    const fs::path source = write_source_file(
        tmp_root() / "nested_m2_generics.ax",
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
        "  return unwrap_holder(holder) - id::[i32](7);\n"
        "}\n"
    );

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "id[i32] -> i32",
        "make_holder[i32]",
        "unwrap_holder[i32]",
        "Holder[i32]",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn id[i32](value: i32)",
        "fn make_holder[i32](value: i32)",
        "fn unwrap_holder[i32]",
    });

    const fs::path binary = test_bin_root() / "nested_m2_generics";
    require_success(aurexc() + " " + q(source) + " -o " + q(binary));
    require_success(q(binary));
}

TEST_F(AurexIntegrationTest, M2GenericEdgeCasesAndImports) {
    const fs::path import_dir = tmp_root() / "generic_imports";
    fs::create_directories(import_dir);
    static_cast<void>(write_source_file(
        import_dir / "generic_a.ax",
        "module generic_a;\n"
        "pub struct RemoteBox[T] { pub value: T; }\n"
        "priv struct SecretBox[T] { pub value: T; }\n"
        "pub fn remote_id[T](value: T) -> T { return value; }\n"
        "priv fn secret_id[T](value: T) -> T { return value; }\n"
        "pub fn make_box[T](value: T) -> RemoteBox[T] {\n"
        "  return RemoteBox[T] { value: value };\n"
        "}\n"
    ));
    static_cast<void>(write_source_file(
        import_dir / "generic_b.ax",
        "module generic_b;\n"
        "pub struct RemoteBox[T] { pub value: T; }\n"
        "pub fn remote_id[T](value: T) -> T { return value; }\n"
    ));

    const fs::path use_imported = write_source_file(
        tmp_root() / "use_imported_generics.ax",
        "module use_imported_generics;\n"
        "import generic_a as ga;\n"
        "fn main() -> i32 {\n"
        "  let box: ga::RemoteBox[i32] = ga::make_box::[i32](ga::remote_id(5));\n"
        "  let ptr: *const i32 = &box.value;\n"
        "  return ga::remote_id(unsafe { *ptr }) - 5;\n"
        "}\n"
    );
    require_success(aurexc() + " -I " + q(import_dir) + " --check " + q(use_imported));

    const fs::path imported_checked_source = write_source_file(
        tmp_root() / "imported_checked_generics.ax",
        "module imported_checked_generics;\n"
        "import generic_a;\n"
        "fn main() -> i32 {\n"
        "  let box: RemoteBox[i32] = make_box(9);\n"
        "  return remote_id(box.value) - 9;\n"
        "}\n"
    );
    const std::string checked = require_success(aurexc() + " -I " + q(import_dir) + " --emit=checked " + q(imported_checked_source)).output;
    expect_contains_all(checked, {
        "RemoteBox[i32]",
        "remote_id[i32]",
        "make_box[i32]",
    });

    const fs::path private_generic_type = write_source_file(
        tmp_root() / "private_generic_type.ax",
        "module private_generic_type;\n"
        "import generic_a as ga;\n"
        "fn main() -> i32 {\n"
        "  let box: ga::SecretBox[i32] = ga::SecretBox[i32] { value: 1 };\n"
        "  return box.value;\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_generic_type)).output,
        "generic type is private: generic_a.SecretBox"
    );

    const fs::path private_generic_fn = write_source_file(
        tmp_root() / "private_generic_fn.ax",
        "module private_generic_fn;\n"
        "import generic_a as ga;\n"
        "fn main() -> i32 { return ga::secret_id::[i32](1); }\n"
    );
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_generic_fn)).output,
        "generic function is private: generic_a.secret_id"
    );

    const fs::path ambiguous_generic = write_source_file(
        tmp_root() / "ambiguous_generic.ax",
        "module ambiguous_generic;\n"
        "import generic_a;\n"
        "import generic_b;\n"
        "fn main() -> i32 {\n"
        "  let box: RemoteBox[i32] = RemoteBox[i32] { value: 1 };\n"
        "  return remote_id(box.value);\n"
        "}\n"
    );
    const std::string ambiguous_output = require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(ambiguous_generic)).output;
    expect_contains(ambiguous_output, "ambiguous generic type name");
    expect_contains(ambiguous_output, "ambiguous generic function name");

    const fs::path unqualified_private_generics = write_source_file(
        tmp_root() / "unqualified_private_generics.ax",
        "module unqualified_private_generics;\n"
        "import generic_a;\n"
        "fn main() -> i32 {\n"
        "  let box: SecretBox[i32] = SecretBox[i32] { value: 1 };\n"
        "  return box.value;\n"
        "}\n"
    );
    const std::string unqualified_private_output =
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(unqualified_private_generics)).output;
    expect_contains(unqualified_private_output, "unknown generic type: SecretBox");

    const fs::path unqualified_private_generic_fn = write_source_file(
        tmp_root() / "unqualified_private_generic_fn.ax",
        "module unqualified_private_generic_fn;\n"
        "import generic_a;\n"
        "fn main() -> i32 { return secret_id::[i32](1); }\n"
    );
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(unqualified_private_generic_fn)).output,
        "unknown generic function: secret_id"
    );

    const fs::path generic_inference_edges = write_source_file(
        tmp_root() / "generic_inference_edges.ax",
        "module generic_inference_edges;\n"
        "struct Box[T] { value: T; }\n"
        "struct ArrayBox[T] { data: [2]T; }\n"
        "fn pick[T](lhs: T, rhs: T) -> T { return lhs; }\n"
        "fn ptr_id[T](value: *const T) -> *const T { return value; }\n"
        "fn array_ptr_id[T](value: *mut [2]T) -> *mut [2]T { return value; }\n"
        "fn main() -> i32 {\n"
        "  let b: Box[i32] = Box[i32] { value: 1 };\n"
        "  let chosen: Box[i32] = pick(b, Box[i32] { value: 2 });\n"
        "  let ptr: *const i32 = &chosen.value;\n"
        "  let same_ptr = ptr_id(ptr);\n"
        "  let values: *mut [2]i32 = null;\n"
        "  let same_values = array_ptr_id(values);\n"
        "  let size: usize = sizeof[ArrayBox[i32]];\n"
        "  return unsafe { *same_ptr } + unsafe { (*same_values)[0] } + cast[i32](size) - 9;\n"
        "}\n"
    );
    require_success(aurexc() + " --check " + q(generic_inference_edges));

    const fs::path generic_placeholder_edges = write_source_file(
        tmp_root() / "generic_placeholder_edges.ax",
        "module generic_placeholder_edges;\n"
        "struct Box[T] { value: T; }\n"
        "fn id[T](value: T) -> T { return value; }\n"
        "fn ptr_box[T](value: *const T) -> Box[*const T] {\n"
        "  return Box[*const T] { value: id::[*const T](value) };\n"
        "}\n"
        "fn array_ptr_box[T](value: *mut [2]T) -> Box[*mut [2]T] {\n"
        "  return Box[*mut [2]T] { value: id::[*mut [2]T](value) };\n"
        "}\n"
        "fn wrap_ptr[U](value: *const U) -> Box[*const U] {\n"
        "  return ptr_box::[U](value);\n"
        "}\n"
        "fn wrap_array[V](value: *mut [2]V) -> Box[*mut [2]V] {\n"
        "  return array_ptr_box::[V](value);\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let ptr: *const i32 = null;\n"
        "  let boxed = wrap_ptr(ptr);\n"
        "  let values: *mut [2]i32 = null;\n"
        "  let array_boxed = wrap_array(values);\n"
        "  let size: usize = sizeof[Box[*const i32]] + sizeof[Box[*mut [2]i32]];\n"
        "  return cast[i32](size) - cast[i32](sizeof[Box[*const i32]] + sizeof[Box[*mut [2]i32]]);\n"
        "}\n"
    );
    require_success(aurexc() + " --check " + q(generic_placeholder_edges));

    const fs::path missing_visible_generic_type = write_source_file(
        tmp_root() / "missing_visible_generic_type.ax",
        "module missing_visible_generic_type;\n"
        "fn main() -> i32 {\n"
        "  let value: Missing[i32] = Missing[i32] { value: 1 };\n"
        "  return 0;\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(missing_visible_generic_type)).output,
        "unknown generic type: Missing"
    );

    const fs::path missing_qualified_generic_type = write_source_file(
        tmp_root() / "missing_qualified_generic_type.ax",
        "module missing_qualified_generic_type;\n"
        "import generic_a as ga;\n"
        "fn main() -> i32 {\n"
        "  let value: ga::Missing[i32] = ga::Missing[i32] { value: 1 };\n"
        "  return 0;\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(missing_qualified_generic_type)).output,
        "unknown generic type in module generic_a: Missing"
    );

    const fs::path missing_visible_generic_fn = write_source_file(
        tmp_root() / "missing_visible_generic_fn.ax",
        "module missing_visible_generic_fn;\n"
        "fn main() -> i32 { return missing::[i32](1); }\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(missing_visible_generic_fn)).output,
        "unknown generic function: missing"
    );

    const fs::path invalid_generic_type_arg = write_source_file(
        tmp_root() / "invalid_generic_type_arg.ax",
        "module invalid_generic_type_arg;\n"
        "struct Box[T] { value: T; }\n"
        "fn id[T](value: T) -> T { return value; }\n"
        "fn main() -> i32 {\n"
        "  let value: Box[Missing] = Box[Missing] { value: 1 };\n"
        "  return id::[Missing](1);\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(invalid_generic_type_arg)).output,
        "unknown type: Missing"
    );

    const fs::path missing_qualified_generic_fn = write_source_file(
        tmp_root() / "missing_qualified_generic_fn.ax",
        "module missing_qualified_generic_fn;\n"
        "import generic_a as ga;\n"
        "fn main() -> i32 { return ga::missing::[i32](1); }\n"
    );
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(missing_qualified_generic_fn)).output,
        "unknown generic function in module generic_a: missing"
    );

    const fs::path generic_struct_duplicate_field = write_source_file(
        tmp_root() / "generic_struct_duplicate_field.ax",
        "module generic_struct_duplicate_field;\n"
        "struct Bad[T] { value: T; value: T; }\n"
        "fn main() -> i32 {\n"
        "  let size: usize = sizeof[Bad[i32]];\n"
        "  return cast[i32](size);\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_struct_duplicate_field)).output,
        "duplicate struct field: value"
    );

    const fs::path generic_struct_invalid_field = write_source_file(
        tmp_root() / "generic_struct_invalid_field.ax",
        "module generic_struct_invalid_field;\n"
        "struct Bad[T] { value: void; }\n"
        "fn main() -> i32 {\n"
        "  let size: usize = sizeof[Bad[i32]];\n"
        "  return cast[i32](size);\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_struct_invalid_field)).output,
        "field type is not valid storage"
    );

    const fs::path mismatched_inference = write_source_file(
        tmp_root() / "mismatched_inference.ax",
        "module mismatched_inference;\n"
        "fn pick[T](lhs: T, rhs: T) -> T { return lhs; }\n"
        "fn main() -> i32 { return pick(1, true); }\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(mismatched_inference)).output,
        "cannot infer generic type argument for call to pick"
    );

    const fs::path pointer_inference_mutability_mismatch = write_source_file(
        tmp_root() / "pointer_inference_mutability_mismatch.ax",
        "module pointer_inference_mutability_mismatch;\n"
        "fn ptr_id[T](value: *mut T) -> *mut T { return value; }\n"
        "fn main() -> i32 {\n"
        "  let ptr: *const i32 = null;\n"
        "  let same = ptr_id(ptr);\n"
        "  return 0;\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(pointer_inference_mutability_mismatch)).output,
        "cannot infer generic type argument for call to ptr_id"
    );

    const fs::path array_inference_count_mismatch = write_source_file(
        tmp_root() / "array_inference_count_mismatch.ax",
        "module array_inference_count_mismatch;\n"
        "fn array_ptr_id[T](value: *mut [2]T) -> *mut [2]T { return value; }\n"
        "fn main() -> i32 {\n"
        "  let values: *mut [3]i32 = null;\n"
        "  let same = array_ptr_id(values);\n"
        "  return 0;\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(array_inference_count_mismatch)).output,
        "cannot infer generic type argument for call to array_ptr_id"
    );

    const fs::path builtin_inference_mismatch = write_source_file(
        tmp_root() / "builtin_inference_mismatch.ax",
        "module builtin_inference_mismatch;\n"
        "fn require_i32[T](value: i32, payload: T) -> T { return payload; }\n"
        "fn main() -> i32 { return require_i32(true, 1); }\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(builtin_inference_mismatch)).output,
        "cannot infer generic type argument for call to require_i32"
    );

    const fs::path struct_origin_inference_mismatch = write_source_file(
        tmp_root() / "struct_origin_inference_mismatch.ax",
        "module struct_origin_inference_mismatch;\n"
        "struct Box[T] { value: T; }\n"
        "struct Wrap[T] { value: T; }\n"
        "fn take_box[T](value: Box[T]) -> T { return value.value; }\n"
        "fn main() -> i32 { return take_box(Wrap[i32] { value: 1 }); }\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(struct_origin_inference_mismatch)).output,
        "cannot infer generic type argument for call to take_box"
    );

    const fs::path generic_arity = write_source_file(
        tmp_root() / "generic_arity.ax",
        "module generic_arity;\n"
        "fn id[T](value: T) -> T { return value; }\n"
        "fn main() -> i32 { return id::[i32, bool](1); }\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_arity)).output,
        "too many generic function type arguments"
    );

    const fs::path generic_prototype = write_source_file(
        tmp_root() / "generic_prototype.ax",
        "module generic_prototype;\n"
        "fn id[T](value: T) -> T;\n"
        "fn main() -> i32 { return 0; }\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_prototype)).output,
        "generic functions with C ABI or prototypes are not supported by M2 semantic analysis"
    );

    const fs::path generic_export = write_source_file(
        tmp_root() / "generic_export.ax",
        "module generic_export;\n"
        "export c fn id[T](value: T) -> T @name(\"id\") { return value; }\n"
        "fn main() -> i32 { return 0; }\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_export)).output,
        "generic functions with C ABI or prototypes are not supported by M2 semantic analysis"
    );

    const fs::path generic_method = write_source_file(
        tmp_root() / "generic_method.ax",
        "module generic_method;\n"
        "struct Box { value: i32; }\n"
        "impl Box {\n"
        "  fn id[T](self: *const Box, value: T) -> T { return value; }\n"
        "}\n"
        "fn main() -> i32 { return 0; }\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_method)).output,
        "generic methods are not supported by M2 semantic analysis"
    );

    const fs::path generic_method_call = write_source_file(
        tmp_root() / "generic_method_call.ax",
        "module generic_method_call;\n"
        "struct Box { value: i32; }\n"
        "impl Box {\n"
        "  fn read(self: *const Box) -> i32 { return self.value; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let box: Box = Box { value: 1 };\n"
        "  return box.read::[i32]();\n"
        "}\n"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_method_call)).output,
        "explicit generic calls use '::[...]', for example id::[i32](...)"
    );

    const fs::path generic_return_inference = write_source_file(
        tmp_root() / "generic_return_inference.ax",
        "module generic_return_inference;\n"
        "fn identity[T](value: T) { return value; }\n"
        "fn main() -> i32 { return identity(1); }\n"
    );
    const std::string inferred_return_checked = require_success(aurexc() + " --emit=checked " + q(generic_return_inference)).output;
    expect_contains(inferred_return_checked, "identity[i32] -> i32");
}

} // namespace aurex::test
