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
        "fn first<T, U>",
        "enum Option<T>",
        "name `identity`<i32>",
        "name `identity`<T>",
        "name `first`<i32, bool>",
        "struct_literal Pair",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "generic_functions 8",
        "fn generic_function_identity.identity<i32> -> i32",
        "fn generic_function_identity.identity<bool> -> bool",
        "fn generic_function_identity.twice<bool> -> bool",
        "fn generic_function_identity.make_pair<i32> -> generic_function_identity.Pair<i32>",
        "fn generic_function_identity.ptr_identity<i32> -> i32",
        "fn generic_function_identity.array_ptr<i32> -> *mut [2]i32",
        "fn generic_function_identity.unwrap_or<i32> -> i32",
        "fn generic_function_identity.first<i32, bool> -> i32",
        "case Option<i32>_some : generic_function_identity.Option<i32>(i32)",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn generic_function_identity.identity<i32>(value: i32)",
        "fn generic_function_identity.identity<bool>(value: bool)",
        "fn generic_function_identity.twice<bool>(value: bool)",
        "fn generic_function_identity.make_pair<i32>(left: i32, right: i32)",
        "fn generic_function_identity.ptr_identity<i32>(value: *mut i32)",
        "fn generic_function_identity.array_ptr<i32>()",
        "fn generic_function_identity.unwrap_or<i32>(value: generic_function_identity.Option<i32>, fallback: i32)",
        "fn generic_function_identity.first<i32, bool>(left: i32, right: bool)",
        "call m0_generic_function_identity_identity__i32",
        "call m0_generic_function_identity_identity__bool",
        "call m0_generic_function_identity_ptr_identity__i32",
        "call m0_generic_function_identity_array_ptr__i32",
        "call m0_generic_function_identity_unwrap_or__i32",
        "call m0_generic_function_identity_first__i32__bool",
    });

    const fs::path bin = test_bin_root() / "generic_function_identity";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");
}

TEST_F(AurexIntegrationTest, GenericFunctionImport) {
    const std::string import_flags = sample_import_flags();
    const fs::path source = positive_sample("generics", "generic_function_import.ax");

    const std::string checked = require_success(aurexc() + " " + import_flags + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "generic_functions 1",
        "fn samplelib.generic_a.pick<i32> -> i32",
    });

    const std::string ir = require_success(aurexc() + " " + import_flags + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn samplelib.generic_a.pick<i32>(value: i32)",
        "call m0_samplelib_generic_a_pick__i32",
    });

    const fs::path bin = test_bin_root() / "generic_function_import";
    require_success(aurexc() + " " + import_flags + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");
}

TEST_F(AurexIntegrationTest, GenericImplMethods) {
    const fs::path source = positive_sample("generics", "generic_impl_methods.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "impl<T> for Box<T>",
        "fn make<T> for Box<T>",
        "fn read<T> for Box<T>",
        "fn replace<T> for Box<T>",
        "fn pair_with<T, U> for Box<T>",
        "fn passthrough<T, U> for Box<T>",
        "fn make_pair<T, U> for Box<T>",
        "fn wrap<T> for Choice<T>",
        "fn identity<U> for Plain",
        "name `Box`<i32>",
        "field .make",
        "name `Choice`<i32>",
        "field .wrap",
        "field .read",
        "field .replace",
        "field .pair_with",
        "field .passthrough<u64>",
        "field .make_pair<u8>",
        "field .identity",
        "field .identity<u16>",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "generic_functions 9",
        "fn method generic_impl_methods.Box<i32>.make -> generic_impl_methods.Box<i32>",
        "fn method generic_impl_methods.Box<i32>.read -> i32",
        "fn method generic_impl_methods.Box<i32>.replace -> i32",
        "fn method generic_impl_methods.Box<i32>.pair_with<bool> -> generic_impl_methods.Pair<i32, bool>",
        "fn method generic_impl_methods.Box<i32>.passthrough<u64> -> u64",
        "fn method generic_impl_methods.Box<i32>.make_pair<u8> -> generic_impl_methods.Pair<i32, u8>",
        "fn method generic_impl_methods.Choice<i32>.wrap -> generic_impl_methods.Choice<i32>",
        "fn method generic_impl_methods.Plain.identity<i32> -> i32",
        "fn method generic_impl_methods.Plain.identity<u16> -> u16",
        "struct generic_impl_methods.Box<i32> fields=1",
        "struct Plain fields=1",
        "struct generic_impl_methods.Pair<i32, bool> fields=2",
        "struct generic_impl_methods.Pair<i32, u8> fields=2",
        "case Choice<i32>_some : generic_impl_methods.Choice<i32>(i32)",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn generic_impl_methods.Box<i32>.make(value: i32)",
        "fn generic_impl_methods.Box<i32>.read(self: *const generic_impl_methods.Box<i32>)",
        "fn generic_impl_methods.Box<i32>.replace(self: *mut generic_impl_methods.Box<i32>, value: i32)",
        "fn generic_impl_methods.Box<i32>.pair_with<bool>(self: *const generic_impl_methods.Box<i32>, right: bool)",
        "fn generic_impl_methods.Box<i32>.passthrough<u64>(self: *const generic_impl_methods.Box<i32>, value: u64)",
        "fn generic_impl_methods.Box<i32>.make_pair<u8>(left: i32, right: u8)",
        "fn generic_impl_methods.Choice<i32>.wrap(value: i32)",
        "fn generic_impl_methods.Plain.identity<i32>(value: i32)",
        "fn generic_impl_methods.Plain.identity<u16>(value: u16)",
        "call m0_generic_impl_methods_Box__i32_make",
        "call m0_generic_impl_methods_Box__i32_read",
        "call m0_generic_impl_methods_Box__i32_replace",
        "call m0_generic_impl_methods_Box__i32_pair_with__bool",
        "call m0_generic_impl_methods_Box__i32_passthrough__u64",
        "call m0_generic_impl_methods_Box__i32_make_pair__u8",
        "call m0_generic_impl_methods_Choice__i32_wrap",
        "call m0_generic_impl_methods_Plain_identity__i32",
        "call m0_generic_impl_methods_Plain_identity__u16",
    });

    const fs::path bin = test_bin_root() / "generic_impl_methods";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");
}

TEST_F(AurexIntegrationTest, QualifiedGenericSubstitutionImport) {
    const std::string import_flags = sample_import_flags();
    const fs::path source = positive_sample("generics", "qualified_generic_substitution.ax");

    const std::string checked = require_success(aurexc() + " " + import_flags + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn qualified_generic_substitution.wrap_box<i32> -> samplelib.generic_a.Box<i32>",
        "fn qualified_generic_substitution.wrap_choice<i32> -> samplelib.generic_a.Choice<i32>",
        "struct samplelib.generic_a.Box<i32> fields=1",
        "case Choice<i32>_some : samplelib.generic_a.Choice<i32>(i32)",
    });

    const std::string ir = require_success(aurexc() + " " + import_flags + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn qualified_generic_substitution.wrap_box<i32>(value: i32)",
        "fn qualified_generic_substitution.wrap_choice<i32>(value: i32)",
        "record samplelib.generic_a.Box<i32>",
        "record samplelib.generic_a.Choice<i32>",
    });

    const fs::path bin = test_bin_root() / "qualified_generic_substitution";
    require_success(aurexc() + " " + import_flags + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");
}

TEST_F(AurexIntegrationTest, QualifiedGenericInferenceUsesAliasScope) {
    const std::string import_flags = sample_import_flags();
    const fs::path source = positive_sample("generics", "qualified_generic_inference_import.ax");

    const std::string checked = require_success(aurexc() + " " + import_flags + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn qualified_generic_inference_import.read_box<i32> -> i32",
        "fn qualified_generic_inference_import.unwrap_choice<i32> -> i32",
        "struct samplelib.generic_a.Box<i32> fields=1",
        "struct samplelib.generic_b.Box<i32> fields=1",
        "case Choice<i32>_some : samplelib.generic_a.Choice<i32>(i32)",
    });

    const std::string ir = require_success(aurexc() + " " + import_flags + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn qualified_generic_inference_import.read_box<i32>(box: samplelib.generic_a.Box<i32>)",
        "fn qualified_generic_inference_import.unwrap_choice<i32>(choice: samplelib.generic_a.Choice<i32>)",
        "call m0_qualified_generic_inference_import_read_box__i32",
        "call m0_qualified_generic_inference_import_unwrap_choice__i32",
    });

    const fs::path bin = test_bin_root() / "qualified_generic_inference_import";
    require_success(aurexc() + " " + import_flags + " " + q(source) + " -o " + q(bin));
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

    const fs::path unknown = negative_sample("generics", "unknown_generic_function.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(unknown)).output, "unknown generic function: missing");

    const fs::path plain_type_args = negative_sample("generics", "generic_function_type_args_on_plain.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(plain_type_args)).output, "type arguments require a generic function: plain");

    const fs::path generic_method = negative_sample("generics", "generic_method_unsupported.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_method)).output, "generic method requires explicit type arguments: id");

    const fs::path generic_method_arity = negative_sample("generics", "generic_method_type_arg_count.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_method_arity)).output,
        "generic method type argument count mismatch for passthrough"
    );

    const fs::path plain_method_type_args = negative_sample("generics", "generic_method_type_args_on_plain.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(plain_method_type_args)).output,
        "type arguments require a generic method"
    );

    const fs::path generic_method_conflict = negative_sample("generics", "generic_method_inference_conflict.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_method_conflict)).output,
        "generic method type inference conflict for T"
    );

    const fs::path generic_impl_self_type = negative_sample("generics", "generic_impl_method_self_type.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_impl_self_type)).output,
        "method self parameter must use the impl type or a pointer to it"
    );

    const fs::path generic_impl_target = negative_sample("generics", "generic_impl_target_param.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_impl_target)).output, "impl target must be a named type");

    const fs::path generic_impl_uninferred = negative_sample("generics", "generic_impl_method_uninferred_param.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_impl_uninferred)).output,
        "unknown method: generic_impl_method_uninferred_param.Box<i32>.hidden"
    );

    const fs::path generic_impl_no_self = negative_sample("generics", "generic_impl_method_no_self_receiver.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_impl_no_self)).output,
        "unknown method: generic_impl_method_no_self_receiver.Box<i32>.make"
    );

    const fs::path generic_impl_associated_unknown = negative_sample("generics", "generic_impl_associated_unknown.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(generic_impl_associated_unknown)).output,
        "unknown method: generic_impl_associated_unknown.Box<i32>.missing"
    );

    const fs::path generic_extern = negative_sample("generics", "generic_extern_unsupported.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_extern)).output, "generic extern c functions are not supported");

    const fs::path generic_prototype = negative_sample("generics", "generic_prototype_unsupported.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_prototype)).output, "generic function prototypes are not supported");

    const fs::path generic_variadic = negative_sample("generics", "generic_variadic_unsupported.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_variadic)).output, "generic variadic functions are not supported");

    const fs::path generic_export = negative_sample("generics", "generic_export_c_unsupported.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(generic_export)).output, "generic export c functions are not supported");

    const fs::path inference_conflict = negative_sample("generics", "generic_function_inference_conflict.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(inference_conflict)).output, "generic function type inference conflict for T");

    const fs::path struct_inference_conflict = negative_sample("generics", "generic_struct_literal_inference_conflict.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(struct_inference_conflict)).output,
        "generic struct literal type inference conflict for T"
    );

    const fs::path struct_invalid_storage = negative_sample("generics", "generic_struct_invalid_storage.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(struct_invalid_storage)).output, "field type is not valid storage");

    const fs::path enum_invalid_storage = negative_sample("generics", "generic_enum_invalid_payload_storage.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(enum_invalid_storage)).output, "enum payload type is not valid storage");

    const fs::path array_param = negative_sample("generics", "generic_function_array_param_storage.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(array_param)).output, "array type cannot be used as a function parameter");

    const fs::path array_struct_param = negative_sample("generics", "generic_function_array_struct_param_storage.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(array_struct_param)).output,
        "struct containing array cannot be passed by value"
    );

    const fs::path enum_arity = negative_sample("generics", "generic_enum_type_arg_count_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(enum_arity)).output, "generic enum type argument count mismatch");

    const fs::path struct_arity = negative_sample("generics", "generic_struct_type_arg_count_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(struct_arity)).output, "generic struct type argument count mismatch");

    const fs::path enum_base = negative_sample("generics", "generic_enum_base_non_integer.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(enum_base)).output, "enum base type must be an integer type");

    const fs::path enum_array_payload = negative_sample("generics", "generic_enum_array_payload_storage.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(enum_array_payload)).output, "enum payload cannot contain array storage");

    const fs::path inference_shapes = negative_sample("generics", "generic_function_inference_shape_mismatch.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(inference_shapes)).output,
        "generic function requires explicit type arguments"
    );
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

    const fs::path ambiguous_function = negative_sample("generics", "ambiguous_generic_function.ax");
    const std::string ambiguous_function_output =
        require_failure(aurexc() + " " + import_flags + " --check " + q(ambiguous_function)).output;
    expect_contains(ambiguous_function_output, "ambiguous generic function 'pick'");
    expect_not_contains(ambiguous_function_output, "unknown function: pick");

    const fs::path private_function = negative_sample("generics", "private_generic_function_import.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(private_function)).output,
        "unknown function: hidden_id"
    );

    const fs::path qualified_struct_missing_args = negative_sample("generics", "qualified_generic_struct_missing_args.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(qualified_struct_missing_args)).output,
        "generic struct type requires type arguments: ga::Box"
    );

    const fs::path qualified_plain_type_args = negative_sample("generics", "qualified_plain_type_args.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(qualified_plain_type_args)).output,
        "type arguments require a generic type: vis::PublicInt"
    );

    const fs::path qualified_private_function = negative_sample("generics", "qualified_private_generic_function.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(qualified_private_function)).output,
        "generic function is private: samplelib.generic_private.hidden_id"
    );

    const fs::path qualified_function_arity = negative_sample("generics", "qualified_generic_function_arity.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(qualified_function_arity)).output,
        "generic function type argument count mismatch for pick"
    );

    const fs::path qualified_plain_function = negative_sample("generics", "qualified_plain_function_type_args.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(qualified_plain_function)).output,
        "type arguments require a generic function: vis::exported"
    );

    const fs::path qualified_private_struct = negative_sample("generics", "qualified_private_generic_struct.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(qualified_private_struct)).output,
        "type arguments require a generic type: gp::Hidden"
    );

    const fs::path qualified_private_enum = negative_sample("generics", "qualified_private_generic_enum.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(qualified_private_enum)).output,
        "type arguments require a generic type: gp::Secret"
    );
}

} // namespace aurex::test
