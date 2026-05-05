#include "support/test_support.hpp"

namespace aurex::test {

TEST_F(AurexIntegrationTest, GenericEnumOption) {
    const fs::path source = positive_sample("generics", "generic_enum_option.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "enum Option<T>",
        "param value : Option<i32>",
        "name `Option`<i32>",
        "match_arm .some(x)",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "enum_cases 2",
        "case Option<i32>_some : generic_enum_option.Option<i32>(i32)",
        "case Option<i32>_none : generic_enum_option.Option<i32>",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "record generic_enum_option.Option<i32>",
        "@m0_generic_enum_option_Option__i32",
        "aggregate {.tag",
        "ptr_cast",
        "field_addr",
    });

    const fs::path bin = test_bin_root() / "generic_enum_option";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");
}

TEST_F(AurexIntegrationTest, GenericEnumResultExpectedType) {
    const fs::path source = positive_sample("generics", "generic_enum_result.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "enum Result<T, E>",
        "Result<i32, bool>",
        "match_arm .ok(x)",
        "match_arm .err(flag)",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "enum_cases 2",
        "case Result<i32, bool>_ok : generic_enum_result.Result<i32, bool>(i32)",
        "case Result<i32, bool>_err : generic_enum_result.Result<i32, bool>(bool)",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "record generic_enum_result.Result<i32, bool>",
        "@m0_generic_enum_result_Result__i32__bool",
        "aggregate {.tag",
        "ptr_cast",
    });

    const fs::path bin = test_bin_root() / "generic_enum_result";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");
}

TEST_F(AurexIntegrationTest, GenericEnumDiagnostics) {
    const fs::path arity = negative_sample("generics", "generic_enum_type_arity.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(arity)).output, "generic enum type requires type arguments: Option");

    const fs::path unknown_type = negative_sample("generics", "generic_enum_unknown_type_arg.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(unknown_type)).output, "unknown type: Missing");

    const fs::path mismatch = negative_sample("generics", "generic_enum_constructor_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(mismatch)).output, "enum payload constructor argument type mismatch");

    const fs::path empty_inference = negative_sample("generics", "generic_enum_empty_inference.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(empty_inference)).output, "generic enum constructor requires explicit type arguments for Option");

    const fs::path partial_inference = negative_sample("generics", "generic_enum_result_partial_inference.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(partial_inference)).output, "generic enum constructor requires explicit type arguments for Result");
}

TEST_F(AurexIntegrationTest, GenericStructPair) {
    const fs::path source = positive_sample("generics", "generic_struct_pair.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "struct Pair<T>",
        "struct_literal Pair<i32>",
        "field left",
        "field right",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "structs 1",
        "struct generic_struct_pair.Pair<i32> fields=2",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "record generic_struct_pair.Pair<i32>",
        "@m0_generic_struct_pair_Pair__i32",
        ".left: i32",
        ".right: i32",
    });

    const fs::path bin = test_bin_root() / "generic_struct_pair";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");
}

TEST_F(AurexIntegrationTest, GenericStructLiteralInference) {
    const fs::path source = positive_sample("generics", "generic_struct_literal_inference.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "struct Pair<T>",
        "struct PairBox<T>",
        "struct_literal Pair",
        "struct_literal PairBox",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "structs 2",
        "struct generic_struct_literal_inference.Pair<i32> fields=2",
        "struct generic_struct_literal_inference.PairBox<i32> fields=1",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "record generic_struct_literal_inference.Pair<i32>",
        "record generic_struct_literal_inference.PairBox<i32>",
        "@m0_generic_struct_literal_inference_Pair__i32",
        "@m0_generic_struct_literal_inference_PairBox__i32",
    });

    const fs::path bin = test_bin_root() / "generic_struct_literal_inference";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");
}

TEST_F(AurexIntegrationTest, GenericFunctionIdentity) {
    const fs::path source = positive_sample("generics", "generic_function_identity.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "fn identity<T>",
        "name `identity`<i32>",
        "name `identity`<T>",
        "struct_literal Pair",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "generic_functions 4",
        "fn generic_function_identity.identity<i32> -> i32",
        "fn generic_function_identity.identity<bool> -> bool",
        "fn generic_function_identity.twice<bool> -> bool",
        "fn generic_function_identity.make_pair<i32> -> generic_function_identity.Pair<i32>",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn generic_function_identity.identity<i32>(value: i32)",
        "fn generic_function_identity.identity<bool>(value: bool)",
        "fn generic_function_identity.twice<bool>(value: bool)",
        "fn generic_function_identity.make_pair<i32>(left: i32, right: i32)",
        "call m0_generic_function_identity_identity__i32",
        "call m0_generic_function_identity_identity__bool",
    });

    const fs::path bin = test_bin_root() / "generic_function_identity";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");
}

TEST_F(AurexIntegrationTest, GenericStructArrayFieldAndSmallPayloadEnum) {
    const fs::path struct_source = positive_sample("generics", "generic_struct_array_field.ax");

    const std::string struct_checked = require_success(aurexc() + " --emit=checked " + q(struct_source)).output;
    expect_contains_all(struct_checked, {
        "structs 1",
        "struct generic_struct_array_field.Holder<[2]i32> fields=1",
    });

    const std::string struct_ir = require_success(aurexc() + " --emit=ir " + q(struct_source)).output;
    expect_contains_all(struct_ir, {
        "record generic_struct_array_field.Holder<[2]i32>",
        ".value: [2]i32",
        "fn touch(value: *mut generic_struct_array_field.Holder<[2]i32>)",
    });

    const fs::path struct_bin = test_bin_root() / "generic_struct_array_field";
    require_success(aurexc() + " " + q(struct_source) + " -o " + q(struct_bin));
    EXPECT_EQ(require_success(q(struct_bin)).output, "");

    const fs::path enum_source = positive_sample("generics", "generic_enum_small_payload.ax");

    const std::string enum_checked = require_success(aurexc() + " --emit=checked " + q(enum_source)).output;
    expect_contains_all(enum_checked, {
        "enum_cases 2",
        "case SmallSlot<u16>_full : generic_enum_small_payload.SmallSlot<u16>(u16)",
    });

    const std::string enum_ir = require_success(aurexc() + " --emit=ir " + q(enum_source)).output;
    expect_contains_all(enum_ir, {
        "record generic_enum_small_payload.SmallSlot<u16>",
        ".payload: u16",
        "ptr_cast",
    });

    const fs::path enum_bin = test_bin_root() / "generic_enum_small_payload";
    require_success(aurexc() + " " + q(enum_source) + " -o " + q(enum_bin));
    EXPECT_EQ(require_success(q(enum_bin)).output, "");
}

TEST_F(AurexIntegrationTest, GenericStructDiagnostics) {
    const fs::path arity = negative_sample("generics", "generic_struct_type_arity.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(arity)).output, "generic struct type requires type arguments: Pair");

    const fs::path unknown_type = negative_sample("generics", "generic_struct_unknown_type_arg.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(unknown_type)).output, "unknown type: Missing");

    const fs::path missing_args = negative_sample("generics", "generic_struct_literal_missing_args.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(missing_args)).output, "generic struct literal requires explicit type arguments: Tagged");

    const fs::path mismatch = negative_sample("generics", "generic_struct_field_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(mismatch)).output, "struct literal field type mismatch");
}

TEST_F(AurexIntegrationTest, GenericFunctionDiagnostics) {
    const fs::path arity = negative_sample("generics", "generic_function_type_arity.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(arity)).output, "generic function type argument count mismatch for identity");

    const fs::path inference = negative_sample("generics", "generic_function_inference_failure.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(inference)).output, "generic function requires explicit type arguments: zero");

    const fs::path duplicate = negative_sample("generics", "generic_function_duplicate_params.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(duplicate)).output, "duplicate generic parameter in function bad: T");

    const fs::path missing_return = negative_sample("generics", "generic_function_missing_return_type.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(missing_return)).output, "generic function return type must be explicit: identity");
}

TEST_F(AurexIntegrationTest, GenericImportVisibilityAndAmbiguityDiagnostics) {
    const std::string import_flags = sample_import_flags();

    const fs::path ambiguous_struct = negative_sample("generics", "ambiguous_generic_struct.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(ambiguous_struct)).output,
        "ambiguous generic struct 'Box'"
    );

    const fs::path ambiguous_enum = negative_sample("generics", "ambiguous_generic_enum.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(ambiguous_enum)).output,
        "ambiguous generic enum 'Choice'"
    );

    const fs::path private_struct = negative_sample("generics", "private_generic_struct_import.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(private_struct)).output,
        "unknown generic struct: Hidden"
    );

    const fs::path private_enum = negative_sample("generics", "private_generic_enum_import.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(private_enum)).output,
        "unknown generic enum: Secret"
    );
}

} // namespace aurex::test
